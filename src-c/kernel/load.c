/*
 * load.c -- C port of src/kernel/LOAD.SUP's `load:`/`load_entry`
 * (the whole intermodule load path except `cload_entry`'s own
 * chain-load mode, see below), plus the real disk I/O and memory
 * allocation it depends on: `diskread`, `load_group`, `get_ch_info`,
 * and the tail portion (`lb_more` through `load_exit`) ported as
 * `load_finish`.
 *
 * `diskread`'s real `mpmif(f_freadseq)` call (a sequential BDOS file
 * read) is modeled by `freadseq` below against a deliberately
 * simplified "virtual disk" (`SD_LOAD_DISK`/`SD_LOAD_DISK_RECS`, see
 * defs.h's own comment there): a single extent's worth of 128-byte
 * records, indexed directly by the FCB's own `fcb_nxtrec` field, with
 * no disk-map/extent-switch logic at all. Every file this project's
 * own tests load is small enough to stay within one extent, so this
 * is a deliberate, documented simplification (same spirit as
 * `dsptch.c`'s own `term_act` divergence) rather than a real BDOS
 * file system -- `tools/i86sim.py`'s own differential test needs the
 * *identical* simplified model on the reference-assembly side too
 * (a `call_hooks` entry at `mpmif(f_freadseq)`'s own call site,
 * implementing the same `freadseq` semantics directly against
 * `sim.mem`), so what's actually being compared is everything
 * *around* that shared primitive -- the real, interesting logic in
 * `diskread`/`load_group` themselves. `f_setdmab`/`f_setdma` (which
 * `diskread` also calls, to set up the real DMA target before
 * `f_freadseq`) need no modeling of their own: `diskread`'s own input
 * registers already fully determine where the read goes, so this
 * port passes them straight into `freadseq` instead of maintaining
 * separate "current DMA" state.
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
 * `cload_entry`'s own chain-load mode (`bx==1` into `load:`) is NOT
 * ported: its own real memory-transfer cleanup (`ld_fnext`/
 * `ld_freeit`) depends on `CPMMEM.MEM`'s own `cpmfree_entry`
 * succeeding for a real MAU release, which that file's own port
 * still can't do faithfully (see `cpmmem.c`'s own header comment on
 * why its own `free_seg` retry is real-but-unexercisable) -- a
 * separate, deeper blocker than either disk I/O or allocation. `load`
 * below is `load_entry`'s own path (`bx=0`) plus an intermodule
 * caller supplying a real, uninitialized PD to init (`bx`=that PD's
 * own address) -- never `bx=1`.
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

/*
 * freadseq -- stand-in for the real mpmif(f_freadseq): a simplified,
 * single-extent sequential read off SD_LOAD_DISK. See this file's
 * own header comment.
 *	input:	wrkseg = u_wrkseg; fcb = address of the open FCB in it;
 *		dma_seg/dma_ofst = where to copy the record
 *	output:	returns bl (0 = ok, 1 = eof -- diskread treats both the
 *		same way; 0xFF, a hard read error, is never produced by
 *		this simplified model)
 */
static uint8_t freadseq(uint8_t *mem, uint16_t wrkseg, uint16_t fcb, uint16_t dma_seg, uint16_t dma_ofst) {
    uint8_t nxtrec = mem_r8_seg(mem, wrkseg, (uint16_t)(fcb + fcb_nxtrec));
    uint16_t recs = mem_r16(mem, SD_LOAD_DISK_RECS);
    if (nxtrec >= recs) {
        return 1; /* eof */
    }
    uint16_t src = (uint16_t)(SD_LOAD_DISK + nxtrec * 128);
    for (unsigned i = 0; i < 128; i++) {
        mem_w8_seg(mem, dma_seg, (uint16_t)(dma_ofst + i), mem[(uint16_t)(src + i)]);
    }
    mem_w8_seg(mem, wrkseg, (uint16_t)(fcb + fcb_nxtrec), (uint8_t)(nxtrec + 1));
    return 0;
}

