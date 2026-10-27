/*
 * rt.c -- runs the src-c kernel (src-c/kernel/, the C port of the
 * MP/M-86 kernel) as a real multitasking MP/M-86 in kernel.elf, with
 * programs written in C (src-c/rsp/) as its processes.
 *
 * The port's routines are faithful translations, each checked against
 * the real asm; what they can't do on their own is everything at the
 * port's boundaries (docs/src-c.md): a call into another module, the
 * dispatcher's actual switch to another process, the XIOS's hardware
 * interrupt. This file supplies those, as the real hardware and the
 * asm's own glue do:
 *
 *   - Memory: src-c/gensys builds MP/M's low-memory image where the
 *     real boot puts it -- the modules' code segments from 1000h, the
 *     RSPs, SYSDAT after them with its tables, pools and the RSP chain
 *     -- from src-c's own data (the same bytes as src-assem's build,
 *     tests/src-c/test_gensys.py). The kernel works on it through
 *     far_ptr(phys, seg, off) with phys = 0.
 *   - System calls: rt_int224 is SUPIF.SUP's user_entry (u_wrkseg = the
 *     caller's DS, u_retseg = its ES, u_insys), then `func`: func1
 *     (supif.c) resolves the function, and the router below plays
 *     `gofar`'s far call through the module table, calling the module's
 *     ported entry (rtmif_entry, memif_entry, cioif_entry,
 *     bdosif_entry, or the XIOS). Calls between modules -- `call mpmif`,
 *     a boundary hook in each file that makes one (defs.h) -- go
 *     through rt_mpmif, SUPIF.SUP's mpmif (u_wrkseg = the caller's DS
 *     for the call, restored after).
 *   - The RTM entries' own tail jumps into the dispatcher (`jmp dsptch`
 *     after poll/delay/sleep/terminate, `jmp pdisp` after dispatch/
 *     setprior/wakeup) are made here after the entry returns; the ones
 *     in the middle of a routine (flag wait, queue waits) are hooks.
 *   - Processes: each process runs on its own C stack. dsptch.c calls
 *     the switch hook at the end of every dispatch; if the dispatcher
 *     chose another process, rt_switch saves this stack and resumes
 *     that one's -- the asm's `mov ss,u_ss ! mov sp,u_sp` -- so a
 *     process blocked in the kernel resumes inside the call that
 *     blocked it, as in MP/M. A process the dispatcher reaches for the
 *     first time starts in the C program named like it (rt.h), or for
 *     TICK and IDLE in their SUP code's C form below.
 *   - Interrupts: kernel code runs with interrupts off (the port has no
 *     cli/sti of its own, so nothing may preempt it mid-update);
 *     processes run with them on. The PIT's IRQ0 runs XIOS86Q's tick
 *     ISR -- the tick countdown (tickint_once), the second/tick flag
 *     sets, EOI -- then enters the dispatcher at fdisp, as the ISR's
 *     `jmpf mydispatch` does.
 *
 * The XIOS is xios86q.c on the real UART/PIT/PIC/FDC, reached through
 * rt_xiosmod -- SYSDAT's xiosmod far pointer -- so CACHE can hook it.
 */
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "rt_syms.h"
#include "gensys.h"

/* -- the kernel's entry points, and their result layouts ---------------- */

typedef struct {
    uint8_t mod, func;
    uint16_t bx, cx;
    uint8_t reached_xios, al, ah, cl, dl;
} sup_func1_result;
extern sup_func1_result func1(uint8_t *phys, uint16_t sysdat, uint8_t ch, uint8_t cl, uint16_t dx, uint16_t bx,
                              uint16_t user_retf_offset, uint16_t cs);

typedef struct { uint16_t ax, bx, cx, dx, si, di, bp; } rtm_result;
extern rtm_result rtmif_entry(uint8_t *phys, uint8_t cl, uint16_t dx, uint16_t ax, uint16_t bx, uint16_t cx,
                              uint16_t si, uint16_t di, uint16_t bp, uint16_t ds, uint16_t sysdat,
                              uint8_t *was_already_indisp);
