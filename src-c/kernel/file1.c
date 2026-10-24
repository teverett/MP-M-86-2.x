/*
 * file1.c -- C port of a self-contained subset of src/kernel/FILE1.BDO:
 * the error handlers (pererror/roderror/roferror/selerror, sharing
 * goerr) and the low-level disk-I/O/FCB-arithmetic helpers (move,
 * selectdisk, home, PASS_ARECORD, rdbuff, wrbuff/wrbufflg, seekdir,
 * seek, dmposition, GET_DMA, getdm, bdos_index, alloct, atran, GET_ATTS,
 * GET_S1, GET_RRA, GET_RCNTA, getexta, gtfcba, getfcb, setfcb, cmpecs,
 * CHKSUM_FCB, SET_CHKSUM_FCB, RESET_CHECKSUM_FCB, CHEK_FCB, setcdisk,
 * nowrite, setro, ckrodir/ckrofile/RO_TEST, checkwrite, getdptra,
 * getmodnum, clrmodnum, CLR_EXT, setfwf, compcdr, setcdr,
 * rddirbuf, setdata/setdir/setdma, dirtouser, MAKE_FCB_INV,
 * CHK_INV_FCB, TST_INV_FCB, endofdir, setenddir, getallocbit,
 * setallocbit, checksum/newchecksum, rddir/rddir0, wrdir,
 * check_fcb1/check_fcb).
 *
 * FILE1.BDO is not self-contained -- it's one of 4 files (FILE1-4.BDO)
 * meant to be assembled together into one BDOS module, sharing data
 * and tail-calling into each other. `checksum`/`newchecksum`'s own
 * MPM branch calls `FLUSH_FILE0`/`FLUSH_FILES` (`src-c/kernel/
 * file3.c`, now real internal calls rather than a boundary stub --
 * see that file's own header comment for the segment-relative-
 * addressing fix that unblocked them), which in turn unblocked
 * `rddir`/`rddir0` (both call `checksum`) and `wrdir` (calls
 * `newchecksum`). `subdh` (`checksum`'s own `dx<bx` size check) folds
 * directly into an `if` there instead of a separate function -- its
 * only two real callers are `checksum` and (transitively, via
 * `rddir0`) `rddir`, both now ported, but the subtraction itself was
 * never worth a standalone C routine.
 *
 * `CHECK_FCB`/`CHECK_FCB1` are ported now too (see `check_fcb1`'s own
 * header comment below for the full calling-convention writeup). An
 * earlier revision of this comment claimed `CHECK_FCB` had its own
 * further manual-stack complication beyond `CHECK_FCB1`'s own (on
 * top of the "invalid checksum, partial close might fix it" path's
 * own `POP BX ;DISCARD RETURN ADDRESS`) -- re-reading it closely once
 * `FILE4.BDO`'s own `func15`/`func16` needed `CHECK_FCB1` directly
 * showed that's wrong: `CHECK_FCB` (under MPM) is nothing but `MOV
 * CHECK_FCB_RET,FALSE` immediately falling through into `CHECK_FCB1`
 * itself -- no separate complication at all, just the same stack
 * trick with a different flag value, already covered by
 * `check_fcb1`'s own boolean-return translation. `FILE4.BDO`'s own
 * `func20`/`func21` (sequential read/write) are `CHECK_FCB`'s own two
 * real callers -- both just need "stop immediately on failure,"
 * which this boolean return already gives for free.
 *
 * `GOBACK` (pererror/roderror/roferror/selerror/goerr's own common
 * tail, via RTN_PHY_ERRS) lives in FILE4.BDO: `mov sp,save_sp ! jmp
 * bdos_return`, itself a trivial 2-instruction tail into the
 * still-unported BDOSIF.BDO dispatcher. Reaching it is this file's own
 * natural exit boundary -- nothing to port past it, same class as a
 * tail JMP into `mpmif` elsewhere.
 *
 * `readf`/`writef`/`seldskf`/`homef`/`settrkf`/`sectran`/`setsecf`/
 * `setdmbf`/`setdmf` are near CALLs into the XIOS BIOS jump table, a
 * boundary stub per docs/src-c.md's convention -- same shape as
 * `LBDOSIO.A86`'s `constf`/`coninf`/`wbootf`. `readf`/`writef`'s own
 * result (the real AL byte `diocomp` branches on) is, like
 * `xiosif`'s own `bx` in `src-c/kernel/chario.c`, an undefined
 * artifact under the convention (no sentinel is pre-loaded into the
 * register the caller reads back) -- fixed at 0 (success) here, the
 * same choice made there, which makes `diocomp`'s own `pererror`/
 * `roderror` branches unreachable (translated faithfully, not
 * exercised by the test). `XCRLF`/`XPRINT` (both in BDOSIF.BDO) are
 * likewise pure boundary-stub tails into `mpmif` -- `goerr`'s own
 * `REPORT_ERR` message-formatting is complete display, no observable
 * effect under the convention, so it's not translated at all; only
 * the one real side effect on that path (setting `pf_ctlc`) is kept.
 *
 * `move` is also called from `src-c/kernel/file3.c`'s own
 * `set_olist_item` (an ordinary cross-file call, same convention as
 * `que1.c`'s own cross-file calls into `que2.c` or `file2.c`'s own
 * call into this file's `getallocbit`), hence non-`static`.
 */
#include <stdint.h>

#include "defs.h"

/*
 * move -- move cl bytes from dx to bx
 */
void move(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl) {
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(bx + i)] = mem[(uint16_t)(dx + i)];
    }
}

