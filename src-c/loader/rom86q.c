/*
 * rom86q.c -- C port of the whole of src-assem/loader/ROM86Q.A86, the QEMU
 * PC boot sector: `start:` (ported here as `start_entry`), plus its
 * internal subroutines `advance_chs`, `read_sector`, `pmsg`, `putc`.
 *
 * This is the first file in this port needing a real BIOS service
 * call (`INT 13h`, the disk read) rather than a boundary stub or a
 * raw hardware port -- backed by a new `int13_fn` callback parameter,
 * matching tools/i86sim.py's own new `int_hooks` mechanism (see that
 * module's own docstring). tests/src-c/fixtures/bootdisk.py's
 * `BootDisk` is the shared test-only synthetic floppy image backing
 * both sides, same "one real model, two call sites" shape as
 * `uart86q.py`'s own `Uart86Q`.
 *
 * Neither the header sector's own bytes nor any body sector's own
 * bytes are modeled as landing anywhere in a flat `mem` array the
 * way `src-c/loader/ldbios86q.c`'s own READ/WRITE at least gesture
 * at (via `mem_r8_seg`/`mem_w8_seg`) -- there's no consumer anywhere
 * in this port for either one (nothing here parses MPMLDR.bin's own
 * bytes once loaded, same as nothing here boots a real floppy
 * image), so `int13_fn` hands the 512 just-read bytes straight back
 * through an output parameter instead, and `start_entry` only ever
 * looks at the one 5-byte prefix it actually needs (the CMD header's
 * own Len/Abs fields) before discarding the rest -- same "not
 * modeled, nothing reads it back" scope `src-c/loader/ldbios86q.c`'s
 * own header comment uses for the real DMA transfer READ/WRITE kick
 * off. Consequently this file needs no `mem` parameter at all.
 *
 * The real stack switch (`mov sp,7c00h`) and the final `jmpf dword
 * ptr leap_off` (the boot sector's own real exit into MPMLDR.bin,
 * CS:IP = entry_seg:0000) have no C equivalent, same as every other
 * real OS entry point in this port -- `start_entry` returns 0 and
 * the resolved `entry_seg` through an output parameter in place of
 * that jump, for the test to check.
 *
 * `read_sector`'s own retry loop (`bp=4`, with a disk-system reset
 * between attempts) is translated as a real bounded loop, exercised
 * for real by tests/src-c/test_rom86q.py (not just "translated
 * faithfully, unexercised" -- `BootDisk` can report failure on
 * demand).
 */
#include <stdint.h>

#define spt 18

#define uart_rxtx 0x3F8
#define uart_ier  0x3F9
#define uart_lcr  0x3FB
#define uart_mcr  0x3FC
#define uart_lsr  0x3FD
#define uart_lsr_txrdy 0x20
#define uart_divisor   0x000C

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

/* int13_fn -- stands in for a real BIOS `INT 13h` AH=2 (read sector)/
 * AH=0 (reset) call: `sector_out` is always filled with the 512
 * bytes a real read would deposit at ES:BX (ignored by the caller
 * for AH=0). Returns cf (0 = success, 1 = failure). */
typedef uint8_t (*int13_fn)(uint8_t ah, uint8_t al, uint8_t ch, uint8_t cl,
                            uint8_t dh, uint8_t dl, uint8_t *sector_out);

static void putc_(uint8_t ch, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t al;
    do {
        /* putc_wait */
        al = io_in8(uart_lsr);
        al = (uint8_t)(al & uart_lsr_txrdy);
    } while (al == 0);
    io_out8(uart_rxtx, ch);
}

static void pmsg(const char *s, io_in8_fn io_in8, io_out8_fn io_out8) {
    for (; *s != '\0'; s++) {
        putc_((uint8_t)*s, io_in8, io_out8);
    }
}

/* advance_chs -- advance cur_cyl/cur_head/cur_sector by one physical
 * sector (2 heads/cylinder, `spt` sectors/track). */