typedef struct { uint16_t bx, cx; } mem_result;
extern mem_result memif_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl, uint16_t dx, uint16_t bx);
typedef struct { uint16_t ax, bx, cx; } cio_result;
extern cio_result cioif_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl, uint16_t dx, uint8_t bh, uint8_t bl);
typedef struct { uint16_t bx, cx; } bdos_result;
extern bdos_result bdosif_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl, uint16_t dx, uint16_t bx);

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp;
    uint16_t new_ss, new_sp;
    uint8_t went_to_int_disp_exit;
} switch_result;
#define DISPATCH_ARGS uint8_t *phys, uint16_t sysdat, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, \
    uint16_t si, uint16_t di, uint16_t bp, uint8_t *was_already_indisp
extern switch_result dsptch(DISPATCH_ARGS);
extern switch_result pdisp(DISPATCH_ARGS);
extern switch_result fdisp(DISPATCH_ARGS);

extern void sup_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs, uint16_t notick_offset,
                     uint16_t idle_offset, uint16_t user_entry_offset);
extern void rtmif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs, uint16_t fdisp_offset,
                       uint16_t farpdisp_offset);
extern void memif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs);
extern void bdosif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs);
extern void cioif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs);

extern void notick_once(uint8_t *phys, uint16_t sysdat);
extern void tick_once(uint8_t *phys, uint16_t sysdat);

/* xios86q.c */
typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);
extern void init_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8);
extern uint8_t polldev_entry(io_in8_fn io_in8, uint8_t cl);
extern uint8_t const_entry(io_in8_fn io_in8);
extern uint8_t xios_conin_entry(io_in8_fn io_in8, io_out8_fn io_out8);
extern void xios_conout_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl);
extern void plist_entry(io_in8_fn io_in8, io_out8_fn io_out8, uint8_t cl);
extern uint8_t punch_entry(void);
extern uint8_t reader_entry(void);
extern uint8_t listst_entry(io_in8_fn io_in8);
extern uint16_t maxconsole_entry(void);
extern uint16_t maxlist_entry(void);
extern void disk_home_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8);
extern uint16_t seldsk_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl);
extern void settrk_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl);
extern void setsec_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl);
extern void setdma_entry(uint8_t *phys, uint16_t sysdat, uint16_t cx);
extern void setdmab_entry(uint8_t *phys, uint16_t sysdat, uint16_t cx);
extern uint16_t sectran_entry(uint8_t *phys, uint16_t sysdat, uint16_t cx, uint16_t dx);
extern uint8_t diskread_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8);
extern uint8_t diskwrite_entry(uint8_t *phys, uint16_t sysdat, io_in8_fn io_in8, io_out8_fn io_out8);
extern uint16_t getsegt_entry(void);
extern void selmemory_entry(void);
extern uint8_t flushbuf_entry(void);
extern void strtclk_entry(uint8_t *phys, uint16_t sysdat);
extern void stopclk_entry(uint8_t *phys, uint16_t sysdat);
typedef struct { uint8_t sec_flag_set, tick_flag_set; } tickint_result;
extern tickint_result tickint_once(uint8_t *phys, uint16_t sysdat);

/* the boundary hooks' setters (defs.h) */
extern void dsptch_set_mpmif(mpmif_fn fn);
extern void dsptch_set_xiosif(xiosif_fn fn);
typedef void (*dsptch_switch_fn)(uint8_t *phys, uint16_t sysdat, uint16_t from);
extern void dsptch_set_switch(dsptch_switch_fn fn);
extern void flag_set_dsptch(dsptch_fn fn);
extern void que1_set_mpmif(mpmif_fn fn);
extern void que2_set_mpmif(mpmif_fn fn);
extern void console_set_mpmif(mpmif_fn fn);
extern void chario_set_mpmif(mpmif_fn fn);
extern void memif_set_mpmif(mpmif_fn fn);
extern void init_set_mpmif(mpmif_fn fn);
extern void abort_set_mpmif(mpmif_fn fn);
extern void command_set_mpmif(mpmif_fn fn);
extern void init_set_xiosif(xiosif_fn fn);
typedef void (*init_module_fn)(uint8_t *phys, uint16_t sysdat, uint8_t mod);
extern void init_set_module_init(init_module_fn fn);
typedef void (*xios86q_poll_fn)(uint8_t device);
extern void xios86q_set_poll(xios86q_poll_fn fn);

