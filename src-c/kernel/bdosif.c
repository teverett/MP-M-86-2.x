/*
 * bdosif.c -- C port of src/kernel/BDOSIF.BDO's `init` and `entry`.
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
 * Every one of these 10 targets' own `ds_bdos` parameter is passed as
 * a flat `0` here, matching `tests/src-c/test_file4.py`'s own
 * established convention for them (see that file's own `func52`
 * case) -- `BD_DLOG`/`BD_RODSK`/`BD_RETURNSEG` etc. are harness-
 * chosen flat scratch addresses, not real `DATA.BDO` offsets, same
 * class as every other `SD_*`/`BD_*` field in this tree.
 *
 * `init` does two things:
 *
 *   1. The same `sysdat`/`supervisor` copy as `CIOIF.CIO`/`MEMIF.MEM`/
 *      `RTMIF.RTM`'s own `init` (see cioif.c's own header comment).
 *      This module's own `init` has no far `CALL` and nothing that
 *      forces genuine segmented addressing the way `RTMIF.RTM`'s own
 *      `init` did (see that file's own header comment), so it's
 *      tested non-segmented, same as `CIOIF.CIO`/`MEMIF.MEM` --
 *      `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG` flat scratch addresses, not
 *      real `SYSDAT.FMT` raw offsets.
 *
 *   2. Creates, opens, and writes the MX disk queue via three real
 *      `mpmif` calls. Traced via `MODFUNC.DEF`'s `f_qmake`(134)/
 *      `f_qopen`(135)/`f_qwrite`(139) through `SYSTEM.DAT`'s own
 *      `sysent` table to `RTMIF.RTM`'s own `function` table entries
 *      4/5/9 -- all already ported in `que1.c` as `makeq_entry`/
 *      `openq_entry`/`writeq_entry`. `mxdiskqd`/`mxdiskqpb`
 *      (`src/kernel/DATA.BDO`) are addressed via `dx`, same flat-
 *      scratch convention as every other `SD_*`-relative field in
 *      this tree (`SD_MXDISKQD`/`SD_MXDISKQPB`, `defs.h`) -- not
 *      their real raw offset within the combined `SYSDAT.FMT`+
 *      `DATA.BDO` dseg (probed directly: 0xf92/0xfae), since
 *      `que1.c`'s own already-verified routines (`SD_QLR`/`SD_RLR`/
 *      etc.) are themselves only correct under that same flat
 *      convention (confirmed via `tests/src-c/test_que.py`'s own
 *      `UDA_WRKSEG == sysdat` setup, which makes `getqdaddr`'s own
 *      "already in MPM range" path a no-op passthrough regardless of
 *      `dx`'s actual value).
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} bdos_bxcx;

extern bdos_bxcx makeq_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat);
extern bdos_bxcx openq_entry(uint8_t *mem, uint16_t dx);
extern bdos_bxcx writeq_entry(uint8_t *mem, uint16_t dx);

void init(uint8_t *mem, uint16_t ds_sysdat) {
    mem_w16(mem, BDOSIF_SYSDAT, ds_sysdat);
    mem_w16(mem, BDOSIF_SUPERVISOR_OFF, mem_r16(mem, SD_SUPMOD_OFF));
    mem_w16(mem, BDOSIF_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));

    /* create disk mx queue */
    (void)makeq_entry(mem, SD_MXDISKQD, ds_sysdat);
    /* open mx disk queue */
    (void)openq_entry(mem, SD_MXDISKQPB);
    /* write mx disk queue */
    (void)writeq_entry(mem, SD_MXDISKQPB);
}

extern uint16_t func24_entry(const uint8_t *mem, uint16_t ds_bdos, uint16_t *cx_out);
extern uint16_t func25_entry(const uint8_t *mem, uint16_t *cx_out);
extern void func26_entry(uint8_t *mem, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern uint16_t func29_entry(const uint8_t *mem, uint16_t ds_bdos, uint16_t *cx_out);
extern uint16_t func32_entry(uint8_t *mem, uint8_t dl, uint16_t *cx_out);
extern void func44_entry(uint8_t *mem, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out);
extern void func45_entry(uint8_t *mem, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out);
extern void func51_entry(uint8_t *mem, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern uint16_t func52_entry(uint8_t *mem, uint16_t ds_bdos, uint16_t ds, uint16_t *cx_out);

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
entry_result entry(uint8_t *mem, uint8_t cl, uint16_t dx, uint16_t bx, uint16_t ds) {
    entry_result r = {bx, cl, 0};
    uint8_t dl = (uint8_t)dx;
    switch (cl) {
    case 11: r.bx = func24_entry(mem, 0, &r.cx); break;
    case 12: r.bx = func25_entry(mem, &r.cx); break;
    case 13: func26_entry(mem, dx, &r.bx, &r.cx); break;
    case 16: r.bx = func29_entry(mem, 0, &r.cx); break;
    case 19: r.bx = func32_entry(mem, dl, &r.cx); break;
    case 31: func44_entry(mem, dl, &r.bx, &r.cx); break;
    case 32: func45_entry(mem, dl, &r.bx, &r.cx); break;
    case 36: func51_entry(mem, dx, &r.bx, &r.cx); break;
    case 37: r.bx = func52_entry(mem, 0, ds, &r.cx); break;
    case 45: break; /* func107/funcret: pure no-op, bx/cx stay ambient */
    default: r.reached_getdiskmx = 1; break;
    }
    return r;
}
