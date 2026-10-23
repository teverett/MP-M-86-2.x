/*
 * file4.c -- C port of a clean, self-contained subset of src/kernel/
 * FILE4.BDO (the BDOS function-dispatch table): func24, func25,
 * func29, func32, FUNC44, FUNC45, func51, func52.
 *
 * All 8 are MPM-branch BDOS entries that only touch registers and a
 * few scratch fields -- no calls into anything deferred. Everything
 * else in FILE4.BDO (func13-func23, func30, func33-func43, FUNC46,
 * FUNC48) routes through `srch`/`rddir`, `CURSELECT`/`initialize`,
 * `CHECK_FCB`, or `FLUSH_FILES` -- all transitively blocked by
 * `FLUSH_FILE0`'s real segment-relative-addressing gap (see
 * src-c/kernel/file1.c's/file2.c's own header comments). `GOBACK` is
 * just a 2-instruction tail jump into BDOSIF.BDO's own module-
 * dispatch epilogue (`BDOS_RETURN`), the same jump-table-dispatcher
 * class already deferred elsewhere -- not worth porting alone.
 *
 * `rlr` and the UDA fields these routines touch (`u_dma_ofst`,
 * `u_dma_seg`, `u_error_mode`, `u_mult_cnt`, `u_retseg`) are real
 * separate-segment fields whose own small real addresses collide with
 * cseg's own early code once collapsed into tools/i86sim.py's flat
 * memory model -- same class of gap as SYSDAT.FMT's own rlr=0x68 (see
 * file1.c's own header comment). This port uses the already-
 * established SD_RLR scratch constant for rlr (same one sysent.c/
 * sysfunc.c use for the same concept) and new UDA_DMA_OFST/UDA_DMA_SEG/
 * UDA_ERROR_MODE/UDA_MULT_CNT scratch constants for the rest; `dlog`
 * and `returnseg` are real DATA.BDO fields that don't collide (same
 * BD_* convention as file1.c/file2.c/file3.c).
 */
#include <stdint.h>

#include "defs.h"

/* func24 -- return the login vector
 *	output:	returns bx = dlog, *cx_out = 0
 */
uint16_t func24_entry(const uint8_t *mem, uint16_t *cx_out) {
    *cx_out = 0;
    return mem_r16(mem, BD_DLOG);
}

/* func25 -- return selected disk number
 *	output:	returns bx = the running PD's own current default disk
 *		(zero-extended), *cx_out = 0
 */
uint16_t func25_entry(const uint8_t *mem, uint16_t *cx_out) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    *cx_out = 0;
    return mem[(uint16_t)(bx + p_dsk)];
}

/* func29 -- return r/o bit vector
 *	output:	returns bx = rodsk, *cx_out = 0
 */
uint16_t func29_entry(const uint8_t *mem, uint16_t *cx_out) {
    *cx_out = 0;
    return mem_r16(mem, BD_RODSK);
}

/* func32 -- set user code
 *	input:	dl = new user code, or 0xFF to interrogate the current one
 *	output:	returns bx = the running PD's own current user code
 *		(zero-extended) when interrogating, else 0; *cx_out = 0
 */
uint16_t func32_entry(uint8_t *mem, uint8_t dl, uint16_t *cx_out) {
    uint16_t si = mem_r16(mem, SD_RLR);
    *cx_out = 0;
    if (dl == 0xFF) {
        return mem[(uint16_t)(si + p_user)];
    }
    mem[(uint16_t)(si + p_user)] = (uint8_t)(dl & 0x0F);
    return 0;
}

/* func44 -- set multi-sector count
 *	input:	dl = new count (valid range 1-16)
 *	output:	*bx_out = 0xFF, *cx_out = 0 if dl is out of range
 *		(u_mult_cnt untouched); else *bx_out = 0, *cx_out = 0
 */
void func44_entry(uint8_t *mem, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out) {
    *cx_out = 0;
    if (dl == 0 || dl >= 17) {
        *bx_out = 0xFF;
        return;
    }
    mem[UDA_MULT_CNT] = dl;
    *bx_out = 0;
}

/* func45 -- set bdos error mode
 *	input:	dl = new error mode
 *	output:	*bx_out = 0, *cx_out = 0
 */
void func45_entry(uint8_t *mem, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out) {
    mem[UDA_ERROR_MODE] = dl;
    *bx_out = 0;
    *cx_out = 0;
}

/* func51 -- set dma base
 *	input:	dx = new dma segment
 *	output:	*bx_out = 0, *cx_out = 0
 */
void func51_entry(uint8_t *mem, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out) {
    mem_w16(mem, UDA_DMA_SEG, dx);
    *bx_out = 0;
    *cx_out = 0;
}

/* func52 -- get dma
 *	input:	ds = this process's current data segment
 *	output:	returnseg = ds, u_retseg = u_dma_seg, returns bx =
 *		u_dma_ofst, *cx_out = 0
 */
uint16_t func52_entry(uint8_t *mem, uint16_t ds, uint16_t *cx_out) {
    mem_w16(mem, BD_RETURNSEG, ds);
    mem_w16(mem, UDA_RETSEG, mem_r16(mem, UDA_DMA_SEG));
    *cx_out = 0;
    return mem_r16(mem, UDA_DMA_OFST);
}
