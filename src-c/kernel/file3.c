/*
 * file3.c -- C port of a self-contained subset of src/kernel/
 * FILE3.BDO: PACK_SDCNT, COMPARE, SEARCH_OLIST/SEARCH_LLIST/
 * SEARCHN_LIST (+ internal search_listo), DELETE_ITEM, CREATE_ITEM,
 * SET_OLIST_ITEM.
 *
 * FILE3.BDO is not self-contained -- same situation as FILE1.BDO (see
 * src-c/kernel/file1.c's own header comment): it's one of 4 files
 * meant to be assembled together into one BDOS module. This is a
 * small, scoped follow-up pass, not a full file3 port, and a 2nd pass
 * over this same file: an earlier revision of this comment claimed
 * `TST_OLIST`/`CHK_OLIST`/`PR_TERM` were also self-contained -- on
 * closer reading they aren't: `CHK_OLIST` calls the still-unported
 * `openx06`/`compare_pds` and relies on its own caller having pushed
 * a word onto the stack for it to `pop` partway through (an implicit
 * calling convention, not a boundary stub this port can safely stand
 * in for); `PR_TERM` calls the still-unported `tst_tbl_lmt`/
 * `remove_locks`/`func48`. Left for a later pass along with the rest
 * of the file. `FLUSH_FILES`/`FLUSH_FILE0` hit a different, real
 * blocker: `FLUSH_FILE0` dynamically switches `ES` to a PD's own UDA
 * segment (`mov es,p_uda[bx]`) and then reads a fixed offset relative
 * to it (`u_pd_cnt`) -- the same real segment-relative-addressing gap
 * as `PROC.RTM`/`ML.MEM`. One other routine in this file (not
 * identified by name here) uses `std` (reverse-direction string ops),
 * the same gap as `MAU.MEM`.
 *
 * `DELETE_ITEM`/`CREATE_ITEM` touch `free_root` -- not a FILE1-4.BDO
 * scratch field like `BD_CURDSK` et al, but a real `SYSDAT.FMT` field
 * (confirmed via `--symbols`: it resolves to 0x52, nowhere near the
 * BD_* block's own 0xC00+ addresses) -- MP/M-86's BDOS module has no
 * private DSEG of its own at all; every one of its scratch fields,
 * `free_root` included, really is SYSDAT-resident on real hardware.
 * 0x52 is safely below every address this combined module's own
 * `tests/src-c/fixtures/file3_wrapper.A86` test entry points
 * (`delete_item`=0x1919, `create_item`=0x1933, `set_olist_item`=
 * 0x1953) ever execute forward from, so poking test input there is
 * safe (unlike the fallthrough case `test_tmp.py`'s own header
 * comment describes) -- confirmed via `--symbols`, not just assumed.
 * `BD_FREE_ROOT` in defs.h uses this real, verified address directly
 * (same reasoning as every other BD_* constant, just a different
 * source file).
 *
 * `SET_OLIST_ITEM` calls `move` (`src-c/kernel/file1.c`, now
 * non-`static` for this), relying on real 8086 `rep movs`' own
 * DI-after-the-copy side effect (`move`'s own real asm sets `di=bx`
 * before the copy, and `rep movs` advances both SI/DI by the copy
 * count) to compute the destination word it zeroes afterward -- not
 * modeled as a side effect of `move` itself (file1.c's own C version
 * doesn't need or track DI), just recomputed directly as `bx+3+5`.
 *
 * This cluster was picked out specifically because `src/kernel/
 * FILE1.BDO`'s own deferred `CHECK_FCB` calls `PACK_SDCNT`/
 * `SEARCH_OLIST` -- but `CHECK_FCB` also calls `CLOSE1` (`FILE2.BDO`),
 * which cascades into `CLOSE_FCB`/`GET_DIR_EXT`/`RESTORE_RC`/`srch`/
 * `getdptra`, a chain not scoped or ported here, so `CHECK_FCB` stays
 * deferred for now despite this pass.
 *
 * search_listo's own "compare ch bytes starting right after curdsk"
 * trick (see `search_listo`'s own comment) relies on `BD_CURDSK`,
 * `BD_PACKED_DCNT` (3 bytes) and `pdaddr` sitting immediately adjacent
 * in that exact order -- confirmed against the real assembled
 * addresses in src-c/kernel/defs.h's own header comment on them, not
 * just assumed.
 */
