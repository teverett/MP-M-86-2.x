/*
 * stscom.c -- C version of src-assem/utils/STSCOM.PLM (stscom.h): the
 * MPMSTAT status display, shared by the transient (stscmd.c) and the
 * RSP.
 *
 * Every structure is reached as the PL/M reaches it: through one of
 * STSCOM's far pointers (pd$pointer, qd$pointer, ...), whose offset the
 * code sets and whose segment the main program set to SYSDAT's -- in
 * the program's own data, so the pointers and link$list hold what the
 * PL/M's do. PL/M's semantics are kept as in dir.c: bytes wrap, a
 * relation is 0FFh or 0, IF tests bit 0 (STSCOM's `true` is 0FFh), AND
 * evaluates both sides. display$links and display$memory copy a list
 * into link$list with interrupts off (PL/M's `disable`), then dispatch
 * (function 142) to turn them back on; `enable` comes at the end of the
 * listing, and not at all on the early return for an empty list, as in
 * the PL/M.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "stscom.h"

#define T 0xFF /* STSCOM's true (comlit.lit) */
#define F 0
#define screenwidth 80
#define pnamsiz 8
#define dummy 0
#define dispatch 142
#define qf_mx 0x01

/* the structures' fields (proces.lit, qd.lit, mdsat.lit, flag.lit,
 * ccb.lit, sd.lit) */
#define pd_link   0x00
#define pd_thread 0x02
#define pd_name   0x08
#define pd_mem    0x16
#define pd_cns    0x20
#define qd_link   0x00
#define qd_flags  0x04
#define qd_name   0x06
#define qd_dq     0x12
#define qd_nq     0x14
#define qd_buffer 0x1A
#define md_link   0x00
#define md_start  0x02
#define md_length 0x04
#define ms_mau_   0x08
#define sat_size  5 /* size(sat) */
#define sat_start_   0
#define sat_len      2
#define sat_numallocs 4
#define flag_size 3 /* size(flag) */
#define ccb_size  12 /* size(ccb) */
#define ccb_attach  0x00
#define ccb_queue   0x02
#define ccb_msource 0x09
#define sd_supmod 0x00
#define sd_ncns   0x47
#define sd_nlst   0x48
#define sd_nccb   0x49
#define sd_nflags 0x4A
#define sd_mmp    0x4C
#define sd_ccb    0x54
#define sd_flags  0x56
#define sd_mdul   0x58
#define sd_mfl    0x5A
#define sd_pul    0x5C
#define sd_qul    0x5E
#define sd_qmau   0x60
#define sd_rlr    0x68
#define sd_dlr    0x6A
#define sd_plr    0x6E
#define sd_thrdrt 0x72
#define sd_qlr    0x74

static uint16_t g_ds;
static stscom_vars *v;
static uint16_t s_strs;

#define S(f) ((uint16_t)(s_strs + offsetof(stscom_strs, f)))

void stscom_init(uint16_t ds, stscom_vars *vars, uint16_t strs) {
    g_ds = ds;
    v = vars;
    s_strs = strs;
}

static uint16_t mon3(uint8_t func, uint16_t info) { return rt_mpm(g_ds, func, info).bx; }
static uint8_t mon2(uint8_t func, uint16_t info) { return (uint8_t)mon3(func, info); }
static void mon1(uint8_t func, uint16_t info) { (void)mon3(func, info); }
void stscom_mon1(uint8_t func, uint16_t info) { mon1(func, info); }

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

/* PL/M's disable and enable */
static void disable(void) {
#if defined(__i386__) /* kernel.elf; the host tests have no interrupts */
    __asm__ volatile("cli" : : : "memory");
#endif
}
static void enable(void) {
#if defined(__i386__)
    __asm__ volatile("sti" : : : "memory");
#endif
}

