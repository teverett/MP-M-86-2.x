/*
 * xios86q.c -- C port of the whole of src/kernel/XIOS86Q.A86: a
 * from-scratch QEMU XIOS this project wrote (not DRI source -- see
 * that file's own header comment), the file actually combined into
 * the real bootable MPM.bin (unlike the real XIOS.A86, replaced here
 * for QEMU the same way LDBIOS86Q.A86 replaces LDBIOS.A86 for
 * MPMLDR.bin -- see src-c/loader/ldbios86q.c, whose architecture this
 * file follows closely: same NEC 765/8272 FDC + 8237 DMA + 8250 UART
 * hardware, same `io_in8`/`io_out8` callback convention).
 *
 * Not ported, same conventions already established elsewhere in this
 * project:
 *   - `entry`/`supif` -- far-call trampolines, same convention as
 *     every other module's own `entry`/`mpmif`/`xiosif`.
 *   - `idle` -- a pure `mov cx,f_dispatch ! call supif ! jmp idle`
 *     passthrough with zero data transformation of its own, same
 *     "porting it would add no real logic" reasoning as
 *     `src-c/kernel/xios.c`'s own `idle` (a different real routine
 *     that happens to share the label name).
 *   - `tickint`'s own real `INT`-handler shell (the stack switch,
 *     register save/restore, PIC EOI, and the final `jmpf
 *     cs:dword ptr mydispatch` tail-dispatch) -- no C equivalent,
 *     same class as every other real interrupt entry/exit point in
 *     this port. Its own real logic (the tick_count countdown and
 *     which flag(s) it requests) IS ported, as `tickint_once`,
 *     returning which `f_flagset` calls the real ISR would have made
 *     (each one itself a boundary-stub far call, same convention as
 *     `clock.c`'s own `clockloop`) instead of making them.
 *   - `rtm_poll`'s own "yield via the Supervisor's f_polldev" tail
 *     (`jmp supif`) -- a boundary-stub tail jump into unported
 *     machinery, same "resumes exactly here once rescheduled"
 *     reasoning the boundary-stub convention describes; modeled as
 *     an immediate return once `polldev` reports ready, since
 *     neither `xios_conin_entry` nor `xios_conout_entry` (its only two
 *     real callers, named with an `xios_` prefix here -- see below
 *     for why) ever inspect `rtm_poll`'s own return value anyway --
 *     its whole observable job, from their point of view, is just
 *     "block until ready," which this port's synthetic test device
 *     always already is.
 *
 * `init_entry` ports the hardware-arming sequence (UART/PIT/PIC/FDC)
 * and the signon message, but not the real `init:`'s own far-pointer
 * copies out of SYSDAT's module table (`supervisor`/`mydispatch` --
 * genuinely external state this isolated port has nothing to copy
 * to/from, same class as `src-c/kernel/init.c`'s own un-exercised
 * SYSDAT module-table reads), its real-mode interrupt-vector
 * write (`mov word ptr 0020h,offset tickint` -- writing a real IVT
 * entry has no C equivalent to model), or its final `retf` (no C
 * equivalent, same as every other real OS entry point in this port).
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the XIOS's DS -- SYSDAT, whose
 * segment this module's data follows (the XQ_* fields are real
 * XIOS86Q.A86 offsets, from 1000h). The 128-byte logical-record copy
 * `diskread_entry`/`diskwrite_entry` do between `secbuf` and the
 * caller's `dma_seg:dma_adr` buffer goes through that segment, and
 * `dma_setup` arms the 8237 with secbuf's physical address from it.
 * (The asm's `push cs ! pop ds` around the copy reads secbuf through
 * CS, which is the same segment as DS for this module.)
 */
#include <stdint.h>
#include <string.h>

#include "defs.h"

#define c0indev       0x00
#define c0outdev      0x04
#define flpy_poll_dev 0x09

#define uart_rxtx 0x3F8
#define uart_lsr  0x3FD
#define uart_lsr_rxrdy    0x01
#define uart_lsr_txrdy    0x20
#define uart_lsr_errmask  0x1E

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
#define cmd_sense_int 0x08

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

#define fd_heads     2
#define fd_secsize   512

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

/* -- POLL DEVICE / rtm_poll ------------------------------------------ */

/* polldev_entry -- Function 17: Poll Device.
 *	input:	cl = device number (c0indev/c0outdev/flpy_poll_dev)
 *	output:	returns al (0xFF if ready, 0 if not)
 */
