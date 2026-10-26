/*
 * bdosif.c -- C port of src/kernel/BDOSIF.BDO's `init` and `entry`,
 * named here `bdosif_init`/`bdosif_entry` -- every module's own real
 * asm label for these two routines is just `init`/`entry` (purely
 * locally/table-relative in the real system, never a globally
 * unique name), but `CIOIF.CIO`/`MEMIF.MEM`/`RTMIF.RTM`'s own C
 * ports (cioif.c/memif.c/rtmif.c) use the exact same bare names too
 * -- genuinely different real routines colliding once this whole
 * tree is linked into one real image (`src-c/Makefile`'s own
 * `kernel.elf` target), not a copy-paste mistake. Prose below still
 * says `init`/`entry` throughout for the real asm routine itself;
 * only the two C symbols themselves needed the module prefix.
 *
 * `entry`'s own real shape: compute `si` from `cl` (the BDOS function
 * number, 0-46) into `functab`, then branch on that entry's own
 * `btab_flag` byte. Every entry except 10 has `bf_getmx` set, meaning
 * the real dispatch goes through `getdiskmx` first -- a genuinely
 * large piece of real OS plumbing (acquire the mx disk queue,
 * critical-section stack switch, copy the UDA's own BDOS-relevant
 * fields into BDOSIF.BDO's own local area, and -- only for functions
 * whose own `btab_flag` also sets `BF_CSHELL`/`BF_TANDW` -- the SHELL
 * multi-sector-I/O wrapper around `CALL_BDOS` itself) that has no C
 * port here at all; `entry` reports reaching it (`reached_getdiskmx`)
 * rather than silently wrong-answering those 37 functions.
 *
 * The other 10 (`btab_flag == 0`: func24/25/26/29/32/44/45/51/52/107,
 * functions 11/12/13/16/19/31/32/36/37/45) skip `getdiskmx` entirely
 * -- real asm's own `nomx: call cs:btab_addr[si]` -- so `entry` calls
 * straight into their own already-ported-and-tested C targets
 * (file4.c's own `func24_entry`.../`func52_entry`; function 45,
 * `FUNC107 EQU FUNCRET`, is `BDOSIF.BDO`'s own empty `funcret: ret`,
 * a genuine no-op -- `bx`/`cx` just stay whatever they already were,
 * same "collapses to nothing observable" convention as `CHARIO.CIO`'s
 * own `stdout`). `func26` itself was found genuinely missing (not
 * merely deferred) while wiring this up -- see file4.c's own header
 * comment -- and is now ported too.
 *
 * `init` and `entry` both run against real memory (defs.h's far_ptr),
 * like the RTM: `phys` is the base of real physical memory and the
 * SYSDAT segment is the BDOS's DS (SUPIF enters it with DS = sysdat; the
 * combined SYSDAT+DATA.BDO data segment, so `BD_*` are DATA.BDO's real
 * offsets in it). `entry` passes it to every target as their `ds_bdos`.
 * `init` also takes `cs`, this module's own code segment. `init`:
 *
 *   1. Saves `ds_sysdat` in this module's own `cs:sysdat` cell and copies
 *      SYSDAT's module_table[SUP] entry far pointer into
 *      `cs:supervisor` (same shape as CIOIF.CIO/MEMIF.MEM/RTMIF.RTM's
 *      `init`).
 *
 *   2. Creates, opens, and writes the MX disk queue via three real
 *      `mpmif` calls -- `f_qmake`/`f_qopen`/`f_qwrite`, which reach
 *      que1.c's makeq_entry/openq_entry/writeq_entry (also on real
 *      memory). `mxdiskqd`/`mxdiskqpb` are DATA.BDO's real offsets in
 *      the combined SYSDAT+DATA.BDO data segment (SYSDAT_MXDISKQD/
 *      SYSDAT_MXDISKQPB). The real call goes through BDOSIF's and then
 *      SUPIF's `mpmif`, and the latter sets the running process's
 *      u_wrkseg to the caller's DS (SYSDAT here) for the call and
 *      restores it afterwards -- the queue code resolves dx through
 *      u_wrkseg, so the direct calls here do the same.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} bdos_bxcx;

extern bdos_bxcx makeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern bdos_bxcx openq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern bdos_bxcx writeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);

void bdosif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs) {
    uint8_t *sysdat = far_ptr(phys, ds_sysdat, 0);
    uint8_t *bdos_cs = far_ptr(phys, cs, 0);

    wr16(bdos_cs + BDOSIF_SYSDAT, ds_sysdat);
    wr16(bdos_cs + BDOSIF_SUPERVISOR_OFF, rd16(sysdat + SYSDAT_SUPMOD));
    wr16(bdos_cs + BDOSIF_SUPERVISOR_SEG, rd16(sysdat + SYSDAT_SUPMOD + 2));

    /* mpmif: u_wrkseg = caller's DS for the call, restored after */
    uint8_t *uda = far_ptr(phys, rlr_uda(sysdat), 0);
    uint16_t saved_wrkseg = rd16(uda + u_wrkseg);
    wr16(uda + u_wrkseg, ds_sysdat);

    /* create disk mx queue */
    (void)makeq_entry(phys, ds_sysdat, SYSDAT_MXDISKQD);
    /* open mx disk queue */
    (void)openq_entry(phys, ds_sysdat, SYSDAT_MXDISKQPB);
    /* write mx disk queue */
    (void)writeq_entry(phys, ds_sysdat, SYSDAT_MXDISKQPB);

    wr16(uda + u_wrkseg, saved_wrkseg);
}

