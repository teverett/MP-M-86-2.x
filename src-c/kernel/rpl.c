/*
 * rpl.c -- C port of src/kernel/RPL.SUP: rpl_entry (Call Resident
 * System Procedure).
 *
 * `rpl_entry`'s real job is to look up a named message queue (the
 * `mpmif(f_qopen)` call), confirm it's flagged as an RPL queue
 * (`qf_rpl`), read one message off it (`mpmif(f_qread)`), and -- this
 * is the whole point of the routine -- transfer control into the
 * Resident System Procedure whose far address that message *is*,
 * using a hand-built fake return-address frame on the stack and a
 * plain `retf` to "return" into it (the frame's own layout is spelled
 * out in RPL.SUP's own ASCII diagram). There is no C equivalent for
 * that transfer at all -- same class of real hardware mechanism as
 * `dsptch.c`'s own documented gaps (a real interrupt return, a real
 * context switch's register/stack resumption): nothing to resume
 * into, no real CS/IP to jump to.
 *
 * Unlike those gaps, though, this one turns out to need no explicit
 * modeling at all, because it's provably unreachable under this
 * project's own boundary-stub convention (docs/src-c.md): a mid-
 * routine `call` into an unported routine is an instant no-op return,
 * meaning `cx` comes back holding whatever it already held going in
 * -- here, the function code just loaded for the call
 * (`f_qopen`/`f_qread`, MODFUNC.DEF: `(user*0100h)+135`/`+137`, never
 * zero). `rpl_entry`'s very next instruction is `cmp cx,0 ! jne
 * rpl_err`, so under this convention it *always* takes the error
 * path, on every call, regardless of the CPB it was given -- the
 * same mechanical reason every other `mpmif`-function-code-gated
 * routine in this tree is deterministic, not a judgment call specific
 * to this file. So, same as `CONSOLE.CIO`'s own unreachable branches
 * (`conbrk`'s "a new character arrived" path, `chkctl`'s wait loop)
 * and `CPMMEM.MEM`'s `cpmfree_entry` `free_seg` retry: the real
 * success path -- the QPB build, both `mpmif` calls, and the `retf`
 * transfer itself -- is real code with a well-defined shape, just
 * never actually reached, so there's nothing to port beyond
 * documenting it here. What *is* ported is the one truly observable,
 * always-taken outcome: `bx=1`, `cx=e_no_queue`; `ds`/`es` end up
 * back exactly where they started (the real asm's own `push ds!push
 * es` / `pop es!pop ds` bracket), so there's nothing for this port to
 * represent there either.
 *
 * `dx` (the CPB's own offset in `u_wrkseg`, read via a bare `mov
 * ds,u_wrkseg` -- a plain DSEG scratch field here, not a UDA one, see
 * `src-c/kernel/share.c`'s own identical `spb`-via-`u_wrkseg`
 * precedent) is accordingly unused: every value the real routine
 * would derive from the CPB (`cpb_name`/`cpb_param`) only ever feeds
 * the QPB build on the unreachable success path.
 */
#include <stdint.h>

#include "defs.h"

void rpl_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out) {
    (void)phys;
    (void)sysdat;
    (void)dx;
    *bx_out = 1;
    *cx_out = e_no_queue;
}
