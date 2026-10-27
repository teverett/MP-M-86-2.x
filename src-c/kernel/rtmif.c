/*
 * rtmif.c -- C port of src-assem/kernel/RTMIF.RTM's `init` AND now `entry`,
 * named here `rtmif_init`/`rtmif_entry` -- every module's own real
 * asm label for these two routines is just `init`/`entry`; see
 * bdosif.c's own header comment for why the C ports need a module
 * prefix instead.
 *
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
 * list-root context); its `dx` is the name's offset in u_wrkseg.
 * `abort_spec_entry` (case 17) needs `RTMIF_ABORT_TERMINATE_OFFSET`
 * -- not a per-call register at all, but a real, fixed address
 * within this same combined module (`ABORT.RTM`'s own `terminate:`
 * label, probed via `--symbols` against `src-assem/kernel/RTM.A86`, same
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
 * `init` (rtmif_init) runs against real memory, the same way init.c's
 * sup_init does (defs.h's far_ptr): `phys` is the base of the real
 * 8086 physical address space, and every field is reached through its
 * real segment at its real offset. `entry` and every target it
 * dispatches to run on real memory too. `init`:
 *
 *   1. Saves `ds_sysdat` in this module's own `cs:sysdat` cell and
 *      copies SYSDAT's module_table[SUP] entry far pointer into
 *      `cs:supervisor` (same shape as CIOIF.CIO/MEMIF.MEM's `init`).
 *
 *   2. Points SYSDAT's `dispatcher`/`rtm_pdisp` far pointers at
 *      DSPTCH.RTM's own `fdisp`/`farpdisp` in this same `cs`. Those
 *      offsets are RTM-internal labels nothing here can resolve, so
 *      they're parameters (same as init.c's notick/idle/user_entry).
 *
 *   3. Sets up the tick and idle processes' UDAs. Each PD's `p_uda` is
 *      a paragraph offset relative to SYSDAT (GENSYS's stock template),
 *      so the real UDA segment is `p_uda + cs:sysdat`. That segment is
 *      only used to reach the UDA, never stored back into `p_uda`.
 *      Each UDA gets `u_initcs` = the Supervisor's segment (both
 *      processes first run through SUPIF.SUP's `user_entry`) and
 *      `u_initss` = SYSDAT's segment, the already-fixed /src-assem bug the
 *      real asm's own comment documents.
 *
 *   4. `mov dx,offset idlepd ! call proc_creat` -- a real call into
 *      proc.c's proc_creat (also on real memory), which drains the
 *      pending-creation chain starting at idlepd. The real asm discards
 *      its result (`retf` follows immediately).
 *
 * No `callf`/far CALL anywhere in `init`.
 */
#include <stdint.h>

#include "defs.h"

extern uint16_t proc_creat(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *dx_out);

/* ABORT.RTM's own real `terminate:` label address within the combined
 * RTM.A86 build -- see this file's own header comment. Probed via
 * `python3 tools/asm86.py src-assem/kernel/RTM.A86 -I src-assem/kernel --symbols`;
 * re-probe if that build's own encoding ever shifts. */
#define RTMIF_ABORT_TERMINATE_OFFSET 0x0C76

typedef struct {
    uint16_t bx, cx;
} entry_bxcx;

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp;
    uint16_t new_ss, new_sp;
    uint8_t went_to_int_disp_exit;
} switch_result;

extern uint16_t sysreset_entry(uint8_t *phys, uint16_t sysdat);
extern void poll_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx flag_wait_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl);
extern entry_bxcx flag_set_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl);
extern entry_bxcx makeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx openq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx deleteq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx readq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx creadq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx writeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_bxcx cwriteq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern void delay_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern void dispatch_entry(void);
extern entry_bxcx terminate_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern switch_result creat_proc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx,
                                       uint16_t ax, uint16_t bx, uint16_t cx,
                                       uint16_t si, uint16_t di, uint16_t bp,
                                       uint8_t *was_already_indisp);
extern void set_prior_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl);
extern uint16_t pd_entry(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint16_t *cx_out);
extern entry_bxcx abort_spec_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t terminate_offset);
extern void sleep_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern void wakeup_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern uint16_t findpdname_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx, uint16_t *cx_out);

/* set up one process's UDA: its PD's p_uda is relative to SYSDAT */
static void init_uda(uint8_t *phys, uint8_t *rtm_cs, uint8_t *sysdat, uint16_t pd) {
    uint16_t uda_seg = (uint16_t)(rd16(sysdat + pd + p_uda) + rd16(rtm_cs + RTMIF_SYSDAT));
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    wr16(uda + u_initcs, rd16(rtm_cs + RTMIF_SUPERVISOR_SEG));
    wr16(uda + u_initss, rd16(rtm_cs + RTMIF_SYSDAT));
}

