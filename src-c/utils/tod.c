/*
 * tod.c -- C version of src-assem/utils/TOD.PLM (a transient, TOD.CMD):
 * shows the time and date SYSDAT's clock holds (its tod at 7Eh: the
 * day number since 1/1/78, and BCD hours, minutes and seconds) as
 * "Day mm/dd/yy hh:mm:ss"; given a time and date ("TOD 10/26/26
 * 12:34:56"), sets the clock to it at a key; with P, shows the time over
 * and over until a key.
 *
 * Its data segment, below, is TOD.PLM's as tools/plmc.py lays it out
 * (utils_syms.h): the base page, the PL/M variables with their initial
 * values (the month tables, the day names), the string constants, and
 * the stack; tod_main installs the image from 100h on. PL/M's semantics
 * are kept as in dir.c: bytes wrap, a relation is 0FFh or 0, IF tests
 * bit 0, AND/OR evaluate both sides; the variables are the PL/M
 * globals, the procedures' own are C locals. tod$ASCII works on the
 * structure at tod$adr and the string at string$adr, as the PL/M does;
 * the clock is reached through tod$pointer, a far pointer into SYSDAT,
 * and copied with PL/M-86's movb. scan$numeric's CARRY tests are the
 * byte additions' carries, as PL/M-86 compiles them (see the note
 * there).
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "utils_syms.h"

/* the base page (the PL/M interface stub's names) */
#define bp_fcb  0x5C
#define bp_buff 0x80

/* the tod structure (tod$adr's, and lcltod) */
#define tod_opcode 0
#define tod_date   1
#define tod_hrs    3
#define tod_min    4
#define tod_sec    5
#define tod_ascii_ 6

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    uint16_t tod_adr, string_adr;
    uint8_t index, chr;
    uint8_t month_size[12];
    uint16_t month_days[12];
    uint16_t word_value;
    uint8_t month, day, year, hrs, min, sec;
    uint8_t week_day;
    char day_list[28];
    uint8_t leap_bias;
    uint8_t date_test;
    uint16_t test_value;
    uint16_t tod_pointer[2]; /* offset, segment */
    uint8_t lcltod[27];      /* opcode, date, hrs, min, sec, ASCII(21) */
    uint8_t i;
    uint16_t ret, vers;
    uint8_t last_dseg_byte;
    char arr1[33], arr111[23], arr119[23];
    uint8_t stack[1024];
    uint8_t stack_base;
} tod_data;

_Static_assert(offsetof(tod_data, tod_adr) == TOD_TODADR, "todadr");
_Static_assert(offsetof(tod_data, string_adr) == TOD_STRINGADR, "stringadr");
_Static_assert(offsetof(tod_data, index) == TOD_INDEX, "index");
_Static_assert(offsetof(tod_data, chr) == TOD_CHR, "chr");
_Static_assert(offsetof(tod_data, month_size) == TOD_MONTHSIZE, "monthsize");
_Static_assert(offsetof(tod_data, month_days) == TOD_MONTHDAYS, "monthdays");
_Static_assert(offsetof(tod_data, word_value) == TOD_WORDVALUE, "wordvalue");
_Static_assert(offsetof(tod_data, month) == TOD_MONTH, "month");
_Static_assert(offsetof(tod_data, day) == TOD_DAY, "day");
_Static_assert(offsetof(tod_data, year) == TOD_YEAR, "year");
_Static_assert(offsetof(tod_data, hrs) == TOD_HRS, "hrs");
_Static_assert(offsetof(tod_data, min) == TOD_MIN, "min");
_Static_assert(offsetof(tod_data, sec) == TOD_SEC, "sec");
_Static_assert(offsetof(tod_data, week_day) == TOD_WEEKDAY, "weekday");
_Static_assert(offsetof(tod_data, day_list) == TOD_DAYLIST, "daylist");
_Static_assert(offsetof(tod_data, leap_bias) == TOD_LEAPBIAS, "leapbias");
_Static_assert(offsetof(tod_data, date_test) == TOD_DATETEST, "datetest");
_Static_assert(offsetof(tod_data, test_value) == TOD_TESTVALUE, "testvalue");
_Static_assert(offsetof(tod_data, tod_pointer) == TOD_TODPOINTER, "todpointer");
_Static_assert(offsetof(tod_data, lcltod) == TOD_LCLTOD, "lcltod");
_Static_assert(offsetof(tod_data, i) == TOD_I, "i");
_Static_assert(offsetof(tod_data, ret) == TOD_RET, "ret");
_Static_assert(offsetof(tod_data, vers) == TOD_VERS, "vers");
_Static_assert(offsetof(tod_data, last_dseg_byte) == TOD_LASTDSEGBYTE, "lastdsegbyte");
_Static_assert(offsetof(tod_data, arr1) == TOD_ARR1, "arr1");
_Static_assert(offsetof(tod_data, arr111) == TOD_ARR111, "arr111");
_Static_assert(offsetof(tod_data, arr119) == TOD_ARR119, "arr119");
_Static_assert(offsetof(tod_data, stack_base) == TOD_STACK_BASE, "stack_base");
_Static_assert(sizeof(tod_data) == TOD_DATA_LEN, "length");

