/*
 * command.c -- C port of src/kernel/COMMAND.SUP's `clicmd_entry`
 * (the whole non-chain path -- a real, typed CLI command, not a
 * chain-loaded one) plus every routine it calls: `shifttail`,
 * `fileopen`, `user_retf`, the console-print helpers (`crlf`/
 * `prchar`/`prtime`/`prnum`/`prdisk`/`prcusp`/`prcsm`/`prtyp`/
 * `prnam`/`prn1`/`cprnt`/`pruser`/`prfilnam`), `cli_checkque`,
 * `cli_fload`, `cli_pfn` (this port's own name for the real `pfn`),
 * `mxwrite`/`mxread`, and `conasn`.
 *
 * `chain_entry` itself (always `dx==0` into `clicmd_entry`, i.e.
 * always `cli_chain==true`) is NOT ported: its own chain-mode UDA
 * copy (`cli_fload`'s own `cli_ld1` block) dynamically switches
 * `DS`/`ES` to the newly loaded process's own UDA segment and reads/
 * writes fixed `UDA.FMT` offsets relative to it -- the same real
 * segment-relative-addressing gap already solved for `PROC.RTM`/
 * `ML.MEM`/`MAU.MEM`/`SHARE.MEM`, just not applied here, since
 * `clicmd_entry`'s own ordinary path (a typed command, `cli_chain`
 * always false) is the far more common case and needs none of it.
 *
 * Many of `clicmd_entry`'s own `mpmif(...)` calls are now real
 * internal calls into already-ported routines, the same technique
 * `sysent.c`'s own `creat_proc_entry`/`que1.c`'s own `readq`/`writeq`
 * already use: `mxread`/`mxwrite` (`que1.c`'s `readq_entry`/
 * `writeq_entry`, both unconditional -- same real "retries forever if
 * the MXCli queue is empty/full" shape as `que1.c`'s own header
 * comment describes, which this port's own test avoids by always
 * pre-seeding that queue with room), `cli_checkque`'s own
 * `openq_entry`/`cwriteq_entry` (que1.c) and `conasn` (`console.c`'s
 * `conassign_entry`), `cconattch_entry` (`console.c`, conditional --
 * `CONSOLE.CIO`'s own real `ca_sleep` blocking path is never reached
 * since this call is always conditional here), `prchar`/`cprnt`
 * (`chario.c`'s `conout_entry`/`conprint_entry`), and `pfn`
 * (`parse.c`'s `parse_entry`, via this port's own `cli_pfn` helper).
 *
 * `cli_pcb`'s own two fields (`pcb_flnmptr`/`pcb_fcbptr`) and
 * `cli_cuspqpb`'s own `qpb_buffptr`/`qpb_nmsgs` are never written
 * anywhere in the real `COMMAND.SUP` source itself -- they're real
 * SYSDAT fields the real system's own boot-time init sets once and
 * COMMAND.SUP only ever *reads* (`cli_fload`'s own "2nd argument"
 * parse temporarily overwrites `pcb_flnmptr` and restores it, the
 * one exception). This port's own `cli_pfn`/`cli_checkque` set them
 * explicitly on every call instead of modeling that one-time init.
 *
 * `fileopen`'s own `mpmif(f_fopen)` (real file open, via `FILE1.BDO`/
 * `FILE2.BDO`'s own still-unported `srch`-dependent bulk -- see
 * docs/src-c.md) stays a boundary stub, but unlike a plain no-op,
 * `BL`/`BH` are genuinely undefined under that convention here (no
 * function-code sentinel the way `mpmif`'s own calls usually leave
 * one in `CX`) -- fixed at a well-defined constant instead, same
 * precedent as `CONSOLE.CIO`'s/`CPMMEM.MEM`'s own undefined-register
 * fixes: `BL=0xFF` (a hard open failure), `BH=0` (not an "extended"
 * error). `cli_fload`'s own real success path (the file actually
 * opens, loads, and a process gets created) is translated faithfully
 * below but never reachable under that fix -- same class as
 * `CONSOLE.CIO`'s own `ca_sleep`/`CPMMEM.MEM`'s own `free_seg` retry.
 * `f_load`/`f_cload`/`f_createproc`/`f_setprior`/`f_terminate` are
 * therefore left as plain boundary stubs too (never reached either,
 * so upgrading them here would add nothing testable) -- `f_load`'s
 * own real target (`load.c`'s `load`) and `f_createproc`'s (`sysent.c`'s
 * `creat_proc_entry`) are both already ported elsewhere regardless.
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} cmd_bxcx;

extern cmd_bxcx openq_entry(uint8_t *mem, uint16_t dx);
extern cmd_bxcx readq_entry(uint8_t *mem, uint16_t dx);
extern cmd_bxcx writeq_entry(uint8_t *mem, uint16_t dx);
extern cmd_bxcx cwriteq_entry(uint8_t *mem, uint16_t dx);
extern cmd_bxcx cconattch_entry(uint8_t *mem);
extern cmd_bxcx conassign_entry(uint8_t *mem, uint16_t dx);
extern void conout_entry(uint8_t *mem, uint8_t dl);
extern void conprint_entry(uint8_t *mem, uint16_t dx, uint8_t bh, uint8_t bl);
extern cmd_bxcx parse_entry(uint8_t *mem, uint16_t dx);

void shifttail(uint8_t *mem, uint16_t ax);

/*
 * shifttail -- shift the remaining command tail down to the start of
 * the command buffer (or clear it if there's no tail)
 *	input:	ax = output of a previous parsefilename call (0 = no tail)
 */
