/*
 * pip.c -- C version of src-assem/utils/PIP.PLM (a transient, PIP.CMD),
 * the Peripheral Interchange Program: copies files to files (fast, a
 * buffer at a time, or a character at a time when a parameter needs
 * it), several files at once (A:=B:*.*), concatenations (A=B,C), and to
 * and from devices (CON:, LST:, PRN:, EOF:, NUL:), with the parameters
 * in brackets ([V] verify, [N] number lines, [T8] tabs, [U]/[L] case,
 * [Ssss^Z]/[Qqqq^Z] start and quit strings, [Gn] the source's user,
 * [A] archive, ...). With no command tail it prompts with * for one
 * command a line until an empty one.
 *
 * Its data segment, below, is PIP.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (ERROR's message tables, DATA, among them), the string
 * constants, the compiler's main_bp and the stack; pip_main installs
 * the image from 100h on. PL/M's semantics are kept as in dir.c (PIP's
 * own `true` is 1): bytes wrap, a relation is 0FFh or 0, IF tests bit
 * 0, AND and OR evaluate both sides (so MULTCOPY's `not archiv or
 * archck` always runs archck), the main program's variables are the
 * PL/M globals, and procedures' own variables are C locals -- except
 * where a nested procedure shares its parent's (file statics), and
 * MULTCOPY's counters, which must outlive an error retry (PL/M's
 * locals are static).
 *
 * The copy buffers are past the image: DBUFF is at .MEMORY (the end of
 * the data, PIP_MEMORY) and SBUFF is BASED SBASE, sized from MAXB (the
 * base page's data group length, which LOAD sets from the CMD header's
 * maximum). The C reaches them as the PL/M does, as DS offsets.
 *
 * ERROR ends with GO TO RETRY, a jump out of however many procedures
 * into the main program: here a longjmp back to pip_main's retry point
 * (setjmp on the host, GCC's __builtin_setjmp in the freestanding
 * kernel build, as bdosif.c does). Everything pip_main uses after it is
 * in the image, so nothing is lost across the jump.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

#if __STDC_HOSTED__
#include <setjmp.h>
static jmp_buf retry_point;
#define PIP_SETJMP()  setjmp(retry_point)
#define PIP_LONGJMP() longjmp(retry_point, 1)
#else
static void *retry_point[5];
#define PIP_SETJMP()  __builtin_setjmp(retry_point)
#define PIP_LONGJMP() __builtin_longjmp(retry_point, 1)
#endif

/* the base page (the PL/M interface stub's names) */
#define bp_maxb  0x06 /* MAXB: the data group's length */
#define bp_fcb   0x5C /* FCB, SEARFCB */
#define bp_buff  0x80 /* BUFF */

/* a file's fcb, password and password mode, and user (source, dest,
 * odest; SCAN's fcbs) */
typedef struct RSP_PACKED {
    uint8_t fcb[36]; /* FRSIZE: a random fcb */
    uint8_t pwnam[8];
    uint8_t pwmode;
    uint8_t user;
} pip_fcbs;

#define FCBS_PWNAM  36
#define FCBS_PWMODE 44
#define FCBS_USER   45

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    char copyright[33];
    uint8_t column, lineno, ambig, feedbase, feedlen, matchlen, quitlen, nbuf, cdisk;
    uint16_t sblen, dblen, tblen, sbase;
    uint8_t scom, dhex;
    pip_fcbs source, dest, odest;
    uint8_t hsource;
    uint8_t hbuff[80];
    uint8_t dxfcb[33];
    uint16_t bufsize, nsource, hardeof, ndest;
    uint8_t pdest, psource;
    uint8_t getpw, fastcopy, multcom, putnum, concnt, ch, type, flen;
    uint8_t f1, f2, f3, f4, ro, sys, dcnt, exten, odcnt, eretry;
    uint8_t cbuff[130]; /* maxlen, comlen, combuff(128) */
    uint8_t cbp, cuser, anything, last_user;
    uint8_t cont[26];
    uint8_t zerosup, c3, c2, c1;
    uint8_t copying;
    /* ERROR's messages (DATA, static) */
    char er00[10], er01[11], er02[7], er03[20], er04[15], er05[13], er06[14], er07[20], er08[15], er09[10],
        er10[15], er11[16], er12[15], er13[10], er14[11], er15[13], er16[18], er17[19], er18[23];
    uint16_t errmsg[19];
    char ex00[1], ex01[15], ex02[9], ex03[9], ex04[20], ex05[18], ex06[13], ex07[17], ex08[15], ex09[17],
        ex10[15], ex11[29];
    uint16_t extmsg[12];
    char arr21[9], arr25[4], arr211[18];
    char io[64];   /* SCAN's device names */
    uint8_t del[12]; /* DELIMITER's */
    char arr486[16], arr493[14], arr500[20], arr521[15], arr542[36], arr549[4], arr550[3], arr609[4],
        arr631[34], arr635[16], arr737[10], arr760[24], arr764[17];
    uint16_t main_bp; /* the compiler's: the main program's BP, for GO TO RETRY */
    uint8_t stack[1024];
    uint8_t stack_base;
} pip_data;

