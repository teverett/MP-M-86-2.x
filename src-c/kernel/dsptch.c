/*
 * dsptch.c -- C port of src/kernel/DSPTCH.RTM: the process dispatcher
 * (fdisp, int_disp_exit, farpdisp/pdisp, dsptch, insert_process,
 * schedule, switch_, and the whole dsp_table dispatch: sleep_act,
 * poll_act, delay_act, swap_act, flag_act, term_act, disp_act).
 *
 * Written specifically as a reading/debugging aid for the live
 * scheduler-hang investigation (docs/bugs.md #83-98 and onward), per
 * the user's own request -- NOT as a component of a linked, bootable
 * kernel (this project has no 8086 real-mode toolchain; see docs/
 * src-c.md's own "Status" section for what /src-c is and isn't).
 * Keep this file and src/kernel/DSPTCH.RTM in sync: whenever one
 * changes, re-read the other and update both, same as this project's
 * own AGENTS.md instructions for every /src <-> /src-c pair.
 *
 * `call cs:dsp_table[bx]` is ported as a plain C switch
 * (`dispatch_action`) on `p_stat`'s own value, same technique as
 * `abort.c`'s own `dispatch_abort` (`abort_tab`). `freepd` (proc.c)
 * is called as an ordinary internal call, not a boundary stub, same
 * convention as every other already-ported routine this project
 * calls into directly.
 *
 * Three real gaps have genuinely NO C equivalent, unlike every other
 * boundary-stub convention in this project (which model "a call
 * whose result doesn't matter" -- these are different: real hardware
 * mechanisms this single-threaded, no-real-registers host port
 * cannot represent at all):
 *
 *   1. `fdisp`'s own `pop ds!iret` and `int_disp_exit`'s own `ds=
 *      u_ds_sav!es=u_es_sav!iret` -- a real interrupt return. No real
 *      DS/ES/IP/CS/FLAGS registers exist here to resume into, so
 *      these functions simply stop after their own real, observable
 *      memory effects (same spirit as every other boundary-tail-jump
 *      in this project, just for a hardware mechanism instead of an
 *      unported callee).
 *   2. `dsptch`'s own register-save step (`mov ds:u_ax,ax`, etc.) and
 *      `switch_`'s own register-*restore* step are a REAL context
 *      switch: saving/restoring the actual CPU registers of whichever
 *      process is involved. This port takes the "saved" registers as
 *      explicit input parameters (`ax`..`bp`, representing whatever
 *      the real caller's own registers were at the call site) and
 *      returns the "restored" ones as output parameters (via
 *      `switch_result`) -- there is no way for a single C function
 *      call to actually *resume* a different call stack.
 *   3. `dsptch`'s own `mov u_ss,ss!mov u_sp,sp` (saving the real host
 *      stack pointer) and `switch_`'s own `mov ss,u_ss!mov sp,u_sp`
 *      (restoring it) are likewise real-hardware-only; `switch_`
 *      still returns the stored `u_ss`/`u_sp` values themselves
 *      (reading them back out of the UDA, a real memory read) so a
 *      caller/debugger can at least see what the real asm would
 *      switch the stack to.
 *
 * `term_act`'s own memory-free loop is a deliberate, documented
 * DIVERGENCE from the real asm, not just a missing-hardware gap: the
 * real asm's own loop re-reads `p_mem[si]` from scratch every
 * iteration, relying on `mpmif(f_memfree)`'s own real effect to pop
 * the list head each time -- under this project's boundary-stub
 * convention (no real effect), that would infinite-loop whenever a
 * terminating process still owns memory. Ported here as a plain
 * forward walk of the same `ms_link` chain instead (same shape as
 * `ml.c`'s own `mlfree_entry`), which terminates but does not call
 * `mfree_entry` for real. `schedule`'s own `io_polldev` poll (via the
 * boundary-stub `xiosif`) is fixed at "device not ready" (0), the
 * same already-established choice `chario.c` made for `xiosif`
 * elsewhere -- makes the "promote from PLR to RLR" branch
 * unreachable, translated faithfully but not exercised by this
 * port's own test.
 */
#include <stdint.h>

#include "defs.h"

extern void freepd(uint8_t *mem, uint16_t si);

