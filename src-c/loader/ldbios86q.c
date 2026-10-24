/*
 * ldbios86q.c -- C port of the whole of src/loader/LDBIOS86Q.A86:
 * the console-I/O subset (CONST, CONIN, CONOUT, PUNCH, READER,
 * GETIOBF), INIT, and every disk-I/O entry point (SELDSK, HOME,
 * SETTRK, SETSEC, SECTRAN, SETDMA, SETDMAB, GETSEGT, READ, WRITE)
 * plus the FDC/DMA utilities they need (fdc_send_cmd/fdc_out_byte/
 * fdc_in_byte/fdc_sense_int/fdc_wait_seek/fdc_pio_in/rw_physical/
 * fdc_fill_rw_parms/fdc_seek_to/dma_setup). (LISTOUT/LISTST/SETIOBF
 * are bare `ret`s with no observable effect at all -- nothing to
 * port there, same class as src-c/loader/lbdosnew.c's own
 * func50/52-57.)
 *
 * This is the first file in this port to need real 8086 port-mapped
 * I/O (`IN`/`OUT`) rather than memory -- backed by two function-
 * pointer parameters, `io_in8`/`io_out8`, matching
 * tools/i86sim.py's own new `io_in_hook`/`io_out_hook` (see that
 * file's own docstring): tests/src-c/fixtures/uart86q.py's `Uart86Q`
 * (console) and fdc86q.py's `Fdc86Q` (floppy controller) are the
 * test-only device models backing both the `I86Sim` hooks and these
 * same two C callbacks via ctypes, so the differential test drives
 * one real virtual device from both sides at once.
 *
 * PUNCH falls straight through into READER's own code in the real
 * asm (`PUNCH: READER: mov al,1ah ! ret` -- two labels, one body,
 * no code of PUNCH's own) -- ported here as one function,
 * `reader_entry`, with `punch_entry` just calling it, same as any
 * other same-body-different-label pair in this port (e.g.
 * src-c/loader/lbdosf3.c's `func38`/`func39`).
 *
 * `INIT`'s own real stack switch (`mov sp,offset stkbase`) and its
 * final `jmp ccp_offset` (a hardcoded jump back into LDMPM.A86, see
 * that file's own header comment) have no C equivalent, same as
 * every other real OS entry point in this port -- `init_entry` here
 * is everything in between: the UART/FDC hardware-setup port writes,
 * verified for real against a simulated device. `WBOOT` is nothing
 * but that same `jmp ccp_offset` -- nothing to port there either.
 *
 * `fdc_send_cmd` takes a plain `const uint8_t *cmd` rather than a
 * `mem`+address pair: `specify_parms`/`recal_parms` (INIT/HOME's own
 * callers) are static cseg data, never written at runtime by
 * anything, so a local C array is exactly equivalent and avoids
 * needing to pre-seed `mem` with constant tables neither function
 * actually touches through `mem` itself.
 *
 * `SELDSK`'s own `offset dpe0` and `GETSEGT`'s own `offset segtable`
 * are both real, deterministically-resolved addresses (this file has
 * no `INCLUDE`s at all, confirmed by grep, so it always assembles to
 * one fixed layout) -- ported as the constants themselves
 * (`LDB86Q_DPE0`/`LDB86Q_SEGTABLE`, src-c/loader/defs.h), with their
 * own pointed-to content left unmodeled: nothing ported so far reads
 * either one's own structure (the DPH/segment-table bytes), same
 * "unconsumed, not modeled" precedent as src-c/kernel/init.c's own
 * `p_uda` write.
 *
 * `fdc_fill_rw_parms`/`fdc_seek_to` each rewrite their own command
 * table's FULL content (the static byte(s) too, not just the
 * head/cyl/sector fields the real asm's own version touches) every
 * call, rather than relying on the table's own static cseg content
 * persisting from assembly time the way the real asm does -- this
 * port's own `mem` has no equivalent "preloaded image" when called
 * standalone (unlike `fdc_send_cmd`'s `specify_parms`/`recal_parms`,
 * these two tables ARE mutated at runtime, so they need to live in
 * `mem`, not a local throwaway array). Produces identical observable
 * behavior (nothing else ever reads/writes the untouched bytes of
 * either table), same "simplified without changing what's
 * observable" precedent as `SELDSK`'s own fixed `bx=0`.
 *
 * `READ`/`WRITE`'s own `cs` (this module's own real code segment --
 * genuinely ambient/external, the same "not derivable from this file
 * alone" class as `src-c/kernel/init.c`'s own `cs`/`ds_sysdat`
 * parameters) is a parameter here, needed by `dma_setup`'s own 20-bit
 * physical-address computation for `secbuf`. The actual `fd_secsize`
 * (512-byte) disk<->`secbuf` transfer DMA itself performs is NOT
 * modeled (real 8237 hardware moves those bytes with the CPU and
 * this port equally uninvolved -- there's no disk media here to
 * transfer from/to either); what IS modeled, and real, is the
 * CPU-visible 128-byte copy between `secbuf`+`cur_bufoff` and the
 * caller's own `dma_seg:dma_adr` buffer, a genuine segment:offset
 * access (`mem_r8_seg`/`mem_w8_seg`, src-c/loader/defs.h) -- tested
 * for real via `tools/i86sim.py`'s `segmented=True` mode with this
 * module's own `cs`/`ds` pinned to 0 for the test (this file's own
 * `cs` is genuinely unconstrained from outside, so 0 is as valid a
 * choice as any, and keeps every *other* bare/DS-relative access in
 * the same routine flat-equivalent to its usual scratch address --
 * unlike `INIT.SUP`'s own `u_wrkseg`/RSP-loop, there's no OTHER real
 * field in this same routine that depends on `ds` being nonzero, so
 * there's no version of the conflict that file's own header comment
 * describes).
 *
 * `motor_delay` has no caller anywhere in this file (confirmed by
 * grep), same as `pmsg`/`signon` -- dead code, nothing to port, same
 * "assembles but never executed" class as other such findings in
 * this tree.
 */
