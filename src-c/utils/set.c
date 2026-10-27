/*
 * set.c -- C version of src-assem/utils/SET.PLM (a transient, SET.CMD):
 * sets a file's attributes (RO/RW, SYS/DIR, ARCHIVE, F1-F4), its
 * password and protection mode and time stamping (through its XFCB),
 * a drive's directory label (name, password, protection, the stamp
 * and make-XFCB options), the default password, and a drive's R/O or
 * R/W status. SET name [options], SET [options] or SET d:[options];
 * a wildcard name sets every file it matches, one at a time.
 *
 * Its data segment, below, is SET.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (the DATA messages and tables among them), the string
 * constants, and the stack; set_main installs the image from 100h on.
 * PL/M's semantics are kept as in dir.c (SET's own `true` is 1): bytes
 * wrap, a relation is 0FFh or 0, IF tests bit 0, AND/OR evaluate both
 * sides, and the main program's variables are the PL/M globals; the
 * procedures' own variables are C locals, except where a nested
 * procedure shares them (readlabel's, here file statics). The PL/M's
 * address variables (fcbp, ibp, parse$fn, ...) are DS offsets, so the
 * C works on the image through them as the PL/M does.
 *
 * cmd, the parse fcb, is cmd(27) with 13 bytes initialized. (plmc used
 * to allocate only those 13, so every parse ran on into passwd and
 * overwrote the passwords: docs/bugs.md #131.) The C reaches it through
 * the image (bp[SET_CMD + i]), as the PL/M's parse does.
 *
 * A wildcard keeps search next's place across the set file indicators
 * and XFCB calls (which search the directory themselves) by saving and
 * restoring the UDA's dcnt, searchl and searcha, as SET.PLM does.
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
    char copyright[38], versiondate[8], version[8];
    char notfound[16], nospace[23], invalid[9], setprot[13], dirlabel[17], optionset[16], readonly[10],
        ro[6], readwrite[16], comma[3], setto[8], errormsg[8], readmode[5], writemode[6], deletemode[7],
        nopasswd[5], timestamp[15], on[10], off[10], failed[22], labelname[5];
    uint8_t usercode;
    uint16_t resetmask[16];
    uint16_t dpbptr[2];            /* offset, segment */
    uint16_t pdpointer[2];
    uint16_t pdparentpointer[2];
    uint16_t udapointer[2];
    uint16_t udaparentpointer[2];
    uint16_t fcbp;
    uint8_t xfcb[32];              /* xfcbmode is xfcb(12) */
    char cmd[27];                  /* the parse fcb */
    uint8_t passwd[17];
    uint8_t scase, fileref, lblcmd, xfcbcmd, wild, optdel, option_found, time_opt, password, option;
    uint8_t more;
    uint16_t opt_adr, ibp;
    uint16_t sav_dcnt, sav_searcha;
    uint8_t sav_searchl;
    uint8_t dirbuf[128];
    uint8_t cdisk;
    uint16_t ver;
    uint16_t error_code;
    uint16_t parse_fn[2];          /* buff$adr, fcb$adr */
    uint16_t last_buff_adr;
    uint8_t attr_byte[14], attr_value[14];
    char attributes[28], values[17], boolean[25], drvattributes[14], label1[181];
    uint8_t i, last_dseg_byte;
    char arr7[8], arr8[16], arr9[56], arr10[20], arr11[42], arr12[52], arr13[50], arr14[59], arr15[56], arr16[45],
        arr17[56], arr18[58], arr19[56], arr20[48], arr21[42], arr22[38], arr26[3], arr30[10], arr31[5], arr62[12],
        arr96[16], arr100[7], arr107[17], arr111[15], arr133[3], arr134[4], arr144[13], arr145[16], arr146[3],
        arr171[7], arr175[18];
    uint8_t readlbl_s_d;
    char arr182[8], arr204[12], arr247[23], arr263[12], arr264[17], arr296[33], arr297[14], arr469[35], arr480[18];
    uint8_t stack[1024];
    uint8_t stack_base;
} set_data;