/*
 * diskread -- read the next sequential record of load_fcb into a
 * given DMA buffer.
 *	input:	dx = dma offset, cx = dma segment
 *	output:	bx=rlr, cx=0 on success/eof; bx=0xFFFF, cx=e_bad_read on
 *		a hard read error (translated faithfully, never
 *		exercised by this simplified model); bx=0xFFFF,
 *		cx=e_abort if the calling process's own pf_ctlc is set
 */
load_bxcx diskread(uint8_t *mem, uint16_t dx, uint16_t cx) {
    uint16_t dma_seg = cx, dma_ofst = dx;
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    uint16_t fcb = mem_r16(mem, SD_LOAD_FCB);
    uint8_t bl = freadseq(mem, wrkseg, fcb, dma_seg, dma_ofst);
    if (bl == 0xFF) {
        return (load_bxcx){0xFFFF, e_bad_read};
    }
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (mem_r16(mem, (uint16_t)(rlr + p_flag)) & pf_ctlc) {
        return (load_bxcx){0xFFFF, e_abort};
    }
    return (load_bxcx){rlr, 0};
}

/*
 * load_group -- load a group described in ldtab, filling it from the
 * virtual disk (via diskread), reusing whatever's already sitting in
 * the shared load_dma buffer (SD_LOAD_DMA/SD_LOAD_INDMA) when
 * possible.
 *	input:	si = ldtab entry address; sysdat = this module's own
 *		copy of the SYSDAT segment (the partial-record read's
 *		own DMA segment, same as `cs:sysdat` in the real asm)
 *	output:	returns the error code (0 = ok)
 */
uint16_t load_group(uint8_t *mem, uint16_t si, uint16_t sysdat) {
    uint16_t bx = si;
    for (;;) {
        uint16_t indma = mem_r16(mem, SD_LOAD_INDMA);
        uint16_t fstrt = mem_r16(mem, (uint16_t)(bx + ldtab_fstrt));
        if (fstrt >= indma) {
            uint16_t off = (uint16_t)(fstrt - indma);
            if (off < 8) {
                uint16_t avail = (uint16_t)(8 - off);
                uint16_t flen = mem_r16(mem, (uint16_t)(bx + ldtab_flen));
                uint16_t take = (avail <= flen) ? avail : flen;
                uint16_t dest_seg = mem_r16(mem, (uint16_t)(bx + ldtab_start));
                for (uint16_t i = 0; i < (uint16_t)(take * 16); i++) {
                    mem_w8_seg(mem, dest_seg, i, mem[(uint16_t)(SD_LOAD_DMA + off * 16 + i)]);
                }
                mem_w16(mem, (uint16_t)(bx + ldtab_start), (uint16_t)(dest_seg + take));
                mem_w16(mem, (uint16_t)(bx + ldtab_flen), (uint16_t)(flen - take));
                mem_w16(mem, (uint16_t)(bx + ldtab_fstrt), (uint16_t)(fstrt + take));
            }
        }

        uint16_t flen = mem_r16(mem, (uint16_t)(bx + ldtab_flen));
        if (flen == 0) {
            return 0;
        }

        if (flen >= 8) {
            /* xfer_direct -- a full record goes straight to the
             * destination segment, bypassing load_dma entirely */
            uint16_t dest_seg = mem_r16(mem, (uint16_t)(bx + ldtab_start));
            load_bxcx r = diskread(mem, 0, dest_seg);
            if (r.cx != 0) return r.cx;
            mem_w16(mem, (uint16_t)(bx + ldtab_start), (uint16_t)(dest_seg + 8));
            mem_w16(mem, (uint16_t)(bx + ldtab_fstrt), (uint16_t)(mem_r16(mem, (uint16_t)(bx + ldtab_fstrt)) + 8));
            mem_w16(mem, (uint16_t)(bx + ldtab_flen), (uint16_t)(flen - 8));
            continue; /* real asm: jmp read_first -- a no-op re-run of
                the "already in dma" check above, since fstrt is now
                >= indma+8 */
        }

        /* read_1st -- less than a full record remains: read one into
         * the shared buffer, then restart from the top (jmp load_group) */
        load_bxcx r = diskread(mem, SD_LOAD_DMA, sysdat);
        if (r.cx != 0) return r.cx;
        mem_w16(mem, SD_LOAD_INDMA, mem_r16(mem, (uint16_t)(bx + ldtab_fstrt)));
    }
}