void shifttail(uint8_t *mem, uint16_t ax) {
    if (ax == 0) {
        mem[CLI_CMDTAIL] = 0;
        return;
    }
    uint16_t si = ax;
    uint16_t off = (uint16_t)(ax - CLI_CMDTAIL);
    uint16_t cx = (uint16_t)((uint16_t)(128 - off) >> 1); /* word count */
    uint16_t di = CLI_CMDTAIL;
    for (uint16_t i = 0; i < cx; i++) {
        mem_w16(mem, (uint16_t)(di + i * 2), mem_r16(mem, (uint16_t)(si + i * 2)));
    }
}

typedef struct {
    uint8_t bl, bh;
} fileopen_result;

/*
 * fileopen -- open cli_fcb (forcing read-only), via the unported
 * mpmif trampoline (f_fopen) -- see this file's own header comment
 * for why BL/BH are fixed at a well-defined constant instead of a
 * plain no-op (0xFF/0 -- a hard, non-extended open failure)
 */
fileopen_result fileopen(uint8_t *mem) {
    mem[(uint16_t)(CLI_FCB + fcb_name + 5)] |= 0x80; /* f6`= open read-only */
    fileopen_result r = {0xFF, 0};
    return r;
}

/* fileclose -- close cli_fcb; its own result is never checked by any
 * real caller in this file, so this stays void, same no-op boundary
 * stub as every other file this project ports treats an unread
 * result with */
void fileclose(uint8_t *mem) {
    (void)mem;
}

/*
 * user_retf -- cleanup when an RSP falls off the end of its own code
 * via a bare `retf` (the normal way an RSP like TMP finishes running)
 *
 * `and p_flag[bx],not (pf_keep+pf_tempkeep+pf_ctlc+pf_sys)` reads as
 * though it might hit the missing-parens `not` precedence bug
 * docs/bugs.md #80 found and fixed in this exact routine (`not` binds
 * to the single following term only, same precedence as unary `-`) --
 * but the parens shown here *are* that fix already applied (confirmed
 * directly against tools/asm86: `not (A+B+C+D)` assembles to the
 * correct mask `0xFD7C`, not the pre-fix `0x027E`). No bug to
 * reproduce; this is a plain, correct mask.
 */
void user_retf(uint8_t *mem) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
    uint16_t mask = (uint16_t)~(uint16_t)(pf_keep | pf_tempkeep | pf_ctlc | pf_sys);
    mem_w16(mem, (uint16_t)(rlr + p_flag), (uint16_t)(pflag & mask));
    /* mov cx,f_terminate!sub dx,dx!int mpmint -- boundary tail via a
     * software interrupt instead of mpmif's own usual near CALL;
     * nothing to port past this point, see header comment */
}

/* -- console-print helpers -- */

static void prchar(uint8_t *mem, uint8_t dl) {
    conout_entry(mem, dl);
}

static void crlf(uint8_t *mem) {
    prchar(mem, 13);
    prchar(mem, 10);
}

static void prnum(uint8_t *mem, uint8_t dl) {
    prchar(mem, (uint8_t)((dl >> 4) + '0'));
    prchar(mem, (uint8_t)((dl & 0x0F) + '0'));
}

