/*
 * share.c -- C port of the whole of src/kernel/SHARE.MEM: share_entry.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the MEM
 * module's DS). PDs, their `p_mem` MS lists and the new MD are
 * SYSDAT-relative; the SPB (`spb_opd`/`spb_rpd`/`spb_start`) is in the
 * running process's u_wrkseg (the asm's `mov ds,u_wrkseg` bracket),
 * read from its UDA; the SAT scan near the end is in the MAU's own
 * segment, `m_start[bx]`. The asm's bare `.0` is this assembler's
 * "leading dot forces a memory operand" convention applied to a
 * literal 0 -- SAT0's own `ents` field, same as mau.c's header explains.
 *
 * `getmd`/`freemd` (src/kernel/UTIL.MEM, already ported in
 * util_mem.c) are called as ordinary internal calls, not boundary
 * stubs -- same convention as ml.c's/memory.c's own calls into them.
 *
 * The MD `getmd` hands out (`BP`) used to leak on two paths: sharing
 * with oneself (`se_s` went straight to the success exit) and the
 * owner having no segment at the requested start (`se_err`, unlike
 * `se_err1`, skipped `freemd`). Both now free it, here and in
 * src/kernel/SHARE.MEM -- docs/bugs.md #105.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} share_bxcx;

typedef struct {
    uint16_t bx, cx;
} md_bxcx;

extern md_bxcx getmd(uint8_t *phys, uint16_t sysdat);
extern void freemd(uint8_t *phys, uint16_t sysdat, uint16_t bx);

/*
 * share_entry -- share memory: give the requestor PD its own MS
 * entry referencing the same underlying MAU as one of the owner PD's
 * own existing MS entries (located by its own start paragraph).
 *	input:	dx = address of SPB in u_wrkseg
 *	output:	bx = 0 on success, 0xFFFF on failure; cx = error code
 */
share_bxcx share_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    md_bxcx got = getmd(phys, sysdat);
    if (got.cx != 0) {
        return (share_bxcx){0xFFFF, got.cx};
    }
    uint16_t bp = got.bx; /* new MD */

    const uint8_t *wseg = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t spb = dx;
    uint16_t bx = seg_r16(wseg, (uint16_t)(spb + spb_opd)); /* owner PD */
    uint16_t si = seg_r16(wseg, (uint16_t)(spb + spb_rpd)); /* requestor PD */
    uint16_t start = seg_r16(wseg, (uint16_t)(spb + spb_start));

    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    if (bx == 0) {
        bx = rlr;
    }
    if (si == 0) {
        si = rlr;
    }
    if (si == bx) {
        freemd(phys, sysdat, bp); /* se_self -- sharing with self, nothing to do */
        return (share_bxcx){0, 0};
    }

    /* walk the owner's own p_mem list (ms_link-chained) for an entry
     * starting at the requested paragraph */
    uint16_t walk = (uint16_t)(bx + p_mem); /* sentinel: (p_mem-ms_link)[bx], ms_link==0 */
    for (;;) {
        walk = seg_r16(sd, (uint16_t)(walk + ms_link));
        if (walk == 0) {
            freemd(phys, sysdat, bp); /* se_free */
            return (share_bxcx){0xFFFF, e_no_memory};
        }
        if (seg_r16(sd, (uint16_t)(walk + ms_start)) == start) {
            break;
        }
    }
    uint16_t owner_ms = walk;

    /* build the requestor's own new MS entry from the owner's */
    uint16_t new_ms = bp;
    seg_w16(sd, (uint16_t)(new_ms + ms_start), start);
    seg_w16(sd, (uint16_t)(new_ms + ms_length), seg_r16(sd, (uint16_t)(owner_ms + ms_length)));
    seg_w16(sd, (uint16_t)(new_ms + ms_flags), seg_r16(sd, (uint16_t)(owner_ms + ms_flags)));
    uint16_t mau = seg_r16(sd, (uint16_t)(owner_ms + ms_mau));
    seg_w16(sd, (uint16_t)(new_ms + ms_mau), mau);

    /* find the matching SAT entry in the MAU's own segment, bump its
     * share count */
    uint8_t *maup = far_ptr(phys, seg_r16(sd, (uint16_t)(mau + m_start)), 0);
    uint16_t sat = 0;
    uint8_t cl = seg_r8(maup, 0);
    for (;;) {
        if (cl == 0) {
            freemd(phys, sysdat, bp);
            return (share_bxcx){0xFFFF, e_no_memory}; /* se_err1 -> se_free */
        }
        sat = (uint16_t)(sat + satlen);
        cl--;
        if (seg_r16(maup, (uint16_t)(sat + sat_start)) == start) {
            break;
        }
    }
    seg_w8(maup, (uint16_t)(sat + sat_nall),
               (uint8_t)(seg_r8(maup, (uint16_t)(sat + sat_nall)) + 1));

    /* place the new MS onto the requestor's own p_mem list, next to
     * any existing entry that already shares the same MAU */
    uint16_t prev = (uint16_t)(si + p_mem);
    uint16_t next;
    for (;;) {
        next = seg_r16(sd, (uint16_t)(prev + ms_link));
        if (next == 0 || seg_r16(sd, (uint16_t)(next + ms_mau)) == mau) {
            break;
        }
        prev = next;
    }
    seg_w16(sd, (uint16_t)(new_ms + ms_link), next);
    seg_w16(sd, (uint16_t)(prev + ms_link), new_ms);

    return (share_bxcx){0, 0}; /* se_ge */
}
