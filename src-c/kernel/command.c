/*
 * command.c -- C port of src/kernel/COMMAND.SUP: `clicmd_entry`/
 * `chain_entry` (the CLI: run a typed command, or chain to one, `dx==0`)
 * and every routine they call -- `cli_checkque` (hand the command to an
 * RSP's queue), `cli_fload`/`cli_gopen` (open and load a .CMD file, set
 * up its Base Page, create its process), the `cli_exit` tail,
 * `shifttail`, `fileopen`/`fileclose`, the console-print helpers, and
 * `user_retf`.
 *
 * Runs against real memory (defs.h's far_ptr): `phys` is the base of
 * real physical memory and `sysdat` the SYSDAT segment, the SUP's DS.
 * The cli_* work area, the MXCli QPB and the strings it prints are
 * SYSDAT fields; the ES-relative u_* fields are the running process's
 * UDA (rlr's p_uda -- which a chain moves, see below); the caller's
 * command buffer is in its u_wrkseg.
 *
 * Every `mpmif` call is made as the asm makes it: u_wrkseg = the
 * caller's DS for the call (SYSDAT -- so the QPBs, the ACB, the PCB and
 * the FCB are all addressed in SYSDAT), restored afterwards. The calls
 * are direct calls to the ported routines: f_qread/f_qwrite/f_qopen/
 * f_cqwrite (que1.c), f_cconattch/f_conassign (console.c), f_conout/
 * f_conprint (chario.c), f_parsefilename (parse.c), f_load/f_cload
 * (load.c) and f_setprior (sysent.c). `prcsm` prints a string in the
 * SUP's own code segment (u_wrkseg = CS); here that string is a C
 * constant, printed through chario.c's `conprint_cs`. f_createproc and
 * f_terminate go through the `command_set_mpmif` hook (no-ops when it
 * is unset, as in the host tests); user_retf's `int mpmint` is a
 * boundary.
 *
 * f_fopen/f_fclose go through the BDOS (its getdiskmx path) via a
 * hook, `cli_set_bdos` (defs.h), like load.c's f_freadseq: the hook gets the function and
 * the FCB (u_wrkseg:cli_fcb) and returns the BDOS's BX. With no hook an
 * open fails (00FFh: not found).
 *
 * A chain (`dx==0`) differs from a typed command in three places, as in
 * the asm: the command comes from the caller's DMA buffer instead of a
 * CLICB; `cli_ld1` hands the caller's UDA to the new PD (copying it to
 * SYSDAT's inituda block and moving rlr's p_uda -- and, in the asm, SS
 * -- there first; the stack switch has no C equivalent) and loads with
 * f_cload; and when cli_term is set the exit is `jmp mpmif(f_terminate)`,
 * which never returns in the real system. That exit returns (0,0) here,
 * a fixed choice for a value no caller sees.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} cmd_bxcx;

extern cmd_bxcx openq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern cmd_bxcx readq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern cmd_bxcx writeq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern cmd_bxcx cwriteq_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern cmd_bxcx cconattch_entry(uint8_t *phys, uint16_t sysdat);
extern cmd_bxcx conassign_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern void conout_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl);
extern void conprint_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint8_t bh, uint8_t bl);
extern void conprint_cs(uint8_t *phys, uint16_t sysdat, const char *s, uint8_t bh, uint8_t bl);
extern void set_prior_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl);

extern cmd_bxcx parse_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx);
extern cmd_bxcx load(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t bx, uint16_t user_retf_offset,
                     uint16_t cs, int load_chain);

static const char questr[] = " Msg Qued";
static const char userstr[] = "(User 0)";

static cli_bdos_fn cli_bdos;

void cli_set_bdos(cli_bdos_fn fn) {
    cli_bdos = fn;
}

/* the running process's UDA */
static uint8_t *run_uda(uint8_t *phys, uint16_t sysdat) {
    return far_ptr(phys, rlr_uda(far_ptr(phys, sysdat, 0)), 0);
}

/* the dispatcher calls (f_createproc, f_terminate) go through the
 * Supervisor's mpmif hook (DS = SYSDAT); unset, they're no-ops */
