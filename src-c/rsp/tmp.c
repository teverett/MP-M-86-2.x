/*
 * tmp.c -- C version of src-assem/rsp/TMP.A86, the Terminal Message
 * Processor: the console's command interpreter. It prints the prompt,
 * reads a command line, handles the drive change (X:) and the USER and
 * PRINTER built-ins itself, and hands anything else to the CLI
 * (f_clicmd), reporting the CLI's errors.
 *
 * Its data segment, below, is TMP.A86's DSEG byte for byte (rsp.h).
 * TMP's messages live in its code segment in the .A86 (printstring
 * prints them with DS = CS); tmp_main puts the same bytes at the same
 * offsets of its real code segment and prints them the same way.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "rsp_syms.h"

#define false 0
#define true  0xFF

#define mpm_conout     2
#define mpm_conwrite   9
#define mpm_conread    10
#define mpm_diskselect 14
#define mpm_getdefdisk 25
#define mpm_usercode   32
#define mpm_freedrive  39
#define mpm_conattach  146
#define mpm_condetach  147
#define mpm_setdefcon  148
#define mpm_clicmd     150
#define mpm_parse      152
#define mpm_setdeflst  160
#define mpm_getdeflst  164

#define s_mpmseg  0x40 /* word: begin MPM segment */
#define s_sysdisk 0x4B /* byte: system disk */
#define s_ncns    0x47 /* byte: sys. consoles */
#define s_version 0x78 /* word: ofst ver. str in SUP */

#define e_q_full_    15
#define e_illdisk_   23
#define e_badfname_  24
#define e_badftype_  25
#define e_bad_load_  28
#define e_bad_read_  29
#define e_bad_open_  30
#define e_nullcmd_   31
#define e_ill_passwd_ 38

#define maxcmdlen 128

typedef struct RSP_PACKED {
    uint16_t sysdatseg; /* rsp_link: INIT.SUP gives the RSP SYSDAT here */
    uint16_t sdatvar;
    uint8_t defconsole, pad;
    uint16_t hdr[5];
    rsp_pd_t pd;
    rsp_uda_t uda;
    uint8_t dma[128];
    uint16_t stack[12];
    rsp_frame_t stack_top;
    uint8_t read_maxcmd; /* the Read Console Buffer and the CLICB share */
    uint8_t read_blen;   /* memory: read_blen is clicb_net */
    uint8_t clicb_cmd[maxcmdlen + 1];
    uint8_t cmdsent;
    uint16_t parseret;
    uint16_t pcb[2];
    uint8_t fcb[32];
    uint8_t savebuf[128];
    uint8_t hexend; /* "make sure hex is formed" */
} tmp_data;

_Static_assert(offsetof(tmp_data, sdatvar) == TMP_SDATVAR, "sdatvar");
_Static_assert(offsetof(tmp_data, defconsole) == TMP_DEFCONSOLE, "defconsole");
_Static_assert(offsetof(tmp_data, pd) == TMP_PD, "pd");
_Static_assert(offsetof(tmp_data, uda) == TMP_UDA, "uda");
_Static_assert(offsetof(tmp_data, dma) == TMP_DMA, "dma");
_Static_assert(offsetof(tmp_data, stack) == TMP_STACK, "stack");
_Static_assert(offsetof(tmp_data, stack_top) == TMP_STACK_TOP, "stack_top");
_Static_assert(offsetof(tmp_data, read_maxcmd) == TMP_READ_BUF, "read_buf");
_Static_assert(offsetof(tmp_data, read_blen) == TMP_CLICB_NET, "clicb_net");
_Static_assert(offsetof(tmp_data, read_blen) == TMP_CLICB, "clicb");
_Static_assert(offsetof(tmp_data, clicb_cmd) == TMP_CLICB_CMD, "clicb_cmd");
_Static_assert(offsetof(tmp_data, cmdsent) == TMP_CMDSENT, "cmdsent");
_Static_assert(offsetof(tmp_data, parseret) == TMP_PARSERET, "parseret");
_Static_assert(offsetof(tmp_data, pcb) == TMP_PCB, "pcb");
_Static_assert(offsetof(tmp_data, fcb) == TMP_FCB, "fcb");
_Static_assert(offsetof(tmp_data, savebuf) == TMP_SAVEBUF, "savebuf");

const uint16_t tmp_image_len = sizeof(tmp_data);
const tmp_data tmp_image = {
    .sdatvar = s_ncns,
    .pd = RSP_PD(198, pf_sys | pf_keep, "Tmp     ", 0xFFFF),
    .uda = RSP_UDA(TMP_DMA, TMP_STACK_TOP),
    .stack = {0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC,
              0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC},
    .read_maxcmd = 128,
    .pcb = {TMP_SAVEBUF, TMP_FCB},
};

/* the constants in TMP.A86's code segment, from `prompt` on */
typedef struct RSP_PACKED {
    char prompt[2];
    uint8_t crlfstr[3];
    char memerr[19], pderr[15], fnameerr[15], loaderr[12], openerr[20], catcherr[2], qfullerr[22];
    uint8_t usererrmsg[33];
    uint8_t usermsg[17];
    uint8_t printemsg[36];
    uint8_t printermsg[20];
    char usercmd[8], printercmd[8];
} tmp_consts_t;

