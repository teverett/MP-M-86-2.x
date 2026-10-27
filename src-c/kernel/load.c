/*
 * load.c -- C port of src-assem/kernel/LOAD.SUP's `load:`/`load_entry`/
 * `cload_entry`, plus the real disk I/O and memory allocation it
 * depends on: `diskread`, `load_group`, `get_ch_info`, and the tail
 * portion (`lb_more` through `load_exit`) ported as `load_finish`.
 *
 * `diskread` makes the BDOS calls the asm makes: `f_setdmab`/`f_setdma`
 * (file4.c's func51/func26, which set the running UDA's u_dma_seg/
 * u_dma_ofst) and then `f_freadseq`, a sequential read through the
 * BDOS's getdiskmx path, which isn't ported. That read is a boundary
 * hook (`load_set_readseq`, defs.h): it gets the FCB (u_wrkseg:
 * load_fcb) and the DMA address and returns the BDOS's AL (0 = ok,
 * 1 = end of file, 0FFh = error). With no hook set it returns 0FFh, so
 * a load fails with e_bad_read rather than loading garbage. An earlier
 * version of this file simulated a one-extent disk in the test array
 * itself; the tests now script the same records through the hook (and
 * through the reference's f_freadseq handler).
 *
 * The MXLoad mutex is real too: `load` reads `mxloadqpb` at the start
 * and writes it back at `load_exit`, through que1.c, as the asm does
 * through mpmif(f_qread/f_qwrite).
 *
 * `load`'s own `get_ch_info`/`lt_more`/`lt_spread` memory allocation
 * now calls `memory.c`'s own `malloc_entry`/`mfree_entry` for real --
 * both genuinely allocate/free now that `malloc_entry`'s own
 * `mlalloc`/`maualloc` calls are real internal calls too (see
 * `memory.c`'s own header comment). `ch_alloc`'s/the combined
 * relative-request malloc's own `mpmif(f_malloc)` reuses the target
 * `ldtab` entry itself as the MPB passed to `malloc_entry` directly --
 * faithful to the real asm, which does the exact same thing (`ldtab`'s
 * own `start`/`min`/`max`/`pd`/`attrib` fields share the real MPB's
 * layout byte-for-byte).
 *
 * `cload_entry`'s own chain-load mode is now ported too, as a thin
 * wrapper calling `load` with an explicit `load_chain` flag instead
 * of the real asm's own magic `bx==1` sentinel (see `cload_entry`'s
 * own header comment below for why). Its own memory-transfer cleanup
 * (`ld_fnext`/`ld_freeit`, inside `load`'s own `if (load_chain)`
 * block) depends on `cpmmem.c`'s own `cpmfree_entry` and `memory.c`'s
 * own `mfree_entry` both succeeding for a real MAU release -- both
 * are real now (see those files' own header comments for the pass
 * that made them so). `ld_freeit`'s own `mpmif(f_freemem)` resolves,
 * via `MEMIF.MEM`'s own `functab`, to `cpmmem.c`'s `cpmfree_entry`
 * itself (func#4 within the MEM module, same table `f_malloc`/
 * `f_memfree`'s own func#6/#7 live in) -- called here directly, in
 * free-all mode (`mcb_ext = 0xFF`), bypassing the dispatch table the
 * same way every other direct internal call in this project does.
 * `load` below now takes an explicit `int load_chain` parameter
 * instead of smuggling it through `bx` the way the real asm's shared
 * `load:` entry point does.
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
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the SYSDAT segment, the SUP's DS.
 * `load_*`, `ldtab` and `load_dma` are SYSDAT fields; the FCB is in the
 * running process's u_wrkseg. The MCB/MFPB the asm builds on its stack
 * for f_freemem/f_memfree (DS = SS = the running UDA) go at the UDA
 * stack-area spots UDA_STK_LOAD_MCB/UDA_STK_LOAD_MFPB, with u_wrkseg
 * pointed at the UDA for the call; f_malloc's MPB is the ldtab entry
 * itself (DS = SYSDAT, so u_wrkseg = SYSDAT for that call).
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

extern load_bxcx malloc_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern load_bxcx mfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern load_bxcx cpmfree_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern load_bxcx readq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern load_bxcx writeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern void func26_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);
extern void func51_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out);

load_bxcx load_finish(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs);

static load_readseq_fn load_readseq;

void load_set_readseq(load_readseq_fn fn) {
    load_readseq = fn;
}

/* mpmif from the SUP: u_wrkseg = the caller's DS for the call (SYSDAT,
 * or the UDA for a block built on the stack), restored afterwards */
