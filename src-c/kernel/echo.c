/*
 * echo.c -- C port of src/rsp/ECHO.A86's `main:`/`loop:` body.
 *
 * ECHO is a whole Resident System Process: `main:` runs its one-time
 * setup (create/open its queue, set its priority) then falls straight
 * into `loop:`, which never returns (`jmps loop` at the very end). Per
 * docs/src-c.md's boundary-stub convention, this port models exactly one
 * pass through that loop -- reading one command tail off the queue,
 * echoing it, detaching the console -- stopping where the real code would
 * jump back to the top (tests/src-c/test_echo.py stops the real routine
 * at the matching address).
 *
 * Every real syscall ECHO makes goes through its own `mpm:` trampoline
 * (`int mpmint ! ret`); since ECHO never checks a return code from any of
 * them, every one is a `call_stub_addrs` boundary stub (instant no-op
 * return) in the test, and simply isn't called here at all -- there's
 * nothing for this port to do at those points except the side effects
 * this function already makes explicit (reading the PD's console byte,
 * writing the patched command tail).
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `ds` ECHO's data segment, with
 * `sdatseg`/`pdadr`/`cmdtail`/`console` at their real ECHO.A86 DSEG
 * offsets. The queue message (`qread`'s result, preset by the caller)
 * is the CLI's: the sending process's PD, then the command tail. The PD
 * is read through ES = sdatseg, the SYSDAT segment `main:` loads.
 */
#include <stdint.h>

#include "defs.h"

#define ECHO_SDATSEG 0x000
#define ECHO_P_CNS  0x20
#define ECHO_PDADR  0x283
#define ECHO_CMDTAIL 0x285
#define ECHO_CONSOLE 0x319

void echo_loop_once(uint8_t *phys, uint16_t ds) {
    uint8_t *d = far_ptr(phys, ds, 0);
    const uint8_t *es = far_ptr(phys, seg_r16(d, ECHO_SDATSEG), 0);
    seg_w8(d, ECHO_CONSOLE, seg_r8(es, (uint16_t)(seg_r16(d, ECHO_PDADR) + ECHO_P_CNS)));
    /* mpm_setdefcon -- boundary stub, no-op */

    uint16_t bx = ECHO_CMDTAIL;
    uint16_t dx = (uint16_t)(bx + 131);
    while (bx <= dx && seg_r8(d, bx) != '$' && seg_r8(d, bx) != 0) {
        bx++;
    }
    seg_w8(d, bx, 13);
    seg_w8(d, (uint16_t)(bx + 1), 10);
    seg_w8(d, (uint16_t)(bx + 2), '$');
    /* mpm_conwrite, mpm_condetach -- boundary stubs, no-op */
}