_Static_assert(offsetof(pip_data, copyright) == PIP_COPYRIGHT, "copyright");
_Static_assert(offsetof(pip_data, column) == PIP_COLUMN, "column");
_Static_assert(offsetof(pip_data, lineno) == PIP_LINENO, "lineno");
_Static_assert(offsetof(pip_data, ambig) == PIP_AMBIG, "ambig");
_Static_assert(offsetof(pip_data, feedbase) == PIP_FEEDBASE, "feedbase");
_Static_assert(offsetof(pip_data, feedlen) == PIP_FEEDLEN, "feedlen");
_Static_assert(offsetof(pip_data, matchlen) == PIP_MATCHLEN, "matchlen");
_Static_assert(offsetof(pip_data, quitlen) == PIP_QUITLEN, "quitlen");
_Static_assert(offsetof(pip_data, nbuf) == PIP_NBUF, "nbuf");
_Static_assert(offsetof(pip_data, cdisk) == PIP_CDISK, "cdisk");
_Static_assert(offsetof(pip_data, sblen) == PIP_SBLEN, "sblen");
_Static_assert(offsetof(pip_data, dblen) == PIP_DBLEN, "dblen");
_Static_assert(offsetof(pip_data, tblen) == PIP_TBLEN, "tblen");
_Static_assert(offsetof(pip_data, sbase) == PIP_SBASE, "sbase");
_Static_assert(offsetof(pip_data, scom) == PIP_SCOM, "scom");
_Static_assert(offsetof(pip_data, dhex) == PIP_DHEX, "dhex");
_Static_assert(offsetof(pip_data, source) == PIP_SOURCE, "source");
_Static_assert(offsetof(pip_data, dest) == PIP_DEST, "dest");
_Static_assert(offsetof(pip_data, odest) == PIP_ODEST, "odest");
_Static_assert(offsetof(pip_data, hsource) == PIP_HSOURCE, "hsource");
_Static_assert(offsetof(pip_data, hbuff) == PIP_HBUFF, "hbuff");
_Static_assert(offsetof(pip_data, dxfcb) == PIP_DXFCB, "dxfcb");
_Static_assert(offsetof(pip_data, bufsize) == PIP_BUFSIZE, "bufsize");
_Static_assert(offsetof(pip_data, nsource) == PIP_NSOURCE, "nsource");
_Static_assert(offsetof(pip_data, hardeof) == PIP_HARDEOF, "hardeof");
_Static_assert(offsetof(pip_data, ndest) == PIP_NDEST, "ndest");
_Static_assert(offsetof(pip_data, pdest) == PIP_PDEST, "pdest");
_Static_assert(offsetof(pip_data, psource) == PIP_PSOURCE, "psource");
_Static_assert(offsetof(pip_data, getpw) == PIP_GETPW, "getpw");
_Static_assert(offsetof(pip_data, fastcopy) == PIP_FASTCOPY, "fastcopy");
_Static_assert(offsetof(pip_data, multcom) == PIP_MULTCOM, "multcom");
_Static_assert(offsetof(pip_data, putnum) == PIP_PUTNUM, "putnum");
_Static_assert(offsetof(pip_data, concnt) == PIP_CONCNT, "concnt");
_Static_assert(offsetof(pip_data, ch) == PIP_CHAR, "char");
_Static_assert(offsetof(pip_data, type) == PIP_TYPE, "type");
_Static_assert(offsetof(pip_data, flen) == PIP_FLEN, "flen");
_Static_assert(offsetof(pip_data, f1) == PIP_F1, "f1");
_Static_assert(offsetof(pip_data, f2) == PIP_F2, "f2");
_Static_assert(offsetof(pip_data, f3) == PIP_F3, "f3");
_Static_assert(offsetof(pip_data, f4) == PIP_F4, "f4");
_Static_assert(offsetof(pip_data, ro) == PIP_RO, "ro");
_Static_assert(offsetof(pip_data, sys) == PIP_SYS, "sys");
_Static_assert(offsetof(pip_data, dcnt) == PIP_DCNT, "dcnt");
_Static_assert(offsetof(pip_data, exten) == PIP_EXTEN, "exten");
_Static_assert(offsetof(pip_data, odcnt) == PIP_ODCNT, "odcnt");
_Static_assert(offsetof(pip_data, eretry) == PIP_ERETRY, "eretry");
_Static_assert(offsetof(pip_data, cbuff) == PIP_CBUFF, "cbuff");
_Static_assert(offsetof(pip_data, cbp) == PIP_CBP, "cbp");
_Static_assert(offsetof(pip_data, cuser) == PIP_CUSER, "cuser");
_Static_assert(offsetof(pip_data, anything) == PIP_ANYTHING, "anything");
_Static_assert(offsetof(pip_data, last_user) == PIP_LASTUSER, "lastuser");
_Static_assert(offsetof(pip_data, cont) == PIP_CONT, "cont");
_Static_assert(offsetof(pip_data, zerosup) == PIP_ZEROSUP, "zerosup");
_Static_assert(offsetof(pip_data, c3) == PIP_C3, "c3");
_Static_assert(offsetof(pip_data, c2) == PIP_C2, "c2");
_Static_assert(offsetof(pip_data, c1) == PIP_C1, "c1");
_Static_assert(offsetof(pip_data, copying) == PIP_COPYING, "copying");
_Static_assert(offsetof(pip_data, er00) == PIP_ERROR_S_ER00, "er00");
_Static_assert(offsetof(pip_data, er01) == PIP_ERROR_S_ER01, "er01");
_Static_assert(offsetof(pip_data, er02) == PIP_ERROR_S_ER02, "er02");
_Static_assert(offsetof(pip_data, er03) == PIP_ERROR_S_ER03, "er03");
_Static_assert(offsetof(pip_data, er04) == PIP_ERROR_S_ER04, "er04");
_Static_assert(offsetof(pip_data, er05) == PIP_ERROR_S_ER05, "er05");
_Static_assert(offsetof(pip_data, er06) == PIP_ERROR_S_ER06, "er06");
_Static_assert(offsetof(pip_data, er07) == PIP_ERROR_S_ER07, "er07");
_Static_assert(offsetof(pip_data, er08) == PIP_ERROR_S_ER08, "er08");
_Static_assert(offsetof(pip_data, er09) == PIP_ERROR_S_ER09, "er09");
_Static_assert(offsetof(pip_data, er10) == PIP_ERROR_S_ER10, "er10");
_Static_assert(offsetof(pip_data, er11) == PIP_ERROR_S_ER11, "er11");
_Static_assert(offsetof(pip_data, er12) == PIP_ERROR_S_ER12, "er12");
_Static_assert(offsetof(pip_data, er13) == PIP_ERROR_S_ER13, "er13");
_Static_assert(offsetof(pip_data, er14) == PIP_ERROR_S_ER14, "er14");
_Static_assert(offsetof(pip_data, er15) == PIP_ERROR_S_ER15, "er15");
_Static_assert(offsetof(pip_data, er16) == PIP_ERROR_S_ER16, "er16");
_Static_assert(offsetof(pip_data, er17) == PIP_ERROR_S_ER17, "er17");
_Static_assert(offsetof(pip_data, er18) == PIP_ERROR_S_ER18, "er18");
_Static_assert(offsetof(pip_data, errmsg) == PIP_ERROR_S_ERRMSG, "errmsg");
_Static_assert(offsetof(pip_data, ex00) == PIP_ERROR_S_EX00, "ex00");
_Static_assert(offsetof(pip_data, ex01) == PIP_ERROR_S_EX01, "ex01");
_Static_assert(offsetof(pip_data, ex02) == PIP_ERROR_S_EX02, "ex02");
_Static_assert(offsetof(pip_data, ex03) == PIP_ERROR_S_EX03, "ex03");
_Static_assert(offsetof(pip_data, ex04) == PIP_ERROR_S_EX04, "ex04");
_Static_assert(offsetof(pip_data, ex05) == PIP_ERROR_S_EX05, "ex05");
_Static_assert(offsetof(pip_data, ex06) == PIP_ERROR_S_EX06, "ex06");
_Static_assert(offsetof(pip_data, ex07) == PIP_ERROR_S_EX07, "ex07");
_Static_assert(offsetof(pip_data, ex08) == PIP_ERROR_S_EX08, "ex08");
_Static_assert(offsetof(pip_data, ex09) == PIP_ERROR_S_EX09, "ex09");
_Static_assert(offsetof(pip_data, ex10) == PIP_ERROR_S_EX10, "ex10");
_Static_assert(offsetof(pip_data, ex11) == PIP_ERROR_S_EX11, "ex11");
_Static_assert(offsetof(pip_data, extmsg) == PIP_ERROR_S_EXTMSG, "extmsg");
_Static_assert(offsetof(pip_data, arr21) == PIP_ARR21, "arr21");
_Static_assert(offsetof(pip_data, arr25) == PIP_ARR25, "arr25");
_Static_assert(offsetof(pip_data, arr211) == PIP_ARR211, "arr211");
_Static_assert(offsetof(pip_data, io) == PIP_SCAN_S_IO, "io");
_Static_assert(offsetof(pip_data, del) == PIP_DELIMITER_S_DEL, "del");
_Static_assert(offsetof(pip_data, arr486) == PIP_ARR486, "arr486");
_Static_assert(offsetof(pip_data, arr493) == PIP_ARR493, "arr493");
_Static_assert(offsetof(pip_data, arr500) == PIP_ARR500, "arr500");
_Static_assert(offsetof(pip_data, arr521) == PIP_ARR521, "arr521");
_Static_assert(offsetof(pip_data, arr542) == PIP_ARR542, "arr542");
_Static_assert(offsetof(pip_data, arr549) == PIP_ARR549, "arr549");
_Static_assert(offsetof(pip_data, arr550) == PIP_ARR550, "arr550");
_Static_assert(offsetof(pip_data, arr609) == PIP_ARR609, "arr609");
_Static_assert(offsetof(pip_data, arr631) == PIP_ARR631, "arr631");
_Static_assert(offsetof(pip_data, arr635) == PIP_ARR635, "arr635");
_Static_assert(offsetof(pip_data, arr737) == PIP_ARR737, "arr737");
_Static_assert(offsetof(pip_data, arr760) == PIP_ARR760, "arr760");
_Static_assert(offsetof(pip_data, arr764) == PIP_ARR764, "arr764");
_Static_assert(offsetof(pip_data, main_bp) == PIP_MAIN_BP, "main_bp");
_Static_assert(offsetof(pip_data, stack_base) == PIP_STACK_BASE, "stack_base");
_Static_assert(sizeof(pip_data) == PIP_DATA_LEN, "length");
_Static_assert(sizeof(pip_data) == PIP_MEMORY, "memory");

#define T 1 /* PIP's true */
#define F 0
#define VERSION 0x1130 /* required for operation */
#define ENDFILE 0x1A
#define LPP  60   /* lines per page */
#define TAB  0x09
#define FF   0x0C
#define LA   0x5F /* left arrow */
#define LB   0x5B /* left bracket */
#define RB   0x5D /* right bracket */
#define XOFF 0x13
#define RDR  5
#define LST  10
#define PUNP 15   /* position of 'PUN' + 1 */
#define CONP 19   /* console */
#define NULP 19   /* NUL: before increment */
#define EOFP 20   /* EOF: before increment */
#define PRNT 10   /* printer */
#define FSIZE  33
#define FRSIZE 36
#define NSIZE  8
#define FNSIZE 11
#define FEXT   9
#define FEXTL  3
#define CR   13
#define LF   10
#define WHAT 63
/* scanner return types */
#define T_ERR      0
#define T_SPECL    1
#define T_FILE     2
#define T_PERIPH   3
#define T_DISKNAME 4

/* the control toggle vector, one per letter */
#define ARCHIV (g->cont[0])  /* file archive */
#define BLOCK  (g->cont[1])  /* block mode transfer */
#define DELET  (g->cont[3])  /* delete characters */
#define ECHO   (g->cont[4])  /* echo console characters */
#define FORMF  (g->cont[5])  /* form filter */
#define HEXT   (g->cont[7])  /* hex file transfer */
#define IGNOR  (g->cont[8])  /* ignore :00 record on file */
#define KILDS  (g->cont[10]) /* kill filename display */
#define LOWER  (g->cont[11]) /* translate to lower case */
#define NUMB   (g->cont[13]) /* number output lines */
#define OBJ    (g->cont[14]) /* object file transfer */
#define PAGCNT (g->cont[15]) /* page length */
#define QUITS  (g->cont[16]) /* quit copy */
#define RSYS   (g->cont[17]) /* read system files */
#define STARTS (g->cont[18]) /* start copy */
#define TABS   (g->cont[19]) /* tab set */
#define UPPER  (g->cont[20]) /* upper case translate */
#define VERIF  (g->cont[21]) /* verify equal files only */
#define WRROF  (g->cont[22]) /* write to r/o file */
#define ZEROP  (g->cont[25]) /* zero parity on input */