_Static_assert(offsetof(set_data, copyright) == SET_COPYRIGHT, "copyright");
_Static_assert(offsetof(set_data, versiondate) == SET_VERSIONDATE, "versiondate");
_Static_assert(offsetof(set_data, version) == SET_VERSION, "version");
_Static_assert(offsetof(set_data, notfound) == SET_NOTFOUND, "notfound");
_Static_assert(offsetof(set_data, nospace) == SET_NOSPACE, "nospace");
_Static_assert(offsetof(set_data, invalid) == SET_INVALID, "invalid");
_Static_assert(offsetof(set_data, setprot) == SET_SETPROT, "setprot");
_Static_assert(offsetof(set_data, dirlabel) == SET_DIRLABEL, "dirlabel");
_Static_assert(offsetof(set_data, optionset) == SET_OPTIONSET, "optionset");
_Static_assert(offsetof(set_data, readonly) == SET_READONLY, "readonly");
_Static_assert(offsetof(set_data, ro) == SET_RO, "ro");
_Static_assert(offsetof(set_data, readwrite) == SET_READWRITE, "readwrite");
_Static_assert(offsetof(set_data, comma) == SET_COMMA, "comma");
_Static_assert(offsetof(set_data, setto) == SET_SETTO, "setto");
_Static_assert(offsetof(set_data, errormsg) == SET_ERRORMSG, "errormsg");
_Static_assert(offsetof(set_data, readmode) == SET_READMODE, "readmode");
_Static_assert(offsetof(set_data, writemode) == SET_WRITEMODE, "writemode");
_Static_assert(offsetof(set_data, deletemode) == SET_DELETEMODE, "deletemode");
_Static_assert(offsetof(set_data, nopasswd) == SET_NOPASSWD, "nopasswd");
_Static_assert(offsetof(set_data, timestamp) == SET_TIMESTAMP, "timestamp");
_Static_assert(offsetof(set_data, on) == SET_ON, "on");
_Static_assert(offsetof(set_data, off) == SET_OFF, "off");
_Static_assert(offsetof(set_data, failed) == SET_FAILED, "failed");
_Static_assert(offsetof(set_data, labelname) == SET_LABELNAME, "labelname");
_Static_assert(offsetof(set_data, usercode) == SET_USERCODE, "usercode");
_Static_assert(offsetof(set_data, resetmask) == SET_RESETMASK, "resetmask");
_Static_assert(offsetof(set_data, dpbptr) == SET_DPBPTR, "dpbptr");
_Static_assert(offsetof(set_data, pdpointer) == SET_PDPOINTER, "pdpointer");
_Static_assert(offsetof(set_data, pdparentpointer) == SET_PDPARENTPOINTER, "pdparentpointer");
_Static_assert(offsetof(set_data, udapointer) == SET_UDAPOINTER, "udapointer");
_Static_assert(offsetof(set_data, udaparentpointer) == SET_UDAPARENTPOINTER, "udaparentpointer");
_Static_assert(offsetof(set_data, fcbp) == SET_FCBP, "fcbp");
_Static_assert(offsetof(set_data, xfcb) == SET_XFCB, "xfcb");
_Static_assert(offsetof(set_data, cmd) == SET_CMD, "cmd");
_Static_assert(offsetof(set_data, passwd) == SET_PASSWD, "passwd");
_Static_assert(offsetof(set_data, scase) == SET_SCASE, "scase");
_Static_assert(offsetof(set_data, fileref) == SET_FILEREF, "fileref");
_Static_assert(offsetof(set_data, lblcmd) == SET_LBLCMD, "lblcmd");
_Static_assert(offsetof(set_data, xfcbcmd) == SET_XFCBCMD, "xfcbcmd");
_Static_assert(offsetof(set_data, wild) == SET_WILD, "wild");
_Static_assert(offsetof(set_data, optdel) == SET_OPTDEL, "optdel");
_Static_assert(offsetof(set_data, option_found) == SET_OPTIONFOUND, "optionfound");
_Static_assert(offsetof(set_data, time_opt) == SET_TIMEOPT, "timeopt");
_Static_assert(offsetof(set_data, password) == SET_PASSWORD, "password");
_Static_assert(offsetof(set_data, option) == SET_OPTION, "option");
_Static_assert(offsetof(set_data, more) == SET_MORE, "more");
_Static_assert(offsetof(set_data, opt_adr) == SET_OPTADR, "optadr");
_Static_assert(offsetof(set_data, ibp) == SET_IBP, "ibp");
_Static_assert(offsetof(set_data, sav_dcnt) == SET_SAVDCNT, "savdcnt");
_Static_assert(offsetof(set_data, sav_searcha) == SET_SAVSEARCHA, "savsearcha");
_Static_assert(offsetof(set_data, sav_searchl) == SET_SAVSEARCHL, "savsearchl");
_Static_assert(offsetof(set_data, dirbuf) == SET_DIRBUF, "dirbuf");
_Static_assert(offsetof(set_data, cdisk) == SET_CDISK, "cdisk");
_Static_assert(offsetof(set_data, ver) == SET_VER, "ver");
_Static_assert(offsetof(set_data, error_code) == SET_ERRORCODE, "errorcode");
_Static_assert(offsetof(set_data, parse_fn) == SET_PARSEFN, "parsefn");
_Static_assert(offsetof(set_data, last_buff_adr) == SET_LASTBUFFADR, "lastbuffadr");
_Static_assert(offsetof(set_data, attr_byte) == SET_ATTRBYTE, "attrbyte");
_Static_assert(offsetof(set_data, attr_value) == SET_ATTRVALUE, "attrvalue");
_Static_assert(offsetof(set_data, attributes) == SET_ATTRIBUTES, "attributes");
_Static_assert(offsetof(set_data, values) == SET_VALUES, "values");
_Static_assert(offsetof(set_data, boolean) == SET_BOOLEAN, "boolean");
_Static_assert(offsetof(set_data, drvattributes) == SET_DRVATTRIBUTES, "drvattributes");
_Static_assert(offsetof(set_data, label1) == SET_LABEL1, "label1");
_Static_assert(offsetof(set_data, i) == SET_I, "i");
_Static_assert(offsetof(set_data, last_dseg_byte) == SET_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(set_data, arr7) == SET_ARR7, "arr7");
_Static_assert(offsetof(set_data, arr8) == SET_ARR8, "arr8");
_Static_assert(offsetof(set_data, arr9) == SET_ARR9, "arr9");
_Static_assert(offsetof(set_data, arr10) == SET_ARR10, "arr10");
_Static_assert(offsetof(set_data, arr11) == SET_ARR11, "arr11");
_Static_assert(offsetof(set_data, arr12) == SET_ARR12, "arr12");
_Static_assert(offsetof(set_data, arr13) == SET_ARR13, "arr13");
_Static_assert(offsetof(set_data, arr14) == SET_ARR14, "arr14");
_Static_assert(offsetof(set_data, arr15) == SET_ARR15, "arr15");
_Static_assert(offsetof(set_data, arr16) == SET_ARR16, "arr16");
_Static_assert(offsetof(set_data, arr17) == SET_ARR17, "arr17");
_Static_assert(offsetof(set_data, arr18) == SET_ARR18, "arr18");
_Static_assert(offsetof(set_data, arr19) == SET_ARR19, "arr19");
_Static_assert(offsetof(set_data, arr20) == SET_ARR20, "arr20");
_Static_assert(offsetof(set_data, arr21) == SET_ARR21, "arr21");
_Static_assert(offsetof(set_data, arr22) == SET_ARR22, "arr22");
_Static_assert(offsetof(set_data, arr26) == SET_ARR26, "arr26");
_Static_assert(offsetof(set_data, arr30) == SET_ARR30, "arr30");
_Static_assert(offsetof(set_data, arr31) == SET_ARR31, "arr31");
_Static_assert(offsetof(set_data, arr62) == SET_ARR62, "arr62");
_Static_assert(offsetof(set_data, arr96) == SET_ARR96, "arr96");
_Static_assert(offsetof(set_data, arr100) == SET_ARR100, "arr100");
_Static_assert(offsetof(set_data, arr107) == SET_ARR107, "arr107");
_Static_assert(offsetof(set_data, arr111) == SET_ARR111, "arr111");
_Static_assert(offsetof(set_data, arr133) == SET_ARR133, "arr133");
_Static_assert(offsetof(set_data, arr134) == SET_ARR134, "arr134");
_Static_assert(offsetof(set_data, arr144) == SET_ARR144, "arr144");
_Static_assert(offsetof(set_data, arr145) == SET_ARR145, "arr145");
_Static_assert(offsetof(set_data, arr146) == SET_ARR146, "arr146");
_Static_assert(offsetof(set_data, arr171) == SET_ARR171, "arr171");
_Static_assert(offsetof(set_data, arr175) == SET_ARR175, "arr175");
_Static_assert(offsetof(set_data, readlbl_s_d) == SET_READLBL_S_D, "readlbl_s_d");
_Static_assert(offsetof(set_data, arr182) == SET_ARR182, "arr182");
_Static_assert(offsetof(set_data, arr204) == SET_ARR204, "arr204");
_Static_assert(offsetof(set_data, arr247) == SET_ARR247, "arr247");
_Static_assert(offsetof(set_data, arr263) == SET_ARR263, "arr263");
_Static_assert(offsetof(set_data, arr264) == SET_ARR264, "arr264");
_Static_assert(offsetof(set_data, arr296) == SET_ARR296, "arr296");
_Static_assert(offsetof(set_data, arr297) == SET_ARR297, "arr297");
_Static_assert(offsetof(set_data, arr469) == SET_ARR469, "arr469");
_Static_assert(offsetof(set_data, arr480) == SET_ARR480, "arr480");
_Static_assert(offsetof(set_data, stack_base) == SET_STACK_BASE, "stack_base");
_Static_assert(sizeof(set_data) == SET_DATA_LEN, "length");

