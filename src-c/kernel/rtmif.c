/*
 * rtmif.c -- C port of src/kernel/RTMIF.RTM's `init` AND now `entry`
 * (the module's own inter-module RTM dispatch, `mov ch,0!shl cx,1!
 * mov si,cx!call cs:function[si]!retf` -- unlike `MEMIF.MEM`'s own
 * `entry`, no mutex wrapper, just a plain 21-way dispatch, same
 * shape as `CIOIF.CIO`'s own).
 *
 * Real `RTMIF.RTM` leaves EVERY register the called function sets
 * exactly as-is, with no normalization (`call cs:function[si] !
 * rtm_ret: retf`, no `mov ax,bx` the way `CIOIF.CIO`/`MEMIF.MEM`'s
 * own `entry` tails do) -- so `entry_result` here is a wider
 * {ax,bx,cx,dx,si,di,bp} grab-bag than `cioif.c`'s own, defaulting to
 * a straight passthrough of whatever the caller's own ambient
 * registers were (matching the real asm leaving an untouched
 * register alone) and overwritten only where a given target's own
 * real body actually defines something -- confirmed per-target
 * against each one's own "input:"/"output:" comment, not assumed
 * uniform.
 *
 * `creat_proc_entry` (case 14) needs the widest ambient set of all
 * (`ax`/`bx`/`cx`/`si`/`di`/`bp`, plus `sysdat` and
 * `was_already_indisp`) because its own real tail is `call proc_creat
 * ! jmp pdisp` -- a real context-switch outcome (`dsptch.c`'s own
 * `switch_result`), the richest thing any of these 21 targets can
 * produce; see `sysent.c`'s own header comment for why `ax`/`bx`/`cx`/
 * `si`/`di` end up NOT meaningfully specified by the real asm itself
 * either (both sides agree on that, so it's not a gap this dispatcher
 * introduces). `pd_entry` (case 16) needs the caller's own `ds`;
 * `findpdname_entry` (case 20) needs the caller's own `bx` (thread-
 * list-root context) and reads its own `name` argument straight out
 * of `mem` at `dx` (matching its own real "DX -> name in u_wrkseg"
 * convention, under this project's flat-memory host model).
 * `abort_spec_entry` (case 17) needs `RTMIF_ABORT_TERMINATE_OFFSET`
 * -- not a per-call register at all, but a real, fixed address
 * within this same combined module (`ABORT.RTM`'s own `terminate:`
 * label, probed via `--symbols` against `src/kernel/RTM.A86`, same
 * convention `tests/src-c/test_abort.py` already established for
 * `abort_spec_entry`'s own direct tests) -- hardcoded here as a
 * `#define`, since every real caller of this combined module shares
 * the one assembled layout.
 *
 * `dispatch_entry` (case 12) is `sysent.c`'s own already-established
 * no-op stub (the real body is a bare `jmp pdisp` tail-jump, no
 * observable transformation of its own under the boundary-stub
 * convention) -- nothing extra needed here.
 *
 * Unlike `CIOIF.CIO`/`MEMIF.MEM`'s own `init` (a trivial 7-instruction
 * body), this one is substantially larger, but -- confirmed directly
 * against the real asm, not assumed -- still has no `callf`/far `CALL`
 * of any kind (only one ordinary near `call proc_creat`), so it needs
 * none of `tools/i86sim.py`'s far-call machinery either.
 *
 * `init` does four things in sequence:
 *
 *   1. The same `sysdat`/`supervisor` copy as `CIOIF.CIO`/`MEMIF.MEM`'s
 *      own `init` (see that file's own header comment) -- copies
 *      `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG` into this module's own real
 *      cseg cells (`RTMIF_SUPERVISOR_OFF`/`RTMIF_SUPERVISOR_SEG`).
 *
 *   2. Initializes `SYSDAT.FMT`'s own `dispatcher`/`rtm_pdisp` far
 *      pointers to `DSPTCH.RTM`'s own `fdisp`/`farpdisp` (both
 *      already ported, `dsptch.c`) within this same `cs` -- these are
 *      genuinely external/ambient offsets nothing in this file can
 *      resolve on its own (same reasoning as `init.c`'s own
 *      `notick_offset`/`idle_offset`/`user_entry_offset`), so they're
 *      parameters here too.
 *
 *   3. Creates the tick and idle processes' own UDA setup: each
 *      one's own `p_uda` field (`SD_TICKPD`/`SD_IDLEPD`) is a
 *      paragraph offset relative to `sysdat`'s own base, not yet an
 *      absolute segment (the same "GENSYS stock template" relocation
 *      class the real asm's own comment on `u_initss` documents in
 *      detail) -- relocated here by adding `ds_sysdat`. Each one's
 *      own UDA gets `u_initcs` set to the *Supervisor* module's own
 *      segment (`RTMIF_SUPERVISOR_SEG`, just written in step 1) --
 *      both processes first run through `SUPIF.SUP`'s own
 *      `user_entry`, not a real user `CS` -- and `u_initss` set to
 *      `sysdat`'s own segment, the already-fixed `/src` bug the real
 *      asm's own comment documents at length (confirmed fixed in the
 *      current `src/kernel/RTMIF.RTM`, not re-fixed here).
 *
 *   4. One real call into `proc.c`'s already-ported `proc_creat`
 *      (`dx = SD_IDLEPD`, the real asm's own last-assigned `bx`,
 *      since `proc_creat` itself walks the whole pending-creation
 *      chain `p_link` threads together -- one call handles both
 *      tick and idle). Its own output is never read by the real asm
 *      (`retf` follows immediately), so it's discarded here too,
 *      same as `sysent.c`'s own `creat_proc_entry` already discards
 *      several of `proc_creat`'s own side effects it can't usefully
 *      re-expose (see that file's own header comment).
 *
 * `cs`/`ds_sysdat` are genuinely external/ambient values, same
 * convention as `init.c`'s own identically-named parameters.
 * `ds_sysdat` additionally has to be a *real* segment here (unlike
 * every other such parameter elsewhere in this project, which can be
 * an arbitrary tag value under non-segmented testing): this routine
 * reads/writes several genuine `SYSDAT.FMT`-relative fields
 * (`supmod`/`dispatcher`/`rtm_pdisp`/`idlepd`/`tickpd`, all real
 * `DS`-relative references in the real asm) via `mem_*_seg(mem,
 * ds_sysdat, ...)` and `defs.h`'s own `SYSDAT_*` raw offsets, rather
 * than a flat `SD_*` scratch address -- see `defs.h`'s own header
 * comment on those constants for why.
 */