#define ER(n) PIP_ERROR_S_ER##n
#define EX(n) PIP_ERROR_S_EX##n

const uint16_t pip_image_len = sizeof(pip_data);
const pip_data pip_image = {
    .copyright = " (10/05/81) MP/M-86 PIP VERS 2.0 ",
    .er00 = "DISK READ$",
    .er01 = "DISK WRITE$",
    .er02 = "VERIFY$",
    .er03 = "INVALID DESTINATION$",
    .er04 = "INVALID SOURCE$",
    .er05 = "USER ABORTED$",
    .er06 = "BAD PARAMETER$",
    .er07 = "INVALID USER NUMBER$",
    .er08 = "INVALID FORMAT$",
    .er09 = "MAKE FILE$",
    .er10 = "FILE NOT FOUND$",
    .er11 = "START NOT FOUND$",
    .er12 = "QUIT NOT FOUND$",
    .er13 = "OPEN FILE$",
    .er14 = "CLOSE FILE$",
    .er15 = "PRINTER BUSY$",
    .er16 = "INVALID SEPARATOR$",
    .er17 = "NO DIRECTORY SPACE$",
    .er18 = "CAN'T DELETE TEMP FILE$",
    .errmsg = {ER(00), ER(01), ER(02), ER(03), ER(04), ER(05), ER(06), ER(07), ER(08), ER(09), ER(10), ER(11),
               ER(12), ER(13), ER(14), ER(15), ER(16), ER(17), ER(18)},
    .ex00 = "$", /* no message */
    .ex01 = "NONRECOVERABLE$",
    .ex02 = "R/O DISK$",
    .ex03 = "R/O FILE$",
    .ex04 = "INVALID DISK SELECT$",
    .ex05 = "INCOMPATIBLE MODE$",
    .ex06 = "FCB CHECKSUM$",
    .ex07 = "INVALID PASSWORD$",
    .ex08 = "ALREADY EXISTS$",
    .ex09 = "INVALID FILENAME$",
    .ex10 = "LIMIT EXCEEDED$",
    .ex11 = "INTERNAL LOCK LIMIT EXCEEDED$",
    .extmsg = {EX(00), EX(01), EX(02), EX(03), EX(04), EX(05), EX(06), EX(07), EX(08), EX(09), EX(10), EX(11)},
    .arr21 = "ERROR - $",
    .arr25 = " - $",
    .arr211 = "READER STOPPING\r\n$",
    .io = "INPIRDPTRUR1UR2RDROUTLPTUL1PRNLST"
          "PTPUP1UP2PUNTTYCRTUC1CONNULEOF",
    .del = {' ', '=', '.', ':', ';', ',', '<', '>', CR, LA, LB, RB},
    .arr486 = "RECORD TOO LONG$",
    .arr493 = "INVALID DIGIT$",
    .arr500 = "END OF FILE, CTL-Z?$",
    .arr521 = "CHECKSUM ERROR$",
    .arr542 = "CORRECT ERROR, TYPE RETURN OR CTL-Z$",
    .arr549 = "H86$",
    .arr550 = "$$$",
    .arr609 = "CMD$",
    .arr631 = "DESTINATION IS R/O, DELETE (Y/N)?$",
    .arr635 = "**NOT DELETED**$",
    .arr737 = "COPYING -$",
    .arr760 = "MP/M-86 PIP VERSION 2.0$",
    .arr764 = "REQUIRES MP/M-86$",
};

static uint16_t g_ds;
static pip_data *g;
static uint8_t *bp = 0; /* the base page: DS:0, every PL/M address is an offset from it */

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */
static uint8_t rol1(uint8_t b) { return (uint8_t)((b << 1) | (b >> 7)); }

/* the buffers: COMBUFF (cbuff+2), DBUFF at .MEMORY, SBUFF based sbase */
#define COMLEN     (g->cbuff[1])
#define COMBUFF(i) bp[(uint16_t)(PIP_CBUFF + 2 + (uint8_t)(i))]
#define DBUFF(i)   bp[(uint16_t)(PIP_MEMORY + (i))]
#define SBUFF(i)   bp[(uint16_t)(g->sbase + (i))]
#define DESTR      (g->dest.fcb + 33) /* random record position */

static void error(uint8_t errtype, uint8_t extention, uint8_t retflag, uint16_t fileadr) __attribute__((noreturn));

static void retcodes(uint16_t a) {
    g->dcnt = (uint8_t)a;
    g->exten = (uint8_t)(a >> 8);
}

static void boot(void) { mon1(0, 0); } /* system reboot */
static uint8_t rdchar(void) { return mon2(1, 0); }
static void printchar(uint8_t c) { mon1(2, c & 0x7F); }

static void crlf(void) {
    printchar(CR);
    printchar(LF);
}

static void printx(uint16_t a) { mon1(9, a); }

/* print the string at a up to its $, on a new line */
static void print(uint16_t a) {
    crlf();
    printx(a);
}

static void rdcom(void) {
    g->cbuff[0] = 128; /* maxlen */
    mon1(10, PIP_CBUFF);
}

static uint8_t conbrk(void) { return mon2(11, 0); }
static uint16_t cversion(void) { return mon3(12, 0); }
static void setdma(uint16_t a) { mon1(26, a); }
static void setpw(uint16_t fcba) { setdma((uint16_t)(fcba + FCBS_PWNAM)); }

static void open(uint16_t fcb) {
    setpw(fcb);
    retcodes(mon3(15, fcb));
}

static void close(uint16_t fcb) { retcodes(mon3(16, fcb)); }

static void ck_user(void) {
    for (;;) {
        if (g->anything & 1) return;
        if (rel(g->dcnt == 0xFF) & 1) return;
        if (rel(g->last_user == bp[bp_buff + (((g->dcnt >> 3) | (g->dcnt << 5)) & 0x60)] /* ror(dcnt,3) */) & 1) return;
        retcodes(mon3(18, 0));
    }
}

static void search(uint16_t fcb) {
    g->anything = rel(bp[fcb] == '?');
    retcodes(mon3(17, fcb));
    ck_user();
}

static void searchn(void) {
    retcodes(mon3(18, 0));
    ck_user();
}

static void delete(uint16_t fcb) {
    setpw(fcb);
    retcodes(mon3(19, fcb));
}

static void diskrd(uint16_t fcb) { retcodes(mon3(20, fcb)); }
static void diskwrite(uint16_t fcb) { retcodes(mon3(21, fcb)); }

static void make(uint16_t fcba) {
    uint8_t *fcbs = bp + fcba;
    if (rel(fcbs[FCBS_PWNAM] == 0) & 1) { /* zero if no password */
        fcbs[6] &= 0x7F;                   /* reset password attribute */
    } else {
        fcbs[6] |= 0x80;                   /* set password attribute */
        setdma((uint16_t)(fcba + FCBS_PWNAM));
    }
    retcodes(mon3(22, fcba));
}

static void rename(uint16_t fcb) {
    setpw(fcb);
    retcodes(mon3(23, fcb));
}

static uint8_t getdisk(void) { return mon2(25, 0); }
static void setind(uint16_t fcb) { retcodes(mon3(30, fcb)); }
static uint8_t getuser(void) { return mon2(32, 0xFF); }
static void setuser(uint8_t user) { mon1(32, (g->last_user = user)); }
static void setcuser(void) { setuser(g->cuser); }
static void setduser(void) { setuser(g->odest.user); }
static void setsuser(void) { setuser(g->source.user); }
static uint8_t rd_random(uint16_t fcb) { return mon2(33, fcb); }
static void set_random(uint16_t fcb) { mon1(36, fcb); } /* set random record position */
static void multsect(uint16_t a) { mon1(44, a); }
static void errmode(uint16_t a) { mon1(45, a); }
static void rdxfcb(uint16_t xfcb) { retcodes(mon3(102, xfcb)); }
static uint8_t conatlst(void) { return mon2(161, 0); }

/* PIP's own MOVE(s, d, n), on DS addresses */
static void move(uint16_t s, uint16_t d, uint8_t n) {
    while (rel((n = (uint8_t)(n - 1)) != 255) & 1) bp[d++] = bp[s++];
}

/* move three character extent into dest fcb */
static void movext(uint16_t a) { move(a, PIP_DEST + FEXT, FEXTL); }

/*
 * ERROR: close and delete the destination's temporary file, print
 * "ERROR - message extended-message - d:file", and GO TO RETRY. A
 * retflag (open, make, close: the file can be skipped) has MULTCOPY,
 * from RETRY, carry on with the next file of a multi-file copy.
 */
