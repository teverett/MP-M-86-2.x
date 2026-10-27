/*
 * ed.c -- C version of src-assem/utils/ED.PLM (a transient, ED.CMD):
 * the CP/M context editor. ED name edits a file through a text buffer:
 * A appends lines from the file, W writes them out, the commands move,
 * type, find, substitute, insert, delete and transfer text, and E (or
 * H) writes what's left to name.$$$, renames the old file to name.BAK
 * and the $$$ to name. Errors print BREAK "x" AT c.
 *
 * Its data segment, below, is ED.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values, the string constants, and the stack; ed_main installs the
 * image from 100h on. PL/M's semantics are kept as in dir.c (ED's own
 * `true` is 1, so NOT TRUE is 0FEh: IF tests bit 0).
 *
 * The text buffer is MEMORY, the memory past the image (.MEMORY = the
 * end of the data segment) up to MAXB, the data group's length in the
 * base page: ED takes its source and destination buffers from the top
 * and keeps the text below them. It is reached, as the PL/M reaches
 * it, by DS offset (bp[]), as are the arrays (RFCB and XFCB, 33 bytes
 * with 15 initialized, and PASSWORD, 16 with 1: until plmc allocated the
 * rest of a short INITIAL list's array, their later bytes were the next
 * variables', docs/bugs.md #131).
 *
 * The PL/M's GO TOs from a procedure to the main program's labels
 * (START, RESTART, OVERCOUNT, BADCOM, OVERFLOW, RESET) unwind to
 * ed_main's setjmp (go()); everything they leave is in the image or in
 * file statics, as it is in the PL/M.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the jump back to the main program: setjmp on the host, GCC's
 * builtin in freestanding kernel.elf */
#if __STDC_HOSTED__
#include <setjmp.h>
static jmp_buf ed_jump;
#define ED_SETJMP()  setjmp(ed_jump)
#define ED_LONGJMP() longjmp(ed_jump, 1)
#else
static void *ed_jump[5];
#define ED_SETJMP()  __builtin_setjmp(ed_jump)
#define ED_LONGJMP() __builtin_longjmp(ed_jump, 1)
#endif

/* the base page (the PL/M interface stub's names) */
#define bp_bdisk 0x04
#define bp_maxb  0x06
#define bp_len0  0x53
#define bp_fcb   0x5C
#define bp_fcb16 0x6C
#define bp_buff  0x80
#define bp_tbuff 0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    char copyright[38];
    uint16_t max, maxm, hmax;
    uint8_t rfcb[33], rbp, xfcb[33], xbuff[128], xbp, xferon, xchksum, nbuf;
    uint16_t bufflength, sbuffadr;
    uint8_t password[16], dfcb[33];
    uint16_t dbuffadr, nsource, ndest;
    uint8_t sdisk, ddisk, printsuppress, sys, protection;
    uint16_t error_code;
    uint8_t column, scolumn, tcolumn, qcolumn, dcnt, maxlen, comlen, combuff[128], tcbp, cbp;
    uint16_t parse_fn[2]; /* buff$adr, fcb$adr */
    uint16_t baseline, relline;
    uint8_t lineset, macro[128], scratch[100], wbp, wbe, wbj, flag, mp, mi, xp;
    uint16_t mt;
    uint8_t inserting, readbuff, translate, upper;
    uint16_t distance, tdist;
    uint8_t direction, ch; /* CHAR */
    uint16_t front, back, first, last, ver;
    char arr39[24], arr46[23], arr89[3], arr90[3], arr101[12], arr192[22], arr202[24], arr215[9];
    char arr216[3], arr220[3], arr418[8], arr437[18], arr441[10], arr445[18], arr452[8], arr453[6];
    uint16_t main_bp; /* the PL/M's main frame pointer (its non-local GO TOs) */
    uint8_t stack[1024];
    uint8_t stack_base;
} ed_data;