/* rt_asm.s */
extern void rt_ctx_switch(uint32_t *save_esp, uint32_t new_esp);
extern void rt_ctx_start(void);
extern void rt_irq0_stub(void);
extern void rt_spurious_stub(void);

/* -- machine ------------------------------------------------------------ */

#define MPM_CODE_SEG GENSYS_CODE_SEG
#define tick_flag 1         /* XIOS86Q.A86 */
#define sec_flag  2

static uint8_t *volatile phys_base = 0;
static uint16_t sysdat_seg;

uint8_t *rt_phys(void) { return phys_base; }
uint16_t rt_sysdat(void) { return sysdat_seg; }

static void outb(uint16_t port, uint8_t val) { __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint32_t irq_save(void) {
    uint32_t fl;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(fl) : : "memory");
    return fl;
}
static void irq_restore(uint32_t fl) {
    if (fl & 0x200) __asm__ volatile("sti" : : : "memory");
}

void rt_puts(const char *s) {
    for (; *s; s++) {
        while (!(inb(0x3FD) & 0x20)) {
        }
        outb(0x3F8, (uint8_t)*s);
    }
}

void rt_puthex(uint16_t v) {
    char b[5];
    for (int i = 0; i < 4; i++) b[i] = "0123456789ABCDEF"[(v >> (12 - 4 * i)) & 0xF];
    b[4] = 0;
    rt_puts(b);
}

static uint8_t *sd(void) { return far_ptr(phys_base, sysdat_seg, 0); }
static uint16_t rlr(void) { return seg_r16(sd(), SYSDAT_RLR); }
static uint8_t *rlr_uda_p(void) { return far_ptr(phys_base, rlr_uda(sd()), 0); }
/* a module's code segment: its module_table entry's segment word */
static uint16_t mod_cs(uint8_t mod) { return seg_r16(sd(), (uint16_t)((mod - 1) * 8 + 2)); }

/* -- the XIOS ----------------------------------------------------------- */

static uint16_t xios86q(uint8_t *phys, uint16_t sysdat, uint8_t al, uint16_t cx, uint16_t dx) {
    switch (al) {
    case io_const: return const_entry(inb);
    case io_conin: return xios_conin_entry(inb, outb);
    case io_conout: xios_conout_entry(inb, outb, (uint8_t)cx); return 0;
    case io_list: plist_entry(inb, outb, (uint8_t)cx); return 0;
    case 4: return punch_entry();
    case io_reader: return reader_entry();
    case io_home: disk_home_entry(phys, sysdat, inb, outb); return 0;
    case io_seldsk: return seldsk_entry(phys, sysdat, (uint8_t)cx);
    case io_settrk: settrk_entry(phys, sysdat, (uint8_t)cx); return 0;
    case io_setsec: setsec_entry(phys, sysdat, (uint8_t)cx); return 0;
    case io_setdma: setdma_entry(phys, sysdat, cx); return 0;
    case io_read: return diskread_entry(phys, sysdat, inb, outb);
    case io_write: return diskwrite_entry(phys, sysdat, inb, outb);
    case io_listst: return listst_entry(inb);
    case io_sectran: return sectran_entry(phys, sysdat, cx, dx);
    case io_setdmab: setdmab_entry(phys, sysdat, cx); return 0;
    case io_getsegt: return getsegt_entry();
    case io_polldev: return polldev_entry(inb, (uint8_t)cx);
    case io_strtclk: strtclk_entry(phys, sysdat); return 0;
    case io_stopclk: stopclk_entry(phys, sysdat); return 0;
    case io_maxconsole: return maxconsole_entry();
    case io_maxlist: return maxlist_entry();
    case io_selmemory: selmemory_entry(); return 0;
    case io_flush: return flushbuf_entry();
    default: return 0;
    }
}

rt_xios_fn rt_xiosmod = xios86q;

static uint16_t rt_xiosif(uint8_t *phys, uint16_t sysdat, uint8_t al, uint16_t cx, uint16_t dx) {
    return rt_xiosmod(phys, sysdat, al, cx, dx);
}

