/*
 * ldbios.c -- C port of the whole of src/loader/LDBIOS.A86: the real
 * iSBC 86/12 + iSBC 204 driver src/loader/LDBIOS86Q.A86 replaces for
 * the QEMU target (see ldbios86q.c's own header comment for that
 * sibling's own architecture, which this file follows closely).
 *
 * `INIT` does no hardware setup at all (confirmed directly against
 * the real asm) -- just the usual stack/segment-register switch and
 * `bdos_offset`/`bdos_segment` writes, then `jmp ccp` -- same "no C
 * equivalent, nothing left to port" scope as every other real OS
 * entry point in this port (the real hardware init this file's
 * callers need already happened in src/loader/ROM.A86's own boot
 * sequence before LDBIOS.A86 is even loaded). `WBOOT` is nothing but
 * `jmp ccp+6` -- nothing to port there either. `LISTOUT`/`LISTST` are
 * bare `ret`s, `SETIOBF` likewise -- same "no observable effect"
 * class as `ldbios86q.c`'s own equivalents.
 *
 * Real console I/O is the simpler i8251 (not the 8250 UART
 * LDBIOS86Q.A86/LDBIOS86Q.c model): `CONST`'s own status bit is RDA
 * (bit 1), `CONOUT`'s is TBE (bit 0), both on the same status port.
 *
 * Real disk I/O is the iSBC 204's 8271 FDC + 8257 DMA controller,
 * genuinely simpler than the QEMU NEC765 model in two ways: the 8271
 * transfers native 128-byte sectors (no 4:1 deblocking, no `secbuf`
 * intermediary -- `fdc_fill_rw_parms`/`fdc_seek_to`/`rw_physical`
 * have no equivalent here, trk/sect go to the FDC as-is), and its own
 * DMA controller (the 8257) takes a segment straight through a
 * dedicated `fdcsegment` port rather than computing a physical
 * address from a code segment the way `ldbios86q.c`'s own
 * `dma_setup` does from `cs` -- so there's no `cs` parameter here at
 * all, and no byte-level transfer to model either (same "no disk
 * media here to transfer from/to" scope `ldbios86q.c`'s own header
 * comment already establishes, just with nothing left to copy even
 * at the CPU-visible level, since there's no deblocking step for a
 * mem_r8_seg/mem_w8_seg copy to bridge).
 *
 * The real retry-with-console-interaction error path (`execute`'s
 * own tail: on a disk error, print a message, read an upper-cased
 * console character, and retry/cancel/ignore depending on which key)
 * is NOT modeled -- same "this driver has no read/write-error
 * detection" simplification `ldbios86q.c`'s own `read_entry`/
 * `write_entry` already make, kept for consistency with that sibling
 * rather than because this file's own error path is somehow harder;
 * `read_entry`/`write_entry`/`home_entry` all just run the happy path
 * (send the command, poll for completion, done).
 *
 * `SELDSK` supports 2 real drives (unlike LDBIOS86Q.A86's own
 * single-drive restriction) -- ported faithfully, not simplified to
 * match the sibling.
 */
#include <stdint.h>

#include "defs.h"

#define csts  0xDA /* i8251 status port */
#define cdata 0xD8 /* i8251 data port */

#define fdc_com       0xA0
#define fdc_stat      0xA0
#define fdc_parm      0xA1
#define fdc_rslt      0xA1
#define fdc_rst       0xA2
#define dmac_adr      0xA4
#define dmac_cont     0xA5
#define dmac_mode     0xA8
#define fdc_segment   0xAA

#define cmd_read  0x12
#define cmd_write 0x0A

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

/* const_entry -- CONST: console status.
 *	output:	returns al (0xFF if a character is ready, else 0)
 */
uint8_t const_entry(io_in8_fn io_in8) {
    uint8_t al = (uint8_t)(io_in8(csts) & 2);
    return al == 0 ? 0 : 0xFF;
}

/* conin_entry -- CONIN: console input, waits for a character. */
uint8_t conin_entry(io_in8_fn io_in8) {
    while (const_entry(io_in8) == 0) {
        /* wait for RDA */
    }
    return (uint8_t)(io_in8(cdata) & 0x7F);
}

/* conout_entry -- CONOUT: console output, waits for TBE.
 *	input:	cl = character to send
 */
void conout_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl) {
    while ((io_in8(csts) & 1) == 0) {
        /* wait for TBE */
    }
    io_out8(cdata, cl);
}

/* reader_entry -- READER: not implemented, always reports EOF. */
uint8_t reader_entry(void) {
    return 0x1A;
}

/* punch_entry -- PUNCH: same body as READER in the real asm. */
uint8_t punch_entry(void) {
    return reader_entry();
}

/* getiobf_entry -- GETIOBF: IOBYTE not implemented. */
uint8_t getiobf_entry(void) {
    return 0;
}

/* -- FDC (8271) / DMA (8257) command-sequencing --------------------- */

/* dma_setup -- arm the 8257 for one transfer in the given direction
 * (dir: 0x40 = read, 0x80 = write), using the caller's own dma_adr/
 * dma_seg (the real target buffer -- no secbuf, see this file's own
 * header comment). */
static void dma_setup(const uint8_t *mem, io_out8_fn io_out8, uint8_t dir) {
    io_out8(dmac_mode, 0x04); /* enable dmac */
    io_out8(dmac_cont, 0x00); /* dummy first byte */
    io_out8(dmac_cont, dir);  /* direction register */
    uint16_t adr = mem_r16(mem, LDB_DMA_ADR);
    io_out8(dmac_adr, (uint8_t)(adr & 0xFF));
    io_out8(dmac_adr, (uint8_t)(adr >> 8));
    uint16_t seg = mem_r16(mem, LDB_DMA_SEG);
    io_out8(fdc_segment, (uint8_t)(seg & 0xFF));
    io_out8(fdc_segment, (uint8_t)(seg >> 8));
}