#define mpmproduct 0x11
#define cpmversion 0x30
#define base_year 78 /* base year for computations */
#define base_day  0  /* starting day for base$year 0..6 */
#define tod_sd_offset 0x7E /* offset of TOD structure in MP/M-86 */

const uint16_t tod_image_len = sizeof(tod_data);
const tod_data tod_image = {
    /* jan feb mar apr may jun jul aug sep oct nov dec */
    .month_size = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    .month_days = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    .day_list = "Sun$Mon$Tue$Wed$Thu$Fri$Sat$",
    .last_dseg_byte = 0,
    .arr1 = "Illegal time/date specification.$",
    .arr111 = "\r\nRequires MP/M-86 2.0$",
    .arr119 = "Strike key to set time$",
};

static uint16_t g_ds;
static tod_data *g;
static uint8_t *bp = 0; /* the base page: DS:0 */

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

static uint8_t read_console(void) { return mon2(1, 0); }
static void write_console(uint8_t c) { mon1(2, c); }
static void print_buffer(uint16_t a) { mon1(9, a); }
static uint8_t check_console_status(void) { return mon2(11, 0); }
static uint16_t version(void) { return mon3(12, 0); }
static void terminate(void) { mon1(0, 0); }

/* tod$pointer = get$sysdat (mon4: ES:BX) */
static void get_sysdat(void) {
    mpm_regs r = rt_mpm(g_ds, 154, 0);
    g->tod_pointer[0] = r.bx;
    g->tod_pointer[1] = r.es;
}

static void crlf(void) {
    write_console(0x0D);
    write_console(0x0A);
}

static void error(void) {
    print_buffer(TOD_ARR1); /* Illegal time/date specification. */
    terminate();
}

/* PL/M-86's movb(source, destination, count), pointers as (offset, segment) */
static void movb(uint16_t src_off, uint16_t src_seg, uint16_t dst_off, uint16_t dst_seg, uint16_t n) {
    for (uint16_t k = 0; k < n; k++)
        *far_ptr(rt_phys(), dst_seg, (uint16_t)(dst_off + k)) = *far_ptr(rt_phys(), src_seg, (uint16_t)(src_off + k));
}

/*****************************************************

          Time & Date ASCII Conversion Code

 *****************************************************/

/* tod, based tod$adr; string, based string$adr */
static uint8_t *tod(uint16_t field) { return bp + (uint16_t)(g->tod_adr + field); }
static uint8_t *string(uint8_t k) { return bp + (uint16_t)(g->string_adr + k); }

static void emitchar(uint8_t c) { *string(g->index = (uint8_t)(g->index + 1)) = c; }

static void emitn(uint16_t a) {
    while (rel(bp[a] != '$') & 1) {
        *string(g->index = (uint8_t)(g->index + 1)) = bp[a];
        a = (uint16_t)(a + 1);
    }
}

static void emit_bcd(uint8_t b) { emitchar((uint8_t)('0' + b)); }

static void emit_bcd_pair(uint8_t b) {
    emit_bcd(b >> 4);
    emit_bcd(b & 0x0F);
}

static void emit_colon(uint8_t b) {
    emit_bcd_pair(b);
    emitchar(':');
}

static void emit_bin_pair(uint8_t b) {
    emit_bcd(b / 10);
    emit_bcd(b % 10);
}

static void emit_slant(uint8_t b) {
    emit_bin_pair(b);
    emitchar('/');
}

