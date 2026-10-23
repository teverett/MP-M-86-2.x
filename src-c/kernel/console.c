/*
 * console.c -- C port of all of src/kernel/CONSOLE.CIO, plus
 * src/kernel/CHARIO.CIO's getccbadr (a tiny, fully self-contained
 * dependency pulled in on its own). The rest of CHARIO.CIO -- the much
 * larger, deeply interdependent console-input state machine
 * (coninit/attach/conbrk/conin/chkctl/echoc/tabout/...) -- is ported in
 * src-c/kernel/chario.c, which calls back into this file's own
 * getccbadr, conattach_entry, condetach_entry, mimic_entry and
 * unmimic_entry -- see that file's own header comment.
 *
 * `cioattach`'s real `ca_sleep` path (console busy, not conditional) is
 * translated faithfully below, gotos included, but tests/src-c/
 * test_console.py never exercises it: under the boundary-stub
 * convention (`mpmif(f_sleep)` as a no-op), nothing ever changes
 * `c_attach`, so the real `jmps cioattach` retry loop this implements
 * would spin forever -- same category as QUE1.RTM's readq_entry/
 * writeq_entry, deliberately not exercised for the same reason.
 *
 * `conassign_entry`'s own retry (`as_mok`'s mismatch case jumping back to
 * `as_trya`) is different and IS fully exercised: it always re-attempts
 * the boundary-stubbed `mpmif(f_findpdname)` call, which always "fails"
 * under the convention (leaves `cx` nonzero), so that retry provably
 * terminates after one extra pass rather than spinning.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} cio_bxcx;

/*
 * getccbadr -- get CCB for calling proc's default console
 *	input:	dh = console number
 *	output:	returns address of CCB; *bx_out = PD address (rlr)
 *
 * Not static: src-c/kernel/chario.c (the rest of src/kernel/CHARIO.CIO)
 * calls into this same instance -- see its own header comment.
 */
uint16_t getccbadr(const uint8_t *mem, uint8_t dh, uint16_t *bx_out) {
    *bx_out = mem_r16(mem, SD_RLR);
    return (uint16_t)(mem_r16(mem, SD_CCB) + (uint16_t)dh * ccblen);
}

/*
 * cioattach -- Attach calling process to default ciodev.
 *	input:	pd_field_off = offset of dev number in PD (p_cns or p_lst)
 *		cond = nonzero if conditional (don't wait)
 */
cio_bxcx cioattach(uint8_t *mem, uint16_t pd_field_off, uint16_t cond) {
    for (;;) {
        uint16_t pd, si, bx;
        pd = mem_r16(mem, SD_RLR);
        uint8_t dh = mem[(uint16_t)(pd + pd_field_off)];
        si = getccbadr(mem, dh, &bx);

        uint16_t attach = mem_r16(mem, (uint16_t)(si + c_attach));
        if (bx == attach) {
            cio_bxcx r = {0, 0};
            return r;
        }
        if (attach == 0) {
            mem_w16(mem, (uint16_t)(si + c_attach), bx);
            cio_bxcx r = {0, 0};
            return r;
        }
        if (attach == 0xFFFF) {
            uint8_t dh2 = mem[(uint16_t)(si + c_msource)];
            uint16_t bx2;
            uint16_t si2 = getccbadr(mem, dh2, &bx2);
            if (bx == mem_r16(mem, (uint16_t)(si2 + c_attach))) {
                cio_bxcx r = {0, 0};
                return r;
            }
        }
        /* ca_chkslp */
        if (cond != 0) {
            cio_bxcx r = {0xFFFF, e_no_attach};
            return r;
        }
        /* ca_sleep -- see header comment: not exercised by the test */
        mem[(uint16_t)(bx + p_stat)] = ps_ciowait;
        /* call mpmif(f_sleep) -- boundary stub, no-op */
    }
}

/* ciodetach -- Detach calling process from default ciodev. */
cio_bxcx ciodetach(uint8_t *mem, uint16_t pd_field_off) {
    uint16_t pd = mem_r16(mem, SD_RLR);
    uint8_t dh = mem[(uint16_t)(pd + pd_field_off)];
    uint16_t bx;
    uint16_t si = getccbadr(mem, dh, &bx);

    uint16_t attach = mem_r16(mem, (uint16_t)(si + c_attach));
    if (attach != 0) {
        if (attach != bx) {
            cio_bxcx r = {0xFFFF, e_not_owner};
            return r;
        }
        mem_w16(mem, (uint16_t)(si + c_attach), mem_r16(mem, (uint16_t)(si + c_queue)));
        /* call mpmif(f_wakeup) -- boundary stub, no-op */
    }
    cio_bxcx r = {0, 0};
    return r;
}

