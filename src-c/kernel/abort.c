/*
 * abort.c -- C port of src/kernel/ABORT.RTM: sysreset_entry,
 * terminate_entry (+ internal abt_chk), abort_spec_entry (+ internal
 * find_pd, find_pdthrd, findpdnc, findpdq, fflgpd, snip_it,
 * abt_putdrl, gccba, and the whole abort_tab dispatch: abtrun,
 * abtpol, abtdly, abtswp, abtdq, abtnq, abtflg, abtcns, abtdrl,
 * abt_common).
 *
 * `rlsmx` (src/kernel/QUE2.RTM, already ported in que2.c) and
 * `findpdname_entry` (src/kernel/FINDPD.RTM, already ported in
 * findpd.c) are called as ordinary internal calls, not boundary
 * stubs -- same convention as ml.c's/memory.c's own calls into
 * getmd/freemd.
 *
 * `jmp dsptch` (terminate_entry's own tail) and `jmp pdisp`
 * (abort_spec_entry's own success tail) are boundary tail-jumps into
 * the still-unported dispatcher, modeled the same way every other
 * already-ported caller of pdisp/dsptch in this tree does (e.g.
 * SYSENT.RTM's wakeup_entry): the state set immediately beforehand is
 * the whole observable result, since neither call is ever checked
 * for a return value by its own caller.
 *
 * `call cs:abort_tab[di]` -- an indirect near CALL through a jump
 * table -- needed two new tools/i86sim.py opcodes, `CALL r/m16` and
 * `JMP r/m16` (opcode 0xFF /2 and /4); every earlier file's own CALL/
 * JMP targets were fixed rel16 displacements. Ported here as a plain
 * C `dispatch_abort` switch instead, matching `p_stat`'s own value to
 * the real table's own row (`abtrun` reused for rows 0/4/5, matching
 * the real table's own `dw abtrun` repeated for "run"/"term"/"sleep").
 *
 * `abort_spec_entry`'s own `mov es:[si],offset terminate` (faking a
 * return address on the aborted process's own stack, so switching
 * back into it later lands directly in `terminate:`) needs
 * `terminate`'s own real assembled offset -- passed in as an explicit
 * parameter (`terminate_offset`), same convention as `sysdat`/
 * `user_retf_offset` elsewhere (que1.c/que2.c/ml.c/proc.c/load.c).
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} term_bxcx;

extern void rlsmx(uint8_t *mem, uint16_t si);
extern uint16_t findpdname_entry(uint8_t *mem, uint16_t bx, const uint8_t *name, uint16_t *cx_out);

/*
 * abt_chk -- check flags and termination code for abort/termination.
 *	output: 0 = not ok to abort, 1 = ok to abort, 0xFFFF = tempkeep
 */
uint16_t abt_chk(uint8_t *mem, uint16_t bx, uint8_t dl) {
    uint16_t flag = mem_r16(mem, (uint16_t)(bx + p_flag));
    if (flag & pf_keep) {
        mem_w16(mem, (uint16_t)(bx + p_flag), (uint16_t)(flag & (uint16_t)~pf_ctlc)); /* ac_n */
        return 0;
    }
    if (flag & pf_tempkeep) {
        mem_w16(mem, (uint16_t)(bx + p_flag), (uint16_t)(flag | pf_ctlc)); /* ac_tk */
        return 0xFFFF;
    }
    if (!(flag & pf_sys) || dl == 0xFF) {
        return 1; /* ac_y */
    }
    mem_w16(mem, (uint16_t)(bx + p_flag), (uint16_t)(flag & (uint16_t)~pf_ctlc)); /* ac_n */
    return 0;
}

/*
 * terminate_entry -- terminate the calling process (or, via
 * abort_spec_entry's own faked return address, a process that's just
 * been switched back into after being marked for abort).
 *	output: on failure, bx=0xFFFF, cx=error code; on success (or
 *		tempkeep), bx/cx reflect the state right at the real
 *		asm's own "jmp dsptch" -- see header comment
 */
