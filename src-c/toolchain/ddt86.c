/*
 * ddt86.c -- C version of src-assem/toolchain/DDT86.A86 (a transient,
 * DDT86.CMD): DRI's DDT86 1.1 debugger, the command loop and every
 * command but the three that run user code:
 *   A  assemble (the assembler overlay, ass86.c)
 *   B  block compare           D, DW  display memory
 *   E  load a program (MP/M function 59, program load), then its
 *      segments (as V) and a cleared fcb (as I)
 *   F, FW  fill               H  hex sum and difference
 *   I  the fcbs and command tail (DDT86's base page, and the loaded
 *      program's after an E)
 *   L  list (the disassembler overlay, dis86.c)
 *   M  move                   R  read a file into memory
 *   S, SW  set memory         V  the last R's or E's extent
 *   W  write memory to a file
 *   X  examine and set the user's registers and flags
 *
 * G, T and U (go, trace, untrace) can't work here: they run or trace
 * user 8086 code, with breakpoints through int 3 and single steps
 * through int 1 and the interrupt vector table, and the C kernel runs
 * only C. Each is refused exactly as DDT86 refuses an invalid command:
 * CR LF '?', then the prompt. The breakpoint and single-step vectors
 * are not set at start-up either (bpvect's stores into 0:4-0:0Fh), but
 * their old contents are saved into vectorsave as DDT86 does
 * (savevect), so the data image matches.
 *
 * DDT86 is an 8080-model program: code and data in one segment, `ds`.
 * Its data starts at endcs (TC_DDT86_ENDCS): ddt86_image below, at the
 * real offsets (toolchain_syms.h), which ddt86_main installs; the code
 * area below endcs is zeros in the image, except that DDT86's user
 * register save area (userax..userfl, savess, savesp, breakfl) is data
 * kept in its code segment, just below endcs, and is reached there too.
 * Every variable lives in that memory (S can patch DDT86's own data,
 * COL40 or NLINES say, as on the real thing); the C keeps nothing in
 * statics that DDT86 keeps in memory, except the stack. Its messages and
 * name tables (in DDT86's code) are C constants.
 *
 * Every BDOS call is rt_mpm. DDT86 copies the BDOS interrupt vector
 * (224) to the next (225) at start-up and makes its own calls through
 * that one (setbdosint); the C makes the same copy in the 8086 IVT, then
 * calls rt_mpm, function and DX as DDT86's int 225 has them. The CCP's
 * SS:SP and the flags DDT86 records at start-up (for X and G) are the
 * 8080-model loader's: SS = DS, SP = 100h (DDT86_CCP_SP), interrupts on
 * (DDT86_SYSFLAGS, LOAD's initial flags).
 *
 * DDT86's `jmp start`s (err, errm, ctlchek's abort, badver, plmset's
 * wrap) unwind to ddt86_main's setjmp; the assembler's error exit
 * (asment, ddt_asment) to the A command's. Everything they leave is in
 * the image. The overlays call back through the ddt_* services
 * (toolchain.h): DDT86's vectors 103h-10Fh.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "toolchain.h"
#include "toolchain_syms.h"

#if __STDC_HOSTED__
#include <setjmp.h>
static jmp_buf start_jump, asm_jump;
#define DDT_SETJMP(b)  setjmp(b)
#define DDT_LONGJMP(b) longjmp(b, 1)
#else
static void *start_jump[5], *asm_jump[5];
#define DDT_SETJMP(b)  __builtin_setjmp(b)
#define DDT_LONGJMP(b) __builtin_longjmp(b, 1)
#endif

/* DDT86's user register save area, in its code segment just below
 * endcs (userax..userfl: 14 words, then savess, savesp, breakfl) */
#define DDT86_USERREG DDT86_USERAX
#define R_AX 0
#define R_SP 4
#define R_CS 8
#define R_DS 9
#define R_SS 10
#define R_ES 11
#define R_IP 12
#define R_FL 13
#define UREG(n) ((uint16_t)(DDT86_USERREG + 2 * (n)))

/* the base page */
#define FCB  0x5C
#define FCB2 0x6C
#define BUFF 0x80

#define BDOSI 224          /* bdosi: the BDOS interrupt (DDT86 uses bdosi+1) */
#define DDT86_CCP_SP 0x0100 /* the loader's SP (SS = DS) */
#define DDT86_SYSFLAGS 0x0200 /* the flags DDT86 starts with: IF set */
#define DDT86_ASMSP (TC_DDT86_DATA_END - 2) /* SP in the A command (stackp less one call) */

#define CONBUFFMAX 64
#define TOTREG 13
#define NFLAG 9
#define CR 0x0D
#define LF 0x0A
#define EOL CR
#define CTLS 0x13

/* DDT86's data, from endcs, as DDT86.A86's dseg lays it out */
typedef struct __attribute__((packed)) {
    uint8_t code[TC_DDT86_ENDCS]; /* the base page and DDT86's code: zeros */
    uint16_t ccpss, ccpsp;
    uint8_t conbuffhdr, conbuffcnt, conbuff[CONBUFFMAX + 1];
    uint16_t conptr;
    uint8_t assempresent;
    uint16_t asmspsav;
    uint16_t disloc, disseg, dismax, tdisp;
    uint8_t linemax, nd80, nd40, ndw40, nlines;
    uint16_t goloc, goseg;
    uint8_t vectorsave[12], bpcnt;
    uint16_t brk1loc, brk1seg;
    uint8_t brk1byt;
    uint16_t brk2loc, brk2seg;
    uint8_t brk2byt;
    uint16_t lasloc, lasseg, lasmax;
    uint8_t lascntsw, lascnt, disempresent;
    uint16_t startreadoff, startreadseg, endreadoff, endreadseg, dmaoff, dmaseg;
    uint16_t tracecount;
    uint8_t traceprint, skipbdos, userifoff;
    uint16_t startwriteoff, startwriteseg, endwriteoff, endwriteseg;
    uint8_t nreg, segflag;
    uint16_t regnum;
    uint8_t xtemp;
    uint16_t type1loc, type1seg, type2loc, type2seg, usermax, userloc2, userseg2;
    uint16_t mcbbase_, mcblen_;
    uint8_t mcbext_, sysif, numreq, wmode, mode, savevecflag, col40, errmode;
    uint8_t basepagesave[48];
    uint8_t delims[11], lastchar;
    uint16_t fcbadr;
    uint8_t stack[96];
} ddt86_data;

