/*
 * lbdosf1.c -- C port of src/loader/LBDOSF1.A86: a near-full standalone
 * copy of the kernel's own FILE1.BDO disk-I/O/FCB-arithmetic/checksum/
 * rddir/getallocbit logic, used by the boot-time loader before the
 * real BDOS (src/kernel/FILE1-4.BDO) is running.
 *
 * Unlike the kernel's FILE1.BDO (see src-c/kernel/file1.c's own header
 * comment), this module's checksum/rddir/wrdir chain is entirely
 * self-contained in this one file -- it isn't split across FILE2.BDO/
 * FILE3.BDO/FILE4.BDO's own real logic, so none of that cross-module
 * blocking applies here. Deferred instead:
 *   - selerror/goerr -- a pure boundary-stub tail (`JMP w[bx]`) into
 *     LBDOSENT.A86's own error-reporting table (persub/selsub/rodsub/
 *     rofsub), itself deferred along with the rest of that module
 *     (`jmp cs:functab[bx]` dispatch + `iret`, same class as
 *     DSPTCH.RTM/ABORT.RTM). Unlike the kernel's own goerr, this one
 *     has no real side effect of its own before that tail (no aret/
 *     pf_ctlc-equivalent here) -- nothing to port, same class as
 *     SYSENT.RTM's own `creat_proc_entry`.
 *   - setdata/setdir/setdma -- all 3 are pure boundary-stub setup
 *     (XIOS setdmbf/setdmf calls), no observable effect under the
 *     convention, same as src-c/kernel/file1.c's own treatment; not
 *     even defined as functions here.
 *
 * setallocbit/rotr -- previously deferred here, now ported (see
 * src-c/loader/lbdosf2.c's own header comment for the full story):
 * `rotr`'s own shift count comes from whatever's ambient in CH at its
 * own call site, not from getallocbit's returned shift count -- a
 * real, faithfully-reproduced quirk, not a boundary-stub artifact, so
 * both now take that byte as an explicit parameter from their own
 * caller rather than hiding it.
 *
 * `seldskf`/`homef`/`readf`/`writef`/`settrkf`/`sectran`/`setsecf`/
 * `setdmbf`/`setdmf` are near CALLs into the XIOS BIOS jump table, a
 * boundary stub per docs/src-c.md's convention -- same shape as
 * `LBDOSIO.A86`'s own `constf`/`coninf`/`wbootf` (src-c/loader/
 * lbdosio.c). `readf`/`writef`'s own result (the real AL byte
 * `diocomp` branches on) is an undefined artifact under the
 * convention, fixed at 0 (success) -- same choice made throughout
 * src-c/kernel/file1.c -- which makes `diocomp`'s own error branch
 * (into the deferred `goerr`) unreachable, translated faithfully but
 * not exercised. `selectdisk`'s own `seldskf` result is likewise fixed
 * at 0 (its own *error* indication), making everything past the early
 * return unreachable, same reasoning. `seek`'s/`home`'s own BIOS calls
 * (`settrkf`/`sectran`/`setsecf`/`homef`) are likewise no-ops, but each
 * one has ONE real side effect of its own that doesn't depend on any
 * stub result (an unconditional write through the `curtrka` pointer) --
 * kept.
 *
 * `getallocbit` is NOT algebraically simplified to the equivalent
 * kernel routine's own "cx>>3" formula, despite the matching header
 * comment -- read literally, the real instruction sequence computes
 * something else (confirmed empirically against the real assembled
 * routine, see docs/src-c.md); translated here as a literal,
 * register-level transliteration of the real asm instead.
 */
#include <stdint.h>

#include "defs.h"

/* move -- move cl bytes from dx to bx */
static void move(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl) {
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(bx + i)] = mem[(uint16_t)(dx + i)];
    }
}