term_bxcx terminate_entry(uint8_t *mem, uint16_t dx) {
    uint16_t bx = mem_r16(mem, SD_RLR);
    uint16_t chk = abt_chk(mem, bx, (uint8_t)dx);
    if ((int16_t)chk <= 0) {
        /* term_err */
        if (chk == 0) {
            return (term_bxcx){0xFFFF, e_pd_noterm};
        }
        return (term_bxcx){0, 0}; /* tempkeep */
    }

    /* terminate: -- unparent any of my own children */
    uint16_t me = mem_r16(mem, SD_RLR);
    uint16_t walk = (uint16_t)(SD_THRDRT - p_thread);
    for (;;) {
        walk = mem_r16(mem, (uint16_t)(walk + p_thread));
        if (walk == 0) {
            break;
        }
        if (mem_r16(mem, (uint16_t)(walk + p_parent)) == me) {
            mem_w16(mem, (uint16_t)(walk + p_parent), 0);
        }
    }

    bx = me; /* nochld */
    if (mem_r16(mem, (uint16_t)(bx + p_flag)) & pf_ctlc) {
        mem_w16(mem, (uint16_t)(bx + p_flag),
                (uint16_t)(mem_r16(mem, (uint16_t)(bx + p_flag)) & (uint16_t)~pf_ctlc));
        uint16_t parent = mem_r16(mem, (uint16_t)(bx + p_parent));
        if (parent != 0) {
            mem_w16(mem, (uint16_t)(parent + p_flag),
                    (uint16_t)(mem_r16(mem, (uint16_t)(parent + p_flag)) | pf_childabort));
        }
    }

    rlsmx(mem, mem_r16(mem, SD_RLR));
    mem[UDA_ERROR_MODE] = 0xFE;
    /* mpmif(f_bdosterm), mpmif(f_qread, mxmemqpb) -- boundary stubs, no
     * real effect, but CX is left holding f_qread's own value: nothing
     * resets it before the real asm's own "jmp dsptch" */
    bx = mem_r16(mem, SD_RLR);
    mem_w16(mem, SD_MXMEMOWNER, bx);
    mem[SD_MXMEMCOUNT] = 1;
    mem[bx + p_stat] = ps_term;
    return (term_bxcx){bx, f_qread};
}

uint16_t sysreset_entry(uint8_t *mem) {
    return terminate_entry(mem, 0).bx;
}

/* ---- abort_spec_entry and its own internal helpers ---- */

typedef struct {
    uint16_t si, di, cx;
} find_pd_result;

/* find_pd -- find PD bx on the linked list rooted at (di - p_link) */
static find_pd_result find_pd(uint8_t *mem, uint16_t bx, uint16_t di) {
    uint16_t si = mem_r16(mem, (uint16_t)(di + p_link));
    for (;;) {
        if (si == 0) {
            return (find_pd_result){si, di, 0};
        }
        if (si == bx) {
            return (find_pd_result){si, di, 1};
        }
        di = si;
        si = mem_r16(mem, (uint16_t)(si + p_link));
    }
}

static void snip_it(uint8_t *mem, uint16_t bx, uint16_t di) {
    mem_w16(mem, (uint16_t)(di + p_link), mem_r16(mem, (uint16_t)(bx + p_link)));
}

static void abt_putdrl(uint8_t *mem, uint16_t bx) {
    uint16_t drl = mem_r16(mem, SD_DRL);
    mem_w16(mem, (uint16_t)(bx + p_link), drl);
    mem_w16(mem, SD_DRL, bx);
}

static uint16_t gccba(uint8_t al) {
    return (uint16_t)((uint16_t)(al * ccblen) + SD_CCB);
}

static uint16_t find_pdthrd(uint8_t *mem, uint16_t bx) {
    uint16_t di = (uint16_t)(SD_THRDRT - p_thread);
    for (;;) {
        di = mem_r16(mem, (uint16_t)(di + p_thread));
        if (di == 0) {
            return 0;
        }
        if (di == bx) {
            return 1;
        }
    }
}

typedef struct {
    uint16_t bx, cx;
} findpdnc_result;