#define T 1 /* SET's true */
#define F 0
#define MPMPRODUCT 0x01
#define CPMVERSION 0x30
#define TAB   9
#define CR    13
#define LF    10
#define CTRLC 3
#define CTRLX 0x18
#define CTRLH 8

/* fcb offsets (SET.PLM's literals) */
#define fnam   11
#define ftyp   9
#define rofile 9  /* read/only file */
#define infile 10 /* invisible file */
#define archiv 11 /* archived file */
#define attrb1 1  /* attribute F1' */
#define attrb2 2
#define attrb3 3
#define attrb4 4

/* the UDA's and PD's fields SET uses (uda.lit, proces.lit) */
#define uda_searchl     0x07
#define uda_searcha     0x08
#define uda_dcnt        0x0C
#define uda_df_password 0x12
#define pd_uda          0x10
#define pd_parent       0x1E
#define dpb_exm         4 /* dpb.exm, extmsk */

const uint16_t set_image_len = sizeof(set_data);
const set_data set_image = {
    .copyright = " Copyright (c) 1981, Digital Research ",
    .versiondate = "08/09/81",
    .version = "SET 2.0",
    .notfound = " File not found",
    .nospace = " or no directory space",
    .invalid = "Invalid ",
    .setprot = "[protect=on]",
    .dirlabel = "Directory Label ",
    .optionset = " attribute set ",
    .readonly = "read only",
    .ro = " (RO)",
    .readwrite = "read write (RW)",
    .comma = ", ",
    .setto = "set to ",
    .errormsg = "ERROR: ",
    .readmode = "READ",
    .writemode = "WRITE",
    .deletemode = "DELETE",
    .nopasswd = "NONE",
    .timestamp = "Time Stamps ON",
    .on = "    on   ",
    .off = "    off  ",
    .failed = "Unsuccessful Function",
    .labelname = "Label",
    .resetmask = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800,
                  0x1000, 0x2000, 0x4000, 0x8000},
    .cmd = "\0HELP       ",
    .scase = 0xFF, /* -1 */
    .fileref = F,
    .lblcmd = F,
    .xfcbcmd = F,
    .wild = F,
    .optdel = F,
    .option_found = F,
    .time_opt = F,
    .password = F,
    .option = F,
    .more = T,
    /*            RW RO DIR SYS  A F F F F  A F F F F */
    .attr_byte = {9, 9, 10, 10, 11, 1, 2, 3, 4, 11, 1, 2, 3, 4},
    .attr_value = {0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    .attributes = "RWRODISYARF1F2F3F4Attribute",
    .values = "OFONREWRDENOMode",
    .boolean = "OFONValue, Use ON or OFF",
    .drvattributes = "RWROAttribute", /* drive status's own (docs/bugs.md #132) */
    .label1 = "Directory       Passwds  Make     Stamp    Stamp    Stamp\r\n"
              "Label           Reqd     XFCBs    Create   Access   Update\r\n"
              "--------------  -------  -------  -------  -------  -------\r\n",
    .i = 1,
    .last_dseg_byte = 0,
    .arr7 = "Aborted",
    .arr8 = "\t\t\tSET EXAMPLES",
    .arr9 = "\r\nFOR FILES\r\n\r\nset *.asm [rw, dir] \t\t\t(File Attributes)",
    .arr10 = "set *.prl [ro, sys]",
    .arr11 = "set *.dat [archive=on,f1=off,f2=on,f3=on]",
    .arr12 = "set *.asm [time]    \t\t\t(Time Stamping on ASM files)",
    .arr13 = "set *.asm [password = xyz]\t\t(Password Protection)",
    .arr14 = "set *.asm [protect  = read]\t\t(read, write, delete or none)",
    .arr15 = "\r\nFOR DRIVES\r\n\r\nset [password = xyz]\t\t\t(Label Password)",
    .arr16 = "set [protect  = on] \t\t\t(Password Protection)",
    .arr17 = "set [update   = on] \t\t\t(Update Time Stamps - on or off)",
    .arr18 = "set [create   = on] \t\t\t(Creation Time Stamps - on or off)",
    .arr19 = "set [access   = on] \t\t\t(Access Time Stamps - on or off)",
    .arr20 = "set [make     = on] \t\t\t(Make XFCBs - on or off)",
    .arr21 = "set [default  = xyz]\t\t\t(Default Password)",
    .arr22 = "set a:[rw],   b:[ro]\t\t\t(Drive Status)",
    .arr26 = " ?",
    .arr30 = "Parameter",
    .arr31 = "File",
    .arr62 = "Password ? ",
    .arr96 = "BDOS Bad Sector",
    .arr100 = "Drive ",
    .arr107 = "Currently Opened",
    .arr111 = "Wrong Password",
    .arr133 = "ON",
    .arr134 = "OFF",
    .arr144 = "system (SYS)",
    .arr145 = "directory (DIR)",
    .arr146 = ", ",
    .arr171 = "Drive ",
    .arr175 = "Default Password ",
    .readlbl_s_d = '?',
    .arr182 = "lbl err",
    .arr204 = " First SET ",
    .arr247 = "[access=on, update=on]",
    .arr263 = "Password = ",
    .arr264 = "Label for drive ",
    .arr296 = " Assign a password to this file.",
    .arr297 = "Protection = ",
    .arr469 = "Parameter Required, try SET [HELP]",
    .arr480 = "Requires MP/M 2.0",
};