#define CHK(f, o) _Static_assert(offsetof(ddt86_data, f) == (o), #f)
CHK(ccpss, DDT86_CCPSS);
CHK(ccpsp, DDT86_CCPSP);
CHK(conbuffhdr, DDT86_CONBUFFHDR);
CHK(conbuffcnt, DDT86_CONBUFFCNT);
CHK(conbuff, DDT86_CONBUFF);
CHK(conptr, DDT86_CONPTR);
CHK(assempresent, DDT86_ASSEMPRESENT);
CHK(asmspsav, DDT86_ASMSPSAV);
CHK(disloc, DDT86_DISLOC);
CHK(disseg, DDT86_DISSEG);
CHK(dismax, DDT86_DISMAX);
CHK(tdisp, DDT86_TDISP);
CHK(linemax, DDT86_LINEMAX);
CHK(nd80, DDT86_ND80);
CHK(nd40, DDT86_ND40);
CHK(ndw40, DDT86_NDW40);
CHK(nlines, DDT86_NLINES);
CHK(goloc, DDT86_GOLOC);
CHK(goseg, DDT86_GOSEG);
CHK(vectorsave, DDT86_VECTORSAVE);
CHK(bpcnt, DDT86_BPCNT);
CHK(brk1loc, DDT86_BRK1LOC);
CHK(brk1seg, DDT86_BRK1SEG);
CHK(brk1byt, DDT86_BRK1BYT);
CHK(brk2loc, DDT86_BRK2LOC);
CHK(brk2seg, DDT86_BRK2SEG);
CHK(brk2byt, DDT86_BRK2BYT);
CHK(lasloc, DDT86_LASLOC);
CHK(lasseg, DDT86_LASSEG);
CHK(lasmax, DDT86_LASMAX);
CHK(lascntsw, DDT86_LASCNTSW);
CHK(lascnt, DDT86_LASCNT);
CHK(disempresent, DDT86_DISEMPRESENT);
CHK(startreadoff, DDT86_STARTREADOFF);
CHK(startreadseg, DDT86_STARTREADSEG);
CHK(endreadoff, DDT86_ENDREADOFF);
CHK(endreadseg, DDT86_ENDREADSEG);
CHK(dmaoff, DDT86_DMAOFF);
CHK(dmaseg, DDT86_DMASEG);
CHK(tracecount, DDT86_TRACECOUNT);
CHK(traceprint, DDT86_TRACEPRINT);
CHK(skipbdos, DDT86_SKIPBDOS);
CHK(userifoff, DDT86_USERIFOFF);
CHK(startwriteoff, DDT86_STARTWRITEOFF);
CHK(startwriteseg, DDT86_STARTWRITESEG);
CHK(endwriteoff, DDT86_ENDWRITEOFF);
CHK(endwriteseg, DDT86_ENDWRITESEG);
CHK(nreg, DDT86_NREG);
CHK(segflag, DDT86_SEGFLAG);
CHK(regnum, DDT86_REGNUM);
CHK(xtemp, DDT86_XTEMP);
CHK(type1loc, DDT86_TYPE1LOC);
CHK(type1seg, DDT86_TYPE1SEG);
CHK(type2loc, DDT86_TYPE2LOC);
CHK(type2seg, DDT86_TYPE2SEG);
CHK(usermax, DDT86_USERMAX);
CHK(userloc2, DDT86_USERLOC2);
CHK(userseg2, DDT86_USERSEG2);
CHK(mcbbase_, DDT86_MCBBASE);
CHK(mcblen_, DDT86_MCBLEN);
CHK(mcbext_, DDT86_MCBEXT);
CHK(sysif, DDT86_SYSIF);
CHK(numreq, DDT86_NUMREQ);
CHK(wmode, DDT86_WMODE);
CHK(mode, DDT86_MODE);
CHK(savevecflag, DDT86_SAVEVECFLAG);
CHK(col40, DDT86_COL40);
CHK(errmode, DDT86_ERRMODE);
CHK(basepagesave, DDT86_BASEPAGESAVE);
CHK(delims, DDT86_DELIMS);
CHK(lastchar, DDT86_LASTCHAR);
CHK(fcbadr, DDT86_FCBADR);
_Static_assert(sizeof(ddt86_data) == DDT86_STACKP, "stackp");
_Static_assert(sizeof(ddt86_data) == TC_DDT86_DATA_END, "length");
_Static_assert(DDT86_MCB == DDT86_MCBBASE, "mcb");

const uint16_t ddt86_image_len = sizeof(ddt86_data);
const ddt86_data ddt86_image = {
    .conbuffhdr = CONBUFFMAX,
    .assempresent = 1,
    .nd80 = 16,
    .nd40 = 6,
    .ndw40 = 8,
    .nlines = 12,
    .disempresent = 1,
    .segflag = 1,
    .mode = 'I',
    .delims = {' ', '=', '.', ',', ':', ';', '[', ']', '<', '>', EOL},
};

/* DDT86's messages and name tables (in its code): the last character
 * of each has its parity bit set */
#define HI(c) ((uint8_t)((c) | 0x80))
static const uint8_t signon[] = {'D', 'D', 'T', '8', '6', ' ', '1', '.', HI('1')};
static const uint8_t regname[] = {'A', HI('X'), 'B', HI('X'), 'C', HI('X'), 'D', HI('X'), 'S', HI('P'),
                                  'B', HI('P'), 'S', HI('I'), 'D', HI('I'),
                                  /* segreg */ 'C', HI('S'), 'D', HI('S'), 'S', HI('S'), 'E', HI('S'),
                                  'I', HI('P'),
                                  /* flagname, which chkreg's scan reaches */ 'O', 'D'};
#define SEGREG (regname + 16)
static const uint8_t flagname[] = {'O', 'D', 'I', 'T', 'S', 'Z', 'A', 'P', 'C',
                                   /* flagbits, which checkflag's scan reaches */ 5};
static const uint8_t flagbits[] = {5, 6, 7, 8, 9, 10, 12, 14, 16};
static const uint8_t segnames[] = {'C', HI('S'), 'D', HI('S'), 'E', HI('S'), 'S', HI('S'),
                                   'X', HI('1'), 'X', HI('2'), 'X', HI('3'), 'X', HI('4')};
