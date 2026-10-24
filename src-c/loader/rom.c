/*
 * rom.c -- C port of the whole of src/loader/ROM.A86: the real
 * SBC86/12 + iSBC 204 boot ROM src/loader/ROM86Q.A86/rom86q.c above
 * replaces for the QEMU target (see rom86q.c's own header comment
 * for that sibling's own architecture, which this file follows
 * closely -- same CMD-header convention, same `sector1` buffer
 * layout: `Ty` (byte), `Len` (word), `Abs` (word), confirmed by
 * reading src/loader/ROM.A86 itself, not assumed from the sibling).
 *
 * `debug equ true` (0FFh) might look like it turns the `IF NOT DEBUG`
 * console USART/8253 baud-rate setup block off at assembly time, but
 * `NOT` in this assembler (like real MASM-family ones) is a bitwise
 * complement, not a logical one, and `IF` treats any nonzero value
 * as true -- `NOT 0FFh` is `0FF00h`, still nonzero, so the block
 * assembles in regardless (confirmed directly against the real cseg
 * bytes: `OUT 0DAh,AL` does appear). Ported here for that reason.
 * `conin`/`conout`/`pmsg` and the fatal-error-with-console-retry tail
 * they only otherwise exist for are still NOT ported -- the UART is
 * armed but, on the happy path this port models, never actually
 * used for output; see this file's own scope note on the error path
 * below.
 *
 * The initial `rep movs` that copies this file's own cseg-resident
 * data image (`drombegin`..`dromend`: the FDC command strings this
 * routine sends, plus the never-reached error table/messages) into
 * RAM at a fixed offset is a pure, deterministic byte copy with no
 * transformation -- ported here as the already-copied VALUES
 * themselves (`specs1`/`specs2`/`specs3`/`home_cmd`/`read0`/`read1`/
 * `read2` below), same "static cseg data, a local C array is exactly
 * equivalent" reasoning as `ldbios86q.c`'s own `specify_parms`/
 * `recal_parms`, rather than modeling the copy mechanically.
 *
 * The real iSBC 204/8271 FDC sequencing (`execute`/`sendcom`) is the
 * same shape as `src-c/loader/ldbios.c`'s own (same hardware), with
 * two real differences confirmed directly against this file, not
 * assumed: `sendcom` here never merges in a drive-select mask (ROM.A86
 * only ever drives drive A), and `execute`'s own interrupt-type-vs-
 * busy-type classification masks the command byte with 0x3Fh first
 * (`and AL,3Fh`) before comparing it, which `ldbios.c`'s own `execute`
 * does not. `fdc_rslt` is still drained once after either poll
 * completes, for the same real hardware handshake reason, but (same
 * convention as `ldbios.c`) not acted on -- there's no error path or
 * retry here either.
 *
 * `setupdma` here takes its own segment:offset as plain parameters
 * (ES:BX at the real call sites), not a `dma_seg`/`dma_adr` pair read
 * out of RAM the way `ldbios.c`'s own `dma_setup` does -- confirmed
 * directly against the real asm (`mov ax,es ! out fdcsegment,al ...`).
 *
 * The actual byte-level disk transfer the FDC/DMA sequence kicks off
 * is not modeled, same "no disk media here to transfer from" scope as
 * `ldbios.c`'s own `read_entry`/`write_entry` -- EXCEPT for the one
 * 5-byte CMD header (`Ty`/`Len`/`Abs`) this routine's own control flow
 * genuinely branches on (which segment to load the rest of MPMLDR.bin
 * into): `disk_read` hands that back directly, same "the callback
 * hands back only the bytes this routine actually consumes" shape as
 * `rom86q.c`'s own `int13_fn`'s `sector_out` parameter, just narrower
 * (5 bytes, not a full sector, since nothing here parses the rest).
 *
 * The real EPROM-to-RAM data copy's own destination addresses,
 * `restart`'s own retry label (there's nothing to retry without the
 * error path), and the final `jmpf dword ptr leap_offset` (CS:IP =
 * leap_segment:0000, LDBIOS.A86's own `INIT`) have no C equivalent or
 * nothing left to port, same as every other real OS entry point in
 * this project -- `start_entry` returns the resolved `leap_segment`
 * (renamed `entry_seg_out`, matching `rom86q.c`'s own naming) through
 * an output parameter in its place.
 */
#include <stdint.h>

#include "defs.h"

#define fdc_com     0xA0
#define fdc_stat    0xA0
#define fdc_parm    0xA1
#define fdc_rslt    0xA1
#define fdc_rst     0xA2
#define dmac_adr    0xA4
#define dmac_cont   0xA5
#define dmac_mode   0xA8
#define fdc_segment 0xAA
#define reset204    0xAF

#define icp1 0xC0
#define icp2 0xC2

#define csts 0xDA
#define tch2 0xD4
#define tcmd 0xD6
#define baud 0x08 /* 768/(9600/100) */

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

/* disk_read_fn -- hands back the one 5-byte CMD header (Ty, Len lo/hi,
 * Abs lo/hi) `start_entry` reads off physical T0S1. See this file's
 * own header comment for why nothing else this routine reads ever
 * needs its own transferred bytes back this way. */
typedef void (*disk_read_fn)(uint8_t *header_out);

/* send_com -- send a whole command (cmd[0] = byte count, cmd[1] =
 * opcode, cmd[2..count] = parameters) to the 8271. No drive-select
 * mask merge here -- see this file's own header comment. */
