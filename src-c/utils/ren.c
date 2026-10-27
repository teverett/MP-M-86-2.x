/*
 * ren.c -- C version of src-assem/utils/REN.PLM (a transient, REN.CMD):
 * REN new=old renames a file, or with matching ?s in both names (a
 * wildcard) every file the old name matches, one at a time -- offering
 * to delete a new name that exists and asking for passwords.
 *
 * Its data segment, below, is REN.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (the DATA messages among them), the string constants, and the
 * stack; ren_main installs the image from 100h on. PL/M's semantics are
 * kept as in dir.c (REN's own `true` is 0FFh). The one-at-a-time rename
 * keeps search next's place across its renames by saving and restoring
 * the UDA's dcnt, searcha and searchl, as REN.PLM does.
 *
 * Three fcbs: new_fcb, the new name (the base page's fcb; its ?s are
 * filled in from each old name); cur_fcb, the old name as given; and
 * old_fcb, a directory entry cur_fcb matches, the one renamed.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb   0x5C
#define bp_tbuff 0x80

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t parse_fn[2];    /* buff$adr, fcb$adr */
    uint16_t pd_pointer[2];  /* offset, segment */
    uint16_t uda_pointer[2];
    char failed[16], read_only[18], bad_wildcard[17];
    uint8_t passwd[8];
    uint16_t new_fcb_adr;
    uint8_t cur_fcb[33];
    uint16_t ver;
    uint8_t last_dseg_byte;
    char arr7[23], arr11[18], arr21[17], arr25[15], arr29[15], arr51[12], arr97[16], arr107[13], arr115[18];
    uint8_t stack[1024];
    uint8_t stack_base;
} ren_data;

_Static_assert(offsetof(ren_data, parse_fn) == REN_PARSEFN, "parsefn");
_Static_assert(offsetof(ren_data, pd_pointer) == REN_PDPOINTER, "pdpointer");
_Static_assert(offsetof(ren_data, uda_pointer) == REN_UDAPOINTER, "udapointer");
_Static_assert(offsetof(ren_data, failed) == REN_FAILED, "failed");
_Static_assert(offsetof(ren_data, read_only) == REN_READONLY, "readonly");
_Static_assert(offsetof(ren_data, bad_wildcard) == REN_BADWILDCARD, "badwildcard");
_Static_assert(offsetof(ren_data, passwd) == REN_PASSWD, "passwd");
_Static_assert(offsetof(ren_data, new_fcb_adr) == REN_NEWFCBADR, "newfcbadr");
_Static_assert(offsetof(ren_data, cur_fcb) == REN_CURFCB, "curfcb");
_Static_assert(offsetof(ren_data, ver) == REN_VER, "ver");
_Static_assert(offsetof(ren_data, last_dseg_byte) == REN_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(ren_data, arr7) == REN_ARR7, "arr7");
_Static_assert(offsetof(ren_data, arr11) == REN_ARR11, "arr11");
_Static_assert(offsetof(ren_data, arr21) == REN_ARR21, "arr21");
_Static_assert(offsetof(ren_data, arr25) == REN_ARR25, "arr25");
_Static_assert(offsetof(ren_data, arr29) == REN_ARR29, "arr29");
_Static_assert(offsetof(ren_data, arr51) == REN_ARR51, "arr51");
_Static_assert(offsetof(ren_data, arr97) == REN_ARR97, "arr97");
_Static_assert(offsetof(ren_data, arr107) == REN_ARR107, "arr107");
_Static_assert(offsetof(ren_data, arr115) == REN_ARR115, "arr115");
_Static_assert(offsetof(ren_data, stack_base) == REN_STACK_BASE, "stack_base");
_Static_assert(sizeof(ren_data) == REN_DATA_LEN, "length");

#define T 0xFF /* REN's true */
#define F 0
#define SUCCESSFUL 0xFF
#define MPMPRODUCT 0x01
#define CPMVERSION 0x30
#define CR    13
#define LF    10
#define CTRLC 3
#define CTRLX 0x18
#define BKSP  8

/* the UDA's fields REN uses (uda.lit) */
#define uda_searchl 0x07
#define uda_searcha 0x08
#define uda_dcnt    0x0C
#define pd_uda      0x10 /* proces.lit */

const uint16_t ren_image_len = sizeof(ren_data);
const ren_data ren_image = {
    .failed = "\r\nNot renamed: $",
    .read_only = "\r\nDrive Read Only$",
    .bad_wildcard = "Invalid Wildcard$",
    .last_dseg_byte = 0,
    .arr7 = "No such file to rename$",
    .arr11 = "\r\nBDOS Bad Sector$",
    .arr21 = "Currently Opened$",
    .arr25 = "Password Error$",
    .arr29 = "already exists$",
    .arr51 = "Password ? $",
    .arr97 = ", delete (Y/N)?$",
    .arr107 = "Invalid File$",
    .arr115 = "Requires MP/M 2.0$",
};

