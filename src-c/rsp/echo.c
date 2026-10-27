/*
 * echo.c -- C version of src/rsp/ECHO.A86, the ECHO resident system
 * process: it makes and opens a queue named ECHO, and prints every
 * command tail the CLI writes to it ("ECHO hello") on the console of
 * the process that typed it.
 *
 * Its data segment, below, is ECHO.A86's DSEG byte for byte (rsp.h);
 * echo_main is `main:`/`loop:`.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "rsp_syms.h"

#define mpm_conwrite  9
#define mpm_qmake     134
#define mpm_qopen     135
#define mpm_qread     137
#define mpm_setprior  145
#define mpm_condetach 147
#define mpm_setdefcon 148
#define qf_rsp 0x08

typedef struct RSP_PACKED {
    uint16_t link;
    uint8_t net, org;
    uint16_t flags;
    char name[8] __attribute__((nonstring));
    uint16_t msglen, nmsgs, dq, nq, msgcnt, msgout, buf;
} rsp_qd_t;

typedef struct RSP_PACKED {
    uint8_t flgs[2];
    uint16_t qid, nmsgs, buffptr;
    char name[8] __attribute__((nonstring));
} rsp_qpb_t;

typedef struct RSP_PACKED {
    uint16_t sdatseg;   /* rsp_link: INIT.SUP gives the RSP SYSDAT here */
    uint16_t hdr[7];
    rsp_pd_t pd;
    rsp_uda_t uda;
    uint8_t qbuf[131];  /* queue buffer */
    rsp_qd_t qd;
    uint8_t dma[128];
    uint16_t stack[15];
    rsp_frame_t stack_tos;
    uint16_t pdadr;     /* QPB buffer: the CLI's message is the */
    uint8_t cmdtail[129]; /* sender's PD, then the command tail */
    uint8_t crlf[3];
    rsp_qpb_t qpb;
    uint8_t console;
} echo_data;

_Static_assert(offsetof(echo_data, pd) == ECHO_PD, "pd");
_Static_assert(offsetof(echo_data, uda) == ECHO_UDA, "uda");
_Static_assert(offsetof(echo_data, qbuf) == ECHO_QBUF, "qbuf");
_Static_assert(offsetof(echo_data, qd) == ECHO_QD, "qd");
_Static_assert(offsetof(echo_data, dma) == ECHO_DMA, "dma");
_Static_assert(offsetof(echo_data, stack) == ECHO_STACK, "stack");
_Static_assert(offsetof(echo_data, stack_tos) == ECHO_STACK_TOS, "stack_tos");
_Static_assert(offsetof(echo_data, pdadr) == ECHO_PDADR, "pdadr");
_Static_assert(offsetof(echo_data, cmdtail) == ECHO_CMDTAIL, "cmdtail");
_Static_assert(offsetof(echo_data, qpb) == ECHO_QPB, "qpb");
_Static_assert(offsetof(echo_data, console) == ECHO_CONSOLE, "console");

const uint16_t echo_image_len = sizeof(echo_data);
const echo_data echo_image = {
    .pd = RSP_PD(190, pf_keep, "ECHO    ", 0),
    .uda = RSP_UDA(ECHO_DMA, ECHO_STACK_TOS),
    .qd = {0, 0, 0, qf_rsp, "ECHO    ", 131, 1, 0, 0, 0, 0, ECHO_QBUF},
    .stack = {0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC,
              0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC},
    .crlf = {13, 10, '$'},
    .qpb = {{0, 0}, 0, 1, ECHO_PDADR, "ECHO    "},
};

void echo_main(uint16_t ds) {
    echo_data *d = RSP_DATA(echo_data, ds);
    /* create, then open, the ECHO queue; set priority to normal */
    (void)rt_mpm(ds, mpm_qmake, ECHO_QD);
    (void)rt_mpm(ds, mpm_qopen, ECHO_QPB);
    (void)rt_mpm(ds, mpm_setprior, 200);
    uint16_t es = d->sdatseg; /* ES points to SYSDAT */
    for (;;) {
        /* read a cmdtail from the queue */
        (void)rt_mpm(ds, mpm_qread, ECHO_QPB);
        /* set the default console from the sender's PD */
        d->console = far_ptr(rt_phys(), es, (uint16_t)(d->pdadr + p_cns))[0];
        (void)rt_mpm(ds, mpm_setdefcon, d->console); /* only DL counts */
        /* scan the cmdtail for '$' or 0; replace it with cr,lf,'$' */
        uint16_t bx = ECHO_CMDTAIL;
        uint16_t end = (uint16_t)(ECHO_CMDTAIL + 131);
        uint8_t *b = (uint8_t *)d;
        while (bx <= end && b[bx] != '$' && b[bx] != 0) bx++;
        b[bx] = 13;
        b[bx + 1] = 10;
        b[bx + 2] = '$';
        /* write the command tail, then detach the console */
        (void)rt_mpm(ds, mpm_conwrite, ECHO_CMDTAIL);
        (void)rt_mpm(ds, mpm_condetach, d->console);
    }
}