static void error(uint8_t errtype, uint8_t extention, uint8_t retflag, uint16_t fileadr) {
    uint8_t i, temp;
    /* error retry = retry flag, in a multi-file copy (AMBIG) only: else
     * RETRY's MULTCOPY, after a one-file copy's error, would copy what
     * the CLI's fcb at 5Ch names (a fix, made in PIP.PLM too) */
    g->eretry = retflag & g->ambig;
    multsect(1);
    setduser();
    close(PIP_DEST);
    delete(PIP_DEST); /* delete destination scratch file */
    close(PIP_ODEST); /* try to close original destination */
    setcuser();

    /* print out error message */
    print(PIP_ARR21); /* ERROR - */
    printx(rd16(bp + (uint16_t)(PIP_ERROR_S_ERRMSG + errtype * 2)));
    if (rel((extention = extention & 0x0F) <= 12) & 1) { /* nummsgs */
        printchar(' ');
        printx(rd16(bp + (uint16_t)(PIP_ERROR_S_EXTMSG + extention * 2)));
    }
    printx(PIP_ARR25); /* " - " */
    if (rel(fileadr != 0) & 1) {
        printchar((uint8_t)('A' + bp[fileadr] - 1));
        printchar(':');
        for (i = 1; i <= FNSIZE; i++) {
            if (rel((temp = bp[(uint16_t)(fileadr + i)] & 0x7F) != ' ') & 1) {
                if (rel(i == FEXT) & 1) printchar('.');
                printchar(temp);
            }
        }
    }

    /* zero the comlen in case this is a single command */
    COMLEN = 0;
    crlf();
    PIP_LONGJMP(); /* GO TO RETRY */
}

static void formerr(void) { error(8, 0, 0, 0); } /* invalid format */

/* fill the source buffers */
static void fillsource(void) {
    uint8_t i, n;
    g->nsource = 0;
    setsuser(); /* source user number set */
    if (g->fastcopy & 1) {
        n = (uint8_t)((g->nbuf >> 3) - 1);
        g->bufsize = 1024;
        multsect(8);
    } else {
        g->bufsize = 128;
        multsect(1);
        n = g->nbuf;
    }
    for (i = 0; i <= n; i++) {
        /* set dma address to next buffer position */
        setdma((uint16_t)(g->sbase + g->nsource));
        diskrd(PIP_SOURCE);
        if (rel(g->dcnt != 0) & 1) {
            if (rel(g->dcnt != 1) & 1) error(0, g->exten, 0, PIP_SOURCE); /* disk read error */
            /* end - of - file */
            if (g->fastcopy & 1) /* add no. sectors copied */
                g->nsource = (uint16_t)(g->nsource + 128 * (g->exten >> 4));
            g->hardeof = g->nsource; /* set hard end-of-file */
            SBUFF(g->nsource) = ENDFILE;
            i = n;
        } else {
            g->nsource = (uint16_t)(g->nsource + g->bufsize);
        }
    }
    g->tblen = g->nsource;
    g->nsource = 0;
    setcuser(); /* back to current user number */
}

/* write output buffers up to but not including position ndest -- the
 * low order 7 bits of ndest are zero */
static void writedest(void) {
    uint8_t i, j, n, dataok;
    if (rel((n = (uint8_t)((uint8_t)(g->ndest >> 7) - 1)) == 255) & 1) return;
    if (g->fastcopy & 1) {
        g->bufsize = 1024;
        multsect(8);
    } else {
        g->bufsize = 128;
        multsect(1);
    }
    g->ndest = 0;
    setduser();           /* destination user */
    set_random(PIP_DEST); /* set base record for verify */
    for (i = 0; i <= n; i++) {
        if (g->fastcopy & 1) {
            if (rel((uint8_t)(n - i) < 7) & 1) {
                g->bufsize = 128;
                multsect(1);
            } else {
                i = (uint8_t)(i + 7);
            }
        }
        /* set dma address to next buffer */
        setdma((uint16_t)(PIP_MEMORY + g->ndest));
        diskwrite(PIP_DEST);
        if (rel(g->dcnt != 0) & 1) error(1, g->exten, 0, PIP_DEST); /* disk write error */
        g->ndest = (uint16_t)(g->ndest + g->bufsize);
    }
    multsect(1);
    if (VERIF & 1) { /* verify data written ok */
        g->ndest = 0;
        setdma(bp_buff); /* for compare */
        for (i = 0; i <= n; i++) {
            dataok = rel(rd_random(PIP_DEST) == 0);
            wr16(DESTR, (uint16_t)(rd16(DESTR) + 1)); /* next random read */
            j = 0;
            /* perform comparison */
            while ((dataok & rel(j < 0x80)) & 1) {
                dataok = rel(bp[bp_buff + j] == DBUFF(g->ndest + j));
                j++;
            }
            g->ndest = (uint16_t)(g->ndest + 128);
            if ((uint8_t)~dataok & 1) error(2, 0, 0, PIP_DEST); /* verify error */
        }
        diskwrite(PIP_DEST);
        dataok = g->dcnt;
        (void)dataok;
        /* now ready to continue the write operation */
    }
    g->ndest = 0;
    setcuser(); /* back to current user */
}

/* write byte b to the destination device given by pdest */
static void putdchar(uint8_t b) {
    if (rel(b >= ' ') & 1) {
        g->column++;
        if (rel(DELET > 0) & 1) { /* may be past right side */
            if (rel(g->column > DELET) & 1) return;
        }
    }
    switch (g->pdest) {
    case 0: /* the destination file */
        if (rel(g->ndest >= g->dblen) & 1) writedest();
        DBUFF(g->ndest) = b;
        g->ndest++;
        break;
    case 10: /* PRN (tabs expanded, lines listed, changed to LST): GO TO LSTL */
    case 11: /* LST */
        mon1(5, b);
        break;
    case 19: /* CON */
        mon1(2, b);
        break;
    default: /* ARD, IRD, PTR, UR1, UR2, RDR, OUT, LPT, UL1, PTP, UP1, UP2, PUN, TTY, CRT, UC1 */
        error(3, 0, 0, 0); /* NOTDEST: invalid destination */
    }
}

/* write destination character, tab expansion */
static void putdestc(uint8_t b) {
    uint8_t i;
    if (rel(b != TAB) & 1) {
        putdchar(b);
    } else if (rel(TABS == 0) & 1) {
        putdchar(b);
    } else { /* b is tab char, tabs > 0 */
        i = g->column;
        while (rel(i >= TABS) & 1) i = (uint8_t)(i - TABS);
        i = (uint8_t)(TABS - i);
        while (rel(i > 0) & 1) {
            i--;
            putdchar(' ');
        }
    }
    if (rel(b == CR) & 1) g->column = 0;
}

static void print1(uint8_t b) {
    if ((g->zerosup = g->zerosup & rel(b == 0)) & 1)
        putdestc(' ');
    else
        putdestc((uint8_t)('0' + b));
}

static void printdig(uint8_t d) {
    print1(d >> 4);
    print1(d & 0x0F);
}

/* DEC(a + b) and DEC(a PLUS b): the 8086's add (adc) then daa; carry is
 * the flag the next PLUS adds in */
static uint8_t carry;

static uint8_t dec_add(uint8_t a, uint8_t b, uint8_t cin) {
    unsigned sum = (unsigned)a + b + cin;
    uint8_t al = (uint8_t)sum, old_al = al, af = (uint8_t)(((a & 0x0F) + (b & 0x0F) + cin) > 0x0F);
    uint8_t cf = (uint8_t)(sum > 0xFF), old_cf = cf;
    cf = 0;
    if ((al & 0x0F) > 9 || af) {
        cf = (uint8_t)(old_cf || al + 6 > 0xFF);
        al = (uint8_t)(al + 6);
    }
    if (old_al > 0x99 || old_cf) {
        al = (uint8_t)(al + 0x60);
        cf = 1;
    }
    carry = cf;
    return al;
}

static void newline(void) {
    uint8_t one;
    one = 1;
    g->zerosup = rel(NUMB == 1);
    g->c1 = dec_add(g->c1, one, 0);
    g->c2 = dec_add(g->c2, 0, carry); /* PLUS: the carry out of c1 */
    g->c3 = dec_add(g->c3, 0, carry);
    printdig(g->c3);
    printdig(g->c2);
    printdig(g->c1);
    if (rel(NUMB == 1) & 1) { /* usually printer output */
        putdestc(':');
        putdestc(' ');
    } else {
        putdestc(TAB);
    }
}

/* clear output buffer in block mode transmision */
static void clearbuff(void) {
    uint16_t na;
    uint8_t i;
    i = (uint8_t)(g->ndest & 0x7F); /* remaining partial buffer length */
    na = g->ndest & 0xFF80;         /* start of segment not written */
    writedest();                    /* clears buffers */
    move((uint16_t)(PIP_MEMORY + na), PIP_MEMORY, i);
    /* data moved to beginning of buffer */
    g->ndest = i;
}

