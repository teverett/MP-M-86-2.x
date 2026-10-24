/*
 * file3.c -- C port of a self-contained subset of src/kernel/
 * FILE3.BDO: PACK_SDCNT, COMPARE, SEARCH_OLIST/SEARCH_LLIST/
 * SEARCHN_LIST (+ internal search_listo), DELETE_ITEM, CREATE_ITEM,
 * SET_OLIST_ITEM, TST_TBL_LMT, REMOVE_LOCKS, FLUSH_FILES/FLUSH_FILE0,
 * INC_PDCNT, CURSELECT (+ internal select_), COMPARE_PDS,
 * TST_OLIST/CHK_OLIST (+ internal chk_olist0), the password-FCB
 * cluster: CHK_PASSWORD/CHK_PWS (+ internal get_dir_mode/cmp_pw/
 * get_xfcb/get_xfcb0/chk_pw), and now `OPEN`'s own remaining FILE3.BDO
 * dependencies: SET_SELDSK, reselect, parsave/parsave33/parsave33r
 * (the first two really BDOSIF.BDO's own, see `parsave`'s own header
 * comment), COPY_DMA_IN/COPY_DMA_OUT, STAMP1/STAMP2 (+ internal
 * stamp3), COUNT_OPENS, CREATE_OLIST_ITEM, FIX_OLIST_ITEM,
 * diskread/sqread, diskwrite/sqwrite (+ internal test_disk_fcb/
 * test_lock/move_arecord/reset_fwf), chk_xfcb_password1, init_xfcb/
 * init_xfcb0, set_pw, diskreset, rseek/rseek1 (the latter split out
 * for FILE4.BDO's own func40), parsave36/parsave36r, rdread, rdwrite,
 * getfilesize/setrandom (+ internal cmperr), tmp_select, and now
 * lock_ (+ internal incr_rr/save_rr/reset_rr, count_locks,
 * check_free, get_lockadd, lock_err_apply, free_lock, set_lock) --
 * this closes out FILE4.BDO entirely, see that file's own header
 * comment.
 *
 * `curselect` was one of `FILE2.BDO`'s own two remaining blockers for
 * its own `DELETE`/`RENAME`/`OPEN`/`MAKE` (the other, `initialize`,
 * is ported in `src-c/kernel/file2.c`'s own same pass). `curselect`'s
 * own real tail, `SELECT`'s disk-login/reinit body and the final
 * `jmp initialize`, is translated faithfully but never reached:
 * `selectdisk`'s own real BDOS-disk-header lookup is a fixed, always-
 * failing boundary stub (see `file1.c`'s own header comment), so
 * `select_` always ends at `selerror` -- see `select_`'s own header
 * comment below for why nothing past that point is translated.
 * `TMP_SELECT`/`XCURSELECT`/`SET_SELDSK`/`RESELECTX`/`reselect` are
 * separate entry points that call *into* `curselect`, not part of it
 * -- not ported here.
 *
 * `COMPARE_PDS`/`TST_OLIST`/`CHK_OLIST` were `DELETE`/`RENAME`'s own
 * cited blocker. `COMPARE_PDS` turned out to be a 1-line tail call
 * into the already-ported `compare`. An earlier revision of this
 * comment claimed `CHK_OLIST` "relies on its own caller having
 * pushed a word onto the stack for it to pop partway through" --
 * wrong, caught re-reading it closely this pass: the `pop dx`/
 * `push dx` pair around its own `compare_pds` call is just ordinary
 * register-save plumbing (freeing a register across one call,
 * restored before any real `ret`), not an implicit calling
 * convention at all. `TST_OLIST`/`CHK_OLIST` turned out to be the
 * same routine (`chk_olist0`) with one flag flipped (verify-only vs.
 * also-remove-and-bump-`BD_PD_CNT`); `openx06` (`FILE4.BDO`) is
 * ported too, see that file's own header comment.
 *
 * `CHK_PWS` was in turn `DELETE`'s own next real blocker once
 * `TST_OLIST`/`CHK_OLIST` were resolved -- a whole password-FCB
 * subsystem nobody had reconned yet. `chk_pw`'s own `CHK_PW1` tail
 * (`mov al,7 ! jmp set_aret`) never returns normally to its own
 * caller, same as `openx06`; unlike that one, though, `chk_pw`'s own
 * real callers (`CHECK_PASSWORD`, and `DELETE`'s own `CHK_PWS`) *do*
 * each need to know whether this happened (so they can stop too,
 * matching the real asm's own never-returns effect) -- so `chk_pw`/
 * `chk_password`/`chk_pws` all return a plain boolean instead of
 * discarding anything, same convention `check_fcb1` already
 * established (see that function's own header comment, `file1.c`).
 * `CMP_PW`'s own `STD`/`LODS` (walking an xfcb's own password field
 * backwards while decrypting it against its own checksum byte) needed
 * no new `tools/i86sim.py` support -- direction-flag tracking was
 * already added for `MAU.MEM`'s own `mau_insert`.
 *
 * FILE3.BDO is not self-contained -- same situation as FILE1.BDO (see
 * src-c/kernel/file1.c's own header comment): it's one of 4 files
 * meant to be assembled together into one BDOS module. This is a
 * small, scoped follow-up pass, not a full file3 port: `PR_TERM`
 * calls the still-unported `func48`, and is left for a later pass.
 *
 * `FLUSH_FILES`/`FLUSH_FILE0` (+ their own `REMOVE_LOCKS`) were
 * flagged in an earlier pass as blocked by the real segment-relative-
 * addressing gap (`FLUSH_FILE0` dynamically switches `ES` to a PD's
 * own UDA segment, `mov es,p_uda[bx]`, then reads `u_pd_cnt` relative
 * to it) -- now resolved the same way as `ml.c`/`proc.c`/`mau.c`/
 * `share.c`/`load.c` (`mem_r8_seg`/`mem_w8_seg` against that PD's own
 * UDA segment, `u_pd_cnt`'s real UDA.FMT offset already in defs.h).
 * `INC_PDCNT` is `CHK_OLIST`'s own internal label, shared by
 * `FLUSH_FILE0`; ported standalone since `CHK_OLIST` itself stays
 * deferred. One other routine in this file (not identified by name
 * here) uses `std` (reverse-direction string ops), the same gap as
 * `MAU.MEM` (now also resolved, `tools/i86sim.py` tracks the
 * direction flag) -- not reconned this pass, left for the next one.
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
 *
 * `OPEN`'s own real top-level entry point is `FILE4.BDO`'s own
 * `func15` (ported in `file4.c`, see that function's own header
 * comment) -- this pass's `reselect`/`parsave33r`/`COUNT_OPENS`/
 * `CREATE_OLIST_ITEM` etc. are its own dependencies, not independently
 * callable BDOS entries. `--symbols` against this combined bundle
 * confirms `parsave`/`parsave33`/`SWAP` (`file2.c`'s own) land inside
 * DATA.BDO's own dseg range (0x000-0x1000, same class of real-address
 * collision as `searchn`'s own header comment describes) -- no
 * differential test written for those specifically, same as that
 * precedent -- but everything else this pass added (`reselect`'s own
 * cluster, `COPY_DMA_IN`/`COPY_DMA_OUT`, `STAMP1`/`STAMP2`,
 * `COUNT_OPENS`, `CREATE_OLIST_ITEM`) is individually clear of it.
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
void pack_sdcnt(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_SDBLK);
    if (bx == 0) {
        bx = mem_r16_seg(mem, ds_bdos, BD_SDCNT);
        uint8_t cl = (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_BLKSHF) + 2);
        bx = (uint16_t)(bx >> cl);
    }
    bx = (uint16_t)(bx << 1); /* shl bx,1 -- shift dblk left 1 */
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_SDCNT);
    uint16_t ax = (uint16_t)((uint16_t)mem_r8_seg(mem, ds_bdos,BD_BLKMSK) << 2); /* ah=0;al=blkmsk;shl ax,1 (x2) */
    ax = (uint16_t)((ax & 0xFF00) | ((ax & 0xFF) | 0x03)); /* or al,11b */
    ax = (uint16_t)(ax & dx); /* dcnt = sdcnt & (blkmsk||'11'b) */
    uint8_t ah = (uint8_t)(ax >> 8);
    bx = (uint16_t)((bx & 0xFF00) | ((bx & 0xFF) | ah)); /* or bl,ah */
    mem_w8_seg(mem, ds_bdos,BD_PACKED_DCNT, (uint8_t)(ax & 0xFF));
    mem_w16_seg(mem, ds_bdos, (uint16_t)(BD_PACKED_DCNT + 1), bx);
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
uint8_t search_listo(uint8_t *mem, uint16_t ds_bdos, uint16_t bx, uint8_t ch) {
    mem_w16_seg(mem, ds_bdos, BD_PRV_POS, bx);
    for (;;) {
        bx = mem_r16(mem, bx); /* srch_list1: mov bx,[bx] */
        if (bx == 0) {
            return 0; /* srch_list3 */
        }
        if (ch == 0) {
            mem_w16_seg(mem, ds_bdos, BD_CUR_POS, bx);
            return 1; /* srch_list6 */
        }
        uint16_t item = bx;
        uint8_t cl = (uint8_t)(mem[(uint16_t)(item + 2)] & 0x0F);
        uint8_t al = mem_r8_seg(mem, ds_bdos,BD_CURDSK);
        if (al == cl) {
            if (ch == 1) {
                mem_w16_seg(mem, ds_bdos, BD_CUR_POS, item);
                return 1; /* srch_list5 */
            }
            if (compare(mem, (uint16_t)(item + 3),mem_seg_addr(ds_bdos, (uint16_t)(BD_CURDSK + 1)), ch)) {
                mem_w16_seg(mem, ds_bdos, BD_CUR_POS, item);
                return 1; /* srch_list5 */
            }
            /* fall through to srch_list2 */
        } else if (al < cl) {
            /* srch_list4 -> srch_list5 fallthrough: not found, but
             * this is the sorted-order insertion point */
            mem_w16_seg(mem, ds_bdos, BD_CUR_POS, item);
            return 0;
        }
        /* srch_list2 */
        mem_w16_seg(mem, ds_bdos, BD_PRV_POS, item);
        bx = item;
    }
}

uint8_t search_olist(uint8_t *mem, uint16_t ds_bdos, uint8_t ch) {
    return search_listo(mem, ds_bdos, BD_OPEN_ROOT, ch);
}

uint8_t search_llist(uint8_t *mem, uint16_t ds_bdos, uint8_t ch) {
    return search_listo(mem, ds_bdos, BD_LOCK_ROOT, ch);
}

uint8_t searchn_list(uint8_t *mem, uint16_t ds_bdos, uint8_t ch) {
    return search_listo(mem, ds_bdos, mem_r16_seg(mem, ds_bdos, BD_CUR_POS), ch);
}

