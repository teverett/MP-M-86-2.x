/*
 * dir.c -- C version of src-assem/utils/DIR.PLM (a transient, DIR.CMD):
 * lists the directory, four names a line, for the current user (or the
 * one a [G n] option names), leaving out system files unless [S] is
 * given, and says when there are any.
 *
 * Its data segment, below, is DIR.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, then the PL/M variables with their
 * initial values, the string constants, and the stack; dir_main
 * installs the image from 100h on. PL/M's semantics are kept: bytes
 * wrap, a relation is 0FFh or 0, IF tests bit 0 (DIR's own `true` is
 * 1), and the variables are the PL/M globals, so they keep their values
 * between procedures as they do there.
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
    char dir_title[23];
    uint8_t sys, temp, dcnt, cnt, user;
    uint8_t i, new_user, sys_exists, incl_sys, option;
    uint8_t dirbuf[128];
    uint8_t last_dseg_byte;
    char arr47[18], arr51[21], arr55[12];
    uint8_t stack[1024];
    uint8_t stack_base;
} dir_data;

_Static_assert(offsetof(dir_data, parse_fn) == DIR_PARSEFN, "parsefn");
_Static_assert(offsetof(dir_data, dir_title) == DIR_DIRTITLE, "dirtitle");
_Static_assert(offsetof(dir_data, sys) == DIR_SYS, "sys");
_Static_assert(offsetof(dir_data, user) == DIR_USER, "user");
_Static_assert(offsetof(dir_data, i) == DIR_I, "i");
_Static_assert(offsetof(dir_data, option) == DIR_OPTION, "option");
_Static_assert(offsetof(dir_data, dirbuf) == DIR_DIRBUF, "dirbuf");
_Static_assert(offsetof(dir_data, last_dseg_byte) == DIR_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(dir_data, arr47) == DIR_ARR47, "arr47");
_Static_assert(offsetof(dir_data, arr51) == DIR_ARR51, "arr51");
_Static_assert(offsetof(dir_data, arr55) == DIR_ARR55, "arr55");
_Static_assert(offsetof(dir_data, stack_base) == DIR_STACK_BASE, "stack_base");
_Static_assert(sizeof(dir_data) == DIR_DATA_LEN, "length");

#define T 1 /* DIR's true */
#define F 0

const uint16_t dir_image_len = sizeof(dir_data);
const dir_data dir_image = {
    .dir_title = "Directory for User  x:$",
    .i = 0,
    .new_user = T,
    .sys_exists = F,
    .incl_sys = F,
    .option = F,
    .arr47 = "\r\nFile not found.$",
    .arr51 = "\r\nSystem Files Exist$",
    .arr55 = "\r\nBad entry$",
};

static uint16_t g_ds;
static dir_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *fcb = 0;   /* the base page's fcb, tbuff, fcb16 */
static uint8_t *tbuff = 0;
static uint8_t *fcb16 = 0;
static uint8_t *bp = 0;

static uint8_t read_console(void) { return mon2(1, 0); }
static void write_console(uint8_t c) { mon1(2, c); }
static void print_buf(uint16_t a) { mon1(9, a); }
static uint8_t check_con_stat(void) { return mon2(11, 0); }
static uint8_t search_first(uint16_t a) { return mon2(17, a); }
static uint8_t search_next(uint16_t a) { return mon2(18, a); }
static void setdma(uint16_t a) { mon1(26, a); }
static uint8_t get_user_code(void) { return mon2(32, 0xFF); }
static void set_user_code(uint8_t u) { mon1(32, u); }
static uint16_t parse(void) { return mon3(152, DIR_PARSEFN); }
static void terminate(void) { mon1(143, 0); }

static void crlf(void) {
    write_console(0x0D);
    write_console(0x0A);
}

/* delimiter: the byte parse$fn.buff$adr points at */
static uint8_t delimiter(void) { return bp[g->parse_fn[0]]; }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* display directory heading */
static void heading(void) {
    if (g->user > 9) {
        g->dir_title[19] = '1';
        g->dir_title[20] = (char)(g->user - 10 + '0');
    } else {
        g->dir_title[19] = ' ';
        g->dir_title[20] = (char)(g->user + '0');
    }
    print_buf(DIR_DIRTITLE);
}