_Static_assert(offsetof(ed_data, copyright) == ED_COPYRIGHT, "copyright");
_Static_assert(offsetof(ed_data, max) == ED_MAX, "max");
_Static_assert(offsetof(ed_data, maxm) == ED_MAXM, "maxm");
_Static_assert(offsetof(ed_data, hmax) == ED_HMAX, "hmax");
_Static_assert(offsetof(ed_data, rfcb) == ED_RFCB, "rfcb");
_Static_assert(offsetof(ed_data, rbp) == ED_RBP, "rbp");
_Static_assert(offsetof(ed_data, xfcb) == ED_XFCB, "xfcb");
_Static_assert(offsetof(ed_data, xbuff) == ED_XBUFF, "xbuff");
_Static_assert(offsetof(ed_data, xbp) == ED_XBP, "xbp");
_Static_assert(offsetof(ed_data, xferon) == ED_XFERON, "xferon");
_Static_assert(offsetof(ed_data, xchksum) == ED_XCHKSUM, "xchksum");
_Static_assert(offsetof(ed_data, nbuf) == ED_NBUF, "nbuf");
_Static_assert(offsetof(ed_data, bufflength) == ED_BUFFLENGTH, "bufflength");
_Static_assert(offsetof(ed_data, sbuffadr) == ED_SBUFFADR, "sbuffadr");
_Static_assert(offsetof(ed_data, password) == ED_PASSWORD, "password");
_Static_assert(offsetof(ed_data, dfcb) == ED_DFCB, "dfcb");
_Static_assert(offsetof(ed_data, dbuffadr) == ED_DBUFFADR, "dbuffadr");
_Static_assert(offsetof(ed_data, nsource) == ED_NSOURCE, "nsource");
_Static_assert(offsetof(ed_data, ndest) == ED_NDEST, "ndest");
_Static_assert(offsetof(ed_data, sdisk) == ED_SDISK, "sdisk");
_Static_assert(offsetof(ed_data, ddisk) == ED_DDISK, "ddisk");
_Static_assert(offsetof(ed_data, printsuppress) == ED_PRINTSUPPRESS, "printsuppress");
_Static_assert(offsetof(ed_data, sys) == ED_SYS, "sys");
_Static_assert(offsetof(ed_data, protection) == ED_PROTECTION, "protection");
_Static_assert(offsetof(ed_data, error_code) == ED_ERRORCODE, "errorcode");
_Static_assert(offsetof(ed_data, column) == ED_COLUMN, "column");
_Static_assert(offsetof(ed_data, scolumn) == ED_SCOLUMN, "scolumn");
_Static_assert(offsetof(ed_data, tcolumn) == ED_TCOLUMN, "tcolumn");
_Static_assert(offsetof(ed_data, qcolumn) == ED_QCOLUMN, "qcolumn");
_Static_assert(offsetof(ed_data, dcnt) == ED_DCNT, "dcnt");
_Static_assert(offsetof(ed_data, maxlen) == ED_MAXLEN, "maxlen");
_Static_assert(offsetof(ed_data, comlen) == ED_COMLEN, "comlen");
_Static_assert(offsetof(ed_data, combuff) == ED_COMBUFF, "combuff");
_Static_assert(offsetof(ed_data, tcbp) == ED_TCBP, "tcbp");
_Static_assert(offsetof(ed_data, cbp) == ED_CBP, "cbp");
_Static_assert(offsetof(ed_data, parse_fn) == ED_PARSEFN, "parsefn");
_Static_assert(offsetof(ed_data, baseline) == ED_BASELINE, "baseline");
_Static_assert(offsetof(ed_data, relline) == ED_RELLINE, "relline");
_Static_assert(offsetof(ed_data, lineset) == ED_LINESET, "lineset");
_Static_assert(offsetof(ed_data, macro) == ED_MACRO, "macro");
_Static_assert(offsetof(ed_data, scratch) == ED_SCRATCH, "scratch");
_Static_assert(offsetof(ed_data, wbp) == ED_WBP, "wbp");
_Static_assert(offsetof(ed_data, wbe) == ED_WBE, "wbe");
_Static_assert(offsetof(ed_data, wbj) == ED_WBJ, "wbj");
_Static_assert(offsetof(ed_data, flag) == ED_FLAG, "flag");
_Static_assert(offsetof(ed_data, mp) == ED_MP, "mp");
_Static_assert(offsetof(ed_data, mi) == ED_MI, "mi");
_Static_assert(offsetof(ed_data, xp) == ED_XP, "xp");
_Static_assert(offsetof(ed_data, mt) == ED_MT, "mt");
_Static_assert(offsetof(ed_data, inserting) == ED_INSERTING, "inserting");
_Static_assert(offsetof(ed_data, readbuff) == ED_READBUFF, "readbuff");
_Static_assert(offsetof(ed_data, translate) == ED_TRANSLATE, "translate");
_Static_assert(offsetof(ed_data, upper) == ED_UPPER, "upper");
_Static_assert(offsetof(ed_data, distance) == ED_DISTANCE, "distance");
_Static_assert(offsetof(ed_data, tdist) == ED_TDIST, "tdist");
_Static_assert(offsetof(ed_data, direction) == ED_DIRECTION, "direction");
_Static_assert(offsetof(ed_data, ch) == ED_CHAR, "char");
_Static_assert(offsetof(ed_data, front) == ED_FRONT, "front");
_Static_assert(offsetof(ed_data, back) == ED_BACK, "back");
_Static_assert(offsetof(ed_data, first) == ED_FIRST, "first");
_Static_assert(offsetof(ed_data, last) == ED_LAST, "last");
_Static_assert(offsetof(ed_data, ver) == ED_VER, "ver");
_Static_assert(offsetof(ed_data, arr39) == ED_ARR39, "arr39");
_Static_assert(offsetof(ed_data, arr46) == ED_ARR46, "arr46");
_Static_assert(offsetof(ed_data, arr89) == ED_ARR89, "arr89");
_Static_assert(offsetof(ed_data, arr90) == ED_ARR90, "arr90");
_Static_assert(offsetof(ed_data, arr101) == ED_ARR101, "arr101");
_Static_assert(offsetof(ed_data, arr192) == ED_ARR192, "arr192");
_Static_assert(offsetof(ed_data, arr202) == ED_ARR202, "arr202");
_Static_assert(offsetof(ed_data, arr215) == ED_ARR215, "arr215");
_Static_assert(offsetof(ed_data, arr216) == ED_ARR216, "arr216");
_Static_assert(offsetof(ed_data, arr220) == ED_ARR220, "arr220");
_Static_assert(offsetof(ed_data, arr418) == ED_ARR418, "arr418");
_Static_assert(offsetof(ed_data, arr437) == ED_ARR437, "arr437");
_Static_assert(offsetof(ed_data, arr441) == ED_ARR441, "arr441");
_Static_assert(offsetof(ed_data, arr445) == ED_ARR445, "arr445");
_Static_assert(offsetof(ed_data, arr452) == ED_ARR452, "arr452");
_Static_assert(offsetof(ed_data, arr453) == ED_ARR453, "arr453");
_Static_assert(offsetof(ed_data, main_bp) == ED_MAIN_BP, "main_bp");
_Static_assert(offsetof(ed_data, stack_base) == ED_STACK_BASE, "stack_base");
_Static_assert(sizeof(ed_data) == ED_DATA_LEN, "length");
_Static_assert(sizeof(ed_data) == ED_MEMORY, "memory");

#define T 1 /* ED's true */
#define F 0
#define MPMPRODUCT 0x01
#define CPMVERSION 0x30

#define CR      13
#define LF      10
#define WHAT    63
#define CTLL    0x0C
#define CTLR    0x12 /* repeat line in insert mode */
#define CTLU    0x15 /* line delete in insert mode */
#define CTLX    0x18 /* equivalent to CTLU */
#define CTLH    0x08 /* backspace */
#define TAB     0x09
#define LCA     0x61 /* lower case a */
#define LCZ     0x7A /* lower case z */
#define ENDFILE 0x1A
#define RUBOUT  0x7F
#define POUND   0x23
#define LPP     23   /* lines per page */
#define FORWARD  1
#define BACKWARD 0
#define SCRSIZE  100 /* scratch buffer size */
#define SECTSHF  7
#define SECTSIZE 0x80

/* fcb fields */
#define US 8  /* file from user 0 */
#define RO 9  /* R/O file indicator */
#define SY 10 /* system file attribute */
#define EX 12 /* extent number */
#define CK 13 /* checksum */
#define MD 14 /* module number */
#define NR 32 /* next record */

const uint16_t ed_image_len = sizeof(ed_data);
const ed_data ed_image = {
    .copyright = " COPYRIGHT (C) 1981, DIGITAL RESEARCH ",
    .rfcb = {0, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'L', 'I', 'B', 0, 0, 0},
    .xfcb = {0, 'X', '$', '$', '$', '$', '$', '$', '$', 'L', 'I', 'B', 0, 0, 0},
    .password = {0},
    .sys = 0,
    .protection = 0,
    .arr39 = "Error Creating Password$",
    .arr46 = "DISK OR DIRECTORY FULL$",
    .arr89 = "BAK",
    .arr90 = "$$$",
    .arr101 = "Password ? $",
    .arr192 = "FILE EXISTS, ERASE IT$",
    .arr202 = "** FILE IS READ/ONLY **$",
    .arr215 = "NEW FILE$",
    .arr216 = "BAK",
    .arr220 = "$$$",
    .arr418 = "-(Y/N)?$",
    .arr437 = "Requires MP/M 2.0$",
    .arr441 = "NO MEMORY$",
    .arr445 = "Filename Required$",
    .arr452 = "BREAK \"$",
    .arr453 = "\" AT $",
    .main_bp = 0,
};

static uint16_t g_ds;
static ed_data *g;
static uint8_t *bp = 0;  /* DS:0 -- the base page, the image, then MEMORY */
static uint8_t *fcb = 0; /* the base page's fcb (SFCB) */

/* the DS-relative arrays (see the header comment) */
#define MEMORY(i)   bp[(uint16_t)(ED_MEMORY + (i))]
#define RFCB(i)     bp[(uint16_t)(ED_RFCB + (i))]
#define XBUFF(i)    bp[(uint16_t)(ED_XBUFF + (i))]
#define PASSWORD(i) bp[(uint16_t)(ED_PASSWORD + (i))]
#define DFCB(i)     bp[(uint16_t)(ED_DFCB + (i))]
#define COMBUFF(i)  bp[(uint16_t)(ED_COMBUFF + (i))]
#define MACRO(i)    bp[(uint16_t)(ED_MACRO + (i))]
#define SCRATCH(i)  bp[(uint16_t)(ED_SCRATCH + (i))]
#define SBUFF(i)    bp[(uint16_t)(g->sbuffadr + (i))]
#define DBUFF(i)    bp[(uint16_t)(g->dbuffadr + (i))]
#define XFCBE       bp[ED_XFCB + EX]   /* xfcb extent */
#define XFCBR       bp[ED_XFCB + NR]   /* xfcb record # */
#define XFCBC       bp[ED_XFCB + CK]   /* xfcb checksum */