static const uint8_t closem[] = {CR, LF, 'C', 'A', 'N', 'N', 'O', 'T', ' ', 'C', 'L', 'O', 'S', HI('E')};
static const uint8_t loadm[] = {CR, LF, 'I', 'N', 'S', 'U', 'F', 'F', 'I', 'C', 'I', 'E', 'N', 'T', ' ',
                                'M', 'E', 'M', 'O', 'R', HI('Y')};
static const uint8_t makem[] = {CR, LF, 'N', 'O', ' ', 'S', 'P', 'A', 'C', HI('E')};
static const uint8_t memm[] = {CR, LF, 'M', 'E', 'M', 'O', 'R', 'Y', ' ', 'R', 'E', 'Q', 'U', 'E', 'S', 'T',
                               ' ', 'D', 'E', 'N', 'I', 'E', HI('D')};
static const uint8_t openm[] = {CR, LF, 'N', 'O', ' ', 'F', 'I', 'L', HI('E')};
static const uint8_t readm[] = {' ', ' ', 'S', 'T', 'A', 'R', 'T', ' ', ' ', ' ', ' ', ' ', ' ', 'E', 'N',
                                HI('D')};
static const uint8_t verm[] = {CR, LF, 'V', 'E', 'R', 'I', 'F', 'Y', ' ', 'E', 'R', 'R', 'O', 'R', ' ', 'A',
                               'T', HI(' ')};
static const uint8_t writem[] = {CR, LF, 'D', 'I', 'S', 'K', ' ', 'W', 'R', 'I', 'T', 'E', ' ', 'E', 'R',
                                 'R', 'O', HI('R')};
static const uint8_t cmdtype[] = {'C', 'M', 'D'};

static uint16_t g_ds;
static uint8_t *m; /* DS:0 */

#define D8(a) m[(uint16_t)(a)]
static uint16_t D16(uint16_t a) { return (uint16_t)(m[a] | (m[(uint16_t)(a + 1)] << 8)); }
static void S16(uint16_t a, uint16_t v) {
    m[a] = (uint8_t)v;
    m[(uint16_t)(a + 1)] = (uint8_t)(v >> 8);
}
#define INC16(a) S16((a), (uint16_t)(D16(a) + 1))

/* user memory, seg:off */
static uint8_t *at(uint16_t seg, uint16_t off) { return far_ptr(rt_phys(), seg, off); }

static _Noreturn void start(void) { DDT_LONGJMP(start_jump); }

/* ---- the BDOS interface (DDT86's int bdosi+1) ---- */

static mpm_regs bdos(uint8_t cl, uint16_t dx) { return rt_mpm(g_ds, cl, dx); }

static void printm(const uint8_t *s);

static _Noreturn void errm(const uint8_t *msg) {
    printm(msg);
    start();
}

static void conout(uint8_t c) { (void)bdos(2, c); }
static uint8_t consin(void) { return (uint8_t)bdos(1, 0).bx; }
static uint8_t constat(void) { return (uint8_t)bdos(11, 0).bx; }

static void open_(uint16_t fcb) {
    if ((uint8_t)(bdos(15, fcb).bx + 1) == 0) errm(openm);
}

static void close_(uint16_t fcb) {
    if ((uint8_t)(bdos(16, fcb).bx + 1) == 0) errm(closem);
}

static void setdma(uint16_t off) { (void)bdos(26, off); }
static void setdmab(uint16_t seg) { (void)bdos(51, seg); }
static void freemem(uint16_t mcb) { (void)bdos(57, mcb); }

/* ---- console i/o ---- */

static void getline(void) {
    (void)bdos(10, DDT86_CONBUFFHDR);
    D8(DDT86_CONBUFF + D8(DDT86_CONBUFFCNT)) = EOL;
    S16(DDT86_CONPTR, 0);
}

static uint8_t upper(uint8_t c) { return (c >= 'a' && c <= 'z') ? (uint8_t)(c & 0x5F) : c; }

static uint8_t conin(void) {
    uint16_t p = D16(DDT86_CONPTR);
    uint8_t c = D8(DDT86_CONBUFF + p);
    S16(DDT86_CONPTR, (uint16_t)(p + 1));
    return upper(c);
}

#define DEC_CONPTR() S16(DDT86_CONPTR, (uint16_t)(D16(DDT86_CONPTR) - 1))

static void ctlchek(void) {
    if (constat() == 0) return;
    if (consin() == CTLS) {
        (void)consin(); /* wait for another key */
        return;
    }
    start(); /* any other key restarts */
}

static void crlf(void) {
    conout(CR);
    conout(LF);
}

static void crlfchk(void) {
    crlf();
    ctlchek();
}

static void blank(void) { conout(' '); }

static void tabs(uint16_t n) {
    do blank();
    while (--n);
}

static void printm(const uint8_t *s) {
    for (;; s++) {
        if (*s & 0x80) {
            conout(*s & 0x7F);
            return;
        }
        conout(*s);
    }
}

static void ascout(uint8_t c) { conout((c < ' ' || c > 0x7E) ? '.' : c); }

static void printnibble(uint8_t n) {
    n &= 0x0F;
    conout((uint8_t)(n < 10 ? '0' + n : 'A' + n - 10));
}

static void printbyte(uint8_t b) {
    printnibble(b >> 4);
    printnibble(b);
}

static void printword(uint16_t w) {
    printbyte((uint8_t)(w >> 8));
    printbyte((uint8_t)w);
}

static void printdword(uint16_t seg, uint16_t off) {
    printword(seg);
    conout(':');
    printword(off);
}

/* the byte or word at seg:off, as wmode says */
static void print8or16(uint16_t seg, uint16_t off) {
    uint16_t w = (uint16_t)(*at(seg, off) | (*at(seg, (uint16_t)(off + 1)) << 8));
    if (D8(DDT86_WMODE) & 1)
        printword(w);
    else
        printbyte((uint8_t)w);
}

static _Noreturn void err(void) {
    crlf();
    conout('?');
    start();
}

/* ---- file name parsing ---- */

static void fill(uint16_t *di, uint16_t *cx, uint8_t c) {
    for (; *cx; --*cx) D8((*di)++) = c;
}

static int pdelim(uint8_t c) {
    for (uint16_t i = 0; i < 11; i++)
        if (D8(DDT86_DELIMS + i) == c) return 1;
    return 0;
}

/* a field of the name, cx characters at di (blank filled; '*' fills
 * with '?') */