/* the byte at a based structure's field: ptr is one of the far pointers */
static uint8_t *at(const stscom_ptr *p, uint16_t field) {
    return far_ptr(rt_phys(), p->segment, (uint16_t)(p->offset + field));
}
static uint8_t b_(const stscom_ptr *p, uint16_t field) { return *at(p, field); }
static uint16_t w_(const stscom_ptr *p, uint16_t field) { return rd16(at(p, field)); }
#define sd_b(f) b_(&v->sysdat_ptr, (f))
#define sd_w(f) w_(&v->sysdat_ptr, (f))

/* BDOS Calls */

void stscom_reboot(void) { mon1(0, 0); }
uint8_t stscom_conin(void) { return mon2(1, 0); }
void stscom_co(uint8_t c) { mon1(2, c); }
void stscom_print_buffer(uint16_t a) { mon1(9, a); }
uint16_t stscom_get_version(void) { return mon3(12, 0); }
void stscom_terminate(void) { mon1(143, 0); }
void stscom_get_sysdat(stscom_ptr *p) {
    mpm_regs r = rt_mpm(g_ds, 154, 0); /* mon4: ES:BX */
    p->offset = r.bx;
    p->segment = r.es;
}

#define co stscom_co
#define print_buffer stscom_print_buffer

/* utility functions */

void stscom_crlf(void) {
    co(0x0D);
    co(0x0A);
}
#define crlf stscom_crlf

void stscom_display_hex_byte(uint8_t value) {
    co((uint8_t)v->hex_digit[value >> 4]);
    co((uint8_t)v->hex_digit[value % 16]);
}

void stscom_display_hex_word(uint16_t value) {
    stscom_display_hex_byte((uint8_t)(value >> 8));
    stscom_display_hex_byte((uint8_t)value);
}

/* does byte at a time console write from possibly another segment */
void stscom_display_text(uint8_t count, uint16_t off, uint16_t seg) {
    stscom_ptr src = {off, seg};
    uint8_t i;
    if (rel(count == 0) & 1) {
        while (rel(b_(&src, 0) != '$') & 1) {
            co(b_(&src, 0) & 0x7F);
            src.offset = (uint16_t)(src.offset + 1);
        }
    } else {
        for (i = 1; i <= count; i++) {
            co(b_(&src, 0) & 0x7F);
            src.offset = (uint16_t)(src.offset + 1);
        }
    }
}

/* Print linked list of PDs starting with root$offset. Print title if
 * not dummy; display console number if dis$con = true. First line
 * starts on column init$col. */
static void display_links(uint16_t title_adr, uint16_t root_offset, uint8_t dis_con, uint8_t init_col) {
    uint8_t col; /* column number relative to 1 */
    uint8_t n, k;
    if (rel(title_adr != dummy) & 1) {
        crlf();
        print_buffer(title_adr);
        col = screenwidth + 1; /* start new line */
    } else {
        col = init_col; /* initial column position from calling procedure */
    }
    n = 0xFF; /* -1 */
    disable(); /* critical section required to obtain list */
    v->pd_ptr.offset = root_offset;
    while ((rel(v->pd_ptr.offset != 0) & rel(n != 63)) & 1) {
        v->link_list[n = (uint8_t)(n + 1)] = v->pd_ptr.offset;
        v->pd_ptr.offset = w_(&v->pd_ptr, pd_link);
    }
    mon1(dispatch, 0); /* enable interrupts by dispatching */
    if (rel(n == 0xFF) & 1) return;
    for (k = 0; k <= n; k++) {
        v->pd_ptr.offset = v->link_list[k];
        if (rel(col > screenwidth - 13) & 1) { /* 13 chars could be used already */
            crlf();
            col = 1;
        }
        co(' '); /* 13 characters for pd */
        stscom_display_text(pnamsiz, (uint16_t)(v->pd_ptr.offset + pd_name), v->pd_ptr.segment);
        if ((rel(w_(&v->pd_ptr, pd_mem) != 0xFF) & dis_con) & 1) { /* display console number ? */
            co(v->lparen);
            stscom_display_hex_byte(b_(&v->pd_ptr, pd_cns)); /* prints 2 chars */
            co(v->rparen);
        } else {
            print_buffer(S(arr34)); /* not printing console # */
        }
        col = (uint8_t)(col + 13); /* but pad to make things line up */
    }
    enable();
}

