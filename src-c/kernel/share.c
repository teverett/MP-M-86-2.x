/*
 * share.c -- C port of the whole of src/kernel/SHARE.MEM: share_entry.
 *
 * The real segment-relative-addressing gap docs/src-c.md flagged this
 * file for (same class as src-c/kernel/ml.c's/mau.c's own): the SAT
 * scan near the end switches DS to the underlying MAU's own
 * dynamically-supplied segment (`m_start[bx]`) and reads fixed SAT
 * offsets relative to it -- ported with defs.h's existing
 * `mem_r16_seg`/`mem_w16_seg`/`mem_r8_seg`/`mem_w8_seg` helpers and
 * `sat_start`/`sat_nall`/`satlen` constants (added for ml.c). The
 * real asm's own bare `.0` is this assembler's "leading dot forces a
 * memory operand" convention applied to a literal 0 -- SAT0's own
 * `ents` field, same as mau.c's own header comment explains.
 *
 * `spb_opd`/`spb_rpd`/`spb_start` (MEM.DEF) are read through an
 * explicit `mov ds,u_wrkseg`/`pop ds` bracket in the real asm (never
 * a bare reference), so -- unlike src-c/kernel/proc.c's own UDA.FMT
 * fields -- there's no implicit-segment surprise to account for;
 * ported directly as `mem_r16_seg(mem, wrkseg, ...)`.
 *
 * `getmd`/`freemd` (src/kernel/UTIL.MEM, already ported in
 * util_mem.c) are called as ordinary internal calls, not boundary
 * stubs -- same convention as ml.c's/memory.c's own calls into them.
 *
 * Two paths leave the real asm's own newly-`getmd`'d MD (`BP`)
 * unfreed on failure: `se_err`, reached directly when the owner's own
 * `p_mem` list has no entry at the requested start paragraph at all
 * (no `call freemd` anywhere on that path), unlike `se_err1` (reached
 * when the *MAU's own SAT* has no matching entry instead), which does
 * free it first. Ported faithfully either way -- not confirmed as a
 * genuine bug (no parallel call site in this file to compare against,
 * unlike docs/bugs.md #102's own two findings), so not changed.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} share_bxcx;

typedef struct {
    uint16_t bx, cx;
} md_bxcx;

extern md_bxcx getmd(uint8_t *mem);
extern void freemd(uint8_t *mem, uint16_t bx);

/*
 * share_entry -- share memory: give the requestor PD its own MS
 * entry referencing the same underlying MAU as one of the owner PD's
 * own existing MS entries (located by its own start paragraph).
 *	input:	dx = address of SPB in u_wrkseg
 *	output:	bx = 0 on success, 0xFFFF on failure; cx = error code
 */
share_bxcx share_entry(uint8_t *mem, uint16_t dx) {
    md_bxcx got = getmd(mem);
    if (got.cx != 0) {
        return (share_bxcx){0xFFFF, got.cx};
    }
    uint16_t bp = got.bx; /* new MD */

    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t spb = dx;
    uint16_t bx = mem_r16_seg(mem, wrkseg, (uint16_t)(spb + spb_opd)); /* owner PD */
    uint16_t si = mem_r16_seg(mem, wrkseg, (uint16_t)(spb + spb_rpd)); /* requestor PD */
    uint16_t start = mem_r16_seg(mem, wrkseg, (uint16_t)(spb + spb_start));

    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (bx == 0) {
        bx = rlr;
    }
    if (si == 0) {
        si = rlr;
    }
    if (si == bx) {
        return (share_bxcx){0, 0}; /* se_ge -- sharing with self, nothing to do */
    }

    /* walk the owner's own p_mem list (ms_link-chained) for an entry
     * starting at the requested paragraph */
    uint16_t walk = (uint16_t)(bx + p_mem); /* sentinel: (p_mem-ms_link)[bx], ms_link==0 */
    for (;;) {
        walk = mem_r16(mem, (uint16_t)(walk + ms_link));
        if (walk == 0) {
            return (share_bxcx){0xFFFF, e_no_memory}; /* se_err -- BP leaks, see header comment */
        }
        if (mem_r16(mem, (uint16_t)(walk + ms_start)) == start) {
            break;
        }
    }
    uint16_t owner_ms = walk;

    /* build the requestor's own new MS entry from the owner's */
    uint16_t new_ms = bp;
    mem_w16(mem, (uint16_t)(new_ms + ms_start), start);
    mem_w16(mem, (uint16_t)(new_ms + ms_length), mem_r16(mem, (uint16_t)(owner_ms + ms_length)));
    mem_w16(mem, (uint16_t)(new_ms + ms_flags), mem_r16(mem, (uint16_t)(owner_ms + ms_flags)));
    uint16_t mau = mem_r16(mem, (uint16_t)(owner_ms + ms_mau));
    mem_w16(mem, (uint16_t)(new_ms + ms_mau), mau);

    /* find the matching SAT entry in the MAU's own segment, bump its
     * share count */
    uint16_t seg = mem_r16(mem, (uint16_t)(mau + m_start));
    uint16_t sat = 0;
    uint8_t cl = mem_r8_seg(mem, seg, 0);
    for (;;) {
        if (cl == 0) {
            freemd(mem, bp);
            return (share_bxcx){0xFFFF, e_no_memory}; /* se_err1 -> se_err */
        }
        sat = (uint16_t)(sat + satlen);
        cl--;
        if (mem_r16_seg(mem, seg, (uint16_t)(sat + sat_start)) == start) {
            break;
        }
    }
    mem_w8_seg(mem, seg, (uint16_t)(sat + sat_nall),
               (uint8_t)(mem_r8_seg(mem, seg, (uint16_t)(sat + sat_nall)) + 1));

    /* place the new MS onto the requestor's own p_mem list, next to
     * any existing entry that already shares the same MAU */
    uint16_t prev = (uint16_t)(si + p_mem);
    uint16_t next;
    for (;;) {
        next = mem_r16(mem, (uint16_t)(prev + ms_link));
        if (next == 0 || mem_r16(mem, (uint16_t)(next + ms_mau)) == mau) {
            break;
        }
        prev = next;
    }
    mem_w16(mem, (uint16_t)(new_ms + ms_link), next);
    mem_w16(mem, (uint16_t)(prev + ms_link), new_ms);

    return (share_bxcx){0, 0}; /* se_ge */
}
