/*
 * submit.c -- C port of src/rsp/SUBMIT.A86's command processing: the
 * command-tail `deblank` and `fill_argtable` steps (unlabeled code in
 * `submit:` after the SUB file is opened), `$n` argument expansion
 * (`nextchar:` .. `donecopy:`, as `expand_command`), `readbuffer`/
 * `readchar`, and one pass of the command loop (`topofcmdloop:` to its
 * `getnxtcmd`/`submitexit` exits, as `topofcmdloop_once`), including the
 * USER/PRINTER/$INCLUDE builtins and the CLI call.
 *
 * Not ported: `submit:`'s one-time setup (version check, PD address,
 * priority, DMA, opening the SUB file) -- system calls whose results
 * the tests preset (pdadr, sysdatseg, command_disk/user, the FCB).
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory, `sysdat` the SYSDAT segment and `ds` SUBMIT's
 * data segment, its fields at their real SUBMIT.A86 DSEG offsets
 * (SUBMIT_* below; its constants -- usercmd etc. -- are in DS too). The
 * PD is reached as the asm reaches it, through its own `sysdatseg`:pdadr.
 *
 * System calls are `int mpmint`. f_parsefilename (parse.c) and the CLI
 * call (command.c's clicmd_entry) are real calls, entered as SUPIF's
 * user_entry enters them (defs.h's `user_enter`: u_wrkseg = SUBMIT's
 * DS). The calls whose results SUBMIT uses -- conin (the ctl-C
 * question), getdefdisk, get user code, and the SUB file's open/read in
 * readchar -- are boundaries whose results are parameters; console
 * output, set user/disk/list device and attach have no memory effect.
 */
#include <stdint.h>

#include "defs.h"

/* src/rsp/SUBMIT.A86's DSEG fields */
#define SUBMIT_FCB         0x005C /* byte[24h]: the SUB file */
#define SUBMIT_CMDTAIL     0x0080 /* byte[128]: length, then the tail */
#define SUBMIT_DMA         0x013C /* byte[128]: the SUB file's record */
#define SUBMIT_NUMARGS     0x01BC /* word */
#define SUBMIT_ARGTABLE    0x01BE /* byte[64]: argument offsets in deblankbuf */
#define SUBMIT_DEBLANKBUF  0x01FE /* byte[maxcmdlen+1] */
#define SUBMIT_CMDBUF      0x027F /* byte: readbuffer's max length */
#define SUBMIT_BLEN        0x0280 /* byte: its count */
#define SUBMIT_CMD         0x0281 /* byte[maxcmdlen+1]: the line read */
#define SUBMIT_CLICB       0x0302 /* the CLICB: clicb_net, then clicmd */
#define SUBMIT_CLICMD      0x0303 /* byte[maxcmdlen+1]: the expanded command */
#define SUBMIT_COMMAND_USER 0x0384 /* byte */
#define SUBMIT_COMMAND_DISK 0x0385 /* byte */
#define SUBMIT_DEFDISK     0x0387 /* byte */
#define SUBMIT_DEFUSER     0x0388 /* byte */
#define SUBMIT_BUF_PTR     0x0389 /* byte: next char in dma */
#define SUBMIT_PROMPTUTENS 0x038B /* byte */
#define SUBMIT_PROMPTUONES 0x038C /* byte */
#define SUBMIT_PROMPTDISK  0x038D /* byte */
#define SUBMIT_PCB         0x0390 /* word[2]: string, fcb */
#define SUBMIT_PARSERET    0x0394 /* word */
#define SUBMIT_PARSERESULT 0x0396 /* byte[36] */
#define SUBMIT_CMDSENT     0x03BA /* byte */
#define SUBMIT_PDADR       0x03BB /* word: our PD (a SYSDAT offset) */
#define SUBMIT_SYSDATSEG   0x03BD /* word */
#define SUBMIT_SUBFILENAME 0x03DF /* byte[11] */
#define SUBMIT_USERCMD     0x03F4 /* byte[8]: 'USER    ' */
#define SUBMIT_PRINTERCMD  0x03FC /* byte[8]: 'PRINTER ' */
#define SUBMIT_INCLUDECMD  0x0404 /* byte[8]: '$INCLUDE' */

#define SUBMIT_MAXCMDLEN 128
#define fcb_mode2 6 /* SUBMIT.A86: 2nd open mode byte */

/* topofcmdloop_once's exits */
#define SUBMIT_GETNXTCMD  0 /* jmp getnxtcmd: read the next line (readbuffer) */
#define SUBMIT_SUBMITEXIT 1 /* jmp submitexit: SUBMIT terminates */

typedef struct {
    uint16_t bx, cx;
} sub_bxcx;

extern sub_bxcx parse_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern sub_bxcx clicmd_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t user_retf_offset, uint16_t cs);

