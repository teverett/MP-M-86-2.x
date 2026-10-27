/*
 * findpd.c -- C port of src-assem/kernel/FINDPD.RTM.
 *
 * Routine-by-routine port (see docs/src-c.md and the top-level plan):
 * same names, same control flow (gotos included), verified against the
 * real assembled bytes by differential testing (tests/src-c/test_findpd.py)
 * rather than by trying to match them byte-for-byte.
 *
 * The original's pushf!cli / popf wrapping (a critical section protecting
 * the thread-list walk from a timer/IO interrupt) has no C equivalent in
 * this single-threaded hosted port and is not modeled, same as CP-M-C's
 * own documented simplifications for interrupt-adjacent assembly idioms.
 *
 * Runs against real memory (defs.h's far_ptr), like the rest of the
 * RTM: PDs and the thread list are SYSDAT-relative, and the name being
 * looked up is at dx in the running process's u_wrkseg (the real asm's
 * `mov es,u_wrkseg` ... `es:[si]`), read from its UDA (p_uda[rlr]).
 */
#include <stdint.h>

#include "defs.h"

/*
 * findpdname_entry -- Find Process Descriptor by Name
 *
 * find process by name in thread list
 *	input:	dx   -> 8-byte name in u_wrkseg
 *		bx   -> thread list root - p_thread
 *	output:	returns bx = pd if found
 *		          = 0xFFFF if not found
 *		*cx_out = 0 if found
 *		        = e_no_pdname if not found
 */
uint16_t findpdname_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx, uint16_t *cx_out) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    const uint8_t *wseg = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t si, di;
    uint8_t cl;

fpn_cmpname:
    si = dx;
    bx = seg_r16(sd, (uint16_t)(bx + p_thread));
    if (bx == 0) goto fpn_nomatch;
    cl = 0;
    di = (uint16_t)(bx + p_name);
fpn_cmplet:
    if (cl == 8) goto fpn_found;
    {
        uint8_t al = (uint8_t)(seg_r8(wseg, si) - seg_r8(sd, di));
        al = (uint8_t)(al << 1); /* drops bit 7 of the difference, same as the
                                  * original's `shl al,1` -- a byte differing
                                  * only in its top bit still counts as a
                                  * match, intentionally, not a bug to fix */
        if (al != 0) goto fpn_cmpname;
        cl++;
        si++;
        di++;
        goto fpn_cmplet;
    }
fpn_found:
    *cx_out = 0;
    return bx;
fpn_nomatch:
    *cx_out = e_no_pdname;
    return 0xFFFF;
}
