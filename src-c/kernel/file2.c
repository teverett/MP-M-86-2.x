/*
 * file2.c -- C port of a self-contained subset of src/kernel/
 * FILE2.BDO: GET_NALBS, getblock (+ internal lefttst/righttst/rblok/
 * rblok0), SEARCH/SEARCHN (`search`/`search1`/`searchn`, + internal
 * `searchi`/`searchi1`/`compext`/`swap_sdcnt`), CLOSE/CLOSE1
 * (`close`/`close1`/`close_fcb`/`seekcopy`, + internal `get_dir_ext`/
 * `restore_rc`/`set_rc`/`merge_word`/`merge_byte`), `initialize`
 * (+ internal `scndm`/`test_dir`), `MAKE` (`make`, + internal
 * `set_dcnt_dblk`/`copy_dir0`/`copy_dir2`), `DELETE` (`delet`, +
 * internal `cpydirloc`/`init_xfcb_search`/`init_xfcb_search1`/
 * `chk_wild`/`does_xfcb_exist`/`delete1_entry`/`delete2_entry`), and
 * now `RENAME` (`rename_`, + internal `check_wild`/`check_wild0`/
 * `copy_user_no`/`copy_dir`), `OPEN`/`OPEN1`/`opencopy`, `openreel`
 * (+ internal `sdcnt_eq_xdcnt`), and `indicators` (`FILE4.BDO`'s own
 * `func30`).
 *
 * FILE2.BDO is not self-contained (same situation as FILE1.BDO/
 * FILE3.BDO -- see their own header comments). This is a scoped pass,
 * not a full file2 port: 30+ remaining labels aren't ported yet.
 *
 * `open`/`OPEN1`/`opencopy` land at 0xF34-0xF3D in the combined
 * wrapper bundle -- inside DATA.BDO's own dseg range (0x000-0x1000),
 * same class of real-address collision as `searchn`/`CLOSE`/
 * `copy_dir0`-`2` above, so, like those, this port has not written a
 * differential test for them. `FILE4.BDO`'s own `func15` (the real
 * BDOS OPEN entry point, ported in `file4.c`) calls `open` as part of
 * its own chain, so `func15` itself inherits the same untestable
 * status even though `func15`'s own real address and most of its own
 * dependencies (`reselect`, `OPENX`, `COUNT_OPENS`, etc., all in
 * `file3.c`/`file4.c`) are individually clear of the collision.
 *
 * `make` was the first of `DELETE`/`RENAME`/`OPEN`/`MAKE` found to be
 * genuinely self-contained: it only needs `search`/`searchn`/
 * `setcdr`/`checkwrite`/`setfwf` (all already ported) plus its own
 * small internal helpers. `delet` needed one more pass: its own
 * `CHK_PWS`/`TST_OLIST`/`CHK_OLIST` dependencies (`FILE3.BDO`) are
 * ported now too (see that file's own header comment for the
 * password-FCB cluster). `delet`'s own single-pass ("one well-
 * defined, non-wild match") branch does `jmps delet0` straight after
 * `ckrodir`, relying on `ckrodir`'s own real ZF there being an
 * incidental side effect of `RO_TEST`'s own `ADD BX,ROFILE` (always
 * leaves ZF clear in practice) rather than a real "was anything
 * found" check -- not modeled as a flag at all, this port just
 * always takes that branch's own fixed destination directly (see
 * `delet`'s own header comment for the full writeup).
 *
 * `delet`'s own `DELETE1` label turned out to need splitting into its
 * own real function (`delete1_entry`, + its own `DELETE2` tail,
 * `delete2_entry`): the real asm's `rename_` own `CALL DOES_XFCB_EXIST
 * ! JNZ $+5 ! CALL DELETE1` calls straight into the *middle* of
 * `DELETE`'s own body as an ordinary subroutine (it eventually
 * returns normally, via `delete2_entry`'s own `cpydirloc`/`staret`
 * tail, to whichever `CALL` reached it -- `delet`'s own, or
 * `rename_`'s own). `copy_dir` itself (the bare `DH=0x80` prologue
 * `COPY_DIR0` falls through from, needed for real now that `rename_`
 * calls it -- `MOV CL,DSKMAP ! MOV DL,EXTNUM` reuses those 2
 * constants' own *numeric* values, 16 and 12, as a plain "start
 * offset" and "length" rather than their usual field-offset meaning,
 * same trick the real asm plays) and `CHECK_WILD`/`CHECK_WILD0` (own
 * callers of `chk_wild`, the opposite sense from `delet`'s own
 * wildcard check -- `RENAME` errors out *if* one is found, `DELETE`
 * wants one) are both ported now too.
 *
 * `OPEN`'s real entry point is `FILE4.BDO`'s own `func15` (not this
 * file's own `open:`, which `func15`'s own chain falls into partway
 * through) -- see that function's own header comment, `file4.c`, for
 * `CHECK_FCB1`'s push/pop fake-return-address convention replicated
 * by hand there.
 *
 * `initialize` was one of this file's own two remaining blockers for
 * `DELETE`/`RENAME`/`OPEN`/`MAKE` (the other, `CURSELECT`, is ported
 * in `src-c/kernel/file3.c`'s own same pass -- its own real tail
 * calls `initialize`, but that call is itself unreachable, see
 * `curselect`'s own header comment there). `scndm`'s own stack-based
 * "carry the 0/1 bit across loop iterations via push/pop instead of a
 * register" trick (the register it would otherwise use, cx, gets
 * reused inside the loop body for the disk-map block number) has no
 * C-level equivalent needed -- a plain local variable does the same
 * job. `test_dir`'s own real asm shares one `ret` between two
 * different callers (its own internal loop-exit jumps, and
 * `initialize`'s own early-return test right after calling it) --
 * ordinary code reuse around a single instruction, not a stack
 * trick, so this returns a plain boolean instead (1 = a flush
 * happened on this drive and it needs relogging for real, 0 = it
 * doesn't) for `initialize` to branch on normally.
 *
 * `CLOSE1` (+ its own `close_fcb`/`CLOSE2` tail, `GET_DIR_EXT`/
 * `RESTORE_RC`/`SET_RC`/`seekcopy`) was `FILE1.BDO`'s own long-
 * standing blocker for `CHECK_FCB` -- deferred only because it called
 * `srch`, now ported above. `GET_DIR_EXT`/`RESTORE_RC` were already
 * identified as clean, self-contained FCB arithmetic in an earlier
 * pass (same shape as `file1.c`'s own `getfcb`/`setfcb`); this pass
 * ports them for real along with the rest of the cluster. `close`
 * itself (the real F_CLOSE BDOS function, `CLOSE1`'s own write-
 * protection/file-write-flag-checking caller) is ported too, since
 * it's a 2-check wrapper with nothing else deferred in its way.
 * `GET_DIR_EXT`'s own real asm comment ("CALL GTFCBA ;BX =
 * .FCB(VRECORD)") is wrong -- `gtfcba`'s real output is `.fcb(nxtrec)`
 * (confirmed against `file1.c`'s own `gtfcba`/`getfcb`), which is
 * what `get_dir_ext` actually starts its backward disk-map scan from,
 * matching the real asm's own behavior, not its own stale comment.
 *
 * `search`/`search1`/`searchn` (`srch`/`srchn`'s own real asm labels)
 * were themselves deferred in an earlier pass only because `rddir`
 * was -- `rddir`'s own real segment-relative-addressing gap is
 * resolved now (see `file1.c`'s/`file3.c`'s own header comments), so
 * this pass ports the whole cluster: the XFCB (extended-FCB) special
 * case (`FIND_XFCB`/`SAVE_XFCB`/`srchnext`'s own AND-mask probe), the
 * per-character match loop (`srchloop`/`srchext`/`srchok`/
 * `SEARCHNM`), and the "user 0" fallback pass (`USER_ZERO_PASS`/
 * `swap_sdcnt`, MPM-only -- this build is always MPM, see
 * docs/src-c.md's own conventions section). `DOES_XFCB_EXIST`/
 * `INIT_XFCB_SEARCH`/`INIT_XFCB_SEARCH1` are NOT part of this cluster
 * -- they're separate entry points other, still-unported routines
 * (OPEN/MAKE) call to *set* `FIND_XFCB`/`XDCNT` before searching;
 * `searchn` only ever *reads* those fields, so they're deferred along
 * with their own real callers.
 *
 * `srchfin`'s own `jmp staret` (with `al=255`) sets `BD_LRET=255` for
 * real -- same inlined-effect convention `file1.c`'s own `tst_inv_fcb`
 * uses, not a boundary stub (`staret`'s own real body, `mov lret,al !
 * ret`, is trivially self-contained, see `src/kernel/BDOSIF.BDO`).
 *
 * `search`/`search1`/`searchn` return the real `ch` register's own
 * final value (0 on an unsuccessful search, matching the comment at
 * `LRET_EQ_FF`; nonzero -- `(ldcnt&dskmsk)+1`, so 1-4 -- on success),
 * the same "return whatever ZF reflects" convention `file1.c`'s own
 * `endofdir`/`compcdr` already use, so a caller can test it exactly
 * like the real `jz`/`jnz` would. `compext` is internal here too
 * (only `searchn` calls it) and returns the opposite sense (1 if the
 * extents *match*, i.e. ZF set after its own final `and`) for the
 * same reason.
 *
 * Translated faithfully but NOT exercised by a differential test --
 * a first for this project, worth spelling out why. `searchn`'s own
 * real cseg footprint, in the combined `tests/src-c/fixtures/
 * file1_wrapper.A86`/`file2_wrapper.A86` bundle (FILE1-4.BDO +
 * BDOSIF.BDO + DATA.BDO, the same one `src/kernel/BDOS.A86` itself
 * assembles), runs from `searchn` (0xbfe) well past where DATA.BDO's
 * own dseg-relative field offsets start (0xc00) -- confirmed via
 * `--symbols`: `SRCHN1`, one of `searchn`'s own internal branch
 * targets, resolves to 0xc11, and `BD_INFO`'s own real address
 * (0x0c17) sits a few bytes further into that same instruction
 * stream. On real hardware this is fine (cseg and dseg are different
 * physical segments, so the numeric offsets never actually collide),
 * but `tools/i86sim.py` has no segment model -- it flattens both into
 * one 64KB array at these same raw offsets, same gap docs/src-c.md's
 * own "Test-harness memory addresses aren't the real hardware
 * offsets" section describes. Every earlier file hitting that gap
 * (SD_RLR et al) could be fixed by moving the *data* to a different,
 * harness-chosen address while leaving the code alone. Here the
 * *code* (`searchn`'s own body) and the *data* it has to read/write
 * (`BD_INFO`/`BD_SRCHA`/`BD_SRCHL`/`BD_DIRLOC`/`BD_FIND_XFCB`/
 * `BD_SAVE_XFCB`/`BD_USER_ZERO_PASS`/...) are the two things that
 * collide, in the one real combined module this project's own
 * FILE1-4.BDO tests already commit to using real, unrelocated
 * addresses for (unlike every other Tier-1+ wrapper, which declares
 * its own scratch DSEG) -- confirmed directly: writing a test FCB
 * pointer into `BD_INFO` overwrites `searchn`'s own real instruction
 * bytes at that exact flat address, and the reference run promptly
 * starts executing its own operand data as code. Relocating just
 * these fields to safe scratch addresses would desync the wrapper
 * from this file's own real BD_* constants (shared with the rest of
 * FILE1-4.BDO, including file1.c's own already-passing tests);
 * shrinking the wrapper to exclude whatever in FILE2-4.BDO pushes
 * `searchn` past 0xc00 isn't straightforward either, since FILE1.BDO
 * itself references FILE3.BDO symbols (`FLUSH_FILE0`/`FLUSH_FILES`)
 * just to assemble, even on paths this port's own test would never
 * take. Fixing this for real needs a differently-shaped wrapper, not
 * a differently-shaped test; left for a future pass.
 *
 * `close`/`close1`/`close_fcb`/`seekcopy`/`get_dir_ext`/`restore_rc`/
 * `set_rc` land in the exact same bucket, confirmed the same way
 * (`--symbols` against `file2_wrapper.A86`): `close` resolves to
 * 0xfa8, `CLOSE1` to 0xfbf, `CLOSE_FCB` to 0xfe3, `CLOSE2` to 0x1004,
 * the merge loop's own labels (`merge0`/`merged`/`dmset`/`endmerge`/
 * `mergerr`) from 0x1023 to 0x10b7 -- all deep inside the same
 * DATA.BDO dseg-field range as `searchn`, for the same reason
 * (FILE2.BDO's own code, this far into the file, has simply
 * accumulated past offset 0xc00 by this point). No new test added
 * for the same root cause as above.
 */