/* parsefilename -- parse the string at di into the FCB at si, through
 * SUBMIT's pcb */
static sub_bxcx parsefilename(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint16_t si, uint16_t di) {
    uint8_t *d = far_ptr(phys, ds, 0);
    seg_w16(d, SUBMIT_PCB + pcb_flnmptr, di);
    seg_w16(d, SUBMIT_PCB + pcb_fcbptr, si);
    user_enter(phys, sysdat, ds);
    return parse_entry(phys, sysdat, SUBMIT_PCB);
}

static int same8(const uint8_t *d, uint16_t a, uint16_t b) {
    for (unsigned i = 0; i < 8; i++) {
        if (seg_r8(d, (uint16_t)(a + i)) != seg_r8(d, (uint16_t)(b + i))) {
            return 0;
        }
    }
    return 1;
}

/*
 * deblank -- copy cmdtail's text to deblankbuf, dropping leading blanks
 * and tabs, collapsing each run of them to one space, and ending with
 * " \0"
 */
void deblank(uint8_t *phys, uint16_t ds) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint16_t si = SUBMIT_CMDTAIL + 1;
    uint16_t di = SUBMIT_DEBLANKBUF;
    while (seg_r8(d, si) == ' ' || seg_r8(d, si) == 9) {
        si++;
    }
    for (;;) {
        uint8_t al = seg_r8(d, si++);
        if (al == ' ' || al == 9) {
            while (seg_r8(d, si) == ' ' || seg_r8(d, si) == 9) {
                si++;
            }
            seg_w8(d, di++, ' ');
            continue;
        }
        if (al == 0) {
            seg_w8(d, di++, ' ');
            seg_w8(d, di, 0);
            return;
        }
        seg_w8(d, di++, al);
    }
}

/*
 * fill_argtable -- record where each word of deblankbuf starts. The asm
 * stores each offset as a word (`mov [bx+di],si`) one byte apart, so
 * each store's high byte (0) is overwritten by the next one, except the
 * last's.
 */
void fill_argtable(uint8_t *phys, uint16_t ds) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint16_t si = 0, di = 0;
    while (seg_r8(d, (uint16_t)(SUBMIT_DEBLANKBUF + si)) != 0) {
        seg_w16(d, (uint16_t)(SUBMIT_ARGTABLE + di), si);
        di++;
        do { /* skiparg */
            si++;
        } while (seg_r8(d, (uint16_t)(SUBMIT_DEBLANKBUF + si)) != ' ');
        si++;
    }
    seg_w16(d, SUBMIT_NUMARGS, di);
}

/* copyit -- store al at clicmd[*di]; at the end of the buffer or the
 * command, `donecopy` '$'-terminates it and returns 1 (the asm leaves
 * through echocommand, dropping copyit's return address) */
static int copyit(uint8_t *d, uint16_t *di, uint8_t al) {
    seg_w8(d, (uint16_t)(SUBMIT_CLICMD + *di), al);
    (*di)++;
    if (*di >= SUBMIT_MAXCMDLEN || al == 0) {
        seg_w8(d, (uint16_t)(SUBMIT_CLICMD + *di), '$');
        return 1;
    }
    return 0;
}

/*
 * expand_command -- copy cmd to clicmd, replacing each `$n` (one or two
 * digits) with argument n from deblankbuf; `$` not followed by a digit
 * is copied, an argument past numargs is dropped
 */
void expand_command(uint8_t *phys, uint16_t ds) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint16_t si = SUBMIT_CMD;
    uint16_t di = 0;
    for (;;) {
        uint8_t al = seg_r8(d, si++);
        uint8_t c = seg_r8(d, si);
        if (al != '$' || c < '0' || c > '9') {
            if (copyit(d, &di, al)) {
                return;
            }
            continue;
        }
        /* realarg */
        uint16_t n = (uint16_t)(seg_r8(d, si++) - '0');
        c = seg_r8(d, si);
        if (c >= '0' && c <= '9') {
            n = (uint16_t)(n * 10 + (uint8_t)(seg_r8(d, si++) - '0'));
        }
        if (n >= seg_r16(d, SUBMIT_NUMARGS)) {
            continue;
        }
        /* oknum/copymore */
        for (uint16_t bx = seg_r8(d, (uint16_t)(SUBMIT_ARGTABLE + n));
             seg_r8(d, (uint16_t)(SUBMIT_DEBLANKBUF + bx)) != ' '; bx++) {
            if (copyit(d, &di, seg_r8(d, (uint16_t)(SUBMIT_DEBLANKBUF + bx)))) {
                return;
            }
        }
    }
}

/*
 * readchar -- read the SUB file's next character into DS:si, re-opening
 * the file and reading its next record when dma is used up
 *	input:	openfile_al/readfile_al -- the f_open/f_readseq results
 *		for a refill
 *	output:	returns 0, or 0FFFFh on an error or end of file
 */
