/*
 * stscmd.c -- C version of src-assem/utils/STSCMD.PLM (a transient,
 * MPMSTAT.CMD): the MP/M-86 status display -- the configuration, the
 * ready, DQ, NQ, delay, poll and flag-wait lists, the flags set, the
 * queues, the processes attached to and waiting for each console and
 * printer, and the memory partitions -- once, or with P in the tail
 * over again after each key.
 *
 * The display itself is STSCOM.PLM's, which STSCMD `$include`s:
 * stscom.c, shared with the MPMSTAT RSP. Its data segment, below, is
 * STSCMD.PLM's as tools/plmc.py lays it out (utils_syms.h): the base
 * page, STSCOM's variables and strings (stscom.h's blocks), STSCMD's
 * own strings, and the stack; stscmd_main installs the image from 100h
 * on. Its main program's variables are PL/M locals, so C locals here.
 * PL/M's semantics are kept as in dir.c (STSCOM's `true` is 0FFh).
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "stscom.h"
#include "utils_syms.h"

#define bp_buff 0x80 /* the base page's buff (the PL/M interface stub's name) */

typedef struct RSP_PACKED {
    uint8_t base[0x100];
    stscom_vars com;
    stscom_strs com_strs;
    char arr183[21], arr197[61], arr198[10], arr199[10], arr203[44];
    uint8_t stack[1024];
    uint8_t stack_base;
} stscmd_data;

#define COM(f) (offsetof(stscmd_data, com) + offsetof(stscom_vars, f))
#define STR(f) (offsetof(stscmd_data, com_strs) + offsetof(stscom_strs, f))
_Static_assert(COM(lparen) == STSCMD_LPAREN, "lparen");
_Static_assert(COM(rparen) == STSCMD_RPAREN, "rparen");
_Static_assert(COM(sysdat_ptr) == STSCMD_SYSDATPOINTER, "sysdatpointer");
_Static_assert(COM(pd_ptr) == STSCMD_PDPOINTER, "pdpointer");
_Static_assert(COM(qd_ptr) == STSCMD_QDPOINTER, "qdpointer");
_Static_assert(COM(md_ptr) == STSCMD_MDPOINTER, "mdpointer");
_Static_assert(COM(ms_ptr) == STSCMD_MSPOINTER, "mspointer");
_Static_assert(COM(sat_ptr) == STSCMD_SATPOINTER, "satpointer");
_Static_assert(COM(flag_ptr) == STSCMD_FLAGPOINTER, "flagpointer");
_Static_assert(COM(ccb_ptr) == STSCMD_CCBPOINTER, "ccbpointer");
_Static_assert(COM(hex_digit) == STSCMD_HEXDIGIT, "hexdigit");
_Static_assert(COM(link_list) == STSCMD_LINKLIST, "linklist");
_Static_assert(STR(arr34) == STSCMD_ARR34, "arr34");
_Static_assert(STR(arr35) == STSCMD_ARR35, "arr35");
_Static_assert(STR(arr36) == STSCMD_ARR36, "arr36");
_Static_assert(STR(arr37) == STSCMD_ARR37, "arr37");
_Static_assert(STR(arr38) == STSCMD_ARR38, "arr38");
_Static_assert(STR(arr42) == STSCMD_ARR42, "arr42");
_Static_assert(STR(arr46) == STSCMD_ARR46, "arr46");
_Static_assert(STR(arr50) == STSCMD_ARR50, "arr50");
_Static_assert(STR(arr57) == STSCMD_ARR57, "arr57");
_Static_assert(STR(arr58) == STSCMD_ARR58, "arr58");
_Static_assert(STR(arr59) == STSCMD_ARR59, "arr59");
_Static_assert(STR(arr60) == STSCMD_ARR60, "arr60");
_Static_assert(STR(arr67) == STSCMD_ARR67, "arr67");
_Static_assert(STR(arr68) == STSCMD_ARR68, "arr68");
_Static_assert(STR(arr69) == STSCMD_ARR69, "arr69");
_Static_assert(STR(arr76) == STSCMD_ARR76, "arr76");
_Static_assert(STR(arr77) == STSCMD_ARR77, "arr77");
_Static_assert(STR(arr78) == STSCMD_ARR78, "arr78");
_Static_assert(STR(arr79) == STSCMD_ARR79, "arr79");
_Static_assert(STR(arr80) == STSCMD_ARR80, "arr80");
_Static_assert(STR(arr87) == STSCMD_ARR87, "arr87");
_Static_assert(STR(arr88) == STSCMD_ARR88, "arr88");
_Static_assert(STR(arr98) == STSCMD_ARR98, "arr98");
_Static_assert(STR(arr110) == STSCMD_ARR110, "arr110");
_Static_assert(STR(arr114) == STSCMD_ARR114, "arr114");
_Static_assert(STR(arr118) == STSCMD_ARR118, "arr118");
_Static_assert(STR(arr119) == STSCMD_ARR119, "arr119");
_Static_assert(STR(arr123) == STSCMD_ARR123, "arr123");
_Static_assert(STR(arr127) == STSCMD_ARR127, "arr127");
_Static_assert(STR(arr128) == STSCMD_ARR128, "arr128");
_Static_assert(STR(arr135) == STSCMD_ARR135, "arr135");
_Static_assert(STR(arr136) == STSCMD_ARR136, "arr136");
_Static_assert(STR(arr137) == STSCMD_ARR137, "arr137");
_Static_assert(STR(arr141) == STSCMD_ARR141, "arr141");
_Static_assert(STR(arr165) == STSCMD_ARR165, "arr165");
_Static_assert(STR(arr166) == STSCMD_ARR166, "arr166");
_Static_assert(STR(arr167) == STSCMD_ARR167, "arr167");
_Static_assert(STR(arr174) == STSCMD_ARR174, "arr174");
_Static_assert(STR(arr175) == STSCMD_ARR175, "arr175");
_Static_assert(offsetof(stscmd_data, arr183) == STSCMD_ARR183, "arr183");
_Static_assert(offsetof(stscmd_data, arr197) == STSCMD_ARR197, "arr197");
_Static_assert(offsetof(stscmd_data, arr198) == STSCMD_ARR198, "arr198");
_Static_assert(offsetof(stscmd_data, arr199) == STSCMD_ARR199, "arr199");
_Static_assert(offsetof(stscmd_data, arr203) == STSCMD_ARR203, "arr203");
_Static_assert(offsetof(stscmd_data, stack_base) == STSCMD_STACK_BASE, "stack_base");
_Static_assert(sizeof(stscmd_data) == STSCMD_DATA_LEN, "length");

