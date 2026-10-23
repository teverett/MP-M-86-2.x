/*
 * lbdosf2.c -- C port of src/loader/LBDOSF2.A86: the loader's own
 * search/open/close/make/rename/delete logic, built on top of
 * src-c/loader/lbdosf1.c's own rddir/checksum/getdptra/etc.
 *
 * Unlike the kernel's own FILE2.BDO (almost entirely deferred -- see
 * src-c/kernel/file2.c's own header comment -- because its `srch`
 * calls `rddir`, itself blocked), this module's own `srch` calls
 * LBDOSF1.A86's already-ported `rddir`, so the whole file turns out
 * to be self-contained and portable.
 *
 * `ch` threading: `rotr`'s own shift count (src-c/loader/lbdosf1.c)
 * comes from whatever's ambient in CH at its caller's own call site,
 * not a value either `getallocbit` or `rotr` itself computes -- a
 * real, faithfully-reproduced quirk (confirmed empirically against
 * the real assembled code), not a boundary-stub artifact. Tracing it
 * through `scndm`'s own real asm shows:
 *   - `scndm`'s own entry-time CH (not anything it computes
 *     internally) is what every `setallocbit` call it makes actually
 *     uses, via a push/pop round-trip at the top of its own loop that
 *     never touches the CX pair it uses for its own per-entry disk-
 *     map scanning;
 *   - but `scndm`'s own EXIT-time CH (as observed by ITS OWN caller,
 *     once it returns) is NOT the same as its entry-time CH -- it
 *     ends up 0 for a single-byte allocation map, or the last
 *     double-byte map word's own high byte otherwise, since nothing
 *     restores the entry-time value before returning.
 * `scndm`'s own two real callers (`initialize`'s own `pdollar`, and
 * `delet0`) both reach it from a loop that never otherwise touches CH
 * between iterations, so each one's own `ch` sequence is genuinely
 * threaded call-to-call through `scndm`'s own return value -- ported
 * here as an explicit parameter/return value instead of hiding it,
 * same reasoning as `rotr`'s own fix. `srch`/`srchn`/`srchnext` (used
 * by `delet0`'s own first call into this chain) likewise thread `ch`
 * through for the same reason -- `srchnext`'s own loop explicitly
 * resets it to 0 and increments it per matched character, and that's
 * genuinely what `delet0`'s own first `scndm` call uses. Every other
 * caller of `srch`/`srchn` (rename/indicators/open/close/make) never
 * reaches `scndm` itself, so for those this port's own `ch` parameter
 * is just inert plumbing -- callers that don't care pass 0 and ignore
 * the return value, same as any other undefined-artifact convention
 * elsewhere in this project.
 *
 * `getblock`'s own call to `rotr` is unrelated to any of the above --
 * its own `ch` comes deterministically from `getallocbit`'s own
 * internal final CX (exposed via its own `cx_out` parameter), not
 * from any caller, so it's computed locally instead of threaded.
 *
 * `staret`/`setlret1` are ordinary cross-file calls into src-c/loader/
 * lbdosio.c (declared here with matching `extern` prototypes, same
 * convention as src-c/kernel/que1.c's own cross-file calls into
 * que2.c).
 */
#include <stdint.h>

#include "defs.h"

extern void staret(uint8_t *mem, uint8_t al);
extern void setlret1(uint8_t *mem);

extern void setenddir(uint8_t *mem);
extern void home(uint8_t *mem);
extern void rddir(uint8_t *mem, uint8_t cl);
extern uint8_t endofdir(const uint8_t *mem);
extern uint8_t compcdr(const uint8_t *mem);
extern uint16_t getdptra(const uint8_t *mem);
extern void checkwrite(const uint8_t *mem);
extern void ckrodir(const uint8_t *mem);
extern void wrdir(uint8_t *mem);
extern void setcdr(uint8_t *mem);
extern void seekdir(uint8_t *mem);
extern uint8_t getmodnum(const uint8_t *mem, uint16_t *bx_out);
extern uint8_t setfwf(uint8_t *mem);
extern uint8_t getallocbit(const uint8_t *mem, uint16_t cx, uint16_t *bx_out, uint8_t *dh_out, uint16_t *cx_out);
extern void setallocbit(uint8_t *mem, uint16_t cx, uint8_t dl, uint8_t ch);
extern void rotr(uint8_t *mem, uint16_t bx, uint8_t al, uint8_t ch);
extern uint8_t nowrite(const uint8_t *mem);
extern void getfcb(uint8_t *mem);

