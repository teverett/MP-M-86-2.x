/*
 * abort.c -- C version of src-assem/utils/ABORT.PLM (a transient,
 * ABORT.CMD): ABORT name [n] aborts the first process called name on
 * console n (this one's own console when n isn't given), through the
 * Abort Parameter Block and function 157, and says when it can't.
 *
 * Its data segment, below, is ABORT.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (the APB among them), the string constants, and the stack;
 * abort_main installs the image from 100h on. PL/M's semantics are
 * kept as in dir.c (ABORT uses no `true`): bytes wrap, a relation is
 * 0FFh or 0, IF tests bit 0, and AND evaluates both sides -- so the
 * console-number loop reads fcb16(i) even when i is 4, or 255 (the
 * "non-numeric" mark), and once i is 255 that read is beyond the base
 * page, in the stack (the value doesn't matter: i < 4 is false). Each
 * digit is converted in place in fcb16, as the PL/M does.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb   0x5C
#define bp_fcb16 0x6C

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t pb_pd;   /* abort$pb: pd, */
    uint16_t pb_term; /* term, */
    uint8_t pb_cns;   /* cns, */
    uint8_t pb_net;   /* net, */
    char pb_pname[8]; /* pname */
    uint8_t i;
    uint16_t console;
    char arr4[19], arr25[30], arr29[16];
    uint8_t stack[1024];
    uint8_t stack_base;
} abort_data;

_Static_assert(offsetof(abort_data, pb_pd) == ABORT_ABORTPB, "abortpb");
_Static_assert(offsetof(abort_data, pb_cns) == ABORT_ABORTPB + 4, "abortpb.cns");
_Static_assert(offsetof(abort_data, pb_pname) == ABORT_ABORTPB + 6, "abortpb.pname");
_Static_assert(offsetof(abort_data, i) == ABORT_I, "i");
_Static_assert(offsetof(abort_data, console) == ABORT_CONSOLE, "console");
_Static_assert(offsetof(abort_data, arr4) == ABORT_ARR4, "arr4");
_Static_assert(offsetof(abort_data, arr25) == ABORT_ARR25, "arr25");
_Static_assert(offsetof(abort_data, arr29) == ABORT_ARR29, "arr29");
_Static_assert(offsetof(abort_data, stack_base) == ABORT_STACK_BASE, "stack_base");
_Static_assert(sizeof(abort_data) == ABORT_DATA_LEN, "length");

#define MPM_VERSION 12
#define MPM86       0x1130

const uint16_t abort_image_len = sizeof(abort_data);
const abort_data abort_image = {
    .pb_pd = 0,
    .pb_term = 0x00FF, /* low byte 0FFh: abort even a system process */
    .pb_cns = 0,
    .pb_net = 0,
    .pb_pname = "        ",
    .arr4 = "\r\nRequires MP/M-86$",
    .arr25 = "\r\nIllegal Console, Use 0-253 $",
    .arr29 = "\r\nAbort Failed.$",
};

static uint16_t g_ds;
static abort_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* DS:0 -- the base page and beyond -- and its fcb16 */
static uint8_t *fcb16 = 0;

static void print_buf(uint16_t a) { mon1(9, a); }
static void terminate(void) { mon1(143, 0); }
static uint8_t console_number(void) { return mon2(153, 0); }
static uint8_t abort_process(uint16_t apb) { return mon2(157, apb); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* PL/M's move(count, source, destination), on DS addresses */
static void move(uint16_t n, uint16_t src, uint16_t dst) {
    while (n--) bp[dst++] = bp[src++];
}

void abort_main(uint16_t ds) {
    rsp_install_transient(ds, &abort_image, abort_image_len);
    g_ds = ds;
    g = RSP_DATA(abort_data, ds);
    bp = (uint8_t *)g;
    fcb16 = bp + bp_fcb16;

    if (rel(mon3(MPM_VERSION, 0) != MPM86) & 1) {
        print_buf(ABORT_ARR4); /* Requires MP/M-86 */
        mon1(0, 0);
    }

    if (rel(fcb16[1] == ' ') & 1) {
        g->pb_cns = console_number();
    } else {
        g->i = 1;
        g->console = 0;
        /* PL/M's AND: both sides, so fcb16(i) is read even when i >= 4 */
        while ((rel(fcb16[g->i] != ' ') & rel(g->i < 4)) & 1) {
            if (rel((fcb16[g->i] = (uint8_t)(fcb16[g->i] - '0')) <= 9) & 1) {
                g->console = (uint16_t)(fcb16[g->i] + 10 * g->console);
                g->i = (uint8_t)(g->i + 1);
            } else {
                g->i = 255; /* non - numeric */
            }
        }
        if ((rel(g->console > 253) | rel(g->i == 255)) & 1) {
            print_buf(ABORT_ARR25); /* Illegal Console, Use 0-253 */
            terminate();
        }
        g->pb_cns = (uint8_t)g->console;
    }

    move(8, bp_fcb + 1, ABORT_ABORTPB + 6);
    if (rel(abort_process(ABORT_ABORTPB) == 0xFF) & 1) print_buf(ABORT_ARR29); /* Abort Failed. */
    /* abort first PD found with same name and console */
    /* consistent with MP/M-80 II but not MP/M 1.x     */
    terminate();
}