extern uint16_t func24_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);
extern uint16_t func25_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);
extern void func26_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern uint16_t func29_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);
extern uint16_t func32_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *cx_out);
extern void func44_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out);
extern void func45_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out);
extern void func51_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern uint16_t func52_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);

typedef struct {
    uint16_t bx, cx;
    uint8_t reached_getdiskmx; /* 1 = real asm would call getdiskmx
        (critical-section switch + possibly SHELL + CALL_BDOS) --
        not modeled, see this file's own header comment; bx is just
        this call's own ambient input in that case, not a real
        answer, and cx is cl zero-extended (see below) */
} entry_result;

/* entry -- resolve cl (BDOS function #, 0-46) into functab's own
 * entry, then (btab_flag == 0 only) dispatch for real into that
 * function's own already-ported C target. See this file's own
 * header comment for exactly which 10 functions that is and why the
 * other 37 are a documented boundary.
 *
 * Real `entry`'s own prologue (`mov ch,0 ! ... ! mul cx ! ...`) only
 * ever clears `ch` -- `cl` (the function # this call was made with)
 * stays in `cx` for the rest of the routine, including into
 * whichever target it dispatches to, unless that target's own body
 * explicitly overwrites it. There's no genuinely independent
 * "ambient cx" input here the way there is for `bx`/`dx` -- so
 * `entry` has no `cx` parameter of its own; `r.cx` starts as `cl`
 * zero-extended (func107/FUNCRET's real no-op return, case 45,
 * leaves it at exactly that) and each other case overwrites it with
 * its own target's real result, same as every other dispatcher in
 * this tree. */
entry_result bdosif_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl, uint16_t dx, uint16_t bx) {
    entry_result r = {bx, cl, 0};
    uint8_t dl = (uint8_t)dx;
    switch (cl) {
    case 11: r.bx = func24_entry(phys, sysdat, &r.cx); break;
    case 12: r.bx = func25_entry(phys, sysdat, &r.cx); break;
    case 13: func26_entry(phys, sysdat, dx, &r.bx, &r.cx); break;
    case 16: r.bx = func29_entry(phys, sysdat, &r.cx); break;
    case 19: r.bx = func32_entry(phys, sysdat, dl, &r.cx); break;
    case 31: func44_entry(phys, sysdat, dl, &r.bx, &r.cx); break;
    case 32: func45_entry(phys, sysdat, dl, &r.bx, &r.cx); break;
    case 36: func51_entry(phys, sysdat, dx, &r.bx, &r.cx); break;
    case 37: r.bx = func52_entry(phys, sysdat, &r.cx); break;
    case 45: break; /* func107/funcret: pure no-op, bx/cx stay ambient */
    default: r.reached_getdiskmx = 1; break;
    }
    return r;
}
