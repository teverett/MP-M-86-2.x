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
 * Runs against real memory (defs.h's far_ptr), like the rest of the
 * kernel: every function takes `phys` (the base of real physical memory)
 * and `ds_bdos`, the BDOS's DS -- the combined SYSDAT+DATA.BDO segment
 * SUPIF enters the BDOS with, so `BD_*` are DATA.BDO's real offsets and
 * `rlr`/PDs/`tod` (SYSDAT_*) are reached through it too. Everything the
 * BDOS works on is DS-relative -- its own copy of the caller's FCB
 * (`parsave` copies it in), directory buffers, DPB/allocation/checksum
 * vectors -- except where the asm switches segments: the caller's
 * parameter segment (`parsave`, `dirtouser`), the DMA segment
 * (`copy_dma_in`/`copy_dma_out`, func18), and the running process's UDA
 * (file4.c's u_* fields, flush_files' per-PD u_pd_cnt). `move` copies
 * DS:si to ES:di, and ES is DS in every BDOS caller of it except
 * `dirtouser`, which copies to the user segment itself.
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
void move(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t bx, uint8_t cl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    for (uint8_t i = 0; i < cl; i++) {
        ds[(uint16_t)(bx + i)] = ds[(uint16_t)(dx + i)];
    }
}

void setdata(uint8_t *phys, uint16_t ds_bdos);
void setdir(uint8_t *phys, uint16_t ds_bdos);

/* BDOSIF.BDO's xiosif and mpmif, and GOBACK's target (bdosif.c) --
 * see defs.h */
xiosif_fn bdos_xiosif;
mpmif_fn bdos_mpmif;
void bdos_set_xiosif(xiosif_fn fn) { bdos_xiosif = fn; }
void bdos_set_mpmif(mpmif_fn fn) { bdos_mpmif = fn; }

/* the BDOS - XIOS interface (BDOSIF.BDO's homef, seldskf, ...): AL =
 * the function, CX/DX its parameters; returns AX (= BX). With no XIOS
 * hook (the host tests) every call returns 0, so SELDSK fails. */
static uint16_t xios(uint8_t *phys, uint16_t ds_bdos, uint8_t al, uint16_t cx, uint16_t dx) {
    return XIOSIF(bdos, phys, ds_bdos, al, cx, dx);
}

/* XCRLF/XPRINT (BDOSIF.BDO): print the string at DS:dx (cx for XPRINT) */
static void xprint(uint8_t *phys, uint16_t ds_bdos, uint16_t dx) {
    (void)MPMIF(bdos, phys, ds_bdos, f_conprint, dx, 0, ds_bdos);
}
static void xcrlf(uint8_t *phys, uint16_t ds_bdos) { xprint(phys, ds_bdos, BD_CRLFSTR); }

/*
 * selectdisk -- select the disk drive given by BD_SELDSK (dl: bit 0 =
 * 0 if not logged in yet, for the XIOS), and fill cdrmaxa..alloca from
 * its disk header and sectpt..offsetv from its DPB.
 *	output: nonzero if the select worked (the asm's NZ)
 */
uint8_t selectdisk(uint8_t *phys, uint16_t ds_bdos, uint8_t dl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, BD_CURDSK, 0xFF);
    uint16_t bx = xios(phys, ds_bdos, io_seldsk, seg_r8(ds, BD_SELDSK), dl); /* bx = 0000 if error */
    if (bx == 0) {
        return 0; /* ret4 */
    }
    uint16_t dx = seg_r16(ds, bx); /* .tran */
    bx = (uint16_t)(bx + 2);
    seg_w16(ds, BD_CDRMAXA, bx);
    bx = (uint16_t)(bx + 2);
    seg_w16(ds, BD_DRVLBLA, bx);
    bx = (uint16_t)(bx + 4);
    seg_w16(ds, BD_TRANV, dx);
    move(phys, ds_bdos, bx, BD_BUFFA, bd_addlist); /* addlist filled */
    move(phys, ds_bdos, seg_r16(ds, BD_DPBADDR), BD_SECTPT, 0x0F); /* dpblist */
    seg_w8(ds, BD_SINGLE, (seg_r8(ds, (uint16_t)(BD_MAXALL + 1)) == 0) ? 0xFF : 0);
    /* retselect */
    uint8_t al = seg_r8(ds, BD_SELDSK);
    seg_w8(ds, BD_CURDSK, al);
    return (uint8_t)(al + 1);
}

/*
 * home -- move to home position, reset the directory block
 */
void home(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    (void)xios(phys, ds_bdos, io_home, 0, 0); /* call homef */
    seg_w16(ds, BD_DBLK, 0);
}

/*
 * PASS_ARECORD -- returns {dx, ch} = BD_ARECORD's own low word/high byte
 */
static void pass_arecord(uint8_t *phys, uint16_t ds_bdos, uint16_t *dx_out, uint8_t *ch_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    *dx_out = seg_r16(ds, BD_ARECORD);
    *ch_out = seg_r8(ds, BD_ARECORD_HI);
}