extern void move(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl);
extern uint16_t test_vector(const uint8_t *mem, uint16_t ds_bdos, uint16_t dx);
extern void selectdisk(uint8_t *mem, uint16_t ds_bdos);
extern void selerror(uint8_t *mem, uint16_t ds_bdos);
extern void openx06(uint8_t *mem, uint16_t ds_bdos);
extern void set_aret(uint8_t *mem, uint16_t ds_bdos, uint8_t al);
extern uint8_t search(uint8_t *mem, uint16_t ds_bdos, uint8_t cl);
extern uint8_t searchn(uint8_t *mem, uint16_t ds_bdos);
extern uint16_t getdptra(const uint8_t *mem, uint16_t ds_bdos);
extern void clr_ext(uint8_t *mem, uint16_t ds_bdos);
extern uint16_t get_rcnta(const uint8_t *mem, uint16_t ds_bdos);
extern void swap_sdcnt(uint8_t *mem, uint16_t ds_bdos);
extern void sdcnt_eq_xdcnt(uint8_t *mem, uint16_t ds_bdos);
extern uint8_t tst_inv_fcb(uint8_t *mem, uint16_t ds_bdos);
extern void getfcb(uint8_t *mem, uint16_t ds_bdos);
extern uint8_t check_fcb(uint8_t *mem, uint16_t ds_bdos);
extern void setfcb(uint8_t *mem, uint16_t ds_bdos);
extern void bdos_index(uint8_t *mem, uint16_t ds_bdos);
extern uint16_t alloct(const uint8_t *mem, uint16_t ds_bdos);
extern void atran(uint8_t *mem, uint16_t ds_bdos);
extern void seek(uint8_t *mem);
extern void rdbuff(uint8_t *mem, uint16_t ds_bdos);
extern uint8_t getmodnum(const uint8_t *mem, uint16_t ds_bdos, uint16_t *bx_out);
extern void close1(uint8_t *mem, uint16_t ds_bdos);
extern void close(uint8_t *mem, uint16_t ds_bdos);
extern void openreel(uint8_t *mem, uint16_t ds_bdos);
extern void checkwrite(uint8_t *mem, uint16_t ds_bdos);
extern void ckrofile(uint8_t *mem, uint16_t ds_bdos, uint16_t bx);
extern uint8_t dmposition(const uint8_t *mem, uint16_t ds_bdos);
extern uint16_t getdm(const uint8_t *mem, uint16_t ds_bdos, uint8_t cl);
extern uint16_t getblock(uint8_t *mem, uint16_t ds_bdos, uint16_t cx);
extern void wrbufflg(uint8_t *mem, uint16_t ds_bdos);
extern void setcdr(uint8_t *mem, uint16_t ds_bdos);
extern uint16_t test_vector1(uint16_t dx, uint8_t cl);
extern uint16_t set_cdisk1(uint8_t al, uint16_t cx);
extern uint16_t remove_drive(const uint8_t *mem, uint16_t ds_bdos, uint16_t bx);
extern uint16_t chk_inv_fcb(const uint8_t *mem, uint16_t ds_bdos);
extern uint8_t compext(const uint8_t *mem, uint16_t ds_bdos, uint8_t al, uint8_t cl);
extern get_dir_ext_result get_dir_ext(uint8_t *mem, uint16_t ds_bdos);
extern void restore_rc(uint8_t *mem, uint16_t ds_bdos, uint16_t bx);
extern void set_rc(uint8_t *mem, uint16_t ds_bdos, uint16_t bx, uint8_t cl);
extern void open(uint8_t *mem, uint16_t ds_bdos);
extern void make(uint8_t *mem, uint16_t ds_bdos);
extern void make_fcb_inv(uint8_t *mem, uint16_t ds_bdos);
extern uint16_t get_rra(const uint8_t *mem, uint16_t ds_bdos);

/*
 * delete_item -- unlink bx (an olist/llist element) from its list
 * (BD_PRV_POS -> bx, i.e. the caller must have already positioned
 * BD_PRV_POS at bx's own predecessor, same as search_listo leaves it)
 * and push it onto the free list (free_root).
 */
void delete_item(uint8_t *mem, uint16_t ds_bdos, uint16_t bx) {
    uint16_t si = mem_r16(mem, bx); /* bx's own link */
    uint16_t di = mem_r16_seg(mem, ds_bdos, BD_PRV_POS);
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, di);
    mem_w16(mem, di, si); /* prv_pos.link = bx.link */
    si = mem_r16_seg(mem, ds_bdos, BD_FREE_ROOT);
    mem_w16_seg(mem, ds_bdos, BD_FREE_ROOT, bx);
    mem_w16(mem, bx, si); /* bx.link = old free_root */
}

/*
 * create_item -- pop an element off the free list and link it in
 * right after BD_PRV_POS.
 *	output:	returns 1 and *bx_out = the new item's address, or 0
 *		if the free list was empty (the real ZF)
 */
uint8_t create_item(uint8_t *mem, uint16_t ds_bdos, uint16_t *bx_out) {
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_FREE_ROOT);
    if (bx == 0) {
        return 0;
    }
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, bx);
    uint16_t si = mem_r16(mem, bx); /* free_root's own link */
    mem_w16_seg(mem, ds_bdos, BD_FREE_ROOT, si);
    si = mem_r16_seg(mem, ds_bdos, BD_PRV_POS);
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
void set_olist_item(uint8_t *mem, uint16_t ds_bdos, uint16_t bx, uint8_t al) {
    mem[(uint16_t)(bx + 2)] = (uint8_t)(al | mem_r8_seg(mem, ds_bdos,BD_CURDSK));
    move(mem,mem_seg_addr(ds_bdos, BD_PACKED_DCNT), (uint16_t)(bx + 3), 5);
    mem_w16(mem, (uint16_t)(bx + 8), 0);
}

/*
 * tst_tbl_lmt -- compare the word at bx+cx against dx
 *	output: returns 1 if equal (the real ZF), 0 if not
 */
static uint8_t tst_tbl_lmt(const uint8_t *mem, uint16_t bx, uint16_t cx, uint16_t dx) {
    return mem_r16(mem, (uint16_t)(bx + cx)) == dx ? 1 : 0;
}

/*
 * remove_locks -- remove every lock-list record belonging to olist
 * item bx (identified by that item's own address, stored at offset 8
 * in each of its own lock-list records)
 */
void remove_locks(uint8_t *mem, uint16_t ds_bdos, uint16_t bx) {
    if (!(mem[(uint16_t)(bx + 2)] & 0x40)) {
        return;
    }
    uint16_t owner = bx;
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_LOCK_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 0)) {
            return; /* retl25 */
        }
        uint16_t found = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        if (tst_tbl_lmt(mem, found, 8, owner)) {
            delete_item(mem, ds_bdos, found);
        }
    }
}

/*
 * inc_pdcnt -- bump BD_PD_CNT by 16, skipping over 0 (CHK_OLIST's own
 * internal label, shared by FLUSH_FILE0)
 */
void inc_pdcnt(uint8_t *mem, uint16_t ds_bdos) {
    uint8_t al = mem_r8_seg(mem, ds_bdos,BD_PD_CNT);
    do {
        al = (uint8_t)(al + 16);
    } while (al == 0);
    mem_w8_seg(mem, ds_bdos,BD_PD_CNT, al);
}

/*
 * flush_file0 -- flush (close) every open file on the current
 * default disk (BD_CURDSK, via searchn_list's own ch==1 mode),
 * removing any locks each one holds, and bump BD_PD_CNT once per
 * distinct process that owned a flushed file (u_pd_cnt's own low bit
 * marks a process already counted this pass, cleared elsewhere).
 */
void flush_file0(uint8_t *mem, uint16_t ds_bdos) {
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_OPEN_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 1)) {
            return; /* retl2 */
        }
        uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        remove_locks(mem, ds_bdos, item);
        delete_item(mem, ds_bdos, item);

        uint16_t pd = mem_r16(mem, (uint16_t)(item + 6));
        uint16_t pd_uda = mem_r16(mem, (uint16_t)(pd + p_uda));
        uint8_t pd_cnt_flag = mem_r8_seg(mem, pd_uda, u_pd_cnt);
        if (pd_cnt_flag & 1) {
            continue;
        }
        mem_w8_seg(mem, pd_uda, u_pd_cnt, (uint8_t)(pd_cnt_flag | 1));
        if (pd == mem_r16_seg(mem, ds_bdos, BD_PDADDR)) {
            inc_pdcnt(mem, ds_bdos);
        }
    }
}

/* flush_files -- flush_file0, but only once (BD_FLUSHED guards it) */
void flush_files(uint8_t *mem, uint16_t ds_bdos) {
    if (mem_r8_seg(mem, ds_bdos,BD_FLUSHED)) {
        return; /* retl2 */
    }
    mem_w8_seg(mem, ds_bdos,BD_FLUSHED, 1);
    flush_file0(mem, ds_bdos);
}

extern void func48(uint8_t *mem); /* src-c/kernel/file4.c */

/*
 * pr_term -- PR_TERM: close every open file BD_PDADDR's own process
 * holds open (removing its locks too), then flush every blocking/
 * deblocking buffer. Same `searchn_list`/`remove_locks`/`delete_item`
 * loop shape as `flush_file0` above, but walking every olist entry
 * unconditionally (`ch=0`) and filtering by PD address directly
 * (`tst_tbl_lmt` against the item's own pdaddr field, offset 6) --
 * src/kernel/FILE3.BDO's own `call func48` once the loop ends
 * resolves to src/kernel/FILE4.BDO's own `FUNC48` (confirmed by
 * grep against `orig/mpm862sr`'s own copy too, after an initial
 * case-sensitive grep for `func48` -- lowercase -- turned up
 * nothing: the real label is `FUNC48`, uppercase).
 */
void pr_term(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t pdaddr = mem_r16_seg(mem, ds_bdos, BD_PDADDR);
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_OPEN_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 0)) {
            func48(mem);
            return; /* retl2 */
        }
        uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        if (!tst_tbl_lmt(mem, item, 6, pdaddr)) {
            continue;
        }
        remove_locks(mem, ds_bdos, item);
        delete_item(mem, ds_bdos, item);
    }
}

/*
 * select_ -- SELECT: select the disk at BD_SELDSK for subsequent I/O.
 * `selectdisk`'s own real BDOS-disk-header lookup is a fixed, always-
 * failing boundary stub (see `file1.c`'s own header comment on it,
 * and `curselect`'s own below) -- under that convention this always
 * ends in `selerror`. The rest of `SELECT`'s own real body (logging
 * the drive in for real, re-initializing it, and the final `jmp
 * initialize`) is real, well-shaped code that's simply never reached
 * under that fix, same class as `selectdisk`'s own documented
 * unreachable success path -- not translated further, same choice
 * `command.c`'s own `cli_gopen` makes for an analogous "stop right
 * past the last real boundary" case.
 */
static void select_(uint8_t *mem, uint16_t ds_bdos) {
    (void)test_vector(mem, ds_bdos, mem_r16_seg(mem, ds_bdos, BD_DLOG)); /* dl = 1 if drive logged in */
    selectdisk(mem, ds_bdos);
    selerror(mem, ds_bdos);
}

/*
 * curselect -- CURSELECT: select BD_SELDSK as the current disk,
 * unless it already is one.
 */
void curselect(uint8_t *mem, uint16_t ds_bdos) {
    if (mem_r8_seg(mem, ds_bdos,BD_SELDSK) == mem_r8_seg(mem, ds_bdos,BD_CURDSK)) {
        /* test_ff_disk */
        if ((uint8_t)(mem_r8_seg(mem, ds_bdos,BD_SELDSK) + 1) != 0) {
            return; /* ret20a */
        }
        selerror(mem, ds_bdos);
        return;
    }
    select_(mem, ds_bdos);
}

/*
 * tmp_select -- TMP_SELECT: temporarily select `dl` as the current
 * disk (via `CURSELECT`), then restore `BD_SELDSK` to whatever it
 * was before -- `CURSELECT`'s own `BD_CURDSK` side effect is left in
 * place, only the "which disk is selected" scratch field is restored.
 */
void tmp_select(uint8_t *mem, uint16_t ds_bdos, uint8_t dl) {
    uint8_t saved_seldsk = mem_r8_seg(mem, ds_bdos,BD_SELDSK);
    mem_w8_seg(mem, ds_bdos,BD_SELDSK, dl);
    curselect(mem, ds_bdos);
    mem_w8_seg(mem, ds_bdos,BD_SELDSK, saved_seldsk);
}