/* the main program's labels a procedure can GO TO */
enum { L_RESTART, L_START, L_OVERCOUNT, L_BADCOM, L_OVERFLOW, L_RESET };
static uint8_t go_label;

static _Noreturn void go(uint8_t label) {
    go_label = label;
    ED_LONGJMP();
}

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */
static uint8_t rol1(uint8_t b) { return (uint8_t)((b << 1) | (b >> 7)); }

/* PL/M's move(count, source, destination), on DS addresses */
static void move(uint16_t n, uint16_t src, uint16_t dst) {
    while (n--) bp[dst++] = bp[src++];
}

/* PL/M's TIME(n): a delay of n x 100 microseconds, a counted loop */
static void plm_time(uint16_t n) {
    for (volatile uint16_t i = n; i != 0; i--)
        for (volatile uint8_t k = 100; k != 0; k--) {}
}

static void boot(void) { mon1(0, 0); } /* system reboot */

/* IO section */

static uint8_t readchar(void) { return mon2(1, 0); }
static uint8_t conin(void) { return mon2(6, 0xFD); }

static void printchar(uint8_t c) {
    if (g->printsuppress & 1) return;
    mon1(2, c);
}

static void ttychar(uint8_t c) {
    if (rel(c >= ' ') & 1) g->column = (uint8_t)(g->column + 1);
    if (rel(c == LF) & 1) g->column = 0;
    printchar(c);
}

/* move back one position */
static void backspace(void) {
    if (rel(g->column == 0) & 1) return;
    ttychar(CTLH); /* column = column - 1 */
    ttychar(' ');  /* column = column + 1 */
    ttychar(CTLH); /* column = column - 1 */
    g->column = (uint8_t)(g->column - 2);
}

static void printabs(uint8_t c) {
    uint8_t i, j;
    i = rel(c == TAB) & (uint8_t)(7 - (g->column & 7));
    if (rel(c == TAB) & 1) c = ' ';
    for (j = 0; j <= i; j++) ttychar(c);
}

/* return true if graphic character */
static uint8_t graphic(uint8_t c) {
    if (rel(c >= ' ') & 1) return T;
    return rel(c == CR) | rel(c == LF) | rel(c == TAB);
}

static void printc(uint8_t c) {
    if (~graphic(c) & 1) {
        printabs('^');
        c = (uint8_t)(c + '@');
    }
    printabs(c);
}

static void crlf(void) {
    printc(CR);
    printc(LF);
}

static void printm(uint16_t a) { mon1(9, a); }

static void print(uint16_t a) {
    crlf();
    printm(a);
}

static void read(uint16_t a) { mon1(10, a); }

static void open(uint16_t f) { g->dcnt = mon2(15, f); }
static uint16_t open_file(uint16_t f) { return mon3(15, f); }
static void close(uint16_t f) { g->dcnt = mon2(16, f); }
static void search(uint16_t f) { g->dcnt = mon2(17, f); }
static void delete(uint16_t f) { mon1(19, f); }
static uint8_t diskread(uint16_t f) { return mon2(20, f); }
static uint8_t diskwrite(uint16_t f) { return mon2(21, f); }
static void make(uint16_t f) { g->dcnt = mon2(22, f); }
static void rename(uint16_t f) { mon1(23, f); }

static void readcom(void) {
    g->maxlen = 128;
    read(ED_MAXLEN);
}

static uint8_t break_key(void) {
    if (mon2(11, 0) & 1) { /* clear char */
        mon1(1, 0);
        return T;
    }
    return F;
}

static uint8_t cselect(void) { return mon2(25, 0); } /* current drive number */
static void select(uint8_t disk) { mon1(14, disk); }
static void setdma(uint16_t a) { mon1(26, a); }
static void set_attribute(uint16_t f) { mon1(30, f); }

static void write_xfcb(uint16_t f) {
    move(8, ED_PASSWORD, ED_PASSWORD + 8);
    if (rel(mon2(103, f) == 0xFF) & 1) print(ED_ARR39); /* Error Creating Password */
}

static void read_xfcb(uint16_t f) { mon1(102, f); }

/* 0ff => return BDOS errors */
static void return_errors(uint8_t mode) { mon1(45, mode); }

static void reboot(void) {
    if (g->xferon & 1) delete(ED_XFCB);
    boot();
}

static uint16_t version(void) { return mon3(12, 0); }
static void parse(void) { mon1(152, ED_PARSEFN); }

/* input / output buffering routines */

static void abort_(uint16_t a) {
    print(a);
    crlf();
    reboot();
}

static void delete_file(uint16_t f) {
    setdma(ED_PASSWORD);
    delete(f);
}

/* fill string @ s for c bytes with f */
static void fill(uint16_t s, uint8_t f, uint8_t c) {
    while (rel((c = (uint8_t)(c - 1)) != 255) & 1) bp[s++] = f;
}

static void ferr(void) {
    close(ED_DFCB);   /* attempt to close file for later recovery */
    abort_(ED_ARR46); /* DISK OR DIRECTORY FULL */
}

static void settype(uint16_t a) { move(3, a, ED_DFCB + 9); }

/* clear the xfer file parameters */
static void xclear(void) { g->xferon = XFCBE = XFCBR = g->xbp = 0; }

static void setxdma(void) {
    select(g->sdisk);
    setdma(ED_XBUFF);
}

static void fillsource(void) {
    uint8_t i;
    g->nsource = 0; /* zn */
    select(g->sdisk);
    for (i = 0; i <= g->nbuf; i++) {
        setdma((uint16_t)(g->sbuffadr + g->nsource));
        if (rel((g->dcnt = diskread(bp_fcb)) != 0) & 1) {
            if (rel(g->dcnt > 1) & 1) ferr();
            SBUFF(g->nsource) = ENDFILE;
            i = g->nbuf;
        } else {
            g->nsource = (uint16_t)(g->nsource + SECTSIZE);
        }
    }
    g->nsource = 0; /* zn */
}

static uint8_t getsource(void) {
    uint8_t b;
    if (rel(g->nsource >= g->bufflength) & 1) fillsource();
    if (rel((b = SBUFF(g->nsource)) != ENDFILE) & 1) g->nsource = (uint16_t)(g->nsource + 1);
    return b;
}

/* write output buffer up to (not including) ndest; low 7 bits of
 * ndest are zero */
static void writedest(void) {
    uint8_t i, n;
    select(g->ddisk);
    if (rel((n = (uint8_t)((g->ndest >> SECTSHF) - 1)) == 255) & 1) return;
    g->ndest = 0; /* zn */
    for (i = 0; i <= n; i++) {
        setdma((uint16_t)(g->dbuffadr + g->ndest));
        if (rel(diskwrite(ED_DFCB) != 0) & 1) ferr();
        g->ndest = (uint16_t)(g->ndest + SECTSIZE);
    }
    g->ndest = 0; /* zn */
}

static void putdest(uint8_t b) {
    if (rel(g->ndest >= g->bufflength) & 1) writedest();
    DBUFF(g->ndest) = b;
    g->ndest = (uint16_t)(g->ndest + 1);
}