static void advance_chs(uint8_t *cyl, uint8_t *head, uint8_t *sector) {
    uint8_t s = (uint8_t)(*sector + 1);
    if (s > spt) {
        s = 1;
        uint8_t h = (uint8_t)(*head ^ 1);
        *head = h;
        if (h == 0) {
            *cyl = (uint8_t)(*cyl + 1);
        }
    }
    *sector = s;
}

/* read_sector -- read one physical sector (CHS/drive given), into
 * `data_out` (always a full 512 bytes, see this file's own header
 * comment), retrying up to 4 times with a disk-system reset between
 * attempts.
 *	output:	returns 0 on success, 1 on final failure (cf)
 */
static int read_sector(uint8_t cyl, uint8_t head, uint8_t sector, uint8_t bootdrv,
                        int13_fn int13, uint8_t *data_out) {
    for (int attempt = 4; attempt > 0; attempt--) {
        uint8_t cf = int13(0x02, 0x01, cyl, sector, head, bootdrv, data_out);
        if (cf == 0) {
            return 0;
        }
        if (attempt == 1) {
            return 1;
        }
        /* reset disk system before retrying -- the real asm only
         * reloads AH and DL here ("xor ah,ah ! mov dl,bootdrv"), so
         * AL/CH/CL/DH are genuinely whatever the failed read attempt
         * just above left them as (0x01/cyl/sector/head here) --
         * translated faithfully even though the reset's own result
         * ignores them regardless */
        uint8_t dummy[512];
        (void)int13(0x00, 0x01, cyl, sector, head, bootdrv, dummy);
    }
    return 1;
}

/* start_entry -- start: arm the UART, read MPMLDR.bin's own 9-byte
 * CMD header (physical sector 2) then its body (sector 3 onward,
 * into `load_seg`, advancing by 32 paragraphs/sector), printing a
 * '.' progress dot per sector. See this file's own header comment
 * for what's not ported (the final `jmpf`).
 *	input:	bootdrv = BIOS boot drive number (DL at entry)
 *	output:	returns 0 and sets *entry_seg_out on success, 1 (boot
 *		failed) otherwise
 */
int start_entry(uint8_t bootdrv, int13_fn int13, io_in8_fn io_in8, io_out8_fn io_out8,
                 uint16_t *entry_seg_out) {
    io_out8(uart_lcr, 0x80);
    io_out8(uart_rxtx, (uint8_t)(uart_divisor & 0xFF));
    io_out8(uart_ier, (uint8_t)(uart_divisor >> 8));
    io_out8(uart_lcr, 0x03);
    io_out8(uart_ier, 0x00);
    io_out8(uart_mcr, 0x03);

    pmsg("\r\nMPMLDR boot\r\n", io_in8, io_out8);

    /* read the 9-byte header from physical sector 2 */
    uint8_t cyl = 0, head = 0, sector = 2;
    uint8_t hdr[512];
    if (read_sector(cyl, head, sector, bootdrv, int13, hdr) != 0) {
        pmsg("\r\nBoot failed\r\n", io_in8, io_out8);
        return 1;
    }

    uint16_t body_len = (uint16_t)(hdr[1] | (hdr[2] << 8));   /* Len */
    uint16_t load_seg = (uint16_t)(hdr[3] | (hdr[4] << 8));   /* Abs */
    uint16_t entry_seg = load_seg;

    /* advance to physical sector 3 (right after the header) and
     * compute how many more sectors the body needs */
    advance_chs(&cyl, &head, &sector);
    uint16_t sector_count = (uint16_t)((body_len + 511) >> 9);  /* ceil(len/512) */

    /* read the body, one physical sector at a time, into
     * load_seg:0000, load_seg advancing by 32 (512/16) each sector */
    uint8_t body[512];
    while (sector_count != 0) {
        if (read_sector(cyl, head, sector, bootdrv, int13, body) != 0) {
            pmsg("\r\nBoot failed\r\n", io_in8, io_out8);
            return 1;
        }
        putc_('.', io_in8, io_out8);
        load_seg = (uint16_t)(load_seg + 32);
        sector_count--;
        advance_chs(&cyl, &head, &sector);
    }

    pmsg("[jmp]", io_in8, io_out8);
    *entry_seg_out = entry_seg;
    return 0;
}
