/*
 * proc.c -- C port of all 3 of src/kernel/PROC.RTM's routines: freepd,
 * proc_creat (+ internal getpdadr).
 *
 * `getpdadr` is the near-identical sibling of src-c/kernel/que2.c's
 * own `getqdaddr` (same cs:sysdat/endseg bounds check, same "grab a
 * free table entry if the caller's address isn't already inside
 * MPM's own segment range" fallback) -- `sysdat` is passed in as a
 * plain parameter here too, the real asm's own `cs:sysdat`, same
 * convention as que1.c/que2.c/ml.c. Unlike `getqdaddr`'s own port,
 * the fallback copy here uses a real dynamically-supplied segment
 * (`mem_r8_seg(mem, wrkseg, ...)`) for its source, since this routine
 * is the one docs/src-c.md flagged as genuinely needing it (not just
 * a moot DS-switch): `proc_creat` itself needs real segmentation for
 * much more below.
 *
 * `proc_creat` writes through THREE different segments in one call:
 * the newly created child's own UDA (`p_uda[si]`, computed fresh each
 * call -- the same real infrastructure gap src-c/kernel/ml.c's own
 * `mla_initmau` exists for for MAU.MEM, see defs.h's own header
 * comment on the `u_*` UDA.FMT offsets above), that same child's own
 * *user stack* segment (`ds:u_ss`, read back out of the UDA it just
 * wrote), and the real, absolute interrupt-vector-table segment 0
 * (`ds,0` for `u_ivectors`/`i_mpm_ip`) -- which happens to need no
 * real segment arithmetic since paragraph 0 offset N is just N.
 * `tools/i86sim.py`'s `segmented=True` mode (see ml.c's own header
 * comment) models all of this.
 *
 * The real asm's own ambient ES (the *creator's* own UDA, read for
 * the password-inherit copy) isn't a register this port carries --
 * derived instead from the creator's own PD (`p_uda[rlr]`), which by
 * this system's own invariant already holds that creator's own
 * physical UDA segment (set by this exact routine, the first time
 * *that* process was itself created).
 *
 * The "share memory" loop (`cp_mnxt`) walks the creator's own
 * `ms_link`-chained `p_mem` list purely to call the still-unported
 * `mpmif(f_share)` for each element -- a boundary stub per docs/
 * src-c.md's convention, modeled as an instant no-op, so (same as
 * que2.c's own `rlsmx`) the loop itself has no observable effect on
 * memory; kept for fidelity anyway.
 *
 * `proc_creat`'s own tail chain (`pop dx ! jmp proc_creat`, draining
 * a whole linked list of pending creations via each processed PD's
 * own `p_link`) is an ordinary C loop here; a `getpdadr` failure on
 * any link stops the whole chain immediately (`bx=0ffffh ! ret`),
 * matching the real asm exactly -- it never resumes the remainder of
 * the chain afterward.
 */
#include <stdint.h>

#include "defs.h"

/*
 * getpdadr -- Make sure pd address is in operating system. If not,
 * copy into one in pd table. Make address relative to sysdat.
 *	input:	dx = pd address in u_wrkseg
 *		sysdat = this module's own copy of the SYSDAT segment
 *		(real asm: `cs:sysdat`)
 *	output:	returns pd address in sysdat; *cx_out = 0 on success,
 *		e_no_pd if the PD-Unused-List is empty (dx returned
 *		unmodified on that path, same as the real asm)
 */
static uint16_t getpdadr(uint8_t *mem, uint16_t dx, uint16_t sysdat, uint16_t *cx_out) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t ax = (uint16_t)(wrkseg + (dx >> 4));
    uint16_t endseg = mem_r16(mem, SD_ENDSEG);
    if (ax >= sysdat && ax < endseg && ax < (uint16_t)(sysdat + 0x1000)) {
        uint16_t off = (uint16_t)((wrkseg - sysdat) << 4);
        *cx_out = 0;
        return (uint16_t)(dx + off);
    }

    /* gpd_bad */
    uint16_t di = mem_r16(mem, SD_PUL);
    if (di == 0) {
        *cx_out = e_no_pd;
        return dx;
    }
    mem_w16(mem, SD_PUL, mem_r16(mem, (uint16_t)(di + p_link)));
    for (unsigned i = 0; i < pdlen; i++) {
        mem[(uint16_t)(di + i)] = mem_r8_seg(mem, wrkseg, (uint16_t)(dx + i));
    }
    mem_w16(mem, (uint16_t)(di + p_flag), (uint16_t)(mem_r16(mem, (uint16_t)(di + p_flag)) | pf_table));
    *cx_out = 0;
    return di;
}