static void prtime(uint8_t *mem) {
    prnum(mem, mem[SD_TOD + 2]); /* tod_hr */
    prchar(mem, ':');
    prnum(mem, mem[SD_TOD + 3]); /* tod_min */
    prchar(mem, ':');
    prnum(mem, mem[SD_TOD + 4]); /* tod_sec */
    prchar(mem, ' ');
}

static void prdisk(uint8_t *mem) {
    uint8_t dl = mem[CLI_FCB + fcb_dr];
    if (dl != 0) {
        dl = (uint8_t)(dl - 1);
    } else {
        uint16_t rlr = mem_r16(mem, SD_RLR);
        dl = mem[(uint16_t)(rlr + p_dsk)];
    }
    prchar(mem, (uint8_t)(dl + 'A'));
    prchar(mem, ':');
}

/* write_cstr -- copy a NUL-terminated literal into the flat memory
 * image at a fixed scratch address, same technique chario.c's own
 * helper of the same name uses for a CS-resident string this file's
 * own `questr`/`userstr` stand in for */
static void write_cstr(uint8_t *mem, uint16_t addr, const char *s) {
    while (*s != '\0') {
        mem[addr] = (uint8_t)*s;
        addr = (uint16_t)(addr + 1);
        s++;
    }
    mem[addr] = 0;
}

static void prcsm(uint8_t *mem, uint16_t dx) {
    conprint_entry(mem, dx, 0, 0); /* no max length, stop at the NUL */
}

static void prnam(uint8_t *mem, uint16_t dx) {
    conprint_entry(mem, dx, 8, ' ');
}

static void prtyp(uint8_t *mem, uint16_t dx) {
    conprint_entry(mem, dx, 3, ' ');
}

static void pruser(uint8_t *mem) {
    write_cstr(mem, userstr_addr, "(User 0)");
    prcsm(mem, userstr_addr);
}

static void prcusp(uint8_t *mem) {
    prnam(mem, (uint16_t)(CLI_CUSPQPB + qpb_name));
    write_cstr(mem, questr_addr, " Msg Qued");
    prcsm(mem, questr_addr);
    crlf(mem);
}

static void prfilnam(uint8_t *mem) {
    prdisk(mem);
    prnam(mem, (uint16_t)(CLI_FCB + fcb_name));
    prchar(mem, '.');
    prtyp(mem, (uint16_t)(CLI_FCB + fcb_type));
    prchar(mem, ' ');
    if (mem[CLI_FCB + fcb_name + 7] & 0x80) {
        pruser(mem);
        return;
    }
    if (mem[SD_CLI_USER] == 0) {
        return;
    }
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (mem[(uint16_t)(rlr + p_user)] == 0) {
        pruser(mem);
    }
}

static void conasn(uint8_t *mem) {
    conassign_entry(mem, CLI_ACB);
}

/*
 * cli_pfn -- parse cli_fcb from the given flnmptr, via parse_entry.
 * Sets cli_pcb's own two fields explicitly on every call -- see this
 * file's own header comment for why.
 */
static cmd_bxcx cli_pfn(uint8_t *mem, uint16_t flnmptr) {
    mem_w16(mem, (uint16_t)(CLI_PCB + pcb_flnmptr), flnmptr);
    mem_w16(mem, (uint16_t)(CLI_PCB + pcb_fcbptr), CLI_FCB);
    return parse_entry(mem, CLI_PCB);
}

/*
 * cli_checkque -- try an RSP access: open a queue named after
 * cli_fcb, confirm it's flagged qf_rsp, write the command tail to
 * it, and assign this process's own console to whatever process owns
 * that name.
 *	output: returns the error code (0 = successful RSP access)
 */
