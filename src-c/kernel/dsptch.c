/*
 * dsptch.c -- C port of src/kernel/DSPTCH.RTM: the process dispatcher
 * (fdisp, int_disp_exit, farpdisp/pdisp, dsptch, insert_process,
 * schedule, switch_, and the whole dsp_table dispatch: sleep_act,
 * poll_act, delay_act, swap_act, flag_act, term_act, disp_act).
 *
 * First written as a reading/debugging aid for the live scheduler-hang
 * investigation (docs/bugs.md #83-98 and onward), per the user's own
 * request; now also linked into src-c's kernel.elf on real memory.
 * Keep this file and src/kernel/DSPTCH.RTM in sync: whenever one
 * changes, re-read the other and update both, same as this project's
 * own AGENTS.md instructions for every /src <-> /src-c pair.
 *
 * Runs against real memory (defs.h's far_ptr), like proc.c: `phys` is
 * the base of the real 8086 physical address space and `sysdat` the
 * real SYSDAT segment (the real asm's DS/cs:sysdat). PDs, the ready/
 * dispatch/poll/delay/swap lists, flags, memory descriptors, indisp
 * and mxmemowner are SYSDAT-relative; u_* fields go through the UDA
 * segment of whichever process the real asm has in ES at that point
 * (p_uda[rlr] -- before the rlr pop for the outgoing process, after
 * it for the incoming one); the saved/restored interrupt vectors are
 * at segment 0.
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

extern void freepd(uint8_t *phys, uint16_t sysdat, uint16_t si);

/* Boundary hooks (defs.h) the runtime fills in at boot; unset, each is
 * the old stub: no-op mpmif, an XIOS that reports "not ready", and no
 * context switch. */
MPMIF_HOOK(dsptch)
XIOSIF_HOOK(dsptch)

/* The context switch itself (header comment, gaps 2 and 3): called at
 * the end of every dispatch with the PD that was running when it began;
 * the runtime resumes the new rlr's own stack there and returns when
 * `from` is dispatched again. */
typedef void (*dsptch_switch_fn)(uint8_t *phys, uint16_t sysdat, uint16_t from);
static dsptch_switch_fn dsptch_switch;
void dsptch_set_switch(dsptch_switch_fn fn) { dsptch_switch = fn; }