static uint16_t g_ds;
static set_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* the base page (DS:0), and its fcb */
static uint8_t *fcb = 0;

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* cmd(i), fcbv(i) and fext (fcbv(12)), xfcbmode (xfcb(12)) */
#define CMD(i)   bp[(uint16_t)(SET_CMD + (i))]
#define FCBV(i)  bp[(uint16_t)(g->fcbp + (i))]
#define FEXT     FCBV(12)
#define XFCBMODE g->xfcb[12]

/* the UDA, through uda$pointer */
static uint8_t *uda(void) { return far_ptr(rt_phys(), g->udapointer[1], g->udapointer[0]); }

/* ---- system function calls ---- */

static void printchar(uint8_t c) { mon1(2, c); }

/* print blank character */
static void printb(void) { printchar(' '); }

/* print the string at a, up to its 0 */
static void printx(uint16_t a) {
    while (rel(bp[a] != 0) & 1) {
        printchar(bp[a]);
        a = (uint16_t)(a + 1);
    }
}

static uint8_t check_con_stat(void) { return mon2(11, 0); /* console ready */ }

static void crlf(void) {
    printchar(CR);
    printchar(LF);
    if (check_con_stat() & 1) {
        mon1(1, 0);           /* read character */
        printx(SET_ARR7);     /* Aborted */
        mon1(0, 0);           /* system reset */
    }
}

/* print the string at a, until the next 0, on a new line */
static void print(uint16_t a) {
    crlf();
    printx(a);
}

static uint16_t get_version(void) { return mon3(12, 0); }
static uint8_t conin(void) { return mon2(6, 0xFD); }
static void select(uint8_t d) { mon1(14, d); }
static uint8_t search_first(uint16_t f) { return mon2(17, f); }
static uint8_t search_next(void) { return mon2(18, 0); }
static uint8_t cselect(void) { return mon2(25, 0); /* current disk */ }
static void setdma(uint16_t dma) { mon1(26, dma); }
static uint8_t writeprot(void) { return mon2(28, 0); /* write protect the current disk */ }
static uint8_t getuser(void) { return mon2(32, 0xFF); }
static void return_errors(uint8_t mode) { mon1(45, mode); /* 0ffh => return BDOS errors */ }

/* set file indicators for the fcb */
static uint16_t setind(uint16_t f) {
    setdma(SET_PASSWD);
    return mon3(30, f);
}

/* set disk parameter block values: dpbptr = mon4(31) */
static void set_dpb(void) {
    mpm_regs r = rt_mpm(g_ds, 31, 0);
    g->dpbptr[0] = r.bx;
    g->dpbptr[1] = r.es;
}

static uint16_t wrlbl(uint16_t f) {
    setdma(SET_PASSWD); /* set dma=password */
    return mon3(100, f);
}

static uint8_t getlbl(uint8_t d) { return mon2(101, d); }

static void readxfcb(uint16_t f) {
    setdma(SET_PASSWD); /* set dma=password */
    mon1(102, f);
}

static uint16_t wrxfcb(uint16_t f) {
    setdma(SET_PASSWD);
    return mon3(103, f);
}

static void get_pd_uda(void) {
    mpm_regs r = rt_mpm(g_ds, 156, 0); /* mon4: the PD's pointer, ES:BX */
    g->pdpointer[0] = r.bx;
    g->pdpointer[1] = r.es;
    g->udapointer[0] = 0;
    g->udapointer[1] = rd16(far_ptr(rt_phys(), g->pdpointer[1], (uint16_t)(g->pdpointer[0] + pd_uda)));
}

static uint8_t reset_drv(uint8_t drv) {
    return mon2(37, rd16(bp + (uint16_t)(SET_RESETMASK + 2 * drv)));
}

static void terminate(void) {
    crlf();
    mon1(0, 0);
}

/* ---- basic routines ---- */

/* help message */
static void help(void) {
    print(SET_ARR8);  /* SET EXAMPLES */
    print(SET_ARR9);  /* FOR FILES ... (File Attributes) */
    print(SET_ARR10);
    print(SET_ARR11);
    print(SET_ARR12);
    print(SET_ARR13);
    print(SET_ARR14);
    print(SET_ARR15); /* FOR DRIVES ... (Label Password) */
    print(SET_ARR16);
    print(SET_ARR17);
    print(SET_ARR18);
    print(SET_ARR19);
    print(SET_ARR20);
    print(SET_ARR21);
    print(SET_ARR22);
}

/* invalid command error */
static void perror(uint16_t msg) {
    print(SET_ERRORMSG);
    if (rel(g->ibp == 0) & 1)
        printx(g->parse_fn[0]);
    else
        printx(g->last_buff_adr);
    printx(SET_ARR26); /* ' ?' */
    print(SET_INVALID);
    printx(msg);
    terminate();
}

/* parsing error */
static void parse_error(void) {
    if (g->option & 1)
        perror(SET_ARR30); /* Parameter */
    else
        perror(SET_ARR31); /* File */
}

/* parse the next lexical item in the command line; parse$fn must be
 * filled in with the input parameters */
static uint16_t parse(void) {
    uint16_t p = mon3(152, SET_PARSEFN);
    if (rel(p == 0xFFFF) & 1) {
        parse_error();
    } else if (rel(p != 0) & 1) {
        if (rel(bp[p] == '[') & 1)
            g->optdel = T;
        else if (rel(bp[p] == ']') & 1)
            g->optdel = F;
        p = (uint16_t)(p + 1);
        if (rel(bp[p] == ',') & 1) p = (uint16_t)(p + 1);
        g->last_buff_adr = (uint16_t)(g->parse_fn[0] - 1);
        g->parse_fn[0] = p;
    } else {
        g->optdel = F;
    }
    return p;
}

/* parse an option value */
static void parse_value(void) {
    if (rel(g->ibp == 0) & 1) parse_error(); /* test for end */
    g->ibp = parse();                         /* more to go */
}

/* fill string @ s for c bytes with f */
static void fill(uint16_t s, uint8_t f, uint8_t c) {
    while (rel((c = (uint8_t)(c - 1)) != 255) & 1) {
        bp[s] = f;
        s = (uint16_t)(s + 1);
    }
}

/* copy c bytes from s to d, a byte at a time */
static void copy(uint16_t s, uint16_t d, uint8_t c) {
    while (rel((c = (uint8_t)(c - 1)) != 255) & 1) {
        bp[d] = bp[s];
        s = (uint16_t)(s + 1);
        d = (uint16_t)(d + 1);
    }
}

/* upper case character from console */
static uint8_t ucase(void) {
    uint8_t c;
    if (rel((c = conin()) >= 'a') & 1)
        if (rel(c < '{') & 1) return (uint8_t)(c - 0x20);
    return c;
}