/*
 * selectdisk -- select the disk drive given by LD_CURDSK, and fill
 * curtrka..curreca/tranv/the addlist/dpblist header copies. seldskf's
 * own boundary-stub result (bx) is fixed at 0 (its own *error*
 * indication), so this always "fails" early; everything past the
 * early return is translated faithfully but unreachable, same choice
 * made throughout this file -- see header comment.
 */
void selectdisk(uint8_t *mem) {
    uint16_t bx = 0; /* mov cl,curdsk ! call seldskf -- boundary stub */
    if (bx == 0) {
        return; /* ret4 */
    }
    /* unreachable under the convention -- translated faithfully */
    uint16_t hdr = bx;
    uint16_t tran = mem_r16(mem, hdr);
    mem_w16(mem, LD_CDRMAXA, (uint16_t)(hdr + 2));
    mem_w16(mem, LD_CURTRKA, (uint16_t)(hdr + 4));
    mem_w16(mem, LD_CURRECA, (uint16_t)(hdr + 6));
    mem_w16(mem, LD_TRANV, tran);
    move(mem, (uint16_t)(hdr + 8), LD_BUFFA, 8); /* addlist */
    move(mem, mem_r16(mem, LD_DPBADDR), LD_SECTPT, 15); /* dpblist */
    mem[LD_SINGLE] = (mem[(uint16_t)(LD_MAXALL + 1)] == 0) ? 1 : 0;
}

/*
 * home -- move to home position, reset the current track to 0 (via
 * the curtrka pointer -- a real side effect, independent of homef's
 * own boundary-stub no-op)
 */
void home(uint8_t *mem) {
    /* call homef -- boundary stub, no-op */
    uint16_t si = mem_r16(mem, LD_CURTRKA);
    mem_w16(mem, si, 0);
}

/* diocomp -- check readf/writef's own result for disk errors
 *	output: always "succeeds" under the convention -- see header
 *		comment; the error branch (goerr) is unreachable
 */
static void diocomp(uint8_t *mem) {
    (void)mem;
}

/* rdbuff -- read buffer and check if ok */
void rdbuff(uint8_t *mem) {
    /* call readf -- boundary stub, no-op */
    diocomp(mem);
}

/* wrbuff -- write buffer and check condition */
void wrbuff(uint8_t *mem) {
    /* call writef -- boundary stub, no-op */
    diocomp(mem);
}

/*
 * seekdir -- seek the record containing the current directory entry
 */
void seek(uint8_t *mem);

void seekdir(uint8_t *mem) {
    uint16_t bx = mem_r16(mem, LD_DCNT);
    bx = (uint16_t)(bx >> dskshf); /* dskshf is a constant (2), not a field */
    mem_w16(mem, LD_ARECORD, bx);
    mem_w16(mem, LD_DREC, bx);
    seek(mem);
}

/*
 * seek -- seek the track given by LD_ARECORD. settrkf/sectran/setsecf
 * are all boundary stubs with no observable effect, but the computed
 * track number is also written through the curtrka pointer directly
 * here, independent of those stubs -- kept.
 */
void seek(uint8_t *mem) {
    uint16_t ax = mem_r16(mem, LD_ARECORD);
    uint16_t sectpt = mem_r16(mem, LD_SECTPT);
    uint16_t track = (uint16_t)(ax / sectpt); /* div sectpt -- dx=0 going in */
    uint16_t si = mem_r16(mem, LD_CURTRKA);
    mem_w16(mem, si, track); /* mov [si],cx */
    /* cx=track+offsetv ! call settrkf -- boundary stub, no-op.
     * cx=remainder ! mov dx,tranv ! call sectran -- boundary stub,
     * result feeds only into another boundary stub (setsecf), no
     * observable effect either way */
}

/* dmposition -- compute disk map position for LD_VRECORD */
static uint8_t dmposition(const uint8_t *mem) {
    uint8_t cl = mem[LD_BLKSHF];
    uint8_t al = (uint8_t)(mem[LD_VRECORD] >> cl);
    uint8_t ch = al;
    cl = (uint8_t)(7 - mem[LD_BLKSHF]);
    al = mem[LD_EXTVAL];
    al = (uint8_t)(al << cl);
    al = (uint8_t)(al + ch);
    return al;
}

