/*
 * console.c -- C port of all of src-assem/kernel/CONSOLE.CIO, plus
 * src-assem/kernel/CHARIO.CIO's getccbadr (a tiny, fully self-contained
 * dependency pulled in on its own). The rest of CHARIO.CIO -- the much
 * larger, deeply interdependent console-input state machine
 * (coninit/attach/conbrk/conin/chkctl/echoc/tabout/...) -- is ported in
 * src-c/kernel/chario.c, which calls back into this file's own
 * getccbadr, conattach_entry, condetach_entry, mimic_entry and
 * unmimic_entry -- see that file's own header comment.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the real SYSDAT segment (the CIO
 * module's DS). The CCB table (`ccb` points at it), PDs, `rlr`/`thrdrt`
 * and the device counts (`ncns`/`ncondev`/`nlstdev`) are SYSDAT-relative;
 * conassign's ACB is in the running process's u_wrkseg, read from its
 * UDA.
 *
 * `cioattach`'s real `ca_sleep` path (console busy, not conditional) is
 * translated faithfully below, but tests/src-c/test_console.py never
 * exercises it: under the boundary-stub convention (`mpmif(f_sleep)` as
 * a no-op), nothing ever changes `c_attach`, so the real `jmps
 * cioattach` retry loop would spin forever -- same category as
 * QUE1.RTM's readq_entry/writeq_entry, deliberately not exercised.
 *
 * `conassign_entry`'s name search (`mpmif(f_findpdname)`, an RTM
 * function) is a real call into findpd.c's findpdname_entry, which runs
 * on real memory too: SUPIF's mpmif hands it the caller's DS as
 * u_wrkseg, and the caller's DS is u_wrkseg here (`mov ds,u_wrkseg`), so
 * the name at u_wrkseg:acb_name is read in place. The asm's retry when
 * a found PD's default console doesn't match (`as_gpd` back to
 * `as_trya`) continues the thread-list search after that PD, here too.
 */
#include <stdint.h>

#include "defs.h"

MPMIF_HOOK(console) /* f_sleep/f_wakeup on a CCB queue (defs.h) */

extern uint16_t findpdname_entry(uint8_t *phys, uint16_t sysdat, uint16_t bx, uint16_t dx, uint16_t *cx_out);

/*
 * getccbadr -- get CCB for calling proc's default console
 *	input:	dh = console number
 *	output:	returns address of CCB (in SYSDAT); *bx_out = PD address
 *		(rlr)
 *
 * Not static: src-c/kernel/chario.c (the rest of src-assem/kernel/CHARIO.CIO)
 * calls into this same instance -- see its own header comment.
 */
uint16_t getccbadr(uint8_t *phys, uint16_t sysdat, uint8_t dh, uint16_t *bx_out) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    *bx_out = seg_r16(sd, SYSDAT_RLR);
    return (uint16_t)(seg_r16(sd, SYSDAT_CCB) + (uint16_t)dh * ccblen);
}

/*
 * cioattach -- Attach calling process to default ciodev.
 *	input:	pd_field_off = offset of dev number in PD (p_cns or p_lst)
 *		cond = nonzero if conditional (don't wait)
 */