#define T 0xFF /* STSCOM's true (comlit.lit) */
#define F 0
#define CR 13
#define LF 10
#define cpmversion 0x30 /* BDOS 3.0 or later */
#define mpmproduct 0x11 /* MP/M-86 */
#define sd_supmod  0x00
#define sd_version 0x78

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

const uint16_t stscmd_image_len = sizeof(stscmd_data);
const stscmd_data stscmd_image = {
    .com = STSCOM_VARS_INIT,
    .com_strs = STSCOM_STRS_INIT,
    .arr183 = "Requires MP/M-86 2.0$",
    .arr197 = "****** Status Display - Values Shown In Hexadecimal *****\r\n\n$",
    .arr198 = "Consoles:$",
    .arr199 = "Printers:$",
    .arr203 = "\r\nHit a Key to Continue, Control C to Stop $",
};

void stscmd_main(uint16_t ds) {
    rsp_install_transient(ds, &stscmd_image, stscmd_image_len);
    stscmd_data *g = RSP_DATA(stscmd_data, ds);
    stscom_vars *v = &g->com;
    uint8_t *bp = g->base;
    uint8_t temp, repeat, i;
    uint16_t ver;
    stscom_ptr vers_str; /* vers$str$pointer */
    stscom_init(ds, v, offsetof(stscmd_data, com_strs));

    ver = stscom_get_version();
    if ((rel((uint8_t)ver != cpmversion) | rel((uint8_t)(ver >> 8) != mpmproduct)) & 1) {
        stscom_print_buffer(STSCMD_ARR183); /* Requires MP/M-86 2.0 */
        stscom_reboot();                    /* use CP/M exit */
    } else {
        stscom_get_sysdat(&v->sysdat_ptr);
        v->ccb_ptr.segment = v->flag_ptr.segment = v->md_ptr.segment = v->ms_ptr.segment = v->sat_ptr.segment =
            v->qd_ptr.segment = v->pd_ptr.segment = v->sysdat_ptr.segment;

        repeat = F;
        for (i = 1; i <= bp[bp_buff]; i++) {
            if ((rel(bp[bp_buff + i] == 'p') | rel(bp[bp_buff + i] == 'P')) & 1) repeat = T;
        }

        temp = T; /* show display at least once */
        while ((repeat | temp) & 1) {
            const uint8_t *sd = far_ptr(rt_phys(), v->sysdat_ptr.segment, v->sysdat_ptr.offset);
            stscom_crlf();
            vers_str.offset = (uint16_t)(rd16(sd + sd_version) + 3); /* skip cr, lf's */
            vers_str.segment = rd16(sd + sd_supmod + 2);               /* sd.supmod(1) */
            stscom_display_text(0, vers_str.offset, vers_str.segment);
            stscom_print_buffer(STSCMD_ARR197); /* ****** Status Display - ... */

            stscom_display_config();
            stscom_display_ready();
            stscom_display_dq();
            stscom_display_nq();
            stscom_display_delay();
            stscom_display_poll();
            stscom_display_flag_wait();
            stscom_display_flag_set();
            stscom_display_queues();
            stscom_display_ccb(STSCMD_ARR198, 0, sd[stscom_ncondev]);                   /* Consoles: */
            stscom_display_ccb(STSCMD_ARR199, sd[stscom_ncondev], sd[stscom_nciodev]);  /* Printers: */
            stscom_display_memory();
            if (repeat & 1) {
                stscom_print_buffer(STSCMD_ARR203); /* Hit a Key to Continue, Control C to Stop */
                stscom_crlf();
                i = stscom_conin();
            }
            temp = F;
        }
    }
    stscom_terminate();
}