uint16_t readchar(uint8_t *phys, uint16_t ds, uint16_t si, uint8_t openfile_al, uint8_t readfile_al) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint16_t bx = seg_r8(d, SUBMIT_BUF_PTR);
    if (bx >= 128) {
        /* set the command file's user; re-open it (so it can be
         * deleted between reads), read-only; read; set the user back */
        seg_w8(d, SUBMIT_FCB, seg_r8(d, SUBMIT_COMMAND_DISK));
        seg_w8(d, SUBMIT_FCB + fcb_mode2, (uint8_t)(seg_r8(d, SUBMIT_FCB + fcb_mode2) | 0x80));
        if (openfile_al == 0xFF || readfile_al != 0) {
            return 0xFFFF; /* readerr */
        }
        bx = 0;
    }
    uint8_t al = seg_r8(d, (uint16_t)(SUBMIT_DMA + bx));
    if (al == 0x1A) {
        return 0xFFFF; /* eof */
    }
    seg_w8(d, si, al);
    seg_w8(d, SUBMIT_BUF_PTR, (uint8_t)(bx + 1));
    return 0;
}

/*
 * readbuffer -- read the next non-blank line of the SUB file into the
 * buffer at dx (max length, count, text), like a console read
 *	output:	returns 0, or 0FFFFh at end of file
 */
uint16_t readbuffer(uint8_t *phys, uint16_t ds, uint16_t dx, uint8_t openfile_al, uint8_t readfile_al) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint16_t di = dx;
    seg_w8(d, (uint16_t)(di + 1), 0);
    if (seg_r8(d, di) == 0) {
        return 0xFFFF;
    }
    uint16_t si = (uint16_t)(di + 2);
    for (;;) {
        while (seg_r8(d, (uint16_t)(di + 1)) < seg_r8(d, di)) {
            uint16_t err = readchar(phys, ds, si, openfile_al, readfile_al);
            if (err != 0) {
                return err;
            }
            uint8_t c = seg_r8(d, si);
            if (c == 0x0A || c == 0x0D) {
                break;
            }
            seg_w8(d, (uint16_t)(di + 1), (uint8_t)(seg_r8(d, (uint16_t)(di + 1)) + 1));
            si++;
        }
        /* done_ok: skip leading CRs and LFs */
        if (seg_r8(d, (uint16_t)(di + 1)) != 0) {
            seg_w8(d, si, 0);
            return 0;
        }
    }
}

/* clierror -- the CLI call failed: a null command goes on to the next
 * one; every other error prints a message and ends SUBMIT */
static int clierror(uint8_t *d, uint16_t cx) {
    if (cx == e_nullcmd) {
        seg_w8(d, SUBMIT_CMDSENT, 0);
        return SUBMIT_GETNXTCMD;
    }
    return SUBMIT_SUBMITEXIT;
}

/* clicall -- send clicmd to the CLI */
static int clicall(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint16_t user_retf_offset, uint16_t sup_cs) {
    uint8_t *d = far_ptr(phys, ds, 0);
    seg_w8(d, SUBMIT_CLICB + cmdcb_net, 0);
    seg_w8(d, SUBMIT_CMDSENT, 0xFF); /* true */
    user_enter(phys, sysdat, ds);
    sub_bxcx r = clicmd_entry(phys, sysdat, SUBMIT_CLICB, user_retf_offset, sup_cs);
    if (r.bx != 0) {
        return clierror(d, r.cx);
    }
    return SUBMIT_GETNXTCMD;
}

/*
 * try_builtin -- USER n and PRINTER n parse their argument (what they
 * do with it is boundary calls and messages); $INCLUDE file sends
 * "<this program's name> file" to the CLI, a nested submit; anything
 * else goes to the CLI as is. Uses parseresult/parseret as they stand
 * (a 2-character command reaches here without a parse).
 */
static int try_builtin(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint16_t user_retf_offset, uint16_t sup_cs) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint16_t parseret = seg_r16(d, SUBMIT_PARSERET);
    if (same8(d, SUBMIT_PARSERESULT + 1, SUBMIT_USERCMD) || same8(d, SUBMIT_PARSERESULT + 1, SUBMIT_PRINTERCMD)) {
        if (parseret != 0) {
            (void)parsefilename(phys, sysdat, ds, SUBMIT_PARSERESULT, parseret);
        }
        return SUBMIT_GETNXTCMD; /* pruser/prprinter */
    }
    if (same8(d, SUBMIT_PARSERESULT + 1, SUBMIT_INCLUDECMD) && parseret != 0) {
        if (parsefilename(phys, sysdat, ds, SUBMIT_PARSERESULT, parseret).cx != 0) {
            return SUBMIT_SUBMITEXIT; /* includerr */
        }
        /* overwrite "$INCLUDE" with this program's name */
        const uint8_t *pd = far_ptr(phys, seg_r16(d, SUBMIT_SYSDATSEG), seg_r16(d, SUBMIT_PDADR));
        for (unsigned i = 0; i < 8; i++) {
            seg_w8(d, (uint16_t)(SUBMIT_CLICMD + i), seg_r8(pd, (uint16_t)(p_name + i)));
        }
    }
    return clicall(phys, sysdat, ds, user_retf_offset, sup_cs);
}

