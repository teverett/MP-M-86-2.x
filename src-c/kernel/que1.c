/*
 * que1.c -- C port of all 10 of src/kernel/QUE1.RTM's routines:
 * makeq_entry, openq_entry, deleteq_entry, queverify, readq_entry,
 * creadq_entry, readq, writeq_entry, cwriteq_entry, writeq.
 *
 * `getqdaddr`/`remqd` (src/kernel/QUE2.RTM, que2.c) are called here as
 * ordinary internal calls, not boundary stubs -- they're ported too.
 *
 * `deleteq_entry`'s `qd_noq` path (the QD being deleted isn't actually on
 * `qlr`) matches src/kernel/QUE1.RTM as it now reads after docs/bugs.md
 * #100's fix (a `pop ax` added there, balancing the routine's own earlier
 * `push es` the same way `qd_use`'s own exit already did -- the original
 * jumped straight to `popf ! ret` with that word still on the stack,
 * corrupting both).
 *
 * `readq`/`writeq` (and their `ax=0`/`ax=0xFFFF` entry points) each
 * retry in a loop (`jmps qr_ver`/`jmps qw_ver`) after a boundary-
 * stubbed `mpmif(f_sleep)` call whenever the queue is empty/full
 * *and* the call is unconditional (`ax==0`) -- under the no-op-
 * return convention that never changes the condition being retried,
 * so that one specific path spins forever, same unexercisable-but-
 * real shape as this same file's own `cioattach` / `CPMMEM.MEM`'s
 * `cpmfree_entry` (see docs/src-c.md). Translated faithfully below as
 * a real `for(;;)`, just never taken by the differential test. Every
 * other path -- conditional-and-empty/full (returns an error
 * immediately, no retry), and any call where a message/buffer slot
 * is already available -- is fully portable and exercised.
 *
 * Runs against real memory (defs.h's far_ptr), like the rest of the
 * RTM: `phys` is the base of real physical memory and `sysdat` the real
 * SYSDAT segment (the real asm's DS and cs:sysdat). QDs, their message
 * buffers (q_buf) and the qlr list are SYSDAT-relative; every QPB and
 * the caller's own message buffer (qpb_buffptr) are in the running
 * process's u_wrkseg, read from its UDA (the real asm's ambient ES,
 * p_uda[rlr]) -- so readq/writeq copy messages between segments, as the
 * real asm's segment-override `rep movsb` does. Like the real asm,
 * readq/writeq call queverify once and their wait-and-retry loops jump
 * back past it (to qr_ver/qw_ver).
 */
#include <stdint.h>

#include "defs.h"

uint16_t getqdaddr(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *cx_out);
void remqd(uint8_t *phys, uint16_t sysdat, uint16_t si);

typedef struct {
    uint16_t bx, cx;
} que_bxcx;

/* the running process's u_wrkseg segment (QPBs live there) */
static uint8_t *wrkseg_ptr(uint8_t *phys, const uint8_t *sd) {
    return far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
}

static int names_equal(const uint8_t *a, uint16_t a_off, const uint8_t *b, uint16_t b_off) {
    for (unsigned i = 0; i < qnamsiz; i++) {
        if (seg_r8(a, (uint16_t)(a_off + i)) != seg_r8(b, (uint16_t)(b_off + i))) return 0;
    }
    return 1;
}

/*
 * makeq_entry -- Create a System Queue
 *	input:	dx = address of QD in u_wrkseg
 *	output:	bx = 0 if okay, 0xFFFF if error
 *		cx = error code
 */
que_bxcx makeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t cx;
    uint16_t di = getqdaddr(phys, sysdat, dx, &cx);
    if (cx != 0) {
        que_bxcx r = {0xFFFF, cx};
        return r;
    }

    uint16_t si = (uint16_t)(SYSDAT_QLR - q_link);
    for (;;) {
        si = seg_r16(sd, (uint16_t)(si + q_link));
        if (si == 0) break;
        if (names_equal(sd, (uint16_t)(di + q_name), sd, (uint16_t)(si + q_name))) {
            que_bxcx r = {0xFFFF, e_q_inuse};
            return r;
        }
    }

    seg_w16(sd, (uint16_t)(di + q_dq), 0);
    seg_w16(sd, (uint16_t)(di + q_nq), 0);
    seg_w16(sd, (uint16_t)(di + q_msgcnt), 0);
    seg_w16(sd, (uint16_t)(di + q_msgout), 0);
    seg_w16(sd, (uint16_t)(di + q_link), seg_r16(sd, SYSDAT_QLR));
    seg_w16(sd, SYSDAT_QLR, di);
    que_bxcx r = {0, 0};
    return r;
}

