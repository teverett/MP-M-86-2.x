/*
 * memif.c -- C port of src/kernel/MEMIF.MEM's `init` AND now `entry`,
 * named here `memif_init`/`memif_entry` -- every module's own real
 * asm label for these two routines is just `init`/`entry`; see
 * bdosif.c's own header comment for why the C ports need a module
 * prefix instead.
 *
 * (the module's own inter-module MEM dispatch). Unlike `CIOIF.CIO`'s
 * own `entry` (plain dispatch), this one wraps every call in a real
 * binary-semaphore acquire/release (`mxmemowner`/`mxmemcount`,
 * a recursive lock: a process that already owns it skips re-
 * acquiring), implemented via the real MX queue `mxmemqpb`
 * (`f_qread` to acquire, `f_qwrite` to release) plus a `pf_tempkeep`/
 * `pf_ctlc` self-terminate check on the way out -- all genuine logic,
 * ported in full; only the `f_qread`/`f_qwrite`/`f_terminate` calls
 * themselves (`mpmif`, a far-call trampoline, same convention as
 * every other module's own) are boundary stubs, modeled as instant
 * no-ops (the acquire always "succeeds" immediately; `f_terminate`
 * falls through to `entry_out` same as the real asm's own call would
 * eventually resume at, if it ever returned at all -- see the
 * boundary-stub convention's own "resumes exactly here" reasoning).
 *
 * `entry_result` is a local {bx,cx} grab-bag matching the identically-
 * shaped (but separately, locally typedef'd) `mem_bxcx`/`mau_bxcx`/
 * `ml_bxcx`/`share_bxcx` each target file already returns -- same
 * "structurally compatible across translation units, no shared header
 * needed" reasoning `cioif.c`'s own `entry` uses for `cio_bxcx`,
 * except this module's own 5 files all keep their own local names
 * (not touched here) since nothing else needs them unified.
 *
 * `freeall_entry` has no C port of its own (`CPMMEM.MEM`'s own
 * one-liner tail-jump into `mpmif(f_terminate)`, nothing to port --
 * see that file's own header comment) -- modeled here as the same
 * boundary-stub no-op as the real `entry`'s own self-terminate call,
 * leaving bx as passed in and cx = f_terminate, as the asm does.
 *
 * `init` (memif_init) has the same shape as CIOIF.CIO's/BDOSIF.BDO's
 * `init`: save `ds_sysdat` in this module's own `cs:sysdat` cell and
 * copy SYSDAT's module_table[SUP] far pointer into `cs:supervisor`
 * (real cseg offsets 6/8/0Ah -- the same 6-byte `jmp init ! jmp entry`
 * header).
 *
 * Both run against real memory (defs.h's far_ptr): `phys` is the base
 * of real physical memory, `sysdat` the real SYSDAT segment (the MEM
 * module's DS) and `cs` this module's own code segment. `rlr`,
 * `mxmemowner`/`mxmemcount` and the PD's `p_flag` are SYSDAT-relative.
 * Every target is called directly, with u_wrkseg left as the caller
 * set it (SUPIF's mpmif already pointed it at the caller's DS).
 */
#include <stdint.h>

#include "defs.h"

MPMIF_HOOK(memif) /* the MX memory queue, f_terminate (defs.h) */

typedef struct {
    uint16_t bx, cx;
} entry_result;

extern entry_result maxmem_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result absmax_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result cpmalloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result cpmabsa_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result cpmfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result malloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result mfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result share_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result maualloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx);
extern entry_result maufree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern entry_result mlalloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx);
extern entry_result mlfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx);

void memif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs) {
    uint8_t *sysdat = far_ptr(phys, ds_sysdat, 0);
    uint8_t *mem_cs = far_ptr(phys, cs, 0);
    wr16(mem_cs + MEMIF_SYSDAT, ds_sysdat);
    wr16(mem_cs + MEMIF_SUPERVISOR_OFF, rd16(sysdat + SYSDAT_SUPMOD));
    wr16(mem_cs + MEMIF_SUPERVISOR_SEG, rd16(sysdat + SYSDAT_SUPMOD + 2));
}