/* chario.c's XIOS hook: AL, CL, DL */
static uint16_t rt_cio_xios(uint8_t func, uint8_t cl, uint8_t dl) {
    return rt_xiosmod(phys_base, sysdat_seg, func, cl, dl);
}

/* -- dispatching ---------------------------------------------------------- */

static void rt_dispatch(int pdisp_) {
    uint8_t ind;
    if (pdisp_) {
        (void)pdisp(phys_base, sysdat_seg, 0, 0, 0, 0, 0, 0, 0, &ind);
    } else {
        (void)dsptch(phys_base, sysdat_seg, 0, 0, 0, 0, 0, 0, 0, &ind);
    }
}

static void rt_dsptch_hook(uint8_t *phys, uint16_t sysdat, int pdisp_) {
    (void)phys;
    (void)sysdat;
    rt_dispatch(pdisp_);
}

/* -- function dispatch: SUPIF.SUP's func/func1/gofar ----------------------- */

static mpm_regs rt_bdos(uint8_t fn, uint16_t dx, uint16_t bx) {
    mpm_regs out = {0, 0, 0, 0, 0};
#ifdef RT_TRACE_BDOS
    rt_puts("{B");
    rt_puthex(fn);
    rt_puts(" ");
    rt_puthex(dx);
#endif
    bdos_result r = bdosif_entry(phys_base, sysdat_seg, fn, dx, bx);
#ifdef RT_TRACE_BDOS
    rt_puts("=");
    rt_puthex(r.bx);
    rt_puts("}");
#endif
    out.bx = r.bx;
    out.cx = r.cx;
    return out;
}

static void rt_mark_dying(void);

static mpm_regs rt_rtm(uint8_t fn, uint16_t dx, uint16_t bx) {
    mpm_regs out = {0, 0, 0, 0, 0};
    uint8_t ind = 0;
    /* wakeup_entry calls pdisp only when it took a process off the list */
    int woke = (fn == 19) && seg_r16(sd(), dx) != 0;
    rtm_result r = rtmif_entry(phys_base, fn, dx, 0, bx, 0, 0, 0, 0, sysdat_seg, sysdat_seg, &ind);
    out.bx = r.bx;
    out.cx = r.cx;
    out.dx = r.dx;
    switch (fn) {
    case 1:  /* poll_entry:  mov bx,0 ! mov cx,bx ! jmp dsptch */
    case 11: /* delay_entry: likewise */
        out.bx = out.cx = 0;
        rt_dispatch(0);
        break;
    case 18: /* sleep_entry: jmp dsptch */
        rt_dispatch(0);
        break;
    case 12: /* dispatch_entry:  mov bx,0 ! mov cx,bx ! jmp pdisp */
    case 15: /* set_prior_entry: likewise */
        out.bx = out.cx = 0;
        rt_dispatch(1);
        break;
    case 19: /* wakeup_entry: call pdisp */
        if (woke) rt_dispatch(1);
        break;
    case 0:  /* sysreset_entry: terminate_entry with dx = 0 */
    case 13: /* terminate_entry: jmp dsptch, once it's set ps_term */
        if (seg_r8(sd(), (uint16_t)(rlr() + p_stat)) == ps_term) {
            rt_mark_dying();
            rt_dispatch(0);
        }
        break;
    case 17: /* abort_spec_entry: jmp pdisp on success */
        if (r.bx == 0 && r.cx == 0) rt_dispatch(1);
        break;
    default:
        break;
    }
    return out;
}

static mpm_regs rt_func(uint8_t ch, uint8_t cl, uint16_t dx, uint16_t bx) {
    mpm_regs out = {0, 0, 0, 0, 0};
    sup_func1_result f = func1(phys_base, sysdat_seg, ch, cl, dx, bx, SUP_USER_RETF, mod_cs(1));
    switch (f.mod) {
    case 1:
        out.bx = f.bx;
        out.cx = f.cx;
        break;
    case 2:
        out = rt_rtm(f.func, dx, bx);
        break;
    case 3: {
        mem_result r = memif_entry(phys_base, sysdat_seg, f.func, dx, bx);
        out.bx = r.bx;
        out.cx = r.cx;
        break;
    }
    case 4: {
        cio_result r = cioif_entry(phys_base, sysdat_seg, f.func, dx, (uint8_t)(bx >> 8), (uint8_t)bx);
        out.bx = r.bx;
        out.cx = r.cx;
        break;
    }
    case 5:
        out = rt_bdos(f.func, dx, bx);
        break;
    case 6:
        out.bx = rt_xiosmod(phys_base, sysdat_seg, f.func, bx, dx);
        break;
    default:
        out.bx = 0xFFFF;
        out.cx = e_not_implemented;
        break;
    }
    out.ax = out.bx;
    return out;
}