/* get next command byte */
static void gnc(void) {
    if (rel(g->chr == 0) & 1) return;
    if (rel(g->index == 20) & 1) {
        g->chr = 0;
        return;
    }
    g->chr = *string(g->index = (uint8_t)(g->index + 1));
}

static void deblank(void) {
    while (rel(g->chr == ' ') & 1) gnc();
}

/* test for numeric */
static uint8_t numeric(void) { return rel((uint8_t)(g->chr - '0') < 10); }

/* CARRY is the carry out of the byte addition just before it, as
 * PL/M-86 computes `b = shl(b,3) + shl(b,1)` and `b = b + (chr - '0')`
 * (ADD AL,..): a number past 255 is an error. (tools/plmc.py adds
 * bytes in 16 bits, so its build never sets it: there 260 is 4.) */
static uint8_t scan_numeric(uint8_t lb, uint8_t ub) {
    uint8_t b;
    uint16_t sum;
    b = 0;
    deblank();
    if ((uint8_t)~numeric() & 1) error();
    while (numeric() & 1) {
        if (rel((b & 0xE0) != 0) & 1) error();
        sum = (uint16_t)((uint8_t)(b << 3) + (uint8_t)(b << 1)); /* b = b * 10 */
        b = (uint8_t)sum;
        if (sum > 0xFF) error(); /* carry */
        sum = (uint16_t)(b + (uint8_t)(g->chr - '0'));
        b = (uint8_t)sum;
        if (sum > 0xFF) error(); /* carry */
        gnc();
    }
    if ((rel(b < lb) | rel(b > ub)) & 1) error();
    return b;
}

static uint8_t scan_delimiter(uint8_t d, uint8_t lb, uint8_t ub) {
    deblank();
    if (rel(g->chr != d) & 1) error();
    gnc();
    return scan_numeric(lb, ub);
}

/* compute days accumulated by leap years */
static uint8_t leap_days(uint8_t y, uint8_t m) {
    uint8_t yp;
    yp = y >> 2; /* yp = y/4 */
    if ((rel((y & 3) == 0) & rel(g->month_days[m] < 59)) & 1)
        /* y not 00, y mod 4 = 0, before march, so not leap yr */
        return (uint8_t)(yp - 1);
    /* otherwise, yp is the number of accumulated leap days */
    return yp;
}

static uint8_t bcd(uint8_t val) { return (uint8_t)(((val / 10) << 4) + val % 10); }

static void set_date_time(void) {
    uint8_t i, leap_flag; /* temporaries */
    g->month = (uint8_t)(scan_numeric(1, 12) - 1);
    /* may be feb 29 */
    if ((leap_flag = rel(g->month == 1)) & 1)
        i = 29;
    else
        i = g->month_size[g->month];
    g->day = scan_delimiter('/', 1, i);
    g->year = scan_delimiter('/', base_year, 99);
    /* ensure that feb 29 is in a leap year */
    if ((leap_flag & rel(g->day == 29) & rel((g->year & 3) != 0)) & 1)
        /* feb 29 of non-leap year */ error();
    /* compute total days */
    wr16(tod(tod_date), (uint16_t)(g->month_days[g->month] + 365 * (uint8_t)(g->year - base_year) + g->day -
                                   leap_days(base_year, 0) + leap_days(g->year, g->month)));

    *tod(tod_hrs) = bcd(scan_numeric(0, 23));
    *tod(tod_min) = bcd(scan_delimiter(':', 0, 59));
    if (rel(*tod(tod_opcode) == 2) & 1) {
        /* date, hours and minutes only */
        if (rel(g->chr == ':') & 1) i = scan_delimiter(':', 0, 59);
        *tod(tod_sec) = 0;
    } else {
        /* include seconds */
        *tod(tod_sec) = bcd(scan_delimiter(':', 0, 59));
    }
    (void)i;
}

/* compute year from number of days in word$value */
static void compute_year(void) {
    uint16_t year_length;
    g->year = base_year;
    for (;;) {
        year_length = 365;
        if (rel((g->year & 3) == 0) & 1) year_length = 366; /* leap year */
        if (rel(g->word_value <= year_length) & 1) return;
        g->word_value = (uint16_t)(g->word_value - year_length);
        g->year = (uint8_t)(g->year + 1);
    }
}

