/*
 * rsp.h -- shared by the C versions of src-assem/rsp/'s programs.
 *
 * Each program's data segment is one packed, initialized C object laid
 * out exactly as its .A86 DSEG is: the same fields at the same offsets
 * with the same initial values (the RSP header, PD and UDA of an RSP,
 * its queue descriptors, buffers, messages). The program runs on the
 * copy at its real segment -- it gets that segment as DS -- so the
 * kernel sees the same bytes it would for the .A86 version. Each image
 * has static asserts on its field offsets (rsp_syms.h: the .A86's
 * label offsets, which tests/src-c/test_syms.py checks against
 * tools/asm86's symbol table), and tests/src-c/test_rsp_images.py
 * compares the whole image with the assembled DSEG.
 *
 * The one thing that can't be the same is a code address: an RSP's
 * initial stack frame starts at its main label, which is 8086 code
 * only the .A86 has. The C images leave the frame's IP:CS 0 and
 * src-c/gensys fills them in as GENSYS does (with u_initcs), when it
 * places the images (the dispatcher never jumps there: the runtime
 * starts a process in its C main).
 */
#ifndef SRC_C_RSP_H
#define SRC_C_RSP_H

#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rt.h"

#define RSP_PACKED __attribute__((packed))

/* a transient (a .CMD's data group, as LOAD left it at `ds`), from its
 * own main: its data image, everything above the base page (which LOAD
 * and the CLI fill in) */
static inline void rsp_install_transient(uint16_t ds, const void *image, uint16_t len) {
    uint8_t *dst = far_ptr(rt_phys(), ds, 0);
    for (uint16_t i = 0x100; i < len; i++) dst[i] = ((const uint8_t *)image)[i];
}

/* the program's own data segment, at its real segment */
#define RSP_DATA(type, ds) ((type *)far_ptr(rt_phys(), (ds), 0))

/* An RSP's Process Descriptor (src-assem/kernel/PD.DEF, 48 bytes) and User
 * Data Area (UDA.FMT, 100h bytes) as the .A86 RSPs lay them out. The
 * PD sits at rsp_pd (10h) and the UDA at rsp_uda (40h); the UDA's only
 * initialized words are u_dma_ofst and u_stack_sp (the initial frame). */
typedef struct RSP_PACKED {
    uint16_t link, thread;
    uint8_t stat, prior;
    uint16_t flag;
    char name[8] __attribute__((nonstring));
    uint16_t uda;
    uint8_t dsk, user, ldsk, luser;
    uint16_t mem, dvract, wait;
    uint8_t org, net;
    uint16_t parent;
    uint8_t cns, abort, cin, cout, lst, sf3, sf4, sf5;
    uint16_t reserved[2], pret, scratch;
} rsp_pd_t;

typedef struct RSP_PACKED {
    uint16_t dparam, dma_ofst;
    uint8_t r1[0x34 - 4];
    uint16_t sp;
    uint8_t r2[0x100 - 0x36];
} rsp_uda_t;

/* An RSP's initial stack frame: IRET's ip, cs, flags (ip and cs are
 * code addresses: 0 in a C image, see above) */
typedef struct RSP_PACKED {
    uint16_t ip, cs, flags;
} rsp_frame_t;

#define RSP_PD(prior_, flag_, name_, mem_) \
    { 0, 0, ps_run, (prior_), (flag_), name_, rsp_uda >> 4, 0, 0, 0, 0, (mem_), 0, 0, 0, 0, 0, \
      0, 0, 0, 0, 0, 0, 0, 0, {0, 0}, 0, 0 }
#define RSP_UDA(dma_, sp_) { 0, (dma_), {0}, (sp_), {0} }

#ifndef rsp_uda
#define rsp_uda    0x040 /* src-assem/kernel/RSP.DEF */
#define rsp_bottom 0x140
#endif

/* a program's code segment (the RSP's, or a transient's code group):
 * proc_creat leaves it in the UDA's u_initcs */
static inline uint16_t rsp_cs(void) {
    const uint8_t *sd = far_ptr(rt_phys(), rt_sysdat(), 0);
    return seg_r16(far_ptr(rt_phys(), rlr_uda(sd), 0), u_initcs);
}

/* copy a constant block to seg:off (a code segment's message area) */
static inline void rsp_place(uint16_t seg, uint16_t off, const void *src, uint16_t len) {
    uint8_t *dst = far_ptr(rt_phys(), seg, off);
    for (uint16_t i = 0; i < len; i++) dst[i] = ((const uint8_t *)src)[i];
}

#endif