/* write c to xfer file */
static void putxfer(uint8_t c) {
    if (rel(g->xbp >= SECTSIZE) & 1) { /* buffer overflow */
        setxdma();
        XFCBC = g->xchksum;
        if (rel(diskwrite(ED_XFCB) != 0) & 1) ferr();
        g->xbp = 0;
        g->xchksum = XFCBC; /* save checksum */
    }
    XBUFF(g->xbp) = c;
    g->xbp = (uint8_t)(g->xbp + 1);
}

static void moveup(void) { move(16, ED_DFCB, ED_DFCB + 16); }

static void finis(void) {
    /* clear output (the unfilled-bytes count is gone for MP/M 2, CP/M 3) */
    while (rel(((uint8_t)g->ndest & 0x7F) != 0) & 1) putdest(ENDFILE);
    writedest();
    close(ED_DFCB);
    if (rel(g->dcnt == 255) & 1) ferr();
    if (g->sys & 1) {
        DFCB(SY) |= 0x80;
        setdma(ED_PASSWORD);
        set_attribute(ED_DFCB);
    }
    /* rename old file to bak */
    settype(ED_ARR89);
    moveup();
    select(g->sdisk);
    move(16, bp_fcb, ED_DFCB);
    setdma(ED_PASSWORD);
    rename(ED_DFCB);
    moveup();
    /* rename $$$ to old name */
    settype(ED_ARR90);
    select(g->ddisk);
    setdma(ED_PASSWORD);
    rename(ED_DFCB);
}

/* print if not in macro expansion */
static void printnmac(uint8_t c) {
    if (rel(g->mp != 0) & 1) return;
    printc(c);
}

/* return true if lower case alphabetic */
static uint8_t lowercase(uint8_t c) { return rel(c >= LCA) & rel(c <= LCZ); }

/* translate c to upper case */
static uint8_t ucase(uint8_t c) {
    if (lowercase(c) & 1) return c & 0x5F;
    return c;
}

/* get password and place at password */
static void getpasswd(void) {
    uint8_t i, c;
    crlf();
    print(ED_ARR101); /* Password ? */
retry:
    fill(ED_PASSWORD, ' ', 8);
    for (i = 0; i <= 7; i++) {
    nxtchr:
        if (rel((c = ucase(conin())) >= ' ') & 1) PASSWORD(i) = c;
        if (rel(c == CR) & 1) goto exit;
        if (rel(c == CTLX) & 1) goto retry;
        if (rel(c == CTLH) & 1) {
            if (rel(i < 1) & 1) goto retry;
            PASSWORD(i = (uint8_t)(i - 1)) = ' ';
            goto nxtchr;
        }
        if (rel(c == 3) & 1) reboot();
    }
exit:
    c = break_key(); /* clear raw I/O mode */
    (void)c;
}

/* translate to upper case if alphabetic lower and translate */
static uint8_t utran(uint8_t c) {
    if (g->translate & 1) return ucase(c);
    return c;
}

/* print the line value v: leading zeros as blanks (the local ZERO is
 * set at the first nonzero digit; until plmc let it hide the built-in
 * flag ZERO, every 0 digit printed as a blank, docs/bugs.md #131) */
static void printvalue(uint16_t v) {
    uint8_t d;
    uint16_t k = 10000;
    uint8_t zero = F;
    while (rel(k != 0) & 1) {
        d = (uint8_t)(v / k);
        v = v % k;
        k = k / 10;
        if ((zero | rel(d != 0)) & 1) {
            zero = T;
            printc((uint8_t)('0' + d));
        } else {
            printc(' ');
        }
    }
}

static void printline(uint16_t v) {
    if (~g->lineset & 1) return;
    printvalue(v);
    printc(':');
    printc(' ');
    if (g->inserting & 1)
        printc(' ');
    else
        printc('*');
}

static void printbase(void) { printline(g->baseline); }

static void printnmbase(void) {
    if (rel(g->mp != 0) & 1) return;
    printbase();
}

/* may be macro expansion */
static uint8_t readc(void) {
    if (rel(g->mp > 0) & 1) {
        if (break_key() & 1) go(L_OVERCOUNT);
        if (rel(g->xp >= g->mp) & 1) { /* start again */
            if (rel(g->mt != 0) & 1) {
                if (rel((g->mt = (uint16_t)(g->mt - 1)) == 0) & 1) go(L_OVERCOUNT);
            }
            g->xp = 0;
        }
        return utran(MACRO((uint8_t)((g->xp = (uint8_t)(g->xp + 1)) - 1)));
    }
    if (g->inserting & 1) return utran(readchar());

    /* get command line */
    if (g->readbuff & 1) {
        g->readbuff = F;
        if ((g->lineset & rel(g->column == 0)) & 1) {
            if (rel(g->back >= g->maxm) & 1)
                printline(0);
            else
                printbase();
        } else {
            printc('*');
        }
        readcom();
        g->cbp = 0;
        printc(LF);
        g->column = 0;
    }
    if ((g->readbuff = rel(g->cbp == g->comlen)) & 1) COMBUFF(g->cbp) = CR;
    return utran(COMBUFF((uint8_t)((g->cbp = (uint8_t)(g->cbp + 1)) - 1)));
}

/* set read lib dma address */
static void setrdma(void) {
    select(g->sdisk);
    setdma(bp_buff);
}

static uint8_t readfile(void) {
    if (rel(g->rbp >= SECTSIZE) & 1) {
        setrdma();
        if (rel(diskread(ED_RFCB) != 0) & 1) return ENDFILE;
        g->rbp = 0;
    }
    return utran(bp[bp_buff + (uint8_t)((g->rbp = (uint8_t)(g->rbp + 1)) - 1)]);
}

static void setup(void) {
    g->nsource = g->bufflength;
    g->ndest = 0;
    g->parse_fn[0] = bp_tbuff + 1;
    g->parse_fn[1] = bp_fcb;
    if (rel(bp[bp_len0] != 0) & 1) {
        parse(); /* password in fcb16 */
        move(8, bp_fcb16, ED_PASSWORD);
    }
    fcb[EX] = fcb[MD] = fcb[NR] = 0; /* reel and record zeroed */
    /* copy name to destination fcb */
    move(33, bp_fcb, ED_DFCB);
    /* source and destination disks set */

    /* if source and destination disks differ, check for an existing
     * source file on the destination disk - there could be a fatal
     * error condition which could destroy a file if the user happened
     * to be addressing the wrong disk */
    if (rel(g->sdisk != g->ddisk) & 1) {
        select(g->ddisk);
        search(bp_fcb);
        if (rel(g->dcnt != 255) & 1) /* source file present on dest disk */
            abort_(ED_ARR192);        /* FILE EXISTS, ERASE IT */
    }
    select(g->sdisk);
    return_errors(0xFE); /* set error mode */
    setdma(ED_PASSWORD);
    g->error_code = open_file(bp_fcb);
    return_errors(0); /* reset error mode */
    if (rel((g->error_code & 0xFF) == 0xFF) & 1)
        if (rel((g->error_code >> 8) == 7) & 1) {
            getpasswd();
            crlf();
            crlf();
            setdma(ED_PASSWORD); /* set dma to password */
            g->error_code = open_file(bp_fcb);
        }
    g->dcnt = (uint8_t)g->error_code;
    if (rol1(fcb[RO]) & 1) {
        abort_(ED_ARR202); /* ** FILE IS READ/ONLY ** */
    } else if (rol1(fcb[SY]) & 1) { /* system attribute */
        if (rol1(fcb[US]) & 1)
            g->dcnt = 0xFF; /* user 0 file so create */
        else
            g->sys = T;
    }
    if (rel(g->dcnt == 255) & 1) {
        make(bp_fcb);
        if (rel(g->dcnt == 255) & 1) ferr();
        print(ED_ARR215); /* NEW FILE */
        crlf();
    }

    read_xfcb(ED_DFCB);
    g->protection = DFCB(EX); /* password protection mode */

    settype(ED_ARR216); /* BAK */
    delete_file(ED_DFCB);
    if (rel(g->sdisk != g->ddisk) & 1) { /* remove bak files from destination disk */
        select(g->ddisk);
        delete_file(ED_DFCB);
    }
    settype(ED_ARR220); /* $$$ */
    delete_file(ED_DFCB);
    DFCB(EX) = 0;
    setdma(ED_PASSWORD);
    make(ED_DFCB);
    if (rel(g->dcnt == 255) & 1) ferr();
    /* the temp file is now created; now create the password if any */
    if (rel(g->protection != 0) & 1) {
        DFCB(EX) = g->protection | 1; /* set password */
        setdma(ED_PASSWORD);
        write_xfcb(ED_DFCB);
    }
    DFCB(EX) = DFCB(32) = 0; /* next record is zero */
    g->baseline = 1;         /* start with line 1 */
}

