/*
 * que2.c -- C port of 5 of src/kernel/QUE2.RTM's 6 routines: rlsmx,
 * getqdaddr, remqd, qspace, qrelease.
 *
 * Every real allocator/scheduler call here goes through the unported
 * `mpmif` trampoline (`f_cqwrite`/`f_maualloc`/`f_maufree`) and is a
 * boundary stub per docs/src-c.md's convention: modeled as an instant
 * no-op return. For `qspace` specifically, that has a real consequence
 * worth calling out: the real routine sets `cx = f_maualloc` immediately
 * before the call and only checks `cx == 0` afterward (the real
 * allocator's own success code) -- since a no-op call never clears it,
 * the "allocation succeeded" path is unreachable under this convention
 * (exercised the same way by both tools/i86sim.py and this port, so the
 * differential test still agrees, it's just never testing that path).
 *
 * `remqd` below matches src/kernel/QUE2.RTM as it now reads after
 * docs/bugs.md #99's fix (`mov qul,si`, not the original's `mov qul,ax`,
 * which left `qul` set to its own unchanged value -- a no-op that meant
 * a freed QD was never actually threaded onto the free list).
 *
 * Runs against real memory (defs.h's far_ptr), like the rest of the
 * RTM: `phys` is the base of real physical memory and `sysdat` the real
 * SYSDAT segment (the real asm's DS and cs:sysdat). QDs and the qlr/qul
 * lists are SYSDAT-relative; the caller's own QD (getqdaddr's dx) is in
 * the running process's u_wrkseg, read from its UDA (the real asm's
 * ambient ES, p_uda[rlr]) -- so getqdaddr's copy into the QD table
 * really goes from u_wrkseg:dx to sysdat:di.
 */
#include <stdint.h>

#include "defs.h"

uint16_t getqdaddr(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *cx_out);
void qspace(uint8_t *phys, uint16_t sysdat, uint16_t si, uint16_t *cx_out);
void qrelease(uint8_t *phys, uint16_t sysdat, uint16_t si);

/*
 * rlsmx -- Release all MX queues held by a given process
 *	input:	si = pd address
 */
void rlsmx(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t di = (uint16_t)(SYSDAT_QLR - q_link);
    for (;;) {
        uint16_t bx = seg_r16(sd, (uint16_t)(di + q_link));
        if (bx == 0) break;
        if ((seg_r16(sd, (uint16_t)(bx + q_flags)) & qf_mx) &&
            si == seg_r16(sd, (uint16_t)(bx + q_buf))) {
            /* call mpmif(f_cqwrite) -- boundary stub, no-op */
        }
        di = bx;
    }
}

/*
 * getqdaddr -- Check qd address. If within MPM, use it else grab qd from
 * table and initialize with buffer space, returning its address. Also
 * sets qf_table flag.
 *	input:	dx = offset of qd in u_wrkseg
 *	output:	returns offset of qd in SYSDAT (on the "already in MPM"
 *		path) or the original dx, unmodified (on every path through
 *		gqd_bad -- the real asm never touches dx again once it's
 *		popped back there)
 *		*cx_out = 0, e_no_qd, or whatever qspace left it as (the
 *		real asm never resets cx after qspace returns)
 */
uint16_t getqdaddr(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *cx_out) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t wrkseg = seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg);
    uint16_t ax = (uint16_t)(wrkseg + (dx >> 4));
    uint16_t endseg = seg_r16(sd, SYSDAT_ENDSEG);
    if (ax >= sysdat && ax < endseg && ax < (uint16_t)(sysdat + 0x1000)) {
        /* the real asm recomputes the offset from the original wrkseg here,
         * not from `ax` above (which it's already popped over by this point) */
        uint16_t off = (uint16_t)((wrkseg - sysdat) << 4);
        uint16_t si = (uint16_t)(dx + off);
        if (seg_r16(sd, (uint16_t)(si + q_buf)) != 0) {
            seg_w16(sd, (uint16_t)(si + q_buf), (uint16_t)(seg_r16(sd, (uint16_t)(si + q_buf)) + off));
        }
        *cx_out = 0;
        return si;
    }

    /* gqd_bad -- dx is popped back to its original value here and never
     * written again on any path out of this routine */
    uint16_t di = seg_r16(sd, SYSDAT_QUL);
    if (di == 0) {
        *cx_out = e_no_qd;
        return dx;
    }
    seg_w16(sd, SYSDAT_QUL, seg_r16(sd, (uint16_t)(di + q_link)));
    const uint8_t *src = far_ptr(phys, wrkseg, 0);
    for (unsigned i = 0; i < qdlen; i++) {
        seg_w8(sd, (uint16_t)(di + i), seg_r8(src, (uint16_t)(dx + i)));
    }
    seg_w16(sd, (uint16_t)(di + q_flags), (uint16_t)(seg_r16(sd, (uint16_t)(di + q_flags)) | qf_table));

    uint16_t qcx;
    qspace(phys, sysdat, di, &qcx);
    if (qcx != 0) {
        seg_w16(sd, (uint16_t)(di + q_link), seg_r16(sd, SYSDAT_QUL));
        seg_w16(sd, SYSDAT_QUL, di);
    }
    *cx_out = qcx;
    return di;
}

/*
 * remqd -- Place qd on queue unused list
 *	input:	si = qd address
 */
void remqd(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (!(seg_r16(sd, (uint16_t)(si + q_flags)) & qf_table)) return;
    seg_w16(sd, (uint16_t)(si + q_link), seg_r16(sd, SYSDAT_QUL));
    seg_w16(sd, SYSDAT_QUL, si);
    qrelease(phys, sysdat, si);
}

/*
 * qspace -- find buffer space for qd
 *	input:	si = QD address
 *	output:	*cx_out = error code (always nonzero here -- see header comment)
 */
void qspace(uint8_t *phys, uint16_t sysdat, uint16_t si, uint16_t *cx_out) {
    (void)phys;
    (void)sysdat;
    (void)si;
    /* mov cx,f_maualloc ! call mpmif -- boundary stub, no-op */
    *cx_out = f_maualloc;
}

/*
 * qrelease -- release buffer space for qd
 *	input:	si = QD address
 */
void qrelease(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    (void)phys;
    (void)sysdat;
    (void)si;
    /* mov cx,f_maufree ! call mpmif -- boundary stub, no-op; qrelease never
     * reads anything back afterward, so there's nothing else to do here */
}
