/*
 * cpmmem.c -- C port of src/kernel/CPMMEM.MEM: maxmem_entry,
 * absmax_entry, cpmalloc_entry, cpmabsa_entry, cpmfree_entry (+
 * internal getmemory, free_memory).
 *
 * freeall_entry is deferred -- same reasoning as SYSENT.RTM's
 * creat_proc_entry (see docs/src-c.md): its entire body is "mov
 * cx,f_terminate ! sub dx,dx ! jmp mpmif", a boundary tail-jump from
 * the very first instruction, nothing to port.
 *
 * getmemory's own mpmif(f_malloc) call IS now a real call into
 * memory.c's already-ported malloc_entry, not a boundary stub --
 * the real asm's own "push ax/bx/dx/0/0 ! mov dx,sp ! mov ds,ss" trick
 * isn't a register-passing convention at all: dx/ds:dx just point the
 * callee at a genuine MPB.DEF block (start/min/max/pdadr/flags) living
 * on the caller's own stack, which MAU.MEM's mau_split writes
 * mpb_start back into on success (see src/kernel/MAU.MEM's own "mov
 * es:mpb_start[si],ax"). getmemory's own trailing "pop ax" after the
 * call reads that same slot back, not a stale echo of its own input --
 * so under this port's flat memory model the on-stack block is simply
 * a fixed scratch MPB at SD_CPMMEM_MPB (defs.h), built before the call
 * and read back after. mpb_min is never written by anything in the
 * real chain, so getmemory's own output "dx = length" is genuinely
 * just an echo of its own min input (matching the real "pop dx"
 * reading back the same unmodified push) -- not a bug, just how the
 * real asm itself behaves.
 *
 * free_memory's own mpmif(f_memfree) call remains a boundary stub: it
 * would need mfree_entry (memory.c), whose own mpmif(f_maufree) call
 * is itself still deferred there (see memory.c's own header comment on
 * why that one's tail-jump-out-of-two-call-frames shape needs more
 * care) -- free_memory can't be upgraded until that lands first.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} mem_bxcx;

extern mem_bxcx malloc_entry(uint8_t *mem, uint16_t dx);

/*
 * getmemory
 *	input:	ax = start, bx = min, dx = max
 *	output:	ax = start (as actually allocated), dx = length (== the
 *		original min input -- see header comment), cx = error code
 */
typedef struct {
    uint16_t ax, dx, cx;
} mem_axdxcx;

static mem_axdxcx getmemory(uint8_t *mem, uint16_t ax, uint16_t bx, uint16_t dx) {
    mem_w16(mem, (uint16_t)(SD_CPMMEM_MPB + mpb_start), ax);
    mem_w16(mem, (uint16_t)(SD_CPMMEM_MPB + mpb_min), bx);
    mem_w16(mem, (uint16_t)(SD_CPMMEM_MPB + mpb_max), dx);
    mem_w16(mem, (uint16_t)(SD_CPMMEM_MPB + mpb_pdadr), 0);
    mem_w16(mem, (uint16_t)(SD_CPMMEM_MPB + mpb_flags), 0);
    mem_bxcx r = malloc_entry(mem, SD_CPMMEM_MPB);
    mem_axdxcx out = {mem_r16(mem, (uint16_t)(SD_CPMMEM_MPB + mpb_start)), bx, r.cx};
    return out;
}

/*
 * free_memory
 *	input:	dx = start
 *	output:	cx = error code (always f_memfree, nonzero); the real
 *		routine never actually writes bx itself -- see
 *		cpmfree_entry's own header comment on its tail-jump here
 */
static uint16_t free_memory(uint8_t *mem, uint16_t dx) {
    (void)mem;
    (void)dx;
    /* push the on-stack MFB parameter block ! mov cx,f_memfree ! call
     * mpmif -- boundary stub, no-op */
    return f_memfree;
}

