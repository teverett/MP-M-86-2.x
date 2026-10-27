/*
 * bdosif.c -- C port of src-assem/kernel/BDOSIF.BDO's `init` and `entry`,
 * named here `bdosif_init`/`bdosif_entry` -- every module's own real
 * asm label for these two routines is just `init`/`entry` (purely
 * locally/table-relative in the real system, never a globally
 * unique name), but `CIOIF.CIO`/`MEMIF.MEM`/`RTMIF.RTM`'s own C
 * ports (cioif.c/memif.c/rtmif.c) use the exact same bare names too
 * -- genuinely different real routines colliding once this whole
 * tree is linked into one real image (`src-c/Makefile`'s own
 * `kernel.elf` target), not a copy-paste mistake. Prose below still
 * says `init`/`entry` throughout for the real asm routine itself;
 * only the two C symbols themselves needed the module prefix.
 *
 * `entry`'s own real shape: compute `si` from `cl` (the BDOS function
 * number, 0-46) into `functab`, then branch on that entry's own
 * `btab_flag` byte. Every entry except 10 has `bf_getmx` set, meaning
 * the real dispatch goes through `getdiskmx` first -- a genuinely
 * large piece of real OS plumbing (acquire the mx disk queue,
 * critical-section stack switch, copy the UDA's own BDOS-relevant
 * fields into BDOSIF.BDO's own local area, and -- only for functions
 * whose own `btab_flag` also sets `BF_CSHELL`/`BF_TANDW` -- the SHELL
 * multi-sector-I/O wrapper around `CALL_BDOS` itself) that has no C
 * port here at all; `entry` reports reaching it (`reached_getdiskmx`)
 * rather than silently wrong-answering those 37 functions.
 *
 * The other 10 (`btab_flag == 0`: func24/25/26/29/32/44/45/51/52/107,
 * functions 11/12/13/16/19/31/32/36/37/45) skip `getdiskmx` entirely
 * -- real asm's own `nomx: call cs:btab_addr[si]` -- so `entry` calls
 * straight into their own already-ported-and-tested C targets
 * (file4.c's own `func24_entry`.../`func52_entry`; function 45,
 * `FUNC107 EQU FUNCRET`, is `BDOSIF.BDO`'s own empty `funcret: ret`,
 * a genuine no-op -- `bx`/`cx` just stay whatever they already were,
 * same "collapses to nothing observable" convention as `CHARIO.CIO`'s
 * own `stdout`). `func26` itself was found genuinely missing (not
 * merely deferred) while wiring this up -- see file4.c's own header
 * comment -- and is now ported too.
 *
 * `init` and `entry` both run against real memory (defs.h's far_ptr),
 * like the RTM: `phys` is the base of real physical memory and the
 * SYSDAT segment is the BDOS's DS (SUPIF enters it with DS = sysdat; the
 * combined SYSDAT+DATA.BDO data segment, so `BD_*` are DATA.BDO's real
 * offsets in it). `entry` passes it to every target as their `ds_bdos`.
 * `init` also takes `cs`, this module's own code segment. `init`:
 *
 *   1. Saves `ds_sysdat` in this module's own `cs:sysdat` cell and copies
 *      SYSDAT's module_table[SUP] entry far pointer into
 *      `cs:supervisor` (same shape as CIOIF.CIO/MEMIF.MEM/RTMIF.RTM's
 *      `init`).
 *
 *   2. Creates, opens, and writes the MX disk queue via three real
 *      `mpmif` calls -- `f_qmake`/`f_qopen`/`f_qwrite`, which reach
 *      que1.c's makeq_entry/openq_entry/writeq_entry (also on real
 *      memory). `mxdiskqd`/`mxdiskqpb` are DATA.BDO's real offsets in
 *      the combined SYSDAT+DATA.BDO data segment (SYSDAT_MXDISKQD/
 *      SYSDAT_MXDISKQPB). The real call goes through BDOSIF's and then
 *      SUPIF's `mpmif`, and the latter sets the running process's
 *      u_wrkseg to the caller's DS (SYSDAT here) for the call and
 *      restores it afterwards -- the queue code resolves dx through
 *      u_wrkseg, so the direct calls here do the same.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} bdos_bxcx;

extern bdos_bxcx makeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern bdos_bxcx openq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern bdos_bxcx writeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);

void bdosif_init(uint8_t *phys, uint16_t ds_sysdat, uint16_t cs) {
    uint8_t *sysdat = far_ptr(phys, ds_sysdat, 0);
    uint8_t *bdos_cs = far_ptr(phys, cs, 0);

    wr16(bdos_cs + BDOSIF_SYSDAT, ds_sysdat);
    wr16(bdos_cs + BDOSIF_SUPERVISOR_OFF, rd16(sysdat + SYSDAT_SUPMOD));
    wr16(bdos_cs + BDOSIF_SUPERVISOR_SEG, rd16(sysdat + SYSDAT_SUPMOD + 2));

    /* mpmif: u_wrkseg = caller's DS for the call, restored after */
    uint8_t *uda = far_ptr(phys, rlr_uda(sysdat), 0);
    uint16_t saved_wrkseg = rd16(uda + u_wrkseg);
    wr16(uda + u_wrkseg, ds_sysdat);

    /* create disk mx queue */
    (void)makeq_entry(phys, ds_sysdat, SYSDAT_MXDISKQD);
    /* open mx disk queue */
    (void)openq_entry(phys, ds_sysdat, SYSDAT_MXDISKQPB);
    /* write mx disk queue */
    (void)writeq_entry(phys, ds_sysdat, SYSDAT_MXDISKQPB);

    wr16(uda + u_wrkseg, saved_wrkseg);
}