/*
 * selectdisk -- select the disk drive given by BD_SELDSK. seldskf's
 * own boundary-stub result (bx) is fixed at 0 here, the same
 * undefined-artifact choice made throughout this file -- see header
 * comment -- which is the real routine's own *error* indication, so
 * this always "fails": curdsk is left at the 0xFF sentinel set below,
 * and the whole cdrmaxa..alloca/sectpt..offsetv header-copy plus the
 * real curdsk update are translated faithfully but never reached,
 * same class as CPMMEM.MEM's own unreachable allocation-success paths.
 */
void selectdisk(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_CURDSK, 0xFF);
    /* mov cl,seldsk!call seldskf -- boundary stub, bx fixed at 0, which
     * is the real routine's own *error* indication ("bx = 0000 if
     * error"), so "cmp bx,0!jz ret4" always takes the early-out here,
     * leaving curdsk at the 0xFF set above -- the retselect-based
     * success path below (which sets curdsk from BD_SELDSK) is the one
     * that's unreachable under the convention, not this one; translated
     * faithfully regardless */
    uint16_t bx = 0;
    if (bx == 0) {
        return; /* ret4 */
    }
    /* unreachable under the convention -- translated faithfully */
    uint16_t dx = mem_r16(mem, bx);
    bx = (uint16_t)(bx + 2);
    mem_w16_seg(mem, ds_bdos, BD_CDRMAXA, bx);
    bx = (uint16_t)(bx + 2);
    mem_w16_seg(mem, ds_bdos, BD_DRVLBLA, bx);
    bx = (uint16_t)(bx + 4);
    mem_w16_seg(mem, ds_bdos, BD_TRANV, dx);
    move(mem,mem_seg_addr(ds_bdos, mem_r16_seg(mem, ds_bdos, BD_DPBADDR)),mem_seg_addr(ds_bdos, BD_SECTPT), 0x0F);
    mem_w8_seg(mem, ds_bdos,BD_SINGLE, (mem_r8_seg(mem, ds_bdos,(uint16_t)(BD_MAXALL + 1)) == 0) ? 1 : 0);
    mem_w8_seg(mem, ds_bdos,BD_CURDSK, mem_r8_seg(mem, ds_bdos,BD_SELDSK));
}

/*
 * home -- move to home position, reset the directory block
 */
void home(uint8_t *mem, uint16_t ds_bdos) {
    /* call homef -- boundary stub, no-op */
    mem_w16_seg(mem, ds_bdos, BD_DBLK, 0);
}

/*
 * PASS_ARECORD -- returns {dx, ch} = BD_ARECORD's own low word/high byte
 */
static void pass_arecord(const uint8_t *mem, uint16_t ds_bdos, uint16_t *dx_out, uint8_t *ch_out) {
    *dx_out = mem_r16_seg(mem, ds_bdos, BD_ARECORD);
    *ch_out = mem_r8_seg(mem, ds_bdos,BD_ARECORD_HI);
}

/*
 * diocomp -- check readf/writef's own result for disk errors
 *	output:	returns 0 (success) always -- see header comment
 * Not `static`: `src-c/kernel/file4.c`'s own `func48` (BDOS flush)
 * also calls `DIOCOMP` directly, an ordinary cross-file call, same
 * as `que1.c`'s own into `que2.c`.
 */
uint8_t diocomp(uint8_t *mem) {
    (void)mem;
    return 0;
}

/* rdbuff -- read buffer and check if ok */
void rdbuff(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t dx;
    uint8_t ch;
    pass_arecord(mem, ds_bdos, &dx, &ch);
    (void)dx;
    (void)ch;
    /* call readf -- boundary stub, no-op */
    (void)diocomp(mem);
}

/* wrbuff/wrbufflg -- write buffer and check condition */
void wrbuff(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t dx;
    uint8_t ch;
    pass_arecord(mem, ds_bdos, &dx, &ch);
    (void)dx;
    (void)ch;
    /* or cl,rem_drv!call writef -- boundary stub, no-op; cl's own
     * value has no observable effect, see header comment */
    (void)diocomp(mem);
}

void wrbufflg(uint8_t *mem, uint16_t ds_bdos) {
    /* mov cl,wflag -- no observable effect, see wrbuff's own comment */
    wrbuff(mem, ds_bdos);
}

/*
 * goerr -- common tail for pererror/roderror/roferror/selerror
 *	input:	ch = message selector (1-4), unused beyond building aret
 */
static void goerr(uint8_t *mem, uint16_t ds_bdos, uint8_t ch) {
    mem_w16_seg(mem, ds_bdos, BD_ARET, (uint16_t)(((uint16_t)ch << 8) | 0xFF));
    if (mem_r8_seg(mem, ds_bdos,BD_ERROR_MODE) != 0xFF) {
        /* REPORT_ERR -- all message formatting is display-only, no
         * observable effect under the convention; the one real side
         * effect on this path: */
        if (mem_r8_seg(mem, ds_bdos,BD_ERROR_MODE) != 0xFE) {
            uint16_t rlr = mem_r16(mem, SD_RLR);
            mem_w16(mem, (uint16_t)(rlr + p_flag),
                    (uint16_t)(mem_r16(mem, (uint16_t)(rlr + p_flag)) | pf_ctlc));
        }
    }
    /* RTN_PHY_ERRS */
    if (mem_r8_seg(mem, ds_bdos,BD_RETURN_FFFF) != 0) {
        mem_w8_seg(mem, ds_bdos,BD_RETURN_FFFF, 0);
        mem_w16_seg(mem, ds_bdos, BD_ARET, 0xFFFF);
    }
    /* jmp GOBACK -- tail into FILE4.BDO, see header comment */
}