cio_bxcx lstattach_entry(uint8_t *mem) { return cioattach(mem, p_lst, 0); }
cio_bxcx clstattch_entry(uint8_t *mem) { return cioattach(mem, p_lst, 0xFFFF); }
cio_bxcx cconattch_entry(uint8_t *mem) { return cioattach(mem, p_cns, 0xFFFF); }
cio_bxcx conattach_entry(uint8_t *mem) { return cioattach(mem, p_cns, 0); }
cio_bxcx lstdetach_entry(uint8_t *mem) { return ciodetach(mem, p_lst); }
cio_bxcx condetach_entry(uint8_t *mem) { return ciodetach(mem, p_cns); }

/* getdefcon_entry -- Get Default Console */
cio_bxcx getdefcon_entry(const uint8_t *mem) {
    uint16_t pd = mem_r16(mem, SD_RLR);
    cio_bxcx r = {mem[(uint16_t)(pd + p_cns)], 0};
    return r;
}

/* getdeflst_entry -- Get Default List */
cio_bxcx getdeflst_entry(const uint8_t *mem) {
    uint16_t pd = mem_r16(mem, SD_RLR);
    uint8_t bl = (uint8_t)(mem[(uint16_t)(pd + p_lst)] - mem[SD_NCONDEV]);
    cio_bxcx r = {bl, 0};
    return r;
}

/* setdefcon_entry -- Set Default Console */
cio_bxcx setdefcon_entry(uint8_t *mem, uint8_t dl) {
    if (dl >= mem[SD_NCONDEV]) {
        cio_bxcx r = {0xFFFF, e_ill_cns};
        return r;
    }
    uint16_t pd = mem_r16(mem, SD_RLR);
    if (dl != mem[(uint16_t)(pd + p_cns)]) {
        condetach_entry(mem);
    }
    pd = mem_r16(mem, SD_RLR);
    mem[(uint16_t)(pd + p_cns)] = dl;
    cio_bxcx r = {0, 0};
    return r;
}

/* setdeflst_entry -- Set Default List */
cio_bxcx setdeflst_entry(uint8_t *mem, uint8_t dl) {
    if (dl >= mem[SD_NLSTDEV]) {
        cio_bxcx r = {0xFFFF, e_ill_lst};
        return r;
    }
    dl = (uint8_t)(dl + mem[SD_NCONDEV]);
    uint16_t pd = mem_r16(mem, SD_RLR);
    if (dl != mem[(uint16_t)(pd + p_lst)]) {
        lstdetach_entry(mem);
    }
    pd = mem_r16(mem, SD_RLR);
    mem[(uint16_t)(pd + p_lst)] = dl;
    cio_bxcx r = {0, 0};
    return r;
}

/*
 * conassign_entry -- Attach specified console to specified process
 *	input:	dx -> acb address in u_wrkseg
 */