#include <stdint.h>

#include "defs.h"

extern uint8_t getallocbit(const uint8_t *mem, uint16_t cx, uint16_t *bx_out, uint8_t *dh_out);
extern void home(uint8_t *mem);
extern void setenddir(uint8_t *mem);
extern void rddir(uint8_t *mem, uint8_t cl);
extern uint8_t endofdir(const uint8_t *mem);
extern uint8_t compcdr(const uint8_t *mem);
extern uint16_t getdptra(const uint8_t *mem);
extern uint16_t getexta(const uint8_t *mem);
extern uint8_t nowrite(const uint8_t *mem);
extern uint8_t getmodnum(const uint8_t *mem, uint16_t *bx_out);
extern uint8_t setfwf(uint8_t *mem);
extern void make_fcb_inv(uint8_t *mem);
extern void seekdir(uint8_t *mem);
extern void wrdir(uint8_t *mem);
extern void setcdr(uint8_t *mem);
extern uint16_t test_vector(const uint8_t *mem, uint16_t dx);
extern uint16_t remove_drive(const uint8_t *mem, uint16_t bx);
extern uint8_t search_olist(uint8_t *mem, uint8_t ch);
extern void setallocbit(uint8_t *mem, uint16_t cx, uint8_t dl);
extern void checkwrite(uint8_t *mem);
extern uint8_t get_atts(uint8_t *mem);
extern void ckrodir(uint8_t *mem);
extern void tst_olist(uint8_t *mem);
extern void chk_olist(uint8_t *mem);
extern uint8_t chk_pws(uint8_t *mem);
extern void set_aret(uint8_t *mem, uint8_t al);
extern void file_exists(uint8_t *mem);
extern uint8_t chk_password(uint8_t *mem);
extern void move(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl);
extern void getfcb(uint8_t *mem);
extern void fix_olist_item(uint8_t *mem);

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

/*
 * swap_sdcnt -- SWAP: exchange SDCNT/SDBLK with SDCNT0/SDBLK0 (the
 * "user 0" extra pass's own saved-vs-current directory position).
 * Not `static`: `src-c/kernel/file4.c`'s own `func15` (BDOS OPEN)
 * also calls `SWAP` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`. `SWAP` itself lands at 0xD1D in the
 * combined wrapper bundle -- inside DATA.BDO's own dseg collision
 * range, see `open`'s own header comment above -- so this export
 * doesn't come with its own new differential test either.
 */
