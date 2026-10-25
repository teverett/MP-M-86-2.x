/*
 * chario.c -- C port of src/kernel/CHARIO.CIO: conin_entry,
 * conout_entry, rconin_entry, rconout_entry, listout_entry,
 * constat_entry, dirio_entry, conwrite_entry, conprint_entry,
 * conread_entry (+ internal rawinit, coninit, getconnum, attach,
 * echoc, conbrk, chkctl, control_p, prcsmsg, prname, conout, compout,
 * stdout, ctlout, tabout, pspace, pbacksp, pcr, crlf, crlfp, conin).
 * src/kernel/CHARIO.CIO's own getccbadr is in src-c/kernel/console.c
 * (ported alongside CONSOLE.CIO, which needed it first -- see that
 * file's own header comment); this file calls back into it, and into
 * console.c's conattach_entry/condetach_entry/mimic_entry/
 * unmimic_entry, as ordinary cross-file calls (declared here with
 * locally-named but layout-identical types, matching the convention
 * src-c/kernel/que1.c already uses for its own cross-file calls into
 * que2.c).
 *
 * Every real character-I/O call here goes through the unported
 * `xiosif` trampoline (via listf/conoutf/constf/coninf's shared
 * `xiosc`/`xiolst`/`xio1` tails) and is a boundary stub per
 * docs/src-c.md's convention. Unlike `mpmif`'s own calls elsewhere
 * (which leave a function-code constant sitting in `cx`, giving every
 * caller's own `cmp cx,0` check a well-defined "always fails" outcome),
 * `xiosif`'s call sites never pre-load a sentinel into the register
 * (`bx`) its real caller eventually reads back -- `bx` genuinely just
 * passes through this whole chain unchanged from whatever it already
 * was. That makes `constf`/`coninf`'s own "character ready"/"character
 * read" results a caller-dependent, undefined quantity under the
 * convention, the same class of issue as CPMMEM.MEM's cpmfree_entry
 * leaving its own `bx` likewise undefined on one path (see that file's
 * own header comment) -- this port makes the same choice it did there:
 * fix the undefined value at a well-defined constant (0) instead of
 * reproducing something genuinely undefined. Concretely, `constf`'s
 * and `coninf`'s own boundary-stub result is fixed at 0 here, which
 * makes `conbrk` always report "not ready" whenever no character is
 * already buffered (`c_nchar[si] == 0`), and makes `conin` always read
 * a NUL character in that same case -- both translated faithfully,
 * with the now-unreachable branches marked as such below.
 * `conoutf`/`listf`/`stdout` have no observable effect at all under
 * the convention (nothing downstream ever reads anything they'd have
 * set), so they're omitted outright -- same as QUE2.RTM's qrelease
 * (src-c/kernel/que2.c) and src-c/loader/lbdosio.c's own conoutf/listf
 * calls.
 *
 * One real consequence of the fixed-at-0 choice: `chkctl`'s own
 * `control_s` handler polls `coninf` in a loop waiting for ctl-C or
 * ctl-Q, which never arrives under the convention -- translated
 * faithfully (a real loop, gotos included) but never exercised by the
 * differential test, same class of issue as QUE1.RTM's cioattach and
 * CPMMEM.MEM's cpmfree_entry (see both files' own header comments).
 * Callers of chkctl never pass it ctls in the test, for that reason.
 */
#include <stdint.h>

#include "defs.h"

extern uint16_t getccbadr(const uint8_t *mem, uint8_t dh, uint16_t *bx_out);
extern cio_bxcx conattach_entry(uint8_t *mem);
extern cio_bxcx condetach_entry(uint8_t *mem);
extern cio_bxcx mimic_entry(uint8_t *mem, uint8_t dh_in, uint8_t dl_in);
extern cio_bxcx unmimic_entry(uint8_t *mem, uint8_t dl_in);

/* write_cstr -- copy a NUL-terminated literal into the flat memory
 * image at a fixed scratch address, standing in for a real CS-resident
 * string CHARIO.CIO would read directly -- see header comment. */
static void write_cstr(uint8_t *mem, uint16_t addr, const char *s) {
    while (*s != '\0') {
        mem[addr] = (uint8_t)*s;
        addr = (uint16_t)(addr + 1);
        s++;
    }
    mem[addr] = 0;
}

/* getconnum -- get console number
 *	output:	returns console # (rlr's own p_cns)
 */