/*
 * openq_entry -- Find an active Queue
 *	input:	dx = address of QPB in u_wrkseg
 *	output:	fills in the QPB's qpb_qaddr field
 *		bx = 0 if okay, 0xFFFF if not; cx = error code
 */
que_bxcx openq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *qpb = wrkseg_ptr(phys, sd);
    uint16_t si = (uint16_t)(SYSDAT_QLR - q_link);
    for (;;) {
        si = seg_r16(sd, (uint16_t)(si + q_link));
        if (si == 0) {
            que_bxcx r = {0xFFFF, e_no_queue};
            return r;
        }
        if (names_equal(sd, (uint16_t)(si + q_name), qpb, (uint16_t)(dx + qpb_name))) {
            seg_w16(qpb, (uint16_t)(dx + qpb_qaddr), si);
            que_bxcx r = {0, 0};
            return r;
        }
    }
}

/*
 * deleteq_entry -- Delete a System Queue
 * Takes a qd off the qlr and places it in the qul.
 *	input:	dx = offset of QPB in u_wrkseg
 *	output:	bx = 0 if ok, 0xFFFF if bad; cx = error code
 */
que_bxcx deleteq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t di = seg_r16(wrkseg_ptr(phys, sd), (uint16_t)(dx + qpb_qaddr));
    uint16_t dq = seg_r16(sd, (uint16_t)(di + q_dq));
    uint16_t nq = seg_r16(sd, (uint16_t)(di + q_nq));
    if ((uint16_t)(dq + nq) != 0) {
        que_bxcx r = {0xFFFF, e_q_inuse};
        return r;
    }

    uint16_t bx = (uint16_t)(SYSDAT_QLR - q_link);
    for (;;) {
        uint16_t si = seg_r16(sd, (uint16_t)(bx + q_link));
        if (si == 0) {
            que_bxcx r = {0xFFFF, e_q_notactive};
            return r;
        }
        if (si == di) {
            seg_w16(sd, (uint16_t)(bx + q_link), seg_r16(sd, (uint16_t)(si + q_link)));
            remqd(phys, sysdat, si);
            que_bxcx r = {0, 0};
            return r;
        }
        bx = si;
    }
}

/*
 * queverify -- check QLR for existence of QD address
 *	input:	dx = offset of QPB in u_wrkseg
 *	output:	returns error code (0 or e_no_queue)
 */
uint16_t queverify(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t target = seg_r16(wrkseg_ptr(phys, sd), (uint16_t)(dx + qpb_qaddr));
    uint16_t bx = SYSDAT_QLR;
    for (;;) {
        bx = seg_r16(sd, (uint16_t)(bx + q_link));
        if (bx == 0) return e_no_queue;
        if (bx == target) return 0;
    }
}

/*
 * readq -- shared body of readq_entry (ax=0, unconditional) and
 * creadq_entry (ax=0xFFFF, conditional): read one message off a
 * queue, or wait for one.
 *	input:	dx = address of QPB in u_wrkseg
 *		ax = 0xFFFF if conditional, 0 if not
 *	output:	bx = 0 if okay, 0xFFFF if error; cx = error code
 */