static void insert_process(uint8_t *sd, uint16_t bx, uint16_t si) {
    uint16_t cx = (uint16_t)(seg_r16(sd, (uint16_t)(si + p_flag)) & pf_resource);
    uint16_t di;
    for (;;) {
        di = seg_r16(sd, (uint16_t)(bx + p_link));
        if (di == 0) {
            break; /* ins_out */
        }
        uint8_t al = seg_r8(sd, (uint16_t)(di + p_prior));
        uint8_t sp = seg_r8(sd, (uint16_t)(si + p_prior));
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
        seg_w16(sd, (uint16_t)(si + p_flag),
                (uint16_t)(seg_r16(sd, (uint16_t)(si + p_flag)) & (uint16_t)~pf_resource));
    }
    seg_w16(sd, (uint16_t)(si + p_link), di);
    seg_w16(sd, (uint16_t)(bx + p_link), si);
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
static void int_disp_exit(uint8_t *phys, uint8_t *sd) {
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    uint8_t *uda = far_ptr(phys, seg_r16(sd, (uint16_t)(rlr + p_uda)), 0);
    seg_w8(uda, u_in_int, 0); /* false */
    seg_w8(sd, (uint16_t)(rlr + p_stat), seg_r8(uda, u_stat_sav));
    seg_w8(sd, SYSDAT_INDISP, 0);
}

/*
 * switch_ -- switch to the first process on the Ready List: restore
 * its own saved interrupt vectors (real, observable) and its own
 * saved registers (returned, see header comment on why).
 */
static switch_result switch_(uint8_t *phys, uint8_t *sd) {
    uint16_t bx = seg_r16(sd, SYSDAT_RLR);
    uint8_t *uda = far_ptr(phys, seg_r16(sd, (uint16_t)(bx + p_uda)), 0);
    uint8_t *ivt = far_ptr(phys, 0, 0);

    for (unsigned i = 0; i < 10; i++) {
        seg_w16(ivt, (uint16_t)(2 * i), seg_r16(uda, (uint16_t)(u_ivectors + 2 * i)));
    }
    for (unsigned i = 0; i < 4; i++) {
        seg_w16(ivt, (uint16_t)(i_mpm_ip + 2 * i), seg_r16(uda, (uint16_t)(u_mpm_ip + 2 * i)));
    }

    switch_result r;
    r.ax = seg_r16(uda, u_ax);
    r.bx = seg_r16(uda, u_bx);
    r.cx = seg_r16(uda, u_cx);
    r.dx = seg_r16(uda, u_dx);
    r.si = seg_r16(uda, u_si);
    r.di = seg_r16(uda, u_di);
    r.bp = seg_r16(uda, u_bp);
    r.new_ss = seg_r16(uda, u_ss);
    r.new_sp = seg_r16(uda, u_sp);

    if (seg_r8(uda, u_in_int)) {
        int_disp_exit(phys, sd);
        r.went_to_int_disp_exit = 1;
    } else {
        /* dret -- the real IF-restore-timing fix (docs/bugs.md
         * #90/#92) has no C equivalent (no real FLAGS register);
         * its only other observable effect is clearing indisp */
        seg_w8(sd, SYSDAT_INDISP, 0);
        r.went_to_int_disp_exit = 0;
    }
    return r;
}

/*
 * schedule -- poll the Poll List (boundary-stub io_polldev, fixed
 * not-ready, see header comment), drain the Dispatch Ready List onto
 * the Ready List, then switch to the first ready process.
 */
static switch_result schedule(uint8_t *phys, uint8_t *sd, uint16_t sysdat) {
    uint16_t di = (uint16_t)(SYSDAT_PLR - p_link);
    for (;;) {
        uint16_t si = seg_r16(sd, (uint16_t)(di + p_link));
        if (si == 0) {
            break; /* drltorlr */
        }
        /* mov cx,p_wait[si] ! mov al,io_polldev ! call xiosif */
        uint8_t al = (uint8_t)XIOSIF(dsptch, phys, sysdat, io_polldev,
                                     seg_r16(sd, (uint16_t)(si + p_wait)), 0);
        if (al != 0) {
            seg_w16(sd, (uint16_t)(di + p_link), seg_r16(sd, (uint16_t)(si + p_link)));
            seg_w8(sd, (uint16_t)(si + p_stat), ps_run);
            insert_process(sd, (uint16_t)(SYSDAT_RLR - p_link), si);
            /* di stays put -- see header comment on polld_another/polld_next */
        } else {
            di = si;
        }
    }
    for (;;) {
        uint16_t si = seg_r16(sd, SYSDAT_DRL);
        if (si == 0) {
            break; /* switch */
        }
        seg_w16(sd, SYSDAT_DRL, seg_r16(sd, (uint16_t)(si + p_link)));
        seg_w8(sd, (uint16_t)(si + p_stat), ps_run);
        insert_process(sd, (uint16_t)(SYSDAT_RLR - p_link), si);
    }
    return switch_(phys, sd);
}

static switch_result sleep_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si, const uint8_t *si_uda) {
    insert_process(sd, seg_r16(si_uda, u_dparam), si);
    seg_w16(sd, (uint16_t)(si + p_flag), (uint16_t)(seg_r16(sd, (uint16_t)(si + p_flag)) | pf_resource));
    seg_w8(sd, (uint16_t)(si + p_stat), seg_r8(sd, (uint16_t)(si + p_scratch)));
    return schedule(phys, sd, sysdat);
}

static switch_result poll_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si, const uint8_t *si_uda) {
    seg_w16(sd, (uint16_t)(si + p_wait), seg_r16(si_uda, u_dparam));
    seg_w16(sd, (uint16_t)(si + p_link), seg_r16(sd, SYSDAT_PLR));
    seg_w16(sd, SYSDAT_PLR, si);
    return schedule(phys, sd, sysdat);
}

static switch_result delay_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si, const uint8_t *si_uda) {
    (void)XIOSIF(dsptch, phys, sysdat, io_strtclk, 0, 0);
    uint16_t bx = (uint16_t)(SYSDAT_DLR - p_link);
    uint16_t cx = (uint16_t)(seg_r16(si_uda, u_dparam) + 1);
    if (cx == 0) {
        cx = (uint16_t)(cx - 1);
    }
    uint16_t di;
    for (;;) {
        di = seg_r16(sd, (uint16_t)(bx + p_link));
        if (di == 0) {
            break; /* del_o */
        }
        uint16_t ax = seg_r16(sd, (uint16_t)(di + p_wait));
        if (ax > cx) {
            break; /* del_o */
        }
        cx = (uint16_t)(cx - ax);
        bx = di;
    }
    seg_w16(sd, (uint16_t)(si + p_link), di);
    seg_w16(sd, (uint16_t)(bx + p_link), si);
    seg_w16(sd, (uint16_t)(si + p_wait), cx);
    if (di != 0) {
        seg_w16(sd, (uint16_t)(di + p_wait), (uint16_t)(seg_r16(sd, (uint16_t)(di + p_wait)) - cx));
    }
    return schedule(phys, sd, sysdat);
}

