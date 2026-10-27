/*
 * proc.c -- C port of all 3 of src-assem/kernel/PROC.RTM's routines: freepd,
 * proc_creat (+ internal getpdadr).
 *
 * Runs against real memory (defs.h's far_ptr), like init.c and
 * rtmif.c's rtmif_init: `phys` is the base of the real 8086 physical
 * address space and `sysdat` is the real SYSDAT segment (the real
 * asm's ambient DS, and its `cs:sysdat`). Each access goes through the
 * segment the real asm uses:
 *   - PDs, the thread/ready/unused lists, and memory descriptors are
 *     DS-relative: offsets within SYSDAT (SYSDAT_* fields, PD
 *     addresses).
 *   - The creator's u_wrkseg and password come from its UDA, the real
 *     asm's ambient ES -- the running process's UDA, p_uda[rlr]
 *     (defs.h's rlr_uda).
 *   - The new child's UDA and user stack are the segments proc_creat
 *     computes and reads back.
 *   - The interrupt vectors it snapshots are at segment 0.
 *
 * `getpdadr` is the near-identical sibling of que2.c's `getqdaddr`
 * (same cs:sysdat/endseg bounds check, same "grab a free table entry
 * if the caller's address isn't already inside MPM's own segment
 * range" fallback).
 *
 * The "share memory" loop (`cp_mnxt`) walks the creator's own
 * `ms_link`-chained `p_mem` list purely to call the still-unported
 * `mpmif(f_share)` for each element -- a boundary stub, so the loop
 * has no observable effect; kept for fidelity.
 *
 * `proc_creat`'s own tail chain (`pop dx ! jmp proc_creat`, draining
 * a whole linked list of pending creations via each processed PD's
 * own `p_link`) is an ordinary C loop here; a `getpdadr` failure on
 * any link stops the whole chain immediately (`bx=0ffffh ! ret`),
 * matching the real asm exactly.
 */
#include <stdint.h>

#include "defs.h"

/*
 * getpdadr -- Make sure pd address is in operating system. If not,
 * copy into one in pd table. Make address relative to sysdat.
 *	input:	dx = pd address in the creator's u_wrkseg
 *	output:	returns pd address in sysdat; *cx_out = 0 on success,
 *		e_no_pd if the PD-Unused-List is empty (dx returned
 *		unmodified on that path, same as the real asm)
 */
static uint16_t getpdadr(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *cx_out) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t wrkseg = seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg);
    uint16_t ax = (uint16_t)(wrkseg + (dx >> 4));
    uint16_t endseg = seg_r16(sd, SYSDAT_ENDSEG);
    if (ax >= sysdat && ax < endseg && ax < (uint16_t)(sysdat + 0x1000)) {
        *cx_out = 0;
        return (uint16_t)(dx + (uint16_t)((wrkseg - sysdat) << 4));
    }

    /* gpd_bad */
    uint16_t di = seg_r16(sd, SYSDAT_PUL);
    if (di == 0) {
        *cx_out = e_no_pd;
        return dx;
    }
    seg_w16(sd, SYSDAT_PUL, seg_r16(sd, (uint16_t)(di + p_link)));
    const uint8_t *src = far_ptr(phys, wrkseg, 0);
    for (unsigned i = 0; i < pdlen; i++) {
        seg_w8(sd, (uint16_t)(di + i), seg_r8(src, (uint16_t)(dx + i)));
    }
    seg_w16(sd, (uint16_t)(di + p_flag), (uint16_t)(seg_r16(sd, (uint16_t)(di + p_flag)) | pf_table));
    *cx_out = 0;
    return di;
}

/*
 * freepd -- Free Process Descriptor. Assumes no memory and on no
 * list except thread.
 *	input:	si = pd address (in sysdat)
 */