static void setff(void) { g->distance = 0xFFFF; }

/* return true if distance is zero */
static uint8_t distzero(void) { return rel(g->distance == 0); }

static void zerodist(void) { g->distance = 0; }

/* check for zero distance and decrement */
static uint8_t distnzero(void) {
    if (~distzero() & 1) {
        g->distance = (uint16_t)(g->distance - 1);
        return T;
    }
    return F;
}

static void setlimits(void) {
    uint16_t i, k, l, m = 0;
    uint8_t middle, looping;
    g->relline = 1; /* relative line count */
    if (rel(g->direction == BACKWARD) & 1) {
        g->distance = (uint16_t)(g->distance + 1);
        i = g->front;
        l = 0;
        k = 0xFFFF;
    } else {
        i = g->back;
        l = g->maxm;
        k = 1;
    }

    looping = T;
    while (looping & 1) {
        while (((middle = rel(i != l)) & rel(MEMORY(m = (uint16_t)(i + k)) != LF)) & 1) i = m;
        g->relline = (uint16_t)(g->relline - 1);
        looping = rel((g->distance = (uint16_t)(g->distance - 1)) != 0);
        if (~middle & 1) {
            looping = F;
            i = (uint16_t)(i - k);
        } else if (looping & 1) {
            i = m;
        }
    }

    if (rel(g->direction == BACKWARD) & 1) {
        g->first = i;
        g->last = (uint16_t)(g->front - 1);
    } else {
        g->first = (uint16_t)(g->back + 1);
        g->last = (uint16_t)(i + 1);
    }
}

static void incfront(void) { g->front = (uint16_t)(g->front + 1); }
static void incback(void) { g->back = (uint16_t)(g->back + 1); }
static void decfront(void) { g->front = (uint16_t)(g->front - 1); }
static void decback(void) { g->back = (uint16_t)(g->back - 1); }
static void incbase(void) { g->baseline = (uint16_t)(g->baseline + 1); }

/* move if moveflag is true */
static void mem_move(uint8_t moveflag) {
    uint8_t c;
    if (rel(g->direction == FORWARD) & 1) {
        while (rel(g->back < g->last) & 1) {
            incback();
            if (moveflag & 1) {
                if (rel((c = MEMORY(g->back)) == LF) & 1) incbase();
                MEMORY(g->front) = c;
                incfront();
            }
        }
    } else {
        while (rel(g->front > g->first) & 1) {
            decfront();
            if (rel((c = MEMORY(g->front)) == LF) & 1) g->baseline = (uint16_t)(g->baseline - 1);
            if (moveflag & 1) {
                MEMORY(g->back) = c;
                decback();
            }
        }
    }
}

static void mover(void) { mem_move(T); }
static void setptrs(void) { mem_move(F); }

static void movelines(void) {
    setlimits();
    mover();
}

static void setclimits(void) {
    if (rel(g->direction == BACKWARD) & 1) {
        g->last = g->back;
        if (rel(g->distance > g->front) & 1)
            g->first = 1;
        else
            g->first = (uint16_t)(g->front - g->distance);
    } else {
        g->first = g->front;
        if (rel(g->distance >= (uint16_t)(g->max - g->back)) & 1)
            g->last = g->maxm;
        else
            g->last = (uint16_t)(g->back + g->distance);
    }
}

/* conditionally translate to upper case on input */
static uint8_t ctran(uint8_t b) {
    if (g->upper & 1) return utran(b);
    return b;
}

/* read another line of input */
static void readline(void) {
    uint8_t b;
    for (;;) {
        if (rel(g->front >= g->back) & 1) go(L_OVERFLOW);
        if (rel((b = ctran(getsource())) == ENDFILE) & 1) {
            zerodist();
            return;
        }
        MEMORY(g->front) = b;
        incfront();
        if (rel(b == LF) & 1) {
            incbase();
            return;
        }
    }
}

/* write one line out */
static void writeline(void) {
    uint8_t b;
    for (;;) {
        if (rel(g->back >= g->maxm) & 1) { /* empty */
            zerodist();
            return;
        }
        incback();
        putdest(b = MEMORY(g->back));
        if (rel(b == LF) & 1) {
            incbase();
            return;
        }
    }
}

/* write lines until at least half the buffer is empty */
static void wrhalf(void) {
    setff();
    while (distnzero() & 1) {
        if (rel(g->hmax >= (uint16_t)(g->maxm - g->back)) & 1)
            zerodist();
        else
            writeline();
    }
}

/* write lines determined by 'distance', called from W and E commands */
static void writeout(void) {
    g->direction = BACKWARD;
    g->first = 1;
    g->last = g->back;
    mover();
    if (distzero() & 1) wrhalf();
    /* distance = 0 if call wrhalf */
    while (distnzero() & 1) writeline();
    if (rel(g->back < g->last) & 1) {
        g->direction = FORWARD;
        mover();
    }
}

/* clear memory buffer */
static void clearmem(void) {
    setff();
    writeout();
}

/* TERMINATE: clear buffers */
static void terminate(void) {
    clearmem();
    while (rel((g->ch = getsource()) != ENDFILE) & 1) putdest(g->ch);
    finis();
}

/* insert char into memory buffer */
static void insert(void) {
    if (rel(g->front == g->back) & 1) go(L_OVERFLOW);
    MEMORY(g->front) = g->ch;
    incfront();
    if (rel(g->ch == LF) & 1) incbase();
}

/* read a character and check for endfile or cr */
static uint8_t scanning(void) {
    uint8_t endfile = rel((g->ch = readc()) == ENDFILE); /* PL/M: left to right */
    return (uint8_t)~(endfile | (rel(g->ch == CR) & (uint8_t)~g->inserting));
}

/* COLLECT's SETSCR */
static void setscr(void) {
    SCRATCH(g->wbe) = g->ch;
    if (rel((g->wbe = (uint8_t)(g->wbe + 1)) >= SCRSIZE) & 1) go(L_OVERFLOW);
}