/* SUPIF.SUP's mpmif: u_wrkseg = the caller's DS for the call */
static mpm_regs rt_mpmif(uint8_t *phys, uint16_t sysdat, uint16_t cx, uint16_t dx, uint16_t bx, uint16_t ds) {
    (void)phys;
    (void)sysdat;
    uint8_t *uda = rlr_uda_p();
    uint16_t saved = seg_r16(uda, u_wrkseg);
    uint8_t saved_func = seg_r8(uda, u_func);
    seg_w16(uda, u_wrkseg, ds);
    seg_w8(uda, u_func, (uint8_t)cx); /* func: mov u_func,cl */
    mpm_regs r = rt_func((uint8_t)(cx >> 8), (uint8_t)cx, dx, bx);
    uda = rlr_uda_p();
    seg_w8(uda, u_func, saved_func);
    seg_w16(uda, u_wrkseg, saved);
    return r;
}

/* SUPIF.SUP's user_entry */
mpm_regs rt_int224(uint16_t ds, uint16_t es, uint8_t cl, uint16_t dx) {
    uint32_t fl = irq_save();
    uint8_t *uda = rlr_uda_p();
    uint8_t insys = seg_r8(uda, u_insys);
    uint16_t saved = seg_r16(uda, u_wrkseg); /* noswt: push u_wrkseg */
    seg_w8(uda, u_insys, (uint8_t)(insys + 1));
    seg_w16(uda, u_retseg, es);
    seg_w16(uda, u_wrkseg, ds);
    seg_w8(uda, u_func, cl); /* func: mov u_func,cl */
    mpm_regs r = rt_func(0, cl, dx, rlr()); /* BX -> pd address */
    uda = rlr_uda_p();
    r.ax = r.bx;
    r.es = seg_r16(uda, u_retseg);
    seg_w8(uda, u_insys, (uint8_t)(seg_r8(uda, u_insys) - 1));
    if (insys != 0) seg_w16(uda, u_wrkseg, saved);
    irq_restore(fl);
    return r;
}

/* command.c's f_fopen/f_fclose and load.c's f_freadseq: the CLI and
 * LOAD call the BDOS through the Supervisor's mpmif, DS = the FCB's
 * segment */
static uint16_t rt_cli_bdos(uint8_t *phys, uint8_t func, uint16_t fcb_seg, uint16_t fcb_off) {
    return rt_mpmif(phys, sysdat_seg, func, fcb_off, 0, fcb_seg).bx;
}

static uint8_t rt_load_readseq(uint8_t *phys, uint16_t fcb_seg, uint16_t fcb_off, uint16_t dma_seg,
                               uint16_t dma_off) {
    (void)dma_seg; /* LOAD has set them (f_setdmab, f_setdma) */
    (void)dma_off;
    return (uint8_t)rt_mpmif(phys, sysdat_seg, f_freadseq, fcb_off, 0, fcb_seg).bx;
}

/* XIOS86Q's rtm_poll tail: f_polldev through the Supervisor (DS =
 * SYSDAT), unless stoppoll is set -- then it just polls again */
static void rt_xios_poll(uint8_t device) {
    if (seg_r16(sd(), XQ_STOPPOLL) == 0xFFFF) return;
    (void)rt_mpmif(phys_base, sysdat_seg, f_polldev, device, 0, sysdat_seg);
}

/* -- processes -------------------------------------------------------------- */

#define RT_NCTX  24
#define RT_STACK 16384

struct ctx {
    uint16_t pd;
    uint8_t live;
    uint8_t dying; /* terminating: never resumed once switched away from */
    uint32_t esp;
    uint8_t stack[RT_STACK] __attribute__((aligned(16)));
};
static struct ctx ctxs[RT_NCTX];
static struct ctx boot_ctx; /* the init process: runs on boot.s's stack */
static struct ctx *cur;