void pererror(uint8_t *mem, uint16_t ds_bdos) { goerr(mem, ds_bdos, 1); }
void roderror(uint8_t *mem, uint16_t ds_bdos) { goerr(mem, ds_bdos, 2); }
void roferror(uint8_t *mem, uint16_t ds_bdos) { goerr(mem, ds_bdos, 3); }
void selerror(uint8_t *mem, uint16_t ds_bdos) { goerr(mem, ds_bdos, 4); }

void seek(uint8_t *mem);

/*
 * seekdir -- seek the record containing the current directory entry.
 * Has no real "ret" of its own -- falls straight through into seek's
 * own body (its only caller, rddir0 (deferred), expects exactly that:
 * a single `call seekdir` performs both the arecord computation below
 * and the actual seek), so this calls seek() as a tail, not just a
 * sibling function.
 */
void seekdir(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t dx = 0xFFFF;
    uint8_t ah = 0;
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_DBLK);
    if (bx != 0) {
        dx = (uint16_t)(dx + 1); /* inc dx -- wraps to 0 */
        uint8_t dl = mem_r8_seg(mem, ds_bdos,BD_BLKMSK);
        dx = (uint16_t)((dx & 0xFF00) | dl); /* mov dl,blkmsk */
        uint8_t cl = mem_r8_seg(mem, ds_bdos,BD_BLKSHF);
        uint8_t al = (uint8_t)(bx >> 8); /* mov al,bh */
        bx = (uint16_t)(bx << cl); /* shl bx,cl */
        uint16_t ax = (uint16_t)((uint16_t)al << cl); /* shl ax,cl (ah was 0) */
        ah = (uint8_t)(ax >> 8);
    }
    uint16_t si = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_DCNT) >> 2); /* dskshf */
    dx = (uint16_t)(dx & si);
    uint16_t sum = (uint16_t)(bx + dx);
    uint8_t carry = (sum < bx) ? 1 : 0; /* adc ah,0 */
    ah = (uint8_t)(ah + carry);
    mem_w16_seg(mem, ds_bdos, BD_ARECORD, sum);
    mem_w8_seg(mem, ds_bdos,BD_ARECORD_HI, ah);
    seek(mem);
}

/*
 * seek -- seek the track given by BD_ARECORD (actual record). Also
 * reached as seekdir's own fallthrough tail, not just directly.
 */
void seek(uint8_t *mem) {
    (void)mem;
    /* mov ax,arecord!xor dx,dx!mov dl,byte ptr arecord+2!div sectpt --
     * computes track/sector, fed only into settrkf/sectran/setsecf,
     * all 3 boundary stubs with no observable effect under the
     * convention -- nothing here is observable either way, see header
     * comment */
}

/*
 * dmposition -- compute disk map position for vrecord
 *	output:	returns the dm$position value
 * Not `static`: `src-c/kernel/file3.c`'s own `diskwrite` also calls
 * `DMPOSITION` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`.
 */
uint8_t dmposition(const uint8_t *mem, uint16_t ds_bdos) {
    uint8_t cl = mem_r8_seg(mem, ds_bdos,BD_BLKSHF);
    uint8_t al = (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_VRECORD) >> cl);
    uint8_t ch = al;
    cl = (uint8_t)(7 - mem_r8_seg(mem, ds_bdos,BD_BLKSHF));
    al = mem_r8_seg(mem, ds_bdos,BD_EXTVAL);
    al = (uint8_t)(al << cl);
    al = (uint8_t)(al + ch);
    return al;
}

/* GET_DMA -- bx = .fcb(dskmap) */
static uint16_t get_dma(const uint8_t *mem, uint16_t ds_bdos) {
    return (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + fcb_dskmap);
}

/*
 * getdm -- return disk map value from position cx. Not `static`:
 * `src-c/kernel/file3.c`'s own `diskwrite` also calls `GETDM`
 * directly, an ordinary cross-file call, same as `que1.c`'s own into
 * `que2.c`.
 */
uint16_t getdm(const uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    uint16_t bx = (uint16_t)(get_dma(mem, ds_bdos) + cl);
    if (mem_r8_seg(mem, ds_bdos,BD_SINGLE) == 0) {
        uint16_t bx2 = (uint16_t)(bx + cl);
        return mem_r16(mem, bx2);
    }
    return mem[bx];
}

/* index -- compute disk block number from the current fcb. Named
 * bdos_index here, not index: clang warns the plain name collides
 * with the C library's own (deprecated) strings.h index() */
void bdos_index(uint8_t *mem, uint16_t ds_bdos) {
    uint8_t al = dmposition(mem, ds_bdos);
    uint16_t bx = getdm(mem, ds_bdos, al);
    mem_w16_seg(mem, ds_bdos, BD_ARECORD, bx);
}

/* alloct -- called after index to see if the block is allocated */
uint16_t alloct(const uint8_t *mem, uint16_t ds_bdos) {
    return mem_r16_seg(mem, ds_bdos, BD_ARECORD);
}

