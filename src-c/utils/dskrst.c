/*
 * dskrst.c -- C version of src-assem/utils/DSKRST.PLM (a transient, on
 * the disk as DSKRESET.CMD): resets the drives the command tail names
 * -- every letter A to P in it, wherever it is -- or, with no tail,
 * all sixteen, through function 37.
 *
 * Its data segment, below, is DSKRST.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (the mask table, PL/M DATA, among them), the string constant,
 * and the stack; dskrst_main installs the image from 100h on. PL/M's
 * semantics are kept as in dir.c (DSKRST uses no `true`): bytes wrap, a
 * relation is 0FFh or 0, IF tests bit 0, and AND evaluates both sides.
 * The drive bits are read from the image's mask table, as the PL/M
 * reads them.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_tbuff 0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t mask[16];
    uint16_t drive_mask;
    uint8_t i;
    uint16_t vers;
    char arr8[23];
    uint8_t stack[1024];
    uint8_t stack_base;
} dskrst_data;

_Static_assert(offsetof(dskrst_data, mask) == DSKRST_MASK, "mask");
_Static_assert(offsetof(dskrst_data, drive_mask) == DSKRST_DRIVEMASK, "drivemask");
_Static_assert(offsetof(dskrst_data, i) == DSKRST_I, "i");
_Static_assert(offsetof(dskrst_data, vers) == DSKRST_VERS, "vers");
_Static_assert(offsetof(dskrst_data, arr8) == DSKRST_ARR8, "arr8");
_Static_assert(offsetof(dskrst_data, stack_base) == DSKRST_STACK_BASE, "stack_base");
_Static_assert(sizeof(dskrst_data) == DSKRST_DATA_LEN, "length");

#define MPMPRODUCT 0x11
#define CPMVERSION 0x30

const uint16_t dskrst_image_len = sizeof(dskrst_data);
const dskrst_data dskrst_image = {
    .mask = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800,
             0x1000, 0x2000, 0x4000, 0x8000},
    .drive_mask = 0,
    .vers = 0,
    .arr8 = "\r\nRequires MP/M-86 2.0$",
};

static uint16_t g_ds;
static dskrst_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *tbuff = 0; /* the base page's tbuff */

static void print_con_buffer(uint16_t a) { mon1(9, a); }
static uint16_t version(void) { return mon3(12, 0); }
static void reset_drives(uint16_t drive_vector) { mon1(37, drive_vector); }
static void terminate(void) { mon1(143, 0); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

void dskrst_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"DSKRESET", (const uint8_t *)&dskrst_image, dskrst_image_len});
    g_ds = ds;
    g = RSP_DATA(dskrst_data, ds);
    tbuff = (uint8_t *)g + bp_tbuff;

    g->vers = version();
    if ((rel((g->vers >> 8) != MPMPRODUCT) | rel((g->vers & 0xFF) != CPMVERSION)) & 1) {
        print_con_buffer(DSKRST_ARR8); /* Requires MP/M-86 2.0 */
        mon1(0, 0);
    }
    g->i = 0;
    if (rel(tbuff[0] == 0) & 1) {
        g->drive_mask = 0xFFFF;
    } else {
        while (rel((g->i = (uint8_t)(g->i + 1)) <= tbuff[0]) & 1) {
            /* PL/M's AND: both comparisons */
            if ((rel(tbuff[g->i] >= 'A') & rel(tbuff[g->i] <= 'P')) & 1)
                g->drive_mask |= g->mask[(uint8_t)(tbuff[g->i] - 'A')];
        }
    }
    reset_drives(g->drive_mask);
    terminate();
}
