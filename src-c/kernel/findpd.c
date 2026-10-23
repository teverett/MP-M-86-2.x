/*
 * findpd.c -- C port of src/kernel/FINDPD.RTM.
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
 */
#include <stdint.h>

#include "defs.h"

/*
 * findpdname_entry -- Find Process Descriptor by Name
 *
 * find process by name in thread list
 *	input:	name -> 8-byte name, in mem[] (DX->name in u_wrkseg originally)
 *		bx   -> thread list root - p_thread
 *	output:	returns bx = pd if found
 *		          = 0xFFFF if not found
 *		*cx_out = 0 if found
 *		        = e_no_pdname if not found
 */
uint16_t findpdname_entry(uint8_t *mem, uint16_t bx, const uint8_t *name, uint16_t *cx_out) {
    uint16_t si, di;
    uint8_t cl;

fpn_cmpname:
    si = 0;
    bx = mem_r16(mem, (uint16_t)(bx + p_thread));
    if (bx == 0) goto fpn_nomatch;
    cl = 0;
    di = (uint16_t)(bx + p_name);
fpn_cmplet:
    if (cl == 8) goto fpn_found;
    {
        uint8_t al = (uint8_t)(name[si] - mem[di]);
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
