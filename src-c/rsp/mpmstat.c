/*
 * mpmstat.c -- C version of src-assem/utils/STSRSP.PLM, the MPMSTAT
 * resident system process: it makes and opens a queue named MPMSTAT,
 * and for every command the CLI writes to it ("MPMSTAT") prints the
 * status display on the console of the process that typed it.
 *
 * The display is STSCOM.PLM's, which STSRSP `$include`s:
 * src-c/utils/stscom.c, shared with the transient MPMSTAT.CMD
 * (src-c/utils/stscmd.c). Its data segment, below, is STSRSP.PLM's as
 * plmc --rsp MPMSTAT lays it out (src-assem/Makefile's MPMSTAT.RSP;
 * rsp_syms.h): RSPASM's shell -- the RSP header, the PD (priority 190,
 * system + keep), the UDA, the DMA buffer, the stack and its initial
 * frame, the fcbs -- then STSCOM's variables, STSRSP's queue
 * descriptor, buffer, command and QPB, STSCOM's strings and STSRSP's
 * own. mpmstat_main is plm$start; its variables are PL/M locals, so C
 * locals here. PL/M's semantics are kept as in src-c/utils/dir.c.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "rsp_syms.h"
#include "stscom.h"

#define mpm_qmake     134
#define mpm_qopen     135
#define mpm_qread     137
#define mpm_setprior  145
#define mpm_condetach 147
#define mpm_setdefcon 148
#define qf_keep 0x02
#define qf_rsp  0x08
#define sd_supmod  0x00
#define sd_version 0x78

typedef struct RSP_PACKED {
    uint16_t link;
    uint8_t net, org;
    uint16_t flags;
    char name[8] __attribute__((nonstring));
    uint16_t msglen, nmsgs, dq, nq, msgcnt, msgout, buffer;
} sts_qd_t;

typedef struct RSP_PACKED {
    uint8_t flgs[2];
    uint16_t qaddr, nmsgs, buffptr;
    char name[8] __attribute__((nonstring));
} sts_qpb_t;

typedef struct RSP_PACKED {
    uint16_t rsplink;   /* rsp_link: INIT.SUP gives the RSP SYSDAT here */
    uint16_t hdr[7];
    rsp_pd_t pd;
    rsp_uda_t uda;
    uint8_t buff[128];  /* tbuff, the DMA buffer */
    uint8_t stack[1024];
    rsp_frame_t stack_top;
    uint8_t fcb[16], fcb16[20];
    stscom_vars com;
    sts_qd_t sts_qd;
    uint8_t sts_qd_buf[131]; /* not copied if within 64K of sysdat */
    uint16_t sts_cmd_pd;     /* sts$cmd: info from qd$buf copied here */
    uint8_t sts_cmd_tail[129];
    sts_qpb_t sts_qpb;
    stscom_strs com_strs;
    char arr179[61], arr180[10], arr181[10];
} mpmstat_data;

#define COM(f) (offsetof(mpmstat_data, com) + offsetof(stscom_vars, f))
#define STR(f) (offsetof(mpmstat_data, com_strs) + offsetof(stscom_strs, f))
_Static_assert(offsetof(mpmstat_data, pd) == MPMSTAT_RSP_PD, "pd");
_Static_assert(offsetof(mpmstat_data, uda) == MPMSTAT_UDA, "uda");
_Static_assert(offsetof(mpmstat_data, buff) == MPMSTAT_BP_TBUFF, "tbuff");
_Static_assert(offsetof(mpmstat_data, stack_top) == MPMSTAT_RSP_STACK_TOP, "rsp_stack_top");
_Static_assert(offsetof(mpmstat_data, fcb) == MPMSTAT_BP_FCB, "fcb");
_Static_assert(offsetof(mpmstat_data, fcb16) == MPMSTAT_BP_FCB16, "fcb16");
_Static_assert(COM(lparen) == MPMSTAT_LPAREN, "lparen");
_Static_assert(COM(rparen) == MPMSTAT_RPAREN, "rparen");
_Static_assert(COM(sysdat_ptr) == MPMSTAT_SYSDATPOINTER, "sysdatpointer");
_Static_assert(COM(pd_ptr) == MPMSTAT_PDPOINTER, "pdpointer");
_Static_assert(COM(qd_ptr) == MPMSTAT_QDPOINTER, "qdpointer");
_Static_assert(COM(md_ptr) == MPMSTAT_MDPOINTER, "mdpointer");
_Static_assert(COM(ms_ptr) == MPMSTAT_MSPOINTER, "mspointer");
_Static_assert(COM(sat_ptr) == MPMSTAT_SATPOINTER, "satpointer");
_Static_assert(COM(flag_ptr) == MPMSTAT_FLAGPOINTER, "flagpointer");
_Static_assert(COM(ccb_ptr) == MPMSTAT_CCBPOINTER, "ccbpointer");
_Static_assert(COM(hex_digit) == MPMSTAT_HEXDIGIT, "hexdigit");
_Static_assert(COM(link_list) == MPMSTAT_LINKLIST, "linklist");
_Static_assert(offsetof(mpmstat_data, sts_qd) == MPMSTAT_STSQD, "stsqd");
_Static_assert(offsetof(mpmstat_data, sts_qd_buf) == MPMSTAT_STSQDBUF, "stsqdbuf");
_Static_assert(offsetof(mpmstat_data, sts_cmd_pd) == MPMSTAT_STSCMD, "stscmd");
_Static_assert(offsetof(mpmstat_data, sts_qpb) == MPMSTAT_STSQPB, "stsqpb");
_Static_assert(STR(arr34) == MPMSTAT_ARR34, "arr34");
_Static_assert(STR(arr42) == MPMSTAT_ARR42, "arr42");
_Static_assert(STR(arr98) == MPMSTAT_ARR98, "arr98");
_Static_assert(STR(arr141) == MPMSTAT_ARR141, "arr141");
_Static_assert(STR(arr175) == MPMSTAT_ARR175, "arr175");
_Static_assert(offsetof(mpmstat_data, arr179) == MPMSTAT_ARR179, "arr179");
_Static_assert(offsetof(mpmstat_data, arr180) == MPMSTAT_ARR180, "arr180");
_Static_assert(offsetof(mpmstat_data, arr181) == MPMSTAT_ARR181, "arr181");
_Static_assert(sizeof(mpmstat_data) == MPMSTAT_DATA_LEN, "length");

