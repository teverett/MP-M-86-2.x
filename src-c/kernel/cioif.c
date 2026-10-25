/*
 * cioif.c -- C port of src/kernel/CIOIF.CIO's `init` AND now `entry`,
 * named here `cioif_init`/`cioif_entry` -- every module's own real
 * asm label for these two routines is just `init`/`entry`; see
 * bdosif.c's own header comment for why the C ports need a module
 * prefix instead.
 *
 * (the module's own inter-module CIO dispatch, `mov ch,0!shl cx,1!
 * mov si,cx!call cs:functab[si]!retf`) -- CL selects one of the 23
 * functions below, each already ported in console.c/chario.c; this
 * is purely the near-indirect-call wiring between them, confirmed
 * register-by-register against each target's own real "input:"/
 * "output:" comment in CONSOLE.CIO/CHARIO.CIO, not assumed from a
 * uniform convention (there isn't one -- some return via BL, some
 * via BX:CX, some have no defined return at all).
 *
 * `entry_result` is a generic {ax,bx,cx} grab-bag, not a claim that
 * the real asm ever defines all three for every function -- each
 * `case` below only fills in the field(s) that function's own real
 * body actually sets (see each one's own comment), leaving the rest
 * 0 (as the real asm would leave them *whatever they already were*,
 * which this port has no way to reproduce and no real caller has
 * ever needed -- same "undefined, not modeled" class as
 * `CPMMEM.MEM`'s own `cpmfree_entry` BX).
 */
#include <stdint.h>

#include "defs.h"

extern uint8_t conin_entry(uint8_t *mem);
extern void conout_entry(uint8_t *mem, uint8_t dl);
extern uint8_t rconin_entry(uint8_t *mem);
extern void rconout_entry(uint8_t *mem, uint8_t dl);
extern void listout_entry(uint8_t *mem, uint8_t dl);
extern uint8_t dirio_entry(uint8_t *mem, uint8_t dl);
extern void conwrite_entry(uint8_t *mem, uint16_t dx);
extern void conread_entry(uint8_t *mem, uint16_t dx);
extern uint8_t constat_entry(uint8_t *mem);
extern void conprint_entry(uint8_t *mem, uint16_t dx, uint8_t bh, uint8_t bl);
extern cio_bxcx conattach_entry(uint8_t *mem);
extern cio_bxcx condetach_entry(uint8_t *mem);
extern cio_bxcx setdefcon_entry(uint8_t *mem, uint8_t dl);
extern cio_bxcx conassign_entry(uint8_t *mem, uint16_t dx);
extern cio_bxcx getdefcon_entry(const uint8_t *mem);
extern cio_bxcx lstattach_entry(uint8_t *mem);
extern cio_bxcx lstdetach_entry(uint8_t *mem);
extern cio_bxcx setdeflst_entry(uint8_t *mem, uint8_t dl);
extern cio_bxcx clstattch_entry(uint8_t *mem);
extern cio_bxcx cconattch_entry(uint8_t *mem);
extern cio_bxcx getdeflst_entry(const uint8_t *mem);
extern cio_bxcx mimic_entry(uint8_t *mem, uint8_t dh_in, uint8_t dl_in);
extern cio_bxcx unmimic_entry(uint8_t *mem, uint8_t dl_in);

typedef struct {
    uint16_t ax, bx, cx;
} entry_result;

/* entry -- CIOIF.CIO's own inter-module CIO dispatch.
 *	input:	cl = function # (0-22, functab's own order);
 *		dx/bh/bl = whichever of this specific function's own
 *		real inputs it needs (most use dl or dx; conprint_entry
 *		alone also needs bh/bl)
 */