static void insert_process(uint8_t *mem, uint16_t bx, uint16_t si) {
    uint16_t cx = (uint16_t)(mem_r16(mem, (uint16_t)(si + p_flag)) & pf_resource);
    uint16_t di;
    for (;;) {
        di = mem_r16(mem, (uint16_t)(bx + p_link));
        if (di == 0) {
            break; /* ins_out */
        }
        uint8_t al = mem[di + p_prior];
        uint8_t sp = mem[si + p_prior];
        if (al > sp) {
            break; /* ins_out */
        }
        if (al < sp) {
            bx = di;
            continue; /* ins_nxt */
        }
        if (cx == 0) {
            bx = di;
            continue; /* ins_nxt */
        }
        break; /* ins_out */
    }
    if (cx != 0) {
        mem_w16(mem, (uint16_t)(si + p_flag),
                (uint16_t)(mem_r16(mem, (uint16_t)(si + p_flag)) & (uint16_t)~pf_resource));
    }
    mem_w16(mem, (uint16_t)(si + p_link), di);
    mem_w16(mem, (uint16_t)(bx + p_link), si);
}

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp;
    uint16_t new_ss, new_sp; /* no C equivalent for "resume there", see header comment */
    uint8_t went_to_int_disp_exit;
} switch_result;

/*
 * int_disp_exit -- the interrupt-specific half of a dispatch return:
 * restore the (now-running) process's own pre-interrupt p_stat.
 *	(the real asm's own ax/bx shuffle through es_sav/bx_sav nets to
 *	no change and isn't modeled; ds/es-restore + iret have no C
 *	equivalent, see header comment)
 */
static void int_disp_exit(uint8_t *mem) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t uda = mem_r16(mem, (uint16_t)(rlr + p_uda));
    mem_w8_seg(mem, uda, u_in_int, 0); /* false */
    uint8_t stat = mem_r8_seg(mem, uda, u_stat_sav);
    mem[rlr + p_stat] = stat;
    mem[SD_INDISP] = 0;
}

/*
 * switch_ -- switch to the first process on the Ready List: restore
 * its own saved interrupt vectors (real, observable) and its own
 * saved registers (returned, see header comment on why).
 */
static switch_result switch_(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    uint16_t uda = mem_r16(mem, (uint16_t)(bx + p_uda));

    for (unsigned i = 0; i < 10; i++) {
        mem_w16(mem, (uint16_t)(2 * i), mem_r16_seg(mem, uda, (uint16_t)(u_ivectors + 2 * i)));
    }
    for (unsigned i = 0; i < 4; i++) {
        mem_w16(mem, (uint16_t)(i_mpm_ip + 2 * i), mem_r16_seg(mem, uda, (uint16_t)(u_mpm_ip + 2 * i)));
    }

    switch_result r;
    r.ax = mem_r16_seg(mem, uda, u_ax);
    r.bx = mem_r16_seg(mem, uda, u_bx);
    r.cx = mem_r16_seg(mem, uda, u_cx);
    r.dx = mem_r16_seg(mem, uda, u_dx);
    r.si = mem_r16_seg(mem, uda, u_si);
    r.di = mem_r16_seg(mem, uda, u_di);
    r.bp = mem_r16_seg(mem, uda, u_bp);
    r.new_ss = mem_r16_seg(mem, uda, u_ss);
    r.new_sp = mem_r16_seg(mem, uda, u_sp);

    if (mem_r8_seg(mem, uda, u_in_int)) {
        int_disp_exit(mem);
        r.went_to_int_disp_exit = 1;
    } else {
        /* dret -- the real IF-restore-timing fix (docs/bugs.md
         * #90/#92) has no C equivalent (no real FLAGS register);
         * its only other observable effect is clearing indisp */
        mem[SD_INDISP] = 0;
        r.went_to_int_disp_exit = 0;
    }
    return r;
}

/*
 * schedule -- poll the Poll List (boundary-stub io_polldev, fixed
 * not-ready, see header comment), drain the Dispatch Ready List onto
 * the Ready List, then switch to the first ready process.
 */
static switch_result schedule(uint8_t *mem) {
    uint16_t di = (uint16_t)(SD_PLR - p_link);
    for (;;) {
        uint16_t si = mem_r16(mem, (uint16_t)(di + p_link));
        if (si == 0) {
            break; /* drltorlr */
        }
        uint8_t al = 0; /* xiosif(io_polldev) -- boundary stub, fixed not-ready */
        if (al != 0) {
            uint16_t next = mem_r16(mem, (uint16_t)(si + p_link));
            mem_w16(mem, (uint16_t)(di + p_link), next);
            mem[si + p_stat] = ps_run;
            insert_process(mem, (uint16_t)(SD_RLR - p_link), si);
            /* di stays put -- see header comment on polld_another/polld_next */
        } else {
            di = si;
        }
    }
    for (;;) {
        uint16_t si = mem_r16(mem, SD_DRL);
        if (si == 0) {
            break; /* switch */
        }
        uint16_t ax = mem_r16(mem, (uint16_t)(si + p_link));
        mem_w16(mem, SD_DRL, ax);
        mem[si + p_stat] = ps_run;
        insert_process(mem, (uint16_t)(SD_RLR - p_link), si);
    }
    return switch_(mem);
}

