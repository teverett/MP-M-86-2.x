/*
 * rt_host.c -- src-c/rt's interface (rt.h) for running the C programs
 * in src-c/rsp/ on the host, in tests/src-c/rspref.py: real memory is
 * the test's buffer, and each `int 224` (rt_int224) goes to the test's
 * handler, which returns BX, CX and ES -- or asks to stop, which ends
 * the program (a longjmp back to rt_host_run). rt_puts output is kept
 * for the test to read.
 */
#include <setjmp.h>
#include <stdint.h>

#include "rt.h"

/* the test's int 224: out = {bx, cx, es}, ES preset; nonzero = stop */
typedef int (*rt_host_fn)(uint16_t ds, uint16_t es, uint8_t cl, uint16_t dx, uint16_t *out);

static uint8_t *host_phys;
static uint16_t host_sysdat;
static rt_host_fn host_handler;
static jmp_buf host_stop;
static char host_log[4096];
static unsigned host_log_len;

rt_xios_fn rt_xiosmod;

uint8_t *rt_phys(void) { return host_phys; }
uint16_t rt_sysdat(void) { return host_sysdat; }

mpm_regs rt_int224(uint16_t ds, uint16_t es, uint8_t cl, uint16_t dx) {
    uint16_t out[3] = {0, 0, es};
    if (host_handler(ds, es, cl, dx, out)) longjmp(host_stop, 1);
    return (mpm_regs){out[0], out[0], out[1], dx, out[2]};
}

void rt_puts(const char *s) {
    for (; *s && host_log_len < sizeof host_log - 1; s++) host_log[host_log_len++] = *s;
    host_log[host_log_len] = 0;
}

void rt_puthex(uint16_t v) {
    char b[5];
    for (int i = 0; i < 4; i++) b[i] = "0123456789ABCDEF"[(v >> (12 - 4 * i)) & 0xF];
    b[4] = 0;
    rt_puts(b);
}

/* run main(ds) until it returns (0) or the handler stops it (1) */
int rt_host_run(uint8_t *phys, uint16_t sysdat, rt_host_fn handler, void (*main)(uint16_t), uint16_t ds) {
    host_phys = phys;
    host_sysdat = sysdat;
    host_handler = handler;
    host_log_len = 0;
    host_log[0] = 0;
    if (setjmp(host_stop)) return 1;
    main(ds);
    return 0;
}

const char *rt_host_log(void) { return host_log; }