/* get password and place in passwd */
static void getpasswd(void) {
    uint8_t i, c;
    print(SET_ARR62); /* Password ? */
retry:
    fill(SET_PASSWD, ' ', 8);
    for (i = 0; i <= 7; i++) {
    nxtchr:
        if (rel((c = ucase()) >= ' ') & 1) g->passwd[i] = c;
        if (rel(c == CR) & 1) goto exit;
        if (rel(c == CTRLX) & 1) goto retry;
        if (rel(c == CTRLH) & 1) {
            if (rel(i < 1) & 1) goto retry;
            g->passwd[i = (uint8_t)(i - 1)] = ' ';
            goto nxtchr;
        }
        if (rel(c == CTRLC) & 1) terminate(); /* end of program */
    }
exit:
    c = check_con_stat(); /* clear raw I/O mode */
    (void)c;
}

/* print drive name */
static void printdrv(void) {
    printchar((uint8_t)(g->cdisk + 'A'));
    printchar(':');
}

/* print file name */
static void printfn(void) {
    printdrv();
    for (uint8_t k = 1; k <= fnam; k++) {
        if (rel(k == ftyp) & 1) printchar('.');
        printchar(FCBV(k) & 0x7F);
    }
}

/* error message routine */
static void bdos_error(void) {
    uint8_t code;
    if (rel((code = (uint8_t)(g->error_code >> 8)) < 3) & 1) {
        print(SET_ERRORMSG);
        printdrv();
        printb();
        if (rel(code == 1) & 1) printx(SET_ARR96); /* BDOS Bad Sector */
        if (rel(code == 2) & 1) {
            printx(SET_ARR100); /* Drive */
            printx(SET_READONLY);
        }
        terminate();
    }
    printx(SET_ERRORMSG);
    if (rel(code == 3) & 1) printx(SET_READONLY);
    if (rel(code == 5) & 1) printx(SET_ARR107); /* Currently Opened */
    if (rel(code == 7) & 1) printx(SET_ARR111); /* Wrong Password */
}

/* get address of FCB in dirbuf */
static void set_up_file(uint8_t dir_index) {
    if (rel(dir_index != 0xFF) & 1) {
        g->sav_dcnt = rd16(uda() + uda_dcnt);
        g->sav_searchl = uda()[uda_searchl];
        g->sav_searcha = rd16(uda() + uda_searcha);
        g->fcbp = (uint16_t)((uint8_t)(dir_index << 5) + SET_DIRBUF);
        FCBV(0) = fcb[0]; /* set drive byte */
    }
}

/* match the command in cmd(1..2) against the 2-letter commands at
 * commands_adr; past the last comes the error message */
static uint8_t match(uint16_t commands_adr, uint8_t last_cmd) {
    uint8_t i, j, matched, scase;
    j = 0;
    for (scase = 0; scase <= last_cmd; scase++) {
        matched = T;
        for (i = 1; i <= 2; i++) {
            if (rel(bp[(uint16_t)(commands_adr + j)] != CMD(i)) & 1) matched = F;
            j = (uint8_t)(j + 1);
        }
        if (matched & 1) return scase;
    }
    perror((uint16_t)(commands_adr + j));
    return 0; /* (perror doesn't return) */
}

/* return boolean option value */
static uint8_t bool_(void) {
    if (match(SET_BOOLEAN, 1) & 1) return T;
    return F;
}

/* (pbool, open, getfilesize, setuser and boot are declared but never
 * called) */

/* print command */
static void printcmd(void) {
    printx(SET_SETTO);
    CMD(12) = 0;
    printx(SET_CMD + 1);
}

/* ---- file attributes ---- */

/* test if attribute fcbv(i) is on: rol(fcbv(i),1) */
static uint8_t attribute(uint8_t i) {
    if ((uint8_t)((FCBV(i) << 1) | (FCBV(i) >> 7)) & 1) return T;
    return F;
}

/* print character c if attribute(b) is true */
static void prnt_attrib(uint8_t b, uint8_t c) {
    if (attribute(b) & 1) printchar(c);
}

/* print attribute set: sys, ro, a, f1-f4 */
static void printatt(void) {
    printx(SET_SETTO);
    if (attribute(infile) & 1)
        printx(SET_ARR144); /* system (SYS) */
    else
        printx(SET_ARR145); /* directory (DIR) */
    printx(SET_ARR146);     /* , */
    if (attribute(rofile) & 1) {
        printx(SET_READONLY);
        printx(SET_RO);
    } else {
        printx(SET_READWRITE);
    }
    printchar(TAB);
    prnt_attrib(archiv, 'A');
    prnt_attrib(attrb1, '1');
    prnt_attrib(attrb2, '2');
    prnt_attrib(attrb3, '3');
    prnt_attrib(attrb4, '4');
}

/* read current file attributes */
static void rd_attributes(void) {
    if (rel(g->scase == 0xFF) & 1)
        if ((uint8_t)~g->wild & 1) {
            setdma(SET_DIRBUF);
            set_up_file(search_first(bp_fcb));
        }
}

/* set up file attributes; scase ranges from 0 - 13:
 *   0 - RW   3 - SYS       6 - F2 (on)  9 - not Archived
 *   1 - RO   4 - ARCHIVED  7 - F3 (on) 10 - F1 (off) 12 - F3 (off)
 *   2 - DIR  5 - F1 (on)   8 - F4 (on) 11 - F2 (off) 13 - F4 (off) */
static void set_attributes(void) {
    rd_attributes();
    if (rel((g->scase = match(SET_ATTRIBUTES, 8)) > 3) & 1) {
        parse_value();
        if ((uint8_t)~bool_() & 1) g->scase = (uint8_t)(g->scase + 5);
    }
    if (g->attr_value[g->scase] & 1)
        FCBV(g->attr_byte[g->scase]) = FCBV(g->attr_byte[g->scase]) | 0x80;
    else
        FCBV(g->attr_byte[g->scase]) = FCBV(g->attr_byte[g->scase]) & 0x7F;
}

/* ---- drive attributes ---- */

/* set drive attributes */
static void setdrvstatus(void) {
    uint8_t code;
    if ((g->scase = match(SET_DRVATTRIBUTES, 1)) & 1) /* set the drive */
        code = writeprot();                        /* RO */
    else
        code = reset_drv(g->cdisk);                /* RW */
    if (rel(code != 0xFF) & 1) {                   /* display */
        print(SET_ARR171); /* Drive */
        printdrv();
        printb();
        printx(SET_SETTO);
        if (g->scase & 1) {
            printx(SET_READONLY);
            printx(SET_RO);
        } else {
            printx(SET_READWRITE);
        }
    } else {
        print(SET_FAILED);
    }
    g->scase = 0xFF;
}

