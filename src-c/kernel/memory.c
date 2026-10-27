/*
 * memory.c -- C port of src-assem/kernel/MEMORY.MEM: malloc_entry,
 * mall_try_mau, mfree_entry, mfree_it.
 *
 * Unlike src-assem/kernel/MAU.MEM/SHARE.MEM, none of these switch DS to a
 * dynamically-computed segment and then use a fixed offset relative to
 * it -- only the already-established `u_wrkseg`/`cs:sysdat` patterns --
 * so the real segment-addressing gap documented for those two files (and
 * PROC.RTM/ML.MEM) doesn't apply here.
 *
 * `mall_try_mau`'s/`malloc_entry`'s own `mpmif(f_maualloc)`/
 * `mpmif(f_mlalloc)` calls are now real internal calls into
 * `mau.c`'s `maualloc_entry` and `ml.c`'s `mlalloc_entry` -- both are
 * fully ported, so (same technique as `sysent.c`'s own
 * `creat_proc_entry`, `que1.c`'s own `readq`/`writeq`) there's no
 * reason left to stub them out: `malloc_entry`'s own "successful
 * allocation from an existing MAU or a freshly-created one" paths are
 * real and exercised now, which is what unblocks `LOAD.SUP`'s own
 * `load:` from being able to use this port for its own `mpmif(f_malloc)`
 * dependency (see `load.c`'s own header comment).
 *
 * `mfree_it`'s own `mpmif(f_maufree)` is a real call into `mau.c`'s
 * `maufree_entry` too. The asm builds the MAF on its own stack ("push
 * dx ! push ms_start[si] ! push ms_mau[si] ! mov dx,sp ! push ss!pop
 * ds"), and SUPIF's `mpmif` then sets u_wrkseg to that DS (= SS = the
 * running UDA during a system call). This port puts the MAF at the
 * fixed UDA stack-area spot `UDA_STK_MAF` (defs.h), points the running
 * UDA's u_wrkseg at the UDA for the call, and restores it afterwards.
 *
 * The real asm's own "free an empty MAU back to the Memory Free List"
 * path really does tail-jump clear out of `mfree_it` *and*
 * `mfree_entry` via `mpmif(f_mlfree)` (bypassing both routines' own
 * normal `ret`) -- but since `ml.c`'s own `mlfree_entry` always
 * leaves `bx=0,cx=0` (per that file's own header comment) and
 * `mfree_exit`'s own plain "MAU still in use" tail is *also* always
 * `bx=0,cx=0`, both of `mfree_it`'s own reachable "freed the whole
 * segment" outcomes collapse to the same fixed `{0, 0}` result --
 * translated directly as a return of that fixed pair after the real
 * call, rather than as an actual tail-jump (this port has no such
 * construct). The one *unreachable* edge (the freed MAU not found on
 * the active MAU list, `mal` -- should never happen, since every
 * in-use MAU is invariantly on that list) is translated faithfully
 * too: the real asm's own stale `BX` there is the freed MD's own
 * address (`si`), not a deliberate value -- same "undefined artifact,
 * fixed choice" treatment used throughout this project.
 *
 * `getmd`/`freemd` (src-assem/kernel/UTIL.MEM, util_mem.c) are called as
 * ordinary internal calls below, not boundary stubs. They're declared
 * here with locally-named but layout-identical types rather than shared
 * ones, matching the convention src-c/kernel/que1.c already uses for
 * its own cross-file calls into que2.c.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the MEM
 * module's DS). PDs, the thread list, `p_mem` MS lists, MDs, `mal` and
 * `mfl` are SYSDAT-relative; the MPB/MFPB is in the running process's
 * u_wrkseg (the asm's `mov es,u_wrkseg`), read from its UDA.
 * `mall_ml`/`mall_try_mau` call `mpmif` with DS = u_wrkseg, so u_wrkseg
 * is unchanged across those calls. Their `mov ds,cs:sysdat` afterwards
 * only restores DS -- moot here.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} getmd_result;

extern getmd_result getmd(uint8_t *phys, uint16_t sysdat);
extern void freemd(uint8_t *phys, uint16_t sysdat, uint16_t bx);

typedef struct {
    uint16_t bx, cx;
} mem_bxcx;

extern mem_bxcx maualloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx);
extern mem_bxcx mlalloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx);
extern mem_bxcx maufree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern mem_bxcx mlfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx);

static uint8_t *wrkseg_ptr(uint8_t *phys, const uint8_t *sd) {
    return far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
}

/*
 * mall_try_mau -- try to allocate the MPB's own request out of a
 * specific, already-existing MAU.
 *	input:	bx -> MAU, dx -> MPB in u_wrkseg
 *	output:	returns the error code (0 = ok)
 */