#include <stdint.h>

#include "defs.h"

extern uint16_t proc_creat(uint8_t *mem, uint16_t dx, uint16_t sysdat, uint16_t *dx_out);

/* ABORT.RTM's own real `terminate:` label address within the combined
 * RTM.A86 build -- see this file's own header comment. Probed via
 * `python3 tools/asm86.py src/kernel/RTM.A86 -I src/kernel --symbols`;
 * re-probe if that build's own encoding ever shifts. */
#define RTMIF_ABORT_TERMINATE_OFFSET 0x0C68

typedef struct {
    uint16_t bx, cx;
} entry_bxcx;

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp;
    uint16_t new_ss, new_sp;
    uint8_t went_to_int_disp_exit;
} switch_result;

extern uint16_t sysreset_entry(uint8_t *mem);
extern void poll_entry(uint8_t *mem, uint16_t dx);
extern entry_bxcx flag_wait_entry(uint8_t *mem, uint8_t dl);
extern entry_bxcx flag_set_entry(uint8_t *mem, uint8_t dl);
extern entry_bxcx makeq_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat);
extern entry_bxcx openq_entry(uint8_t *mem, uint16_t dx);
extern entry_bxcx deleteq_entry(uint8_t *mem, uint16_t dx);
extern entry_bxcx readq_entry(uint8_t *mem, uint16_t dx);
extern entry_bxcx creadq_entry(uint8_t *mem, uint16_t dx);
extern entry_bxcx writeq_entry(uint8_t *mem, uint16_t dx);
extern entry_bxcx cwriteq_entry(uint8_t *mem, uint16_t dx);
extern void delay_entry(uint8_t *mem, uint16_t dx);
extern void dispatch_entry(void);
extern entry_bxcx terminate_entry(uint8_t *mem, uint16_t dx);
extern switch_result creat_proc_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat,
                                       uint16_t ax, uint16_t bx, uint16_t cx,
                                       uint16_t si, uint16_t di, uint16_t bp,
                                       uint8_t *was_already_indisp);
extern void set_prior_entry(uint8_t *mem, uint8_t dl);
extern uint16_t pd_entry(uint8_t *mem, uint16_t ds, uint16_t *cx_out);
extern entry_bxcx abort_spec_entry(uint8_t *mem, uint16_t dx, uint16_t terminate_offset);
extern void sleep_entry(uint8_t *mem, uint16_t dx);
extern void wakeup_entry(uint8_t *mem, uint16_t dx);
extern uint16_t findpdname_entry(uint8_t *mem, uint16_t bx, const uint8_t *name, uint16_t *cx_out);

