/*
 * cioif.c -- C port of src/kernel/CIOIF.CIO's `init` (module
 * initialization). `entry` (the module's own inter-module CIO
 * dispatch, `call cs:functab[si] ! retf`) is not ported here -- same
 * reasoning as `INIT.SUP`'s own `entry` (see init.c's own header
 * comment): every real caller that needs one of `entry`'s own
 * functab targets already traces straight to the already-ported
 * routine itself (the same "boundary call resolves to an
 * already-ported routine" pattern as `fileopen`/`cli_gopen`), so
 * nothing needs to call *through* `entry` to reach them.
 *
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
 * the Supervisor module, if `entry` were ever called from here (it
 * isn't).
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
#include <stdint.h>

#include "defs.h"

void init(uint8_t *mem, uint16_t ds_sysdat) {
    mem_w16(mem, CIOIF_SYSDAT, ds_sysdat);
    mem_w16(mem, CIOIF_SUPERVISOR_OFF, mem_r16(mem, SD_SUPMOD_OFF));
    mem_w16(mem, CIOIF_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));
}