MPMIF_HOOK(command)

/* mpmif with DS = SYSDAT: u_wrkseg = SYSDAT around the call */
static uint16_t mpm_enter(uint8_t *phys, uint16_t sysdat) {
    uint8_t *uda = run_uda(phys, sysdat);
    uint16_t saved = seg_r16(uda, u_wrkseg);
    seg_w16(uda, u_wrkseg, sysdat);
    return saved;
}

static void mpm_leave(uint8_t *phys, uint16_t sysdat, uint16_t saved) {
    seg_w16(run_uda(phys, sysdat), u_wrkseg, saved);
}

static cmd_bxcx mpm_call(uint8_t *phys, uint16_t sysdat, cmd_bxcx (*fn)(uint8_t *, uint16_t, uint16_t),
                         uint16_t dx) {
    uint16_t w = mpm_enter(phys, sysdat);
    cmd_bxcx r = fn(phys, sysdat, dx);
    mpm_leave(phys, sysdat, w);
    return r;
}

/*
 * shifttail -- set up the command tail to be parsed: shift what follows
 * the parsed name down to the start of cli_cmdtail (or clear it)
 *	input:	ax = the previous parsefilename's result (0 = no tail)
 */
void shifttail(uint8_t *phys, uint16_t sysdat, uint16_t ax) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    if (ax == 0) {
        seg_w8(sd, SYSDAT_CLI_CMDTAIL, 0);
        return;
    }
    uint16_t cx = (uint16_t)((uint16_t)(128 - (uint16_t)(ax - SYSDAT_CLI_CMDTAIL)) >> 1); /* words */
    for (uint16_t i = 0; i < cx; i++) {
        seg_w16(sd, (uint16_t)(SYSDAT_CLI_CMDTAIL + i * 2), seg_r16(sd, (uint16_t)(ax + i * 2)));
    }
}

/* fileopen -- open cli_fcb read-only (f6'); returns the BDOS's BX */
uint16_t fileopen(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    seg_w8(sd, (uint16_t)(SYSDAT_CLI_FCB + fcb_name + 5),
           (uint8_t)(seg_r8(sd, (uint16_t)(SYSDAT_CLI_FCB + fcb_name + 5)) | 0x80));
    return cli_bdos ? cli_bdos(phys, 15, sysdat, SYSDAT_CLI_FCB) : 0x00FF;
}

/* fileclose -- close cli_fcb (no caller reads the result) */
void fileclose(uint8_t *phys, uint16_t sysdat) {
    if (cli_bdos) {
        (void)cli_bdos(phys, 16, sysdat, SYSDAT_CLI_FCB);
    }
}

/*
 * user_retf -- where a program that does a RETF expecting the CCP ends
 * up: clear the keep/tempkeep/ctlc/sys flags and terminate (the
 * `int mpmint` is a boundary)
 */
void user_retf(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    seg_w16(sd, (uint16_t)(rlr + p_flag),
            (uint16_t)(seg_r16(sd, (uint16_t)(rlr + p_flag)) & (uint16_t)~(pf_keep | pf_tempkeep | pf_ctlc | pf_sys)));
}

/* -- console-print helpers -- */

static void prchar(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint16_t w = mpm_enter(phys, sysdat);
    conout_entry(phys, sysdat, dl);
    mpm_leave(phys, sysdat, w);
}

static void crlf(uint8_t *phys, uint16_t sysdat) {
    prchar(phys, sysdat, 13);
    prchar(phys, sysdat, 10);
}

static void prnum(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    prchar(phys, sysdat, (uint8_t)((dl >> 4) + '0'));
    prchar(phys, sysdat, (uint8_t)((dl & 0x0F) + '0'));
}

static void prtime(uint8_t *phys, uint16_t sysdat) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    prnum(phys, sysdat, seg_r8(sd, SYSDAT_TOD + 2)); /* tod_hr */
    prchar(phys, sysdat, ':');
    prnum(phys, sysdat, seg_r8(sd, SYSDAT_TOD + 3)); /* tod_min */
    prchar(phys, sysdat, ':');
    prnum(phys, sysdat, seg_r8(sd, SYSDAT_TOD + 4)); /* tod_sec */
    prchar(phys, sysdat, ' ');
}