/* atran -- compute the actual record address, assuming index was called */
void atran(uint8_t *mem, uint16_t ds_bdos) {
    uint8_t cl = mem_r8_seg(mem, ds_bdos,BD_BLKSHF);
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_ARECORD);
    uint8_t al = (uint8_t)(bx >> 8); /* mov al,bh */
    bx = (uint16_t)(bx << cl); /* shl bx,cl */
    uint16_t ax = (uint16_t)((uint16_t)al << cl); /* shl ax,cl (ah was 0) */
    uint8_t ah = (uint8_t)(ax >> 8);
    mem_w16_seg(mem, ds_bdos, BD_ABLOCK, bx);
    al = mem_r8_seg(mem, ds_bdos,BD_VRECORD);
    al = (uint8_t)(al & mem_r8_seg(mem, ds_bdos,BD_BLKMSK));
    uint8_t bl = (uint8_t)((bx & 0xFF) | al); /* or bl,al */
    bx = (uint16_t)((bx & 0xFF00) | bl);
    mem_w16_seg(mem, ds_bdos, BD_ARECORD, bx);
    mem_w8_seg(mem, ds_bdos,BD_ARECORD_HI, ah);
}

/*
 * GET_ATTS -- get interface attributes (F5'-F8') from the fcb,
 * returned left-shifted in al; zeroes the interface attribute bits
 * in the fcb
 */
uint8_t get_atts(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + 5);
    uint8_t dh = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t al = mem[bx];
        uint8_t ch = al;
        uint8_t carry = (uint8_t)(al >> 7);
        al = (uint8_t)(al << 1);
        dh = (uint8_t)((dh << 1) | carry);
        ch = (uint8_t)(ch & 0x7F);
        mem[bx] = ch;
        bx = (uint16_t)(bx + 1);
    }
    return (uint8_t)(dh << 4);
}

/* getexta -- get the current extent field address to al (bx out).
 * Not static: src-c/kernel/file2.c's own CLOSE_FCB/CLOSE1 cluster
 * calls it too (an ordinary cross-file call, same convention as
 * que1.c's own cross-file calls into que2.c). */
uint16_t getexta(const uint8_t *mem, uint16_t ds_bdos) {
    return (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + fcb_extnum);
}

/* GET_S1 -- get the current S1 field to al */
uint8_t get_s1(const uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = (uint16_t)(getexta(mem, ds_bdos) + 1);
    return mem[bx];
}

/* GET_RRA -- get the current ran-rec field address to bx */
uint16_t get_rra(const uint8_t *mem, uint16_t ds_bdos) {
    return (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + fcb_ranrec);
}

/* GET_RCNTA -- get the reccnt field address to bx */
uint16_t get_rcnta(const uint8_t *mem, uint16_t ds_bdos) {
    return (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + fcb_reccnt);
}

/* gtfcba -- compute reccnt and nxtrec addresses for get/setfcb */
static void gtfcba(const uint8_t *mem, uint16_t ds_bdos, uint16_t *reccnt_addr, uint16_t *nxtrec_addr) {
    uint16_t info = mem_r16_seg(mem, ds_bdos, BD_INFO);
    *reccnt_addr = (uint16_t)(info + fcb_reccnt);
    *nxtrec_addr = (uint16_t)(info + fcb_nxtrec);
}

/* getfcb -- set BDOS scratch variables from the currently addressed fcb */
void getfcb(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t reccnt_addr, nxtrec_addr;
    gtfcba(mem, ds_bdos, &reccnt_addr, &nxtrec_addr);
    mem_w8_seg(mem, ds_bdos,BD_VRECORD, mem[nxtrec_addr]);
    mem_w8_seg(mem, ds_bdos,BD_RCOUNT, mem[reccnt_addr]);
    uint16_t bx = getexta(mem, ds_bdos);
    mem_w8_seg(mem, ds_bdos,BD_EXTVAL, (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_EXTMSK) & mem[bx]));
}

/* setfcb -- place BDOS scratch values back into the current fcb */
void setfcb(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t reccnt_addr, nxtrec_addr;
    gtfcba(mem, ds_bdos, &reccnt_addr, &nxtrec_addr);
    uint8_t al = mem_r8_seg(mem, ds_bdos,BD_SEQIO);
    if (al == 2) {
        al = 0;
    }
    al = (uint8_t)(al + mem_r8_seg(mem, ds_bdos,BD_VRECORD));
    mem[nxtrec_addr] = al;
    mem[reccnt_addr] = mem_r8_seg(mem, ds_bdos,BD_RCOUNT);
}

/* cmpec0 -- checksum cl bytes starting at bx */
static uint8_t cmpec0(const uint8_t *mem, uint16_t bx, uint16_t cx) {
    uint8_t al = 0;
    for (uint16_t i = 0; i < cx; i++) {
        al = (uint8_t)(al + mem[(uint16_t)(bx + i)]);
    }
    return al;
}

/* cmpecs -- compute checksum for the current directory buffer */
uint8_t cmpecs(const uint8_t *mem, uint16_t ds_bdos) {
    return cmpec0(mem,mem_seg_addr(ds_bdos, mem_r16_seg(mem, ds_bdos, BD_BUFFA)), 128 /* recsiz */);
}

/*
 * CHKSUM_FCB -- compute checksum for fcb: add the first 12 bytes of
 * the fcb, plus BD_PD_CNT/BD_HIGH_EXT/BD_XFCB_READ_ONLY/BD_CURDSK
 * (4 contiguous bytes, see header comment), plus S1, plus the 16-byte
 * disk map, plus a bias byte
 *	output:	returns the checksum (0 = valid)
 */
