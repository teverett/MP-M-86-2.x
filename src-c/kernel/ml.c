/*
 * ml.c -- C port of the whole of src-assem/kernel/ML.MEM: mlalloc_entry
 * (+ internal mla_value, mla_makemau, mla_initmau), mlfree_entry
 * (+ internal ml_insert).
 *
 * `getmd`/`freemd` (src-assem/kernel/UTIL.MEM, already ported in
 * util_mem.c) are called as ordinary internal calls, not boundary
 * stubs -- same convention as `memory.c`'s own calls into them.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the MEM
 * module's DS). Three segments are involved, each exactly as the asm
 * reaches it:
 *   - Memory List elements (the `m_*`-offset MDs `bx`/`si`/`di` walk)
 *     are SYSDAT-relative, like every other MD.
 *   - The caller's MPB is in the running process's u_wrkseg (the asm's
 *     `mov es,u_wrkseg`, ES being the running UDA), read from its UDA.
 *   - `mla_initmau` lays out a new MAU's SAT in that MAU's own segment,
 *     `m_start[bx]` (the asm's `mov es,dx`/`mov ds,dx`). Its closing
 *     `mov ds,cs:sysdat` only restores the caller's DS -- moot here.
 *
 * `mlfree_entry` returns bx = cx = 0, as the asm does (`sub bx,bx ! mov
 * cx,bx`); memif.c's dispatch passes that through.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} ml_bxcx;

typedef struct {
    uint16_t cx, ax;
} ml_value_result;

typedef struct {
    uint16_t bx, cx;
} md_bxcx;

extern md_bxcx getmd(uint8_t *phys, uint16_t sysdat);
extern void freemd(uint8_t *phys, uint16_t sysdat, uint16_t bx);

/*
 * mla_value -- score how well the Memory List window starting at
 * m_link[bx] satisfies the MPB at si in the caller's u_wrkseg (wseg).
 *	output: cx = value (0 = perfect, 0xFFFF = no fit), ax = # of
 *		elements the value covers
 */
static ml_value_result mla_value(const uint8_t *sd, const uint8_t *wseg, uint16_t bx, uint16_t si) {
    uint16_t ax = 0;
    uint16_t dx = 0;
    uint16_t di = seg_r16(sd, (uint16_t)(bx + m_link));
    if (di == 0) {
        return (ml_value_result){0xFFFF, 0}; /* mlav_err */
    }

    uint16_t mpb_start_val = seg_r16(wseg, (uint16_t)(si + mpb_start));
    if (mpb_start_val != 0) {
        /* an absolute request -- confirm this window actually covers it */
        uint16_t cx = (uint16_t)(seg_r16(sd, (uint16_t)(di + m_start)) + 2 * satlen);
        if (mpb_start_val < cx) {
            return (ml_value_result){0xFFFF, 0};
        }
        cx = (uint16_t)(cx + seg_r16(sd, (uint16_t)(di + m_length)) - 2 * satlen);
        if (mpb_start_val >= cx) {
            return (ml_value_result){0xFFFF, 0};
        }
    }

    for (;;) {
        /* mlav_next */
        uint16_t cx = seg_r16(wseg, (uint16_t)(si + mpb_max));
        int go_add = (cx >= dx);
        if (!go_add) {
            /* mlav_chk */
            cx = (uint16_t)(cx + 2 * satlen);
            go_add = (cx > dx);
        }
        int do_chkmin = 0;
        if (go_add) {
            /* mlav_add */
            if (ax != 0) {
                uint16_t range_end = (uint16_t)(seg_r16(sd, (uint16_t)(bx + m_start)) +
                                                 seg_r16(sd, (uint16_t)(bx + m_length)));
                if (range_end != seg_r16(sd, (uint16_t)(di + m_start))) {
                    do_chkmin = 1;
                }
            }
            if (!do_chkmin) {
                /* mlav_ad1 */
                ax++;
                dx = (uint16_t)(dx + seg_r16(sd, (uint16_t)(di + m_length)));
                bx = di;
                di = seg_r16(sd, (uint16_t)(bx + m_link));
                if (di != 0) {
                    continue; /* back to mlav_next */
                }
                do_chkmin = 1;
            }
        }
        if (do_chkmin) {
            uint16_t cx2 = (uint16_t)(seg_r16(wseg, (uint16_t)(si + mpb_min)) + 2 * satlen);
            if (cx2 > dx) {
                return (ml_value_result){0xFFFF, 0}; /* mlav_err */
            }
        }
        break; /* mlav_calc */
    }

    uint16_t count = ax;
    uint16_t scaled = (uint16_t)((uint16_t)(ax - 1) << 5);
    uint16_t cx = (uint16_t)(seg_r16(wseg, (uint16_t)(si + mpb_max)) + 2 * satlen);
    if (cx > dx) {
        /* "cmp cx,dx ! jbe mlav_ab ! xchg dx,cx" -- the swap only
         * happens on this (cx > dx) branch; jbe skips it */
        uint16_t tmp = dx;
        dx = cx;
        cx = tmp;
    }
    dx = (uint16_t)((uint16_t)(dx - cx) >> 4); /* unsigned 16-bit shr, not signed int arithmetic */
    dx = (uint16_t)(dx + scaled);
    return (ml_value_result){dx, count};
}

