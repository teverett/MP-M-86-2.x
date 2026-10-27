/*
 * type.c -- C version of src-assem/utils/TYPE.PLM (a transient,
 * TYPE.CMD): types a file on the console up to its ^Z, asking for the
 * password if the file has one, and with a [P] or [Pnn] option pausing
 * every 24 (nn) lines for a key.
 *
 * Its data segment, below, is TYPE.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values, the string constants, and the stack; type_main installs the
 * image from 100h on. PL/M's semantics are kept as in dir.c: bytes
 * wrap, a relation is 0FFh or 0, IF tests bit 0 (TYPE's own `true` is
 * 0FFh), AND evaluates both sides (so the read loop reads a record even
 * once eod is set), and the main program's variables are the PL/M
 * globals; the procedures' own variables are on PL/M's stack, here C
 * locals.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_len0  0x53
#define bp_fcb   0x5C
#define bp_fcb16 0x6C
#define bp_tbuff 0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t parse_fn[2]; /* buff$adr, fcb$adr */
    uint8_t eod, i, ch, cnt, tcnt;
    uint16_t ver, error_code;
    uint8_t last_dseg_byte;
    char arr10[12], arr39[18], arr91[9];
    uint8_t stack[1024];
    uint8_t stack_base;
} type_data;

_Static_assert(offsetof(type_data, parse_fn) == TYPE_PARSEFN, "parsefn");
_Static_assert(offsetof(type_data, eod) == TYPE_EOD, "eod");
_Static_assert(offsetof(type_data, i) == TYPE_I, "i");
_Static_assert(offsetof(type_data, ch) == TYPE_CHAR, "char");
_Static_assert(offsetof(type_data, cnt) == TYPE_CNT, "cnt");
_Static_assert(offsetof(type_data, tcnt) == TYPE_TCNT, "tcnt");
_Static_assert(offsetof(type_data, ver) == TYPE_VER, "ver");
_Static_assert(offsetof(type_data, error_code) == TYPE_ERRORCODE, "errorcode");
_Static_assert(offsetof(type_data, last_dseg_byte) == TYPE_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(type_data, arr10) == TYPE_ARR10, "arr10");
_Static_assert(offsetof(type_data, arr39) == TYPE_ARR39, "arr39");
_Static_assert(offsetof(type_data, arr91) == TYPE_ARR91, "arr91");
_Static_assert(offsetof(type_data, stack_base) == TYPE_STACK_BASE, "stack_base");
_Static_assert(sizeof(type_data) == TYPE_DATA_LEN, "length");

#define T 0xFF /* TYPE's true */
#define MPMPRODUCT 0x11
#define CPMVERSION 0x30
#define CR    13
#define LF    10
#define CTRLX 0x18
#define BKSP  8
#define CTRLZ 0x1A

const uint16_t type_image_len = sizeof(type_data);
const type_data type_image = {
    .last_dseg_byte = 0,
    .arr10 = "Password ? $",
    .arr39 = "Requires MP/M 2.0$",
    .arr91 = "No file.$",
};

static uint16_t g_ds;
static type_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* the base page, and its fcb, fcb16, tbuff */
static uint8_t *fcb = 0;
static uint8_t *fcb16 = 0;
static uint8_t *tbuff = 0;