static void fillfield(uint16_t cx, uint16_t di) {
    for (;;) {
        uint8_t c = conin();
        D8(DDT86_LASTCHAR) = c;
        if (c == '*') {
            fill(&di, &cx, '?');
            continue;
        }
        if (pdelim(c)) {
            fill(&di, &cx, ' ');
            return;
        }
        if (cx == 0) err();
        D8(di++) = c;
        cx--;
    }
}

/* fcb byte 0 from a d: prefix; -> the name's first byte */
static uint16_t setupdisk(uint16_t di) {
    uint8_t c;
    do {
        c = conin();
        D8(DDT86_LASTCHAR) = c;
    } while (c == ' ');
    if (c != EOL) {
        if (conin() == ':') {
            D8(di - 1) = (uint8_t)(D8(DDT86_LASTCHAR) - ('A' - 1));
            return di;
        }
        DEC_CONPTR();
    }
    DEC_CONPTR();
    return di;
}

/* parse a name into the fcb at di without clearing it; -> the last
 * character scanned; *q = a '?' in the name */
static uint8_t parse2(uint16_t di, int *q) {
    S16(DDT86_FCBADR, di);
    di = setupdisk((uint16_t)(di + 1));
    fillfield(8, di);
    if (D8(DDT86_LASTCHAR) == '.') {
        fillfield(3, (uint16_t)(di + 8));
    } else {
        uint16_t t = (uint16_t)(di + 8), n = 3;
        fill(&t, &n, ' ');
    }
    /* scanq */
    int found = 0;
    for (uint16_t i = 0; i < 11; i++)
        if (D8(D16(DDT86_FCBADR) + 1 + i) == '?') found = 1;
    if (q) *q = found;
    return D8(DDT86_LASTCHAR);
}

static uint8_t parse(uint16_t di, int *q) {
    for (uint16_t i = 0; i < 36; i++) D8(di + i) = 0;
    return parse2(di, q);
}

/* ---- the user's CPU state ---- */

static uint16_t ureg(int n) { return D16(UREG(n)); }

static void chkreg(uint16_t name) {
    for (uint16_t i = 0; i < TOTREG + 1; i++) {
        if ((uint16_t)(regname[2 * i] | (regname[2 * i + 1] << 8)) == name) {
            if (i == TOTREG) break; /* found with the count run out: jcxz */
            S16(DDT86_REGNUM, i);
            return;
        }
    }
    err();
}

static void checkflag(uint8_t c) {
    for (uint16_t i = 0; i < NFLAG + 1; i++) {
        if (flagname[i] == c) {
            if (i == NFLAG) break;
            S16(DDT86_REGNUM, i);
            return;
        }
    }
    err();
}

static uint16_t ror16(uint16_t v, uint8_t n) {
    n &= 15;
    return n ? (uint16_t)((v >> n) | (v << (16 - n))) : v;
}

static int getflag(uint16_t n) { return (ror16(1, flagbits[n]) & ureg(R_FL)) != 0; }

static void setflag(uint16_t n, uint16_t v) {
    uint8_t cl = flagbits[n];
    uint16_t fl = (uint16_t)(ureg(R_FL) & ror16(0xFFFE, cl));
    S16(UREG(R_FL), (uint16_t)(fl | ror16(v, cl)));
}

static void printflags(void) {
    for (uint16_t i = 0; i < 9; i++) conout(getflag(i) ? flagname[i] : '-');
}

static void printregval(uint16_t n) { printword(ureg(n)); }
static void printregname(uint16_t n) { printm(regname + 2 * n); }

/* print this register? (the segment registers only with segflag) */
static int testregcl(uint8_t cl) { return D8(DDT86_SEGFLAG) || cl > 11 || cl < 8; }

static void printregs_from(uint8_t cl, uint8_t nreg) {
    D8(DDT86_NREG) = nreg;
    do {
        if (testregcl(cl)) {
            printregval(cl);
            blank();
        }
        cl++;
    } while (cl < D8(DDT86_NREG));
}

static void regheader_from(uint8_t cl, uint8_t nreg) {
    D8(DDT86_NREG) = nreg;
    do {
        if (testregcl(cl)) {
            printregname(cl);
            tabs(3);
        }
        cl++;
    } while (cl < D8(DDT86_NREG));
}

static void setuphdr(void) {
    crlf();
    tabs(11);
}

static void printinstr(void) {
    uint16_t es = ureg(R_CS), si = ureg(R_IP);
    if (D8(DDT86_DISEMPRESENT)) {
        if (D8(DDT86_SEGFLAG)) crlf();
        (void)dis86_disem(g_ds, si, es);
    } else {
        printbyte(*at(es, si));
    }
}

static void printseginfo(uint8_t n) {
    uint16_t si = (uint16_t)(DDT86_BASEPAGESAVE + n * 6);
    uint16_t len = D16(si);
    uint8_t hi = D8(si + 2);
    if ((len | hi) == 0) return;
    uint16_t base = D16(si + 3);
    crlf();
    printm(segnames + 2 * n);
    blank();
    printdword(base, 0);
    blank();
    printdword((uint16_t)(base + ((uint16_t)hi << 12)), len);
}

/* the user's registers after E: bx = the loaded program's base page */
static void setcpustate(uint16_t bx) {
    S16(UREG(R_SS), D16(DDT86_CCPSS));
    S16(UREG(R_SP), D16(DDT86_CCPSP));
    S16(UREG(R_DS), bx);
    S16(UREG(R_CS), bx);
    S16(UREG(R_ES), bx);
    S16(DDT86_TYPE1SEG, bx);
    S16(DDT86_TYPE2SEG, bx);
    uint8_t *bp = at(bx, 0);
    uint16_t ax;
    if (bp[5] & 1) {
        ax = 0x100; /* 8080 model */
    } else {
        S16(UREG(R_CS), rd16(bp + 3));
        S16(DDT86_TYPE1SEG, rd16(bp + 3));
        if (rd16(bp + 15)) S16(UREG(R_ES), rd16(bp + 15));
        ax = rd16(bp + 21);
        if (ax) {
            S16(UREG(R_SS), ax);
            S16(UREG(R_SP), rd16(bp + 18));
            ax = 0;
        }
    }
    S16(UREG(R_IP), ax);
    S16(DDT86_LASLOC, ax);
}

static void setdefseg(void) {
    S16(DDT86_LASLOC, ureg(R_IP));
    S16(DDT86_TYPE1SEG, ureg(R_CS));
}