static switch_result sleep_act(uint8_t *mem, uint16_t si, uint16_t si_uda) {
    uint16_t bx = mem_r16_seg(mem, si_uda, u_dparam);
    insert_process(mem, bx, si);
    mem_w16(mem, (uint16_t)(si + p_flag), (uint16_t)(mem_r16(mem, (uint16_t)(si + p_flag)) | pf_resource));
    mem[si + p_stat] = mem[si + p_scratch];
    return schedule(mem);
}

static switch_result poll_act(uint8_t *mem, uint16_t si, uint16_t si_uda) {
    uint16_t dx = mem_r16_seg(mem, si_uda, u_dparam);
    mem_w16(mem, (uint16_t)(si + p_wait), dx);
    uint16_t ax = mem_r16(mem, SD_PLR);
    mem_w16(mem, (uint16_t)(si + p_link), ax);
    mem_w16(mem, SD_PLR, si);
    return schedule(mem);
}

static switch_result delay_act(uint8_t *mem, uint16_t si, uint16_t si_uda) {
    /* xiosif(io_strtclk) -- boundary stub, no-op (result unused) */
    uint16_t bx = (uint16_t)(SD_DLR - p_link);
    uint16_t cx = (uint16_t)(mem_r16_seg(mem, si_uda, u_dparam) + 1);
    if (cx == 0) {
        cx = (uint16_t)(cx - 1);
    }
    uint16_t di;
    for (;;) {
        di = mem_r16(mem, (uint16_t)(bx + p_link));
        if (di == 0) {
            break; /* del_o */
        }
        uint16_t ax = mem_r16(mem, (uint16_t)(di + p_wait));
        if (ax > cx) {
            break; /* del_o */
        }
        cx = (uint16_t)(cx - ax);
        bx = di;
    }
    mem_w16(mem, (uint16_t)(si + p_link), di);
    mem_w16(mem, (uint16_t)(bx + p_link), si);
    mem_w16(mem, (uint16_t)(si + p_wait), cx);
    if (di != 0) {
        mem_w16(mem, (uint16_t)(di + p_wait), (uint16_t)(mem_r16(mem, (uint16_t)(di + p_wait)) - cx));
    }
    return schedule(mem);
}

static switch_result swap_act(uint8_t *mem, uint16_t si) {
    insert_process(mem, (uint16_t)(SD_SLR - p_link), si);
    return schedule(mem);
}

static switch_result disp_act(uint8_t *mem, uint16_t si) {
    mem[si + p_stat] = ps_run;
    insert_process(mem, (uint16_t)(SD_RLR - p_link), si);
    return schedule(mem);
}

static switch_result flag_act(uint8_t *mem, uint16_t si, uint16_t si_uda) {
    uint16_t bx = mem_r16_seg(mem, si_uda, u_dparam);
    if (mem_r16(mem, (uint16_t)(bx + flg_pd)) == flag_on) {
        mem_w16(mem, (uint16_t)(bx + flg_pd), flag_off);
        return disp_act(mem, si);
    }
    mem_w16(mem, (uint16_t)(bx + flg_pd), si);
    mem_w16(mem, (uint16_t)(si + p_link), 0);
    return schedule(mem);
}

static switch_result term_act(uint8_t *mem, uint16_t si) {
    uint16_t ax = mem_r16(mem, SD_RLR);
    mem_w16(mem, (uint16_t)(si + p_link), ax);
    mem_w16(mem, SD_RLR, si);

    uint8_t ncondev = mem[SD_NCONDEV];
    for (int16_t cl = (int16_t)ncondev - 1; cl >= 0; cl--) {
        mem[si + p_cns] = (uint8_t)cl;
        /* mpmif(f_condetach) -- boundary stub, no-op */
    }
    uint8_t nlstdev = mem[SD_NLSTDEV];
    for (int16_t cl = (int16_t)nlstdev - 1; cl >= 0; cl--) {
        mem[si + p_lst] = (uint8_t)(cl + ncondev);
        /* mpmif(f_lstdetach) -- boundary stub, no-op */
    }

    /* free this process's own memory list -- see header comment on
     * why this diverges from the real asm's own loop shape */
    uint16_t seg = mem_r16(mem, (uint16_t)(si + p_mem));
    while (seg != 0) {
        uint16_t next = mem_r16(mem, (uint16_t)(seg + ms_link));
        /* mpmif(f_memfree) -- boundary stub, no-op */
        seg = next;
    }

    mem_w16(mem, SD_MXMEMOWNER, 0);
    /* mpmif(f_qwrite, mxmemqpb) -- boundary stub, no-op */

    ax = mem_r16(mem, (uint16_t)(si + p_link));
    mem_w16(mem, SD_RLR, ax);
    mem_w16(mem, (uint16_t)(si + p_link), 0);
    freepd(mem, si);
    return schedule(mem);
}