void freepd(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t di = (uint16_t)(SYSDAT_THRDRT - p_thread); /* bias so the first
        read below lands on thrdrt itself, matching the real "mov
        di,(offset thrdrt)-p_thread" */
    for (;;) {
        uint16_t bx = seg_r16(sd, (uint16_t)(di + p_thread));
        if (bx == si) {
            break;
        }
        di = bx;
    }
    seg_w16(sd, (uint16_t)(di + p_thread), seg_r16(sd, (uint16_t)(si + p_thread)));
    if (seg_r16(sd, (uint16_t)(si + p_flag)) & pf_table) {
        seg_w16(sd, (uint16_t)(si + p_link), seg_r16(sd, SYSDAT_PUL));
        seg_w16(sd, SYSDAT_PUL, si);
    }
}

/*
 * proc_creat -- Create Process (drains dx's own p_link chain of
 * pending creations, one proc_creat per link).
 *	input:	dx = pd address in the creator's u_wrkseg (0 = nothing
 *		to do)
 *	output:	returns 0 (dx was 0) or 0xFFFF (getpdadr failed) -- the
 *		real asm leaves no other observable bx value.
 *		*dx_out, if non-NULL: dx's own real final value at the
 *		routine's own exit (0 on the success path, the PD
 *		address `getpdadr` failed on otherwise) -- needed so
 *		sysent.c's own `creat_proc_entry` can feed the real
 *		post-loop dx into `pdisp`, matching its own `jmp pdisp`
 */
