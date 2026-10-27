/*
 * supif.c -- C port of src-assem/kernel/SUPIF.SUP's `func1`, now including
 * its own real `mod == sup` dispatch-through (the real asm's own
 * `inmod:`/`jmp cs:supfunc[si]` tail -- there's no separate real
 * label for "func1 after inmod," it's genuinely one continuous
 * routine, so extending `func1` itself rather than adding a new name
 * matches the real shape, not a new abstraction).
 *
 * Not ported, same conventions already established elsewhere in this
 * project:
 *   - `user_entry` -- the real `INT 224` entry prologue/epilogue
 *     (critical-section stack switch, `u_insys` nesting count, `iret`
 *     back to the caller) -- no C equivalent, same class as
 *     `src-c/loader/lbdosent.c`'s own `bdose`.
 *   - `func` -- a one-line `mov u_func,cl ! call func1` wrapper.
 *     `u_func` is UDA-relative state read back only by `mpmif`'s own
 *     save/restore (`user_entry`/`mpmif` are both deferred above), so
 *     writing it has no observable effect anywhere this port can see
 *     -- same "collapses to nothing observable" reasoning as
 *     `CHARIO.CIO`'s own `stdout`, see `chario.c`'s header comment.
 *   - `entry`/`mpmif`/`xiosif` -- far-call trampolines, same
 *     convention as every other module's own `entry`/`mpmif`/`xiosif`.
 *     (`entry`'s own `f_flagset` special case calls `func1` directly
 *     too, but its own default path is `call mpmif ! retf` -- not a
 *     second route into the dispatch below, just another far-call
 *     boundary, so porting `entry` itself still adds nothing new.)
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the SYSDAT segment, func1's DS
 * (sysent is a SYSDAT table).
 *
 * `func1` resolves (ch, cl) into (mod, func) exactly as before (the
 * real network-intercept path is still provably unreachable, see
 * below), then:
 *   - `mod == sup`: dispatches into this module's 15-entry `supfunc`
 *     table (0-14) -- every target is ported (`not_implemented`/
 *     `bad_entry`/`version_entry`/`callbios_entry`/`sysdat_entry`/
 *     `tod_entry`/`mpmversion_entry`/`serial_entry` in sysfunc.c,
 *     `load`/`cload_entry` in load.c, `clicmd_entry`/`chain_entry` in
 *     command.c, `rpl_entry` in rpl.c, `parse_entry` in parse.c).
 *     `load_entry` (slot 4) is `load:` with bx forced to 0, called here
 *     as load() with 0; `load` (slot 10) takes the caller's bx (a PD to
 *     initialize), and so does `cload_entry` (slot 14: the asm pushes
 *     the caller's bx as the PD and sets bx = 1, its chain flag), so bx
 *     is a parameter, alongside `dx`/`user_retf_offset`/`cs`.
 *   - `mod != sup`: still a real cross-module far call
 *     (`callf dword ptr module_table[si]`) this port has no way to
 *     make -- the same far-call gap `init.c`'s own module-table
 *     dispatch has, not reconned further here.
 *
 * The real code's network-intercept path (`ent_flag[si]`'s own
 * `ef_network` bit, `callf dword ptr .netmod`) is provably
 * unreachable in this project: no NETWORK module/RSP exists anywhere
 * in `/src-assem`, so `INIT.SUP`'s own RSP-creation loop can never set
 * `module_map`'s own `netmod_bit`, and `test module_map,netmod_bit !
 * jz nonet` always takes the jump -- same "deterministic outcome
 * under the boundary-stub convention" reasoning as `RPL.SUP`'s own
 * `rpl_err` collapse (see `rpl.c`). Collapsed away here rather than
 * modeled and always-false.
 */
#include <stdint.h>

#include "defs.h"

#define sup_module 1 /* MODFUNC.DEF: sup equ 1 */

typedef struct {
    uint16_t bx, cx;
} entry_bxcx;

typedef struct {
    uint16_t bx, cx;
    uint8_t reached_xios;
    uint8_t al, ah, cl, dl;
} callbios_result;