#include <stdint.h>
#include <string.h>

#include "defs.h"

#define uart_rxtx 0x3F8
#define uart_lsr  0x3FD
#define uart_lcr  0x3FB
#define uart_mcr  0x3FC
#define uart_ier  0x3F9
#define uart_lsr_rxrdy 0x01
#define uart_lsr_txrdy 0x20
#define uart_divisor   0x000C

#define fdc_dor  0x3F2
#define fdc_msr  0x3F4
#define fdc_data 0x3F5
#define msr_rqm  0x80
#define msr_dio  0x40
#define msr_busy 0x10
#define dor_mota      0x10
#define dor_ndreset   0x04
#define dor_dmaen     0x08
#define dor_drvsel_a  0x00
#define cmd_specify   0x03
#define cmd_recal     0x07
#define cmd_sense_int 0x08
#define cmd_seek      0x0F
#define cmd_read      0x46
#define cmd_write     0x45

#define fd_heads      2
#define fd_spt        18
#define fd_secsize_n  2
#define fd_secsize    512
#define fd_gap3       0x1B
#define fd_dtl        0xFF

#define dma_addr2       0x04
#define dma_count2      0x05
#define dma_page2       0x81
#define dma_mask        0x0A
#define dma_mode        0x0B
#define dma_clrff       0x0C
#define dma_mask_set2   0x06
#define dma_mask_clr2   0x02
#define dma_mode_read   0x46
#define dma_mode_write  0x4A

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

/* const_entry -- CONST: console status.
 *	output:	returns al (0xFF = a character is ready, 0 = not ready)
 */
uint8_t const_entry(io_in8_fn io_in8) {
    uint8_t al = io_in8(uart_lsr);
    al = (uint8_t)(al & uart_lsr_rxrdy);
    if (al == 0) {
        return 0;
    }
    return 0xFF;
}