/* send_com -- send a whole command (cmd[0] = byte count, cmd[1] =
 * opcode, cmd[2..count] = parameters) to the 8271, merging the
 * current drive-select mask into the opcode byte (and arming the DMA
 * controller first, if this is a read or write) -- see this file's
 * own header comment for why there's no byte transfer to follow it
 * with. */
static void send_com(const uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *cmd) {
    uint8_t al;
    do {
        al = io_in8(fdc_stat);
    } while ((al & 0x80) != 0);

    if (cmd[1] == cmd_read) {
        dma_setup(mem, io_out8, 0x40);
    } else if (cmd[1] == cmd_write) {
        dma_setup(mem, io_out8, 0x80);
    }

    uint8_t count = cmd[0];
    io_out8(fdc_com, (uint8_t)(cmd[1] | mem[LDB_SEL_MASK]));
    for (uint8_t i = 2; i <= count; i++) {
        do {
            al = io_in8(fdc_stat);
        } while ((al & 0x20) != 0);
        io_out8(fdc_parm, cmd[i]);
    }
}

/* execute -- send a command string, then poll for completion the
 * same way the real asm classifies it: opcodes below 0x2Ch wait for
 * the interrupt-request bit to set, opcodes at/above it either wait
 * for not-command-busy or (if the low nibble is above 0x0C) need no
 * poll at all. Either poll, once satisfied, is always followed by one
 * `fdc_rslt` read in the real asm (to check for an error) -- drained
 * here too, for the same real hardware handshake sequencing, but not
 * acted on: see this file's own header comment for why error
 * detection itself is not modeled.
 *	output: returns al (0, always)
 */
static uint8_t execute(const uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *cmd) {
    send_com(mem, io_in8, io_out8, cmd);
    uint8_t opcode = cmd[1];
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
    return 0;
}

/* -- disk-I/O entry points ------------------------------------------ */

/* seldsk_entry -- SELDSK: select disk.
 *	input:	cl = disk number (0 or 1 -- this driver supports 2 drives)
 *	output:	returns bx (the DPH address, or 0 if cl is out of range)
 */
uint16_t seldsk_entry(uint8_t *mem, uint8_t cl) {
    if (cl >= 2) {
        return 0;
    }
    mem[LDB_SEL_MASK] = (cl == 0) ? 0x40 : 0x80;
    return (uint16_t)(LDB_DPBASE + cl * 16);
}

/* home_entry -- HOME: seek to track 0. */
void home_entry(const uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8) {
    static const uint8_t hom_com[] = {2, 0x29, 0};
    (void)execute(mem, io_in8, io_out8, hom_com);
}

/* settrk_entry -- SETTRK: set track.
 *	input:	cl = track number
 */
void settrk_entry(uint8_t *mem, uint8_t cl) {
    mem[LDB_TRK] = cl;
}

/* setsec_entry -- SETSEC: set sector.
 *	input:	cl = sector number
 */
void setsec_entry(uint8_t *mem, uint8_t cl) {
    mem[LDB_SECT] = cl;
}

/* sectran_entry -- SECTRAN: translate a logical sector number. Unlike
 * LDBIOS86Q.A86/XIOS.A86's own SECTRAN, the real asm here has no
 * `dx == 0` shortcut at all -- it's always a table lookup, and the
 * result comes back in BX, not AX: `mov bx,cx ! add bx,dx ! mov
 * bl,[bx]` only overwrites BL, leaving BH as the high byte of
 * `cx+dx` itself (not cleared or recomputed), so that quirk is
 * reproduced exactly here too.
 *	input:	cx = logical sector number; dx = translate table address
 *	output:	returns bx (BL = mem[cx+dx], BH = high byte of cx+dx)
 */
uint16_t sectran_entry(const uint8_t *mem, uint16_t cx, uint16_t dx) {
    uint16_t bx = (uint16_t)(cx + dx);
    uint8_t bl = mem[bx];
    return (uint16_t)((bx & 0xFF00) | bl);
}

/* setdma_entry -- SETDMA: set DMA (transfer) offset.
 *	input:	cx = dma offset
 */
void setdma_entry(uint8_t *mem, uint16_t cx) {
    mem_w16(mem, LDB_DMA_ADR, cx);
}

/* setdmab_entry -- SETDMAB: set DMA (transfer) segment.
 *	input:	cx = dma segment
 */
void setdmab_entry(uint8_t *mem, uint16_t cx) {
    mem_w16(mem, LDB_DMA_SEG, cx);
}

/* getsegt_entry -- GETSEGT: get the system-memory segment table.
 *	output:	returns bx (the segment-table address)
 */
uint16_t getsegt_entry(void) {
    return LDB_SEGTABLE;
}

/* read_entry -- READ: read the current trk/sect into the caller's
 * dma_seg:dma_adr buffer. See this file's own header comment for why
 * there's no byte-level transfer modeled here at all.
 *	output:	returns al (0, always)
 */
uint8_t read_entry(const uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t io_com[4] = {3, cmd_read, mem[LDB_TRK], mem[LDB_SECT]};
    return execute(mem, io_in8, io_out8, io_com);
}

/* write_entry -- WRITE: write the caller's dma_seg:dma_adr buffer
 * into the current trk/sect. Same scope as read_entry.
 *	output:	returns al (0, always)
 */
uint8_t write_entry(const uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t io_com[4] = {3, cmd_write, mem[LDB_TRK], mem[LDB_SECT]};
    return execute(mem, io_in8, io_out8, io_com);
}