/* getdm/getdmd -- return the disk map value from position cl */
static uint16_t getdm(const uint8_t *mem, uint8_t cl) {
    uint16_t bx = (uint16_t)(mem_r16(mem, LD_INFO) + dskmap + cl);
    if (mem[LD_SINGLE] == 0) {
        uint16_t bx2 = (uint16_t)(bx + cl);
        return mem_r16(mem, bx2);
    }
    return mem[bx];
}

/* index -- compute disk block number from the current fcb. Named
 * bdos_index here, not index: clang warns the plain name collides
 * with the C library's own (deprecated) strings.h index() -- same
 * reasoning as src-c/kernel/file1.c's own bdos_index */
void bdos_index(uint8_t *mem) {
    uint8_t al = dmposition(mem);
    uint16_t bx = getdm(mem, al);
    mem_w16(mem, LD_ARECORD, bx);
}

/* alloct -- called after bdos_index to see if the block is allocated */
uint16_t alloct(const uint8_t *mem) {
    return mem_r16(mem, LD_ARECORD);
}

/* atran -- compute the actual record address, assuming bdos_index was called */
void atran(uint8_t *mem) {
    uint8_t cl = mem[LD_BLKSHF];
    uint16_t bx = mem_r16(mem, LD_ARECORD);
    bx = (uint16_t)(bx << cl);
    mem_w16(mem, LD_ABLOCK, bx);
    uint8_t al = mem[LD_VRECORD];
    al = (uint8_t)(al & mem[LD_BLKMSK]);
    uint8_t bl = (uint8_t)((bx & 0xFF) | al);
    bx = (uint16_t)((bx & 0xFF00) | bl);
    mem_w16(mem, LD_ARECORD, bx);
}

/* getexta -- get the current extent field address */
static uint16_t getexta(const uint8_t *mem) {
    return (uint16_t)(mem_r16(mem, LD_INFO) + extnum);
}

/* gtfcba -- compute reccnt and nxtrec addresses for get/setfcb */
static void gtfcba(const uint8_t *mem, uint16_t *reccnt_addr, uint16_t *nxtrec_addr) {
    uint16_t info = mem_r16(mem, LD_INFO);
    *reccnt_addr = (uint16_t)(info + reccnt);
    *nxtrec_addr = (uint16_t)(info + nxtrec);
}

/* getfcb -- set scratch variables from the currently addressed fcb */
void getfcb(uint8_t *mem) {
    uint16_t reccnt_addr, nxtrec_addr;
    gtfcba(mem, &reccnt_addr, &nxtrec_addr);
    mem[LD_VRECORD] = mem[nxtrec_addr];
    mem[LD_RCOUNT] = mem[reccnt_addr];
    uint16_t bx = getexta(mem);
    mem[LD_EXTVAL] = (uint8_t)(mem[LD_EXTMSK] & mem[bx]);
}

/* setfcb -- place scratch values back into the current fcb */
void setfcb(uint8_t *mem) {
    uint16_t reccnt_addr, nxtrec_addr;
    gtfcba(mem, &reccnt_addr, &nxtrec_addr);
    uint8_t al = mem[LD_SEQIO];
    if (al == 2) {
        al = 0; /* setfc1 */
    }
    al = (uint8_t)(al + mem[LD_VRECORD]);
    mem[nxtrec_addr] = al;
    mem[reccnt_addr] = mem[LD_RCOUNT];
}

/* cmpec0 -- checksum cx bytes starting at bx */
static uint8_t cmpec0(const uint8_t *mem, uint16_t bx, uint16_t cx) {
    uint8_t al = 0;
    for (uint16_t i = 0; i < cx; i++) {
        al = (uint8_t)(al + mem[(uint16_t)(bx + i)]);
    }
    return al;
}