/* cprnt -- f_conprint of dx (in SYSDAT), at most bh chars, up to bl */
static void cprnt(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint8_t bh, uint8_t bl) {
    uint16_t w = mpm_enter(phys, sysdat);
    conprint_entry(phys, sysdat, dx, bh, bl);
    mpm_leave(phys, sysdat, w);
}

/* prcsm -- print a NUL-terminated string in the SUP's code segment */
static void prcsm(uint8_t *phys, uint16_t sysdat, const char *s) {
    uint8_t *uda = run_uda(phys, sysdat);
    uint16_t saved = seg_r16(uda, u_wrkseg);
    conprint_cs(phys, sysdat, s, 0, 0);
    seg_w16(uda, u_wrkseg, saved);
}

static void prdisk(uint8_t *phys, uint16_t sysdat) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t dl = seg_r8(sd, SYSDAT_CLI_FCB + fcb_dr);
    if (dl != 0) {
        dl = (uint8_t)(dl - 1);
    } else {
        dl = seg_r8(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_dsk));
    }
    prchar(phys, sysdat, (uint8_t)(dl + 'A'));
    prchar(phys, sysdat, ':');
}

static void prfilnam(uint8_t *phys, uint16_t sysdat) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    prdisk(phys, sysdat);
    cprnt(phys, sysdat, SYSDAT_CLI_FCB + fcb_name, 8, ' ');
    prchar(phys, sysdat, '.');
    cprnt(phys, sysdat, SYSDAT_CLI_FCB + fcb_type, 3, ' ');
    prchar(phys, sysdat, ' ');
    if (!(seg_r8(sd, SYSDAT_CLI_FCB + fcb_name + 7) & 0x80)) {
        if (seg_r8(sd, SYSDAT_CLI_USER) == 0) {
            return;
        }
        if (seg_r8(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_user)) != 0) {
            return;
        }
    }
    prcsm(phys, sysdat, userstr); /* pruser */
}

static void prcusp(uint8_t *phys, uint16_t sysdat) {
    cprnt(phys, sysdat, SYSDAT_CLI_CUSPQPB + qpb_name, 8, ' ');
    prcsm(phys, sysdat, questr);
    crlf(phys, sysdat);
}

/* pfn -- parse cli_pcb (its flnmptr into cli_fcb) */
static cmd_bxcx pfn(uint8_t *phys, uint16_t sysdat) {
    return mpm_call(phys, sysdat, parse_entry, SYSDAT_CLI_PCB);
}

static cmd_bxcx conasn(uint8_t *phys, uint16_t sysdat) {
    return mpm_call(phys, sysdat, conassign_entry, SYSDAT_CLI_ACB);
}

/*
 * cli_checkque -- try an RSP: open the queue named after cli_fcb, check
 * it's an RSP queue, write the command to it and give the console to
 * the process of that name.
 *	output: returns the error code (0 = successful RSP access)
 */
static uint16_t cli_checkque(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    for (unsigned i = 0; i < qnamsiz; i++) {
        uint8_t ch = seg_r8(sd, (uint16_t)(SYSDAT_CLI_FCB + fcb_name + i));
        seg_w8(sd, (uint16_t)(SYSDAT_CLI_CUSPQPB + qpb_name + i), ch);
        seg_w8(sd, (uint16_t)(SYSDAT_CLI_ACB + acb_name + i), ch);
    }
    cmd_bxcx r = mpm_call(phys, sysdat, openq_entry, SYSDAT_CLI_CUSPQPB);
    if (r.cx != 0) {
        return r.cx;
    }
    uint16_t qaddr = seg_r16(sd, SYSDAT_CLI_CUSPQPB + qpb_qaddr);
    if (!(seg_r16(sd, (uint16_t)(qaddr + q_flags)) & qf_rsp)) {
        return e_no_queue;
    }
    /* cli_gq */
    if (mpm_call(phys, sysdat, cwriteq_entry, SYSDAT_CLI_CUSPQPB).cx != 0) {
        return e_q_full;
    }
    /* cli_qw */
    if (seg_r8(sd, SYSDAT_CLI_DFIL) == 0xFF) {
        prcusp(phys, sysdat);
    }
    seg_w8(sd, SYSDAT_CLI_ACB + acb_cns, seg_r8(sd, SYSDAT_CLI_CNS));
    seg_w8(sd, SYSDAT_CLI_ACB + acb_match, 0); /* false */
    seg_w16(sd, SYSDAT_CLI_ACB + acb_pd, 0);
    conasn(phys, sysdat);
    return 0;
}

