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
 */
#include <stdint.h>

#include "defs.h"

uint16_t getqdaddr(uint8_t *mem, uint16_t dx, uint16_t sysdat, uint16_t *cx_out);
void remqd(uint8_t *mem, uint16_t si);

typedef struct {
    uint16_t bx, cx;
} que_bxcx;

/*
 * makeq_entry -- Create a System Queue
 *	input:	dx = address of QD in u_wrkseg
 *		sysdat = this module's own copy of the SYSDAT segment
 *		(real asm: `cs:sysdat`)
 *	output:	bx = 0 if okay, 0xFFFF if error
 *		cx = error code
 */
que_bxcx makeq_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat) {
    uint16_t cx;
    uint16_t di = getqdaddr(mem, dx, sysdat, &cx);
    if (cx != 0) {
        que_bxcx r = {0xFFFF, cx};
        return r;
    }

    uint16_t si = SD_QLR;
    for (;;) {
        si = mem_r16(mem, (uint16_t)(si + q_link));
        if (si == 0) break;
        int eq = 1;
        for (unsigned i = 0; i < qnamsiz; i++) {
            if (mem[(uint16_t)(di + q_name + i)] != mem[(uint16_t)(si + q_name + i)]) {
                eq = 0;
                break;
            }
        }
        if (eq) {
            que_bxcx r = {0xFFFF, e_q_inuse};
            return r;
        }
    }

    mem_w16(mem, (uint16_t)(di + q_dq), 0);
    mem_w16(mem, (uint16_t)(di + q_nq), 0);
    mem_w16(mem, (uint16_t)(di + q_msgcnt), 0);
    mem_w16(mem, (uint16_t)(di + q_msgout), 0);
    mem_w16(mem, (uint16_t)(di + q_link), mem_r16(mem, SD_QLR));
    mem_w16(mem, SD_QLR, di);
    que_bxcx r = {0, 0};
    return r;
}

/*
 * openq_entry -- Find an active Queue
 *	input:	dx = address of QPB (segment is u_wrkseg)
 *	output:	fills in the QPB's qpb_qaddr field
 *		bx = 0 if okay, 0xFFFF if not; cx = error code
 */