/* set default password, in this process's UDA and its parent's */
static void defaultpass(void) {
    fill(SET_CMD + 1, ' ', 8);
    g->ibp = parse();         /* get password */
    mon1(106, SET_CMD + 1);   /* set default password */
    print(SET_ARR175);        /* Default Password */
    printcmd();

    get_pd_uda();
    g->pdparentpointer[1] = g->pdpointer[1];
    g->pdparentpointer[0] = rd16(far_ptr(rt_phys(), g->pdpointer[1], (uint16_t)(g->pdpointer[0] + pd_parent)));
    g->udaparentpointer[1] =
        rd16(far_ptr(rt_phys(), g->pdparentpointer[1], (uint16_t)(g->pdparentpointer[0] + pd_uda)));
    g->udaparentpointer[0] = 0;
    /* movw(@uda.df$password, @uda$parent.df$password, 4) */
    uint8_t *src = far_ptr(rt_phys(), g->udapointer[1], (uint16_t)(g->udapointer[0] + uda_df_password));
    uint8_t *dst = far_ptr(rt_phys(), g->udaparentpointer[1], (uint16_t)(g->udaparentpointer[0] + uda_df_password));
    for (int w = 0; w < 4; w++) wr16(dst + 2 * w, rd16(src + 2 * w));
}

/* ---- label attributes ---- */

/* readlabel's variables (PL/M locals readlbl shares) */
static uint8_t rl_mode, rl_dcnt;

/* find the directory label in the directory */
static void readlbl(void) {
    setdma(SET_DIRBUF);
    rl_dcnt = search_first(SET_READLBL_S_D); /* d = '?' */
    while (rel(rl_dcnt != 0xFF) & 1) {
        if (rel(g->dirbuf[(uint8_t)((rl_dcnt >> 3) | (rl_dcnt << 5)) & 0x60] == 0x20) & 1) return;
        rl_dcnt = search_next();
    }
    print(SET_ARR182); /* lbl err */
    terminate();
}

/* read the directory label before writing the label, to preserve the
 * name, type, and stamps */
static void readlabel(void) {
    if (g->lblcmd & 1) return;
    rl_mode = getlbl(g->cdisk);
    g->password = F;
    if (rel(rl_mode > 0) & 1) {
        readlbl();
        g->fcbp = (uint16_t)((uint8_t)(rl_dcnt << 5) + SET_DIRBUF);
        FEXT = FEXT & 0xF0; /* turn off set passwd */
        if (rel(FCBV(16) != ' ') & 1)
            if (rel(FCBV(16) != 0) & 1) g->password = T;
    } else {
        g->fcbp = bp_fcb;
        copy(SET_LABELNAME, bp_fcb + 1, 5); /* length(label$name) */
    }
    if (g->password & 1) getpasswd();
    g->lblcmd = T;
}

/* ---- XFCB attributes ---- */

/* read xfcb into xfcb buffer */
static void set_up_xfcb(void) {
    if ((uint8_t)~g->xfcbcmd & 1) {
        copy(g->fcbp, SET_XFCB, 12);
        g->password = XFCBMODE = 0;
        readxfcb(SET_XFCB); /* read xfcb */
        if (rel(XFCBMODE != 0) & 1) g->password = T;
        g->xfcbcmd = T;
    }
    /* else already done */
}

/* no directory label exists */
static void no_label(uint16_t msg) {
    crlf();
    print(SET_ERRORMSG);
    printx(SET_ARR204); /* First SET */
    printdrv();
    printx(msg);
    terminate();
}

/* ---- password and password mode routines ---- */

/* set file or label password */
static void set_password(void) {
    uint16_t p, q;
    if (g->fileref & 1) {
        if (rel(getlbl(g->cdisk) == 0) & 1) no_label(SET_SETPROT);
        set_up_xfcb();                       /* read xfcb */
        XFCBMODE = XFCBMODE | 1;             /* set passwd */
    } else {
        readlabel();
        FEXT = FEXT | 1;
    }
    p = (uint16_t)((q = g->parse_fn[0]) - 1);
    if ((rel(bp[p] == ',') | rel(bp[q] == ']')) & 1) { /* null password */
        fill(SET_PASSWD + 8, ' ', 8);
        if (rel(bp[q] == ']') & 1) g->ibp = parse(); /* past the ] as a password would (bugs.md #132) */
    } else {
        g->ibp = parse();                    /* parse password */
        copy(SET_CMD + 1, SET_PASSWD + 8, 8); /* copy it to fcb */
        g->password = T;
    }
}

static void zeropass(void) {
    XFCBMODE = 1;
    fill(SET_PASSWD + 8, ' ', 8);
    g->password = F;
}

static void rmode(void) { XFCBMODE = 0x80; }

/* set file or drive protection mode */
static void protect(void) {
    uint8_t new_password;
    parse_value(); /* protection value */
    if (g->fileref & 1) {
        if (rel(getlbl(g->cdisk) == 0) & 1) no_label(SET_SETPROT);
        set_up_xfcb();
        if (XFCBMODE & 1) /* lsb */
            new_password = T; /* save */
        else
            new_password = F;
        switch (match(SET_VALUES, 5)) {
        case 0: zeropass(); break;       /* OFF */
        case 1: rmode(); break;          /* ON */
        case 2: rmode(); break;          /* READ */
        case 3: XFCBMODE = 0x40; break;  /* WRITE */
        case 4: XFCBMODE = 0x20; break;  /* DELETE */
        case 5: zeropass(); break;       /* NONE */
        }
        if (new_password & 1) XFCBMODE = XFCBMODE | 1; /* restore */
    } else {
        readlabel();
        if (bool_() & 1)
            FEXT = FEXT | 0x80; /* turn on passwords */
        else
            FEXT = FEXT & 0x7F; /* turn off passwords */
    }
}

/* set file time stamping */
static void time_(void) {
    set_up_xfcb();
    if (rel((getlbl(g->cdisk) & 0x60) == 0) & 1) no_label(SET_ARR247); /* [access=on, update=on] */
    g->time_opt = T;
}

/* ---- label attribute routines ---- */

/* gets the label option boolean value */
static void getbool(void) {
    if (g->fileref & 1) parse_error();
    readlabel();   /* get label name */
    parse_value(); /* option value */
}