/*
 * freepd -- Free Process Descriptor. Assumes no memory and on no
 * list except thread.
 *	input:	si = pd address
 */
void freepd(uint8_t *mem, uint16_t si) {
    uint16_t di = (uint16_t)(SD_THRDRT - p_thread); /* bias so the first
        read below lands on thrdrt itself, matching the real "mov
        di,(offset thrdrt)-p_thread" */
    uint16_t bx;
    for (;;) {
        bx = mem_r16(mem, (uint16_t)(di + p_thread));
        if (bx == si) {
            break;
        }
        di = bx;
    }
    mem_w16(mem, (uint16_t)(di + p_thread), mem_r16(mem, (uint16_t)(si + p_thread)));
    if (mem_r16(mem, (uint16_t)(si + p_flag)) & pf_table) {
        uint16_t pul = mem_r16(mem, SD_PUL);
        mem_w16(mem, (uint16_t)(si + p_link), pul);
        mem_w16(mem, SD_PUL, si);
    }
}

/*
 * proc_creat -- Create Process (drains dx's own p_link chain of
 * pending creations, one proc_creat per link).
 *	input:	dx = pd address in u_wrkseg (0 = nothing to do)
 *		sysdat = this module's own copy of the SYSDAT segment
 *	output:	returns 0 (dx was 0), 0xFFFF (getpdadr failed), or
 *		(real asm leaves no other observable bx value)
 */
