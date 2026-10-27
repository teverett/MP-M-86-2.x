/*
 * submit.c -- C version of src/rsp/SUBMIT.A86 (a transient,
 * SUBMIT.CMD): it runs the commands in a .SUB file one line at a time,
 * putting the command tail's arguments in for $1..$99, printing each
 * command before handing it to the CLI, and handling X:, USER, PRINTER
 * and $INCLUDE itself.
 *
 * Its data group, below, is SUBMIT.A86's DSEG byte for byte (rsp.h),
 * messages included; the FCB and command tail in its base page are
 * filled in by LOAD and the CLI, so submit_main installs the image from
 * 100h on.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "rsp_syms.h"

#define false 0
#define true  0xFF

#define mpm_conin      1
#define mpm_conout     2
#define mpm_conwrite   9
#define mpm_version    12
#define mpm_diskselect 14
#define mpm_openfile   15
#define mpm_readfile   20
#define mpm_getdefdisk 25
#define mpm_setdma     26
#define mpm_usercode   32
#define mpm_setprior   145
#define mpm_conattach  146
#define mpm_clicmd     150
#define mpm_parse      152
#define mpm_getdefcon  153
#define mpm_getpdadr   156
#define mpm_setdeflst  160
#define mpm_getdeflst  164

/* fcb offsets */
#define mode2 6  /* 2nd open mode byte */
#define ftype 9  /* file type */
#define fcb_cr 32 /* current record */

#define e_q_full_   15
#define e_badfname_ 24
#define e_badftype_ 25
#define e_bad_load_ 28
#define e_bad_read_ 29
#define e_bad_open_ 30
#define e_nullcmd_  31

#define maxcmdlen 128

typedef struct RSP_PACKED {
    uint8_t base[0x5C];
    uint8_t fcb[0x24];
    uint8_t cmdtail[128];
    uint16_t stack[30];
    uint8_t dma[128];
    uint16_t numargs;
    uint8_t argtable[64];
    uint8_t deblankbuf[maxcmdlen + 1];
    uint8_t cmdbuf, blen; /* the Read Console Buffer readbuffer fills */
    uint8_t cmd[maxcmdlen + 1];
    uint8_t clicb_net; /* clicb */
    uint8_t clicmd[maxcmdlen + 1];
    uint8_t command_user, command_disk, command_con, defdisk, defuser;
    uint8_t buf_ptr;
    uint8_t prompt, promptutens, promptuones, promptdisk;
    char promptend[2];
    uint16_t pcb[2];
    uint16_t parseret;
    uint8_t parseresult[36];
    uint8_t cmdsent;
    uint16_t pdadr, sysdatseg;
    char wrongvers[16];
    uint8_t newline[3];
    uint8_t askabortmsg[13];
    uint8_t subfilename[11];
    char askabortend[10];
    char usercmd[8], printercmd[8], includecmd[8];
    uint8_t usererrmsg[33], usermsg[17], printererrmsg[36], printermsg[20];
    uint8_t includerrmsg[16], includemsg[11];
    char memerr[19], pderr[15], fnameerr[17], loaderr[12], openerr[20], catcherr[2], qfullerr[22];
    char sopenerr[22], argerr[17];
} submit_data;