extern uint16_t func24_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);
extern uint16_t func25_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);
extern void func26_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern uint16_t func29_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);
extern uint16_t func32_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *cx_out);
extern void func44_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out);
extern void func45_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out);
extern void func51_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern uint16_t func52_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out);

/* the getdiskmx functions (file1-4.c) */
typedef void (*bdos_fn)(uint8_t *phys, uint16_t ds_bdos);
extern void func13(uint8_t *, uint16_t), func14(uint8_t *, uint16_t), func15(uint8_t *, uint16_t),
    func16(uint8_t *, uint16_t), func17(uint8_t *, uint16_t), func18(uint8_t *, uint16_t),
    func19(uint8_t *, uint16_t), func20(uint8_t *, uint16_t), func21(uint8_t *, uint16_t),
    func22(uint8_t *, uint16_t), func23(uint8_t *, uint16_t), func27(uint8_t *, uint16_t),
    func28(uint8_t *, uint16_t), func30(uint8_t *, uint16_t), func31(uint8_t *, uint16_t),
    func33(uint8_t *, uint16_t), func34(uint8_t *, uint16_t), func35(uint8_t *, uint16_t),
    func36(uint8_t *, uint16_t), func37(uint8_t *, uint16_t), func40(uint8_t *, uint16_t),
    func42(uint8_t *, uint16_t), func43(uint8_t *, uint16_t), func48(uint8_t *, uint16_t),
    func100(uint8_t *, uint16_t), func102(uint8_t *, uint16_t), func103(uint8_t *, uint16_t),
    func104(uint8_t *, uint16_t), func106(uint8_t *, uint16_t), pr_term(uint8_t *, uint16_t);
extern void func46(uint8_t *phys, uint16_t ds_bdos, uint8_t dl);
extern void func101(uint8_t *phys, uint16_t ds_bdos, uint8_t dl);
extern void func105(uint8_t *phys, uint16_t ds_bdos, uint16_t dx);
extern void setdata(uint8_t *phys, uint16_t ds_bdos);
extern void setdir(uint8_t *phys, uint16_t ds_bdos);
extern void inc_pdcnt(uint8_t *phys, uint16_t ds_bdos);
extern void parsave33(uint8_t *phys, uint16_t ds_bdos);
extern void parsave36(uint8_t *phys, uint16_t ds_bdos);
extern void incr_rr(uint8_t *phys, uint16_t ds_bdos);
extern void save_rr(uint8_t *phys, uint16_t ds_bdos);
extern void reset_rr(uint8_t *phys, uint16_t ds_bdos);
extern void set_chksum_fcb(uint8_t *phys, uint16_t ds_bdos);
extern void parunsave(uint8_t *phys, uint16_t ds_bdos);
extern uint16_t getexta(uint8_t *phys, uint16_t ds_bdos);
extern uint16_t get_rcnta(uint8_t *phys, uint16_t ds_bdos);