/*
 * mla_initmau -- lay out a brand-new SAT in bx's own segment
 * (`m_start[bx]`): zero the whole 32-entry table, mark slot 0 with
 * its own sentinel (31), and record the remaining allocatable area
 * (right after the SAT's own `2*satlen`-paragraph header) at slot
 * `satlen`.
 *	output: bx unchanged, cx = 0
 */
static ml_bxcx mla_initmau(uint8_t *phys, const uint8_t *sd, uint16_t bx) {
    uint16_t seg = seg_r16(sd, (uint16_t)(bx + m_start));
    uint16_t length = seg_r16(sd, (uint16_t)(bx + m_length));
    uint8_t *mau = far_ptr(phys, seg, 0);

    for (uint16_t i = 0; i < 32 * satlen; i++) {
        seg_w8(mau, i, 0);
    }
    seg_w8(mau, 0, 31);

    uint16_t avail_start = (uint16_t)(seg + 2 * satlen);
    uint16_t avail_len = (uint16_t)(length - 2 * satlen);
    seg_w16(mau, (uint16_t)(satlen + sat_start), avail_start);
    seg_w16(mau, (uint16_t)(satlen + sat_length), avail_len);
    seg_w8(mau, (uint16_t)(satlen + sat_nall), 0);

    return (ml_bxcx){bx, 0};
}

/*
 * mla_makemau -- turn the `ax`-element window starting at
 * m_link[bx] into one MAU, unlinking it from the Memory List:
 *	- ax <= 1: the single element itself becomes the MAU directly.
 *	- ax > 1: a fresh MD (via getmd) becomes a "partition list"
 *	  MAU spanning all `ax` elements (`m_plist` -> the first one,
 *	  chained via each element's own `m_link`), merging their
 *	  total length.
 */
