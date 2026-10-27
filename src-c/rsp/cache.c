/*
 * cache.c -- C version of src/rsp/CACHE.RSP, DRI's disk directory
 * cache. There is no source for it, only the GENCMD binary
 * (orig/lastdri/22, docs/bugs.md #124); this follows its disassembly.
 *
 * Cachinit, the RSP's process, puts the cache's handler in front of the
 * XIOS entry and terminates. The handler keeps records 0-15 of drives
 * A and B (their 64-entry directories) in two 2K buffers: a read of a
 * cached record is copied from the buffer, and writes, and reads before
 * the buffer is filled, go to the XIOS and are copied into it.
 *
 * Its data segment, below, is CACHE.RSP's data group byte for byte
 * (rsp.h). The .RSP has no symbols, so the offsets are the ones its code
 * uses (CACHE_* below). In the binary the handler finds its DS and the
 * XIOS entry it replaced in its code segment (CS:0, CS:2); cache_main
 * writes them there as it does, and SYSDAT's xiosmod gets CS:0038, the
 * handler's address. The C kernel calls the XIOS through rt_xiosmod
 * (rt.h), so that's what the C handler hooks.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"

#define mpm_terminate 0x8F

/* its code segment */
#define CACHE_CS_DS      0x0000 /* word: the cache's DS */
#define CACHE_CS_XIOS    0x0002 /* dword: the XIOS entry it replaced */
#define CACHE_CS_HANDLER 0x0038 /* the handler */

/* its data segment */
#define CACHE_STACK      0x0140
#define CACHE_STACK_TOS  0x0158 /* start frame: IP 9 (cachinit) */
#define CACHE_DMA        0x015E
#define CACHE_DMA_OFF    0x01DE /* word: SETDMA's offset */
#define CACHE_DMA_SEG    0x01E0 /* word: SETDMAB's segment */
#define CACHE_CUR        0x01E2 /* word: the selected drive's entry */
#define CACHE_DISK       0x01E4 /* byte: SELDSK's drive */
#define CACHE_ACTIVE     0x01E5 /* byte: 0FFh if that drive is cached */
#define CACHE_NDRIVES    0x01E6 /* byte */
#define CACHE_TABLE      0x01E7 /* cache_entry[ndrives] */
#define CACHE_BUF_A      0x0203
#define CACHE_BUF_B      0x0A13
#define CACHE_LEN        0x1230 /* 123h paragraphs */

#define SYSDAT_XIOSMOD 0x28 /* dword: XIOS entry */

#define nrecs 16

/* a drive's entry: drive, valid (0FFh once the buffer holds every
 * record), the buffer, the last record cached */
typedef struct RSP_PACKED {
    uint8_t drive, valid;
    uint16_t buffer, last;
} cache_entry;

typedef struct RSP_PACKED {
    uint16_t sysdat;    /* rsp_link: INIT.SUP gives the RSP SYSDAT here */
    uint16_t hdr[7];
    rsp_pd_t pd;
    rsp_uda_t uda;
    uint16_t stack[12];
    rsp_frame_t stack_tos;
    uint8_t dma[128];
    uint16_t dma_off, dma_seg, cur;
    uint8_t disk, active, ndrives;
    cache_entry table[2];
    uint16_t guard_a[8]; /* 0ABCDh fill around the buffers */
    uint8_t buf_a[nrecs * 128];
    uint16_t guard_b[8];
    uint8_t buf_b[nrecs * 128];
    uint16_t guard_c[8];
    uint8_t pad[13];
} cache_data;

_Static_assert(offsetof(cache_data, pd) == rsp_pd, "pd");
_Static_assert(offsetof(cache_data, uda) == rsp_uda, "uda");
_Static_assert(offsetof(cache_data, stack) == CACHE_STACK, "stack");
_Static_assert(offsetof(cache_data, stack_tos) == CACHE_STACK_TOS, "stack_tos");
_Static_assert(offsetof(cache_data, dma) == CACHE_DMA, "dma");
_Static_assert(offsetof(cache_data, dma_off) == CACHE_DMA_OFF, "dma_off");
_Static_assert(offsetof(cache_data, dma_seg) == CACHE_DMA_SEG, "dma_seg");
_Static_assert(offsetof(cache_data, cur) == CACHE_CUR, "cur");
_Static_assert(offsetof(cache_data, disk) == CACHE_DISK, "disk");
_Static_assert(offsetof(cache_data, active) == CACHE_ACTIVE, "active");
_Static_assert(offsetof(cache_data, ndrives) == CACHE_NDRIVES, "ndrives");
_Static_assert(offsetof(cache_data, table) == CACHE_TABLE, "table");
_Static_assert(offsetof(cache_data, buf_a) == CACHE_BUF_A, "buf_a");
_Static_assert(offsetof(cache_data, buf_b) == CACHE_BUF_B, "buf_b");
_Static_assert(sizeof(cache_data) == CACHE_LEN, "len");

#define GUARD {0xABCD, 0xABCD, 0xABCD, 0xABCD, 0xABCD, 0xABCD, 0xABCD, 0xABCD}

