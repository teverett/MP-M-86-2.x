/*
 * eraq.c -- C version of src-assem/utils/ERAQ.PLM (a transient,
 * ERAQ.CMD): erases with a query -- it collects the current user's
 * directory entries a name matches (up to 128), then names each one
 * and erases it if the answer is Y, naming the ones it couldn't (R/O,
 * open, a password) and asking for the password. [X] erases the XFCBs
 * only.
 *
 * Its data segment, below, is ERAQ.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the variables with their initial
 * values (dir$entries is the 128-name table), the string constants,
 * and the stack; eraq_main installs the image from 100h on. PL/M's
 * semantics are kept as in dir.c (ERAQ's own `true` is 1): `i = -1`
 * is 0FFh, the empty-table mark, and the DO loops leave j and k one
 * past their ends in the image.
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
    uint8_t xfcb;
    uint8_t dir_entries[128][12]; /* file (12): name, type, extent */
    uint16_t dir_entry_adr;
    uint8_t i, j, k, code, response, user, dcnt;
    uint16_t ver;
    uint8_t last_dseg_byte;
    char arr7[18], arr11[9], arr19[10], arr23[17], arr27[15], arr50[12], arr72[13], arr80[18], arr87[18],
        arr103[38], arr107[8];
    uint8_t stack[1024];
    uint8_t stack_base;
} eraq_data;

_Static_assert(offsetof(eraq_data, parse_fn) == ERAQ_PARSEFN, "parsefn");
_Static_assert(offsetof(eraq_data, xfcb) == ERAQ_XFCB, "xfcb");
_Static_assert(offsetof(eraq_data, dir_entries) == ERAQ_DIRENTRIES, "direntries");
_Static_assert(offsetof(eraq_data, dir_entry_adr) == ERAQ_DIRENTRYADR, "direntryadr");
_Static_assert(offsetof(eraq_data, i) == ERAQ_I, "i");
_Static_assert(offsetof(eraq_data, j) == ERAQ_J, "j");
_Static_assert(offsetof(eraq_data, k) == ERAQ_K, "k");
_Static_assert(offsetof(eraq_data, code) == ERAQ_CODE, "code");
_Static_assert(offsetof(eraq_data, response) == ERAQ_RESPONSE, "response");
_Static_assert(offsetof(eraq_data, user) == ERAQ_USER, "user");
_Static_assert(offsetof(eraq_data, dcnt) == ERAQ_DCNT, "dcnt");
_Static_assert(offsetof(eraq_data, ver) == ERAQ_VER, "ver");
_Static_assert(offsetof(eraq_data, last_dseg_byte) == ERAQ_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(eraq_data, arr7) == ERAQ_ARR7, "arr7");
_Static_assert(offsetof(eraq_data, arr11) == ERAQ_ARR11, "arr11");
_Static_assert(offsetof(eraq_data, arr19) == ERAQ_ARR19, "arr19");
_Static_assert(offsetof(eraq_data, arr23) == ERAQ_ARR23, "arr23");
_Static_assert(offsetof(eraq_data, arr27) == ERAQ_ARR27, "arr27");
_Static_assert(offsetof(eraq_data, arr50) == ERAQ_ARR50, "arr50");
_Static_assert(offsetof(eraq_data, arr72) == ERAQ_ARR72, "arr72");
_Static_assert(offsetof(eraq_data, arr80) == ERAQ_ARR80, "arr80");
_Static_assert(offsetof(eraq_data, arr87) == ERAQ_ARR87, "arr87");
_Static_assert(offsetof(eraq_data, arr103) == ERAQ_ARR103, "arr103");
_Static_assert(offsetof(eraq_data, arr107) == ERAQ_ARR107, "arr107");
_Static_assert(offsetof(eraq_data, stack_base) == ERAQ_STACK_BASE, "stack_base");
_Static_assert(sizeof(eraq_data) == ERAQ_DATA_LEN, "length");

#define T 1 /* ERAQ's true */
#define F 0
#define SUCCESSFUL 0xFF
#define MPMPRODUCT 0x01
#define CPMVERSION 0x30
#define CR    13
#define LF    10
#define CTRLX 0x18
#define BKSP  8

const uint16_t eraq_image_len = sizeof(eraq_data);
const eraq_data eraq_image = {
    .xfcb = 0,
    .last_dseg_byte = 0,
    .arr7 = "\r\nBDOS Bad Sector$",
    .arr11 = "\r\nDrive $",
    .arr19 = "Read Only$",
    .arr23 = "Currently Opened$",
    .arr27 = "Password Error$",
    .arr50 = "Password ? $",
    .arr72 = "Not erased, $",
    .arr80 = "Requires MP/M 2.0$",
    .arr87 = "Invalid Parameter$",
    .arr103 = "Too many directory entries for query.$",
    .arr107 = "No file$",
};

static uint16_t g_ds;
static eraq_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* the base page, and its fcb, fcb16 */
static uint8_t *fcb = 0;
static uint8_t *fcb16 = 0;