uint8_t polldev_entry(io_in8_fn io_in8, uint8_t cl) {
    if (cl == c0indev) {
        uint8_t al = io_in8(uart_lsr);
        if ((al & uart_lsr_errmask) != 0) {
            return 0;
        }
        return (al & uart_lsr_rxrdy) != 0 ? 0xFF : 0;
    }
    if (cl == c0outdev) {
        uint8_t al = io_in8(uart_lsr);
        return (al & uart_lsr_txrdy) != 0 ? 0xFF : 0;
    }
    /* flpy_poll_dev (and anything else): floppy I/O is fully
     * synchronous PIO, already complete by the time anything polls
     * it -- see this file's own header comment. */
    return 0xFF;
}

/* rtm_poll -- block (conceptually) until `device` is ready. See this
 * file's own header comment for why the real "yield via Supervisor"
 * tail collapses to an immediate return here. */
static void rtm_poll(io_in8_fn io_in8, uint8_t device) {
    (void)polldev_entry(io_in8, device);
}

/* -- CHARACTER I/O ---------------------------------------------------- */

/* const_entry -- Function 0: Console Status.
 *	output:	returns al (0xFF if a character is ready, else 0)
 */
uint8_t const_entry(io_in8_fn io_in8) {
    return polldev_entry(io_in8, c0indev);
}

/* xios_conin_entry -- Function 1: Console Input. Named with an
 * `xios_` prefix, not the bare real asm label `conin_entry` -- that
 * collides with CHARIO.CIO's own, genuinely different, real
 * `conin_entry` (chario.c) once this whole tree is linked into one
 * real image; see bdosif.c's own header comment for the same class
 * of rename, needed for the same reason. */
uint8_t xios_conin_entry(io_in8_fn io_in8, io_out8_fn io_out8) {
    (void)io_out8;
    rtm_poll(io_in8, c0indev);
    return io_in8(uart_rxtx);
}

/* xios_conout_entry -- Function 2: Console Output. Named with an
 * `xios_` prefix for the same reason as `xios_conin_entry` above.
 *	input:	cl = character to send
 */
void xios_conout_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl) {
    rtm_poll(io_in8, c0outdev);
    io_out8(uart_rxtx, cl);
}

/* plist_entry -- Function 3: List Output -- no list device. */
void plist_entry(void) {
}

/* punch_entry -- Function 4 / reader_entry -- Function 5: not
 * supported under MP/M-86; return EOF like the real XIOS. */
uint8_t punch_entry(void) {
    return 0x1A;
}

uint8_t reader_entry(void) {
    return 0x1A;
}

/* listst_entry -- Function 13: List Status -- no list device. */
uint8_t listst_entry(void) {
    return 0;
}

/* maxconsole_entry -- Function 20: Maximum Consoles. */
uint16_t maxconsole_entry(void) {
    return 1;
}

/* maxlist_entry -- Function 21: Maximum List Devices. */
uint16_t maxlist_entry(void) {
    return 0;
}

/* -- FLOPPY CONTROLLER (NEC 765/8272) DRIVER -------------------------- */

/* fdc_out_byte -- the byte waits in outbyte_tmp (SYSDAT) while the
 * FIFO isn't ready, as in the asm */
static void fdc_out_byte(uint8_t *sd, io_in8_fn io_in8, io_out8_fn io_out8, uint8_t val) {
    sd[XQ_OUTBYTE_TMP] = val;
    uint8_t al;
    do {
        al = io_in8(fdc_msr);
        al = (uint8_t)(al & (msr_rqm | msr_dio));
    } while (al != msr_rqm);
    io_out8(fdc_data, val);
}

static uint8_t fdc_in_byte(io_in8_fn io_in8) {
    uint8_t al;
    do {
        al = io_in8(fdc_msr);
        al = (uint8_t)(al & (msr_rqm | msr_dio));
    } while (al != (msr_rqm | msr_dio));
    return io_in8(fdc_data);
}

static void fdc_send_cmd(uint8_t *sd, io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *cmd) {
    uint8_t count = cmd[0];
    for (uint8_t i = 1; i <= count; i++) {
        fdc_out_byte(sd, io_in8, io_out8, cmd[i]);
    }
}

static void fdc_sense_int(uint8_t *sd, io_in8_fn io_in8, io_out8_fn io_out8) {
    fdc_out_byte(sd, io_in8, io_out8, cmd_sense_int);
    (void)fdc_in_byte(io_in8); /* ST0 (ignored) */
    (void)fdc_in_byte(io_in8); /* PCN (ignored) */
}

static void fdc_wait_seek(uint8_t *sd, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint16_t cx = 0;
    uint8_t al;
    do {
        al = io_in8(fdc_msr);
        cx = (uint16_t)(cx - 1);
    } while ((al & msr_busy) != 0 && cx != 0);
    fdc_sense_int(sd, io_in8, io_out8);
}

