/*
 * cpmmem.c -- C port of src-assem/kernel/CPMMEM.MEM: maxmem_entry,
 * absmax_entry, cpmalloc_entry, cpmabsa_entry, cpmfree_entry (+
 * internal getmemory, free_memory).
 *
 * freeall_entry is deferred -- same reasoning as SYSENT.RTM's
 * creat_proc_entry (see docs/src-c.md): its entire body is "mov
 * cx,f_terminate ! sub dx,dx ! jmp mpmif", a boundary tail-jump from
 * the very first instruction, nothing to port.
 *
 * getmemory's own mpmif(f_malloc) call is a real call into memory.c's
 * malloc_entry, not a boundary stub. The asm's "push ax/bx/dx/0/0 !
 * mov dx,sp ! mov ds,ss" builds a genuine MPB.DEF block
 * (start/min/max/pdadr/flags) on its own stack and hands it over with
 * DS = SS, which SUPIF's mpmif turns into u_wrkseg = SS -- the running
 * UDA during a system call. On success MAU.MEM's maua_alloc writes the
 * allocated start and length back into it (mpb_start, and mpb_min =
 * mpb_max = the length), and getmemory's trailing "pop ax ! pop dx"
 * reads mpb_start/mpb_min back. An earlier version of this port echoed
 * the min input as the length instead, which only agreed when min ==
 * max (cpmalloc/cpmabsa): maxmem/absmax (min 0) returned length 0
 * instead of what was actually allocated.
 *
 * free_memory's own mpmif(f_memfree) call is likewise a real call into
 * memory.c's mfree_entry, with a genuine on-stack MFPB.DEF block
 * (start/pdadr). Its real tail ("pop dx!pop dx!pop ds!ret") never
 * restores BX or CX, so its output is mfree_entry's own {bx, cx}
 * passed straight through, and so is cpmfree_entry's on the
 * mcb_ext != 0xFF path (a real tail-jump, `jne free_memory`).
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the MEM
 * module's DS). The MCB is in the running process's u_wrkseg (the
 * asm's `mov ds,u_wrkseg`), the `p_mem` walk SYSDAT-relative. The two
 * on-stack blocks go at the fixed UDA stack-area spots UDA_STK_MPB/
 * UDA_STK_MFPB (defs.h), with u_wrkseg pointed at the UDA for the call
 * and restored afterwards, as mpmif does.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} mem_bxcx;

extern mem_bxcx malloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern mem_bxcx mfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);

static uint8_t *mcb_ptr(uint8_t *phys, const uint8_t *sd) {
    return far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
}

/*
 * getmemory
 *	input:	ax = start, bx = min, dx = max
 *	output:	ax = start, dx = length (both as actually allocated, read
 *		back from the MPB -- see header comment), cx = error code
 */
typedef struct {
    uint16_t ax, dx, cx;
} mem_axdxcx;

static mem_axdxcx getmemory(uint8_t *phys, uint16_t sysdat, uint16_t ax, uint16_t bx, uint16_t dx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t uda_seg = rlr_uda(sd);
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    seg_w16(uda, (uint16_t)(UDA_STK_MPB + mpb_start), ax);
    seg_w16(uda, (uint16_t)(UDA_STK_MPB + mpb_min), bx);
    seg_w16(uda, (uint16_t)(UDA_STK_MPB + mpb_max), dx);
    seg_w16(uda, (uint16_t)(UDA_STK_MPB + mpb_pdadr), 0);
    seg_w16(uda, (uint16_t)(UDA_STK_MPB + mpb_flags), 0);
    uint16_t saved_wrkseg = seg_r16(uda, u_wrkseg);
    seg_w16(uda, u_wrkseg, uda_seg); /* mpmif with DS = SS */
    mem_bxcx r = malloc_entry(phys, sysdat, UDA_STK_MPB);
    seg_w16(uda, u_wrkseg, saved_wrkseg);
    mem_axdxcx out = {seg_r16(uda, (uint16_t)(UDA_STK_MPB + mpb_start)),
                      seg_r16(uda, (uint16_t)(UDA_STK_MPB + mpb_min)), r.cx};
    return out;
}

/*
 * free_memory
 *	input:	dx = start
 *	output:	returns mfree_entry's own {bx, cx} straight through -- see
 *		header comment
 */
static mem_bxcx free_memory(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t uda_seg = rlr_uda(sd);
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    seg_w16(uda, (uint16_t)(UDA_STK_MFPB + mfpb_start), dx);
    seg_w16(uda, (uint16_t)(UDA_STK_MFPB + mfpb_pd), 0);
    uint16_t saved_wrkseg = seg_r16(uda, u_wrkseg);
    seg_w16(uda, u_wrkseg, uda_seg); /* mpmif with DS = SS */
    mem_bxcx r = mfree_entry(phys, sysdat, UDA_STK_MFPB);
    seg_w16(uda, u_wrkseg, saved_wrkseg);
    return r;
}