/*
 * maxmem_entry -- Get Max Memory (CP/M-86 call 53)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx maxmem_entry(uint8_t *mem, uint16_t dx) {
    uint16_t si = dx;
    uint16_t max = mem_r16(mem, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(mem, 0, 0, max);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* mm_gm */
    mem_w16(mem, (uint16_t)(si + mcb_length), got.dx);
    mem_w16(mem, (uint16_t)(si + mcb_base), got.ax);
    mem[(uint16_t)(si + mcb_ext)] = 1;
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * absmax_entry -- Get Abs Max Mem (CP/M-86 call 54)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx absmax_entry(uint8_t *mem, uint16_t dx) {
    uint16_t si = dx;
    uint16_t start = mem_r16(mem, (uint16_t)(si + mcb_base));
    uint16_t max = mem_r16(mem, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(mem, start, 0, max);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* am_gm */
    mem_w16(mem, (uint16_t)(si + mcb_length), got.dx);
    mem_w16(mem, (uint16_t)(si + mcb_base), got.ax);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * cpmalloc_entry -- Alloc Mem (CP/M-86 call 55)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx cpmalloc_entry(uint8_t *mem, uint16_t dx) {
    uint16_t si = dx;
    uint16_t length = mem_r16(mem, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(mem, 0, length, length);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* ca_gm */
    mem_w16(mem, (uint16_t)(si + mcb_length), got.dx);
    mem_w16(mem, (uint16_t)(si + mcb_base), got.ax);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * cpmabsa_entry -- Alloc Abs Mem (CP/M-86 call 56)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx cpmabsa_entry(uint8_t *mem, uint16_t dx) {
    uint16_t si = dx;
    uint16_t start = mem_r16(mem, (uint16_t)(si + mcb_base));
    uint16_t length = mem_r16(mem, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(mem, start, length, length);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* caa_gm */
    mem_w16(mem, (uint16_t)(si + mcb_length), got.dx);
    mem_w16(mem, (uint16_t)(si + mcb_base), got.ax);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * cpmfree_entry -- Free Mem (CP/M-86 call 57)
 *	input:	dx = offset of MCB in u_wrkseg; mcb_ext = 0xFF to free all
 *		but Load memory, else mcb_base names the one segment to free
 *	output:	bx = 0 if successful, 0xFFFF on failure; cx = error code --
 *		on the mcb_ext != 0xFF tail-jump into free_memory, the real
 *		routine never writes bx on that path either, so its actual
 *		value there is just whatever the caller's own bx happened to
 *		be; this port fixes that at 0 instead of reproducing an
 *		undefined value
 */
mem_bxcx cpmfree_entry(uint8_t *mem, uint16_t dx) {
    uint16_t si = dx;
    uint8_t ext = mem[(uint16_t)(si + mcb_ext)];
    uint16_t target = mem_r16(mem, (uint16_t)(si + mcb_base));
    if (ext != 0xFF) {
        uint16_t cx = free_memory(mem, target);
        mem_bxcx r = {0, cx};
        return r;
    }

    /* cpmf_root: walk the calling process's own p_mem list, freeing
     * every segment that isn't a Load segment */
    uint16_t root = (uint16_t)(mem_r16(mem, SD_RLR) + p_mem - ms_link);
    uint16_t bx = root;
    for (;;) {
        uint16_t walk = mem_r16(mem, (uint16_t)(bx + ms_link));
        if (walk == 0) {
            mem_bxcx r = {0, 0};
            return r;
        }
        /* try_seg: the real "and ms_flags[si],mf_load" clobbers every
         * other flag bit in place, not just tests one -- translated
         * faithfully */
        uint16_t masked = (uint16_t)(mem_r16(mem, (uint16_t)(walk + ms_flags)) & mf_load);
        mem_w16(mem, (uint16_t)(walk + ms_flags), masked);
        if (masked != 0) {
            bx = walk; /* try_seg's mf_load branch: mov bx,si */
            continue;
        }
        /* free_seg: on a real free (cx==0) go back to cpmf_root, since
         * the list just changed under us. On failure (cx != 0) the real
         * asm's "jmps cpmf_next" leaves bx untouched -- NOT advanced to
         * walk -- which under the boundary-stub convention (free_memory
         * always "fails") spins forever once the list holds a non-Load
         * segment: the same class of issue as QUE1.RTM's cioattach
         * "must actually wait" path, translated faithfully but not
         * exercised by the differential test for that reason, see
         * docs/src-c.md */
        uint16_t target2 = mem_r16(mem, (uint16_t)(walk + ms_start));
        uint16_t qcx = free_memory(mem, target2);
        if (qcx == 0) {
            bx = root; /* cpmf_root, unreachable under the convention */
        }
    }
}