/* ---- miscellaneous ---- */

static int delim(uint8_t c) { return c == EOL || c == ',' || c == ' ' || c == ':'; }

static uint8_t hexcon(uint8_t c) {
    c = (uint8_t)(c - '0');
    if (c < 10) return c;
    c = (uint8_t)(c + (uint8_t)('0' - 'A' + 10));
    if (c >= 16 || c < 10) err();
    return c;
}

/* store and verify the byte at seg:off */
static void setbyte(uint16_t seg, uint16_t off, uint8_t b) {
    uint8_t *p = at(seg, off);
    *p = b;
    if (*p != b) {
        printm(verm);
        printdword(seg, off);
        start();
    }
}

static void set8or16(uint16_t seg, uint16_t off, uint16_t v) {
    setbyte(seg, off, (uint8_t)v);
    if (D8(DDT86_WMODE) & 1) {
        off++;
        if (off != 0) setbyte(seg, off, (uint8_t)(v >> 8));
    }
}

/* add 1 or 2 (wmode) to the word at a; -> it wrapped */
static int inc1or2(uint16_t a) {
    uint16_t n = (uint16_t)((D8(DDT86_WMODE) & 1) + 1);
    uint16_t v = (uint16_t)(D16(a) + n);
    S16(a, v);
    return v < n;
}

typedef struct {
    uint16_t bx;
    uint8_t al, ah; /* the delimiter; 1 = a number was there */
} number;

static number getnumber(void) {
    number r = {0, 0, 0};
    for (;;) {
        r.al = conin();
        if (delim(r.al)) return r;
        r.bx = (uint16_t)(r.bx << 4);
        r.bx = (uint16_t)((r.bx & 0xFF00) | (uint8_t)(r.bx + hexcon(r.al)));
        r.ah = 1;
    }
}

static number getoffset(void) {
    number r = getnumber();
    if (!r.ah || r.al == ':') err();
    return r;
}

static uint16_t getlastoffset(void) {
    number r = getoffset();
    if (r.al != EOL) err();
    return r.bx;
}

static void checkword(void) {
    if (conin() == 'W')
        D8(DDT86_WMODE) = 1;
    else
        DEC_CONPTR();
}

/* a segment register prefix (sr:)? -> its value in *dx */
static int checksegreg(uint16_t *dx) {
    uint16_t save = D16(DDT86_CONPTR);
    uint8_t c1 = conin(), c2 = conin(), c3 = conin();
    if (c3 == ':') {
        uint16_t bx = (uint16_t)(c1 | ((c2 | 0x80) << 8));
        for (uint16_t si = 0; si < 8; si += 2) {
            if ((uint16_t)(SEGREG[si] | (SEGREG[si + 1] << 8)) == bx) {
                *dx = D16((uint16_t)(UREG(R_CS) + si));
                return 1;
            }
        }
    }
    S16(DDT86_CONPTR, save);
    return 0;
}

/* a location: <empty>, nnnn, sr:nnnn or mmmm:nnnn, into the offset and
 * segment words at di; -> the delimiter */
static uint8_t getsegandoff(uint16_t di) {
    uint16_t dx;
    if (!checksegreg(&dx)) {
        number n = getnumber();
        if (!n.ah) {
            if (D8(DDT86_NUMREQ)) err();
            return n.al;
        }
        dx = n.bx;
        if (n.al != ':') {
            S16(di, dx);
            return n.al;
        }
    }
    number o = getoffset();
    S16(di, o.bx);
    S16((uint16_t)(di + 2), dx);
    return o.al;
}

/* ---- disk i/o ---- */

static void loaderr(void) { errm(loadm); }

static void readfile(void) {
    uint16_t ax = D16(DDT86_MCBBASE);
    S16(DDT86_STARTREADSEG, ax);
    S16(DDT86_ENDREADSEG, ax);
    S16(DDT86_DMASEG, ax);
    S16(DDT86_STARTREADOFF, 0);
    S16(DDT86_ENDREADOFF, 0);
    for (;;) {
        setdmab(D16(DDT86_DMASEG));
        S16(DDT86_DMAOFF, 0);
        do {
            setdma(D16(DDT86_DMAOFF));
            if (D16(DDT86_MCBLEN) < 8) loaderr();
            if ((uint8_t)bdos(20, FCB).bx != 0) {
                close_(FCB);
                return;
            }
            S16(DDT86_MCBBASE, (uint16_t)(D16(DDT86_MCBBASE) + 8));
            S16(DDT86_MCBLEN, (uint16_t)(D16(DDT86_MCBLEN) - 8));
            ax = D16(DDT86_DMAOFF);
            S16(DDT86_ENDREADOFF, (uint16_t)((ax & 0xFF00) | (uint8_t)(ax + 0x7F)));
            S16(DDT86_ENDREADSEG, D16(DDT86_DMASEG));
            S16(DDT86_DMAOFF, (uint16_t)(D16(DDT86_DMAOFF) + 0x80));
        } while (D16(DDT86_DMAOFF) != 0);
        S16(DDT86_DMASEG, (uint16_t)(D16(DDT86_DMASEG) + 0x1000));
    }
}

static void writefile(void) {
    uint16_t ax = (uint16_t)((D16(DDT86_STARTWRITEOFF) >> 4) + D16(DDT86_STARTWRITESEG));
    S16(DDT86_MCBBASE, ax);
    S16(DDT86_DMASEG, ax);
    uint16_t bx = ax;
    ax = (uint16_t)((D16(DDT86_ENDWRITEOFF) >> 4) + D16(DDT86_ENDWRITESEG));
    if (ax < bx) err();
    S16(DDT86_MCBLEN, (uint16_t)(ax - bx));
    uint16_t r = bdos(19, FCB).bx; /* delete */
    if (D8(DDT86_ERRMODE) && (uint8_t)(r + 1) == 0 && (r >> 8) != 0) err();
    if ((uint8_t)(bdos(22, FCB).bx + 1) == 0) errm(makem);
    for (;;) {
        setdmab(D16(DDT86_DMASEG));
        S16(DDT86_DMAOFF, 0);
        do {
            setdma(D16(DDT86_DMAOFF));
            if ((uint8_t)bdos(21, FCB).bx != 0) errm(writem);
            uint16_t len = D16(DDT86_MCBLEN);
            S16(DDT86_MCBLEN, (uint16_t)(len - 8));
            if (len < 8) return;
            S16(DDT86_DMAOFF, (uint16_t)(D16(DDT86_DMAOFF) + 0x80));
        } while (D16(DDT86_DMAOFF) != 0);
        S16(DDT86_DMASEG, (uint16_t)(D16(DDT86_DMASEG) + 0x1000));
    }
}

