/*
 * show.c -- C version of src-assem/utils/SHOW.PLM (a transient,
 * SHOW.CMD): shows the free space and R/O status of each logged-in
 * drive (or of the one named), the active users and the users that
 * have files, the directory label and its stamps, and a drive's disk
 * parameter block, as SHOW [d:][SPACE | USERS | LABEL | DRIVE | HELP].
 *
 * Its data segment, below, is SHOW.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (the DATA tables and messages among them), the string
 * constants, and the stack; show_main installs the image from 100h on.
 * PL/M's semantics are kept as in dir.c: bytes wrap, a relation is 0FFh
 * or 0, IF tests bit 0 (SHOW's own `true` is 1), AND/OR evaluate both
 * sides, and the main program's variables are the PL/M globals. The
 * procedures' own variables live on PL/M's stack, here C locals; the
 * ones a nested procedure shares with its parent (drivestatus's b3a,
 * printbcd's zerosup and i, labelstatus's fcbp) are file statics.
 *
 * The free space is 16-, 24- and 32-bit arithmetic done PL/M's way: a
 * 3-byte record count (BDOS function 46's, or a block count shifted by
 * blkshf) divided by 8 for kilobytes, then turned into seven BCD digits
 * by getbcd and the 65,536 * 2**n factor table, and printed with
 * commas; words wrap at 16 bits, and divide/mod are unsigned.
 *
 * The DPB is reached through DPBPTR, the far pointer mon4 (function
 * 31) returns in ES:BX; the allocation vector (function 27, used only
 * under a pre-3.0 BDOS) through alloca, in DS, as the PL/M does.
 *
 * Not ported, because nothing calls them: open, writeprot, setind,
 * setuser, getfilesize, compare, scan (and its setacc), bcd$pair and
 * get$next$digit. tod$ASCII is ported whole, though SHOW only ever
 * calls it with opcode 3 (display$ts), so set$date$time and its
 * scanner are never reached.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb   0x5C
#define bp_buff  0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    char copyright[38], verdate[8], version[9];
    uint8_t user_code, cversion, cdisk;
    uint16_t dpbptr[2]; /* offset, segment */
    uint16_t alloca;
    uint8_t dcnt, anything;
    uint8_t dirbuf[128];
    uint16_t parse_fn[2]; /* buff$adr, fcb$adr */
    uint16_t tod_adr, string_adr;
    uint8_t index, chr;
    uint8_t month_size[12];
    uint16_t month_days[12];
    uint16_t word_value;
    uint8_t month, day, year, hrs, min, sec, week_day;
    char day_list[28];
    uint8_t leap_bias, date_test;
    uint16_t test_value;
    uint8_t lcltod[27]; /* opcode, date (2), hrs, min, sec, ASCII (21) */
    uint16_t datapgadr, extrnl_todadr, ret, bpb;
    uint8_t accum[4], ibp;
    uint8_t val[7], fac[7], f0[7], f1[7], f2[7], f3[7], f4[7], f5[7], f6[7];
    uint16_t ptr[7];
    char label1[44], label2[31], label3[40], label4[77];
    uint8_t i, last_dseg_byte;
    char arr211[3], arr224[9], arr225[22], arr226[25], arr227[25], arr228[27], arr229[27], arr230[26],
        arr231[16], arr232[16], arr233[17], arr240[45], arr241[45], arr242[45], arr243[45], arr244[35],
        arr261[10];
    char ufcb[15]; /* userstatus's ufcb */
    char arr274[14], arr275[14], arr291[18], arr295[20], arr299[20];
    char readlbl_d; /* readlbl's d */
    char arr318[17], arr322[7], arr323[7], arr327[10], arr328[10], arr334[8], arr335[8], arr341[9],
        arr342[9], arr346[7], arr347[7], arr348[5], arr349[3], arr350[26], arr367[18];
    uint8_t stack[1024];
    uint8_t stack_base;
} show_data;