/* conin_entry -- CONIN: console input, waits for a character.
 *	output:	returns al (the character read)
 */
uint8_t conin_entry(io_in8_fn io_in8) {
    while (const_entry(io_in8) == 0) {
        /* cin_wait -- real loop, exercised by the differential test
         * via a fixed-ready uart86q.py model (not a genuine stall:
         * see that fixture's own header comment) */
    }
    return io_in8(uart_rxtx);
}

/* conout_entry -- CONOUT: console output, waits for the UART to be
 * ready to transmit.
 *	input:	cl = character to send
 */
void conout_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl) {
    uint8_t al;
    do {
        /* cout_wait */
        al = io_in8(uart_lsr);
        al = (uint8_t)(al & uart_lsr_txrdy);
    } while (al == 0);
    io_out8(uart_rxtx, cl);
}

/* reader_entry -- READER (and, falling through to the same body in
 * the real asm, PUNCH): paper-tape reader input, always reports EOF.
 *	output:	returns al (0x1A, ctrl-Z/EOF)
 */
uint8_t reader_entry(void) {
    return 0x1A;
}

/* punch_entry -- PUNCH: see reader_entry's own header comment --
 * PUNCH has no code of its own in the real asm, just READER's. */
uint8_t punch_entry(void) {
    return reader_entry();
}

/* getiobf_entry -- GETIOBF: get I/O byte.
 *	output:	returns al (0, always -- this driver has no real IOBYTE)
 */
uint8_t getiobf_entry(void) {
    return 0;
}

/* -- FDC (NEC 765/8272) command-sequencing utilities -------------- */

/* fdc_out_byte -- send one command/parameter byte to the FDC, once
 * it's ready to accept it. */
static void fdc_out_byte(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t val) {
    uint8_t al;
    do {
        /* fob_wait */
        al = io_in8(fdc_msr);
        al = (uint8_t)(al & (msr_rqm | msr_dio));
    } while (al != msr_rqm);
    io_out8(fdc_data, val);
}

/* fdc_in_byte -- read one result byte from the FDC, once it's ready
 * to supply one. */
static uint8_t fdc_in_byte(io_in8_fn io_in8) {
    uint8_t al;
    do {
        /* fib_wait */
        al = io_in8(fdc_msr);
        al = (uint8_t)(al & (msr_rqm | msr_dio));
    } while (al != (msr_rqm | msr_dio));
    return io_in8(fdc_data);
}

/* fdc_send_cmd -- send a whole command (cmd[0] = byte count,
 * cmd[1..count] = the bytes themselves) to the FDC. See this file's
 * own header comment for why this takes a plain pointer, not
 * `mem`+address. */
static void fdc_send_cmd(io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *cmd) {
    uint8_t count = cmd[0];
    for (uint8_t i = 1; i <= count; i++) {
        fdc_out_byte(io_in8, io_out8, cmd[i]);
    }
}

/* fdc_sense_int -- SENSE INTERRUPT STATUS: drains its 2-byte result
 * phase (status register 0, present cylinder number); neither
 * result byte has a consumer anywhere in this port yet, so they're
 * discarded, same as the real asm's own callers (neither HOME nor
 * fdc_wait_seek's own tail here ever looks at them either). */
static void fdc_sense_int(io_in8_fn io_in8, io_out8_fn io_out8) {
    fdc_out_byte(io_in8, io_out8, cmd_sense_int);
    (void)fdc_in_byte(io_in8);
    (void)fdc_in_byte(io_in8);
}

/* fdc_wait_seek -- poll MSR's own BUSY bit until it clears (bounded
 * the same way the real `loopnz`, starting `cx` at 0, bounds it: up
 * to 65536 polls, not a genuine infinite loop), then drain the one
 * SENSE INTERRUPT STATUS a real seek/recalibrate command's own
 * completion interrupt requires. */
