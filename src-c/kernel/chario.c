/*
 * chario.c -- C port of src-assem/kernel/CHARIO.CIO: conin_entry,
 * conout_entry, rconin_entry, rconout_entry, listout_entry,
 * constat_entry, dirio_entry, conwrite_entry, conprint_entry,
 * conread_entry (+ internal rawinit, coninit, getconnum, attach,
 * echoc, conbrk, chkctl, control_p, prcsmsg, prname, conout, compout,
 * stdout, ctlout, tabout, pspace, pbacksp, pcr, crlf, crlfp, conin).
 * src-assem/kernel/CHARIO.CIO's own getccbadr is in src-c/kernel/console.c
 * (ported alongside CONSOLE.CIO, which needed it first -- see that
 * file's own header comment); this file calls back into it, and into
 * console.c's conattach_entry/condetach_entry/mimic_entry/
 * unmimic_entry, as ordinary cross-file calls (declared here with
 * locally-named but layout-identical types, matching the convention
 * src-c/kernel/que1.c already uses for its own cross-file calls into
 * que2.c).
 *
 * Every character-I/O call goes to the XIOS through CIOIF.CIO's `xiosif`
 * (listf/conoutf/constf/coninf and their shared xiosc/xiolst/xio1
 * tails). Here they all go through xiosc(), which makes the call
 * through a hook (cio_set_xios, defs.h) with the asm's AL/CL/DL and
 * returns the XIOS's AX/BX. With no hook set, every call returns 0:
 * nothing ready, a NUL read, output dropped -- the boundary-stub
 * convention. The tests set a hook that scripts the input and records
 * the calls, and compare that stream with the reference's.
 *
 * Under the no-hook convention two real loops can't end, as in the
 * asm with a no-op XIOS: `chkctl`'s ctl-S handler waits for ctl-Q or
 * ctl-C, and `cioattach`'s busy-console wait (console.c). The tests
 * drive the first with scripted input.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the CIO
 * module's DS). CCBs, PDs and process names are SYSDAT-relative; the
 * caller's print/read buffers (conwrite/conprint/conread's dx) are in
 * the running process's u_wrkseg, read from its UDA. The three
 * messages prcsmsg prints live in the CIO code segment in the asm
 * (`push u_wrkseg ! mov u_wrkseg,cs ! call conprint_entry`); here
 * they're C constants, printed by the same body conprint_entry runs.
 */
#include <stdint.h>

#include "defs.h"

MPMIF_HOOK(chario) /* f_terminate/f_dispatch (defs.h) */

extern uint16_t getccbadr(uint8_t *phys, uint16_t sysdat, uint8_t dh, uint16_t *bx_out);
extern cio_bxcx conattach_entry(uint8_t *phys, uint16_t sysdat);
extern cio_bxcx condetach_entry(uint8_t *phys, uint16_t sysdat);
extern cio_bxcx mimic_entry(uint8_t *phys, uint16_t sysdat, uint8_t dh_in, uint8_t dl_in);
extern cio_bxcx unmimic_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl_in);
extern cio_bxcx lstattach_entry(uint8_t *phys, uint16_t sysdat);

/* wrkseg_ptr -- the running process's u_wrkseg, from its UDA */
static uint8_t *wrkseg_ptr(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    return far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
}

/* The XIOS boundary (CIOIF.CIO's xiosif, a far call through SYSDAT's
 * xiosmod). Every character-I/O call CHARIO.CIO makes goes through
 * xiosc() with the asm's AL (function), CL and DL; when cio_xios is
 * set it handles the call and returns the XIOS's AX/BX, otherwise the
 * call returns 0 (the boundary-stub convention: nothing ready, a NUL
 * read, output dropped). See the header comment. */
static cio_xios_fn cio_xios;

void cio_set_xios(cio_xios_fn fn) {
    cio_xios = fn;
}

static uint8_t xiosc(uint8_t func, uint8_t cl, uint8_t dl) {
    return cio_xios ? (uint8_t)cio_xios(func, cl, dl) : 0;
}

/* the console/device number (DH) that goes with a CCB address */
static uint8_t ccbnum(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    return (uint8_t)((uint16_t)(si - seg_r16(far_ptr(phys, sysdat, 0), SYSDAT_CCB)) / ccblen);
}