static switch_result dispatch_action(uint8_t *mem, uint16_t si, uint16_t si_uda, uint8_t stat) {
    switch (stat) {
        case 0: case 6: case 7: return disp_act(mem, si); /* run, (nop)-dq, (nop)-nq */
        case 1: return poll_act(mem, si, si_uda);
        case 2: return delay_act(mem, si, si_uda);
        case 3: return swap_act(mem, si);
        case 4: return term_act(mem, si);
        case 5: return sleep_act(mem, si, si_uda);
        case 8: return flag_act(mem, si, si_uda);
        default: return disp_act(mem, si); /* 9 = ciowait: no real table entry */
    }
}

/*
 * dsptch -- the dispatch function: takes the calling context's own
 * registers (no C equivalent for "the real caller's own live
 * registers", see header comment -- passed explicitly instead), sets
 * the re-entrancy guard, saves them into the current process's own
 * UDA along with its interrupt vectors, takes it off the Ready List,
 * and dispatches on its own p_stat.
 *	output: *was_already_indisp is set and the function returns
 *		immediately (the real asm's own "popf!ret") if a
 *		dispatch was already in progress
 */
switch_result dsptch(uint8_t *mem, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx,
                      uint16_t si, uint16_t di, uint16_t bp, uint8_t *was_already_indisp) {
    if (mem[SD_INDISP]) {
        *was_already_indisp = 1;
        switch_result none = {0};
        return none;
    }
    *was_already_indisp = 0;
    mem[SD_INDISP] = 1;

    uint16_t cur = mem_r16(mem, SD_RLR);
    uint16_t cur_uda = mem_r16(mem, (uint16_t)(cur + p_uda));
    mem_w16_seg(mem, cur_uda, u_ax, ax);
    mem_w16_seg(mem, cur_uda, u_bx, bx);
    mem_w16_seg(mem, cur_uda, u_cx, cx);
    mem_w16_seg(mem, cur_uda, u_dx, dx);
    mem_w16_seg(mem, cur_uda, u_di, di);
    mem_w16_seg(mem, cur_uda, u_si, si);
    mem_w16_seg(mem, cur_uda, u_bp, bp);

    for (unsigned i = 0; i < 10; i++) {
        mem_w16_seg(mem, cur_uda, (uint16_t)(u_ivectors + 2 * i), mem_r16(mem, (uint16_t)(2 * i)));
    }
    for (unsigned i = 0; i < 4; i++) {
        mem_w16_seg(mem, cur_uda, (uint16_t)(u_mpm_ip + 2 * i), mem_r16(mem, (uint16_t)(i_mpm_ip + 2 * i)));
    }

    uint16_t next = mem_r16(mem, (uint16_t)(cur + p_link));
    mem_w16(mem, SD_RLR, next);
    mem_w16(mem, (uint16_t)(cur + p_link), 0);

    uint8_t stat = mem[cur + p_stat];
    return dispatch_action(mem, cur, cur_uda, stat);
}

/* pdisp/farpdisp -- call the dispatcher with no special action (the
 * current process just goes back on the Ready List as-is) */
switch_result pdisp(uint8_t *mem, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx,
                     uint16_t si, uint16_t di, uint16_t bp, uint8_t *was_already_indisp) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    mem[rlr + p_stat] = ps_run;
    return dsptch(mem, ax, bx, cx, dx, si, di, bp, was_already_indisp);
}

switch_result farpdisp(uint8_t *mem, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx,
                        uint16_t si, uint16_t di, uint16_t bp, uint8_t *was_already_indisp) {
    return pdisp(mem, ax, bx, cx, dx, si, di, bp, was_already_indisp);
}

/*
 * fdisp -- the dispatcher's own interrupt-entry point (used by XIOS
 * interrupt routines). "pop ds!iret" (already-in-dispatch case) has
 * no C equivalent; see header comment.
 */
switch_result fdisp(uint8_t *mem, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx,
                     uint16_t si, uint16_t di, uint16_t bp, uint8_t *was_already_indisp) {
    if (mem[SD_INDISP]) {
        *was_already_indisp = 1;
        switch_result none = {0};
        return none;
    }
    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t rlr_uda = mem_r16(mem, (uint16_t)(rlr + p_uda));
    mem_w8_seg(mem, rlr_uda, u_in_int, 0xFF); /* true */
    /* pop u_ds_sav -- no C equivalent (no real DS register), not modeled */
    uint8_t stat_sav = mem[rlr + p_stat];
    mem_w8_seg(mem, rlr_uda, u_stat_sav, stat_sav);
    mem[rlr + p_stat] = ps_run;
    return dsptch(mem, ax, bx, cx, dx, si, di, bp, was_already_indisp);
}
