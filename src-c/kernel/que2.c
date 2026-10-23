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
 */
#include <stdint.h>

#include "defs.h"

uint16_t getqdaddr(uint8_t *mem, uint16_t dx, uint16_t sysdat, uint16_t *cx_out);
void qspace(uint8_t *mem, uint16_t si, uint16_t *cx_out);
void qrelease(uint8_t *mem, uint16_t si);

/*
 * rlsmx -- Release all MX queues held by a given process
 *	input:	si = pd address
 */
void rlsmx(uint8_t *mem, uint16_t si) {
    uint16_t di = SD_QLR;
    for (;;) {
        uint16_t bx = mem_r16(mem, (uint16_t)(di + q_link));
        if (bx == 0) break;
        if ((mem_r16(mem, (uint16_t)(bx + q_flags)) & qf_mx) &&
            si == mem_r16(mem, (uint16_t)(bx + q_buf))) {
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
 *		sysdat = this module's own copy of the SYSDAT segment
 *		(real asm: `cs:sysdat`)
 *	output:	returns offset of qd in MPM CSEG (on the "already in MPM"
 *		path) or the original dx, unmodified (on every path through
 *		gqd_bad -- the real asm never touches dx again once it's
 *		popped back there)
 *		*cx_out = 0, e_no_qd, or whatever qspace left it as (the
 *		real asm never resets cx after qspace returns)
 */
uint16_t getqdaddr(uint8_t *mem, uint16_t dx, uint16_t sysdat, uint16_t *cx_out) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t ax = (uint16_t)(wrkseg + (dx >> 4));
    uint16_t endseg = mem_r16(mem, SD_ENDSEG);
    if (ax >= sysdat && ax < endseg && ax < (uint16_t)(sysdat + 0x1000)) {
        /* the real asm recomputes the offset from the original wrkseg here,
         * not from `ax` above (which it's already popped over by this point) */
        uint16_t off = (uint16_t)((wrkseg - sysdat) << 4);
        uint16_t si = (uint16_t)(dx + off);
        if (mem_r16(mem, (uint16_t)(si + q_buf)) != 0) {
            mem_w16(mem, (uint16_t)(si + q_buf), (uint16_t)(mem_r16(mem, (uint16_t)(si + q_buf)) + off));
        }
        *cx_out = 0;
        return si;
    }

    /* gqd_bad -- dx is popped back to its original value here and never
     * written again on any path out of this routine */
    uint16_t di = mem_r16(mem, SD_QUL);
    if (di == 0) {
        *cx_out = e_no_qd;
        return dx;
    }
    mem_w16(mem, SD_QUL, mem_r16(mem, (uint16_t)(di + q_link)));
    for (unsigned i = 0; i < qdlen; i++) mem[(uint16_t)(di + i)] = mem[(uint16_t)(dx + i)];
    mem_w16(mem, (uint16_t)(di + q_flags), (uint16_t)(mem_r16(mem, (uint16_t)(di + q_flags)) | qf_table));

    uint16_t qcx;
    qspace(mem, di, &qcx);
    if (qcx != 0) {
        mem_w16(mem, (uint16_t)(di + q_link), mem_r16(mem, SD_QUL));
        mem_w16(mem, SD_QUL, di);
    }
    *cx_out = qcx;
    return di;
}

/*
 * remqd -- Place qd on queue unused list
 *	input:	si = qd address
 */
void remqd(uint8_t *mem, uint16_t si) {
    if (!(mem_r16(mem, (uint16_t)(si + q_flags)) & qf_table)) return;
    mem_w16(mem, (uint16_t)(si + q_link), mem_r16(mem, SD_QUL));
    mem_w16(mem, SD_QUL, si);
    qrelease(mem, si);
}

/*
 * qspace -- find buffer space for qd
 *	input:	si = QD address
 *	output:	*cx_out = error code (always nonzero here -- see header comment)
 */
void qspace(uint8_t *mem, uint16_t si, uint16_t *cx_out) {
    (void)mem;
    (void)si;
    /* mov cx,f_maualloc ! call mpmif -- boundary stub, no-op */
    *cx_out = f_maualloc;
}

/*
 * qrelease -- release buffer space for qd
 *	input:	si = QD address
 */
void qrelease(uint8_t *mem, uint16_t si) {
    (void)mem;
    (void)si;
    /* mov cx,f_maufree ! call mpmif -- boundary stub, no-op; qrelease never
     * reads anything back afterward, so there's nothing else to do here */
}
