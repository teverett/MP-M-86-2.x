/*
 * gensys.h -- src-c's GENSYS: builds MP/M-86's low-memory image (the
 * modules' code segments, the RSPs, SYSDAT with its tables and pools)
 * from src-c's own data, so kernel.elf needs nothing from /src-assem's build.
 */
#ifndef SRC_C_GENSYS_H
#define SRC_C_GENSYS_H

#include <stdint.h>

#define GENSYS_CODE_SEG 0x1000 /* the first module's code segment (the SUP's) */

/* an RSP the image holds: its data segment's initial image (the C
 * version's, src-c/rsp/), the length of its code segment, and its start
 * address (the IP in its initial stack frame) */
typedef struct {
    const char *name;
    const uint8_t *data;
    uint16_t len;
    uint16_t code_len;
    uint16_t start;
} gensys_rsp;

/* what gensys built */
typedef struct {
    uint16_t sysdat;    /* SYSDAT's segment */
    uint16_t sysdat_len; /* its length, pools included */
    uint16_t free_start; /* the first paragraph of the memory partitions */
} gensys_layout;

/* the RSPs src-c has (src-c/rsp/programs.c), in INIT.SUP's order */
const gensys_rsp *gensys_rsps(unsigned *n);

/* SYSDAT's initial data (sysdat.c): SYSTEM.DAT, DATA.BDO and the XIOS's
 * data, at sd (XQD_DSEG_LEN bytes) */
void gensys_sysdat(uint8_t *sd);

/* build the whole image at physical address phys + GENSYS_CODE_SEG*16 on:
 * the module code segments, then each RSP (code, then data, chained by
 * rsp_link), then SYSDAT */
gensys_layout gensys(uint8_t *phys, const gensys_rsp *rsps, unsigned nrsps);

#endif