static load_bxcx mem_call(uint8_t *phys, uint16_t sysdat, uint16_t ds,
                          load_bxcx (*fn)(uint8_t *, uint16_t, uint16_t), uint16_t dx) {
    uint8_t *uda = far_ptr(phys, rlr_uda(far_ptr(phys, sysdat, 0)), 0);
    uint16_t saved_wrkseg = seg_r16(uda, u_wrkseg);
    seg_w16(uda, u_wrkseg, ds);
    load_bxcx r = fn(phys, sysdat, dx);
    seg_w16(uda, u_wrkseg, saved_wrkseg);
    return r;
}

/*
 * diskread -- read the next sequential record of load_fcb into a
 * given DMA buffer.
 *	input:	dx = dma offset, cx = dma segment
 *	output:	bx=rlr, cx=0 on success/eof; bx=0xFFFF, cx=e_bad_read on
 *		a hard read error; bx=0xFFFF, cx=e_abort if the calling
 *		process's own pf_ctlc is set
 */
load_bxcx diskread(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t cx) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t bx_, cx_;
    func51_entry(phys, sysdat, cx, &bx_, &cx_); /* mpmif(f_setdmab) */
    func26_entry(phys, sysdat, dx, &bx_, &cx_); /* mpmif(f_setdma) */
    uint16_t wrkseg = seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg);
    uint8_t bl = load_readseq ? load_readseq(phys, wrkseg, seg_r16(sd, SYSDAT_LOAD_FCB), cx, dx) : 0xFF;
    if (bl == 0xFF) {
        return (load_bxcx){0xFFFF, e_bad_read};
    }
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    if (seg_r16(sd, (uint16_t)(rlr + p_flag)) & pf_ctlc) {
        return (load_bxcx){0xFFFF, e_abort};
    }
    return (load_bxcx){rlr, 0};
}

/*
 * load_group -- load a group described in ldtab, filling it from the
 * file (via diskread), reusing whatever's already sitting in the shared
 * load_dma buffer when possible.
 *	input:	si = ldtab entry address (a SYSDAT offset)
 *	output:	returns the error code (0 = ok)
 */