static void send_com(io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *cmd) {
    uint8_t al;
    do {
        al = io_in8(fdc_stat);
    } while ((al & 0x80) != 0);

    uint8_t count = cmd[0];
    io_out8(fdc_com, cmd[1]);
    for (uint8_t i = 2; i <= count; i++) {
        do {
            al = io_in8(fdc_stat);
        } while ((al & 0x20) != 0);
        io_out8(fdc_parm, cmd[i]);
    }
}

/* execute -- send a command string, then poll for completion the way
 * the real asm classifies it (masking off the drive-code bits first,
 * unlike ldbios.c's own execute): opcodes below 0x2Ch wait for the
 * interrupt-request bit to set, opcodes at/above it either wait for
 * not-command-busy or (if the low nibble is above 0x0C) need no poll
 * at all. Error detection is not modeled -- see this file's own
 * header comment. */
static void execute(io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *cmd) {
    send_com(io_in8, io_out8, cmd);
    uint8_t opcode = (uint8_t)(cmd[1] & 0x3F);
    uint8_t al;
    if (opcode < 0x2C) {
        do {
            al = io_in8(fdc_stat);
        } while ((al & 0x08) == 0);
        (void)io_in8(fdc_rslt);
    } else if ((opcode & 0x0F) <= 0x0C) {
        do {
            al = io_in8(fdc_stat);
        } while ((al & 0x80) != 0);
        (void)io_in8(fdc_rslt);
    }
}

/* setupdma -- arm the 8257 for one transfer to/from es:bx. See this
 * file's own header comment for why es/bx are plain parameters here. */
static void setupdma(io_out8_fn io_out8, uint16_t es, uint16_t bx) {
    io_out8(dmac_mode, 0x04);
    io_out8(dmac_cont, 0x00);
    io_out8(dmac_cont, 0x40);
    io_out8(fdc_segment, (uint8_t)(es & 0xFF));
    io_out8(fdc_segment, (uint8_t)(es >> 8));
    io_out8(dmac_adr, (uint8_t)(bx & 0xFF));
    io_out8(dmac_adr, (uint8_t)(bx >> 8));
}

/* start_entry -- the boot ROM's own entry point: init the 8259 PIC,
 * reset and program the 8271 FDC (3 SPECIFY-class commands), home
 * drive A, read the T0S1 CMD header to learn where to load the rest
 * of MPMLDR.bin, then read T0 sectors 2-26 and T1 sectors 1-26 into
 * that segment. See this file's own header comment for what's not
 * ported (console I/O, the EPROM-to-RAM copy, the final far jump).
 *	output: sets *entry_seg_out to the resolved load segment (the
 *		real `leap_segment`, offset always 0)
 */
void start_entry(io_in8_fn io_in8, io_out8_fn io_out8, disk_read_fn disk_read,
                  uint16_t *entry_seg_out) {
    io_out8(csts, 0x0E); /* give 8251 dummy mode */
    io_out8(csts, 0x40); /* reset 8251 to accept mode */
    io_out8(csts, 0x4E); /* normal 8 bit asynch mode, * 16 */
    io_out8(csts, 0x37); /* enable Tx & Rx */
    io_out8(tcmd, 0xB6); /* 8253 ch.2 square wave mode */
    io_out8(tch2, (uint8_t)(baud & 0xFF));
    io_out8(tch2, (uint8_t)(baud >> 8));

    io_out8(icp1, 0x13); /* 8259a ICW1: 8086 mode */
    io_out8(icp2, 0x10); /* 8259a ICW2: vector @ 40-5Fh */
    io_out8(icp2, 0x1F); /* 8259a ICW4: auto EOI master */
    io_out8(icp2, 0xFF); /* 8259a OCW1: mask all levels off */

    io_out8(reset204, 0x00); /* reset iSBC 204 logic */
    io_out8(fdc_rst, 0x01);  /* give 8271 FDC */
    io_out8(fdc_rst, 0x00);  /*   a reset command */

    static const uint8_t specs1[] = {5, 0x35, 0x0D, 0x08, 0x08, 0xE9};
    static const uint8_t specs2[] = {5, 0x35, 0x10, 0xFF, 0xFF, 0xFF};
    static const uint8_t specs3[] = {5, 0x35, 0x18, 0xFF, 0xFF, 0xFF};
    send_com(io_in8, io_out8, specs1); /* program Shugart SA-800 */
    send_com(io_in8, io_out8, specs2); /*   drive characteristics */
    send_com(io_in8, io_out8, specs3);

    static const uint8_t home_cmd[] = {2, 0x69, 0x00};
    execute(io_in8, io_out8, home_cmd); /* home drive 0 */

    setupdma(io_out8, 0x0000, ROM_SECTOR1); /* get T0 S1 (the CMD header) */
    static const uint8_t read0[] = {3, 0x52, 0x00, 0x01};
    execute(io_in8, io_out8, read0);

    uint8_t hdr[5];
    disk_read(hdr);
    uint16_t abs_seg = (uint16_t)(hdr[3] | (uint16_t)(hdr[4] << 8));

    setupdma(io_out8, abs_seg, 0x0000); /* get loader load address */
    static const uint8_t read1[] = {4, 0x53, 0x00, 0x02, 25}; /* T0 S2-26 */
    execute(io_in8, io_out8, read1);
    static const uint8_t read2[] = {4, 0x53, 0x01, 0x01, 26}; /* T1 S1-26 */
    execute(io_in8, io_out8, read2);

    *entry_seg_out = abs_seg;
}