void pererror(uint8_t *phys, uint16_t ds_bdos);
void roderror(uint8_t *phys, uint16_t ds_bdos);

/*
 * diocomp -- check readf/writef's result (al) for disk errors: 2 is a
 * read/only disk, anything else nonzero a permanent error (both GOBACK,
 * so they don't return in a real BDOS call)
 *	output:	returns al
 * Not `static`: `src-c/kernel/file4.c`'s own `func48` (BDOS flush)
 * also calls `DIOCOMP` directly, an ordinary cross-file call, same
 * as `que1.c`'s own into `que2.c`.
 */
uint8_t diocomp(uint8_t *phys, uint16_t ds_bdos, uint8_t al) {
    if (al == 0) return 0; /* ret4 */
    if (al == 2) roderror(phys, ds_bdos);
    else pererror(phys, ds_bdos);
    return al;
}

/* rdbuff -- read buffer and check if ok: readf with ch:dx = arecord */
void rdbuff(uint8_t *phys, uint16_t ds_bdos) {
    uint16_t dx;
    uint8_t ch;
    pass_arecord(phys, ds_bdos, &dx, &ch);
    uint8_t al = (uint8_t)xios(phys, ds_bdos, io_read, (uint16_t)(ch << 8), dx);
    (void)diocomp(phys, ds_bdos, al);
}

/* wrbuff/wrbufflg -- write buffer and check condition; cl = the write
 * type (0 normal, 1 directory, 2 start of a new block), or'd with
 * rem_drv for the XIOS */
void wrbuff(uint8_t *phys, uint16_t ds_bdos, uint8_t cl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dx;
    uint8_t ch;
    pass_arecord(phys, ds_bdos, &dx, &ch);
    cl = (uint8_t)(cl | seg_r8(ds, BD_REM_DRV));
    uint8_t al = (uint8_t)xios(phys, ds_bdos, io_write, (uint16_t)((ch << 8) | cl), dx);
    (void)diocomp(phys, ds_bdos, al);
}

void wrbufflg(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    wrbuff(phys, ds_bdos, seg_r8(ds, BD_WFLAG)); /* mov cl,wflag */
}

/* RTN_PHY_ERRS: aret = 0FFFFh if return_ffff was set; then GOBACK */
static void rtn_phy_errs(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (seg_r8(ds, BD_RETURN_FFFF) != 0) {
        seg_w8(ds, BD_RETURN_FFFF, 0);
        seg_w16(ds, BD_ARET, 0xFFFF);
    }
    bdos_goback();
}

/*
 * REPORT_ERR -- print "Bdos Err On D: <message>", then "Bdos Function
 * = nn" (and " File = name.typ" if reselect ran), set pf_ctlc unless
 * the error mode is "print and return", and RTN_PHY_ERRS.
 *	input: bx = the error message (a DS offset). Not static: SET_ARET
 *	(file4.c) prints the extended errors with it too.
 */
void report_err(uint8_t *phys, uint16_t ds_bdos, uint16_t bx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    xcrlf(phys, ds_bdos);
    seg_w8(ds, BD_DSKERR, (uint8_t)(seg_r8(ds, BD_SELDSK) + 'A')); /* the D: field */
    xprint(phys, ds_bdos, BD_DSKMSG); /* "Bdos Err On D:" */
    xprint(phys, ds_bdos, bx);        /* the error message */

    /* the function number in decimal */
    uint8_t al = seg_r8(ds, BD_FX);
    uint8_t ch = '0';
    uint16_t p = BD_PR_FX1;
    if (al >= 100) {
        ds[p++] = '1';
        al = (uint8_t)(al - 100);
    }
    for (;;) { /* rpt_err1: sub al,10 until it borrows */
        uint8_t borrow = al < 10;
        al = (uint8_t)(al - 10);
        if (borrow) break;
        ch++;
    }
    ds[p++] = ch;
    ds[p++] = (uint8_t)(al + 0x3A);
    ds[p] = ' ';
    ds[BD_PR_FCB] = 0; /* 0 = message delimiter */
    if (seg_r8(ds, BD_RESEL)) { /* reselect ran: the file name too */
        ds[BD_PR_FCB] = ' ';
        uint16_t si = (uint16_t)(seg_r16(ds, BD_INFO) + 1);
        move(phys, ds_bdos, si, BD_PR_FCB1, 8);
        ds[BD_PR_FCB1 + 8] = '.';
        move(phys, ds_bdos, (uint16_t)(si + 8), BD_PR_FCB1 + 9, 3);
    }
    xcrlf(phys, ds_bdos);
    xprint(phys, ds_bdos, BD_PR_FX); /* "Bdos Function = ### File = FFFFFFFF.TTT" */
    xcrlf(phys, ds_bdos);
    if (seg_r8(ds, BD_ERROR_MODE) != 0xFE) {
        uint16_t rlr = seg_r16(ds, SYSDAT_RLR);
        seg_w16(ds, (uint16_t)(rlr + p_flag),
                (uint16_t)(seg_r16(ds, (uint16_t)(rlr + p_flag)) | pf_ctlc));
    }
    rtn_phy_errs(phys, ds_bdos);
}