/*
 * scndm -- scan the disk map at dptr for nonzero entries, setting the
 * corresponding allocation-vector bit to dl for each one
 *	input:	dl = 0/1 (bit value to set); ch = setallocbit's own
 *		rotr shift count for every entry this call processes
 *
 * The real asm saves/restores ch around every entry's own per-entry
 * CX overwrite (the block# it feeds getallocbit/setallocbit), so this
 * routine's own exit ch always equals its entry ch -- confirmed
 * empirically against the real assembled code -- hence no return
 * value here (unlike setallocbit/rotr, where ch genuinely is an
 * external, caller-supplied value).
 */
void scndm(uint8_t *mem, uint8_t dl, uint8_t ch) {
    uint16_t bx = (uint16_t)(getdptra(mem) + dskmap);
    uint8_t cl = (uint8_t)(fcblen - dskmap + 1);
    for (;;) {
        cl = (uint8_t)(cl - 1);
        if (cl == 0) {
            return;
        }
        uint16_t cx;
        uint16_t map_ptr;
        if (mem[LD_SINGLE] != 0) {
            cx = mem[bx];
            map_ptr = bx;
        } else {
            cl = (uint8_t)(cl - 1);
            cx = mem_r16(mem, bx);
            bx = (uint16_t)(bx + 1);
            map_ptr = bx;
        }
        if (cx != 0 && mem_r16(mem, LD_MAXALL) >= cx) {
            setallocbit(mem, cx, dl, ch);
        }
        bx = (uint16_t)(map_ptr + 1);
    }
}

/*
 * initialize -- initialize the current disk: zero the allocation
 * vector, scan the directory, and mark every allocated block
 */
void initialize(uint8_t *mem) {
    uint16_t maxall = mem_r16(mem, LD_MAXALL);
    uint16_t count = (uint16_t)((maxall >> 3) + 1);
    uint8_t ch = (uint8_t)(count >> 8); /* scndm's own first-call ch, see header comment */
    uint16_t alloca = mem_r16(mem, LD_ALLOCA);
    for (uint16_t i = 0; i < count; i++) {
        mem[(uint16_t)(alloca + i)] = 0;
    }
    mem_w16(mem, alloca, mem_r16(mem, LD_DIRBLK));
    home(mem);
    mem_w16(mem, mem_r16(mem, LD_CDRMAXA), 3);
    setenddir(mem);
    for (;;) {
        rddir(mem, 0xFF);
        if (endofdir(mem)) {
            return;
        }
        uint16_t dptr_addr = getdptra(mem);
        if (mem[dptr_addr] == empty) {
            continue;
        }
        if (mem[LD_USRCODE] == mem[dptr_addr] && mem[(uint16_t)(dptr_addr + 1)] == '$') {
            mem[LD_ARET] = 0xFF; /* lret = 0xFF */
        }
        scndm(mem, 1, ch);
        setcdr(mem);
    }
}

/* cpydirloc -- copy dirloc to lret, e.g. following delete/rename */
void cpydirloc(uint8_t *mem) {
    staret(mem, mem[LD_DIRLOC]);
}

/* compext -- compare extent# in al with that in cl; nonzero = mismatch */
static uint8_t compext(const uint8_t *mem, uint8_t al, uint8_t cl) {
    uint8_t ch = (uint8_t)(~mem[LD_EXTMSK]);
    cl = (uint8_t)(cl & ch);
    al = (uint8_t)(al & ch);
    uint8_t result = (uint8_t)(al - cl);
    return (uint8_t)(result & maxext);
}

static uint8_t endsearch(uint8_t *mem, uint8_t ch) {
    mem[LD_ARET] = (uint8_t)(mem[LD_DCNT] & dskmsk); /* lret = ldcnt & dskmsk */
    uint8_t dirloc = mem[LD_DIRLOC];
    if ((dirloc & 0x80) != 0) { /* rcl al,1 ! jae ret9: CF = old bit7 */
        mem[LD_DIRLOC] = 0;
    }
    return ch;
}

static uint8_t srchfin(uint8_t *mem, uint8_t ch) {
    setenddir(mem);
    staret(mem, 255);
    return ch;
}

uint8_t srchn(uint8_t *mem, uint8_t ch);

static uint8_t srchnext(uint8_t *mem, uint16_t dx, uint8_t cl) {
    uint16_t bx = getdptra(mem);
    uint8_t ch = 0;
    while (cl != 0) {
        uint8_t al = mem[dx]; /* mov si,dx ! lods al */
        if (al != '?' && ch != ubytes) {
            if (ch == extnum) {
                uint8_t dir_char = mem[bx];
                if (compext(mem, al, dir_char) != 0) {
                    return srchn(mem, ch);
                }
            } else {
                uint8_t diff = (uint8_t)((uint8_t)(al - mem[bx]) & 0x7F);
                if (diff != 0) {
                    return srchn(mem, ch);
                }
            }
        }
        dx = (uint16_t)(dx + 1);
        bx = (uint16_t)(bx + 1);
        ch = (uint8_t)(ch + 1);
        cl = (uint8_t)(cl - 1);
    }
    return endsearch(mem, ch);
}