static void fdc_pio_in(io_in8_fn io_in8) {
    for (int i = 0; i < 7; i++) {
        (void)fdc_in_byte(io_in8);
    }
}

static void fdc_pio_out(io_in8_fn io_in8) {
    for (int i = 0; i < 7; i++) {
        (void)fdc_in_byte(io_in8);
    }
}

/* real `mov cx,0 ! md_loop: in al,dx ! loop md_loop` -- a real LOOP
 * starting at cx=0 wraps to 0xFFFF on its first decrement, so this
 * is a genuine 65536-iteration bounded delay, reproduced exactly. */
static void motor_delay(io_in8_fn io_in8) {
    uint16_t cx = 0;
    do {
        (void)io_in8(fdc_msr);
        cx = (uint16_t)(cx - 1);
    } while (cx != 0);
}

static void rw_setup(io_in8_fn io_in8, io_out8_fn io_out8) {
    io_out8(fdc_dor, dor_ndreset | dor_mota | dor_dmaen | dor_drvsel_a);
    motor_delay(io_in8);
}

/* dma_setup -- arm 8237 DMA channel 2 for one fd_secsize-byte
 * transfer into/out of secbuf (SYSDAT:secbuf); `mode` is
 * dma_mode_read/dma_mode_write. */
static void dma_setup(io_out8_fn io_out8, uint16_t sysdat, uint8_t mode) {
    io_out8(dma_mask, dma_mask_set2);
    io_out8(dma_clrff, 0x00);
    io_out8(dma_mode, mode);

    uint16_t ax = (uint16_t)(sysdat << 4);
    uint16_t bx = (uint16_t)(sysdat >> 12);
    uint32_t sum = (uint32_t)ax + (uint32_t)XQ_SECBUF;
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

/* rw_physical -- shared geometry math (trk/sect -> cur_cyl/cur_head/
 * cur_psec/cur_bufoff), 0-based logical sector -- see the real asm's
 * own comment on why this isn't 1-based. */
static void rw_physical(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t sect = sd[XQ_SECT];
    sd[XQ_CUR_PSEC] = (uint8_t)(sect / 4 + 1);
    seg_w16(sd, XQ_CUR_BUFOFF, (uint16_t)((sect % 4) * 128));

    uint8_t trk = sd[XQ_TRK];
    sd[XQ_CUR_CYL] = (uint8_t)(trk / fd_heads);
    sd[XQ_CUR_HEAD] = (uint8_t)(trk % fd_heads);
}

#define cmd_read  0x46
#define cmd_write 0x45
#define cmd_seek  0x0F
#define fd_spt       18
#define fd_secsize_n 2
#define fd_gap3      0x1B
#define fd_dtl       0xFF

/* fdc_fill_rw_parms -- (re)build the READ DATA/WRITE DATA command
 * tables from cur_head/cur_cyl/cur_psec. Writes the WHOLE table each
 * time, not just the head/cyl/sector fields the real asm's own
 * version patches in place -- this port's own `mem` has no
 * equivalent "preloaded image" when exercised standalone the way the
 * real asm's static dseg content does, so the static bytes
 * (length/cmd/N/SPT/GPL/DTL) need rebuilding too; same "simplified
 * without changing what's observable" precedent as
 * ldbios86q.c's own identical fix. */
static void fdc_fill_rw_parms(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t head = sd[XQ_CUR_HEAD];
    uint8_t cyl = sd[XQ_CUR_CYL];
    uint8_t psec = sd[XQ_CUR_PSEC];
    uint8_t hds = (uint8_t)(head << 2);
    const uint8_t read_tbl[10] = {9, cmd_read, hds, cyl, head, psec,
                                   fd_secsize_n, fd_spt, fd_gap3, fd_dtl};
    const uint8_t write_tbl[10] = {9, cmd_write, hds, cyl, head, psec,
                                    fd_secsize_n, fd_spt, fd_gap3, fd_dtl};
    memcpy(&sd[XQ_READ_PARMS], read_tbl, sizeof read_tbl);
    memcpy(&sd[XQ_WRITE_PARMS], write_tbl, sizeof write_tbl);
}

/* fdc_seek_to -- seek to cur_cyl/cur_head. Same "rewrite the whole
 * table" reasoning as fdc_fill_rw_parms above. */
static void fdc_seek_to(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t head = sd[XQ_CUR_HEAD];
    uint8_t cyl = sd[XQ_CUR_CYL];
    uint8_t hds = (uint8_t)(head << 2);
    const uint8_t seek_tbl[4] = {3, cmd_seek, hds, cyl};
    memcpy(&sd[XQ_SEEK_PARMS], seek_tbl, sizeof seek_tbl);
    fdc_send_cmd(sd, io_in8, io_out8, &sd[XQ_SEEK_PARMS]);
    fdc_wait_seek(sd, io_in8, io_out8);
}

/* -- DISK I/O ---------------------------------------------------------- */

/* disk_home_entry -- Function 6: Home. Seek drive 0 back to track 0. */
void disk_home_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    static const uint8_t recal_parms[] = {2, 0x07, 0x00};
    sd[XQ_TRK] = 0;
    fdc_send_cmd(sd, io_in8, io_out8, recal_parms);
    fdc_wait_seek(sd, io_in8, io_out8);
}