/*
 * goerr -- common tail for pererror/roderror/roferror/selerror
 *	input:	ch = message selector (1-4), bx its message
 */
static void goerr(uint8_t *phys, uint16_t ds_bdos, uint8_t ch, uint16_t bx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_ARET, (uint16_t)(((uint16_t)ch << 8) | 0xFF));
    if (seg_r8(ds, BD_ERROR_MODE) == 0xFF) {
        rtn_phy_errs(phys, ds_bdos); /* return the physical error to the user */
        return;
    }
    report_err(phys, ds_bdos, bx);
}

void pererror(uint8_t *phys, uint16_t ds_bdos) { goerr(phys, ds_bdos, 1, BD_PERMSG); }
void roderror(uint8_t *phys, uint16_t ds_bdos) { goerr(phys, ds_bdos, 2, BD_RODMSG); }
void roferror(uint8_t *phys, uint16_t ds_bdos) { goerr(phys, ds_bdos, 3, BD_ROFMSG); }
void selerror(uint8_t *phys, uint16_t ds_bdos) { goerr(phys, ds_bdos, 4, BD_SELMSG); }

void seek(uint8_t *phys, uint16_t ds_bdos);

/*
 * seekdir -- seek the record containing the current directory entry.
 * Has no real "ret" of its own -- falls straight through into seek's
 * own body (its only caller, rddir0 (deferred), expects exactly that:
 * a single `call seekdir` performs both the arecord computation below
 * and the actual seek), so this calls seek() as a tail, not just a
 * sibling function.
 */
void seekdir(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dx = 0xFFFF;
    uint8_t ah = 0;
    uint16_t bx = seg_r16(ds, BD_DBLK);
    if (bx != 0) {
        dx = (uint16_t)(dx + 1); /* inc dx -- wraps to 0 */
        uint8_t dl = seg_r8(ds, BD_BLKMSK);
        dx = (uint16_t)((dx & 0xFF00) | dl); /* mov dl,blkmsk */
        uint8_t cl = seg_r8(ds, BD_BLKSHF);
        uint8_t al = (uint8_t)(bx >> 8); /* mov al,bh */
        bx = (uint16_t)(bx << cl); /* shl bx,cl */
        uint16_t ax = (uint16_t)((uint16_t)al << cl); /* shl ax,cl (ah was 0) */
        ah = (uint8_t)(ax >> 8);
    }
    uint16_t si = (uint16_t)(seg_r16(ds, BD_DCNT) >> 2); /* dskshf */
    dx = (uint16_t)(dx & si);
    uint16_t sum = (uint16_t)(bx + dx);
    uint8_t carry = (sum < bx) ? 1 : 0; /* adc ah,0 */
    ah = (uint8_t)(ah + carry);
    seg_w16(ds, BD_ARECORD, sum);
    seg_w8(ds, BD_ARECORD_HI, ah);
    seek(phys, ds_bdos);
}

/*
 * seek -- seek the track given by BD_ARECORD (actual record). Also
 * reached as seekdir's own fallthrough tail, not just directly.
 */
void seek(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    /* dx:ax = arecord; div sectpt: dx = sector, ax = track */
    uint32_t arecord = (uint32_t)seg_r16(ds, BD_ARECORD) | ((uint32_t)seg_r8(ds, BD_ARECORD_HI) << 16);
    uint16_t sectpt = seg_r16(ds, BD_SECTPT);
    uint16_t track = sectpt ? (uint16_t)(arecord / sectpt) : 0;
    uint16_t sector = sectpt ? (uint16_t)(arecord % sectpt) : 0;
    (void)xios(phys, ds_bdos, io_settrk, (uint16_t)(track + seg_r16(ds, BD_OFFSETV)), 0);
    uint16_t bx = xios(phys, ds_bdos, io_sectran, sector, seg_r16(ds, BD_TRANV));
    (void)xios(phys, ds_bdos, io_setsec, bx, 0);
}

/*
 * dmposition -- compute disk map position for vrecord
 *	output:	returns the dm$position value
 * Not `static`: `src-c/kernel/file3.c`'s own `diskwrite` also calls
 * `DMPOSITION` directly, an ordinary cross-file call, same as
 * `que1.c`'s own into `que2.c`.
 */
uint8_t dmposition(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t cl = seg_r8(ds, BD_BLKSHF);
    uint8_t al = (uint8_t)(seg_r8(ds, BD_VRECORD) >> cl);
    uint8_t ch = al;
    cl = (uint8_t)(7 - seg_r8(ds, BD_BLKSHF));
    al = seg_r8(ds, BD_EXTVAL);
    al = (uint8_t)(al << cl);
    al = (uint8_t)(al + ch);
    return al;
}