static uint16_t cli_checkque(uint8_t *mem) {
    for (unsigned i = 0; i < qnamsiz; i++) {
        uint8_t ch = mem[CLI_FCB + fcb_name + i];
        mem[CLI_CUSPQPB + qpb_name + i] = ch;
        mem[CLI_ACB + acb_name + i] = ch;
    }

    cmd_bxcx r = openq_entry(mem, CLI_CUSPQPB);
    if (r.cx != 0) {
        return r.cx;
    }

    uint16_t qaddr = mem_r16(mem, (uint16_t)(CLI_CUSPQPB + qpb_qaddr));
    if (!(mem_r16(mem, (uint16_t)(qaddr + q_flags)) & qf_rsp)) {
        return e_no_queue;
    }

    /* cli_gq -- write the command tail to the queue. qpb_buffptr/
     * qpb_nmsgs are never written in the real asm either -- same
     * boot-time-constant convention as cli_pcb, see header comment */
    mem_w16(mem, (uint16_t)(CLI_CUSPQPB + qpb_buffptr), CLI_CMDTAIL);
    mem_w16(mem, (uint16_t)(CLI_CUSPQPB + qpb_nmsgs), 1);
    cmd_bxcx w = cwriteq_entry(mem, CLI_CUSPQPB);
    if (w.cx != 0) {
        return e_q_full;
    }

    /* cli_qw -- successful queue write, assign console */
    if (mem[SD_CLI_DFIL]) {
        prcusp(mem);
    }
    mem[CLI_ACB + acb_cns] = mem[SD_CLI_CNS];
    mem[CLI_ACB + acb_match] = 0;
    mem_w16(mem, (uint16_t)(CLI_ACB + acb_pd), 0);
    conasn(mem);
    return 0;
}

static cmd_bxcx clicmd_exit(uint8_t *mem);
static cmd_bxcx cli_rmpd(uint8_t *mem, uint16_t cli_pd);
static cmd_bxcx cli_gopen(uint8_t *mem, uint16_t cli_pd);

/*
 * cli_fload -- try to load cli_fcb (given type forced to 'CMD') for
 * execution. The real open-the-file step (fileopen) is fixed at a
 * deterministic failure (see this file's own header comment), so
 * this always ends in e_bad_open -- the branches below are
 * translated faithfully (matching exactly which retry the real asm
 * takes for a given cli_dsk/srchdisk/cli_fcb combination) but every
 * one of them lands on the same outcome under that fix.
 */
static cmd_bxcx cli_fload(uint8_t *mem) {
    uint16_t pul = mem_r16(mem, SD_PUL);
    if (pul == 0) {
        mem_w16(mem, SD_CLI_ERR, e_no_pd);
        return clicmd_exit(mem); /* jmp cli_exit -- no PD to release */
    }
    mem_w16(mem, SD_PUL, mem_r16(mem, (uint16_t)(pul + p_link)));
    uint16_t cli_pd = pul;
    mem_w16(mem, SD_CLI_PD, cli_pd);

    /* zero PD */
    for (uint16_t i = 0; i < pdlen; i++) {
        mem[(uint16_t)(cli_pd + i)] = 0;
    }

    /* Initialize the PD for Load */
    mem[SD_CLI_LDDSK] = mem[CLI_FCB + fcb_dr];
    mem_w16(mem, (uint16_t)(cli_pd + p_flag), pf_table);
    for (unsigned i = 0; i < pnamsiz; i++) {
        mem[(uint16_t)(cli_pd + p_name + i)] = mem[CLI_FCB + fcb_name + i];
    }
    uint16_t rlr = mem_r16(mem, SD_RLR);
    mem[cli_pd + p_dsk] = mem[SD_CLI_DSK];
    mem[cli_pd + p_user] = mem[SD_CLI_USER];
    mem[cli_pd + p_cns] = mem[SD_CLI_CNS];
    mem[cli_pd + p_lst] = (uint8_t)(mem[(uint16_t)(rlr + p_lst)] - mem[SD_NCONDEV]);

    /* 3. Open the file -- cli_dma/u_dma_ofst/u_dma_seg setup has no
     * observable effect under fileopen's own fixed-failure outcome
     * (nothing downstream ever reads the DMA buffer it would have
     * read into), so it's not modeled -- same reasoning as other
     * provably-dead register/memory setup elsewhere in this project */
    mem[CLI_FCB + fcb_type + 0] = 'C';
    mem[CLI_FCB + fcb_type + 1] = 'M';
    mem[CLI_FCB + fcb_type + 2] = 'D';

    fileopen_result o = fileopen(mem);
    if (o.bl != 0xFF) {
        return cli_gopen(mem, cli_pd); /* unreachable under the fixed fileopen result */
    }

    int go_bopen;
    if (o.bh != 0) {
        go_bopen = 1; /* extended error -- never produced by the fixed result */
    } else if (mem[SD_SRCHDISK] == mem[SD_CLI_DSK]) {
        go_bopen = 1;
    } else if (mem[CLI_FCB + fcb_dr] != 0) {
        go_bopen = 1;
    } else {
        /* try the system disk */
        mem[(uint16_t)(rlr + p_dsk)] = mem[SD_SRCHDISK];
        fileopen_result o2 = fileopen(mem);
        if (o2.bl != 0xFF) {
            if (mem[CLI_FCB + fcb_type + 1] & 0x80) {
                return cli_gopen(mem, cli_pd); /* unreachable */
            }
            fileclose(mem);
            uint16_t rlr2 = mem_r16(mem, SD_RLR);
            if (mem[(uint16_t)(rlr2 + p_user)] != 0) {
                mem[(uint16_t)(rlr2 + p_user)] = 0;
                fileopen_result o3 = fileopen(mem);
                if (o3.bl != 0xFF) {
                    if (mem[CLI_FCB + fcb_type + 1] & 0x80) {
                        return cli_gopen(mem, cli_pd); /* unreachable */
                    }
                    fileclose(mem);
                }
            }
            /* could not find CMD file (fell through from the SYS-attribute
             * recovery attempts above) -- cli_bopene */
            mem_w16(mem, SD_CLI_ERR, e_bad_open);
            return cli_rmpd(mem, cli_pd);
        }
        go_bopen = 1;
    }
    if (go_bopen) {
        if (o.bh != 0) {
            return cli_rmpd(mem, cli_pd); /* cli_bopen, extended error -- unreachable */
        }
        mem_w16(mem, SD_CLI_ERR, e_bad_open); /* cli_bopene */
        return cli_rmpd(mem, cli_pd);
    }
    return cli_rmpd(mem, cli_pd);
}

