/*
 * ml.c -- C port of the whole of src/kernel/ML.MEM: mlalloc_entry
 * (+ internal mla_value, mla_makemau, mla_initmau), mlfree_entry
 * (+ internal ml_insert).
 *
 * This file is exactly the case docs/src-c.md's own "Deferred, with
 * real infrastructure gaps" section flagged as needing real
 * segment-relative addressing, not just the boundary-stub convention:
 * `mla_initmau` dynamically switches ES/DS to a brand-new MAU's own
 * segment (`m_start[bx]`, a real, different segment each call) and
 * writes fixed SAT-header offsets relative to it; `mla_value` reads
 * its caller's own MPB through yet another dynamically-supplied
 * segment, `u_wrkseg`'s own stored value. Now portable:
 * `tools/i86sim.py` gained an opt-in `segmented=True` mode (see its
 * own docstring) performing the identical `(seg * 16 + offset) &
 * 0xFFFF` arithmetic for the real asm reference run, and this port
 * uses `defs.h`'s new `mem_r16_seg`/`mem_w16_seg`/`mem_r8_seg`/
 * `mem_w8_seg` helpers to do the same thing explicitly wherever the
 * real asm reads/writes through one of these dynamic segments.
 *
 * `getmd`/`freemd` (src/kernel/UTIL.MEM, already ported in
 * util_mem.c) are called as ordinary internal calls, not boundary
 * stubs -- same convention as `memory.c`'s own calls into them.
 *
 * `mla_initmau`'s own closing `mov ds,cs:sysdat` restores the ambient
 * DS for its caller -- this port has no "ambient DS" register to
 * restore (every access is explicit), so it's moot, same reasoning
 * as every other plain DS/ES-switch-back elsewhere in this project.
 *
 * Memory List elements (the `m_*`-offset MD structures `bx`/`si`/`di`
 * walk throughout) are SYSDAT-resident -- ordinary flat-model
 * addresses, same as every other RTM/SUP list this project already
 * ports (`thrdrt`, `drl`, `dlr`, ...). Only the MPB (reached through
 * `u_wrkseg`) and a newly-made MAU's own SAT (reached through
 * `m_start[bx]`) are genuinely different segments.
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

extern md_bxcx getmd(uint8_t *mem);
extern void freemd(uint8_t *mem, uint16_t bx);

/*
 * mla_value -- score how well the Memory List window starting at
 * m_link[bx] satisfies the MPB at (wrkseg:si).
 *	output: cx = value (0 = perfect, 0xFFFF = no fit), ax = # of
 *		elements the value covers
 */