/* seldsk_entry -- Function 7: Select Disk.
 *	input:	cl = logical disk to select
 *	output:	returns ax (0 if illegal disk, else the DPH address)
 */
uint16_t seldsk_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    static const uint8_t dtrntbl[4] = {0x00, 0xff, 0xff, 0xff};
    uint8_t al = dtrntbl[cl & 0x03];
    if (al == 0xff) {
        /* `xor ax,ax` then AL is overwritten by the table read before
         * this branch, with AH never restored to 0 after -- AX comes
         * back as 0x00FF here, not a clean 0, reproduced exactly. */
        return al;
    }
    sd[XQ_DISK] = al;
    return XQ_DPH0;
}

/* settrk_entry -- Function 8: Set Track. */
void settrk_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    sd[XQ_TRK] = cl;
}

/* setsec_entry -- Function 9: Set Sector (128-byte, 0-based logical). */
void setsec_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    sd[XQ_SECT] = cl;
}

/* setdma_entry -- Function 10: Set DMA Offset. */
void setdma_entry(uint8_t *phys, uint16_t sysdat, uint16_t cx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    seg_w16(sd, XQ_DMA_ADR, cx);
}

/* setdmab_entry -- Function 15: Set DMA Base. */
void setdmab_entry(uint8_t *phys, uint16_t sysdat, uint16_t cx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    seg_w16(sd, XQ_DMA_SEG, cx);
}

/* sectran_entry -- Function 14: Sector Translate.
 *	input:	cx = sector number, dx = translate table address (0 = none)
 *	output:	returns ax (the translated sector number)
 */
uint16_t sectran_entry(uint8_t *phys, uint16_t sysdat, uint16_t cx, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (dx == 0) {
        return cx;
    }
    return sd[(uint16_t)(cx + dx)];
}

/* diskread_entry -- Function 11: Read.
 *	output:	returns al (0, always -- no error detection yet)
 */
uint8_t diskread_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    rw_setup(io_in8, io_out8);
    rw_physical(phys, sysdat);
    fdc_seek_to(phys, sysdat, io_in8, io_out8);
    fdc_fill_rw_parms(phys, sysdat);
    dma_setup(io_out8, sysdat, dma_mode_read);
    fdc_send_cmd(sd, io_in8, io_out8, &sd[XQ_READ_PARMS]);
    fdc_pio_in(io_in8);

    uint16_t cur_bufoff = seg_r16(sd, XQ_CUR_BUFOFF);
    uint16_t dma_adr = seg_r16(sd, XQ_DMA_ADR);
    uint16_t dma_seg = seg_r16(sd, XQ_DMA_SEG);
    for (uint16_t i = 0; i < 128; i++) {
        uint8_t b = sd[(uint16_t)(XQ_SECBUF + cur_bufoff + i)];
        seg_w8(far_ptr(phys, dma_seg, 0), (uint16_t)(dma_adr + i), b);
    }
    return 0;
}

/* diskwrite_entry -- Function 12: Write (read/modify/write).
 *	output:	returns al (0, always -- no error detection yet)
 */
uint8_t diskwrite_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    rw_setup(io_in8, io_out8);
    rw_physical(phys, sysdat);
    fdc_seek_to(phys, sysdat, io_in8, io_out8);

    fdc_fill_rw_parms(phys, sysdat);
    dma_setup(io_out8, sysdat, dma_mode_read);
    fdc_send_cmd(sd, io_in8, io_out8, &sd[XQ_READ_PARMS]);
    fdc_pio_in(io_in8);

    uint16_t cur_bufoff = seg_r16(sd, XQ_CUR_BUFOFF);
    uint16_t dma_adr = seg_r16(sd, XQ_DMA_ADR);
    uint16_t dma_seg = seg_r16(sd, XQ_DMA_SEG);
    for (uint16_t i = 0; i < 128; i++) {
        uint8_t b = seg_r8(far_ptr(phys, dma_seg, 0), (uint16_t)(dma_adr + i));
        sd[(uint16_t)(XQ_SECBUF + cur_bufoff + i)] = b;
    }

    fdc_fill_rw_parms(phys, sysdat);
    dma_setup(io_out8, sysdat, dma_mode_write);
    fdc_send_cmd(sd, io_in8, io_out8, &sd[XQ_WRITE_PARMS]);
    fdc_pio_out(io_in8);
    return 0;
}

