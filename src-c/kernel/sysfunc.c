/*
 * sysfunc.c -- C port of 4 self-contained entries in src/kernel/SYSFUNC.SUP.
 *
 * `callbios_entry`, `sysdat_entry`, `tod_entry`, `serial_entry` (same file)
 * are deferred: they call into unported modules (`mpmif`/`xiosif`) or copy
 * from module-local data this port doesn't have a home for yet -- see
 * docs/src-c.md.
 */
#include <stdint.h>

#include "defs.h"

/* not_implemented -- Function not implemented */
uint16_t not_implemented(uint16_t *cx_out) {
    *cx_out = e_not_implemented;
    return 0xFFFF;
}

/* bad_entry -- Illegal System Function */
uint16_t bad_entry(uint16_t *cx_out) {
    *cx_out = e_bad_entry;
    return 0xFFFF;
}

/* version_entry -- Get Version # */
uint16_t version_entry(const uint8_t *mem, uint16_t *cx_out) {
    *cx_out = 0;
    return mem_r16(mem, SD_VERNUM);
}

/* mpmversion_entry -- Get MP/M Version # */
uint16_t mpmversion_entry(const uint8_t *mem, uint16_t *cx_out) {
    *cx_out = 0;
    return mem_r16(mem, SD_MPMVERNUM);
}