#define CONST_AT(f) (TMP_PROMPT + offsetof(tmp_consts_t, f))
_Static_assert(CONST_AT(crlfstr) == TMP_CRLFSTR, "crlfstr");
_Static_assert(CONST_AT(memerr) == TMP_MEMERR, "memerr");
_Static_assert(CONST_AT(pderr) == TMP_PDERR, "pderr");
_Static_assert(CONST_AT(fnameerr) == TMP_FNAMEERR, "fnameerr");
_Static_assert(CONST_AT(loaderr) == TMP_LOADERR, "loaderr");
_Static_assert(CONST_AT(openerr) == TMP_OPENERR, "openerr");
_Static_assert(CONST_AT(catcherr) == TMP_CATCHERR, "catcherr");
_Static_assert(CONST_AT(qfullerr) == TMP_QFULLERR, "qfullerr");
_Static_assert(CONST_AT(usererrmsg) == TMP_USERERRMSG, "usererrmsg");
_Static_assert(CONST_AT(usermsg) == TMP_USERMSG, "usermsg");
_Static_assert(CONST_AT(printemsg) == TMP_PRINTEMSG, "printemsg");
_Static_assert(CONST_AT(printermsg) == TMP_PRINTERMSG, "printermsg");
_Static_assert(CONST_AT(usercmd) == TMP_USERCMD, "usercmd");
_Static_assert(CONST_AT(printercmd) == TMP_PRINTERCMD, "printercmd");

static const tmp_consts_t tmp_consts = {
    ">$",
    {13, 10, '$'},
    "?Not Enough Memory$", "?PD Table Full$", "?Bad File Spec$", "?Load Error$",
    "?Can't Find Command$", "?$", "?RSP Command Que Full$",
    "\r\nInvalid User Number, IGNORED\r\n$",
    "\r\nUser Number = $",
    "\r\nInvalid Printer Number, IGNORED\r\n$",
    "\r\nPrinter Number = $",
    "USER    ", "PRINTER ",
};

/* one TMP: TMP.A86's code is shared by a copy of the data per console,
 * so these helpers take the copy they run for (DS, and the shared CS) */
typedef struct {
    tmp_data *d;
    uint16_t ds, cs;
} tmp_t;

static mpm_regs mpm(const tmp_t *t, uint8_t cl, uint16_t dx) { return rt_mpm(t->ds, cl, dx); }
static void prchar(const tmp_t *t, uint8_t dl) { (void)mpm(t, mpm_conout, dl); }
static void print_ds_string(const tmp_t *t, uint16_t dx) { (void)mpm(t, mpm_conwrite, dx); }
static void printstring(const tmp_t *t, uint16_t dx) { (void)rt_mpm(t->cs, mpm_conwrite, dx); } /* DS = CS */
static void crlf(const tmp_t *t) { printstring(t, TMP_CRLFSTR); }
static uint8_t getuser(const tmp_t *t) { return (uint8_t)mpm(t, mpm_usercode, 0xFF).bx; }
static void setuser(const tmp_t *t, uint8_t dl) { (void)mpm(t, mpm_usercode, dl); }
static uint8_t getdisk(const tmp_t *t) { return (uint8_t)mpm(t, mpm_getdefdisk, 0).bx; }
static void setdisk(const tmp_t *t, uint8_t dl) { (void)mpm(t, mpm_diskselect, dl); }
static uint8_t getlist(const tmp_t *t) { return (uint8_t)mpm(t, mpm_getdeflst, 0).bx; }

static void prnum(const tmp_t *t, uint8_t dl) { /* dl = num (0-15) */
    if (dl >= 10) {
        prchar(t, '1');
        dl = (uint8_t)(dl - 10);
    }
    prchar(t, (uint8_t)(dl + '0'));
}

/* parsefilename: SI = fcb, DI = string; returns BX, CX */
static mpm_regs parsefilename(const tmp_t *t, uint16_t si, uint16_t di) {
    t->d->pcb[0] = di;
    t->d->pcb[1] = si;
    return mpm(t, mpm_parse, TMP_PCB);
}

/* a_to_b: dl = 1st char, dh = 2nd char; returns bl (0FFh if not 0-99) */
static uint8_t a_to_b(uint8_t dl, uint8_t dh) {
    if (dh == ' ') {
        dh = dl;
        dl = '0';
    }
    if (dh < '0' || dh > '9' || dl < '0' || dl > '9') return 0xFF;
    return (uint8_t)((dl - '0') * 10 + (dh - '0'));
}