static void compute_month(void) {
    g->month = 12;
    while (rel(g->month > 0) & 1) {
        if (rel((g->month = (uint8_t)(g->month - 1)) < 2) & 1) g->leap_bias = 0; /* jan or feb */
        if (rel((uint16_t)(g->month_days[g->month] + g->leap_bias) < g->word_value) & 1) return;
    }
}

/* get date and time */
static void get_date_time(void) {
    g->hrs = *tod(tod_hrs);
    g->min = *tod(tod_min);
    g->sec = *tod(tod_sec);
    g->word_value = rd16(tod(tod_date));
    /* word$value contains total number of days */
    g->week_day = (uint8_t)((uint16_t)(g->word_value + base_day - 1) % 7);
    compute_year();
    /* year has been set, word$value is remainder */
    g->leap_bias = 0;
    if ((rel((g->year & 3) == 0) & rel(g->word_value > 59)) & 1)
        /* after feb 29 on leap year */ g->leap_bias = 1;
    compute_month();
    g->day = (uint8_t)(g->word_value - (uint16_t)(g->month_days[g->month] + g->leap_bias));
    g->month = (uint8_t)(g->month + 1);
}

static void emit_date_time(void) {
    emitn((uint16_t)(TOD_DAYLIST + (uint8_t)(g->week_day << 2)));
    emitchar(' ');
    emit_slant(g->month);
    emit_slant(g->day);
    emit_bin_pair(g->year);
    emitchar(' ');
    emit_colon(g->hrs);
    emit_colon(g->min);
    emit_bcd_pair(g->sec);
}

static void tod_ascii(uint16_t parameter) {
    uint16_t ret;
    ret = 0;
    g->tod_adr = parameter;
    g->string_adr = (uint16_t)(g->tod_adr + tod_ascii_);
    if (rel(*tod(tod_opcode) == 0) & 1) {
        get_date_time();
        g->index = 0xFF; /* -1 */
        emit_date_time();
    } else {
        if ((rel(*tod(tod_opcode) == 1) | rel(*tod(tod_opcode) == 2)) & 1) {
            g->chr = *string(g->index = 0);
            set_date_time();
            ret = (uint16_t)(g->string_adr + g->index); /* .string(index) */
        } else {
            error();
        }
    }
    (void)ret;
}

/********************************************************
 ********************************************************/

static void display_tod(void) {
    g->lcltod[tod_opcode] = 0; /* read tod */
    movb(g->tod_pointer[0], g->tod_pointer[1], TOD_LCLTOD + tod_date, g_ds, 5);
    tod_ascii(TOD_LCLTOD);
    write_console(0x0D);
    for (g->i = 0; g->i <= 20; g->i++) write_console(g->lcltod[tod_ascii_ + g->i]);
}

/*
  Main Program
*/
void tod_main(uint16_t ds) {
    rsp_install_transient(ds, &(rsp_image){"TOD     ", (const uint8_t *)&tod_image, tod_image_len});
    g_ds = ds;
    g = RSP_DATA(tod_data, ds);
    bp = g->base;
    uint8_t *fcb = bp + bp_fcb;

    g->vers = version();
    if ((rel((uint8_t)g->vers != cpmversion) | rel((uint8_t)(g->vers >> 8) != mpmproduct)) & 1) {
        print_buffer(TOD_ARR111); /* Requires MP/M-86 2.0 */
        mon1(0, 0);
    }

    get_sysdat();
    g->tod_pointer[0] = tod_sd_offset;
    if ((rel(fcb[1] != ' ') & rel(fcb[1] != 'P')) & 1) {
        for (uint16_t k = 0; k < 21; k++) g->lcltod[tod_ascii_ + k] = bp[bp_buff + 1 + k]; /* move(21,...) */
        g->lcltod[tod_opcode] = 1;
        tod_ascii(TOD_LCLTOD);
        print_buffer(TOD_ARR119); /* Strike key to set time */
        g->ret = read_console();
        movb(TOD_LCLTOD + tod_date, g_ds, g->tod_pointer[0], g->tod_pointer[1], 5); /* use pl/m-86 move */
        crlf();
    }
    while (rel(fcb[1] == 'P') & 1) {
        display_tod();
        if (check_console_status() & 1) {
            g->ret = read_console();
            fcb[1] = 0;
        }
    }
    display_tod();
    terminate();
}