/* functab's btab_flag bits */
#define bf_getmx  0x01 /* get mxdisk queue */
#define bf_cshell 0x02 /* conditional shell function */
#define bf_tandw  0x04 /* test & write function */
#define bf_fcb36  0x08 /* shell 36 byte fcb flag */

#define functab  0x74 /* BDOSIF.BDO's functab, in its code segment */
#define btablen  3
#define rdran_fn 20 /* RDRAN_OFF: functab's read random */
#define wrran_fn 21 /* WRRAN_OFF: write random */
#define nfuncs   47

/* functab's btab_flag column, by BDOS module function (0-46) */
static const uint8_t btab_flag[nfuncs] = {
    1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 0, 0, 0, 1, 1, /*  0-15 */
    0, 1, 1, 0, 11, 11, 1, 1, 1, 1, 1, 3, 13, 1, 1, 0, /* 16-31 */
    0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1,       /* 32-46 */
};

typedef struct {
    uint16_t bx, cx;
} entry_result;

/* GOBACK's save point (file4.c) */
extern void *bdos_save_sp[5];
extern int bdos_save_sp_armed;

/* the BDOS's mpmif: DS = SYSDAT */
static mpm_regs bdos_call(uint8_t *phys, uint16_t sysdat, uint16_t cx, uint16_t dx) {
    return MPMIF(bdos, phys, sysdat, cx, dx, 0, sysdat);
}

/* `call cs:btab_addr[si]` for a getdiskmx function: dx = the argument */
static void call_target(uint8_t *phys, uint16_t ds_bdos, uint8_t fn, uint16_t dx) {
    static const bdos_fn targets[nfuncs] = {
        [0] = func13, [1] = func14, [2] = func15, [3] = func16, [4] = func17, [5] = func18,
        [6] = func19, [7] = func20, [8] = func21, [9] = func22, [10] = func23, [14] = func27,
        [15] = func28, [17] = func30, [18] = func31, [20] = func33, [21] = func34, [22] = func35,
        [23] = func36, [24] = func37, [27] = func40, [29] = func42, [30] = func43, [35] = func48,
        [38] = func100, [40] = func102, [41] = func103, [42] = func104, [44] = func106,
        [46] = pr_term,
    };
    switch (fn) {
    case 33: func46(phys, ds_bdos, (uint8_t)dx); return;
    case 39: func101(phys, ds_bdos, (uint8_t)dx); return;
    case 43: func105(phys, ds_bdos, dx); return;
    default:
        if (targets[fn]) targets[fn](phys, ds_bdos);
        /* else funcret: 25 access drive, 26 free drive, 28 test & write,
         * 34 chain to program */
        return;
    }
}

/* BDOS_RETURN: put back what reselect changed in the FCB (checksum,
 * the r/o and extent bits, the drive), and copy a saved parameter
 * block back to the caller */
static void bdos_return(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (seg_r8(ds, BD_RESEL) != 0) {
        if (seg_r8(ds, BD_COMP_FCB_CKS) == 0xFF) set_chksum_fcb(phys, ds_bdos);
        uint16_t info = seg_r16(ds, BD_INFO);
        uint8_t al = seg_r8(ds, BD_XFCB_READ_ONLY);
        if (al) ds[(uint16_t)(info + 7)] |= al;
        uint16_t bx = getexta(phys, ds_bdos);
        al = seg_r8(ds, BD_HIGH_EXT);
        if (al == 0x60) {
            bx = (uint16_t)(bx - 4);
            al = 0x80;
        }
        ds[bx] |= al;
        al = seg_r8(ds, BD_ACTUAL_RC);
        if (al) ds[get_rcnta(phys, ds_bdos)] |= al;
        ds[info] = seg_r8(ds, BD_FCBDSK);
    }
    if (seg_r8(ds, BD_PARCOPFL) == 0xFF) parunsave(phys, ds_bdos);
}