/* conoutf/listf: "mov cl,dl ! mov dl,dh ! mov dh,0" -- char in CL,
 * device in DL; constf/coninf: "mov cl,dh" -- console in CL, the asm's
 * DL whatever it was (not modeled; passed as 0) */
static void conoutf(uint8_t dh, uint8_t dl) {
    (void)xiosc(io_conout, dl, dh);
}

static void listf(uint8_t dh, uint8_t dl) {
    (void)xiosc(io_list, dl, dh);
}

static uint8_t constf(uint8_t dh) {
    return xiosc(io_const, dh, 0);
}

static uint8_t coninf(uint8_t dh) {
    return xiosc(io_conin, dh, 0);
}

/* CHARIO.CIO's string constants, which live in the CIO code segment */
static const char ptrmsg[] = "\r\nPrinter Busy\r\n";
static const char abortmsg[] = ": Abort (Y/N)? ";
static const char attachmsg[] = "\r\nAttach: ";

/* getconnum -- get console number
 *	output:	returns console # (rlr's own p_cns)
 */
static uint8_t getconnum(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    return sd[(uint16_t)(rlr + p_cns)];
}

/* coninit / rawinit -- set up for normal / raw console I/O
 *	output:	*si_out = CCB address for the calling process's console
 */
static void coninit(uint8_t *phys, uint16_t sysdat, uint16_t *si_out) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dh = getconnum(phys, sysdat);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    seg_w16(sd, (uint16_t)(rlr + p_flag),
            (uint16_t)(seg_r16(sd, (uint16_t)(rlr + p_flag)) & ~(uint16_t)pf_raw));
    uint16_t bx;
    *si_out = getccbadr(phys, sysdat, dh, &bx);
}

static void rawinit(uint8_t *phys, uint16_t sysdat, uint16_t *si_out) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dh = getconnum(phys, sysdat);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    seg_w16(sd, (uint16_t)(rlr + p_flag),
            (uint16_t)(seg_r16(sd, (uint16_t)(rlr + p_flag)) | pf_raw));
    uint16_t bx;
    *si_out = getccbadr(phys, sysdat, dh, &bx);
}

static void prcsmsg(uint8_t *phys, uint16_t sysdat, const char *msg);
static void prname(uint8_t *phys, uint16_t sysdat, uint16_t dx);
static void conout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl);
static void tabout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl);
static void crlf(uint8_t *phys, uint16_t sysdat, uint16_t si);
static void crlfp(uint8_t *phys, uint16_t sysdat, uint16_t si);
static uint8_t conbrk(uint8_t *phys, uint16_t sysdat, uint16_t si);
static uint8_t conin(uint8_t *phys, uint16_t sysdat, uint16_t si);
static uint8_t chkctl(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t bl);
static void ctlout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl);
static void pspace(uint8_t *phys, uint16_t sysdat, uint16_t si);
static void pbacksp(uint8_t *phys, uint16_t sysdat, uint16_t si);
static void pcr(uint8_t *phys, uint16_t sysdat, uint16_t si);
void conprint_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint8_t bh, uint8_t bl);

/* attach -- attach the calling process to its default console
 *	input:	si = CCB address (from coninit/rawinit)
 */
