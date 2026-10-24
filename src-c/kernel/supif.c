/*
 * supif.c -- C port of src/kernel/SUPIF.SUP's `func1`.
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
 *
 * `func1` is the real "resolve a function number into a target
 * module+function" dispatcher, and is ported in full up to the point
 * where the real code commits to one of two tails neither of which
 * has a C target to call into yet:
 *   - `mod == sup`: `jmp cs:supfunc[func]`, a near tail-jump into this
 *     same module's own 14-entry function table -- every target is
 *     already ported (`not_implemented`/`bad_entry`/`version_entry`/
 *     `callbios_entry` in sysfunc.c, `load_entry`/`load`/`cload_entry`
 *     in load.c, `clicmd_entry` in command.c, `rpl_entry` in rpl.c,
 *     `parse_entry` in parse.c, `sysdat_entry`/`tod_entry`/
 *     `mpmversion_entry`/`chain_entry`/`serial_entry` in sysfunc.c/
 *     command.c), same "wires up work already done, not reconned
 *     into one call-through switch yet" framing as `bdosif.c`'s own
 *     `CALL_BDOS` header comment.
 *   - `mod != sup`: `callf dword ptr module_table[si]`, a real
 *     cross-module far call into another module's own `entry` --
 *     the same far-call gap `init.c`'s own module-table dispatch has,
 *     not reconned further here either.
 *
 * The real code's network-intercept path (`ent_flag[si]`'s own
 * `ef_network` bit, `callf dword ptr .netmod`) is provably
 * unreachable in this project: no NETWORK module/RSP exists anywhere
 * in `/src`, so `INIT.SUP`'s own RSP-creation loop can never set
 * `module_map`'s own `netmod_bit`, and `test module_map,netmod_bit !
 * jz nonet` always takes the jump -- same "deterministic outcome
 * under the boundary-stub convention" reasoning as `RPL.SUP`'s own
 * `rpl_err` collapse (see `rpl.c`). Collapsed away here rather than
 * modeled and always-false.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint8_t mod;
    uint8_t func;
} sup_func1_result;

/* func1 -- resolve (ch, cl) into (mod, func).
 * ch != 0: an explicit module/function pair, passed straight through.
 * ch == 0: a user function number, looked up in SYSDAT's own 256-entry
 * `sysent` table (3 bytes/entry: mod, func, flag -- the network check
 * on the flag byte always resolves to "no network", see above, so
 * only mod/func are ever read). */
sup_func1_result func1(const uint8_t *mem, uint16_t ds, uint8_t ch, uint8_t cl) {
    sup_func1_result r;
    if (ch != 0) {
        r.mod = ch;
        r.func = cl;
        return r;
    }
    uint16_t si = (uint16_t)(SD_SYSENT + (uint16_t)(cl * entlen));
    r.func = mem_r8_seg(mem, ds, (uint16_t)(si + ENT_FUNC));
    r.mod = mem_r8_seg(mem, ds, (uint16_t)(si + ENT_MOD));
    return r;
}