/*
 * maxmem_entry -- Get Max Memory (CP/M-86 call 53)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx maxmem_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *mcb = mcb_ptr(phys, sd);
    uint16_t si = dx;
    uint16_t max = seg_r16(mcb, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(phys, sysdat, 0, 0, max);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* mm_gm */
    seg_w16(mcb, (uint16_t)(si + mcb_length), got.dx);
    seg_w16(mcb, (uint16_t)(si + mcb_base), got.ax);
    seg_w8(mcb, (uint16_t)(si + mcb_ext), 1);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * absmax_entry -- Get Abs Max Mem (CP/M-86 call 54)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx absmax_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *mcb = mcb_ptr(phys, sd);
    uint16_t si = dx;
    uint16_t start = seg_r16(mcb, (uint16_t)(si + mcb_base));
    uint16_t max = seg_r16(mcb, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(phys, sysdat, start, 0, max);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* am_gm */
    seg_w16(mcb, (uint16_t)(si + mcb_length), got.dx);
    seg_w16(mcb, (uint16_t)(si + mcb_base), got.ax);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * cpmalloc_entry -- Alloc Mem (CP/M-86 call 55)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx cpmalloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *mcb = mcb_ptr(phys, sd);
    uint16_t si = dx;
    uint16_t length = seg_r16(mcb, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(phys, sysdat, 0, length, length);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* ca_gm */
    seg_w16(mcb, (uint16_t)(si + mcb_length), got.dx);
    seg_w16(mcb, (uint16_t)(si + mcb_base), got.ax);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * cpmabsa_entry -- Alloc Abs Mem (CP/M-86 call 56)
 *	input:	dx = address of MCB in u_wrkseg
 *	output:	bx = 0xFFFF if failure, 0 if success; cx = error code
 */
mem_bxcx cpmabsa_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *mcb = mcb_ptr(phys, sd);
    uint16_t si = dx;
    uint16_t start = seg_r16(mcb, (uint16_t)(si + mcb_base));
    uint16_t length = seg_r16(mcb, (uint16_t)(si + mcb_length));
    mem_axdxcx got = getmemory(phys, sysdat, start, length, length);
    if (got.cx != 0) {
        mem_bxcx r = {0xFFFF, got.cx};
        return r;
    }
    /* caa_gm */
    seg_w16(mcb, (uint16_t)(si + mcb_length), got.dx);
    seg_w16(mcb, (uint16_t)(si + mcb_base), got.ax);
    mem_bxcx r = {0, 0};
    return r;
}

/*
 * cpmfree_entry -- Free Mem (CP/M-86 call 57)
 *	input:	dx = offset of MCB in u_wrkseg; mcb_ext = 0xFF to free all
 *		but Load memory, else mcb_base names the one segment to free
 *	output:	returns free_memory's own {bx, cx} straight through on the
 *		mcb_ext != 0xFF path (a real tail-jump in the real asm) --
 *		see header comment
 *
 * The free-all walk used to `and` each segment's flags with mf_load
 * (wiping a kept Load segment's other flag bits) where a test was
 * meant, and retried a segment that failed to free forever; both are
 * fixed here and in src-assem/kernel/CPMMEM.MEM (docs/bugs.md #107).
 */
mem_bxcx cpmfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *mcb = mcb_ptr(phys, sd);
    uint16_t si = dx;
    uint8_t ext = seg_r8(mcb, (uint16_t)(si + mcb_ext));
    uint16_t target = seg_r16(mcb, (uint16_t)(si + mcb_base));
    if (ext != 0xFF) {
        return free_memory(phys, sysdat, target);
    }

    /* cpmf_root: walk the calling process's own p_mem list, freeing
     * every segment that isn't a Load segment */
    uint16_t root = (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_mem - ms_link);
    uint16_t bx = root;
    for (;;) {
        uint16_t walk = seg_r16(sd, (uint16_t)(bx + ms_link));
        if (walk == 0) {
            mem_bxcx r = {0, 0};
            return r;
        }
        /* try_seg: Load segments stay */
        if (seg_r16(sd, (uint16_t)(walk + ms_flags)) & mf_load) {
            bx = walk; /* mov bx,si */
            continue;
        }
        /* free_seg: on a real free (cx==0) go back to cpmf_root, since
         * the list just changed under us; on failure step past this
         * segment */
        uint16_t target2 = seg_r16(sd, (uint16_t)(walk + ms_start));
        mem_bxcx fr = free_memory(phys, sysdat, target2);
        bx = (fr.cx == 0) ? root : walk;
    }
}