/*
 * xcurselect -- XCURSELECT: select BD_SELDSK via CURSELECT, with
 * BD_RETURN_FFFF set around the call -- `goerr`'s own real consumer
 * (src-c/kernel/file1.c), making a select error reported back as
 * `aret=0xFFFF` instead of this process's own usual error message,
 * for the two BDOS functions (27, 31) that need that.
 */
void xcurselect(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_RETURN_FFFF, 1); /* true */
    curselect(mem, ds_bdos);
    mem_w8_seg(mem, ds_bdos,BD_RETURN_FFFF, 0); /* false */
}

/*
 * compare_pds -- COMPARE_PDS: compare an olist/llist element's own
 * saved PD address (2 bytes right after its own packed-dcnt field,
 * item+6) against BD_PDADDR, the PD this call is actually for
 *	output: returns 1 if equal (the real ZF), 0 if not
 */
uint8_t compare_pds(const uint8_t *mem, uint16_t ds_bdos, uint16_t bx) {
    return compare(mem, (uint16_t)(bx + 6),mem_seg_addr(ds_bdos, BD_PDADDR), 2);
}

/*
 * chk_olist0 -- shared tail of TST_OLIST/CHK_OLIST: move dcnt/dblk
 * into sdcnt/sdblk (temporarily substituting them for pack_sdcnt's
 * own purposes), search the open list for a matching entry (ch=3);
 * if found, and it's neither excluded by its own attribute byte's
 * bit 7 nor owned by this call's own target PD (`compare_pds`), bail
 * out via `openx06` (never returns normally, see that function's own
 * header comment). Otherwise, if this is `CHK_OLIST` (not
 * `TST_OLIST`, which only verifies), delete the matching item and
 * bump `BD_PD_CNT`.
 *	input: chk_olist_flag = 0 for TST_OLIST's own behavior (verify
 *		only), nonzero for CHK_OLIST's (also remove + bump)
 */
static void chk_olist0(uint8_t *mem, uint16_t ds_bdos, uint8_t chk_olist_flag) {
    move(mem,mem_seg_addr(ds_bdos, BD_DCNT),mem_seg_addr(ds_bdos, BD_SDCNT), 4);
    pack_sdcnt(mem, ds_bdos);
    if (!search_olist(mem, ds_bdos, 3)) {
        return; /* retl1 -- not found at all */
    }
    uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
    if ((mem[(uint16_t)(item + 2)] & 0x80) == 0) {
        openx06(mem, ds_bdos);
        return;
    }
    if (!compare_pds(mem, ds_bdos, item)) {
        openx06(mem, ds_bdos);
        return;
    }
    if (!chk_olist_flag) {
        return; /* tst_olist -- verification only */
    }
    delete_item(mem, ds_bdos, item);
    /* inc_pdcnt */
    uint8_t al = mem_r8_seg(mem, ds_bdos,BD_PD_CNT);
    do {
        al = (uint8_t)(al + 16);
    } while (al == 0);
    mem_w8_seg(mem, ds_bdos,BD_PD_CNT, al);
}

void tst_olist(uint8_t *mem, uint16_t ds_bdos) {
    chk_olist0(mem, ds_bdos, 0);
}

void chk_olist(uint8_t *mem, uint16_t ds_bdos) {
    chk_olist0(mem, ds_bdos, 1);
}

/*
 * get_dir_mode -- GET_DIR_MODE: the directory label data byte.
 * Not `static`: `src-c/kernel/file4.c`'s own `func15` (BDOS OPEN)
 * also calls `GET_DIR_MODE` directly, an ordinary cross-file call,
 * same as `que1.c`'s own into `que2.c`.
 */
uint8_t get_dir_mode(const uint8_t *mem, uint16_t ds_bdos) {
    return mem_r8_seg(mem, ds_bdos,mem_r16_seg(mem, ds_bdos, BD_DRVLBLA));
}

/*
 * cmp_pw -- CMP_PW: compare a user-supplied password (BD_XDMAAD) --
 * encrypted against an xfcb's own checksum byte -- against the
 * xfcb's own stored password at bx, falling back first to the
 * special "null password" case (the xfcb's own password is all
 * blanks and/or zeros, which matches anything) and then, on a
 * mismatch, to the system-wide default password.
 *	input: bx = .xfcb(12)
 *	output: returns 1 if the passwords match (the real ZF), 0 if not
 * Not `static`: `src-c/kernel/file4.c`'s own `func15` (BDOS OPEN)
 * also calls `CMP_PW` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`.
 */
uint8_t cmp_pw(const uint8_t *mem, uint16_t ds_bdos, uint16_t bx) {
    bx = (uint16_t)(bx + 1); /* .xfcb(13) */
    uint8_t ch = mem[bx];
    if (ch != 0) {
        goto cmp_pw2;
    }
    {
        uint16_t saved_bx = bx;
        uint16_t probe = (uint16_t)(bx + 2);
        uint8_t cl = 9;
    cmp_pw1:
        probe = (uint16_t)(probe + 1);
        {
            uint8_t al = mem[probe];
            cl = (uint8_t)(cl - 1);
            if (cl == 0) {
                return 1; /* ret20 -- null password matches anything */
            }
            if (al == 0 || al == 0x20) {
                goto cmp_pw1;
            }
        }
        bx = saved_bx; /* a real, non-blank password -- compare for real */
    }
cmp_pw2:
    {
        uint16_t si = (uint16_t)(bx + 10);
        uint16_t dx = (uint16_t)(bx + 3);
        uint16_t xbx = mem_r16_seg(mem, ds_bdos, BD_XDMAAD);
        uint8_t cl = 8;
    cmp_pw3:
        {
            uint8_t al = (uint8_t)(mem[si] ^ ch); /* std ! lods al */
            si = (uint16_t)(si - 1);
            if (al != mem[xbx]) {
                goto cmp_pw4;
            }
            xbx = (uint16_t)(xbx + 1);
            cl = (uint8_t)(cl - 1);
            if (cl != 0) {
                goto cmp_pw3;
            }
            return 1; /* password matches */
        }
    cmp_pw4:
        return compare(mem,mem_seg_addr(ds_bdos, BD_DF_PASSWORD), dx, 8);
    }
}

/*
 * get_xfcb0 -- GET_XFCB/GET_XFCB0: locate the current (ch==0) or
 * next (ch!=0) extended FCB matching BD_INFO's own name/type, via
 * `search`/`searchn` with FCB(0)'s own bit 4 temporarily forced on.
 *	output: found=0 if no xfcb was found; otherwise found=1, al =
 *		the xfcb's own password mode (bits 5-7) with bit 0 set,
 *		and bx = .xfcb(extnum) (`cmp_pw`'s own expected input;
 *		`get_xfcb_result` itself now lives in defs.h, since
 *		`file4.c`'s own `func15` needs it too)
 */
static get_xfcb_result get_xfcb0(uint8_t *mem, uint16_t ds_bdos, uint8_t ch) {
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_INFO);
    uint8_t saved_al = mem[bx];
    mem[bx] = (uint8_t)(saved_al | 0x10);
    uint8_t found = (ch == 0) ? search(mem, ds_bdos, fcb_extnum) : searchn(mem, ds_bdos);
    mem_w8_seg(mem, ds_bdos,BD_LRET, 0);
    bx = mem_r16_seg(mem, ds_bdos, BD_INFO);
    mem[bx] = saved_al;
    get_xfcb_result r;
    if (!found) {
        r.al = 0;
        r.found = 0;
        r.bx = 0;
        return r;
    }
    bx = (uint16_t)(getdptra(mem, ds_bdos) + fcb_extnum);
    r.al = (uint8_t)((mem[bx] & 0xE0) | 1);
    r.found = 1;
    r.bx = bx;
    return r;
}

/*
 * get_xfcb -- GET_XFCB. Not `static`: `src-c/kernel/file4.c`'s own
 * `func15` (BDOS OPEN) also calls `GET_XFCB` directly, an ordinary
 * cross-file call, same as `que1.c`'s own into `que2.c`.
 */
get_xfcb_result get_xfcb(uint8_t *mem, uint16_t ds_bdos) {
    return get_xfcb0(mem, ds_bdos, 0);
}

/*
 * chk_pw -- CHK_PW: verify the password on the currently addressed
 * xfcb, if any.
 *	output: returns 1 if ok (no xfcb, password mode 0, or a real
 *		match), 0 on a password error. On that error, the real
 *		asm tail-calls `set_aret` (which never returns normally
 *		to CHK_PW's own caller -- see that function's own header
 *		comment, `file4.c`) -- this calls it too, but, same as
 *		`check_fcb1`'s own convention, propagates the "stop,
 *		don't continue normally" signal back as a plain boolean
 *		instead of literally discarding a return address, so
 *		every real caller (`CHECK_WILD`'s own analog, `DELETE`'s
 *		own `CHK_PWS`, not yet ported when this was written) must
 *		itself return immediately on a 0 here, replicating that
 *		same never-returns-normally effect by hand.
 */
static uint8_t chk_pw(uint8_t *mem, uint16_t ds_bdos) {
    get_xfcb_result r = get_xfcb(mem, ds_bdos);
    if (!r.found) {
        return 1; /* ret20 */
    }
    if ((uint8_t)(r.al - 1) == 0) {
        return 1; /* ret20 -- password mode 0 */
    }
    if (cmp_pw(mem, ds_bdos, r.bx)) {
        return 1; /* ret20 -- password ok */
    }
    set_aret(mem, ds_bdos, 7); /* chk_pw1 -- password error */
    return 0;
}

/*
 * chk_password -- CHK_PASSWORD: verify the current xfcb's own
 * password, but only if the directory's own label byte says
 * passwords are active at all (bit 7).
 *	output: see `chk_pw`'s own header comment
 */
uint8_t chk_password(uint8_t *mem, uint16_t ds_bdos) {
    if (get_dir_mode(mem, ds_bdos) & 0x80) {
        return chk_pw(mem, ds_bdos);
    }
    return 1;
}

/*
 * chk_pws -- CHK_PWS: like `chk_password`, but checks the password
 * on *every* xfcb for this fcb's own name/type, not just the first
 * one.
 *	output: see `chk_pw`'s own header comment
 */
uint8_t chk_pws(uint8_t *mem, uint16_t ds_bdos) {
    if ((get_dir_mode(mem, ds_bdos) & 0x80) == 0) {
        return 1; /* ret21 */
    }
    get_xfcb_result r = get_xfcb(mem, ds_bdos);
    for (;;) {
        if (!r.found) {
            return 1; /* ret21 */
        }
        if ((uint8_t)(r.al - 1) != 0 && !cmp_pw(mem, ds_bdos, r.bx)) {
            return 0; /* ret21 -- password error */
        }
        r = get_xfcb0(mem, ds_bdos, 1);
    }
}

/*
 * set_seldsk -- SET_SELDSK: select BD_LINFO (the disk number
 * `reselect` normalized the current fcb's own drive-select byte
 * into) as the running process's current disk.
 */
void set_seldsk(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_SELDSK, mem_r8_seg(mem, ds_bdos,BD_LINFO));
}

/*
 * reselect_tail -- RESELECT1 onward: check the current fcb to see if
 * reselection is necessary, switching to the fcb's own named drive
 * via `set_seldsk`/`curselect` if it names one, then folding this
 * process's own user code into the fcb's drive byte. Shared by
 * `reselect` (which computes BD_ACTUAL_RC for real first) and
 * `reselectx` (which jumps straight here, see that function's own
 * header comment).
 */