_Static_assert(offsetof(show_data, copyright) == SHOW_COPYRIGHT, "copyright");
_Static_assert(offsetof(show_data, verdate) == SHOW_VERDATE, "verdate");
_Static_assert(offsetof(show_data, version) == SHOW_VERSION, "version");
_Static_assert(offsetof(show_data, user_code) == SHOW_USERCODE, "usercode");
_Static_assert(offsetof(show_data, cversion) == SHOW_CVERSION, "cversion");
_Static_assert(offsetof(show_data, cdisk) == SHOW_CDISK, "cdisk");
_Static_assert(offsetof(show_data, dpbptr) == SHOW_DPBPTR, "dpbptr");
_Static_assert(offsetof(show_data, alloca) == SHOW_ALLOCA, "alloca");
_Static_assert(offsetof(show_data, dcnt) == SHOW_DCNT, "dcnt");
_Static_assert(offsetof(show_data, anything) == SHOW_ANYTHING, "anything");
_Static_assert(offsetof(show_data, dirbuf) == SHOW_DIRBUF, "dirbuf");
_Static_assert(offsetof(show_data, parse_fn) == SHOW_PARSEFN, "parsefn");
_Static_assert(offsetof(show_data, tod_adr) == SHOW_TODADR, "todadr");
_Static_assert(offsetof(show_data, string_adr) == SHOW_STRINGADR, "stringadr");
_Static_assert(offsetof(show_data, index) == SHOW_INDEX, "index");
_Static_assert(offsetof(show_data, chr) == SHOW_CHR, "chr");
_Static_assert(offsetof(show_data, month_size) == SHOW_MONTHSIZE, "monthsize");
_Static_assert(offsetof(show_data, month_days) == SHOW_MONTHDAYS, "monthdays");
_Static_assert(offsetof(show_data, word_value) == SHOW_WORDVALUE, "wordvalue");
_Static_assert(offsetof(show_data, month) == SHOW_MONTH, "month");
_Static_assert(offsetof(show_data, day) == SHOW_DAY, "day");
_Static_assert(offsetof(show_data, year) == SHOW_YEAR, "year");
_Static_assert(offsetof(show_data, hrs) == SHOW_HRS, "hrs");
_Static_assert(offsetof(show_data, min) == SHOW_MIN, "min");
_Static_assert(offsetof(show_data, sec) == SHOW_SEC, "sec");
_Static_assert(offsetof(show_data, week_day) == SHOW_WEEKDAY, "weekday");
_Static_assert(offsetof(show_data, day_list) == SHOW_DAYLIST, "daylist");
_Static_assert(offsetof(show_data, leap_bias) == SHOW_LEAPBIAS, "leapbias");
_Static_assert(offsetof(show_data, date_test) == SHOW_DATETEST, "datetest");
_Static_assert(offsetof(show_data, test_value) == SHOW_TESTVALUE, "testvalue");
_Static_assert(offsetof(show_data, lcltod) == SHOW_LCLTOD, "lcltod");
_Static_assert(offsetof(show_data, datapgadr) == SHOW_DATAPGADR, "datapgadr");
_Static_assert(offsetof(show_data, extrnl_todadr) == SHOW_EXTRNLTODADR, "extrnltodadr");
_Static_assert(offsetof(show_data, ret) == SHOW_RET, "ret");
_Static_assert(offsetof(show_data, bpb) == SHOW_BPB, "bpb");
_Static_assert(offsetof(show_data, accum) == SHOW_ACCUM, "accum");
_Static_assert(offsetof(show_data, ibp) == SHOW_IBP, "ibp");
_Static_assert(offsetof(show_data, val) == SHOW_VAL, "val");
_Static_assert(offsetof(show_data, fac) == SHOW_FAC, "fac");
_Static_assert(offsetof(show_data, f0) == SHOW_F0, "f0");
_Static_assert(offsetof(show_data, f1) == SHOW_F1, "f1");
_Static_assert(offsetof(show_data, f2) == SHOW_F2, "f2");
_Static_assert(offsetof(show_data, f3) == SHOW_F3, "f3");
_Static_assert(offsetof(show_data, f4) == SHOW_F4, "f4");
_Static_assert(offsetof(show_data, f5) == SHOW_F5, "f5");
_Static_assert(offsetof(show_data, f6) == SHOW_F6, "f6");
_Static_assert(offsetof(show_data, ptr) == SHOW_PTR, "ptr");
_Static_assert(offsetof(show_data, label1) == SHOW_LABEL1, "label1");
_Static_assert(offsetof(show_data, label2) == SHOW_LABEL2, "label2");
_Static_assert(offsetof(show_data, label3) == SHOW_LABEL3, "label3");
_Static_assert(offsetof(show_data, label4) == SHOW_LABEL4, "label4");
_Static_assert(offsetof(show_data, i) == SHOW_I, "i");
_Static_assert(offsetof(show_data, last_dseg_byte) == SHOW_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(show_data, arr211) == SHOW_ARR211, "arr211");
_Static_assert(offsetof(show_data, arr224) == SHOW_ARR224, "arr224");
_Static_assert(offsetof(show_data, arr225) == SHOW_ARR225, "arr225");
_Static_assert(offsetof(show_data, arr226) == SHOW_ARR226, "arr226");
_Static_assert(offsetof(show_data, arr227) == SHOW_ARR227, "arr227");
_Static_assert(offsetof(show_data, arr228) == SHOW_ARR228, "arr228");
_Static_assert(offsetof(show_data, arr229) == SHOW_ARR229, "arr229");
_Static_assert(offsetof(show_data, arr230) == SHOW_ARR230, "arr230");
_Static_assert(offsetof(show_data, arr231) == SHOW_ARR231, "arr231");
_Static_assert(offsetof(show_data, arr232) == SHOW_ARR232, "arr232");
_Static_assert(offsetof(show_data, arr233) == SHOW_ARR233, "arr233");
_Static_assert(offsetof(show_data, arr240) == SHOW_ARR240, "arr240");
_Static_assert(offsetof(show_data, arr241) == SHOW_ARR241, "arr241");
_Static_assert(offsetof(show_data, arr242) == SHOW_ARR242, "arr242");
_Static_assert(offsetof(show_data, arr243) == SHOW_ARR243, "arr243");
_Static_assert(offsetof(show_data, arr244) == SHOW_ARR244, "arr244");
_Static_assert(offsetof(show_data, arr261) == SHOW_ARR261, "arr261");
_Static_assert(offsetof(show_data, ufcb) == SHOW_USERSTATUS_S_UFCB, "ufcb");
_Static_assert(offsetof(show_data, arr274) == SHOW_ARR274, "arr274");
_Static_assert(offsetof(show_data, arr275) == SHOW_ARR275, "arr275");
_Static_assert(offsetof(show_data, arr291) == SHOW_ARR291, "arr291");
_Static_assert(offsetof(show_data, arr295) == SHOW_ARR295, "arr295");
_Static_assert(offsetof(show_data, arr299) == SHOW_ARR299, "arr299");
_Static_assert(offsetof(show_data, readlbl_d) == SHOW_READLBL_S_D, "readlbl_d");
_Static_assert(offsetof(show_data, arr318) == SHOW_ARR318, "arr318");
_Static_assert(offsetof(show_data, arr322) == SHOW_ARR322, "arr322");
_Static_assert(offsetof(show_data, arr323) == SHOW_ARR323, "arr323");
_Static_assert(offsetof(show_data, arr327) == SHOW_ARR327, "arr327");
_Static_assert(offsetof(show_data, arr328) == SHOW_ARR328, "arr328");
_Static_assert(offsetof(show_data, arr334) == SHOW_ARR334, "arr334");
_Static_assert(offsetof(show_data, arr335) == SHOW_ARR335, "arr335");
_Static_assert(offsetof(show_data, arr341) == SHOW_ARR341, "arr341");
_Static_assert(offsetof(show_data, arr342) == SHOW_ARR342, "arr342");
_Static_assert(offsetof(show_data, arr346) == SHOW_ARR346, "arr346");
_Static_assert(offsetof(show_data, arr347) == SHOW_ARR347, "arr347");
_Static_assert(offsetof(show_data, arr348) == SHOW_ARR348, "arr348");
_Static_assert(offsetof(show_data, arr349) == SHOW_ARR349, "arr349");
_Static_assert(offsetof(show_data, arr350) == SHOW_ARR350, "arr350");
_Static_assert(offsetof(show_data, arr367) == SHOW_ARR367, "arr367");
_Static_assert(offsetof(show_data, stack_base) == SHOW_STACK_BASE, "stack_base");
_Static_assert(sizeof(show_data) == SHOW_DATA_LEN, "length");