void swap_sdcnt(uint8_t *mem) {
    uint16_t a = mem_r16(mem, BD_SDCNT);
    uint16_t b = mem_r16(mem, BD_SDCNT0);
    mem_w16(mem, BD_SDCNT, b);
    mem_w16(mem, BD_SDCNT0, a);
    a = mem_r16(mem, BD_SDBLK);
    b = mem_r16(mem, BD_SDBLK0);
    mem_w16(mem, BD_SDBLK, b);
    mem_w16(mem, BD_SDBLK0, a);
}

/*
 * compext -- compare the extent# in al with that in cl, each first
 * masked by ~extmsk.
 *	output: returns 1 if they match (ZF set after the real asm's
 *		own final `and al,maxext`), 0 if not -- see this file's
 *		own header comment for why this is the opposite sense
 *		of the real asm's own comment ("return nonzero if they
 *		do not match", describing AL, not ZF)
 * Not `static`: `src-c/kernel/file3.c`'s own `rseek` also calls
 * `COMPEXT` directly, an ordinary cross-file call, same as `que1.c`'s
 * own into `que2.c`.
 */
uint8_t compext(const uint8_t *mem, uint8_t al, uint8_t cl) {
    uint8_t ch = (uint8_t)~mem[BD_EXTMSK];
    cl = (uint8_t)(cl & ch);
    al = (uint8_t)(al & ch);
    al = (uint8_t)(al - cl);
    al = (uint8_t)(al & maxext);
    return al == 0;
}

/*
 * save_dcnt_pos/save_dcnt_pos1 -- SAVE_DCNT_POS/SAVE_DCNT_POS1: save
 * the current dcnt/dblk into xdcnt/xdblk (save_dcnt_pos1 only does so
 * if xdcnt's own high byte is still the 0xFF "unset" sentinel)
 */
static void save_dcnt_pos(uint8_t *mem) {
    mem_w16(mem, BD_XDCNT, mem_r16(mem, BD_DCNT));
    mem_w16(mem, BD_XDBLK, mem_r16(mem, BD_DBLK));
}

static void save_dcnt_pos1(uint8_t *mem) {
    if (mem[(uint16_t)(BD_XDCNT + 1)] == 0xFF) {
        save_dcnt_pos(mem);
    }
}

/*
 * searchi/searchi1 -- SEARCH INITIALIZATION. `searchi1` isn't static:
 * `src-c/kernel/file3.c`'s own `does_xfcb_exist` calls it directly
 * too (reusing the existing BD_SRCHA pointer rather than resetting
 * it from BD_INFO, same as the real asm does), an ordinary cross-
 * file call, same as `que1.c`'s own into `que2.c`.
 */
void searchi1(uint8_t *mem, uint8_t cl) {
    mem[BD_DIRLOC] = 0xFF;
    mem[BD_SRCHL] = cl;
}

/*
 * searchi -- SEARCHI: like `searchi1`, but also resets BD_SRCHA from
 * BD_INFO first. Not `static`: `src-c/kernel/file4.c`'s own `func15`
 * (BDOS OPEN) also calls `SEARCHI` directly, an ordinary cross-file
 * call, same as `que1.c`'s own into `que2.c`.
 */
void searchi(uint8_t *mem, uint8_t cl) {
    mem_w16(mem, BD_SRCHA, mem_r16(mem, BD_INFO));
    searchi1(mem, cl);
}

/*
 * searchn -- SEARCHN/srchn: search for the next directory element,
 * assuming a previous call on `search` set up BD_SRCHA/BD_SRCHL.
 *	output: returns the real `ch` register's own final value -- see
 *		this file's own header comment for what it means
 */
uint8_t searchn(uint8_t *mem) {
    uint16_t dx, bx;
    uint8_t cl, ch, al;

top:
    if (mem[BD_USER_ZERO_PASS]) {
        mem[BD_USER_ZERO_PASS] = 0;
        swap_sdcnt(mem);
    }
    rddir(mem, 0);
    if (endofdir(mem)) {
        goto srchfin;
    }
    dx = mem_r16(mem, BD_SRCHA); /* mov dx,srcha ! mov si,dx -- dx itself never
                                  * changes again until srchok's own "inc dx",
                                  * no matter which path below is taken */
    al = mem[dx]; /* lods al -- si advances, dx doesn't */
    if (al == bdos_empty) {
        goto srchnext;
    }
    if (!compcdr(mem)) { /* jnb srchfin -- carry (cdrmax>dcnt) clear */
        goto srchfin;
    }

srchnext:
    bx = getdptra(mem);
    cl = mem[BD_SRCHL];
    ch = 0;
    if (mem[bx] == bdos_empty) {
        save_dcnt_pos1(mem);
        mem[BD_SAVE_XFCB] = 0;
    }
    al = mem[bx]; /* is FCB an XFCB? */
    {
        uint8_t masked = (uint8_t)(al & 0xEF);
        if (masked != al) {
            /* it IS an xfcb (bit 4 was set) -- check the user# match
             * ("mov si,dx" -- si==dx, already BD_SRCHA from above) */
            if (masked == mem[dx]) {
                if (mem[BD_FIND_XFCB]) {
                    mem[BD_SAVE_XFCB] = 0xFF;
                    goto srchok; /* JMPS SRCHOK -- bx/cl/ch/dx are all
                                  * still exactly at their srchnext-
                                  * entry values here, same as the
                                  * real asm's own registers */
                }
                goto top; /* jz SEARCHN */
            }
        }
    }

srchloop:
    if (cl == 0) {
        goto endsearch;
    }
    al = mem[dx]; /* mov si,dx ! lods al */
    if (al == '?') {
        goto srchok;
    }
    if (ch == fcb_ubytes) {
        goto srchok;
    }
    if (ch == fcb_extnum) {
        goto srchext;
    }
    if (ch == fcb_modnum) {
        al = (uint8_t)(al & 0x3F);
    }
    al = (uint8_t)(al - mem[bx]);
    al = (uint8_t)(al & 0x7F);
    if (al != 0) {
        goto searchnm;
    }
    goto srchok;

srchext:
    if (mem[(uint16_t)(BD_SDCNT + 1)] == 0xFF) {
        mem_w16(mem, BD_SDCNT, mem_r16(mem, BD_DCNT));
        mem_w16(mem, BD_SDBLK, mem_r16(mem, BD_DBLK));
    }
    {
        uint8_t match = compext(mem, al, mem[bx]);
        if (!mem[BD_USER_ZERO_PASS]) {
            mem[BD_SEARCH_USER0] = 0;
            mem[BD_FCB_EXISTS] = 0xFF;
            if (!match) {
                goto top; /* jnz SEARCHN_JMP */
            }
            goto srchok;
        }
        /* SAVE_DCNT_POS2 */
        if (!match) {
            goto top;
        }
        bx = (uint16_t)(bx + 1);
        if (mem[bx] != 0) {
            goto top;
        }
        save_dcnt_pos(mem);
        goto top;
    }

searchnm:
    if (ch != 0) {
        goto top;
    }
    if (mem[bx] != 0) {
        goto top;
    }
    if (!mem[BD_SEARCH_USER0]) {
        goto top;
    }
    mem[BD_USER_ZERO_PASS] = 0xFF;
    swap_sdcnt(mem);
    goto srchok;

srchok:
    dx = (uint16_t)(dx + 1);
    bx = (uint16_t)(bx + 1);
    ch = (uint8_t)(ch + 1);
    cl = (uint8_t)(cl - 1);
    goto srchloop;

endsearch:
    if (mem[BD_SAVE_XFCB]) {
        if (mem[(uint16_t)(BD_XDCNT + 1)] != 0xFE) {
            goto top;
        }
        save_dcnt_pos(mem);
    }
    /* ENDSEARCH1 */
    mem[BD_DIRLOC] = 0;
    al = (uint8_t)(mem[BD_DCNT] & dskmsk); /* ldcnt */
    mem[BD_LRET] = al;
    return (uint8_t)(al + 1);

srchfin:
    save_dcnt_pos1(mem);
    setenddir(mem);
    mem[BD_LRET] = 255;
    return 0;
}