uint16_t proc_creat(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t *dx_out) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    for (;;) {
        if (dx == 0) {
            if (dx_out) *dx_out = dx;
            return 0;
        }

        uint16_t cx;
        uint16_t si = getpdadr(phys, sysdat, dx, &cx);
        if (cx != 0) {
            if (dx_out) *dx_out = dx;
            return 0xFFFF;
        }

        /* cp_gpd -- capture the next pending creation in si's own
         * p_link chain before anything below overwrites it */
        uint16_t next_dx = seg_r16(sd, (uint16_t)(si + p_link));

        /* cp_uda -- set Parent */
        uint16_t bx = seg_r16(sd, SYSDAT_RLR);
        seg_w16(sd, (uint16_t)(si + p_parent), bx);
        seg_w16(sd, (uint16_t)(bx + p_flag),
                (uint16_t)(seg_r16(sd, (uint16_t)(bx + p_flag)) & (uint16_t)~pf_childabort));

        uint8_t al = seg_r8(sd, (uint16_t)(si + p_lst));
        if (al >= seg_r8(sd, SYSDAT_NLSTDEV)) {
            al = 0;
        }
        seg_w8(sd, (uint16_t)(si + p_lst), (uint8_t)(al + seg_r8(sd, SYSDAT_NCONDEV)));
        seg_w16(sd, (uint16_t)(si + p_flag),
                (uint16_t)(seg_r16(sd, (uint16_t)(si + p_flag)) & (uint16_t)~pf_resource));

        /* share memory -- boundary stub (mpmif(f_share)), no
         * observable effect; see header comment */
        if (seg_r16(sd, (uint16_t)(bx + p_mem)) != 0 && seg_r16(sd, (uint16_t)(si + p_mem)) == 0) {
            uint16_t walk = (uint16_t)(bx + p_mem);
            for (;;) {
                walk = seg_r16(sd, (uint16_t)(walk + ms_link));
                if (walk == 0) {
                    break;
                }
                /* call mpmif(f_share) -- boundary stub, no-op */
            }
        }
        /* p_mem 0FFFFh (TMP.A86's PD) only means "don't share the
         * creator's memory" -- it's no list (docs/bugs.md #122) */
        if (seg_r16(sd, (uint16_t)(si + p_mem)) == 0xFFFF) {
            seg_w16(sd, (uint16_t)(si + p_mem), 0);
        }

        /* cp_nm -- set physical UDA segment (creator's UDA is the
         * real asm's ambient ES) */
        uint8_t *creator = far_ptr(phys, seg_r16(sd, (uint16_t)(bx + p_uda)), 0);
        uint16_t creator_wrkseg = seg_r16(creator, u_wrkseg);
        uint16_t uda_seg = (uint16_t)(seg_r16(sd, (uint16_t)(si + p_uda)) + creator_wrkseg);
        seg_w16(sd, (uint16_t)(si + p_uda), uda_seg);
        uint8_t *uda = far_ptr(phys, uda_seg, 0);

        /* inherit password from the creator's UDA */
        for (unsigned i = 0; i < 8; i++) {
            seg_w8(uda, (uint16_t)(u_df_password + i), seg_r8(creator, (uint16_t)(u_df_password + i)));
        }

        /* set initial segment values -- default any still-0 field to
         * the creator's own u_wrkseg */
        static const uint16_t init_fields[] = {u_initcs, u_initds, u_initss, u_inites};
        for (unsigned i = 0; i < 4; i++) {
            if (seg_r16(uda, init_fields[i]) == 0) {
                seg_w16(uda, init_fields[i], creator_wrkseg);
            }
        }

        /* interrupt vector save areas: ints 0-4, then mpmint/debugint
         * (mpm_ip/mpm_cs/debug_ip/debug_cs), from segment 0 */
        const uint8_t *ivt = far_ptr(phys, 0, 0);
        for (unsigned i = 0; i < 10; i++) {
            seg_w16(uda, (uint16_t)(u_ivectors + 2 * i), seg_r16(ivt, (uint16_t)(2 * i)));
        }
        for (unsigned i = 0; i < 4; i++) {
            seg_w16(uda, (uint16_t)(u_mpm_ip + 2 * i), seg_r16(ivt, (uint16_t)(i_mpm_ip + 2 * i)));
        }

        /* set user's stack */
        seg_w16(uda, u_ss, seg_r16(uda, u_initss));
        seg_w16(uda, u_sp, seg_r16(uda, u_stack_sp));

        /* set other uda values */
        seg_w8(uda, u_error_mode, 0);
        seg_w8(uda, u_pd_cnt, 0);
        seg_w8(uda, u_mult_cnt, 1);
        seg_w8(uda, u_in_int, 0xFF); /* true (SYSTEM.DEF: true equ 0ffffh,
            truncated to this byte field the same way the real asm's
            "mov ds:u_in_int,true" is) */
        seg_w8(uda, u_insys, 0);
        uint16_t initds = seg_r16(uda, u_initds);
        seg_w16(uda, u_ds_sav, initds);
        seg_w16(uda, u_dma_seg, initds);
        seg_w16(uda, u_es_sav, seg_r16(uda, u_inites));
        seg_w16(uda, u_dparam, 0);
        seg_w16(uda, u_flag_sav, 0);

        /* setup user stack for iret -- `u_sp`/`u_initcs` are bare eseg
         * references in the real asm, so they resolve through ES (the
         * child's UDA), not through DS (just switched to the user
         * stack segment on the line before) */
        uint8_t *user_stack = far_ptr(phys, seg_r16(uda, u_ss), 0);
        uint16_t di = seg_r16(uda, u_sp); /* sp->offset */
        seg_w16(user_stack, (uint16_t)(di + 2), seg_r16(uda, u_initcs)); /* ->segment */
        seg_w16(user_stack, (uint16_t)(di + 4), 0x0200); /* ->flags */

        /* put on thread list */
        seg_w16(sd, (uint16_t)(si + p_thread), seg_r16(sd, SYSDAT_THRDRT));
        seg_w16(sd, SYSDAT_THRDRT, si);

        /* put on dispatcher ready list */
        seg_w16(sd, (uint16_t)(si + p_link), seg_r16(sd, SYSDAT_DRL));
        seg_w16(sd, SYSDAT_DRL, si);

        /* do the next process */
        dx = next_dx;
    }
}
