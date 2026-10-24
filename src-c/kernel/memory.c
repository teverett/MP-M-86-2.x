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
 * `mfree_entry`/`mfree_it`'s own `mpmif(f_maufree)` IS upgraded now
 * too, into a real call into `mau.c`'s `maufree_entry` -- the same
 * "it looked like a register-passing convention but is really a
 * genuine MAF.DEF block in memory" resolution `cpmmem.c`'s own
 * `getmemory` fix found for `malloc_entry`'s MPB: the real asm's own
 * "push ms_mau[si]/ms_start[si]/dx ! mov dx,sp ! push ss!pop ds"
 * just builds a MAF at a stack address with DS=SS, which under this
 * port's flat model is simply a fixed scratch block (`SD_MFREE_MAF`,
 * `defs.h`), built before the call and read back after (`BX`'s own
 * "0 = MAU still in use, 0xFFFF = MAU now empty" meaning). `mau.c`'s
 * `maufree_entry` itself reads the MAF through `UDA_WRKSEG`, so this
 * sets that to 0 first (the same "ambient-flat" trick `tmp.c`/
 * `submit.c`'s own `parsefilename` calls already use) -- `mem_r16_seg`
 * with segment 0 degenerates to an ordinary flat read, so the MAF's
 * own address doubles as its own flat offset.
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
 * the active MAU list, `SD_MAL` -- should never happen, since every
 * in-use MAU is invariantly on that list) is translated faithfully
 * too: the real asm's own stale `BX` there is the freed MD's own
 * address (`si`), not a deliberate value -- same "undefined artifact,
 * fixed choice" treatment used throughout this project.
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

extern mem_bxcx maualloc_entry(uint8_t *mem, uint16_t bx, uint16_t dx);
extern mem_bxcx mlalloc_entry(uint8_t *mem, uint16_t dx, uint16_t bx);
extern mem_bxcx maufree_entry(uint8_t *mem, uint16_t dx);
extern void mlfree_entry(uint8_t *mem, uint16_t dx, uint16_t bx);

/*
 * mall_try_mau -- try to allocate the MPB's own request out of a
 * specific, already-existing MAU.
 *	input:	bx -> MAU, dx -> MPB in u_wrkseg
 *	output:	returns the error code (0 = ok)
 */
static uint16_t mall_try_mau(uint8_t *mem, uint16_t bx, uint16_t dx) {
    return maualloc_entry(mem, bx, dx).cx;
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
            mem_bxcx ml = mlalloc_entry(mem, dx, SD_MFL);
            if (ml.cx != 0) {
                freemd(mem, new_md);
                mem_bxcx r = {0xFFFF, ml.cx};
                return r;
            }
            /* mall_ml0: link the new MAU onto the active MAU list (mal) */
            uint16_t new_mau = ml.bx;
            mem_w16(mem, (uint16_t)(new_mau + m_link), mem_r16(mem, SD_MAL));
            mem_w16(mem, SD_MAL, new_mau);
            uint16_t qcx = mall_try_mau(mem, new_mau, dx);
            if (qcx != 0) {
                freemd(mem, new_md);
                mem_bxcx r = {0xFFFF, qcx};
                return r;
            }
            mau = new_mau;
            link_si = 0; /* si=0 -- "from new MAU", same as the real asm's own mov si,0 */
            break;
        }
        if (last_tried == mem_r16(mem, (uint16_t)(si + ms_mau))) {
            continue;
        }
        uint16_t candidate = mem_r16(mem, (uint16_t)(si + ms_mau));
        if (mall_try_mau(mem, candidate, dx) != 0) {
            continue;
        }
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
 *	output:	returns {bx, cx} -- see header comment for why both of the
 *		"freed the whole segment" sub-cases collapse to {0, 0}
 */
static mem_bxcx mfree_it(uint8_t *mem, uint16_t bx, uint16_t si, uint16_t dx) {
    uint16_t mau = mem_r16(mem, (uint16_t)(si + ms_mau));
    mem_w16(mem, UDA_WRKSEG, 0); /* ambient-flat, see header comment */
    mem_w16(mem, (uint16_t)(SD_MFREE_MAF + maf_mau), mau);
    mem_w16(mem, (uint16_t)(SD_MFREE_MAF + maf_sat), mem_r16(mem, (uint16_t)(si + ms_start)));
    mem_w16(mem, (uint16_t)(SD_MFREE_MAF + maf_start), dx);
    mem_bxcx got = maufree_entry(mem, SD_MFREE_MAF);
    uint16_t mau_empty = got.bx; /* 0 = MAU still in use, 0xFFFF = now empty -- only meaningful when cx==0 */
    uint16_t qcx = got.cx;
    if (qcx != 0) {
        mem_bxcx r = {bx, qcx};
        return r;
    }
    if (dx != mem_r16(mem, (uint16_t)(si + ms_start))) {
        /* decrease length */
        mem_w16(mem, (uint16_t)(si + ms_length), (uint16_t)(dx - mem_r16(mem, (uint16_t)(si + ms_start))));
        mem_bxcx r = {bx, 0};
        return r;
    }

    /* mfree_off -- take the MD off its own p_mem list and free it */
    mem_w16(mem, (uint16_t)(bx + ms_link), mem_r16(mem, (uint16_t)(si + ms_link)));
    freemd(mem, si);
    if (mau_empty == 0) {
        mem_bxcx r = {0, 0}; /* mfree_exit -- MAU still in use */
        return r;
    }

    /* free the MAU itself -- it's now completely empty: find it on the
     * active MAU list (SD_MAL) and hand it back to the Memory Free List */
    uint16_t walk = (uint16_t)(SD_MAL - m_link);
    for (;;) {
        uint16_t prev = walk;
        walk = mem_r16(mem, (uint16_t)(walk + m_link));
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
        mem_w16(mem, (uint16_t)(prev + m_link), mem_r16(mem, (uint16_t)(mau + m_link)));
        mem_w16(mem, (uint16_t)(mau + m_link), 0);
        mlfree_entry(mem, mau, SD_MFL);
        mem_bxcx r = {0, 0}; /* mlfree_entry's own tail-jump output, always 0,0 */
        return r;
    }
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