/* write destination character, check tabs and lines */
static void putdest(uint8_t b) {
    uint8_t i;
    if (FORMF & 1) { /* skip form feeds */
        if (rel(b == FF) & 1) return;
    }
    if (g->putnum & 1) { /* end of line or start of file */
        if (rel(b != FF) & 1) { /* not form feed */
            if (rel((i = PAGCNT) != 0) & 1) { /* page eject */
                if (rel(i == 1) & 1) i = LPP;
                if (rel((g->lineno = (uint8_t)(g->lineno + 1)) >= i) & 1) {
                    g->lineno = 0; /* new page */
                    putdestc(FF);
                }
            }
            if (rel(NUMB > 0) & 1) newline();
            g->putnum = F;
        }
    }
    if (BLOCK & 1) { /* block mode transfer */
        if ((rel(b == XOFF) & rel(g->pdest == 0)) & 1) {
            clearbuff(); /* buffers written */
            return;      /* don't pass the x-off */
        }
    }
    if (rel(b == FF) & 1) g->lineno = 0;
    putdestc(b);
    if (rel(b == LF) & 1) g->putnum = T;
}

/* translate alpha to upper case */
static uint8_t utran(uint8_t b) {
    if ((rel(b >= 0x61) & rel(b <= 0x7A)) & 1) b &= 0x5F;
    return b;
}

/* translate to lower case alpha */
static uint8_t ltran(uint8_t b) {
    if ((rel(b >= 'A') & rel(b <= 'Z')) & 1) b |= 0x20;
    return b;
}

/* read next source character */
static uint8_t getsourcec(void) {
    uint8_t temp, b, conchk;
    if (rel((uint8_t)(g->psource - 1) <= RDR) & 1) { /* 1 ... RDR+1 */
        uint8_t l = BLOCK | HEXT;
        if ((l & conbrk()) & 1) {
            if (rel(rdchar() == ENDFILE) & 1) return ENDFILE;
            print(PIP_ARR211); /* READER STOPPING */
            return XOFF;
        }
    }
    conchk = T; /* console status check below */
    switch (g->psource) {
    case 0: /* source file */
        if (rel(g->nsource >= g->sblen) & 1) fillsource();
        b = SBUFF(g->nsource);
        g->nsource++;
        break;
    case 19: /* CON */
        conchk = F; /* don't check console status */
        b = mon2(1, 0);
        break;
    default: /* INP, IRD, PTR, UR1, UR2, RDR, OUT, LPT, UL1, PRN, LST, PTP, UP1, UP2, PUN, TTY, CRT, UC1 */
        error(4, 0, 0, 0); /* NOTSOURCE: invalid source */
    }
    if (ECHO & 1) { /* copy to console device */
        temp = g->pdest;
        g->pdest = CONP;
        putdest(b);
        g->pdest = temp;
    }
    if (conchk & 1) { /* test for console char ready */
        if (g->scom & 1) /* source is a com file */
            conchk = rel((g->concnt = (uint8_t)(g->concnt + 1)) == 0);
        else /* ascii */
            conchk = rel(b == LF);
        if (conchk & 1) {
            if (conbrk() & 1) {
                if (rel(rdchar() == ENDFILE) & 1) return ENDFILE;
                error(5, 0, 0, 0); /* user aborted */
            }
        }
    }
    if (ZEROP & 1) b &= 0x7F;
    if (UPPER & 1) return utran(b);
    if (LOWER & 1) return ltran(b);
    return b;
}

/* getsource's CHAR (its own, not the global one; MATCH uses it) */
static uint8_t gs_char;

/* match start and quit strings */
static uint8_t match(uint8_t b) {
    uint8_t c;
    if (rel((c = COMBUFF(b = (uint8_t)(b + g->matchlen))) == ENDFILE) & 1) { /* end match */
        COMBUFF(b) = gs_char; /* save current character */
        return T;
    }
    if (rel(c == gs_char) & 1)
        g->matchlen++;
    else
        g->matchlen = 0; /* no match */
    return F;
}

/* get next source character */
static uint8_t getsource(void) {
    if (rel(g->quitlen > 0) & 1) {
        if (rel((g->quitlen = (uint8_t)(g->quitlen - 1)) == 1) & 1) return LF;
        return ENDFILE; /* terminated with cr,lf,endfile */
    }
    for (;;) { /* looking for start */
        if (rel(g->feedlen > 0) & 1) { /* get search characters */
            g->feedlen--;
            gs_char = COMBUFF(g->feedbase);
            g->feedbase++;
            return gs_char;
        }
        if (rel((gs_char = getsourcec()) == ENDFILE) & 1) return ENDFILE;
        if (rel(STARTS > 0) & 1) { /* looking for start string */
            if (match(STARTS) & 1) {
                g->feedbase = STARTS;
                STARTS = 0;
                g->feedlen = (uint8_t)(g->matchlen + 1);
                g->matchlen = 0;
            } /* otherwise no match, skip character */
        } else if (rel(QUITS > 0) & 1) { /* pass characters til match */
            if (match(QUITS) & 1) {
                QUITS = 0;
                g->quitlen = 2;
                /* subsequently return cr, lf, endfile */
                return CR;
            }
            return gs_char;
        } else {
            return gs_char;
        }
    }
}

static uint8_t gnc(void) {
    if (rel((g->cbp = (uint8_t)(g->cbp + 1)) >= COMLEN) & 1) return CR;
    return utran(COMBUFF(g->cbp));
}

static void deblank(void) {
    while (rel((g->ch = gnc()) == ' ') & 1) {
    }
}

/* SCAN's fcbs (the fcb it fills) and K, which SCANPAR shares */
static uint16_t sc_fcba;
static uint8_t sc_k;
#define FCBS(i) bp[(uint16_t)(sc_fcba + (i))]

static uint8_t delimiter(uint8_t c) {
    for (uint8_t i = 0; i <= 11; i++) /* last(del) */
        if (rel(c == g->del[i]) & 1) return T;
    return F;
}

static void putchar_(void) {
    FCBS(g->flen = (uint8_t)(g->flen + 1)) = g->ch;
    if (rel(g->ch == WHAT) & 1) g->ambig = T; /* contains ambiguous ref */
}

/* fill current name or type with question marks */
static void fillq(uint8_t len) {
    g->ch = WHAT; /* question mark */
    while (rel(g->flen < len) & 1) putchar_();
}

/* scan optional parameters */
static void scanpar(void) {
    uint8_t i, j;
    g->ch = gnc(); /* scan past bracket */
    while ((uint8_t)~(rel(g->ch == CR) | rel(g->ch == RB)) & 1) {
        if (rel((i = (uint8_t)(g->ch - 'A')) > 25) & 1) { /* not alpha */
            if (rel(g->ch == ' ') & 1)
                g->ch = gnc();
            else
                error(6, 0, 0, 0); /* bad parameter */
        } else { /* scan parameter value */
            if ((rel(g->ch == 'S') | rel(g->ch == 'Q')) & 1) { /* start or quit command */
                j = (uint8_t)(g->cbp + 1);                     /* start of string */
                do
                    g->ch = gnc();
                while ((uint8_t)~(rel(g->ch == ENDFILE) | rel(g->ch == CR)) & 1);
                g->ch = gnc();
            } else if (rel((j = (uint8_t)((g->ch = gnc()) - '0')) > 9) & 1) {
                j = 1;
            } else {
                while (rel((sc_k = (uint8_t)((g->ch = gnc()) - '0')) <= 9) & 1) j = (uint8_t)(j * 10 + sc_k);
            }
            g->cont[i] = j;
            if (rel(i == 6) & 1) { /* set source user */
                if (rel(j > 31) & 1) error(7, 0, 0, 0); /* invalid user number */
                FCBS(FCBS_USER) = j;
            }
        }
    }
    g->ch = gnc();
}

static void chkset(void) {
    if (rel(g->ch == LA) & 1) g->ch = '=';
}

/* SCAN looks for the next delimiter, device name, or file name. The
 * value of cbp must be 255 upon entry the first time */
