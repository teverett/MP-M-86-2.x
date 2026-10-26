/*
 * mau.c -- C port of the whole of src/kernel/MAU.MEM: maualloc_entry,
 * maufree_entry (+ internal mau_split, mau_insert, mau_release,
 * mau_collapse).
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the MEM
 * module's DS). Each structure is reached through the segment the asm
 * uses for it:
 *   - The MAU's MD (`m_start`/`m_length`) is SYSDAT-relative.
 *   - Every SAT (suballocation table) field lives in the MAU's own
 *     segment, `m_start[bx]` (the asm's `mov ds,m_start[bx]`); the
 *     internal helpers take a pointer to that segment (`mau`).
 *   - The MPB/MAF is in the running process's u_wrkseg (the asm's `mov
 *     es,u_wrkseg`, ES being the running UDA), read from its UDA.
 * `maua_out`/`mauf_out`'s `mov ds,cs:sysdat` only restores the caller's
 * DS -- moot here.
 *
 * The real asm's own bare `.0` (e.g. "cmp dl,.0") is this assembler's
 * "a leading dot forces a memory operand" convention (see docs/
 * bugs.md's own writeup elsewhere) applied to a literal 0 -- i.e. "the
 * byte at offset 0 of the ambient DS", not an immediate 0. DS is the
 * MAU's own segment throughout every routine here (set once at entry,
 * via `mov ds,m_start[bx]` in the real asm), and offset 0 of a MAU's
 * own segment is SAT0's own `ents` field (src-c/kernel/ml.c's own
 * `mla_initmau` writes 31 there for a fresh MAU) -- the maximum valid
 * SAT entry number. Ported as `seg_r8(mau, 0)` everywhere `.0`
 * appears.
 *
 * `mpb_start`/`mpb_min`/`mpb_max`/`mpb_flags` (MPB.DEF) and
 * `maf_mau`/`maf_sat`/`maf_start` (MPB.DEF's own MAF block) are real,
 * ordinary structure offsets reached through an explicit `es:`
 * override onto `u_wrkseg` throughout the real asm (never a bare
 * reference).
 *
 * **Found and fixed a real, 40-year-old /src bug while porting this
 * file** (present identically in `orig/mpm862sr/02/MAU.MEM` and
 * `orig/lastdri/19/MAU.MEM` -- see docs/bugs.md #102): `maualloc_entry`'s
 * exact-absolute-split path did `mov bx,cx ! call mau_split`,
 * clobbering `BX` (which must stay the target SAT entry's own address
 * for `mau_split`'s own use) with the split length just computed into
 * `CX` -- confirmed wrong by comparing `maufree_entry`'s own call to
 * `mau_split` a few lines later, which gets it right (no `mov bx,...`
 * before the call). This port implements the corrected behavior
 * directly (see `mau_split`'s own caller in `maualloc_entry` below).
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} mau_bxcx;

static uint16_t mau_insert(uint8_t *mau, uint16_t bx) {
    uint16_t table_end = (uint16_t)(satlen * seg_r8(mau, 0));
    uint16_t si = table_end;
    if (seg_r16(mau, (uint16_t)(si + sat_start)) != 0) {
        return 0xFFFF; /* table full */
    }
    uint16_t count = (uint16_t)(si - bx);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t off = (uint16_t)(si - 1 - i);
        seg_w8(mau, (uint16_t)(off + satlen), seg_r8(mau, off));
    }
    seg_w16(mau, (uint16_t)(bx + sat_start), 0);
    seg_w16(mau, (uint16_t)(bx + sat_length), 0);
    seg_w8(mau, (uint16_t)(bx + sat_nall), 0);
    return 0;
}

/*
 * mau_split -- split SAT element `bx` into 2 at `cx`: the new element
 * (at bx+satlen, with nall=0) gets the back portion, the original
 * (bx, keeping its own nall unchanged) shrinks to length `cx`. If the
 * table has no room for a new element, `bx` is just shrunk to `cx`
 * anyway ("create hole" -- the real asm's own comment), discarding
 * the remainder.
 */