cio_bxcx cioattach(uint8_t *phys, uint16_t sysdat, uint16_t pd_field_off, uint16_t cond) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    for (;;) {
        uint16_t pd, si, bx;
        pd = seg_r16(sd, SYSDAT_RLR);
        uint8_t dh = seg_r8(sd, (uint16_t)(pd + pd_field_off));
        si = getccbadr(phys, sysdat, dh, &bx);

        uint16_t attach = seg_r16(sd, (uint16_t)(si + c_attach));
        if (bx == attach) {
            cio_bxcx r = {0, 0};
            return r;
        }
        if (attach == 0) {
            seg_w16(sd, (uint16_t)(si + c_attach), bx);
            cio_bxcx r = {0, 0};
            return r;
        }
        if (attach == 0xFFFF) {
            uint8_t dh2 = seg_r8(sd, (uint16_t)(si + c_msource));
            uint16_t bx2;
            uint16_t si2 = getccbadr(phys, sysdat, dh2, &bx2);
            if (bx == seg_r16(sd, (uint16_t)(si2 + c_attach))) {
                cio_bxcx r = {0, 0};
                return r;
            }
        }
        /* ca_chkslp */
        if (cond != 0) {
            cio_bxcx r = {0xFFFF, e_no_attach};
            return r;
        }
        /* ca_sleep: lea dx,c_queue[si] ! ... ! mov cx,f_sleep ! call
         * mpmif, then try again (see header comment: not exercised by
         * the test, which has no hook) */
        seg_w8(sd, (uint16_t)(bx + p_stat), ps_ciowait);
        (void)MPMIF(console, phys, sysdat, f_sleep, (uint16_t)(si + c_queue), 0, sysdat);
    }
}

/* ciodetach -- Detach calling process from default ciodev. */
cio_bxcx ciodetach(uint8_t *phys, uint16_t sysdat, uint16_t pd_field_off) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t pd = seg_r16(sd, SYSDAT_RLR);
    uint8_t dh = seg_r8(sd, (uint16_t)(pd + pd_field_off));
    uint16_t bx;
    uint16_t si = getccbadr(phys, sysdat, dh, &bx);

    uint16_t attach = seg_r16(sd, (uint16_t)(si + c_attach));
    if (attach != 0) {
        if (attach != bx) {
            cio_bxcx r = {0xFFFF, e_not_owner};
            return r;
        }
        seg_w16(sd, (uint16_t)(si + c_attach), seg_r16(sd, (uint16_t)(si + c_queue)));
        /* lea dx,c_queue[si] ! mov cx,f_wakeup ! call mpmif */
        (void)MPMIF(console, phys, sysdat, f_wakeup, (uint16_t)(si + c_queue), 0, sysdat);
    }
    cio_bxcx r = {0, 0};
    return r;
}

cio_bxcx lstattach_entry(uint8_t *phys, uint16_t sysdat) { return cioattach(phys, sysdat, p_lst, 0); }
cio_bxcx clstattch_entry(uint8_t *phys, uint16_t sysdat) { return cioattach(phys, sysdat, p_lst, 0xFFFF); }
cio_bxcx cconattch_entry(uint8_t *phys, uint16_t sysdat) { return cioattach(phys, sysdat, p_cns, 0xFFFF); }
cio_bxcx conattach_entry(uint8_t *phys, uint16_t sysdat) { return cioattach(phys, sysdat, p_cns, 0); }
cio_bxcx lstdetach_entry(uint8_t *phys, uint16_t sysdat) { return ciodetach(phys, sysdat, p_lst); }
cio_bxcx condetach_entry(uint8_t *phys, uint16_t sysdat) { return ciodetach(phys, sysdat, p_cns); }

/* getdefcon_entry -- Get Default Console */
cio_bxcx getdefcon_entry(uint8_t *phys, uint16_t sysdat) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t pd = seg_r16(sd, SYSDAT_RLR);
    cio_bxcx r = {seg_r8(sd, (uint16_t)(pd + p_cns)), 0};
    return r;
}

/* getdeflst_entry -- Get Default List */
cio_bxcx getdeflst_entry(uint8_t *phys, uint16_t sysdat) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t pd = seg_r16(sd, SYSDAT_RLR);
    uint8_t bl = (uint8_t)(seg_r8(sd, (uint16_t)(pd + p_lst)) - seg_r8(sd, SYSDAT_NCONDEV));
    cio_bxcx r = {bl, 0};
    return r;
}

