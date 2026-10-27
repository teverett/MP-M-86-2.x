/*
 * era.c -- C version of src-assem/utils/ERA.PLM (a transient, ERA.CMD):
 * erases the files a name matches -- asking first when it's *.* --
 * and, if that fails for some of them (read-only, open, a password),
 * erases them one at a time, naming the ones it couldn't and asking
 * for the passwords. [X] erases the XFCBs only.
 *
 * Its data segment, below, is ERA.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, PL/M's plmstack and stack$size, the
 * variables with their initial values, the string constants, and the
 * stack; era_main installs the image from 100h on. PL/M's semantics
 * are kept as in dir.c (ERA's own `true` is 1). The one-at-a-time
 * erase keeps search next's place across its deletes by saving and
 * restoring the UDA's dcnt and searchl, as ERA.PLM does.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb   0x5C
#define bp_fcb16 0x6C
#define bp_tbuff 0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t plmstack[16];
    uint16_t stack_size;
    uint16_t parse_fn[2];    /* buff$adr, fcb$adr */
    uint16_t pd_pointer[2];  /* offset, segment */
    uint16_t uda_pointer[2];
    uint8_t xfcb, i, response, user, code;
    uint16_t ver;
    uint8_t last_dseg_byte;
    char arr7[18], arr11[9], arr19[10], arr23[17], arr27[15], arr56[12], arr78[13], arr95[18], arr102[18],
        arr112[37], arr126[10];
    uint8_t stack[1024];
    uint8_t stack_base;
} era_data;

_Static_assert(offsetof(era_data, plmstack) == ERA_PLMSTACK, "plmstack");
_Static_assert(offsetof(era_data, stack_size) == ERA_STACKSIZE, "stacksize");
_Static_assert(offsetof(era_data, parse_fn) == ERA_PARSEFN, "parsefn");
_Static_assert(offsetof(era_data, pd_pointer) == ERA_PDPOINTER, "pdpointer");
_Static_assert(offsetof(era_data, uda_pointer) == ERA_UDAPOINTER, "udapointer");
_Static_assert(offsetof(era_data, xfcb) == ERA_XFCB, "xfcb");
_Static_assert(offsetof(era_data, i) == ERA_I, "i");
_Static_assert(offsetof(era_data, response) == ERA_RESPONSE, "response");
_Static_assert(offsetof(era_data, user) == ERA_USER, "user");
_Static_assert(offsetof(era_data, code) == ERA_CODE, "code");
_Static_assert(offsetof(era_data, ver) == ERA_VER, "ver");
_Static_assert(offsetof(era_data, last_dseg_byte) == ERA_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(era_data, arr7) == ERA_ARR7, "arr7");
_Static_assert(offsetof(era_data, arr11) == ERA_ARR11, "arr11");
_Static_assert(offsetof(era_data, arr19) == ERA_ARR19, "arr19");
_Static_assert(offsetof(era_data, arr23) == ERA_ARR23, "arr23");
_Static_assert(offsetof(era_data, arr27) == ERA_ARR27, "arr27");
_Static_assert(offsetof(era_data, arr56) == ERA_ARR56, "arr56");
_Static_assert(offsetof(era_data, arr78) == ERA_ARR78, "arr78");
_Static_assert(offsetof(era_data, arr95) == ERA_ARR95, "arr95");
_Static_assert(offsetof(era_data, arr102) == ERA_ARR102, "arr102");
_Static_assert(offsetof(era_data, arr112) == ERA_ARR112, "arr112");
_Static_assert(offsetof(era_data, arr126) == ERA_ARR126, "arr126");
_Static_assert(offsetof(era_data, stack_base) == ERA_STACK_BASE, "stack_base");
_Static_assert(sizeof(era_data) == ERA_DATA_LEN, "length");

#define T 1 /* ERA's true */
#define F 0
#define SUCCESSFUL 0xFF
#define MPMPRODUCT 0x01
#define CPMVERSION 0x30
#define CR    13
#define LF    10
#define CTRLX 0x18
#define BKSP  8
#define INT3  0xCCCC

/* the UDA's fields ERA uses (uda.lit) */
#define uda_searchl 0x07
#define uda_dcnt    0x0C
#define pd_uda      0x10 /* proces.lit */

