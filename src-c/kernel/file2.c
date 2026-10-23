/*
 * file2.c -- C port of a self-contained subset of src/kernel/
 * FILE2.BDO: GET_NALBS, getblock (+ internal lefttst/righttst/rblok/
 * rblok0).
 *
 * FILE2.BDO is not self-contained (same situation as FILE1.BDO/
 * FILE3.BDO -- see their own header comments). This is a small,
 * scoped pass, not a full file2 port: almost everything else in this
 * file (SEARCH/SEARCHN/DELETE/RENAME/OPEN/MAKE and the disk-map merge
 * logic -- 80+ labels) routes through `srch`, which calls `rddir` --
 * one of `FILE1.BDO`'s own deferred routines, itself blocked
 * transitively by `FLUSH_FILE0`'s real segment-relative-addressing
 * gap (`FILE3.BDO`). `GET_DIR_EXT` and `RESTORE_RC` are themselves
 * clean, self-contained FCB arithmetic (same shape as `file1.c`'s
 * own `getfcb`/`setfcb`), but their only real caller, `CLOSE_FCB`
 * (part of `CLOSE1`, which `FILE1.BDO`'s own deferred `CHECK_FCB`
 * needs), calls `srch` itself -- so porting them alone wouldn't
 * un-defer anything. This cluster was picked out instead because it's
 * both clean and a natural extension of `file1.c`'s own allocation-
 * bitmap work (`getallocbit`/`setallocbit`): it calls `getallocbit`
 * directly, as an ordinary cross-file call (declared here with a
 * matching prototype, same convention as `src-c/kernel/que1.c`'s own
 * cross-file calls into `que2.c`).
 */
#include <stdint.h>

#include "defs.h"

extern uint8_t getallocbit(const uint8_t *mem, uint16_t cx, uint16_t *bx_out, uint8_t *dh_out);

/*
 * rotr -- rotate al right by dh positions and store the result at
 * [bx]. The same tiny shared helper getallocbit's own callers use in
 * the real asm (src/kernel/FILE1.BDO's own setallocbit inlines the
 * identical logic rather than calling a separate function) --
 * duplicated here rather than exposed from file1.c, since it's a
 * 1-line rotate with no real translation risk either way.
 */
static void rotr(uint8_t *mem, uint16_t bx, uint8_t al, uint8_t dh) {
    mem[bx] = (uint8_t)(((al >> dh) | (al << (8 - dh))) & 0xFF);
}

/*
 * GET_NALBS -- number of allocation-vector bytes needed
 */
uint16_t get_nalbs(const uint8_t *mem) {
    uint16_t bx = mem_r16(mem, BD_MAXALL);
    bx = (uint16_t)(bx >> 3); /* maxall/8 */
    return (uint16_t)(bx + 1);
}

/*
 * getblock -- given an allocation-vector bit position, find the bit
 * closest to it that's still free (0) by searching left then right,
 * claim it (set to 1)
 *	output:	returns the claimed bit position, or 0 if none found
 *		(passing 0 on the left, or maxall on the right)
 *
 * "RCR AL,1 ! JAE ..." directly tests getallocbit's own returned al
 * bit 0 (the bit shifted out is al's own low bit before the shift),
 * so this just tests `al & 1` instead of modeling the rotate. The
 * following "RCL AL,1" (undoing the RCR with the same carry content)
 * nets to a no-op on al itself -- a perfect inverse rotation -- so
 * claiming the bit is just `al + 1` on the *original* al, not the
 * rotated value; translated accordingly rather than literally.
 *
 * cx (left pointer) and dx (right pointer) are two independent,
 * persistent cursors in the real asm: righttst only borrows cx as a
 * scratch copy of dx to make the getallocbit call (`mov cx,dx`), then
 * restores cx's own left-pointer value from the stack before falling
 * back to lefttst. A first translation pass collapsed both cursors
 * into a single C variable, which silently clobbered the left
 * pointer on every righttst attempt; translated here with separate
 * `left`/`right` variables so each keeps its own persistent value.
 */
uint16_t getblock(uint8_t *mem, uint16_t cx) {
    uint16_t left = cx;
    uint16_t right = cx;
lefttst:
    if (left == 0) {
        goto righttst;
    }
    left = (uint16_t)(left - 1);
    {
        uint16_t bx;
        uint8_t dh;
        uint8_t al = getallocbit(mem, left, &bx, &dh);
        if ((al & 1) == 0) {
            rotr(mem, bx, (uint8_t)(al + 1), dh); /* rblok */
            return left;
        }
    }
righttst:
    if (right >= mem_r16(mem, BD_MAXALL)) {
        goto rblok0;
    }
    right = (uint16_t)(right + 1);
    {
        uint16_t bx;
        uint8_t dh;
        uint8_t al = getallocbit(mem, right, &bx, &dh);
        if ((al & 1) == 0) {
            rotr(mem, bx, (uint8_t)(al + 1), dh); /* rblok */
            return right;
        }
    }
    goto lefttst;
rblok0:
    if (left != 0) {
        goto lefttst;
    }
    return 0;
}