/* cmpecs -- compute checksum for the current directory buffer */
uint8_t cmpecs(const uint8_t *mem) {
    return cmpec0(mem, mem_r16(mem, LD_BUFFA), recsiz);
}

/* setcdisk -- set a "1" bit in LD_CURDSK's position of cx */
uint16_t setcdisk(const uint8_t *mem, uint16_t cx) {
    uint8_t cl = mem[LD_CURDSK];
    uint16_t bx = (uint16_t)(1u << cl);
    return (uint16_t)(bx | cx);
}

/* nowrite -- return non-zero if the current disk is in the r/o vector */
uint8_t nowrite(const uint8_t *mem) {
    uint16_t ax = mem_r16(mem, LD_RODSK);
    uint8_t cl = mem[LD_CURDSK];
    ax = (uint16_t)(ax >> cl);
    return (uint8_t)(ax & 1);
}

/* setro -- set the current disk to read-only */
void setro(uint8_t *mem) {
    uint16_t bx = setcdisk(mem, mem_r16(mem, LD_RODSK));
    mem_w16(mem, LD_RODSK, bx);
    uint16_t dx = (uint16_t)(mem_r16(mem, LD_DIRMAX) + 1);
    bx = mem_r16(mem, LD_CDRMAXA);
    mem_w16(mem, bx, dx);
}

/* getdptra -- compute the address of a directory element at LD_DPTR */
uint16_t getdptra(const uint8_t *mem) {
    uint16_t bx = mem_r16(mem, LD_BUFFA);
    uint8_t al = mem[LD_DPTR];
    return (uint16_t)(bx + al);
}

/*
 * ckrodir/ckrofile -- check a directory element/fcb for r/o status.
 * The r/o branch tail-jumps into the deferred `goerr` (via `roferr`) --
 * nothing to port there, see header comment, so this just returns
 * early on the r/o branch instead of reproducing that tail.
 */
void ckrofile(const uint8_t *mem, uint16_t bx) {
    bx = (uint16_t)(bx + rofile);
    uint8_t al = mem[bx];
    if ((al & 0x80) == 0) {
        return; /* ret5 */
    }
    /* roferror/goerr -- deferred, nothing to port, see header comment */
}

void ckrodir(const uint8_t *mem) {
    ckrofile(mem, getdptra(mem));
}

/*
 * checkwrite -- check for a write-protected disk. The error branch
 * tail-jumps into the deferred `goerr` (via `roderr`) -- same
 * reasoning as ckrofile above.
 */
void checkwrite(const uint8_t *mem) {
    if (nowrite(mem) == 0) {
        return; /* ret5 */
    }
    /* roderror/goerr -- deferred, nothing to port, see header comment */
}

/* getmodnum -- bx = .fcb(modnum); returns fcb(modnum) */
uint8_t getmodnum(const uint8_t *mem, uint16_t *bx_out) {
    uint16_t bx = (uint16_t)(mem_r16(mem, LD_INFO) + modnum);
    *bx_out = bx;
    return mem[bx];
}

/* clrmodnum -- clear the module number field for user open/make */
void clrmodnum(uint8_t *mem) {
    uint16_t bx;
    (void)getmodnum(mem, &bx);
    mem[bx] = 0;
}

/* setfwf -- set the file write flag (high bit of modnum) */
uint8_t setfwf(uint8_t *mem) {
    uint16_t bx;
    uint8_t al = getmodnum(mem, &bx);
    al = (uint8_t)(al | fwfmsk);
    mem[bx] = al;
    return al;
}

/* compcdr -- return non-zero (cy) if cdrmax > dcnt */
uint8_t compcdr(const uint8_t *mem) {
    uint16_t dcnt = mem_r16(mem, LD_DCNT);
    uint16_t cdrmax = mem_r16(mem, mem_r16(mem, LD_CDRMAXA));
    return (uint8_t)(dcnt < cdrmax);
}

