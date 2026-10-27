/*
 * que2.c -- C port of 5 of src-assem/kernel/QUE2.RTM's 6 routines: rlsmx,
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
 * `remqd` below matches src-assem/kernel/QUE2.RTM as it now reads after
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

MPMIF_HOOK(que2) /* f_cqwrite/f_maualloc/f_maufree (defs.h) */

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
            si == seg_r16(sd, (uint16_t)(bx + q_buf)) && que2_mpmif) {
            /* push bx ! sub ax,ax ! push ax: a QPB {flags 0, qaddr bx} on
             * the stack, DS = SS; mov cx,f_cqwrite ! call mpmif (with no
             * hook -- a differential test -- there's nothing to call) */
            uint16_t uda_seg = rlr_uda(sd);
            uint8_t *uda = far_ptr(phys, uda_seg, 0);
            seg_w16(uda, UDA_STK_QPB, 0);
            seg_w16(uda, (uint16_t)(UDA_STK_QPB + qpb_qaddr), bx);
            (void)MPMIF(que2, phys, sysdat, f_cqwrite, UDA_STK_QPB, 0, uda_seg);
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
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (!que2_mpmif) {
        /* no MEM to call (a differential test): the old stub, which
         * leaves CX = f_maualloc -- see header comment */
        *cx_out = f_maualloc;
        return;
    }
    /* mov ax,q_msglen[si] ! mul q_nmsgs[si]: the buffer size; an MPB
     * {start 0, min = max = size, pdadr 0, flags 0} on the stack (DS =
     * SS), bx = offset qmau; mov cx,f_maualloc ! call mpmif */
    uint16_t size = (uint16_t)(seg_r16(sd, (uint16_t)(si + q_msglen)) * seg_r16(sd, (uint16_t)(si + q_nmsgs)));
    uint16_t uda_seg = rlr_uda(sd);
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    seg_w16(uda, (uint16_t)(UDA_STK_QMPB + mpb_start), 0);
    seg_w16(uda, (uint16_t)(UDA_STK_QMPB + mpb_min), size);
    seg_w16(uda, (uint16_t)(UDA_STK_QMPB + mpb_max), size);
    seg_w16(uda, (uint16_t)(UDA_STK_QMPB + mpb_pdadr), 0);
    seg_w16(uda, (uint16_t)(UDA_STK_QMPB + mpb_flags), 0);
    mpm_regs r = MPMIF(que2, phys, sysdat, f_maualloc, UDA_STK_QMPB, SYSDAT_QMAU, uda_seg);
    *cx_out = r.cx;
    if (r.cx == 0) {
        seg_w16(sd, (uint16_t)(si + q_buf), seg_r16(uda, (uint16_t)(UDA_STK_QMPB + mpb_start)));
    }
}

/*
 * qrelease -- release buffer space for qd
 *	input:	si = QD address
 */
void qrelease(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    /* push ax ! push ax ! push cx: a MAF {mau = qmau, sat = start =
     * q_buf} on the stack (DS = SS); mov cx,f_maufree ! call mpmif.
     * Nothing is read back. */
    uint16_t buf = seg_r16(sd, (uint16_t)(si + q_buf));
    uint16_t uda_seg = rlr_uda(sd);
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    if (!que2_mpmif) return;
    seg_w16(uda, (uint16_t)(UDA_STK_QMAF + maf_mau), SYSDAT_QMAU);
    seg_w16(uda, (uint16_t)(UDA_STK_QMAF + maf_sat), buf);
    seg_w16(uda, (uint16_t)(UDA_STK_QMAF + maf_start), buf);
    (void)MPMIF(que2, phys, sysdat, f_maufree, UDA_STK_QMAF, 0, uda_seg);
}
