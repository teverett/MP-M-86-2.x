/*
 * tmp.c -- C port of src/rsp/TMP.A86's nextcommand:/clicall:/clierror:
 * body (the MP/M "Terminal Message Processor" shell's own main loop).
 *
 * TMP is a whole Resident System Process: `tmp:` runs its own one-time
 * setup (set default console/disk/user, print the version banner) then
 * falls into `nextcommand:`, which never returns. Per docs/src-c.md's
 * boundary-stub convention, this port models exactly one pass through
 * that loop, same precedent as src-c/kernel/echo.c's own `loop:` and
 * src-c/kernel/clock.c's own `clockloop:` (tests/src-c/test_tmp.py stops
 * the real routine at `nextcommand`'s own address).
 *
 * `tmp:`'s own one-time setup is moot under this convention and isn't
 * modeled at all: every single operation in it is either a boundary-stub
 * `call mpm` (setconsole/setdisk/setuser/attach/print_ds_string/detach,
 * none of which have an observable memory effect under the convention)
 * or a register load feeding one, the same "nothing to port" reasoning
 * as src-c/kernel/cpmmem.c's own `freeall_entry`.
 *
 * Every real syscall TMP makes goes through its own `mpm:` trampoline
 * (`int mpmint ! ret`), reached via a tail-JMP chain through one of
 * TMP.A86's own small per-function trampolines (e.g. `prchar: mov
 * cl,mpm_conout ! jmp mpm1` / `mpm1: jmp mpm`) rather than ECHO.A86's/
 * CLOCK.A86's own direct `call mpm` at every site. `tools/i86sim.py`'s
 * `call_stub_addrs` only intercepts a near CALL landing exactly on a
 * listed address, which a JMP chain never does -- so this port's own
 * test instead stubs the `INT mpmint` instruction itself
 * (`int_stub_nums={224}`, added for this file after all): the no-op
 * stub resumes at `mpm`'s own `ret`, which unwinds correctly back to
 * the original CALL site no matter how many tail JMPs preceded it.
 *
 * Per the boundary-stub convention, a no-op `call mpm` leaves every
 * register exactly as it was immediately before the call -- so any
 * register the real code happens to pre-load right before the call
 * (e.g. `parsefilename`'s `mov cx,mpm_parse`) comes back deterministically
 * unchanged, while one nothing pre-loads (e.g. `getuser`'s own `bl`
 * result) is a genuinely undefined, caller-dependent quantity, same
 * distinction CONSOLE.CIO's `xiosif` needed (see that file's own
 * docs/src-c.md writeup). This drives two real consequences here:
 *
 * - `getuser`/`getdisk`/`getlist`'s own results (printed via `prnum`/
 *   `prchar` in the prompt) are undefined-but-unobservable: print calls
 *   are themselves no-op boundary stubs with no memory effect in this
 *   flat model, so nothing downstream of them needs modeling at all.
 * - `clicall`'s own first `call parsefilename` deterministically returns
 *   `cx = mpm_parse` (152) every time (pre-loaded right before the tail
 *   JMP into `mpm`), which never satisfies `jcxz goodparse` (152 != 0)
 *   and never matches any of `clierror`'s own real error codes either
 *   (all < 40) -- so `goodparse` and everything downstream of it
 *   (the built-in USER/PRINTER command checks, `not_builtin`'s own real
 *   CLI dispatch, and all but `clierror`'s final catch-all branch) are
 *   real, provably unreachable code under this convention, same class
 *   as src-c/kernel/console.c's own un-exercised `ca_sleep` retry loop
 *   and src-c/kernel/cpmmem.c's own `cpmfree_entry` free_seg path -- not
 *   translated here since there's no way to reach, let alone verify,
 *   them under the test's own mechanical rules.
 *
 * `cmdsent`/`read_blen`/`clicb_cmd`/`savebuf` are TMP.A86's own DSEG
 * fields, moved to fixed harness-chosen scratch addresses (SD_TMP_*,
 * see defs.h) since TMP.A86's own real DSEG offsets collide with its
 * own cseg -- see defs.h's own comment on them.
 */
#include <stdint.h>
#include <string.h>

#include "defs.h"

#define TMP_MAXCMDLEN 128

/* nextcommand_once -- one pass through the TMP shell's own main loop.
 *
 * Models every real memory effect reachable from `nextcommand:` with
 * `read_blen`/`clicb_cmd` preset as this pass's "typed command" (same
 * stand-in-via-preset-memory convention as CLOCK.A86's own TOD bytes):
 * clearing `cmdsent` on entry if it was set, and -- for a non-empty,
 * non-`;`-prefixed command that isn't a 2-char `X:` disk-change
 * shortcut -- null-terminating `clicb_cmd`, copying it to `savebuf`
 * (the real code always copies a fixed 128 bytes via `rep movsw`,
 * regardless of the real command length), and `savebuf`'s own
 * '$'-terminator write right before the (always-taken, see header
 * comment) fall into `clierror`'s catch-all branch.
 */
void nextcommand_once(uint8_t *mem) {
    /* mov dx,0ffffh ! call freedrive -- boundary stub, no-op */
    /* call attach -- boundary stub, no-op */

    if (mem[SD_TMP_CMDSENT] != 0) {
        mem[SD_TMP_CMDSENT] = 0;
        /* call crlf -- boundary stub, no-op */
    }

    /* print prompt (getuser/prnum/getdisk/prchar/printstring) --
     * boundary stubs, no memory effect: print calls have nothing
     * observable to model in this flat memory model */

    /* mov dx,offset read_buf ! call conread -- boundary stub; the
     * "result" (read_blen/clicb_cmd) is this call's own test input,
     * already preset in mem before this function is called */

    /* mov dl,lf ! call prchar -- boundary stub, no-op */

    uint8_t read_blen = mem[SD_TMP_READ_BLEN];
    uint8_t *clicb_cmd = &mem[SD_TMP_CLICB_CMD];

    if (read_blen == 0 || clicb_cmd[0] == ';') {
        return; /* gonextcmd */
    }

    if (read_blen == 2 && clicb_cmd[1] == ':') {
        uint8_t dl = (uint8_t)(clicb_cmd[0] & 0x5F); /* upper case */
        dl = (uint8_t)(dl - 'A');
        if (dl <= 15) {
            /* call setdisk -- boundary stub, no-op */
        }
        return; /* gonextcmd */
    }

    /* clicall: */
    clicb_cmd[read_blen] = 0;
    memcpy(&mem[SD_TMP_SAVEBUF], clicb_cmd, TMP_MAXCMDLEN);

    /* call parsefilename -- boundary stub; cx deterministically comes
     * back as mpm_parse (152), so `jcxz goodparse` is never taken (see
     * header comment) -- falls straight through to the bad-parse path */
    mem[(uint16_t)(SD_TMP_SAVEBUF + read_blen)] = '$';

    /* jmp clierror -- cx=152 matches none of clierror's real error
     * codes (all < 40), so this always falls through to the catch-all
     * `mov dx,offset catcherr` -> showerr; showerr's own print calls
     * (print_ds_string/prchar/printstring/crlf) are boundary stubs
     * with no memory effect to model */
}
