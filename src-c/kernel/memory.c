/*
 * memory.c -- C port of src/kernel/MEMORY.MEM: malloc_entry,
 * mall_try_mau, mfree_entry, mfree_it.
 *
 * Unlike src/kernel/MAU.MEM/SHARE.MEM, none of these switch DS to a
 * dynamically-computed segment and then use a fixed offset relative to
 * it -- only the already-established `u_wrkseg`/`cs:sysdat` patterns --
 * so the real segment-addressing gap documented for those two files (and
 * PROC.RTM/ML.MEM) doesn't apply here.
 *
 * Every real allocator call here goes through the unported `mpmif`
 * trampoline (`f_mlalloc`/`f_maualloc`/`f_maufree`) and is a boundary
 * stub per docs/src-c.md's convention. As with QUE2.RTM's own `qspace`,
 * this has a real consequence: the real code sets `cx` to the function
 * code immediately before each call and only checks `cx == 0`
 * afterward, so under the no-op convention every one of those checks
 * always "fails" -- `malloc_entry`'s own "successful allocation from an
 * existing MAU or a freshly-created one" paths, and `mfree_it`'s own
 * "actually free the segment" path, are therefore unreachable here,
 * exactly as they are in the real reference assembly run the same way.
 *
 * `getmd`/`freemd` (src/kernel/UTIL.MEM, util_mem.c) are called as
 * ordinary internal calls below, not boundary stubs. They're declared
 * here with locally-named but layout-identical types rather than shared
 * ones, matching the convention src-c/kernel/que1.c already uses for
 * its own cross-file calls into que2.c.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} getmd_result;

extern getmd_result getmd(uint8_t *mem);
extern void freemd(uint8_t *mem, uint16_t bx);

typedef struct {
    uint16_t bx, cx;
} mem_bxcx;

/*
 * mall_try_mau
 *	input:	bx -> MAU, dx -> MPB in u_wrkseg
 *	output:	returns the error code (always nonzero here -- see header
 *		comment)
 */
static uint16_t mall_try_mau(uint8_t *mem, uint16_t bx, uint16_t dx) {
    (void)mem;
    (void)bx;
    (void)dx;
    /* mov cx,f_maualloc ! call mpmif -- boundary stub, no-op */
    return f_maualloc;
}

/*
 * malloc_entry -- Allocate Memory
 *	input:	dx = MPB in u_wrkseg
 *	output:	bx = 0 (unused memory) or 1 (shared code) if successful,
 *		0xFFFF if failure
 *		cx = error code
 */