void init(uint8_t *mem, uint16_t ds_sysdat, uint16_t cs,
          uint16_t fdisp_offset, uint16_t farpdisp_offset) {
    mem_w16(mem, RTMIF_SYSDAT, ds_sysdat);

    mem_w16(mem, RTMIF_SUPERVISOR_OFF, mem_r16_seg(mem, ds_sysdat, SYSDAT_SUPMOD));
    mem_w16(mem, RTMIF_SUPERVISOR_SEG, mem_r16_seg(mem, ds_sysdat, (uint16_t)(SYSDAT_SUPMOD + 2)));

    /* init interrupt pdisp / intermodule pdisp */
    mem_w16_seg(mem, ds_sysdat, SYSDAT_DISPATCHER, fdisp_offset);
    mem_w16_seg(mem, ds_sysdat, (uint16_t)(SYSDAT_DISPATCHER + 2), cs);
    mem_w16_seg(mem, ds_sysdat, SYSDAT_RTM_PDISP, farpdisp_offset);
    mem_w16_seg(mem, ds_sysdat, (uint16_t)(SYSDAT_RTM_PDISP + 2), cs);

    /* create tick and idle process */
    uint16_t sup_seg = mem_r16(mem, RTMIF_SUPERVISOR_SEG);

    uint16_t tick_uda = (uint16_t)(mem_r16_seg(mem, ds_sysdat, (uint16_t)(SYSDAT_TICKPD + p_uda)) + ds_sysdat);
    mem_w16_seg(mem, tick_uda, u_initcs, sup_seg);
    mem_w16_seg(mem, tick_uda, u_initss, ds_sysdat);

    uint16_t idle_uda = (uint16_t)(mem_r16_seg(mem, ds_sysdat, (uint16_t)(SYSDAT_IDLEPD + p_uda)) + ds_sysdat);
    mem_w16_seg(mem, idle_uda, u_initcs, sup_seg);
    mem_w16_seg(mem, idle_uda, u_initss, ds_sysdat);

    /* dx = offset idlepd (the real asm's own last-assigned bx), a
     * real, flat-equivalent PD address under this port's own
     * convention: SYSDAT itself is flat-mapped at ds_sysdat*16 */
    uint16_t idlepd_flat = (uint16_t)(ds_sysdat * 16 + SYSDAT_IDLEPD);
    uint16_t dx_after;
    (void)proc_creat(mem, idlepd_flat, ds_sysdat, &dx_after);
    /* init is now a process and can run concurrently with idle process */
}

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp;
} entry_result;

/* entry -- RTMIF.RTM's own inter-module RTM dispatch.
 *	input:	cl = function # (0-20, function[]'s own order);
 *		dx = whichever of this specific function's own real
 *		inputs it needs (most use dl or dx); ax/bx/cx/si/di/bp =
 *		ambient, passed straight through by default (the real
 *		asm leaves an untouched register alone); ds/sysdat =
 *		genuinely external context (see this file's own header
 *		comment)
 *	output:	*was_already_indisp is only meaningful after case 14
 *		(creat_proc_entry) -- see dsptch.c's own header comment
 */
entry_result entry(uint8_t *mem, uint8_t cl, uint16_t dx, uint16_t ax, uint16_t bx, uint16_t cx,
                    uint16_t si, uint16_t di, uint16_t bp, uint16_t ds, uint16_t sysdat,
                    uint8_t *was_already_indisp) {
    entry_result r = {ax, bx, cx, dx, si, di, bp};
    uint8_t dl = (uint8_t)dx;
    uint16_t cx_out = 0;
    switch (cl) {
    case 0: r.bx = sysreset_entry(mem); break;
    case 1: poll_entry(mem, dx); break;
    case 2: { entry_bxcx v = flag_wait_entry(mem, dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 3: { entry_bxcx v = flag_set_entry(mem, dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 4: { entry_bxcx v = makeq_entry(mem, dx, sysdat); r.bx = v.bx; r.cx = v.cx; break; }
    case 5: { entry_bxcx v = openq_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 6: { entry_bxcx v = deleteq_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 7: { entry_bxcx v = readq_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 8: { entry_bxcx v = creadq_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 9: { entry_bxcx v = writeq_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 10: { entry_bxcx v = cwriteq_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 11: delay_entry(mem, dx); break;
    case 12: dispatch_entry(); break; /* real jmp-pdisp tail, boundary stub */
    case 13: { entry_bxcx v = terminate_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 14: {
        switch_result v = creat_proc_entry(mem, dx, sysdat, ax, bx, cx, si, di, bp, was_already_indisp);
        r.ax = v.ax; r.bx = v.bx; r.cx = v.cx; r.dx = v.dx; r.si = v.si; r.di = v.di; r.bp = v.bp;
        break;
    }
    case 15: set_prior_entry(mem, dl); break;
    case 16: r.bx = pd_entry(mem, ds, &cx_out); r.cx = cx_out; break;
    case 17: { entry_bxcx v = abort_spec_entry(mem, dx, RTMIF_ABORT_TERMINATE_OFFSET); r.bx = v.bx; r.cx = v.cx; break; }
    case 18: sleep_entry(mem, dx); break;
    case 19: wakeup_entry(mem, dx); break;
    case 20: r.bx = findpdname_entry(mem, bx, &mem[dx], &cx_out); r.cx = cx_out; break;
    default: break;
    }
    return r;
}