static cmd_bxcx cli_exit(uint8_t *phys, uint16_t sysdat);
static cmd_bxcx cli_gopen(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs);

/* cli_rmpd -- release cli_pd back onto pul, then exit */
static cmd_bxcx cli_rmpd(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t pd = seg_r16(sd, SYSDAT_CLI_PD);
    seg_w16(sd, (uint16_t)(pd + p_link), seg_r16(sd, SYSDAT_PUL));
    seg_w16(sd, SYSDAT_PUL, pd);
    return cli_exit(phys, sysdat);
}

/* sys_file -- the opened file has the SYS attribute (t2') */
static int sys_file(const uint8_t *sd) {
    return (seg_r8(sd, SYSDAT_CLI_FCB + fcb_type + 1) & 0x80) != 0;
}

/*
 * cli_fload -- load cli_fcb (as a .CMD file) for execution: take a PD
 * from pul, set it up, and open the file -- on the current disk, then
 * (no explicit disk, and the search disk differs) on the search disk
 * if it's a SYS file there, under the current user and then user 0.
 */
static cmd_bxcx cli_fload(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t pd = seg_r16(sd, SYSDAT_PUL);
    if (pd == 0) {
        seg_w16(sd, SYSDAT_CLI_ERR, e_no_pd);
        return cli_exit(phys, sysdat);
    }
    seg_w16(sd, SYSDAT_PUL, seg_r16(sd, (uint16_t)(pd + p_link)));
    seg_w16(sd, SYSDAT_CLI_PD, pd);
    for (uint16_t i = 0; i < pdlen; i++) {
        seg_w8(sd, (uint16_t)(pd + i), 0);
    }

    /* initialize the PD for load */
    seg_w8(sd, SYSDAT_CLI_LDDSK, seg_r8(sd, SYSDAT_CLI_FCB));
    seg_w16(sd, (uint16_t)(pd + p_flag), pf_table);
    for (unsigned i = 0; i < pnamsiz; i++) {
        seg_w8(sd, (uint16_t)(pd + p_name + i), seg_r8(sd, (uint16_t)(SYSDAT_CLI_FCB + fcb_name + i)));
    }
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    seg_w8(sd, (uint16_t)(pd + p_dsk), seg_r8(sd, SYSDAT_CLI_DSK));
    seg_w8(sd, (uint16_t)(pd + p_user), seg_r8(sd, SYSDAT_CLI_USER));
    seg_w8(sd, (uint16_t)(pd + p_cns), seg_r8(sd, SYSDAT_CLI_CNS));
    seg_w8(sd, (uint16_t)(pd + p_lst),
           (uint8_t)(seg_r8(sd, (uint16_t)(rlr + p_lst)) - seg_r8(sd, SYSDAT_NCONDEV)));

    /* open the file: DMA = cli_dma, primed with the FCB's first 8 map bytes */
    for (unsigned i = 0; i < 8; i++) {
        seg_w8(sd, (uint16_t)(SYSDAT_CLI_DMA + i), seg_r8(sd, (uint16_t)(SYSDAT_CLI_FCB + fcb_dskmap + i)));
    }
    uint8_t *uda = run_uda(phys, sysdat);
    seg_w16(uda, u_dma_ofst, SYSDAT_CLI_DMA);
    seg_w16(uda, u_dma_seg, sysdat);
    seg_w8(sd, SYSDAT_CLI_FCB + fcb_type + 0, 'C');
    seg_w8(sd, SYSDAT_CLI_FCB + fcb_type + 1, 'M');
    seg_w8(sd, SYSDAT_CLI_FCB + fcb_type + 2, 'D');
    seg_w8(uda, u_error_mode, 0xFE);

    uint16_t bx = fileopen(phys, sysdat);
    if ((bx & 0xFF) != 0xFF) {
        return cli_gopen(phys, sysdat, user_retf_offset, cs);
    }
    if ((bx >> 8) == 0) {
        uint8_t cl = seg_r8(sd, SYSDAT_SRCHDISK);
        if (cl != seg_r8(sd, SYSDAT_CLI_DSK) && seg_r8(sd, SYSDAT_CLI_FCB) == 0) {
            /* try the system disk */
            seg_w8(sd, (uint16_t)(rlr + p_dsk), cl);
            bx = fileopen(phys, sysdat);
            if ((bx & 0xFF) != 0xFF) {
                if (sys_file(sd)) {
                    return cli_gopen(phys, sysdat, user_retf_offset, cs);
                }
                /* a non-SYS file: try user 0, if not already */
                fileclose(phys, sysdat);
                if (seg_r8(sd, (uint16_t)(rlr + p_user)) == 0) {
                    goto bopene;
                }
                seg_w8(sd, (uint16_t)(rlr + p_user), 0);
                bx = fileopen(phys, sysdat);
                if ((bx & 0xFF) != 0xFF) {
                    if (sys_file(sd)) {
                        return cli_gopen(phys, sysdat, user_retf_offset, cs);
                    }
                    fileclose(phys, sysdat);
                    goto bopene;
                }
            }
        }
    }
    /* cli_bopen -- an extended error is left as is */
    if ((bx >> 8) != 0) {
        return cli_rmpd(phys, sysdat);
    }
bopene:
    seg_w16(sd, SYSDAT_CLI_ERR, e_bad_open);
    return cli_rmpd(phys, sysdat);
}