/* count a free list linked through its first word */
static uint8_t count_list(stscom_ptr *p, uint16_t root) {
    uint8_t count = 0;
    p->offset = root;
    while (rel(p->offset != 0) & 1) {
        count = (uint8_t)(count + 1);
        p->offset = w_(p, 0); /* .link */
    }
    return count;
}

void stscom_display_config(void) {
    uint8_t count;
    uint16_t qsize;

    print_buffer(S(arr35)); /* Number of Physical Consoles = */
    stscom_display_hex_byte(sd_b(sd_ncns));
    print_buffer(S(arr36)); /* Number of Virtual Consoles = */
    stscom_display_hex_byte((uint8_t)(sd_b(sd_nccb) - (uint8_t)(sd_b(sd_nlst) + sd_b(sd_ncns))));
    print_buffer(S(arr37)); /* Number of List Devices = */
    stscom_display_hex_byte(sd_b(sd_nlst));

    print_buffer(S(arr38)); /* Number of Free Process Descriptors = */
    count = count_list(&v->pd_ptr, sd_w(sd_pul));
    stscom_display_hex_byte(count);

    print_buffer(S(arr42)); /* Number of Free Memory Descriptors = */
    count = count_list(&v->md_ptr, sd_w(sd_mdul));
    stscom_display_hex_byte(count);

    print_buffer(S(arr46)); /* Number of Free Queue Control Blocks = */
    count = count_list(&v->qd_ptr, sd_w(sd_qul));
    stscom_display_hex_byte(count);

    print_buffer(S(arr50)); /* Free Queue Buffer Area = */
    v->md_ptr.offset = (uint16_t)(v->sysdat_ptr.offset + sd_qmau); /* .sd.qmau(0) */
    v->sat_ptr.segment = w_(&v->md_ptr, md_start);
    v->sat_ptr.offset = sat_size;
    qsize = 0;
    while (rel(w_(&v->sat_ptr, sat_start_) != 0) & 1) { /* byte offset for q buffer area */
        if (rel(b_(&v->sat_ptr, sat_numallocs) == 0) & 1) qsize = (uint16_t)(qsize + w_(&v->sat_ptr, sat_len));
        v->sat_ptr.offset = (uint16_t)(sat_size + v->sat_ptr.offset);
    }
    stscom_display_hex_word(qsize);

    print_buffer(S(arr57)); /* Number of Flags = */
    stscom_display_hex_byte(sd_b(sd_nflags));
    print_buffer(S(arr58)); /* Maximum Paragraphs Per Process = */
    stscom_display_hex_word(sd_w(sd_mmp));
    crlf();
}

void stscom_display_ready(void) {
    display_links(S(arr59), sd_w(sd_rlr), T, 1); /* Ready Process(es): */
}

/* display$DQ and display$NQ: each queue with processes on its dq (nq)
 * list, and the list */
static void display_qlist(uint16_t title, uint16_t list, uint16_t crlf_sp, uint16_t two_sp) {
    crlf();
    print_buffer(title);
    v->qd_ptr.offset = sd_w(sd_qlr);
    while (rel(v->qd_ptr.offset != 0) & 1) {
        if (rel(w_(&v->qd_ptr, list) != 0) & 1) {
            print_buffer(crlf_sp);
            co(v->lparen);
            stscom_display_text(8, (uint16_t)(v->qd_ptr.offset + qd_name), v->qd_ptr.segment);
            co(v->rparen);
            print_buffer(two_sp);
            display_links(dummy, w_(&v->qd_ptr, list), T, 14);
        }
        v->qd_ptr.offset = w_(&v->qd_ptr, qd_link);
    }
}

void stscom_display_dq(void) { display_qlist(S(arr60), qd_dq, S(arr67), S(arr68)); } /* Process(es) DQing: */
void stscom_display_nq(void) { display_qlist(S(arr69), qd_nq, S(arr76), S(arr77)); } /* Process(es) NQing: */