static void mau_split(uint8_t *mau, uint16_t bx, uint16_t cx) {
    uint16_t orig_bx = bx, orig_cx = cx;
    if (mau_insert(mau, (uint16_t)(bx + satlen)) == 0) {
        uint16_t di = (uint16_t)(satlen + orig_bx);
        uint16_t len = seg_r16(mau, (uint16_t)(orig_bx + sat_length));
        seg_w16(mau, (uint16_t)(di + sat_length), (uint16_t)(len - orig_cx));
        uint16_t start = (uint16_t)(orig_cx + seg_r16(mau, (uint16_t)(orig_bx + sat_start)));
        seg_w16(mau, (uint16_t)(di + sat_start), start);
        seg_w8(mau, (uint16_t)(di + sat_nall), 0);
    }
    seg_w16(mau, (uint16_t)(orig_bx + sat_length), orig_cx);
}

static void mau_release(uint8_t *mau, uint16_t bx) {
    uint16_t table_end = (uint16_t)(satlen * seg_r8(mau, 0));
    uint16_t count = (uint16_t)(table_end - bx);
    for (uint16_t i = 0; i < count; i++) {
        seg_w8(mau, (uint16_t)(bx + i), seg_r8(mau, (uint16_t)(bx + satlen + i)));
    }
    seg_w16(mau, (uint16_t)(table_end + sat_length), 0);
    seg_w16(mau, (uint16_t)(table_end + sat_start), 0);
    seg_w8(mau, (uint16_t)(table_end + sat_nall), 0);
}

/*
 * mau_collapse -- collapse adjacent unallocated SATs and holes.
 *	output: bx = 0 if not empty, 0xFFFF if empty; cx always 0 (the
 *		real asm's only "ret" is at mauc_done, reached from every
 *		other label by a fall-through or jmp). Both table-end exits
 *		go through mauc_last, which checks the last SAT in use --
 *		the asm used to skip that and report a MAU empty while its
 *		last SAT was still allocated (docs/bugs.md #106).
 */
static mau_bxcx mau_collapse(uint8_t *mau) {
    uint16_t bp = 0xFFFF; /* assume empty */
    uint16_t bx = 0;
    uint16_t cx = 0;
    uint16_t di;
    uint16_t ents = seg_r8(mau, 0);

    for (;;) {
        cx++;
        if (cx >= ents) {
            bx = (uint16_t)(bx + satlen); /* the table's last entry */
            break;                        /* mauc_last */
        }

        bx = (uint16_t)(bx + satlen);
        di = (uint16_t)(satlen + bx);
        if (seg_r16(mau, (uint16_t)(bx + sat_start)) == 0) {
            return (mau_bxcx){bp, 0}; /* mauc_done */
        }
        if (seg_r16(mau, (uint16_t)(di + sat_start)) == 0) {
            break; /* mauc_last */
        }

        if (seg_r8(mau, (uint16_t)(bx + sat_nall)) == 0) {
            /* this SAT is free */
            uint16_t end = (uint16_t)(seg_r16(mau, (uint16_t)(bx + sat_start)) +
                                       seg_r16(mau, (uint16_t)(bx + sat_length)));
            if (end != seg_r16(mau, (uint16_t)(di + sat_start))) {
                /* followed by a hole */
                uint16_t newlen = (uint16_t)(seg_r16(mau, (uint16_t)(di + sat_start)) -
                                              seg_r16(mau, (uint16_t)(bx + sat_start)));
                seg_w16(mau, (uint16_t)(bx + sat_length), newlen);
            }
            /* mauc_fnohole -- free SAT, no hole */
            if (seg_r8(mau, (uint16_t)(di + sat_nall)) != 0) {
                continue; /* mauc_nsat */
            }
            /* followed by a free SAT -- merge it in and release it */
            uint16_t add = seg_r16(mau, (uint16_t)(di + sat_length));
            seg_w16(mau, (uint16_t)(bx + sat_length),
                        (uint16_t)(seg_r16(mau, (uint16_t)(bx + sat_length)) + add));
            mau_release(mau, di);
            bx = (uint16_t)(bx - satlen);
            cx--;
            continue;
        }

        /* mauc_notfree -- allocated SAT */
        bp = 0; /* not empty */
        uint16_t end2 = (uint16_t)(seg_r16(mau, (uint16_t)(bx + sat_start)) +
                                    seg_r16(mau, (uint16_t)(bx + sat_length)));
        if (end2 == seg_r16(mau, (uint16_t)(di + sat_start))) {
            continue; /* mauc_nsat */
        }
        /* followed by a hole */
        if (seg_r8(mau, (uint16_t)(di + sat_nall)) != 0) {
            /* next SAT is allocated -- give the hole its own entry */
            mau_insert(mau, bx);
            continue;
        }
        /* mauc_nfhole -- next SAT is free: absorb the hole into it */
        uint16_t shrink = (uint16_t)(seg_r16(mau, (uint16_t)(di + sat_start)) -
                                      seg_r16(mau, (uint16_t)(bx + sat_start)));
        seg_w16(mau, (uint16_t)(di + sat_start),
                    (uint16_t)(seg_r16(mau, (uint16_t)(di + sat_start)) - shrink));
        seg_w16(mau, (uint16_t)(di + sat_length),
                    (uint16_t)(seg_r16(mau, (uint16_t)(di + sat_length)) + shrink));
    }

    /* mauc_last -- bx is the last SAT in use, which the loop never
     * judges (it decides each SAT by its successor); it still decides
     * emptiness (docs/bugs.md #106) */
    if (seg_r16(mau, (uint16_t)(bx + sat_start)) != 0 && seg_r8(mau, (uint16_t)(bx + sat_nall)) != 0) {
        bp = 0; /* not empty */
    }
    return (mau_bxcx){bp, 0};
}

