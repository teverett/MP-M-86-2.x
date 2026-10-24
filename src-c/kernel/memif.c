/*
 * memif.c -- C port of src/kernel/MEMIF.MEM's `init` (module
 * initialization). `entry` (the module's own inter-module MEM
 * dispatch, `call cs:functab[si]`) is not ported here -- same
 * reasoning as `CIOIF.CIO`'s own `entry` (see cioif.c's own header
 * comment): every real caller that needs one of `entry`'s own
 * functab targets already calls the already-ported routine directly
 * (`cpmmem.c`'s own `getmemory`/`load.c`'s own calls into
 * `malloc_entry`/`mfree_entry`, etc.), so nothing needs to call
 * *through* `entry` to reach them.
 *
 * `init` itself is byte-for-byte identical to `CIOIF.CIO`'s own (same
 * 7-instruction body, no `call`/`callf`/`jmp` at all, ending in a far
 * `RET`) -- see that file's own header comment for the full
 * rationale, which applies here unchanged. `sysdat`=6/`supervisor`=
 * 8/0xA are the same real, already-resolved cseg offsets for the same
 * reason (this module's own `jmp init ! jmp entry` header is the
 * identical 6 bytes); confirmed directly against `tests/src-c/
 * fixtures/memif_wrapper.A86` via `--symbols`.
 */
#include <stdint.h>

#include "defs.h"

void init(uint8_t *mem, uint16_t ds_sysdat) {
    mem_w16(mem, MEMIF_SYSDAT, ds_sysdat);
    mem_w16(mem, MEMIF_SUPERVISOR_OFF, mem_r16(mem, SD_SUPMOD_OFF));
    mem_w16(mem, MEMIF_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));
}