/* setcdr -- if not (cdrmax > dcnt) then cdrmax = dcnt+1 */
void setcdr(uint8_t *mem) {
    if (compcdr(mem)) { /* jb ret6: return if cdrmax > dcnt */
        return;
    }
    uint16_t bx = mem_r16(mem, LD_CDRMAXA);
    uint16_t dcnt = mem_r16(mem, LD_DCNT);
    mem_w16(mem, bx, (uint16_t)(dcnt + 1));
}

/* subdh -- compute bx = dx - bx (dx unchanged); returns the borrow flag */
static uint8_t subdh(uint16_t dx, uint16_t bx, uint16_t *bx_out) {
    *bx_out = (uint16_t)(dx - bx);
    return (uint8_t)(dx < bx);
}

/*
 * newchecksum/checksum -- compute the current checksum record and
 * update the directory element if cl=true (0xFF), or check for a
 * match if not
 */
void checksum(uint8_t *mem, uint8_t cl) {
    uint16_t drec = mem_r16(mem, LD_DREC);
    uint16_t chksiz = mem_r16(mem, LD_CHKSIZ);
    uint16_t unused_bx;
    if (!subdh(drec, chksiz, &unused_bx)) { /* jae ret6: past the checksum vector */
        return;
    }
    uint8_t al = cmpecs(mem);
    uint16_t bx = (uint16_t)(drec + mem_r16(mem, LD_CHECKA));
    cl = (uint8_t)(cl + 1);
    if (cl == 0) { /* initcs */
        mem[bx] = al;
        return;
    }
    if (al == mem[bx]) {
        return; /* ret7 -- checksum ok */
    }
    if (!compcdr(mem)) { /* jae ret7 -- no message */
        return;
    }
    setro(mem);
}

void newchecksum(uint8_t *mem) {
    checksum(mem, 0xFF);
}

/*
 * wrdir/rddirbuf -- write/read the current directory entry. setdir/
 * setdata have no observable effect, see header comment.
 */
void wrdir(uint8_t *mem) {
    newchecksum(mem);
    wrbuff(mem);
}

void rddirbuf(uint8_t *mem) {
    rdbuff(mem);
}

/*
 * dirtouser -- copy the directory entry to the user buffer. The real
 * asm switches ES to `parametersegment` first -- moot under this
 * port's flat memory model, same as src-c/kernel/file1.c's own
 * treatment.
 */
void dirtouser(uint8_t *mem) {
    uint16_t dx = mem_r16(mem, LD_BUFFA);
    uint16_t bx = mem_r16(mem, LD_DMAAD);
    move(mem, dx, bx, recsiz);
}

/* endofdir -- true (zero flag set) if at the end of the directory */
uint8_t endofdir(const uint8_t *mem) {
    return (uint8_t)(mem_r16(mem, LD_DCNT) == 0xFFFF);
}

/* setenddir -- set dcnt to the end of the directory */
void setenddir(uint8_t *mem) {
    mem_w16(mem, LD_DCNT, 0xFFFF);
}

/*
 * rddir/rddir0 -- read the next directory entry
 *	input:	cl = true (0xFF) if initializing
 */
void rddir0(uint8_t *mem, uint8_t cl);

void rddir(uint8_t *mem, uint8_t cl) {
    uint16_t dirmax = mem_r16(mem, LD_DIRMAX);
    uint16_t dcnt = (uint16_t)(mem_r16(mem, LD_DCNT) + 1);
    mem_w16(mem, LD_DCNT, dcnt);
    uint16_t unused_bx;
    if (subdh(dirmax, dcnt, &unused_bx)) { /* jb: dirmax < dcnt */
        setenddir(mem);
        return;
    }
    rddir0(mem, cl);
}

void rddir0(uint8_t *mem, uint8_t cl) {
    uint8_t al = (uint8_t)(mem[LD_DCNT] & dskmsk); /* ldcnt */
    al = (uint8_t)(al << fcbshf);
    mem[LD_DPTR] = al;
    if (al != 0) {
        return; /* ret71 */
    }
    seekdir(mem);
    rddirbuf(mem);
    checksum(mem, cl);
}