static uint8_t read_console(void) { return mon2(1, 0); }
static void printchar(uint8_t c) { mon1(2, c); }
static uint8_t conin(void) { return mon2(6, 0xFD); }
static void print_buf(uint16_t a) { mon1(9, a); }
static uint8_t check_con_stat(void) { return mon2(11, 0); }
static uint16_t version(void) { return mon3(12, 0); }
static void setdma(uint16_t a) { mon1(26, a); }
static uint8_t search_first(uint16_t a) { return mon2(17, a); }
static uint8_t search_next(void) { return mon2(18, 0); }
static uint16_t delete_file(uint16_t a) { return mon3(19, a); }
static uint8_t get_user_code(void) { return mon2(32, 0xFF); }
static void return_errors(void) { mon1(45, 0xFF); }
static void terminate(void) { mon1(143, 0); }
static void parse(void) { mon1(152, ERAQ_PARSEFN); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */
static uint8_t ror(uint8_t v, uint8_t n) { return (uint8_t)((v >> n) | (v << (8 - n))); }

static void move(uint16_t n, const uint8_t *src, uint8_t *dst) {
    while (n--) *dst++ = *src++;
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
    if (rel(code == 1) & 1) print_buf(ERAQ_ARR7);                     /* BDOS Bad Sector */
    if (rel(code == 2) & 1) print_buf(ERAQ_ARR11);                    /* Drive */
    if ((rel(code == 3) | rel(code == 2)) & 1) print_buf(ERAQ_ARR19); /* Read Only */
    if (rel(code == 5) & 1) print_buf(ERAQ_ARR23);                    /* Currently Opened */
    if (rel(code == 7) & 1) print_buf(ERAQ_ARR27);                    /* Password Error */
    if (rel(code < 3) & 1) terminate();
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
    print_buf(ERAQ_ARR50); /* Password ? */
retry:
    fill(fcb16, ' ', 8);
    for (i = 0; i <= 7; i++) {
    nxtchr:
        if (rel((c = ucase()) >= ' ') & 1) fcb16[i] = c;
        if (rel(c == CR) & 1) goto exit;
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

/* error on deleting a file */
static void file_err(uint8_t code) {
    crlf();
    print_buf(ERAQ_ARR72); /* Not erased, */
    error(code);
    crlf();
}

void eraq_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"ERAQ    ", (const uint8_t *)&eraq_image, eraq_image_len});
    g_ds = ds;
    g = RSP_DATA(eraq_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;
    fcb16 = bp + bp_fcb16;

    g->ver = version();
    if ((rel((g->ver & 0xFF) != CPMVERSION) | rel(((g->ver >> 8) & 0x0F) != MPMPRODUCT)) & 1) {
        print_buf(ERAQ_ARR80); /* Requires MP/M 2.0 */
        mon1(0, 0);
    }

    if (rel(fcb[17] != ' ') & 1) {
        if (rel(fcb[17] == 'X') & 1) {
            g->xfcb = T;
        } else {
            print_buf(ERAQ_ARR87); /* Invalid Parameter */
            terminate();
        }
    }

    if (rel(bp[bp_len0] != 0) & 1) { /* a password: parse it to fcb+16 */
        g->parse_fn[0] = bp_tbuff + 1;
        g->parse_fn[1] = bp_fcb;
        parse();
    }
    if (rel(fcb[0] == 0) & 1) fcb[0] = (uint8_t)(mon2(25, 0) + 1);
    g->i = (uint8_t)-1;
    g->user = get_user_code();
    return_errors();
    /* collect the user's matching entries: dcnt's 32-byte entry in tbuff */
    g->dcnt = search_first(bp_fcb);
    while (rel(g->dcnt != 0xFF) & 1) {
        g->dir_entry_adr = (uint16_t)(bp_tbuff + (ror(g->dcnt, 3) & 0x60));
        if (rel(bp[g->dir_entry_adr] == g->user) & 1) {
            if (rel((g->i = (uint8_t)(g->i + 1)) == 128) & 1) {
                print_buf(ERAQ_ARR103); /* Too many directory entries for query. */
                terminate();
            }
            move(12, bp + (uint16_t)(g->dir_entry_adr + 1), g->dir_entries[g->i]);
        }
        g->dcnt = search_next();
    }
    if (rel(g->i == (uint8_t)-1) & 1) {
        print_buf(ERAQ_ARR107); /* No file */
    } else {
        for (g->j = 0; rel(g->j <= g->i) & 1; g->j++) {
            printchar((uint8_t)('A' + fcb[0] - 1));
            printchar(':');
            printchar(' ');
            for (g->k = 0; rel(g->k <= 10) & 1; g->k++) {
                if (rel(g->k == 8) & 1) printchar('.');
                printchar(g->dir_entries[g->j][g->k]); /* attribute bits and all */
            }
            printchar(' ');
            printchar('?');
            g->response = read_console();
            printchar(0x0D);
            printchar(0x0A);
            if ((rel(g->response == 'y') | rel(g->response == 'Y')) & 1) {
                move(12, g->dir_entries[g->j], fcb + 1);
                if (rel((g->code = delete(bp_fcb)) != SUCCESSFUL) & 1) {
                    if (rel(g->code < 3) & 1) {
                        error(g->code); /* fatal errors */
                    } else if (rel(g->code == 7) & 1) {
                        file_err(g->code);
                        getpasswd();
                        g->code = delete(bp_fcb);
                    }
                    if (rel(g->code != SUCCESSFUL) & 1) file_err(g->code);
                    crlf();
                }
            }
        }
    }
    terminate();
}