/* ---- the commands ---- */

/* A: assemble */
static void assm(void) {
    if (!D8(DDT86_ASSEMPRESENT)) err();
    S16(DDT86_ASMSPSAV, DDT86_ASMSP);
    (void)getsegandoff(DDT86_TYPE1LOC);
    (void)DDT_SETJMP(asm_jump); /* asment comes back here: asm0 */
    for (;;) {
        uint16_t ax = ass86_asm(g_ds, D16(DDT86_TYPE1LOC), D16(DDT86_TYPE1SEG));
        if (ax <= D16(DDT86_TYPE1LOC)) return;
        S16(DDT86_TYPE1LOC, ax);
    }
}

static void printerror(uint16_t di) {
    uint16_t seg = D16(di + 2), off = D16(di);
    uint8_t b = *at(seg, off);
    printdword(seg, off);
    blank();
    printbyte(b);
}

/* B and M's three arguments: start, end, destination */
static void threeargs(void) {
    (void)getsegandoff(DDT86_TYPE2LOC);
    number e = getoffset();
    S16(DDT86_USERMAX, e.bx);
    if (e.al == EOL || e.bx < D16(DDT86_TYPE2LOC)) err();
    S16(DDT86_USERSEG2, D16(DDT86_TYPE2SEG));
    if (getsegandoff(DDT86_USERLOC2) != EOL) err();
}

/* B: block compare */
static void blockcompare(void) {
    threeargs();
    for (;;) {
        uint8_t a = *at(D16(DDT86_TYPE2SEG), D16(DDT86_TYPE2LOC));
        if (a != *at(D16(DDT86_USERSEG2), D16(DDT86_USERLOC2))) {
            crlfchk();
            printerror(DDT86_TYPE2LOC);
            blank();
            blank();
            printerror(DDT86_USERLOC2);
        }
        INC16(DDT86_TYPE2LOC);
        if (D16(DDT86_USERMAX) < D16(DDT86_TYPE2LOC)) return;
        INC16(DDT86_USERLOC2);
        if (D16(DDT86_USERLOC2) == 0) return;
    }
}

/* D: display memory */
static void display(void) {
    D8(DDT86_NUMREQ) = 0;
    S16(DDT86_DISSEG, D16(DDT86_TYPE2SEG));
    checkword();
    if (D8(DDT86_COL40))
        D8(DDT86_LINEMAX) = (D8(DDT86_WMODE) & 1) ? D8(DDT86_NDW40) : D8(DDT86_ND40);
    else
        D8(DDT86_LINEMAX) = D8(DDT86_ND80);
    uint8_t al = getsegandoff(DDT86_DISLOC);
    S16(DDT86_TYPE2SEG, D16(DDT86_DISSEG));
    uint16_t bx;
    if (al == ',') {
        bx = getlastoffset();
    } else {
        uint16_t ax = (uint16_t)(D8(DDT86_LINEMAX) * D8(DDT86_NLINES));
        ax = (uint16_t)((ax & 0xFF00) | (uint8_t)(ax - 1)); /* dec al */
        bx = (uint16_t)(D16(DDT86_DISLOC) + ax);
        if (bx < D16(DDT86_DISLOC)) bx = 0xFFFF;
    }
    S16(DDT86_DISMAX, bx);
    for (;;) {
        crlfchk();
        S16(DDT86_TDISP, D16(DDT86_DISLOC));
        printdword(D16(DDT86_DISSEG), D16(DDT86_DISLOC));
        for (;;) {
            blank();
            print8or16(D16(DDT86_DISSEG), D16(DDT86_DISLOC));
            if (inc1or2(DDT86_DISLOC)) break;
            if ((uint8_t)(D16(DDT86_DISLOC) - D16(DDT86_TDISP)) == D8(DDT86_LINEMAX)) break;
            if (D16(DDT86_DISLOC) > D16(DDT86_DISMAX)) break;
        }
        blank();
        uint16_t t;
        do {
            ascout(*at(D16(DDT86_DISSEG), D16(DDT86_TDISP)));
            INC16(DDT86_TDISP);
            t = D16(DDT86_TDISP);
            if (t == 0) return;
        } while (t != D16(DDT86_DISLOC));
        if (t > D16(DDT86_DISMAX)) return;
    }
}

static void verify(void);
static void ifcb(void);

/* E: load a program */
static void execute(void) {
    if (conin() == EOL) err();
    DEC_CONPTR();
    int q;
    if (parse(FCB, &q) != EOL || q) err();
    if (D8(FCB + 9) == ' ')
        for (int i = 0; i < 3; i++) D8(FCB + 9 + i) = cmdtype[i];
    open_(FCB);
    D8(DDT86_MCBEXT) = 0xFF; /* free everything below DDT86 */
    freemem(DDT86_MCB);
    uint16_t bx = bdos(59, FCB).bx; /* program load */
    if ((uint16_t)(bx + 1) == 0) loaderr();
    setcpustate(bx);
    setdmab(bx);
    setdma(0x80);
    close_(FCB);
    D8(DDT86_MODE) = 'E';
    uint8_t *bp = at(ureg(R_DS), 0);
    for (int i = 0; i < 48; i++) D8(DDT86_BASEPAGESAVE + i) = bp[i];
    verify();
    DEC_CONPTR(); /* to rescan the CR */
    ifcb();       /* to clear the fcb */
}

/* F: fill */
static void fill_(void) {
    checkword();
    (void)getsegandoff(DDT86_TYPE2LOC);
    S16(DDT86_USERMAX, getoffset().bx);
    uint16_t bx = getlastoffset();
    if (!(D8(DDT86_WMODE) & 1) && (bx >> 8)) err();
    if (D16(DDT86_USERMAX) < D16(DDT86_TYPE2LOC)) err();
    do {
        set8or16(D16(DDT86_TYPE2SEG), D16(DDT86_TYPE2LOC), bx);
        if (inc1or2(DDT86_TYPE2LOC)) return;
    } while (D16(DDT86_TYPE2LOC) <= D16(DDT86_USERMAX));
}