que_bxcx openq_entry(uint8_t *mem, uint16_t dx) {
    uint16_t di = dx;
    uint16_t si = SD_QLR;
    for (;;) {
        si = mem_r16(mem, (uint16_t)(si + q_link));
        if (si == 0) {
            que_bxcx r = {0xFFFF, e_no_queue};
            return r;
        }
        int eq = 1;
        for (unsigned i = 0; i < qnamsiz; i++) {
            if (mem[(uint16_t)(si + q_name + i)] != mem[(uint16_t)(di + qpb_name + i)]) {
                eq = 0;
                break;
            }
        }
        if (eq) {
            mem_w16(mem, (uint16_t)(di + qpb_qaddr), si);
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
que_bxcx deleteq_entry(uint8_t *mem, uint16_t dx) {
    uint16_t di = mem_r16(mem, (uint16_t)(dx + qpb_qaddr));
    uint16_t dq = mem_r16(mem, (uint16_t)(di + q_dq));
    uint16_t nq = mem_r16(mem, (uint16_t)(di + q_nq));
    if ((uint16_t)(dq + nq) != 0) {
        que_bxcx r = {0xFFFF, e_q_inuse};
        return r;
    }

    uint16_t bx = SD_QLR;
    for (;;) {
        uint16_t si = mem_r16(mem, (uint16_t)(bx + q_link));
        if (si == 0) {
            que_bxcx r = {0xFFFF, e_q_notactive};
            return r;
        }
        if (si == di) {
            mem_w16(mem, (uint16_t)(bx + q_link), mem_r16(mem, (uint16_t)(si + q_link)));
            remqd(mem, si);
            que_bxcx r = {0, 0};
            return r;
        }
        bx = si;
    }
}

/*
 * queverify -- check QLR for existence of QD address
 *	input:	dx = offset of QPB
 *	output:	returns error code (0 or e_no_queue)
 */
uint16_t queverify(const uint8_t *mem, uint16_t dx) {
    uint16_t target = mem_r16(mem, (uint16_t)(dx + qpb_qaddr));
    uint16_t bx = SD_QLR;
    for (;;) {
        bx = mem_r16(mem, (uint16_t)(bx + q_link));
        if (bx == 0) return e_no_queue;
        if (bx == target) return 0;
    }
}

/*
 * readq -- shared body of readq_entry (ax=0, unconditional) and
 * creadq_entry (ax=0xFFFF, conditional): read one message off a
 * queue, or wait for one.
 *	input:	dx = address of QPB in u_wrkseg (treated as already
 *		flat, same simplification openq_entry/deleteq_entry/
 *		queverify above already use in this file)
 *		ax = 0xFFFF if conditional, 0 if not
 *	output:	bx = 0 if okay, 0xFFFF if error; cx = error code
 */
que_bxcx readq(uint8_t *mem, uint16_t dx, uint16_t ax) {
    for (;;) {
        uint16_t cx = queverify(mem, dx);
        if (cx != 0) {
            que_bxcx r = {0xFFFF, cx};
            return r;
        }
        uint16_t qpb = dx;                                       /* es:[si]->QPB */
        uint16_t bx = mem_r16(mem, (uint16_t)(qpb + qpb_qaddr));   /* bx->QD */
        uint16_t di = mem_r16(mem, (uint16_t)(qpb + qpb_buffptr)); /* es:[di]->buff */
        uint16_t msgcnt = mem_r16(mem, (uint16_t)(bx + q_msgcnt));
        if (msgcnt == 0) {
            if (ax == 0) {
                /* qr_wait: the running process waits on the QD's own DQ
                 * list; mpmif(f_sleep) is a boundary stub (no-op), which
                 * never changes msgcnt, so this retries forever -- see
                 * this file's own header comment */
                uint16_t rlr = mem_r16(mem, SD_RLR);
                mem[rlr + p_stat] = ps_dq;
                continue; /* jmps qr_ver */
            }
            que_bxcx r = {0xFFFF, e_q_empty};
            return r;
        }
        uint16_t msglen = mem_r16(mem, (uint16_t)(bx + q_msglen));
        if (msglen == 0) {
            if (mem_r16(mem, (uint16_t)(bx + q_flags)) & qf_mx) {
                mem_w16(mem, (uint16_t)(bx + q_buf), mem_r16(mem, SD_RLR)); /* MX queue: BUF=owner */
            }
        } else {
            uint16_t msgout = mem_r16(mem, (uint16_t)(bx + q_msgout));
            uint16_t si = (uint16_t)((uint16_t)(msgout * msglen) + mem_r16(mem, (uint16_t)(bx + q_buf)));
            for (uint16_t i = 0; i < msglen; i++) {
                mem[(uint16_t)(di + i)] = mem[(uint16_t)(si + i)];
            }
            msgout++;
            if (msgout == mem_r16(mem, (uint16_t)(bx + q_nmsgs))) msgout = 0;
            mem_w16(mem, (uint16_t)(bx + q_msgout), msgout);
        }
        mem_w16(mem, (uint16_t)(bx + q_msgcnt), (uint16_t)(msgcnt - 1));
        /* call mpmif(f_wakeup) on the QD's own NQ list -- boundary stub, no-op */
        que_bxcx r = {0, 0};
        return r;
    }
}

que_bxcx readq_entry(uint8_t *mem, uint16_t dx) {
    return readq(mem, dx, 0);
}

que_bxcx creadq_entry(uint8_t *mem, uint16_t dx) {
    return readq(mem, dx, 0xFFFF);
}

/*
 * writeq -- shared body of writeq_entry (ax=0, unconditional) and
 * cwriteq_entry (ax=0xFFFF, conditional): write one message into a
 * queue, or wait for room.
 *	input:	dx = address of QPB in u_wrkseg (same flat simplification
 *		as readq above)
 *		ax = 0xFFFF if conditional, 0 if not
 *	output:	bx = 0 if okay, 0xFFFF if error; cx = error code
 */
que_bxcx writeq(uint8_t *mem, uint16_t dx, uint16_t ax) {
    for (;;) {
        uint16_t cx = queverify(mem, dx);
        if (cx != 0) {
            que_bxcx r = {0xFFFF, cx};
            return r;
        }
        uint16_t qpb = dx;                                       /* es:[di]->QPB */
        uint16_t bx = mem_r16(mem, (uint16_t)(qpb + qpb_qaddr));   /* bx->QD */
        uint16_t si = mem_r16(mem, (uint16_t)(qpb + qpb_buffptr)); /* es:[si]->QPB.buff */
        uint16_t msgcnt = mem_r16(mem, (uint16_t)(bx + q_msgcnt));
        uint16_t nmsgs = mem_r16(mem, (uint16_t)(bx + q_nmsgs));
        if (msgcnt == nmsgs) {
            if (ax == 0) {
                /* qw_wait: the running process waits on the QD's own NQ
                 * list; mpmif(f_sleep) is a boundary stub (no-op), which
                 * never changes msgcnt, so this retries forever -- see
                 * this file's own header comment */
                uint16_t rlr = mem_r16(mem, SD_RLR);
                mem[rlr + p_stat] = ps_nq;
                continue; /* jmps qw_ver */
            }
            que_bxcx r = {0xFFFF, e_q_full};
            return r;
        }
        uint16_t msglen = mem_r16(mem, (uint16_t)(bx + q_msglen));
        if (msglen != 0) {
            uint16_t slot = (uint16_t)(mem_r16(mem, (uint16_t)(bx + q_msgout)) + msgcnt);
            if (slot >= nmsgs) slot = (uint16_t)(slot - nmsgs);
            uint16_t di = (uint16_t)((uint16_t)(slot * msglen) + mem_r16(mem, (uint16_t)(bx + q_buf)));
            for (uint16_t i = 0; i < msglen; i++) {
                mem[(uint16_t)(di + i)] = mem[(uint16_t)(si + i)];
            }
        }
        mem_w16(mem, (uint16_t)(bx + q_msgcnt), (uint16_t)(msgcnt + 1));
        if (mem_r16(mem, (uint16_t)(bx + q_flags)) & qf_mx) {
            mem_w16(mem, (uint16_t)(bx + q_buf), 0); /* an MX queue, no owner */
        }
        /* call mpmif(f_wakeup) on the QD's own DQ list -- boundary stub, no-op */
        que_bxcx r = {0, 0};
        return r;
    }
}

que_bxcx writeq_entry(uint8_t *mem, uint16_t dx) {
    return writeq(mem, dx, 0);
}

que_bxcx cwriteq_entry(uint8_t *mem, uint16_t dx) {
    return writeq(mem, dx, 0xFFFF);
}