_Static_assert(offsetof(submit_data, fcb) == SUBMIT_FCB, "fcb");
_Static_assert(offsetof(submit_data, cmdtail) == SUBMIT_CMDTAIL, "cmdtail");
_Static_assert(offsetof(submit_data, stack) == SUBMIT_STACK, "stack");
_Static_assert(offsetof(submit_data, dma) == SUBMIT_DMA, "dma");
_Static_assert(offsetof(submit_data, numargs) == SUBMIT_NUMARGS, "numargs");
_Static_assert(offsetof(submit_data, argtable) == SUBMIT_ARGTABLE, "argtable");
_Static_assert(offsetof(submit_data, deblankbuf) == SUBMIT_DEBLANKBUF, "deblankbuf");
_Static_assert(offsetof(submit_data, cmdbuf) == SUBMIT_CMDBUF, "cmdbuf");
_Static_assert(offsetof(submit_data, blen) == SUBMIT_BLEN, "blen");
_Static_assert(offsetof(submit_data, cmd) == SUBMIT_CMD, "cmd");
_Static_assert(offsetof(submit_data, clicb_net) == SUBMIT_CLICB, "clicb");
_Static_assert(offsetof(submit_data, clicmd) == SUBMIT_CLICMD, "clicmd");
_Static_assert(offsetof(submit_data, command_user) == SUBMIT_COMMAND_USER, "command_user");
_Static_assert(offsetof(submit_data, command_con) == SUBMIT_COMMAND_CON, "command_con");
_Static_assert(offsetof(submit_data, buf_ptr) == SUBMIT_BUF_PTR, "buf_ptr");
_Static_assert(offsetof(submit_data, prompt) == SUBMIT_PROMPT, "prompt");
_Static_assert(offsetof(submit_data, promptend) == SUBMIT_PROMPTEND, "promptend");
_Static_assert(offsetof(submit_data, pcb) == SUBMIT_PCB, "pcb");
_Static_assert(offsetof(submit_data, parseret) == SUBMIT_PARSERET, "parseret");
_Static_assert(offsetof(submit_data, parseresult) == SUBMIT_PARSERESULT, "parseresult");
_Static_assert(offsetof(submit_data, cmdsent) == SUBMIT_CMDSENT, "cmdsent");
_Static_assert(offsetof(submit_data, pdadr) == SUBMIT_PDADR, "pdadr");
_Static_assert(offsetof(submit_data, sysdatseg) == SUBMIT_SYSDATSEG, "sysdatseg");
_Static_assert(offsetof(submit_data, wrongvers) == SUBMIT_WRONGVERS, "wrongvers");
_Static_assert(offsetof(submit_data, newline) == SUBMIT_NEWLINE, "newline");
_Static_assert(offsetof(submit_data, askabortmsg) == SUBMIT_ASKABORTMSG, "askabortmsg");
_Static_assert(offsetof(submit_data, subfilename) == SUBMIT_SUBFILENAME, "subfilename");
_Static_assert(offsetof(submit_data, usercmd) == SUBMIT_USERCMD, "usercmd");
_Static_assert(offsetof(submit_data, printercmd) == SUBMIT_PRINTERCMD, "printercmd");
_Static_assert(offsetof(submit_data, includecmd) == SUBMIT_INCLUDECMD, "includecmd");
_Static_assert(offsetof(submit_data, usererrmsg) == SUBMIT_USERERRMSG, "usererrmsg");
_Static_assert(offsetof(submit_data, usermsg) == SUBMIT_USERMSG, "usermsg");
_Static_assert(offsetof(submit_data, printererrmsg) == SUBMIT_PRINTERERRMSG, "printererrmsg");
_Static_assert(offsetof(submit_data, printermsg) == SUBMIT_PRINTERMSG, "printermsg");
_Static_assert(offsetof(submit_data, includerrmsg) == SUBMIT_INCLUDERRMSG, "includerrmsg");
_Static_assert(offsetof(submit_data, includemsg) == SUBMIT_INCLUDEMSG, "includemsg");
_Static_assert(offsetof(submit_data, memerr) == SUBMIT_MEMERR, "memerr");
_Static_assert(offsetof(submit_data, pderr) == SUBMIT_PDERR, "pderr");
_Static_assert(offsetof(submit_data, fnameerr) == SUBMIT_FNAMEERR, "fnameerr");
_Static_assert(offsetof(submit_data, loaderr) == SUBMIT_LOADERR, "loaderr");
_Static_assert(offsetof(submit_data, openerr) == SUBMIT_OPENERR, "openerr");
_Static_assert(offsetof(submit_data, catcherr) == SUBMIT_CATCHERR, "catcherr");
_Static_assert(offsetof(submit_data, qfullerr) == SUBMIT_QFULLERR, "qfullerr");
_Static_assert(offsetof(submit_data, sopenerr) == SUBMIT_SOPENERR, "sopenerr");
_Static_assert(offsetof(submit_data, argerr) == SUBMIT_ARGERR, "argerr");

#define CC10 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC

const uint16_t submit_image_len = sizeof(submit_data);
const submit_data submit_image = {
    .stack = {CC10, CC10, CC10},
    .buf_ptr = 128, /* force the first read in readchar */
    .prompt = 13,
    .promptutens = '0',
    .promptuones = '0',
    .promptdisk = 'A',
    .promptend = ">$",
    .pcb = {SUBMIT_CLICMD, SUBMIT_PARSERESULT},
    .cmdsent = false,
    .wrongvers = "Requires MP/M-86",
    .newline = {10, 13, '$'},
    .askabortmsg = "\r\n\nTerminate ",
    .askabortend = " (Y/N) ? $",
    .usercmd = "USER    ",
    .printercmd = "PRINTER ",
    .includecmd = "$INCLUDE",
    .usererrmsg = "\r\nInvalid User Number, IGNORED\r\n$",
    .usermsg = "\r\nUser Number = $",
    .printererrmsg = "\r\nInvalid Printer Number, IGNORED\r\n$",
    .printermsg = "\r\nPrinter Number = $",
    .includerrmsg = "\r\nInclude Error$",
    .includemsg = "(submit)\n\r$",
    .memerr = "?Not Enough Memory$",
    .pderr = "?PD Table Full$",
    .fnameerr = "?Illegal Command$",
    .loaderr = "?Load Error$",
    .openerr = "?Can't Find Command$",
    .catcherr = "?$",
    .qfullerr = "?RSP Command Que Full$",
    .sopenerr = "No 'SUB' File Present$",
    .argerr = "Illegal Argument$",
};

typedef struct {
    submit_data *d;
    uint16_t ds;
} submit_t;

static mpm_regs mpm(const submit_t *s, uint8_t cl, uint16_t dx) { return rt_mpm(s->ds, cl, dx); }
static void con_write(const submit_t *s, uint16_t dx) { (void)mpm(s, mpm_conwrite, dx); }
static void crlf(const submit_t *s) { con_write(s, SUBMIT_NEWLINE); }
static void prchar(const submit_t *s, uint8_t dl) { (void)mpm(s, mpm_conout, dl); }
static uint8_t charin(const submit_t *s) { return (uint8_t)mpm(s, mpm_conin, 0).bx; }
static uint8_t getuser(const submit_t *s) { return (uint8_t)mpm(s, mpm_usercode, 0xFF).bx; }
static void setuser(const submit_t *s, uint8_t dl) { (void)mpm(s, mpm_usercode, dl); }
static uint8_t openfile(const submit_t *s, uint16_t dx) { return (uint8_t)mpm(s, mpm_openfile, dx).bx; }
static uint8_t readfile(const submit_t *s, uint16_t dx) { return (uint8_t)mpm(s, mpm_readfile, dx).bx; }