/* sets the label name */
static void lname(void) {
    getbool();
    copy(SET_CMD + 1, (uint16_t)(g->fcbp + 1), 11); /* copy label name */
}

/* set access time stamping */
static void access(void) {
    getbool();
    if ((uint8_t)~bool_() & 1) {
        FEXT = FEXT & 0xBF; /* turn off access ts */
    } else {
        FEXT = FEXT | 0x40; /* turn on access ts */
        FEXT = FEXT | 0x10; /* turn on make xfcb */
    }
}

/* set update time stamping */
static void update(void) {
    getbool();
    if ((uint8_t)~bool_() & 1) {
        FEXT = FEXT & 0xDF; /* turn off update ts */
    } else {
        FEXT = FEXT | 0x20; /* turn on update ts */
        FEXT = FEXT | 0x10; /* turn on make xfcb */
    }
}

/* set create time stamping */
static void create(void) {
    getbool();
    if ((uint8_t)~bool_() & 1) {
        FEXT = FEXT | 0x40; /* turn on access ts */
    } else {
        FEXT = FEXT & 0xBF; /* turn off access ts */
        FEXT = FEXT | 0x10; /* turn on make xfcb */
    }
}

/* set make xfcbs option */
static void makestamp(void) {
    getbool();
    if ((uint8_t)~bool_() & 1)
        FEXT = FEXT & 0xEF; /* turn off make xfcb */
    else
        FEXT = FEXT | 0x10; /* turn on make xfcb */
}

/* ---- show label & xfcb ---- */

/* display the new password */
static void show_passwd(void) {
    printx(SET_ARR263); /* Password = */
    g->passwd[16] = 0;
    printx(SET_PASSWD + 8);
}

/* show the label options */
static void showlbl(void) {
    uint8_t make, access_;
    print(SET_ARR264); /* Label for drive */
    printdrv();
    crlf();
    print(SET_LABEL1);
    printfn();

    /* passwords required */
    if (rel((FEXT & 0x80) == 0x80) & 1)
        printx(SET_ON);
    else
        printx(SET_OFF);

    /* make xfcbs */
    if ((make = rel((FEXT & 0x10) == 0x10)) & 1)
        printx(SET_ON);
    else
        printx(SET_OFF);

    /* stamp create */
    access_ = rel((FEXT & 0x40) == 0x40);
    if ((make & (uint8_t)~access_) & 1)
        printx(SET_ON);
    else
        printx(SET_OFF);

    /* stamp access */
    if (access_ & 1)
        printx(SET_ON);
    else
        printx(SET_OFF);

    /* stamp update */
    if (rel((FEXT & 0x20) == 0x20) & 1)
        printx(SET_ON);
    else
        printx(SET_OFF);

    crlf();
    if (FEXT & 1) {
        crlf();
        show_passwd();
    }
}

/* display xfcb attributes */
static void show_xfcb(void) {
    if (rel(XFCBMODE != 0) & 1) {
        if (rel(XFCBMODE > 1) & 1)
            if ((uint8_t)~g->password & 1) {
                printx(SET_ERRORMSG);
                printx(SET_ARR296); /* Assign a password to this file. */
                return;             /* error condition */
            }
        printx(SET_ARR297); /* Protection = */
        if (rel((XFCBMODE & 0x80) == 0x80) & 1)
            printx(SET_READMODE);
        else if (rel((XFCBMODE & 0x40) == 0x40) & 1)
            printx(SET_WRITEMODE);
        else if (rel((XFCBMODE & 0x20) == 0x20) & 1)
            printx(SET_DELETEMODE);
        else if (((uint8_t)~XFCBMODE | rel(g->passwd[8] == ' ')) & 1)
            printx(SET_NOPASSWD);
        else
            printx(SET_READMODE);
        if (g->time_opt & 1) printx(SET_COMMA);
    }
    if (g->time_opt & 1) printx(SET_TIMESTAMP);
    if (XFCBMODE & 1) { /* lsb on */
        printx(SET_COMMA);
        show_passwd();
    }
}

/* ---- write xfcb, label and file attributes ---- */

/* display the file or xfcb */
static void put_file(void) {
    crlf();
    printfn();
    printb();
    printb();
}

/* write file attributes */
static void put_attributes(void) {
    g->error_code = setind(g->fcbp);
    if (rel((g->error_code & 0xFF) == 0xFF) & 1) {
        if (rel((g->error_code >> 8) != 0) & 1) {
            put_file();
            bdos_error();
            if (rel((g->error_code >> 8) == 7) & 1) {
                crlf();
                getpasswd();
                crlf();
                g->error_code = setind(g->fcbp);
                if (rel((g->error_code >> 8) != 0) & 1) {
                    put_file();
                    bdos_error();
                }
            }
        } else {
            printx(SET_NOTFOUND);
        }
    }
    if (rel((g->error_code & 0xFF) != 0xFF) & 1) {
        uint8_t extmsk = far_ptr(rt_phys(), g->dpbptr[1], (uint16_t)(g->dpbptr[0] + dpb_exm))[0];
        if (rel(FEXT <= extmsk) & 1) {
            put_file();
            printatt();
        }
    }
    g->scase = 0xFF;
}

/* write label's error */
static void err(void) {
    print(SET_DIRLABEL);
    bdos_error();
}

/* write new label */
static void write_label(void) {
    g->error_code = wrlbl(g->fcbp);
    if (rel((g->error_code & 0xFF) == 0xFF) & 1) {
        if (rel((g->error_code >> 8) != 0) & 1) {
            err();
            if (rel((g->error_code >> 8) == 7) & 1) {
                crlf();
                getpasswd();
                g->error_code = wrlbl(g->fcbp);
                if (rel((g->error_code >> 8) != 0) & 1) {
                    err();
                    terminate();
                }
                crlf();
            }
        } else {
            print(SET_FAILED);
            terminate();
        }
    }
    /* successful */
    showlbl();
    g->lblcmd = F;
}

/* write out new xfcb */
static void write_xfcb(void) {
    put_file();
    g->error_code = wrxfcb(SET_XFCB);
    if (rel((g->error_code & 0xFF) == 0xFF) & 1) {
        if (rel((g->error_code >> 8) != 0) & 1) {
            bdos_error();
            if (rel((g->error_code >> 8) == 7) & 1) {
                crlf();
                getpasswd();
                crlf();
                put_file();
                g->error_code = wrxfcb(SET_XFCB);
                if (rel((g->error_code >> 8) != 0) & 1) bdos_error();
            }
        } else {
            printx(SET_NOTFOUND);
            printx(SET_NOSPACE);
        }
    }
    if (rel((g->error_code & 0xFF) != 0xFF) & 1) show_xfcb();
    g->xfcbcmd = F;
}