extern load_bxcx malloc_entry(uint8_t *mem, uint16_t dx);
extern load_bxcx mfree_entry(uint8_t *mem, uint16_t dx);
load_bxcx load_finish(uint8_t *mem, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs);

/*
 * load_giveup -- release every ldtab entry (0..n inclusive -- n+1 of
 * them, since the caller's own combined-relative-request scratch
 * entry at ldtab[n] counts too) that was actually allocated
 * (`ldtab_id != 0`), via mfree_entry, then report the given failure.
 *	input:	n = load_nldtabents; bx_fail/cx_fail = what to return
 */
static load_bxcx load_giveup(uint8_t *mem, uint16_t n, uint16_t bx_fail, uint16_t cx_fail) {
    for (uint16_t i = 0, bx = SD_LOAD_LDTAB; i <= n; i++, bx = (uint16_t)(bx + ldtablen)) {
        uint16_t id = mem_r16(mem, (uint16_t)(bx + ldtab_id));
        if (id == 0) continue;
        mem_w16(mem, SD_LOAD_MFPB + mfpb_start, id);
        mem_w16(mem, SD_LOAD_MFPB + mfpb_pd, mem_r16(mem, (uint16_t)(bx + ldtab_pd)));
        mfree_entry(mem, SD_LOAD_MFPB);
    }
    return (load_bxcx){bx_fail, cx_fail};
}

/*
 * get_ch_info -- parse the .CMD header (already sitting in
 * SD_LOAD_DMA from load's own earlier diskread) into ldtab, one
 * entry per non-empty header slot; absolute-start and shared-code
 * (type 9) entries are allocated immediately via malloc_entry
 * (ch_alloc).
 *	input/output: *n (load_nldtabents) is read for the starting
 *		ldtab slot (si = SD_LOAD_LDTAB + *n*ldtablen, matching
 *		form_uda's own possible ldtab[0]) and updated as entries
 *		are added
 *	output: returns the error code (0 = ok); on failure, *n is left
 *		as load_giveup's own already-released state (0xFFFF/cx)
 */
static uint16_t get_ch_info(uint8_t *mem, uint16_t *n, uint16_t *bx_fail, uint16_t *cx_fail) {
    uint16_t si = (uint16_t)(SD_LOAD_LDTAB + *n * ldtablen);
    uint16_t load_pd = mem_r16(mem, SD_LOAD_PD);
    uint16_t filepos = 8;

    for (uint16_t i = 0; i < ch_entmax; i++) {
        uint16_t hdr = (uint16_t)(SD_LOAD_DMA + i * chlen);
        uint8_t form = mem[hdr + ch_form];
        if (form == 0) continue;

        mem[si + ldtab_type] = form;
        uint16_t length = mem_r16(mem, (uint16_t)(hdr + ch_length));
        mem_w16(mem, (uint16_t)(si + ldtab_flen), length);
        mem_w16(mem, (uint16_t)(si + ldtab_fstrt), filepos);
        filepos = (uint16_t)(filepos + length);
        uint16_t base = mem_r16(mem, (uint16_t)(hdr + ch_base));
        mem_w16(mem, (uint16_t)(si + ldtab_start), base);
        uint16_t minv = mem_r16(mem, (uint16_t)(hdr + ch_min));
        mem_w16(mem, (uint16_t)(si + ldtab_min), minv);
        uint16_t maxv = mem_r16(mem, (uint16_t)(hdr + ch_max));
        if (maxv == 0) maxv = minv;
        mem_w16(mem, (uint16_t)(si + ldtab_max), maxv);
        mem_w16(mem, (uint16_t)(si + ldtab_pd), load_pd);

        uint16_t attrib = (load_pd != 0) ? mf_load : 0;
        if (form == 1) {
            attrib = (uint16_t)(attrib + mf_code);
        } else if (form == 9) {
            attrib = (uint16_t)(attrib + mf_code + mf_share);
        }
        mem_w16(mem, (uint16_t)(si + ldtab_attrib), attrib);

        if (base != 0 || form == 9) {
            /* ch_alloc -- an absolute request, or shared code: allocate
             * right now, using this ldtab entry itself as the MPB */
            load_bxcx alloc = malloc_entry(mem, si);
            mem_w16(mem, (uint16_t)(si + ldtab_id), mem_r16(mem, (uint16_t)(si + ldtab_start)));
            if (alloc.cx != 0) {
                *bx_fail = 0xFFFF;
                *cx_fail = e_no_memory;
                return alloc.cx;
            }
        }
        si = (uint16_t)(si + ldtablen);
        (*n)++;
    }
    return 0;
}