static uint16_t mall_try_mau(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx) {
    return maualloc_entry(phys, sysdat, bx, dx).cx;
}

/*
 * malloc_entry -- Allocate Memory
 *	input:	dx = MPB in u_wrkseg
 *	output:	bx = 0 (unused memory) or 1 (shared code) if successful,
 *		0xFFFF if failure
 *		cx = error code
 */
mem_bxcx malloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    getmd_result md = getmd(phys, sysdat);
    if (md.cx != 0) {
        mem_bxcx r = {0xFFFF, md.cx};
        return r;
    }
    uint16_t new_md = md.bx;

    uint8_t *mpb = wrkseg_ptr(phys, sd);
    uint16_t pdadr = seg_r16(mpb, (uint16_t)(dx + mpb_pdadr));
    if (pdadr == 0) {
        pdadr = seg_r16(sd, SYSDAT_RLR);
        seg_w16(mpb, (uint16_t)(dx + mpb_pdadr), pdadr);
    }

    if (pdadr != seg_r16(sd, SYSDAT_RLR)) {
        uint16_t walk = (uint16_t)(SYSDAT_THRDRT - p_thread); /* bias so the
            first read below lands on thrdrt itself, matching the real
            "mov si,(offset thrdrt)-p_thread" */
        for (;;) {
            walk = seg_r16(sd, (uint16_t)(walk + p_thread));
            if (walk == 0) break; /* mall_pdver */
            if (walk != pdadr) continue;
            if (walk == seg_r16(sd, SYSDAT_RLR)) break; /* mall_pdver */
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
        si = seg_r16(sd, (uint16_t)(si + ms_link));
        if (si == 0) {
            /* mall_ml: create a new MAU from the Memory Free List */
            mem_bxcx ml = mlalloc_entry(phys, sysdat, dx, SYSDAT_MFL);
            if (ml.cx != 0) {
                freemd(phys, sysdat, new_md);
                mem_bxcx r = {0xFFFF, ml.cx};
                return r;
            }
            /* mall_ml0: link the new MAU onto the active MAU list (mal) */
            uint16_t new_mau = ml.bx;
            seg_w16(sd, (uint16_t)(new_mau + m_link), seg_r16(sd, SYSDAT_MAL));
            seg_w16(sd, SYSDAT_MAL, new_mau);
            uint16_t qcx = mall_try_mau(phys, sysdat, new_mau, dx);
            if (qcx != 0) {
                freemd(phys, sysdat, new_md);
                mem_bxcx r = {0xFFFF, qcx};
                return r;
            }
            mau = new_mau;
            link_si = 0; /* si=0 -- "from new MAU", same as the real asm's own mov si,0 */
            break;
        }
        if (last_tried == seg_r16(sd, (uint16_t)(si + ms_mau))) {
            continue;
        }
        uint16_t candidate = seg_r16(sd, (uint16_t)(si + ms_mau));
        if (mall_try_mau(phys, sysdat, candidate, dx) != 0) {
            continue;
        }
        mau = candidate;
        link_si = si;
        break;
    }

    /* mall_linkit */
    seg_w16(sd, (uint16_t)(new_md + ms_start), seg_r16(mpb, (uint16_t)(dx + mpb_start)));
    seg_w16(sd, (uint16_t)(new_md + ms_length), seg_r16(mpb, (uint16_t)(dx + mpb_min)));
    seg_w16(sd, (uint16_t)(new_md + ms_flags), seg_r16(mpb, (uint16_t)(dx + mpb_flags)));
    if (link_si == 0) {
        link_si = (uint16_t)(seg_r16(mpb, (uint16_t)(dx + mpb_pdadr)) + p_mem - ms_link);
    }
    uint16_t next = seg_r16(sd, (uint16_t)(link_si + ms_link));
    seg_w16(sd, (uint16_t)(new_md + ms_link), next);
    seg_w16(sd, (uint16_t)(link_si + ms_link), new_md);
    seg_w16(sd, (uint16_t)(new_md + ms_mau), mau);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * mfree_it
 *	input:	bx = root, si = MD ([bx]), dx = segment to free
 *	output:	returns {bx, cx} -- see header comment for why both of the
 *		"freed the whole segment" sub-cases collapse to {0, 0}
 */
static mem_bxcx mfree_it(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t si, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t mau = seg_r16(sd, (uint16_t)(si + ms_mau));
    /* the MAF, on the running UDA's stack; mpmif with DS = SS */
    uint16_t uda_seg = rlr_uda(sd);
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    seg_w16(uda, (uint16_t)(UDA_STK_MAF + maf_mau), mau);
    seg_w16(uda, (uint16_t)(UDA_STK_MAF + maf_sat), seg_r16(sd, (uint16_t)(si + ms_start)));
    seg_w16(uda, (uint16_t)(UDA_STK_MAF + maf_start), dx);
    uint16_t saved_wrkseg = seg_r16(uda, u_wrkseg);
    seg_w16(uda, u_wrkseg, uda_seg);
    mem_bxcx got = maufree_entry(phys, sysdat, UDA_STK_MAF);
    seg_w16(uda, u_wrkseg, saved_wrkseg);
    uint16_t mau_empty = got.bx; /* 0 = MAU still in use, 0xFFFF = now empty -- only meaningful when cx==0 */
    uint16_t qcx = got.cx;
    if (qcx != 0) {
        mem_bxcx r = {bx, qcx};
        return r;
    }
    if (dx != seg_r16(sd, (uint16_t)(si + ms_start))) {
        /* decrease length */
        seg_w16(sd, (uint16_t)(si + ms_length), (uint16_t)(dx - seg_r16(sd, (uint16_t)(si + ms_start))));
        mem_bxcx r = {bx, 0};
        return r;
    }

    /* mfree_off -- take the MD off its own p_mem list and free it */
    seg_w16(sd, (uint16_t)(bx + ms_link), seg_r16(sd, (uint16_t)(si + ms_link)));
    freemd(phys, sysdat, si);
    if (mau_empty == 0) {
        mem_bxcx r = {0, 0}; /* mfree_exit -- MAU still in use */
        return r;
    }

    /* free the MAU itself -- it's now completely empty: find it on the
     * active MAU list (mal) and hand it back to the Memory Free List */
    uint16_t walk = (uint16_t)(SYSDAT_MAL - m_link);
    for (;;) {
        uint16_t prev = walk;
        walk = seg_r16(sd, (uint16_t)(walk + m_link));
        if (walk == 0) {
            /* mfree_r -- not found on MAL; should never happen in
             * practice (every in-use MAU is invariantly on this list) --
             * see header comment on this edge's own stale BX */
            mem_bxcx r = {si, 0};
            return r;
        }
        if (walk != mau) {
            continue;
        }
        seg_w16(sd, (uint16_t)(prev + m_link), seg_r16(sd, (uint16_t)(mau + m_link)));
        seg_w16(sd, (uint16_t)(mau + m_link), 0);
        return mlfree_entry(phys, sysdat, mau, SYSDAT_MFL); /* jmp mpmif: its output, always 0,0 */
    }
}

/*
 * mfree_entry -- Free the memory segment with the given segment addr.
 *	input:	dx = MFPB in u_wrkseg
 *	output:	bx = 0 if successful, 0xFFFF on failure; cx = error code
 */
mem_bxcx mfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    const uint8_t *mfpb = wrkseg_ptr(phys, sd);
    uint16_t bx = seg_r16(mfpb, (uint16_t)(dx + mfpb_pd));
    uint16_t target = seg_r16(mfpb, (uint16_t)(dx + mfpb_start));

    if (bx == 0) {
        bx = seg_r16(sd, SYSDAT_RLR);
    } else {
        uint16_t walk = (uint16_t)(SYSDAT_THRDRT - p_thread); /* bias so the
            first read below lands on thrdrt itself, matching the real
            "mov si,(offset thrdrt)-p_thread" */
        for (;;) {
            walk = seg_r16(sd, (uint16_t)(walk + p_thread)); /* was p_link --
                mismatched the SYSDAT_THRDRT (thrdrt - p_thread) bias; same bug
                as src-assem/kernel/MEMORY.MEM's mfree_nxtpd, see docs/src-c.md */
            if (walk == 0) break; /* mfree_gotpd */
            if (walk != bx) continue;
            if (walk == seg_r16(sd, SYSDAT_RLR)) break; /* mfree_gotpd */
            mem_bxcx r = {0xFFFF, e_active_pd};
            return r;
        }
    }

    /* mfree_gotpd */
    uint16_t root = (uint16_t)(bx + p_mem);
    for (;;) {
        bx = root;
        uint16_t si = seg_r16(sd, (uint16_t)(bx + ms_link));
        if (si == 0) {
            mem_bxcx r = {0xFFFF, e_no_memory};
            return r;
        }
        uint16_t start = seg_r16(sd, (uint16_t)(si + ms_start));
        if (start == target) {
            mem_bxcx r = mfree_it(phys, sysdat, bx, si, target);
            return r;
        }
        if (start > target) {
            root = si;
            continue;
        }
        uint16_t end = (uint16_t)(start + seg_r16(sd, (uint16_t)(si + ms_length)));
        if (end <= target) {
            root = si;
            continue;
        }
        mem_bxcx r = mfree_it(phys, sysdat, bx, si, target);
        if (r.cx != 0) {
            root = si;
            continue;
        }
        mem_bxcx ok = {0, 0};
        return ok;
    }
}