/* cli_pfn_arg -- copy a parsed argument's FCB front (16 bytes) and
 * password pointer/length into the Base Page */
static void cli_pfn_arg(uint8_t *phys, uint16_t sysdat, uint8_t *bp, uint16_t fcb, uint16_t pwptr,
                        uint16_t pwlen) {
    const uint8_t *sd = far_ptr(phys, sysdat, 0);
    seg_w16(bp, pwptr, seg_r16(sd, SYSDAT_CLI_FCB + fcb_pptr));
    seg_w8(bp, pwlen, seg_r8(sd, SYSDAT_CLI_FCB + fcb_plen));
    for (unsigned i = 0; i < 16; i++) {
        seg_w8(bp, (uint16_t)(fcb + i), seg_r8(sd, (uint16_t)(SYSDAT_CLI_FCB + i)));
    }
}

/*
 * cli_gopen -- the file is open: load it (f_load, or f_cload for a
 * chain, both with bx = cli_pd), set up its Base Page (command tail,
 * default FCBs from up to two parsed arguments), create its process and
 * give it the console.
 */
static cmd_bxcx cli_gopen(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    int chain = seg_r8(sd, SYSDAT_CLI_CHAIN) == 0xFF;
    uint16_t pd = seg_r16(sd, SYSDAT_CLI_PD);
    cmd_bxcx ld;
    if (seg_r16(sd, (uint16_t)(rlr + p_flag)) & pf_ctlc) {
        ld = (cmd_bxcx){0xFFFF, e_abort};
        goto cli_cl;
    }
    /* cli_ld1 -- a chain gives the caller's UDA to the new PD, after
     * moving the caller onto SYSDAT's inituda block */
    if (chain) {
        uint16_t old_uda = rlr_uda(sd);
        seg_w16(sd, (uint16_t)(pd + p_uda), old_uda);
        uint16_t new_uda = (uint16_t)((SYSDAT_INITUDA >> 4) + sysdat);
        uint8_t *src = far_ptr(phys, old_uda, 0), *dst = far_ptr(phys, new_uda, 0);
        for (uint16_t i = 0; i < ulen; i++) {
            seg_w8(dst, i, seg_r8(src, i));
        }
        seg_w16(sd, (uint16_t)(rlr + p_uda), new_uda);
    }
    if (seg_r8(sd, SYSDAT_CLI_DFIL) == 0xFF) {
        prfilnam(phys, sysdat);
    }
    /* noprfil */
    if (chain) {
        seg_w8(sd, SYSDAT_CLI_TERM, 0xFF);
    }
    {
        uint16_t w = mpm_enter(phys, sysdat);
        ld = load(phys, sysdat, SYSDAT_CLI_FCB, pd, user_retf_offset, cs, chain);
        mpm_leave(phys, sysdat, w);
    }

cli_cl:
    seg_w8(run_uda(phys, sysdat), u_error_mode, 0);
    fileclose(phys, sysdat);
    if (ld.cx != 0) {
        if (ld.cx != e_abort) {
            seg_w16(sd, SYSDAT_CLI_ERR, ld.cx); /* cli_lnab */
        }
        return cli_rmpd(phys, sysdat);
    }

    /* cli_gload */
    uint16_t bpage = ld.bx;
    seg_w16(sd, SYSDAT_CLI_BPAGE, bpage);
    uint8_t *bp = far_ptr(phys, bpage, 0);
    for (uint16_t i = 0; i < 127; i++) {
        seg_w8(bp, (uint16_t)(bpg_dma + 1 + i), seg_r8(sd, (uint16_t)(SYSDAT_CLI_CMDTAIL + i)));
    }
    /* count the tail, upper-casing it */
    uint8_t cl = 0;
    for (uint16_t di = bpg_dma + 1; seg_r8(bp, di) != 0; di++, cl++) {
        uint8_t ch = seg_r8(bp, di);
        if (ch >= 'a' && ch <= 'z') {
            seg_w8(bp, di, (uint8_t)(ch & 0x5F));
        }
    }
    seg_w8(bp, bpg_dma, cl);

    /* default FCBs: disk 0, blank name/type, 2 zero words, twice */
    seg_w8(bp, bpg_lddsk, seg_r8(sd, SYSDAT_CLI_LDDSK));
    seg_w8(bp, bpg_fcb0, 0);
    for (unsigned i = 0; i < 11; i++) {
        seg_w8(bp, (uint16_t)(bpg_fcb0 + 1 + i), ' ');
    }
    for (unsigned i = 0; i < 4; i++) {
        seg_w8(bp, (uint16_t)(bpg_fcb0 + 12 + i), 0);
    }
    for (unsigned i = 0; i < 16; i++) {
        seg_w8(bp, (uint16_t)(bpg_fcb0 + 16 + i), seg_r8(bp, (uint16_t)(bpg_fcb0 + i)));
    }

    /* parse up to two arguments from the tail */
    if (seg_r8(sd, SYSDAT_CLI_CMDTAIL) != 0) {
        cmd_bxcx pr = pfn(phys, sysdat);
        if (pr.bx != 0xFFFF) {
            cli_pfn_arg(phys, sysdat, bp, bpg_fcb0, bpg_pw1ptr, bpg_pw1len);
            if (pr.bx != 0) {
                uint16_t saved = seg_r16(sd, SYSDAT_CLI_PCB + pcb_flnmptr);
                seg_w16(sd, SYSDAT_CLI_PCB + pcb_flnmptr, (uint16_t)(pr.bx + 1));
                cmd_bxcx pr2 = pfn(phys, sysdat);
                seg_w16(sd, SYSDAT_CLI_PCB + pcb_flnmptr, saved);
                if (pr2.bx != 0xFFFF) {
                    cli_pfn_arg(phys, sysdat, bp, bpg_fcb1, bpg_pw2ptr, bpg_pw2len);
                }
            }
        }
    }

    /* assign the console to the new process -- before creating it, so
     * it can't run without its console if the caller blocks in between
     * (docs/bugs.md #126) */
    if (seg_r8(sd, SYSDAT_CLI_DFIL) == 0xFF) {
        crlf(phys, sysdat);
    }
    seg_w8(sd, SYSDAT_CLI_ACB + acb_cns, seg_r8(sd, SYSDAT_CLI_CNS));
    seg_w16(sd, SYSDAT_CLI_ACB + acb_pd, pd);
    seg_w8(sd, SYSDAT_CLI_ACB + acb_match, 0xFF); /* true */
    seg_w16(sd, SYSDAT_CLI_ERR, conasn(phys, sysdat).cx);

    /* create the process: f_setprior(1) for a chain */
    if (chain) {
        uint16_t w = mpm_enter(phys, sysdat);
        set_prior_entry(phys, sysdat, 1);
        mpm_leave(phys, sysdat, w);
    }
    (void)MPMIF(command, phys, sysdat, f_createproc, seg_r16(sd, SYSDAT_CLI_PD), 0, sysdat);
    return cli_exit(phys, sysdat);
}