uint8_t chksum_fcb(const uint8_t *mem, uint16_t ds_bdos) {
    uint8_t al = cmpec0(mem,mem_seg_addr(ds_bdos, BD_PD_CNT), 4);
    al = (uint8_t)(al + 0xBB);
    al = (uint8_t)(al + cmpec0(mem,mem_seg_addr(ds_bdos, mem_r16_seg(mem, ds_bdos, BD_INFO)), 12));
    /* "inc bx" after that 12-byte CMPEC0 call lands on INFO+12 (where
     * the loop's own bx ended up, not back at INFO), i.e. INFO+fcb_
     * extnum+1 -- the same address GET_S1 computes */
    uint16_t bx = (uint16_t)(getexta(mem, ds_bdos) + 1);
    al = (uint8_t)(al + mem[bx]); /* S1 */
    bx = (uint16_t)(bx + 3); /* INFO+16 == INFO+fcb_dskmap */
    al = (uint8_t)(al + cmpec0(mem, bx, 16)); /* disk map */
    return al;
}

/* SET_CHKSUM_FCB -- compute the fcb checksum; if invalid, fix S1 to match */
void set_chksum_fcb(uint8_t *mem, uint16_t ds_bdos) {
    uint8_t al = chksum_fcb(mem, ds_bdos);
    if (al == 0) {
        return; /* RET45 */
    }
    uint8_t ah = al;
    al = get_s1(mem, ds_bdos);
    ah = (uint8_t)(ah - al);
    ah = (uint8_t)(-(int8_t)ah);
    uint16_t bx = (uint16_t)(getexta(mem, ds_bdos) + 1);
    mem[bx] = ah;
}

/* RESET_CHECKSUM_FCB -- invalidate S1 if the fcb checksum is valid */
void reset_checksum_fcb(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_COMP_FCB_CKS, 0);
    if (chksum_fcb(mem, ds_bdos) != 0) {
        return; /* RET45 */
    }
    uint16_t bx = (uint16_t)(getexta(mem, ds_bdos) + 1);
    mem[bx] += 1;
}

/*
 * CHEK_FCB -- as CHKSUM_FCB, but first zeroes fcb(0) if high_ext==0x60
 */
uint8_t chek_fcb(uint8_t *mem, uint16_t ds_bdos) {
    if (mem_r8_seg(mem, ds_bdos,BD_HIGH_EXT) == 0x60) {
        mem_w8_seg(mem, ds_bdos,mem_r16_seg(mem, ds_bdos, BD_INFO), 0);
    }
    return chksum_fcb(mem, ds_bdos);
}

extern void close1(uint8_t *mem, uint16_t ds_bdos);
extern void pack_sdcnt(uint8_t *mem, uint16_t ds_bdos);
extern uint8_t search_olist(uint8_t *mem, uint16_t ds_bdos, uint8_t ch);

/*
 * check_fcb1 -- CHECK_FCB1: verify the currently addressed fcb's own
 * checksum (chek_fcb), applying the "invalid checksum, but one open
 * copy might just be stale" MPM recovery (a partial `close1`, then a
 * locked-file search) before giving up for real.
 *	output: returns 1 if the checksum is valid (including after a
 *		successful recovery) -- a plain, ordinary return in the
 *		real asm too (`RET45`). Returns 0 otherwise; the real
 *		asm reaches that case by first discarding its own return
 *		address (`POP BX ;DISCARD RETURN ADDRESS`) so that its
 *		own eventual `ret` (immediately, if the caller set
 *		`BD_CHECK_FCB_RET`, or via `staret` after setting
 *		`BD_LRET=10` otherwise) lands one call frame higher than
 *		wherever `CHECK_FCB1` was actually called from -- same
 *		category of manual stack trick as `PARSE.SUP`'s own
 *		`delim` (see `parse.c`'s own header comment). Both of the
 *		real callers (`FILE4.BDO`'s own `OPEN`/`CLOSE` MPM
 *		branches, not yet ported) manually push the address they
 *		want control to land at instead of their own normal
 *		return address, right before calling `CHECK_FCB1`, and
 *		pop it right back off immediately after (discarded, since
 *		that's only reached on a valid checksum) -- so a 0
 *		return here just means "do the same thing: skip straight
 *		to your own designated fallback instead of continuing
 *		normally", without this function literally touching the
 *		call stack to get there.
 *
 * `CHECK_FCB` itself (`CHECK_FCB1`'s own caller, which first sets
 * `BD_CHECK_FCB_RET=false` before falling into `CHECK_FCB1`) is
 * NOT dead -- an earlier revision of this comment claimed it was,
 * since `func15`/`func16` both set `BD_CHECK_FCB_RET=true` and call
 * `CHECK_FCB1` directly, but `func20`/`func21` (sequential read/
 * write, `src-c/kernel/file4.c`) call `CHECK_FCB` itself, with
 * `BD_CHECK_FCB_RET` left false. Same boolean-return convention
 * either way: a 0 there means "stop now," matching what both those
 * callers already do unconditionally on failure.
 */
