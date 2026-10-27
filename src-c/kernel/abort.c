/*
 * abort.c -- C port of src-assem/kernel/ABORT.RTM: sysreset_entry,
 * terminate_entry (+ internal abt_chk), abort_spec_entry (+ internal
 * find_pd, find_pdthrd, findpdnc, findpdq, fflgpd, snip_it,
 * abt_putdrl, gccba, and the whole abort_tab dispatch: abtrun,
 * abtpol, abtdly, abtswp, abtdq, abtnq, abtflg, abtcns, abtdrl,
 * abt_common).
 *
 * `rlsmx` (src-assem/kernel/QUE2.RTM, already ported in que2.c) and
 * `findpdname_entry` (src-assem/kernel/FINDPD.RTM, already ported in
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
 *
 * Runs against real memory (defs.h's far_ptr), like the rest of the
 * RTM: `phys` is the base of real physical memory and `sysdat` the real
 * SYSDAT segment (the real asm's DS). PDs, every process list, the QD
 * list, the flag and CCB tables, and mxmemowner/mxmemcount are SYSDAT-
 * relative; the APB (abort_spec_entry's dx) is in the running process's
 * u_wrkseg; u_error_mode is the running process's UDA; the aborted
 * process's own UDA is its p_uda segment.
 *
 * `gccba` adds the *contents* of SYSDAT's `ccb` word (`ccb rw 1 ;addr.
 * Console Ctrl Blk Table`, so `add si,ccb` is a memory operand). An
 * earlier version added the field's own address instead, which sent
 * `abtcns` (aborting a process waiting on a console/list device) to the
 * wrong CCB; tests/src-c/test_abort.py now covers that path.
 *
 * `findpdq` walks every QD on qlr. The asm saved the DQ/NQ list root
 * instead of the QD across `find_pd`, so it only ever searched the first
 * QD -- docs/bugs.md #104, fixed in src-assem/kernel/ABORT.RTM to match.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} term_bxcx;

MPMIF_HOOK(abort)

extern void rlsmx(uint8_t *phys, uint16_t sysdat, uint16_t si);
extern uint16_t findpdname_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx, uint16_t *cx_out);

static uint16_t pd_r16(const uint8_t *sd, uint16_t pd, uint16_t field) {
    return seg_r16(sd, (uint16_t)(pd + field));
}

static void pd_w16(uint8_t *sd, uint16_t pd, uint16_t field, uint16_t v) {
    seg_w16(sd, (uint16_t)(pd + field), v);
}

/*
 * abt_chk -- check flags and termination code for abort/termination.
 *	output: 0 = not ok to abort, 1 = ok to abort, 0xFFFF = tempkeep
 */