/*
 * cli_gopen -- the real successful-open path: a pf_ctlc abort check,
 * then (if cli_dfil) logging the filename via prfilnam, then loading
 * the program (f_load/f_cload) and creating its process. Unreachable
 * under fileopen's own fixed, always-failing result (see this file's
 * own header comment), and not translated past prfilnam -- the rest
 * depends on f_load's own result, which is a plain boundary stub for
 * the same reason f_createproc/f_setprior/f_terminate are (see header
 * comment), so there is nothing further here that's actually testable.
 */
static cmd_bxcx cli_gopen(uint8_t *mem, uint16_t cli_pd) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (mem_r16(mem, (uint16_t)(rlr + p_flag)) & pf_ctlc) {
        cmd_bxcx result = {0xFFFF, e_abort};
        return result;
    }
    if (mem[SD_CLI_DFIL]) {
        prfilnam(mem);
    }
    return cli_rmpd(mem, cli_pd);
}

/* cli_rmpd -- release cli_pd back onto pul's own free list, then exit */
static cmd_bxcx cli_rmpd(uint8_t *mem, uint16_t cli_pd) {
    uint16_t pul = mem_r16(mem, SD_PUL);
    mem_w16(mem, (uint16_t)(cli_pd + p_link), pul);
    mem_w16(mem, SD_PUL, cli_pd);
    return clicmd_exit(mem);
}

/*
 * clicmd_exit -- cli_exit/clirls/cli_nctl/cli_gexit: restore the
 * running process's own saved error_mode/dma_ofst/dma_seg, write the
 * MXCli queue to release it (mxwrite, a real call to writeq_entry --
 * same convention as mxread in clicmd_entry below), merge cli_pflag's
 * own pf_tempkeep/pf_keep bits back into the live p_flag, and report
 * cli_err. `cli_chain` is always false in this port's own scope (see
 * this file's own header comment), so the chain-only `f_terminate`
 * tail-jump branch right after the dsk/user/err_mode/dma restores is
 * omitted entirely, and `pf_ctlc`'s own `f_terminate` call stays a
 * boundary stub -- same "dispatch-adjacent" exclusion `f_createproc`/
 * `f_setprior` get elsewhere in this file, since nothing here ever
 * reads its result either.
 */
static cmd_bxcx clicmd_exit(uint8_t *mem) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    mem[(uint16_t)(rlr + p_dsk)] = mem[SD_CLI_DSK];
    mem[(uint16_t)(rlr + p_user)] = mem[SD_CLI_USER];
    mem[UDA_ERROR_MODE] = mem[SD_CLI_ERR_MODE];
    mem_w16(mem, UDA_DMA_OFST, mem_r16(mem, SD_CLI_DMA_OFST));
    mem_w16(mem, UDA_DMA_SEG, mem_r16(mem, SD_CLI_DMA_SEG));

    /* clirls */
    writeq_entry(mem, MXCLIQPB); /* mxwrite */
    uint16_t keepbits = (uint16_t)(mem_r16(mem, SD_CLI_PFLAG) & (pf_tempkeep | pf_keep));
    rlr = mem_r16(mem, SD_RLR);
    uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
    pflag = (uint16_t)((pflag & (uint16_t)~(uint16_t)(pf_tempkeep | pf_keep)) | keepbits);
    mem_w16(mem, (uint16_t)(rlr + p_flag), pflag);

    if (pflag & pf_ctlc) {
        /* mov cx,f_terminate!sub dx,dx!call mpmif -- stays a boundary
         * stub, see this function's own header comment */
        mem_w16(mem, SD_CLI_ERR, e_abort);
    }

    uint16_t err = mem_r16(mem, SD_CLI_ERR);
    cmd_bxcx result;
    result.cx = err;
    result.bx = (err != 0) ? 0xFFFF : 0;
    return result;
}

