/*
 * sysfunc.c -- C port of all 8 entries in src/kernel/SYSFUNC.SUP.
 *
 * `sysdat_entry`/`tod_entry`/`serial_entry` were deferred as "copy from
 * module-local data this port doesn't have a home for yet" -- revisited:
 * `tod` is a real `SYSDAT.FMT` field (`tod`/`tod_day`/`tod_hr`/`tod_min`/
 * `tod_sec`), given its own harness-chosen scratch address (`SD_TOD`,
 * `todlen`=5) same as every other such field elsewhere in this tree;
 * `serial` is a fixed, literal 6-character string constant in
 * `RTMIF.RTM`'s own cseg data (`serial db '654321'`), modeled the same
 * way `chario.c`'s own `write_cstr` stands in for CS-resident literal
 * data (just inlined directly here, it's a single fixed value, not a
 * runtime-chosen message). `sysdat_entry` itself is trivial, same
 * `u_retseg=ds` pattern as `SYSENT.RTM`'s own already-ported `pd_entry`.
 *
 * `callbios_entry` was deferred as "calls into unported mpmif/xiosif" --
 * revisited: only one path (`BOOT`/`WBOOT`, `al<=1`) tail-jumps into
 * `mpmif(f_terminate)`, a true boundary stub with nothing to port past
 * it (same class as `CPMMEM.MEM`'s own `freeall_entry`); a second path
 * (`al` outside `{0..7,15}`) is a real, fully observable `RET`
 * (`bx=cx=0xFFFF`); the third (`al` in `{2..7,15}`) tail-jumps into
 * `xiosif` with `al`/`ah`/`cl`/`dl` set up first -- per docs/src-c.md's
 * boundary-stub convention, this port's job there is exactly that
 * register state, same as `SYSENT.RTM`'s own already-ported `poll_entry`/
 * `delay_entry`/etc. stopping at their own dispatcher tail-jumps. `cl`'s
 * own "stays whatever it already was" branches (`al` in `{4..7}`) are
 * NOT a caller-register input at all, despite looking like one at
 * first glance: the real asm's own very first instructions load `cx`
 * from the input block's own 2nd field (`mov cx,1[si]`), and those
 * branches simply never overwrite it again -- see `callbios_entry`'s
 * own parameter comment below.
 */
#include <stdint.h>

#include "defs.h"

/* not_implemented -- Function not implemented */
uint16_t not_implemented(uint16_t *cx_out) {
    *cx_out = e_not_implemented;
    return 0xFFFF;
}

/* bad_entry -- Illegal System Function */
uint16_t bad_entry(uint16_t *cx_out) {
    *cx_out = e_bad_entry;
    return 0xFFFF;
}

/* version_entry -- Get Version # */
uint16_t version_entry(const uint8_t *mem, uint16_t *cx_out) {
    *cx_out = 0;
    return mem_r16(mem, SD_VERNUM);
}

/* mpmversion_entry -- Get MP/M Version # */
uint16_t mpmversion_entry(const uint8_t *mem, uint16_t *cx_out) {
    *cx_out = 0;
    return mem_r16(mem, SD_MPMVERNUM);
}

/* sysdat_entry -- Ret Addr of System Data Area
 *	input:	ds = this process's current data segment
 *	output:	returns bx=cx=0
 */
uint16_t sysdat_entry(uint8_t *mem, uint16_t ds, uint16_t *cx_out) {
    mem_w16(mem, UDA_RETSEG, ds);
    *cx_out = 0;
    return 0;
}

/* tod_entry -- Return current Time of Day
 *	input:	dx = address of a todlen-byte buffer in u_wrkseg
 *	output:	copies the system's own tod struct into it; returns bx=cx=0
 */
uint16_t tod_entry(uint8_t *mem, uint16_t dx, uint16_t *cx_out) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    for (unsigned i = 0; i < todlen; i++) {
        mem_w8_seg(mem, wrkseg, (uint16_t)(dx + i), mem[SD_TOD + i]);
    }
    *cx_out = 0;
    return 0;
}

/* serial_entry -- Return Serial Number
 *	input:	dx = address of a 6-byte buffer in u_wrkseg
 *	output:	copies RTMIF.RTM's own fixed `serial db '654321'` into it;
 *		returns bx=cx=0
 */
uint16_t serial_entry(uint8_t *mem, uint16_t dx, uint16_t *cx_out) {
    static const uint8_t serial[6] = {'6', '5', '4', '3', '2', '1'};
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    for (unsigned i = 0; i < 6; i++) {
        mem_w8_seg(mem, wrkseg, (uint16_t)(dx + i), serial[i]);
    }
    *cx_out = 0;
    return 0;
}

typedef struct {
    uint16_t bx, cx;
    uint8_t reached_xios; /* 0 = real bx/cx return, 1 = tail-jumped
        into mpmif(f_terminate) (BOOT/WBOOT, boundary stub, nothing
        else to check), 2 = tail-jumped into xiosif with al/ah/cl/dl
        set below */
    uint8_t al, ah, cl, dl;
} callbios_result;

/* callbios_entry -- Direct BIOS call
 *	input:	dx = address of a 4-byte block in u_wrkseg: byte 0 is the
 *		BIOS function number (al); byte 1 (the low byte of the
 *		block's own 2nd field, `mov cx,1[si]`) is read into cl
 *		and, on the al in {4..7} paths, is never overwritten
 *		again -- genuinely observable, not a caller-register
 *		input (the block's 3rd field, `mov dx,3[si]`, by
 *		contrast really is dead on every path, nothing to port
 *		for it)
 */
callbios_result callbios_entry(uint8_t *mem, uint16_t dx) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint8_t al = mem_r8_seg(mem, wrkseg, dx);

    if (al <= 1) {
        callbios_result r = {0, 0, 1, 0, 0, 0, 0};
        return r;
    }
    if (!(al <= 7 || al == 15)) {
        callbios_result r = {0xFFFF, 0xFFFF, 0, 0, 0, 0, 0};
        return r;
    }

    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint8_t dl, cl = mem_r8_seg(mem, wrkseg, (uint16_t)(dx + 1));
    if (al <= 4) {
        dl = mem[(uint16_t)(rlr + p_cns)];
        if (al != 4) cl = dl;
    } else {
        dl = mem[(uint16_t)(rlr + p_lst)];
        if (al == 15) cl = dl;
    }
    callbios_result r = {0, 0, 2, (uint8_t)(al - 2), 0, cl, dl};
    return r;
}