/* rtmif_init -- RTM module initialization; see this file's header.
 *	input:	phys = base of real physical memory; ds_sysdat = DS at
 *		entry (the real sysdat segment); cs = this module's own
 *		real code segment; fdisp_offset/farpdisp_offset =
 *		DSPTCH.RTM's own `fdisp`/`farpdisp` offsets in that cs
 */
void rtmif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs,
                uint16_t fdisp_offset, uint16_t farpdisp_offset) {
    uint8_t *sysdat = far_ptr(phys, ds_sysdat, 0);
    uint8_t *rtm_cs = far_ptr(phys, cs, 0);

    wr16(rtm_cs + RTMIF_SYSDAT, ds_sysdat);
    wr16(rtm_cs + RTMIF_SUPERVISOR_OFF, rd16(sysdat + SYSDAT_SUPMOD));
    wr16(rtm_cs + RTMIF_SUPERVISOR_SEG, rd16(sysdat + SYSDAT_SUPMOD + 2));

    /* init interrupt pdisp / intermodule pdisp */
    wr16(sysdat + SYSDAT_DISPATCHER, fdisp_offset);
    wr16(sysdat + SYSDAT_DISPATCHER + 2, cs);
    wr16(sysdat + SYSDAT_RTM_PDISP, farpdisp_offset);
    wr16(sysdat + SYSDAT_RTM_PDISP + 2, cs);

    /* create tick and idle process */
    init_uda(phys, rtm_cs, sysdat, SYSDAT_TICKPD);
    init_uda(phys, rtm_cs, sysdat, SYSDAT_IDLEPD);

    /* mov dx,offset idlepd ! call proc_creat -- drains the tick/idle
     * creation chain; the real asm discards the result (retf) */
    (void)proc_creat(phys, ds_sysdat, SYSDAT_IDLEPD, 0);
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
 *
 * Every target runs on real memory (see this file's header): `phys` is
 * the base of physical memory and `sysdat` the real SYSDAT segment.
 */
entry_result rtmif_entry(uint8_t *phys, uint8_t cl, uint16_t dx, uint16_t ax, uint16_t bx, uint16_t cx,
                    uint16_t si, uint16_t di, uint16_t bp, uint16_t ds, uint16_t sysdat,
                    uint8_t *was_already_indisp) {
    entry_result r = {ax, bx, cx, dx, si, di, bp};
    uint8_t dl = (uint8_t)dx;
    uint16_t cx_out = 0;
    switch (cl) {
    case 0: r.bx = sysreset_entry(phys, sysdat); break;
    case 1: poll_entry(phys, sysdat, dx); break;
    case 2: { entry_bxcx v = flag_wait_entry(phys, sysdat, dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 3: { entry_bxcx v = flag_set_entry(phys, sysdat, dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 4: { entry_bxcx v = makeq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 5: { entry_bxcx v = openq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 6: { entry_bxcx v = deleteq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 7: { entry_bxcx v = readq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 8: { entry_bxcx v = creadq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 9: { entry_bxcx v = writeq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 10: { entry_bxcx v = cwriteq_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 11: delay_entry(phys, sysdat, dx); break;
    case 12: dispatch_entry(); break; /* real jmp-pdisp tail, boundary stub */
    case 13: { entry_bxcx v = terminate_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 14: {
        switch_result v = creat_proc_entry(phys, sysdat, dx, ax, bx, cx, si, di, bp, was_already_indisp);
        r.ax = v.ax; r.bx = v.bx; r.cx = v.cx; r.dx = v.dx; r.si = v.si; r.di = v.di; r.bp = v.bp;
        break;
    }
    case 15: set_prior_entry(phys, sysdat, dl); break;
    case 16: r.bx = pd_entry(phys, sysdat, ds, &cx_out); r.cx = cx_out; break;
    case 17: { entry_bxcx v = abort_spec_entry(phys, sysdat, dx, RTMIF_ABORT_TERMINATE_OFFSET); r.bx = v.bx; r.cx = v.cx; break; }
    case 18: sleep_entry(phys, sysdat, dx); break;
    case 19: wakeup_entry(phys, sysdat, dx); break;
    case 20: r.bx = findpdname_entry(phys, sysdat, bx, dx, &cx_out); r.cx = cx_out; break;
    default: break;
    }
    return r;
}