extern uint16_t not_implemented(uint16_t *cx_out);
extern uint16_t bad_entry(uint16_t *cx_out);
extern uint16_t version_entry(uint8_t *phys, uint16_t sysdat, uint16_t *cx_out);
extern callbios_result callbios_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern uint16_t sysdat_entry(uint8_t *phys, uint16_t sysdat, uint16_t *cx_out);
extern uint16_t tod_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *cx_out);
extern uint16_t mpmversion_entry(uint8_t *phys, uint16_t sysdat, uint16_t *cx_out);
extern uint16_t serial_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *cx_out);
extern entry_bxcx load(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx, uint16_t user_retf_offset,
                       uint16_t cs, int load_chain);
extern entry_bxcx cload_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx, uint16_t user_retf_offset,
                              uint16_t cs);
extern entry_bxcx clicmd_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t user_retf_offset, uint16_t cs);
extern entry_bxcx chain_entry(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs);
extern void rpl_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern entry_bxcx parse_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);

typedef struct {
    uint8_t mod;
    uint8_t func;
    uint16_t bx, cx;
    uint8_t reached_xios, al, ah, cl, dl; /* only meaningful for callbios_entry (func==3) */
} sup_func1_result;

/* func1 -- resolve (ch, cl) into (mod, func), then (mod == sup only)
 * dispatch into this module's own supfunc table for real. See this
 * file's own header comment for the exact parameter-to-target
 * mapping and what's still a boundary (mod != sup). */
sup_func1_result func1(uint8_t *phys, uint16_t sysdat, uint8_t ch, uint8_t cl, uint16_t dx, uint16_t bx,
                       uint16_t user_retf_offset, uint16_t cs) {
    sup_func1_result r = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (ch != 0) {
        r.mod = ch;
        r.func = cl;
    } else {
        const uint8_t *sd = far_ptr(phys, sysdat, 0);
        uint16_t si = (uint16_t)(SYSDAT_SYSENT + (uint16_t)(cl * entlen));
        r.func = seg_r8(sd, (uint16_t)(si + ENT_FUNC));
        r.mod = seg_r8(sd, (uint16_t)(si + ENT_MOD));
    }
    if (r.mod != sup_module) {
        return r; /* gofar -- real cross-module far call, no C target */
    }
    switch (r.func) {
    case 0: r.bx = not_implemented(&r.cx); break;
    case 1: r.bx = bad_entry(&r.cx); break;
    case 2: r.bx = version_entry(phys, sysdat, &r.cx); break;
    case 3: {
        callbios_result v = callbios_entry(phys, sysdat, dx);
        r.bx = v.bx; r.cx = v.cx; r.reached_xios = v.reached_xios;
        r.al = v.al; r.ah = v.ah; r.cl = v.cl; r.dl = v.dl;
        break;
    }
    case 4: { /* load_entry: load() with bx forced to 0 */
        entry_bxcx v = load(phys, sysdat, dx, 0, user_retf_offset, cs, 0);
        r.bx = v.bx; r.cx = v.cx;
        break;
    }
    case 5: { entry_bxcx v = clicmd_entry(phys, sysdat, dx, user_retf_offset, cs); r.bx = v.bx; r.cx = v.cx; break; }
    case 6: rpl_entry(phys, sysdat, dx, &r.bx, &r.cx); break;
    case 7: { entry_bxcx v = parse_entry(phys, sysdat, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 8: r.bx = sysdat_entry(phys, sysdat, &r.cx); break;
    case 9: r.bx = tod_entry(phys, sysdat, dx, &r.cx); break;
    case 10: { /* load: the caller's bx */
        entry_bxcx v = load(phys, sysdat, dx, bx, user_retf_offset, cs, 0);
        r.bx = v.bx; r.cx = v.cx;
        break;
    }
    case 11: r.bx = mpmversion_entry(phys, sysdat, &r.cx); break;
    case 12: { entry_bxcx v = chain_entry(phys, sysdat, user_retf_offset, cs); r.bx = v.bx; r.cx = v.cx; break; }
    case 13: r.bx = serial_entry(phys, sysdat, dx, &r.cx); break;
    case 14: { /* cload_entry: the caller's bx is the PD */
        entry_bxcx v = cload_entry(phys, sysdat, dx, bx, user_retf_offset, cs);
        r.bx = v.bx; r.cx = v.cx;
        break;
    }
    default: break;
    }
    return r;
}