/* setdefcon_entry -- Set Default Console */
cio_bxcx setdefcon_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (dl >= seg_r8(sd, SYSDAT_NCONDEV)) {
        cio_bxcx r = {0xFFFF, e_ill_cns};
        return r;
    }
    uint16_t pd = seg_r16(sd, SYSDAT_RLR);
    if (dl != seg_r8(sd, (uint16_t)(pd + p_cns))) {
        condetach_entry(phys, sysdat);
    }
    pd = seg_r16(sd, SYSDAT_RLR);
    seg_w8(sd, (uint16_t)(pd + p_cns), dl);
    cio_bxcx r = {0, 0};
    return r;
}

/* setdeflst_entry -- Set Default List */
cio_bxcx setdeflst_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (dl >= seg_r8(sd, SYSDAT_NLSTDEV)) {
        cio_bxcx r = {0xFFFF, e_ill_lst};
        return r;
    }
    dl = (uint8_t)(dl + seg_r8(sd, SYSDAT_NCONDEV));
    uint16_t pd = seg_r16(sd, SYSDAT_RLR);
    if (dl != seg_r8(sd, (uint16_t)(pd + p_lst))) {
        lstdetach_entry(phys, sysdat);
    }
    pd = seg_r16(sd, SYSDAT_RLR);
    seg_w8(sd, (uint16_t)(pd + p_lst), dl);
    cio_bxcx r = {0, 0};
    return r;
}

/*
 * conassign_entry -- Attach specified console to specified process
 *	input:	dx -> acb address in u_wrkseg
 */
cio_bxcx conassign_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    const uint8_t *acb = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    uint16_t di = dx;
    uint8_t ah = seg_r8(acb, (uint16_t)(di + acb_cns));
    if (ah > seg_r8(sd, SYSDAT_NCNS)) {
        cio_bxcx r = {0xFFFF, e_ill_cns};
        return r;
    }

    uint16_t calling_pd;
    uint16_t bp = getccbadr(phys, sysdat, ah, &calling_pd); /* the CCB to assign */

    uint16_t attach = seg_r16(sd, (uint16_t)(bp + c_attach));
    if (!(attach == 0 || attach == calling_pd)) {
        cio_bxcx r = {0xFFFF, e_not_owner};
        return r;
    }

    /* as_own: find the PD to assign to */
    uint16_t bx = seg_r16(acb, (uint16_t)(di + acb_pd));
    if (bx == 0) {
        bx = (uint16_t)(SYSDAT_THRDRT - p_thread);
        goto as_trya;
    }
    for (;;) {
        /* as_gpd: see if the PD has the same default console */
        if (seg_r8(acb, (uint16_t)(di + acb_match)) == 0 ||
            ah == seg_r8(sd, (uint16_t)(bx + p_cns))) {
            break; /* as_mok */
        }
    as_trya: {
            /* mpmif(f_findpdname): search the thread list after bx */
            uint16_t cx;
            bx = findpdname_entry(phys, sysdat, bx, (uint16_t)(di + acb_name), &cx);
            if (cx != 0) {
                cio_bxcx r = {0xFFFF, cx};
                return r;
            }
        }
    }

    /* as_mok */
    uint16_t si = bx;
    seg_w16(sd, (uint16_t)(bp + c_attach), si);
    uint16_t walk = (uint16_t)(bp + c_queue - p_link);
    for (;;) {
        uint16_t link = seg_r16(sd, (uint16_t)(walk + p_link));
        if (link == 0) {
            cio_bxcx r = {0, 0}; /* as_gexit */
            return r;
        }
        if (link == si) {
            /* as_qfix: take it off the queue, put it at the head */
            uint16_t ax = seg_r16(sd, (uint16_t)(si + p_link));
            seg_w16(sd, (uint16_t)(walk + p_link), ax);
            uint16_t bp2 = seg_r16(sd, (uint16_t)(bp + c_queue));
            seg_w16(sd, (uint16_t)(bp + c_queue), si);
            seg_w16(sd, (uint16_t)(si + p_link), bp2);
            /* lea dx,c_queue[bx] ! mov cx,f_wakeup ! call mpmif */
            (void)MPMIF(console, phys, sysdat, f_wakeup, (uint16_t)(bp + c_queue), 0, sysdat);
            cio_bxcx r = {0, 0};
            return r;
        }
        walk = link;
    }
}

