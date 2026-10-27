/*
 * kernel_main.c -- entered from boot.s's `_start`. Hands console I/O
 * to the real ported XIOS (src-c/kernel/xios86q.c), then runs the
 * real ported system initialization (sup_init, src-c/kernel/init.c)
 * against real low memory and reports what it did.
 *
 * XIOS: all of xios86q.c's port I/O goes through the io_in8/io_out8
 * callbacks, so the real inb/outb below satisfy it directly. Its data
 * is SYSDAT's, from offset 1000h (just past SYSDAT.A86's own dseg), so
 * kernel_main first builds the low-memory image with src-c/gensys (the
 * signon it prints is there, at XQ_SIGNON) and takes SYSDAT's segment
 * from it.
 *
 * sup_init: runs on real memory (defs.h's far_ptr), so it gets the
 * physical base (0 -- multiboot leaves us in flat 32-bit mode with
 * base-0 segments, so linear == physical) and real segments, laid out
 * the way the real boot lays them out: the SUP code segment at
 * GENSYS_CODE_SEG (1000h) and SYSDAT past the code and the RSPs, where
 * gensys put it. This check replaces SYSDAT with the minimal image
 * sup_init reads (module_table[SUP]'s entry segment, an empty RSP
 * list); rt_boot builds the real one again afterwards. The notick/idle/user_entry offsets are
 * the real ones from src-assem/kernel/SUP.A86; sup_init only stores them,
 * nothing jumps to them in protected mode.
 *
 * Then every field sup_init should have written is read back from real
 * memory and checked; the result goes out the serial console.
 * tests/src-c/test_boot.py checks for "sup_init: ok".
 */
#include <stdint.h>

#include "defs.h"
#include "gensys.h"

#define SUP_CS          GENSYS_CODE_SEG
static uint16_t SYSDAT_SEG; /* from gensys */
#define SYSDAT_LEN      0x1000 /* SYSDAT.A86's own 4096-byte dseg */
#define SUP_NOTICK      0x018C /* src-assem/kernel/SUP.A86: notick */
#define SUP_IDLE        0x0178 /* src-assem/kernel/SUP.A86: idle */
#define SUP_USER_ENTRY  0x01EC /* src-assem/kernel/SUP.A86: user_entry */
#define SYSDAT_XIOS_INIT_SEG 0x002E /* the old bare `mov u_wrkseg,ds` bug's target */

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);

extern void init_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8);
extern void xios_conout_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl);
extern void rt_boot(void);
extern void sup_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs,
                     uint16_t notick_offset, uint16_t idle_offset,
                     uint16_t user_entry_offset);

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void puts_con(const char *s) {
    for (; *s; s++) {
        xios_conout_entry(inb, outb, (uint8_t)*s);
    }
}

static void puthex16(uint16_t v) {
    for (int shift = 12; shift >= 0; shift -= 4) {
        xios_conout_entry(inb, outb, (uint8_t)"0123456789ABCDEF"[(v >> shift) & 0xF]);
    }
}

/* Physical base, kept opaque so the compiler treats far_ptr() results
 * as real addresses rather than offsets from a known-null pointer. */
static uint8_t *volatile phys_base = 0;

struct field_check {
    const char *name;
    uint16_t seg, off;
    uint16_t expected;
    int is_byte;
};

static int run_sup_init(void) {
    uint8_t *phys = phys_base;
    uint8_t *sysdat = far_ptr(phys, SYSDAT_SEG, 0);
    uint16_t uda_seg = (uint16_t)(SYSDAT_SEG + (SYSDAT_INITUDA >> 4));

    for (unsigned i = 0; i < SYSDAT_LEN; i++) {
        sysdat[i] = 0;
    }
    wr16(sysdat + SYSDAT_SUPMOD + 2, SUP_CS);
    wr16(sysdat + SYSDAT_RSPSEG, 0);

    sup_init(phys, SYSDAT_SEG, SUP_CS, SUP_NOTICK, SUP_IDLE, SUP_USER_ENTRY);

    const struct field_check checks[] = {
        {"cs:sysdat", SUP_CS, INIT_SYSDAT, SYSDAT_SEG, 0},
        {"cs:supervisor seg", SUP_CS, INIT_SUPERVISOR_SEG, SUP_CS, 0},
        {"version", SYSDAT_SEG, SYSDAT_VERSION, INIT_VER, 0},
        {"tick_tos", SYSDAT_SEG, SYSDAT_TICK_TOS, SUP_NOTICK, 0},
        {"idle_tos", SYSDAT_SEG, SYSDAT_IDLE_TOS, SUP_IDLE, 0},
        {"i_mpm_ip", 0, i_mpm_ip, SUP_USER_ENTRY, 0},
        {"i_mpm_cs", 0, i_mpm_cs, SUP_CS, 0},
        {"initpd.p_uda", SYSDAT_SEG, SYSDAT_INITPD + p_uda, uda_seg, 0},
        {"inituda.u_wrkseg", uda_seg, u_wrkseg, SYSDAT_SEG, 0},
        {"intflag", SYSDAT_SEG, SYSDAT_INTFLAG, 0xFF, 1},
        {"rspseg", SYSDAT_SEG, SYSDAT_RSPSEG, 0, 0},
        {"SYSDAT 2Eh untouched", SYSDAT_SEG, SYSDAT_XIOS_INIT_SEG, 0, 0},
    };

    int failures = 0;
    for (unsigned i = 0; i < sizeof checks / sizeof checks[0]; i++) {
        const uint8_t *p = far_ptr(phys, checks[i].seg, checks[i].off);
        uint16_t got = checks[i].is_byte ? *p : rd16(p);
        if (got != checks[i].expected) {
            puts_con("sup_init: FAIL ");
            puts_con(checks[i].name);
            puts_con(" = ");
            puthex16(got);
            puts_con(", expected ");
            puthex16(checks[i].expected);
            puts_con("\r\n");
            failures++;
        }
    }
    return failures;
}

void kernel_main(void) {
    unsigned nrsps;
    const gensys_rsp *rsps = gensys_rsps(&nrsps);
    SYSDAT_SEG = gensys(phys_base, rsps, nrsps).sysdat;
    init_entry(phys_base, SYSDAT_SEG, inb, outb);
    puts_con("MP/M-86 src-c image-builder: pipeline ok\r\n");

    if (run_sup_init() == 0) {
        puts_con("sup_init: ok (sysdat ");
        puthex16(SYSDAT_SEG);
        puts_con(", init uda ");
        puthex16((uint16_t)(SYSDAT_SEG + (SYSDAT_INITUDA >> 4)));
        puts_con(")\r\n");
    }

    /* then run MP/M-86 itself on the C kernel (src-c/rt/rt.c) */
    rt_boot();
}