/* ---- command processing ---- */

/* select the disk specified in cmd line */
static void setdisk(void) {
    if (rel(CMD(0) != 0) & 1) {
        g->cdisk = (uint8_t)(CMD(0) - 1);
        select(g->cdisk);
        set_dpb();
    }
}

/* find the next file matching the wildcard */
static uint8_t getfile(void) {
    uint8_t dir_index;
    setdma(SET_DIRBUF);
    if (g->wild & 1) {
        wr16(uda() + uda_dcnt, g->sav_dcnt);
        uda()[uda_searchl] = g->sav_searchl;
        wr16(uda() + uda_searcha, g->sav_searcha);
        dir_index = search_next();
    } else {
        dir_index = search_first(bp_fcb);
    }
    if (rel(dir_index != 0xFF) & 1) {
        set_up_file(dir_index);
        return T;
    }
    return F;
}

/* test if the file is a wildcard */
static uint8_t wildcard(void) {
    for (uint8_t i = 1; i <= fnam; i++)
        if (rel(fcb[i] == '?') & 1) return T;
    return F;
}

/* set up the next file or drive reference */
static void setup_fcb(void) {
    setdisk();
    copy(SET_CMD, bp_fcb, 12);              /* name */
    copy(SET_CMD + 16, SET_PASSWD, 8);      /* password */
    g->time_opt = g->option_found = F;
    if ((rel(fcb[1] != ' ') | rel(fcb[ftyp] != ' ')) & 1) {
        g->fileref = T;
        if (wildcard() & 1) {
            if (getfile() & 1) {
                g->wild = T;
                g->opt_adr = g->parse_fn[0];
            } else {
                print(SET_NOTFOUND);
                terminate();
            }
        } else {
            g->fcbp = bp_fcb;
        }
    } else {
        g->fileref = F;
    }
}

/* parse next option */
static void parse_option(void) {
    if (rel(CMD(1) == 'A') & 1) {             /* A */
        if (rel(CMD(2) == 'C') & 1)
            access();
        else if (g->fileref & 1)
            set_attributes();
        else
            parse_error();
    } else if (rel(CMD(1) == 'C') & 1) {      /* C */
        create();
    } else if (rel(CMD(1) == 'D') & 1) {      /* D */
        if (g->fileref & 1)
            set_attributes();
        else if (rel(CMD(2) == 'E') & 1)
            defaultpass();
        else
            parse_error();
    } else if (rel(CMD(1) == 'F') & 1) {      /* F */
        set_attributes();
    } else if (rel(CMD(1) == 'H') & 1) {      /* H */
        help();
    } else if (rel(CMD(1) == 'M') & 1) {      /* M */
        makestamp();
    } else if (rel(CMD(1) == 'N') & 1) {      /* N */
        lname();
    } else if (rel(CMD(1) == 'P') & 1) {      /* P */
        if (rel(CMD(2) == 'R') & 1)
            protect();
        else if (rel(CMD(2) == 'A') & 1)
            set_password();
        else
            parse_error();
    } else if (rel(CMD(1) == 'R') & 1) {      /* R */
        if (g->fileref & 1)
            set_attributes();
        else
            setdrvstatus();
    } else if ((rel(CMD(1) == 'S') & g->fileref) & 1) { /* S */
        set_attributes();
    } else if ((rel(CMD(1) == 'T') & g->fileref) & 1) { /* T */
        time_();
    } else if (rel(CMD(1) == 'U') & 1) {      /* U */
        update();
    } else if (rel(CMD(1) == 'V') & 1) {      /* V */
        print(SET_VERSION);
    } else if ((rel(CMD(1) == 'X') & g->fileref) & 1) { /* X */
        time_();
    } else {
        parse_error();
    }
}

/* check for more to parse */
static void is_there_more(void) {
    if (rel(g->ibp == 0) & 1) {
        if ((uint8_t)~g->option_found & 1) {
            printx(SET_VERSION);
            print(SET_ERRORMSG);
            printx(SET_ARR469); /* Parameter Required, try SET [HELP] */
            terminate();
        }
        if ((uint8_t)~g->wild & 1) g->more = F;
    }
}

void set_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"SET     ", (const uint8_t *)&set_image, set_image_len});
    g_ds = ds;
    g = RSP_DATA(set_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;

    /* process request */
    g->ver = get_version();
    if ((rel((g->ver & 0xFF) < CPMVERSION) | rel(((g->ver >> 8) & 0x0F) != MPMPRODUCT)) & 1) {
        print(SET_ARR480); /* Requires MP/M 2.0 */
    } else {
        /* scan for global option */
        while (rel(bp[(uint16_t)(bp_buff + g->i)] == ' ') & 1) g->i = (uint8_t)(g->i + 1);
        if (rel(bp[(uint16_t)(bp_buff + g->i)] == '[') & 1) {
            g->option = g->optdel = g->option_found = T;
            g->parse_fn[0] = (uint16_t)(bp_buff + (uint8_t)(g->i + 1));
        } else {
            g->parse_fn[0] = bp_buff + 1;
        }
        g->last_buff_adr = bp_buff + 1; /* used by perror routine */
        g->parse_fn[1] = SET_CMD;
        g->usercode = getuser();
        get_pd_uda();          /* get process descriptor */
        set_dpb();             /* get disk parameter blk */
        g->cdisk = cselect();  /* get current disk */
        g->ibp = parse();
        while (g->more & 1) {
            is_there_more();
            if (g->option & 1)
                parse_option();
            else if (g->more & 1)
                setup_fcb(); /* file or drive reference */

            if (g->optdel & 1) {
                g->option = g->option_found = T;
            } else {
                g->option = F;
                return_errors(0xFF);                      /* bdos return errors */
                if (g->lblcmd & 1) write_label();         /* label options */
                if (rel(g->scase != 0xFF) & 1) put_attributes(); /* file attributes */
                if (g->xfcbcmd & 1) write_xfcb();         /* xfcb attributes */
                return_errors(0);
                if (g->wild & 1) {
                    if (getfile() & 1) {
                        g->parse_fn[0] = g->opt_adr;
                        g->option = g->optdel = T;
                    } else {
                        g->wild = F;
                    }
                }
            }
            is_there_more();
            g->ibp = parse();
        }
    }
    terminate();
}