const uint16_t cache_image_len = sizeof(cache_data);
const cache_data cache_image = {
    .pd = RSP_PD(0x40, 0, "Cachinit", 0),
    .uda = RSP_UDA(CACHE_DMA, CACHE_STACK_TOS),
    .stack = {0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC,
              0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC},
    .ndrives = 2,
    .table = {{0, 0, CACHE_BUF_A, nrecs - 1}, {1, 0, CACHE_BUF_B, nrecs - 1}},
    .guard_a = GUARD,
    .guard_b = GUARD,
    .guard_c = GUARD,
};

/* CS:0 and CS:2, as the handler has them: the cache's DS, and the XIOS
 * it passes calls on to */
static uint16_t cache_ds;
static rt_xios_fn cache_xios_next;

static uint16_t xios(uint8_t *phys, uint16_t sysdat, uint8_t al, uint16_t cx, uint16_t dx) {
    return cache_xios_next(phys, sysdat, al, cx, dx);
}

/* the 128-byte record `dx` of the entry's buffer, and the DMA */
static uint8_t *record(uint8_t *phys, cache_entry *e, uint16_t dx) {
    return far_ptr(phys, cache_ds, (uint16_t)(e->buffer + (uint16_t)(dx << 7)));
}

static void copy128(uint8_t *dst, const uint8_t *src) {
    for (int i = 0; i < 128; i++) dst[i] = src[i];
}

/* CS:0038 -- the XIOS entry while the cache is in */
static uint16_t cache_xios(uint8_t *phys, uint16_t sysdat, uint8_t al, uint16_t cx, uint16_t dx) {
    if (al < io_home) return xios(phys, sysdat, al, cx, dx);
    cache_data *d = RSP_DATA(cache_data, cache_ds);
    uint8_t *ds = (uint8_t *)d;

    if (al == io_seldsk) {
        uint8_t cl = (uint8_t)cx;
        d->disk = cl;
        uint16_t si = CACHE_TABLE;
        uint8_t n = d->ndrives;
        for (;;) {
            if (n == 0) {
                d->active = 0;
                break;
            }
            cache_entry *e = (cache_entry *)(ds + si);
            if (e->drive == cl) {
                d->active = 0xFF;
                d->cur = si;
                if ((uint8_t)dx == 0) e->valid = 0; /* first select: log in again */
                break;
            }
            n--;
            si = (uint16_t)(si + sizeof(cache_entry));
        }
        return xios(phys, sysdat, al, cx, dx);
    }
    if (al == io_setdma) {
        d->dma_off = cx;
        return xios(phys, sysdat, al, cx, dx);
    }
    if (al == io_setdmab) {
        d->dma_seg = cx;
        return xios(phys, sysdat, al, cx, dx);
    }
    if (d->active != 0xFF) return xios(phys, sysdat, al, cx, dx);

    cache_entry *e = (cache_entry *)(ds + d->cur);
    uint8_t *dma = far_ptr(phys, d->dma_seg, d->dma_off);
    if (al == io_home) {
        if (e->valid != 0xFF) return xios(phys, sysdat, al, cx, dx);
        return al; /* the directory is in the buffer: no seek */
    }
    if (al == io_read) {
        if ((cx >> 8) != 0 || dx > e->last) return xios(phys, sysdat, al, cx, dx);
        if (e->valid == 0xFF) {
            copy128(dma, record(phys, e, dx));
            return 0;
        }
        if (dx == e->last) e->valid = 0xFF; /* the buffer's full after this one */
    } else if (al == io_write) {
        if ((cx >> 8) != 0 || dx > e->last) return xios(phys, sysdat, al, cx, dx);
    } else {
        return xios(phys, sysdat, al, cx, dx);
    }
    /* through to the disk, then into the buffer */
    uint16_t ax = xios(phys, sysdat, al, cx, dx);
    copy128(record(phys, e, dx), dma);
    return ax;
}

void cache_main(uint16_t ds) {
    cache_data *d = RSP_DATA(cache_data, ds);
    uint8_t *phys = rt_phys();
    uint16_t cs = rsp_cs();
    uint8_t *code = far_ptr(phys, cs, 0);
    uint8_t *sys = far_ptr(phys, d->sysdat, 0);

    /* save our DS and the XIOS entry; point the entry at the handler */
    wr16(code + CACHE_CS_DS, ds);
    wr16(code + CACHE_CS_XIOS, rd16(sys + SYSDAT_XIOSMOD));
    wr16(code + CACHE_CS_XIOS + 2, rd16(sys + SYSDAT_XIOSMOD + 2));
    cache_ds = ds;
    cache_xios_next = rt_xiosmod;
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory"); /* pushf ! cli */
    wr16(sys + SYSDAT_XIOSMOD, CACHE_CS_HANDLER);
    wr16(sys + SYSDAT_XIOSMOD + 2, cs);
    rt_xiosmod = cache_xios;
    __asm__ volatile("pushl %0; popfl" : : "r"(flags) : "memory", "cc"); /* popf */

    (void)rt_mpm(ds, mpm_terminate, 0);
}