void stscom_display_delay(void) {
    display_links(S(arr78), sd_w(sd_dlr), T, 1); /* Delayed Process(es): */
}

void stscom_display_poll(void) {
    display_links(S(arr79), sd_w(sd_plr), T, 1); /* Polling Process(es): */
}

void stscom_display_flag_wait(void) {
    int16_t i;
    crlf();
    v->flag_ptr.offset = sd_w(sd_flags);
    print_buffer(S(arr80)); /* Process(es) Flag Waiting: */
    for (i = 0; i <= (int16_t)(uint8_t)(sd_b(sd_nflags) - 1); i++) {
        if (rel(w_(&v->flag_ptr, 0) < 0xFFFE) & 1) { /* flag.pd */
            crlf();
            co(' ');
            co(' ');
            stscom_display_hex_byte((uint8_t)i);
            print_buffer(S(arr87)); /*  -        */
            display_links(dummy, w_(&v->flag_ptr, 0), T, 14);
        }
        v->flag_ptr.offset = (uint16_t)(v->flag_ptr.offset + flag_size);
    }
}

void stscom_display_flag_set(void) {
    uint8_t j, i;
    v->flag_ptr.offset = sd_w(sd_flags);
    crlf();
    print_buffer(S(arr88)); /* Flag(s) Set: */
    i = 0;
    j = screenwidth;
    while (rel(i < sd_b(sd_nflags)) & 1) {
        if (rel(w_(&v->flag_ptr, 0) == 0xFFFE) & 1) {
            if (rel(j >= screenwidth) & 1) {
                crlf();
                j = 0;
            }
            co(' ');
            co(' ');
            stscom_display_hex_byte(i);
            j = (uint8_t)(j + 4);
        }
        v->flag_ptr.offset = (uint16_t)(v->flag_ptr.offset + flag_size);
        i = (uint8_t)(i + 1);
    }
}

void stscom_display_queues(void) {
    uint8_t i;
    v->qd_ptr.offset = sd_w(sd_qlr);
    print_buffer(S(arr98)); /* Queue(s): */
    i = screenwidth;
    while (rel(v->qd_ptr.offset != 0) & 1) {
        if (rel(i > screenwidth - 19) & 1) {
            crlf();
            i = 0;
        }
        co(' ');
        stscom_display_text(8, (uint16_t)(v->qd_ptr.offset + qd_name), v->qd_ptr.segment);
        if (((w_(&v->qd_ptr, qd_flags) & qf_mx) & rel(w_(&v->qd_ptr, qd_buffer) != 0)) & 1) {
            v->pd_ptr.offset = w_(&v->qd_ptr, qd_buffer); /* addr of of owning process */
            co(v->lparen);
            stscom_display_text(pnamsiz, (uint16_t)(v->pd_ptr.offset + pd_name), v->pd_ptr.segment);
            co(v->rparen);
        } else {
            print_buffer(S(arr110));
        }
        v->qd_ptr.offset = w_(&v->qd_ptr, qd_link);
        i = (uint8_t)(i + 19);
    }
}