/*
 * topofcmdloop_once -- one pass of SUBMIT's command loop: handle a ctl-C
 * from the last command (ask whether to stop), build the prompt, expand
 * the line read into clicmd, and run it -- a disk change, a builtin, or
 * the CLI
 *	input:	charin_al -- the answer to the ctl-C question (conin);
 *		defdisk_al/defuser_al -- getdefdisk's and get user code's
 *		results
 *	output:	SUBMIT_GETNXTCMD or SUBMIT_SUBMITEXIT
 */
int topofcmdloop_once(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint8_t charin_al, uint8_t defdisk_al,
                      uint8_t defuser_al, uint16_t user_retf_offset, uint16_t sup_cs) {
    uint8_t *d = far_ptr(phys, ds, 0);
    uint8_t *sd = far_ptr(phys, seg_r16(d, SUBMIT_SYSDATSEG), 0);
    uint16_t pf = (uint16_t)(seg_r16(d, SUBMIT_PDADR) + p_flag);
    if (seg_r16(sd, pf) & pf_childabort) {
        seg_w16(sd, pf, (uint16_t)(seg_r16(sd, pf) & (uint16_t)~pf_childabort));
        for (unsigned i = 0; i < 11; i++) {
            seg_w8(d, (uint16_t)(SUBMIT_SUBFILENAME + i), seg_r8(d, (uint16_t)(SUBMIT_FCB + 1 + i)));
        }
        if ((charin_al & 0x5F) == 'Y') {
            seg_w16(sd, pf, (uint16_t)(seg_r16(sd, pf) | pf_ctlc));
            return SUBMIT_SUBMITEXIT;
        }
    }

    /* gonext */
    if (seg_r8(d, SUBMIT_CMDSENT) != 0) {
        seg_w8(d, SUBMIT_CMDSENT, 0); /* then crlf */
    }
    seg_w8(d, SUBMIT_DEFDISK, defdisk_al);
    seg_w8(d, SUBMIT_DEFUSER, defuser_al);
    uint8_t al = defuser_al;
    seg_w8(d, SUBMIT_PROMPTUTENS, 13);
    seg_w8(d, SUBMIT_PROMPTUONES, '0');
    if (al >= 10) {
        seg_w8(d, SUBMIT_PROMPTUTENS, '1');
        al = (uint8_t)(al - 10);
    }
    seg_w8(d, SUBMIT_PROMPTUONES, (uint8_t)(seg_r8(d, SUBMIT_PROMPTUONES) + al));
    seg_w8(d, SUBMIT_PROMPTDISK, (uint8_t)('A' + defdisk_al));

    expand_command(phys, ds); /* then echo it, and crlf */

    uint8_t blen = seg_r8(d, SUBMIT_BLEN);
    if (blen == 0 || seg_r8(d, SUBMIT_CMD) == ';') {
        return SUBMIT_GETNXTCMD;
    }
    if (blen == 2) {
        if (seg_r8(d, SUBMIT_CMD + 1) != ':') {
            return try_builtin(phys, sysdat, ds, user_retf_offset, sup_cs);
        }
        uint8_t dl = (uint8_t)((seg_r8(d, SUBMIT_CMD) & 0x5F) - 'A');
        return dl > 15 ? SUBMIT_SUBMITEXIT : SUBMIT_GETNXTCMD; /* subex / select the disk */
    }

    /* notdrive */
    sub_bxcx pr = parsefilename(phys, sysdat, ds, SUBMIT_PARSERESULT, SUBMIT_CLICMD);
    if (pr.cx != 0) {
        /* '$' after clicmd's first word (repne scasb: di ends one past
         * the space, or 126 bytes in) */
        uint16_t di = SUBMIT_CLICMD;
        uint16_t cx = 126;
        while (cx != 0) {
            cx--;
            if (seg_r8(d, di++) == ' ') {
                break;
            }
        }
        seg_w8(d, di, '$');
        return clierror(d, pr.cx);
    }
    /* goodparse */
    seg_w16(d, SUBMIT_PARSERET, pr.bx);
    if (seg_r8(d, SUBMIT_PARSERESULT) == 0) {
        return try_builtin(phys, sysdat, ds, user_retf_offset, sup_cs);
    }
    return clicall(phys, sysdat, ds, user_retf_offset, sup_cs);
}