static void fdc_wait_seek(io_in8_fn io_in8, io_out8_fn io_out8) {
    uint16_t cx = 0;
    uint8_t al;
    do {
        al = io_in8(fdc_msr);
        cx = (uint16_t)(cx - 1);
    } while ((al & msr_busy) != 0 && cx != 0);
    fdc_sense_int(io_in8, io_out8);
}

/* -- disk-I/O entry points (non-DMA subset) ----------------------- */

/* seldsk_entry -- SELDSK: select disk.
 *	input:	cl = disk number
 *	output:	returns bx (the DPH address, or 0 if cl is an unsupported
 *		drive -- this driver only supports drive 0)
 */
uint16_t seldsk_entry(uint8_t *mem, uint8_t cl) {
    if (cl != 0) {
        return 0;
    }
    mem[LDB86Q_DISK] = 0;
    return LDB86Q_DPE0;
}

/* home_entry -- HOME: seek to track 0 (recalibrate). */
void home_entry(uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8) {
    static const uint8_t recal_parms[] = {2, cmd_recal, 0};
    mem[LDB86Q_TRK] = 0;
    fdc_send_cmd(io_in8, io_out8, recal_parms);
    fdc_wait_seek(io_in8, io_out8);
}

/* settrk_entry -- SETTRK: set track.
 *	input:	cl = track number
 */
void settrk_entry(uint8_t *mem, uint8_t cl) {
    mem[LDB86Q_TRK] = cl;
}

/* setsec_entry -- SETSEC: set sector.
 *	input:	cl = sector number
 */
void setsec_entry(uint8_t *mem, uint8_t cl) {
    mem[LDB86Q_SECT] = cl;
}

/* sectran_entry -- SECTRAN: translate a logical sector number.
 *	input:	cx = logical sector number; dx = translate table address
 *		(0 = no translation, this driver's own xlt0)
 *	output:	returns ax (the translated physical sector number)
 */
uint16_t sectran_entry(const uint8_t *mem, uint16_t cx, uint16_t dx) {
    if (dx == 0) {
        return cx;
    }
    uint16_t bx = (uint16_t)(cx + dx);
    return mem[bx];
}

/* setdma_entry -- SETDMA: set DMA (transfer) address.
 *	input:	cx = dma offset
 */
void setdma_entry(uint8_t *mem, uint16_t cx) {
    mem_w16(mem, LDB86Q_DMA_ADR, cx);
}

/* setdmab_entry -- SETDMAB: set DMA (transfer) segment base.
 *	input:	cx = dma segment
 */
void setdmab_entry(uint8_t *mem, uint16_t cx) {
    mem_w16(mem, LDB86Q_DMA_SEG, cx);
}

/* getsegt_entry -- GETSEGT: get the system-memory segment table.
 *	output:	returns bx (the segment-table address)
 */
uint16_t getsegt_entry(void) {
    return LDB86Q_SEGTABLE;
}

/* init_entry -- INIT: arm the UART and FDC hardware. See this file's
 * own header comment for what's deliberately not ported here (the
 * stack switch, and the final jump back into LDMPM.A86).
 */
void init_entry(io_in8_fn io_in8, io_out8_fn io_out8) {
    io_out8(uart_lcr, 0x80);
    io_out8(uart_rxtx, (uint8_t)(uart_divisor & 0xFF));
    io_out8(uart_ier, (uint8_t)(uart_divisor >> 8));
    io_out8(uart_lcr, 0x03);
    io_out8(uart_ier, 0x00);
    io_out8(uart_mcr, 0x03);

    io_out8(fdc_dor, dor_ndreset | dor_mota | dor_dmaen | dor_drvsel_a);

    static const uint8_t specify_parms[] = {3, cmd_specify, 0xDF, 0x02};
    fdc_send_cmd(io_in8, io_out8, specify_parms);
}

/* -- the real sector-transfer path --------------------------------- */