/* -- HOUSEKEEPING ------------------------------------------------------ */

/* getsegt_entry -- Function 16: Get Segment Table -- not supported. */
uint16_t getsegt_entry(void) {
    return 0xffff;
}

/* selmemory_entry -- Function 22: Select Memory -- single bank, no-op. */
void selmemory_entry(void) {
}

/* flushbuf_entry -- Function 24: Flush Buffer -- nothing buffered. */
uint8_t flushbuf_entry(void) {
    return 0;
}

/* strtclk_entry -- Function 18: Start Clock. */
void strtclk_entry(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    sd[XQ_CLOCKON] = 0xff;
}

/* stopclk_entry -- Function 19: Stop Clock. */
void stopclk_entry(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    sd[XQ_CLOCKON] = 0x00;
}

/* -- UTILITY: print a null-terminated string via CONOUT --------------- */

static void pmsg(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8, const uint8_t *msg) {
    (void)phys;
    (void)sysdat;
    for (; *msg != 0; msg++) {
        xios_conout_entry(io_in8, io_out8, *msg);
    }
}

/* -- INITIALIZATION ----------------------------------------------------- */

/* init_entry -- arm the UART/PIT/PIC/FDC hardware and print the
 * signon message. See this file's own header comment for what's not
 * ported (the far-pointer copies out of SYSDAT's module table --
 * genuinely external state this isolated port has nothing to copy
 * to/from -- and the final `retf`).
 */
void init_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    io_out8(0x3FB, 0x80); /* uart_lcr: set DLAB */
    io_out8(uart_rxtx, 0x0C); /* divisor low byte */
    io_out8(0x3F9, 0x00); /* uart_ier: divisor high byte */
    io_out8(0x3FB, 0x03); /* uart_lcr: 8N1, DLAB off */
    io_out8(0x3F9, 0x00); /* uart_ier: no UART interrupts */
    io_out8(0x3FC, 0x03); /* uart_mcr: assert DTR/RTS */

    io_out8(0x43, 0x36); /* pit_cmd: ch0, lo/hi, mode 3, binary */
    io_out8(0x40, 0xAE); /* pit_ch0: divisor low byte (0x4DAE) */
    io_out8(0x40, 0x4D); /* pit_ch0: divisor high byte */
    sd[XQ_TICK_COUNT] = 60;

    uint8_t mask = io_in8(0x21);
    io_out8(0x21, (uint8_t)(mask & 0xFE)); /* unmask IRQ0 */
    mask = io_in8(0x21);
    io_out8(0x21, (uint8_t)(mask | 0x06)); /* mask IRQ1/IRQ2 */
    mask = io_in8(0x21);
    io_out8(0x21, (uint8_t)(mask | 0x40)); /* mask IRQ6 */

    io_out8(fdc_dor, dor_ndreset | dor_mota | dor_dmaen | dor_drvsel_a);
    static const uint8_t specify_parms[] = {3, 0x03, 0xDF, 0x02};
    fdc_send_cmd(sd, io_in8, io_out8, specify_parms);

    sd[XQ_CLOCKON] = 0x00;

    pmsg(phys, sysdat, io_in8, io_out8, &sd[XQ_SIGNON]);
}

/* -- TICK INTERRUPT (logic only -- see this file's own header comment) - */

typedef struct {
    uint8_t sec_flag_set;
    uint8_t tick_flag_set;
} tickint_result;

/* tickint_once -- the real ISR's own tick_count countdown and flag
 * decisions, one tick. */
tickint_result tickint_once(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    tickint_result r = {0, 0};
    uint8_t tick_count = (uint8_t)(sd[XQ_TICK_COUNT] - 1);
    sd[XQ_TICK_COUNT] = tick_count;
    if (tick_count == 0) {
        sd[XQ_TICK_COUNT] = 60;
        r.sec_flag_set = 1;
    }
    if (sd[XQ_CLOCKON] == 0xff) {
        r.tick_flag_set = 1;
    }
    return r;
}
