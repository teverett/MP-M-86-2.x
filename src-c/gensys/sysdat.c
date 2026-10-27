/*
 * sysdat.c -- the initial contents of the system data segment, as
 * src-assem/kernel/SYSDAT.A86 assembles them (SYSTEM.DAT, then DATA.BDO from
 * C00h), and XIOS86Q.A86's private data past it (from 1000h): every
 * initialized field, at its label's offset (gensys_syms.h), in the
 * order the .A86 files declare them. Everything else is 0, as the asm's
 * `rb`/`rw` and zero `dw`s are. GENSYS's own work on top of this (the
 * module table's code segments, the pools, the RSP chain) is gensys.c.
 *
 * tests/src-c/test_gensys.py compares the result with the assembled
 * dseg, byte for byte.
 */
#include <stdint.h>

#include "defs.h"
#include "gensys.h"
#include "gensys_syms.h"

/* SYSTEM.DEF / PD.DEF / QD.DEF / MODFUNC.DEF values the data uses */
#define qf_keep   0x02
#define qf_mx     0x01
#define pf_kernal 0x04
#define mod_sup   1
#define mod_rtm   2
#define mod_mem   3
#define mod_cio   4
#define mod_bdos  5
#define mod_net   7

static void w8(uint8_t *d, uint16_t off, uint8_t v) { d[off] = v; }
static void w16v(uint8_t *d, uint16_t off, uint16_t v) { wr16(d + off, v); }
static void str(uint8_t *d, uint16_t off, const char *s) {
    while (*s) d[off++] = (uint8_t)*s++;
}
static void fill16(uint8_t *d, uint16_t off, uint16_t v, int n) {
    for (int i = 0; i < n; i++) wr16(d + off + 2 * i, v);
}

/* a QD (QD.DEF): link, net/org, flags, name, then msglen, nmsgs, dq,
 * nq, msgcnt, msgout, buf */
static void qd(uint8_t *d, uint16_t off, uint16_t link, uint16_t flags, const char *name, uint16_t nmsgs,
               uint16_t msgcnt) {
    w16v(d, off, link);
    w16v(d, off + 4, flags);
    str(d, off + 6, name);
    w16v(d, off + 16, nmsgs);
    w16v(d, off + 22, msgcnt);
}

/* a QPB: flags (2), qid, nmsgs, buffptr, name */
static void qpb(uint8_t *d, uint16_t off, uint16_t qid, uint16_t nmsgs, uint16_t buffptr, const char *name) {
    w16v(d, off + 2, qid);
    w16v(d, off + 4, nmsgs);
    w16v(d, off + 6, buffptr);
    str(d, off + 8, name);
}

/* the 256 entries of sysent (SYSTEM.DAT): each function's module and
 * its number there; entries left out are `sup, 1` */
