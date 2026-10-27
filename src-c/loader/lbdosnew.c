/*
 * lbdosnew.c -- C port of src-assem/loader/LBDOSNEW.A86: func51 (set segment
 * base for disk I/O).
 *
 * func50/func52/func53/func54/func55/func56/func57 are each just a
 * separate entry point for the bare `ret` at the very end of this file
 * (func51's own real tail falls through all of their labels to reach
 * the same `ret`, but none of them has any code of its own) -- nothing
 * to port, same reasoning as src-assem/loader/LBDOSIO.A86's own
 * func1/func3/func6/func7/func8/func10/func11 (src-c/loader/lbdosio.c).
 */
#include <stdint.h>

#include "defs.h"

/* func51 -- set segment base for disk I/O */
void func51(uint8_t *mem, uint16_t dx) {
    mem_w16(mem, LD_DMABASE, dx);
}