uint8_t check_fcb1(uint8_t *mem, uint16_t ds_bdos) {
    uint8_t al = chek_fcb(mem, ds_bdos);
    if (al == 0) {
        return 1; /* ret45 -- valid */
    }
    if ((al & 0x0F) != 0) {
        goto check_fcb3;
    }
    if (mem_r8_seg(mem, ds_bdos,BD_PD_CNT) == 0) {
        goto check_fcb3;
    }
    mem_w8_seg(mem, ds_bdos,(uint16_t)(BD_SDCNT + 1), 0xFF);
    mem_w8_seg(mem, ds_bdos,BD_DONT_CLOSE, 0xFF);
    close1(mem, ds_bdos);
    mem_w8_seg(mem, ds_bdos,BD_LRET, (uint8_t)(mem_r8_seg(mem, ds_bdos,BD_LRET) + 1));
    if (mem_r8_seg(mem, ds_bdos,BD_LRET) == 0) {
        goto check_fcb3; /* partial close failed */
    }
    mem_w8_seg(mem, ds_bdos,BD_LRET, 0);
    pack_sdcnt(mem, ds_bdos);
    if (search_olist(mem, ds_bdos, 5)) {
        return 1; /* found -- checksum ok */
    }
check_fcb3:
    if (!mem_r8_seg(mem, ds_bdos,BD_CHECK_FCB_RET)) {
        mem_w8_seg(mem, ds_bdos,BD_LRET, 10);
    }
    return 0;
}

/*
 * check_fcb -- CHECK_FCB: see check_fcb1's own header comment --
 * `func20`/`func21` (`file4.c`) are its own two real callers.
 */
uint8_t check_fcb(uint8_t *mem, uint16_t ds_bdos) {
    mem_w8_seg(mem, ds_bdos,BD_CHECK_FCB_RET, 0);
    return check_fcb1(mem, ds_bdos);
}

/*
 * set_cdisk1 -- SET_CDISK1: set a "1" bit in al's own position of cx.
 * Not `static`: `src-c/kernel/file3.c`'s own `diskreset` also calls
 * `SET_CDISK1` directly (with `al=BD_CURDSK`, not `BD_SELDSK`), an
 * ordinary cross-file call, same as `que1.c`'s own into `que2.c`.
 */
/* `SET_CDISK1` lands at 0x8c5 in the combined wrapper bundle -- inside
 * DATA.BDO's own dseg collision range (0x000-0x1000, same class of
 * gap as `searchn`'s own header comment, `file2.c`) -- so, same as
 * that, `diskreset`'s own cluster (`file3.c`/`file4.c`) gets no new
 * differential test either. */
uint16_t set_cdisk1(uint8_t al, uint16_t cx) {
    uint16_t bx = (uint16_t)(1u << al);
    return (uint16_t)(bx | cx);
}

/* setcdisk -- set a "1" bit in BD_SELDSK's own position of cx */
uint16_t setcdisk(const uint8_t *mem, uint16_t ds_bdos, uint16_t cx) {
    return set_cdisk1(mem_r8_seg(mem, ds_bdos,BD_SELDSK), cx);
}

/* nowrite -- return non-zero if the current disk is in the r/o vector */
uint8_t nowrite(const uint8_t *mem, uint16_t ds_bdos) {
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_RODSK);
    uint8_t cl = mem_r8_seg(mem, ds_bdos,BD_SELDSK);
    dx = (uint16_t)(dx >> cl);
    return (uint8_t)(dx & 1);
}

/*
 * test_vector1 -- TEST_VECTOR1: shift dx right by cl bits, return
 * bit 0. Not `static`: `src-c/kernel/file3.c`'s own `diskreset` also
 * calls `TEST_VECTOR1` directly (with `cl=BD_CURDSK`, already set to
 * the disk being reset, not `BD_SELDSK`), an ordinary cross-file
 * call, same as `que1.c`'s own into `que2.c`.
 */
uint16_t test_vector1(uint16_t dx, uint8_t cl) {
    dx = (uint16_t)(dx >> cl);
    return (uint16_t)(dx & 1);
}

/*
 * test_vector -- TEST_VECTOR: shift dx right by BD_SELDSK bits,
 * return bit 0. `nowrite` above is this same shift, just always
 * applied to BD_RODSK specifically; this is the general entry
 * `src-c/kernel/file2.c`'s own `initialize`/`src-c/kernel/file3.c`'s
 * own `curselect` call with other vectors (BD_DLOG/BD_TLOG/BD_RLOG).
 */
uint16_t test_vector(const uint8_t *mem, uint16_t ds_bdos, uint16_t dx) {
    return test_vector1(dx, mem_r8_seg(mem, ds_bdos,BD_SELDSK));
}

/* remove_drive -- REMOVE_DRIVE: clear BD_CURDSK's own bit in bx */
uint16_t remove_drive(const uint8_t *mem, uint16_t ds_bdos, uint16_t bx) {
    uint8_t cl = mem_r8_seg(mem, ds_bdos,BD_CURDSK);
    uint16_t dx = (uint16_t)(1u << cl);
    return (uint16_t)(bx & (uint16_t)~dx);
}

/* setro -- set the current disk to read-only */
void setro(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = setcdisk(mem, ds_bdos, mem_r16_seg(mem, ds_bdos, BD_RODSK));
    mem_w16_seg(mem, ds_bdos, BD_RODSK, bx);
    uint16_t dx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_DIRMAX) + 1);
    bx = mem_r16_seg(mem, ds_bdos, BD_CDRMAXA);
    mem_w16(mem, bx, dx);
}

extern void flush_files(uint8_t *mem, uint16_t ds_bdos);
extern void flush_file0(uint8_t *mem, uint16_t ds_bdos);
uint8_t compcdr(const uint8_t *mem, uint16_t ds_bdos);
void checkwrite(uint8_t *mem, uint16_t ds_bdos);
void setenddir(uint8_t *mem, uint16_t ds_bdos);

/*
 * checksum/newchecksum -- compute (or, if cl==true, initialize) the
 * current directory record's own checksum; on a mismatch, flush the
 * files open on this disk and mark it read-only (unless it's already
 * read-only, in which case just flush) -- now portable since
 * FLUSH_FILES/FLUSH_FILE0 (src-c/kernel/file3.c) are.
 *	input:	cl = true (0xFF) to initialize, or (MPM-specific) 0xFE
 *		for the "directory checksum" variant (TEST_DIR_CS)
 */