static uint8_t getconnum(const uint8_t *mem) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    return mem[(uint16_t)(rlr + p_cns)];
}

/* coninit / rawinit -- set up for normal / raw console I/O
 *	output:	*si_out = CCB address for the calling process's console
 */
static void coninit(uint8_t *mem, uint16_t *si_out) {
    uint8_t dh = getconnum(mem);
    uint16_t rlr = mem_r16(mem, SD_RLR);
    mem_w16(mem, (uint16_t)(rlr + p_flag),
            (uint16_t)(mem_r16(mem, (uint16_t)(rlr + p_flag)) & ~(uint16_t)pf_raw));
    uint16_t bx;
    *si_out = getccbadr(mem, dh, &bx);
}

static void rawinit(uint8_t *mem, uint16_t *si_out) {
    uint8_t dh = getconnum(mem);
    uint16_t rlr = mem_r16(mem, SD_RLR);
    mem_w16(mem, (uint16_t)(rlr + p_flag),
            (uint16_t)(mem_r16(mem, (uint16_t)(rlr + p_flag)) | pf_raw));
    uint16_t bx;
    *si_out = getccbadr(mem, dh, &bx);
}

static void prcsmsg(uint8_t *mem, uint16_t dx);
static void prname(uint8_t *mem, uint16_t dx);
static void conout(uint8_t *mem, uint16_t si, uint8_t dl);
static void tabout(uint8_t *mem, uint16_t si, uint8_t dl);
static void crlf(uint8_t *mem, uint16_t si);
static void crlfp(uint8_t *mem, uint16_t si);
static uint8_t conbrk(uint8_t *mem, uint16_t si);
static uint8_t conin(uint8_t *mem, uint16_t si);
static uint8_t chkctl(uint8_t *mem, uint16_t si, uint8_t bl);
static void ctlout(uint8_t *mem, uint16_t si, uint8_t dl);
static void pspace(uint8_t *mem);
static void pbacksp(uint8_t *mem);
static void pcr(uint8_t *mem);
void conprint_entry(uint8_t *mem, uint16_t dx, uint8_t bh, uint8_t bl);

/* attach -- attach the calling process to its default console
 *	input:	si = CCB address (from coninit/rawinit)
 */
static void attach(uint8_t *mem, uint16_t si) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (mem_r16(mem, (uint16_t)(si + c_attach)) == rlr) {
        return; /* attret */
    }
    (void)conattach_entry(mem); /* real call, for effect; its own return
                                    value is discarded, same as the asm's
                                    own disregard of it here */
    rlr = mem_r16(mem, SD_RLR);
    uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
    if ((pflag & pf_ctld) == 0) {
        return; /* noattmsg */
    }
    mem_w16(mem, (uint16_t)(rlr + p_flag), (uint16_t)(pflag & ~(uint16_t)pf_ctld));
    write_cstr(mem, CHARIO_ATTACHMSG, "\r\nAttach: ");
    prcsmsg(mem, CHARIO_ATTACHMSG);
    prname(mem, (uint16_t)(rlr + p_name));
    crlf(mem, si);
}

/* echoc -- is a character graphic?
 *	output:	returns 1 if not graphic, 0 if graphic (or cr/lf/tab/ctlh)
 */
static uint8_t echoc(uint8_t dl) {
    if (dl == cr || dl == lf || dl == tab || dl == ctlh) {
        return 0; /* ret0 -- these equality compares always clear CF */
    }
    return (dl < ' ') ? 1 : 0; /* CF from the final "cmp dl,' '" */
}

/* conbrk -- check for character ready
 *	input:	si = CCB address
 *	output:	returns 1 if a character is ready, 0 if not
 */
static uint8_t conbrk(uint8_t *mem, uint16_t si) {
    if (mem[(uint16_t)(si + c_nchar)] != 0) {
        return 1; /* conb_y */
    }
    /* constf's own boundary-stub result is fixed at 0 under the
     * convention (see header comment), so "and bl,1 ! jz conb_no" is
     * always taken here -- everything below (coninf, the raw/chkctl
     * check, conb0's "mov c_nchar[si],bl") is unreachable */
    return 0; /* conb_no */
}

/* conin -- read next character
 *	input:	si = CCB address
 *	output:	returns the character; clears c_nchar[si]
 */