/* read command buffer and insert characters into scratch 'til next
 * control-z or cr for find, next, juxt, or substitute commands - fill
 * at wbe and increment wbe so it addresses next empty position of
 * scratch */
static void collect(void) {
    while (scanning() & 1) {
        if (rel(g->ch == CTLL) & 1) {
            g->ch = CR;
            setscr();
            g->ch = LF;
        }
        if (rel(g->ch == 0) & 1) go(L_BADCOM);
        setscr();
    }
}

/* find the string in scratch starting at pa and ending at pb */
static uint8_t find(uint8_t pa, uint8_t pb) {
    uint16_t j;
    uint8_t k, match;
    j = g->back;
    match = F;
    while (((uint8_t)~match & rel(g->maxm > j)) & 1) {
        g->last = j = (uint16_t)(j + 1); /* start scan at j */
        k = pa;                           /* attempt string match at k */
        while ((rel(SCRATCH(k) == MEMORY(g->last)) & (uint8_t)~(match = rel(k == pb))) & 1) {
            /* matched one more character */
            k = (uint8_t)(k + 1);
            g->last = (uint16_t)(g->last + 1);
        }
    }
    if (match & 1) { /* move storage */
        g->last = (uint16_t)(g->last - 1);
        mover();
    }
    return match;
}

/* setup the search string for f, n, and s commands */
static void setfind(void) {
    g->wbe = 0;
    collect();
    g->wbp = g->wbe;
}

/* check for found string in f and s commands */
static void chkfound(void) {
    if (~find(0, g->wbp) & 1) go(L_OVERCOUNT); /* no match */
}

/* place char into read file control block and increment */
static void setrfcb(void) { RFCB((uint8_t)((g->rbp = (uint8_t)(g->rbp + 1)) - 1)) = ucase(g->ch); }

static void printrel(void) { printline((uint16_t)(g->baseline + g->relline)); }

static void typelines(void) {
    uint16_t i;
    uint8_t c;
    setlimits();
    /* disable the * prompt */
    g->inserting = T;
    if (rel(g->direction == FORWARD) & 1) {
        g->relline = 0;
        i = g->front;
    } else {
        i = g->first;
    }
    if ((rel((c = MEMORY(i - 1)) == LF) & rel(g->column != 0)) & 1) crlf();
    for (i = g->first; i <= g->last; i++) {
        if (rel(c == LF) & 1) {
            printrel();
            g->relline = (uint16_t)(g->relline + 1);
            if (break_key() & 1) go(L_OVERCOUNT);
        }
        printc(c = MEMORY(i));
    }
}

/* set distance to lines per page */
static void setlpp(void) { g->distance = LPP; }
static void savedist(void) { g->tdist = g->distance; }
static void restdist(void) { g->distance = g->tdist; }

static void page(void) {
    uint8_t i;
    savedist();
    setlpp();
    movelines();
    i = g->direction;
    g->direction = FORWARD;
    setlpp();
    typelines();
    g->direction = i;
    if ((rel(g->last == g->maxm) | rel(g->first == 1)) & 1)
        zerodist();
    else
        restdist();
}

/* 1/2 second time out */
static void wait(void) {
    for (uint8_t i = 0; i <= 19; i++) {
        if (break_key() & 1) go(L_RESET);
        plm_time(250);
    }
}

static void setforward(void) {
    g->direction = FORWARD;
    g->distance = 1;
}

/* append 'til buffer is at least half full */
static void apphalf(void) {
    setff(); /* distance = 0ffffh */
    while (distnzero() & 1) {
        if (rel(g->front >= g->hmax) & 1)
            zerodist();
        else
            readline();
    }
}

/* insert cr lf characters */
static void inscrlf(void) {
    g->ch = CR;
    insert();
    g->ch = LF;
    insert();
}

/* test for upper or lower case command and set translate flag (used to
 * determine if characters which follow go to upper) */
static void testcase(void) {
    uint8_t t;
    g->translate = T;
    t = lowercase(g->ch);
    g->ch = utran(g->ch);
    g->translate = g->upper | (uint8_t)~t;
}

/* set translate to false and read next character */
static void readctran(void) {
    g->translate = F;
    g->ch = readc();
    testcase();
}

/* return true if command is only character, not in macro */
static uint8_t singlecom(uint8_t c) { return rel(g->ch == c) & rel(g->comlen == 1) & rel(g->mp == 0); }

/* return true if command is only character, not in macro, and the
 * operator has responded with 'Y' to a Y/N request */
static uint8_t singlercom(uint8_t c) {
    if (singlecom(c) & 1) {
        crlf();
        printchar(c);
        mon1(9, ED_ARR418); /* -(Y/N)? */
        c = ucase(readchar());
        crlf();
        if (rel(c != 'Y') & 1) go(L_START);
        return T;
    }
    return F;
}

/* the main program's DO blocks' own variables (R's I, the number
 * scan's I, J's T, X's I) */
static uint8_t r_i, n_i;
static uint16_t j_t, x_i;

static uint8_t digit(void) { return rel((n_i = (uint8_t)(g->ch - '0')) <= 9); }

static void number(void) {
    g->distance = 0;
    while (digit() & 1) {
        g->distance = (uint16_t)((g->distance << 3) + (g->distance << 1) + n_i);
        readctran();
    }
    /* return with distance = number, char = next */
}

static void reldistance(void) {
    if (rel(g->distance > g->baseline) & 1) {
        g->direction = FORWARD;
        g->distance = (uint16_t)(g->distance - g->baseline);
    } else {
        g->direction = BACKWARD;
        g->distance = (uint16_t)(g->baseline - g->distance);
    }
}