/*
 * maualloc_entry -- allocate a piece of memory from the MAU described
 * by MD `bx`, satisfying the MPB at `dx` (an offset in u_wrkseg).
 *	output: bx = 0 on success, 0xFFFF on failure; cx = error code
 */
mau_bxcx maualloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *wseg = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t si = dx;

    uint16_t cx = seg_r16(wseg, (uint16_t)(si + mpb_start));
    if (cx != 0) {
        /* if absolute, see if within this MAU at all */
        uint16_t ax = seg_r16(sd, (uint16_t)(bx + m_start));
        if (cx < ax) {
            return (mau_bxcx){0xFFFF, e_no_memory};
        }
        ax = (uint16_t)(ax + seg_r16(sd, (uint16_t)(bx + m_length)));
        if (cx >= ax) {
            return (mau_bxcx){0xFFFF, e_no_memory};
        }
    }

    uint8_t *mau = far_ptr(phys, seg_r16(sd, (uint16_t)(bx + m_start)), 0);

    for (;;) { /* maua_start */
        uint16_t sat = 0;
        uint8_t dl = 0;
        uint8_t ents = seg_r8(mau, 0);

        for (;;) { /* maua_nsat */
            dl++;
            sat = (uint16_t)(sat + satlen);
            if (dl > ents) {
                return (mau_bxcx){0xFFFF, e_no_memory}; /* maua_err */
            }
            if (seg_r16(mau, (uint16_t)(sat + sat_start)) == 0) {
                return (mau_bxcx){0xFFFF, e_no_memory}; /* maua_err */
            }
            if (seg_r8(mau, (uint16_t)(sat + sat_nall)) != 0) {
                continue; /* already allocated */
            }

            cx = seg_r16(wseg, (uint16_t)(si + mpb_start));
            if (cx != 0) {
                uint16_t ax = seg_r16(mau, (uint16_t)(sat + sat_start));
                if (cx < ax) {
                    continue;
                }
                ax = (uint16_t)(ax + seg_r16(mau, (uint16_t)(sat + sat_length)));
                if (cx >= ax) {
                    continue;
                }
                if (cx != seg_r16(mau, (uint16_t)(sat + sat_start))) {
                    /* not exact -- split off exactly the requested address
                     * (docs/bugs.md #102: BX/`sat` must stay the target
                     * SAT entry's own address going into mau_split) */
                    uint16_t split_at = (uint16_t)(cx - seg_r16(mau, (uint16_t)(sat + sat_start)));
                    mau_split(mau, sat, split_at);
                    goto restart; /* jmps maua_start */
                }
            }

            /* maua_nabs */
            {
                uint16_t mn = seg_r16(wseg, (uint16_t)(si + mpb_min));
                uint16_t len = seg_r16(mau, (uint16_t)(sat + sat_length));
                if (mn > len) {
                    continue; /* maua_nsat */
                }
                uint16_t mx = seg_r16(wseg, (uint16_t)(si + mpb_max));
                if (mx < len) {
                    /* the real asm's own "cmp sat_nall[bx],0 ! je
                     * maua_splitit" is always true here -- sat_nall is
                     * already guaranteed 0 by the check above, with
                     * nothing between writing to it, so the mf_share
                     * fallback beneath it is unreachable; kept anyway
                     * for fidelity */
                    if (seg_r8(mau, (uint16_t)(sat + sat_nall)) == 0) {
                        mau_split(mau, sat, mx);
                    } else if (!(seg_r16(wseg, (uint16_t)(si + mpb_flags)) & mf_share)) {
                        continue; /* maua_nsat */
                    }
                }

                /* maua_alloc */
                uint16_t alloc_len = seg_r16(mau, (uint16_t)(sat + sat_length));
                seg_w16(wseg, (uint16_t)(si + mpb_min), alloc_len);
                seg_w16(wseg, (uint16_t)(si + mpb_max), alloc_len);
                uint16_t alloc_start = seg_r16(mau, (uint16_t)(sat + sat_start));
                seg_w16(wseg, (uint16_t)(si + mpb_start), alloc_start);
                seg_w8(mau, (uint16_t)(sat + sat_nall),
                           (uint8_t)(seg_r8(mau, (uint16_t)(sat + sat_nall)) + 1));
                mau_collapse(mau);
                return (mau_bxcx){0, 0};
            }
        }
    restart:;
    }
}

