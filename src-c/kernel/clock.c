/*
 * clock.c -- C port of src/rsp/CLOCK.A86's clockloop: body.
 *
 * CLOCK is a whole Resident System Process: `clock:` runs its own
 * one-time setup (DS=sysdat, BX=tod_offset) then falls into
 * `clockloop:`, which never returns (every branch ends in `jmp
 * clockloop`, fixed from a real /src typo -- see docs/bugs.md #1).
 * Per docs/src-c.md's boundary-stub convention, this port models
 * exactly one pass through that loop, stopping where the real code
 * would jump back to the top (tests/src-c/test_clock.py stops the
 * real routine at clockloop's own address, same precedent as
 * src-c/kernel/echo.c's own `loop:`).
 *
 * Every real syscall CLOCK makes goes through its own `mpm:`
 * trampoline (`int mpmint ! ret`); since this port never needs its
 * return value (same reasoning as echo.c), it's a call_stub_addrs
 * boundary stub in the test and isn't modeled here beyond the
 * register setup right before each call (mpm_flagw/sec_flag,
 * mpm_flags/min_flag) -- neither has an observable effect under the
 * convention.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the SYSDAT segment -- the asm's
 * `mov ds,sysdat ! mov bx,tod_offset` setup, which this loop body runs
 * under: BX -> the TOD structure in SYSDAT (day:word, hour:byte,
 * min:byte, sec:byte, at SYSDAT_TOD).
 */
#include <stdint.h>

#include "defs.h"

#define TOD_DAY  (SYSDAT_TOD + 0) /* word */
#define TOD_HOUR (SYSDAT_TOD + 2) /* byte */
#define TOD_MIN  (SYSDAT_TOD + 3) /* byte */
#define TOD_SEC  (SYSDAT_TOD + 4) /* byte */

/* bcd_inc -- "clc ! inc al ! daa": increment a 2-digit BCD byte by 1 */
static uint8_t bcd_inc(uint8_t al) {
    al = (uint8_t)(al + 1);
    if ((al & 0x0F) > 9) {
        al = (uint8_t)(al + 6);
    }
    if (al > 0x9F) {
        al = (uint8_t)(al + 0x60);
    }
    return al;
}

/* clockloop -- one pass through the clock process's own main loop */
void clockloop(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    /* mov cx,mpm_flagw ! mov dx,sec_flag ! call mpm -- boundary stub, no-op */
    uint8_t al = bcd_inc(seg_r8(sd, TOD_SEC));
    seg_w8(sd, TOD_SEC, al);
    if (al < 0x60) {
        return; /* back to clockloop */
    }
    /* update_min */
    seg_w8(sd, TOD_SEC, 0);
    /* mov cx,mpm_flags ! mov dx,min_flag ! call mpm -- boundary stub, no-op */
    al = bcd_inc(seg_r8(sd, TOD_MIN));
    seg_w8(sd, TOD_MIN, al);
    if (al < 0x60) {
        return;
    }
    /* update_hour */
    seg_w8(sd, TOD_MIN, 0);
    al = bcd_inc(seg_r8(sd, TOD_HOUR));
    seg_w8(sd, TOD_HOUR, al);
    if (al < 0x24) {
        return;
    }
    /* update_day */
    seg_w8(sd, TOD_HOUR, 0);
    seg_w16(sd, TOD_DAY, (uint16_t)(seg_r16(sd, TOD_DAY) + 1));
}