/* I: insert characters */
static void insert_command(void) {
    if ((g->inserting = rel(g->cbp == g->comlen) & rel(g->mp == 0)) & 1) printnmbase();
    g->scolumn = g->column; /* starting column position */
    while (scanning() & 1) {
        while (rel(g->ch != 0) & 1) {
            if ((rel(g->ch == CTLU) | rel(g->ch == CTLX) | rel(g->ch == CTLR)) & 1) {
                /* line delete or retype */
                g->distance = 0;
                g->direction = BACKWARD;
                /* eliminate or repeat the line */
                if (rel(g->ch == CTLR) & 1) {
                    crlf();
                    typelines();
                } else { /* line delete */
                    setlimits();
                    setptrs();
                    if (rel(g->ch == CTLU) & 1) {
                        crlf();
                        printnmbase();
                    } else { /* must be ctlx */
                        while (rel(g->column > g->scolumn) & 1) backspace();
                    }
                }
            } else if (rel(g->ch == CTLH) & 1) {
                if (rel((g->tcolumn = g->column) > 0) & 1) printnmac(' '); /* restore aft backsp */
                if ((rel(g->front > 1) & rel(g->tcolumn > g->scolumn)) & 1) {
                    if (rel(MEMORY(g->front - 1) != LF) & 1) { /* character can be eliminated */
                        decfront();
                        g->printsuppress = T;
                        /* backspace character accepted */
                        g->column = 0;
                        g->distance = 0;
                        g->direction = BACKWARD;
                        typelines();
                        g->printsuppress = F;
                        /* column position now reset */
                        if (rel((g->qcolumn = g->column) < g->scolumn) & 1) g->qcolumn = g->scolumn;
                        g->column = g->tcolumn; /* original value */
                        while (rel(g->column > g->qcolumn) & 1) backspace();
                        g->tcolumn = g->column;
                    }
                }
                g->ch = 0;
                g->column = g->tcolumn;
            } else if (rel(g->ch == RUBOUT) & 1) {
                if (rel(g->front == 1) & 1) go(L_RESET);
                decfront();
                printc(g->ch = MEMORY(g->front));
                if (rel(g->ch == LF) & 1) g->baseline = (uint16_t)(g->baseline - 1);
                g->ch = 0;
            } else { /* not a special case */
                if (~graphic(g->ch) & 1) {
                    printnmac('^');
                    printnmac((uint8_t)(g->ch + '@'));
                }
                if (rel(g->ch == CTLL) & 1) {
                    inscrlf();
                } else {
                    /* column count goes up if graphic; compute output
                     * column position */
                    if (rel(g->mp == 0) & 1) {
                        if (rel(g->ch >= ' ') & 1)
                            g->column = (uint8_t)(g->column + 1);
                        else if (rel(g->ch == TAB) & 1)
                            g->column = (uint8_t)(g->column + (8 - (g->column & 7)));
                    }
                    insert();
                }
            }
            if (rel(g->ch == LF) & 1) printnmbase();
            if (rel(g->ch == CR) & 1)
                printnmac(g->ch = LF);
            else
                g->ch = 0;
        }
    }
    if (rel(g->ch != ENDFILE) & 1) inscrlf(); /* must have stopped on cr */
    if ((g->inserting & g->lineset) & 1) crlf();
}

/* R: read from lib file */
static void read_command(void) {
    g->rbp = 1;
    setrdma();
    while (scanning() & 1) {
        if (rel(g->rbp > 8) & 1) go(L_OVERCOUNT);
        setrfcb();
    }
    g->ch = ' ';
    if ((g->flag = rel(g->rbp == 1)) & 1) { /* read from xfer file */
        move(8, ED_XFCB + 1, ED_RFCB + 1);
        if (g->xferon & 1) {
            XFCBC = g->xchksum; /* restore checksum */
            close(ED_XFCB);
        }
    } else { /* lib name specified */
        while (rel(g->rbp <= 8) & 1) setrfcb();
    }
    RFCB(12) = RFCB(32) = 0; /* fill reel, and next record */
    open(ED_RFCB);
    g->rbp = SECTSIZE;
    if (rel(g->dcnt == 255) & 1) {
        g->flag = 'O';
        go(L_RESET);
    }
    while (rel((g->ch = readfile()) != ENDFILE) & 1) insert();
    r_i = 0;
    if (g->flag & 1) /* may be xfer data in buffer */
        while (rel(r_i < g->xbp) & 1) {
            g->ch = XBUFF(r_i);
            r_i = (uint8_t)(r_i + 1);
            insert();
        }
}

/* the commands which allow only a preceding number: A F J M N S W X Z */
static void forward_command(void) {
    if (rel(g->ch == 'A') & 1) {
        g->direction = FORWARD;
        g->first = g->front;
        g->last = g->maxm;
        mover();
        /* all storage forward */
        if (distzero() & 1) apphalf();
        /* distance = 0 if apphalf called */
        while (distnzero() & 1) readline();
        g->direction = BACKWARD;
        mover();
        /* pointers repositioned */
    } else if (rel(g->ch == 'F') & 1) {
        setfind(); /* search string scanned and setup between 0 and wbp-1 in scratch */
        while (distnzero() & 1) chkfound();
    } else if (rel(g->ch == 'J') & 1) { /* juxtaposition operation */
        setfind();
        collect();
        g->wbj = g->wbe;
        collect();
        /* search for string 0 - wbp-1, insert string wbp to wbj-1, and
         * then delete up to string wbj to wbe-1 */
        while (distnzero() & 1) {
            chkfound();
            /* insert string */
            g->mi = (uint8_t)(g->wbp - 1);
            while (rel((g->mi = (uint8_t)(g->mi + 1)) < g->wbj) & 1) {
                g->ch = SCRATCH(g->mi);
                insert();
            }
            j_t = g->front; /* save position for delete */
            if (~find(g->wbj, g->wbe) & 1) go(L_OVERCOUNT);
            /* string found, so move it back */
            g->first = (uint16_t)(g->front - (uint8_t)(g->wbe - g->wbj));
            g->direction = BACKWARD;
            mover();
            /* now remove the intermediate string */
            g->front = j_t;
            /* forward again for the next find (find's mover): without
             * it nJ, n > 1, moved the text backward from the second
             * find on and garbled the line (a fix, in ED.PLM too) */
            g->direction = FORWARD;
        }
    } else if ((rel(g->ch == 'M') & rel(g->mp == 0)) & 1) { /* macro definition */
        g->xp = 255;
        if (rel(g->distance == 1) & 1) zerodist();
        while (rel((MACRO(g->xp = (uint8_t)(g->xp + 1)) = readc()) != CR) & 1) {}
        g->mp = g->xp;
        g->xp = 0;
        g->mt = g->distance;
    } else if (rel(g->ch == 'N') & 1) { /* search for string with autoscan */
        setfind();                      /* search string scanned */
        while (distnzero() & 1) {
            /* find another occurrence of string */
            while (~find(0, g->wbp) & 1) { /* not in buffer */
                if (break_key() & 1) go(L_RESET);
                savedist();
                clearmem();
                /* memory buffer written */
                apphalf();
                g->direction = BACKWARD;
                g->first = 1;
                mover();
                restdist();
                g->direction = FORWARD;
                /* may be end of file */
                if (rel(g->back >= g->maxm) & 1) go(L_OVERCOUNT);
            }
        }
    } else if (rel(g->ch == 'S') & 1) { /* substitute command */
        setfind();
        collect();
        /* find string from 0 to wbp-1, substitute string between wbp
         * and wbe-1 in scratch */
        while (distnzero() & 1) {
            chkfound();
            /* front and back now positioned at found string - replace it */
            g->front = (uint16_t)(g->front - (g->mi = g->wbp)); /* backed up */
            while (rel(g->mi < g->wbe) & 1) {
                g->ch = SCRATCH(g->mi);
                g->mi = (uint8_t)(g->mi + 1);
                insert();
            }
        }
    } else if (rel(g->ch == 'W') & 1) {
        writeout();
    } else if (rel(g->ch == 'X') & 1) { /* transfer lines */
        setxdma();
        if (distzero() & 1) { /* clear the file */
            xclear();
            delete(ED_XFCB);
            g->xchksum = XFCBC; /* save checksum */
        } else {              /* transfer lines */
            if (~g->xferon & 1) { /* create xfer file */
                xclear();
                g->xferon = T;
                delete(ED_XFCB); /* old version gone */
                make(ED_XFCB);
                g->xchksum = XFCBC; /* save checksum */
                if (rel(g->dcnt == 255) & 1) ferr();
            }
            setlimits();
            for (x_i = g->first; x_i <= g->last; x_i++) putxfer(MEMORY(x_i));
        }
    } else if (rel(g->ch == 'Z') & 1) { /* sleep */
        if (distzero() & 1) {
            if (rel(readchar() == ENDFILE) & 1) go(L_RESET);
        }
        while (distnzero() & 1) wait();
    } else if (rel(g->ch != 0) & 1) { /* not break left over from stop */
        /* direction forward, but not one of the above */
        go(L_BADCOM);
    }
}