#include <stdint.h>

#include "defs.h"

/*
 * compare -- compare cl bytes starting at bx against cl bytes
 * starting at dx
 *	output:	returns 1 if equal (the real ZF), 0 if not
 */
uint8_t compare(const uint8_t *mem, uint16_t bx, uint16_t dx, uint8_t cl) {
    for (uint8_t i = 0; i < cl; i++) {
        if (mem[(uint16_t)(bx + i)] != mem[(uint16_t)(dx + i)]) {
            return 0;
        }
    }
    return 1;
}

/*
 * pack_sdcnt -- pack BD_SDBLK/BD_SDCNT into BD_PACKED_DCNT
 *	packed_dcnt format (24 bits): dblk (low 15 bits) || dcnt (low 9 bits)
 */
void pack_sdcnt(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, BD_SDBLK);
    if (bx == 0) {
        bx = mem_r16(mem, BD_SDCNT);
        uint8_t cl = (uint8_t)(mem[BD_BLKSHF] + 2);
        bx = (uint16_t)(bx >> cl);
    }
    bx = (uint16_t)(bx << 1); /* shl bx,1 -- shift dblk left 1 */
    uint16_t dx = mem_r16(mem, BD_SDCNT);
    uint16_t ax = (uint16_t)((uint16_t)mem[BD_BLKMSK] << 2); /* ah=0;al=blkmsk;shl ax,1 (x2) */
    ax = (uint16_t)((ax & 0xFF00) | ((ax & 0xFF) | 0x03)); /* or al,11b */
    ax = (uint16_t)(ax & dx); /* dcnt = sdcnt & (blkmsk||'11'b) */
    uint8_t ah = (uint8_t)(ax >> 8);
    bx = (uint16_t)((bx & 0xFF00) | ((bx & 0xFF) | ah)); /* or bl,ah */
    mem[BD_PACKED_DCNT] = (uint8_t)(ax & 0xFF);
    mem_w16(mem, (uint16_t)(BD_PACKED_DCNT + 1), bx);
}

/*
 * search_listo -- shared traversal for search_olist/search_llist/
 * searchn_list
 *	input:	bx = list root (or current position for searchn_list),
 *		ch = search mode (0 = next item, 1 = matching drive,
 *		2/5 = matching drive + ch-1 more packed-dcnt/pdaddr bytes)
 *	output:	returns 1 if found (the real ZF), 0 if not; BD_CUR_POS/
 *		BD_PRV_POS are left the same way the real routine leaves
 *		them (see its own asm comment: prv_pos -> previous element
 *		if found, or the insert-after position if not)
 *
 * olist/llist element format: link(2) || atts(1) || dcnt(3) ||
 * pdaddr(2) || opncnt(2). atts' own low nibble is the drive code,
 * which is why ch==1 only needs the atts byte; ch==2/5 additionally
 * compares bytes starting right at the element's own dcnt field
 * (item+3) against BD_CURDSK+1 -- i.e. BD_PACKED_DCNT and (for ch==5)
 * pdaddr right after it, see header comment.
 */
