/*
 * init.c -- C port of src/kernel/INIT.SUP's `init` (system
 * initialization). `entry` (the module's own other real label,
 * inter-module BDOS-style dispatch into the Supervisor) is not
 * ported here -- it's genuinely a different routine, not reconned
 * in this pass.
 *
 * `init` never returns in the normal sense: it ends by starting every
 * RSP (`int mpmint`, `f_createproc`, once per linked-list entry) and
 * then terminating itself (`int mpmint`, `f_terminate`) -- both
 * already-supported `int_stub_nums` boundary-stub no-ops (same
 * convention `src/rsp/CLOCK.A86` already uses for its own `int
 * mpmint`). The real stack switch at the very top (`mov ss,ax ! mov
 * sp,offset init_tos`) has no C equivalent, same as every other real
 * OS entry point in this port.
 *
 * The five `callf dword ptr <rtm|mem|bdos|cio|xios>mod[bx]` calls
 * (far-calling into each of those modules' own `init:` entry point,
 * through SYSDAT.FMT's module table) are boundary-stub no-ops too,
 * needing `tools/i86sim.py`'s new `far_call_stub_addrs` (an indirect
 * far CALL's own *target* is only known at runtime -- loaded from the
 * module table -- so it can't be stubbed by target address the way
 * an ordinary CALL is). `call xiosif` (SUPIF.SUP's own, the same real
 * SUP module INIT.SUP is itself part of, per src/kernel/SUP.A86's own
 * include order) is an ordinary near-CALL boundary stub, the usual
 * `call_stub_addrs` convention -- `io_maxconsole`/`io_maxlist`'s own
 * real `bl` results are consequently undefined artifacts; fixed at 0
 * here, same choice as `selectdisk`'s own fixed `bx=0` (`file1.c`).
 *
 * `sysdat`/`supervisor`/`ver` are INIT.SUP's own real cseg cells
 * (`INIT_SYSDAT`/`INIT_SUPERVISOR_SEG`, `defs.h`'s own header comment
 * has the detail on why these are real resolved offsets, not harness
 * scratch addresses, same reasoning as the BD_* block in
 * `file1.c`/`file2.c`/etc.); `notick`/`idle`/`offset user_entry`/
 * `cs`/`ds` (at cold-boot entry, "assume DS = Sysdat Segment") are
 * all genuinely external or ambient values nothing in this file can
 * resolve on its own, so they're parameters instead of guessed
 * constants.
 *
 * The init process's own PD/UDA segment arithmetic (`shr ax,4 ! add
 * ax,sysdat`, computing `inituda`'s own real paragraph segment to
 * store into `p_uda`) has no consumer anywhere in this port -- no
 * process-dispatch/scheduling code here ever follows a PD's own
 * `p_uda` field to locate its UDA -- so it's not modeled as real
 * segment arithmetic; `u_wrkseg` (the one real field this
 * computation's own result, `es`, was headed toward) is written as a
 * flat offset into `SD_INITUDA` instead, the same "ambient flat"
 * simplification used throughout this port for segment-relative
 * addressing whose own exact value isn't independently meaningful
 * (e.g. `selectdisk`'s own fixed `bx=0`). This was also the site of a
 * real, previously-uncaught bug in `/src` (now fixed): a bare `mov
 * u_wrkseg,ds` (no `es:` prefix) landed on `SYSDAT`'s own offset 2Eh
 * instead -- which happens to be the back half of the module table's
 * own XIOS entry (its "initialize" far pointer's segment word),
 * right before the `xiosmod` `callf` a few lines later would have
 * used it -- confirmed by offset arithmetic against `MODTAB.DEF`
 * (`xiosmod`'s own base + `mod_init` + 2 = 2Eh) and `UDA.FMT`
 * (`u_wrkseg`=2Eh exactly), same bug class as two other bare-`sysdat`
 * writes this exact file had already caught and fixed.
 *
 * The RSP-creation loop (walking `SD_RSPSEG`'s own linked list via
 * each RSP's own `rsp_link` field) *is* modeled faithfully, via
 * `mem_r16_seg`/`mem_w16_seg` (the segment-relative-addressing
 * convention already established for `ml.c`/`proc.c`/`mau.c`/
 * `share.c`/`load.c`) -- real logic worth getting right even though
 * the `int mpmint` it drives is itself a no-op under the convention.
 *
 * Neither the `u_wrkseg` fix above nor this RSP loop is independently
 * exercised by `tests/src-c/test_init.py`, though: both go through a
 * real `es:`-prefixed access, and `tools/i86sim.py` only computes a
 * real segment:offset address for one when `segmented=True` --
 * which would also turn every *other* field this file touches
 * (version/tick_tos/etc, all plain bare instructions defaulting to
 * DS) from flat scratch addresses into real `ds*16+offset`
 * computations, breaking them instead. Translated correctly either
 * way (via `mem_r16_seg`/`mem_w16_seg`), just not independently
 * verified by this particular test -- same "translated faithfully
 * but not exercised" precedent as `QUE1.RTM`'s own retry loops.
 */