/* GET_DMA -- bx = .fcb(dskmap) */
static uint16_t get_dma(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return (uint16_t)(seg_r16(ds, BD_INFO) + fcb_dskmap);
}

/*
 * getdm -- return disk map value from position cx. Not `static`:
 * `src-c/kernel/file3.c`'s own `diskwrite` also calls `GETDM`
 * directly, an ordinary cross-file call, same as `que1.c`'s own into
 * `que2.c`.
 */
uint16_t getdm(uint8_t *phys, uint16_t ds_bdos, uint8_t cl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = (uint16_t)(get_dma(phys, ds_bdos) + cl);
    if (seg_r8(ds, BD_SINGLE) == 0) {
        uint16_t bx2 = (uint16_t)(bx + cl);
        return seg_r16(ds, bx2);
    }
    return ds[bx];
}

/* index -- compute disk block number from the current fcb. Named
 * bdos_index here, not index: clang warns the plain name collides
 * with the C library's own (deprecated) strings.h index() */
void bdos_index(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = dmposition(phys, ds_bdos);
    uint16_t bx = getdm(phys, ds_bdos, al);
    seg_w16(ds, BD_ARECORD, bx);
}

/* alloct -- called after index to see if the block is allocated */
uint16_t alloct(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return seg_r16(ds, BD_ARECORD);
}

/* atran -- compute the actual record address, assuming index was called */
void atran(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t cl = seg_r8(ds, BD_BLKSHF);
    uint16_t bx = seg_r16(ds, BD_ARECORD);
    uint8_t al = (uint8_t)(bx >> 8); /* mov al,bh */
    bx = (uint16_t)(bx << cl); /* shl bx,cl */
    uint16_t ax = (uint16_t)((uint16_t)al << cl); /* shl ax,cl (ah was 0) */
    uint8_t ah = (uint8_t)(ax >> 8);
    seg_w16(ds, BD_ABLOCK, bx);
    al = seg_r8(ds, BD_VRECORD);
    al = (uint8_t)(al & seg_r8(ds, BD_BLKMSK));
    uint8_t bl = (uint8_t)((bx & 0xFF) | al); /* or bl,al */
    bx = (uint16_t)((bx & 0xFF00) | bl);
    seg_w16(ds, BD_ARECORD, bx);
    seg_w8(ds, BD_ARECORD_HI, ah);
}

/*
 * GET_ATTS -- get interface attributes (F5'-F8') from the fcb,
 * returned left-shifted in al; zeroes the interface attribute bits
 * in the fcb
 */
uint8_t get_atts(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = (uint16_t)(seg_r16(ds, BD_INFO) + 5);
    uint8_t dh = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t al = ds[bx];
        uint8_t ch = al;
        uint8_t carry = (uint8_t)(al >> 7);
        al = (uint8_t)(al << 1);
        dh = (uint8_t)((dh << 1) | carry);
        ch = (uint8_t)(ch & 0x7F);
        ds[bx] = ch;
        bx = (uint16_t)(bx + 1);
    }
    return (uint8_t)(dh << 4);
}

/* getexta -- get the current extent field address to al (bx out).
 * Not static: src-c/kernel/file2.c's own CLOSE_FCB/CLOSE1 cluster
 * calls it too (an ordinary cross-file call, same convention as
 * que1.c's own cross-file calls into que2.c). */
uint16_t getexta(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return (uint16_t)(seg_r16(ds, BD_INFO) + fcb_extnum);
}

/* GET_S1 -- get the current S1 field to al */
uint8_t get_s1(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = (uint16_t)(getexta(phys, ds_bdos) + 1);
    return ds[bx];
}

/* GET_RRA -- get the current ran-rec field address to bx */
uint16_t get_rra(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return (uint16_t)(seg_r16(ds, BD_INFO) + fcb_ranrec);
}

/* GET_RCNTA -- get the reccnt field address to bx */
uint16_t get_rcnta(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return (uint16_t)(seg_r16(ds, BD_INFO) + fcb_reccnt);
}

/* gtfcba -- compute reccnt and nxtrec addresses for get/setfcb */
static void gtfcba(uint8_t *phys, uint16_t ds_bdos, uint16_t *reccnt_addr, uint16_t *nxtrec_addr) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t info = seg_r16(ds, BD_INFO);
    *reccnt_addr = (uint16_t)(info + fcb_reccnt);
    *nxtrec_addr = (uint16_t)(info + fcb_nxtrec);
}

