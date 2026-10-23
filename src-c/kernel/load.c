/*
 * load.c -- C port of a self-contained tail portion of src/kernel/
 * LOAD.SUP's `load:` routine: from "init Base Page" through
 * `load_exit` (the real asm's own `lb_more`..`load_exit`), ported
 * here as `load_finish`.
 *
 * `load:`'s own front half -- reading the .CMD header off disk and
 * populating `ldtab` (`get_ch_info`'s own `diskread` calls), then
 * allocating and spreading memory across `ldtab`'s entries (`lt_more`
 * onward, more `mpmif(f_malloc)` calls) -- is NOT ported here: unlike
 * every file ported so far, where `mpmif` calls are decorative (a
 * boundary-stub no-op whose result nothing downstream actually
 * depends on), nearly every branch in that front half depends on a
 * *real* disk-read or allocation result (`jcxz`/`cmp bx,0ffffh`
 * against registers a no-op stub never meaningfully sets). Disk I/O
 * (`f_freadseq`, via `diskread`/`load_group`) isn't ported anywhere
 * in this tree yet, so that front half stays deferred.
 *
 * This tail portion has no `mpmif` dependency of its own: it just
 * builds a Base Page and initializes a new PD's own UDA/LDSTK from an
 * already-populated `ldtab` (+ `load_pd`/`load_nldtabents`/`load_fcb`)
 * -- taken here as given input, the same way other tests in this tree
 * pre-build PD/MD/SAT structures by hand rather than deriving them
 * from a still-unported pipeline. It needs the same real
 * segment-relative-addressing this project already solved for ml.c/
 * proc.c/mau.c/share.c: the chosen group's own Base Page segment
 * (`ldtab_start[si]`), the new process's own UDA segment
 * (`load_uda`), and that same process's own user-stack segment
 * (`load_lstk`, via `u_initss`) are all genuinely different,
 * dynamically-computed segments.
 *
 * `cs:sysdat` and `cs` itself (`ls_rcseg = cs`) are passed in as
 * plain parameters, same convention as `sysdat` elsewhere
 * (que1.c/que2.c/ml.c/proc.c) -- `user_retf`'s own offset is passed
 * the same way, since it's a fixed code address this port has no
 * "cs-resident label" to resolve on its own.
 *
 * `ls_sp`/`ls_offset`/`ls_cseg`/`ls_flags` below are the *second*
 * half of a two-file relationship with src-c/kernel/proc.c's own
 * `proc_creat`: this routine sets `u_stack_sp = offset(ls_sp)` (a
 * fixed offset *within the same segment* `load_lstk`/`u_initss`
 * resolves to) and fills in `ls_offset`/`ls_flags`/`ls_roffset`/
 * `ls_rcseg` directly; `proc_creat`'s own later, more generic
 * "setup user stack for iret" step (which doesn't know anything
 * LOAD.SUP-specific) copies `u_stack_sp`'s value into `u_sp`, then
 * reads it back as an offset into the user stack segment to fill in
 * *that* same structure's own `ls_cseg`/`ls_flags` fields a second
 * time -- confirmed by direct arithmetic: `proc_creat`'s own
 * `di+2`/`di+4` writes land exactly on `ls_cseg`/`ls_flags` (0x56+2
 * = 0x58, 0x56+4 = 0x5A), and the `0x0200` flags value matches this
 * file's own literal exactly. This cross-confirms both ports.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} load_bxcx;

load_bxcx load_finish(uint8_t *mem, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    uint8_t load_8080 = 0;
    uint16_t n = mem_r16(mem, SD_LOAD_NLDTABENTS);

    /* lb_more -- find the first Data group (type 2) */
    uint16_t si = SD_LOAD_LDTAB;
    int found = 0;
    for (uint16_t i = 0; i < n; i++, si = (uint16_t)(si + ldtablen)) {
        if (mem[si + ldtab_type] == 2) {
            found = 1;
            break;
        }
    }
    if (!found) {
        /* lbc_mre -- no Data group; fall back to the first Code group
         * (type 1), an 8080-model program */
        si = SD_LOAD_LDTAB;
        for (uint16_t i = 0; i < n; i++, si = (uint16_t)(si + ldtablen)) {
            if (mem[si + ldtab_type] == 1) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return (load_bxcx){0xFFFF, e_no_cseg}; /* load_giveup -- no
                memory-release loop to replicate: nothing was ever
                allocated by this ported tail on its own */
        }
        load_8080 = 1;
    }

    /* lb_found */
    uint16_t load_basep = mem_r16(mem, (uint16_t)(si + ldtab_start));
    mem_w16(mem, SD_LOAD_BASEP, load_basep);
    for (uint16_t i = 0; i < 0x5B / 2; i++) {
        mem_w16_seg(mem, load_basep, (uint16_t)(2 * i), 0);
    }
    mem_w8_seg(mem, load_basep, bpg_8080, load_8080);

    /* lbb_mre -- fill in each group's own clen/cseg-shaped slot */
    si = SD_LOAD_LDTAB;
    for (uint16_t i = 0; i < n; i++, si = (uint16_t)(si + ldtablen)) {
        uint8_t type = mem[si + ldtab_type];
        if (type == 0) {
            continue;
        }
        uint16_t slot = (uint16_t)(6 * (type - 1));
        uint16_t minv = mem_r16(mem, (uint16_t)(si + ldtab_min));
        mem_w8_seg(mem, load_basep, (uint16_t)(slot + 2), (uint8_t)(minv >> 12));
        mem_w16_seg(mem, load_basep, slot, (uint16_t)((uint16_t)(minv << 4) - 1));
        mem_w16_seg(mem, load_basep, (uint16_t)(slot + 3),
                    mem_r16(mem, (uint16_t)(si + ldtab_start)));
    }

    /* if 8080 model, copy CS info into DS info */
    if (load_8080) {
        for (uint16_t i = 0; i < 6; i++) {
            mem_w8_seg(mem, load_basep, (uint16_t)(6 + i), mem_r8_seg(mem, load_basep, i));
        }
    }

    /* init PD, UDA and LDSTK */
    uint16_t load_pd = mem_r16(mem, SD_LOAD_PD);
    if (load_pd == 0) {
        return (load_bxcx){load_basep, 0};
    }

    si = SD_LOAD_LDTAB; /* lip_1 -- the UDA+LSTK entry is always ldtab[0] */
    uint16_t bx = load_pd;
    uint16_t uda_abs = mem_r16(mem, (uint16_t)(si + ldtab_start));
    mem_w16(mem, (uint16_t)(bx + p_uda), uda_abs);
    uint16_t load_uda = uda_abs;
    mem_w16(mem, SD_LOAD_UDA, load_uda);
    uint16_t load_lstk = (uint16_t)(uda_abs + (ulen / 16));
    mem_w16(mem, SD_LOAD_LSTK, load_lstk);

    for (uint16_t i = 0; i < (ulen + lstklen) / 2; i++) {
        mem_w16_seg(mem, load_uda, (uint16_t)(2 * i), 0);
    }

    mem_w16(mem, (uint16_t)(bx + p_uda), (uint16_t)(uda_abs - sysdat));
    mem[bx + p_stat] = ps_run;
    mem[bx + p_prior] = 200;
    mem[bx + p_luser] = mem[bx + p_user];
    mem[bx + p_ldsk] = mem[bx + p_dsk];

    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t load_fcb = mem_r16(mem, SD_LOAD_FCB);
    uint8_t load_fcb_dr = mem_r8_seg(mem, wrkseg, load_fcb);
    if (load_fcb_dr != 0) {
        mem[bx + p_ldsk] = load_fcb_dr;
    }

    /* init UDA */
    mem_w16_seg(mem, load_uda, u_dma_ofst, bpg_dma);
    mem_w16_seg(mem, load_uda, u_dma_seg, load_basep);
    mem_w16_seg(mem, load_uda, u_initds, load_basep);
    mem_w16_seg(mem, load_uda, u_inites, load_basep);
    mem_w16_seg(mem, load_uda, u_initss, load_lstk);

    uint16_t cseg = mem_r16_seg(mem, load_basep, bpg_cseg);
    if (cseg == 0) {
        return (load_bxcx){0xFFFF, e_no_cseg};
    }
    mem_w16_seg(mem, load_uda, u_initcs, cseg);

    uint16_t eseg = mem_r16_seg(mem, load_basep, bpg_eseg);
    if (eseg != 0) {
        mem_w16_seg(mem, load_uda, u_inites, eseg);
    }

    mem_w16_seg(mem, load_uda, u_stack_sp, ls_sp);

    uint16_t ip = 0;
    if (mem_r8_seg(mem, load_basep, bpg_8080) != 0) {
        ip = 0x0100;
    }

    mem_w16_seg(mem, load_lstk, ls_offset, ip);
    mem_w16_seg(mem, load_lstk, ls_flags, 0x0200);
    mem_w16_seg(mem, load_lstk, ls_roffset, user_retf_offset);
    mem_w16_seg(mem, load_lstk, ls_rcseg, cs);

    return (load_bxcx){load_basep, 0};
}