/* clierror: the CLI call failed, display an error message */
static void clierror(const tmp_t *t, uint16_t cx) {
    tmp_data *d = t->d;
    uint16_t dx;
    if (cx == e_nullcmd_) {
        d->cmdsent = false;
        return;
    }
    if (cx == e_no_memory) dx = TMP_MEMERR;
    else if (cx == e_no_pd) dx = TMP_PDERR;
    else if (cx == e_badfname_ || cx == e_illdisk_ || cx == e_ill_passwd_ || cx == e_badftype_) dx = TMP_FNAMEERR;
    else if (cx == e_bad_load_ || cx == e_bad_read_) dx = TMP_LOADERR;
    else if (cx == e_bad_open_) dx = TMP_OPENERR;
    else if (cx == e_q_full_) dx = TMP_QFULLERR;
    else dx = TMP_CATCHERR;
    /* showerr */
    print_ds_string(t, TMP_SAVEBUF);
    prchar(t, ':');
    prchar(t, ' ');
    printstring(t, dx);
    crlf(t);
}

static int fcb_name_is(const tmp_data *d, const char *name) {
    for (int i = 0; i < 8; i++) {
        if (d->fcb[1 + i] != (uint8_t)name[i]) return 0;
    }
    return 1;
}

void tmp_main(uint16_t ds) {
    tmp_data *d = RSP_DATA(tmp_data, ds);
    tmp_t tt = {d, ds, rsp_cs()};
    const tmp_t *t = &tt;
    rsp_place(t->cs, TMP_PROMPT, &tmp_consts, sizeof tmp_consts);
    const uint8_t *sys = far_ptr(rt_phys(), d->sysdatseg, 0);

    /* set default console # = TMP#, default disk = the system disk,
     * default user # = console */
    (void)mpm(t, mpm_setdefcon, d->defconsole);
    setdisk(t, sys[s_sysdisk]);
    setuser(t, d->defconsole);

    /* print version: DS = s_mpmseg, DX = s_version */
    (void)mpm(t, mpm_conattach, 0);
    (void)rt_mpm(rd16(sys + s_mpmseg), mpm_conwrite, rd16(sys + s_version));
    (void)mpm(t, mpm_condetach, 0);

    for (;;) {
        /* nextcommand: free drive, attach console */
        (void)mpm(t, mpm_freedrive, 0xFFFF);
        (void)mpm(t, mpm_conattach, 0);
        if (d->cmdsent != false) { /* CR,LF if we just sent a command */
            d->cmdsent = false;
            crlf(t);
        }
        /* the prompt: user # and disk, from the current defaults */
        prchar(t, cr);
        prnum(t, getuser(t));
        prchar(t, (uint8_t)('A' + getdisk(t)));
        printstring(t, TMP_PROMPT);

        (void)mpm(t, mpm_conread, TMP_READ_BUF);
        prchar(t, lf);

        /* make sure not a null command */
        if (d->read_blen == 0 || d->clicb_cmd[0] == ';') continue;
        /* 'X:' changes the default disk to X */
        if (d->read_blen == 2 && d->clicb_cmd[1] == ':') {
            uint8_t dl = (uint8_t)((d->clicb_cmd[0] & 0x5F) - 'A');
            if (dl <= 15) setdisk(t, dl);
            continue;
        }

        /* clicall: null at the end of the input; copy the command for
         * error reporting and the built-in checks */
        d->clicb_cmd[d->read_blen] = 0;
        for (int i = 0; i < 128; i++) d->savebuf[i] = d->clicb_cmd[i];
        mpm_regs r = parsefilename(t, TMP_FCB, TMP_SAVEBUF);
        if (r.cx != 0) {
            d->savebuf[d->read_blen] = '$';
            clierror(t, r.cx);
            continue;
        }
        /* goodparse */
        d->parseret = r.bx;
        uint16_t bx = r.bx;
        if (bx == 0) bx = (uint16_t)(TMP_SAVEBUF + d->read_blen);
        ((uint8_t *)d)[bx] = '$';

        if (d->fcb[0] == 0) {
            /* try_builtin: USER [n] */
            if (fcb_name_is(d, tmp_consts.usercmd)) {
                if (d->parseret != 0) {
                    r = parsefilename(t, TMP_FCB, (uint16_t)(d->parseret + 1));
                    if (r.cx == 0) {
                        uint8_t bl = a_to_b(d->fcb[1], d->fcb[2]);
                        if (bl > 15) printstring(t, TMP_USERERRMSG);
                        else setuser(t, bl);
                    }
                }
                printstring(t, TMP_USERMSG); /* pruser */
                prnum(t, getuser(t));
                crlf(t);
                continue;
            }
            /* PRINTER [n] */
            if (fcb_name_is(d, tmp_consts.printercmd)) {
                if (d->parseret != 0) {
                    r = parsefilename(t, TMP_FCB, (uint16_t)(d->parseret + 1));
                    if (r.cx == 0) {
                        uint8_t bl = a_to_b(d->fcb[1], d->fcb[2]);
                        if (bl == 0xFF || mpm(t, mpm_setdeflst, bl).cx != 0) printstring(t, TMP_PRINTEMSG);
                    }
                }
                printstring(t, TMP_PRINTERMSG); /* prprinter */
                prnum(t, getlist(t));
                crlf(t);
                continue;
            }
        }

        /* not_builtin: initialize the Cli Control Block, make the call */
        d->read_blen = 0; /* clicb_net */
        d->cmdsent = true;
        r = mpm(t, mpm_clicmd, TMP_CLICB);
        if (r.bx != 0) clierror(t, r.cx);
    }
}