uint16_t load_group(uint8_t *phys, uint16_t sysdat, uint16_t si) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t bx = si;
    for (;;) {
        uint16_t indma = seg_r16(sd, SYSDAT_LOAD_INDMA);
        uint16_t fstrt = seg_r16(sd, (uint16_t)(bx + ldtab_fstrt));
        if (fstrt >= indma) {
            uint16_t off = (uint16_t)(fstrt - indma);
            if (off < 8) {
                uint16_t avail = (uint16_t)(8 - off);
                uint16_t flen = seg_r16(sd, (uint16_t)(bx + ldtab_flen));
                uint16_t take = (avail <= flen) ? avail : flen;
                uint16_t dest_seg = seg_r16(sd, (uint16_t)(bx + ldtab_start));
                uint8_t *dest = far_ptr(phys, dest_seg, 0);
                for (uint16_t i = 0; i < (uint16_t)(take * 16); i++) {
                    seg_w8(dest, i, seg_r8(sd, (uint16_t)(SYSDAT_LOAD_DMA + off * 16 + i)));
                }
                seg_w16(sd, (uint16_t)(bx + ldtab_start), (uint16_t)(dest_seg + take));
                seg_w16(sd, (uint16_t)(bx + ldtab_flen), (uint16_t)(flen - take));
                seg_w16(sd, (uint16_t)(bx + ldtab_fstrt), (uint16_t)(fstrt + take));
            }
        }

        uint16_t flen = seg_r16(sd, (uint16_t)(bx + ldtab_flen));
        if (flen == 0) {
            return 0;
        }

        if (flen >= 8) {
            /* xfer_direct -- a full record goes straight to the
             * destination segment, bypassing load_dma entirely */
            uint16_t dest_seg = seg_r16(sd, (uint16_t)(bx + ldtab_start));
            load_bxcx r = diskread(phys, sysdat, 0, dest_seg);
            if (r.cx != 0) return r.cx;
            seg_w16(sd, (uint16_t)(bx + ldtab_start), (uint16_t)(dest_seg + 8));
            seg_w16(sd, (uint16_t)(bx + ldtab_fstrt), (uint16_t)(seg_r16(sd, (uint16_t)(bx + ldtab_fstrt)) + 8));
            seg_w16(sd, (uint16_t)(bx + ldtab_flen), (uint16_t)(flen - 8));
            continue; /* jmp read_first */
        }

        /* read_1st -- less than a full record remains: read one into
         * the shared buffer, then restart from the top (jmp load_group) */
        load_bxcx r = diskread(phys, sysdat, SYSDAT_LOAD_DMA, sysdat);
        if (r.cx != 0) return r.cx;
        seg_w16(sd, SYSDAT_LOAD_INDMA, seg_r16(sd, (uint16_t)(bx + ldtab_fstrt)));
    }
}

/*
 * load_giveup -- release every ldtab entry (0..n inclusive -- n+1 of
 * them, since the combined relative request at ldtab[n] counts too)
 * that was actually allocated (`ldtab_id != 0`), via mfree_entry with
 * the MFPB on the stack, then report the given failure.
 *	input:	n = load_nldtabents; bx_fail/cx_fail = what to return
 */
static load_bxcx load_giveup(uint8_t *phys, uint16_t sysdat, uint16_t n, uint16_t bx_fail, uint16_t cx_fail) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t uda_seg = rlr_uda(sd);
    uint8_t *uda = far_ptr(phys, uda_seg, 0);
    for (uint16_t i = 0, bx = SYSDAT_LDTAB; i <= n; i++, bx = (uint16_t)(bx + ldtablen)) {
        uint16_t id = seg_r16(sd, (uint16_t)(bx + ldtab_id));
        if (id == 0) continue;
        seg_w16(uda, (uint16_t)(UDA_STK_LOAD_MFPB + mfpb_start), id);
        seg_w16(uda, (uint16_t)(UDA_STK_LOAD_MFPB + mfpb_pd), seg_r16(sd, (uint16_t)(bx + ldtab_pd)));
        mem_call(phys, sysdat, uda_seg, mfree_entry, UDA_STK_LOAD_MFPB);
    }
    return (load_bxcx){bx_fail, cx_fail};
}

/*
 * get_ch_info -- parse the .CMD header (already in load_dma) into
 * ldtab, one entry per non-empty header slot; absolute-start and
 * shared-code (type 9) entries are allocated immediately via
 * malloc_entry (ch_alloc), with the ldtab entry itself as the MPB.
 * load_nldtabents counts the entries in SYSDAT as they're added.
 *	output: returns the error code (0 = ok)
 */