/*
 * cli_exit -- restore the caller's disk/user/error mode/DMA; then either
 * terminate (a chain that loaded or reached an RSP -- see the header) or
 * release MXCli, put back the caller's keep/tempkeep flags, terminate on
 * a pending ctl-C, and return cli_err.
 */
static cmd_bxcx cli_exit(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *uda = run_uda(phys, sysdat);
    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    seg_w8(sd, (uint16_t)(rlr + p_dsk), seg_r8(sd, SYSDAT_CLI_DSK));
    seg_w8(sd, (uint16_t)(rlr + p_user), seg_r8(sd, SYSDAT_CLI_USER));
    seg_w8(uda, u_error_mode, seg_r8(sd, SYSDAT_CLI_ERR_MODE));
    seg_w16(uda, u_dma_ofst, seg_r16(sd, SYSDAT_CLI_DMA_OFST));
    seg_w16(uda, u_dma_seg, seg_r16(sd, SYSDAT_CLI_DMA_SEG));
    if (seg_r8(sd, SYSDAT_CLI_CHAIN) == 0xFF && seg_r8(sd, SYSDAT_CLI_TERM) == 0xFF) {
        seg_w16(sd, (uint16_t)(rlr + p_flag), (uint16_t)(seg_r16(sd, (uint16_t)(rlr + p_flag)) &
                                                         (uint16_t)~(pf_keep | pf_sys | pf_tempkeep | pf_ctlc)));
        (void)MPMIF(command, phys, sysdat, f_terminate, 0, 0, sysdat); /* jmp mpmif */
        return (cmd_bxcx){0, 0};
    }

    /* clirls */
    mpm_call(phys, sysdat, writeq_entry, SYSDAT_MXCLIQPB); /* mxwrite */
    uint16_t keep = (uint16_t)(seg_r16(sd, SYSDAT_CLI_PFLAG) & (pf_tempkeep | pf_keep));
    rlr = seg_r16(sd, SYSDAT_RLR);
    uint16_t pflag = (uint16_t)((seg_r16(sd, (uint16_t)(rlr + p_flag)) & (uint16_t)~(pf_tempkeep | pf_keep)) | keep);
    seg_w16(sd, (uint16_t)(rlr + p_flag), pflag);
    if (pflag & pf_ctlc) {
        (void)MPMIF(command, phys, sysdat, f_terminate, 0, 0, sysdat);
        seg_w16(sd, SYSDAT_CLI_ERR, e_abort);
    }
    /* cli_nctl */
    uint16_t err = seg_r16(sd, SYSDAT_CLI_ERR);
    return (cmd_bxcx){err != 0 ? 0xFFFF : 0, err};
}