/* search/search1 -- SEARCH/srch and its own RENAME-only entry point */
uint8_t search1(uint8_t *mem) {
    setenddir(mem);
    home(mem);
    return searchn(mem);
}

uint8_t search(uint8_t *mem, uint8_t cl) {
    searchi(mem, cl);
    return search1(mem);
}

/*
 * get_dir_ext -- GET_DIR_EXT: scan the fcb's own disk map backwards
 * for the last non-zero block entry, and compute the directory
 * extent that entry belongs to.
 *	output: see `get_dir_ext_result`'s own comment, `defs.h`
 * Not `static`: `src-c/kernel/file3.c`'s own `rseek` also calls
 * `GET_DIR_EXT` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`.
 */
get_dir_ext_result get_dir_ext(uint8_t *mem) {
    /* call gtfcba -- the real asm's own comment ("bx = .fcb(vrecord)")
     * is wrong, gtfcba's own real output is .fcb(nxtrec); that's what
     * this starts from regardless of the comment */
    uint16_t bx = (uint16_t)(mem_r16(mem, BD_INFO) + fcb_nxtrec);
    uint8_t dh = 0x10;
    uint8_t dl = 0x00;
get_de1:
    dh = (uint8_t)(dh - 1);
    bx = (uint16_t)(bx - 1);
    if (mem[bx] == 0) {
        if (dh != 0) {
            goto get_de1;
        }
        dl = (uint8_t)(dl - 1);
    }
    uint8_t al = dh;
    if (mem[BD_SINGLE] == 0) {
        al = (uint8_t)(al >> 1);
    }
    {
        uint8_t cl = (uint8_t)(7 - mem[BD_BLKSHF]);
        al = (uint8_t)(al >> cl); /* ah was 0 */
        uint8_t ah = mem[BD_EXTMSK];
        if (ah < al) {
            goto get_de1;
        }
        uint16_t bx2 = getexta(mem);
        uint8_t fcb_ext = mem[bx2];
        ah = (uint8_t)(~ah & maxext);
        ah = (uint8_t)(ah & fcb_ext);
        al = (uint8_t)(al | ah);
    }
    get_dir_ext_result r;
    r.bx = bx;
    r.al = al;
    r.dl = dl;
    return r;
}

/*
 * restore_rc -- RESTORE_RC: if actual_rc (SET_RC's own saved value) is
 * non-zero, restore it into fcb(rc)
 *	input: bx = .fcb(extnum)
 * Not `static`: `src-c/kernel/file3.c`'s own `rseek` also calls
 * `RESTORE_RC` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`.
 */
void restore_rc(uint8_t *mem, uint16_t bx) {
    uint8_t al = mem[BD_ACTUAL_RC];
    if (al == 0) {
        return;
    }
    uint16_t si = (uint16_t)(bx + fcb_reccnt - fcb_extnum);
    mem[si] = al;
}

/*
 * set_rc -- SET_RC: compare the fcb's own extent (at bx) against the
 * directory extent (cl) CLOSE_FCB just found; if they differ, force
 * fcb(rc) to 0 (fcb ext > dir ext) or 128 (fcb ext < dir ext, saving
 * the old fcb(rc) into actual_rc for RESTORE_RC to put back later)
 * Not `static`: `src-c/kernel/file3.c`'s own `rseek` also calls
 * `SET_RC` directly, an ordinary cross-file call, same as `que1.c`'s
 * own into `que2.c`.
 */
void set_rc(uint8_t *mem, uint16_t bx, uint8_t cl) {
    uint16_t si = (uint16_t)(bx + fcb_reccnt - fcb_extnum);
    uint8_t al = mem[bx];
    uint8_t ch = 0;
    if (al != cl) {
        if (al >= cl) {
            al = 0;
        } else {
            al = 128;
            ch = mem[si];
        }
        mem[si] = al;
    }
    mem[BD_ACTUAL_RC] = ch;
}

/*
 * merge_word -- mergezero(bx,dx) then mergezero(dx,bx): if either
 * word is 0, copy the other's value into it; returns 1 if they're
 * equal afterward (the real asm's own CLOSE_FCB tests exactly this),
 * 0 on a merge error (both non-zero and different)
 */
static uint8_t merge_word(uint8_t *mem, uint16_t bx, uint16_t dx) {
    if (mem_r16(mem, bx) == 0) {
        mem_w16(mem, bx, mem_r16(mem, dx));
    }
    if (mem_r16(mem, dx) == 0) {
        mem_w16(mem, dx, mem_r16(mem, bx));
    }
    return mem_r16(mem, dx) == mem_r16(mem, bx);
}

/*
 * merge_byte -- the single-byte-map equivalent of merge_word, one
 * disk-map byte at a time
 */
static uint8_t merge_byte(uint8_t *mem, uint16_t bx, uint16_t dx) {
    if (mem[bx] == 0) {
        mem[bx] = mem[dx];
    }
    uint8_t al = mem[dx];
    if (al == 0) {
        mem[dx] = mem[bx];
    }
    return al == mem[bx];
}

void seekcopy(uint8_t *mem);

/*
 * close_fcb -- CLOSE_FCB/CLOSE2: given the fcb at BD_INFO, find its
 * matching directory entry (via search) and merge the fcb's own disk
 * map into it, then rewrite the directory record -- CLOSE1's own
 * real caller doesn't check this routine's own success/failure
 * (there's no jz/jnz after "call close_fcb" in the real asm), so this
 * is void: every observable effect is a memory write, not a return
 * value.
 */
static void close_fcb(uint8_t *mem) {
    get_dir_ext_result r = get_dir_ext(mem);
    uint16_t bx = r.bx;
    uint8_t ch = mem[bx]; /* save current fcb extent */
    mem[bx] = r.al;       /* fcb(ext) = dir ext */
    uint8_t al = (uint8_t)(r.al + 1);
    if (ch == al && r.dl == 0) {
        uint16_t si = (uint16_t)(bx + fcb_reccnt - fcb_extnum);
        if (mem[si] == 0) {
            mem[si] = 0x80;
        }
    }
    restore_rc(mem, bx);

    if (!search(mem, namlen)) {
        return; /* ret12 -- not found */
    }

    uint16_t dir_dm = (uint16_t)(getdptra(mem) + fcb_dskmap);
    uint16_t fcb_dm = (uint16_t)(mem_r16(mem, BD_INFO) + fcb_dskmap);
    uint8_t cl = (uint8_t)(fcblen - fcb_dskmap);
    int ok = 1;
    while (cl != 0) {
        if (mem[BD_SINGLE] == 0) {
            /* merged: double-byte disk map */
            if (!merge_word(mem, fcb_dm, dir_dm)) {
                ok = 0;
                break;
            }
            dir_dm = (uint16_t)(dir_dm + 1);
            fcb_dm = (uint16_t)(fcb_dm + 1);
            cl = (uint8_t)(cl - 1);
        } else if (!merge_byte(mem, fcb_dm, dir_dm)) {
            ok = 0;
            break;
        }
        dir_dm = (uint16_t)(dir_dm + 1);
        fcb_dm = (uint16_t)(fcb_dm + 1);
        cl = (uint8_t)(cl - 1);
    }
    if (!ok) {
        make_fcb_inv(mem); /* mergerr */
        mem[BD_LRET] = (uint8_t)(mem[BD_LRET] - 1);
        return;
    }

    /* end of disk map merge -- reconcile the extent/reccnt fields */
    uint16_t fcb_ext = (uint16_t)(mem_r16(mem, BD_INFO) + fcb_extnum);
    uint16_t dir_ext = (uint16_t)(getdptra(mem) + fcb_extnum);
    uint8_t fcb_ext_val = mem[fcb_ext];
    if (fcb_ext_val >= mem[dir_ext]) {
        int equal = (fcb_ext_val == mem[dir_ext]);
        mem[dir_ext] = fcb_ext_val;
        uint16_t fcb_rc = (uint16_t)(mem_r16(mem, BD_INFO) + fcb_reccnt);
        uint16_t dir_rc = (uint16_t)(getdptra(mem) + fcb_reccnt);
        if (equal && mem[dir_rc] >= mem[fcb_rc]) {
            mem[fcb_rc] = mem[dir_rc];
        }
        mem[dir_rc] = mem[fcb_rc];
    }
    /* endmerge */

    if (mem[BD_DONT_CLOSE]) {
        return; /* ret13 */
    }

    /* clear the T3' bit, indicating a file update */
    uint16_t bx2 = (uint16_t)(getdptra(mem) + 11);
    mem[bx2] = (uint8_t)(mem[bx2] & 0x7F);
    (void)setfwf(mem);
    seekcopy(mem); /* jmp seekcopy */
}

