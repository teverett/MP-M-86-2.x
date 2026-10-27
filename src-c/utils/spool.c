/*
 * spool.c -- C version of src-assem/utils/SPOOL.PLM (a transient,
 * SPOOL.CMD): prints files in the background. It opens each file the
 * tail names (read-only), stopping at the first it can't, takes the
 * largest memory block it can (to 0FFFh paragraphs; its own 512-byte
 * buffer if that's under 40 paragraphs), detaches from the console,
 * and for each file attaches the list device and copies the file to it
 * up to its ^Z, a buffer-load at a time, expanding tabs; a key on the
 * console stops it.
 *
 * Its data segment, below, is SPOOL.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the variables with their initial
 * values, the string constants, and the stack; spool_main installs the
 * image from 100h on. PL/M's semantics are kept as in dir.c (SPOOL's
 * `true` is comlit.lit's 0FFh). SPOOL is $compact: bufpointer is a
 * segment:offset pointer, at the allocated block or local$buffer.
 * Two of its PL/M quirks are kept: `if ret = mon2(get$max$mem, ...)`
 * compares (ret is never set, so it tests for success), and AND
 * evaluates both sides -- the length test reads the MCB after the call.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb  0x5C
#define bp_buff 0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint8_t ch, column, itab, jtab, i;
    uint16_t bufpointer[2]; /* offset, segment */
    uint16_t nmbufs, actbuf;
    uint8_t local_buffer[512];
    uint16_t pcb[2]; /* field$adr, fcb$adr */
    uint8_t ret, ret2;
    uint16_t nxt_chr_adr;
    uint8_t reserved_for_disk[3];
    uint16_t mcb_b, mcb_len; /* mcb: base, length, ext (defs.h's mcb_base etc. are offsets) */
    uint8_t mcb_x;
    char arr1[3], arr50[19], arr57[22], arr61[21], arr72[154];
    uint8_t stack[1024];
    uint8_t stack_base;
} spool_data;

_Static_assert(offsetof(spool_data, ch) == SPOOL_CHAR, "char");
_Static_assert(offsetof(spool_data, column) == SPOOL_COLUMN, "column");
_Static_assert(offsetof(spool_data, itab) == SPOOL_ITAB, "itab");
_Static_assert(offsetof(spool_data, jtab) == SPOOL_JTAB, "jtab");
_Static_assert(offsetof(spool_data, i) == SPOOL_I, "i");
_Static_assert(offsetof(spool_data, bufpointer) == SPOOL_BUFPOINTER, "bufpointer");
_Static_assert(offsetof(spool_data, nmbufs) == SPOOL_NMBUFS, "nmbufs");
_Static_assert(offsetof(spool_data, actbuf) == SPOOL_ACTBUF, "actbuf");
_Static_assert(offsetof(spool_data, local_buffer) == SPOOL_LOCALBUFFER, "localbuffer");
_Static_assert(offsetof(spool_data, pcb) == SPOOL_PCB, "pcb");
_Static_assert(offsetof(spool_data, ret) == SPOOL_RET, "ret");
_Static_assert(offsetof(spool_data, ret2) == SPOOL_RET2, "ret2");
_Static_assert(offsetof(spool_data, nxt_chr_adr) == SPOOL_NXTCHRADR, "nxtchradr");
_Static_assert(offsetof(spool_data, reserved_for_disk) == SPOOL_RESERVEDFORDISK, "reservedfordisk");
_Static_assert(offsetof(spool_data, mcb_b) == SPOOL_MCB, "mcb");
_Static_assert(offsetof(spool_data, arr1) == SPOOL_ARR1, "arr1");
_Static_assert(offsetof(spool_data, arr50) == SPOOL_ARR50, "arr50");
_Static_assert(offsetof(spool_data, arr57) == SPOOL_ARR57, "arr57");
_Static_assert(offsetof(spool_data, arr61) == SPOOL_ARR61, "arr61");
_Static_assert(offsetof(spool_data, arr72) == SPOOL_ARR72, "arr72");
_Static_assert(offsetof(spool_data, stack_base) == SPOOL_STACK_BASE, "stack_base");
_Static_assert(sizeof(spool_data) == SPOOL_DATA_LEN, "length");

