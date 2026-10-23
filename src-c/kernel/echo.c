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
 * `pdadr`/`cmdtail`/`console` below are ECHO.A86's own DSEG addresses,
 * confirmed via `python3 tools/asm86.py src/rsp/ECHO.A86 --symbols`, not
 * shared with the rest of the kernel (ECHO.A86 defines everything it
 * needs itself, with no INCLUDEs at all) -- hence hardcoded here rather
 * than added to defs.h.
 *
 * Not modeled: ECHO.A86's own `sdatseg` word (its copy of the pattern
 * seen in MEMIF.MEM/RTMIF.RTM/SUPIF.SUP) sits at cseg offset 0, which
 * numerically collides with this test's own flat memory model placing
 * ECHO's code at address 0 too (on real hardware CS and DS/ES are
 * different physical segments, so there's no actual collision there).
 * This is harmless here specifically because nothing this port implements
 * ever depends on the resulting (garbage, in the test) ES value -- see
 * tools/i86sim.py's own note on segment overrides being no-ops.
 */
#include <stdint.h>

#include "defs.h"

#define ECHO_P_CNS  0x20
#define ECHO_PDADR  0x283
#define ECHO_CMDTAIL 0x285
#define ECHO_CONSOLE 0x319

void echo_loop_once(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, ECHO_PDADR);
    uint8_t dl = mem[(uint16_t)(bx + ECHO_P_CNS)];
    mem[ECHO_CONSOLE] = dl;
    /* mpm_setdefcon -- boundary stub, no-op */

    bx = ECHO_CMDTAIL;
    uint16_t dx = (uint16_t)(bx + 131);
    for (;;) {
        if (bx > dx) break;
        if (mem[bx] == '$' || mem[bx] == 0) break;
        bx++;
    }
    mem[bx] = 13;
    mem[(uint16_t)(bx + 1)] = 10;
    mem[(uint16_t)(bx + 2)] = '$';
    /* mpm_conwrite, mpm_condetach -- boundary stubs, no-op */
}
