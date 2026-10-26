/*
 * init.c -- C port of src/kernel/INIT.SUP's `init` (system
 * initialization), named here `sup_init` -- every module's own real
 * asm label for this routine is just `init`; see bdosif.c's own
 * header comment for why the C ports need a module prefix instead
 * (this one collided with bdosif.c/cioif.c/memif.c/rtmif.c's own
 * `init` once this whole tree is linked into one real image).
 * `entry` (the module's own other real label, inter-module dispatch
 * into the Supervisor) is not ported here.
 *
 * Unlike the rest of src-c/kernel/, this routine runs against real
 * memory (defs.h's far_ptr()), not the 64KB harness array: `phys` is
 * the base of the real 8086 physical address space, and every field
 * is reached through its real segment at its real offset --
 *   - SYSDAT.FMT fields through the `ds_sysdat` segment (SYSDAT_*),
 *   - INIT.SUP's own `sysdat`/`supervisor` cells through `cs`
 *     (INIT_SYSDAT/INIT_SUPERVISOR_SEG),
 *   - the INT mpmint vector through segment 0 (i_mpm_ip/i_mpm_cs),
 *   - the init UDA and each RSP through the segments the real code
 *     computes or follows at run time.
 * So the real `p_uda` paragraph arithmetic, the `es:`-relative
 * u_wrkseg write, and the RSP-list walk are translated as the real
 * asm does them, and tests/src-c/test_init.py exercises all of them.
 *
 * `cs` is this module's real code segment; `notick`/`idle`/
 * `user_entry` are entry offsets in other SUP files, passed in since
 * nothing here can resolve them.
 *
 * Boundary stubs, same conventions as everywhere else in this port:
 *   - the real stack switch at the top (`mov ss,ax ! mov sp,offset
 *     init_tos`) has no C equivalent;
 *   - the five `callf dword ptr <rtm|mem|bdos|cio|xios>mod[bx]` module
 *     inits are cross-module far calls through SYSDAT's module table;
 *   - `call xiosif` (io_maxconsole/io_maxlist) is a near call into
 *     unported machinery. With it stubbed, `bl` is deterministically 0
 *     (the real code just did `sub bx,bx`), so the device counts are 0;
 *   - each RSP's `int mpmint` (f_createproc) and the final one
 *     (f_terminate) are system calls into unported machinery.
 *
 * Three real bugs in /src were found and fixed at this routine's own
 * sites (see INIT.SUP's own comments): bare `sysdat` reads/writes that
 * landed on SYSDAT's offset 6 instead of this module's `cs:sysdat`
 * cell, a bare `mov u_wrkseg,ds` that hit SYSDAT's offset 2Eh (the
 * XIOS module-table entry's init segment) instead of the init UDA's
 * own u_wrkseg, and `mov cl,f_createproc` leaving a stale CH. This
 * port follows the fixed asm.
 */
#include <stdint.h>

#include "defs.h"

/*
 * sup_init -- system initialization.
 *	input:	phys = base of real physical memory; ds_sysdat = DS at
 *		cold-boot entry (the real sysdat segment, per this
 *		routine's own "assume DS = Sysdat Segment" comment);
 *		cs = this module's own real code segment;
 *		notick_offset/idle_offset = TICK.SUP's own `notick`/
 *		IDLE.SUP's own `idle` entry offsets; user_entry_offset =
 *		SUPIF.SUP's own `user_entry` offset
 */
void sup_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs,
              uint16_t notick_offset, uint16_t idle_offset,
              uint16_t user_entry_offset) {
    uint8_t *sysdat = far_ptr(phys, ds_sysdat, 0);
    uint8_t *sup_cs = far_ptr(phys, cs, 0);
    uint8_t *ivt = far_ptr(phys, 0, 0);

    wr16(sup_cs + INIT_SYSDAT, ds_sysdat);

    /* set up supervisor entry point */
    wr16(sup_cs + INIT_SUPERVISOR_SEG, rd16(sysdat + SYSDAT_SUPMOD + 2));

    wr16(sysdat + SYSDAT_VERSION, INIT_VER);
    wr16(sysdat + SYSDAT_TICK_TOS, notick_offset);
    wr16(sysdat + SYSDAT_IDLE_TOS, idle_offset);

    /* set up mpm entry point */
    wr16(ivt + i_mpm_ip, user_entry_offset);
    wr16(ivt + i_mpm_cs, cs);

    /* initialize init uda: p_uda = inituda's own paragraph segment */
    uint16_t uda_seg = (uint16_t)((SYSDAT_INITUDA >> 4) + rd16(sup_cs + INIT_SYSDAT));
    wr16(sysdat + SYSDAT_INITPD + p_uda, uda_seg);
    wr16(far_ptr(phys, uda_seg, u_wrkseg), ds_sysdat);

    /* initialize modules -- 5 far-call boundary stubs */

    sysdat[SYSDAT_INTFLAG] = 0xFF;

    /* reset interrupt vectors */
    wr16(ivt + i_mpm_ip, user_entry_offset);
    wr16(ivt + i_mpm_cs, cs);

    /* get character dev info from XIOS -- xiosif stubbed, bl = 0 */
    sysdat[SYSDAT_NCONDEV] = 0;
    sysdat[SYSDAT_NCIODEV] = 0;
    sysdat[SYSDAT_NLSTDEV] = 0;

    /* start RSPs: walk the list, giving each RSP the sysdat segment */
    for (;;) {
        uint16_t ds = rd16(sup_cs + INIT_SYSDAT);
        uint8_t *sd = far_ptr(phys, ds, 0);
        uint16_t rspseg = rd16(sd + SYSDAT_RSPSEG);
        if (rspseg == 0) {
            break; /* rsp_out */
        }
        uint8_t *rsp = far_ptr(phys, rspseg, 0);
        wr16(sd + SYSDAT_RSPSEG, rd16(rsp + rsp_link));
        wr16(rsp + rsp_link, ds);
        /* int mpmint, f_createproc -- boundary stub */
    }

    /* terminate init process -- int mpmint, f_terminate, boundary stub */
}