mem_bxcx malloc_entry(uint8_t *mem, uint16_t dx) {
    getmd_result md = getmd(mem);
    if (md.cx != 0) {
        mem_bxcx r = {0xFFFF, md.cx};
        return r;
    }
    uint16_t new_md = md.bx;

    uint16_t pdadr = mem_r16(mem, (uint16_t)(dx + mpb_pdadr));
    if (pdadr == 0) {
        pdadr = mem_r16(mem, SD_RLR);
        mem_w16(mem, (uint16_t)(dx + mpb_pdadr), pdadr);
    }

    if (pdadr != mem_r16(mem, SD_RLR)) {
        uint16_t walk = (uint16_t)(SD_THRDRT - p_thread); /* bias so the
            first read below lands on thrdrt itself, matching the real
            "mov si,(offset thrdrt)-p_thread" */
        for (;;) {
            walk = mem_r16(mem, (uint16_t)(walk + p_thread));
            if (walk == 0) break; /* mall_pdver */
            if (walk != pdadr) continue;
            if (walk == mem_r16(mem, SD_RLR)) break; /* mall_pdver */
            mem_bxcx r = {0xFFFF, e_active_pd};
            return r;
        }
    }

    /* mall_pdver: try to allocate from an existing MAU this PD already uses */
    uint16_t si = (uint16_t)(pdadr + p_mem - ms_link);
    uint16_t last_tried = 0; /* see header comment: the real asm's own
                                 cmp-then-pop trick means this is never
                                 actually updated to a new value either */
    uint16_t link_si = 0; /* si to link the new MD onto, 0 = a fresh MAU */
    uint16_t mau = 0;

    for (;;) {
        si = mem_r16(mem, (uint16_t)(si + ms_link));
        if (si == 0) {
            /* mall_ml: create a new MAU from the Memory Free List */
            uint16_t qcx = f_mlalloc; /* boundary stub, see header comment */
            if (qcx != 0) {
                freemd(mem, new_md);
                mem_bxcx r = {0xFFFF, qcx};
                return r;
            }
            /* mall_ml0: unreachable under the boundary-stub convention --
             * see header comment. mau/link_si stay 0 (fresh MAU). */
            break;
        }
        if (last_tried == mem_r16(mem, (uint16_t)(si + ms_mau))) {
            continue;
        }
        uint16_t candidate = mem_r16(mem, (uint16_t)(si + ms_mau));
        if (mall_try_mau(mem, candidate, dx) != 0) {
            continue;
        }
        /* unreachable under the boundary-stub convention -- see header
         * comment -- but translated faithfully regardless */
        mau = candidate;
        link_si = si;
        break;
    }

    /* mall_linkit */
    mem_w16(mem, (uint16_t)(new_md + ms_start), mem_r16(mem, (uint16_t)(dx + mpb_start)));
    mem_w16(mem, (uint16_t)(new_md + ms_length), mem_r16(mem, (uint16_t)(dx + mpb_min)));
    mem_w16(mem, (uint16_t)(new_md + ms_flags), mem_r16(mem, (uint16_t)(dx + mpb_flags)));
    if (link_si == 0) {
        link_si = (uint16_t)(mem_r16(mem, (uint16_t)(dx + mpb_pdadr)) + p_mem - ms_link);
    }
    uint16_t next = mem_r16(mem, (uint16_t)(link_si + ms_link));
    mem_w16(mem, (uint16_t)(new_md + ms_link), next);
    mem_w16(mem, (uint16_t)(link_si + ms_link), new_md);
    mem_w16(mem, (uint16_t)(new_md + ms_mau), mau);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * mfree_it
 *	input:	bx = root, si = MD ([bx]), dx = segment to free
 *	output:	returns {bx, cx}: bx is always the input bx unchanged
 *		(see header comment); cx = error code
 */
static mem_bxcx mfree_it(uint8_t *mem, uint16_t bx, uint16_t si, uint16_t dx) {
    /* push MAF structure ! mov cx,f_maufree ! call mpmif -- boundary
     * stub, no-op; cx is left holding f_maufree's own value, so this
     * always "fails" (cx != 0) under the convention: the real routine
     * pops bx/si/dx back to their call-time values before testing cx, so
     * on this always-taken path it returns bx unchanged and never runs
     * the decrease-length/mfree_off work below -- see header comment */
    uint16_t qcx = f_maufree;
    if (qcx == 0) {
        if (dx == mem_r16(mem, (uint16_t)(si + ms_start))) {
            /* mfree_off -- unreachable under the convention, see above */
        } else {
            /* decrease length */
            mem_w16(mem, (uint16_t)(si + ms_length), (uint16_t)(dx - mem_r16(mem, (uint16_t)(si + ms_start))));
        }
    }
    mem_bxcx r = {bx, qcx};
    return r;
}

/*
 * mfree_entry -- Free the memory segment with the given segment addr.
 *	input:	dx = MFPB in u_wrkseg
 *	output:	bx = 0 if successful, 0xFFFF on failure; cx = error code
 */
mem_bxcx mfree_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bx = mem_r16(mem, (uint16_t)(dx + mfpb_pd));
    uint16_t target = mem_r16(mem, (uint16_t)(dx + mfpb_start));

    if (bx == 0) {
        bx = mem_r16(mem, SD_RLR);
    } else {
        uint16_t walk = (uint16_t)(SD_THRDRT - p_thread); /* bias so the
            first read below lands on thrdrt itself, matching the real
            "mov si,(offset thrdrt)-p_thread" */
        for (;;) {
            walk = mem_r16(mem, (uint16_t)(walk + p_thread)); /* was p_link --
                mismatched the SD_THRDRT (thrdrt - p_thread) bias; same bug
                as src/kernel/MEMORY.MEM's mfree_nxtpd, see docs/src-c.md */
            if (walk == 0) break; /* mfree_gotpd */
            if (walk != bx) continue;
            if (walk == mem_r16(mem, SD_RLR)) break; /* mfree_gotpd */
            mem_bxcx r = {0xFFFF, e_active_pd};
            return r;
        }
    }

    /* mfree_gotpd */
    uint16_t root = (uint16_t)(bx + p_mem);
    for (;;) {
        bx = root;
        uint16_t si = mem_r16(mem, (uint16_t)(bx + ms_link));
        if (si == 0) {
            mem_bxcx r = {0xFFFF, e_no_memory};
            return r;
        }
        uint16_t start = mem_r16(mem, (uint16_t)(si + ms_start));
        if (start == target) {
            mem_bxcx r = mfree_it(mem, bx, si, target);
            return r;
        }
        if (start > target) {
            root = si;
            continue;
        }
        uint16_t end = (uint16_t)(start + mem_r16(mem, (uint16_t)(si + ms_length)));
        if (end <= target) {
            root = si;
            continue;
        }
        mem_bxcx r = mfree_it(mem, bx, si, target);
        if (r.cx != 0) {
            root = si;
            continue;
        }
        mem_bxcx ok = {0, 0};
        return ok;
    }
}