/*
 * srchn -- search for the next directory element matching srcha's own
 * first srchl characters
 *	input:	ch = passed straight through on every early-exit path
 *		here (none of rddir/endofdir/compcdr/getdptra touch it)
 *	output:	returns this call's own exit ch -- see header comment
 */
uint8_t srchn(uint8_t *mem, uint8_t ch) {
    rddir(mem, 0);
    if (endofdir(mem)) {
        return srchfin(mem, ch);
    }
    uint16_t dx = mem_r16(mem, LD_SRCHA);
    uint8_t al = mem[dx]; /* mov si,dx ! lods al */
    if (al != empty) {
        if (compcdr(mem)) { /* jnb srchfin: NOT taken when CF=0 */
            return srchfin(mem, ch);
        }
    }
    return srchnext(mem, dx, mem[LD_SRCHL]);
}

/* srch -- search for a directory element matching the current fcb's
 * first cl characters */
uint8_t srch(uint8_t *mem, uint8_t cl, uint8_t ch) {
    mem[LD_DIRLOC] = 0xFF;
    mem[LD_SRCHL] = cl;
    mem_w16(mem, LD_SRCHA, mem_r16(mem, LD_INFO));
    setenddir(mem);
    home(mem);
    return srchn(mem, ch);
}

/* delet -- delete the currently addressed file */
static void delet0(uint8_t *mem, uint8_t ch) {
    while (!endofdir(mem)) {
        ckrodir(mem);
        uint16_t bx = getdptra(mem);
        mem[bx] = empty;
        scndm(mem, 0, ch);
        wrdir(mem);
        ch = srchn(mem, ch);
    }
}

void delet(uint8_t *mem) {
    checkwrite(mem);
    uint8_t ch = srch(mem, extnum, 0);
    delet0(mem, ch);
}

/*
 * getblock -- given allocation-vector position cx, find the nearest
 * free (0) bit by searching left then right; if found, claim it (set
 * to 1) and return its position; else return 0
 */
uint16_t getblock(uint8_t *mem, uint16_t cx) {
    uint16_t dx = cx;
    for (;;) {
    lefttst:
        if (cx != 0) {
            cx = (uint16_t)(cx - 1);
            uint16_t bx;
            uint8_t dh;
            uint16_t cx_out;
            uint8_t al = getallocbit(mem, cx, &bx, &dh, &cx_out);
            if ((al & 1) == 0) {
                al = (uint8_t)(al + 1); /* rcl al,1 ! inc al */
                uint8_t ch = (uint8_t)(cx_out >> 8); /* getallocbit's own internal cx, not a caller-supplied value -- see header comment */
                rotr(mem, bx, al, ch);
                return cx;
            }
        }
        if (dx >= mem_r16(mem, LD_MAXALL)) {
            goto rblok0;
        }
        dx = (uint16_t)(dx + 1);
        {
            uint16_t bx;
            uint8_t dh;
            uint16_t cx_out;
            uint8_t al = getallocbit(mem, dx, &bx, &dh, &cx_out);
            if ((al & 1) == 0) {
                al = (uint8_t)(al + 1);
                uint8_t ch = (uint8_t)(cx_out >> 8);
                rotr(mem, bx, al, ch);
                return dx;
            }
        }
        continue;
    rblok0:
        if (cx != 0) {
            goto lefttst;
        }
        return 0;
    }
}

/* copyfcb -- copy the entire fcb into the currently addressed directory entry */
static void move_bytes(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl) {
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(bx + i)] = mem[(uint16_t)(dx + i)];
    }
}

/* seekcopy -- enter from close to seek and copy the current element */
static void seekcopy(uint8_t *mem) {
    seekdir(mem);
    wrdir(mem);
}

/*
 * copydir -- copy fcb information starting at cl into the currently
 * addressed directory entry, length dl. Has no RET of its own in the
 * real asm -- falls straight through into seekcopy (same class as
 * src-c/loader/lbdosf1.c's own seekdir/seek), so every real caller
 * (copyfcb, rename, indicators) gets a seekdir+wrdir too, not just
 * the byte copy.
 */