static void attach(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    if (seg_r16(sd, (uint16_t)(si + c_attach)) == rlr) {
        return; /* attret */
    }
    (void)conattach_entry(phys, sysdat); /* real call, for effect; its own return
                                    value is discarded, same as the asm's
                                    own disregard of it here */
    rlr = seg_r16(sd, SYSDAT_RLR);
    uint16_t pflag = seg_r16(sd, (uint16_t)(rlr + p_flag));
    if ((pflag & pf_ctld) == 0) {
        return; /* noattmsg */
    }
    seg_w16(sd, (uint16_t)(rlr + p_flag), (uint16_t)(pflag & ~(uint16_t)pf_ctld));
    prcsmsg(phys, sysdat, attachmsg);
    prname(phys, sysdat, (uint16_t)(rlr + p_name));
    crlf(phys, sysdat, si);
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
static uint8_t conbrk(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dh = ccbnum(phys, sysdat, si);
    if (sd[(uint16_t)(si + c_nchar)] != 0) {
        return 1; /* conb_y */
    }
    /* char not ready, check external break */
    if ((constf(dh) & 1) == 0) {
        return 0; /* conb_no */
    }
    uint8_t bl = coninf(dh);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    if ((seg_r16(sd, (uint16_t)(rlr + p_flag)) & pf_raw) == 0) {
        if (chkctl(phys, sysdat, si, bl) == 0) {
            return 0; /* conb_no */
        }
    }
    sd[(uint16_t)(si + c_nchar)] = bl; /* conb0 */
    return 1;
}

/* conin -- read next character
 *	input:	si = CCB address
 *	output:	returns the character; clears c_nchar[si]
 */
static uint8_t conin(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dl = sd[(uint16_t)(si + c_nchar)];
    sd[(uint16_t)(si + c_nchar)] = 0;
    if (dl != 0) {
        return dl; /* ciout */
    }
    return coninf(ccbnum(phys, sysdat, si));
}

/* chkctl -- check character for ctl-C/ctl-D/ctl-S
 *	input:	si = CCB address, bl = character to check
 *	output:	returns 0 if a ctl char was found (and handled), 1 if not
 */
static uint8_t chkctl(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t bl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dh = ccbnum(phys, sysdat, si);
    if (bl == ctld) {
        uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
        seg_w16(sd, (uint16_t)(rlr + p_flag),
                (uint16_t)(seg_r16(sd, (uint16_t)(rlr + p_flag)) | pf_ctld));
        (void)condetach_entry(phys, sysdat);
        return 0; /* chk0 */
    }
    if (bl == ctls) {
        /* control_s: wait for ctl-Q (resume) or ctl-C, ringing the bell
         * at anything else */
        for (;;) {
            bl = coninf(dh);
            if (bl == ctlc) {
                goto chkc1;
            }
            if (bl == ctlq) {
                return 0; /* chk0 */
            }
            conoutf(dh, bell);
        }
    }
    if (bl != ctlc) {
        return 1; /* chk1 */
    }
chkc1:
    /* print Abort string, get the answer */
    crlf(phys, sysdat, si);
    prname(phys, sysdat, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_name));
    prcsmsg(phys, sysdat, abortmsg);
    bl = (uint8_t)(coninf(dh) & 0x5F);
    if (bl >= ' ') {
        conoutf(dh, bl);
        if (bl == 'Y') {
            uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
            seg_w16(sd, (uint16_t)(rlr + p_flag),
                    (uint16_t)(seg_r16(sd, (uint16_t)(rlr + p_flag)) | pf_ctlc));
            (void)MPMIF(chario, phys, sysdat, f_terminate, 0, 0, sysdat); /* sub dx,dx */
        }
    }
    /* ctlc_r */
    if (bl == ctlc) {
        goto chkc1;
    }
    crlf(phys, sysdat, si);
    return 0; /* chk0 */
}

/* control_p -- toggle list-device mimicking for the current console */
static void control_p(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    if (sd[(uint16_t)(si + c_flag)] & cf_listcp) {
        uint8_t dl = sd[(uint16_t)(si + c_mimic)];
        cio_bxcx r = unmimic_entry(phys, sysdat, dl);
        if (r.cx == 0) {
            sd[(uint16_t)(si + c_flag)] ^= cf_listcp; /* mimout */
        }
        return; /* mimpret */
    }
    uint8_t dh = sd[(uint16_t)(rlr + p_cns)];
    uint8_t dl = sd[(uint16_t)(rlr + p_lst)];
    cio_bxcx r = mimic_entry(phys, sysdat, dh, dl);
    if (r.cx == 0) {
        sd[(uint16_t)(si + c_flag)] ^= cf_listcp; /* mimout */
        return;
    }
    prcsmsg(phys, sysdat, ptrmsg);
}

static void cprint(uint8_t *phys, uint16_t sysdat, const uint8_t *src, uint16_t dx, uint8_t bh, uint8_t bl);

/* prcsmsg -- print a NUL-terminated string in the CIO code segment. The
 * asm points u_wrkseg at CS around a call to conprint_entry; this runs
 * the same body (cprint) straight over the C string.
 * prname -- print an 8-char name in DS (SYSDAT), stopping at a space;
 * the asm does the same with u_wrkseg = DS. */
static void prcsmsg(uint8_t *phys, uint16_t sysdat, const char *msg) {
    cprint(phys, sysdat, (const uint8_t *)msg, 0, 0, 0);
}

static void prname(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    cprint(phys, sysdat, far_ptr(phys, sysdat, 0), dx, 8, ' ');
}

/* compout -- track column position for a character about to be/having
 * been written
 */
static void compout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t al = sd[(uint16_t)(si + c_column)];
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
    sd[(uint16_t)(si + c_column)] = al;
}