static findpdnc_result findpdnc(uint8_t *mem, uint16_t bx, const uint8_t *name, uint8_t ah) {
    for (;;) {
        uint16_t cx;
        uint16_t found = findpdname_entry(mem, bx, name, &cx);
        if (cx != 0) {
            return (findpdnc_result){0, 0};
        }
        bx = found;
        if (mem[bx + p_cns] == ah) {
            return (findpdnc_result){bx, 1};
        }
    }
}

typedef struct {
    uint16_t di, cx;
} findpdq_result;

static findpdq_result findpdq(uint8_t *mem, uint16_t bx, uint8_t al) {
    uint16_t qd = (uint16_t)(SD_QLR - q_link);
    for (;;) {
        qd = mem_r16(mem, (uint16_t)(qd + q_link));
        if (qd == 0) {
            return (findpdq_result){0, 0};
        }
        uint16_t di = (uint16_t)((uint16_t)(qd + (al != 0 ? q_nq : q_dq)) - p_link);
        find_pd_result r = find_pd(mem, bx, di);
        if (r.cx != 0) {
            return (findpdq_result){r.di, 1};
        }
    }
}

typedef struct {
    uint16_t di, cx;
} fflgpd_result;

static fflgpd_result fflgpd(uint8_t *mem, uint16_t bx) {
    uint16_t di = mem_r16(mem, SD_FLAGS);
    uint8_t n = mem[SD_NFLAGS];
    for (uint8_t i = 0; i < n; i++) {
        if (mem_r16(mem, (uint16_t)(di + flg_pd)) == bx) {
            return (fflgpd_result){di, 1};
        }
        di = (uint16_t)(di + flglen);
    }
    return (fflgpd_result){di, 0};
}

static uint16_t abt_find_snip(uint8_t *mem, uint16_t bx, uint16_t di) {
    find_pd_result r = find_pd(mem, bx, di);
    if (r.cx == 0) {
        return 0;
    }
    snip_it(mem, bx, r.di);
    return 1;
}

static uint16_t abtrun(uint8_t *mem, uint16_t bx) {
    if (abt_find_snip(mem, bx, (uint16_t)(SD_RLR - p_link))) {
        return 1;
    }
    return abt_find_snip(mem, bx, (uint16_t)(SD_DRL - p_link)); /* abtr_drl -> abt_common */
}

static uint16_t abtpol(uint8_t *mem, uint16_t bx) {
    return abt_find_snip(mem, bx, (uint16_t)(SD_PLR - p_link));
}

static uint16_t abtswp(uint8_t *mem, uint16_t bx) {
    return abt_find_snip(mem, bx, (uint16_t)(SD_SLR - p_link));
}

static uint16_t abtdq(uint8_t *mem, uint16_t bx) {
    findpdq_result r = findpdq(mem, bx, 0);
    if (r.cx == 0) {
        return 0;
    }
    snip_it(mem, bx, r.di);
    return 1;
}

static uint16_t abtnq(uint8_t *mem, uint16_t bx) {
    findpdq_result r = findpdq(mem, bx, 0xFF);
    if (r.cx == 0) {
        return 0;
    }
    snip_it(mem, bx, r.di);
    return 1;
}

static uint16_t abtflg(uint8_t *mem, uint16_t bx) {
    fflgpd_result r = fflgpd(mem, bx);
    if (r.cx == 0) {
        return 0;
    }
    mem[r.di + flg_ignore]++;
    mem_w16(mem, (uint16_t)(r.di + flg_pd), flag_off);
    return 1;
}

static uint16_t abtdly(uint8_t *mem, uint16_t bx) {
    find_pd_result r = find_pd(mem, bx, (uint16_t)(SD_DLR - p_link));
    if (r.cx == 0) {
        return 0;
    }
    uint16_t next = mem_r16(mem, (uint16_t)(r.si + p_link));
    if (next != 0) {
        uint16_t dx = (uint16_t)(mem_r16(mem, (uint16_t)(next + p_wait)) +
                                  mem_r16(mem, (uint16_t)(bx + p_wait)));
        mem_w16(mem, (uint16_t)(next + p_wait), dx);
    }
    snip_it(mem, bx, r.di);
    return 1;
}