/* getfcb -- set BDOS scratch variables from the currently addressed fcb */
void getfcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t reccnt_addr, nxtrec_addr;
    gtfcba(phys, ds_bdos, &reccnt_addr, &nxtrec_addr);
    seg_w8(ds, BD_VRECORD, ds[nxtrec_addr]);
    seg_w8(ds, BD_RCOUNT, ds[reccnt_addr]);
    uint16_t bx = getexta(phys, ds_bdos);
    seg_w8(ds, BD_EXTVAL, (uint8_t)(seg_r8(ds, BD_EXTMSK) & ds[bx]));
}

/* setfcb -- place BDOS scratch values back into the current fcb */
void setfcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t reccnt_addr, nxtrec_addr;
    gtfcba(phys, ds_bdos, &reccnt_addr, &nxtrec_addr);
    uint8_t al = seg_r8(ds, BD_SEQIO);
    if (al == 2) {
        al = 0;
    }
    al = (uint8_t)(al + seg_r8(ds, BD_VRECORD));
    ds[nxtrec_addr] = al;
    ds[reccnt_addr] = seg_r8(ds, BD_RCOUNT);
}

/* cmpec0 -- checksum cl bytes starting at bx */
static uint8_t cmpec0(uint8_t *phys, uint16_t ds_bdos, uint16_t bx, uint16_t cx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = 0;
    for (uint16_t i = 0; i < cx; i++) {
        al = (uint8_t)(al + ds[(uint16_t)(bx + i)]);
    }
    return al;
}

/* cmpecs -- compute checksum for the current directory buffer */
uint8_t cmpecs(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return cmpec0(phys, ds_bdos, (seg_r16(ds, BD_BUFFA)), 128 /* recsiz */);
}

/*
 * CHKSUM_FCB -- compute checksum for fcb: add the first 12 bytes of
 * the fcb, plus BD_PD_CNT/BD_HIGH_EXT/BD_XFCB_READ_ONLY/BD_CURDSK
 * (4 contiguous bytes, see header comment), plus S1, plus the 16-byte
 * disk map, plus a bias byte
 *	output:	returns the checksum (0 = valid)
 */
uint8_t chksum_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = cmpec0(phys, ds_bdos, (BD_PD_CNT), 4);
    al = (uint8_t)(al + 0xBB);
    al = (uint8_t)(al + cmpec0(phys, ds_bdos, (seg_r16(ds, BD_INFO)), 12));
    /* "inc bx" after that 12-byte CMPEC0 call lands on INFO+12 (where
     * the loop's own bx ended up, not back at INFO), i.e. INFO+fcb_
     * extnum+1 -- the same address GET_S1 computes */
    uint16_t bx = (uint16_t)(getexta(phys, ds_bdos) + 1);
    al = (uint8_t)(al + ds[bx]); /* S1 */
    bx = (uint16_t)(bx + 3); /* INFO+16 == INFO+fcb_dskmap */
    al = (uint8_t)(al + cmpec0(phys, ds_bdos, bx, 16)); /* disk map */
    return al;
}

/* SET_CHKSUM_FCB -- compute the fcb checksum; if invalid, fix S1 to match */
void set_chksum_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = chksum_fcb(phys, ds_bdos);
    if (al == 0) {
        return; /* RET45 */
    }
    uint8_t ah = al;
    al = get_s1(phys, ds_bdos);
    ah = (uint8_t)(ah - al);
    ah = (uint8_t)(-(int8_t)ah);
    uint16_t bx = (uint16_t)(getexta(phys, ds_bdos) + 1);
    ds[bx] = ah;
}

/* RESET_CHECKSUM_FCB -- invalidate S1 if the fcb checksum is valid */
void reset_checksum_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, BD_COMP_FCB_CKS, 0);
    if (chksum_fcb(phys, ds_bdos) != 0) {
        return; /* RET45 */
    }
    uint16_t bx = (uint16_t)(getexta(phys, ds_bdos) + 1);
    ds[bx] += 1;
}

/*
 * CHEK_FCB -- as CHKSUM_FCB, but first zeroes fcb(0) if high_ext==0x60
 */
uint8_t chek_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (seg_r8(ds, BD_HIGH_EXT) == 0x60) {
        seg_w8(ds, seg_r16(ds, BD_INFO), 0);
    }
    return chksum_fcb(phys, ds_bdos);
}