static uint8_t conin(uint8_t *mem, uint16_t si) {
    uint8_t dl = mem[(uint16_t)(si + c_nchar)];
    mem[(uint16_t)(si + c_nchar)] = 0;
    if (dl != 0) {
        return dl; /* ciout */
    }
    /* coninf's own boundary-stub result is fixed at 0 under the
     * convention (see header comment) -- always reads NUL here */
    return 0;
}

/* chkctl -- check character for ctl-C/ctl-D/ctl-S
 *	input:	si = CCB address, bl = character to check
 *	output:	returns 0 if a ctl char was found (and handled), 1 if not
 */
static uint8_t chkctl(uint8_t *mem, uint16_t si, uint8_t bl) {
    if (bl == ctld) {
        uint16_t rlr = mem_r16(mem, SD_RLR);
        mem_w16(mem, (uint16_t)(rlr + p_flag),
                (uint16_t)(mem_r16(mem, (uint16_t)(rlr + p_flag)) | pf_ctld));
        (void)condetach_entry(mem);
        return 0; /* chk0 */
    }
    if (bl == ctls) {
        /* control_s -- see header comment: spins forever under the
         * convention, not exercised by the differential test */
        for (;;) {
            uint8_t cs_bl = 0; /* coninf()'s fixed artifact value */
            if (cs_bl == ctlc) {
                bl = cs_bl;
                goto chkc1;
            }
            if (cs_bl == ctlq) {
                return 0; /* chk0 */
            }
            /* push dx!mov dl,bell!call conoutf!pop dx -- no-op */
        }
    }
    if (bl != ctlc) {
        return 1; /* chk1 */
    }
chkc1:
    crlf(mem, si);
    prname(mem, (uint16_t)(mem_r16(mem, SD_RLR) + p_name));
    write_cstr(mem, CHARIO_ABORTMSG, ": Abort (Y/N)? ");
    prcsmsg(mem, CHARIO_ABORTMSG);
    /* coninf's fixed artifact value, masked -- always 0 under the
     * convention, so the "echo answer, check for Y" block below is
     * unreachable */
    bl = (uint8_t)(0 & 0x5F);
    if (bl < ' ') {
        /* ctlc_r */
        if (bl == ctlc) {
            goto chkc1; /* unreachable: bl is always 0 here */
        }
        crlf(mem, si);
    } else {
        /* unreachable under the convention, see above -- translated
         * faithfully regardless */
        if (bl == 'Y') {
            uint16_t rlr = mem_r16(mem, SD_RLR);
            mem_w16(mem, (uint16_t)(rlr + p_flag),
                    (uint16_t)(mem_r16(mem, (uint16_t)(rlr + p_flag)) | pf_ctlc));
            /* mov cx,f_terminate!sub dx,dx!call mpmif -- boundary stub, no-op */
        }
        if (bl == ctlc) {
            goto chkc1; /* unreachable */
        }
        crlf(mem, si);
    }
    return 0; /* chk0 */
}

/* control_p -- toggle list-device mimicking for the current console */
static void control_p(uint8_t *mem, uint16_t si) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (mem[(uint16_t)(si + c_flag)] & cf_listcp) {
        uint8_t dl = mem[(uint16_t)(si + c_mimic)];
        cio_bxcx r = unmimic_entry(mem, dl);
        if (r.cx == 0) {
            mem[(uint16_t)(si + c_flag)] ^= cf_listcp; /* mimout */
        }
        return; /* mimpret */
    }
    uint8_t dh = mem[(uint16_t)(rlr + p_cns)];
    uint8_t dl = mem[(uint16_t)(rlr + p_lst)];
    cio_bxcx r = mimic_entry(mem, dh, dl);
    if (r.cx == 0) {
        mem[(uint16_t)(si + c_flag)] ^= cf_listcp; /* mimout */
        return;
    }
    write_cstr(mem, CHARIO_PTRMSG, "\r\nPrinter Busy\r\n");
    prcsmsg(mem, CHARIO_PTRMSG);
}

/* prcsmsg -- print a NUL-terminated string at the given flat address
 * prname -- print an 8-char name, stopping early at a space
 */
static void prcsmsg(uint8_t *mem, uint16_t dx) {
    conprint_entry(mem, dx, 0, 0);
}

static void prname(uint8_t *mem, uint16_t dx) {
    conprint_entry(mem, dx, 8, ' ');
}

/* compout -- track column position for a character about to be/having
 * been written
 */
