/*
 * lbdosf3.c -- C port of src-assem/loader/LBDOSF3.A86: sequential disk read,
 * disk (re)selection, and the dispatcher epilogue (goback/retmon) for
 * the loader's own BDOS.
 *
 * Over half of the real file is already dead, commented-out code
 * from an earlier rewrite (sqread's own old rseek/rdread/rdwrite/
 * cmperr/getfilesize family) -- never assembled, nothing to port.
 * Everything else here is self-contained and portable now that
 * src-c/loader/lbdosf1.c's own getfcb/alloct/atran/seek/rdbuff/setfcb
 * and src-c/loader/lbdosf2.c's own open_/openreel are ported.
 *
 * `parsave33`/`parunsave` genuinely live in the still-otherwise-
 * deferred LBDOSENT.A86 (its own `jmp cs:functab[bx]` dispatch +
 * `iret`, same class as DSPTCH.RTM/ABORT.RTM), but `func15`/`func20`/
 * `goback` all need them, and both are independently portable: the
 * real asm's own `mov ds,parametersegment` is moot under this port's
 * flat memory model, same reasoning as src-c/loader/lbdosf1.c's own
 * `dirtouser`. Ported here (not in a new lbdosent.c) since this is
 * their only real use so far; `func50`-`func57` (the rest of
 * LBDOSENT.A86's own 8086-function table) and the dispatch/`iret`
 * itself remain deferred.
 *
 * `slect`'s own `CALL selerror` branch depends on `selectdisk`'s own
 * success/failure indication, which boils down to a flag (ZF, via
 * `JNZ`), not a register byte. Mechanically replaying `selectdisk`'s
 * own established convention (`file1.c`'s own "bx fixed at 0" choice,
 * made for a different caller that never looked at its own ZF) would
 * leave ZF=1 here (from `selectdisk`'s own "cmp bx,0"), which would
 * make `slect` *always* fall through to the still-deferred `selerror`
 * -- but "what flags an undefined-artifact convention leaves behind
 * for a different caller to observe" is itself just as undefined as
 * the original artifact, so this picks ZF=0 instead: the same
 * reasoning as every other undefined-artifact choice in this project
 * (e.g. `diocomp`'s own AL, fixed at 0/success), choosing the value
 * that keeps the routine's own real job (disk selection) reachable.
 * The `CALL selerror` branch is correspondingly unreachable here,
 * translated as a comment rather than code, same as `goerr`'s own
 * treatment elsewhere.
 *
 * `func12`/`func16`-`func19`/`func21`-`func25`/`func27`-`func37`/
 * `func40` are each just an `EQU` alias for the bare `return:` label
 * -- nothing to port, same class as `SYSENT.RTM`'s own
 * `creat_proc_entry`. `func14` is an alias for `curselect` itself,
 * already ported under its own name. `func38`/`func39` are aliases
 * for `funcret`, already ported in src-c/loader/lbdosio.c.
 */
#include <stdint.h>

#include "defs.h"

extern void getfcb(uint8_t *mem);
extern uint16_t alloct(const uint8_t *mem);
extern void atran(uint8_t *mem);
extern void seek(uint8_t *mem);
extern void rdbuff(uint8_t *mem);
extern void setfcb(uint8_t *mem);
extern void bdos_index(uint8_t *mem);
extern void setlret1(uint8_t *mem);
extern void selectdisk(uint8_t *mem);
extern uint16_t setcdisk(const uint8_t *mem, uint16_t cx);
extern void clrmodnum(uint8_t *mem);
extern void open_(uint8_t *mem);
extern void openreel(uint8_t *mem);

/* diskeof -- lret = 1 */
static void diskeof(uint8_t *mem) {
    setlret1(mem);
}

/* diskread -- read the next record from the current fcb */
void diskread(uint8_t *mem) {
    mem[LD_RMF] = 0xFF;
    getfcb(mem);
    uint8_t al = mem[LD_VRECORD];
    if (al >= mem[LD_RCOUNT]) { /* NOT jb recordok */
        if (al != 128) {
            diskeof(mem);
            return;
        }
        openreel(mem);
        mem[LD_VRECORD] = 0;
        if (mem[LD_ARET] != 0) { /* cmp lret,0 ! jnz diskeof */
            diskeof(mem);
            return;
        }
    }
    /* recordok */
    bdos_index(mem);
    if (alloct(mem) == 0) {
        diskeof(mem);
        return;
    }
    atran(mem);
    seek(mem);
    rdbuff(mem);
    setfcb(mem);
}

/* sqread -- sequential disk read */
void sqread(uint8_t *mem) {
    mem[LD_SEQIO] = 1;
    diskread(mem);
}

extern void initialize(uint8_t *mem);

/*
 * slect -- select disk info for subsequent i/o. See header comment
 * for the CALL selerror branch's own boundary-stub handling.
 */
