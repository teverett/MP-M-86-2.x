/*
 * bdosif.c -- C port of src/kernel/BDOSIF.BDO's `init` (module
 * initialization). `entry` (the module's own inter-module BDOS
 * dispatch) is not ported here -- same reasoning as every other
 * module dispatcher in this tree (see cioif.c's own header comment):
 * no real caller needs to go through it.
 *
 * `init` does two things:
 *
 *   1. The same `sysdat`/`supervisor` copy as `CIOIF.CIO`/`MEMIF.MEM`/
 *      `RTMIF.RTM`'s own `init` (see cioif.c's own header comment).
 *      This module's own `init` has no far `CALL` and nothing that
 *      forces genuine segmented addressing the way `RTMIF.RTM`'s own
 *      `init` did (see that file's own header comment), so it's
 *      tested non-segmented, same as `CIOIF.CIO`/`MEMIF.MEM` --
 *      `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG` flat scratch addresses, not
 *      real `SYSDAT.FMT` raw offsets.
 *
 *   2. Creates, opens, and writes the MX disk queue via three real
 *      `mpmif` calls. Traced via `MODFUNC.DEF`'s `f_qmake`(134)/
 *      `f_qopen`(135)/`f_qwrite`(139) through `SYSTEM.DAT`'s own
 *      `sysent` table to `RTMIF.RTM`'s own `function` table entries
 *      4/5/9 -- all already ported in `que1.c` as `makeq_entry`/
 *      `openq_entry`/`writeq_entry`. `mxdiskqd`/`mxdiskqpb`
 *      (`src/kernel/DATA.BDO`) are addressed via `dx`, same flat-
 *      scratch convention as every other `SD_*`-relative field in
 *      this tree (`SD_MXDISKQD`/`SD_MXDISKQPB`, `defs.h`) -- not
 *      their real raw offset within the combined `SYSDAT.FMT`+
 *      `DATA.BDO` dseg (probed directly: 0xf92/0xfae), since
 *      `que1.c`'s own already-verified routines (`SD_QLR`/`SD_RLR`/
 *      etc.) are themselves only correct under that same flat
 *      convention (confirmed via `tests/src-c/test_que.py`'s own
 *      `UDA_WRKSEG == sysdat` setup, which makes `getqdaddr`'s own
 *      "already in MPM range" path a no-op passthrough regardless of
 *      `dx`'s actual value).
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} bdos_bxcx;

extern bdos_bxcx makeq_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat);
extern bdos_bxcx openq_entry(uint8_t *mem, uint16_t dx);
extern bdos_bxcx writeq_entry(uint8_t *mem, uint16_t dx);

void init(uint8_t *mem, uint16_t ds_sysdat) {
    mem_w16(mem, BDOSIF_SYSDAT, ds_sysdat);
    mem_w16(mem, BDOSIF_SUPERVISOR_OFF, mem_r16(mem, SD_SUPMOD_OFF));
    mem_w16(mem, BDOSIF_SUPERVISOR_SEG, mem_r16(mem, SD_SUPMOD_SEG));

    /* create disk mx queue */
    (void)makeq_entry(mem, SD_MXDISKQD, ds_sysdat);
    /* open mx disk queue */
    (void)openq_entry(mem, SD_MXDISKQPB);
    /* write mx disk queue */
    (void)writeq_entry(mem, SD_MXDISKQPB);
}