/* seekcopy -- enter from close to seek to and rewrite the directory element */
void seekcopy(uint8_t *mem) {
    seekdir(mem);
    wrdir(mem);
}

/*
 * close1 -- CLOSE1: the half of the F_CLOSE BDOS function `close`'s
 * own tail that `FILE1.BDO`'s own `CHECK_FCB` calls directly (via
 * `close`'s own write-protection/file-write-flag checks, which
 * `CHECK_FCB`'s own caller has presumably already done) -- the
 * project's own long-standing blocker for `CHECK_FCB` itself.
 */
void close1(uint8_t *mem) {
    mem[BD_COMP_FCB_CKS] = 0xFF; /* mov comp_fcb_cks,true */
    uint16_t bx = getexta(mem);
    mem[BD_SAVE_EXT] = mem[bx];
    close_fcb(mem);
    bx = getdptra(mem);
    uint8_t cl = mem[(uint16_t)(bx + fcb_extnum)];
    bx = getexta(mem);
    mem[bx] = mem[BD_SAVE_EXT];
    set_rc(mem, bx, cl);
}

/*
 * close -- the real F_CLOSE BDOS function: skip entirely on a
 * read-only disk or if the fcb's own write flag says nothing changed,
 * otherwise fall into close1.
 */
void close(uint8_t *mem) {
    mem[BD_LRET] = 0;
    mem[BD_DONT_CLOSE] = 0;
    if (nowrite(mem)) {
        return; /* ret12 -- skip close on a r/o disk */
    }
    uint16_t bx;
    uint8_t al = getmodnum(mem, &bx);
    if (al & fwfmsk) {
        return; /* ret12 -- fwf bit still set */
    }
    close1(mem);
}

/*
 * scndm -- scan the disk map addressed by BD_DPTR for non-zero
 * entries; the allocation vector entry corresponding to each one is
 * set to `bit` (0 or 1, both callers of this always pass 1)
 */
static void scndm(uint8_t *mem, uint8_t bit) {
    uint16_t bx = (uint16_t)(getdptra(mem) + fcb_dskmap);
    uint8_t cl = (uint8_t)(fcblen - fcb_dskmap + 1);
    for (;;) {
        cl = (uint8_t)(cl - 1);
        if (cl == 0) {
            return;
        }
        uint16_t block;
        if (mem[BD_SINGLE] == 0) {
            /* double-byte disk map */
            cl = (uint8_t)(cl - 1);
            block = mem_r16(mem, bx);
            bx = (uint16_t)(bx + 1);
        } else {
            block = mem[bx];
        }
        if (block != 0 && block <= mem_r16(mem, BD_MAXALL)) {
            setallocbit(mem, block, bit);
        }
        bx = (uint16_t)(bx + 1);
    }
}

/*
 * test_dir -- TEST_DIR: re-read the directory (via rddir's own
 * cl=0xFE initialization mode) until either a checksum mismatch
 * flushes something (BD_FLUSHED becomes true -- the media really did
 * change) or the directory is exhausted without one (nothing changed)
 *	output: returns 1 if a flush happened (the caller should relog
 *		the drive for real), 0 if not (no relog needed)
 */
static uint8_t test_dir(uint8_t *mem) {
    home(mem);
    setenddir(mem);
    for (;;) {
        rddir(mem, 0xFE);
        if (mem[BD_FLUSHED] != 0) {
            return 1;
        }
        if (endofdir(mem)) {
            return 0;
        }
    }
}

/*
 * initialize -- initialize the current disk: if it's already logged
 * in on this drive and nothing has actually changed since (no flush,
 * `test_dir`), there's nothing to do; otherwise zero the allocation
 * vector, re-home the disk, and rebuild the allocation vector from
 * every real directory entry's own disk map.
 */
void initialize(uint8_t *mem) {
    if (test_vector(mem, mem_r16(mem, BD_TLOG))) {
        /* TLOG(curdsk) was on -- clear it and see if a relog is
         * really needed */
        mem_w16(mem, BD_TLOG, remove_drive(mem, mem_r16(mem, BD_TLOG)));
        mem[BD_FLUSHED] = 0;
        if (search_olist(mem, 1)) {
            /* a file is currently open on this drive */
            if (!test_dir(mem)) {
                return; /* ret8a -- no media change, don't relog */
            }
        }
    }
    /* initialize1 -- log in drive */
    {
        uint16_t count = get_nalbs(mem);
        uint16_t bx = mem_r16(mem, BD_ALLOCA);
        for (uint16_t i = 0; i < count; i++) {
            mem[(uint16_t)(bx + i)] = 0;
        }
    }
    mem[mem_r16(mem, BD_DRVLBLA)] = 0;
    mem_w16(mem, mem_r16(mem, BD_ALLOCA), mem_r16(mem, BD_DIRBLK));
    home(mem);
    mem_w16(mem, mem_r16(mem, BD_CDRMAXA), 3);
    setenddir(mem);
    for (;;) {
        rddir(mem, 1); /* true -- initializing */
        if (endofdir(mem)) {
            return; /* ret8b */
        }
        uint16_t dptr = getdptra(mem);
        if (mem[dptr] == bdos_empty) {
            continue; /* another entry */
        }
        if (mem[dptr] == 0x20) {
            /* DRV_LBL -- directory label entry */
            mem[mem_r16(mem, BD_DRVLBLA)] = mem[(uint16_t)(dptr + fcb_extnum)];
        } else if ((mem[dptr] & 0x70) == 0) {
            /* not a special-type fcb -- mark its own disk map allocated */
            scndm(mem, 1);
        }
        /* initial3 */
        setcdr(mem);
    }
}

/*
 * set_dcnt_dblk -- SET_DCNT_DBLK: set dcnt/dblk from the last XFCB
 * search's own saved position (one record back, so the next rddir
 * lands back on it). Not `static`: `src-c/kernel/file4.c`'s own
 * `func15` (BDOS OPEN) also calls `SET_DCNT_DBLK` directly, an
 * ordinary cross-file call, same as `que1.c`'s own into `que2.c`.
 */