void checksum(uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_ARECORD);
    uint16_t bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_CHKSIZ) & 0x7FFF); /* and bh,7fh */
    if (dx >= bx) {
        return; /* ret6 -- past the checksum vector's own size */
    }
    uint8_t al = cmpecs(mem, ds_bdos);
    bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_ARECORD) + mem_r16_seg(mem, ds_bdos, BD_CHECKA));
    cl = (uint8_t)(cl + 1);
    if (cl == 0) {
        mem[bx] = al; /* initcs */
        return;
    }
    cl = (uint8_t)(cl + 1);
    if (cl == 0) {
        /* TEST_DIR_CS */
        if (al != mem[bx]) {
            flush_files(mem, ds_bdos);
        }
        return;
    }
    if (al == mem[bx]) {
        return; /* ret7 */
    }
    if (!compcdr(mem, ds_bdos)) {
        return; /* ret7 */
    }
    if (nowrite(mem, ds_bdos)) {
        return; /* ret7 */
    }
    flush_file0(mem, ds_bdos);
    setro(mem, ds_bdos);
}

void newchecksum(uint8_t *mem, uint16_t ds_bdos) {
    checksum(mem, ds_bdos, 0xFF); /* true */
}

/* rddirbuf, declared ahead for rddir0's own use below */
void rddirbuf(uint8_t *mem, uint16_t ds_bdos);
void seekdir(uint8_t *mem, uint16_t ds_bdos);

/*
 * rddir0 -- seek and read the next directory record if the current
 * entry starts one (dcnt's own low byte lands on a record boundary),
 * then checksum it
 */
void rddir0(uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    uint8_t al = (uint8_t)((mem_r8_seg(mem, ds_bdos,BD_DCNT) & dskmsk) << fcbshf);
    mem_w8_seg(mem, ds_bdos,BD_DPTR, al);
    if (al != 0) {
        return; /* ret71 -- not a new record */
    }
    seekdir(mem, ds_bdos);
    rddirbuf(mem, ds_bdos);
    checksum(mem, ds_bdos, cl);
}

/*
 * rddir -- read the next directory entry (cl = true if initializing)
 */
void rddir(uint8_t *mem, uint16_t ds_bdos, uint8_t cl) {
    uint16_t dirmax = mem_r16_seg(mem, ds_bdos, BD_DIRMAX);
    uint16_t dcnt = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_DCNT) + 1);
    mem_w16_seg(mem, ds_bdos, BD_DCNT, dcnt);
    if (dirmax < dcnt) {
        setenddir(mem, ds_bdos);
        return;
    }
    rddir0(mem, ds_bdos, cl);
}

/* wrdir -- write the current directory entry, setting its checksum */
void wrdir(uint8_t *mem, uint16_t ds_bdos) {
    checkwrite(mem, ds_bdos);
    newchecksum(mem, ds_bdos);
    /* call setdir -- boundary stub, no-op, see header comment */
    wrbuff(mem, ds_bdos);
    /* jmp setdata -- boundary stub, no-op */
}

/*
 * RO_TEST -- test a directory element/fcb for r/o status, carry set
 * if r/o. Not `static`: `src-c/kernel/file4.c`'s own `func15` (BDOS
 * OPEN) also calls `RO_TEST` directly, an ordinary cross-file call,
 * same as `que1.c`'s own into `que2.c`.
 */
uint8_t ro_test(const uint8_t *mem, uint16_t bx) {
    bx = (uint16_t)(bx + fcb_type); /* rofile == fcb_type's own offset */
    uint8_t al = mem[bx];
    return (uint8_t)(al >> 7);
}

/* getdptra -- compute the address of a directory element at BD_DPTR */
uint16_t getdptra(const uint8_t *mem, uint16_t ds_bdos) {
    return (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_BUFFA) + mem_r8_seg(mem, ds_bdos,BD_DPTR));
}

/* ckrodir/ckrofile -- check a directory element/fcb for r/o status */
void ckrofile(uint8_t *mem, uint16_t ds_bdos, uint16_t bx) {
    if (!ro_test(mem, bx)) {
        return; /* ret5 */
    }
    roferror(mem, ds_bdos);
}

void ckrodir(uint8_t *mem, uint16_t ds_bdos) {
    ckrofile(mem, ds_bdos, getdptra(mem, ds_bdos));
}

/* checkwrite -- check for a write-protected disk */
void checkwrite(uint8_t *mem, uint16_t ds_bdos) {
    if (nowrite(mem, ds_bdos) == 0) {
        return; /* ret5 */
    }
    roderror(mem, ds_bdos);
}

/* getmodnum -- bx = .fcb(modnum); returns fcb(modnum) */
uint8_t getmodnum(const uint8_t *mem, uint16_t ds_bdos, uint16_t *bx_out) {
    uint16_t bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_INFO) + fcb_modnum);
    *bx_out = bx;
    return mem[bx];
}

/* clrmodnum -- clear the module number field for user open/make */
void clrmodnum(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx;
    (void)getmodnum(mem, ds_bdos, &bx);
    mem[bx] = 0;
}

/* CLR_EXT -- clear the high bits of the extent field */
void clr_ext(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = getexta(mem, ds_bdos);
    mem[bx] = (uint8_t)(mem[bx] & 0x1F);
}