#define T 0xFF /* SPOOL's true (comlit.lit) */
#define F 0
#define MPM_PRODUCT 0x11
#define CTRLZ 0x1A
#define LF    0x0A
#define MODE1 6 /* offset in fcb for r/o attribute */

/* CP/M, XDOS function numbers */
#define SET_DMA_BASE 51
#define GET_MAX_MEM  53
#define FREE_MEM     57
#define DETACH       147
#define PARSE_FNAME  152
#define ATTACH_LIST  158
#define DETACH_LIST  159

const uint16_t spool_image_len = sizeof(spool_data);
const spool_data spool_image = {
    .pcb = {0, bp_fcb},
    .mcb_b = 0,
    .mcb_len = 0x0FFF, /* alloc 64k max */
    .mcb_x = 0,
    .arr1 = "\n\r$",
    .arr50 = "Requires MP/M-86\r\n$",
    .arr57 = "\r\nIllegal File Name\r\n$",
    .arr61 = "\r\nCan't Open File = $",
    .arr72 = "MP/M-86 V2.0 Spooler\r\n"
             "- Enter STOPSPLR to abort the spooler\r\n"
             "- Enter ATTACH SPOOL to re-attach console to spooler\r\n"
             "*** Spooler Detaching From Console ***$",
};

static uint16_t g_ds;
static spool_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *fcb = 0; /* the base page's fcb */