static uint16_t get_ch_info(uint8_t *phys, uint16_t sysdat, uint16_t *bx_fail, uint16_t *cx_fail) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t si = (uint16_t)(SYSDAT_LDTAB + seg_r16(sd, SYSDAT_LOAD_NLDTABENTS) * ldtablen);
    uint16_t load_pd = seg_r16(sd, SYSDAT_LOAD_PD);
    uint16_t filepos = 8;

    for (uint16_t i = 0; i < ch_entmax; i++) {
        uint16_t hdr = (uint16_t)(SYSDAT_LOAD_DMA + i * chlen);
        uint8_t form = seg_r8(sd, (uint16_t)(hdr + ch_form));
        if (form == 0) continue;

        seg_w8(sd, (uint16_t)(si + ldtab_type), form);
        uint16_t length = seg_r16(sd, (uint16_t)(hdr + ch_length));
        seg_w16(sd, (uint16_t)(si + ldtab_flen), length);
        seg_w16(sd, (uint16_t)(si + ldtab_fstrt), filepos);
        filepos = (uint16_t)(filepos + length);
        uint16_t base = seg_r16(sd, (uint16_t)(hdr + ch_base));
        seg_w16(sd, (uint16_t)(si + ldtab_start), base);
        uint16_t minv = seg_r16(sd, (uint16_t)(hdr + ch_min));
        seg_w16(sd, (uint16_t)(si + ldtab_min), minv);
        uint16_t maxv = seg_r16(sd, (uint16_t)(hdr + ch_max));
        if (maxv == 0) maxv = minv;
        seg_w16(sd, (uint16_t)(si + ldtab_max), maxv);
        seg_w16(sd, (uint16_t)(si + ldtab_pd), load_pd);

        uint16_t attrib = (load_pd != 0) ? mf_load : 0;
        if (form == 1) {
            attrib = (uint16_t)(attrib + mf_code);
        } else if (form == 9) {
            attrib = (uint16_t)(attrib + mf_code + mf_share);
        }
        seg_w16(sd, (uint16_t)(si + ldtab_attrib), attrib);

        if (base != 0 || form == 9) {
            /* ch_alloc */
            load_bxcx alloc = mem_call(phys, sysdat, sysdat, malloc_entry, si);
            seg_w16(sd, (uint16_t)(si + ldtab_id), seg_r16(sd, (uint16_t)(si + ldtab_start)));
            if (alloc.cx != 0) {
                *bx_fail = 0xFFFF;
                *cx_fail = e_no_memory;
                return alloc.cx;
            }
        }
        si = (uint16_t)(si + ldtablen);
        seg_w16(sd, SYSDAT_LOAD_NLDTABENTS, (uint16_t)(seg_r16(sd, SYSDAT_LOAD_NLDTABENTS) + 1));
    }
    return 0;
}