static void reselect_tail(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_RESEL, 1);
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_INFO);
    uint8_t al = (uint8_t)((mem[bx] & 0x1F) - 1);
    mem_w8_seg(mem, ds_bdos,BD_LINFO, al);
    if (al != 0xFF) {
        mem_w8_seg(mem, ds_bdos,BD_OLDDSK, mem_r8_seg(mem, ds_bdos,BD_SELDSK));
        mem_w8_seg(mem, ds_bdos,BD_FCBDSK, mem[bx]);
        mem[bx] = (uint8_t)(mem[bx] & 0xE0);
        set_seldsk(mem, ds_bdos);
    }
    curselect(mem, ds_bdos);
    mem[bx] = (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_USRCODE) | mem[bx]);
}

/*
 * reselect -- reselect: normalize the current fcb's own read-only/
 * high-extent/actual-record-count bits (same fields `CURSELECT`'s own
 * callers rely on `search`/`searchn` to have set up), then, if the
 * fcb names an explicit (non-auto-select) drive, switch to it via
 * `set_seldsk`/`curselect` before folding this process's own user
 * code into the fcb's drive byte.
 */
void reselect(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + 7);
    uint8_t al = mem[bx];
    mem_w8_seg(mem, ds_bdos,BD_XFCB_READ_ONLY, (uint8_t)(al & 0x80));
    mem[bx] = (uint8_t)(al & 0x7F);
    bx++;
    al = (uint8_t)(mem[bx] & 0x7F);
    uint8_t high_ext;
    if (al != mem[bx]) {
        /* fcb(8) had its own high bit set */
        mem[bx] = al;
        high_ext = 0x60;
    } else {
        mem[bx] = al;
        bx = (uint16_t)(bx + 4); /* info+12 */
        high_ext = (uint8_t)(0xE0 & mem[bx]);
    }
    mem_w8_seg(mem, ds_bdos,BD_HIGH_EXT, high_ext);
    clr_ext(mem, ds_bdos);
    bx = get_rcnta(mem, ds_bdos);
    al = (uint8_t)(mem[bx] & 0x80);
    if (al != 0) {
        al = (uint8_t)(mem[bx] & 0x7F);
        mem[bx] = 0x80;
    }
    mem_w8_seg(mem, ds_bdos,BD_ACTUAL_RC, al);
    reselect_tail(mem, ds_bdos);
}

/*
 * reselectx -- RESELECTX: same as `reselect`, but forcing
 * BD_HIGH_EXT=0 and skipping the XFCB_READ_ONLY/BD_ACTUAL_RC
 * computation (and the `clr_ext`/`get_rcnta` calls that feed it)
 * entirely -- the real asm jumps directly into the middle of
 * `reselect`'s own body (RESELECT1), with BD_ACTUAL_RC left at
 * whatever AL happened to be at RESELECTX's own call site (an
 * undefined artifact under this port, fixed at 0, same "undefined
 * artifact, fixed choice" precedent as `selectdisk`'s own fixed
 * `bx=0`).
 */
void reselectx(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_HIGH_EXT, 0);
    mem_w8_seg(mem, ds_bdos,BD_ACTUAL_RC, 0);
    reselect_tail(mem, ds_bdos);
}

/*
 * parsave -- src/kernel/BDOSIF.BDO's own `parsave`: copy `cl` bytes
 * of the caller's own incoming parameter block from the user's
 * segment into BDOS's local scratch area (`BD_LOC_PAR_AREA`), then
 * redirect `BD_INFO` to point at the copy instead of the original.
 * Skipped entirely (no copy, `BD_INFO` untouched) when running under
 * the command shell (`BD_SHELL_FLAG` set) -- a real BDOSIF.BDO
 * routine, not FILE3.BDO's own, but ported here (no bdosif.c exists
 * yet) since `reselect`'s own `parsave33r` is its only real caller so
 * far. Like every other cross-segment routine in this port
 * (`src-c/kernel/file1.c`'s own header comment explains the general
 * convention), the real `ds=parametersegment` switch is modeled as
 * flat -- the source bytes are read from this same `mem` buffer, at
 * whatever flat address `BD_INFO` already held.
 */
void parsave(uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    if (mem_r8_seg(mem, ds_bdos,BD_SHELL_FLAG)) {
        return; /* parret */
    }
    mem_w8_seg(mem, ds_bdos,BD_PARCOPFL, 1);
    mem_w8_seg(mem, ds_bdos,BD_PARLG, cl);
    uint16_t si = mem_r16_seg(mem, ds_bdos, BD_INFO);
    mem_w16_seg(mem, ds_bdos, BD_INFOSAVE, si);
    uint16_t di = mem_seg_addr(ds_bdos, BD_LOC_PAR_AREA);
    mem_w16_seg(mem, ds_bdos, BD_INFO, di);
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(di + i)] = mem[(uint16_t)(si + i)];
    }
}

/* parsave33 -- BDOSIF.BDO's own `parsave33`: parsave a 33-byte fcb */
void parsave33(uint8_t *mem, uint16_t ds_bdos) {
    parsave(mem, ds_bdos, 33);
}

/*
 * parsave33r -- parsave a 33-byte fcb, then run `reselect` on the
 * copy (the real asm's own tail `jmp reselect`, which returns
 * normally via `reselect`'s own `ret20`).
 */
void parsave33r(uint8_t *mem, uint16_t ds_bdos) {
    parsave33(mem, ds_bdos);
    reselect(mem, ds_bdos);
}

/*
 * parsave36 -- BDOSIF.BDO's own `parsave36`: parsave a 36-byte xfcb.
 * Lands at 0x454 in the combined wrapper bundle -- inside DATA.BDO's
 * own dseg collision range, same as `parsave`/`parsave33` (see
 * `open`'s own header comment, `file2.c`) -- so, same as those, no
 * new differential test for anything that calls through this.
 */
void parsave36(uint8_t *mem, uint16_t ds_bdos) {
    parsave(mem, ds_bdos, 36);
}

/*
 * parsave36r -- parsave a 36-byte xfcb, then run `reselect` on the
 * copy (same reasoning as `parsave33r`'s own header comment).
 */
void parsave36r(uint8_t *mem, uint16_t ds_bdos) {
    parsave36(mem, ds_bdos);
    reselect(mem, ds_bdos);
}

/*
 * copy_dma_in -- COPY_DMA_IN: copy `cl` bytes from the user's own dma
 * area into `BD_COMMON_DMA` (a FILE3.BDO-own flat scratch buffer, not
 * a flag -- despite `defs.h`'s own single-byte `--symbols` size).
 * Same ambient-flat segment convention as `parsave`'s own header
 * comment (the real `ds=DMABASE` switch is not modeled).
 */
void copy_dma_in(uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    uint16_t si = mem_r16_seg(mem, ds_bdos, BD_DMAAD);
    for (uint8_t i = 0; i < cl; i++) {
        mem_w8_seg(mem, ds_bdos,(uint16_t)(BD_COMMON_DMA + i), mem[(uint16_t)(si + i)]);
    }
}

/* copy_dma_out -- COPY_DMA_OUT: the reverse copy of `copy_dma_in` */
void copy_dma_out(uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    uint16_t di = mem_r16_seg(mem, ds_bdos, BD_DMAAD);
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(di + i)] = mem_r8_seg(mem, ds_bdos,(uint16_t)(BD_COMMON_DMA + i));
    }
}

/*
 * fix_olist_item -- FIX_OLIST_ITEM: after `MAKE`'s own `openreel`
 * extends a file, propagate its new (bigger) directory
 * count/block-count into every open-file-list entry for this file.
 * The real asm's own `swap`/`pack_sdcnt`/`swap` sandwiched right
 * before the `searchn_list` call recomputes `BD_PACKED_DCNT` from
 * whatever `BD_SDCNT`/`BD_SDBLK` held *before* this call (restored by
 * the second `swap`) -- but that result is unconditionally
 * overwritten by the second `pack_sdcnt` call below, before anything
 * ever reads it. Provably dead the same way `CHECK_FCB` is (see that
 * function's own header comment, `file1.c`), so not translated.
 */
void fix_olist_item(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t xdcnt = mem_r16_seg(mem, ds_bdos, BD_XDCNT);
    uint16_t sdcnt = mem_r16_seg(mem, ds_bdos, BD_SDCNT);
    if (xdcnt != sdcnt) {
        if (xdcnt >= sdcnt) {
            return; /* retl7 */
        }
    } else if (mem_r16_seg(mem, ds_bdos, BD_XDBLK) >= mem_r16_seg(mem, ds_bdos, BD_SDBLK)) {
        return; /* retl7 */
    }
    swap_sdcnt(mem, ds_bdos);
    sdcnt_eq_xdcnt(mem, ds_bdos);
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_OPEN_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 3)) {
            return; /* retl8 */
        }
        uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        pack_sdcnt(mem, ds_bdos);
        move(mem,mem_seg_addr(ds_bdos, BD_PACKED_DCNT), (uint16_t)(item + 3), 3);
    }
}

/*
 * stamp3 -- shared tail for `stamp1`/`stamp2`: copy the 4-byte
 * time-of-day stamp (`SD_FILE_TOD`'s own day/hour/min bytes -- `sec`
 * is not copied, same as the real asm) into the current directory
 * entry at dptr+cx.
 */
static void stamp3(uint8_t *mem, uint16_t ds_bdos, uint16_t cx) {
    uint16_t bx = (uint16_t)(getdptra(mem, ds_bdos) + cx);
    move(mem, SD_FILE_TOD, bx, 4);
}

/* stamp1 -- STAMP1: set the directory entry's own access timestamp */
void stamp1(uint8_t *mem, uint16_t ds_bdos) {
    stamp3(mem, ds_bdos, 24);
}

/* stamp2 -- STAMP2: set the directory entry's own update timestamp */
void stamp2(uint8_t *mem, uint16_t ds_bdos) {
    stamp3(mem, ds_bdos, 28);
}

/*
 * count_opens -- COUNT_OPENS: count how many entries in the open-file
 * list (BD_OPEN_ROOT) belong to this call's own PD (BD_PDADDR, each
 * olist item's own item+6 field), against the system-wide limit
 * (BD_OPEN_MAX).
 *	output: returns 1 (real carry set, via the real SUB's own
 *		borrow) if this process is still under the limit, else 0
 *		(BD_OPEN_CNT left at the full count either way)
 */
uint8_t count_opens(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_OPEN_CNT, 0);
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_PDADDR);
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_OPEN_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 0)) {
            break;
        }
        uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        if (tst_tbl_lmt(mem, bx, 6, dx)) {
            mem_w8_seg(mem, ds_bdos,BD_OPEN_CNT, (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_OPEN_CNT) + 1));
        }
    }
    return mem_r8_seg(mem, ds_bdos,BD_OPEN_CNT) < mem_r8_seg(mem, ds_bdos,BD_OPEN_MAX);
}

/*
 * create_olist_item -- CREATE_OLIST_ITEM: allocate a fresh olist
 * element for the file currently being opened (right after whatever
 * sorted-order insertion point `search_olist`'s own by-drive search
 * leaves in BD_PRV_POS -- its own found/not-found result is
 * discarded, only that positioning side effect matters here), and
 * record this call's own open-mode attributes into it.
 */
void create_olist_item(uint8_t *mem, uint16_t ds_bdos) {
    (void)search_olist(mem, ds_bdos, 1);
    uint16_t bx;
    (void)create_item(mem, ds_bdos, &bx);
    set_olist_item(mem, ds_bdos, bx, mem_r8_seg(mem, ds_bdos,BD_ATTRIBUTES));
}

/*
 * test_disk_fcb -- TEST_DISK_FCB/TEST_DISK_FCB1: for an unlocked-mode
 * open, re-fetch the directory fcb via a partial close, in case
 * another process has allocated a block to it since this one opened
 * it. The real asm's own `POP SI .../ PUSH SI` pair around this,
 * freeing up the top-of-stack slot, has no other pushes between them
 * in this version and so has no observable effect -- not translated.
 *	output: returns 1 ("retry the read/write from the top") only
 *		when the partial close actually ran and succeeded
 *		(`BD_RCOUNT`/`BD_DONT_CLOSE`/`BD_LRET` updated); returns
 *		0 otherwise (not unlocked, `BD_DONT_CLOSE` already
 *		false, or the partial close itself failed -- `BD_LRET`
 *		set to 11 in that last case)
 */