static uint8_t read_console(void) { return mon2(1, 0); }
static void print_console_buffer(uint16_t a) { mon1(9, a); }
static uint8_t check_console_status(void) { return mon2(11, 0); }
static uint16_t version(void) { return mon3(12, 0); }
static uint8_t open(uint16_t a) { return mon2(15, a); }
static uint8_t readbf(uint16_t a) { return mon2(20, a); }
static void set_dma(uint16_t a) { mon1(26, a); }
static void free_drives(void) { mon1(39, 0xFFFF); }
static void lo(uint8_t c) { mon1(5, c); }
static void system_reset(void) { mon1(0, 0); }
static void crlf(void) { print_console_buffer(SPOOL_ARR1); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* buffer, based bufpointer: a far pointer */
static uint8_t buffer(uint16_t n) {
    return *far_ptr(rt_phys(), g->bufpointer[1], (uint16_t)(g->bufpointer[0] + n));
}

/* list one 128-byte record of the buffer; true at its ^Z */
static uint8_t list_buf(uint16_t sector) {
    uint8_t i;
    for (i = 0; rel(i <= 127) & 1; i++) {
        if (rel((g->ch = buffer((uint16_t)(i + sector))) == CTRLZ) & 1) return T;
        g->itab = rel(g->ch == 0x09) & (uint8_t)(7 - (g->column & 7));
        if (rel(g->ch == 0x09) & 1) g->ch = ' ';
        for (g->jtab = 0; rel(g->jtab <= g->itab) & 1; g->jtab++) {
            if (rel(g->ch >= ' ') & 1) g->column = (uint8_t)(g->column + 1);
            if (rel(g->ch == LF) & 1) g->column = 0;
            lo(g->ch);
            if (check_console_status() & 1) {
                i = read_console(); /* under MP/M-80 forced a dispatch */
                system_reset();
            }
        }
    }
    return F;
}

/* read the file a buffer-load at a time, listing each, to its ^Z or end */
static void copy_file(void) {
    uint8_t ok;
    uint16_t i;
    while (T & 1) {
        g->actbuf = 0;
        ok = T;
        while (ok & 1) {
            set_dma((uint16_t)(g->bufpointer[0] + g->actbuf * 128));
            if ((ok = rel(readbf(bp_fcb) == 0)) & 1) {
                ok = rel((g->actbuf = (uint16_t)(g->actbuf + 1)) < g->nmbufs);
            } else {
                if (rel(g->actbuf == 0) & 1) return;
            }
        }
        for (i = 1; rel(i <= g->actbuf) & 1; i++)
            if (list_buf((uint16_t)((i - 1) * 128)) & 1) return;
    }
}

void spool_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"SPOOL   ", (const uint8_t *)&spool_image, spool_image_len});
    g_ds = ds;
    g = RSP_DATA(spool_data, ds);
    fcb = g->base + bp_fcb;

    if (rel((version() >> 8) != MPM_PRODUCT) & 1) {
        print_console_buffer(SPOOL_ARR50); /* Requires MP/M-86 */
        system_reset();
    }

    g->nxt_chr_adr = bp_buff; /* make sure files exist */
    while (rel(g->nxt_chr_adr != 0) & 1) {
        g->pcb[0] = (uint16_t)(g->nxt_chr_adr + 1);
        g->nxt_chr_adr = mon3(PARSE_FNAME, SPOOL_PCB);
        fcb[MODE1] |= 0x80; /* open files in read only mode */
        if (rel(g->nxt_chr_adr == 0xFFFF) & 1) {
            print_console_buffer(SPOOL_ARR57); /* Illegal File Name */
            system_reset();
        } else if (rel(open(bp_fcb) == 0xFF) & 1) {
            print_console_buffer(SPOOL_ARR61); /* Can't Open File = */
            fcb[MODE1] &= 0x7F; /* the R/O bit off to print the name (bugs.md) */
            fcb[12] = '$';
            print_console_buffer(bp_fcb + 1);
            crlf();
            system_reset();
        }
    }

    /* AND evaluates both sides, the MCB's length after the call */
    uint8_t got = rel(g->ret == mon2(GET_MAX_MEM, SPOOL_MCB));
    if ((got & rel(g->mcb_len >= sizeof g->local_buffer / 16 + 8)) & 1) {
        /* successful memory allocation and bigger than local buf */
        if (rel((g->nmbufs = (uint16_t)(g->mcb_len >> 3)) > 512) & 1) { /* larger than 64K ? */
            g->nmbufs = 512;
            g->mcb_b = (uint16_t)(g->mcb_b + 0x1000);
            g->mcb_len = (uint16_t)(g->mcb_len - 0x1000);
            g->mcb_x = 0;
            mon1(FREE_MEM, SPOOL_MCB); /* return extra memory past 64K */
        }
        mon1(SET_DMA_BASE, g->mcb_b);
        g->bufpointer[1] = g->mcb_b;
        g->bufpointer[0] = 0;
    } else { /* not enough external memory: use buffer internal to program */
        g->bufpointer[0] = SPOOL_LOCALBUFFER;
        g->bufpointer[1] = g_ds;
        g->nmbufs = sizeof g->local_buffer / 128;
    }

    print_console_buffer(SPOOL_ARR72); /* MP/M-86 V2.0 Spooler ... */
    mon1(DETACH, 0);

    g->nxt_chr_adr = bp_buff;
    while ((rel(g->nxt_chr_adr != 0) & rel(g->nxt_chr_adr != 0xFFFF)) & 1) {
        g->pcb[0] = (uint16_t)(g->nxt_chr_adr + 1);
        g->nxt_chr_adr = mon3(PARSE_FNAME, SPOOL_PCB);
        if (rel(g->nxt_chr_adr != 0xFFFF) & 1) {
            fcb[MODE1] |= 0x80; /* open files in read only mode */
            if (rel(open(bp_fcb) != 0xFF) & 1) {
                fcb[32] = 0;
                mon1(ATTACH_LIST, 0);
                copy_file();
                mon1(DETACH_LIST, 0);
                free_drives();
            }
        }
    }

    system_reset();
}