/* load's body, between taking and releasing the MXLoad queue */
static load_bxcx load_body(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t load_pd = seg_r16(sd, SYSDAT_LOAD_PD);
    uint16_t load_fcb = seg_r16(sd, SYSDAT_LOAD_FCB);

    /* ld_read -- read the header into load_dma */
    uint8_t *w = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg), 0);
    seg_w8(w, (uint16_t)(load_fcb + fcb_nxtrec), 0);
    load_bxcx hdr_r = diskread(phys, sysdat, SYSDAT_LOAD_DMA, sysdat);
    if (hdr_r.cx != 0) {
        return hdr_r;
    }
    seg_w16(sd, SYSDAT_LOAD_INDMA, 0);

    /* zero ldtab */
    for (uint16_t i = 0; i < ldtabsiz; i++) {
        seg_w8(sd, (uint16_t)(SYSDAT_LDTAB + i), 0);
    }

    seg_w16(sd, SYSDAT_LOAD_NLDTABENTS, 0);
    if (load_pd != 0) {
        /* form_uda -- ldtab[0] is the UDA+LDSTK group */
        uint16_t si0 = SYSDAT_LDTAB;
        seg_w16(sd, (uint16_t)(si0 + ldtab_min), (lstklen + ulen) / 16);
        seg_w16(sd, (uint16_t)(si0 + ldtab_max), (lstklen + ulen) / 16);
        seg_w16(sd, (uint16_t)(si0 + ldtab_pd), load_pd);
        seg_w16(sd, (uint16_t)(si0 + ldtab_attrib), mf_load);
        seg_w16(sd, SYSDAT_LOAD_NLDTABENTS, 1);

        if (seg_r8(sd, SYSDAT_LOAD_CHAIN) == 0xFF) {
            /* ld_fnext -- un-Load-flag every segment the calling
             * process owns except the new PD's own UDA (p_uda) */
            uint16_t uda_seg = seg_r16(sd, (uint16_t)(load_pd + p_uda));
            uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
            uint16_t walk = seg_r16(sd, (uint16_t)(rlr + p_mem));
            while (walk != 0) {
                if (seg_r16(sd, (uint16_t)(walk + ms_start)) != uda_seg) {
                    uint16_t flags = seg_r16(sd, (uint16_t)(walk + ms_flags));
                    seg_w16(sd, (uint16_t)(walk + ms_flags), (uint16_t)(flags & (uint16_t)~mf_load));
                }
                walk = seg_r16(sd, (uint16_t)(walk + ms_link));
            }

            /* ld_freeit -- free all but Load memory (cpmfree free-all
             * mode), MCB on the stack; its result is discarded */
            uint16_t my_uda = rlr_uda(sd);
            uint8_t *uda = far_ptr(phys, my_uda, 0);
            seg_w8(uda, (uint16_t)(UDA_STK_LOAD_MCB + mcb_ext), 0xFF);
            mem_call(phys, sysdat, my_uda, cpmfree_entry, UDA_STK_LOAD_MCB);

            /* the kept UDA+LDSTK group is already in place */
            seg_w16(sd, (uint16_t)(si0 + ldtab_start), uda_seg);

            /* free load memory except uda,ldstack -- MFPB on the stack */
            seg_w16(uda, (uint16_t)(UDA_STK_LOAD_MFPB + mfpb_start), (uint16_t)(uda_seg + (lstklen + ulen) / 16));
            seg_w16(uda, (uint16_t)(UDA_STK_LOAD_MFPB + mfpb_pd), 0);
            mem_call(phys, sysdat, my_uda, mfree_entry, UDA_STK_LOAD_MFPB);

            /* transfer memory to new pd */
            seg_w16(sd, (uint16_t)(load_pd + p_mem), seg_r16(sd, (uint16_t)(rlr + p_mem)));
            seg_w16(sd, (uint16_t)(rlr + p_mem), 0);
        }
    }

    uint16_t bx_fail, cx_fail;
    if (get_ch_info(phys, sysdat, &bx_fail, &cx_fail) != 0) {
        return load_giveup(phys, sysdat, seg_r16(sd, SYSDAT_LOAD_NLDTABENTS), bx_fail, cx_fail);
    }
    uint16_t n = seg_r16(sd, SYSDAT_LOAD_NLDTABENTS);

    /* alloc all other (relative) memory -- si is the scratch slot
     * right after the n real entries, reused as a combined MPB */
    uint16_t si = (uint16_t)(SYSDAT_LDTAB + n * ldtablen);
    seg_w8(sd, SYSDAT_LOAD_NRELSEGS, 0);
    for (uint16_t i = 0, b = SYSDAT_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (seg_r16(sd, (uint16_t)(b + ldtab_min)) == 0) continue;
        if (seg_r16(sd, (uint16_t)(b + ldtab_start)) != 0) continue;
        seg_w16(sd, (uint16_t)(si + ldtab_min),
                (uint16_t)(seg_r16(sd, (uint16_t)(si + ldtab_min)) + seg_r16(sd, (uint16_t)(b + ldtab_min))));
        seg_w16(sd, (uint16_t)(si + ldtab_max),
                (uint16_t)(seg_r16(sd, (uint16_t)(si + ldtab_max)) + seg_r16(sd, (uint16_t)(b + ldtab_max))));
        seg_w8(sd, SYSDAT_LOAD_NRELSEGS, (uint8_t)(seg_r8(sd, SYSDAT_LOAD_NRELSEGS) + 1));
    }

    seg_w16(sd, SYSDAT_LOAD_MAXWANTED, seg_r16(sd, (uint16_t)(si + ldtab_max)));
    seg_w16(sd, SYSDAT_LOAD_MINWANTED, seg_r16(sd, (uint16_t)(si + ldtab_min)));
    if (load_pd != 0) {
        seg_w16(sd, (uint16_t)(si + ldtab_attrib), mf_load);
    }
    seg_w16(sd, (uint16_t)(si + ldtab_pd), load_pd);
    load_bxcx alloc = mem_call(phys, sysdat, sysdat, malloc_entry, si);
    seg_w16(sd, (uint16_t)(si + ldtab_id), seg_r16(sd, (uint16_t)(si + ldtab_start)));
    if (alloc.bx == 0xFFFF) {
        return load_giveup(phys, sysdat, n, 0xFFFF, alloc.cx);
    }

    /* lt_spread -- 1st give everyone the minimum */
    for (uint16_t i = 0, b = SYSDAT_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (seg_r16(sd, (uint16_t)(b + ldtab_start)) != 0) continue;
        uint16_t minv = seg_r16(sd, (uint16_t)(b + ldtab_min));
        seg_w16(sd, (uint16_t)(si + ldtab_min), (uint16_t)(seg_r16(sd, (uint16_t)(si + ldtab_min)) - minv));
        if (minv == seg_r16(sd, (uint16_t)(b + ldtab_max))) {
            uint16_t start = seg_r16(sd, (uint16_t)(si + ldtab_start));
            seg_w16(sd, (uint16_t)(b + ldtab_start), start);
            seg_w16(sd, (uint16_t)(si + ldtab_start), (uint16_t)(start + minv));
            seg_w8(sd, SYSDAT_LOAD_NRELSEGS, (uint8_t)(seg_r8(sd, SYSDAT_LOAD_NRELSEGS) - 1));
        }
    }
    /* spread what's left amongst those that need more */
    for (uint16_t i = 0, b = SYSDAT_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (seg_r16(sd, (uint16_t)(b + ldtab_start)) != 0) continue;
        uint16_t start = seg_r16(sd, (uint16_t)(si + ldtab_start));
        seg_w16(sd, (uint16_t)(b + ldtab_start), start);
        uint16_t extra = seg_r16(sd, (uint16_t)(si + ldtab_min));
        if (extra != 0) {
            uint8_t nrelsegs = seg_r8(sd, SYSDAT_LOAD_NRELSEGS);
            uint16_t q = (uint16_t)(extra / nrelsegs);
            uint16_t rem = (uint16_t)(extra % nrelsegs);
            if (rem != 0) q = (uint16_t)(q + 1);
            uint16_t room = (uint16_t)(seg_r16(sd, (uint16_t)(b + ldtab_max)) - seg_r16(sd, (uint16_t)(b + ldtab_min)));
            if (q > room) q = room;
            seg_w16(sd, (uint16_t)(b + ldtab_min), (uint16_t)(seg_r16(sd, (uint16_t)(b + ldtab_min)) + q));
            seg_w16(sd, (uint16_t)(si + ldtab_min), (uint16_t)(seg_r16(sd, (uint16_t)(si + ldtab_min)) - q));
        }
        seg_w16(sd, (uint16_t)(si + ldtab_start),
                (uint16_t)(start + seg_r16(sd, (uint16_t)(b + ldtab_min))));
        seg_w8(sd, SYSDAT_LOAD_NRELSEGS, (uint8_t)(seg_r8(sd, SYSDAT_LOAD_NRELSEGS) - 1));
    }

    /* fill memory from file -- load_group advances ldtab_start as it
     * goes; the asm's "push ldtab_start[si] ! call load_group ! pop
     * ldtab_start[si]" puts it back */
    for (uint16_t i = 0, b = SYSDAT_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (seg_r16(sd, (uint16_t)(b + ldtab_flen)) == 0) continue;
        uint16_t saved_start = seg_r16(sd, (uint16_t)(b + ldtab_start));
        uint16_t lg_cx = load_group(phys, sysdat, b);
        seg_w16(sd, (uint16_t)(b + ldtab_start), saved_start);
        if (lg_cx != 0) {
            return load_giveup(phys, sysdat, n, 0xFFFF, lg_cx);
        }
    }

    load_bxcx r = load_finish(phys, sysdat, user_retf_offset, cs);
    if (r.bx == 0xFFFF) {
        /* both of load_finish's e_no_cseg exits are `jmp load_giveup` */
        return load_giveup(phys, sysdat, n, 0xFFFF, r.cx);
    }
    return r;
}