const uint16_t mpmstat_image_len = sizeof(mpmstat_data);
const mpmstat_data mpmstat_image = {
    .pd = RSP_PD(190, pf_sys | pf_keep, "MPMSTAT ", 0),
    .uda = RSP_UDA(MPMSTAT_BP_BUFF, MPMSTAT_RSP_STACK_TOP),
    .com = STSCOM_VARS_INIT,
    .sts_qd = {0, 0, 0, qf_keep + qf_rsp, "MPMSTAT ", 131, 1, 0, 0, 0, 0, MPMSTAT_STSQDBUF},
    .sts_qpb = {{0, 0}, 0, 1, MPMSTAT_STSCMD, "MPMSTAT "},
    .com_strs = STSCOM_STRS_INIT,
    .arr179 = "****** Status Display - Values Shown In Hexadecimal *****\r\n\n$",
    .arr180 = "Consoles:$",
    .arr181 = "Printers:$",
};

void mpmstat_main(uint16_t ds) {
    mpmstat_data *d = RSP_DATA(mpmstat_data, ds);
    stscom_vars *v = &d->com;
    stscom_ptr vers_str; /* vers$str$pointer */
    stscom_init(ds, v, offsetof(mpmstat_data, com_strs));

    stscom_mon1(mpm_qmake, MPMSTAT_STSQD);  /* make MPMSTAT queue */
    stscom_mon1(mpm_qopen, MPMSTAT_STSQPB); /* open it */

    v->ccb_ptr.segment = v->flag_ptr.segment = v->md_ptr.segment = v->ms_ptr.segment = v->sat_ptr.segment =
        v->qd_ptr.segment = v->pd_ptr.segment = v->sysdat_ptr.segment = d->rsplink;
    v->sysdat_ptr.offset = 0;

    const uint8_t *sd = far_ptr(rt_phys(), v->sysdat_ptr.segment, v->sysdat_ptr.offset);
    vers_str.offset = (uint16_t)(rd16(sd + sd_version) + 3); /* skip cr, lf's */
    vers_str.segment = rd16(sd + sd_supmod + 2);               /* sd.supmod(1) */

    stscom_mon1(mpm_setprior, 200); /* back to the same as transients */

    for (;;) {
        stscom_mon1(mpm_qread, MPMSTAT_STSQPB);

        v->pd_ptr.offset = d->sts_cmd_pd;
        stscom_mon1(mpm_setdefcon, far_ptr(rt_phys(), v->pd_ptr.segment, v->pd_ptr.offset)[p_cns]);
        stscom_crlf(); /* to who typed MPMSTAT */
        stscom_display_text(0, vers_str.offset, vers_str.segment);
        stscom_print_buffer(MPMSTAT_ARR179); /* ****** Status Display - ... */
        stscom_display_config();
        stscom_display_ready();
        stscom_display_dq();
        stscom_display_nq();
        stscom_display_delay();
        stscom_display_poll();
        stscom_display_flag_wait();
        stscom_display_flag_set();
        stscom_display_queues();
        stscom_display_ccb(MPMSTAT_ARR180, 0, sd[stscom_ncondev]);                  /* Consoles: */
        stscom_display_ccb(MPMSTAT_ARR181, sd[stscom_ncondev], sd[stscom_nciodev]); /* Printers: */
        stscom_display_memory();
        stscom_mon1(mpm_condetach, 0); /* dummy */
    }
}