/* CALL_BDOS: run the function, to its return or a GOBACK, then
 * BDOS_RETURN */
static void __attribute__((noinline)) call_bdos(uint8_t *phys, uint16_t ds_bdos, uint8_t fn, uint16_t dx) {
#if BDOS_GOBACK
    if (__builtin_setjmp(bdos_save_sp) == 0) {
        bdos_save_sp_armed = 1;
        call_target(phys, ds_bdos, fn, dx);
    }
    bdos_save_sp_armed = 0;
#else
    call_target(phys, ds_bdos, fn, dx);
#endif
    bdos_return(phys, ds_bdos);
}

/* CBDOS/CBDOS1: one call of the SHELL's function; returns low(aret) */
static uint8_t cbdos(uint8_t *phys, uint16_t ds_bdos, uint8_t fn) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    call_bdos(phys, ds_bdos, fn, seg_r16(ds, BD_INFO));
    return seg_r8(ds, BD_ARET);
}

/* SHELL_ERR: aret, with the number of records done (mult_cnt - the
 * remaining count) in its top 4 bits */
static uint16_t shell_err(uint8_t *phys, uint16_t ds_bdos, uint8_t remaining) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = seg_r16(ds, BD_ARET);
    uint8_t dl = (uint8_t)(seg_r8(ds, BD_MULT_CNT) - remaining);
    return (uint16_t)(bx | ((uint16_t)(uint8_t)(dl << 4) << 8));
}

/* the next record of the shell: dmaad += 80h */
static void next_dma(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_DMAAD, (uint16_t)(seg_r16(ds, BD_DMAAD) + 0x80));
}

/* MULT_IO: the function mult_cnt times, one record further each time */
static uint16_t mult_io(uint8_t *phys, uint16_t ds_bdos, uint8_t ah) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = seg_r8(ds, BD_MULT_CNT);
    do {
        uint8_t fn = (uint8_t)((seg_r16(ds, BD_SHELL_SI) - functab) / btablen);
        if (cbdos(phys, ds_bdos, fn) != 0) return shell_err(phys, ds_bdos, al);
        if (ah & bf_fcb36) incr_rr(phys, ds_bdos);
        next_dma(phys, ds_bdos);
        setdata(phys, ds_bdos);
    } while (--al != 0);
    return 0;
}

/* TST_WRT: read each record (into the directory buffer) and compare it
 * with the caller's data; if they all match, write them (MULT_IO with
 * write random) */
static uint16_t tst_wrt(uint8_t *phys, uint16_t ds_bdos, uint8_t ah) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t al = seg_r8(ds, BD_MULT_CNT);
    setdir(phys, ds_bdos);
    do {
        if (cbdos(phys, ds_bdos, rdran_fn) != 0) return shell_err(phys, ds_bdos, al);
        /* COMPARE_RECS: es:dmaad (dmabase) against buffa, 64 words */
        const uint8_t *user = far_ptr(phys, seg_r16(ds, BD_DMABASE), seg_r16(ds, BD_DMAAD));
        const uint8_t *buf = ds + seg_r16(ds, BD_BUFFA);
        for (int i = 0; i < 128; i++) {
            if (user[i] != buf[i]) return shell_err(phys, ds_bdos, al); /* (its bx = 7 is lost) */
        }
        incr_rr(phys, ds_bdos);
        next_dma(phys, ds_bdos);
    } while (--al != 0);
    setdata(phys, ds_bdos);
    reset_rr(phys, ds_bdos);
    seg_w16(ds, BD_SHELL_SI, functab + wrran_fn * btablen); /* WRRAN_OFF */
    return mult_io(phys, ds_bdos, ah);
}

/* SHELL: a multi-sector (or test & write) function as mult_cnt calls
 * of the single-record one, then SHELL_RTN */