static switch_result swap_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si) {
    insert_process(sd, (uint16_t)(SYSDAT_SLR - p_link), si);
    return schedule(phys, sd, sysdat);
}

static switch_result disp_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si) {
    seg_w8(sd, (uint16_t)(si + p_stat), ps_run);
    insert_process(sd, (uint16_t)(SYSDAT_RLR - p_link), si);
    return schedule(phys, sd, sysdat);
}

static switch_result flag_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si, const uint8_t *si_uda) {
    uint16_t bx = seg_r16(si_uda, u_dparam);
    if (seg_r16(sd, (uint16_t)(bx + flg_pd)) == flag_on) {
        seg_w16(sd, (uint16_t)(bx + flg_pd), flag_off);
        return disp_act(phys, sd, sysdat, si);
    }
    seg_w16(sd, (uint16_t)(bx + flg_pd), si);
    seg_w16(sd, (uint16_t)(si + p_link), 0);
    return schedule(phys, sd, sysdat);
}

static switch_result term_act(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si) {
    seg_w16(sd, (uint16_t)(si + p_link), seg_r16(sd, SYSDAT_RLR));
    seg_w16(sd, SYSDAT_RLR, si);

    uint8_t ncondev = seg_r8(sd, SYSDAT_NCONDEV);
    for (int16_t cl = (int16_t)ncondev - 1; cl >= 0; cl--) {
        seg_w8(sd, (uint16_t)(si + p_cns), (uint8_t)cl);
        (void)MPMIF(dsptch, phys, sysdat, f_condetach, 0, 0, sysdat);
    }
    uint8_t nlstdev = seg_r8(sd, SYSDAT_NLSTDEV);
    for (int16_t cl = (int16_t)nlstdev - 1; cl >= 0; cl--) {
        seg_w8(sd, (uint16_t)(si + p_lst), (uint8_t)(cl + ncondev));
        (void)MPMIF(dsptch, phys, sysdat, f_lstdetach, 0, 0, sysdat);
    }

    if (dsptch_mpmif) {
        /* free_nxt: the asm's own loop -- f_memfree takes the head MD off
         * p_mem, so p_mem is re-read each time. Its MFPB {start, pd=0}
         * is on the dispatcher's stack (SS = DS = SYSDAT). */
        for (;;) {
            uint16_t md = seg_r16(sd, (uint16_t)(si + p_mem));
            if (md == 0) break;
            uint16_t mfpb = (uint16_t)(SYSDAT_DSPTCHTOS - 4);
            seg_w16(sd, (uint16_t)(mfpb + mfpb_start), seg_r16(sd, (uint16_t)(md + ms_start)));
            seg_w16(sd, (uint16_t)(mfpb + mfpb_pd), 0);
            (void)MPMIF(dsptch, phys, sysdat, f_memfree, mfpb, 0, sysdat);
        }
    } else {
        /* no MEM to call (a differential test): a plain forward walk of
         * the same chain -- see header comment */
        uint16_t seg = seg_r16(sd, (uint16_t)(si + p_mem));
        while (seg != 0) {
            seg = seg_r16(sd, (uint16_t)(seg + ms_link));
        }
    }

    seg_w16(sd, SYSDAT_MXMEMOWNER, 0);
    (void)MPMIF(dsptch, phys, sysdat, f_qwrite, SYSDAT_MXMEMQPB, 0, sysdat);

    seg_w16(sd, SYSDAT_RLR, seg_r16(sd, (uint16_t)(si + p_link)));
    seg_w16(sd, (uint16_t)(si + p_link), 0);
    freepd(phys, sysdat, si);
    return schedule(phys, sd, sysdat);
}