void set_dcnt_dblk(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, BD_XDCNT);
    bx = (uint16_t)((bx & 0xFF00) | (bx & 0xFC)); /* and bl,0fch */
    bx = (uint16_t)(bx - 1);
    mem_w16(mem, BD_DCNT, bx);
    mem_w16(mem, BD_DBLK, mem_r16(mem, BD_XDBLK));
}

/*
 * copy_dir2 -- COPY_DIR2: shared setup for COPY_DIR0/COPY_DIR1 --
 * input cl (fcb byte offset) and dx (dh=attribute mask, dl=length,
 * packed together as one word by the caller)
 *	output: bx = .buff(dptr) (the directory's own copy destination),
 *		cx = the caller's own dx (attribute mask + length,
 *		unpacked by the caller), dx = .fcb(cl) (the copy source)
 */
typedef struct {
    uint16_t bx, cx, dx;
} copy_dir2_result;

static copy_dir2_result copy_dir2(uint8_t *mem, uint16_t cx, uint16_t dx) {
    uint16_t saved_dx = dx;
    cx = (uint16_t)(cx & 0x00FF); /* mov ch,0 */
    dx = (uint16_t)(mem_r16(mem, BD_INFO) + cx);
    uint16_t bx = getdptra(mem);
    copy_dir2_result r;
    r.bx = bx;
    r.cx = saved_dx;
    r.dx = dx;
    return r;
}

/*
 * copy_dir0 -- COPY_DIR0/COPY_DIR1: copy `length` fcb bytes starting
 * at offset `cl` into the matching directory entry, OR'ing each dir
 * byte's own masked attribute bits back in (same per-byte merge
 * `attrmask` governs as `reselect`'s own copy, not ported here) --
 * then rewrites the directory record (`seekcopy`).
 *	input: cl = fcb byte offset to start at, dx = (dh=attrmask,
 *		dl=length)
 */
static void copy_dir0(uint8_t *mem, uint16_t cl, uint16_t dx) {
    copy_dir2_result r = copy_dir2(mem, cl, dx);
    uint16_t bx = r.bx;
    uint8_t ch = (uint8_t)(r.cx >> 8);
    uint8_t count = (uint8_t)(r.cx & 0xFF);
    uint16_t d = r.dx;
    for (;;) {
        if (count == 0) {
            seekcopy(mem);
            return;
        }
        count = (uint8_t)(count - 1);
        uint8_t ah = (uint8_t)(mem[bx] & ch);
        uint8_t al = (uint8_t)((mem[d] & 0x7F) | ah);
        mem[bx] = al;
        bx = (uint16_t)(bx + 1);
        d = (uint16_t)(d + 1);
    }
}

/*
 * sdcnt_eq_xdcnt -- SDCNT_EQ_XDCNT: copy BD_XDCNT/BD_XDBLK (4 bytes)
 * over BD_SDCNT/BD_SDBLK. Not `static`: `src-c/kernel/file3.c`'s own
 * `fix_olist_item` also calls `SDCNT_EQ_XDCNT` directly, an ordinary
 * cross-file call, same as `que1.c`'s own into `que2.c`.
 */
void sdcnt_eq_xdcnt(uint8_t *mem) {
    move(mem, BD_XDCNT, BD_SDCNT, 4);
}

/*
 * make -- MAKE: create a new file by finding an empty directory slot
 * (searching with INFO temporarily pointed at BD_EFCB), copying the
 * fcb into it, and setting its own file-write flag.
 */
void make(uint8_t *mem) {
    if (mem_r16(mem, BD_XDCNT) == 0xFFFF) {
        mem[BD_LRET] = 255; /* lret_eq_ff */
        return;
    }
    set_dcnt_dblk(mem);
    checkwrite(mem);

    uint16_t saved_info = mem_r16(mem, BD_INFO);
    mem_w16(mem, BD_INFO, BD_EFCB);
    uint8_t found = search(mem, 1);
    mem_w16(mem, BD_INFO, saved_info); /* pop info */
    if (!found) {
        return; /* ret13 -- 255, not found (search's own srchfin set it) */
    }

    if (mem[BD_MAKE_XFCB]) {
        return; /* ret13 -- making an xfcb, nothing more to do here */
    }

    uint16_t bx = (uint16_t)(mem_r16(mem, BD_INFO) + 13);
    mem[bx] = 0; /* clear s1 byte */
    bx = (uint16_t)(bx + 1); /* .fcb(modnum) */
    uint8_t saved_al = mem[bx];
    uint16_t saved_bx = bx;
    mem[bx] = (uint8_t)(saved_al & 0x3F); /* clear + save file write flag */
    bx = (uint16_t)(bx + 1);
    for (uint8_t i = 0; i < fcblen - namlen; i++) { /* clear the remainder of the fcb */
        mem[bx] = 0;
        bx = (uint16_t)(bx + 1);
    }
    setcdr(mem); /* may have extended the directory */
    copy_dir0(mem, 0, fcblen); /* now copy the entry to the directory */

    mem[saved_bx] = saved_al; /* restore file write flag */
    mem[BD_ACTUAL_RC] = 0;
    setfwf(mem); /* jmp setfwf -- set file write flag to "1" */
}

/* cpydirloc -- copy BD_DIRLOC to BD_LRET (via the real asm's own `jmp staret`) */
static void cpydirloc(uint8_t *mem) {
    mem[BD_LRET] = mem[BD_DIRLOC];
}

/*
 * indicators -- set file indicators for every directory entry
 * matching the current fcb: unlike `COPY_DIR0`/`COPY_DIR1`'s own
 * per-byte attribute-masked copy, this one (`COPY_DIR2` followed
 * directly by a plain `MOVE`, not `COPY_DIR1`'s own masking loop)
 * copies the first `fcb_extnum` (12) bytes of the fcb over the
 * directory entry unconditionally -- `fcb_extnum` reused purely for
 * its *numeric* value as a byte count here, same "constant reuse for
 * numeric value, not semantic meaning" trick as `rename_`'s own
 * `DSKMAP`/`EXTNUM` (see that function's own header comment). Not
 * `static`: `src-c/kernel/file4.c`'s own `func30` also calls
 * `indicators` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`. Lands at 0xf0f in the combined
 * wrapper bundle -- inside DATA.BDO's own dseg collision range, same
 * as `copy_dir2`/`copy_dir0`-`2` already are (see `open`'s own header
 * comment above) -- so, same as those, no new differential test.
 */
void indicators(uint8_t *mem) {
    (void)get_atts(mem);
    if (!search(mem, fcb_extnum)) {
        return; /* ret10 */
    }
    chk_olist(mem);
    for (;;) {
        copy_dir2_result r = copy_dir2(mem, 0, fcb_extnum);
        move(mem, r.dx, r.bx, (uint8_t)r.cx);
        seekcopy(mem);
        if (!searchn(mem)) {
            break;
        }
    }
    cpydirloc(mem);
}

/*
 * init_xfcb_search1/init_xfcb_search -- (re)start an XFCB search: set
 * BD_FIND_XFCB and reset BD_XDCNT's own high byte to the 0xFE "unset"
 * sentinel `does_xfcb_exist` checks for.
 */
static void init_xfcb_search1(uint8_t *mem, uint8_t al) {
    mem[BD_FIND_XFCB] = al;
    mem[(uint16_t)(BD_XDCNT + 1)] = 0xFE;
}

static void init_xfcb_search(uint8_t *mem) {
    init_xfcb_search1(mem, 0xFF);
}

/*
 * chk_wild -- CHK_WILD: scan an fcb's own name+type (11 bytes) for a
 * '?' wildcard character.
 *	input: bx = .fcb(0)
 *	output: returns 1 if a '?' was found (the real ZF), 0 if not
 */