static void copydir(uint8_t *mem, uint8_t cl, uint8_t dl) {
    uint16_t dx = (uint16_t)(mem_r16(mem, LD_INFO) + cl);
    uint16_t bx = getdptra(mem);
    move_bytes(mem, dx, bx, dl);
    seekcopy(mem);
}

void copyfcb(uint8_t *mem) {
    copydir(mem, 0, fcblen);
}

/* rename -- rename the file described by the fcb's own first half to
 * the name in its own second half */
void rename_(uint8_t *mem) {
    checkwrite(mem);
    (void)srch(mem, extnum, 0);
    uint16_t bx = mem_r16(mem, LD_INFO);
    uint8_t al = mem[bx];
    mem[(uint16_t)(bx + dskmap)] = al;
    while (endofdir(mem) == 0) {
        ckrodir(mem);
        copydir(mem, dskmap, extnum);
        (void)srchn(mem, 0);
    }
}

/* indicators -- set file indicators for the current fcb */
void indicators(uint8_t *mem) {
    (void)srch(mem, extnum, 0);
    while (endofdir(mem) == 0) {
        copydir(mem, 0, extnum);
        (void)srchn(mem, 0);
    }
}

/* open -- search for the directory entry and copy it into the fcb */
static void opencopy(uint8_t *mem) {
    uint16_t bx = (uint16_t)(mem_r16(mem, LD_INFO) + extnum);
    uint8_t user_ext = mem[bx];
    uint16_t dptr_addr = getdptra(mem);
    move_bytes(mem, dptr_addr, mem_r16(mem, LD_INFO), nxtrec);
    setfwf(mem);
    uint8_t dir_ext = mem[(uint16_t)(dptr_addr + extnum)];
    uint8_t dir_reccnt = mem[(uint16_t)(dptr_addr + reccnt)];
    mem[bx] = user_ext; /* restore extent number (move_bytes overwrote it) */

    uint8_t al;
    if (user_ext == dir_ext) {
        al = dir_reccnt;
    } else if (user_ext < dir_ext) {
        al = 0;
    } else {
        al = 128;
    }
    mem[(uint16_t)(mem_r16(mem, LD_INFO) + reccnt)] = al;
}

void open_(uint8_t *mem) {
    (void)srch(mem, namlen, 0);
    if (endofdir(mem)) {
        return; /* ret10, lret=255 */
    }
    opencopy(mem);
}

/* mergezero -- if fcb1(i) == 0 then fcb1(i) := fcb2(i) */
static void mergezero(uint8_t *mem, uint16_t bx, uint16_t dx) {
    if (mem_r16(mem, bx) != 0) {
        return;
    }
    mem_w16(mem, bx, mem_r16(mem, dx));
}

/*
 * close -- locate the directory element and re-write it, merging the
 * disk map
 */
void close_(uint8_t *mem) {
    mem[LD_ARET] = 0;
    mem_w16(mem, LD_DCNT, 0);
    if (nowrite(mem) != 0) {
        return;
    }
    uint16_t bx;
    uint8_t modnum_val = getmodnum(mem, &bx);
    if ((modnum_val & fwfmsk) != 0) {
        return;
    }
    (void)srch(mem, namlen, 0);
    if (endofdir(mem)) {
        return;
    }
    uint16_t dx_ptr = (uint16_t)(getdptra(mem) + dskmap); /* .buff(dptr+dskmap) */
    uint16_t bx_ptr = (uint16_t)(mem_r16(mem, LD_INFO) + dskmap); /* .fcb(dskmap) */
    uint8_t cl = (uint8_t)(fcblen - dskmap);
    while (cl != 0) {
        if (mem[LD_SINGLE] != 0) {
            /* single-byte map: if fcb(i)==0, fcb(i):=buff(i); if
             * buff(i)==0 (checked on the ORIGINAL buff(i) value),
             * buff(i):=fcb(i); then they must match */
            uint8_t fcb_was_zero = (mem[bx_ptr] == 0);
            uint8_t al = mem[dx_ptr]; /* mov si,dx ! lods al */
            if (fcb_was_zero) {
                mem[bx_ptr] = al; /* fcb(i) = buff(i) */
            }
            if (al == 0) {
                al = mem[bx_ptr]; /* re-read, possibly just updated above */
                mem[dx_ptr] = al; /* buff(i) = fcb(i) */
            }
            if (al != mem[bx_ptr]) {
                mem[LD_ARET] = (uint8_t)(mem[LD_ARET] - 1); /* dec lret */
                return;
            }
            dx_ptr = (uint16_t)(dx_ptr + 1);
            bx_ptr = (uint16_t)(bx_ptr + 1);
            cl = (uint8_t)(cl - 1);
        } else {
            /* double-byte map */
            mergezero(mem, bx_ptr, dx_ptr); /* fcb(i) := buff(i) if fcb(i) == 0 */
            mergezero(mem, dx_ptr, bx_ptr); /* buff(i) := fcb(i) if buff(i) == 0 */
            if (mem_r16(mem, dx_ptr) != mem_r16(mem, bx_ptr)) {
                mem[LD_ARET] = (uint8_t)(mem[LD_ARET] - 1);
                return;
            }
            dx_ptr = (uint16_t)(dx_ptr + 1);
            bx_ptr = (uint16_t)(bx_ptr + 1);
            cl = (uint8_t)(cl - 1);
            dx_ptr = (uint16_t)(dx_ptr + 1);
            bx_ptr = (uint16_t)(bx_ptr + 1);
            cl = (uint8_t)(cl - 1);
        }
    }
    /* end of disk map merge: dx_ptr = .buff(dptr+fcblen), bx_ptr =
     * .fcb(fcblen) -- check the record count */
    uint16_t fcb_ext = (uint16_t)(bx_ptr - (fcblen - extnum));
    uint16_t buf_ext = (uint16_t)(dx_ptr - (fcblen - extnum));
    uint8_t fcb_extval = mem[fcb_ext];
    if (fcb_extval >= mem[buf_ext]) {
        mem[buf_ext] = fcb_extval;
        uint16_t fcb_rec = (uint16_t)(fcb_ext + (reccnt - extnum));
        uint16_t buf_rec = (uint16_t)(buf_ext + (reccnt - extnum));
        mem[buf_rec] = mem[fcb_rec];
    }
    mem[LD_FCBCOPIED] = 0xFF;
    seekcopy(mem);
}