/* entry -- MEMIF.MEM's own inter-module MEM dispatch, wrapped in the
 * module's own binary-semaphore acquire/release.
 *	input:	cl = function # (0-12, functab's own local index);
 *		dx/bx = whichever of this specific function's own real
 *		inputs it needs
 */
entry_result memif_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl, uint16_t dx, uint16_t bx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    uint16_t saved_pflag = 0;
    int reacquired = 0;
    if (seg_r16(sd, SYSDAT_MXMEMOWNER) != rlr) {
        /* mov dx,offset mxmemqpb ! mov cx,f_qread ! call mpmif: take the
         * MX memory queue (waits while another process holds it; with no
         * hook -- a differential test -- it's taken at once) */
        (void)MPMIF(memif, phys, sysdat, f_qread, SYSDAT_MXMEMQPB, 0, sysdat);
        rlr = seg_r16(sd, SYSDAT_RLR);
        seg_w16(sd, SYSDAT_MXMEMOWNER, rlr);
        seg_w8(sd, SYSDAT_MXMEMCOUNT, 0);
        saved_pflag = seg_r16(sd, (uint16_t)(rlr + p_flag));
        seg_w16(sd, (uint16_t)(rlr + p_flag), (uint16_t)(saved_pflag | pf_tempkeep));
        reacquired = 1;
    }
    seg_w8(sd, SYSDAT_MXMEMCOUNT, (uint8_t)(seg_r8(sd, SYSDAT_MXMEMCOUNT) + 1));

    entry_result r = {0, 0};
    switch (cl) {
    case 0: r = maxmem_entry(phys, sysdat, dx); break;
    case 1: r = absmax_entry(phys, sysdat, dx); break;
    case 2: r = cpmalloc_entry(phys, sysdat, dx); break;
    case 3: r = cpmabsa_entry(phys, sysdat, dx); break;
    case 4: r = cpmfree_entry(phys, sysdat, dx); break;
    case 5: { /* freeall_entry -- "mov cx,f_terminate ! sub dx,dx ! jmp
             * mpmif"; with no hook bx is untouched, cx keeps the code */
        mpm_regs t = MPMIF(memif, phys, sysdat, f_terminate, 0, bx, sysdat);
        r.bx = memif_mpmif ? t.bx : bx;
        r.cx = memif_mpmif ? t.cx : f_terminate;
        break;
    }
    case 6: r = malloc_entry(phys, sysdat, dx); break;
    case 7: r = mfree_entry(phys, sysdat, dx); break;
    case 8: r = share_entry(phys, sysdat, dx); break;
    case 9: r = maualloc_entry(phys, sysdat, bx, dx); break;
    case 10: r = maufree_entry(phys, sysdat, dx); break;
    case 11: r = mlalloc_entry(phys, sysdat, dx, bx); break;
    case 12: r = mlfree_entry(phys, sysdat, dx, bx); break;
    default: break;
    }

    seg_w8(sd, SYSDAT_MXMEMCOUNT, (uint8_t)(seg_r8(sd, SYSDAT_MXMEMCOUNT) - 1));
    if (seg_r8(sd, SYSDAT_MXMEMCOUNT) == 0) {
        seg_w16(sd, SYSDAT_MXMEMOWNER, 0);
        if (reacquired && (saved_pflag & pf_tempkeep) == 0) {
            uint16_t pflag = seg_r16(sd, (uint16_t)(rlr + p_flag));
            seg_w16(sd, (uint16_t)(rlr + p_flag), (uint16_t)(pflag & (uint16_t)~pf_tempkeep));
        }
        /* mov dx,offset mxmemqpb ! mov cx,f_qwrite ! call mpmif */
        (void)MPMIF(memif, phys, sysdat, f_qwrite, SYSDAT_MXMEMQPB, 0, sysdat);
        if (seg_r16(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_flag)) & pf_ctlc) {
            /* sub dx,dx ! mov cx,f_terminate ! call mpmif */
            (void)MPMIF(memif, phys, sysdat, f_terminate, 0, 0, sysdat);
        }
    }
    return r;
}