/*
 * maufree_entry -- free SAT in the MAU named by the MAF at `dx` (an
 * offset in u_wrkseg).
 *	output: cx = 0 on success, e_no_memory on failure (the real
 *		asm's own header comment claims cx=0xffffh on failure,
 *		but the actual code sets e_no_memory -- the code wins);
 *		if cx == 0, bx = mau_collapse's own result (0 = MAU
 *		still in use, 0xFFFF = MAU now empty); if cx != 0,
 *		bx = 0xFFFF
 */
mau_bxcx maufree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    const uint8_t *wseg = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t si = dx;
    uint16_t bx = seg_r16(wseg, (uint16_t)(si + maf_mau));
    uint8_t *mau = far_ptr(phys, seg_r16(sd, (uint16_t)(bx + m_start)), 0);

    bx = 0;
    uint16_t cx = 0;
    uint16_t target = seg_r16(wseg, (uint16_t)(si + maf_sat));
    uint8_t ents = seg_r8(mau, 0);

    for (;;) { /* mauf_nsat */
        bx = (uint16_t)(bx + satlen);
        cx++;
        if (cx > ents) {
            return (mau_bxcx){0xFFFF, e_no_memory}; /* mauf_err */
        }
        if (seg_r16(mau, (uint16_t)(bx + sat_start)) != target) {
            continue;
        }

        uint16_t maf_start_val = seg_r16(wseg, (uint16_t)(si + maf_start));
        if (target == maf_start_val) {
            /* mauf_free -- the whole entry */
            seg_w8(mau, (uint16_t)(bx + sat_nall),
                       (uint8_t)(seg_r8(mau, (uint16_t)(bx + sat_nall)) - 1));
        } else {
            uint16_t end = (uint16_t)(seg_r16(mau, (uint16_t)(bx + sat_start)) +
                                       seg_r16(mau, (uint16_t)(bx + sat_length)));
            if (end < maf_start_val) {
                return (mau_bxcx){0xFFFF, e_no_memory}; /* mauf_err */
            }
            /* must split this SAT -- the new (freed) entry starts at
             * maf_start_val; mau_split's own "nall=0" on the new part
             * is what actually marks it free, so there's no separate
             * dec here */
            uint16_t split_at = (uint16_t)(maf_start_val - target);
            mau_split(mau, bx, split_at);
        }

        mau_bxcx r = mau_collapse(mau);
        return r;
    }
}