static ml_bxcx mla_makemau(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t ax) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (ax <= 1) {
        uint16_t si = seg_r16(sd, (uint16_t)(bx + m_link));
        uint16_t cx = seg_r16(sd, (uint16_t)(si + m_link));
        seg_w16(sd, (uint16_t)(bx + m_link), cx);
        bx = si;
        seg_w16(sd, (uint16_t)(bx + m_plist), 0);
        seg_w16(sd, (uint16_t)(bx + m_link), 0);
        return mla_initmau(phys, sd, bx);
    }

    md_bxcx got = getmd(phys, sysdat);
    uint16_t di = got.bx;
    if (got.cx != 0) {
        return (ml_bxcx){0, got.cx};
    }

    seg_w16(sd, (uint16_t)(di + m_link), 0);
    seg_w16(sd, (uint16_t)(di + m_length), 0);
    uint16_t si = seg_r16(sd, (uint16_t)(bx + m_link));
    seg_w16(sd, (uint16_t)(di + m_plist), si);
    seg_w16(sd, (uint16_t)(di + m_start), seg_r16(sd, (uint16_t)(si + m_start)));

    for (;;) {
        uint16_t len = seg_r16(sd, (uint16_t)(si + m_length));
        seg_w16(sd, (uint16_t)(di + m_length), (uint16_t)(seg_r16(sd, (uint16_t)(di + m_length)) + len));
        ax--;
        if (ax == 0) {
            break;
        }
        si = seg_r16(sd, (uint16_t)(si + m_link));
    }

    uint16_t orig_bx = bx;
    bx = seg_r16(sd, (uint16_t)(si + m_link));
    seg_w16(sd, (uint16_t)(si + m_link), 0);
    seg_w16(sd, (uint16_t)(orig_bx + m_link), bx);
    return mla_initmau(phys, sd, di);
}

/*
 * mlalloc_entry -- create a MAU from the Memory List rooted at bx,
 * best satisfying the MPB at dx (an offset in u_wrkseg's own
 * segment), by scoring every possible contiguous window via
 * mla_value and keeping the best (lowest) score, stopping early on a
 * perfect (0) score.
 *	output: bx = MAU address (0 if error), cx = error code
 */
ml_bxcx mlalloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    const uint8_t *wseg = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t si = dx;
    uint16_t best_score = 0xFFFF;
    uint16_t best_bx = 0;
    uint16_t best_count = 0;

    while (seg_r16(sd, (uint16_t)(bx + m_link)) != 0) {
        ml_value_result v = mla_value(sd, wseg, bx, si);
        if (v.cx < best_score) {
            best_score = v.cx;
            best_count = v.ax;
            best_bx = bx;
            if (best_score == 0) {
                break; /* mla_found: perfect fit */
            }
        }
        bx = seg_r16(sd, (uint16_t)(bx + m_link));
    }

    if (best_bx == 0) {
        return (ml_bxcx){0, e_no_memory};
    }
    return mla_makemau(phys, sysdat, best_bx, best_count);
}

/*
 * ml_insert -- insert MD si into the Memory List rooted at bx, kept
 * sorted ascending by m_start.
 */
static void ml_insert(uint8_t *sd, uint16_t si, uint16_t bx) {
    for (;;) {
        uint16_t di = seg_r16(sd, (uint16_t)(bx + m_link));
        if (di == 0) {
            break;
        }
        if (seg_r16(sd, (uint16_t)(di + m_start)) > seg_r16(sd, (uint16_t)(si + m_start))) {
            break;
        }
        bx = di;
    }
    uint16_t ax = seg_r16(sd, (uint16_t)(bx + m_link));
    seg_w16(sd, (uint16_t)(bx + m_link), si);
    seg_w16(sd, (uint16_t)(si + m_link), ax);
}

/*
 * mlfree_entry -- free MAU dx back onto the Memory List rooted at
 * bx: a plain (non-partitioned) MAU is just reinserted directly; a
 * partition-list MAU has each of its own underlying MDs reinserted
 * individually, then the MAU descriptor itself freed back to the MD
 * pool (freemd).
 *	output: bx = 0, cx = 0 (always, per the real asm)
 */
ml_bxcx mlfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t si = dx;
    if (seg_r16(sd, (uint16_t)(si + m_plist)) == 0) {
        ml_insert(sd, si, bx);
        return (ml_bxcx){0, 0};
    }
    uint16_t di = si;
    si = seg_r16(sd, (uint16_t)(si + m_plist));
    while (si != 0) {
        uint16_t next = seg_r16(sd, (uint16_t)(si + m_link));
        ml_insert(sd, si, bx);
        si = next;
    }
    freemd(phys, sysdat, di);
    return (ml_bxcx){0, 0};
}