static void scan(uint16_t fcba) {
    uint8_t i;
    sc_fcba = fcba;

    /* initialize file control block to empty */
    g->ambig = F;
    g->type = T_ERR;
    g->ch = ' ';
    g->flen = 0;
    while (rel(g->flen < FRSIZE + NSIZE) & 1) {
        if (rel(g->flen == FNSIZE) & 1)
            g->ch = 0;
        else if (rel(g->flen == FRSIZE) & 1)
            g->ch = ' ';
        FCBS(g->flen = (uint8_t)(g->flen + 1)) = g->ch;
    }
    FCBS(FCBS_PWMODE) = 1;

    /* deblank command buffer */
    deblank();

    /* may be a separator */
    if (delimiter(g->ch) & 1) {
        chkset();
        g->type = T_SPECL;
        return;
    }

    /* check peripherals and disk files */
    FCBS(0) = (uint8_t)(g->cdisk + 1); /* initialize to current disk */
    FCBS(FCBS_USER) = g->cuser;        /* and current user */
    /* clear parameters */
    for (i = 0; i <= 25; i++) g->cont[i] = 0;
    g->feedlen = g->matchlen = g->quitlen = 0;
    /* scan next name */
    for (;;) {
        g->flen = 0;
        while ((uint8_t)~delimiter(g->ch) & 1) {
            if (rel(g->flen >= NSIZE) & 1) return; /* error, file name too long */
            if (rel(g->ch == '*') & 1)
                fillq(NSIZE);
            else
                putchar_();
            g->ch = gnc();
        }

        /* check for disk name or device name */
        if (rel(g->ch == ':') & 1) {
            if (rel(g->flen == 1) & 1) {
                /* may be disk name a ... p */
                if (rel((FCBS(0) = (uint8_t)(FCBS(1) - 'A' + 1)) > 17) & 1) return; /* invalid disk name */
                deblank(); /* may be disk name only */
                if (delimiter(g->ch) & 1) {
                    if (rel(g->ch == LB) & 1) scanpar();
                    g->cbp--;
                    g->type = T_DISKNAME;
                    return;
                }
            } else if (rel(g->flen != 3) & 1) {
                return; /* error, cannot be device name */
            } else {
                /* look for device name: all reader-like devices are
                 * before RDR, all listing-like ones below LST but
                 * above RDR (the RDR, LST and PUNP literals) */
                uint8_t di, dj, dk;
                dj = 255;
                for (dk = 0; dk <= 20; dk++) {
                    di = 0;
                    for (;;) {
                        uint8_t more = rel((di = (uint8_t)(di + 1)) <= 3);
                        if (!((more & rel(g->io[(uint8_t)(dj + di)] == FCBS(di))) & 1)) break;
                    }
                    if (rel(di == 4) & 1) { /* complete match */
                        g->type = T_PERIPH;
                        /* scan parameters */
                        if (rel(gnc() == LB) & 1) scanpar();
                        g->cbp--;
                        g->ch = dk;
                        return;
                    }
                    /* otherwise try next device */
                    dj = (uint8_t)(dj + 3);
                }
                return; /* error, no device name match */
            }
            if (rel(g->ch == LB) & 1) scanpar(); /* parameters follow */
        } else {
            /* char is not ':', so file name is set. scan remainder */
            if (rel(g->flen == 0) & 1) return; /* error, no primary name */
            g->flen = NSIZE;
            if (rel(g->ch == '.') & 1) { /* scan file type */
                while ((uint8_t)~delimiter(g->ch = gnc()) & 1) {
                    if (rel(g->flen >= FNSIZE) & 1) return; /* error, type field too long */
                    if (rel(g->ch == '*') & 1)
                        fillq(FNSIZE);
                    else
                        putchar_();
                }
            }

            g->flen = 0;
            if (rel(g->ch == ';') & 1) { /* scan password */
                while ((uint8_t)~delimiter(g->ch = gnc()) & 1) {
                    if (rel(g->flen >= NSIZE) & 1) return; /* error, pw too long */
                    FCBS(FCBS_PWNAM + g->flen) = g->ch;      /* save password */
                    g->flen++;
                }
            }
            if (rel(sc_fcba == PIP_ODEST) & 1) g->getpw = F; /* use dest password */

            if (rel(g->ch == LB) & 1) scanpar();
            /* rescan delimiter next time around */
            g->cbp--;
            g->type = T_FILE;
            FCBS(32) = 0;
            return;
        }
    }
}

/* send 40 nulls to output device */
static void nulls(void) {
    for (uint8_t i = 0; i <= 39; i++) putdest(0);
}

/* compare the strings at a and b until either a mismatch or a '$' is
 * encountered in string b */
static uint8_t equal(uint16_t a, uint16_t b) {
    while (rel(bp[b] != '$') & 1) {
        if (rel((bp[b] & 0x7F) != (bp[a] & 0x7F)) & 1) return F;
        a++;
        b++;
    }
    return T;
}

/* return true if end of file */
static uint8_t rd_eof(void) {
    g->ch = getsource();
    if (g->scom & 1) return rel(g->hardeof < g->nsource);
    return rel(g->ch == ENDFILE);
}

/* HEXRECORD's variables, which its nested procedures share */
static uint8_t hx_xoffset; /* true if xoff recvd */
static uint8_t hx_noerrs;  /* true if no errors in this record */
static uint8_t hx_m, hx_rl, hx_cs, hx_rt;
static uint16_t hx_lda;    /* load address which follows : */

/* print error message if noerrs true */
static void printerr(uint16_t a) {
    if (hx_noerrs & 1) {
        hx_noerrs = F;
        print(a);
    }
}

static void ckxoff(void) {
    if (hx_xoffset & 1) {
        hx_xoffset = F;
        clearbuff();
    }
}

/* read character and save in buffer */
static uint8_t savechar(void) {
    uint8_t i;
    if (hx_noerrs & 1) {
        while (rel((i = getsource()) == XOFF) & 1) hx_xoffset = T;
        bp[(uint16_t)(PIP_HBUFF + g->hsource)] = i;
        if (rel((g->hsource = (uint8_t)(g->hsource + 1)) >= 79) & 1) /* last(hbuff) */
            printerr(PIP_ARR486); /* RECORD TOO LONG */
        return i;
    }
    return ENDFILE; /* on error flag */
}

static uint8_t rdhex(void) {
    uint8_t h;
    if (rel((uint8_t)((h = savechar()) - '0') <= 9) & 1) return (uint8_t)(h - '0');
    if (rel((uint8_t)(h - 'A') > 5) & 1) printerr(PIP_ARR493); /* INVALID DIGIT */
    return (uint8_t)(h - 'A' + 10);
}

/* read two hex digits */
static uint8_t rdbyte(void) {
    uint8_t hi = rdhex();
    return (uint8_t)((hi << 4) | rdhex());
}

/* read byte with checksum */
static uint8_t rdcs(void) {
    uint8_t c = hx_cs;
    return hx_cs = (uint8_t)(c + rdbyte());
}

/* read double byte with checksum */
static uint16_t rdaddr(void) {
    uint16_t hi = (uint16_t)(rdcs() << 8);
    return hi | rdcs();
}

/* read one record into sbuff and check for proper form: 0 if record
 * ok, 1 if end of tape (:00000), 2 if error in record */
static uint8_t hexrecord(void) {
    hx_noerrs = T; /* no errors detected in this record */

    /* read next record: scan for the ':' */
    g->hsource = 0;
    while (rel((hx_cs = savechar()) != ':') & 1) {
        g->hsource = 0;
        if (rel(hx_cs == ENDFILE) & 1) {
            print(PIP_ARR500); /* END OF FILE, CTL-Z? */
            if (rel(rdchar() == ENDFILE) & 1) return 1;
            g->hsource = 0;
        }
        ckxoff();
    }

    /* ':' found */
    hx_cs = 0;
    if (rel((hx_rl = rdcs()) == 0) & 1) { /* end of tape */
        while (rel((hx_rl = savechar()) != ENDFILE) & 1) ckxoff();
        if (hx_noerrs & 1) return 1;
        return 2;
    }

    /* record length is not zero */
    hx_lda = rdaddr(); /* load address */

    /* read words until record length exhausted */
    hx_rt = rdcs(); /* record type */
    while ((rel(hx_rl != 0) & hx_noerrs) & 1) {
        hx_rl--;
        hx_m = rdcs();
        /* increment la here for exact address */
    }

    /* check sum */
    {
        uint8_t c = hx_cs;
        if (rel((uint8_t)(c + rdbyte()) != 0) & 1) printerr(PIP_ARR521); /* CHECKSUM ERROR */
    }

    ckxoff();
    if (hx_noerrs & 1) return 0;
    return 2;
}

/* read hex file from high speed reader to 'H86' file, check each record
 * for valid digits, and proper checksum */
static void rdtape(void) {
    uint8_t i, a;
    for (;;) {
        while (rel((i = hexrecord()) <= 1) & 1) {
            if ((uint8_t)~(rel(i == 1) & IGNOR) & 1)
                for (a = 1; a <= g->hsource; a++) putdest(bp[(uint16_t)(PIP_HBUFF + (uint8_t)(a - 1))]);
            putdest(CR);
            putdest(LF);
            if (rel(i == 1) & 1) return; /* end of tape encountered */
        }
        crlf();
        bp[(uint16_t)(PIP_HBUFF + g->hsource)] = '$';
        print(PIP_HBUFF);
        print(PIP_ARR542); /* CORRECT ERROR, TYPE RETURN OR CTL-Z */
        crlf();
        if (rel(rdchar() == ENDFILE) & 1) return;
    }
}

static void setpswd(void) {
    uint8_t i, j;
    j = 23;
    for (i = 0; i <= 7; i++) {
        g->dest.pwnam[i] = g->dxfcb[j] ^ g->dxfcb[13];
        j--;
    }
    g->dest.pwmode = g->dxfcb[12];
}