static uint16_t abt_chk(uint8_t *sd, uint16_t bx, uint8_t dl) {
    uint16_t flag = pd_r16(sd, bx, p_flag);
    if (flag & pf_keep) {
        pd_w16(sd, bx, p_flag, (uint16_t)(flag & (uint16_t)~pf_ctlc)); /* ac_n */
        return 0;
    }
    if (flag & pf_tempkeep) {
        pd_w16(sd, bx, p_flag, (uint16_t)(flag | pf_ctlc)); /* ac_tk */
        return 0xFFFF;
    }
    if (!(flag & pf_sys) || dl == 0xFF) {
        return 1; /* ac_y */
    }
    pd_w16(sd, bx, p_flag, (uint16_t)(flag & (uint16_t)~pf_ctlc)); /* ac_n */
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
term_bxcx terminate_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t bx = seg_r16(sd, SYSDAT_RLR);
    uint16_t chk = abt_chk(sd, bx, (uint8_t)dx);
    if ((int16_t)chk <= 0) {
        /* term_err */
        if (chk == 0) {
            return (term_bxcx){0xFFFF, e_pd_noterm};
        }
        return (term_bxcx){0, 0}; /* tempkeep */
    }

    /* terminate: -- unparent any of my own children */
    uint16_t me = seg_r16(sd, SYSDAT_RLR);
    uint16_t walk = (uint16_t)(SYSDAT_THRDRT - p_thread);
    for (;;) {
        walk = pd_r16(sd, walk, p_thread);
        if (walk == 0) {
            break;
        }
        if (pd_r16(sd, walk, p_parent) == me) {
            pd_w16(sd, walk, p_parent, 0);
        }
    }

    bx = me; /* nochld */
    if (pd_r16(sd, bx, p_flag) & pf_ctlc) {
        pd_w16(sd, bx, p_flag, (uint16_t)(pd_r16(sd, bx, p_flag) & (uint16_t)~pf_ctlc));
        uint16_t parent = pd_r16(sd, bx, p_parent);
        if (parent != 0) {
            pd_w16(sd, parent, p_flag, (uint16_t)(pd_r16(sd, parent, p_flag) | pf_childabort));
        }
    }

    rlsmx(phys, sysdat, seg_r16(sd, SYSDAT_RLR));
    seg_w8(far_ptr(phys, rlr_uda(sd), 0), u_error_mode, 0xFE);
    /* the BDOS releases the process's files and locks (PR_TERM); then
     * take the MXmem queue for term_act's memory release. CX is left
     * holding f_qread's result: nothing resets it before the real asm's
     * own "jmp dsptch". (No hook: f_qread itself.) */
    (void)MPMIF(abort, phys, sysdat, f_bdosterm, dx, 0, sysdat);
    mpm_regs q = MPMIF(abort, phys, sysdat, f_qread, SYSDAT_MXMEMQPB, 0, sysdat);
    uint16_t cx = abort_mpmif ? q.cx : f_qread;
    bx = seg_r16(sd, SYSDAT_RLR);
    seg_w16(sd, SYSDAT_MXMEMOWNER, bx);
    seg_w8(sd, SYSDAT_MXMEMCOUNT, 1);
    seg_w8(sd, (uint16_t)(bx + p_stat), ps_term);
    return (term_bxcx){bx, cx};
}

uint16_t sysreset_entry(uint8_t *phys, uint16_t sysdat) {
    return terminate_entry(phys, sysdat, 0).bx;
}

/* ---- abort_spec_entry and its own internal helpers ---- */

typedef struct {
    uint16_t si, di, cx;
} find_pd_result;

/* find_pd -- find PD bx on the linked list rooted at (di - p_link) */
static find_pd_result find_pd(const uint8_t *sd, uint16_t bx, uint16_t di) {
    uint16_t si = pd_r16(sd, di, p_link);
    for (;;) {
        if (si == 0) {
            return (find_pd_result){si, di, 0};
        }
        if (si == bx) {
            return (find_pd_result){si, di, 1};
        }
        di = si;
        si = pd_r16(sd, si, p_link);
    }
}

static void snip_it(uint8_t *sd, uint16_t bx, uint16_t di) {
    pd_w16(sd, di, p_link, pd_r16(sd, bx, p_link));
}

static void abt_putdrl(uint8_t *sd, uint16_t bx) {
    pd_w16(sd, bx, p_link, seg_r16(sd, SYSDAT_DRL));
    seg_w16(sd, SYSDAT_DRL, bx);
}

/* gccba -- CCB address for console/list al: al*ccblen + [ccb] */
static uint16_t gccba(const uint8_t *sd, uint8_t al) {
    return (uint16_t)((uint16_t)(al * ccblen) + seg_r16(sd, SYSDAT_CCB));
}