extern void close1(uint8_t *phys, uint16_t ds_bdos);
extern void pack_sdcnt(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t search_olist(uint8_t *phys, uint16_t ds_bdos, uint8_t ch);

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
uint8_t check_fcb1(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = chek_fcb(phys, ds_bdos);
    if (al == 0) {
        return 1; /* ret45 -- valid */
    }
    if ((al & 0x0F) != 0) {
        goto check_fcb3;
    }
    if (seg_r8(ds, BD_PD_CNT) == 0) {
        goto check_fcb3;
    }
    seg_w8(ds, (uint16_t)(BD_SDCNT + 1), 0xFF);
    seg_w8(ds, BD_DONT_CLOSE, 0xFF);
    close1(phys, ds_bdos);
    seg_w8(ds, BD_LRET, (uint8_t)(seg_r8(ds, BD_LRET) + 1));
    if (seg_r8(ds, BD_LRET) == 0) {
        goto check_fcb3; /* partial close failed */
    }
    seg_w8(ds, BD_LRET, 0);
    pack_sdcnt(phys, ds_bdos);
    if (search_olist(phys, ds_bdos, 5)) {
        return 1; /* found -- checksum ok */
    }
check_fcb3:
    if (!seg_r8(ds, BD_CHECK_FCB_RET)) {
        seg_w8(ds, BD_LRET, 10);
    }
    return 0;
}

/*
 * check_fcb -- CHECK_FCB: see check_fcb1's own header comment --
 * `func20`/`func21` (`file4.c`) are its own two real callers.
 */
uint8_t check_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, BD_CHECK_FCB_RET, 0);
    return check_fcb1(phys, ds_bdos);
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
uint16_t setcdisk(uint8_t *phys, uint16_t ds_bdos, uint16_t cx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return set_cdisk1(seg_r8(ds, BD_SELDSK), cx);
}