/* rw_physical -- convert the logical track/sector (trk/sect, set by
 * SETTRK/SETSEC) into physical cylinder/head/sector numbers plus the
 * byte offset of this logical (128-byte) record within the physical
 * (512-byte, 4:1 deblocked) sector's own secbuf copy. */
static void rw_physical(uint8_t *mem) {
    uint8_t sect = mem[LDB86Q_SECT];
    mem[LDB86Q_CUR_PSEC] = (uint8_t)(sect / 4 + 1);
    mem_w16(mem, LDB86Q_CUR_BUFOFF, (uint16_t)((sect % 4) * 128));

    uint8_t trk = mem[LDB86Q_TRK];
    mem[LDB86Q_CUR_CYL] = (uint8_t)(trk / fd_heads);
    mem[LDB86Q_CUR_HEAD] = (uint8_t)(trk % fd_heads);
}

/* fdc_fill_rw_parms -- (re)build the READ DATA/WRITE DATA command
 * tables from cur_head/cur_cyl/cur_psec. See this file's own header
 * comment for why this writes the WHOLE table, not just the 4 bytes
 * the real asm's own version touches. */
static void fdc_fill_rw_parms(uint8_t *mem) {
    uint8_t head = mem[LDB86Q_CUR_HEAD];
    uint8_t cyl = mem[LDB86Q_CUR_CYL];
    uint8_t psec = mem[LDB86Q_CUR_PSEC];
    uint8_t hds = (uint8_t)(head << 2);
    const uint8_t read_tbl[10] = {9, cmd_read, hds, cyl, head, psec,
                                   fd_secsize_n, fd_spt, fd_gap3, fd_dtl};
    const uint8_t write_tbl[10] = {9, cmd_write, hds, cyl, head, psec,
                                    fd_secsize_n, fd_spt, fd_gap3, fd_dtl};
    memcpy(&mem[LDB86Q_READ_PARMS], read_tbl, sizeof read_tbl);
    memcpy(&mem[LDB86Q_WRITE_PARMS], write_tbl, sizeof write_tbl);
}

/* fdc_seek_to -- seek to cur_cyl/cur_head. See this file's own
 * header comment for why this writes the WHOLE seek_parms table. */
static void fdc_seek_to(uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t head = mem[LDB86Q_CUR_HEAD];
    uint8_t cyl = mem[LDB86Q_CUR_CYL];
    uint8_t hds = (uint8_t)(head << 2);
    const uint8_t seek_tbl[4] = {3, cmd_seek, hds, cyl};
    memcpy(&mem[LDB86Q_SEEK_PARMS], seek_tbl, sizeof seek_tbl);
    fdc_send_cmd(io_in8, io_out8, &mem[LDB86Q_SEEK_PARMS]);
    fdc_wait_seek(io_in8, io_out8);
}

/* fdc_pio_in -- drain READ DATA/WRITE DATA's own 7-byte PIO result
 * phase (status registers 0-2, cyl/head/sector/size echoed back) --
 * no consumer anywhere in this port, same as fdc_sense_int's own
 * 2 result bytes. */
static void fdc_pio_in(io_in8_fn io_in8) {
    for (int i = 0; i < 7; i++) {
        (void)fdc_in_byte(io_in8);
    }
}

/* dma_setup -- arm 8237 DMA channel 2 for one fd_secsize-byte
 * transfer into/out of secbuf. `cs` is this module's own real code
 * segment -- see this file's own header comment for why it's a
 * parameter. `mode` is `dma_mode_read`/`dma_mode_write`. */