static uint16_t find_pdthrd(const uint8_t *sd, uint16_t bx) {
    uint16_t di = (uint16_t)(SYSDAT_THRDRT - p_thread);
    for (;;) {
        di = pd_r16(sd, di, p_thread);
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

/* findpdnc -- find a PD by name (dx -> name in u_wrkseg) on console ah */
static findpdnc_result findpdnc(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx, uint8_t ah) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    for (;;) {
        uint16_t cx;
        uint16_t found = findpdname_entry(phys, sysdat, bx, dx, &cx);
        if (cx != 0) {
            return (findpdnc_result){0, 0};
        }
        bx = found;
        if (seg_r8(sd, (uint16_t)(bx + p_cns)) == ah) {
            return (findpdnc_result){bx, 1};
        }
    }
}

typedef struct {
    uint16_t di, cx;
} findpdq_result;

static findpdq_result findpdq(const uint8_t *sd, uint16_t bx, uint8_t al) {
    uint16_t qd = (uint16_t)(SYSDAT_QLR - q_link);
    for (;;) {
        qd = seg_r16(sd, (uint16_t)(qd + q_link));
        if (qd == 0) {
            return (findpdq_result){0, 0};
        }
        uint16_t di = (uint16_t)((uint16_t)(qd + (al != 0 ? q_nq : q_dq)) - p_link);
        find_pd_result r = find_pd(sd, bx, di);
        if (r.cx != 0) {
            return (findpdq_result){r.di, 1};
        }
    }
}

typedef struct {
    uint16_t di, cx;
} fflgpd_result;

static fflgpd_result fflgpd(const uint8_t *sd, uint16_t bx) {
    uint16_t di = seg_r16(sd, SYSDAT_FLAGS);
    uint8_t n = seg_r8(sd, SYSDAT_NFLAGS);
    for (uint8_t i = 0; i < n; i++) {
        if (seg_r16(sd, (uint16_t)(di + flg_pd)) == bx) {
            return (fflgpd_result){di, 1};
        }
        di = (uint16_t)(di + flglen);
    }
    return (fflgpd_result){di, 0};
}

static uint16_t abt_find_snip(uint8_t *sd, uint16_t bx, uint16_t di) {
    find_pd_result r = find_pd(sd, bx, di);
    if (r.cx == 0) {
        return 0;
    }
    snip_it(sd, bx, r.di);
    return 1;
}

static uint16_t abtrun(uint8_t *sd, uint16_t bx) {
    if (abt_find_snip(sd, bx, (uint16_t)(SYSDAT_RLR - p_link))) {
        return 1;
    }
    return abt_find_snip(sd, bx, (uint16_t)(SYSDAT_DRL - p_link)); /* abtr_drl -> abt_common */
}

static uint16_t abtpol(uint8_t *sd, uint16_t bx) {
    return abt_find_snip(sd, bx, (uint16_t)(SYSDAT_PLR - p_link));
}

static uint16_t abtswp(uint8_t *sd, uint16_t bx) {
    return abt_find_snip(sd, bx, (uint16_t)(SYSDAT_SLR - p_link));
}

static uint16_t abtq(uint8_t *sd, uint16_t bx, uint8_t al) {
    findpdq_result r = findpdq(sd, bx, al);
    if (r.cx == 0) {
        return 0;
    }
    snip_it(sd, bx, r.di);
    return 1;
}

static uint16_t abtflg(uint8_t *sd, uint16_t bx) {
    fflgpd_result r = fflgpd(sd, bx);
    if (r.cx == 0) {
        return 0;
    }
    seg_w8(sd, (uint16_t)(r.di + flg_ignore), (uint8_t)(seg_r8(sd, (uint16_t)(r.di + flg_ignore)) + 1));
    seg_w16(sd, (uint16_t)(r.di + flg_pd), flag_off);
    return 1;
}

static uint16_t abtdly(uint8_t *sd, uint16_t bx) {
    find_pd_result r = find_pd(sd, bx, (uint16_t)(SYSDAT_DLR - p_link));
    if (r.cx == 0) {
        return 0;
    }
    uint16_t next = pd_r16(sd, r.si, p_link);
    if (next != 0) {
        pd_w16(sd, next, p_wait, (uint16_t)(pd_r16(sd, next, p_wait) + pd_r16(sd, bx, p_wait)));
    }
    snip_it(sd, bx, r.di);
    return 1;
}

static uint16_t abtcns(uint8_t *sd, uint16_t bx) {
    uint16_t ccb = gccba(sd, seg_r8(sd, (uint16_t)(bx + p_cns)));
    if (abt_find_snip(sd, bx, (uint16_t)((uint16_t)(ccb + c_queue) - p_link))) {
        return 1;
    }
    ccb = gccba(sd, seg_r8(sd, (uint16_t)(bx + p_lst))); /* abtlst -> abt_common */
    return abt_find_snip(sd, bx, (uint16_t)((uint16_t)(ccb + c_queue) - p_link));
}

static uint16_t dispatch_abort(uint8_t *sd, uint16_t bx, uint8_t stat) {
    switch (stat) {
        case 0: case 4: case 5: return abtrun(sd, bx); /* run, term, sleep */
        case 1: return abtpol(sd, bx);
        case 2: return abtdly(sd, bx);
        case 3: return abtswp(sd, bx);
        case 6: return abtq(sd, bx, 0);    /* abtdq */
        case 7: return abtq(sd, bx, 0xFF); /* abtnq */
        case 8: return abtflg(sd, bx);
        case 9: return abtcns(sd, bx);
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
term_bxcx abort_spec_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t terminate_offset) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    const uint8_t *apb = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t bx = seg_r16(apb, (uint16_t)(dx + apb_pd));
    uint16_t term = seg_r16(apb, (uint16_t)(dx + apb_term));
    uint8_t ah = seg_r8(apb, (uint16_t)(dx + apb_cns));

    if (bx != 0) {
        if (find_pdthrd(sd, bx) == 0) {
            return (term_bxcx){0xFFFF, e_no_pdname}; /* abt_err */
        }
    } else {
        findpdnc_result r = findpdnc(phys, sysdat, (uint16_t)(SYSDAT_THRDRT - p_thread),
                                     (uint16_t)(dx + apb_pdname), ah);
        if (r.cx == 0) {
            return (term_bxcx){0xFFFF, e_no_pdname}; /* abt_err */
        }
        bx = r.bx;
    }

    if (bx == seg_r16(sd, SYSDAT_RLR)) {
        return terminate_entry(phys, sysdat, term); /* we are the PD to abort */
    }

    uint16_t chk = abt_chk(sd, bx, (uint8_t)term);
    if ((int16_t)chk <= 0) {
        if (chk == 0) {
            return (term_bxcx){0xFFFF, e_pd_noterm};
        }
        return (term_bxcx){0, 0}; /* tempkeep */
    }

    if (dispatch_abort(sd, bx, seg_r8(sd, (uint16_t)(bx + p_stat))) == 0) {
        return (term_bxcx){0xFFFF, e_no_pdname}; /* abt_tab_err -> abt_err */
    }

    seg_w8(sd, (uint16_t)(bx + p_prior), abt_prior);
    pd_w16(sd, bx, p_flag, (uint16_t)(pd_r16(sd, bx, p_flag) & (uint16_t)~pf_ctlc));

    /* fake a return into `terminate:` on the aborted process's own stack */
    uint16_t pd_uda = pd_r16(sd, bx, p_uda);
    uint8_t *uda = far_ptr(phys, pd_uda, 0);
    uint16_t fake_sp = (uint16_t)(ulen - 2);
    seg_w16(uda, u_sp, fake_sp);
    seg_w16(uda, u_ss, pd_uda);
    seg_w8(uda, u_in_int, 0);
    seg_w16(uda, fake_sp, terminate_offset);
    seg_w16(uda, u_dparam, term);

    abt_putdrl(sd, bx);
    return (term_bxcx){0, 0}; /* jmp pdisp -- boundary tail, see header comment */
}
