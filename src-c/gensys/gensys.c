/*
 * gensys.c -- the system generation tools/build_mpmsys.py does for /src-assem
 * (standing in for DRI's GENSYS), for src-c, in the same order and with
 * the same results:
 *
 *   - Each module (SUP, RTM, MEM, CIO, BDOS, XIOS) gets its own
 *     paragraph-aligned code segment from GENSYS_CODE_SEG on, its real
 *     length (gensys_syms.h). The C kernel never runs 8086 code, so the
 *     segments are zero, except INIT.SUP's version message, which TMP
 *     prints from the SUP's code segment (s_mpmseg:s_version).
 *   - The module table gets each module's entry and init far pointers;
 *     the dispatcher vector, RTM's entry.
 *   - The answers to GENSYS's questions for this target: one console,
 *     one list device, 60 ticks a second, drive A, mpmseg, nccb.
 *   - The QD pool (in SYSDAT's gap after mxmemcount), then each RSP: its
 *     code segment (zero), then its data image, with its code segment
 *     in its initial frame and u_initcs, chained through rsp_link.
 *   - SYSDAT (sysdat.c) after the RSPs, then the pools past the XIOS's
 *     data: the queue buffer's MAU, the flag table, the PD pool, the
 *     Lock List, and the MD pool with the memory partitions up to 9000h.
 *
 * tests/src-c/test_gensys.py compares the image with build_mpmsys.py's.
 */
#include <stdint.h>

#include "defs.h"
#include "gensys.h"
#include "gensys_syms.h"

#define FREE_MEM_END      0x9000 /* paragraphs: the memory partitions end here */
#define PARTITION_SIZE    0x400  /* 16K */
#define MD_LEN            10     /* MEM.DEF's mdlen */
#define MD_SPARE          40
#define QD_POOL_SLOTS     4
#define QD_SLOT_SIZE      0x1C   /* qdlen */
#define QMAU_BUF_SIZE     0x200
#define QMAU_SAT_ENTRIES  10
#define SAT_LEN           5      /* sat_start, sat_length, sat_nall */
#define NFLAGS            0x20
#define FLG_LEN           3
#define PD_POOL_SLOTS     8
#define PD_LEN            0x30
#define LOCK_ITEMS        16
#define LOCK_ITEM_SIZE    10
#define CCB_LEN           12
#define SYSDAT_SPACE      0x8000 /* room for SYSDAT and its pools */

/* the modules, in image order: code length, entry, init, module-table
 * field */
static const struct {
    uint16_t len, entry, init, field;
} modules[] = {
    {GS_SUP_CODE_LEN, GS_SUP_ENTRY, GS_SUP_INIT, 0x00},    {GS_RTM_CODE_LEN, GS_RTM_ENTRY, GS_RTM_INIT, 0x08},
    {GS_MEM_CODE_LEN, GS_MEM_ENTRY, GS_MEM_INIT, 0x10},    {GS_CIO_CODE_LEN, GS_CIO_ENTRY, GS_CIO_INIT, 0x18},
    {GS_BDOS_CODE_LEN, GS_BDOS_ENTRY, GS_BDOS_INIT, 0x20}, {GS_XIOS_CODE_LEN, GS_XIOS_ENTRY, GS_XIOS_INIT, 0x28},
};
#define DISPATCHER_FIELD 0x38

/* INIT.SUP's `ver`, at INIT_VER in the SUP's code segment */
static const char init_ver[] = "\r\n\nMP/M-86 2.0 [5 Oct 81]\r\nCopyright (C) 1981, Digital Research\r\n\n$";

static uint32_t para_up(uint32_t n) { return (n + 15) & ~(uint32_t)15; }