/* make -- create a new file by creating a directory entry */
void make(uint8_t *mem) {
    checkwrite(mem);
    uint16_t saved_info = mem_r16(mem, LD_INFO);
    mem_w16(mem, LD_INFO, LD_EFCB);
    (void)srch(mem, 1, 0);
    uint8_t at_end = endofdir(mem);
    mem_w16(mem, LD_INFO, saved_info);
    if (at_end) {
        return;
    }
    uint16_t dx = getdptra(mem); /* xchg bx,dx -- srch's own aret-ish return address */
    uint16_t bx = (uint16_t)(namlen + dx);
    for (uint8_t i = 0; i < (uint8_t)(fcblen - namlen); i++) {
        mem[(uint16_t)(bx + i)] = 0;
    }
    mem[(uint16_t)(ubytes + dx)] = 0;
    setcdr(mem);
    copyfcb(mem);
    setfwf(mem);
}

/* openreel -- close the current extent, open the next one if possible */
static void openmod(uint8_t *mem);
static void openr0(uint8_t *mem);
static void openr1(uint8_t *mem);
static void openr2(uint8_t *mem);
static void openerr(uint8_t *mem);

void openreel(uint8_t *mem) {
    mem[LD_FCBCOPIED] = 0;
    close_(mem);
    if (endofdir(mem)) {
        return;
    }
    uint16_t bx = (uint16_t)(mem_r16(mem, LD_INFO) + extnum);
    uint8_t al = (uint8_t)((mem[bx] + 1) & maxext);
    mem[bx] = al;
    if (al == 0) {
        openmod(mem);
        return;
    }
    uint8_t ch = (uint8_t)(mem[LD_EXTMSK] & al);
    ch = (uint8_t)(ch & mem[LD_FCBCOPIED]);
    if (ch == 0) {
        openr0(mem);
        return;
    }
    openr1(mem);
}

static void openmod(uint8_t *mem) {
    uint16_t bx = (uint16_t)(mem_r16(mem, LD_INFO) + modnum);
    mem[bx] = (uint8_t)(mem[bx] + 1);
    if ((mem[bx] & maxmod) == 0) {
        openerr(mem);
        return;
    }
    openr0(mem);
}

static void openr0(uint8_t *mem) {
    (void)srch(mem, namlen, 0);
    if (!endofdir(mem)) {
        openr1(mem);
        return;
    }
    if (mem[LD_RMF] == 0xFF) { /* inc al; jz openerr means rmf==0xFF (read mode) -> error */
        openerr(mem);
        return;
    }
    make(mem);
    if (endofdir(mem)) {
        openerr(mem);
        return;
    }
    openr2(mem);
}

static void openr1(uint8_t *mem) {
    opencopy(mem);
    openr2(mem);
}

static void openr2(uint8_t *mem) {
    getfcb(mem);
    staret(mem, 0);
}

static void openerr(uint8_t *mem) {
    setlret1(mem);
    setfwf(mem);
}
