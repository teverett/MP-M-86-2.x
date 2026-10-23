/*
 * sysent.c -- C port of 7 of src/kernel/SYSENT.RTM's 8 entries.
 *
 * `creat_proc_entry` is deferred: it's one line (`call proc_creat ! jmp
 * pdisp`) with nothing to port without `proc_creat` itself -- see
 * docs/src-c.md.
 *
 * poll_entry/delay_entry/dispatch_entry/set_prior_entry/sleep_entry all
 * tail-jump into the dispatcher (`dsptch`/`pdisp`) and never return in the
 * real system; this port's job for those is the memory/register state
 * right up to that jump (per docs/src-c.md's boundary-stub convention --
 * tests/src-c/test_sysent.py checks the PD fields each one wrote, not a
 * return value). pd_entry and wakeup_entry really do return (RET), so they
 * return bx/cx the normal way.
 */
#include <stdint.h>

#include "defs.h"

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