/* stdout -- write to console/list device dh (the mimic forwarding) */
static void stdout_(uint8_t *phys, uint16_t sysdat, uint8_t dh, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t ncondev = seg_r8(sd, SYSDAT_NCONDEV);
    if ((int8_t)dh < (int8_t)ncondev) { /* jl: signed */
        conoutf(dh, dl); /* xcout */
        return;
    }
    dh = (uint8_t)(dh - ncondev);
    if ((int8_t)dh < (int8_t)seg_r8(sd, SYSDAT_NLSTDEV)) {
        listf(dh, dl); /* xlout */
    }
}

/* conout -- write one character to the console, tracking column
 *	input:	si = CCB address, dl = character
 */
static void conout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if ((sd[(uint16_t)(si + c_flag)] & cf_compc) == 0) {
        (void)conbrk(phys, sysdat, si); /* result discarded, side-effect only */
        attach(phys, sysdat, si);
        conoutf(ccbnum(phys, sysdat, si), dl);
        /* check if mimic device */
        uint8_t mimic = sd[(uint16_t)(si + c_mimic)];
        if (mimic != 0xFF) {
            stdout_(phys, sysdat, mimic, dl);
        }
    }
    compout(phys, sysdat, si, dl);
}

/* tabout -- expand tabs to the console, otherwise a direct tail-call
 * to conout
 */
static void tabout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (dl != tab) {
        conout(phys, sysdat, si, dl);
        return;
    }
    do {
        conout(phys, sysdat, si, ' ');
    } while ((sd[(uint16_t)(si + c_column)] & 0x07) != 0);
}

/* ctlout -- send a (possibly control) character, with a preceding
 * up-arrow if it isn't graphic
 */
static void ctlout(uint8_t *phys, uint16_t sysdat, uint16_t si, uint8_t dl) {
    if (!echoc(dl)) {
        tabout(phys, sysdat, si, dl);
        return;
    }
    conout(phys, sysdat, si, ctl);
    dl = (uint8_t)(dl | 0x40);
    tabout(phys, sysdat, si, dl);
}

/* pspace/pbacksp/pcr -- "jmps conoutf" with a fixed character, no
 * column tracking */
static void pspace(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    conoutf(ccbnum(phys, sysdat, si), ' ');
}

static void pbacksp(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    conoutf(ccbnum(phys, sysdat, si), ctlh);
}

static void pcr(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    conoutf(ccbnum(phys, sysdat, si), cr);
}

/* crlf -- carriage return / line feed */
static void crlf(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    conout(phys, sysdat, si, cr);
    conout(phys, sysdat, si, lf);
}

/* crlfp -- print '#', crlf, then pad with spaces out to strtcol --
 * used by the ctl-X/ctl-U/ctl-R line-editing handlers
 */
static void crlfp(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    conout(phys, sysdat, si, '#');
    crlf(phys, sysdat, si);
    for (;;) {
        if (sd[(uint16_t)(si + c_column)] >= sd[(uint16_t)(si + c_strtcol)]) {
            return; /* crlfp1 */
        }
        conout(phys, sysdat, si, ' ');
    }
}

/*
 * conin_entry -- Function 1: console input
 *	output:	returns the character
 */
uint8_t conin_entry(uint8_t *phys, uint16_t sysdat) {
    for (;;) { /* je conin_entry -- retry from the top */
        uint16_t si;
        coninit(phys, sysdat, &si);
        attach(phys, sysdat, si);
        (void)conbrk(phys, sysdat, si); /* result discarded, side-effect only */
        uint8_t dl = conin(phys, sysdat, si);
        uint8_t al = chkctl(phys, sysdat, si, dl);
        if (al == 0) {
            continue;
        }
        if (!echoc(dl)) {
            tabout(phys, sysdat, si, dl);
        }
        return dl; /* ciexit */
    }
}

/*
 * conout_entry -- Function 2: console output
 *	input:	dl = character
 */
void conout_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint16_t si;
    coninit(phys, sysdat, &si);
    attach(phys, sysdat, si);
    tabout(phys, sysdat, si, dl);
}

/*
 * rconin_entry -- raw console input
 *	output:	returns the character
 */
uint8_t rconin_entry(uint8_t *phys, uint16_t sysdat) {
    uint16_t si;
    rawinit(phys, sysdat, &si);
    attach(phys, sysdat, si);
    return conin(phys, sysdat, si);
}