static void compout(uint8_t *mem, uint16_t si, uint8_t dl) {
    uint8_t al = mem[(uint16_t)(si + c_column)];
    if (dl == rubout) {
        goto ccret;
    }
    al = (uint8_t)(al + 1);
    if (dl >= ' ') {
        goto ccret; /* jae */
    }
    al = (uint8_t)(al - 1);
    if (al == 0) {
        goto ccret;
    }
    if (dl == ctlh) {
        al = (uint8_t)(al - 1);
        goto ccret;
    }
    if (dl == cr) {
        al = 0;
    }
ccret:
    mem[(uint16_t)(si + c_column)] = al;
}

/* conout -- write one character to the console, tracking column
 *	input:	si = CCB address, dl = character
 */
static void conout(uint8_t *mem, uint16_t si, uint8_t dl) {
    if ((mem[(uint16_t)(si + c_flag)] & cf_compc) == 0) {
        (void)conbrk(mem, si); /* result discarded, side-effect only */
        attach(mem, si);
        /* call conoutf -- no-op under the convention, see header
         * comment. The mimic-device forwarding below it in the real
         * asm (getccbadr+stdout) is also unconditionally a no-op
         * under the convention regardless of c_mimic[si]'s value --
         * both getccbadr (pure computation) and stdout (its own
         * conoutf/listf calls) have no observable effect */
    }
    compout(mem, si, dl);
}

/* tabout -- expand tabs to the console, otherwise a direct tail-call
 * to conout
 */
static void tabout(uint8_t *mem, uint16_t si, uint8_t dl) {
    if (dl != tab) {
        conout(mem, si, dl);
        return;
    }
    do {
        conout(mem, si, ' ');
    } while ((mem[(uint16_t)(si + c_column)] & 0x07) != 0);
}

/* ctlout -- send a (possibly control) character, with a preceding
 * up-arrow if it isn't graphic
 */
static void ctlout(uint8_t *mem, uint16_t si, uint8_t dl) {
    if (!echoc(dl)) {
        tabout(mem, si, dl);
        return;
    }
    conout(mem, si, ctl);
    dl = (uint8_t)(dl | 0x40);
    tabout(mem, si, dl);
}

/* pspace/pbacksp/pcr -- jmps conoutf with a fixed character: all three
 * are complete no-ops under the convention, see header comment */
static void pspace(uint8_t *mem) {
    (void)mem;
}

static void pbacksp(uint8_t *mem) {
    (void)mem;
}

static void pcr(uint8_t *mem) {
    (void)mem;
}

/* crlf -- carriage return / line feed */
static void crlf(uint8_t *mem, uint16_t si) {
    conout(mem, si, cr);
    conout(mem, si, lf);
}

/* crlfp -- print '#', crlf, then pad with spaces out to strtcol --
 * used by the ctl-X/ctl-U/ctl-R line-editing handlers
 */
static void crlfp(uint8_t *mem, uint16_t si) {
    conout(mem, si, '#');
    crlf(mem, si);
    for (;;) {
        if (mem[(uint16_t)(si + c_column)] >= mem[(uint16_t)(si + c_strtcol)]) {
            return; /* crlfp1 */
        }
        conout(mem, si, ' ');
    }
}

/*
 * conin_entry -- Function 1: console input
 *	output:	returns the character
 */
uint8_t conin_entry(uint8_t *mem) {
    for (;;) { /* je conin_entry -- retry from the top */
        uint16_t si;
        coninit(mem, &si);
        attach(mem, si);
        (void)conbrk(mem, si); /* result discarded, side-effect only */
        uint8_t dl = conin(mem, si);
        uint8_t al = chkctl(mem, si, dl);
        if (al == 0) {
            continue;
        }
        if (!echoc(dl)) {
            tabout(mem, si, dl);
        }
        return dl; /* ciexit */
    }
}

/*
 * conout_entry -- Function 2: console output
 *	input:	dl = character
 */
void conout_entry(uint8_t *mem, uint8_t dl) {
    uint16_t si;
    coninit(mem, &si);
    attach(mem, si);
    tabout(mem, si, dl);
}

/*
 * rconin_entry -- raw console input
 *	output:	returns the character
 */
uint8_t rconin_entry(uint8_t *mem) {
    uint16_t si;
    rawinit(mem, &si);
    attach(mem, si);
    return conin(mem, si);
}

/*
 * rconout_entry -- raw console output
 *	input:	dl = character
 */
