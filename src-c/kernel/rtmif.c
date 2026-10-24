/*
 * rtmif.c -- C port of src/kernel/RTMIF.RTM's `init` (module
 * initialization). `entry` (the module's own inter-module RTM
 * dispatch, `call cs:function[si]`) is not ported here -- same
 * reasoning as `CIOIF.CIO`'s own `entry` (see cioif.c's own header
 * comment): every real caller that needs one of `entry`'s own
 * function-table targets already calls the already-ported routine
 * directly (`que1.c`'s `readq_entry`/`writeq_entry`, `sysent.c`'s
 * `creat_proc_entry`, etc.), so nothing needs to call *through*
 * `entry` to reach them.
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
