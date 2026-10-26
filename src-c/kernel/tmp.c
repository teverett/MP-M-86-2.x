/*
 * tmp.c -- C port of src/rsp/TMP.A86's main loop body (`nextcommand:`
 * through `clicall:`/`goodparse:`/`try_builtin:`/`not_builtin:`/
 * `clierror:`), the MP/M "Terminal Message Processor" shell.
 *
 * TMP is a Resident System Process: `tmp:` does its one-time setup
 * (default console/disk/user, the version banner -- all console/disk
 * calls with no memory effect here) and falls into `nextcommand:`,
 * which loops forever. This port runs exactly one pass of that loop
 * (`nextcommand_once`; tests/src-c/test_tmp.py stops the real loop when
 * it gets back to `nextcommand`), the same convention as echo.c and
 * clock.c. The command the pass reads is its input: `conread` is a
 * boundary, so read_blen/clicb_cmd are preset in TMP's data segment.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory, `sysdat` the SYSDAT segment and `ds` TMP's own
 * data segment (one per console), where its fields live at their real
 * TMP.A86 DSEG offsets (TMP_* below). The read buffer and the CLICB
 * share memory, as in the asm: clicb_net is read_blen.
 *
 * Every system call is `int mpmint` (via `mpm:`). Two of them are real
 * calls here: f_parsefilename (parse.c's parse_entry) and the CLI call
 * (command.c's clicmd_entry), entered as SUPIF's `user_entry` enters a
 * function (defs.h's `user_enter`: u_wrkseg = TMP's DS). The rest --
 * console output/input, attach/detach, disk/user/list device get/set,
 * free drive -- are boundaries with no memory effect: they print, or
 * set/report state this pass doesn't use (`getuser`'s BL only feeds a
 * print).
 */
#include <stdint.h>

#include "defs.h"

/* src/rsp/TMP.A86's DSEG fields */
#define TMP_READ_BLEN 0x01DF /* byte: read_blen = clicb_net */
#define TMP_CLICB     0x01DF /* the CLICB: clicb_net, then clicb_cmd */
#define TMP_CLICB_CMD 0x01E0 /* byte[maxcmdlen+1] */
#define TMP_CMDSENT   0x0261 /* byte */
#define TMP_PARSERET  0x0262 /* word */
#define TMP_PCB       0x0264 /* word[2]: string, fcb */
#define TMP_FCB       0x0268 /* byte[32] */
#define TMP_SAVEBUF   0x0288 /* byte[128] */

#define mpm_clicmd 150
#define mpm_parse  152

typedef struct {
    uint16_t bx, cx;
} tmp_bxcx;

extern tmp_bxcx parse_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern tmp_bxcx clicmd_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t user_retf_offset, uint16_t cs);

/* the CSEG constants usercmd/printercmd */
static const uint8_t usercmd[8] = {'U', 'S', 'E', 'R', ' ', ' ', ' ', ' '};
static const uint8_t printercmd[8] = {'P', 'R', 'I', 'N', 'T', 'E', 'R', ' '};

/* parsefilename -- parse the string at di into the FCB at si, through
 * TMP's pcb */
static tmp_bxcx parsefilename(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint16_t si, uint16_t di) {
    uint8_t *d = far_ptr(phys, ds, 0);
    seg_w16(d, TMP_PCB + pcb_flnmptr, di);
    seg_w16(d, TMP_PCB + pcb_fcbptr, si);
    user_enter(phys, sysdat, ds);
    return parse_entry(phys, sysdat, TMP_PCB);
}

static int same8(const uint8_t *d, uint16_t off, const uint8_t *s) {
    for (unsigned i = 0; i < 8; i++) {
        if (seg_r8(d, (uint16_t)(off + i)) != s[i]) {
            return 0;
        }
    }
    return 1;
}

/* a builtin's argument (USER n / PRINTER n): parse it, if there is one.
 * What it then does -- a_to_b, setuser/setlist, the messages -- is all
 * boundary calls and prints. */
static void builtin_arg(uint8_t *phys, uint16_t sysdat, uint16_t ds) {
    uint16_t parseret = seg_r16(far_ptr(phys, ds, 0), TMP_PARSERET);
    if (parseret != 0) {
        (void)parsefilename(phys, sysdat, ds, TMP_FCB, (uint16_t)(parseret + 1));
    }
}

/* clierror -- the CLI call failed: a null command clears cmdsent; every
 * other error just picks a message to print */
static void clierror(uint8_t *phys, uint16_t ds, uint16_t cx) {
    if (cx == e_nullcmd) {
        seg_w8(far_ptr(phys, ds, 0), TMP_CMDSENT, 0);
    }
}

/* nextcommand_once -- one pass of the TMP's command loop */
void nextcommand_once(uint8_t *phys, uint16_t sysdat, uint16_t ds, uint16_t user_retf_offset, uint16_t sup_cs) {
    uint8_t *d = far_ptr(phys, ds, 0);
    /* freedrive, attach */
    if (seg_r8(d, TMP_CMDSENT) != 0) {
        seg_w8(d, TMP_CMDSENT, 0); /* then crlf */
    }
    /* the prompt; conread (the preset command); echo lf */
    uint8_t blen = seg_r8(d, TMP_READ_BLEN);
    if (blen == 0 || seg_r8(d, TMP_CLICB_CMD) == ';') {
        return;
    }
    if (blen == 2 && seg_r8(d, TMP_CLICB_CMD + 1) == ':') {
        return; /* 'X:' -- setdisk, if in range */
    }

    /* clicall: null-terminate, copy to savebuf (64 words), parse the front */
    seg_w8(d, (uint16_t)(TMP_CLICB_CMD + blen), 0);
    for (uint16_t i = 0; i < 128; i++) {
        seg_w8(d, (uint16_t)(TMP_SAVEBUF + i), seg_r8(d, (uint16_t)(TMP_CLICB_CMD + i)));
    }
    tmp_bxcx pr = parsefilename(phys, sysdat, ds, TMP_FCB, TMP_SAVEBUF);
    if (pr.cx != 0) {
        seg_w8(d, (uint16_t)(TMP_SAVEBUF + seg_r8(d, TMP_READ_BLEN)), '$');
        clierror(phys, ds, pr.cx);
        return;
    }

    /* goodparse */
    seg_w16(d, TMP_PARSERET, pr.bx);
    uint16_t bx = pr.bx != 0 ? pr.bx : (uint16_t)(TMP_SAVEBUF + seg_r8(d, TMP_READ_BLEN));
    seg_w8(d, bx, '$'); /* haveatail */
    if (seg_r8(d, TMP_FCB) == 0) {
        /* try_builtin */
        if (same8(d, TMP_FCB + 1, usercmd) || same8(d, TMP_FCB + 1, printercmd)) {
            builtin_arg(phys, sysdat, ds);
            return;
        }
    }

    /* not_builtin */
    seg_w8(d, TMP_CLICB + cmdcb_net, 0);
    seg_w8(d, TMP_CMDSENT, 0xFF); /* true */
    user_enter(phys, sysdat, ds);
    tmp_bxcx r = clicmd_entry(phys, sysdat, TMP_CLICB, user_retf_offset, sup_cs);
    if (r.bx != 0) {
        clierror(phys, ds, r.cx);
    }
}