void rconout_entry(uint8_t *mem, uint8_t dl) {
    uint16_t si;
    rawinit(mem, &si);
    attach(mem, si);
    conout(mem, si, dl);
}

/*
 * listout_entry -- write to the list device
 *	input:	dl = character
 */
void listout_entry(uint8_t *mem, uint8_t dl) {
    (void)dl; /* listf's own real use of dl/dh is a no-op under the
                 convention, see header comment */
    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint8_t dh = mem[(uint16_t)(rlr + p_lst)];
    uint16_t bx;
    (void)getccbadr(mem, dh, &bx); /* pure computation, no side effects,
                                       result unused -- matches the asm's
                                       own disregard of it here */
    /* lstattach_entry is declared in console.c but not included here
     * to keep this file's extern list to what it actually calls --
     * include it below if a later pass needs its real side effect */
    extern cio_bxcx lstattach_entry(uint8_t *mem);
    (void)lstattach_entry(mem); /* real call, for its side effect; its
                                    own return value discarded */
    /* sub dh,ncondev!jmp listf -- no-op under the convention */
}

/*
 * constat_entry -- check console status
 *	output:	returns status (bl)
 */
uint8_t constat_entry(uint8_t *mem) {
    uint16_t si;
    coninit(mem, &si);
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (rlr == mem_r16(mem, (uint16_t)(si + c_attach))) {
        return conbrk(mem, si); /* cse_cl */
    }
    /* mov cx,f_dispatch!call mpmif -- boundary stub, no-op (also the
     * real DRI "mov cl,f_dispatch" 8-bit-move typo noted in the asm's
     * own comment -- irrelevant here either way) */
    return 0;
}

/*
 * dirio_entry -- direct console i/o
 *	input:	dl = 0xFF if readif, 0xFE if status, 0xFD if read (block),
 *		else a character to write
 *	output:	returns a status/character byte depending on dl
 */
uint8_t dirio_entry(uint8_t *mem, uint8_t dl) {
    uint16_t si;
    rawinit(mem, &si);
    attach(mem, si);
    if (dl == 0xFD) {
        return conin(mem, si); /* je conin */
    }
    if (dl < 0xFD) {
        /* jb rconout -- tail jump into conout (rconout is itself just
         * "jmp conout") */
        conout(mem, si, dl);
        return 0; /* conout's own tail-landing leaves no defined return
                     value here -- same class of artifact as
                     CPMMEM.MEM's cpmfree_entry bx, fixed at 0 */
    }
    uint8_t bl = conbrk(mem, si);
    if (dl == 0xFE) {
        bl = (uint8_t)(bl - 1);
        bl = (uint8_t)~bl;
        return bl;
    }
    /* diocin */
    if (bl != 0) {
        return conin(mem, si); /* jne conin */
    }
    /* mov cx,f_dispatch!call mpmif -- boundary stub, no-op */
    return 0;
}

/*
 * conwrite_entry -- write line until $ encountered
 *	input:	dx = buffer addr
 */
void conwrite_entry(uint8_t *mem, uint16_t dx) {
    conprint_entry(mem, dx, 0, '$');
}

/*
 * conprint_entry -- write line until delimiter or max
 *	input:	dx = address of buffer, bh = max characters (0=no max),
 *		bl = delimiter character
 */
void conprint_entry(uint8_t *mem, uint16_t dx, uint8_t bh, uint8_t bl) {
    uint16_t si;
    coninit(mem, &si);
    attach(mem, si);
    uint16_t bx = dx; /* BX -> buffer */
    uint8_t ah = bh;
    uint8_t al = (uint8_t)(bl & 0x7F);
    uint16_t cx = 0;
    for (;;) {
        if (ah == (uint8_t)cx && ah != 0) {
            return; /* cpr_e */
        }
        uint8_t dl = (uint8_t)(mem[bx] & 0x7F);
        if (dl == al) {
            return; /* cpr_e */
        }
        tabout(mem, si, dl);
        cx = (uint16_t)(cx + 1);
        bx = (uint16_t)(bx + 1);
    }
}

/*
 * conread_entry -- read a buffered console line, with ctl-H/rubout/
 * ctl-E/ctl-P/ctl-X/ctl-U/ctl-R line editing
 *	input:	dx = buffer address (buffer[0] = max length,
 *		buffer[1] = current length on return, buffer[2..] = data)
 */