static uint8_t test_disk_fcb1_tail(uint8_t *mem, uint16_t ds_bdos) {
    if (mem_r8_seg(mem, ds_bdos,BD_LRET) == 0xFF) {
        mem_w8_seg(mem, ds_bdos,BD_LRET, 11); /* close operation unsuccessful */
        return 0;
    }
    mem_w8_seg(mem, ds_bdos,BD_LRET, 0);
    uint16_t bx = get_rcnta(mem, ds_bdos);
    mem_w8_seg(mem, ds_bdos,BD_RCOUNT, mem[bx]);
    mem_w8_seg(mem, ds_bdos,BD_DONT_CLOSE, 0);
    return 1;
}

static uint8_t test_disk_fcb(uint8_t *mem, uint16_t ds_bdos) {
    if (!(mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) & 0x80)) {
        return 0; /* ret14 */
    }
    if (!mem_r8_seg(mem, ds_bdos,BD_DONT_CLOSE)) {
        return 0; /* ret14 */
    }
    close1(mem, ds_bdos);
    return test_disk_fcb1_tail(mem, ds_bdos);
}

/* reset_fwf -- RESET_FWF: reset the fcb's own file-write flag */
static void reset_fwf(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx;
    uint8_t al = getmodnum(mem, ds_bdos, &bx);
    mem[bx] = (uint8_t)(al & (uint8_t)~fwfmsk);
}

/*
 * diskread -- diskread/sqread: read the next sequential record from
 * the currently addressed fcb, rolling over to the next extent
 * (`openreel`) when the current one runs out, and re-fetching the
 * directory fcb (`test_disk_fcb`) if an unlocked-mode file's own
 * block allocation might have changed underneath this process.
 *	output: BD_LRET=1 ("setlret1") at end-of-file; otherwise left
 *		however the real record-read tail (`setfcb`) leaves it
 */
static void diskread(uint8_t *mem, uint16_t ds_bdos) {
    (void)tst_inv_fcb(mem, ds_bdos); /* real asm never checks this result either */
    mem_w8_seg(mem, ds_bdos,BD_RMF, 0xFF);
    mem_w8_seg(mem, ds_bdos,BD_DONT_CLOSE, 1);
    getfcb(mem, ds_bdos);

disk_read0:
    if (mem_r8_seg(mem, ds_bdos,BD_VRECORD) < mem_r8_seg(mem, ds_bdos,BD_RCOUNT)) {
        goto recordok;
    }
    if (test_disk_fcb(mem, ds_bdos)) {
        goto disk_read0;
    }
    if (mem_r8_seg(mem, ds_bdos,BD_VRECORD) != 128) {
        goto diskeof;
    }
    openreel(mem, ds_bdos);
    if (mem_r8_seg(mem, ds_bdos,BD_LRET) != 0) {
        goto diskeof;
    }

recordok:
    bdos_index(mem, ds_bdos);
    if (alloct(mem, ds_bdos) == 0) {
        if (test_disk_fcb(mem, ds_bdos)) {
            goto disk_read0;
        }
        goto diskeof;
    }
    /* recordok1 */
    atran(mem, ds_bdos);
    seek(mem);
    rdbuff(mem, ds_bdos);
    setfcb(mem, ds_bdos);
    return;

diskeof:
    mem_w8_seg(mem, ds_bdos,BD_LRET, 1); /* setlret1 */
}

/* sqread -- sqread: sequential disk read operation */
void sqread(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_SEQIO, 1);
    diskread(mem, ds_bdos);
}

/*
 * move_arecord -- MOVE_ARECORD: copy BD_ARECORD's own 3 bytes (low
 * word + high byte) over BD_PACKED_DCNT, so a lock-list search can
 * match a record number through the same packed-dcnt/pdaddr
 * comparison fields `search_listo` already uses for directory
 * positions.
 */
static void move_arecord(uint8_t *mem, uint16_t ds_bdos) {
    move(mem,mem_seg_addr(ds_bdos, BD_ARECORD),mem_seg_addr(ds_bdos, BD_PACKED_DCNT), 3);
}

/*
 * test_lock -- TEST_LOCK: verify the record `BD_ARECORD` addresses
 * isn't locked by another process. Does not itself touch
 * `BD_LRET`/`BD_ARET` on failure -- the real asm's own `LOCK_ERR`
 * tail (reached via `jmp lock_err` on conflict) computes the error
 * code from register/stack state that varies by call site (a flat
 * `BD_LRET=8` for `diskwrite`'s own single call, a `mult_cnt`-
 * shifted value for `lock_`'s own per-record loop -- see that
 * function's own header comment), so each caller applies its own
 * version on a 0 return instead.
 *	output: returns 1 if not locked, or locked by this same
 *		process (ok to continue); returns 0 if locked by
 *		another process
 */
static uint8_t test_lock(uint8_t *mem, uint16_t ds_bdos) {
    move_arecord(mem, ds_bdos);
    if (!search_llist(mem, ds_bdos, 3)) {
        return 1; /* retl6 -- not locked */
    }
    uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
    if (compare_pds(mem, ds_bdos, item)) {
        return 1; /* retl6 -- locked by this same process */
    }
    return 0;
}

/*
 * diskwrite -- diskwite/sqwrite: write the current record to the
 * currently addressed fcb, rolling over to the next extent
 * (`openreel`) when needed, allocating a new block (`getblock`) if
 * the current one isn't allocated yet, and (same as `diskread`)
 * re-fetching the directory fcb (`test_disk_fcb`) if an unlocked-mode
 * file's own allocation might have changed underneath this process.
 * Also handles `seqio==2` ("random write with zero fill", `FUNC40`'s
 * own entry point): zero-filling and writing out every record in a
 * newly-allocated block before writing the actual target record.
 *
 * The real asm's own read/write-only-error check (`TEST
 * XFCB_READ_ONLY,TRUE` / `TEST HIGH_EXT,40H`) both report the same
 * error code (3, via a shared `JMP SET_ARET` with `AL` set once),
 * so it collapses into one combined condition here. Not `static`:
 * `src-c/kernel/file4.c`'s own `func40` also calls `diskwite`
 * directly (after setting `BD_SEQIO=2` and calling `rseek1` itself),
 * an ordinary cross-file call, same as `que1.c`'s own into `que2.c`.
 */
void diskwrite(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_RMF, 0);
    checkwrite(mem, ds_bdos);

    if (mem_r8_seg(mem, ds_bdos,BD_XFCB_READ_ONLY) || (mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) & 0x40)) {
        set_aret(mem, ds_bdos, 3);
        return;
    }

    ckrofile(mem, ds_bdos, mem_r16_seg(mem, ds_bdos, BD_INFO));
    (void)tst_inv_fcb(mem, ds_bdos); /* real asm never checks this result either */
    getfcb(mem, ds_bdos);

    if (mem_r8_seg(mem, ds_bdos,BD_VRECORD) >= lstrec + 1) {
        openreel(mem, ds_bdos);
        if (mem_r8_seg(mem, ds_bdos,BD_LRET) != 0) {
            return; /* ret14 */
        }
    }
    mem_w8_seg(mem, ds_bdos,BD_DONT_CLOSE, 1);

    uint8_t cl;
disk_write1:
    bdos_index(mem, ds_bdos);
    if (alloct(mem, ds_bdos) != 0) {
        if (mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) & 0x80) {
            atran(mem, ds_bdos);
            if (!test_lock(mem, ds_bdos)) {
                mem_w8_seg(mem, ds_bdos,BD_LRET, 8);
                return;
            }
            mem_w8_seg(mem, ds_bdos,BD_WFLAG, 0);
            goto diskwr10;
        }
        cl = 0;
        goto dskwr1;
    }
    /* disk_write2 -- not yet allocated */
    if (test_disk_fcb(mem, ds_bdos)) {
        goto disk_write1;
    }

    {
        uint8_t al = dmposition(mem, ds_bdos);
        mem_w8_seg(mem, ds_bdos,BD_DMINX, al);
        uint16_t cx = 0;
        if (al != 0) {
            cx = getdm(mem, ds_bdos, (uint8_t)(al - 1));
        }
        uint16_t bx = getblock(mem, ds_bdos, cx);
        if (bx == 0) {
            mem_w8_seg(mem, ds_bdos,BD_LRET, 2);
            return;
        }
        mem_w8_seg(mem, ds_bdos,BD_COMP_FCB_CKS, 1);
        mem_w16_seg(mem, ds_bdos, BD_ARECORD, bx);
        uint16_t dx = bx;
        bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + fcb_dskmap);
        al = mem_r8_seg(mem, ds_bdos,BD_DMINX);
        if (mem_r8_seg(mem, ds_bdos,BD_SINGLE) == 0) {
            bx = (uint16_t)(bx + al + al);
            mem_w16(mem, bx, dx);
        } else {
            bx = (uint16_t)(bx + al);
            mem[bx] = (uint8_t)dx;
        }
        cl = 2;
    }

dskwr1:
    mem_w8_seg(mem, ds_bdos,BD_WFLAG, cl);
    atran(mem, ds_bdos);

diskwr10:
    if (mem_r8_seg(mem, ds_bdos,BD_SEQIO) == 2) {
        /* random write with zero fill (FUNC40) */
        for (int i = 0; i < 128; i++) {
            mem_w8_seg(mem, ds_bdos,(uint16_t)(BD_BUFFA + i), 0);
        }
        /* call setdir -- boundary stub, no-op */
        uint16_t saved_arecord = mem_r16_seg(mem, ds_bdos, BD_ARECORD);
        uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_ABLOCK);
        for (;;) {
            mem_w16_seg(mem, ds_bdos, BD_ARECORD, bx);
            seek(mem);
            wrbufflg(mem, ds_bdos);
            bx = mem_r16_seg(mem, ds_bdos, BD_ARECORD);
            mem_w8_seg(mem, ds_bdos,BD_WFLAG, 0);
            uint8_t al2 = (uint8_t)((uint8_t)bx & mem_r8_seg(mem, ds_bdos,BD_BLKMSK));
            bx = (uint16_t)(bx + 1);
            if (al2 == mem_r8_seg(mem, ds_bdos,BD_BLKMSK)) {
                break;
            }
        }
        mem_w16_seg(mem, ds_bdos, BD_ARECORD, saved_arecord);
        mem_w8_seg(mem, ds_bdos,BD_WFLAG, 2);
        /* call setdata -- boundary stub, no-op */
    }

    seek(mem);
    {
        uint8_t al = mem_r8_seg(mem, ds_bdos,BD_VRECORD);
        if (mem_r8_seg(mem, ds_bdos,BD_BLKMSK) & al) {
            mem_w8_seg(mem, ds_bdos,BD_WFLAG, 0);
        }
    }
    wrbufflg(mem, ds_bdos);

    {
        uint8_t al = mem_r8_seg(mem, ds_bdos,BD_VRECORD);
        if (al < mem_r8_seg(mem, ds_bdos,BD_RCOUNT)) {
            goto dskwr2;
        }
        mem_w8_seg(mem, ds_bdos,BD_RCOUNT, (uint8_t)(al + 1));
        if (mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) & 0x80) {
            uint8_t blkmsk = mem_r8_seg(mem, ds_bdos,BD_BLKMSK);
            al = (uint8_t)(al & (uint8_t)~blkmsk);
            al = (uint8_t)(al + blkmsk + 1);
            mem_w8_seg(mem, ds_bdos,BD_RCOUNT, al);
        }
        mem_w8_seg(mem, ds_bdos,BD_WFLAG, 2);
    }