/*
 * clicmd_entry -- create a process from an ASCII command line
 *	input:	dx -> command buffer (CLICB) in u_wrkseg; 0 = chain, the
 *		command in the caller's DMA buffer
 *	output:	bx = 0 if successful, 0FFFFh on failure; cx = error code
 */
cmd_bxcx clicmd_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx, uint16_t user_retf_offset, uint16_t cs) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    mpm_call(phys, sysdat, readq_entry, SYSDAT_MXCLIQPB); /* mxread */

    uint16_t rlr = seg_r16(sd, SYSDAT_RLR);
    uint16_t pflag = seg_r16(sd, (uint16_t)(rlr + p_flag));
    seg_w16(sd, SYSDAT_CLI_PFLAG, pflag);
    seg_w16(sd, (uint16_t)(rlr + p_flag), (uint16_t)((pflag & (uint16_t)~pf_keep) | pf_tempkeep));

    int chain = dx == 0;
    seg_w8(sd, SYSDAT_CLI_CHAIN, chain ? 0xFF : 0);
    if (chain) {
        seg_w8(sd, SYSDAT_CLI_TERM, 0);
    }

    /* cli_cli -- defaults from the parent PD */
    seg_w8(sd, SYSDAT_CLI_DFIL, 0);
    if (seg_r8(sd, SYSDAT_DAYFILE) == 0xFF) {
        uint16_t w = mpm_enter(phys, sysdat);
        cmd_bxcx att = cconattch_entry(phys, sysdat);
        mpm_leave(phys, sysdat, w);
        if (att.cx == 0) {
            seg_w8(sd, SYSDAT_CLI_DFIL, 0xFF);
            prtime(phys, sysdat);
        }
    }
    uint8_t *uda = run_uda(phys, sysdat);
    rlr = seg_r16(sd, SYSDAT_RLR);
    seg_w16(sd, SYSDAT_CLI_PPD, rlr);
    seg_w8(sd, SYSDAT_CLI_DSK, seg_r8(sd, (uint16_t)(rlr + p_dsk)));
    seg_w8(sd, SYSDAT_CLI_USER, seg_r8(sd, (uint16_t)(rlr + p_user)));
    seg_w8(sd, SYSDAT_CLI_CNS, seg_r8(sd, (uint16_t)(rlr + p_cns)));
    seg_w8(sd, SYSDAT_CLI_ERR_MODE, seg_r8(uda, u_error_mode));
    seg_w16(sd, SYSDAT_CLI_DMA_OFST, seg_r16(uda, u_dma_ofst));
    seg_w16(sd, SYSDAT_CLI_DMA_SEG, seg_r16(uda, u_dma_seg));
    seg_w16(sd, SYSDAT_CLI_ERR, 0);

    /* copy the command into cli_cmdtail */
    if (chain) {
        const uint8_t *dma = far_ptr(phys, seg_r16(sd, SYSDAT_CLI_DMA_SEG), 0);
        uint16_t ofst = seg_r16(sd, SYSDAT_CLI_DMA_OFST);
        for (uint16_t i = 0; i < 128; i++) {
            seg_w8(sd, (uint16_t)(SYSDAT_CLI_CMDTAIL + i), seg_r8(dma, (uint16_t)(ofst + i)));
        }
    } else {
        const uint8_t *wrk = far_ptr(phys, seg_r16(uda, u_wrkseg), 0);
        seg_w8(sd, SYSDAT_CLI_NET, seg_r8(wrk, (uint16_t)(dx + cmdcb_net)));
        for (uint16_t i = 0; i < cmdcblen - cmdcb_cmd; i++) {
            seg_w8(sd, (uint16_t)(SYSDAT_CLI_CMDTAIL + i), seg_r8(wrk, (uint16_t)(dx + cmdcb_cmd + i)));
        }
    }

    /* cli_parse */
    cmd_bxcx pr = pfn(phys, sysdat);
    if (pr.cx != 0) {
        seg_w16(sd, SYSDAT_CLI_ERR, pr.cx);
        return cli_exit(phys, sysdat);
    }
    shifttail(phys, sysdat, pr.bx);

    /* no explicit disk or password: try an RSP first */
    if (seg_r8(sd, SYSDAT_CLI_FCB) == 0 && seg_r8(sd, SYSDAT_CLI_FCB + fcb_plen) == 0) {
        uint16_t err = cli_checkque(phys, sysdat);
        if (err == e_q_full) {
            seg_w16(sd, SYSDAT_CLI_ERR, err);
        }
        if (err == 0 || err == e_q_full) {
            /* cli_qful */
            if (chain) {
                seg_w8(sd, SYSDAT_CLI_TERM, 0xFF);
            }
            return cli_exit(phys, sysdat);
        }
    }
    return cli_fload(phys, sysdat, user_retf_offset, cs);
}

/* chain_entry -- clicmd_entry with dx = 0 */
cmd_bxcx chain_entry(uint8_t *phys, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    return clicmd_entry(phys, sysdat, 0, user_retf_offset, cs);
}