static uint16_t g_ds;
static ren_data *g;

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t *bp = 0; /* the base page, and its fcb */
static uint8_t *fcb = 0;

static uint8_t read_console(void) { return mon2(1, 0); }
static uint8_t conin(void) { return mon2(6, 0xFD); }
static void printchar(uint8_t c) { mon1(2, c); }
static uint8_t check_con_stat(void) { return mon2(11, 0); }
static void print_buf(uint16_t a) { mon1(9, a); }
static uint16_t version(void) { return mon3(12, 0); }
static uint8_t search_first(uint16_t a) { return mon2(17, a); }
static uint8_t search_next(void) { return mon2(18, 0); }
static void delete_file(uint16_t a) { mon1(19, a); }
static uint16_t rename_file(uint16_t a) { return mon3(23, a); }
static void setdma(uint16_t a) { mon1(26, a); }
static void return_errors(uint8_t mode) { mon1(45, mode); }
static void terminate(void) { mon1(143, 0); }
static uint16_t parse(void) { return mon3(152, REN_PARSEFN); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* PL/M's move(count, source, destination), on DS addresses */
static void move(uint16_t n, uint16_t src, uint16_t dst) {
    while (n--) bp[dst++] = bp[src++];
}

static uint8_t *new_fcb(void) { return bp + g->new_fcb_adr; }

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
    if (rel(code == 0) & 1) {
        print_buf(REN_ARR7); /* No such file to rename */
        terminate();
    }
    if (rel(code == 1) & 1) {
        print_buf(REN_ARR11); /* BDOS Bad Sector */
        terminate();
    }
    if (rel(code == 2) & 1) {
        print_buf(REN_READONLY);
        terminate();
    }
    if (rel(code == 3) & 1) print_buf(REN_READONLY + 8); /* Read Only */
    if (rel(code == 5) & 1) print_buf(REN_ARR21);       /* Currently Opened */
    if (rel(code == 7) & 1) print_buf(REN_ARR25);       /* Password Error */
    if (rel(code == 8) & 1) print_buf(REN_ARR29);       /* already exists */
    if (rel(code == 9) & 1) {
        print_buf(REN_BADWILDCARD);
        terminate();
    }
}

/* print file name */
static void print_file(uint16_t fcbp) {
    for (uint8_t k = 1; k <= 11; k++) {
        if (rel(k == 9) & 1) printchar('.');
        printchar(bp[(uint16_t)(fcbp + k)] & 0x7F);
    }
}

/* try to rename fcb at old_fcb_adr to the name at new_fcb_adr; return
 * error code if unsuccessful */
static uint8_t rename(uint16_t old_fcb_adr) {
    uint16_t error_code;
    uint8_t code;
    move(16, g->new_fcb_adr, (uint16_t)(old_fcb_adr + 16));
    setdma(REN_PASSWD);  /* password */
    return_errors(0xFF); /* return bdos errors */
    error_code = rename_file(old_fcb_adr);
    return_errors(0);    /* normal error mode */
    if (rel((error_code & 0xFF) == 0xFF) & 1) {
        code = (uint8_t)(error_code >> 8);
        if (rel(code < 3) & 1) error(code);
        return code;
    }
    return SUCCESSFUL;
}

/* upper case character */
static uint8_t ucase(uint8_t c) {
    if (rel(c >= 'a') & 1)
        if (rel(c < '{') & 1) return (uint8_t)(c - 0x20);
    return c;
}

/* get password and place at passwd */
static void getpasswd(void) {
    uint8_t i, c;
    crlf();
    print_buf(REN_ARR51); /* Password ? */
retry:
    fill(g->passwd, ' ', 8);
    for (i = 0; i <= 7; i++) {
    nxtchr:
        if (rel((c = ucase(conin())) >= ' ') & 1) g->passwd[i] = c;
        if (rel(c == CR) & 1) {
            crlf();
            goto exit;
        }
        if (rel(c == CTRLX) & 1) goto retry;
        if (rel(c == BKSP) & 1) {
            if (rel(i < 1) & 1) goto retry;
            g->passwd[i = (uint8_t)(i - 1)] = ' ';
            goto nxtchr;
        }
        if (rel(c == CTRLC) & 1) terminate();
    }
exit:
    c = check_con_stat();
    (void)c;
}