void stscom_display_ccb(uint16_t name, uint8_t first_ccb, uint8_t last_ccb) {
    uint8_t i;
    if (rel((uint8_t)(last_ccb - first_ccb) == 0) & 1) return;
    v->ccb_ptr.offset = (uint16_t)(sd_w(sd_ccb) + ccb_size * first_ccb);
    print_buffer(S(arr114)); /* Process(es) Attached to */
    print_buffer(name);
    for (i = 0; i <= (uint8_t)(last_ccb - first_ccb - 1); i++) {
        print_buffer(S(arr118));
        co(v->lparen);
        stscom_display_hex_byte(i);
        co(v->rparen);
        print_buffer(S(arr119)); /*       - */
        if (rel(w_(&v->ccb_ptr, ccb_attach) == 0) & 1) {
            print_buffer(S(arr123)); /* Unattached */
        } else if (rel(w_(&v->ccb_ptr, ccb_attach) == 0xFFFF) & 1) {
            print_buffer(S(arr127)); /* Control P - Console */
            co(v->lparen);
            stscom_display_hex_byte(b_(&v->ccb_ptr, ccb_msource));
            co(v->rparen);
        } else {
            v->pd_ptr.offset = w_(&v->ccb_ptr, ccb_attach);
            stscom_display_text(pnamsiz, (uint16_t)(v->pd_ptr.offset + pd_name), v->pd_ptr.segment);
        }
        v->ccb_ptr.offset = (uint16_t)(v->ccb_ptr.offset + ccb_size);
    }
    v->ccb_ptr.offset = (uint16_t)(sd_w(sd_ccb) + ccb_size * first_ccb);
    print_buffer(S(arr128)); /* Process(es) Waiting for */
    print_buffer(name);
    for (i = 0; i <= (uint8_t)(last_ccb - first_ccb - 1); i++) {
        if (rel(w_(&v->ccb_ptr, ccb_queue) != 0) & 1) {
            print_buffer(S(arr135));
            co(v->lparen);
            stscom_display_hex_byte(i);
            co(v->rparen);
            print_buffer(S(arr136)); /* put out 13 chars to */
            display_links(dummy, w_(&v->ccb_ptr, ccb_queue), F, 13); /* line up with other PD displays */
        }
        v->ccb_ptr.offset = (uint16_t)(v->ccb_ptr.offset + ccb_size);
    }
}

void stscom_display_memory(void) {
    uint8_t i, n, col;
    crlf();
    print_buffer(S(arr137)); /* Memory Partitions: */
    crlf();
    for (col = 1; col <= screenwidth / 23; col++) print_buffer(S(arr141)); /* Start Length Process | */
    col = screenwidth + 1; /* force new line */

    n = 0xFF; /* -1 */
    disable(); /* critical section required to obtain list */
    v->pd_ptr.offset = sd_w(sd_thrdrt);
    while ((rel(v->pd_ptr.offset != 0) & rel(n != 63)) & 1) {
        v->link_list[n = (uint8_t)(n + 1)] = v->pd_ptr.offset;
        v->pd_ptr.offset = w_(&v->pd_ptr, pd_thread);
    }
    mon1(dispatch, 0); /* enable interrupts by dispatching */
    if (rel(n == 0xFF) & 1) return;

    for (i = 0; i <= n; i++) {
        v->pd_ptr.offset = v->link_list[i];
        if (rel(col > screenwidth - 23) & 1) {
            crlf();
            col = 1;
        }
        if ((rel(w_(&v->pd_ptr, pd_mem) != 0) & rel(w_(&v->pd_ptr, pd_mem) != 8)) & 1) {
            v->ms_ptr.offset = w_(&v->pd_ptr, pd_mem);
            v->md_ptr.offset = w_(&v->ms_ptr, ms_mau_); /* the mau field of a MS descriptor */
            stscom_display_hex_word(w_(&v->md_ptr, md_start)); /* one or more partitions */
            print_buffer(S(arr165));                              /* described by the MAU */
            stscom_display_hex_word(w_(&v->md_ptr, md_length));
            print_buffer(S(arr166));
            stscom_display_text(pnamsiz, (uint16_t)(v->pd_ptr.offset + pd_name), v->pd_ptr.segment);
            print_buffer(S(arr167));
            col = (uint8_t)(col + 23);
        }
    }

    v->md_ptr.offset = sd_w(sd_mfl);
    while (rel(v->md_ptr.offset != 0) & 1) { /* don't need critical region for MD list - they aren't deleted */
        if (rel(col > screenwidth - 23) & 1) {
            crlf();
            col = 0;
        }
        stscom_display_hex_word(w_(&v->md_ptr, md_start));
        print_buffer(S(arr174));
        stscom_display_hex_word(w_(&v->md_ptr, md_length));
        print_buffer(S(arr175)); /*   * FREE *   */
        v->md_ptr.offset = w_(&v->md_ptr, md_link);
        col = (uint8_t)(col + 23);
    }
    enable();
}
