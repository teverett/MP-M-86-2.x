/*
 * tick.c -- C port of src/kernel/TICK.SUP: `notick:`/`tick:`.
 *
 * TICK is the MP/M tick-flag RSP: it waits for the hardware clock's
 * tick flag, then walks the Delay List (`dlr`) decrementing the head
 * entry's wait count and moving it (and any further head entries that
 * also happen to expire on the exact same tick) onto the Ready List
 * (`drl`) once its count reaches 0. When the Delay List empties out,
 * it stops the hardware clock (`notick:`) and waits again.
 *
 * Ported as two functions, one per real label, since each is reached
 * a different way: `notick:` is reached only by falling straight
 * through from `tick:`'s own real control flow never loops back to it
 * except via an explicit `jmps notick`, so there's no entry/exit
 * ambiguity splitting it out this way (unlike CLOCK.A86/ECHO.A86's
 * single merged loop body, this file has two distinct re-entrant
 * loop heads).
 *
 * `tick_once` models exactly one real clock tick's worth of work (the
 * Delay List head's own `p_wait` decrement, cascading to any further
 * head entries that expire on the same tick), stopping where the real
 * code would jump back to `tick:` (another tick needed) or `notick:`
 * (Delay List now empty) -- same one-pass-per-call convention as
 * CLOCK.A86/ECHO.A86 (tests/src-c/test_tick.py stops the real routine
 * at either address).
 *
 * `tick:`'s own `mov dx,flag_tick ! mov cx,f_flagwait ! int mpmint`
 * (wait for the tick flag) is a boundary stub per docs/src-c.md's
 * convention -- under it, the wait always "succeeds" instantly, so
 * this port's job starts right after it, at the real Delay List walk.
 *
 * `notick_once` models `notick:`'s own body: `mov es,p_uda[rlr] ! mov
 * u_wrkseg,ds` -- the running (TICK) process's u_wrkseg becomes its DS,
 * SYSDAT (TICK is a SUP process). Its own `mov al,io_stopclk ! call xiosif` (stop the
 * hardware clock) is a boundary stub, no-op; `notick:` always falls
 * straight through into `tick:`'s own flag-wait, so there's nothing
 * further for this function to model.
 *
 * Runs against real memory: `dlr`/`drl` and the PDs are SYSDAT-relative.
 */
#include <stdint.h>

#include "defs.h"

/* notick_once -- stop the hardware clock (no one is on the delay list) */
void notick_once(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    seg_w16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg, sysdat);
    /* mov al,io_stopclk ! call xiosif -- boundary stub, no-op */
}

/* tick_once -- process exactly one real clock tick against the Delay
 * List, moving any now-expired entries onto the Ready List.
 */
void tick_once(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    /* mov dx,flag_tick ! mov cx,f_flagwait ! int mpmint -- boundary
     * stub, no-op: waiting for the tick flag always "succeeds"
     * instantly under the convention */
    uint16_t bx = seg_r16(sd, SYSDAT_DLR);
    if (bx == 0) {
        return; /* drl_empty -> notick */
    }

    uint16_t wait = (uint16_t)(seg_r16(sd, (uint16_t)(bx + p_wait)) - 1);
    seg_w16(sd, (uint16_t)(bx + p_wait), wait);
    if (wait != 0) {
        return; /* next_tick -> tick */
    }

    for (;;) {
        /* enddelay: this entry's wait just hit 0 -- unlink it from the
         * Delay List and push it onto the Ready List */
        uint16_t si = seg_r16(sd, (uint16_t)(bx + p_link));
        seg_w16(sd, SYSDAT_DLR, si);
        sd[(uint16_t)(bx + p_stat)] = ps_run;
        uint16_t ax = seg_r16(sd, SYSDAT_DRL);
        seg_w16(sd, (uint16_t)(bx + p_link), ax);
        seg_w16(sd, SYSDAT_DRL, bx);

        if (si == 0) {
            return; /* drl_empty -> notick */
        }
        if (seg_r16(sd, (uint16_t)(si + p_wait)) != 0) {
            return; /* next_tick -> tick */
        }
        bx = si; /* another entry expires on this same tick -- repeat */
    }
}
