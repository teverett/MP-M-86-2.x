/*
 * lbdosent.c -- C port of src/loader/LBDOSENT.A86: `oldbdos`'s own
 * function-number dispatch (ported as `oldbdos`), and the error-
 * reporting tail it can reach (`persub`/`selsub`/`rodsub`/`rofsub`,
 * + internal `errflg`). `parsave`/`parsave33`/`parunsave` are this
 * module's own routines too, but already ported in src-c/loader/
 * lbdosf3.c (borrowed there, since `func15`/`func20`/`goback` need
 * them first -- see that file's own header comment).
 *
 * `bdose` (the real `INT bdos_int` entry point: switch SS:SP, save/
 * restore the 5 registers an 8086 ISR has to preserve, `iret`) has
 * no C equivalent, same as every other real OS/interrupt entry
 * point in this port -- `oldbdos` here is `bdose`'s own real body,
 * the part genuinely worth porting (same split LDMPM.A86's own
 * `start_entry` makes between the real stack switch and everything
 * after it).
 *
 * `oldbdos`'s own `branch:`/`functab` dispatch is ported as a plain
 * C switch, same convention `src-c/kernel/dsptch.c`'s own `call
 * cs:dsp_table[bx]` already uses. Of its 49 function numbers, the
 * overwhelming majority (`func1`/`func3`/`func6`-`func8`/`func10`-
 * `func12`/`func16`-`func19`/`func21`-`func25`/`func27`-`func37`/
 * `func40`/`func50`/`func52`-`func57`) are each just an `EQU` alias
 * for the bare `return:` label (confirmed in `src/loader/
 * LBDOSF3.A86`'s own docs row) -- translated as the `default` case,
 * a real call to `funcret` (src-c/loader/lbdosio.c), not skipped
 * outright, since `funcret` is itself a genuine already-ported
 * routine (a no-op, but a real one, not a boundary-stub artifact).
 * The rest resolve to already-ported real functions this file wires
 * up directly: `func2`=`tabout`, `func9`, `func13`, `func14`=
 * `curselect`, `func15`, `func20`, `func26` (all `src-c/loader/
 * lbdosf3.c`/`lbdosio.c`), `func51`... no -- `func51` (`LBDOSNEW.A86`)
 * is NOT reachable through this dispatch at all (`sfunc86`'s own
 * adjustment only covers `cl` 50-57, and `functab`'s own second
 * block is `func50`,`func51`,... -- wait, it genuinely IS reachable,
 * at `cl`=51; omitted here only because nothing in `LDMPM.A86`'s own
 * call sites ever uses it -- added anyway, for completeness) =
 * `func51` (`src-c/loader/lbdosnew.c`), and `punchf`=`PUNCH`
 * (`src-c/loader/ldbios86q.c`'s own `punch_entry`, confirmed via
 * `LBDOSEQU.A86`'s own `punchf EQU BIOS+3*6` resolving to
 * `LDBIOS86Q.A86`'s own BIOS jump-vector entry 6, `PUNCH`).
 *
 * `wbootf`=`WBOOT` and `listf`=`LISTOUT` (via the same `BIOS+3*N`
 * jump-vector equates) are BOTH real boundary exits under this
 * port's own existing choices for those two (`WBOOT`: nothing but a
 * hardcoded jump back into `LDMPM.A86`, nothing to port; `LISTOUT`:
 * bare `ret`, no observable effect -- see `src-c/loader/ldbios86q.c`'s
 * own header comment for both). `wbootf`'s own dispatch case in
 * particular never reaches `goback` in the real asm at all (`WBOOT`
 * jumps away, it doesn't return) -- this port's own switch still
 * falls through to `goback` for that case regardless, since nothing
 * in this whole port's own call sites ever dispatches function 0
 * (a controlled-reboot-only function number, not part of ordinary
 * LDMPM.A86 traffic), so the discrepancy is never actually
 * observable; same "unreachable under this port's own scope, not
 * worth a special case" precedent used throughout this tree.
 *
 * `errflg`'s own final `jmp coninf` (a BIOS `CONIN` call, waiting
 * for one keypress before continuing/rebooting) stays a boundary
 * stub, same as `src-c/loader/lbdosio.c`'s own already-established
 * treatment of `coninf`/`conouf`/`listf` elsewhere in that exact
 * file (not upgraded to a real call into `src-c/loader/ldbios86q.c`'s
 * now-ported `conin_entry` here, to stay consistent with that
 * existing choice rather than silently changing scope) -- its own
 * result (AL, the keypress) is an undefined artifact, fixed at 0
 * (not `ctlc`), same "undefined artifact, fixed choice" precedent
 * as `selectdisk`'s own fixed `bx=0`. That fixes `persub`'s own
 * `cmp al,ctlc`/reboot branch at "never taken" -- translated
 * faithfully (the check is still there, just never true under this
 * port's own fixed artifact), not skipped.
 *
 * `selsub`/`rodsub`/`rofsub` all end in an unconditional `JMP
 * wbootf` after `errflg` -- a real boundary exit (reboot), nothing
 * to port past the message. `src/loader/LBDOSF1.A86`'s own
 * `selerror`/`goerr` call sites (`diocomp`/`ckrofile`/`checkwrite`)
 * are NOT updated to call these for real: they're already
 * unreachable under `src-c/loader/lbdosf1.c`'s own pre-existing,
 * independently-documented choice to fix `readf`/`writef`/`nowrite`'s
 * own boundary-stub results at "always succeeds" (see that file's
 * own header comment) -- a separate fact, true whether or not
 * `persub`/etc. exist, so wiring them up there wouldn't change
 * anything observable. These four functions are still genuinely
 * ported and independently testable here, on their own.
 */