static ml_value_result mla_value(uint8_t *mem, uint16_t bx, uint16_t si, uint16_t wrkseg) {
    uint16_t ax = 0;
    uint16_t dx = 0;
    uint16_t di = mem_r16(mem, (uint16_t)(bx + m_link));
    if (di == 0) {
        return (ml_value_result){0xFFFF, 0}; /* mlav_err */
    }

    uint16_t mpb_start_val = mem_r16_seg(mem, wrkseg, (uint16_t)(si + mpb_start));
    if (mpb_start_val != 0) {
        /* an absolute request -- confirm this window actually covers it */
        uint16_t cx = (uint16_t)(mem_r16(mem, (uint16_t)(di + m_start)) + 2 * satlen);
        if (mpb_start_val < cx) {
            return (ml_value_result){0xFFFF, 0};
        }
        cx = (uint16_t)(cx + mem_r16(mem, (uint16_t)(di + m_length)) - 2 * satlen);
        if (mpb_start_val >= cx) {
            return (ml_value_result){0xFFFF, 0};
        }
    }

    for (;;) {
        /* mlav_next */
        uint16_t cx = mem_r16_seg(mem, wrkseg, (uint16_t)(si + mpb_max));
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
                uint16_t range_end = (uint16_t)(mem_r16(mem, (uint16_t)(bx + m_start)) +
                                                 mem_r16(mem, (uint16_t)(bx + m_length)));
                if (range_end != mem_r16(mem, (uint16_t)(di + m_start))) {
                    do_chkmin = 1;
                }
            }
            if (!do_chkmin) {
                /* mlav_ad1 */
                ax++;
                dx = (uint16_t)(dx + mem_r16(mem, (uint16_t)(di + m_length)));
                bx = di;
                di = mem_r16(mem, (uint16_t)(bx + m_link));
                if (di != 0) {
                    continue; /* back to mlav_next */
                }
                do_chkmin = 1;
            }
        }
        if (do_chkmin) {
            uint16_t cx2 = (uint16_t)(mem_r16_seg(mem, wrkseg, (uint16_t)(si + mpb_min)) + 2 * satlen);
            if (cx2 > dx) {
                return (ml_value_result){0xFFFF, 0}; /* mlav_err */
            }
        }
        break; /* mlav_calc */
    }

    uint16_t count = ax;
    uint16_t scaled = (uint16_t)((uint16_t)(ax - 1) << 5);
    uint16_t cx = (uint16_t)(mem_r16_seg(mem, wrkseg, (uint16_t)(si + mpb_max)) + 2 * satlen);
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
static ml_bxcx mla_initmau(uint8_t *mem, uint16_t bx) {
    uint16_t seg = mem_r16(mem, (uint16_t)(bx + m_start));
    uint16_t length = mem_r16(mem, (uint16_t)(bx + m_length));

    for (uint16_t i = 0; i < 32 * satlen; i++) {
        mem_w8_seg(mem, seg, i, 0);
    }
    mem_w8_seg(mem, seg, 0, 31);

    uint16_t avail_start = (uint16_t)(seg + 2 * satlen);
    uint16_t avail_len = (uint16_t)(length - 2 * satlen);
    mem_w16_seg(mem, seg, (uint16_t)(satlen + sat_start), avail_start);
    mem_w16_seg(mem, seg, (uint16_t)(satlen + sat_length), avail_len);
    mem_w8_seg(mem, seg, (uint16_t)(satlen + sat_nall), 0);

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
static ml_bxcx mla_makemau(uint8_t *mem, uint16_t bx, uint16_t ax) {
    if (ax <= 1) {
        uint16_t si = mem_r16(mem, (uint16_t)(bx + m_link));
        uint16_t cx = mem_r16(mem, (uint16_t)(si + m_link));
        mem_w16(mem, (uint16_t)(bx + m_link), cx);
        bx = si;
        mem_w16(mem, (uint16_t)(bx + m_plist), 0);
        mem_w16(mem, (uint16_t)(bx + m_link), 0);
        return mla_initmau(mem, bx);
    }

    md_bxcx got = getmd(mem);
    uint16_t di = got.bx;
    if (got.cx != 0) {
        return (ml_bxcx){0, got.cx};
    }

    mem_w16(mem, (uint16_t)(di + m_link), 0);
    mem_w16(mem, (uint16_t)(di + m_length), 0);
    uint16_t si = mem_r16(mem, (uint16_t)(bx + m_link));
    mem_w16(mem, (uint16_t)(di + m_plist), si);
    mem_w16(mem, (uint16_t)(di + m_start), mem_r16(mem, (uint16_t)(si + m_start)));

    for (;;) {
        uint16_t len = mem_r16(mem, (uint16_t)(si + m_length));
        mem_w16(mem, (uint16_t)(di + m_length), (uint16_t)(mem_r16(mem, (uint16_t)(di + m_length)) + len));
        ax--;
        if (ax == 0) {
            break;
        }
        si = mem_r16(mem, (uint16_t)(si + m_link));
    }

    uint16_t orig_bx = bx;
    bx = mem_r16(mem, (uint16_t)(si + m_link));
    mem_w16(mem, (uint16_t)(si + m_link), 0);
    mem_w16(mem, (uint16_t)(orig_bx + m_link), bx);
    return mla_initmau(mem, di);
}

/*
 * mlalloc_entry -- create a MAU from the Memory List rooted at bx,
 * best satisfying the MPB at dx (an offset in u_wrkseg's own
 * segment), by scoring every possible contiguous window via
 * mla_value and keeping the best (lowest) score, stopping early on a
 * perfect (0) score.
 *	output: bx = MAU address (0 if error), cx = error code
 */
ml_bxcx mlalloc_entry(uint8_t *mem, uint16_t dx, uint16_t bx) {
    uint16_t si = dx;
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t best_score = 0xFFFF;
    uint16_t best_bx = 0;
    uint16_t best_count = 0;

    while (mem_r16(mem, (uint16_t)(bx + m_link)) != 0) {
        ml_value_result v = mla_value(mem, bx, si, wrkseg);
        if (v.cx < best_score) {
            best_score = v.cx;
            best_count = v.ax;
            best_bx = bx;
            if (best_score == 0) {
                break; /* mla_found: perfect fit */
            }
        }
        bx = mem_r16(mem, (uint16_t)(bx + m_link));
    }

    if (best_bx == 0) {
        return (ml_bxcx){0, e_no_memory};
    }
    return mla_makemau(mem, best_bx, best_count);
}

/*
 * ml_insert -- insert MD si into the Memory List rooted at bx, kept
 * sorted ascending by m_start.
 */
static void ml_insert(uint8_t *mem, uint16_t si, uint16_t bx) {
    for (;;) {
        uint16_t di = mem_r16(mem, (uint16_t)(bx + m_link));
        if (di == 0) {
            break;
        }
        if (mem_r16(mem, (uint16_t)(di + m_start)) > mem_r16(mem, (uint16_t)(si + m_start))) {
            break;
        }
        bx = di;
    }
    uint16_t ax = mem_r16(mem, (uint16_t)(bx + m_link));
    mem_w16(mem, (uint16_t)(bx + m_link), si);
    mem_w16(mem, (uint16_t)(si + m_link), ax);
}

/*
 * mlfree_entry -- free MAU dx back onto the Memory List rooted at
 * bx: a plain (non-partitioned) MAU is just reinserted directly; a
 * partition-list MAU has each of its own underlying MDs reinserted
 * individually, then the MAU descriptor itself freed back to the MD
 * pool (freemd).
 *	output: bx = 0, cx = 0 (always, per the real asm)
 */
void mlfree_entry(uint8_t *mem, uint16_t dx, uint16_t bx) {
    uint16_t si = dx;
    if (mem_r16(mem, (uint16_t)(si + m_plist)) == 0) {
        ml_insert(mem, si, bx);
        return;
    }
    uint16_t di = si;
    si = mem_r16(mem, (uint16_t)(si + m_plist));
    while (si != 0) {
        uint16_t next = mem_r16(mem, (uint16_t)(si + m_link));
        ml_insert(mem, si, bx);
        si = next;
    }
    freemd(mem, di);
}