load_bxcx load(uint8_t *mem, uint16_t dx, uint16_t bx, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    uint16_t load_pd = bx;
    uint16_t load_fcb = dx;
    mem_w16(mem, SD_LOAD_PD, load_pd);
    mem_w16(mem, SD_LOAD_FCB, load_fcb);

    /* ld_read -- read the header into SD_LOAD_DMA */
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);
    mem_w8_seg(mem, wrkseg, (uint16_t)(load_fcb + fcb_nxtrec), 0);
    load_bxcx hdr_r = diskread(mem, SD_LOAD_DMA, sysdat);
    if (hdr_r.cx != 0) {
        return (load_bxcx){0xFFFF, hdr_r.cx};
    }
    mem_w16(mem, SD_LOAD_INDMA, 0);

    /* zero ldtab -- the whole reserved region, same as the real asm's
     * own "mov cx,ldtabsiz/2" word-fill */
    for (uint16_t i = 0; i < 10 * ldtablen; i++) {
        mem[(uint16_t)(SD_LOAD_LDTAB + i)] = 0;
    }

    uint16_t n = 0;
    if (load_pd != 0) {
        /* form_uda -- ldtab[0] is the UDA+LDSTK group; load_chain's
         * own cleanup (ld_fnext/ld_freeit) is NOT reached here since
         * this port never sets load_chain -- see header comment */
        uint16_t si0 = SD_LOAD_LDTAB;
        mem_w16(mem, (uint16_t)(si0 + ldtab_min), (lstklen + ulen) / 16);
        mem_w16(mem, (uint16_t)(si0 + ldtab_max), (lstklen + ulen) / 16);
        mem_w16(mem, (uint16_t)(si0 + ldtab_pd), load_pd);
        mem_w16(mem, (uint16_t)(si0 + ldtab_attrib), mf_load);
        n = 1;
    }

    uint16_t bx_fail, cx_fail;
    uint16_t gc_cx = get_ch_info(mem, &n, &bx_fail, &cx_fail);
    if (gc_cx != 0) {
        return load_giveup(mem, n, bx_fail, cx_fail);
    }
    mem_w16(mem, SD_LOAD_NLDTABENTS, n);

    /* alloc all other (relative) memory -- si is the scratch slot
     * right after the n real entries, reused as a combined MPB */
    uint16_t si = (uint16_t)(SD_LOAD_LDTAB + n * ldtablen);
    uint16_t nrelsegs = 0;
    for (uint16_t i = 0, b = SD_LOAD_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (mem_r16(mem, (uint16_t)(b + ldtab_min)) == 0) continue;
        if (mem_r16(mem, (uint16_t)(b + ldtab_start)) != 0) continue;
        mem_w16(mem, (uint16_t)(si + ldtab_min),
                (uint16_t)(mem_r16(mem, (uint16_t)(si + ldtab_min)) + mem_r16(mem, (uint16_t)(b + ldtab_min))));
        mem_w16(mem, (uint16_t)(si + ldtab_max),
                (uint16_t)(mem_r16(mem, (uint16_t)(si + ldtab_max)) + mem_r16(mem, (uint16_t)(b + ldtab_max))));
        nrelsegs++;
    }

    if (load_pd != 0) {
        mem_w16(mem, (uint16_t)(si + ldtab_attrib), mf_load);
    }
    mem_w16(mem, (uint16_t)(si + ldtab_pd), load_pd);
    load_bxcx alloc = malloc_entry(mem, si);
    mem_w16(mem, (uint16_t)(si + ldtab_id), mem_r16(mem, (uint16_t)(si + ldtab_start)));
    if (alloc.bx == 0xFFFF) {
        return load_giveup(mem, n, 0xFFFF, alloc.cx);
    }

    /* lt_spread -- give every relative entry its own share */
    for (uint16_t i = 0, b = SD_LOAD_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (mem_r16(mem, (uint16_t)(b + ldtab_start)) != 0) continue;
        uint16_t minv = mem_r16(mem, (uint16_t)(b + ldtab_min));
        mem_w16(mem, (uint16_t)(si + ldtab_min), (uint16_t)(mem_r16(mem, (uint16_t)(si + ldtab_min)) - minv));
        if (minv == mem_r16(mem, (uint16_t)(b + ldtab_max))) {
            uint16_t start = mem_r16(mem, (uint16_t)(si + ldtab_start));
            mem_w16(mem, (uint16_t)(b + ldtab_start), start);
            mem_w16(mem, (uint16_t)(si + ldtab_start), (uint16_t)(start + minv));
            nrelsegs--;
        }
    }
    for (uint16_t i = 0, b = SD_LOAD_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (mem_r16(mem, (uint16_t)(b + ldtab_start)) != 0) continue;
        uint16_t start = mem_r16(mem, (uint16_t)(si + ldtab_start));
        mem_w16(mem, (uint16_t)(b + ldtab_start), start);
        uint16_t extra = mem_r16(mem, (uint16_t)(si + ldtab_min));
        if (extra != 0) {
            uint16_t q = (uint16_t)(extra / nrelsegs);
            uint16_t rem = (uint16_t)(extra % nrelsegs);
            if (rem != 0) q = (uint16_t)(q + 1);
            uint16_t room = (uint16_t)(mem_r16(mem, (uint16_t)(b + ldtab_max)) - mem_r16(mem, (uint16_t)(b + ldtab_min)));
            if (q > room) q = room;
            mem_w16(mem, (uint16_t)(b + ldtab_min), (uint16_t)(mem_r16(mem, (uint16_t)(b + ldtab_min)) + q));
            mem_w16(mem, (uint16_t)(si + ldtab_min), (uint16_t)(mem_r16(mem, (uint16_t)(si + ldtab_min)) - q));
        }
        mem_w16(mem, (uint16_t)(si + ldtab_start),
                (uint16_t)(start + mem_r16(mem, (uint16_t)(b + ldtab_min))));
        nrelsegs--;
    }

    /* fill memory from file -- load_group's own internal bookkeeping
     * (xfer_direct's/xfer_1's own "add ldtab_start[bx],8") advances
     * ldtab_start as it consumes the group's own destination; the
     * real asm's own "push ldtab_start[si] ! call load_group ! pop
     * ldtab_start[si]" deliberately throws that drift away afterward,
     * since everything downstream (load_finish) needs ldtab_start to
     * keep meaning "this group's own fixed destination segment" */
    for (uint16_t i = 0, b = SD_LOAD_LDTAB; i < n; i++, b = (uint16_t)(b + ldtablen)) {
        if (mem_r16(mem, (uint16_t)(b + ldtab_flen)) == 0) continue;
        uint16_t saved_start = mem_r16(mem, (uint16_t)(b + ldtab_start));
        uint16_t lg_cx = load_group(mem, b, sysdat);
        mem_w16(mem, (uint16_t)(b + ldtab_start), saved_start);
        if (lg_cx != 0) {
            return load_giveup(mem, n, 0xFFFF, lg_cx);
        }
    }

    return load_finish(mem, sysdat, user_retf_offset, cs);
}

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