void slect(uint8_t *mem) {
    uint16_t rotated = mem_r16(mem, LD_DLOG);
    uint8_t shift = (uint8_t)(mem[LD_CURDSK] & 15); /* ror bx,cl: 16-bit rotate, periodic every 16 */
    if (shift != 0) {
        rotated = (uint16_t)((rotated >> shift) | (rotated << (16 - shift)));
    }
    selectdisk(mem); /* boundary stub -- success assumed, see header comment */
    uint8_t bit0 = (uint8_t)(rotated & 1); /* rcr bl,1 -- cf = old bit0 */
    if (bit0 != 0) {
        return; /* ret20 -- already logged in */
    }
    uint16_t bx = setcdisk(mem, mem_r16(mem, LD_DLOG));
    mem_w16(mem, LD_DLOG, bx);
    initialize(mem);
}

/* curselect -- select the disk named by linfo */
void curselect(uint8_t *mem) {
    mem[LD_CURDSK] = mem[LD_LINFO];
    slect(mem);
}

/* reselect -- check the current fcb to see if reselection is needed */
void reselect(uint8_t *mem) {
    mem[LD_RESEL] = 0xFF;
    uint16_t bx = mem_r16(mem, LD_INFO);
    uint8_t al = (uint8_t)((mem[bx] & 0x1F) - 1);
    mem[LD_LINFO] = al;
    if (al < 30) {
        mem[LD_OLDDSK] = mem[LD_CURDSK];
        uint8_t orig = mem[bx];
        mem[LD_FCBDSK] = orig;
        mem[bx] = (uint8_t)(orig & 0xE0);
        curselect(mem);
    }
    mem[bx] = (uint8_t)(mem[LD_USRCODE] | mem[bx]);
}

/* func13 -- reset disk system, initialize to disk 0 */
void func13(uint8_t *mem) {
    mem_w16(mem, LD_RODSK, 0);
    mem_w16(mem, LD_DLOG, 0);
    mem[LD_CURDSK] = 0;
    slect(mem);
}

static void move_bytes(uint8_t *mem, uint16_t dx, uint16_t bx, uint8_t cl) {
    for (uint8_t i = 0; i < cl; i++) {
        mem[(uint16_t)(bx + i)] = mem[(uint16_t)(dx + i)];
    }
}

/*
 * parsave/parsave33/parunsave -- real LBDOSENT.A86 routines, ported
 * here, see header comment. The real `mov ds,parametersegment`/
 * `mov es,parametersegment` are moot under this port's flat memory
 * model, same as src-c/loader/lbdosf1.c's own `dirtouser`.
 */
static void parsave(uint8_t *mem, uint8_t cl) {
    mem[LD_PARCOPFL] = 0xFF;
    mem[LD_PARLG] = cl;
    uint16_t si = mem_r16(mem, LD_INFO);
    mem_w16(mem, LD_INFOSAVE, si);
    mem_w16(mem, LD_INFO, LD_LOC_PAR_AREA);
    move_bytes(mem, si, LD_LOC_PAR_AREA, cl);
}

void parsave33(uint8_t *mem) {
    parsave(mem, 33);
}

void parunsave(uint8_t *mem) {
    uint8_t cl = mem[LD_PARLG];
    uint16_t di = mem_r16(mem, LD_INFOSAVE);
    mem_w16(mem, LD_INFO, di);
    move_bytes(mem, LD_LOC_PAR_AREA, di, cl);
}

/* func15 -- open file */
void func15(uint8_t *mem) {
    parsave33(mem);
    clrmodnum(mem);
    reselect(mem);
    open_(mem);
}

/* func20 -- read a file */
void func20(uint8_t *mem) {
    parsave33(mem);
    reselect(mem);
    sqread(mem);
}

/*
 * func26 -- set the subsequent dma address to info. `setdata` has no
 * observable effect, see lbdosf1.c's own comment.
 */
void func26(uint8_t *mem, uint16_t dx) {
    mem_w16(mem, LD_DMAAD, dx);
}

/*
 * goback -- arrive here at the end of processing, to return to the
 * user (the dispatcher epilogue). `retm1`'s own split of aret into
 * al/ch is a register-return convention for the (deferred)
 * dispatcher, not a memory effect -- nothing further to model, so
 * this just returns aret itself.
 */
uint16_t goback(uint8_t *mem) {
    if (mem[LD_RESEL] != 0) {
        uint16_t bx = mem_r16(mem, LD_INFO);
        mem[bx] = 0;
        uint8_t al = mem[LD_FCBDSK];
        if (al != 0) {
            mem[bx] = al;
            mem[LD_LINFO] = mem[LD_OLDDSK];
            curselect(mem);
        }
    }
    if (mem[LD_PARCOPFL] == 0xFF) {
        parunsave(mem);
    }
    return mem_r16(mem, LD_ARET);
}
