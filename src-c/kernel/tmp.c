/*
 * tmp.c -- C port of src/rsp/TMP.A86's nextcommand:/clicall:/
 * goodparse:/try_builtin:/not_builtin:/clierror: body (the MP/M
 * "Terminal Message Processor" shell's own main loop).
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
 * comes back deterministically unchanged, while one nothing pre-loads
 * (e.g. `getuser`'s own `bl` result) is a genuinely undefined, caller-
 * dependent quantity, same distinction CONSOLE.CIO's `xiosif` needed
 * (see that file's own docs/src-c.md writeup). `getuser`/`getdisk`/
 * `getlist`'s own results (printed via `prnum`/`prchar`) stay
 * undefined-but-unobservable: print calls are themselves no-op
 * boundary stubs with no memory effect in this flat model, so nothing
 * downstream of them needs modeling. `setuser`/`setlist` are the same
 * -- their own real targets were never identified (no longer matters:
 * their only observable job, under this flat model, is a `jmp pruser`/
 * `jmp prprinter` either way, which this port already takes directly).
 *
 * `clicall`'s own `call parsefilename` was ORIGINALLY treated as an
 * unidentified boundary stub, with `cx` deterministically coming back
 * as `mpm_parse` (152, pre-loaded right before the tail JMP into
 * `mpm`) -- which never satisfies `jcxz goodparse`, making `goodparse`
 * and everything downstream provably unreachable. That turned out to
 * be a mischaracterization, not a real dead end: `parsefilename`'s
 * own real target is `mpm_parse`'s real dispatch, `src-c/kernel/
 * parse.c`'s already-ported `parse_entry` (confirmed via `src/kernel/
 * MODFUNC.DEF`'s own `f_parsefilename equ (user*0100h)+152`, matching
 * `COMMAND.SUP`'s own `pfn:` trampoline, `src-c/kernel/command.c`'s
 * `cli_pfn`) -- genuinely callable for real, same as `src-c/loader/
 * ldmpm.c`'s own real call into `init_entry`. Likewise `not_builtin`'s
 * own `mpm_clicmd` (150) is `command.c`'s already-ported
 * `clicmd_entry` (`src/kernel/MODFUNC.DEF`: no direct `f_clicmd` equ,
 * but `COMMAND.SUP`'s own header comment confirms `DX -> command
 * buffer in u_wrkseg` matches `clicmd_entry`'s own real parameter).
 * `goodparse`/`try_builtin`/`not_builtin`/`clierror` are ported for
 * real below. `a_to_b` is fully self-contained (pure register
 * arithmetic, no `mpm` call at all).
 *
 * `parsefilename` builds a local `PCB.DEF`-shaped 4-byte block
 * (`SD_TMP_PCB`) from its own `si`(fcb)/`di`(string) parameters --
 * same `pcb_flnmptr`/`pcb_fcbptr` layout `command.c`'s own `cli_pfn`
 * already uses, just with a separate scratch address (this file's
 * own, not `command.c`'s `CLI_PCB`). `not_builtin`'s own `SD_TMP_CLICB`
 * is deliberately placed at `SD_TMP_CLICB_CMD - 1`: `CLICB.DEF`'s own
 * `cmdcb_net`/`cmdcb_cmd` fields are adjacent (offsets 0/1), so this
 * reuses `SD_TMP_CLICB_CMD`'s own already-populated command text
 * directly as `clicmd_entry`'s own `cmdcb_cmd` field, rather than
 * copying it -- same "adjacent fields, no copy needed" trick the real
 * asm's own `clicb_net`/`read_blen` aliasing plays, just for a
 * different reason (there, saving a DSEG byte; here, saving a memcpy).
 * `UDA_WRKSEG` is set to 0 before the call (ambient-flat, ultimately
 * reusing the SAME convention `command.c` and most of this project
 * already assumes -- `clicmd_entry`'s own internal `mem_r8_seg(mem,
 * wrkseg, ...)` calls then reduce to plain flat addressing).
 *
 * `cmdsent`/`read_blen`/`clicb_cmd`/`savebuf` are TMP.A86's own DSEG
 * fields, moved to fixed harness-chosen scratch addresses (SD_TMP_*,
 * see defs.h) since TMP.A86's own real DSEG offsets collide with its
 * own cseg -- see defs.h's own comment on them. `fcb`/`parseret`/`pcb`
 * are new scratch fields of the same kind, added for this pass.
 */
#include <stdint.h>
#include <string.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} parse_bxcx;
typedef struct {
    uint16_t bx, cx;
} cmd_bxcx;
extern parse_bxcx parse_entry(uint8_t *mem, uint16_t dx); /* src-c/kernel/parse.c */
extern cmd_bxcx clicmd_entry(uint8_t *mem, uint16_t dx);  /* src-c/kernel/command.c */

#define TMP_MAXCMDLEN 128

/* a_to_b -- parse a 1- or 2-digit decimal number from dl(1st char)/
 * dh(2nd char, or ' ' if only one digit was typed).
 *	output:	returns bl (0-99, or 0xFF on a non-digit) -- bh is never
 *		consumed by any real caller, so only bl is modeled, same
 *		"undefined artifact, not modeled" precedent used
 *		throughout this port
 */
static uint8_t a_to_b(uint8_t dl, uint8_t dh) {
    uint8_t tens, units;
    if (dh == ' ') {
        tens = '0';
        units = dl;
    } else {
        tens = dl;
        units = dh;
    }
    if (units < '0' || units > '9' || tens < '0' || tens > '9') {
        return 0xFF;
    }
    units = (uint8_t)(units - '0');
    tens = (uint8_t)(tens - '0');
    return (uint8_t)(tens * 10 + units);
}

