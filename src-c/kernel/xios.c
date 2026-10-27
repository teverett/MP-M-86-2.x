/*
 * xios.c -- C port of src-assem/kernel/XIOS.A86's portable function-set
 * routines (the real driver's "logic" half: disk-parameter bookkeeping
 * and a handful of fixed-answer status functions).
 *
 * XIOS.A86 is docs/bugs.md #7's file (fixed now that its own last
 * typo, `dp_base`/`dpbase`, is patched) and the real Intel SBC
 * 204/534-specific hardware driver for MP/M-86's XIOS module -- see
 * src-assem/Makefile's own KERNEL_OK comment. Most of the file is genuine
 * port-level hardware I/O (`in`/`out` against the 8271 floppy
 * controller and the console/list UARTs) or interrupt-service code,
 * neither of which has a C equivalent under this project's flat-memory
 * host model (same "no sensible C translation" class as the real
 * interrupt-entry/exit prologues already excluded elsewhere -- see
 * docs/src-c.md's scope notes). Deliberately NOT ported here, for that
 * reason:
 *   - entry/supif -- far-call trampolines, same convention already
 *     applied to every other module's entry/mpmif/xiosif
 *   - const/conin/conout/plist/punch/reader(no -- see below)/listst/
 *     polldev/rtm_poll -- real `in`/`out` port I/O
 *   - HOME/READ/WRITE/execute/send_com/flpy_* -- real 8271 floppy
 *     controller command sequences (every dskhomtbl/dskrdtbl/dskwrttbl
 *     entry is a flpy_* routine; there is no non-hardware path)
 *   - init/intdisp/int_trap/tickint/pmsg -- real hardware
 *     initialization (DMA controller, 8259 PIC, 8271) and interrupt
 *     service, no C equivalent
 *   - idle -- a pure `mov cl,f_dispatch ! call supif ! jmp idle`
 *     passthrough with zero data transformation of its own (the call
 *     itself is a boundary stub under this project's own convention,
 *     same as SYSENT.RTM's own poll_entry); porting it would add no
 *     real logic, so it's left out like IDLE.SUP's own distinct
 *     `idle:` (src-c/kernel/idle.c) was NOT reused here -- they are
 *     two different real routines that happen to share a label name
 *   - punch/reader -- trivial (`mov al,1ah ! ret`, PUNCH/READER are
 *     unsupported under MP/M-86) but omitted anyway since they carry
 *     no disk/console state and nothing else in this port calls them
 *
 * What's left -- SETTRK/SETSEC/SETDMA/SETDMAB/SECTRAN/SELDSK's own
 * disk-parameter bookkeeping, STRTCLK/STOPCLK's clockon flag, and the
 * fixed-answer MAXCONSOLE/MAXLIST/GETSEGT/SELMEMORY/FLUSHBUF -- is
 * ordinary data-in-data-out logic, ported directly.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the XIOS's segment -- SYSDAT, which
 * this XIOS lives in (CS = DS) -- holding its fields at their real
 * XIOS.A86 offsets (XIOS_* in defs.h).
 */
#include <stdint.h>

#include "defs.h"

/* settrk -- Function 8: Set Track. Real code only moves CL (8 bits). */
void settrk(uint8_t *phys, uint16_t sysdat, uint8_t cl) {
    seg_w8(far_ptr(phys, sysdat, 0), XIOS_TRK, cl);
}

/* setsec -- Function 9: Set Sector. Real code only moves CL (8 bits). */
void setsec(uint8_t *phys, uint16_t sysdat, uint8_t cl) {
    seg_w8(far_ptr(phys, sysdat, 0), XIOS_SECT, cl);
}

/* setdma -- Function 10: Set DMA Offset */
void setdma(uint8_t *phys, uint16_t sysdat, uint16_t cx) {
    seg_w16(far_ptr(phys, sysdat, 0), XIOS_DMA_ADR, cx);
}

/* setdmab -- Function 15: Set DMA Base (segment) */
void setdmab(uint8_t *phys, uint16_t sysdat, uint16_t cx) {
    seg_w16(far_ptr(phys, sysdat, 0), XIOS_DMA_SEG, cx);
}

/* sectran -- Function 14: Sector Translate. If the translate table
 * address (dx) is 0, the sector number passes through unchanged;
 * otherwise it's a byte offset into a real caller-supplied table. */
uint16_t sectran(uint8_t *phys, uint16_t sysdat, uint16_t cx, uint16_t dx) {
    if (dx == 0) {
        return cx;
    }
    return seg_r8(far_ptr(phys, sysdat, 0), (uint16_t)(cx + dx));
}

/* seldsk -- Function 7: Select Disk.
 * Returns 0 if cl names an illegal disk, else the DPH address for it
 * (dpbase + 16 * physical drive number). dtrntbl is XIOS.A86's
 * logical-to-physical table (A-P, memdisk disabled -- only A/B are
 * valid, drives 0/1), indexed by the whole of CL as the asm does; the
 * `shl al,cl` multiply is 8 bits wide. */
uint16_t seldsk(uint8_t *phys, uint16_t sysdat, uint8_t cl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t al = seg_r8(sd, (uint16_t)(XIOS_DTRNTBL + cl));
    if (al == 0xff) {
        return 0;
    }
    seg_w8(sd, XIOS_DISK, al);
    return (uint16_t)((uint8_t)(al << 4) + XIOS_DPBASE);
}

/* maxconsole -- Function 20: Maximum Consoles (XIOS.A86's own `nconsoles equ 4`) */
uint16_t maxconsole(void) {
    return 4;
}

/* maxlist -- Function 21: Maximum List Devices (XIOS.A86's own `nlists equ 1`) */
uint16_t maxlist(void) {
    return 1;
}

/* getsegt -- Function 16: Get Segment Table. Not supported by MP/M-86. */
uint16_t getsegt(void) {
    return 0xffff;
}

/* selmemory -- Function 22: Select Memory. No-op. */
void selmemory(void) {
}

/* flushbuf -- Function 24: Flush Buffer. No blocking/deblocking buffer to flush. */
uint8_t flushbuf(void) {
    return 0;
}

/* strtclk -- Function 18: Start Clock (enable flagsets on tick interrupts).
 * `mov clockon,true` -- true is SYSTEM.DEF's own 0ffffh, truncated to
 * clockon's own byte size. */
void strtclk(uint8_t *phys, uint16_t sysdat) {
    seg_w8(far_ptr(phys, sysdat, 0), XIOS_CLOCKON, 0xff);
}

/* stopclk -- Function 19: Stop Clock (disable flagsets on tick interrupts) */
void stopclk(uint8_t *phys, uint16_t sysdat) {
    seg_w8(far_ptr(phys, sysdat, 0), XIOS_CLOCKON, 0);
}