dskwr2:
    if (mem_r8_seg(mem, ds_bdos,BD_WFLAG) == 2) {
        if (mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) & 0x80) {
            uint8_t al = mem_r8_seg(mem, ds_bdos,BD_RCOUNT);
            uint16_t bx = get_rcnta(mem, ds_bdos);
            mem[bx] = al;
            close(mem, ds_bdos);
            (void)test_disk_fcb1_tail(mem, ds_bdos);
        }
        reset_fwf(mem, ds_bdos);
    }

    {
        uint16_t modbx;
        uint8_t modval = getmodnum(mem, ds_bdos, &modbx);
        if (!(modval & 0x40)) {
            mem[modbx] = (uint8_t)(modval | 0x40);
            reset_fwf(mem, ds_bdos);
        }
    }
    setfcb(mem, ds_bdos);
}

/* sqwrite -- sqwrite: sequential disk write operation */
void sqwrite(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_SEQIO, 1);
    diskwrite(mem, ds_bdos);
}

/*
 * chk_xfcb_password1 -- CHK_XFCB_PASSWORD1: `al` is the xfcb's own
 * password-mode indicator (already known nonzero by every real
 * caller reached through this exact entry point, since each masks
 * it against a relevant bit pattern and branches away first on
 * zero -- `CHK_XFCB_PASSWORD`'s own full entry, not yet ported,
 * reaches this with `al` meaning something slightly different, a
 * decremented "mode==0?" test; this entry's own `al==0` case is kept
 * anyway for whenever that full entry does get ported).
 *	output: returns 1 if ok (no password set, or it matched);
 *		returns 0 on a password error (`set_aret`, never
 *		returns normally, see that function's own header
 *		comment)
 * Not `static`: `src-c/kernel/file4.c`'s own `func22` (BDOS MAKE)
 * also calls `CHK_XFCB_PASSWORD1` directly, an ordinary cross-file
 * call, same as `que1.c`'s own into `que2.c`.
 */
uint8_t chk_xfcb_password1(uint8_t *mem, uint16_t ds_bdos, uint16_t bx, uint8_t al) {
    if (al == 0) {
        return 1; /* ret21 */
    }
    if (cmp_pw(mem, ds_bdos, bx)) {
        return 1; /* ret21 -- password ok */
    }
    set_aret(mem, ds_bdos, 7); /* chk_pw1 -- password error */
    return 0;
}

/*
 * init_xfcb0 -- INIT_XFCB0: initialize a freshly-allocated xfcb from
 * the current fcb: xfcb(0) = fcb(0) | ch, xfcb(1..11) = fcb(1..11),
 * and zero the remaining cl bytes. `move`'s own real DI-after-the-
 * copy side effect (same as `set_olist_item`'s own header comment,
 * `file3.c`) is not modeled -- the zero-fill's own start address is
 * recomputed directly instead.
 */
static void init_xfcb0(uint8_t *mem, uint16_t ds_bdos, uint8_t ch, uint8_t cl) {
    uint16_t bx = getdptra(mem, ds_bdos);
    uint16_t si = mem_r16_seg(mem, ds_bdos, BD_INFO);
    mem[bx] = (uint8_t)(mem[si] | ch);
    si++;
    bx++;
    move(mem, si, bx, 11);
    bx = (uint16_t)(bx + 11);
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(bx + i)] = 0;
    }
}

/* init_xfcb -- INIT_XFCB: initialize a newly-created xfcb */
void init_xfcb(uint8_t *mem, uint16_t ds_bdos) {
    setcdr(mem, ds_bdos); /* may have extended the directory */
    init_xfcb0(mem, ds_bdos, 16, 20);
}

/*
 * set_pw -- SET_PW: copy the user's own supplied 8-byte password
 * (BD_XDMAAD) into the xfcb at bx (an xfcb(12)/password-mode address,
 * same as `get_xfcb_result.bx`), clearing the password mode if it
 * turns out to be the null password (all blanks and/or zeros), then
 * encrypt it in place against the checksum this same pass computes.
 *	output: returns the password checksum (the real `ch`);
 *		*bx_out = bx+1 (the real asm's own final `bx`, its own
 *		xfcb(13) checksum-byte address)
 */
uint8_t set_pw(uint8_t *mem, uint16_t ds_bdos, uint16_t bx, uint16_t *bx_out) {
    uint16_t saved_bx = bx;
    uint16_t si = (uint16_t)(bx + 11);
    uint16_t src = mem_r16_seg(mem, ds_bdos, BD_XDMAAD);
    uint8_t ch = 0;
    uint8_t ah = 0;
    for (uint8_t cl = 8; cl > 0; cl--) {
        uint8_t al = mem[src];
        mem[si] = al;
        if (al != 0 && al != 0x20) {
            ah++;
        }
        ch = (uint8_t)(ch + al);
        src++;
        si--;
    }
    if ((uint8_t)(ah | ch) == 0) {
        mem[saved_bx] = ah; /* ah==0 here -- zero password mode */
    }
    si = (uint16_t)(saved_bx + 4);
    for (uint8_t cl = 8; cl > 0; cl--) {
        mem[si] = (uint8_t)(mem[si] ^ ch);
        si++;
    }
    *bx_out = (uint16_t)(saved_bx + 1);
    return ch;
}

/*
 * diskreset -- DISKRESET: for each disk bit set in BD_INFO, check
 * whether some other process has a file open on it in a removable/
 * read-only-forced way; if so, report "denied" (`BD_ARET=0xFFFF`,
 * the denied-process display itself a boundary-stub no-op, see
 * `file1.c`'s own header comment on `XCRLF`/`XPRINT`) and stop.
 * Otherwise bump `BD_TLOG` with the disks actually reset.
 *	output: returns 1 only when there was nothing open anywhere to
 *		begin with (`BD_OPEN_ROOT==0`) -- `FILE4.BDO`'s own
 *		`func13` takes this as "safe to wipe every disk
 *		vector" (`RESET_ALL`); returns 0 otherwise (success or
 *		error), which `func13` takes as "only clear what
 *		`RESET_37` clears" -- the real asm's own flag state
 *		entering its error path is murkier than this (several
 *		calls happen in between with no explicit flag check),
 *		but reporting an error and then wiping every disk
 *		vector anyway makes no sense, so this picks the
 *		sensible reading over the letter of the one ambiguous
 *		case, same class of judgment call as `selectdisk`'s own
 *		fixed `bx=0`.
 *
 * `diskreset` itself lands clear of the DATA.BDO collision, but it
 * calls into `TEST_VECTOR1`/`SET_CDISK1` (`file1.c`), which don't --
 * see that file's own header comment -- so no new differential test.
 */
uint8_t diskreset(uint8_t *mem, uint16_t ds_bdos) {
    mem_w16_seg(mem, ds_bdos, BD_NTLOG, 0);
    mem_w8_seg(mem, ds_bdos,BD_SET_RO_FLAG, 0);
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_INFO);

    if (mem_r16_seg(mem, ds_bdos, BD_OPEN_ROOT) == 0) {
        return 1; /* retl8 */
    }
    uint8_t saved_curdsk = mem_r8_seg(mem, ds_bdos,BD_CURDSK);
    uint8_t ch = 0;

dskrst1:
    {
        uint8_t carry = (uint8_t)(bx & 1);
        bx = (uint16_t)(bx >> 1);
        if (!carry) {
            goto dskrst2;
        }
    }
    {
        /* dskrst3 -- check disk #ch */
        uint16_t saved_bx = bx;
        mem_w8_seg(mem, ds_bdos,BD_CURDSK, ch);
        uint8_t tlog_bit = (uint8_t)test_vector1(mem_r16_seg(mem, ds_bdos, BD_TLOG), ch);
        uint8_t rlog_bit = (uint8_t)test_vector1(mem_r16_seg(mem, ds_bdos, BD_RLOG), ch);
        uint8_t rodsk_bit = (uint8_t)test_vector1(mem_r16_seg(mem, ds_bdos, BD_RODSK), ch);
        mem_w8_seg(mem, ds_bdos,BD_CHECK_DISK, (uint8_t)(rlog_bit | mem_r8_seg(mem, ds_bdos,BD_SET_RO_FLAG) | rodsk_bit | tlog_bit));
        mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_OPEN_ROOT);

        uint16_t conflict_item = 0;
        uint8_t conflict = 0;
        for (;;) {
            if (!searchn_list(mem, ds_bdos, 1)) {
                break; /* dskrst6 -- no (more) conflicts on this disk */
            }
            uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
            if (!mem_r8_seg(mem, ds_bdos,BD_CHECK_DISK)) {
                /* dskrst5 -- not removable/ro, ok to reset anyway */
                mem_w16_seg(mem, ds_bdos, BD_INFO, remove_drive(mem, ds_bdos, mem_r16_seg(mem, ds_bdos, BD_INFO)));
                break;
            }
            if (compare_pds(mem, ds_bdos, item)) {
                /* dskrst45 -- this process's own open file, not a conflict */
                mem_w16_seg(mem, ds_bdos, BD_NTLOG, set_cdisk1(mem_r8_seg(mem, ds_bdos,BD_CURDSK), mem_r16_seg(mem, ds_bdos, BD_NTLOG)));
                continue; /* dskrst4 */
            }
            conflict_item = item;
            conflict = 1;
            break;
        }
        /* dskrst6 */
        bx = saved_bx;
        if (conflict) {
            mem_w8_seg(mem, ds_bdos,BD_CURDSK, saved_curdsk);
            if (mem_r8_seg(mem, ds_bdos,BD_ERROR_MODE) != 0xFF) {
                uint8_t drv = (uint8_t)(ch + 0x41);
                mem_w8_seg(mem, ds_bdos,BD_DENIEDDRV, drv);
                uint16_t pd = mem_r16(mem, (uint16_t)(conflict_item + 6));
                mem_w8_seg(mem, ds_bdos,BD_DENIEDCNS, (uint8_t)(mem[(uint16_t)(pd + p_cns)] + 0x30));
                move(mem, (uint16_t)(pd + p_name),mem_seg_addr(ds_bdos, BD_DENIEDPRC), 8);
                /* call xprint -- boundary stub, no-op */
            }
            mem_w16_seg(mem, ds_bdos, BD_ARET, 0xFFFF);
            return 0; /* dskrst7 */
        }
    }

dskrst2:
    ch++;
    if (bx != 0) {
        goto dskrst1;
    }
    mem_w8_seg(mem, ds_bdos,BD_CURDSK, saved_curdsk);
    mem_w16_seg(mem, ds_bdos, BD_TLOG, (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_TLOG) | mem_r16_seg(mem, ds_bdos, BD_NTLOG)));
    return 0;
}