/* nowrite -- return non-zero if the current disk is in the r/o vector */
uint8_t nowrite(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dx = seg_r16(ds, BD_RODSK);
    uint8_t cl = seg_r8(ds, BD_SELDSK);
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
uint16_t test_vector(uint8_t *phys, uint16_t ds_bdos, uint16_t dx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return test_vector1(dx, seg_r8(ds, BD_SELDSK));
}

/* remove_drive -- REMOVE_DRIVE: clear BD_CURDSK's own bit in bx */
uint16_t remove_drive(uint8_t *phys, uint16_t ds_bdos, uint16_t bx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t cl = seg_r8(ds, BD_CURDSK);
    uint16_t dx = (uint16_t)(1u << cl);
    return (uint16_t)(bx & (uint16_t)~dx);
}

/* setro -- set the current disk to read-only */
void setro(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = setcdisk(phys, ds_bdos, seg_r16(ds, BD_RODSK));
    seg_w16(ds, BD_RODSK, bx);
    uint16_t dx = (uint16_t)(seg_r16(ds, BD_DIRMAX) + 1);
    bx = seg_r16(ds, BD_CDRMAXA);
    seg_w16(ds, bx, dx);
}

extern void flush_files(uint8_t *phys, uint16_t ds_bdos);
extern void flush_file0(uint8_t *phys, uint16_t ds_bdos);
uint8_t compcdr(uint8_t *phys, uint16_t ds_bdos);
void checkwrite(uint8_t *phys, uint16_t ds_bdos);
void setenddir(uint8_t *phys, uint16_t ds_bdos);

/*
 * checksum/newchecksum -- compute (or, if cl==true, initialize) the
 * current directory record's own checksum; on a mismatch, flush the
 * files open on this disk and mark it read-only (unless it's already
 * read-only, in which case just flush) -- now portable since
 * FLUSH_FILES/FLUSH_FILE0 (src-c/kernel/file3.c) are.
 *	input:	cl = true (0xFF) to initialize, or (MPM-specific) 0xFE
 *		for the "directory checksum" variant (TEST_DIR_CS)
 */
void checksum(uint8_t *phys, uint16_t ds_bdos, uint8_t cl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dx = seg_r16(ds, BD_ARECORD);
    uint16_t bx = (uint16_t)(seg_r16(ds, BD_CHKSIZ) & 0x7FFF); /* and bh,7fh */
    if (dx >= bx) {
        return; /* ret6 -- past the checksum vector's own size */
    }
    uint8_t al = cmpecs(phys, ds_bdos);
    bx = (uint16_t)(seg_r16(ds, BD_ARECORD) + seg_r16(ds, BD_CHECKA));
    cl = (uint8_t)(cl + 1);
    if (cl == 0) {
        ds[bx] = al; /* initcs */
        return;
    }
    cl = (uint8_t)(cl + 1);
    if (cl == 0) {
        /* TEST_DIR_CS */
        if (al != ds[bx]) {
            flush_files(phys, ds_bdos);
        }
        return;
    }
    if (al == ds[bx]) {
        return; /* ret7 */
    }
    if (!compcdr(phys, ds_bdos)) {
        return; /* ret7 */
    }
    if (nowrite(phys, ds_bdos)) {
        return; /* ret7 */
    }
    flush_file0(phys, ds_bdos);
    setro(phys, ds_bdos);
}

void newchecksum(uint8_t *phys, uint16_t ds_bdos) {
    checksum(phys, ds_bdos, 0xFF); /* true */
}

/* rddirbuf, declared ahead for rddir0's own use below */
void rddirbuf(uint8_t *phys, uint16_t ds_bdos);
void seekdir(uint8_t *phys, uint16_t ds_bdos);

/*
 * rddir0 -- seek and read the next directory record if the current
 * entry starts one (dcnt's own low byte lands on a record boundary),
 * then checksum it
 */
void rddir0(uint8_t *phys, uint16_t ds_bdos, uint8_t cl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = (uint8_t)((seg_r8(ds, BD_DCNT) & dskmsk) << fcbshf);
    seg_w8(ds, BD_DPTR, al);
    if (al != 0) {
        return; /* ret71 -- not a new record */
    }
    seekdir(phys, ds_bdos);
    rddirbuf(phys, ds_bdos);
    checksum(phys, ds_bdos, cl);
}

/*
 * rddir -- read the next directory entry (cl = true if initializing)
 */
void rddir(uint8_t *phys, uint16_t ds_bdos, uint8_t cl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dirmax = seg_r16(ds, BD_DIRMAX);
    uint16_t dcnt = (uint16_t)(seg_r16(ds, BD_DCNT) + 1);
    seg_w16(ds, BD_DCNT, dcnt);
    if (dirmax < dcnt) {
        setenddir(phys, ds_bdos);
        return;
    }
    rddir0(phys, ds_bdos, cl);
}

/* wrdir -- write the current directory entry, setting its checksum */
void wrdir(uint8_t *phys, uint16_t ds_bdos) {
    checkwrite(phys, ds_bdos);
    newchecksum(phys, ds_bdos);
    setdir(phys, ds_bdos);     /* directory dma */
    wrbuff(phys, ds_bdos, 1);  /* indicates a write directory */
    setdata(phys, ds_bdos);    /* to data dma address */
}

/*
 * RO_TEST -- test a directory element/fcb for r/o status, carry set
 * if r/o. Not `static`: `src-c/kernel/file4.c`'s own `func15` (BDOS
 * OPEN) also calls `RO_TEST` directly, an ordinary cross-file call,
 * same as `que1.c`'s own into `que2.c`.
 */
uint8_t ro_test(uint8_t *phys, uint16_t ds_bdos, uint16_t bx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    bx = (uint16_t)(bx + fcb_type); /* rofile == fcb_type's own offset */
    uint8_t al = ds[bx];
    return (uint8_t)(al >> 7);
}

/* getdptra -- compute the address of a directory element at BD_DPTR */
uint16_t getdptra(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return (uint16_t)(seg_r16(ds, BD_BUFFA) + seg_r8(ds, BD_DPTR));
}

/* ckrodir/ckrofile -- check a directory element/fcb for r/o status */
void ckrofile(uint8_t *phys, uint16_t ds_bdos, uint16_t bx) {
    if (!ro_test(phys, ds_bdos, bx)) {
        return; /* ret5 */
    }
    roferror(phys, ds_bdos);
}

void ckrodir(uint8_t *phys, uint16_t ds_bdos) {
    ckrofile(phys, ds_bdos, getdptra(phys, ds_bdos));
}

/* checkwrite -- check for a write-protected disk */
void checkwrite(uint8_t *phys, uint16_t ds_bdos) {
    if (nowrite(phys, ds_bdos) == 0) {
        return; /* ret5 */
    }
    roderror(phys, ds_bdos);
}

/* getmodnum -- bx = .fcb(modnum); returns fcb(modnum) */
uint8_t getmodnum(uint8_t *phys, uint16_t ds_bdos, uint16_t *bx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = (uint16_t)(seg_r16(ds, BD_INFO) + fcb_modnum);
    *bx_out = bx;
    return ds[bx];
}

/* clrmodnum -- clear the module number field for user open/make */
void clrmodnum(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx;
    (void)getmodnum(phys, ds_bdos, &bx);
    ds[bx] = 0;
}

/* CLR_EXT -- clear the high bits of the extent field */
void clr_ext(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = getexta(phys, ds_bdos);
    ds[bx] = (uint8_t)(ds[bx] & 0x1F);
}

/* setfwf -- set the file-write-flag bit in fcb(modnum) */
uint8_t setfwf(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx;
    uint8_t al = getmodnum(phys, ds_bdos, &bx);
    al = (uint8_t)(al | fwfmsk);
    ds[bx] = al;
    return al;
}

/* compcdr -- return non-zero (carry) if cdrmax > dcnt */
uint8_t compcdr(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dx = seg_r16(ds, BD_DCNT);
    uint16_t bx = seg_r16(ds, BD_CDRMAXA);
    return (dx < seg_r16(ds, bx)) ? 1 : 0;
}

/* setcdr -- if not (cdrmax > dcnt), set cdrmax = dcnt+1 */
void setcdr(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (compcdr(phys, ds_bdos)) {
        return;
    }
    uint16_t bx = seg_r16(ds, BD_CDRMAXA);
    uint16_t dx = (uint16_t)(seg_r16(ds, BD_DCNT) + 1);
    seg_w16(ds, bx, dx);
}

/* subdh -- compute dx - bx. Not a standalone function here: its only
 * real use (checksum's own "drec < chksiz?" size check, reached
 * transitively from rddir0/rddir too) folds directly into checksum's
 * own `if (dx >= bx) return;` above, see this file's own header
 * comment */

/* rddirbuf -- read a directory entry into the directory buffer */
void rddirbuf(uint8_t *phys, uint16_t ds_bdos) {
    setdir(phys, ds_bdos);  /* directory dma */
    rdbuff(phys, ds_bdos);  /* directory record loaded */
    setdata(phys, ds_bdos); /* falls through into setdata: back to the data dma */
}

/* setdma -- bx = .dma address to set (i.e., buffa or dmaad) */
static void setdma(uint8_t *phys, uint16_t ds_bdos, uint16_t bx) {
    (void)xios(phys, ds_bdos, io_setdma, seg_r16(far_ptr(phys, ds_bdos, 0), bx), 0);
}

/* setdata -- set the data dma address: dmabase:dmaad */
void setdata(uint8_t *phys, uint16_t ds_bdos) {
    (void)xios(phys, ds_bdos, io_setdmab, seg_r16(far_ptr(phys, ds_bdos, 0), BD_DMABASE), 0);
    setdma(phys, ds_bdos, BD_DMAAD);
}

/* setdir -- set the directory dma address: DS:buffa */
void setdir(uint8_t *phys, uint16_t ds_bdos) {
    (void)xios(phys, ds_bdos, io_setdmab, ds_bdos, 0);
    setdma(phys, ds_bdos, BD_BUFFA);
}

/* dirtouser -- copy the directory entry to the user buffer */
void dirtouser(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t dx = seg_r16(ds, BD_BUFFA);
    uint16_t bx = seg_r16(ds, BD_DMAAD);
    /* move with es = parametersegment: DS:buffa -> user:dmaad */
    uint8_t *user = far_ptr(phys, seg_r16(ds, BD_PARAMETERSEGMENT), 0);
    for (uint8_t i = 0; i < 128 /* recsiz */; i++) {
        seg_w8(user, (uint16_t)(bx + i), seg_r8(ds, (uint16_t)(dx + i)));
    }
}

/* MAKE_FCB_INV -- flag an fcb as invalid */
void make_fcb_inv(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx;
    (void)setfwf(phys, ds_bdos); /* also recomputes bx below via getmodnum's own math */
    (void)getmodnum(phys, ds_bdos, &bx);
    bx = (uint16_t)(bx + 2);
    seg_w16(ds, bx, 0xFFFF);
}

/* CHK_INV_FCB -- returns non-zero if the fcb's dskmap word isn't 0xFFFF */
uint16_t chk_inv_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = get_dma(phys, ds_bdos);
    return (uint16_t)(seg_r16(ds, bx) ^ 0xFFFF);
}