/* the running process is terminating (rt_rtm) */
static void rt_mark_dying(void) { cur->dying = 1; }

static struct ctx *ctx_find(uint16_t pd) {
    for (unsigned i = 0; i < RT_NCTX; i++) {
        if (ctxs[i].live && ctxs[i].pd == pd) return &ctxs[i];
    }
    return 0;
}

static struct ctx *ctx_new(uint16_t pd) {
    for (unsigned i = 0; i < RT_NCTX; i++) {
        struct ctx *c = &ctxs[i];
        if (c->live) continue;
        c->live = 1;
        c->pd = pd;
        /* "resume" into rt_ctx_start with EBX = pd and IF off (rt_ctx_switch's layout) */
        uint32_t *sp = (uint32_t *)(c->stack + RT_STACK);
        *--sp = (uint32_t)rt_ctx_start;
        *--sp = 0x002;  /* eflags */
        *--sp = 0;      /* ebp */
        *--sp = pd;     /* ebx */
        *--sp = 0;      /* esi */
        *--sp = 0;      /* edi */
        c->esp = (uint32_t)sp;
        return c;
    }
    rt_puts("\r\nrt: out of process contexts\r\n");
    for (;;) __asm__ volatile("cli; hlt");
}

/* dsptch.c's switch hook: the dispatcher has chosen rlr */
static void rt_switch(uint8_t *phys, uint16_t sysdat, uint16_t from) {
    (void)phys;
    (void)sysdat;
    uint16_t to = rlr();
    if (cur->pd == to) return;
    if (cur->pd != from) {
        rt_puts("\r\nrt: dispatch from ");
        rt_puthex(from);
        rt_puts(" while running ");
        rt_puthex(cur->pd);
        rt_puts("\r\n");
    }
    struct ctx *next = ctx_find(to);
    if (!next) next = ctx_new(to);
#ifdef RT_TRACE
    rt_puts("[");
    rt_puthex(from);
    rt_puts(">");
    rt_puthex(to);
    rt_puts("]");
#endif
    struct ctx *prev = cur;
    uint32_t discard;
    uint32_t *save = &prev->esp;
    if (prev->dying || seg_r8(sd(), (uint16_t)(from + p_stat)) == ps_term) {
        /* terminated: never resumed (next is already allocated). The
         * dispatcher's term_act has freed the PD by now, so its p_stat
         * may not say ps_term any more: terminate marks the context. */
        prev->live = 0;
        prev->dying = 0;
        save = &discard;
    }
    cur = next;
    rt_ctx_switch(save, next->esp);
}

static int name_is(const uint8_t *pdname, const char *name) {
    for (int i = 0; i < pnamsiz; i++) {
        if ((pdname[i] & 0x7F) != (uint8_t)name[i]) return 0;
    }
    return 1;
}

/* TICK.SUP: the tick process */
static void rt_tick_main(uint16_t ds) {
    for (;;) {
        /* notick: no one on the delay list */
        uint32_t fl = irq_save();
        notick_once(phys_base, sysdat_seg);
        (void)rt_xiosmod(phys_base, sysdat_seg, io_stopclk, 0, 0);
        irq_restore(fl);
        for (;;) {
            /* tick: mov dx,flag_tick ! mov cx,f_flagwait ! int mpmint */
            (void)rt_mpm(ds, (uint8_t)f_flagwait, tick_flag);
            fl = irq_save();
            tick_once(phys_base, sysdat_seg);
            uint16_t dlr = seg_r16(sd(), SYSDAT_DLR);
            irq_restore(fl);
            if (dlr == 0) break; /* drl_empty: notick */
        }
    }
}

/* XIOS86Q's idle (IDLE.SUP's `jmp xiosif` with io_idle): dispatch, forever */
static void rt_idle_main(uint16_t ds) {
    for (;;) {
        (void)rt_mpm(ds, (uint8_t)f_dispatch, 0);
    }
}