static void setupdest(void) {
    setduser();                                     /* destination user */
    move(PIP_ODEST, PIP_DEST, FRSIZE + NSIZE + 1); /* save original dest */
    g->dhex = equal(PIP_ODEST + FEXT, PIP_ARR549);  /* H86 */
    movext(PIP_ARR550);                             /* $$$ */
    g->odest.fcb[6] |= 0x80;
    open(PIP_ODEST); /* try to open destination file */
    if ((rel(g->dcnt != 255) & rol1(g->odest.fcb[8])) & 1) {
        g->odcnt = 255;
        close(PIP_ODEST);
    } else {
        g->odcnt = g->dcnt; /* and save error code */
    }
    if ((rel(g->odcnt == 255) & rel((g->exten & 0x0F) != 0)) & 1) /* file exists */
        error(13, g->exten, 1, PIP_ODEST);                         /* but cant open - error */

    if (g->getpw & 1) { /* setup destination password */
        setsuser();
        move(PIP_SOURCE, PIP_DXFCB, FSIZE);
        rdxfcb(PIP_DXFCB);
        if (rel(g->dcnt == 255) & 1)
            g->dest.pwnam[0] = 0;
        else
            setpswd();
        setduser();
    }

    delete(PIP_DEST); /* remove old $$$ file */
    if ((rel(g->dcnt == 255) & rel(g->exten != 0)) & 1)
        error(18, g->exten, 1, PIP_DEST); /* cant delete temp file */
    make(PIP_DEST);                       /* create a new one */
    if (rel(g->dcnt == 255) & 1) {
        if (rel((g->exten & 0x0F) == 0) & 1)
            error(17, 0, 1, PIP_DEST); /* no directory space */
        else
            error(9, g->exten, 1, PIP_DEST); /* make file error */
    }
    setcuser(); /* back to current user */
    g->dest.fcb[32] = g->ndest = 0;
}

static void setupsource(void) {
    g->hardeof = 0xFFFF;
    g->f1 = g->f2 = g->f3 = g->f4 = g->ro = g->sys = 0;
    setsuser(); /* source user */
    g->source.fcb[6] |= 0x80;
    open(PIP_SOURCE); /* open source in r/o mode */
    if (rol1(g->source.fcb[1]) & 1) g->f1 = 0x80;
    if (rol1(g->source.fcb[2]) & 1) g->f2 = 0x80;
    if (rol1(g->source.fcb[3]) & 1) g->f3 = 0x80;
    if (rol1(g->source.fcb[4]) & 1) g->f4 = 0x80;
    if (rol1(g->source.fcb[9]) & 1) g->ro = 0x80;
    if (rol1(g->source.fcb[10]) & 1) g->sys = 0x80;
    setcuser(); /* back to current user */
    if (((uint8_t)~RSYS & rol1(g->source.fcb[10])) & 1) g->dcnt = 255; /* skip system file */
    if (rel(g->dcnt == 255) & 1) {
        if (rel((g->exten & 0x0F) == 0) & 1)
            error(10, 0, 0, PIP_SOURCE); /* file not found */
        else
            error(13, g->exten, 1, PIP_SOURCE); /* open file error */
    }
    g->source.fcb[32] = 0;
    /* cause immediate read */
    g->scom = equal(PIP_SOURCE + FEXT, PIP_ARR609); /* CMD */
    g->nsource = g->sblen;
}

static void ck_strings(void) {
    if (rel(STARTS > 0) & 1) error(11, 0, 0, 0); /* start not found */
    if (rel(QUITS > 0) & 1) error(12, 0, 0, 0);  /* quit not found */
}

static void closedest(void) {
    while (rel((g->ndest & 0x7F) != 0) & 1) putdest(ENDFILE);
    ck_strings();
    writedest();
    setduser(); /* destination user */
    close(PIP_DEST);
    if (rel(g->dcnt == 255) & 1) error(14, g->exten, 1, PIP_DEST); /* close file */
    if (rel(g->odcnt != 255) & 1) { /* file exists */
        close(PIP_ODEST);
        if (rol1(g->odest.fcb[9]) & 1) { /* read only */
            if ((uint8_t)~WRROF & 1) {
                print(PIP_ARR631); /* DESTINATION IS R/O, DELETE (Y/N)? */
                if (rel(utran(rdchar()) != 'Y') & 1) {
                    print(PIP_ARR635); /* **NOT DELETED** */
                    crlf();
                    delete(PIP_DEST);
                    setcuser(); /* back to current user */
                    return;
                }
                crlf();
            }
        }
        /* reset r/o and sys attributes */
        g->odest.fcb[9] &= 0x7F;
        g->odest.fcb[10] &= 0x7F;
        setind(PIP_ODEST);
        delete(PIP_ODEST);
    }
    move(PIP_ODEST, PIP_DEST + 16, 16); /* ready for rename */
    rename(PIP_DEST);
    /* set destination attributes same as source */
    g->odest.fcb[1] = (g->odest.fcb[1] & 0x7F) | g->f1;
    g->odest.fcb[2] = (g->odest.fcb[2] & 0x7F) | g->f2;
    g->odest.fcb[3] = (g->odest.fcb[3] & 0x7F) | g->f3;
    g->odest.fcb[4] = (g->odest.fcb[4] & 0x7F) | g->f4;
    g->odest.fcb[8] = g->odest.fcb[8] & 0x7F;
    g->odest.fcb[9] = (g->odest.fcb[9] & 0x7F) | g->ro;
    g->odest.fcb[10] = (g->odest.fcb[10] & 0x7F) | g->sys;
    g->odest.fcb[11] = g->odest.fcb[11] & 0x7F;
    setind(PIP_ODEST);
    if (ARCHIV & 1) { /* set archive bit */
        setsuser();
        g->source.fcb[11] |= 0x80;
        g->source.fcb[12] = 0;
        setind(PIP_SOURCE);
    }
    setcuser(); /* back to current user */
}

/* compute number of buffers - 1 from dblen (dblen <= 32K, so nbuf
 * <= 255) */
static void size_nbuf(void) { g->nbuf = (uint8_t)((uint8_t)(g->dblen >> 7) - 1); }

/* absorb the source buffer into the dest buffer */
static void set_dblen(void) {
    g->sbase = PIP_MEMORY;
    if (rel(g->dblen >= 0x4000) & 1)
        g->dblen = 0x7F80;
    else
        g->dblen = (uint16_t)(g->dblen + g->sblen);
    size_nbuf();
}

/* set up source and destination buffers */
static void size_memory(void) {
    uint16_t memsize = rd16(bp + bp_maxb);
    g->sbase = (uint16_t)(PIP_MEMORY + ((uint16_t)(memsize - PIP_MEMORY) >> 1));
    g->sblen = g->dblen = (uint16_t)(((uint16_t)(memsize - PIP_MEMORY) & 0xFF00) >> 1);
    size_nbuf();
}

/* perform the actual copy function */
static void copychar(void) {
    uint8_t resized; /* true if sbuff and dbuff combined */
    if ((resized = BLOCK & rel(g->psource != 0)) & 1) set_dblen(); /* block mode: absorb source buffer */
    if ((HEXT | IGNOR) & 1) { /* hex file */
        rdtape();
    } else {
        while ((uint8_t)~rd_eof() & 1) putdest(g->ch);
    }
    if (resized & 1) {
        clearbuff();
        size_memory();
    }
}

static uint8_t real_eof(void) { return rel(g->hardeof != 0xFFFF); }

static void simplecopy(void) {
    uint8_t i;
    size_memory();
    setupdest();
    setupsource();
    /* files ready for direct copy */
    g->fastcopy = T;
    /* look for parameters */
    for (i = 0; i <= 25; i++) {
        if (rel(g->cont[i] != 0) & 1) {
            if ((uint8_t)~(rel(i == 6) | rel(i == 10) | rel(i == 14) | rel(i == 17) | rel(i == 21) | rel(i == 22)) & 1)
                g->fastcopy = F; /* not obj or verify */
        }
    }
    if (g->fastcopy & 1) { /* copy directly to dbuff */
        set_dblen();       /* extend dbuff */
        while ((uint8_t)~real_eof() & 1) {
            fillsource();
            if (real_eof() & 1)
                g->ndest = g->hardeof;
            else
                g->ndest = g->tblen;
            writedest();
        }
        size_memory(); /* reset to two buffers */
    } else {
        copychar();
    }
    setsuser();
    close(PIP_SOURCE);
    setcuser();
    closedest();
}

/* MULTCOPY's counters: PL/M's locals are static, and an error retry
 * (eretry) carries on from them */
static uint16_t mc_nextdir, mc_ndcnt, mc_ncopied;

/* print current file name */
static void prname(void) {
    uint8_t i, c;
    crlf();
    for (i = 1; i <= FNSIZE; i++) {
        if (rel((c = g->odest.fcb[i]) != ' ') & 1) {
            if (rel(i == FEXT) & 1) printchar('.');
            printchar(c);
        }
    }
}

/* check if archive bit is set in any extent of source file: each
 * directory entry search(.source) finds is looked at in BUFF, and
 * source.fcb(12) is 0 again either way. (DRI's copied the entry into
 * source.fcb, the pattern search next rereads, so it saw one extent
 * and could leave its ex there for the open; a fix, made in PIP.PLM
 * too.) */
static uint8_t archck(void) {
    setsuser();
    g->source.fcb[12] = WHAT;
    search(PIP_SOURCE);
    while (rel(g->dcnt != 255) & 1) {
        if ((uint8_t)~rol1(bp[(uint16_t)(bp_buff + (uint8_t)((g->dcnt & 3) << 5) + 11)]) & 1) {
            g->source.fcb[12] = 0;
            setcuser();
            return 1;
        }
        searchn();
    }
    g->source.fcb[12] = 0;
    setcuser();
    return 0;
}