/* H: hex sum and difference */
static void hexmath(void) {
    uint16_t a = getoffset().bx;
    uint16_t b = getlastoffset();
    crlf();
    printword((uint16_t)(a + b));
    blank();
    printword((uint16_t)(a - b));
}

/* I: the fcbs and command tail */
static void ifcb(void) {
    uint16_t save = D16(DDT86_CONPTR);
    if (parse(FCB, NULL) == EOL) DEC_CONPTR(); /* to blank the second name too */
    (void)parse2(FCB2, NULL);
    S16(DDT86_CONPTR, save);
    uint16_t di = 0x81, cx = 0;
    for (;;) {
        uint8_t c = conin();
        if (c == EOL) break;
        D8(di++) = c;
        cx++;
    }
    D8(di) = 0;
    D8(BUFF) = (uint8_t)cx;
    if (D8(DDT86_MODE) == 'E') {
        uint8_t *u = at(ureg(R_DS), 0);
        for (uint16_t i = 0; i < (uint16_t)(cx + 38); i++) u[(uint16_t)(FCB + i)] = D8(FCB + i);
    }
}

/* L: list (disassemble) */
static void lassm(void) {
    if (!D8(DDT86_DISEMPRESENT)) err();
    D8(DDT86_LASCNTSW) = 0;
    D8(DDT86_NUMREQ) = 0;
    S16(DDT86_LASSEG, D16(DDT86_TYPE1SEG));
    uint8_t al = getsegandoff(DDT86_LASLOC);
    S16(DDT86_TYPE1SEG, D16(DDT86_LASSEG));
    uint16_t bx;
    if (al != EOL) {
        bx = getlastoffset();
    } else {
        D8(DDT86_LASCNTSW) = 1;
        D8(DDT86_LASCNT) = D8(DDT86_NLINES);
        bx = 0xFFFF;
    }
    S16(DDT86_LASMAX, bx);
    for (;;) {
        uint16_t di = D16(DDT86_LASLOC);
        if (di > D16(DDT86_LASMAX)) return;
        crlfchk();
        uint16_t es = D16(DDT86_LASSEG);
        printdword(es, di);
        blank();
        uint16_t ax = dis86_disem(g_ds, di, es);
        if (ax < D16(DDT86_LASLOC)) return;
        S16(DDT86_LASLOC, ax);
        if (D8(DDT86_LASCNTSW) && --D8(DDT86_LASCNT) == 0) return;
    }
}

/* M: move */
static void move(void) {
    threeargs();
    do {
        uint8_t b = *at(D16(DDT86_TYPE2SEG), D16(DDT86_TYPE2LOC));
        setbyte(D16(DDT86_USERSEG2), D16(DDT86_USERLOC2), b);
        INC16(DDT86_TYPE2LOC);
        if (D16(DDT86_TYPE2LOC) == 0) return;
        INC16(DDT86_USERLOC2);
        if (D16(DDT86_USERLOC2) == 0) return;
    } while (D16(DDT86_TYPE2LOC) <= D16(DDT86_USERMAX));
}

/* R: read a file */
static void read_(void) {
    if (conin() == EOL) err();
    DEC_CONPTR();
    int q;
    if (parse(FCB, &q), q) err();
    if (D8(DDT86_LASTCHAR) != EOL) err();
    open_(FCB);
    S16(DDT86_MCBLEN, 0xFFFF);
    if ((uint8_t)(bdos(53, DDT86_MCB).bx + 1) == 0) errm(memm); /* get max mem */
    (void)bdos(56, DDT86_MCB);                                  /* alloc abs mem */
    readfile();
    D8(DDT86_MCBEXT) = 0;
    freemem(DDT86_MCB);
    S16(DDT86_TYPE2SEG, D16(DDT86_STARTREADSEG));
    S16(DDT86_TYPE1SEG, D16(DDT86_STARTREADSEG));
    S16(DDT86_DISLOC, 0);
    S16(DDT86_LASLOC, 0);
    D8(DDT86_MODE) = 'R';
    verify();
}

/* S: set memory */
static void setmem(void) {
    checkword();
    (void)getsegandoff(DDT86_TYPE2LOC);
    do {
        crlf();
        printdword(D16(DDT86_TYPE2SEG), D16(DDT86_TYPE2LOC));
        blank();
        print8or16(D16(DDT86_TYPE2SEG), D16(DDT86_TYPE2LOC));
        blank();
        getline();
        uint8_t c = conin();
        if (c != EOL) {
            if (c == '.') return;
            DEC_CONPTR();
            uint16_t bx = getlastoffset();
            if (!(D8(DDT86_WMODE) & 1) && (bx >> 8)) err();
            set8or16(D16(DDT86_TYPE2SEG), D16(DDT86_TYPE2LOC), bx);
        }
    } while (!inc1or2(DDT86_TYPE2LOC));
}

/* V: the last R's or E's extent */
static void verify(void) {
    uint8_t mode = D8(DDT86_MODE);
    if (mode == 'R') {
        crlf();
        printm(readm);
        crlf();
        printdword(D16(DDT86_STARTREADSEG), D16(DDT86_STARTREADOFF));
        blank();
        printdword(D16(DDT86_ENDREADSEG), D16(DDT86_ENDREADOFF));
    } else if (mode == 'E') {
        crlf();
        tabs(3);
        printm(readm);
        uint8_t n = 0;
        do {
            printseginfo(n);
            n++;
            if (D8(DDT86_BASEPAGESAVE + 5) == 1) return; /* 8080 model: one */
        } while (n < 8);
    } else {
        err();
    }
}

/* W: write memory to a file */
static void write_(void) {
    S16(DDT86_STARTWRITESEG, D16(DDT86_STARTREADSEG));
    S16(DDT86_STARTWRITEOFF, D16(DDT86_STARTREADOFF));
    S16(DDT86_ENDWRITESEG, D16(DDT86_ENDREADSEG));
    S16(DDT86_ENDWRITEOFF, D16(DDT86_ENDREADOFF));
    if (conin() == EOL) err();
    DEC_CONPTR();
    int q;
    uint8_t al = parse(FCB, &q);
    if (q) err();
    if (al == EOL) {
        if (D8(DDT86_MODE) != 'R') err();
    } else {
        S16(DDT86_STARTWRITESEG, D16(DDT86_TYPE1SEG));
        if (getsegandoff(DDT86_STARTWRITEOFF) == EOL) err();
        S16(DDT86_ENDWRITESEG, D16(DDT86_STARTWRITESEG));
        if (getsegandoff(DDT86_ENDWRITEOFF) != EOL) err();
    }
    writefile();
    close_(FCB);
}