uint8_t search_listo(uint8_t *mem, uint16_t bx, uint8_t ch) {
    mem_w16(mem, BD_PRV_POS, bx);
    for (;;) {
        bx = mem_r16(mem, bx); /* srch_list1: mov bx,[bx] */
        if (bx == 0) {
            return 0; /* srch_list3 */
        }
        if (ch == 0) {
            mem_w16(mem, BD_CUR_POS, bx);
            return 1; /* srch_list6 */
        }
        uint16_t item = bx;
        uint8_t cl = (uint8_t)(mem[(uint16_t)(item + 2)] & 0x0F);
        uint8_t al = mem[BD_CURDSK];
        if (al == cl) {
            if (ch == 1) {
                mem_w16(mem, BD_CUR_POS, item);
                return 1; /* srch_list5 */
            }
            if (compare(mem, (uint16_t)(item + 3), (uint16_t)(BD_CURDSK + 1), ch)) {
                mem_w16(mem, BD_CUR_POS, item);
                return 1; /* srch_list5 */
            }
            /* fall through to srch_list2 */
        } else if (al < cl) {
            /* srch_list4 -> srch_list5 fallthrough: not found, but
             * this is the sorted-order insertion point */
            mem_w16(mem, BD_CUR_POS, item);
            return 0;
        }
        /* srch_list2 */
        mem_w16(mem, BD_PRV_POS, item);
        bx = item;
    }
}

uint8_t search_olist(uint8_t *mem, uint8_t ch) {
    return search_listo(mem, BD_OPEN_ROOT, ch);
}

uint8_t search_llist(uint8_t *mem, uint8_t ch) {
    return search_listo(mem, BD_LOCK_ROOT, ch);
}

uint8_t searchn_list(uint8_t *mem, uint8_t ch) {
    return search_listo(mem, mem_r16(mem, BD_CUR_POS), ch);
}

extern void move(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl);

/*
 * delete_item -- unlink bx (an olist/llist element) from its list
 * (BD_PRV_POS -> bx, i.e. the caller must have already positioned
 * BD_PRV_POS at bx's own predecessor, same as search_listo leaves it)
 * and push it onto the free list (free_root).
 */
void delete_item(uint8_t *mem, uint16_t bx) {
    uint16_t si = mem_r16(mem, bx); /* bx's own link */
    uint16_t di = mem_r16(mem, BD_PRV_POS);
    mem_w16(mem, BD_CUR_POS, di);
    mem_w16(mem, di, si); /* prv_pos.link = bx.link */
    si = mem_r16(mem, BD_FREE_ROOT);
    mem_w16(mem, BD_FREE_ROOT, bx);
    mem_w16(mem, bx, si); /* bx.link = old free_root */
}

/*
 * create_item -- pop an element off the free list and link it in
 * right after BD_PRV_POS.
 *	output:	returns 1 and *bx_out = the new item's address, or 0
 *		if the free list was empty (the real ZF)
 */
uint8_t create_item(uint8_t *mem, uint16_t *bx_out) {
    uint16_t bx = mem_r16(mem, BD_FREE_ROOT);
    if (bx == 0) {
        return 0;
    }
    mem_w16(mem, BD_CUR_POS, bx);
    uint16_t si = mem_r16(mem, bx); /* free_root's own link */
    mem_w16(mem, BD_FREE_ROOT, si);
    si = mem_r16(mem, BD_PRV_POS);
    uint16_t di = mem_r16(mem, si); /* prv_pos.link */
    mem_w16(mem, bx, di); /* new item.link = prv_pos.link */
    mem_w16(mem, si, bx); /* prv_pos.link = new item */
    *bx_out = bx;
    return 1;
}

/*
 * set_olist_item -- fill in bx (an olist element)'s attrs/drive byte
 * and copy the current packed-dcnt/pdaddr (5 bytes) into it, zeroing
 * its opncnt
 *	input:	al = attributes (drive bits must be 0, merged in from
 *		BD_CURDSK's own low nibble)
 */
void set_olist_item(uint8_t *mem, uint16_t bx, uint8_t al) {
    mem[(uint16_t)(bx + 2)] = (uint8_t)(al | mem[BD_CURDSK]);
    move(mem, BD_PACKED_DCNT, (uint16_t)(bx + 3), 5);
    mem_w16(mem, (uint16_t)(bx + 8), 0);
}