#define T 1 /* SHOW's true */
#define F 0
#define CPMVERSION 0x20 /* requires 2.0 cp/m */
#define CPM3       0x30
#define CR 13
#define LF 10
#define SECTORLEN 128
#define BASE_YEAR 78 /* base year for computations */
#define BASE_DAY  0  /* starting day for base$year 0..6 */

/* the DPB's fields (dpb.spt ... dpb.ofs) */
#define dpb_spt 0
#define dpb_bls 2
#define dpb_bms 3
#define dpb_exm 4
#define dpb_mxa 5
#define dpb_dmx 7
#define dpb_cks 11
#define dpb_ofs 13

/* the tod structure's fields, at tod$adr (lcltod's layout) */
#define tod_opcode 0
#define tod_date   1
#define tod_hrs    3
#define tod_min    4
#define tod_sec    5
#define tod_text   6

const uint16_t show_image_len = sizeof(show_data);
const show_data show_image = {
    .copyright = " Copyright (c) 1981, Digital Research ",
    .verdate = "08/04/81",
    .version = "SHOW 2.0",
    .month_size = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    .month_days = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    .day_list = "Sun$Mon$Tue$Wed$Thu$Fri$Sat$",
    .val = {0, 0, 0, 0, 0, 0, 0},
    .fac = {0, 0, 0, 0, 0, 0, 0},
    .f0 = {6, 3, 5, 5, 6, 0, 0}, /*    65,536 */
    .f1 = {2, 7, 0, 1, 3, 1, 0}, /*   131,072 */
    .f2 = {4, 4, 1, 2, 6, 2, 0}, /*   262,144 */
    .f3 = {8, 8, 2, 4, 2, 5, 0}, /*   524,288 */
    .f4 = {6, 7, 5, 8, 4, 0, 1}, /* 1,048,576 */
    .f5 = {2, 5, 1, 7, 9, 0, 2}, /* 2,097,152 */
    .f6 = {4, 0, 3, 4, 9, 1, 4}, /* 4,194,304 */
    .ptr = {SHOW_F0, SHOW_F1, SHOW_F2, SHOW_F3, SHOW_F4, SHOW_F5, SHOW_F6},
    .label1 = "Directory     Passwds  Make   Stamp   Stamp",
    .label2 = "Label         Reqd     XFCBs  ",
    .label3 = "  Update  Label Created   Label Updated",
    .label4 = "------------  -------  -----  ------  ------  --------------  --------------",
    .i = 1,
    .last_dseg_byte = 0,
    .arr211 = ": ",
    .arr224 = "        ",
    .arr225 = "Drive Characteristics",
    .arr226 = "128 Byte Record Capacity",
    .arr227 = "Kilobyte Drive  Capacity",
    .arr228 = "32 Byte  Directory Entries",
    .arr229 = "Checked  Directory Entries",
    .arr230 = "Records / Directory Entry",
    .arr231 = "Records / Block",
    .arr232 = "Sectors / Track",
    .arr233 = "Reserved  Tracks",
    .arr240 = "Drive Status    : SHOW DRIVE:  SHOW d:DRIVE:",
    .arr241 = "User Status     : SHOW USERS:  SHOW d:USERS:",
    .arr242 = "Directory Label : SHOW LABEL:  SHOW d:LABEL:",
    .arr243 = "Free Disk Space : SHOW SPACE:  SHOW d:SPACE:",
    .arr244 = "Invalid Option, use the following:",
    .arr261 = ", Space: ",
    .ufcb = "????????????\0\0",
    .arr274 = "Active User :",
    .arr275 = "Active Files:",
    .arr291 = "Requires MP/M 2.0",
    .arr295 = "Not yet implemented",
    .arr299 = "Not yet implemented",
    .readlbl_d = '?',
    .arr318 = "Label for drive ",
    .arr322 = "Access",
    .arr323 = "Create",
    .arr327 = "    on   ",
    .arr328 = "    off  ",
    .arr334 = "   on  ",
    .arr335 = "   off ",
    .arr341 = "   on   ",
    .arr342 = "   off  ",
    .arr346 = "   on ",
    .arr347 = "   off",
    .arr348 = "    ",
    .arr349 = "  ",
    .arr350 = "No Directory Label exists",
    .arr367 = "Requires CP/M 2.0",
};