gensys_layout gensys(uint8_t *phys, const gensys_rsp *rsps, unsigned nrsps) {
    uint8_t *code = phys + (uint32_t)GENSYS_CODE_SEG * 16;
    static uint8_t sd_build[SYSDAT_SPACE];
    uint8_t *sd = sd_build; /* SYSDAT, built here, then put after the code */
    for (uint32_t i = 0; i < SYSDAT_SPACE; i++) sd[i] = 0;
    gensys_sysdat(sd);

    /* the module code segments, and the module table */
    uint32_t clen = 0;
    uint16_t rtm_seg = 0;
    for (unsigned m = 0; m < sizeof modules / sizeof modules[0]; m++) {
        clen = para_up(clen);
        uint16_t seg = (uint16_t)(GENSYS_CODE_SEG + clen / 16);
        for (uint32_t i = 0; i < modules[m].len; i++) code[clen + i] = 0;
        if (m == 0) {
            for (unsigned i = 0; init_ver[i]; i++) code[clen + INIT_VER + i] = (uint8_t)init_ver[i];
        }
        if (m == 1) rtm_seg = seg;
        clen += modules[m].len;
        wr16(sd + modules[m].field, modules[m].entry);
        wr16(sd + modules[m].field + 2, seg);
        wr16(sd + modules[m].field + 4, modules[m].init);
        wr16(sd + modules[m].field + 6, seg);
    }
    wr16(sd + DISPATCHER_FIELD, GS_RTM_ENTRY);
    wr16(sd + DISPATCHER_FIELD + 2, rtm_seg);

    /* GENSYS's answers for this target */
    sd[SYS_MODULE_MAP] = 0x3F; /* sup|rtm|mem|bdos|cio|xios, not net */
    sd[SYS_NCONDEV] = 1;
    sd[SYS_NLSTDEV] = 1;
    sd[SYS_NCIODEV] = 2;
    sd[SYS_TICKSPERSEC] = 60;
    sd[SYS_SRCHDISK] = 0;      /* drive A, the only one */
    wr16(sd + SYS_MPMSEG, GENSYS_CODE_SEG);
    sd[SYS_NCCB] = 2;          /* ncondev + nlstdev */
    sd[SYS_NCNS] = 1;
    sd[SYS_NLST] = 1;
    wr16(sd + SYS_MDUL, 0);
    wr16(sd + SYS_QLR, 0);

    /* the QD pool, between mxmemcount and DATA.BDO */
    uint16_t qd_pool = SYS_MXMEMCOUNT + 1;
    for (unsigned i = 0; i < QD_POOL_SLOTS; i++) {
        uint16_t off = (uint16_t)(qd_pool + i * QD_SLOT_SIZE);
        wr16(sd + off, i < QD_POOL_SLOTS - 1 ? (uint16_t)(off + QD_SLOT_SIZE) : 0);
    }

    /* the RSPs: code segment, then data segment, each paragraph-aligned */
    uint16_t first_rsp = 0;
    uint8_t *prev_link = 0;
    for (unsigned r = 0; r < nrsps; r++) {
        clen = para_up(clen);
        uint16_t code_seg = (uint16_t)(GENSYS_CODE_SEG + clen / 16);
        uint32_t code_paras = para_up(rsps[r].code_len) / 16;
        for (uint32_t i = 0; i < code_paras * 16; i++) code[clen + i] = 0;
        clen += code_paras * 16;
        uint16_t data_seg = (uint16_t)(GENSYS_CODE_SEG + clen / 16);
        uint8_t *data = code + clen;
        for (uint32_t i = 0; i < para_up(rsps[r].len); i++) data[i] = i < rsps[r].len ? rsps[r].data[i] : 0;
        /* GENSYS's fill-ins: the start address and code segment in its
         * initial stack frame (at the UDA's u_stack_sp), and u_initcs */
        uint16_t uda = (uint16_t)(rd16(data + 0x10 + p_uda) * 16);
        uint16_t frame = rd16(data + uda + u_stack_sp);
        wr16(data + frame, rsps[r].start);
        wr16(data + frame + 2, code_seg);
        wr16(data + uda + u_initcs, code_seg);
        clen += para_up(rsps[r].len);
        if (prev_link) wr16(prev_link, data_seg);
        else first_rsp = data_seg;
        prev_link = data; /* rsp_link: the next RSP's data segment */
        wr16(data, 0);
    }

    uint16_t sysdat = (uint16_t)(GENSYS_CODE_SEG + para_up(clen) / 16);
    for (uint32_t i = clen; i < para_up(clen); i++) code[i] = 0;

    /* the pools, past the XIOS's data */
    uint32_t len = XQD_DSEG_LEN;
#define ALLOC(n, align) (len = (len + (align) - 1) / (align) * (align), len += (n), (uint16_t)(len - (n)))

    uint16_t qmau_buf = ALLOC(QMAU_BUF_SIZE, 16);
    uint16_t sat_table = SAT_LEN * (QMAU_SAT_ENTRIES + 1);
    sd[qmau_buf] = QMAU_SAT_ENTRIES; /* SAT0's ents: the table's capacity */
    wr16(sd + qmau_buf + SAT_LEN, (uint16_t)(qmau_buf + sat_table)); /* SAT1: start (a SYSDAT offset), */
    wr16(sd + qmau_buf + SAT_LEN + 2, (uint16_t)(QMAU_BUF_SIZE - sat_table)); /* length, free */
    wr16(sd + SYS_QMAU, 0);
    wr16(sd + SYS_QMAU + 2, (uint16_t)(sysdat + (qmau_buf >> 4)));
    wr16(sd + SYS_QMAU + 4, QMAU_BUF_SIZE);
    wr16(sd + SYS_QMAU + 6, 0);
    wr16(sd + SYS_QUL, qd_pool);
    wr16(sd + SYS_ENDSEG, 0xFFFF);
    wr16(sd + SYS_RSPSEG, first_rsp);

    uint16_t flagtab = ALLOC(NFLAGS * FLG_LEN, 1);
    for (unsigned i = 0; i < NFLAGS; i++) {
        wr16(sd + flagtab + i * FLG_LEN, 0xFFFF); /* flag_off */
        sd[flagtab + i * FLG_LEN + 2] = 0xFF;
    }
    wr16(sd + SYS_FLAGS, flagtab);

    uint16_t pd_pool = ALLOC(PD_POOL_SLOTS * PD_LEN, 2);
    for (unsigned i = 0; i < PD_POOL_SLOTS; i++) {
        uint16_t off = (uint16_t)(pd_pool + i * PD_LEN);
        wr16(sd + off, i < PD_POOL_SLOTS - 1 ? (uint16_t)(off + PD_LEN) : 0); /* p_link */
    }
    wr16(sd + SYS_PUL, pd_pool);

    uint16_t lock_list = ALLOC(LOCK_ITEMS * LOCK_ITEM_SIZE, 2);
    for (unsigned i = 0; i < LOCK_ITEMS; i++) {
        uint16_t off = (uint16_t)(lock_list + i * LOCK_ITEM_SIZE);
        wr16(sd + off, i < LOCK_ITEMS - 1 ? (uint16_t)(off + LOCK_ITEM_SIZE) : 0);
    }
    wr16(sd + SYS_FREE_ROOT, lock_list);
    wr16(sd + SYS_SYSLREC, LOCK_ITEMS);
    wr16(sd + SYS_SYSLFIL, LOCK_ITEMS);
    wr16(sd + SYS_SYSLTOT, LOCK_ITEMS);
    sd[SYS_LOCK_MAX] = LOCK_ITEMS;
    sd[SYS_OPEN_MAX] = LOCK_ITEMS;

    /* the CCBs: console 0 and list 0, in the XIOS's ccbtab0, no mimic */
    wr16(sd + SYS_CCB, XQD_CCBTAB0);
    for (unsigned i = 0; i < 2; i++) {
        sd[XQD_CCBTAB0 + CCB_LEN * i + 8] = 0xFF; /* c_mimic */
        sd[XQD_CCBTAB0 + CCB_LEN * i + 9] = 0xFF; /* c_msource */
    }

    /* the MD pool: spare MDs on mdul, and one per 16K partition from
     * past SYSDAT to FREE_MEM_END on mfl, in memory order */
    uint16_t md_pool = (uint16_t)len;
    uint16_t nparts = (uint16_t)((FREE_MEM_END - (sysdat + md_pool / 16)) / PARTITION_SIZE);
    uint16_t md_count = (uint16_t)(MD_SPARE + nparts);
    len += (uint32_t)md_count * MD_LEN;
    uint16_t free_start = (uint16_t)(sysdat + para_up(len) / 16);
    uint16_t nstarts = 0;
    for (uint32_t s = free_start; s < FREE_MEM_END; s += PARTITION_SIZE) nstarts++;
    if (nstarts > 1 && FREE_MEM_END - (free_start + (nstarts - 1) * PARTITION_SIZE) < PARTITION_SIZE)
        nstarts--; /* fold the remainder into the last */
    uint16_t nspare = (uint16_t)(md_count - nstarts);
    for (uint16_t i = 0; i < nspare; i++) {
        uint16_t off = (uint16_t)(md_pool + i * MD_LEN);
        wr16(sd + off, i + 1 < nspare ? (uint16_t)(off + MD_LEN) : 0);
    }
    wr16(sd + SYS_MDUL, md_pool);
    for (uint16_t i = 0; i < nstarts; i++) {
        uint16_t off = (uint16_t)(md_pool + (nspare + i) * MD_LEN);
        uint16_t start = (uint16_t)(free_start + i * PARTITION_SIZE);
        uint16_t end = i + 1 < nstarts ? (uint16_t)(start + PARTITION_SIZE) : FREE_MEM_END;
        wr16(sd + off, i + 1 < nstarts ? (uint16_t)(off + MD_LEN) : 0);
        wr16(sd + off + 2, start);
        wr16(sd + off + 4, (uint16_t)(end - start));
        wr16(sd + off + 6, 0);
    }
    wr16(sd + SYS_MFL, (uint16_t)(md_pool + nspare * MD_LEN));
#undef ALLOC

    /* SYSDAT into place */
    uint8_t *dst = phys + (uint32_t)sysdat * 16;
    for (uint32_t i = 0; i < len; i++) dst[i] = sd[i];
    return (gensys_layout){sysdat, (uint16_t)len, free_start};
}