static const uint8_t sysent[][3] = {
    {0, mod_rtm, 0},     {1, mod_cio, 0},     {2, mod_cio, 1},     {3, mod_cio, 2},     {4, mod_cio, 3},
    {5, mod_cio, 4},     {6, mod_cio, 5},     {7, mod_sup, 0},     {8, mod_sup, 0},     {9, mod_cio, 6},
    {10, mod_cio, 7},    {11, mod_cio, 8},    {12, mod_sup, 2},    {13, mod_bdos, 0},   {14, mod_bdos, 1},
    {15, mod_bdos, 2},   {16, mod_bdos, 3},   {17, mod_bdos, 4},   {18, mod_bdos, 5},   {19, mod_bdos, 6},
    {20, mod_bdos, 7},   {21, mod_bdos, 8},   {22, mod_bdos, 9},   {23, mod_bdos, 10},  {24, mod_bdos, 11},
    {25, mod_bdos, 12},  {26, mod_bdos, 13},  {27, mod_bdos, 14},  {28, mod_bdos, 15},  {29, mod_bdos, 16},
    {30, mod_bdos, 17},  {31, mod_bdos, 18},  {32, mod_bdos, 19},  {33, mod_bdos, 20},  {34, mod_bdos, 21},
    {35, mod_bdos, 22},  {36, mod_bdos, 23},  {37, mod_bdos, 24},  {38, mod_bdos, 25},  {39, mod_bdos, 26},
    {40, mod_bdos, 27},  {41, mod_bdos, 28},  {42, mod_bdos, 29},  {43, mod_bdos, 30},  {44, mod_bdos, 31},
    {45, mod_bdos, 32},  {46, mod_bdos, 33},  {47, mod_sup, 12},   {48, mod_bdos, 35},  {50, mod_sup, 3},
    {51, mod_bdos, 36},  {52, mod_bdos, 37},  {53, mod_mem, 0},    {54, mod_mem, 1},    {55, mod_mem, 2},
    {56, mod_mem, 3},    {57, mod_mem, 4},    {58, mod_mem, 5},    {59, mod_sup, 4},    {64, mod_net, 64},
    {65, mod_net, 65},   {66, mod_net, 66},   {67, mod_net, 67},   {68, mod_net, 68},   {69, mod_net, 69},
    {100, mod_bdos, 38}, {101, mod_bdos, 39}, {102, mod_bdos, 40}, {103, mod_bdos, 41}, {104, mod_bdos, 42},
    {105, mod_bdos, 43}, {106, mod_bdos, 44}, {107, mod_sup, 13},  {128, mod_mem, 6},   {129, mod_mem, 6},
    {130, mod_mem, 7},   {131, mod_rtm, 1},   {132, mod_rtm, 2},   {133, mod_rtm, 3},   {134, mod_rtm, 4},
    {135, mod_rtm, 5},   {136, mod_rtm, 6},   {137, mod_rtm, 7},   {138, mod_rtm, 8},   {139, mod_rtm, 9},
    {140, mod_rtm, 10},  {141, mod_rtm, 11},  {142, mod_rtm, 12},  {143, mod_rtm, 13},  {144, mod_rtm, 14},
    {145, mod_rtm, 15},  {146, mod_cio, 9},   {147, mod_cio, 10},  {148, mod_cio, 11},  {149, mod_cio, 12},
    {150, mod_sup, 5},   {151, mod_sup, 6},   {152, mod_sup, 7},   {153, mod_cio, 13},  {154, mod_sup, 8},
    {155, mod_sup, 9},   {156, mod_rtm, 16},  {157, mod_rtm, 17},  {158, mod_cio, 15},  {159, mod_cio, 16},
    {160, mod_cio, 17},  {161, mod_cio, 18},  {162, mod_cio, 19},  {163, mod_sup, 11},  {164, mod_cio, 20},
};