/*
 * TST_INV_FCB -- test for an invalid fcb
 *	output:	returns 0 (ret8) if the fcb is valid; sets lret=9 and
 *		returns 9 if invalid
 */
uint8_t tst_inv_fcb(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (chk_inv_fcb(phys, ds_bdos) != 0) {
        return 0; /* ret8 */
    }
    seg_w8(ds, BD_LRET, 9); /* mov al,9!jmp staret */
    return 9;
}

/* endofdir/TEST_FFFF -- zero flag set if at end of directory */
uint8_t endofdir(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    return (seg_r16(ds, BD_DCNT) == 0xFFFF) ? 1 : 0;
}

/* setenddir -- set dcnt to the end of the directory */
void setenddir(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_DCNT, 0xFFFF); /* enddir */
}

/*
 * getallocbit -- given an allocation-vector bit position (cx), return
 * the containing byte rotated so the bit is in al's own low position
 *	output:	returns {al, dh} (al = rotated byte, dh = shift count);
 *		*bx_out = the byte's own address
 */
uint8_t getallocbit(uint8_t *phys, uint16_t ds_bdos, uint16_t cx, uint16_t *bx_out, uint8_t *dh_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t cl = (uint8_t)((cx & 0x07) + 1);
    uint8_t ch = cl;
    (void)ch;
    uint16_t dx = (uint16_t)(cx >> 3);
    uint16_t bx = (uint16_t)(seg_r16(ds, BD_ALLOCA) + dx);
    uint8_t al = ds[bx];
    al = (uint8_t)(((al << cl) | (al >> (8 - cl))) & 0xFF); /* rol al,cl */
    *bx_out = bx;
    *dh_out = cl;
    return al;
}

/* setallocbit -- set or reset the allocation bit at position cx to dl */
void setallocbit(uint8_t *phys, uint16_t ds_bdos, uint16_t cx, uint8_t dl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx;
    uint8_t dh;
    uint8_t al = getallocbit(phys, ds_bdos, cx, &bx, &dh);
    al = (uint8_t)(al & 0xFE);
    al = (uint8_t)(al | (dl & 1));
    /* rotr: ror al,dh and store */
    al = (uint8_t)(((al >> dh) | (al << (8 - dh))) & 0xFF);
    ds[bx] = al;
}