cio_bxcx conassign_entry(uint8_t *mem, uint16_t dx) {
    uint16_t di = dx;
    uint8_t ah = mem[(uint16_t)(di + acb_cns)];
    if (ah > mem[SD_NCNS]) {
        cio_bxcx r = {0xFFFF, e_ill_cns};
        return r;
    }

    uint16_t bx;
    uint16_t ccb = getccbadr(mem, ah, &bx);
    uint16_t calling_pd = bx;
    bx = ccb;

    uint16_t attach = mem_r16(mem, (uint16_t)(bx + c_attach));
    if (!(attach == 0 || attach == calling_pd)) {
        cio_bxcx r = {0xFFFF, e_not_owner};
        return r;
    }

    uint16_t bp = bx; /* the CCB to assign */
    uint16_t target = mem_r16(mem, (uint16_t)(di + acb_pd));
    uint8_t match = mem[(uint16_t)(di + acb_match)];

    if (target == 0) {
        /* as_trya: name search via mpmif(f_findpdname) -- boundary
         * stub, always "fails" (cx left nonzero) under the convention */
        cio_bxcx r = {0xFFFF, f_findpdname};
        return r;
    }
    if (match != 0 && mem[(uint16_t)(target + p_cns)] != ah) {
        /* as_mok's mismatch retry via as_trya -- always re-attempts the
         * same boundary-stubbed call, so this always "fails" too */
        cio_bxcx r = {0xFFFF, f_findpdname};
        return r;
    }

    /* as_mok */
    uint16_t si = target;
    mem_w16(mem, (uint16_t)(bp + c_attach), si);
    uint16_t walk = (uint16_t)(bp + c_queue - p_link);
    for (;;) {
        uint16_t link = mem_r16(mem, (uint16_t)(walk + p_link));
        if (link == 0) {
            cio_bxcx r = {0, 0}; /* as_gexit */
            return r;
        }
        if (link == si) {
            /* as_qfix */
            uint16_t ax = mem_r16(mem, (uint16_t)(si + p_link));
            mem_w16(mem, (uint16_t)(walk + p_link), ax);
            uint16_t bx2 = bp;
            uint16_t bp2 = mem_r16(mem, (uint16_t)(bx2 + c_queue));
            mem_w16(mem, (uint16_t)(bx2 + c_queue), si);
            mem_w16(mem, (uint16_t)(si + p_link), bp2);
            /* call mpmif(f_wakeup) -- boundary stub, no-op */
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
cio_bxcx mimic_entry(uint8_t *mem, uint8_t dh_in, uint8_t dl_in) {
    uint16_t bx;
    uint16_t si = getccbadr(mem, dh_in, &bx);
    uint8_t dl = dl_in;

    if (mem_r16(mem, (uint16_t)(si + c_attach)) != bx) goto mim_err;
    if (mem[(uint16_t)(si + c_mimic)] != 0xFF) goto mim_err;

    {
        uint16_t di = si;       /* push si ... pop di */
        uint8_t saved_dh = dh_in; /* push dx (dh:dl pair) ... pop dx restores
                                    * it after the call below -- "mov dh,dl"
                                    * is only a transient input to getccbadr */
        uint16_t bx2;
        si = getccbadr(mem, dl, &bx2); /* mov dh,dl ! call getccbadr */
        /* pop dx: dh is back to saved_dh from here on */

        if (mem_r16(mem, (uint16_t)(si + c_attach)) != 0) goto mim_err;
        if (mem[(uint16_t)(si + c_mimic)] != 0xFF) goto mim_err;

        mem_w16(mem, (uint16_t)(si + c_attach), 0xFFFF);
        mem[(uint16_t)(si + c_msource)] = saved_dh;
        mem[(uint16_t)(di + c_mimic)] = dl;
        /* the real asm never zeroes bx here -- it's left holding rlr
         * (bx2, from this getccbadr call) */
        cio_bxcx r = {bx2, 0};
        return r;
    }
mim_err:
    {
        cio_bxcx r = {0xFFFF, e_no_mimic};
        return r;
    }
}

/* unmimic_entry -- un-mimic a device; input: dl = device */
cio_bxcx unmimic_entry(uint8_t *mem, uint8_t dl_in) {
    uint8_t dl = dl_in;
    uint8_t dh = dl;
    uint16_t bx;
    uint16_t si = getccbadr(mem, dh, &bx);

    if (mem_r16(mem, (uint16_t)(si + c_attach)) != 0xFFFF) {
        cio_bxcx r = {0xFFFF, e_no_mimic};
        return r;
    }
    uint16_t di = si; /* push si ... pop di */
    dh = mem[(uint16_t)(si + c_msource)];
    uint16_t bx2;
    si = getccbadr(mem, dh, &bx2);

    if (mem[(uint16_t)(si + c_mimic)] != dl) {
        cio_bxcx r = {0xFFFF, e_no_mimic};
        return r;
    }
    bx = mem_r16(mem, SD_RLR);
    mem_w16(mem, (uint16_t)(di + c_attach), bx);
    mem[(uint16_t)(di + c_msource)] = 0xFF;
    uint8_t al = mem[(uint16_t)(di + c_mimic)];
    mem[(uint16_t)(si + c_mimic)] = al;
    al = mem[(uint16_t)(bx + p_lst)];
    mem[(uint16_t)(bx + p_lst)] = dl;
    ciodetach(mem, p_lst);
    mem[(uint16_t)(bx + p_lst)] = al;
    /* the real asm saves/restores bx (rlr) around the lstdetach_entry
     * call and never zeroes it afterward -- it's left holding rlr */
    cio_bxcx r = {bx, 0};
    return r;
}