/*
 * load -- intermodule entry point to load a .CMD file
 *	input:	dx = addr of open FCB in u_wrkseg; bx = addr of an unused PD
 *		to initialize (0 - do not init PD); load_chain = chain load
 *		(cload_entry -- the asm smuggles this through bx == 1 with
 *		the PD on the stack)
 *	output:	bx = seg addr of Base Page, 0xFFFF if error; cx = error code
 */
load_bxcx load(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx, uint16_t user_retf_offset,
               uint16_t cs, int load_chain) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    /* get the MXLoad queue: mpmif(f_qread) with DS = SYSDAT */
    (void)mem_call(phys, sysdat, sysdat, readq_entry, SYSDAT_MXLOADQPB);
    seg_w8(sd, SYSDAT_LOAD_CHAIN, load_chain ? 0xFF : 0);
    seg_w16(sd, SYSDAT_LOAD_PD, bx);
    seg_w16(sd, SYSDAT_LOAD_FCB, dx);
    load_bxcx r = load_body(phys, sysdat, user_retf_offset, cs);
    /* load_exit */
    (void)mem_call(phys, sysdat, sysdat, writeq_entry, SYSDAT_MXLOADQPB);
    return r;
}

/* cload_entry -- entry point to load for a chain command (bx = the PD,
 * already carrying a real p_uda) */
