/*
 * cons.c -- C version of src-assem/utils/CONS.PLM (a transient, on the
 * disk as CONSOLE.CMD): prints the number of this program's console,
 * "Console = nnn", right-aligned in three places.
 *
 * Its data segment, below, is CONS.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the one PL/M variable (vers) with its
 * initial value, the string constants, and the stack; cons_main
 * installs the image from 100h on. PL/M's semantics are kept as in
 * dir.c (CONS's own `true` is 0FFh): bytes wrap, a relation is 0FFh or
 * 0, IF tests bit 0, and AND evaluates every operand. p$decimal's
 * arguments and d live on PL/M's stack, here C locals.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t vers;
    char arr18[23], arr19[13];
    uint8_t stack[1024];
    uint8_t stack_base;
} cons_data;

_Static_assert(offsetof(cons_data, vers) == CONS_VERS, "vers");
_Static_assert(offsetof(cons_data, arr18) == CONS_ARR18, "arr18");
_Static_assert(offsetof(cons_data, arr19) == CONS_ARR19, "arr19");
_Static_assert(offsetof(cons_data, stack_base) == CONS_STACK_BASE, "stack_base");
_Static_assert(sizeof(cons_data) == CONS_DATA_LEN, "length");

#define T 0xFF /* CONS's true */
#define F 0
#define MPMPRODUCT 0x11
#define CPMVERSION 0x30

const uint16_t cons_image_len = sizeof(cons_data);
const cons_data cons_image = {
    .vers = 0,
    .arr18 = "\r\nRequires MP/M-86 2.0$",
    .arr19 = "\r\nConsole = $",
};

static uint16_t g_ds;
static cons_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static void print_char(uint8_t c) { mon1(2, c); }
static void print_buf(uint16_t a) { mon1(9, a); }
static uint16_t version(void) { return mon3(12, 0); }
static void terminate(void) { mon1(143, 0); }
static uint8_t get_console_number(void) { return mon2(153, 0); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

static void printb(void) { print_char(' '); }

/* print value v, field size = (log10 prec) + 1
   with leading zero suppression if zerosup = true */
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
            print_char((uint8_t)('0' + d));
        }
    }
}

void cons_main(uint16_t ds) {
    rsp_install_transient(ds, &cons_image, cons_image_len);
    g_ds = ds;
    g = RSP_DATA(cons_data, ds);

    g->vers = version();
    if ((rel((g->vers >> 8) != MPMPRODUCT) | rel((g->vers & 0xFF) != CPMVERSION)) & 1) {
        print_buf(CONS_ARR18); /* Requires MP/M-86 2.0 */
        mon1(0, 0);
    }

    print_buf(CONS_ARR19); /* Console = */
    pdecimal(get_console_number(), 100, T);
    terminate();
}
