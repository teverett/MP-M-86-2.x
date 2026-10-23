/*
 * que1.c -- C port of 4 of src/kernel/QUE1.RTM's 10 routines: makeq_entry,
 * openq_entry, deleteq_entry, queverify.
 *
 * `readq_entry`/`creadq_entry`/`readq`/`writeq_entry`/`cwriteq_entry`/
 * `writeq` are deferred: each retries in a loop after a boundary-stubbed
 * `mpmif(f_sleep)` call (`jmps qr_ver`/`jmps qw_ver`), which would spin
 * forever under the instant-no-op-return convention unless the test
 * deliberately changes the retried condition out from under it -- see
 * docs/src-c.md.
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