uint16_t proc_creat(uint8_t *mem, uint16_t dx, uint16_t sysdat) {
    for (;;) {
        if (dx == 0) {
            return 0;
        }

        uint16_t cx;
        uint16_t si = getpdadr(mem, dx, sysdat, &cx);
        if (cx != 0) {
            return 0xFFFF;
        }

        /* cp_gpd -- capture the next pending creation in si's own
         * p_link chain before anything below overwrites it */
        uint16_t next_dx = mem_r16(mem, (uint16_t)(si + p_link));

        /* cp_uda -- set Parent */
        uint16_t bx = mem_r16(mem, SD_RLR);
        mem_w16(mem, (uint16_t)(si + p_parent), bx);
        mem_w16(mem, (uint16_t)(bx + p_flag), (uint16_t)(mem_r16(mem, (uint16_t)(bx + p_flag)) & (uint16_t)~pf_childabort));

        uint8_t al = mem[(uint16_t)(si + p_lst)];
        if (al >= mem[SD_NLSTDEV]) {
            al = 0;
        }
        al = (uint8_t)(al + mem[SD_NCONDEV]);
        mem[(uint16_t)(si + p_lst)] = al;
        mem_w16(mem, (uint16_t)(si + p_flag), (uint16_t)(mem_r16(mem, (uint16_t)(si + p_flag)) & (uint16_t)~pf_resource));

        /* share memory -- boundary stub (mpmif(f_share)), no
         * observable effect; see header comment */
        if (mem_r16(mem, (uint16_t)(bx + p_mem)) != 0 && mem_r16(mem, (uint16_t)(si + p_mem)) == 0) {
            uint16_t walk = (uint16_t)(bx + p_mem);
            for (;;) {
                walk = mem_r16(mem, (uint16_t)(walk + ms_link));
                if (walk == 0) {
                    break;
                }
                /* call mpmif(f_share) -- boundary stub, no-op */
            }
        }

        /* cp_nm -- set physical UDA segment */
        uint16_t creator_wrkseg = mem_r16(mem, UDA_WRKSEG);
        uint16_t uda_seg = (uint16_t)(mem_r16(mem, (uint16_t)(si + p_uda)) + creator_wrkseg);
        mem_w16(mem, (uint16_t)(si + p_uda), uda_seg);

        /* inherit password -- from the creator's (rlr's) own UDA, the
         * real asm's ambient ES on entry */
        uint16_t creator_uda = mem_r16(mem, (uint16_t)(bx + p_uda));
        for (unsigned i = 0; i < 8; i++) {
            mem_w8_seg(mem, uda_seg, (uint16_t)(u_df_password + i),
                       mem_r8_seg(mem, creator_uda, (uint16_t)(u_df_password + i)));
        }

        /* set initial segment values -- default any still-0 field to
         * the creator's own u_wrkseg */
        if (mem_r16_seg(mem, uda_seg, u_initcs) == 0) {
            mem_w16_seg(mem, uda_seg, u_initcs, creator_wrkseg);
        }
        if (mem_r16_seg(mem, uda_seg, u_initds) == 0) {
            mem_w16_seg(mem, uda_seg, u_initds, creator_wrkseg);
        }
        if (mem_r16_seg(mem, uda_seg, u_initss) == 0) {
            mem_w16_seg(mem, uda_seg, u_initss, creator_wrkseg);
        }
        if (mem_r16_seg(mem, uda_seg, u_inites) == 0) {
            mem_w16_seg(mem, uda_seg, u_inites, creator_wrkseg);
        }

        /* interrupt vector save areas -- DX=0; set ints 0-4 from the
         * real, absolute interrupt vector table (segment 0) */
        for (unsigned i = 0; i < 10; i++) {
            mem_w16_seg(mem, uda_seg, (uint16_t)(u_ivectors + 2 * i), mem_r16(mem, (uint16_t)(2 * i)));
        }
        /* set mpmint,debugint -- 4 words: mpm_ip/mpm_cs/debug_ip/debug_cs */
        for (unsigned i = 0; i < 4; i++) {
            mem_w16_seg(mem, uda_seg, (uint16_t)(u_mpm_ip + 2 * i), mem_r16(mem, (uint16_t)(i_mpm_ip + 2 * i)));
        }

        /* set user's stack */
        uint16_t initss = mem_r16_seg(mem, uda_seg, u_initss);
        mem_w16_seg(mem, uda_seg, u_ss, initss);
        mem_w16_seg(mem, uda_seg, u_sp, mem_r16_seg(mem, uda_seg, u_stack_sp));

        /* set other uda values */
        mem_w8_seg(mem, uda_seg, u_error_mode, 0);
        mem_w8_seg(mem, uda_seg, u_pd_cnt, 0);
        mem_w8_seg(mem, uda_seg, u_mult_cnt, 1);
        mem_w8_seg(mem, uda_seg, u_in_int, 0xFF); /* true (src/kernel/
            SYSTEM.DEF: true equ 0ffffh -- truncates to 0xFF in this
            byte field, same as the real asm's own "mov ds:u_in_int,
            true" would) */
        mem_w8_seg(mem, uda_seg, u_insys, 0);
        uint16_t initds = mem_r16_seg(mem, uda_seg, u_initds);
        mem_w16_seg(mem, uda_seg, u_ds_sav, initds);
        mem_w16_seg(mem, uda_seg, u_dma_seg, initds);
        uint16_t inites = mem_r16_seg(mem, uda_seg, u_inites);
        mem_w16_seg(mem, uda_seg, u_es_sav, inites);
        mem_w16_seg(mem, uda_seg, u_dparam, 0);
        mem_w16_seg(mem, uda_seg, u_flag_sav, 0);

        /* setup user stack for iret -- `u_sp`/`u_initcs` below are bare
         * (no explicit segment override) in the real asm, unlike every
         * other UDA.FMT field access in this routine: since they're
         * declared in UDA.FMT's own `eseg`, a bare reference to them
         * resolves through the ambient ES (this whole routine's own
         * child-UDA segment, set once during the password-inherit
         * step and never changed again), not through DS -- even
         * though DS was *just* switched to the user stack segment on
         * the line above. Confirmed directly against the real
         * assembler's own encoding (an implicit ES: prefix byte on
         * both bare references) -- this one genuinely surprised a
         * first differential-test pass, which assumed naively that
         * DS was already the right segment for both reads. */
        uint16_t user_ss = mem_r16_seg(mem, uda_seg, u_ss);
        uint16_t di = mem_r16_seg(mem, uda_seg, u_sp); /* sp->offset */
        uint16_t initcs = mem_r16_seg(mem, uda_seg, u_initcs);
        mem_w16_seg(mem, user_ss, (uint16_t)(di + 2), initcs); /* ->segment */
        mem_w16_seg(mem, user_ss, (uint16_t)(di + 4), 0x0200); /* ->flags */

        /* put on thread list */
        mem_w16(mem, (uint16_t)(si + p_thread), mem_r16(mem, SD_THRDRT));
        mem_w16(mem, SD_THRDRT, si);

        /* put on dispatcher ready list */
        mem_w16(mem, (uint16_t)(si + p_link), mem_r16(mem, SD_DRL));
        mem_w16(mem, SD_DRL, si);

        /* do the next process */
        dx = next_dx;
    }
}
