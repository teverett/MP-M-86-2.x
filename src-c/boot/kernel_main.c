/*
 * kernel_main.c -- the one piece of this stub written in C, proving
 * i686-elf-gcc's own output actually links (via linker.ld) and runs.
 * Not a port of anything -- see boot.s's own header comment for what
 * this directory is and isn't.
 *
 * Writes a fixed signon string out COM1 (0x3F8), the standard way a
 * freestanding/`-nostdlib` kernel gets observable output under QEMU
 * without needing a VGA framebuffer or any BIOS calls (both
 * unavailable once a multiboot bootloader has already dropped us
 * into 32-bit protected mode) -- `tests/src-c/test_boot.py` reads it
 * back via QEMU's own `-serial file:...`.
 */
#include <stdint.h>

#define COM1_PORT 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); /* disable interrupts */
    outb(COM1_PORT + 3, 0x80); /* enable DLAB (set baud rate divisor) */
    outb(COM1_PORT + 0, 0x03); /* divisor low byte: 38400 baud */
    outb(COM1_PORT + 1, 0x00); /* divisor high byte */
    outb(COM1_PORT + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7); /* enable FIFO, clear, 14-byte threshold */
}

static void serial_putc(char c) {
    while ((inb(COM1_PORT + 5) & 0x20) == 0) {
        /* wait for the transmit holding register to empty */
    }
    outb(COM1_PORT, (uint8_t)c);
}

static void serial_puts(const char *s) {
    while (*s) {
        serial_putc(*s++);
    }
}

void kernel_main(void) {
    serial_init();
    serial_puts("MP/M-86 src-c image-builder: pipeline ok\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
