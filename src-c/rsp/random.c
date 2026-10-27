/*
 * random.c -- C version of src-assem/rsp/RANDOM.A86, DRI's sample random
 * access program (a transient, RANDOM.CMD). It opens (or makes) the
 * file named on its command line, then takes commands: `nW` writes a
 * typed line to record n, `nZ` does the same with write random zero
 * fill, `nR` prints record n, Q closes the file and quits.
 *
 * Its data group, below, is RANDOM.A86's DSEG byte for byte (rsp.h);
 * the base page part of it (the FCB, the record number, the DMA
 * buffer) is filled in by LOAD and the CLI, so random_main installs the
 * image from 100h on.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "rsp_syms.h"

#define coninp  1
#define conout  2
#define pstring 9
#define rstring 10
#define version 12
#define openf   15
#define closef  16
#define makef   22
#define readr   33
#define writer  34
#define writerz 40

typedef struct RSP_PACKED {
    uint8_t base[0x5C];
    uint8_t fcb[33];
    uint16_t ranrec;
    uint8_t ranovf;
    uint8_t buff[128];
    char badver[31], nospace[19], datmsg[12], errmsg[18], prompt[15];
    uint8_t conbuf, consiz;
    uint8_t conlin[32];
    uint8_t stack_area[31];
    uint8_t stack;
    uint8_t end; /* "end byte for GENCMD" */
} random_data;

_Static_assert(offsetof(random_data, fcb) == RANDOM_FCB, "fcb");
_Static_assert(offsetof(random_data, ranrec) == RANDOM_RANREC, "ranrec");
_Static_assert(offsetof(random_data, ranovf) == RANDOM_RANOVF, "ranovf");
_Static_assert(offsetof(random_data, buff) == RANDOM_BUFF, "buff");
_Static_assert(offsetof(random_data, badver) == RANDOM_BADVER, "badver");
_Static_assert(offsetof(random_data, nospace) == RANDOM_NOSPACE, "nospace");
_Static_assert(offsetof(random_data, datmsg) == RANDOM_DATMSG, "datmsg");
_Static_assert(offsetof(random_data, errmsg) == RANDOM_ERRMSG, "errmsg");
_Static_assert(offsetof(random_data, prompt) == RANDOM_PROMPT, "prompt");
_Static_assert(offsetof(random_data, conbuf) == RANDOM_CONBUF, "conbuf");
_Static_assert(offsetof(random_data, consiz) == RANDOM_CONSIZ, "consiz");
_Static_assert(offsetof(random_data, conlin) == RANDOM_CONLIN, "conlin");
_Static_assert(offsetof(random_data, stack) == RANDOM_STACK, "stack");

const uint16_t random_image_len = sizeof(random_data);
const random_data random_image = {
    .badver = "sorry, you need cp/m version 2$",
    .nospace = "no directory space$",
    .datmsg = "type data: $",
    .errmsg = "error, try again.$",
    .prompt = "next command? $",
    .conbuf = RANDOM_STACK - RANDOM_CONSIZ - 31, /* conlen: offset $ - offset consiz */
};

static mpm_regs bdos(uint16_t ds, uint8_t cl, uint16_t dx) { return rt_mpm(ds, cl, dx); }
static uint8_t getchr(uint16_t ds) { return (uint8_t)bdos(ds, coninp, 0).bx; }
static void putchr(uint16_t ds, uint8_t al) { (void)bdos(ds, conout, al); }

static void crlf(uint16_t ds) {
    putchr(ds, cr);
    putchr(ds, lf);
}

static void print(uint16_t ds, uint16_t dx) {
    crlf(ds);
    (void)bdos(ds, pstring, dx);
}

/* readcom: read the next command line; returns the command letter
 * (AL), the record number in *dx and its high byte in *ch. 0 for "too
 * large" (the .A86 jumps straight to error from inside readcom). */
static int readcom(uint16_t ds, random_data *d, uint8_t *al, uint16_t *dx, uint8_t *ch) {
    print(ds, RANDOM_PROMPT);
    (void)bdos(ds, rstring, RANDOM_CONBUF);
    uint16_t ax = 0;
    uint16_t bx = RANDOM_CONLIN;
    uint8_t *b = (uint8_t *)d;
    *ch = 0;
    for (;;) {
        uint8_t dl = b[bx++];
        if (dl == 0) { /* end of the command: AL = the number's low byte, DX = 0 */
            *al = (uint8_t)ax;
            *dx = 0;
            return 1;
        }
        dl = (uint8_t)(dl - '0');
        if (dl >= 10) break;
        if (*ch != 0) return 0; /* number too large */
        uint32_t prod = (uint32_t)ax * 10;
        *ch = (uint8_t)(prod >> 16);
        ax = (uint16_t)prod;
        uint32_t sum = (uint32_t)ax + dl;
        ax = (uint16_t)sum;
        *ch = (uint8_t)(*ch + (sum >> 16));
    }
    /* endrd: the value in DX, the command letter in AL, upper case */
    *dx = ax;
    *al = b[bx - 1];
    if (*al >= 'a') *al &= 0x5F;
    return 1;
}

/* fill buff from the console until cr, up to 127 characters */
static void readdata(uint16_t ds, random_data *d) {
    print(ds, RANDOM_DATMSG);
    uint16_t bx = 0;
    for (uint16_t cx = 127; cx != 0; cx--) {
        uint8_t al = getchr(ds);
        if (al == cr) break;
        d->buff[bx++] = al;
    }
    d->buff[bx] = 0;
}

void random_main(uint16_t ds) {
    random_data *d = RSP_DATA(random_data, ds);
    rsp_install_transient(ds, &random_image, random_image_len);

    if ((uint8_t)bdos(ds, version, 0).bx < 0x20) {
        print(ds, RANDOM_BADVER);
        (void)bdos(ds, 0, 0); /* abort */
        return;
    }
    /* open the default FCB; if that fails, create the file */
    if ((uint8_t)(bdos(ds, openf, RANDOM_FCB).bx + 1) == 0 &&
        (uint8_t)(bdos(ds, makef, RANDOM_FCB).bx + 1) == 0) {
        print(ds, RANDOM_NOSPACE); /* directory full */
        (void)bdos(ds, 0, 0);
        return;
    }

    for (;;) {
        /* ready: read the next command */
        uint8_t al, ch;
        uint16_t dx;
        int ok = readcom(ds, d, &al, &dx, &ch);
        if (ok) {
            d->ranrec = dx;
            d->ranovf = ch;
            if (al == 'Q') { /* close the file */
                if ((uint8_t)(bdos(ds, closef, RANDOM_FCB).bx + 1) != 0) {
                    (void)bdos(ds, 0, 0);
                    return;
                }
                ok = 0;
            } else if (al == 'W' || al == 'Z') { /* random write (zero fill) */
                readdata(ds, d);
                ok = (uint8_t)bdos(ds, al == 'W' ? writer : writerz, RANDOM_FCB).bx == 0;
            } else if (al == 'R') { /* random read: type the record */
                ok = (uint8_t)bdos(ds, readr, RANDOM_FCB).bx == 0;
                if (ok) {
                    crlf(ds);
                    for (uint16_t i = 0; i < 128; i++) {
                        uint8_t c = d->buff[i] & 0x7F;
                        if (c == 0) break;
                        if (c >= ' ') putchr(ds, c); /* skip non-graphics */
                    }
                }
            } else {
                ok = 0;
            }
        }
        if (!ok) print(ds, RANDOM_ERRMSG); /* error: message, retry */
    }
}