/*
 * rconout_entry -- raw console output
 *	input:	dl = character
 */
void rconout_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint16_t si;
    rawinit(phys, sysdat, &si);
    attach(phys, sysdat, si);
    conout(phys, sysdat, si, dl);
}

/*
 * listout_entry -- write to the list device
 *	input:	dl = character
 */
void listout_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    uint8_t dh = sd[(uint16_t)(rlr + p_lst)];
    (void)lstattach_entry(phys, sysdat); /* real call, for its side effect; its
                                             own return value discarded */
    listf((uint8_t)(dh - seg_r8(sd, SYSDAT_NCONDEV)), dl); /* sub dh,ncondev ! jmp listf */
}

/*
 * constat_entry -- check console status
 *	output:	returns status (bl)
 */
uint8_t constat_entry(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t si;
    coninit(phys, sysdat, &si);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    if (rlr == seg_r16(sd, (uint16_t)(si + c_attach))) {
        return conbrk(phys, sysdat, si); /* cse_cl */
    }
    /* mov cx,f_dispatch!call mpmif (the real DRI "mov cl,f_dispatch"
     * 8-bit-move typo noted in the asm's own comment is fixed there) */
    (void)MPMIF(chario, phys, sysdat, f_dispatch, 0, 0, sysdat);
    return 0;
}

/*
 * dirio_entry -- direct console i/o
 *	input:	dl = 0xFF if readif, 0xFE if status, 0xFD if read (block),
 *		else a character to write
 *	output:	returns a status/character byte depending on dl
 */
uint8_t dirio_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint16_t si;
    rawinit(phys, sysdat, &si);
    attach(phys, sysdat, si);
    if (dl == 0xFD) {
        return conin(phys, sysdat, si); /* je conin */
    }
    if (dl < 0xFD) {
        /* jb rconout -- tail jump into conout (rconout is itself just
         * "jmp conout") */
        conout(phys, sysdat, si, dl);
        return 0; /* conout's own tail-landing leaves no defined return
                     value here -- same class of artifact as
                     CPMMEM.MEM's cpmfree_entry bx, fixed at 0 */
    }
    uint8_t bl = conbrk(phys, sysdat, si);
    if (dl == 0xFE) {
        bl = (uint8_t)(bl - 1);
        bl = (uint8_t)~bl;
        return bl;
    }
    /* diocin */
    if (bl != 0) {
        return conin(phys, sysdat, si); /* jne conin */
    }
    (void)MPMIF(chario, phys, sysdat, f_dispatch, 0, 0, sysdat); /* mov cx,f_dispatch!call mpmif */
    return 0;
}

/*
 * conwrite_entry -- write line until $ encountered
 *	input:	dx = buffer addr
 */
void conwrite_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    conprint_entry(phys, sysdat, dx, 0, '$');
}

/*
 * conprint_entry -- write line until delimiter or max
 *	input:	dx = address of buffer, bh = max characters (0=no max),
 *		bl = delimiter character
 */
void conprint_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint8_t bh, uint8_t bl) {
    cprint(phys, sysdat, wrkseg_ptr(phys, sysdat), dx, bh, bl);
}

/* conprint_cs -- conprint_entry for a string in the caller's code
 * segment: the asm points u_wrkseg at CS for the call (COMMAND.SUP's
 * prcsm); here the string is a C constant */
void conprint_cs(uint8_t *phys, uint16_t sysdat, const char *s, uint8_t bh, uint8_t bl) {
    cprint(phys, sysdat, (const uint8_t *)s, 0, bh, bl);
}