#include <stdint.h>

#include "defs.h"

extern void tabout(uint8_t *mem, uint8_t cl);  /* src-c/loader/lbdosio.c */
extern void crlf(uint8_t *mem);                /* src-c/loader/lbdosio.c */
extern void func9(uint8_t *mem, uint16_t dx);  /* src-c/loader/lbdosio.c */
extern void funcret(uint8_t *mem);             /* src-c/loader/lbdosio.c */
extern void func13(uint8_t *mem);              /* src-c/loader/lbdosf3.c */
extern void curselect(uint8_t *mem);           /* src-c/loader/lbdosf3.c */
extern void func15(uint8_t *mem);              /* src-c/loader/lbdosf3.c */
extern void func20(uint8_t *mem);              /* src-c/loader/lbdosf3.c */
extern void func26(uint8_t *mem, uint16_t dx); /* src-c/loader/lbdosf3.c */
extern uint16_t goback(uint8_t *mem);          /* src-c/loader/lbdosf3.c */
extern void func51(uint8_t *mem, uint16_t dx); /* src-c/loader/lbdosnew.c */
extern uint8_t punch_entry(void);              /* src-c/loader/ldbios86q.c */

#define nfuncs80 41 /* number of 8080 functions */
#define sfunc86  50 /* start number for 8086 functions */
#define nfuncs   49 /* total number of BDOS functions */

static void print_cstr(uint8_t *mem, const char *s) {
    for (; *s != '\0'; s++) {
        tabout(mem, (uint8_t)*s);
    }
}

/* errflg -- report an error to the console: "Bdos Err On <disk>: "
 * followed by `suffix`, then wait for a keypress.
 *	output:	returns al (the keypress) -- undefined artifact, fixed
 *		at 0, see this file's own header comment
 */
static uint8_t errflg(uint8_t *mem, const char *suffix) {
    crlf(mem);
    char letter = (char)('A' + mem[LD_CURDSK]);
    print_cstr(mem, "Bdos Err On ");
    tabout(mem, (uint8_t)letter);
    print_cstr(mem, ": ");
    print_cstr(mem, suffix);
    return 0; /* jmp coninf -- boundary stub, see header comment */
}

/* persub -- report a permanent (disk I/O) error. The real asm's own
 * ctlc-triggered reboot (`JMP wbootf`) is unreachable under this
 * port's own fixed `errflg` artifact -- see header comment.
 */
void persub(uint8_t *mem) {
    uint8_t al = errflg(mem, "Bad Sector");
    if (al == ctlc) {
        /* JMP wbootf -- unreachable under this port's own convention */
        return;
    }
    /* parret -- falls through to a plain return */
}

/* selsub -- report a disk-select error, then reboot (boundary exit,
 * nothing to port past the message). */
void selsub(uint8_t *mem) {
    (void)errflg(mem, "Select");
}

/* rodsub -- report a write-to-read-only-disk error, then reboot. */
void rodsub(uint8_t *mem) {
    (void)errflg(mem, "R/O");
}

/* rofsub -- report a read-only-file error, then reboot. */
void rofsub(uint8_t *mem) {
    (void)errflg(mem, "File R/O");
}

/* oldbdos -- the loader's own synthetic BDOS dispatch (reached via
 * LDMPM.A86's own `int bdos_int`/`sys_vec`, NOT the real MP/M-86
 * kernel's `int mpmint` -- see src-c/loader/ldmpm.c's own header
 * comment).
 *	input:	cl = function number; dx = "information address"
 *		(the real asm's own `info` parameter)
 *	output:	returns ax (`goback`'s own result)
 */
uint16_t oldbdos(uint8_t *mem, uint8_t cl, uint16_t dx) {
    mem_w16(mem, LD_INFO, dx);
    mem[LD_LINFO] = (uint8_t)(dx & 0xFF); /* linfo = low(info) */
    mem_w16(mem, LD_ARET, 0);
    mem[LD_PARCOPFL] = 0; /* false */
    mem[LD_FCBDSK] = 0;
    mem[LD_RESEL] = 0;

    uint8_t fn = cl;
    int in_range = 1;
    if (fn >= nfuncs80) {
        fn = (uint8_t)(fn - (sfunc86 - nfuncs80));
        in_range = (fn < nfuncs);
    }
    if (in_range) {
        uint8_t ch = (uint8_t)(dx & 0xFF); /* MOV CL,DL -- possible output char */
        switch (fn) {
        case 0: /* wbootf = WBOOT -- real boundary exit, see header comment */ break;
        case 2: tabout(mem, ch); break;                 /* func2 */
        case 4: (void)punch_entry(); break;              /* punchf */
        case 5: /* listf = LISTOUT -- nothing to port */ break;
        case 9: func9(mem, dx); break;
        case 13: func13(mem); break;
        case 14: curselect(mem); break;                  /* func14 */
        case 15: func15(mem); break;
        case 20: func20(mem); break;
        case 26: func26(mem, dx); break;
        case 41: func51(mem, dx); break;                 /* cl==50 */
        default: funcret(mem); break;
        }
    }
    return goback(mem);
}
