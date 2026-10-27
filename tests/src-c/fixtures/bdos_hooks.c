/*
 * bdos_hooks.c -- test doubles for the BDOS's two exits (defs.h's
 * bdos_mpmif/bdos_xiosif), the C side of tests/src-c/bdosref.py's
 * far_call_hooks: mpmif returns at once with the registers as they were
 * (as the reference's stub does), xiosif returns AX = BX = 0. Each call
 * is logged as (kind << 16) | value -- 'm' with CX, 'x' with AL -- the
 * same sequence BdosRef.calls records.
 */
#include <stdint.h>

#include "defs.h"

#define NLOG 256
static uint32_t calls[NLOG];
static int ncalls;

static mpm_regs test_mpmif(uint8_t *phys, uint16_t sysdat, uint16_t cx, uint16_t dx, uint16_t bx, uint16_t ds) {
    (void)phys;
    (void)sysdat;
    (void)ds;
    if (ncalls < NLOG) calls[ncalls++] = ((uint32_t)'m' << 16) | cx;
    return (mpm_regs){bx, bx, cx, dx, 0};
}

static uint16_t test_xiosif(uint8_t *phys, uint16_t sysdat, uint8_t al, uint16_t cx, uint16_t dx) {
    (void)phys;
    (void)sysdat;
    (void)cx;
    (void)dx;
    if (ncalls < NLOG) calls[ncalls++] = ((uint32_t)'x' << 16) | al;
    return 0;
}

/* install the doubles and clear the log */
void bdos_test_install(void) {
    bdos_set_mpmif(test_mpmif);
    bdos_set_xiosif(test_xiosif);
    ncalls = 0;
}

/* the log so far: copies up to max entries to out, returns the count */
int bdos_test_calls(uint32_t *out, int max) {
    for (int i = 0; i < ncalls && i < max; i++) out[i] = calls[i];
    return ncalls;
}