static uint16_t g_ds;
static show_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* the base page (DS:0), and its fcb */
static uint8_t *fcb = 0;

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* PL/M's shifts and rotates: SHL of a byte or a word drops what goes
 * past its top (8086 SHL by CL, which doesn't stop at 8 or 16), ROR/ROL
 * of a byte go round */
static uint8_t shl8(uint8_t v, uint8_t n) { return n >= 8 ? 0 : (uint8_t)(v << n); }
static uint16_t shl16(uint16_t v, uint8_t n) { return n >= 16 ? 0 : (uint16_t)(v << n); }
static uint8_t ror8(uint8_t v, uint8_t n) {
    n &= 7;
    return (uint8_t)((v >> n) | (v << ((8 - n) & 7)));
}
static uint8_t rol8(uint8_t v, uint8_t n) {
    n &= 7;
    return (uint8_t)((v << n) | (v >> ((8 - n) & 7)));
}

/* PL/M's move(count, source, destination), on DS addresses */
static void move(uint16_t n, uint16_t src, uint16_t dst) {
    while (n--) bp[dst++] = bp[src++];
}

/* the DPB, through DPBPTR (a far pointer) */
static uint8_t dpb8(uint16_t f) { return *far_ptr(rt_phys(), g->dpbptr[1], (uint16_t)(g->dpbptr[0] + f)); }
static uint16_t dpb16(uint16_t f) { return rd16(far_ptr(rt_phys(), g->dpbptr[1], (uint16_t)(g->dpbptr[0] + f))); }

static void printchar(uint8_t c) { mon1(2, c); }

/* print blank character */
static void printb(void) { printchar(' '); }

/* print the string at a until the next 0 */
static void printx(uint16_t a) {
    while (rel(bp[a] != 0) & 1) {
        printchar(bp[a]);
        a = (uint16_t)(a + 1);
    }
}

static uint8_t brk(void) { return mon2(11, 0); } /* console ready */

static void crlf(void) {
    printchar(CR);
    printchar(LF);
    if (brk() & 1) {
        mon1(1, 0); /* read character */
        mon1(0, 0); /* system reset */
    }
}

/* print the string starting at address a until the next 0 is
 * encountered */
static void print(uint16_t a) {
    crlf();
    printx(a);
}

static uint8_t get_version(void) { return mon2(12, 0); } /* current cp/m version # */
static void select(uint8_t d) { mon1(14, d); }

static void check_user(void) {
    for (;;) {
        if (g->anything & 1) return;
        if (rel(g->dcnt == 0xFF) & 1) return;
        if (rel(g->dirbuf[ror8(g->dcnt, 3) & 0x60] == g->user_code) & 1) return;
        g->dcnt = mon2(18, 0);
    }
}

static void search(uint16_t fcba) {
    g->anything = rel(bp[fcba] == '?');
    g->dcnt = mon2(17, fcba);
    check_user();
}

static void searchn(void) {
    g->dcnt = mon2(18, 0);
    check_user();
}

static uint8_t cselect(void) { return mon2(25, 0); } /* current disk number */
static void setdma(uint16_t dma) { mon1(26, dma); }
static uint16_t getalloca(void) { return mon3(27, 0); } /* base address of alloc vector */
static uint16_t getlogin(void) { return mon3(24, 0); }  /* the login vector */
static uint16_t getrodisk(void) { return mon3(29, 0); } /* the read-only disk vector */

/* set disk parameter block values: mon4, the DPB's far pointer in
 * ES:BX */
static void set_dpb(void) {
    mpm_regs r = rt_mpm(g_ds, 31, 0);
    g->dpbptr[0] = r.bx;
    g->dpbptr[1] = r.es;
}

static uint8_t getuser(void) { return mon2(32, 0xFF); } /* current user number */
static void getfreesp(uint8_t d) { mon1(46, d); }
static uint8_t getlbl(uint8_t d) { return mon2(101, d); }
static uint16_t parse(void) { return mon3(152, SHOW_PARSEFN); }
static void terminate(void) { mon1(0, 0); } /* system reset */

/* delimiter: the byte parse$fn.buff$adr points at */
static uint8_t delimiter(void) { return bp[g->parse_fn[0]]; }

/*
 * Time & Date ASCII Conversion Code
 */

/* string(index := index + 1) = c */
static void emitchar(uint8_t c) {
    bp[(uint16_t)(g->string_adr + (g->index = (uint8_t)(g->index + 1)))] = c;
}

static void emitn(uint16_t a) {
    while (rel(bp[a] != '$') & 1) {
        bp[(uint16_t)(g->string_adr + (g->index = (uint8_t)(g->index + 1)))] = bp[a];
        a = (uint16_t)(a + 1);
    }
}

static void emit_bcd(uint8_t b) { emitchar((uint8_t)('0' + b)); }

static void emit_bcd_pair(uint8_t b) {
    emit_bcd(b >> 4);
    emit_bcd(b & 0x0F);
}

static void emit_colon(uint8_t b) {
    emit_bcd_pair(b);
    emitchar(':');
}

static void emit_bin_pair(uint8_t b) {
    emit_bcd(b / 10); /* makes garbage if not < 10 */
    emit_bcd(b % 10);
}

static void emit_slant(uint8_t b) {
    emit_bin_pair(b);
    emitchar('/');
}

/* get next command byte */
static void gnc(void) {
    if (rel(g->chr == 0) & 1) return;
    if (rel(g->index == 20) & 1) {
        g->chr = 0;
        return;
    }
    g->chr = bp[(uint16_t)(g->string_adr + (g->index = (uint8_t)(g->index + 1)))];
}

static void deblank(void) {
    while (rel(g->chr == ' ') & 1) gnc();
}

/* test for numeric */
static uint8_t numeric(void) { return rel((uint8_t)(g->chr - '0') < 10); }

/* CARRY after a byte addition: PL/M-86 adds bytes as bytes, so it's
 * the carry out of bit 7 (set$date$time isn't reached) */
static uint8_t scan_numeric(uint8_t lb, uint8_t ub) {
    uint8_t b = 0;
    uint16_t sum;
    deblank();
    if ((uint8_t)~numeric() & 1) terminate();
    while (numeric() & 1) {
        if (rel((b & 0xE0) != 0) & 1) terminate();
        sum = (uint16_t)(shl8(b, 3) + shl8(b, 1)); /* b = b * 10 */
        b = (uint8_t)sum;
        if (sum > 0xFF) terminate(); /* carry */
        sum = (uint16_t)(b + (uint8_t)(g->chr - '0'));
        b = (uint8_t)sum;
        if (sum > 0xFF) terminate(); /* carry */
        gnc();
    }
    if ((rel(b < lb) | rel(b > ub)) & 1) terminate();
    return b;
}

static uint8_t scan_delimiter(uint8_t d, uint8_t lb, uint8_t ub) {
    deblank();
    if (rel(g->chr != d) & 1) terminate();
    gnc();
    return scan_numeric(lb, ub);
}

/* compute days accumulated by leap years */
static uint8_t leap_days(uint8_t y, uint8_t m) {
    uint8_t yp = y >> 2; /* yp = y/4 */
    if ((rel((y & 3) == 0) & rel(g->month_days[m] < 59)) & 1)
        /* y not 00, y mod 4 = 0, before march, so not leap yr */
        return (uint8_t)(yp - 1);
    /* otherwise, yp is the number of accumulated leap days */
    return yp;
}

static uint8_t bcd(uint8_t val) { return (uint8_t)(shl8(val / 10, 4) + val % 10); }

static void set_date_time(void) {
    uint8_t i, leap_flag; /* temporaries */
    uint8_t *tod = bp + g->tod_adr;
    g->month = (uint8_t)(scan_numeric(1, 12) - 1);
    /* may be feb 29 */
    if ((leap_flag = rel(g->month == 1)) & 1)
        i = 29;
    else
        i = g->month_size[g->month];
    g->day = scan_delimiter('/', 1, i);
    g->year = scan_delimiter('/', BASE_YEAR, 99);
    /* ensure that feb 29 is in a leap year */
    if ((leap_flag & rel(g->day == 29) & rel((g->year & 3) != 0)) & 1)
        /* feb 29 of non-leap year */ terminate();
    /* compute total days */
    wr16(tod + tod_date, (uint16_t)(g->month_days[g->month] + 365 * (uint8_t)(g->year - BASE_YEAR) + g->day -
                                    leap_days(BASE_YEAR, 0) + leap_days(g->year, g->month)));

    tod[tod_hrs] = bcd(scan_numeric(0, 23));
    tod[tod_min] = bcd(scan_delimiter(':', 0, 59));
    if (rel(tod[tod_opcode] == 2) & 1) {
        /* date, hours and minutes only */
        if (rel(g->chr == ':') & 1) i = scan_delimiter(':', 0, 59);
        tod[tod_sec] = 0;
    } else {
        /* include seconds */
        tod[tod_sec] = bcd(scan_delimiter(':', 0, 59));
    }
    (void)i;
}

/* compute year from number of days in word$value */
static void compute_year(void) {
    uint16_t year_length;
    g->year = BASE_YEAR;
    for (;;) {
        year_length = 365;
        if (rel((g->year & 3) == 0) & 1) year_length = 366; /* leap year */
        if (rel(g->word_value <= year_length) & 1) return;
        g->word_value = (uint16_t)(g->word_value - year_length);
        g->year = (uint8_t)(g->year + 1);
    }
}

static void compute_month(void) {
    g->month = 12;
    while (rel(g->month > 0) & 1) {
        if (rel((g->month = (uint8_t)(g->month - 1)) < 2) & 1) g->leap_bias = 0; /* jan or feb */
        if (rel((uint16_t)(g->month_days[g->month] + g->leap_bias) < g->word_value) & 1) return;
    }
}

/* get date and time */
static void get_date_time(void) {
    uint8_t *tod = bp + g->tod_adr;
    g->hrs = tod[tod_hrs];
    g->min = tod[tod_min];
    g->sec = tod[tod_sec];
    g->word_value = rd16(tod + tod_date);
    /* word$value contains total number of days */
    g->week_day = (uint8_t)((uint16_t)(g->word_value + BASE_DAY - 1) % 7);
    compute_year();
    /* year has been set, word$value is remainder */
    g->leap_bias = 0;
    if ((rel((g->year & 3) == 0) & rel(g->word_value > 59)) & 1)
        /* after feb 29 on leap year */ g->leap_bias = 1;
    compute_month();
    g->day = (uint8_t)(g->word_value - (uint16_t)(g->month_days[g->month] + g->leap_bias));
    g->month = (uint8_t)(g->month + 1);
}

static void emit_date_time(void) {
    uint8_t *tod = bp + g->tod_adr;
    if (rel(tod[tod_opcode] == 0) & 1) {
        emitn((uint16_t)(SHOW_DAYLIST + shl8(g->week_day, 2)));
        emitchar(' ');
    }
    emit_slant(g->month);
    emit_slant(g->day);
    emit_bin_pair(g->year);
    emitchar(' ');
    emit_colon(g->hrs);
    emit_colon(g->min);
    if (rel(tod[tod_opcode] == 0) & 1) emit_bcd_pair(g->sec);
}

static void tod_ascii(uint16_t parameter) {
    uint16_t ret = 0; /* tod$ASCII's own ret, not the global */
    uint8_t op;
    g->tod_adr = parameter;
    g->string_adr = (uint16_t)(g->tod_adr + tod_text);
    op = bp[g->tod_adr + tod_opcode];
    if ((rel(op == 0) | rel(op == 3)) & 1) {
        get_date_time();
        g->index = 0xFF; /* -1 */
        emit_date_time();
    } else if ((rel(op == 1) | rel(op == 2)) & 1) {
        g->chr = bp[(uint16_t)(g->string_adr + (g->index = 0))];
        set_date_time();
        ret = (uint16_t)(g->string_adr + g->index);
    } else {
        terminate();
    }
    (void)ret;
}

/*
 * TOD interface to SHOW
 */

/* display time and date stamp, no seconds */
static void display_ts(uint16_t tsadr) {
    g->lcltod[tod_opcode] = 3;
    move(4, tsadr, SHOW_LCLTOD + tod_date); /* don't copy seconds */
    tod_ascii(SHOW_LCLTOD);
    for (uint8_t i = 0; i <= 13; i++) printchar(g->lcltod[tod_text + i]);
}

/*
 * Basic routines
 */

static void set_bpb(void) {
    set_dpb(); /* disk parameters set */
    g->bpb = (uint16_t)(shl16(1, dpb8(dpb_bls)) * SECTORLEN);
}

/* select disk and set bpb */
static void select_disk(uint8_t d) {
    select(g->cdisk = d);
    set_bpb(); /* bytes per block */
}

/* return the ith bit of the alloc vector */
static uint8_t getalloc(uint16_t i) {
    return rol8(bp[(uint16_t)(g->alloca + (i >> 3))], (uint8_t)((i & 7) + 1));
}

/* fill string @ s for c bytes with f */
static void fill(uint16_t s, uint8_t f, uint8_t c) {
    while (rel((c = (uint8_t)(c - 1)) != 255) & 1) {
        bp[s] = f;
        s = (uint16_t)(s + 1);
    }
}

/*
 * Print a number
 */

/* print value v with precision prec (1,10,100,1000,10000) with leading
 * zero suppression if zerosup = true */
static void pdecimal(uint16_t v, uint16_t prec, uint8_t zerosup) {
    uint8_t d; /* current decimal digit */
    while (rel(prec != 0) & 1) {
        d = (uint8_t)(v / prec); /* get next digit */
        v = v % prec;            /* get remainder back to v */
        prec = prec / 10;        /* ready for next digit */
        if ((rel(prec != 0) & zerosup & rel(d == 0)) & 1) {
            printb();
        } else {
            zerosup = F;
            printchar((uint8_t)('0' + d));
        }
    }
}

/* BCD - convert 16 bit binary to 7 one byte BCD digits */
static void getbcd(uint16_t value) {
    uint16_t prec = 10000;
    uint8_t i = 5; /* digits: 4,3,2,1,0 */
    while (rel(prec != 0) & 1) {
        g->val[i = (uint8_t)(i - 1)] = (uint8_t)(value / prec); /* get next digit */
        value = value % prec;                                    /* remainder in value */
        prec = prec / 10;
    }
}

/* printbcd's variables (PL/M locals pchar shares) */
static uint8_t pb_zerosup, pb_i;

static void pchar(uint8_t c) {
    if (rel(g->val[pb_i] == 0) & 1)
        if (pb_zerosup & 1)
            if (rel(pb_i != 0) & 1) {
                printb();
                return;
            }
    /* else */
    printchar(c);
    pb_zerosup = F;
}

/* print BCD number in val array */
static void printbcd(void) {
    pb_zerosup = T;
    pb_i = 7;
    while (rel((pb_i = (uint8_t)(pb_i - 1)) != 0xFF) & 1) { /* <> -1 */
        pchar((uint8_t)('0' + g->val[pb_i]));
        if ((rel(pb_i == 6) | rel(pb_i == 3)) & 1) pchar(',');
    }
}

/* add two BCD numbers result in second */
static void add(uint16_t ap, uint16_t bpp) {
    uint8_t *a = bp + ap, *b = bp + bpp;
    uint8_t c = 0; /* carry */
    for (uint8_t i = 0; i <= 6; i++) { /* 0 = LSB */
        b[i] = (uint8_t)(a[i] + b[i] + c);
        c = b[i] / 10;
        b[i] = b[i] % 10;
    }
}

/* print 3 byte value based at byte3adr (lword, hbyte) */
static void p3byte(uint16_t byte3adr) {
    uint8_t high_byte;
    fill(SHOW_VAL, 0, 7);
    fill(SHOW_FAC, 0, 7);
    getbcd(rd16(bp + byte3adr)); /* put 16 bit value in val */
    high_byte = bp[(uint16_t)(byte3adr + 2)];
    for (uint8_t i = 0; i <= 6; i++) {             /* factor for bit i */
        if (high_byte & 1) add(g->ptr[i], SHOW_FAC); /* LSB is 1: add in factor */
        high_byte = high_byte >> 1;                  /* get next bit */
    }
    add(SHOW_FAC, SHOW_VAL); /* add factor to value */
    printbcd();              /* print value */
}

/* divide 3 byte value by 8 */
static void shr3byte(uint16_t byte3adr) {
    uint8_t *b3 = bp + byte3adr;
    uint8_t temp2 = ror8(b3[2], 3) & 0xE0; /* get 3 bits */
    b3[2] = b3[2] >> 3;
    wr16(b3, rd16(b3) >> 3);
    b3[1] = b3[1] | temp2; /* or in 3 bits from hbyte */
}

/* multiply 3 byte value by #records per block */
static void shl3byte(uint16_t byte3adr) {
    uint8_t *b3 = bp + byte3adr;
    b3[2] = (uint8_t)((rol8(b3[1], dpb8(dpb_bls)) & dpb8(dpb_bms)) | shl8(b3[2], dpb8(dpb_bls)));
    wr16(b3, shl16(rd16(b3), dpb8(dpb_bls)));
}

static void show_drive(void) {
    printchar((uint8_t)(g->cdisk + 'A'));
    printx(SHOW_ARR211); /* ": " */
}

/*
 * Calculate size
 */

/* add one block to the kilobyte accumulator */
static void add_block(uint16_t *kaccum, uint16_t *baccum) {
    *baccum = (uint16_t)(*baccum + g->bpb);
    while (rel(*baccum >= 1024) & 1) {
        *baccum = (uint16_t)(*baccum - 1024);
        *kaccum = (uint16_t)(*kaccum + 1);
    }
}

/* count kb remaining (mode true: counting 0's) */
static uint16_t count(uint8_t mode) {
    uint16_t ka = 0, ba = 0; /* kb and byte accumulators */
    uint8_t bit = 0;         /* always 1 if mode = false */
    for (uint16_t i = 0; i <= dpb16(dpb_mxa); i++) {
        if (mode & 1) bit = getalloc(i);
        if ((uint8_t)~bit & 1) add_block(&ka, &ba);
    }
    return ka;
}

/*
 * Status routines
 */

/* drivestatus's b3a (a PL/M local pv and pv3 share): a 3-byte value,
 * lword and hbyte */
static uint16_t ds_b3a;

/* print 3 byte value */
static void pv3(void) {
    crlf();
    p3byte(SHOW_DIRBUF);
    printchar(':');
    printb();
}

/* print address value v */
static void pv(uint16_t v) {
    bp[(uint16_t)(ds_b3a + 2)] = 0;
    wr16(bp + ds_b3a, v);
    pv3();
}

/* print the characteristics of the currently selected drive */
static void drivestatus(void) {
    ds_b3a = SHOW_DIRBUF;
    print(SHOW_ARR224); /* 8 blanks */
    show_drive();
    printx(SHOW_ARR225); /* Drive Characteristics */
    bp[(uint16_t)(ds_b3a + 2)] = 0;
    wr16(bp + ds_b3a, (uint16_t)(dpb16(dpb_mxa) + 1)); /* = # blocks */
    shl3byte(SHOW_DIRBUF);                             /* # blocks * records/block */
    pv3();
    printx(SHOW_ARR226);   /* 128 Byte Record Capacity */
    shr3byte(SHOW_DIRBUF); /* divide by 8 */
    pv3();
    printx(SHOW_ARR227); /* Kilobyte Drive  Capacity */
    pv((uint16_t)(dpb16(dpb_dmx) + 1));
    printx(SHOW_ARR228); /* 32 Byte  Directory Entries */
    pv(shl16(dpb16(dpb_cks), 2));
    printx(SHOW_ARR229); /* Checked  Directory Entries */
    pv((uint16_t)((uint8_t)(dpb8(dpb_exm) + 1) * 128));
    printx(SHOW_ARR230); /* Records / Directory Entry */
    pv(shl16(1, dpb8(dpb_bls)));
    printx(SHOW_ARR231); /* Records / Block */
    pv(dpb16(dpb_spt));
    printx(SHOW_ARR232); /* Sectors / Track */
    pv(dpb16(dpb_ofs));
    printx(SHOW_ARR233); /* Reserved  Tracks */
    crlf();
}

/* characteristics of all logged in disks */
static void diskstatus(void) {
    uint16_t login = getlogin(); /* login vector set */
    uint8_t d = 0;
    while (rel(login != 0) & 1) {
        if (login & 1) {
            select_disk(d);
            drivestatus();
        }
        login = login >> 1;
        d = (uint8_t)(d + 1);
    }
}

/* help message: display possible commands */
static void help(void) {
    print(SHOW_ARR240); /* Drive Status    : ... */
    print(SHOW_ARR241); /* User Status     : ... */
    print(SHOW_ARR242); /* Directory Label : ... */
    print(SHOW_ARR243); /* Free Disk Space : ... */
    crlf();
}

/* parse error message */
static void parse_error(void) {
    print(SHOW_VERSION);
    crlf();
    print(SHOW_ARR244); /* Invalid Option, use the following: */
    crlf();
    help();
    terminate();
}

/*
 * Disk status
 */

static void pvalue(uint16_t v) {
    uint8_t d, zero;
    uint16_t k = 10000;
    zero = F;
    while (rel(k != 0) & 1) {
        d = (uint8_t)(v / k);
        v = v % k;
        k = k / 10;
        if ((zero | rel(k == 0) | rel(d != 0)) & 1) {
            zero = T;
            printchar((uint8_t)('0' + d));
        }
    }
}

/* print the actual byte count */
static void prcount(void) {
    if (rel(g->cversion < CPM3) & 1) {
        g->alloca = getalloca();
        pvalue(count(T));
    } else {
        setdma(SHOW_DIRBUF);
        getfreesp(g->cdisk);
        shr3byte(SHOW_DIRBUF);
        p3byte(SHOW_DIRBUF);
    }
    printchar('k');
}

static void stat(uint16_t rodisk) {
    crlf();
    show_drive();
    printchar('R');
    if (rodisk & 1)
        printchar('O');
    else
        printchar('W');
    printx(SHOW_ARR261); /* , Space: */
    prcount();
}

/* print the status of the disk system */
static void prstatus(void) {
    uint16_t login = getlogin();   /* login vector set */
    uint16_t rodisk = getrodisk(); /* read only disk vector set */
    uint8_t d = 0;
    while (rel(login != 0) & 1) {
        if (login & 1) {
            if (rel(fcb[0] != 0) & 1) {
                if (rel((uint8_t)(fcb[0] - 1) == d) & 1) stat(rodisk); /* do specific disk */
            } else {
                select_disk(d);
                stat(rodisk); /* do all disks */
            }
        }
        login = login >> 1;
        rodisk = rodisk >> 1;
        d = (uint8_t)(d + 1);
    }
}

/*
 * User status
 */

/* display active user numbers */
static void userstatus(void) {
    uint8_t i;
    uint8_t user[16];
    crlf();
    show_drive();
    printx(SHOW_ARR274); /* Active User : */
    pdecimal(getuser(), 100, T);
    crlf();
    show_drive();
    printx(SHOW_ARR275); /* Active Files: */
    for (i = 0; i <= 15; i++) user[i] = F;
    setdma(SHOW_DIRBUF);
    search(SHOW_USERSTATUS_S_UFCB);
    while (rel(g->dcnt != 255) & 1) {
        if (rel((i = g->dirbuf[shl8(g->dcnt & 3, 5)]) != 0xE5) & 1) user[i & 0x0F] = T;
        searchn();
    }
    for (i = 0; i <= 15; i++)
        if (user[i] & 1) pdecimal(i, 100, T);
}

/*
 * MP/M II disk & file status
 */

static void versionerr(void) {
    print(SHOW_ARR291); /* Requires MP/M 2.0 */
    terminate();
}

static void openfiles(void) {
    if (rel(g->cversion < CPM3) & 1) versionerr();
    print(SHOW_ARR295); /* Not yet implemented */
}

static void lockedstatus(void) {
    if (rel(g->cversion < CPM3) & 1) versionerr();
    print(SHOW_ARR299); /* Not yet implemented */
}

/*
 * Label status
 */

/* find the label: a directory entry whose first byte is 20h */
static void readlbl(void) {
    setdma(SHOW_DIRBUF);
    search(SHOW_READLBL_S_D);
    while (rel(g->dcnt != 0xFF) & 1) {
        if (rel(g->dirbuf[ror8(g->dcnt, 3) & 0x60] == 0x20) & 1) return;
        searchn();
    }
}

/* labelstatus's fcbp (a PL/M local printfn shares): a template over
 * dirbuf */
static uint16_t ls_fcbp;

/* print file name */
static void printfn(void) {
    for (uint8_t k = 1; k <= 11; k++) {
        if (rel(k == 9) & 1) printchar('.');
        printchar(bp[(uint16_t)(ls_fcbp + k)] & 0x7F);
    }
}

static void labelstatus(void) {
    uint8_t lbl, make;
    if (rel(g->cversion < CPM3) & 1) versionerr();
    lbl = getlbl(g->cdisk);
    if (rel(lbl > 0) & 1) {
        readlbl();
        ls_fcbp = (uint16_t)(shl8(g->dcnt, 5) + SHOW_DIRBUF);

        /* print heading */
        print(SHOW_ARR318); /* Label for drive */
        show_drive();
        crlf();
        print(SHOW_LABEL1);
        print(SHOW_LABEL2);
        if (rel((lbl & 0x40) == 0x40) & 1)
            printx(SHOW_ARR322); /* Access */
        else
            printx(SHOW_ARR323); /* Create */
        printx(SHOW_LABEL3);
        print(SHOW_LABEL4);
        crlf();
        printfn();
        if (rel((lbl & 0x80) == 0x80) & 1)
            printx(SHOW_ARR327); /* on */
        else
            printx(SHOW_ARR328); /* off */

        if ((make = rel((lbl & 0x10) == 0x10)) & 1)
            printx(SHOW_ARR334);
        else
            printx(SHOW_ARR335);
        if ((rel((lbl & 0x40) == 0x40) | make) & 1)
            printx(SHOW_ARR341);
        else
            printx(SHOW_ARR342);
        if (rel((lbl & 0x20) == 0x20) & 1)
            printx(SHOW_ARR346);
        else
            printx(SHOW_ARR347);

        printx(SHOW_ARR348);
        display_ts((uint16_t)(ls_fcbp + 24));
        printx(SHOW_ARR349);
        display_ts((uint16_t)(ls_fcbp + 28));
    } else {
        print(SHOW_ARR350); /* No Directory Label exists */
    }
    crlf();
}

/*
 * Parsing
 */

static void parse_next(void) {
    /* skip comma or space delimiter */
    g->parse_fn[0] = (uint16_t)(g->parse_fn[0] + 1);
    g->parse_fn[0] = parse();
    if (rel(g->parse_fn[0] == 0xFFFF) & 1) parse_error();
    if ((rel(delimiter() == ']') | rel(delimiter() == ':')) & 1) /* skip */
        g->parse_fn[0] = (uint16_t)(g->parse_fn[0] + 1);
    if (rel(delimiter() == 0) & 1) g->parse_fn[0] = 0;
}

/*
 * Main program
 */

void show_main(uint16_t ds) {
    rsp_install_transient(ds, &show_image, show_image_len);
    g_ds = ds;
    g = RSP_DATA(show_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;

    /* process request */
    g->cversion = get_version();
    g->ibp = 1;
    if (rel(g->cversion < CPMVERSION) & 1) {
        printx(SHOW_ARR367); /* Requires CP/M 2.0 */
    } else {
        /* scan for global option */
        while (rel(bp[(uint16_t)(bp_buff + g->i)] == ' ') & 1) g->i = (uint8_t)(g->i + 1);
        if (rel(bp[(uint16_t)(bp_buff + g->i)] == '[') & 1) /* skip leading [ */
            g->parse_fn[0] = (uint16_t)(bp_buff + g->i);
        else
            g->parse_fn[0] = bp_buff;
        g->parse_fn[1] = bp_fcb;
        g->cdisk = cselect();
        g->user_code = getuser();
        while (rel(g->parse_fn[0] != 0) & 1) {
            parse_next();
            if (rel(fcb[0] != 0) & 1) /* get drive */
                select_disk((uint8_t)(fcb[0] - 1));
            if (rel(delimiter() == '[') & 1) parse_next(); /* get option */
            if ((rel(fcb[1] == ' ') | rel(fcb[1] == 'S')) & 1) {
                prstatus();
            } else if (rel(fcb[1] == 'U') & 1) {
                userstatus();
            } else if (rel(fcb[1] == 'H') & 1) {
                help();
            } else if (rel(fcb[1] == 'D') & 1) {
                if (rel(fcb[0] != 0) & 1)
                    drivestatus();
                else
                    diskstatus();
            } else if (rel(fcb[1] == 'O') & 1) {
                openfiles();
            } else if (rel(fcb[1] == 'L') & 1) {
                if (rel(fcb[2] == 'A') & 1)
                    labelstatus();
                else if (rel(fcb[2] == 'O') & 1)
                    lockedstatus();
                else
                    parse_error();
            } else {
                parse_error();
            }
        }
    }
    terminate();
}