static uint16_t abtcns(uint8_t *mem, uint16_t bx) {
    uint16_t ccb = gccba(mem[bx + p_cns]);
    if (abt_find_snip(mem, bx, (uint16_t)((uint16_t)(ccb + c_queue) - p_link))) {
        return 1;
    }
    ccb = gccba(mem[bx + p_lst]); /* abtlst -> abt_common */
    return abt_find_snip(mem, bx, (uint16_t)((uint16_t)(ccb + c_queue) - p_link));
}

static uint16_t dispatch_abort(uint8_t *mem, uint16_t bx, uint8_t stat) {
    switch (stat) {
        case 0: case 4: case 5: return abtrun(mem, bx); /* run, term, sleep */
        case 1: return abtpol(mem, bx);
        case 2: return abtdly(mem, bx);
        case 3: return abtswp(mem, bx);
        case 6: return abtdq(mem, bx);
        case 7: return abtnq(mem, bx);
        case 8: return abtflg(mem, bx);
        case 9: return abtcns(mem, bx);
        default: return 0;
    }
}

/*
 * abort_spec_entry -- set up the specified PD for termination when
 * it's next in context.
 *	input:	dx = address of APB in u_wrkseg
 *		terminate_offset = the real asm's own `terminate:`
 *		label address (see header comment)
 *	output:	bx = 0 on success, 0xFFFF on failure; cx = error code
 */
term_bxcx abort_spec_entry(uint8_t *mem, uint16_t dx, uint16_t terminate_offset) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t si = dx;
    uint16_t bx = mem_r16_seg(mem, wrkseg, (uint16_t)(si + apb_pd));
    uint16_t term = mem_r16_seg(mem, wrkseg, (uint16_t)(si + apb_term));
    uint8_t ah = mem_r8_seg(mem, wrkseg, (uint16_t)(si + apb_cns));

    if (bx != 0) {
        if (find_pdthrd(mem, bx) == 0) {
            return (term_bxcx){0xFFFF, e_no_pdname}; /* abt_err */
        }
    } else {
        uint8_t name[8];
        for (unsigned i = 0; i < 8; i++) {
            name[i] = mem_r8_seg(mem, wrkseg, (uint16_t)(si + apb_pdname + i));
        }
        findpdnc_result r = findpdnc(mem, (uint16_t)(SD_THRDRT - p_thread), name, ah);
        if (r.cx == 0) {
            return (term_bxcx){0xFFFF, e_no_pdname}; /* abt_err */
        }
        bx = r.bx;
    }

    if (bx == mem_r16(mem, SD_RLR)) {
        return terminate_entry(mem, term); /* we are the PD to abort */
    }

    uint16_t chk = abt_chk(mem, bx, (uint8_t)term);
    if ((int16_t)chk <= 0) {
        if (chk == 0) {
            return (term_bxcx){0xFFFF, e_pd_noterm};
        }
        return (term_bxcx){0, 0}; /* tempkeep */
    }

    uint8_t stat = mem[bx + p_stat];
    if (dispatch_abort(mem, bx, stat) == 0) {
        return (term_bxcx){0xFFFF, e_no_pdname}; /* abt_tab_err -> abt_err */
    }

    mem[bx + p_prior] = abt_prior;
    mem_w16(mem, (uint16_t)(bx + p_flag),
            (uint16_t)(mem_r16(mem, (uint16_t)(bx + p_flag)) & (uint16_t)~pf_ctlc));

    uint16_t pd_uda = mem_r16(mem, (uint16_t)(bx + p_uda));
    uint16_t fake_sp = (uint16_t)(ulen - 2);
    mem_w16_seg(mem, pd_uda, u_sp, fake_sp);
    mem_w16_seg(mem, pd_uda, u_ss, pd_uda);
    mem_w8_seg(mem, pd_uda, u_in_int, 0);
    mem_w16_seg(mem, pd_uda, fake_sp, terminate_offset);
    mem_w16_seg(mem, pd_uda, u_dparam, term);

    abt_putdrl(mem, bx);
    return (term_bxcx){0, 0}; /* jmp pdisp -- boundary tail, see header comment */
}
