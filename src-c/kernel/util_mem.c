/*
 * util_mem.c -- C port of src/kernel/UTIL.MEM: getmd, freemd.
 *
 * Both fully self-contained -- no calls into any other module.
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
md_bxcx getmd(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, SD_MDUL);
    if (bx == 0) {
        md_bxcx r = {0, e_no_umd};
        return r;
    }
    uint16_t si = mem_r16(mem, (uint16_t)(bx + m_link));
    mem_w16(mem, SD_MDUL, si);
    mem_w16(mem, (uint16_t)(bx + m_link), 0);
    mem_w16(mem, (uint16_t)(bx + m_start), 0);
    mem_w16(mem, (uint16_t)(bx + m_length), 0);
    mem_w16(mem, (uint16_t)(bx + m_plist), 0);
    md_bxcx r = {bx, 0};
    return r;
}

/* freemd -- put MD on MDUL; input: bx = MD address */
void freemd(uint8_t *mem, uint16_t bx) {
    uint16_t si = mem_r16(mem, SD_MDUL);
    mem_w16(mem, SD_MDUL, bx);
    mem_w16(mem, (uint16_t)(bx + m_link), si);
}