uint8_t chk_wild(const uint8_t *mem, uint16_t bx) {
    uint8_t ch = 0x3F;
    uint8_t cl = 0x0B;
    for (;;) {
        bx = (uint16_t)(bx + 1);
        uint8_t al = (uint8_t)((uint8_t)(ch - mem[bx]) & ch);
        if (al == 0) {
            return 1; /* ret10 -- ? found ('?' == 0x3F, so ch-mem[bx]==0) */
        }
        cl = (uint8_t)(cl - 1);
        if (cl == 0) {
            return 0; /* no ?s found */
        }
    }
}

/*
 * does_xfcb_exist -- DOES_XFCB_EXIST: is there a saved XFCB position
 * (from an earlier GET_XFCB-family search) still pending cleanup?
 * If so, restart a search from right there, reusing the existing
 * BD_SRCHA pointer (not re-deriving it from BD_INFO).
 *	output: returns 1 if an XFCB match was (re-)found (the real ZF
 *		via `searchn`), 0 if not
 */
static uint8_t does_xfcb_exist(uint8_t *mem) {
    if (mem[(uint16_t)(BD_XDCNT + 1)] == 0xFE) {
        return 0; /* ret9 -- no xfcb position recorded */
    }
    set_dcnt_dblk(mem);
    init_xfcb_search1(mem, 0);
    uint16_t bx = mem_r16(mem, BD_SRCHA);
    mem[bx] |= 0x10;
    searchi1(mem, fcb_extnum);
    return searchn(mem);
}

/*
 * delet -- DELETE: delete every directory entry matching the current
 * fcb (wildcards included), freeing each one's own allocated disk-
 * map blocks, plus any matching XFCBs.
 */
static void delete1_entry(uint8_t *mem);
void delet0(uint8_t *mem, uint8_t found);

/*
 * delete2_entry -- DELETE2: if an xfcb still exists for this fcb's
 * own name/type, delete it too (another pass through `delete1_entry`
 * below); otherwise, done.
 */
static void delete2_entry(uint8_t *mem) {
    if (does_xfcb_exist(mem)) {
        delete1_entry(mem);
        return;
    }
    cpydirloc(mem);
}

/*
 * delete1_entry -- DELETE1: the fcb/xfcb this call is addressing is
 * currently a real directory match (its own disk map entries get
 * freed, same as `delet`'s own header comment on the real "found"
 * convention there); keep deleting matches until `searchn` runs dry,
 * then fall into `delete2_entry` above. Not just `delet`'s own
 * internal label: `rename` calls this directly too (matching the
 * real asm's own "CALL DELETE1" into the middle of `DELETE`), so
 * it's its own function.
 */
static void delete1_entry(uint8_t *mem) {
    for (;;) {
        chk_olist(mem); /* remove matching open list items */
        uint16_t bx = getdptra(mem);
        uint8_t is_xfcb = (uint8_t)(mem[bx] & 0x10);
        mem[bx] = bdos_empty;
        if (!is_xfcb) {
            /* don't free blocks for xfcbs */
            scndm(mem, 0); /* alloc elts set to 0 */
        }
        wrdir(mem); /* write the directory */
        if (!searchn(mem)) { /* to next element */
            break;
        }
    }
    delete2_entry(mem);
}

void delet(uint8_t *mem) {
    checkwrite(mem); /* write protected? */
    uint8_t pw_ok = chk_pws(mem); /* check for password error -- saved for later */
    mem[BD_ATTRIBUTES] = get_atts(mem); /* get & save fcb attributes */
    init_xfcb_search(mem); /* save dcnt of 1st xfcb in search pass */

    uint8_t found = search(mem, fcb_extnum); /* search for file */
    if (!found) {
        /* not found -- password error (if any) doesn't matter */
        delete2_entry(mem);
        return;
    }
    if (!pw_ok) {
        set_aret(mem, 7); /* chk_pw1 -- password error */
        return;
    }

    if (mem[BD_ATTRIBUTES] & 0x80) {
        /* if deleting xfcbs only, a pass through the entire
         * directory is required to locate them */
        goto delete015;
    }
    if (chk_wild(mem, mem_r16(mem, BD_INFO))) {
        /* wild-card delete -- each dcnt must be verified (MP/M) and
         * each fcb must be checked for read/only */
        goto delete015;
    }
    tst_olist(mem); /* verify dcnt against open list */
    ckrodir(mem); /* only one pass is required on normal non-wild deletes */
    /* "jmps delet0" in the real asm, but `ckrodir`'s own real ZF at
     * that point is just an incidental side effect of RO_TEST's own
     * `ADD BX,ROFILE` (always leaves ZF clear in practice, not a
     * real not-found check) -- so this always falls straight through
     * to delete1, never to delete2; not modeled as a flag */
    delete1_entry(mem);
    return;

delete01: /* loop while directory matches */
    if (!found) {
        goto delete02;
    }
delete015:
    tst_olist(mem); /* verify dcnt against open list */
    ckrodir(mem); /* verify fcb is not read/only */
    found = searchn(mem);
    goto delete01;

delete02:
    if (mem[BD_ATTRIBUTES] & 0x80) {
        /* skip delete of fcbs if attribute f5' is set */
        delete2_entry(mem);
        return;
    }
    found = search(mem, fcb_extnum); /* search through file type */
    delet0(mem, found);
}

/*
 * delet0 -- DELET0: the "found" dispatch shared by `delet`'s own two
 * real searches through the file's extents (`DELETE2`/`DELETE1`'s own
 * fork). Not `static`: `src-c/kernel/file4.c`'s own `func22` (BDOS
 * MAKE) also calls `DELET0` directly after its own `SEARCH`, an
 * ordinary cross-file call, same as `que1.c`'s own into `que2.c`.
 */
void delet0(uint8_t *mem, uint8_t found) {
    if (!found) {
        delete2_entry(mem);
        return;
    }
    delete1_entry(mem);
}

/*
 * check_wild0/check_wild -- CHECK_WILD0/CHECK_WILD: error (9) if
 * bx's own fcb name/type contains a '?' wildcard -- the opposite
 * sense of `delet`'s own wildcard check (which *wants* one).
 *	output: returns 1 if ok (no wildcard), 0 on error (never
 *		returns normally in the real asm -- see `set_aret`'s own
 *		header comment, `file4.c`, and `check_fcb1`'s for why
 *		this returns a plain boolean instead)
 *	Not `static`: `src-c/kernel/file4.c`'s own `func15` (BDOS OPEN)
 *	also calls `check_wild` directly, an ordinary cross-file call,
 *	same as `que1.c`'s own into `que2.c`.
 */
static uint8_t check_wild0(uint8_t *mem, uint16_t bx) {
    if (chk_wild(mem, bx)) {
        set_aret(mem, 9);
        return 0;
    }
    return 1; /* ret10 */
}

uint8_t check_wild(uint8_t *mem) {
    return check_wild0(mem, mem_r16(mem, BD_INFO));
}

/*
 * copy_user_no -- COPY_USER_NO: fcb(16) = fcb(0) (propagate the
 * drive-select code into the "new name" half of the fcb, same user)
 *	output: returns bx = .fcb(16)
 */
static uint16_t copy_user_no(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, BD_INFO);
    uint8_t al = mem[bx];
    bx = (uint16_t)(bx + fcb_dskmap);
    mem[bx] = al;
    return bx;
}

/*
 * copy_dir -- copy_dir's own real `MOV DH,80H` prologue before
 * falling into `COPY_DIR0`/`COPY_DIR1`/`COPY_DIR2` -- see `make`'s
 * own header comment for why those stay un-exposed beyond this.
 */
static void copy_dir(uint8_t *mem, uint16_t cl, uint8_t dl) {
    copy_dir0(mem, cl, (uint16_t)(((uint16_t)0x80 << 8) | dl));
}