void conread_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bufaddr = dx;
    for (;;) { /* jmp conread_entry -- restart with the same buffer */
        uint16_t si;
        uint16_t count;
        uint8_t al, dl;

        coninit(mem, &si);
        attach(mem, si);

    read:
        al = mem[(uint16_t)(si + c_column)];
        mem[(uint16_t)(si + c_strtcol)] = al;
        count = 0;

    readnx:
    readn0:
        dl = (uint8_t)(conin(mem, si) & 0x7F);
        if (dl == cr || dl == lf) {
            goto readen;
        }
        if (count != 0) {
            goto tryh;
        }
        if (chkctl(mem, si, dl) == 1) {
            goto tryh;
        }
        continue; /* pop dx!jmp conread_entry */

    tryh:
        if (dl != ctlh) {
            goto noth;
        }
        if (count == 0) {
            goto readnx;
        }
        count = (uint16_t)(count - 1);
        al = mem[(uint16_t)(si + c_column)];
        mem[(uint16_t)(mem_r16(mem, SD_RLR) + p_scratch)] = al;
        mem[(uint16_t)(si + c_flag)] |= cf_compc;
        goto linelen;

    noth:
        if (dl != rubout) {
            goto notrub;
        }
        if (count == 0) {
            goto readnx; /* readnxx -> readnx */
        }
        dl = mem[(uint16_t)(bufaddr + 1 + count)];
        count = (uint16_t)(count - 1);
        goto rdech1;

    notrub:
        if (dl != ctle) {
            goto note;
        }
        pcr(mem);
        /* mov dl,lf!call conoutf -- no-op; dl is overwritten by the
         * next conin() before anything else reads it, see header
         * comment */
        mem[(uint16_t)(si + c_column)] = 0;
        mem[(uint16_t)(si + c_strtcol)] = 0;
        goto readn0;

    note:
        if (dl != ctlp) {
            goto notp;
        }
        control_p(mem, si);
        goto readnx; /* readnxx -> readnx */

    notp:
        if (dl != ctlx) {
            goto notx;
        }
    backx:
        al = mem[(uint16_t)(si + c_strtcol)];
        if (al < mem[(uint16_t)(si + c_column)]) {
            goto backxxx;
        }
        count = 0;
        goto readnx; /* readnxx -> readnx */
    backxxx:
        pbacksp(mem);
        pspace(mem);
        pbacksp(mem);
        mem[(uint16_t)(si + c_column)] -= 1;
        goto backx;

    notx:
        if (dl != ctlu) {
            goto notu;
        }
        crlfp(mem, si);
        goto read;

    notu:
        if (dl != ctlr) {
            goto notr;
        }
    linelen:
        {
            uint16_t cl2 = count;
            crlfp(mem, si);
            uint16_t idx = 0;
        rep0:
            if (cl2 == 0) {
                goto rep1;
            }
            idx = (uint16_t)(idx + 1);
            cl2 = (uint16_t)(cl2 - 1);
            dl = mem[(uint16_t)(bufaddr + 1 + idx)];
            ctlout(mem, si, dl);
            goto rep0;
        rep1:
            if ((mem[(uint16_t)(si + c_flag)] & cf_compc) == 0) {
                goto l_22;
            }
            mem[(uint16_t)(si + c_flag)] = (uint8_t)(mem[(uint16_t)(si + c_flag)] & (uint8_t)~cf_compc);
        backsp:
            {
                uint16_t rlr = mem_r16(mem, SD_RLR);
                uint8_t al2 = (uint8_t)(mem[(uint16_t)(rlr + p_scratch)] - mem[(uint16_t)(si + c_column)]);
                if (al2 == 0) {
                    goto l_22;
                }
                pbacksp(mem);
                pspace(mem);
                pbacksp(mem);
                mem[(uint16_t)(rlr + p_scratch)] -= 1;
                goto backsp;
            }
        }
    l_22:
        goto readn0;

    notr:
        count = (uint16_t)(count + 1);
        mem[(uint16_t)(bufaddr + 1 + count)] = dl;
    rdech1:
        ctlout(mem, si, dl);
        if (count >= mem[bufaddr]) {
            goto readen;
        }
        goto l_22;

    readen:
        mem[(uint16_t)(bufaddr + 1)] = (uint8_t)count;
        pcr(mem);
        mem[(uint16_t)(si + c_column)] = 0;
        return;
    }
}