/* may be a command which has an optional direction and distance */
static void distance_command(void) {
    /* scan a signed integer value (if any) */
    setforward();

    if (rel(g->ch == '-') & 1) {
        readctran();
        g->direction = BACKWARD;
    }

    if (rel(g->ch == POUND) & 1) {
        setff();
        readctran();
    } else if (digit() & 1) {
        number();
        /* may be absolute line reference */
        if (rel(g->ch == ':') & 1) {
            g->ch = 'L';
            reldistance();
        }
    } else if (rel(g->ch == ':') & 1) { /* leading colon */
        readctran();                    /* clear the colon */
        number();
        reldistance();
        if (rel(g->direction == FORWARD) & 1) g->distance = (uint16_t)(g->distance + 1);
    }

    if (distzero() & 1) g->direction = BACKWARD;
    /* direction and distance are now set */

    if (rel(g->ch == 'B') & 1) {
        g->direction = (uint8_t)(1 - g->direction);
        g->first = 1;
        g->last = g->maxm;
        mover();
    } else if (rel(g->ch == 'C') & 1) {
        setclimits();
        mover();
    } else if (rel(g->ch == 'D') & 1) {
        setclimits();
        setptrs(); /* sets back/front */
    } else if (rel(g->ch == 'K') & 1) {
        setlimits();
        setptrs();
    } else if (rel(g->ch == 'L') & 1) {
        movelines();
    } else if (rel(g->ch == 'P') & 1) { /* page mode print */
        if (distzero() & 1) {
            g->direction = FORWARD;
            setlpp();
            typelines();
        } else {
            while (distnzero() & 1) {
                page();
                wait();
            }
        }
    } else if (rel(g->ch == 'T') & 1) {
        typelines();
    } else if (rel(g->ch == 'U') & 1) {
        g->upper = rel(g->direction == FORWARD);
    } else if (rel(g->ch == 'V') & 1) {
        /* 0V displays buffer state */
        if (distzero() & 1) {
            printvalue((uint16_t)(g->back - g->front));
            printc('/');
            printvalue(g->maxm);
            crlf();
        } else {
            g->lineset = rel(g->direction == FORWARD);
        }
    } else if (rel(g->ch == CR) & 1) { /* may be move/type command */
        if ((rel(g->mi == 1) & rel(g->mp == 0)) & 1) { /* first command */
            movelines();
            setforward();
            typelines();
        }
    } else if ((rel(g->direction == FORWARD) | distzero()) & 1) {
        forward_command();
    } else { /* direction not forward */
        go(L_BADCOM);
    }
}

/* one pass of the main loop: read a command and do it */
static void command(void) {
    /* simple commands (cannot be preceded by direction/distance):
     * E H I O R Q */
    g->inserting = F;
    readctran();
    g->mi = g->cbp; /* save starting address for <cr> command */
    if (singlecom('E') & 1) {
        terminate();
        if (rel(g->sdisk != g->ddisk) & 1) /* change disks: user code in high nibble */
            bp[bp_bdisk] = (uint8_t)((bp[bp_bdisk] & 0xF0) | (g->ddisk & 0x0F));
        reboot();
    } else if (singlecom('H') & 1) { /* go to top */
        terminate();
        g->ch = g->ddisk;
        g->ddisk = g->sdisk;
        g->sdisk = g->ch;
        /* ping - pong disks */
        go(L_RESTART);
    } else if (rel(g->ch == 'I') & 1) { /* insert characters */
        insert_command();
    } else if (singlercom('O') & 1) { /* forget this edit */
        go(L_RESTART);
    } else if (rel(g->ch == 'R') & 1) {
        read_command();
    } else if (singlercom('Q') & 1) {
        delete(ED_DFCB);
        reboot();
    } else {
        distance_command();
    }
}

void ed_main(uint16_t ds) {
    rsp_install_transient(ds, &ed_image, ed_image_len);
    g_ds = ds;
    g = RSP_DATA(ed_data, ds);
    bp = far_ptr(rt_phys(), ds, 0);
    fcb = bp + bp_fcb;

    /* initialize the system */
    g->ver = version();
    if ((rel((g->ver & 0xFF) != CPMVERSION) | rel((g->ver >> 8) < MPMPRODUCT)) & 1) {
        print(ED_ARR437); /* Requires MP/M 2.0 */
        mon1(0, 0);
    }
    /* i/o buffer region is 1/8 available memory */
    g->nbuf = (uint8_t)(((g->max = (uint16_t)(rd16(bp + bp_maxb) - ED_MEMORY)) >> (SECTSHF + 3)) - 1);
    /* nbuf is number of buffers - 1 */
    g->bufflength = (uint16_t)((uint8_t)(g->nbuf + 1) << (SECTSHF + 1));
    /* now set max as remainder of free memory */
    if (rel((uint16_t)(g->bufflength + 1024) > g->max) & 1) {
        print(ED_ARR441); /* NO MEMORY */
        boot();
    }
    /* remove buffer space and 00 at end of memory vector */
    g->max = (uint16_t)(g->max - g->bufflength - 1);
    /* reset buffer length for i and o */
    g->bufflength = g->bufflength >> 1;
    g->sbuffadr = (uint16_t)(rd16(bp + bp_maxb) - g->bufflength);
    g->dbuffadr = (uint16_t)(g->sbuffadr - g->bufflength);
    MEMORY(g->max) = 0; /* stops match at end of buffer */
    g->maxm = (uint16_t)(g->max - 1);
    g->hmax = g->maxm >> 1;
    /* no translate, with line numbers */
    g->upper = g->printsuppress = F;
    g->lineset = T;
    /* get source and destination disks */
    if (rel(fcb[1] == ' ') & 1) abort_(ED_ARR445); /* Filename Required */
    if (rel((g->sdisk = fcb[0]) == 0) & 1) {
        g->sdisk = cselect();
    } else {
        g->sdisk = (uint8_t)(g->sdisk - 1);
        fcb[0] = 0; /* clear disk name */
    }
    if (rel((g->ddisk = fcb[16]) == 0) & 1)
        g->ddisk = g->sdisk;
    else
        g->ddisk = (uint8_t)(g->ddisk - 1);
    /* clear the xfer file */
    xclear();

    go_label = L_RESTART;
    (void)ED_SETJMP(); /* each go() comes back here, go_label its label */
    switch (go_label) {
    case L_RESTART:
        setup();
        MEMORY(0) = LF;
        g->front = 1;
        g->back = g->maxm;
        g->column = 0;
        break; /* GO TO START */
    case L_OVERCOUNT:
        g->flag = POUND;
        goto reset;
    case L_BADCOM:
        g->flag = WHAT;
        goto reset;
    case L_OVERFLOW: /* arrive here on overflow condition (i,f,s command) */
        g->flag = '>';
        goto reset;
    case L_RESET: /* arrive here on error condition */
    reset:
        g->printsuppress = F;
        print(ED_ARR452); /* BREAK " */
        printc(g->flag);
        printm(ED_ARR453); /* " AT */
        printc(g->ch);
        crlf();
        break;
    default: /* L_START */
        break;
    }
    /* START: */
    g->readbuff = T;
    g->mp = 0;
    for (;;) command(); /* or until the power is turned off */
}