/*
 * rseek1 -- RSEEK1: random-access seek, decoding the fcb's own
 * packed 3-byte `ranrec` field into {module#, extent#, record#} and
 * either
 * confirming the currently-addressed extent already matches (same
 * module and extent, or an adjacent extent within the same directory
 * fcb group via `COMPEXT`) or closing it and opening/creating the
 * target extent (`RANCLOSE`).
 *	input: `read_mode` = the real asm's own `cl` (nonzero/true if
 *		this is a read, false if a write -- only matters on the
 *		"target extent doesn't exist yet" path, where a write
 *		may create it via `MAKE` but a read may not)
 *	output: returns 1 on success (`BD_LRET=0`); returns 0 on any
 *		error (`BD_LRET` set to the real asm's own error code --
 *		6/3/4/5 -- and the fcb flagged invalid via
 *		`MAKE_FCB_INV` for every error except the first,
 *		`SEEKERR`'s own direct entry). The real asm's own flags
 *		entering its two callers' (`rdread`/`rdwrite`, not yet
 *		ported) own `JNZ RET19` check are unambiguous on both
 *		the pure-success tail (`XOR AL,AL`) and the direct-
 *		`SEEKERR` tail (reached with `ZF` already clear from the
 *		`ranrec` validity check just above it) but genuinely
 *		incidental on the `RANCLOSE`/`BADSEEK` paths (nothing
 *		between `CALL MAKE_FCB_INV` and the final `RET` touches
 *		flags again) -- this returns the clearly *intended*
 *		boolean throughout instead of chasing that ambiguity,
 *		same class of judgment call as `diskreset`'s own.
 *
 * `rseek1` itself lands clear of the DATA.BDO collision, but
 * `COMPEXT`/`GET_DIR_EXT`/`RESTORE_RC`/`SET_RC` (`file2.c`, all part
 * of the already-colliding `CLOSE` cluster) and `close`/`open`/`make`
 * themselves don't -- so, same reasoning as `func15`'s own entry
 * (`file4.c`), no new differential test for this either.
 */
uint8_t rseek1(uint8_t *mem, uint16_t ds_bdos, uint8_t read_mode) {
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_INFO);
    uint16_t ranrec = (uint16_t)(dx + fcb_ranrec);

    uint8_t byte0 = mem[ranrec];
    uint8_t rec_lo = (uint8_t)(byte0 & 0x7F);
    uint8_t carry = (uint8_t)((byte0 >> 7) & 1);
    uint8_t byte1 = mem[(uint16_t)(ranrec + 1)];
    uint8_t ext_num = (uint8_t)(((byte1 << 1) | carry) & 0x1F);
    uint8_t byte2 = mem[(uint16_t)(ranrec + 2)];
    uint8_t mod_al = (uint8_t)(byte1 & 0xF0);
    mod_al = (uint8_t)(mod_al | byte2);
    mod_al = (uint8_t)((uint8_t)(mod_al << 4) | (uint8_t)(mod_al >> 4));
    uint8_t mod_num = mod_al;

    if (byte2 & 0xFC) {
        mem_w8_seg(mem, ds_bdos,BD_LRET, 6); /* seekerr -- seek past physical eod */
        return 0;
    }
    mem[(uint16_t)(dx + fcb_nxtrec)] = rec_lo;

    if (!chk_inv_fcb(mem, ds_bdos)) {
        goto ranclose; /* fcb already flagged invalid */
    }
    {
        uint16_t modbx = (uint16_t)(dx + fcb_modnum);
        if ((uint8_t)((mod_num - mem[modbx]) & 0x3F) != 0) {
            goto ranclose; /* different module */
        }
    }
    {
        uint16_t extbx = (uint16_t)(dx + fcb_extnum);
        if (mem[extbx] == ext_num) {
            goto seekok1; /* extents already equal */
        }
        if (!compext(mem, ds_bdos, mem[extbx], ext_num)) {
            goto ranclose; /* different dir-fcb group */
        }
        mem[extbx] = ext_num; /* same group -- just bump the extent field */
        get_dir_ext_result r = get_dir_ext(mem, ds_bdos);
        restore_rc(mem, ds_bdos, r.bx);
        set_rc(mem, ds_bdos, r.bx, r.al);
        goto seekok1;
    }

ranclose:
    close(mem, ds_bdos); /* current extent closed */
    if (mem_r8_seg(mem, ds_bdos,BD_LRET) == 0xFF) {
        mem_w8_seg(mem, ds_bdos,BD_COMP_FCB_CKS, 1);
        make_fcb_inv(mem, ds_bdos);
        mem_w8_seg(mem, ds_bdos,BD_LRET, 3); /* cannot close */
        return 0;
    }
    mem_w16_seg(mem, ds_bdos, BD_XDCNT, 0xFFFF); /* reset for MAKE */
    mem_w8_seg(mem, ds_bdos,(uint16_t)(BD_SDCNT + 1), 0xFF);

    {
        uint16_t extbx = (uint16_t)(dx + fcb_extnum);
        mem[extbx] = ext_num;
        uint16_t modbx = (uint16_t)(dx + fcb_modnum);
        mem[modbx] = (uint8_t)((mem[modbx] & 0x40) | mod_num);
    }
    open(mem, ds_bdos);
    if (mem_r8_seg(mem, ds_bdos,BD_LRET) != 0xFF) {
        goto seekok;
    }
    /* cannot open -- is this a read? */
    if (read_mode) {
        mem_w8_seg(mem, ds_bdos,BD_COMP_FCB_CKS, 1);
        make_fcb_inv(mem, ds_bdos);
        mem_w8_seg(mem, ds_bdos,BD_LRET, 4); /* seek to unwritten extent */
        return 0;
    }
    make(mem, ds_bdos); /* write -- make a new extent */
    if (mem_r8_seg(mem, ds_bdos,BD_LRET) == 0xFF) {
        mem_w8_seg(mem, ds_bdos,BD_COMP_FCB_CKS, 1);
        make_fcb_inv(mem, ds_bdos);
        mem_w8_seg(mem, ds_bdos,BD_LRET, 5); /* cannot create new extent -- no dir space */
        return 0;
    }
    fix_olist_item(mem, ds_bdos);

seekok:
    mem_w8_seg(mem, ds_bdos,BD_COMP_FCB_CKS, 1);
seekok1:
    mem_w8_seg(mem, ds_bdos,BD_LRET, 0);
    return 1;
}

/*
 * rseek -- rseek: mark this as a random-access operation
 * (`BD_SEQIO=0`) then run `rseek1`. Not `static`: `FILE4.BDO`'s own
 * `func40` (write random with zero fill) calls `rseek1` directly
 * instead, after setting `BD_SEQIO=2` itself -- calling `rseek`
 * there would stomp that back to 0, same reason `rseek1` is its own
 * function rather than folded into `rseek`.
 */
uint8_t rseek(uint8_t *mem, uint16_t ds_bdos, uint8_t read_mode) {
    mem_w8_seg(mem, ds_bdos,BD_SEQIO, 0);
    return rseek1(mem, ds_bdos, read_mode);
}

/*
 * rdread -- rdread: random disk read, via rseek + diskread. Not
 * `static`: `src-c/kernel/file4.c`'s own `func33` also calls
 * `rdread` directly, an ordinary cross-file call, same as `que1.c`'s
 * own into `que2.c`.
 */
void rdread(uint8_t *mem, uint16_t ds_bdos) {
    if (!rseek(mem, ds_bdos, 1)) {
        return; /* ret19 */
    }
    diskread(mem, ds_bdos);
}

/*
 * rdwrite -- rdwrite: random disk write, via rseek + diskwrite. Not
 * `static`: `src-c/kernel/file4.c`'s own `func34` also calls
 * `rdwrite` directly, an ordinary cross-file call, same as `que1.c`'s
 * own into `que2.c`.
 */
void rdwrite(uint8_t *mem, uint16_t ds_bdos) {
    if (!rseek(mem, ds_bdos, 0)) {
        return; /* ret19 */
    }
    diskwrite(mem, ds_bdos);
}

/*
 * cmperr_result -- `cmperr`'s own output: al/cx pack a 24-bit
 * "mmmm eeee errr rrrr" random-record position (module#*1024 +
 * extent#*128 + record#) the same way `ranrec` itself stores one.
 */
typedef struct {
    uint8_t al;
    uint16_t cx;
} cmperr_result;

/*
 * cmperr -- compute a random-record position from the fcb/directory
 * entry at `base`'s own record-count field (`field_offset` --
 * `fcb_reccnt` for `getfilesize`, `fcb_nxtrec` for `setrandom`),
 * extent#, and module#, packed the same bit layout as `ranrec`
 * itself (shared by `getfilesize`/`setrandom`).
 */
static cmperr_result cmperr(const uint8_t *mem, uint16_t base, uint16_t field_offset) {
    uint8_t rec_byte = mem[(uint16_t)(base + field_offset)];
    uint16_t cx = rec_byte;

    uint8_t ext_byte = mem[(uint16_t)(base + fcb_extnum)];
    uint16_t ax = (uint16_t)((uint16_t)ext_byte << 8); /* al=0 */
    ax = (uint16_t)(ax >> 1); /* shr ax,1 */
    uint8_t ah = (uint8_t)(((ax >> 8) & 0x0F)); /* and ah,0fh */
    ax = (uint16_t)(((uint16_t)ah << 8) | (ax & 0xFF));
    cx = (uint16_t)(cx + ax); /* add cx,ax */

    uint8_t mod_byte = (uint8_t)(mem[(uint16_t)(base + fcb_modnum)] & 0x3F);
    uint16_t mul_result = (uint16_t)(mod_byte * 16); /* mul ah(=16) */
    uint8_t mul_lo = (uint8_t)(mul_result & 0xFF);
    uint8_t mul_hi = (uint8_t)(mul_result >> 8);
    uint16_t sum = (uint16_t)((uint8_t)(cx >> 8) + mul_lo); /* add ch,al */
    cx = (uint16_t)(((sum & 0xFF) << 8) | (cx & 0xFF));
    uint8_t carry_out = (uint8_t)(sum > 0xFF);
    uint8_t al_final = (uint8_t)(mul_hi + carry_out); /* adc al,ah (al=0 first) */

    cmperr_result r;
    r.al = al_final;
    r.cx = cx;
    return r;
}

/*
 * getfilesize -- compute the logical file size for the current fcb
 * (the largest directory extent's own record position, across every
 * extent), leaving it in `fcb_ranrec`. `BD_LRET` is left at whatever
 * the first `SEARCH` call set it to (0xFF if the file doesn't exist
 * at all) if the loop below never runs; 0 once it has.
 *
 * `getfilesize`/`cmperr` themselves land clear of the DATA.BDO
 * collision, but `getfilesize` calls into `file2.c`'s own colliding
 * `search`/`searchn` cluster, so no new differential test for it;
 * `setrandom` below, by contrast, never touches that cluster at all
 * (pure fcb-field arithmetic) and would be a legitimate candidate for
 * one, just not written yet (time, not a technical blocker).
 */
void getfilesize(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t ranrec_addr = get_rra(mem, ds_bdos);
    mem_w16(mem, ranrec_addr, 0);
    mem[(uint16_t)(ranrec_addr + 2)] = 0;

    uint8_t found = search(mem, ds_bdos, fcb_extnum);
    while (found) {
        uint16_t dptr = getdptra(mem, ds_bdos);
        cmperr_result r = cmperr(mem, dptr, fcb_reccnt);
        uint16_t word_val = mem_r16(mem, ranrec_addr);
        uint8_t byte2_val = mem[(uint16_t)(ranrec_addr + 2)];
        uint8_t borrow1 = (uint8_t)(r.cx < word_val);
        int diff = (int)r.al - (int)byte2_val - (int)borrow1;
        if (diff >= 0) {
            /* this directory extent's own position is >= the
             * largest one seen so far -- it becomes the new max */
            mem_w16(mem, ranrec_addr, r.cx);
            mem[(uint16_t)(ranrec_addr + 2)] = r.al;
        }
        found = searchn(mem, ds_bdos);
        mem_w8_seg(mem, ds_bdos,BD_LRET, 0);
    }
}

/*
 * setrandom -- set the current fcb's own `ranrec` field from its
 * `nxtrec`/extent#/module# fields (the reverse of what `getfilesize`
 * reads to compare).
 *	input: base = .fcb(0) (the real asm's own `dx`, via `func36`'s
 *		own `MOV DX,INFO`)
 */
void setrandom(uint8_t *mem, uint16_t base) {
    cmperr_result r = cmperr(mem, base, fcb_nxtrec);
    uint16_t ranrec_addr = (uint16_t)(base + fcb_ranrec);
    mem_w16(mem, ranrec_addr, r.cx);
    mem[(uint16_t)(ranrec_addr + 2)] = r.al;
}