/* SYSTEM.DAT */
static void system_dat(uint8_t *d) {
    /* module_table: entry, then init, far pointers; every module's
     * entry offset is 3 (its `jmp entry` after `jmp init`) until
     * GENSYS fills in the segments; the XIOS's are its own */
    for (int m = 0; m < 7; m++) w16v(d, (uint16_t)(m * 8), 3);
    w16v(d, 0x28, 0x0D73);
    w16v(d, 0x2C, 0x0D70);
    w16v(d, SYS_MPMSEG, 0x1008);
    w8(d, SYS_MODULE_MAP, 0x3F);
    w8(d, SYS_NCNS, 4);
    w8(d, SYS_NLST, 1);
    w8(d, SYS_NCCB, 5);
    w8(d, SYS_NFLAGS, 0x20);
    w8(d, SYS_SRCHDISK, 1);
    w16v(d, SYS_MMP, 0xFFFF);
    w8(d, SYS_DAYFILE, 0xFF);
    w8(d, SYS_TICKSPERSEC, 60);
    w16v(d, SYS_MDUL, 0x30);
    w16v(d, SYS_PUL, 0x20);
    w16v(d, SYS_QUL, 0x20);
    w16v(d, SYS_QMAU + 4, 0x200); /* qmau: link, start, length, plist */
    w16v(d, SYS_RLR, SYS_INITPD);
    w16v(d, SYS_THRDRT, SYS_INITPD);
    w16v(d, SYS_QLR, SYS_MXLOADQD);
    w16v(d, SYS_VERNUM, 0x1130);    /* MPM-86 w/BDOS v3.0 */
    w16v(d, SYS_MPMVERNUM, 0x1120); /* MPM-86 V1.0 */
    w16v(d, SYS_TOD_DAY, 0x0554);   /* day since 1/1/78 (5 Oct 81) */
    w8(d, SYS_TOD_HR, 0x12);
    w16v(d, SYS_SYSLREC, 32);
    w16v(d, SYS_SYSLFIL, 32);
    w8(d, SYS_LOCK_MAX, 16);
    w8(d, SYS_OPEN_MAX, 16);
    w16v(d, SYS_SYSLTOT, 48);

    for (int f = 0; f < 256; f++) { /* sysent: module, function, flags */
        w8(d, (uint16_t)(SYS_SYSENT + 3 * f), mod_sup);
        w8(d, (uint16_t)(SYS_SYSENT + 3 * f + 1), 1);
    }
    for (unsigned i = 0; i < sizeof sysent / sizeof sysent[0]; i++) {
        w8(d, (uint16_t)(SYS_SYSENT + 3 * sysent[i][0]), sysent[i][1]);
        w8(d, (uint16_t)(SYS_SYSENT + 3 * sysent[i][0] + 1), sysent[i][2]);
    }

    /* the mutual exclusion queues: MXLoad, MXcli, MXmemory */
    qd(d, SYS_MXLOADQD, SYS_MXCLIQD, qf_keep + qf_mx, "MXLoad  ", 1, 1);
    qpb(d, SYS_MXLOADQPB, SYS_MXLOADQD, 1, 0, "MXLoad  ");
    qd(d, SYS_MXCLIQD, SYS_MXMEMQD, qf_keep + qf_mx, "MXcli   ", 1, 1);
    qpb(d, SYS_MXCLIQPB, SYS_MXCLIQD, 1, 0, "MXcli   ");
    qd(d, SYS_MXMEMQD, 0, qf_keep + qf_mx, "MXmemory", 1, 1);
    qpb(d, SYS_MXMEMQPB, SYS_MXMEMQD, 1, 0, "MXmemory");

    /* the CLI's blocks */
    qpb(d, SYS_CLI_CUSPQPB, 0, 0, SYS_CLI_PPD, "$$$$$$$$");
    str(d, SYS_CLI_ACB + 4, "$$$$$$$$");
    w16v(d, SYS_CLI_PCB, SYS_CLI_CMDTAIL);
    w16v(d, SYS_CLI_PCB + 2, SYS_CLI_FCB);
    w8(d, SYS_CLI_LDDSK, 1);

    /* the idle and tick UDAs: just their stack offsets (u_stack_sp) */
    w16v(d, SYS_IDLEUDA + u_stack_sp, SYS_IDLE_TOS);
    w16v(d, SYS_TICKUDA + u_stack_sp, SYS_TICK_TOS);

    /* the Init, Idle and Tick PDs */
    w8(d, SYS_INITPD + p_prior, 1);
    w16v(d, SYS_INITPD + p_flag, pf_sys + pf_kernal);
    str(d, SYS_INITPD + p_name, "Init    ");
    w16v(d, SYS_IDLEPD + p_link, SYS_TICKPD);
    w8(d, SYS_IDLEPD + p_prior, 255);
    w16v(d, SYS_IDLEPD + p_flag, pf_sys + pf_kernal + pf_keep);
    str(d, SYS_IDLEPD + p_name, "Idle    ");
    w16v(d, SYS_IDLEPD + p_uda, SYS_IDLEUDA / 0x10);
    w8(d, SYS_TICKPD + p_prior, 10);
    w16v(d, SYS_TICKPD + p_flag, pf_sys + pf_kernal + pf_keep);
    str(d, SYS_TICKPD + p_name, "Tick    ");
    w16v(d, SYS_TICKPD + p_uda, SYS_TICKUDA / 0x10);

    /* their stacks: 0CCCCh fill, each with an initial frame (start
     * address and segment unknown, flags 200h) */
    fill16(d, SYS_TICKPD + 0x30, 0xCCCC, 21);
    w16v(d, SYS_IDLE_TOS + 4, 0x0200);
    fill16(d, SYS_IDLE_TOS + 6, 0xCCCC, 21);
    w16v(d, SYS_TICK_TOS + 4, 0x0200);
    fill16(d, SYS_TICK_TOS + 6, 0xCCCC, 39);
    fill16(d, SYS_INIT_TOS, 0xCCCC, 48);
}