const uint16_t era_image_len = sizeof(era_data);
const era_data era_image = {
    .plmstack = {INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3, INT3},
    .stack_size = 16 + 16,
    .xfcb = 0,
    .last_dseg_byte = 0,
    .arr7 = "\r\nBDOS Bad Sector$",
    .arr11 = "\r\nDrive $",
    .arr19 = "Read Only$",
    .arr23 = "Currently Opened$",
    .arr27 = "Password Error$",
    .arr56 = "Password ? $",
    .arr78 = "Not erased: $",
    .arr95 = "Requires MP/M 2.0$",
    .arr102 = "Invalid Parameter$",
    .arr112 = "Confirm delete all user files (Y/N)?$",
    .arr126 = "\r\nNo file$",
};

static uint16_t g_ds;
static era_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* the base page, and its fcb, fcb16 */
static uint8_t *fcb = 0;
static uint8_t *fcb16 = 0;

static uint8_t read_console(void) { return mon2(1, 0); }
static void printchar(uint8_t c) { mon1(2, c); }
static uint8_t conin(void) { return mon2(6, 0xFD); }
static uint8_t check_con_stat(void) { return mon2(11, 0); }
static void print_buf(uint16_t a) { mon1(9, a); }
static uint16_t version(void) { return mon3(12, 0); }
static void setdma(uint16_t a) { mon1(26, a); }
static uint8_t search(uint16_t a) { return mon2(17, a); }
static uint8_t searchn(void) { return mon2(18, 0); }
static uint16_t delete_file(uint16_t a) { return mon3(19, a); }
static uint8_t get_user_code(void) { return mon2(32, 0xFF); }
static void return_errors(void) { mon1(45, 0xFF); }
static void terminate(void) { mon1(143, 0); }
static void parse(void) { mon1(152, ERA_PARSEFN); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* the UDA, through uda$pointer */
static uint8_t *uda(void) { return far_ptr(rt_phys(), g->uda_pointer[1], g->uda_pointer[0]); }

static void get_uda(void) {
    mpm_regs r = rt_mpm(g_ds, 156, 0); /* mon4: the PD's pointer, ES:BX */
    g->pd_pointer[0] = r.bx;
    g->pd_pointer[1] = r.es;
    g->uda_pointer[1] = rd16(far_ptr(rt_phys(), g->pd_pointer[1], (uint16_t)(g->pd_pointer[0] + pd_uda)));
    g->uda_pointer[0] = 0;
}

static void crlf(void) {
    printchar(CR);
    printchar(LF);
}

/* fill string @ s for c bytes with f */
static void fill(uint8_t *s, uint8_t f, uint8_t c) {
    while (rel((c = (uint8_t)(c - 1)) != 255) & 1) *s++ = f;
}

/* error message routine */
static void error(uint8_t code) {
    printchar(' ');
    if (rel(code == 1) & 1) print_buf(ERA_ARR7);                     /* BDOS Bad Sector */
    if (rel(code == 2) & 1) print_buf(ERA_ARR11);                    /* Drive */
    if ((rel(code == 3) | rel(code == 2)) & 1) print_buf(ERA_ARR19); /* Read Only */
    if (rel(code == 5) & 1) print_buf(ERA_ARR23);                    /* Currently Opened */
    if (rel(code == 7) & 1) print_buf(ERA_ARR27);                    /* Password Error */
    if (rel(code < 3) & 1) terminate();
}

/* print file name */
static void print_file(uint16_t fcbp) {
    for (uint8_t k = 1; k <= 11; k++) {
        if (rel(k == 9) & 1) printchar('.');
        printchar(bp[(uint16_t)(fcbp + k)] & 0x7F);
    }
}

/* try to delete fcb at fcb_address; return error code if unsuccessful */
static uint8_t delete(uint16_t fcb_address) {
    uint8_t *fcbv = bp + fcb_address;
    uint16_t error_code;
    uint8_t code;
    if (g->xfcb & 1) fcbv[5] |= 0x80;
    setdma(bp_fcb16); /* password */
    fcbv[0] = fcb[0]; /* drive */
    error_code = delete_file(fcb_address);
    fcbv[5] &= 0x7F;  /* reset xfcb bit */
    if (rel((error_code & 0xFF) == 0xFF) & 1) {
        code = (uint8_t)(error_code >> 8);
        if ((rel(code == 1) | rel(code == 2)) & 1) error(code);
        return code;
    }
    return SUCCESSFUL;
}

/* upper case character from console */
static uint8_t ucase(void) {
    uint8_t c;
    if (rel((c = conin()) >= 'a') & 1)
        if (rel(c < '{') & 1) return (uint8_t)(c - 0x20);
    return c;
}

/* get password and place at fcb + 16 */
static void getpasswd(void) {
    uint8_t i, c;
    crlf();
    print_buf(ERA_ARR56); /* Password ? */
retry:
    fill(fcb16, ' ', 8);
    for (i = 0; i <= 7; i++) {
    nxtchr:
        if (rel((c = ucase()) >= ' ') & 1) fcb16[i] = c;
        if (rel(c == CR) & 1) {
            crlf();
            goto exit;
        }
        if (rel(c == CTRLX) & 1) goto retry;
        if (rel(c == BKSP) & 1) {
            if (rel(i < 1) & 1) goto retry;
            fcb16[i = (uint8_t)(i - 1)] = ' ';
            goto nxtchr;
        }
        if (rel(c == 3) & 1) terminate();
    }
exit:
    c = check_con_stat();
    (void)c;
}

/* single_file's variables (PL/M locals file_err shares) */
static uint8_t sf_code;
static uint16_t sf_fcba;

static void file_err(void) {
    crlf();
    print_buf(ERA_ARR78); /* Not erased: */
    print_file(sf_fcba);
    error(sf_code);
}

/* try deleting files one at a time */
static void single_file(void) {
    uint8_t dcnt, sav_searchl;
    uint16_t sav_dcnt;
    setdma(bp_tbuff);
    dcnt = search(bp_fcb);
    while (rel(dcnt != 0xFF) & 1) {
        sf_fcba = (uint16_t)((uint8_t)(dcnt << 5) + bp_tbuff);
        sav_dcnt = rd16(uda() + uda_dcnt);
        sav_searchl = uda()[uda_searchl];
        if (rel((sf_code = delete(sf_fcba)) == 7) & 1) {
            file_err();
            getpasswd();
            sf_code = delete(sf_fcba);
        }
        if (rel(sf_code != SUCCESSFUL) & 1) file_err();
        setdma(bp_tbuff);
        /* restore dcnt and search length of 11 */
        wr16(uda() + uda_dcnt, sav_dcnt);
        uda()[uda_searchl] = sav_searchl;
        dcnt = searchn();
    }
}

void era_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"ERA     ", (const uint8_t *)&era_image, era_image_len});
    g_ds = ds;
    g = RSP_DATA(era_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;
    fcb16 = bp + bp_fcb16;

    g->ver = version();
    if ((rel((g->ver & 0xFF) != CPMVERSION) | rel(((g->ver >> 8) & 0x0F) != MPMPRODUCT)) & 1) {
        print_buf(ERA_ARR95); /* Requires MP/M 2.0 */
        mon1(0, 0);
    }
    g->parse_fn[0] = bp_tbuff + 1;
    g->parse_fn[1] = bp_fcb;
    g->user = get_user_code();
    get_uda(); /* get uda address */
    return_errors();
    if (rel(fcb[17] != ' ') & 1) {
        if (rel(fcb[17] == 'X') & 1) {
            g->xfcb = T;
        } else {
            print_buf(ERA_ARR102); /* Invalid Parameter */
            terminate();
        }
    }
    g->i = 0;
    while (rel(fcb[g->i = (uint8_t)(g->i + 1)] == '?') & 1) {
    }
    if (rel(g->i > 11) & 1)
        if ((uint8_t)~g->xfcb & 1) {
            print_buf(ERA_ARR112); /* Confirm delete all user files (Y/N)? */
            g->response = read_console();
            if ((uint8_t)~(rel(g->response == 'y') | rel(g->response == 'Y')) & 1) terminate();
        }
    parse();
    if (rel((g->code = delete(bp_fcb)) != SUCCESSFUL) & 1) {
        if (rel(g->code == 0) & 1)
            print_buf(ERA_ARR126); /* No file */
        else if (rel(g->code < 3) & 1)
            error(g->code); /* fatal errors */
        else
            single_file(); /* single file error */
    }
    terminate();
}
