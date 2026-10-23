/*
 * command.c -- C port of a self-contained subset of src/kernel/
 * COMMAND.SUP: shifttail, fileopen, user_retf.
 *
 * `chain_entry`/`clicmd_entry` (the file's own two real entry points,
 * and everything only they call -- cli_fload's own chain-mode UDA
 * copy in particular) are deferred: `cli_fload`'s chain path
 * dynamically switches `DS`/`ES` to the newly loaded process's own UDA
 * segment (computed at runtime from `cs:sysdat` + `inituda`'s offset)
 * and then reads/writes fixed `UDA.FMT` offsets relative to it -- the
 * same real segment-relative-addressing gap already documented for
 * `PROC.RTM`/`ML.MEM`/`MAU.MEM`/`SHARE.MEM` (see docs/src-c.md).
 *
 * Everything else in the file -- `crlf`/`prchar`/`prtime`/`prnum`/
 * `prdisk`/`prcusp`/`prcsm`/`prtyp`/`prnam`/`prn1`/`cprnt`/`pruser`/
 * `prfilnam` (the console-print helpers) and `mxwrite`/`mxread`/
 * `mxw1`/`jmpm`/`pfn`/`conasn`/`fileclose` -- collapses entirely to
 * setting up a few registers before a tail call into the unported
 * `mpmif` trampoline, which is a complete no-op under docs/src-c.md's
 * boundary-stub convention: nothing downstream ever reads anything
 * they'd have set, and none of them touch memory at all. Same
 * reasoning as `freeall_entry` (src-c/kernel/cpmmem.c) and `func1`/
 * `func3`/etc. (src-c/loader/lbdosio.c): nothing to port.
 *
 * Both this file's own print helpers and its two deferred entry
 * points were, until now, only reachable from inside this file
 * itself -- porting `shifttail`/`fileopen`/`user_retf` on their own
 * has no other caller yet either, but is still real, independently
 * testable translated logic, kept ready for when `clicmd_entry`
 * itself becomes portable.
 */
#include <stdint.h>

#include "defs.h"

/*
 * shifttail -- shift the remaining command tail down to the start of
 * the command buffer (or clear it if there's no tail)
 *	input:	ax = output of a previous parsefilename call (0 = no tail)
 */
void shifttail(uint8_t *mem, uint16_t ax) {
    if (ax == 0) {
        mem[CLI_CMDTAIL] = 0;
        return;
    }
    uint16_t si = ax;
    uint16_t off = (uint16_t)(ax - CLI_CMDTAIL);
    uint16_t cx = (uint16_t)((uint16_t)(128 - off) >> 1); /* word count */
    uint16_t di = CLI_CMDTAIL;
    for (uint16_t i = 0; i < cx; i++) {
        mem_w16(mem, (uint16_t)(di + i * 2), mem_r16(mem, (uint16_t)(si + i * 2)));
    }
}

/*
 * fileopen -- open cli_fcb (forcing read-only), via the unported
 * mpmif trampoline (f_fopen) -- a boundary stub, no-op beyond the
 * real bit set below
 */
void fileopen(uint8_t *mem) {
    mem[(uint16_t)(CLI_FCB + fcb_name + 5)] |= 0x80; /* f6`= open read-only */
    /* mov cx,f_fopen!mov dx,si!push es!call mpmif!pop es!ret -- boundary
     * stub, no-op beyond the bit set above */
}

/*
 * user_retf -- cleanup when an RSP falls off the end of its own code
 * via a bare `retf` (the normal way an RSP like TMP finishes running)
 *
 * `and p_flag[bx],not (pf_keep+pf_tempkeep+pf_ctlc+pf_sys)` reads as
 * though it might hit the missing-parens `not` precedence bug
 * docs/bugs.md #80 found and fixed in this exact routine (`not` binds
 * to the single following term only, same precedence as unary `-`) --
 * but the parens shown here *are* that fix already applied (confirmed
 * directly against tools/asm86: `not (A+B+C+D)` assembles to the
 * correct mask `0xFD7C`, not the pre-fix `0x027E`). No bug to
 * reproduce; this is a plain, correct mask.
 */
void user_retf(uint8_t *mem) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
    uint16_t mask = (uint16_t)~(uint16_t)(pf_keep | pf_tempkeep | pf_ctlc | pf_sys);
    mem_w16(mem, (uint16_t)(rlr + p_flag), (uint16_t)(pflag & mask));
    /* mov cx,f_terminate!sub dx,dx!int mpmint -- boundary tail via a
     * software interrupt instead of mpmif's own usual near CALL;
     * nothing to port past this point, see header comment */
}