static switch_result dispatch_action(uint8_t *phys, uint8_t *sd, uint16_t sysdat, uint16_t si,
                                     const uint8_t *si_uda, uint8_t stat) {
    switch (stat) {
        case 0: case 6: case 7: return disp_act(phys, sd, sysdat, si); /* run, (nop)-dq, (nop)-nq */
        case 1: return poll_act(phys, sd, sysdat, si, si_uda);
        case 2: return delay_act(phys, sd, sysdat, si, si_uda);
        case 3: return swap_act(phys, sd, sysdat, si);
        case 4: return term_act(phys, sd, sysdat, si);
        case 5: return sleep_act(phys, sd, sysdat, si, si_uda);
        case 8: return flag_act(phys, sd, sysdat, si, si_uda);
        default: return disp_act(phys, sd, sysdat, si); /* 9 = ciowait: no real table entry */
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
switch_result dsptch(uint8_t *phys, uint16_t sysdat, uint16_t ax, uint16_t bx, uint16_t cx,
                     uint16_t dx, uint16_t si, uint16_t di, uint16_t bp,
                     uint8_t *was_already_indisp) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (seg_r8(sd, SYSDAT_INDISP)) {
        *was_already_indisp = 1;
        switch_result none = {0};
        return none;
    }
    *was_already_indisp = 0;
    seg_w8(sd, SYSDAT_INDISP, 1);

    uint16_t cur = seg_r16(sd, SYSDAT_RLR);
    uint8_t *cur_uda = far_ptr(phys, seg_r16(sd, (uint16_t)(cur + p_uda)), 0);
    seg_w16(cur_uda, u_ax, ax);
    seg_w16(cur_uda, u_bx, bx);
    seg_w16(cur_uda, u_cx, cx);
    seg_w16(cur_uda, u_dx, dx);
    seg_w16(cur_uda, u_di, di);
    seg_w16(cur_uda, u_si, si);
    seg_w16(cur_uda, u_bp, bp);

    const uint8_t *ivt = far_ptr(phys, 0, 0);
    for (unsigned i = 0; i < 10; i++) {
        seg_w16(cur_uda, (uint16_t)(u_ivectors + 2 * i), seg_r16(ivt, (uint16_t)(2 * i)));
    }
    for (unsigned i = 0; i < 4; i++) {
        seg_w16(cur_uda, (uint16_t)(u_mpm_ip + 2 * i), seg_r16(ivt, (uint16_t)(i_mpm_ip + 2 * i)));
    }

    seg_w16(sd, SYSDAT_RLR, seg_r16(sd, (uint16_t)(cur + p_link)));
    seg_w16(sd, (uint16_t)(cur + p_link), 0);

    switch_result r = dispatch_action(phys, sd, sysdat, cur, cur_uda, seg_r8(sd, (uint16_t)(cur + p_stat)));
    if (dsptch_switch) {
        dsptch_switch(phys, sysdat, cur);
    }
    return r;
}

/* pdisp/farpdisp -- call the dispatcher with no special action (the
 * current process just goes back on the Ready List as-is) */
switch_result pdisp(uint8_t *phys, uint16_t sysdat, uint16_t ax, uint16_t bx, uint16_t cx,
                    uint16_t dx, uint16_t si, uint16_t di, uint16_t bp,
                    uint8_t *was_already_indisp) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    seg_w8(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_stat), ps_run);
    return dsptch(phys, sysdat, ax, bx, cx, dx, si, di, bp, was_already_indisp);
}

switch_result farpdisp(uint8_t *phys, uint16_t sysdat, uint16_t ax, uint16_t bx, uint16_t cx,
                       uint16_t dx, uint16_t si, uint16_t di, uint16_t bp,
                       uint8_t *was_already_indisp) {
    return pdisp(phys, sysdat, ax, bx, cx, dx, si, di, bp, was_already_indisp);
}

/*
 * fdisp -- the dispatcher's own interrupt-entry point (used by XIOS
 * interrupt routines). "pop ds!iret" (already-in-dispatch case) has
 * no C equivalent; see header comment.
 */
switch_result fdisp(uint8_t *phys, uint16_t sysdat, uint16_t ax, uint16_t bx, uint16_t cx,
                    uint16_t dx, uint16_t si, uint16_t di, uint16_t bp,
                    uint8_t *was_already_indisp) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (seg_r8(sd, SYSDAT_INDISP)) {
        *was_already_indisp = 1;
        switch_result none = {0};
        return none;
    }
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    uint8_t *rlr_uda_p = far_ptr(phys, seg_r16(sd, (uint16_t)(rlr + p_uda)), 0);
    seg_w8(rlr_uda_p, u_in_int, 0xFF); /* true */
    /* pop u_ds_sav -- no C equivalent (no real DS register), not modeled */
    seg_w8(rlr_uda_p, u_stat_sav, seg_r8(sd, (uint16_t)(rlr + p_stat)));
    seg_w8(sd, (uint16_t)(rlr + p_stat), ps_run);
    return dsptch(phys, sysdat, ax, bx, cx, dx, si, di, bp, was_already_indisp);
}