/*
 * parsefilename -- parse `string` (at `di`) into the FCB at `si`, via
 * a local PCB.DEF block (SD_TMP_PCB) -- see this file's own header
 * comment for why this is a real call into `parse_entry` now.
 */
static parse_bxcx parsefilename(uint8_t *mem, uint16_t si_fcb, uint16_t di_string) {
    mem_w16(mem, (uint16_t)(SD_TMP_PCB + pcb_flnmptr), di_string);
    mem_w16(mem, (uint16_t)(SD_TMP_PCB + pcb_fcbptr), si_fcb);
    return parse_entry(mem, SD_TMP_PCB);
}

/*
 * clierror -- CLI call unsuccessful: analyze and display an error
 * message. Every branch but `e_nullcmd` just picks a message to
 * print (a no-op boundary stub in this flat memory model, see this
 * file's own header comment) and falls into `nextcommand` -- nothing
 * further to model for those; `e_nullcmd` alone has a real memory
 * effect (clearing `cmdsent`, so the next pass's own prompt doesn't
 * print an extra blank line for a command that was never really
 * sent).
 *	input:	cx = error code
 */
static void clierror(uint8_t *mem, uint16_t cx) {
    if (cx == e_nullcmd) {
        mem[SD_TMP_CMDSENT] = 0;
    }
    /* else: memory_ok/pd_ok/fname_ok/load_ok/open_ok/que_ok all just
     * pick a message (no-op print) -- nothing to port past that */
}

/*
 * not_builtin -- make the real CLI call: build a CLICB.DEF-shaped
 * block (SD_TMP_CLICB, reusing SD_TMP_CLICB_CMD's own already-
 * populated command text directly as its own cmdcb_cmd field -- see
 * this file's own header comment) and call `clicmd_entry`
 * (`command.c`) for real.
 */
static void not_builtin(uint8_t *mem) {
    mem[SD_TMP_CLICB] = 0; /* clicb_net = 0 */
    mem[SD_TMP_CMDSENT] = 0xFF; /* cmdsent = true (0xFF, not 1 -- see TMP.A86's own `true equ 0ffh`) */
    mem_w16(mem, UDA_WRKSEG, 0); /* ambient-flat, see header comment */
    cmd_bxcx r = clicmd_entry(mem, SD_TMP_CLICB);
    if (r.bx != 0) {
        clierror(mem, r.cx);
    }
    /* else: jmp nextcommand -- nothing further to model */
}

/*
 * try_builtin -- check the parsed FCB's own name field against the
 * two built-in commands (USER, PRINTER); either one re-parses its
 * own argument (if any) via a second `parsefilename` call and would
 * dispatch to `setuser`/`setlist` (never identified -- see header
 * comment for why that's moot: their own only observable job is to
 * fall into the same `pruser`/`prprinter` print tail this port
 * already reaches directly either way) -- `not_builtin` otherwise.
 *	input:	parseret = clicall's own parsefilename result (bx)
 */
static void try_builtin(uint8_t *mem, uint16_t parseret) {
    static const uint8_t usercmd[8] = {'U', 'S', 'E', 'R', ' ', ' ', ' ', ' '};
    static const uint8_t printercmd[8] = {'P', 'R', 'I', 'N', 'T', 'E', 'R', ' '};

    if (memcmp(&mem[SD_TMP_FCB + 1], usercmd, 8) == 0) {
        if (parseret != 0) {
            parse_bxcx pr = parsefilename(mem, SD_TMP_FCB, (uint16_t)(parseret + 1));
            if (pr.cx == 0) {
                (void)a_to_b(mem[SD_TMP_FCB + 1], mem[SD_TMP_FCB + 2]);
                /* mov dl,bl ! call setuser -- never identified, moot,
                 * see header comment */
            }
        }
        /* pruser: print the user-code prompt (getuser/prnum/crlf,
         * all no-op boundary stubs) -- jmp nextcommand */
        return;
    }

    if (memcmp(&mem[SD_TMP_FCB + 1], printercmd, 8) == 0) {
        if (parseret != 0) {
            parse_bxcx pr = parsefilename(mem, SD_TMP_FCB, (uint16_t)(parseret + 1));
            if (pr.cx == 0) {
                (void)a_to_b(mem[SD_TMP_FCB + 1], mem[SD_TMP_FCB + 2]);
                /* mov dl,bl ! call setlist -- never identified, moot */
            }
        }
        /* prprinter: print tail, all no-op -- jmp nextcommand */
        return;
    }

    not_builtin(mem);
}

/*
 * goodparse -- clicall's own parsefilename succeeded: save the
 * delimiter position, '$'-terminate the tail (if there's no tail,
 * at the end of the already-'$'-terminated savebuf instead), then
 * either check for a built-in command (if the parsed FCB named no
 * explicit drive) or go straight to the real CLI dispatch.
 *	input:	bx = parsefilename's own result
 */
static void goodparse(uint8_t *mem, uint16_t bx) {
    mem_w16(mem, SD_TMP_PARSERET, bx);
    if (bx == 0) {
        bx = (uint16_t)(SD_TMP_SAVEBUF + mem[SD_TMP_READ_BLEN]);
    }
    mem[bx] = '$'; /* haveatail */

    if (mem[SD_TMP_FCB] == 0) {
        try_builtin(mem, mem_r16(mem, SD_TMP_PARSERET));
    } else {
        not_builtin(mem);
    }
}

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

    parse_bxcx pr = parsefilename(mem, SD_TMP_FCB, SD_TMP_SAVEBUF);
    if (pr.cx != 0) {
        /* bad parse */
        uint16_t bx = (uint16_t)(SD_TMP_SAVEBUF + read_blen);
        mem[bx] = '$';
        clierror(mem, pr.cx);
        return;
    }
    goodparse(mem, pr.bx);
}
