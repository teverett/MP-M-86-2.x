/*
 * util_mem.c -- C port of src/kernel/UTIL.MEM: getmd, freemd.
 *
 * Both fully self-contained -- no calls into any other module.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the MEM
 * module's DS). The MD unused list `mdul` and the MDs on it are
 * SYSDAT-relative.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} md_bxcx;

/*
 * getmd -- get MD from MDUL
 *	output:	bx = MD address, 0 if none found
 *		cx = error code (0 or e_no_umd)
 */
md_bxcx getmd(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t bx = seg_r16(sd, SYSDAT_MDUL);
    if (bx == 0) {
        md_bxcx r = {0, e_no_umd};
        return r;
    }
    uint16_t si = seg_r16(sd, (uint16_t)(bx + m_link));
    seg_w16(sd, SYSDAT_MDUL, si);
    seg_w16(sd, (uint16_t)(bx + m_link), 0);
    seg_w16(sd, (uint16_t)(bx + m_start), 0);
    seg_w16(sd, (uint16_t)(bx + m_length), 0);
    seg_w16(sd, (uint16_t)(bx + m_plist), 0);
    md_bxcx r = {bx, 0};
    return r;
}

/* freemd -- put MD on MDUL; input: bx = MD address */
void freemd(uint8_t *phys, uint16_t sysdat, uint16_t bx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t si = seg_r16(sd, SYSDAT_MDUL);
    seg_w16(sd, SYSDAT_MDUL, bx);
    seg_w16(sd, (uint16_t)(bx + m_link), si);
}