#include <stdint.h>

#include "defs.h"

/*
 * init -- system initialization.
 *	input:	ds_sysdat = DS at cold-boot entry (the real sysdat
 *		segment, per this routine's own "assume DS = Sysdat
 *		Segment" comment); cs = this module's own real code
 *		segment; notick_offset/idle_offset = TICK.SUP's own
 *		`notick`/IDLE.SUP's own `idle` entry offsets;
 *		user_entry_offset = SUPIF.SUP's own `user_entry` offset
 */
void init(uint8_t *mem, uint16_t ds_sysdat, uint16_t cs,
          uint16_t notick_offset, uint16_t idle_offset,
          uint16_t user_entry_offset) {
    mem_w16(mem, INIT_SYSDAT, ds_sysdat);

    /* set up supervisor entry point -- supervisor's own segment half
     * is copied from SYSDAT's own module table (SUP's own "entry"
     * far pointer's segment word, SD_SUPMOD_SEG standing in for it) */
    mem_w16(mem, INIT_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));

    mem_w16(mem, SD_INIT_VERSION, 0x0037); /* offset ver -- INIT.SUP's own cseg cell */
    mem_w16(mem, SD_INIT_TICK_TOS, notick_offset);
    mem_w16(mem, SD_INIT_IDLE_TOS, idle_offset);

    /* set up mpm entry point -- a real absolute (segment-0) write to
     * the interrupt vector table; see this file's own header comment
     * for why it's genuinely consumed elsewhere (proc.c) despite
     * being a no-op under this port's own int_stub_nums convention */
    mem_w16(mem, i_mpm_ip, user_entry_offset);
    mem_w16(mem, i_mpm_cs, cs);

    /* initialize init uda -- see this file's own header comment for
     * why the PD/UDA segment arithmetic itself isn't modeled, only
     * the one real field (u_wrkseg) it was headed toward, as a flat
     * write */
    mem[(uint16_t)(SD_INITPD + p_uda)] = 0; /* unconsumed; see above */
    mem[(uint16_t)(SD_INITPD + p_uda + 1)] = 0;
    mem_w16(mem, (uint16_t)(SD_INITUDA + u_wrkseg), ds_sysdat); /* bug fix applied, see header comment */

    /* initialize modules -- 5 far-call boundary stubs, see this
     * file's own header comment */

    mem[SD_INIT_INTFLAG] = 0xFF;

    /* reset interrupt vectors -- same write, same values, as above */
    mem_w16(mem, i_mpm_ip, user_entry_offset);
    mem_w16(mem, i_mpm_cs, cs);

    /* get character dev info from XIOS -- xiosif is a boundary
     * stub (same-module near CALL into SUPIF.SUP's own, not yet
     * ported), so its own real `bl` result is an undefined
     * artifact, fixed at 0 here (same convention as selectdisk's
     * own fixed bx=0, file1.c) */
    mem[SD_NCONDEV] = 0; /* bl, undefined artifact */
    mem[SD_NCIODEV] = 0; /* = bl, then += bl (both calls' own bl=0) */
    mem[SD_NLSTDEV] = 0; /* bl, undefined artifact */

    /* start RSPs -- walk SD_RSPSEG's own linked list, creating one
     * process per entry (int mpmint/f_createproc, a no-op under the
     * convention) */
    for (;;) {
        uint16_t rspseg = mem_r16(mem, SD_RSPSEG);
        if (rspseg == 0) {
            break; /* rsp_out */
        }
        uint16_t next = mem_r16_seg(mem, rspseg, rsp_link);
        mem_w16(mem, SD_RSPSEG, next);
        mem_w16_seg(mem, rspseg, rsp_link, ds_sysdat); /* give sysdat to the RSP */
        /* mov si,rsp_pd ! mov cx,f_createproc ! mov dx,si ! int mpmint -- no-op */
    }

    /* terminate init process -- int mpmint/f_terminate, no-op */
}