/* parsefilename: SI = fcb, DI = string; returns BX, CX */
static mpm_regs parsefilename(const submit_t *s, uint16_t si, uint16_t di) {
    s->d->pcb[0] = di;
    s->d->pcb[1] = si;
    return mpm(s, mpm_parse, SUBMIT_PCB);
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

static void prnum(const submit_t *s, uint8_t dl) { /* dl = num (0-15) */
    if (dl >= 10) {
        prchar(s, '1');
        dl = (uint8_t)(dl - 10);
    }
    prchar(s, (uint8_t)(dl + '0'));
}

/* readchar: the next character of the SUB file into ds:si; nonzero at
 * the end of the file or on an error. The file is opened again for
 * every record, so it can be deleted while SUBMIT runs. */
static int readchar(const submit_t *s, uint16_t si) {
    submit_data *d = s->d;
    uint8_t bl = d->buf_ptr;
    if (bl >= 128) {
        setuser(s, d->command_user); /* the command file's user */
        d->fcb[0] = d->command_disk; /* and its drive */
        d->fcb[mode2] |= 0x80;       /* open R/O */
        if (openfile(s, SUBMIT_FCB) == 0xFF) return 1;
        if (readfile(s, SUBMIT_FCB) != 0) return 1;
        setuser(s, d->defuser); /* back to what the command file set */
        bl = 0;
    }
    uint8_t al = d->dma[bl];
    if (al == 0x1A) return 1; /* EOF */
    ((uint8_t *)d)[si] = al;
    d->buf_ptr = (uint8_t)(bl + 1);
    return 0;
}

/* readbuffer: Read Console Buffer, from the SUB file; nonzero at EOF */
static int readbuffer(const submit_t *s, uint16_t dx) {
    uint8_t *di = (uint8_t *)s->d + dx;
    di[1] = 0; /* no chars read */
    if (di[0] == 0) return 1; /* zero length buffer */
    uint16_t si = (uint16_t)(dx + 2);
    for (;;) {
        if (di[1] >= di[0]) break; /* full buffer */
        if (readchar(s, si)) return 1;
        uint8_t c = ((uint8_t *)s->d)[si];
        if (c == lf || c == cr) {
            if (di[1] == 0) continue; /* skip leading cr's and lf's */
            break;
        }
        di[1]++;
        si++;
    }
    ((uint8_t *)s->d)[si] = 0;
    return 0;
}

/* deblank the command tail into deblankbuf: one blank between
 * arguments, ending with a blank and a zero */
static void deblank(submit_data *d) {
    const uint8_t *si = d->cmdtail + 1;
    uint8_t *di = d->deblankbuf;
    while (*si == ' ' || *si == 9) si++;
    for (;;) {
        uint8_t al = *si++;
        if (al == ' ' || al == 9) { /* skipwhite */
            while (*si == ' ' || *si == 9) si++;
            *di++ = ' ';
            continue;
        }
        if (al == 0) { /* putlastblank */
            *di++ = ' ';
            *di = al;
            return;
        }
        *di++ = al;
    }
}

/* argtable[n] = the offset of argument n in deblankbuf (the 0th is
 * '<filename>.sub'); each is a word store, as in the .A86 */
static void fill_argtable(submit_data *d) {
    uint16_t si = 0, di = 0;
    uint8_t *b = (uint8_t *)d;
    while (d->deblankbuf[si] != 0) {
        wr16(b + SUBMIT_ARGTABLE + di, si);
        di++;
        do si++; while (d->deblankbuf[si] != ' '); /* skiparg */
        si++;
    }
    d->numargs = di;
}

/* copy cmd to clicmd, putting the arguments in for $n, then '$' */
static void expand_command(submit_data *d) {
    const uint8_t *b = (const uint8_t *)d;
    uint16_t si = SUBMIT_CMD;
    uint16_t di = 0;
    for (;;) {
        uint8_t al = b[si++];
        if (al == '$' && b[si] >= '0' && b[si] <= '9') {
            uint16_t ax = (uint16_t)(b[si++] - '0');
            if (b[si] >= '0' && b[si] <= '9') ax = (uint16_t)(ax * 10 + (b[si++] - '0'));
            if (ax >= d->numargs) continue; /* no such argument */
            uint16_t bx = d->argtable[ax];
            for (; d->deblankbuf[bx] != ' '; bx++) {
                al = d->deblankbuf[bx];
                d->clicmd[di++] = al; /* copyit */
                if (di >= maxcmdlen || al == 0) goto donecopy;
            }
            continue;
        }
        d->clicmd[di++] = al; /* copyit */
        if (di >= maxcmdlen || al == 0) break;
    }
donecopy:
    d->clicmd[di] = '$';
}

/* clierror: the CLI call failed; returns 1 to go on with the next
 * command (a null command), 0 when SUBMIT stops */
static int clierror(const submit_t *s, uint16_t cx) {
    if (cx == e_nullcmd_) {
        s->d->cmdsent = false;
        return 1;
    }
    uint16_t dx;
    if (cx == e_no_memory) dx = SUBMIT_MEMERR;
    else if (cx == e_no_pd) dx = SUBMIT_PDERR;
    else if (cx == e_badfname_ || cx == e_badftype_) dx = SUBMIT_FNAMEERR;
    else if (cx == e_bad_load_ || cx == e_bad_read_) dx = SUBMIT_LOADERR;
    else if (cx == e_bad_open_) dx = SUBMIT_OPENERR;
    else if (cx == e_q_full_) dx = SUBMIT_QFULLERR;
    else dx = SUBMIT_CATCHERR;
    con_write(s, dx); /* showerr */
    return 0;
}

static int name_is(const uint8_t *p, const char *name) {
    for (int i = 0; i < 8; i++) {
        if (p[i] != (uint8_t)name[i]) return 0;
    }
    return 1;
}

/* one command from the SUB file (topofcmdloop .. getnxtcmd); returns 0
 * when SUBMIT stops */
static int do_command(const submit_t *s) {
    submit_data *d = s->d;
    uint8_t *sys = far_ptr(rt_phys(), d->sysdatseg, 0);

    /* was a control C typed during the last command? */
    uint8_t *flag = sys + d->pdadr + p_flag;
    if (rd16(flag) & pf_childabort) {
        wr16(flag, (uint16_t)(rd16(flag) & ~pf_childabort));
        for (int i = 0; i < 11; i++) d->subfilename[i] = d->fcb[1 + i];
        con_write(s, SUBMIT_ASKABORTMSG);
        if ((charin(s) & 0x5F) == 'Y') { /* stop, with pf_ctlc on */
            wr16(flag, (uint16_t)(rd16(flag) | pf_ctlc));
            return 0;
        }
    }

    /* print CR,LF if we just sent a command */
    if (d->cmdsent != false) {
        d->cmdsent = false;
        crlf(s);
    }

    /* the prompt, from the current default disk and user (USER may have
     * changed it) */
    d->defdisk = (uint8_t)mpm(s, mpm_getdefdisk, 0).bx;
    uint8_t al = getuser(s);
    d->defuser = al;
    d->promptutens = 13;
    d->promptuones = '0';
    if (al >= 10) {
        d->promptutens = '1';
        al = (uint8_t)(al - 10);
    }
    d->promptuones = (uint8_t)(d->promptuones + al);
    d->promptdisk = (uint8_t)('A' + d->defdisk);
    con_write(s, d->promptutens == 13 ? SUBMIT_PROMPTUTENS : SUBMIT_PROMPT);

    /* the command, with its arguments put in, echoed */
    expand_command(d);
    con_write(s, d->clicmd[0] == '$' ? SUBMIT_CLICMD + 1 : SUBMIT_CLICMD);
    crlf(s);

    /* make sure not a null command */
    if (d->blen == 0 || d->cmd[0] == ';') return 1;

    int builtin = 0;
    if (d->blen == 2) {
        if (d->cmd[1] == ':') { /* 'X:' changes the default disk to X */
            uint8_t dl = (uint8_t)((d->cmd[0] & 0x5F) - 'A');
            if (dl > 15) return 0;
            (void)mpm(s, mpm_diskselect, dl);
            return 1;
        }
        builtin = 1; /* straight to try_builtin, on the last parse */
    } else {
        mpm_regs r = parsefilename(s, SUBMIT_PARSERESULT, SUBMIT_CLICMD);
        if (r.cx != 0) {
            /* repne scasb for the blank (CL = 126), '$' after it; the
             * scan leaves its count in CX for clierror */
            uint16_t cx = (uint16_t)((r.cx & 0xFF00) | 126);
            uint16_t di = 0;
            while (cx != 0) {
                cx--;
                if (d->clicmd[di++] == ' ') break;
            }
            d->clicmd[di] = '$';
            (void)clierror(s, cx);
            return 0;
        }
        d->parseret = r.bx;
        builtin = d->parseresult[0] == 0;
    }

    if (builtin) {
        const uint8_t *name = d->parseresult + 1;
        if (name_is(name, d->usercmd)) {
            uint16_t di = d->parseret;
            if (di != 0 && parsefilename(s, SUBMIT_PARSERESULT, di).cx == 0) {
                uint8_t bl = a_to_b(d->parseresult[1], d->parseresult[2]);
                if (bl > 15) con_write(s, SUBMIT_USERERRMSG);
                else setuser(s, bl);
            }
            con_write(s, SUBMIT_USERMSG); /* pruser */
            prnum(s, getuser(s));
            crlf(s);
            return 1;
        }
        if (name_is(name, d->printercmd)) {
            uint16_t di = d->parseret;
            if (di != 0 && parsefilename(s, SUBMIT_PARSERESULT, di).cx == 0) {
                uint8_t bl = a_to_b(d->parseresult[1], d->parseresult[2]);
                if (bl == 0xFF || mpm(s, mpm_setdeflst, bl).cx != 0) con_write(s, SUBMIT_PRINTERERRMSG);
            }
            con_write(s, SUBMIT_PRINTERMSG); /* prprinter */
            prnum(s, (uint8_t)mpm(s, mpm_getdeflst, 0).bx);
            crlf(s);
            return 1;
        }
        if (name_is(name, d->includecmd) && d->parseret != 0) {
            if (parsefilename(s, SUBMIT_PARSERESULT, d->parseret).cx != 0) {
                con_write(s, SUBMIT_INCLUDERRMSG);
                return 0;
            }
            /* overwrite $INCLUDE with this program's name, for a
             * recursive submit */
            const uint8_t *pname = sys + d->pdadr + p_name;
            for (int i = 0; i < 8; i++) d->clicmd[i] = pname[i];
            con_write(s, SUBMIT_INCLUDEMSG);
        }
    }

    /* clicall: initialize the Cli Control Block, make the call */
    d->clicb_net = 0;
    d->cmdsent = true;
    mpm_regs r = mpm(s, mpm_clicmd, SUBMIT_CLICB);
    if (r.bx != 0) return clierror(s, r.cx);
    return 1;
}

void submit_main(uint16_t ds) {
    submit_data *d = RSP_DATA(submit_data, ds);
    rsp_install_transient(ds, &(rsp_image){"SUBMIT  ", (const uint8_t *)&submit_image, submit_image_len});
    submit_t st = {d, ds};
    const submit_t *s = &st;

    if ((mpm(s, mpm_version, 0).bx >> 8) != 0x11) {
        con_write(s, SUBMIT_WRONGVERS);
        (void)mpm(s, 0, 0);
    }

    /* set priority better than parent */
    mpm_regs r = rt_int224(ds, ds, mpm_getpdadr, 0);
    d->pdadr = r.bx;      /* our PD */
    d->sysdatseg = r.es;  /* and the system data area */
    uint8_t *sys = far_ptr(rt_phys(), d->sysdatseg, 0);
    uint16_t parent = rd16(sys + d->pdadr + p_parent);
    uint16_t dx = 197; /* no parent: better than TMP */
    if (parent != 0) {
        dx = (uint16_t)(rd16(sys + parent + p_prior) - 1); /* one better */
        if (dx < 64) dx = 64; /* don't compete w/system */
    }
    (void)mpm(s, mpm_setprior, dx);

    /* the DMA at dma, for the command tail and argument expansion */
    (void)mpm(s, mpm_setdma, SUBMIT_DMA);

    /* open <file>.SUB read only */
    d->fcb[ftype] = 'S';
    d->fcb[ftype + 1] = 'U';
    d->fcb[ftype + 2] = 'B';
    d->fcb[fcb_cr] = 0;
    d->fcb[mode2] |= 0x80;
    if (openfile(s, SUBMIT_FCB) == 0xFF) {
        con_write(s, SUBMIT_SOPENERR);
        (void)mpm(s, 0, 0); /* submitexit */
        return;
    }
    /* save the disk and user the command file is on, and our console */
    uint8_t al = d->fcb[0];
    if (al == 0) al = (uint8_t)(mpm(s, mpm_getdefdisk, 0).bx + 1);
    d->command_disk = al;
    d->command_user = getuser(s);
    d->command_con = (uint8_t)mpm(s, mpm_getdefcon, 0).bx;

    deblank(d);
    fill_argtable(d);
    crlf(s);

    /* the first command; then one at a time until EOF */
    d->cmdbuf = maxcmdlen;
    if (readbuffer(s, SUBMIT_CMDBUF) == 0) {
        for (;;) {
            if (!do_command(s)) break;
            d->cmdbuf = maxcmdlen; /* getnxtcmd */
            if (readbuffer(s, SUBMIT_CMDBUF) != 0) break;
            (void)mpm(s, mpm_conattach, 0);
        }
    }
    (void)mpm(s, 0, 0); /* submitexit */
}