entry_result cioif_entry(uint8_t *mem, uint8_t cl, uint16_t dx, uint8_t bh, uint8_t bl) {
    entry_result r = {0, 0, 0};
    uint8_t dl = (uint8_t)dx;
    switch (cl) {
    case 0: r.bx = conin_entry(mem); break;                 /* "mov bl,dl!ret" */
    case 1: conout_entry(mem, dl); break;                   /* no defined return */
    case 2: r.bx = rconin_entry(mem); break;                /* "mov bl,dl!ret" */
    case 3: rconout_entry(mem, dl); break;                  /* no defined return */
    case 4: listout_entry(mem, dl); break;                  /* no defined return */
    case 5: r.bx = dirio_entry(mem, dl); break;             /* returns via AL/BL */
    case 6: conwrite_entry(mem, dx); break;                 /* no defined return */
    case 7: conread_entry(mem, dx); break;                  /* no defined return */
    case 8: r.bx = constat_entry(mem); break;               /* "output: BL" */
    case 9: { cio_bxcx v = conattach_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 10: { cio_bxcx v = condetach_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 11: { cio_bxcx v = setdefcon_entry(mem, dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 12: { cio_bxcx v = conassign_entry(mem, dx); r.bx = v.bx; r.cx = v.cx; break; }
    case 13: { cio_bxcx v = getdefcon_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 14: conprint_entry(mem, dx, bh, bl); break;        /* no defined return */
    case 15: { cio_bxcx v = lstattach_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 16: { cio_bxcx v = lstdetach_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 17: { cio_bxcx v = setdeflst_entry(mem, dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 18: { cio_bxcx v = clstattch_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 19: { cio_bxcx v = cconattch_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 20: { cio_bxcx v = getdeflst_entry(mem); r.bx = v.bx; r.cx = v.cx; break; }
    case 21: { cio_bxcx v = mimic_entry(mem, (uint8_t)(dx >> 8), dl); r.bx = v.bx; r.cx = v.cx; break; }
    case 22: { cio_bxcx v = unmimic_entry(mem, dl); r.bx = v.bx; r.cx = v.cx; break; }
    default: break;
    }
    return r;
}

/*
 * `init` itself has no `call`/`callf`/`jmp` at all -- a straight
 * 7-instruction body ending in a far `RET` (no C equivalent, same as
 * every other real OS entry/exit point in this port, but trivially
 * narrow here since nothing meaningful happens after it). It copies
 * its own caller's `DS` (the real sysdat segment, per `init`'s own
 * "assume DS = Sysdat Segment" precondition) into this module's own
 * local `sysdat` cseg cell, then copies both halves of the SUP
 * module's own real entry far-pointer (`SYSDAT.FMT`'s own
 * `module_table[SUP]`, `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG`) into this
 * module's own local `supervisor` cseg cell -- the same far-pointer
 * `entry`'s own `callf cs:dword ptr .supervisor` would use to reach
 * the Supervisor module (`entry` above doesn't need it for any of
 * its own 23 cases, since none of them call `mpif`/`xiosif`
 * themselves; it's wired up here regardless, since real callers may).
 *
 * `sysdat`/`supervisor` are real, already-resolved cseg offsets (this
 * module's own `jmp init ! jmp entry` header is 6 bytes, so
 * `sysdat`=6 and `supervisor`=8/0xA for its own offset/segment
 * halves), not harness-chosen scratch addresses -- same reasoning as
 * the BD_ and INIT_ blocks elsewhere in this project; confirmed
 * directly against `tests/src-c/fixtures/cioif_wrapper.A86` via
 * `--symbols`, not assumed from the shared header shape with
 * `MEMIF.MEM`/`INIT.SUP` alone.
 *
 * `ds_sysdat` is a genuinely external/ambient value nothing in this
 * file can resolve on its own, so it's a parameter, same convention
 * as `init.c`'s own `ds_sysdat`. Unlike `init.c`'s own `init` (which
 * also needs its caller's `cs`, to fill in a real interrupt-vector
 * far pointer elsewhere in its own body), this routine never reads
 * its own `cs` at all -- it only copies the Supervisor module's own
 * far pointer into `supervisor`, never its own -- so there's no `cs`
 * parameter here.
 */
void cioif_init(uint8_t *mem, uint16_t ds_sysdat) {
    mem_w16(mem, CIOIF_SYSDAT, ds_sysdat);
    mem_w16(mem, CIOIF_SUPERVISOR_OFF, mem_r16(mem, SD_SUPMOD_OFF));
    mem_w16(mem, CIOIF_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));
}