/*
 * incr_rr -- BDOSIF.BDO's own INCR_RR: increment the current fcb's
 * own 3-byte `ranrec` field by 1 (word + carry into the high byte).
 */
static void incr_rr(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = get_rra(mem, ds_bdos);
    uint16_t val = (uint16_t)(mem_r16(mem, bx) + 1);
    mem_w16(mem, bx, val);
    if (val == 0) {
        mem[(uint16_t)(bx + 2)]++;
    }
}

/*
 * save_rr/reset_rr -- BDOSIF.BDO's own SAVE_RR/RESET_RR: save/restore
 * the current fcb's own `ranrec` field to/from `BD_SHELL_RR`, a real
 * DATA.BDO scratch field (confirmed via `--symbols`, not a new
 * scratch choice).
 */
static void save_rr(uint8_t *mem, uint16_t ds_bdos) {
    move(mem, get_rra(mem, ds_bdos),mem_seg_addr(ds_bdos, BD_SHELL_RR), 3);
}

static void reset_rr(uint8_t *mem, uint16_t ds_bdos) {
    move(mem,mem_seg_addr(ds_bdos, BD_SHELL_RR), get_rra(mem, ds_bdos), 3);
}

/*
 * count_locks -- COUNT_LOCKS: count how many lock-list entries
 * belong to the olist item at `BD_FILE_ID` (each lock-list item's
 * own item+8 field, repurposed to hold its owning olist item's
 * address -- see `set_lock`'s own header comment).
 */
static void count_locks(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_LOCK_CNT, 0);
    uint16_t file_id = mem_r16_seg(mem, ds_bdos, BD_FILE_ID);
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_LOCK_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 0)) {
            return;
        }
        uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        if (tst_tbl_lmt(mem, item, 8, file_id)) {
            mem_w8_seg(mem, ds_bdos,BD_LOCK_CNT, (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_LOCK_CNT) + 1));
        }
    }
}

/*
 * check_free -- CHECK_FREE/CHECK_FREE0: verify the olist/llist free
 * list has at least `BD_MULT_CNT` free nodes.
 *	output: returns 1 if so; returns 0 otherwise (`BD_LRET=14`)
 */
static uint8_t check_free(uint8_t *mem, uint16_t ds_bdos) {
    uint8_t dl = mem_r8_seg(mem, ds_bdos,BD_MULT_CNT);
    uint8_t dh = 0;
    mem_w16_seg(mem, ds_bdos, BD_CUR_POS, BD_FREE_ROOT);
    for (;;) {
        if (!searchn_list(mem, ds_bdos, 0)) {
            mem_w8_seg(mem, ds_bdos,BD_LRET, 14);
            return 0;
        }
        dh++;
        if (dh >= dl) {
            return 1; /* retl4 */
        }
    }
}

/*
 * lock_err_apply -- LOCK_ERR: the real asm's own error-reporting
 * tail, reached via a manual stack trick (`POP`/`POP` discarding its
 * own real caller's return address *and* a pushed per-record
 * countdown, landing back at `LOCK`'s own cleanup via a pushed
 * fallback address) from three call sites inside `lock_`'s own loop
 * -- `GET_LOCKADD`'s own two internal checks (`bx=0`/`bx=1`) and
 * `TEST_LOCK`'s own conflict case (`bx=8`). Since this port doesn't
 * model a call stack, `remaining` (the real asm's own popped `ch`,
 * "mult_cnt minus # records already processed") is threaded through
 * explicitly instead -- it's exactly the loop counter's own value at
 * the top of the iteration that failed.
 */
static void lock_err_apply(uint8_t *mem, uint16_t ds_bdos, uint16_t bx, uint8_t remaining) {
    uint8_t processed = (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_MULT_CNT) - remaining);
    uint8_t shifted = (uint8_t)(processed << 4);
    uint8_t bh = (uint8_t)((bx >> 8) | shifted);
    mem_w16_seg(mem, ds_bdos, BD_ARET, (uint16_t)(((uint16_t)bh << 8) | (bx & 0xFF)));
}

/*
 * get_lockadd -- GET_LOCKADD: seek to the record `lock_`'s own loop
 * is currently processing and compute its actual disk address.
 * `BD_LOCK_SP`/`BD_LOCK_SHELL` are not modeled -- confirmed via a
 * full search of src/ that nothing anywhere ever reads either field
 * back, so their own writes here have no observable effect (a
 * stronger case than the usual "undefined artifact" judgment calls
 * elsewhere in this port: this one's provably dead, not just
 * incidental). `rseek`'s own real `cl` (read/write mode) input isn't
 * set by any real caller reaching this point either -- genuinely
 * incidental register content, so a fixed `0` is used, same
 * "undefined artifact" convention as `selectdisk`'s own fixed `bx=0`
 * (`file1.c`).
 *	output: returns 1 on success (`BD_ARECORD` now the record's own
 *		actual disk address); returns 0 on error (`remaining`
 *		already applied via `lock_err_apply`)
 */
static uint8_t get_lockadd(uint8_t *mem, uint16_t ds_bdos, uint8_t remaining) {
    uint8_t seek_ok = rseek(mem, ds_bdos, 0);
    getfcb(mem, ds_bdos);
    if (!seek_ok) {
        lock_err_apply(mem, ds_bdos, 0, remaining);
        return 0;
    }
    bdos_index(mem, ds_bdos);
    if (alloct(mem, ds_bdos) == 0) {
        lock_err_apply(mem, ds_bdos, 1, remaining);
        return 0;
    }
    atran(mem, ds_bdos);
    return 1;
}

/*
 * free_lock -- FREE_LOCK: remove every lock-list entry for the
 * record `BD_ARECORD` addresses.
 */
static void free_lock(uint8_t *mem, uint16_t ds_bdos) {
    move_arecord(mem, ds_bdos);
    if (!search_llist(mem, ds_bdos, 5)) {
        return; /* retl7 */
    }
    for (;;) {
        uint16_t item = mem_r16_seg(mem, ds_bdos, BD_CUR_POS);
        delete_item(mem, ds_bdos, item);
        if (!searchn_list(mem, ds_bdos, 5)) {
            return;
        }
    }
}

/*
 * set_lock -- SET_LOCK: add a lock-list entry for the record
 * `BD_ARECORD` addresses, owned by `BD_FILE_ID`'s own olist item.
 * `set_olist_item`'s own real `rep movs` DI-after-the-copy side
 * effect (same as that function's own header comment) is why the
 * real asm can just `MOV [DI],DX` the owning item's address
 * afterward -- not modeled here either, the address is recomputed
 * directly as `bx+8` instead, overwriting the zero `set_olist_item`
 * itself just wrote there.
 */
static void set_lock(uint8_t *mem, uint16_t ds_bdos) {
    move_arecord(mem, ds_bdos);
    (void)search_llist(mem, ds_bdos, 1); /* position the insertion point */
    uint16_t bx;
    (void)create_item(mem, ds_bdos, &bx);
    set_olist_item(mem, ds_bdos, bx, 0);
    mem_w16(mem, (uint16_t)(bx + 8), mem_r16_seg(mem, ds_bdos, BD_FILE_ID));
}

/*
 * lock_ -- LOCK: lock or unlock `BD_MULT_CNT` consecutive records of
 * the currently addressed (unlocked-mode-open) file, starting at its
 * own `ranrec` field -- first verifying none of them are already
 * locked by another process (locking only), then actually setting/
 * clearing each lock-list entry. `BD_FILE_ID` (the olist item this
 * open file's own `ranrec` field pointed at, per `OPENX08`'s own
 * header comment, `file4.c`) is recovered via the user's own dma
 * buffer (`BD_XDMAAD`), which the real asm expects to hold that
 * item's own address.
 *	input: `lock_unlock` = `BD_LOCK_UNLOCK`'s own real meaning
 *		(true to lock, false to unlock) -- `func42`/`func43`
 *		(`file4.c`) set `BD_LOCK_UNLOCK` before calling this
 *
 * `lock_`/`LOCK_ERR`'s own stack trick deserves its own callout: a
 * full search of `src/` turned up that `LOCK_PERR` (which restores
 * the real SP from `BD_LOCK_SP`) has no caller anywhere -- it's
 * provably dead. Every real path into `LOCK_ERR` goes through the
 * ordinary `pop dx ! pop cx` discard instead, against a stack shape
 * `LOCK`'s own per-record loop deliberately sets up (one pushed
 * countdown word, plus one pushed fallback address that absorbs the
 * final `ret` on *both* the error and success paths -- harmless on
 * success, since it just means `reset_rr` effectively runs twice).
 * None of that needed modeling here: `remaining` (the loop counter)
 * stands in for the popped `ch` directly.
 *
 * `lock_`/`count_locks`/`check_free`/`get_lockadd`/`free_lock`/
 * `set_lock` all land clear of the DATA.BDO collision, but
 * `incr_rr`/`save_rr`/`reset_rr` (BDOSIF.BDO's own, this file's own
 * ported subset) don't, so no new differential test for this
 * cluster either.
 */
void lock_(uint8_t *mem, uint16_t ds_bdos, uint8_t lock_unlock) {
    copy_dma_in(mem, ds_bdos, 2);
    parsave36r(mem, ds_bdos);
    if (!check_fcb(mem, ds_bdos)) {
        return;
    }
    if (!(mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) & 0x80)) {
        return; /* retl4 -- only unlocked-mode opens can be locked */
    }

    uint16_t file_id = mem_r16_seg(mem, ds_bdos, mem_r16_seg(mem, ds_bdos, BD_XDMAAD));
    uint8_t al = (uint8_t)((mem_r8_seg(mem, ds_bdos,BD_CURDSK) - mem[(uint16_t)(file_id + 2)]) & 0x0F);
    if (al != 0) {
        mem_w8_seg(mem, ds_bdos,BD_LRET, 13); /* lock8 */
        return;
    }
    mem_w16_seg(mem, ds_bdos, BD_FILE_ID, file_id);

    if (lock_unlock) {
        count_locks(mem, ds_bdos);
        uint16_t total = (uint16_t)(mem_r8_seg(mem, ds_bdos,BD_LOCK_CNT) + mem_r8_seg(mem, ds_bdos,BD_MULT_CNT));
        if (total > 0xFF || mem_r8_seg(mem, ds_bdos,BD_LOCK_MAX) < (uint8_t)total) {
            mem_w8_seg(mem, ds_bdos,BD_LRET, 12); /* lock0 -- too many locks */
            return;
        }
        if (!check_free(mem, ds_bdos)) {
            return;
        }
    }

    save_rr(mem, ds_bdos);
    uint8_t mult_cnt = mem_r8_seg(mem, ds_bdos,BD_MULT_CNT);
    uint8_t remaining;
    for (remaining = mult_cnt; remaining > 0; remaining--) {
        if (!get_lockadd(mem, ds_bdos, remaining)) {
            reset_rr(mem, ds_bdos);
            return;
        }
        if (lock_unlock) {
            if (!test_lock(mem, ds_bdos)) {
                lock_err_apply(mem, ds_bdos, 8, remaining);
                reset_rr(mem, ds_bdos);
                return;
            }
        }
        incr_rr(mem, ds_bdos);
    }

    reset_rr(mem, ds_bdos);
    for (remaining = mult_cnt; remaining > 0; remaining--) {
        if (!get_lockadd(mem, ds_bdos, remaining)) {
            reset_rr(mem, ds_bdos);
            return;
        }
        if (lock_unlock) {
            set_lock(mem, ds_bdos);
        } else {
            free_lock(mem, ds_bdos);
        }
        incr_rr(mem, ds_bdos);
    }
    reset_rr(mem, ds_bdos);
}