load_bxcx cload_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx,
                       uint16_t user_retf_offset, uint16_t cs) {
    return load(phys, sysdat, dx, bx, user_retf_offset, cs, 1);
}

load_bxcx load_finish(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t load_8080 = 0;
    seg_w8(sd, SYSDAT_LOAD_8080, 0);
    uint16_t n = seg_r16(sd, SYSDAT_LOAD_NLDTABENTS);

    /* lb_more -- find the first Data group (type 2) */
    uint16_t si = SYSDAT_LDTAB;
    int found = 0;
    for (uint16_t i = 0; i < n; i++, si = (uint16_t)(si + ldtablen)) {
        if (sd[si + ldtab_type] == 2) {
            found = 1;
            break;
        }
    }
    if (!found) {
        /* lbc_mre -- no Data group; fall back to the first Code group
         * (type 1), an 8080-model program */
        si = SYSDAT_LDTAB;
        for (uint16_t i = 0; i < n; i++, si = (uint16_t)(si + ldtablen)) {
            if (sd[si + ldtab_type] == 1) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return (load_bxcx){0xFFFF, e_no_cseg}; /* jmp load_giveup --
                load() runs it on this tail's errors */
        }
        load_8080 = 1;
        seg_w8(sd, SYSDAT_LOAD_8080, 1);
    }

    /* lb_found */
    uint16_t load_basep = seg_r16(sd, (uint16_t)(si + ldtab_start));
    seg_w16(sd, SYSDAT_LOAD_BASEP, load_basep);
    for (uint16_t i = 0; i < 0x5B / 2; i++) {
        seg_w16(far_ptr(phys, load_basep, 0), (uint16_t)(2 * i), 0);
    }
    seg_w8(far_ptr(phys, load_basep, 0), bpg_8080, load_8080);

    /* lbb_mre -- fill in each group's own clen/cseg-shaped slot */
    si = SYSDAT_LDTAB;
    for (uint16_t i = 0; i < n; i++, si = (uint16_t)(si + ldtablen)) {
        uint8_t type = sd[si + ldtab_type];
        if (type == 0) {
            continue;
        }
        uint16_t slot = (uint16_t)(6 * (type - 1));
        uint16_t minv = seg_r16(sd, (uint16_t)(si + ldtab_min));
        seg_w8(far_ptr(phys, load_basep, 0), (uint16_t)(slot + 2), (uint8_t)(minv >> 12));
        seg_w16(far_ptr(phys, load_basep, 0), slot, (uint16_t)((uint16_t)(minv << 4) - 1));
        seg_w16(far_ptr(phys, load_basep, 0), (uint16_t)(slot + 3),
                    seg_r16(sd, (uint16_t)(si + ldtab_start)));
    }

    /* if 8080 model, copy CS info into DS info */
    if (load_8080) {
        for (uint16_t i = 0; i < 6; i++) {
            seg_w8(far_ptr(phys, load_basep, 0), (uint16_t)(6 + i), seg_r8(far_ptr(phys, load_basep, 0), i));
        }
    }

    /* init PD, UDA and LDSTK */
    uint16_t load_pd = seg_r16(sd, SYSDAT_LOAD_PD);
    if (load_pd == 0) {
        return (load_bxcx){load_basep, 0};
    }

    si = SYSDAT_LDTAB; /* lip_1 -- the UDA+LSTK entry is always ldtab[0] */
    uint16_t bx = load_pd;
    uint16_t uda_abs = seg_r16(sd, (uint16_t)(si + ldtab_start));
    seg_w16(sd, (uint16_t)(bx + p_uda), uda_abs);
    uint16_t load_uda = uda_abs;
    seg_w16(sd, SYSDAT_LOAD_UDA, load_uda);
    uint16_t load_lstk = (uint16_t)(uda_abs + (ulen / 16));
    seg_w16(sd, SYSDAT_LOAD_LSTK, load_lstk);

    for (uint16_t i = 0; i < (ulen + lstklen) / 2; i++) {
        seg_w16(far_ptr(phys, load_uda, 0), (uint16_t)(2 * i), 0);
    }

    seg_w16(sd, (uint16_t)(bx + p_uda), (uint16_t)(uda_abs - sysdat));
    sd[bx + p_stat] = ps_run;
    sd[bx + p_prior] = 200;
    sd[bx + p_luser] = sd[bx + p_user];
    sd[bx + p_ldsk] = sd[bx + p_dsk];

    uint16_t wrkseg = seg_r16(far_ptr(phys, rlr_uda(sd), 0), u_wrkseg);
    uint16_t load_fcb = seg_r16(sd, SYSDAT_LOAD_FCB);
    uint8_t load_fcb_dr = seg_r8(far_ptr(phys, wrkseg, 0), load_fcb);
    if (load_fcb_dr != 0) {
        sd[bx + p_ldsk] = load_fcb_dr;
    }

    /* init UDA */
    seg_w16(far_ptr(phys, load_uda, 0), u_dma_ofst, bpg_dma);
    seg_w16(far_ptr(phys, load_uda, 0), u_dma_seg, load_basep);
    seg_w16(far_ptr(phys, load_uda, 0), u_initds, load_basep);
    seg_w16(far_ptr(phys, load_uda, 0), u_inites, load_basep);
    seg_w16(far_ptr(phys, load_uda, 0), u_initss, load_lstk);

    uint16_t cseg = seg_r16(far_ptr(phys, load_basep, 0), bpg_cseg);
    if (cseg == 0) {
        return (load_bxcx){0xFFFF, e_no_cseg};
    }
    seg_w16(far_ptr(phys, load_uda, 0), u_initcs, cseg);

    uint16_t eseg = seg_r16(far_ptr(phys, load_basep, 0), bpg_eseg);
    if (eseg != 0) {
        seg_w16(far_ptr(phys, load_uda, 0), u_inites, eseg);
    }

    seg_w16(far_ptr(phys, load_uda, 0), u_stack_sp, ls_sp);

    uint16_t ip = 0;
    if (seg_r8(far_ptr(phys, load_basep, 0), bpg_8080) != 0) {
        ip = 0x0100;
    }

    seg_w16(far_ptr(phys, load_lstk, 0), ls_offset, ip);
    seg_w16(far_ptr(phys, load_lstk, 0), ls_flags, 0x0200);
    seg_w16(far_ptr(phys, load_lstk, 0), ls_roffset, user_retf_offset);
    seg_w16(far_ptr(phys, load_lstk, 0), ls_rcseg, cs);

    return (load_bxcx){load_basep, 0};
}
