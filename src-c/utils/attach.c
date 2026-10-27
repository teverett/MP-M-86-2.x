/*
 * attach.c -- C version of src-assem/utils/ATTACH.PLM (a transient,
 * ATTACH.CMD): ATTACH name gives the console the CLI assigned to this
 * program to the process called name -- one whose own default console
 * is the same (the ACB's match) -- through function 149.
 *
 * Its data segment, below, is ATTACH.PLM's as tools/plmc.py lays it
 * out (utils_syms.h): the base page, the PL/M variables (the ACB, and
 * pd$pointer, which the program declares but never uses), the string
 * constants, and the stack; attach_main installs the image from 100h
 * on. PL/M's semantics are kept as in dir.c (ATTACH uses no `true`);
 * plmstart is a procedure here, so its `ret` is a C local.
 *
 * ret is mon5's: CX, the error code (the kernel's conassign_entry
 * fails with BX = 0FFFFh and the code -- e_no_pdname = 20 for no such
 * process -- in CX; 0 on success). With mon3's BX, as the PL/M had it,
 * "Can't Find PD" never showed: docs/bugs.md #133.
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
        uint8_t cns, match;
        uint16_t pd;
        char name[8];
    } acb;
    uint16_t pd_pointer[2]; /* offset, segment (unused) */
    char arr4[20], arr8[17], arr12[17];
    uint8_t stack[1024];
    uint8_t stack_base;
} attach_data;

_Static_assert(offsetof(attach_data, acb.cns) == ATTACH_ACB, "acb");
_Static_assert(offsetof(attach_data, acb.pd) == ATTACH_ACB + 2, "acb.pd");
_Static_assert(offsetof(attach_data, acb.name) == ATTACH_ACB + 4, "acb.name");
_Static_assert(offsetof(attach_data, pd_pointer) == ATTACH_PDPOINTER, "pdpointer");
_Static_assert(offsetof(attach_data, arr4) == ATTACH_ARR4, "arr4");
_Static_assert(offsetof(attach_data, arr8) == ATTACH_ARR8, "arr8");
_Static_assert(offsetof(attach_data, arr12) == ATTACH_ARR12, "arr12");
_Static_assert(offsetof(attach_data, stack_base) == ATTACH_STACK_BASE, "stack_base");
_Static_assert(sizeof(attach_data) == ATTACH_DATA_LEN, "length");

#define CPM_TERMINATE     0
#define MPM_PRINT_CON_BUF 9
#define MPM_VERSION       12
#define MPM_TERMINATE     143
#define MPM_CONASSIGN     149
#define MPM_GET_CON       153
#define MPM86             0x1130

const uint16_t attach_image_len = sizeof(attach_data);
const attach_data attach_image = {
    .arr4 = "\r\nRequires MP/M-86 $",
    .arr8 = "\r\nCan't Find PD $",
    .arr12 = "\r\nAttach Failed $",
};

static uint16_t g_ds;
static attach_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }
static uint16_t mon5(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).cx; }

static uint8_t *bp = 0; /* DS:0, the base page */

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* PL/M's move(count, source, destination), on DS addresses */
static void move(uint16_t n, uint16_t src, uint16_t dst) {
    while (n--) bp[dst++] = bp[src++];
}

void attach_main(uint16_t ds) {
    uint16_t ret;
    rsp_install_transient(ds, &(rsp_image){"ATTACH  ", (const uint8_t *)&attach_image, attach_image_len});
    g_ds = ds;
    g = RSP_DATA(attach_data, ds);
    bp = (uint8_t *)g;

    if (rel(mon3(MPM_VERSION, 0) != MPM86) & 1) {
        mon1(MPM_PRINT_CON_BUF, ATTACH_ARR4); /* Requires MP/M-86 */
        mon1(CPM_TERMINATE, 0);
    }

    g->acb.cns = mon2(MPM_GET_CON, 0); /* get console number this program is */
    g->acb.match = 0xFF;               /* running at                         */
    g->acb.pd = 0;
    move(8, bp_fcb + 1, ATTACH_ACB + 4);

    if (rel((ret = mon5(MPM_CONASSIGN, ATTACH_ACB)) == 20) & 1)
        mon1(MPM_PRINT_CON_BUF, ATTACH_ARR8); /* Can't Find PD */
    else if (rel(ret != 0) & 1)
        mon1(MPM_PRINT_CON_BUF, ATTACH_ARR12); /* Attach Failed */

    mon1(MPM_TERMINATE, 0);
}