static void multcopy(void) {
    /* initialize counters if not error retry */
    if (rel(g->eretry == 0) & 1) mc_nextdir = mc_ncopied = 0;

    for (;;) {
        /* find a matching entry */
        setsuser(); /* source user */
        setdma(bp_buff);
        bp[bp_fcb + 12] = 0;
        search(bp_fcb);
        mc_ndcnt = 0;
        while ((rel(g->dcnt != 255) & rel(mc_ndcnt < mc_nextdir)) & 1) {
            mc_ndcnt++;
            searchn();
        }
        setcuser();
        /* file control block in buffer */
        if (rel(g->dcnt == 255) & 1) {
            if (rel(mc_ncopied == 0) & 1) error(10, 0, 0, bp_fcb); /* file not found */
            if ((uint8_t)~KILDS & 1) crlf();
            return;
        }
        mc_nextdir = (uint16_t)(mc_ndcnt + 1);
        /* get the file control block name to dest */
        move((uint16_t)(bp_buff + (uint8_t)((g->dcnt & 3) << 5) + 1), PIP_ODEST + 1, 15);
        move(PIP_ODEST + 1, PIP_SOURCE + 1, 15); /* fill both fcb's */
        {
            uint8_t not_archiv = (uint8_t)~ARCHIV;
            if ((not_archiv | archck()) & 1) {
                g->odest.fcb[12] = 0;
                if ((RSYS | (uint8_t)~rol1(g->odest.fcb[10])) & 1) { /* ok to read */
                    if ((uint8_t)~KILDS & 1) { /* kill display option */
                        if (rel(mc_ncopied == 0) & 1) print(PIP_ARR737); /* COPYING - */
                        prname();
                    }
                    mc_ncopied++;
                    simplecopy();
                }
            }
        }
    }
}

/* error if same user and same disk */
static void ck_disk(void) {
    if ((rel(g->odest.user == g->source.user) & rel(g->odest.fcb[0] == g->source.fcb[0])) & 1) formerr();
}

static void ck_eol(void) {
    deblank();
    if (rel(g->ch != CR) & 1) formerr();
}

static void scandest(uint16_t copyfcb) {
    ck_eol();
    move(PIP_SOURCE + 1, copyfcb, 35);
    ck_disk();
}

static void scaneql(void) {
    scan(PIP_SOURCE);
    if ((uint8_t)~(rel(g->type == T_SPECL) & rel(g->ch == '=')) & 1) formerr();
}

void pip_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"PIP     ", (const uint8_t *)&pip_image, pip_image_len});
    g_ds = ds;
    g = RSP_DATA(pip_data, ds);
    bp = g->base;

    /* buffer at 80h contains remainder of line typed following the
     * command 'PIP' - if zero then prompt til cr */
    move(bp_buff, PIP_CBUFF + 1, 0x80);
    g->multcom = rel(COMLEN == 0);

    if (g->multcom & 1) {
        printx(PIP_ARR760); /* MP/M-86 PIP VERSION 2.0 */
        crlf();
    }

    /* get current cp/m version */
    if (rel(cversion() < VERSION) & 1) {
        print(PIP_ARR764); /* REQUIRES MP/M-86 */
        boot();
    }
    g->cuser = getuser(); /* get current user */
    g->cdisk = getdisk(); /* get current disk */
    errmode(255);         /* set return error mode */
    g->eretry = 0;

    /* RETRY: enter here on error exit from the procedure 'error' */
    (void)PIP_SETJMP();
    if (rel(g->eretry != 0) & 1) {
        multcopy();
        COMLEN = g->multcom;
    }
    size_memory();
    /* main processing loop. process until cr only */
    for (;;) {
        g->c1 = g->c2 = g->c3 = 0; /* line count = 000000 */
        g->putnum = T;             /* acts like lf occurred on ascii file */
        g->concnt = g->column = 0; /* printer tabs */
        g->lineno = 254;           /* incremented to 255 > pagcnt */
        /* read from console if not a oneliner */
        if (g->multcom & 1) {
            printchar('*');
            rdcom();
            crlf();
        }
        g->cbp = 255;
        if (rel(COMLEN == 0) & 1) /* single carriage return */
            boot();                /* normal exit from pip here */

        /* look for special cases first */
        g->psource = g->pdest = 0;
        g->fastcopy = F;
        g->getpw = T;
        scan(PIP_ODEST);
        if (rel(g->type == T_PERIPH) & 1) goto simplecom;
        if (rel(g->type == T_DISKNAME) & 1) {
            scaneql();
            scan(PIP_SOURCE);
            /* may be multi copy */
            if (rel(g->type != T_FILE) & 1) formerr();
            if (g->ambig & 1) { /* form is a:=b:afn */
                bp[bp_fcb] = g->source.fcb[0];
                scandest(bp_fcb + 1);
                multcopy();
            } else {
                scandest(PIP_ODEST + 1);
                /* form is a:=b:ufn */
                simplecopy();
            }
            goto endcom;
        }

        if ((rel(g->type != T_FILE) | g->ambig) & 1) formerr();
        scaneql();
        scan(PIP_SOURCE);
        if (rel(g->type == T_DISKNAME) & 1) {
            ck_disk();
            move(PIP_ODEST + 1, PIP_SOURCE + 1, FRSIZE + NSIZE);
            ck_eol();
            simplecopy();
            goto endcom;
        }
        /* may be possible to do a fast disk copy */
        if (rel(g->type == T_FILE) & 1) { /* file to file */
            deblank();
            if (rel(g->ch != CR) & 1) goto simplecom;
            /* file to file */
            simplecopy();
            goto endcom;
        }

    simplecom:
        g->cbp = 255; /* ready for rescan */

        /* otherwise process simple request */
        scan(PIP_ODEST);
        if ((rel(g->type < T_FILE) | g->ambig) & 1) /* delimiter or error */
            error(3, 0, 0, 0);                      /* invalid destination */

        g->dhex = F;
        if (rel(g->type == T_FILE) & 1) { /* destination is a file, save extent name */
            setupdest();
            g->ch = 255;
        } else if ((rel(g->ch >= NULP) | rel(g->ch <= RDR)) & 1) { /* peripheral name */
            error(3, 0, 0, 0); /* invalid destination */
        }
        if (rel((g->pdest = (uint8_t)(g->ch + 1)) == PUNP) & 1) nulls();
        /* now scan the delimiter */
        scan(PIP_SOURCE);
        if ((rel(g->type != T_SPECL) | rel(g->ch != '=')) & 1) formerr(); /* invalid format */

        /* otherwise scan and copy until cr */
        g->copying = T;
        while (g->copying & 1) {
            scan(PIP_SOURCE);
            g->scom = F;
            if ((rel(g->type == T_FILE) & (uint8_t)~g->ambig) & 1) { /* a source file */
                setupsource();
                g->ch = 255;
            } else if ((rel(g->type != T_PERIPH) | (rel(g->ch <= LST) & rel(g->ch > RDR))) & 1) {
                error(4, 0, 0, 0); /* invalid source */
            }

            g->scom = g->scom | OBJ; /* may be absolute copy */
            g->psource = (uint8_t)(g->ch + 1);
            if (rel(g->ch == NULP) & 1) {
                nulls();
            } else if (rel(g->ch == EOFP) & 1) {
                putdest(ENDFILE);
            } else { /* disk copy */
                /* hex file set if source is rdr and dest is hex file */
                if ((rel(g->ch < RDR) & g->dhex) & 1) HEXT = 1;
                if ((rel(g->pdest == 10) | rel(g->pdest == 11)) & 1) { /* periph = prn or lst */
                    if (rel(conatlst() == 255) & 1) error(15, 0, 0, 0); /* printer busy */
                    if (rel(g->pdest == PRNT) & 1) {
                        NUMB = 1;
                        if (rel(TABS == 0) & 1) TABS = 8;
                        if (rel(PAGCNT == 0) & 1) PAGCNT = 1;
                    }
                }
                copychar();
            }

            ck_strings();
            /* read endfile, go to next source */
            if (rel(g->type == T_FILE) & 1) {
                setsuser();
                close(PIP_SOURCE);
                setcuser();
            }
            scan(PIP_SOURCE);
            if ((rel(g->type != T_SPECL) | (rel(g->ch != ',') & rel(g->ch != CR))) & 1)
                error(16, 0, 0, 0); /* invalid separator */

            g->copying = rel(g->ch != CR);
        }

        /* if necessary, close file or punch trailer */
        if (rel(g->pdest == PUNP) & 1) {
            putdest(ENDFILE);
            nulls();
        }
        if (rel(g->pdest == 0) & 1) /* file has to be closed and renamed */
            closedest();

        /* comlen set to 0 if not processing multiple commands */
    endcom:
        COMLEN = g->multcom;
    }
}
