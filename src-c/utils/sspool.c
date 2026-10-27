/*
 * sspool.c -- C version of src-assem/utils/SSPOOL.PLM (a transient,
 * SSPOOL.CMD; its PL/M module is stopsplr): stops the spooler -- it
 * aborts every process named SPOOL on this console, or on the console
 * (0-253) the command names, and says whether there was one.
 *
 * Its data segment, below, is SSPOOL.PLM's as tools/plmc.py lays it
 * out (utils_syms.h): the base page, the variables with their initial
 * values (the abort parameter block names SPOOL), the string
 * constants, and the stack; sspool_main installs the image from 100h
 * on. PL/M's semantics are kept as in dir.c (comlit.lit's `true` is
 * 0FFh, though SSPOOL tests none): the console digits are turned into
 * numbers in place in the fcb, and AND evaluates both sides, so the
 * digit loop reads fcb(255) once it has seen a non-digit.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb 0x5C

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    struct RSP_PACKED {
        uint16_t pd, term;
        uint8_t cns, net;
        char pname[8];
    } abort_param_block;
    uint8_t i;
    uint16_t console;
    char arr4[19], arr25[30], arr32[16], arr33[20];
    uint8_t stack[1024];
    uint8_t stack_base;
} sspool_data;

_Static_assert(offsetof(sspool_data, abort_param_block) == SSPOOL_ABORTPARAMBLOCK, "abortparamblock");
_Static_assert(offsetof(sspool_data, i) == SSPOOL_I, "i");
_Static_assert(offsetof(sspool_data, console) == SSPOOL_CONSOLE, "console");
_Static_assert(offsetof(sspool_data, arr4) == SSPOOL_ARR4, "arr4");
_Static_assert(offsetof(sspool_data, arr25) == SSPOOL_ARR25, "arr25");
_Static_assert(offsetof(sspool_data, arr32) == SSPOOL_ARR32, "arr32");
_Static_assert(offsetof(sspool_data, arr33) == SSPOOL_ARR33, "arr33");
_Static_assert(offsetof(sspool_data, stack_base) == SSPOOL_STACK_BASE, "stack_base");
_Static_assert(sizeof(sspool_data) == SSPOOL_DATA_LEN, "length");

#define MPM_VERSION   12
#define MPM_TERMINATE 143
#define MPM_86        0x1130

const uint16_t sspool_image_len = sizeof(sspool_data);
const sspool_data sspool_image = {
    .abort_param_block = {0, 0x00FF, 0, 0, "SPOOL   "},
    .arr4 = "\r\nRequires MP/M-86$",
    .arr25 = "\r\nIllegal Console, Use 0-253 $",
    .arr32 = "Spooler aborted$",
    .arr33 = "Spooler not running$",
};

static uint16_t g_ds;
static sspool_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *fcb = 0; /* the base page's fcb */

static void print_console_buffer(uint16_t a) { mon1(9, a); }
static void system_reset(void) { mon1(0, 0); }
static uint8_t console_number(void) { return mon2(153, 0); }
static uint8_t abort_process(uint16_t a) { return mon2(157, a); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

void sspool_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"SSPOOL  ", (const uint8_t *)&sspool_image, sspool_image_len});
    g_ds = ds;
    g = RSP_DATA(sspool_data, ds);
    fcb = g->base + bp_fcb;

    if (rel(mon3(MPM_VERSION, 0) != MPM_86) & 1) {
        print_console_buffer(SSPOOL_ARR4); /* Requires MP/M-86 */
        mon1(0, 0);
    }
    if (rel(fcb[1] == ' ') & 1) {
        g->abort_param_block.cns = console_number();
    } else {
        g->i = 1;
        g->console = 0;
        while ((rel(fcb[g->i] != ' ') & rel(g->i < 4)) & 1) {
            if (rel((fcb[g->i] = (uint8_t)(fcb[g->i] - '0')) <= 9) & 1) {
                g->console = (uint16_t)(fcb[g->i] + 10 * g->console);
                g->i = (uint8_t)(g->i + 1);
            } else {
                g->i = 255; /* non - numeric */
            }
        }
        if ((rel(g->console > 253) | rel(g->i == 255)) & 1) {
            print_console_buffer(SSPOOL_ARR25); /* Illegal Console, Use 0-253 */
            mon1(MPM_TERMINATE, 0);
        }
        g->abort_param_block.cns = (uint8_t)g->console;
    }

    if (rel(abort_process(SSPOOL_ABORTPARAMBLOCK) == 0) & 1) {
        while (rel(abort_process(SSPOOL_ABORTPARAMBLOCK) == 0) & 1) {
        }
        print_console_buffer(SSPOOL_ARR32); /* Spooler aborted */
    } else {
        print_console_buffer(SSPOOL_ARR33); /* Spooler not running */
    }
    system_reset();
}
