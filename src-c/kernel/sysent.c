/*
 * sysent.c -- C port of all 8 of src/kernel/SYSENT.RTM's entries.
 *
 * poll_entry/delay_entry/dispatch_entry/set_prior_entry/sleep_entry all
 * tail-jump into the dispatcher (`dsptch`/`pdisp`) and never return in the
 * real system; this port's job for those is the memory/register state
 * right up to that jump (per docs/src-c.md's boundary-stub convention --
 * tests/src-c/test_sysent.py checks the PD fields each one wrote, not a
 * return value). pd_entry and wakeup_entry really do return (RET), so they
 * return bx/cx the normal way.
 *
 * `creat_proc_entry` (`call proc_creat ! jmp pdisp`) was deferred for the
 * same reason as those, before `src-c/kernel/proc.c`'s own `proc_creat`
 * and `src-c/kernel/dsptch.c`'s own `pdisp` existed. Now that they do,
 * unlike its siblings above, it goes all the way: a real internal call
 * into each, returning `pdisp`'s own `switch_result` (see dsptch.c's own
 * header comment for what that stands in for). `switch_result` itself is
 * re-declared here identically to dsptch.c's own (private) typedef --
 * needed verbatim, field-for-field, for the ABI of a function returning
 * it by value.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp;
    uint16_t new_ss, new_sp;
    uint8_t went_to_int_disp_exit;
} switch_result;

uint16_t proc_creat(uint8_t *mem, uint16_t dx, uint16_t sysdat, uint16_t *dx_out);
switch_result pdisp(uint8_t *mem, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx,
                     uint16_t si, uint16_t di, uint16_t bp, uint8_t *was_already_indisp);

/* poll_entry -- Poll device
 *	input:	dx = device
 */
void poll_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    mem[(uint16_t)(bx + p_stat)] = ps_poll;
    mem_w16(mem, UDA_DPARAM, dx);
}

/* delay_entry -- Delay
 *	input:	dx = ticks
 */
void delay_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    mem[(uint16_t)(bx + p_stat)] = ps_delay;
    mem_w16(mem, UDA_DPARAM, dx);
}

/* dispatch_entry -- Call dispatch (no inputs, no memory side effects) */
void dispatch_entry(void) {
}

/* set_prior_entry -- Set Priority
 *	input:	dl = priority
 */
void set_prior_entry(uint8_t *mem, uint8_t dl) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    mem[(uint16_t)(bx + p_prior)] = dl;
}

/* pd_entry -- Return addr of PD
 *	input:	ds = this process's current data segment
 *	output:	returns bx = rlr (the running process's PD address)
 *		*cx_out = 0
 */
uint16_t pd_entry(uint8_t *mem, uint16_t ds, uint16_t *cx_out) {
    mem_w16(mem, UDA_RETSEG, ds);
    *cx_out = 0;
    return mem_r16(mem, SD_RLR);
}

/* sleep_entry -- Put Calling PD on System List
 *	input:	dx = wait parameter
 */
void sleep_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    mem_w16(mem, UDA_DPARAM, dx);
    uint8_t al = mem[(uint16_t)(bx + p_stat)];
    mem[(uint16_t)(bx + p_scratch)] = al;
    mem[(uint16_t)(bx + p_stat)] = ps_sleep;
}

/* wakeup_entry -- wakeup top PD in System List
 *	input:	dx -> System List root
 */
void wakeup_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bx = dx;
    uint16_t si = mem_r16(mem, bx);
    if (si == 0) return; /* wke_out */
    mem_w16(mem, bx, mem_r16(mem, (uint16_t)(si + p_link)));
    mem_w16(mem, (uint16_t)(si + p_link), mem_r16(mem, SD_DRL));
    mem_w16(mem, SD_DRL, si);
    mem[(uint16_t)(si + p_stat)] = ps_run;
    /* call pdisp -- boundary stub, modeled as an instant no-op return */
}

/*
 * creat_proc_entry -- Create Process
 *	input:	dx = pd address in u_wrkseg (pending-creation chain head,
 *		0 = nothing to do); sysdat = this module's own copy of
 *		the SYSDAT segment; ax/bx/cx/si/di/bp = the calling
 *		process's own registers, nominally "to save across the
 *		dispatch" (see below for why `ax`/`bx`/`cx`/`si`/`di`
 *		don't actually end up meaning that here)
 *	output:	returns pdisp's own switch_result
 *
 * `proc_creat`'s own real final `dx` (its own `*dx_out`, see that
 * function's own header comment) feeds `pdisp`, not the original
 * input `dx` -- matching the real asm's own "whatever DX happens to
 * hold when proc_creat falls through to its own ret" (DX is reused
 * as an internal scratch register throughout `proc_creat`'s own
 * per-process setup, and only restored to the next-pending-creation
 * value, 0 on the last pass, right before each loop pass's own exit
 * point).
 *
 * A real, surprising consequence of `call proc_creat ! jmp pdisp`
 * having no save/restore around the call: `ax`/`bx`/`cx`/`si`/`di`
 * are NOT preserved across it (`proc_creat` freely clobbers all five
 * as its own internal scratch, same way `dx` is) -- so whatever
 * `dsptch`'s own "save the calling process's registers" step ends up
 * saving for those five is really just `proc_creat`'s own leftover
 * scratch state, not a value this entry point's own caller ever
 * actually chose. Only visible at all if this same process is later
 * re-dispatched into (switch_'s own restore reads it back) --
 * `tests/src-c/test_sysent.py`'s own `creat_proc_entry` case exercises
 * exactly that, and deliberately does NOT check those five fields of
 * the resulting `switch_result` for that reason (same class as this
 * project's other "real but not meaningfully specified" registers,
 * e.g. `chario.c`'s own `xiosif`-left `bx`) -- only `bp`, which
 * `proc_creat` never touches, is genuinely preserved, and the test
 * checks that one. This port passes `ax`/`bx`/`cx`/`si`/`di` straight
 * through unchanged instead of attempting to re-derive `proc_creat`'s
 * own exact internal leftover values, which would need `proc_creat`
 * to expose its own entire internal scratch state as output -- not
 * worth it for a value no real caller can rely on anyway.
 */
switch_result creat_proc_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat,
                                uint16_t ax, uint16_t bx, uint16_t cx,
                                uint16_t si, uint16_t di, uint16_t bp,
                                uint8_t *was_already_indisp) {
    uint16_t dx_after;
    proc_creat(mem, dx, sysdat, &dx_after);
    return pdisp(mem, ax, bx, cx, dx_after, si, di, bp, was_already_indisp);
}