static void rt_noprog_main(uint16_t ds) {
    const uint8_t *name = far_ptr(phys_base, sysdat_seg, (uint16_t)(rlr() + p_name));
    char buf[pnamsiz + 1];
    for (int i = 0; i < pnamsiz; i++) buf[i] = (char)(name[i] & 0x7F);
    buf[pnamsiz] = 0;
    rt_puts("\r\nrt: no C program for process ");
    rt_puts(buf);
    rt_puts("\r\n");
    (void)ds;
}

/* rt_ctx_start's target: the process's first instructions */
void rt_proc_main(uint32_t pd) {
    uint16_t p = (uint16_t)pd;
    rt_main_fn main = rt_noprog_main;
    if (p == SYSDAT_TICKPD) {
        main = rt_tick_main;
    } else if (p == SYSDAT_IDLEPD) {
        main = rt_idle_main;
    } else {
        const uint8_t *name = far_ptr(phys_base, sysdat_seg, (uint16_t)(p + p_name));
        int sys = (seg_r16(far_ptr(phys_base, sysdat_seg, 0), (uint16_t)(p + p_flag)) & pf_sys) != 0;
        for (const rt_program *pr = rt_programs; pr->name; pr++) {
            if (pr->kind == (sys ? rt_transient_only : rt_rsp_only)) continue;
            if (name_is(name, pr->name)) {
                main = pr->main;
                break;
            }
        }
    }
    uint16_t ds = seg_r16(rlr_uda_p(), u_initds);
    __asm__ volatile("sti");
    main(ds);
    for (;;) {
        (void)rt_mpm(ds, (uint8_t)f_terminate, 0);
    }
}

/* -- interrupts ---------------------------------------------------------------- */

struct idt_entry {
    uint16_t off_lo, sel;
    uint8_t zero, type;
    uint16_t off_hi;
} __attribute__((packed));
static struct idt_entry idt[256];

static void idt_set(int n, void (*fn)(void)) {
    uint32_t a = (uint32_t)fn;
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    idt[n].off_lo = (uint16_t)a;
    idt[n].sel = cs;
    idt[n].zero = 0;
    idt[n].type = 0x8E; /* present, ring 0, 32-bit interrupt gate */
    idt[n].off_hi = (uint16_t)(a >> 16);
}

#define FAULT_DECL(n) extern void rt_fault_##n(void);
FAULT_DECL(0) FAULT_DECL(1) FAULT_DECL(2) FAULT_DECL(3) FAULT_DECL(4) FAULT_DECL(5) FAULT_DECL(6)
FAULT_DECL(7) FAULT_DECL(8) FAULT_DECL(9) FAULT_DECL(10) FAULT_DECL(11) FAULT_DECL(12) FAULT_DECL(13)
FAULT_DECL(14) FAULT_DECL(15) FAULT_DECL(16) FAULT_DECL(17) FAULT_DECL(18) FAULT_DECL(19)

static void idt_init(void) {
    void (*faults[20])(void) = {rt_fault_0, rt_fault_1, rt_fault_2, rt_fault_3, rt_fault_4, rt_fault_5,
                                rt_fault_6, rt_fault_7, rt_fault_8, rt_fault_9, rt_fault_10, rt_fault_11,
                                rt_fault_12, rt_fault_13, rt_fault_14, rt_fault_15, rt_fault_16,
                                rt_fault_17, rt_fault_18, rt_fault_19};
    for (int i = 0; i < 20; i++) idt_set(i, faults[i]);
    for (int i = 0x21; i < 0x30; i++) idt_set(i, rt_spurious_stub);
    idt_set(0x20, rt_irq0_stub);
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idtr = {sizeof idt - 1, (uint32_t)idt};
    __asm__ volatile("lidt %0" : : "m"(idtr));

    /* remap the 8259s to 20h/28h (the BIOS left IRQ0 on vector 8, a CPU
     * exception in protected mode), everything masked; XIOS86Q's init
     * unmasks IRQ0 itself */
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFF); outb(0xA1, 0xFF);
}

struct fault_frame {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vec, err, eip, cs, eflags;
};