/*
 * rename_ -- RENAME: rename the file described by the first half of
 * the currently addressed fcb to the name in its own second half
 * (fcb offset 16+) -- same drive/user, reel number ignored.
 */
void rename_(uint8_t *mem) {
    checkwrite(mem); /* may be write protected */

    if (!check_wild(mem)) { /* check for ?s in 1st filename */
        return;
    }
    if (!chk_password(mem)) { /* check password */
        return;
    }

    init_xfcb_search(mem);

    uint16_t bx = copy_user_no(mem); /* fcb(16) = fcb(0) */
    mem_w16(mem, BD_SRCHA, bx);
    if (!check_wild0(mem, bx)) { /* check 2nd filename for ?s */
        return;
    }

    /* check to see if new filename already exists on drive */
    searchi1(mem, fcb_extnum);
    if (search1(mem)) {
        file_exists(mem); /* error -- file exists (never returns normally) */
        return;
    }
    if (does_xfcb_exist(mem)) { /* delete xfcb if present */
        delete1_entry(mem);
    }
    copy_user_no(mem); /* fcb(16) = fcb(0) again */
    init_xfcb_search(mem);

    /* search up to the extent field */
    if (!search(mem, fcb_extnum)) {
        return; /* ret10 */
    }
    ckrodir(mem); /* check for read/only file */
    chk_olist(mem);

renam0:
    /* not end of directory, rename next element */
    copy_dir(mem, fcb_dskmap, fcb_extnum);
    if (searchn(mem)) { /* element renamed, move to next */
        goto renam0;
    }

    /* rename1 */
    if (does_xfcb_exist(mem)) { /* rename xfcb if present */
        copy_user_no(mem); /* xfcb(16) = xfcb(0) */
        goto renam0;
    }
    cpydirloc(mem);
}

/*
 * opencopy -- the second half of `open`/`OPEN1`: copy the directory
 * entry `search`/`SEARCHI`+`SEARCHN` just found (its own `GETDPTRA`
 * address) into the current fcb, restoring the fcb's own extnum/
 * modnum bytes (the exact bytes `search`'s own matching ignores)
 * instead of letting the raw copy overwrite them, then let `set_rc`
 * reconcile the fcb's own now-restored extent against the directory
 * entry's extent. Inlines `setfwf`'s own body for its first call
 * (needs the fcb(modnum) address `setfwf` itself doesn't expose, same
 * reason `check_fcb1`'s own header comment gives for why some of
 * these real routines resist a clean 1:1 wrapper).
 */
static void opencopy(uint8_t *mem) {
    uint16_t modnum_addr;
    uint8_t al = (uint8_t)(getmodnum(mem, &modnum_addr) | fwfmsk);
    uint16_t extnum_addr = (uint16_t)(modnum_addr - 2);
    uint8_t ah = mem[extnum_addr]; /* saved extnum */

    uint16_t dptr = getdptra(mem);
    uint16_t info = mem_r16(mem, BD_INFO);
    move(mem, dptr, info, fcb_nxtrec); /* copy the whole dir entry over the fcb */
    (void)setfwf(mem); /* re-set the file-write flag the copy just clobbered */

    uint8_t cl = mem[(uint16_t)(dptr + fcb_extnum)]; /* dir ext# */

    mem[modnum_addr] = al; /* restore modnum|fwfmsk */
    mem[extnum_addr] = ah; /* restore extnum */
    set_rc(mem, extnum_addr, cl);
}

/*
 * open1 -- OPEN1: `open`'s own second entry point, used directly by
 * `FILE4.BDO`'s own `func15` right after its own manual `SEARCHI`/
 * `SEARCHN` pair -- `found` is that `SEARCHN`'s own result (the real
 * ZF `OPEN1` itself tests), not re-derived here.
 *	output: BD_LRET is left at 0xFF (set by `searchn`/`search1`
 *		itself when `found` is 0) if not found
 */
void open1(uint8_t *mem, uint8_t found) {
    if (!found) {
        return; /* ret10 */
    }
    opencopy(mem);
}

/*
 * open -- open: `SEARCH` for the current fcb's own directory entry,
 * then `open1`/`opencopy` it into the fcb.
 */
void open(uint8_t *mem) {
    open1(mem, search(mem, namlen));
}

/*
 * openreel -- close the current extent and open the next one if
 * possible (`BD_RMF`=0xFF means read mode, no auto-extend via
 * `MAKE`). The real asm's own `call make` is followed by a comment
 * ("cannot be end of directory") over a commented-out `call
 * endofdir`/`jz openerr` -- so the real `jz openerr` right after
 * tests whatever flag `make`'s own real tail (`jmp setfwf`) happens
 * to leave, and `setfwf`'s own `or al,fwfmsk` always leaves `al`
 * nonzero (`fwfmsk`=0x80), so that `jz` can never actually fire; not
 * modeled, same "translate faithfully but note unreachable"
 * convention as `cli_gopen`/`curselect`'s own `SELECT` success path.
 */
void openreel(uint8_t *mem) {
    uint16_t dummy_bx;
    mem[BD_SAVE_MOD] = getmodnum(mem, &dummy_bx);
    uint16_t extbx = getexta(mem);
    uint8_t al = mem[extbx];
    uint8_t cl = (uint8_t)(al + 1);
    if (compext(mem, al, cl)) {
        goto openr3;
    }

    close(mem);
    if (mem[BD_LRET] == 0xFF) {
        return;
    }
    al = (uint8_t)(maxext & cl);
    mem[extbx] = al;
    if (al != 0) {
        goto openr0;
    }

    /* openmod -- extent number overflowed, move to the next module */
    {
        uint16_t modbx = (uint16_t)(extbx + fcb_modnum - fcb_extnum);
        mem[modbx] = (uint8_t)(mem[modbx] + 1);
        al = (uint8_t)(mem[modbx] & maxmod);
        if (al == 0) {
            goto openerr; /* cannot overflow to module 0 */
        }
    }

openr0:
    mem_w16(mem, BD_XDCNT, 0xFFFF); /* reset for MAKE */
    mem[(uint16_t)(BD_SDCNT + 1)] = 0xFF;
    if (search(mem, namlen)) {
        goto openr1;
    }
    /* end of file encountered */
    if (mem[BD_RMF] == 0xFF) {
        goto openerr; /* read mode -- cannot extend */
    }
    make(mem); /* try to extend the file */
    fix_olist_item(mem);
    mem[BD_COMP_FCB_CKS] = 1;
    goto openr2;

openr1:
    /* not end of file, open the next extent */
    mem[BD_COMP_FCB_CKS] = 1;
    opencopy(mem);
    goto openr2;

openr2:
    getfcb(mem); /* set parameters */
    mem[BD_VRECORD] = 0;
    mem[BD_LRET] = 0; /* staret, al=0 */
    return;

openerr:
    {
        uint16_t bx;
        (void)getmodnum(mem, &bx);
        mem[bx] = mem[BD_SAVE_MOD];
        bx = getexta(mem);
        mem[bx] = mem[BD_SAVE_EXT];
    }
    make_fcb_inv(mem);
    mem[BD_COMP_FCB_CKS] = 0xFF;
    mem[BD_LRET] = 1; /* setlret1 */
    (void)setfwf(mem); /* ensure the fcb won't be closed */
    return;

openr3:
    /* same module -- just bump the extent field in place */
    mem[extbx] = cl;
    {
        get_dir_ext_result r = get_dir_ext(mem);
        restore_rc(mem, r.bx);
        set_rc(mem, r.bx, r.al);
    }
    goto openr2;
}
