/*
 * memif.c -- C port of src/kernel/MEMIF.MEM's `init` AND now `entry`
 * (the module's own inter-module MEM dispatch). Unlike `CIOIF.CIO`'s
 * own `entry` (plain dispatch), this one wraps every call in a real
 * binary-semaphore acquire/release (`SD_MXMEMOWNER`/`SD_MXMEMCOUNT`,
 * a recursive lock: a process that already owns it skips re-
 * acquiring), implemented via the real MX queue `SD_MXMEMQPB`
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
 * boundary-stub no-op as the real `entry`'s own self-terminate call.
 *
 * `init` itself is byte-for-byte identical to `CIOIF.CIO`'s own (same
 * 7-instruction body, no `call`/`callf`/`jmp` at all, ending in a far
 * `RET`) -- see that file's own header comment for the full
 * rationale, which applies here unchanged. `sysdat`=6/`supervisor`=
 * 8/0xA are the same real, already-resolved cseg offsets for the same
 * reason (this module's own `jmp init ! jmp entry` header is the
 * identical 6 bytes); confirmed directly against `tests/src-c/
 * fixtures/memif_wrapper.A86` via `--symbols`.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} entry_result;

extern entry_result maxmem_entry(uint8_t *mem, uint16_t dx);
extern entry_result absmax_entry(uint8_t *mem, uint16_t dx);
extern entry_result cpmalloc_entry(uint8_t *mem, uint16_t dx);
extern entry_result cpmabsa_entry(uint8_t *mem, uint16_t dx);
extern entry_result cpmfree_entry(uint8_t *mem, uint16_t dx);
extern entry_result malloc_entry(uint8_t *mem, uint16_t dx);
extern entry_result mfree_entry(uint8_t *mem, uint16_t dx);
extern entry_result share_entry(uint8_t *mem, uint16_t dx);
extern entry_result maualloc_entry(uint8_t *mem, uint16_t bx, uint16_t dx);
extern entry_result maufree_entry(uint8_t *mem, uint16_t dx);
extern entry_result mlalloc_entry(uint8_t *mem, uint16_t dx, uint16_t bx);
extern entry_result mlfree_entry(uint8_t *mem, uint16_t dx, uint16_t bx);

void init(uint8_t *mem, uint16_t ds_sysdat) {
    mem_w16(mem, MEMIF_SYSDAT, ds_sysdat);
    mem_w16(mem, MEMIF_SUPERVISOR_OFF, mem_r16(mem, SD_SUPMOD_OFF));
    mem_w16(mem, MEMIF_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));
}

/* entry -- MEMIF.MEM's own inter-module MEM dispatch, wrapped in the
 * module's own binary-semaphore acquire/release.
 *	input:	cl = function # (0-12, functab's own local index);
 *		dx/bx = whichever of this specific function's own real
 *		inputs it needs
 */
entry_result entry(uint8_t *mem, uint8_t cl, uint16_t dx, uint16_t bx) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t saved_pflag = 0;
    int reacquired = 0;
    if (mem_r16(mem, SD_MXMEMOWNER) != rlr) {
        /* mpmif(f_qread, mxmemqpb) -- boundary stub, acquire always
         * succeeds immediately */
        mem_w16(mem, SD_MXMEMOWNER, rlr);
        mem[SD_MXMEMCOUNT] = 0;
        saved_pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
        mem_w16(mem, (uint16_t)(rlr + p_flag), (uint16_t)(saved_pflag | pf_tempkeep));
        reacquired = 1;
    }
    mem[SD_MXMEMCOUNT] = (uint8_t)(mem[SD_MXMEMCOUNT] + 1);

    entry_result r = {0, 0};
    switch (cl) {
    case 0: r = maxmem_entry(mem, dx); break;
    case 1: r = absmax_entry(mem, dx); break;
    case 2: r = cpmalloc_entry(mem, dx); break;
    case 3: r = cpmabsa_entry(mem, dx); break;
    case 4: r = cpmfree_entry(mem, dx); break;
    case 5: break; /* freeall_entry -- mpmif(f_terminate) tail, boundary stub */
    case 6: r = malloc_entry(mem, dx); break;
    case 7: r = mfree_entry(mem, dx); break;
    case 8: r = share_entry(mem, dx); break;
    case 9: r = maualloc_entry(mem, bx, dx); break;
    case 10: r = maufree_entry(mem, dx); break;
    case 11: r = mlalloc_entry(mem, dx, bx); break;
    case 12: r = mlfree_entry(mem, dx, bx); break;
    default: break;
    }

    mem[SD_MXMEMCOUNT] = (uint8_t)(mem[SD_MXMEMCOUNT] - 1);
    if (mem[SD_MXMEMCOUNT] == 0) {
        mem_w16(mem, SD_MXMEMOWNER, 0);
        if (reacquired && (saved_pflag & pf_tempkeep) == 0) {
            uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
            mem_w16(mem, (uint16_t)(rlr + p_flag), (uint16_t)(pflag & (uint16_t)~pf_tempkeep));
        }
        /* mpmif(f_qwrite, mxmemqpb) -- boundary stub, release always
         * succeeds immediately */
        /* if p_flag & pf_ctlc: mpmif(f_terminate) -- boundary stub,
         * falls through to entry_out either way */
    }
    return r;
}