/*
 * mimic_entry -- mimic a character device
 *	input:	dh = source device, dl = mimic device
 */
cio_bxcx mimic_entry(uint8_t *phys, uint16_t sysdat, uint8_t dh_in, uint8_t dl_in) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t bx;
    uint16_t si = getccbadr(phys, sysdat, dh_in, &bx);
    uint8_t dl = dl_in;

    if (seg_r16(sd, (uint16_t)(si + c_attach)) != bx) goto mim_err;
    if (seg_r8(sd, (uint16_t)(si + c_mimic)) != 0xFF) goto mim_err;

    {
        uint16_t di = si;       /* push si ... pop di */
        uint8_t saved_dh = dh_in; /* push dx (dh:dl pair) ... pop dx restores
                                    * it after the call below -- "mov dh,dl"
                                    * is only a transient input to getccbadr */
        uint16_t bx2;
        si = getccbadr(phys, sysdat, dl, &bx2); /* mov dh,dl ! call getccbadr */
        /* pop dx: dh is back to saved_dh from here on */

        if (seg_r16(sd, (uint16_t)(si + c_attach)) != 0) goto mim_err;
        if (seg_r8(sd, (uint16_t)(si + c_mimic)) != 0xFF) goto mim_err;

        seg_w16(sd, (uint16_t)(si + c_attach), 0xFFFF);
        seg_w8(sd, (uint16_t)(si + c_msource), saved_dh);
        seg_w8(sd, (uint16_t)(di + c_mimic), dl);
        /* success: bx = cx = 0. The asm used to return without setting
         * either (bx = rlr, cx = the caller's), so control_p's "jcxz
         * mimout" could take a working ctl-P for a failure -- fixed in
         * both, docs/bugs.md #108 */
        (void)bx2;
        cio_bxcx r = {0, 0};
        return r;
    }
mim_err:
    {
        cio_bxcx r = {0xFFFF, e_no_mimic};
        return r;
    }
}

/* unmimic_entry -- un-mimic a device; input: dl = device */
cio_bxcx unmimic_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl_in) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dl = dl_in;
    uint8_t dh = dl;
    uint16_t bx;
    uint16_t si = getccbadr(phys, sysdat, dh, &bx);

    if (seg_r16(sd, (uint16_t)(si + c_attach)) != 0xFFFF) {
        cio_bxcx r = {0xFFFF, e_no_mimic};
        return r;
    }
    uint16_t di = si; /* push si ... pop di */
    dh = seg_r8(sd, (uint16_t)(si + c_msource));
    uint16_t bx2;
    si = getccbadr(phys, sysdat, dh, &bx2);

    if (seg_r8(sd, (uint16_t)(si + c_mimic)) != dl) {
        cio_bxcx r = {0xFFFF, e_no_mimic};
        return r;
    }
    bx = seg_r16(sd, SYSDAT_RLR);
    seg_w16(sd, (uint16_t)(di + c_attach), bx);
    seg_w8(sd, (uint16_t)(di + c_msource), 0xFF);
    uint8_t al = seg_r8(sd, (uint16_t)(di + c_mimic));
    seg_w8(sd, (uint16_t)(si + c_mimic), al);
    al = seg_r8(sd, (uint16_t)(bx + p_lst));
    seg_w8(sd, (uint16_t)(bx + p_lst), dl);
    ciodetach(phys, sysdat, p_lst);
    seg_w8(sd, (uint16_t)(bx + p_lst), al);
    /* the real asm saves/restores bx (rlr) around the lstdetach_entry
     * call and never zeroes it afterward -- it's left holding rlr */
    cio_bxcx r = {bx, 0};
    return r;
}