static void dma_setup(io_out8_fn io_out8, uint16_t cs, uint8_t mode) {
    io_out8(dma_mask, dma_mask_set2);
    io_out8(dma_clrff, 0x00);
    io_out8(dma_mode, mode);

    uint16_t ax = (uint16_t)(cs << 4);
    uint16_t bx = (uint16_t)(cs >> 12);
    uint32_t sum = (uint32_t)ax + (uint32_t)LDB86Q_SECBUF;
    ax = (uint16_t)sum;
    bx = (uint16_t)(bx + (sum >> 16));

    io_out8(dma_addr2, (uint8_t)(ax & 0xFF));
    io_out8(dma_addr2, (uint8_t)(ax >> 8));
    io_out8(dma_page2, (uint8_t)(bx & 0xFF));

    uint16_t count = fd_secsize - 1;
    io_out8(dma_count2, (uint8_t)(count & 0xFF));
    io_out8(dma_count2, (uint8_t)(count >> 8));

    io_out8(dma_mask, dma_mask_clr2);
}

/* read_entry -- READ: read the current trk/sect's logical record
 * into the caller's dma_seg:dma_adr buffer. The actual
 * fd_secsize-byte disk<->secbuf DMA transfer itself is not modeled
 * (there's no disk media here to transfer from) -- what's real here
 * is the FDC command sequencing and the 128-byte secbuf-to-caller
 * copy, a genuine segment:offset write (`mem_w8_seg`). See this
 * file's own header comment.
 *	output:	returns al (0, always -- this driver has no read-error
 *		detection)
 */
uint8_t read_entry(uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8, uint16_t cs) {
    rw_physical(mem);
    fdc_seek_to(mem, io_in8, io_out8);
    fdc_fill_rw_parms(mem);
    dma_setup(io_out8, cs, dma_mode_read);
    fdc_send_cmd(io_in8, io_out8, &mem[LDB86Q_READ_PARMS]);
    fdc_pio_in(io_in8);

    uint16_t cur_bufoff = mem_r16(mem, LDB86Q_CUR_BUFOFF);
    uint16_t dma_adr = mem_r16(mem, LDB86Q_DMA_ADR);
    uint16_t dma_seg = mem_r16(mem, LDB86Q_DMA_SEG);
    for (uint16_t i = 0; i < 128; i++) {
        uint8_t b = mem[(uint16_t)(LDB86Q_SECBUF + cur_bufoff + i)];
        mem_w8_seg(mem, dma_seg, (uint16_t)(dma_adr + i), b);
    }
    return 0;
}

/* write_entry -- WRITE: write the caller's dma_seg:dma_adr buffer's
 * 128-byte logical record into the current trk/sect -- a real
 * read-modify-write (this driver has no way to write less than a
 * whole fd_secsize-byte physical sector): read the other 3/4 of the
 * physical sector into secbuf first, splice in the caller's own 128
 * bytes, then write the whole physical sector back out. Same
 * "DMA itself not modeled" scope as read_entry.
 *	output:	returns al (0, always -- this driver has no write-error
 *		detection)
 */
uint8_t write_entry(uint8_t *mem, io_in8_fn io_in8, io_out8_fn io_out8, uint16_t cs) {
    rw_physical(mem);
    fdc_seek_to(mem, io_in8, io_out8);
    fdc_fill_rw_parms(mem);
    dma_setup(io_out8, cs, dma_mode_read);
    fdc_send_cmd(io_in8, io_out8, &mem[LDB86Q_READ_PARMS]);
    fdc_pio_in(io_in8);

    uint16_t cur_bufoff = mem_r16(mem, LDB86Q_CUR_BUFOFF);
    uint16_t dma_adr = mem_r16(mem, LDB86Q_DMA_ADR);
    uint16_t dma_seg = mem_r16(mem, LDB86Q_DMA_SEG);
    for (uint16_t i = 0; i < 128; i++) {
        uint8_t b = mem_r8_seg(mem, dma_seg, (uint16_t)(dma_adr + i));
        mem[(uint16_t)(LDB86Q_SECBUF + cur_bufoff + i)] = b;
    }

    fdc_fill_rw_parms(mem);
    dma_setup(io_out8, cs, dma_mode_write);
    fdc_send_cmd(io_in8, io_out8, &mem[LDB86Q_WRITE_PARMS]);
    fdc_pio_in(io_in8);
    return 0;
}