static void shell(uint8_t *phys, uint16_t ds_bdos, uint8_t fn, uint8_t ah) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_SHELL_SI, (uint16_t)(functab + fn * btablen)); /* its functab entry */
    seg_w16(ds, BD_SHELL_DMA, seg_r16(ds, BD_DMAAD));
    if (ah & bf_fcb36) {
        parsave36(phys, ds_bdos);
        save_rr(phys, ds_bdos);
    } else {
        parsave33(phys, ds_bdos);
    }
    seg_w8(ds, BD_SHELL_FLAG, 0xFF);
    uint16_t bx = (ah & bf_tandw) ? tst_wrt(phys, ds_bdos, ah) : mult_io(phys, ds_bdos, ah);
    /* SHELL_RTN */
    seg_w16(ds, BD_ARET, bx);
    if (ah & bf_fcb36) reset_rr(phys, ds_bdos);
    seg_w16(ds, BD_DMAAD, seg_r16(ds, BD_SHELL_DMA));
    setdata(phys, ds_bdos);
    seg_w8(ds, BD_SHELL_FLAG, 0);
    parunsave(phys, ds_bdos);
}

/*
 * getdiskmx -- a function that uses the disk: take the MXdisk queue
 * (with pf_tempkeep on, so a ^C can't terminate the process inside the
 * BDOS), load the BDOS's copy of the caller's state (its UDA's BDOS
 * variables, user, disk, DMA), run the function (through the SHELL for
 * multi-sector I/O), copy the state back, release the queue, and
 * terminate now if a ^C came in meanwhile.
 *
 * The asm also switches to the BDOS's own stack in SYSDAT (bdosstack)
 * for the call; the C runs on the process's own stack.
 */
static entry_result getdiskmx(uint8_t *phys, uint16_t sysdat, uint8_t fn, uint16_t dx) {
    uint8_t *ds = far_ptr(phys, sysdat, 0);
    (void)bdos_call(phys, sysdat, f_constat, dx);

    /* do not allow ctrl c while in bdos */
    uint16_t rlr = seg_r16(ds, SYSDAT_RLR);
    uint16_t tempkeep = (uint16_t)(seg_r16(ds, (uint16_t)(rlr + p_flag)) & pf_tempkeep);
    seg_w16(ds, (uint16_t)(rlr + p_flag), (uint16_t)(seg_r16(ds, (uint16_t)(rlr + p_flag)) | pf_tempkeep));
    (void)bdos_call(phys, sysdat, f_qread, SYSDAT_MXDISKQPB);

    rlr = seg_r16(ds, SYSDAT_RLR);
    uint16_t bx = rlr;
    if (!(seg_r16(ds, (uint16_t)(rlr + p_flag)) & pf_ctlc)) {
        /* initialize the bdos data area for the user */
        uint8_t *uda = far_ptr(phys, seg_r16(ds, (uint16_t)(rlr + p_uda)), 0);
        seg_w16(ds, BD_PARAMETERSEGMENT, seg_r16(uda, u_wrkseg));
        seg_w16(ds, BD_RETURNSEG, seg_r16(uda, u_retseg));
        seg_w16(ds, BD_PDADDR, rlr);
        seg_w8(ds, BD_USRCODE, seg_r8(ds, (uint16_t)(rlr + p_user))); /* default user code */
        seg_w8(ds, BD_SELDSK, seg_r8(ds, (uint16_t)(rlr + p_dsk)));   /* default disk */
        /* default dma: setdata if the UDA's differs from ours */
        uint8_t setbf = 0;
        if (seg_r16(uda, u_dma_ofst) != seg_r16(ds, BD_DMAAD)) setbf = 0xFF;
        if (seg_r16(uda, u_dma_seg) != seg_r16(ds, BD_DMABASE)) setbf = 0xFF;
        seg_w8(ds, BD_SETBF, setbf);
        /* copy the uda's bdos vars to the local area */
        for (int i = 0; i < uda_ovl_len; i++) ds[BD_DMAAD + i] = uda[u_dma_ofst + i];
        if (seg_r8(ds, BD_PD_CNT) & 1) inc_pdcnt(phys, sysdat); /* reset pd_cnt if its low bit is set */

        seg_w16(ds, BD_INFO, dx);
        seg_w8(ds, BD_LINFO, (uint8_t)dx);
        for (int i = 0; i < 9; i++) ds[BD_FCBDSK + i] = 0; /* entryzero: fcbdsk .. zerolength */
        if (seg_r8(ds, BD_SETBF) == 0xFF) setdata(phys, sysdat);

        /* ready to go to the function */
        uint8_t ah = btab_flag[fn];
        if ((ah & bf_tandw) || (seg_r8(ds, BD_MULT_CNT) != 1 && (ah & bf_cshell))) {
            shell(phys, sysdat, fn, ah);
        } else {
            call_bdos(phys, sysdat, fn, dx);
        }

        /* RETMON: copy the local vars back to the uda, set the return
         * registers */
        uda = far_ptr(phys, seg_r16(ds, (uint16_t)(seg_r16(ds, SYSDAT_RLR) + p_uda)), 0);
        for (int i = 0; i < uda_ovl_len; i++) uda[u_dma_ofst + i] = ds[BD_DMAAD + i];
        seg_w16(uda, u_retseg, seg_r16(ds, BD_RETURNSEG));
        bx = seg_r16(ds, BD_ARET);
    }

    /* RETMONX: release the mxdisk queue; see if control c occurred */
    mpm_regs r = bdos_call(phys, sysdat, f_qwrite, SYSDAT_MXDISKQPB);
    rlr = seg_r16(ds, SYSDAT_RLR);
    uint16_t keep = (uint16_t)(~tempkeep ^ pf_tempkeep); /* clears pf_tempkeep unless it was on */
    seg_w16(ds, (uint16_t)(rlr + p_flag), (uint16_t)(seg_r16(ds, (uint16_t)(rlr + p_flag)) & keep));
    if (seg_r16(ds, (uint16_t)(rlr + p_flag)) & pf_ctlc) {
        r = bdos_call(phys, sysdat, f_terminate, 0);
    }
    return (entry_result){bx, r.cx};
}