/*
 * getallocbit -- given an allocation-vector bit position (cx), return
 * the containing byte rotated so the target bit is in al's own low
 * position. A literal, register-level transliteration of the real
 * asm, not an algebraic simplification -- see header comment.
 *	output:	returns al (the rotated byte); *bx_out = the byte's own
 *		address; *dh_out = the shift count needed to rotate it
 *		back into position; *cx_out = this routine's own final
 *		internal CX (needed faithfully by src-c/loader/
 *		lbdosf2.c's own getblock, which reads it back after the
 *		call -- see that file's own header comment)
 */
uint8_t getallocbit(const uint8_t *mem, uint16_t cx, uint16_t *bx_out, uint8_t *dh_out, uint16_t *cx_out) {
    uint8_t cl = (uint8_t)(cx & 0xFF);
    uint8_t ch = (uint8_t)(cx >> 8);
    uint8_t dl = (uint8_t)((cl & 0x07) + 1);

    uint8_t al = cl;
    al = (uint8_t)(((al >> 3) | (al << 5)) & 0xFF); /* ror al,3 */
    al = (uint8_t)(al & 0x1F);
    uint8_t p = al; /* "CL shr 3 to CL" */
    al = (uint8_t)((p << 5) & 0xFF);
    cl = (uint8_t)(p | al); /* "bbbccccc to CL" */

    al = ch;
    al = (uint8_t)(((al >> 1) | (al << 7)) & 0xFF); /* ror al,1 (x3) */
    al = (uint8_t)(((al >> 1) | (al << 7)) & 0xFF);
    al = (uint8_t)(((al >> 1) | (al << 7)) & 0xFF);
    al = (uint8_t)(al & 0x1F);
    ch = al; /* "CX shr 3 to CX" */

    uint16_t new_cx = (uint16_t)(((uint16_t)ch << 8) | cl);
    uint16_t bx = (uint16_t)(mem_r16(mem, LD_ALLOCA) + new_cx);
    al = mem[bx];
    al = (uint8_t)(((al << dl) | (al >> (8 - dl))) & 0xFF); /* rol al,cl(=dl) */

    *bx_out = bx;
    *dh_out = dl;
    *cx_out = new_cx;
    return al;
}

/*
 * rotr -- rotate al right by ch positions and store the result at
 * [bx]. ch comes from whatever's ambient at the caller's own call
 * site, not a value rotr computes itself -- see lbdosf2.c's own
 * header comment for why both of rotr's real callers (setallocbit
 * below, and lbdosf2.c's own getblock) need to pass it explicitly.
 */
void rotr(uint8_t *mem, uint16_t bx, uint8_t al, uint8_t ch) {
    /* the real 8086 `ror al,cl` rotates CL times in a loop, not masked
     * to 0-7 the way 286+ does -- but that's periodic every 8 steps
     * for a byte operand, so reducing ch mod 8 first gives the exact
     * same result while avoiding undefined behavior in C for ch >= 8
     * (a real, reachable value here -- see this file's own callers) */
    uint8_t shift = (uint8_t)(ch & 7);
    mem[bx] = (shift == 0) ? al : (uint8_t)(((al >> shift) | (al << (8 - shift))) & 0xFF);
}

/*
 * setallocbit -- set or reset the allocation bit at position cx to dl
 *	input:	ch = rotr's own shift count, from whatever's ambient at
 *		setallocbit's own call site (see rotr's own comment)
 */
void setallocbit(uint8_t *mem, uint16_t cx, uint8_t dl, uint8_t ch) {
    uint16_t bx;
    uint8_t dh_unused;
    uint16_t cx_unused;
    uint8_t al = getallocbit(mem, cx, &bx, &dh_unused, &cx_unused);
    al = (uint8_t)(al & 0xFE);
    al = (uint8_t)(al | (dl & 1));
    rotr(mem, bx, al, ch);
}