void rt_fault(struct fault_frame *f) {
    rt_puts("\r\nrt: CPU exception ");
    rt_puthex((uint16_t)f->vec);
    rt_puts(" err ");
    rt_puthex((uint16_t)f->err);
    rt_puts(" eip ");
    rt_puthex((uint16_t)(f->eip >> 16));
    rt_puthex((uint16_t)f->eip);
    rt_puts(" in pd ");
    rt_puthex(cur ? cur->pd : 0);
    rt_puts("\r\n");
}

/* XIOS86Q's tickint, then `jmpf mydispatch` (fdisp) */
void rt_irq0(void) {
    tickint_result t = tickint_once(phys_base, sysdat_seg);
    /* mov dx,sec_flag/tick_flag ! mov cx,f_flagset ! call supif -- the
     * SUP entry makes it f_inflagset */
    if (t.sec_flag_set) (void)rt_func((uint8_t)(f_inflagset >> 8), (uint8_t)f_inflagset, sec_flag, 0);
    if (t.tick_flag_set) (void)rt_func((uint8_t)(f_inflagset >> 8), (uint8_t)f_inflagset, tick_flag, 0);
    outb(0x20, 0x20); /* eoi */
    uint8_t ind;
    (void)fdisp(phys_base, sysdat_seg, 0, 0, 0, 0, 0, 0, 0, &ind);
}

/* -- boot ------------------------------------------------------------------------ */

/* INIT.SUP's `callf dword ptr <mod>mod[bx]`: each module's own init */
static void rt_module_init(uint8_t *phys, uint16_t sysdat, uint8_t mod) {
    switch (mod) {
    case 2: rtmif_init(phys, sysdat, mod_cs(2), RTM_FDISP, RTM_FARPDISP); break;
    case 3: memif_init(phys, sysdat, mod_cs(3)); break;
    case 4: cioif_init(phys, sysdat, mod_cs(4)); break;
    case 5: bdosif_init(phys, sysdat, mod_cs(5)); break;
    case 6: init_entry(phys, sysdat, inb, outb); break;
    default: break;
    }
}

void rt_boot(void) {
    unsigned nrsps;
    const gensys_rsp *rsps = gensys_rsps(&nrsps);
    gensys_layout layout = gensys(phys_base, rsps, nrsps);
    sysdat_seg = layout.sysdat;

    rt_puts("\r\nrt: MP/M-86 on the C kernel: code at 1000, SYSDAT at ");
    rt_puthex(sysdat_seg);
    rt_puts(", RSPs");
    for (unsigned i = 0; i < nrsps; i++) {
        char name[pnamsiz + 1];
        unsigned n = 0;
        for (; n < pnamsiz && rsps[i].name[n] != ' '; n++) name[n] = rsps[i].name[n];
        name[n] = 0;
        rt_puts(" ");
        rt_puts(name);
    }
    rt_puts("\r\n");

    idt_init();

    dsptch_set_mpmif(rt_mpmif);
    dsptch_set_xiosif(rt_xiosif);
    dsptch_set_switch(rt_switch);
    flag_set_dsptch(rt_dsptch_hook);
    que1_set_mpmif(rt_mpmif);
    que2_set_mpmif(rt_mpmif);
    console_set_mpmif(rt_mpmif);
    chario_set_mpmif(rt_mpmif);
    memif_set_mpmif(rt_mpmif);
    init_set_mpmif(rt_mpmif);
    init_set_xiosif(rt_xiosif);
    init_set_module_init(rt_module_init);
    cio_set_xios(rt_cio_xios);
    bdos_set_mpmif(rt_mpmif);
    bdos_set_xiosif(rt_xiosif);
    abort_set_mpmif(rt_mpmif);
    command_set_mpmif(rt_mpmif);
    cli_set_bdos(rt_cli_bdos);
    load_set_readseq(rt_load_readseq);
    xios86q_set_poll(rt_xios_poll);

    boot_ctx.pd = rlr(); /* the init process, as SYSDAT starts it */
    boot_ctx.live = 1;
    cur = &boot_ctx;

    /* INIT.SUP's init: the module inits, the RSPs, then terminate */
    sup_init(phys_base, sysdat_seg, MPM_CODE_SEG, SUP_NOTICK, SUP_IDLE, SUP_USER_ENTRY);

    rt_puts("\r\nrt: init process returned\r\n");
    for (;;) __asm__ volatile("cli; hlt");
}