/* entry -- resolve cl (BDOS function #, 0-46) into functab's own
 * entry: the btab_flag == 0 functions run directly (`nomx`), the rest
 * through getdiskmx. Returns BX (= AX) and CX.
 *
 * Real `entry`'s own prologue (`mov ch,0 ! ... ! mul cx ! ...`) only
 * ever clears `ch` -- `cl` (the function # this call was made with)
 * stays in `cx` for the rest of the routine, including into
 * whichever target it dispatches to, unless that target's own body
 * explicitly overwrites it. So `r.cx` starts as `cl` zero-extended
 * (func107/FUNCRET's real no-op return, case 45, leaves it at exactly
 * that) and each other case overwrites it with its own target's real
 * result. */
entry_result bdosif_entry(uint8_t *phys, uint16_t sysdat, uint8_t cl, uint16_t dx, uint16_t bx) {
    entry_result r = {bx, cl};
    uint8_t dl = (uint8_t)dx;
    if (cl >= nfuncs) return r;
    if (btab_flag[cl] & bf_getmx) return getdiskmx(phys, sysdat, cl, dx);
    switch (cl) {
    case 11: r.bx = func24_entry(phys, sysdat, &r.cx); break;
    case 12: r.bx = func25_entry(phys, sysdat, &r.cx); break;
    case 13: func26_entry(phys, sysdat, dx, &r.bx, &r.cx); break;
    case 16: r.bx = func29_entry(phys, sysdat, &r.cx); break;
    case 19: r.bx = func32_entry(phys, sysdat, dl, &r.cx); break;
    case 31: func44_entry(phys, sysdat, dl, &r.bx, &r.cx); break;
    case 32: func45_entry(phys, sysdat, dl, &r.bx, &r.cx); break;
    case 36: func51_entry(phys, sysdat, dx, &r.bx, &r.cx); break;
    case 37: r.bx = func52_entry(phys, sysdat, &r.cx); break;
    default: break; /* 45: func107/funcret, bx/cx stay ambient */
    }
    return r;
}