/* setfwf -- set the file-write-flag bit in fcb(modnum) */
uint8_t setfwf(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx;
    uint8_t al = getmodnum(mem, ds_bdos, &bx);
    al = (uint8_t)(al | fwfmsk);
    mem[bx] = al;
    return al;
}

/* compcdr -- return non-zero (carry) if cdrmax > dcnt */
uint8_t compcdr(const uint8_t *mem, uint16_t ds_bdos) {
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_DCNT);
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_CDRMAXA);
    return (dx < mem_r16(mem, bx)) ? 1 : 0;
}

/* setcdr -- if not (cdrmax > dcnt), set cdrmax = dcnt+1 */
void setcdr(uint8_t *mem, uint16_t ds_bdos) {
    if (compcdr(mem, ds_bdos)) {
        return;
    }
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_CDRMAXA);
    uint16_t dx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_DCNT) + 1);
    mem_w16(mem, bx, dx);
}

/* subdh -- compute dx - bx. Not a standalone function here: its only
 * real use (checksum's own "drec < chksiz?" size check, reached
 * transitively from rddir0/rddir too) folds directly into checksum's
 * own `if (dx >= bx) return;` above, see this file's own header
 * comment */

/* rddirbuf -- read a directory entry into the directory buffer */
void rddirbuf(uint8_t *mem, uint16_t ds_bdos) {
    /* setdir!rdbuff -- both boundary-stub-only under the convention
     * (setdir's own setdmbf/setdma calls and rdbuff's own readf call
     * have no observable effect), see header comment */
    rdbuff(mem, ds_bdos);
}

/* setdata/setdir/setdma -- all 3 are pure boundary-stub setup (XIOS
 * setdmbf/setdmf calls), no observable effect under the convention */

/* dirtouser -- copy the directory entry to the user buffer */
void dirtouser(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t dx = mem_r16_seg(mem, ds_bdos, BD_BUFFA);
    uint16_t bx = mem_r16_seg(mem, ds_bdos, BD_DMAAD);
    (void)mem_r16_seg(mem, ds_bdos, BD_PARAMETERSEGMENT); /* es switch -- moot, flat model */
    move(mem, dx, bx, 128 /* recsiz */);
}

/* MAKE_FCB_INV -- flag an fcb as invalid */
void make_fcb_inv(uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx;
    (void)setfwf(mem, ds_bdos); /* also recomputes bx below via getmodnum's own math */
    (void)getmodnum(mem, ds_bdos, &bx);
    bx = (uint16_t)(bx + 2);
    mem_w16(mem, bx, 0xFFFF);
}

/* CHK_INV_FCB -- returns non-zero if the fcb's dskmap word isn't 0xFFFF */
uint16_t chk_inv_fcb(const uint8_t *mem, uint16_t ds_bdos) {
    uint16_t bx = get_dma(mem, ds_bdos);
    return (uint16_t)(mem_r16(mem, bx) ^ 0xFFFF);
}

/*
 * TST_INV_FCB -- test for an invalid fcb
 *	output:	returns 0 (ret8) if the fcb is valid; sets lret=9 and
 *		returns 9 if invalid
 */
uint8_t tst_inv_fcb(uint8_t *mem, uint16_t ds_bdos) {
    if (chk_inv_fcb(mem, ds_bdos) != 0) {
        return 0; /* ret8 */
    }
    mem_w8_seg(mem, ds_bdos,BD_LRET, 9); /* mov al,9!jmp staret */
    return 9;
}

/* endofdir/TEST_FFFF -- zero flag set if at end of directory */
uint8_t endofdir(const uint8_t *mem, uint16_t ds_bdos) {
    return (mem_r16_seg(mem, ds_bdos, BD_DCNT) == 0xFFFF) ? 1 : 0;
}

/* setenddir -- set dcnt to the end of the directory */
void setenddir(uint8_t *mem, uint16_t ds_bdos) {
    mem_w16_seg(mem, ds_bdos, BD_DCNT, 0xFFFF); /* enddir */
}

/*
 * getallocbit -- given an allocation-vector bit position (cx), return
 * the containing byte rotated so the bit is in al's own low position
 *	output:	returns {al, dh} (al = rotated byte, dh = shift count);
 *		*bx_out = the byte's own address
 */
uint8_t getallocbit(const uint8_t *mem, uint16_t ds_bdos, uint16_t cx, uint16_t *bx_out, uint8_t *dh_out) {
    uint8_t cl = (uint8_t)((cx & 0x07) + 1);
    uint8_t ch = cl;
    (void)ch;
    uint16_t dx = (uint16_t)(cx >> 3);
    uint16_t bx = (uint16_t)(mem_r16_seg(mem, ds_bdos, BD_ALLOCA) + dx);
    uint8_t al = mem[bx];
    al = (uint8_t)(((al << cl) | (al >> (8 - cl))) & 0xFF); /* rol al,cl */
    *bx_out = bx;
    *dh_out = cl;
    return al;
}

/* setallocbit -- set or reset the allocation bit at position cx to dl */
void setallocbit(uint8_t *mem, uint16_t ds_bdos, uint16_t cx, uint8_t dl) {
    uint16_t bx;
    uint8_t dh;
    uint8_t al = getallocbit(mem, ds_bdos, cx, &bx, &dh);
    al = (uint8_t)(al & 0xFE);
    al = (uint8_t)(al | (dl & 1));
    /* rotr: ror al,dh and store */
    al = (uint8_t)(((al >> dh) | (al << (8 - dh))) & 0xFF);
    mem[bx] = al;
}