/* do next directory display */
static void directory(void) {
    if (g->new_user & 1) {
        heading();
        g->new_user = F;
    }
    g->sys_exists = F;
    g->cnt = 0xFF; /* -1 */
    /* if drive is 0 (default) then set to current disk */
    if (rel(fcb[0] == 0) & 1) fcb[0] = (uint8_t)(mon2(25, 0) + 1);
    if (rel(fcb[1] == ' ') & 1) {
        /* check for blank filename => wildcard */
        for (g->i = 1; g->i <= 11; g->i++) fcb[g->i] = '?';
    }
    /* get first file */
    if (rel((g->dcnt = search_first(bp_fcb)) != 0xFF) & 1) {
        while (rel(g->dcnt != 0xFF) & 1) {
            g->temp = (uint8_t)(((g->dcnt >> 3) | (g->dcnt << 5)) & 0x60); /* ror(dcnt,3) and 0110$0000b */
            g->sys = rel((g->dirbuf[(uint8_t)(g->temp + 10)] & 0x80) == 0x80);
            if ((rel(g->dirbuf[g->temp] == g->user) & (g->incl_sys | (uint8_t)~g->sys)) & 1) {
                if (rel((g->cnt = (uint8_t)(g->cnt + 1)) % 4 == 0) & 1) {
                    crlf();
                    write_console((uint8_t)('A' + fcb[0] - 1));
                } else {
                    write_console(' ');
                }
                write_console(':');
                write_console(' ');
                for (g->i = 1; g->i <= 11; g->i++) {
                    if (rel(g->i == 9) & 1) write_console(' ');
                    write_console(g->dirbuf[(uint8_t)(g->temp + g->i)] & 0x7F);
                    if (check_con_stat() & 1) {
                        g->dcnt = read_console();
                        terminate();
                    }
                }
            } else if (g->sys & 1) {
                g->sys_exists = T;
            }
            g->dcnt = search_next(bp_fcb);
        }
    }
    if (rel(g->cnt == 0xFF) & 1) print_buf(DIR_ARR47);   /* File not found. */
    if (g->sys_exists & 1) print_buf(DIR_ARR51);         /* System Files Exist */
}

/* parse next item */
static void parse_next(void) {
    /* skip comma or space delimiter */
    g->parse_fn[0] = (uint16_t)(g->parse_fn[0] + 1);
    g->parse_fn[0] = parse();
    if (rel(g->parse_fn[0] == 0xFFFF) & 1) {
        print_buf(DIR_ARR55); /* Bad entry */
        terminate();
    }
    if (rel(delimiter() == ']') & 1) { /* skip */
        g->parse_fn[0] = (uint16_t)(g->parse_fn[0] + 1);
        if (rel(delimiter() == 0) & 1) g->parse_fn[0] = 0;
        g->option = F;
    }
    if (rel(delimiter() == '[') & 1) g->option = T;
    if (rel(g->parse_fn[0] == 0) & 1) g->option = F;
}

/* parse & interpret option */
static void parse_option(void) {
    g->parse_fn[1] = DIR_DIRBUF;
    while (g->option & 1) {
        parse_next();
        if (rel(g->dirbuf[1] == 'S') & 1) {
            g->incl_sys = T;
        } else if (rel(g->dirbuf[1] == 'G') & 1) {
            if (rel(g->dirbuf[3] != ' ') & 1) g->temp = (uint8_t)(g->dirbuf[3] - '0' + 10);
            else if (rel(g->dirbuf[2] != ' ') & 1) g->temp = (uint8_t)(g->dirbuf[2] - '0');
            if (rel(g->temp < 16) & 1) {
                set_user_code(g->user = g->temp);
                g->new_user = T;
            }
        }
    }
    g->parse_fn[1] = bp_fcb;
}

void dir_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"DIR     ", (const uint8_t *)&dir_image, dir_image_len});
    g_ds = ds;
    g = RSP_DATA(dir_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;
    fcb16 = bp + bp_fcb16;
    tbuff = bp + bp_tbuff;

    g->user = get_user_code();
    g->incl_sys = rel(fcb16[1] == 'S');
    setdma(DIR_DIRBUF);
    g->parse_fn[0] = bp_tbuff;
    g->parse_fn[1] = bp_fcb;

    /* scan for global option */
    while (rel(tbuff[g->i = (uint8_t)(g->i + 1)] == ' ') & 1) {
    }
    if (rel(tbuff[g->i] == '[') & 1) { /* skip leading [ */
        g->parse_fn[0] = (uint16_t)(bp_tbuff + g->i);
        g->option = T;
        parse_option();
        fcb[0] = 0;   /* set current disk */
        fcb[1] = ' '; /* clear fcb */
        directory();
    }

    /* do command line */
    while (rel(g->parse_fn[0] != 0) & 1) {
        parse_next(); /* filename */
        if (g->option & 1) parse_option();
        directory();
    }
    terminate();
}