/* check for wildcard in rename command */
static uint8_t wildcard(void) {
    uint8_t wild = F;
    for (uint8_t i = 1; i <= 11; i++) {
        if (rel(g->cur_fcb[i] == '?') & 1) {
            if (rel(new_fcb()[i] != '?') & 1) {
                print_buf(REN_FAILED);
                print_buf(REN_BADWILDCARD);
                terminate();
            } else {
                wild = T;
            }
        }
    }
    return wild;
}

/* set up new name for rename function */
static void set_new_fcb(uint16_t old_fcb_adr) {
    uint8_t *old_fcb = bp + old_fcb_adr;
    old_fcb[0] = g->cur_fcb[0]; /* set up drive */
    for (uint8_t i = 1; i <= 11; i++)
        if (rel(g->cur_fcb[i] == '?') & 1) new_fcb()[i] = old_fcb[i];
}

/* single_file's code (a PL/M local file_err shares) */
static uint8_t sf_code;

static void file_err(uint16_t fcba) {
    print_buf(REN_FAILED);
    print_file(fcba);
    printchar(' ');
    error(sf_code);
}

/* try renaming files one at a time */
static void single_file(void) {
    uint8_t dcnt, savsearchl;
    uint16_t old_fcb_adr, savdcnt, savsearcha;
    setdma(bp_tbuff);
    if (rel((dcnt = search_first(REN_CURFCB)) == 0xFF) & 1) error(0);
    while (rel(dcnt != 0xFF) & 1) {
        old_fcb_adr = (uint16_t)((uint8_t)(dcnt << 5) + bp_tbuff);
        savdcnt = rd16(uda() + uda_dcnt);
        savsearcha = rd16(uda() + uda_searcha);
        savsearchl = uda()[uda_searchl];
        set_new_fcb(old_fcb_adr);
        if (rel((sf_code = rename(old_fcb_adr)) == 8) & 1) {
            file_err(g->new_fcb_adr);
            print_buf(REN_ARR97); /* , delete (Y/N)? */
            if (rel(ucase(read_console()) == 'Y') & 1) {
                delete_file(g->new_fcb_adr);
                sf_code = rename(old_fcb_adr);
            } else {
                goto next;
            }
        }
        if (rel(sf_code == 7) & 1) {
            file_err(old_fcb_adr);
            getpasswd();
            sf_code = rename(old_fcb_adr);
        }
        if (rel(sf_code != SUCCESSFUL) & 1) {
            file_err(old_fcb_adr);
        } else {
            crlf();
            print_file(g->new_fcb_adr);
            printchar('=');
            print_file(old_fcb_adr);
        }
    next:
        setdma(bp_tbuff);
        wr16(uda() + uda_dcnt, savdcnt);
        wr16(uda() + uda_searcha, savsearcha);
        uda()[uda_searchl] = savsearchl;
        dcnt = search_next();
    }
}

/* invalid rename command */
static void bad_entry(void) {
    print_buf(REN_FAILED);
    print_buf(REN_ARR107); /* Invalid File */
    terminate();
}

void ren_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"REN     ", (const uint8_t *)&ren_image, ren_image_len});
    g_ds = ds;
    g = RSP_DATA(ren_data, ds);
    bp = g->base;
    fcb = bp + bp_fcb;

    g->ver = version();
    if ((rel((g->ver & 0xFF) != CPMVERSION) | rel((g->ver >> 8) < MPMPRODUCT)) & 1) {
        print_buf(REN_ARR115); /* Requires MP/M 2.0 */
    } else {
        get_uda();
        g->parse_fn[0] = bp_tbuff + 1;
        g->new_fcb_adr = g->parse_fn[1] = bp_fcb;
        if (rel((g->parse_fn[1] = parse()) != 0xFFFF) & 1) { /* old file */
            g->parse_fn[0] = (uint16_t)(g->parse_fn[1] + 1);  /* skip delim */
            g->parse_fn[1] = REN_CURFCB;
            g->parse_fn[1] = parse();                         /* new file */
            move(8, REN_CURFCB + 16, REN_PASSWD);             /* password */
        }
        if (rel(g->parse_fn[1] == 0xFFFF) & 1) bad_entry();
        if (rel(fcb[0] != 0) & 1) {
            if (rel(g->cur_fcb[0] != 0) & 1) {
                if (rel(fcb[0] != g->cur_fcb[0]) & 1) bad_entry();
            } else {
                g->cur_fcb[0] = new_fcb()[0]; /* set drive */
            }
        }
        if (wildcard() & 1)
            single_file();
        else if (rel(rename(REN_CURFCB) != SUCCESSFUL) & 1)
            single_file();
    }
    mon1(0, 0);
}