/*
 * clicmd_entry -- the non-chain path of COMMAND.SUP's own `chain_entry`/
 * `clicmd_entry` (see this file's own header comment for why chain
 * mode, always `dx==0`, is excluded): run a real CLI command (`dx`
 * points to the caller's own `CLICB.DEF`-shaped buffer, in the
 * caller's own `UDA_WRKSEG`), either as an RSP-queue dispatch
 * (`cli_checkque`) or a CMD-file load (`cli_fload`).
 */
cmd_bxcx clicmd_entry(uint8_t *mem, uint16_t dx) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);

    readq_entry(mem, MXCLIQPB); /* mxread */

    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
    mem_w16(mem, SD_CLI_PFLAG, pflag);
    pflag = (uint16_t)((pflag & (uint16_t)~(uint16_t)pf_keep) | pf_tempkeep);
    mem_w16(mem, (uint16_t)(rlr + p_flag), pflag);

    mem[SD_CLI_CHAIN] = 0;

    /* cli_cli */
    mem[SD_CLI_DFIL] = 0;
    if (mem[SD_DAYFILE] == 0xFF) {
        cmd_bxcx att = cconattch_entry(mem);
        if (att.cx == 0) {
            mem[SD_CLI_DFIL] = 1;
            prtime(mem);
        }
    }
    rlr = mem_r16(mem, SD_RLR);
    mem_w16(mem, SD_CLI_PPD, rlr);
    mem[SD_CLI_DSK] = mem[(uint16_t)(rlr + p_dsk)];
    mem[SD_CLI_USER] = mem[(uint16_t)(rlr + p_user)];
    mem[SD_CLI_CNS] = mem[(uint16_t)(rlr + p_cns)];
    mem[SD_CLI_ERR_MODE] = mem[UDA_ERROR_MODE];
    mem_w16(mem, SD_CLI_DMA_OFST, mem_r16(mem, UDA_DMA_OFST));
    mem_w16(mem, SD_CLI_DMA_SEG, mem_r16(mem, UDA_DMA_SEG));
    mem_w16(mem, SD_CLI_ERR, 0);

    /* cli_copy -- cli_chain is always false here, so always the
     * non-chain copy (clicb_net/clicb_cmd..clicblen out of the
     * caller's own CLICB-shaped buffer at wrkseg:dx), never the
     * chain-mode UDA copy this port excludes (see header comment) */
    mem[SD_CLI_NET] = mem_r8_seg(mem, wrkseg, (uint16_t)(dx + cmdcb_net));
    for (uint16_t i = 0; i < cmdcblen - cmdcb_cmd; i++) {
        mem[CLI_CMDTAIL + i] = mem_r8_seg(mem, wrkseg, (uint16_t)(dx + cmdcb_cmd + i));
    }

    /* cli_parse */
    cmd_bxcx pr = cli_pfn(mem, CLI_CMDTAIL);
    if (pr.cx != 0) {
        mem_w16(mem, SD_CLI_ERR, pr.cx);
        return clicmd_exit(mem);
    }

    /* cli_goodparse */
    shifttail(mem, pr.bx);
    if (mem[CLI_FCB + fcb_dr] == 0 && mem[CLI_FCB + fcb_plen] == 0) {
        uint16_t qerr = cli_checkque(mem);
        if (qerr == 0) {
            /* cli_qful -- successful RSP access */
            return clicmd_exit(mem);
        }
        if (qerr == e_q_full) {
            mem_w16(mem, SD_CLI_ERR, qerr);
            return clicmd_exit(mem);
        }
        /* any other cli_checkque error falls to cli_fload, same as
         * the real asm's own cli_ffload check */
    }
    return cli_fload(mem);
}