/* cprint -- conprint_entry's body, reading the buffer at src:dx */
static void cprint(uint8_t *phys, uint16_t sysdat, const uint8_t *src, uint16_t dx, uint8_t bh, uint8_t bl) {
    uint16_t si;
    coninit(phys, sysdat, &si);
    attach(phys, sysdat, si);
    uint16_t bx = dx; /* BX -> buffer */
    uint8_t ah = bh;
    uint8_t al = (uint8_t)(bl & 0x7F);
    uint16_t cx = 0;
    for (;;) {
        if (ah == (uint8_t)cx && ah != 0) {
            return; /* cpr_e */
        }
        uint8_t dl = (uint8_t)(seg_r8(src, bx) & 0x7F);
        if (dl == al) {
            return; /* cpr_e */
        }
        tabout(phys, sysdat, si, dl);
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
void conread_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t bufaddr = dx;
    uint8_t *buf = wrkseg_ptr(phys, sysdat); /* the buffer, in u_wrkseg */
    for (;;) { /* jmp conread_entry -- restart with the same buffer */
        uint16_t si;
        uint16_t count;
        uint8_t al, dl;

        coninit(phys, sysdat, &si);
        attach(phys, sysdat, si);

    read:
        al = sd[(uint16_t)(si + c_column)];
        sd[(uint16_t)(si + c_strtcol)] = al;
        count = 0;

    readnx:
    readn0:
        dl = (uint8_t)(conin(phys, sysdat, si) & 0x7F);
        if (dl == cr || dl == lf) {
            goto readen;
        }
        if (count != 0) {
            goto tryh;
        }
        if (chkctl(phys, sysdat, si, dl) == 1) {
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
        al = sd[(uint16_t)(si + c_column)];
        sd[(uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_scratch)] = al;
        sd[(uint16_t)(si + c_flag)] |= cf_compc;
        goto linelen;

    noth:
        if (dl != rubout) {
            goto notrub;
        }
        if (count == 0) {
            goto readnx; /* readnxx -> readnx */
        }
        dl = buf[(uint16_t)(bufaddr + 1 + count)];
        count = (uint16_t)(count - 1);
        goto rdech1;

    notrub:
        if (dl != ctle) {
            goto note;
        }
        pcr(phys, sysdat, si);
        conoutf(ccbnum(phys, sysdat, si), lf);
        sd[(uint16_t)(si + c_column)] = 0;
        sd[(uint16_t)(si + c_strtcol)] = 0;
        goto readn0;

    note:
        if (dl != ctlp) {
            goto notp;
        }
        control_p(phys, sysdat, si);
        goto readnx; /* readnxx -> readnx */

    notp:
        if (dl != ctlx) {
            goto notx;
        }
    backx:
        al = sd[(uint16_t)(si + c_strtcol)];
        if (al < sd[(uint16_t)(si + c_column)]) {
            goto backxxx;
        }
        count = 0;
        goto readnx; /* readnxx -> readnx */
    backxxx:
        pbacksp(phys, sysdat, si);
        pspace(phys, sysdat, si);
        pbacksp(phys, sysdat, si);
        sd[(uint16_t)(si + c_column)] -= 1;
        goto backx;

    notx:
        if (dl != ctlu) {
            goto notu;
        }
        crlfp(phys, sysdat, si);
        goto read;

    notu:
        if (dl != ctlr) {
            goto notr;
        }
    linelen:
        {
            uint16_t cl2 = count;
            crlfp(phys, sysdat, si);
            uint16_t idx = 0;
        rep0:
            if (cl2 == 0) {
                goto rep1;
            }
            idx = (uint16_t)(idx + 1);
            cl2 = (uint16_t)(cl2 - 1);
            dl = buf[(uint16_t)(bufaddr + 1 + idx)];
            ctlout(phys, sysdat, si, dl);
            goto rep0;
        rep1:
            if ((sd[(uint16_t)(si + c_flag)] & cf_compc) == 0) {
                goto l_22;
            }
            sd[(uint16_t)(si + c_flag)] = (uint8_t)(sd[(uint16_t)(si + c_flag)] & (uint8_t)~cf_compc);
        backsp:
            {
                uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
                uint8_t al2 = (uint8_t)(sd[(uint16_t)(rlr + p_scratch)] - sd[(uint16_t)(si + c_column)]);
                if (al2 == 0) {
                    goto l_22;
                }
                pbacksp(phys, sysdat, si);
                pspace(phys, sysdat, si);
                pbacksp(phys, sysdat, si);
                sd[(uint16_t)(rlr + p_scratch)] -= 1;
                goto backsp;
            }
        }
    l_22:
        goto readn0;

    notr:
        count = (uint16_t)(count + 1);
        buf[(uint16_t)(bufaddr + 1 + count)] = dl;
    rdech1:
        ctlout(phys, sysdat, si, dl);
        if (count >= buf[bufaddr]) {
            goto readen;
        }
        goto l_22;

    readen:
        buf[(uint16_t)(bufaddr + 1)] = (uint8_t)count;
        pcr(phys, sysdat, si);
        sd[(uint16_t)(si + c_column)] = 0;
        return;
    }
}
