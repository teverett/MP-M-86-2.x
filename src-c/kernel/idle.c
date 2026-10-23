/*
 * idle.c -- C port of src/kernel/IDLE.SUP's `idle:` routine.
 *
 * `idle:` never returns in the real system: it tail-jumps into the XIOS
 * module's own idle handler (`jmp xiosif`), which is not ported. Per
 * docs/src-c.md's boundary-stub convention (the same idea as CP-M-C's own
 * `cpm_bios` struct), this port's "return value" is the register state the
 * real routine would have handed to that jump -- the test
 * (tests/src-c/test_idle.py) checks that, not a control transfer.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t ds; /* mov ds,cs:sysdat */
    uint16_t es; /* mov es,p_uda[bx] */
    uint16_t ax; /* mov ax,io_idle */
} idle_result;

/*
 * idle -- Idle Process
 * Jump to the XIOS idle routine.
 *
 *	input:	(none)
 *	output:	ds, es, ax as handed to `jmp xiosif`
 */
idle_result idle(const uint8_t *mem, uint16_t sysdat) {
    idle_result r;
    r.ds = sysdat;               /* mov ds,cs:sysdat */
    uint16_t bx = mem_r16(mem, SD_RLR);
    r.es = mem_r16(mem, (uint16_t)(bx + p_uda));
    r.ax = io_idle;
    return r;
}