que_bxcx readq(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t ax) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t cx = queverify(phys, sysdat, dx);
    if (cx != 0) {
        que_bxcx r = {0xFFFF, cx};
        return r;
    }
    for (;;) {                                                     /* qr_ver */
        uint8_t *wseg = wrkseg_ptr(phys, sd);
        uint16_t bx = seg_r16(wseg, (uint16_t)(dx + qpb_qaddr));    /* bx->QD */
        uint16_t di = seg_r16(wseg, (uint16_t)(dx + qpb_buffptr));  /* es:[di]->buff */
        uint16_t msgcnt = seg_r16(sd, (uint16_t)(bx + q_msgcnt));
        if (msgcnt == 0) {
            if (ax == 0) {
                /* qr_wait: the running process waits on the QD's own DQ
                 * list; mpmif(f_sleep) is a boundary stub (no-op), which
                 * never changes msgcnt, so this retries forever -- see
                 * this file's own header comment */
                seg_w8(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_stat), ps_dq);
                continue; /* jmps qr_ver */
            }
            que_bxcx r = {0xFFFF, e_q_empty};
            return r;
        }
        uint16_t msglen = seg_r16(sd, (uint16_t)(bx + q_msglen));
        if (msglen == 0) {
            if (seg_r16(sd, (uint16_t)(bx + q_flags)) & qf_mx) {
                seg_w16(sd, (uint16_t)(bx + q_buf), seg_r16(sd, SYSDAT_RLR)); /* MX queue: BUF=owner */
            }
        } else {
            uint16_t msgout = seg_r16(sd, (uint16_t)(bx + q_msgout));
            uint16_t si = (uint16_t)((uint16_t)(msgout * msglen) + seg_r16(sd, (uint16_t)(bx + q_buf)));
            for (uint16_t i = 0; i < msglen; i++) {
                seg_w8(wseg, (uint16_t)(di + i), seg_r8(sd, (uint16_t)(si + i)));
            }
            msgout++;
            if (msgout == seg_r16(sd, (uint16_t)(bx + q_nmsgs))) msgout = 0;
            seg_w16(sd, (uint16_t)(bx + q_msgout), msgout);
        }
        seg_w16(sd, (uint16_t)(bx + q_msgcnt), (uint16_t)(msgcnt - 1));
        /* call mpmif(f_wakeup) on the QD's own NQ list -- boundary stub, no-op */
        que_bxcx r = {0, 0};
        return r;
    }
}

que_bxcx readq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    return readq(phys, sysdat, dx, 0);
}

que_bxcx creadq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    return readq(phys, sysdat, dx, 0xFFFF);
}

/*
 * writeq -- shared body of writeq_entry (ax=0, unconditional) and
 * cwriteq_entry (ax=0xFFFF, conditional): write one message into a
 * queue, or wait for room.
 *	input:	dx = address of QPB in u_wrkseg
 *		ax = 0xFFFF if conditional, 0 if not
 *	output:	bx = 0 if okay, 0xFFFF if error; cx = error code
 */
que_bxcx writeq(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t ax) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t cx = queverify(phys, sysdat, dx);
    if (cx != 0) {
        que_bxcx r = {0xFFFF, cx};
        return r;
    }
    for (;;) {                                                     /* qw_ver */
        uint8_t *wseg = wrkseg_ptr(phys, sd);
        uint16_t bx = seg_r16(wseg, (uint16_t)(dx + qpb_qaddr));    /* bx->QD */
        uint16_t si = seg_r16(wseg, (uint16_t)(dx + qpb_buffptr));  /* es:[si]->QPB.buff */
        uint16_t msgcnt = seg_r16(sd, (uint16_t)(bx + q_msgcnt));
        uint16_t nmsgs = seg_r16(sd, (uint16_t)(bx + q_nmsgs));
        if (msgcnt == nmsgs) {
            if (ax == 0) {
                /* qw_wait: the running process waits on the QD's own NQ
                 * list; mpmif(f_sleep) is a boundary stub (no-op), which
                 * never changes msgcnt, so this retries forever -- see
                 * this file's own header comment */
                seg_w8(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_stat), ps_nq);
                continue; /* jmps qw_ver */
            }
            que_bxcx r = {0xFFFF, e_q_full};
            return r;
        }
        uint16_t msglen = seg_r16(sd, (uint16_t)(bx + q_msglen));
        if (msglen != 0) {
            uint16_t slot = (uint16_t)(seg_r16(sd, (uint16_t)(bx + q_msgout)) + msgcnt);
            if (slot >= nmsgs) slot = (uint16_t)(slot - nmsgs);
            uint16_t di = (uint16_t)((uint16_t)(slot * msglen) + seg_r16(sd, (uint16_t)(bx + q_buf)));
            for (uint16_t i = 0; i < msglen; i++) {
                seg_w8(sd, (uint16_t)(di + i), seg_r8(wseg, (uint16_t)(si + i)));
            }
        }
        seg_w16(sd, (uint16_t)(bx + q_msgcnt), (uint16_t)(msgcnt + 1));
        if (seg_r16(sd, (uint16_t)(bx + q_flags)) & qf_mx) {
            seg_w16(sd, (uint16_t)(bx + q_buf), 0); /* an MX queue, no owner */
        }
        /* call mpmif(f_wakeup) on the QD's own DQ list -- boundary stub, no-op */
        que_bxcx r = {0, 0};
        return r;
    }
}

que_bxcx writeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    return writeq(phys, sysdat, dx, 0);
}

que_bxcx cwriteq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    return writeq(phys, sysdat, dx, 0xFFFF);
}
