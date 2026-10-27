/*
 * lbdosio.c -- C port of src-assem/loader/LBDOSIO.A86: conbrk, conout, tabout,
 * crlf, print, func9, staret, funcret, setlret1.
 *
 * func1/func3/func6/func7/func8/func10/func11 are deferred -- each is
 * just an EQU alias for the bare `return:` label (a no-op `ret`), same
 * reasoning as SYSENT.RTM's creat_proc_entry: nothing to port. func2 is
 * likewise just an alias for tabout itself, already ported under its
 * own name.
 *
 * constf/coninf/conouf/listf/wbootf are near CALLs to fixed offsets in
 * the loader's own BIOS jump table (src-assem/loader/LBDOSEQU.A86: `wbootf
 * equ BIOS+3*1`, etc.), not yet ported -- ordinary mid-routine-call
 * boundary stubs per docs/src-c.md's convention (an instant no-op
 * return, AL left unchanged by whatever CALLed it).
 *
 * That has a real consequence for conbrk: it only ever reaches the
 * `CALL constf` point with AL already 0 (the `kbchar != 0` case returns
 * directly beforehand), and a no-op `constf` leaves AL at 0, so `AND
 * AL,1 ! jz ret0` always takes the zero branch -- the coninf/ctls/ctlc
 * check and the JMP wbootf reboot path below it are unreachable here,
 * exactly as they are in the real reference assembly run the same way.
 * conout's own CALLs to conouf/listf have no observable effect at all
 * under the convention (nothing downstream ever reads anything they'd
 * have set), so they're omitted outright -- same as QUE2.RTM's
 * qrelease (src-c/kernel/que2.c).
 */
#include <stdint.h>

#include "defs.h"

/*
 * conbrk -- check for character ready (also checks for the stop-screen
 * and reboot control characters when a real character is available)
 *	output:	returns AL (0 = not ready, 1 = ready)
 */
uint8_t conbrk(uint8_t *mem) {
    uint8_t al = mem[LD_KBCHAR];
    if (al != 0) {
        /* conb1 -- an already-buffered character is ready; kbchar is
         * left untouched */
        return 1;
    }
    /* al == 0 here: CALL constf (boundary stub, no-op, AL stays 0) !
     * AND AL,1 (still 0) ! jz ret0 -- always taken under the
     * convention, see header comment. Everything below (CALL coninf,
     * the ctls/ctlc check, JMP wbootf, and conb0's `mov kbchar,al`) is
     * unreachable here. */
    return 0; /* ret0 */
}

/*
 * conout -- write one character to the console (CL), tracking column
 * position; also copies to the list device if listcp is set
 *	input:	cl = character
 */
void conout(uint8_t *mem, uint8_t cl) {
    if (mem[LD_COMPCOL] == 0) {
        (void)conbrk(mem); /* result discarded, same as the real asm's push/call/pop */
        /* CALL conouf ! (CALL listf if listcp) -- boundary stubs, no
         * observable effect, see header comment */
    }

    /* compout */
    uint8_t al = cl;
    if (al == rubout) {
        return; /* ret1 */
    }
    mem[LD_COLUMN] = (uint8_t)(mem[LD_COLUMN] + 1);
    if (al >= ' ') {
        return; /* ret1 -- jae, unsigned compare */
    }
    mem[LD_COLUMN] = (uint8_t)(mem[LD_COLUMN] - 1);
    al = mem[LD_COLUMN];
    if (al == 0) {
        return; /* ret1 */
    }
    al = cl;
    if (al == ctlh) {
        mem[LD_COLUMN] = (uint8_t)(mem[LD_COLUMN] - 1);
        return; /* ret1 */
    }
    /* notbacksp */
    if (al == lf) {
        mem[LD_COLUMN] = 0;
    }
    /* else ret1 -- no further change */
}

/*
 * tabout -- expand tabs to the console, otherwise a direct tail-call to
 * conout
 *	input:	cl = character
 */
void tabout(uint8_t *mem, uint8_t cl) {
    if (cl != tab) {
        conout(mem, cl);
        return;
    }
    do {
        conout(mem, ' ');
    } while ((mem[LD_COLUMN] & 0x07) != 0);
}

/*
 * crlf -- carriage return / line feed sequence
 */
void crlf(uint8_t *mem) {
    conout(mem, cr);
    conout(mem, lf); /* JMP conout -- tail call, same observable effect as a call+return */
}

/*
 * print -- print the message at dx until a '$' is encountered
 *	input:	cx = string address
 */
void print(uint8_t *mem, uint16_t cx) {
    uint16_t si = cx;
    for (;;) {
        uint8_t al = mem[si];
        si = (uint16_t)(si + 1);
        if (al == '$') {
            return; /* ret2 */
        }
        tabout(mem, al);
    }
}

/*
 * func9 -- write line until $ encountered (BDOS function 9)
 *	input:	dx = string address
 */
void func9(uint8_t *mem, uint16_t dx) {
    print(mem, dx);
}

/*
 * staret -- set the dispatch return value's low byte (lret)
 *	input:	al = value
 */
void staret(uint8_t *mem, uint8_t al) {
    mem_w16(mem, LD_ARET, (uint16_t)((mem_r16(mem, LD_ARET) & 0xFF00) | al));
}

/*
 * funcret -- bare return point, called directly by name from
 * src-assem/loader/LBDOSF3.A86 (func38/func39)
 */
void funcret(uint8_t *mem) {
    (void)mem;
}

/*
 * setlret1 -- set lret = 1
 */
void setlret1(uint8_t *mem) {
    staret(mem, 1); /* JMPS staret -- tail call, same observable effect */
}