/* X: display the CPU state */
static void xdisp(void) {
    setdefseg();
    if (!D8(DDT86_COL40)) {
        setuphdr();
        regheader_from(0, TOTREG);
        crlf();
        printflags();
        blank();
        printregs_from(0, TOTREG);
        printinstr();
        return;
    }
    setuphdr();
    regheader_from(0, 6); /* the 40-column form: two lines */
    crlf();
    printflags();
    blank();
    printregs_from(0, 6);
    crlf();
    blank();
    regheader_from(6, TOTREG);
    crlf();
    printregs_from(6, TOTREG);
    printinstr();
}

static void xflag(void) {
    checkflag(D8(DDT86_XTEMP));
    crlf();
    conout(flagname[D16(DDT86_REGNUM)]);
    blank();
    conout(getflag(D16(DDT86_REGNUM)) ? '1' : '0');
    blank();
    getline();
    number n = getnumber();
    if (n.al != EOL) err();
    if (!n.ah) return;
    if (n.bx > 1) err();
    setflag(D16(DDT86_REGNUM), n.bx);
}

/* X: examine and set the registers and flags */
static void xcom(void) {
    D8(DDT86_SEGFLAG) = 1;
    uint8_t c = conin();
    if (c == EOL) {
        xdisp();
        return;
    }
    D8(DDT86_XTEMP) = c;
    c = conin();
    if (c == EOL) {
        xflag();
        return;
    }
    chkreg((uint16_t)(D8(DDT86_XTEMP) | ((c | 0x80) << 8)));
    if (conin() != EOL) err();
    do {
        crlf();
        printregname(D16(DDT86_REGNUM));
        blank();
        printregval(D16(DDT86_REGNUM));
        blank();
        getline();
        if (conin() == '.') return;
        DEC_CONPTR();
        number n = getnumber();
        if (n.al != EOL) err();
        if (n.ah) {
            uint16_t cx = D16(DDT86_REGNUM);
            if ((uint8_t)cx == 8) S16(DDT86_TYPE1SEG, n.bx);
            if ((uint8_t)cx == 9) S16(DDT86_TYPE2SEG, n.bx);
            S16(UREG(cx), n.bx);
        }
        INC16(DDT86_REGNUM);
    } while (D16(DDT86_REGNUM) < TOTREG);
}

/* the command table: G, T and U (which run user code) are refused as
 * an invalid command is (see the header comment) */
static void (*const ctable[26])(void) = {
    assm,  blockcompare, err,   display, execute, fill_, err,   hexmath, ifcb,  err, err,    lassm, move,
    err,   err,          err,   err,     read_,   setmem, err,  err,     verify, write_, xcom, err,  err,
};

/* ---- DDT86's services for the overlays (its vectors) ---- */

uint8_t ddt_conin(uint16_t ds) {
    (void)ds;
    return conin();
}

void ddt_conout(uint16_t ds, uint8_t c) {
    (void)ds;
    conout(c);
}

void ddt_getline(uint16_t ds) {
    (void)ds;
    getline();
}

void ddt_asment(uint16_t ds) {
    (void)ds;
    DDT_LONGJMP(asm_jump); /* mov sp,asmspsav: back to the A command's loop */
}

void ddt_set(uint16_t ds, uint16_t off, uint8_t b) {
    (void)ds;
    setbyte(D16(DDT86_TYPE1SEG), off, b);
    if ((uint16_t)(off + 1) == 0) start(); /* wrapped: out of A */
}

/* ---- start-up and the command loop ---- */

void ddt86_main(uint16_t ds) {
    g_ds = ds;
    m = far_ptr(rt_phys(), ds, 0);
    for (uint16_t i = TC_DDT86_ENDCS; i < ddt86_image_len; i++) m[i] = ((const uint8_t *)&ddt86_image)[i];
    /* and the overlays' data, as loading DDT86.CMD leaves it */
    dis86_install(ds);
    ass86_install(ds);

    S16(DDT86_CCPSS, ds);
    S16(DDT86_CCPSP, DDT86_CCP_SP);
    S16(UREG(R_SS), ds);
    S16(UREG(R_SP), DDT86_CCP_SP);
    D8(DDT86_SYSIF) = (uint8_t)((DDT86_SYSFLAGS & 0x0200) >> 8);
    S16(UREG(R_FL), DDT86_SYSFLAGS & 0x0200);

    /* setbdosint: copy the BDOS vector to the next one, which DDT86's
     * own calls use */
    uint8_t *ivt = rt_phys();
    for (int i = 0; i < 4; i++) ivt[(BDOSI + 1) * 4 + i] = ivt[BDOSI * 4 + i];
    /* bpvect, unless the vectors are saved on each G/T/U: savevect (the
     * single-step and breakpoint vectors themselves aren't set) */
    if (!D8(DDT86_SAVEVECFLAG))
        for (int i = 0; i < 12; i++) D8(DDT86_VECTORSAVE + i) = ivt[4 + i];

    printm(signon);
    uint8_t bh = 0;
    if ((uint8_t)bdos(12, 0).bx >= 0x30) { /* MP/M (file system 3) */
        (void)bdos(45, 0xFE);              /* return errors to DDT86 */
        bh = 1;
    }
    D8(DDT86_ERRMODE) = bh;

    if (DDT_SETJMP(start_jump) == 0) {
        /* checkcmdtail: a command tail is an E command */
        uint8_t n = D8(BUFF);
        if (n) {
            if (n > CONBUFFMAX) n = CONBUFFMAX;
            for (uint8_t i = 0; i < n; i++) D8(DDT86_CONBUFF + i) = D8(BUFF + 1 + i);
            D8(DDT86_CONBUFF + n) = EOL;
            execute();
        }
    }
    for (;;) { /* start */
        crlf();
        conout('-');
        getline();
        uint8_t c = conin();
        if (c == EOL || c == ';') continue;
        c = (uint8_t)(c - 'A');
        if (c > 'Z' - 'A') err();
        D8(DDT86_NUMREQ) = 1;
        D8(DDT86_WMODE) = 0;
        ctable[c]();
    }
}
