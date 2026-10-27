/*
 * toolchain_host.c -- the DDT86 services src-c/toolchain's overlays
 * (dis86.c, ass86.c) call, for their module tests (tests/src-c/
 * toolref.py), in place of ddt86.c's: each is an rt_int224 call with a
 * pseudo function number (TC_SVC_*, above any MP/M one) that the test's
 * Python handles -- the same Python that services the asm overlay's
 * calls to DDT86's vectors in i86sim, so both sides see one DDT86.
 * ddt_asment (goddt) is the handler stopping the run, as the asm side's
 * stops at the call.
 *
 * And the two entries' test mains: the test leaves the far pointer at
 * ds:TC_HOST_ARGS (offset, segment) and reads the result back from
 * ds:TC_HOST_ARGS+4.
 */
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "toolchain.h"

#define TC_SVC_CONIN   0xE0
#define TC_SVC_CONOUT  0xE1
#define TC_SVC_GETLINE 0xE2
#define TC_SVC_ASMENT  0xE3
#define TC_SVC_SET     0xE4  /* DX = the offset, ES = the byte */
#define TC_HOST_ARGS   0x9FF0

/* weak stand-ins: a module test builds one overlay, without the other
 * (whose real entry, when it's built in, overrides these) */
__attribute__((weak)) uint16_t dis86_disem(uint16_t ds, uint16_t off, uint16_t seg) {
    (void)ds, (void)seg;
    return off;
}
__attribute__((weak)) uint16_t ass86_asm(uint16_t ds, uint16_t off, uint16_t seg) {
    (void)ds, (void)seg;
    return off;
}

uint8_t ddt_conin(uint16_t ds) { return (uint8_t)rt_int224(ds, ds, TC_SVC_CONIN, 0).bx; }
void ddt_conout(uint16_t ds, uint8_t c) { (void)rt_int224(ds, ds, TC_SVC_CONOUT, c); }
void ddt_getline(uint16_t ds) { (void)rt_int224(ds, ds, TC_SVC_GETLINE, 0); }
void ddt_set(uint16_t ds, uint16_t off, uint8_t b) { (void)rt_int224(ds, b, TC_SVC_SET, off); }

void ddt_asment(uint16_t ds) {
    (void)rt_int224(ds, ds, TC_SVC_ASMENT, 0); /* the handler stops the run */
    for (;;) {
    }
}

void tc_disem_main(uint16_t ds) {
    uint8_t *a = far_ptr(rt_phys(), ds, TC_HOST_ARGS);
    wr16(a + 4, dis86_disem(ds, rd16(a), rd16(a + 2)));
}

void tc_asm_main(uint16_t ds) {
    uint8_t *a = far_ptr(rt_phys(), ds, TC_HOST_ARGS);
    wr16(a + 4, ass86_asm(ds, rd16(a), rd16(a + 2)));
}
