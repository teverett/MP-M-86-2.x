/*
 * kernel_main.c -- entered from boot.s's `_start`; hands the machine
 * to the real ported XIOS (src-c/kernel/xios86q.c, the C port of
 * XIOS86Q.A86) instead of driving any hardware itself.
 *
 * XIOS86Q is the one ported subsystem whose interface already fits
 * real hardware as-is: all of its port I/O goes through the
 * `io_in8`/`io_out8` callbacks, so the real `inb`/`outb` below
 * satisfy it directly. Its `mem` parameter only covers three
 * harness-scratch fields (XQ_TICK_COUNT, XQ_CLOCKON, and the signon
 * text at XQ_SIGNON), so a static buffer here stands in for them.
 * Everything above the XIOS (sup_init() and the rest) still uses the
 * simulated flat-array `mem`+offset convention and isn't called yet.
 *
 * init_entry() arms the UART/PIT/PIC/FDC and prints XIOS86Q's own
 * signon; the pipeline signon after it goes out through the real
 * xios_conout_entry(). tests/src-c/test_boot.py checks for both.
 */
#include <stdint.h>

#define XQ_SIGNON 0x134A /* src-c/kernel/defs.h */
#define XQ_MEM_SIZE 0x2000

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

extern void init_entry(uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8);
extern void xios_conout_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl);

static uint8_t xq_mem[XQ_MEM_SIZE];

static const char xq_signon[] =
    "\r\nMP/M-86 XIOS86Q -- QEMU target (serial console, floppy disk)\r\n";

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void kernel_main(void) {
    for (unsigned i = 0; i < sizeof xq_signon; i++) {
        xq_mem[XQ_SIGNON + i] = (uint8_t)xq_signon[i];
    }
    init_entry(xq_mem, inb, outb);

    for (const char *s = "MP/M-86 src-c image-builder: pipeline ok\r\n"; *s; s++) {
        xios_conout_entry(inb, outb, (uint8_t)*s);
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}