static uint8_t read_console(void) { return mon2(1, 0); }
static void printchar(uint8_t c) { mon1(2, c); }
static uint8_t conin(void) { return mon2(6, 0xFD); }
static void print_buf(uint16_t a) { mon1(9, a); }
static uint8_t check_con_stat(void) { return mon2(11, 0); }
static uint16_t version(void) { return mon3(12, 0); }
static uint8_t con_status(void) { return mon2(11, 0); }
static uint16_t open_file(uint16_t a) { return mon3(15, a); }
static uint8_t read_record(uint16_t a) { return mon2(20, a); }
static void setdma(uint16_t a) { mon1(26, a); }
static void return_errors(uint8_t mode) { mon1(45, mode); }
static void terminate(void) { mon1(143, 0); }
static void parse(void) { mon1(152, TYPE_PARSEFN); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

static void crlf(void) {
    printchar(CR);
    printchar(LF);
}

/* fill string @ s for c bytes with f */
static void fill(uint8_t *s, uint8_t f, uint8_t c) {
    while (rel((c = (uint8_t)(c - 1)) != 255) & 1) *s++ = f;
}

/* upper case character from console */
static uint8_t ucase(void) {
    uint8_t c;
    if (rel((c = conin()) >= 'a') & 1)
        if (rel(c < '{') & 1) return (uint8_t)(c - 0x20);
    return c;
}

/* get password and place at fcb + 16 */
static void getpasswd(void) {
    uint8_t i, c;
    crlf();
    crlf();
    print_buf(TYPE_ARR10); /* Password ? */
retry:
    fill(fcb16, ' ', 8);
    for (i = 0; i <= 7; i++) {
    nxtchr:
        if (rel((c = ucase()) >= ' ') & 1) fcb16[i] = c;
        if (rel(c == CR) & 1) goto exit;
        if (rel(c == CTRLX) & 1) goto retry;
        if (rel(c == BKSP) & 1) {
            if (rel(i < 1) & 1) goto retry;
            fcb16[i = (uint8_t)(i - 1)] = ' ';
            goto nxtchr;
        }
        if (rel(c == 3) & 1) terminate();
    }
exit:
    c = check_con_stat();
    (void)c;
}

void type_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"TYPE    ", (const uint8_t *)&type_image, type_image_len});
    g_ds = ds;
    g = RSP_DATA(type_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;
    fcb16 = bp + bp_fcb16;
    tbuff = bp + bp_tbuff;

    g->ver = version();
    if ((rel((g->ver & 0xFF) != CPMVERSION) | rel((g->ver >> 8) != MPMPRODUCT)) & 1) {
        print_buf(TYPE_ARR39); /* Requires MP/M 2.0 */
        mon1(0, 0);
    }

    g->tcnt = g->cnt = 0;
    if (rel(fcb16[1] == 'P') & 1) {
        if ((rel(fcb16[2] == ' ') | rel(fcb16[2] == 'A')) & 1)
            g->cnt = 24;
        else
            g->cnt = (uint8_t)((fcb16[2] - '0') * 10 + (fcb16[3] - '0'));
    }
    if (rel(bp[bp_len0] != 0) & 1) {
        g->parse_fn[0] = bp_tbuff + 1;
        g->parse_fn[1] = bp_fcb;
        parse(); /* get password */
    }
    return_errors(0xFE); /* return after error message */
    setdma(bp_fcb16);    /* set dma to password */
    fcb[6] |= 0x80;      /* open in RO mode */
    g->error_code = open_file(bp_fcb);
    if (rel((g->error_code & 0xFF) == 0xFF) & 1)
        if (rel((g->error_code >> 8) == 7) & 1) {
            getpasswd();
            crlf();
            setdma(bp_fcb16);
            fcb[6] |= 0x80;
            g->error_code = open_file(bp_fcb);
        }
    if (rel((g->error_code & 0xFF) != 0xFF) & 1) {
        return_errors(0); /* reset error mode */
        setdma(bp_tbuff);
        fcb[32] = 0;
        g->eod = 0;
        /* PL/M's AND: both sides, so the record is read even at eod */
        while (((uint8_t)~g->eod & rel(read_record(bp_fcb) == 0)) & 1) {
            for (g->i = 0; g->i <= 127; g->i++) {
                if (rel((g->ch = tbuff[g->i]) == CTRLZ) & 1) g->eod = T;
                if ((uint8_t)~g->eod & 1) {
                    if (con_status() & 1) {
                        g->i = read_console();
                        terminate();
                    }
                    if (rel(g->cnt != 0) & 1)
                        if (rel(g->ch == LF) & 1)
                            if (rel((g->tcnt = (uint8_t)(g->tcnt + 1)) == g->cnt) & 1) {
                                g->tcnt = read_console();
                                g->tcnt = 0;
                            }
                    printchar(g->ch);
                }
            }
        }
        /* no close: "If this call is left in, the file can be destroyed." */
    } else if (rel((g->error_code >> 8) == 0) & 1) {
        print_buf(TYPE_ARR91); /* No file. */
    }
    terminate();
}