/* DATA.BDO */
static void data_bdo(uint8_t *d) {
    w8(d, SYS_EFCB, 0xE5);
    w8(d, SYS_CURDSK, 0xFF);
    w16v(d, SYS_XDMAAD, SYS_COMMON_DMA);
    w16v(d, SYS_OPEN_ROOT - 2, SYS_FREE_ROOT);

    str(d, SYS_DSKMSG, "Bdos Err On ");
    str(d, SYS_DSKERR, " : ");
    str(d, SYS_PERMSG, "Bad Sector");
    str(d, SYS_SELMSG, "Select");
    str(d, SYS_ROFMSG, "File ");
    str(d, SYS_RODMSG, "R/O");
    static const uint16_t xerr[] = {SYS_XE3, SYS_XE4, SYS_XE5, SYS_XE6, SYS_XE7,
                                    SYS_XE8, SYS_XE9, SYS_XE10, SYS_XE11};
    for (unsigned i = 0; i < sizeof xerr / sizeof xerr[0]; i++)
        w16v(d, (uint16_t)(SYS_XERR_LIST + 2 * i), xerr[i]);
    str(d, SYS_XE3, "File Opened in Read/Only Mode"); /* XE4 is its 0 */
    str(d, SYS_XE5, "File Currently Open");
    str(d, SYS_XE6, "Close Checksum Error");
    str(d, SYS_XE7, "Password Error");
    str(d, SYS_XE8, "File Already Exists");
    str(d, SYS_XE9, "Illegal ? in FCB");
    str(d, SYS_XE10, "Open File Limit Exceeded");
    str(d, SYS_XE11, "No Room in System Lock List");
    str(d, SYS_CRLFSTR, "\r\n");
    str(d, SYS_PR_FX, "Bdos Function: ");
    str(d, SYS_PR_FX1, "   ");
    str(d, SYS_PR_FCB, "  File: ");
    str(d, SYS_DENIEDMSG, "\r\rDisk reset denied, Drive ");
    w8(d, SYS_DENIEDDRV + 1, ':');
    str(d, SYS_DENIEDDRV + 2, " Console ");
    str(d, SYS_DENIEDCNS + 1, " Program ");
    str(d, SYS_DENIEDPRC, "12345678");

    fill16(d, SYS_SPSAVE + 2, 0xCCCC, 60); /* the BDOS's own stack */

    /* MXdisk */
    qd(d, SYS_MXDISKQD, 0, qf_mx, "MXdisk  ", 1, 0);
    qpb(d, SYS_MXDISKQPB, 0, 1, 0, "MXdisk  ");
}

/* XIOS86Q.A86's data, past SYSDAT */
static void xios86q_data(uint8_t *d) {
    static const uint8_t dtrntbl[] = {0x00, 0xFF, 0xFF, 0xFF};         /* logical disk 0 -> drive 0 */
    static const uint8_t specify[] = {3, 0x03, 0xDF, 0x02};            /* NEC765 command strings: */
    static const uint8_t recal[] = {2, 0x07, 0};                       /* [length][command]... */
    static const uint8_t seek[] = {3, 0x0F, 0, 0};
    static const uint8_t read[] = {9, 0x46, 0, 0, 0, 0, 2, 18, 0x1B, 0xFF};
    static const uint8_t write[] = {9, 0x45, 0, 0, 0, 0, 2, 18, 0x1B, 0xFF};
    for (unsigned i = 0; i < sizeof dtrntbl; i++) d[XQD_DTRNTBL + i] = dtrntbl[i];
    for (unsigned i = 0; i < sizeof specify; i++) d[XQD_SPECIFY_PARMS + i] = specify[i];
    for (unsigned i = 0; i < sizeof recal; i++) d[XQD_RECAL_PARMS + i] = recal[i];
    for (unsigned i = 0; i < sizeof seek; i++) d[XQD_SEEK_PARMS + i] = seek[i];
    for (unsigned i = 0; i < sizeof read; i++) d[XQD_READ_PARMS + i] = read[i];
    for (unsigned i = 0; i < sizeof write; i++) d[XQD_WRITE_PARMS + i] = write[i];

    /* DPH: xlt 0, 3 scratch words, then dirbuf, dpb, csv, alv */
    w16v(d, XQD_DPH0 + 8, XQD_DIRBUF0);
    w16v(d, XQD_DPH0 + 10, XQD_DPB0);
    w16v(d, XQD_DPH0 + 12, XQD_CSV0);
    w16v(d, XQD_DPH0 + 14, XQD_ALLOC0);
    /* DPB: 80 cyls * 2 heads * 18 512-byte sectors, 4:1 deblocked */
    w16v(d, XQD_DPB0, 72);      /* spt */
    w8(d, XQD_DPB0 + 2, 6);     /* bsh */
    w8(d, XQD_DPB0 + 3, 63);    /* blm */
    w8(d, XQD_DPB0 + 4, 0);     /* exm */
    w16v(d, XQD_DPB0 + 5, 176); /* dsm */
    w16v(d, XQD_DPB0 + 7, 63);  /* drm */
    w8(d, XQD_DPB0 + 9, 0x80);  /* al0 */
    w8(d, XQD_DPB0 + 10, 0);    /* al1 */
    w16v(d, XQD_DPB0 + 11, 16); /* cks */
    w16v(d, XQD_DPB0 + 13, 2);  /* off */

    str(d, XQD_SIGNON, "\r\nMP/M-86 XIOS86Q -- QEMU target (serial console, floppy disk)\r\n");
}

void gensys_sysdat(uint8_t *sd) {
    for (unsigned i = 0; i < XQD_DSEG_LEN; i++) sd[i] = 0;
    system_dat(sd);
    data_bdo(sd);
    xios86q_data(sd);
}
