/*
 * command.c -- C port of src/kernel/COMMAND.SUP's `chain_entry`/
 * `clicmd_entry` (both paths -- a real, typed CLI command *and* a
 * chain-loaded one, `dx==0`) plus every routine it calls:
 * `shifttail`, `fileopen`, `user_retf`, the console-print helpers
 * (`crlf`/`prchar`/`prtime`/`prnum`/`prdisk`/`prcusp`/`prcsm`/`prtyp`/
 * `prnam`/`prn1`/`cprnt`/`pruser`/`prfilnam`), `cli_checkque`,
 * `cli_fload`, `cli_pfn` (this port's own name for the real `pfn`),
 * `mxwrite`/`mxread`, and `conasn`.
 *
 * `chain_entry` is a thin wrapper (`sub dx,dx` then fall into
 * `clicmd_entry`). Chain mode's own three real differences from the
 * ordinary path, all gated on `SD_CLI_CHAIN` (set once, in
 * `clicmd_entry`, from `dx==0`, same as the real asm's own
 * `cmp dx,0`):
 *
 *   1. `cli_copy`'s own command-tail copy source: the ordinary path
 *      copies from the caller's own `CLICB.DEF`-shaped buffer
 *      (`wrkseg:dx`); chain mode instead copies 128 bytes from the
 *      *calling process's own* DMA buffer (`cli_dma_seg:cli_dma_ofst`,
 *      both just saved a few lines earlier in this same function) --
 *      genuinely segment-relative on the source side (`mem_r8_seg`),
 *      same as every other dynamic-segment read in this tree.
 *
 *   2. `cli_gopen`'s own `cli_ld1` block: chain mode hands the
 *      *current* process's own live UDA memory off to the newly
 *      allocated child PD in place (`cli_pd`'s own `p_uda` := the
 *      running process's own `p_uda`, read via `rlr`, standing in for
 *      the real asm's own ambient `ES`) rather than letting the
 *      ordinary `f_load`/`load_entry` path allocate fresh UDA memory
 *      -- but the *running* process is still executing out of that
 *      same memory, so it first copies its own live UDA content onto
 *      a shared scratch block (`SYSDAT_INITUDA`, the exact same block
 *      `INIT.SUP`'s own `init` reserves for its own init process at
 *      cold boot, `defs.h`'s own header comment has the detail) and
 *      relocates its own `p_uda` there, continuing to run from the
 *      scratch block while its original memory now belongs entirely
 *      to the new child. Both the UDA copy and the relocation are
 *      real segment-relative addressing (`mem_r8_seg`/`mem_w8_seg`/
 *      `mem_r16_seg` against a genuine runtime segment, not a fixed
 *      `SD_*` flat address) -- the same gap this file's own header
 *      comment used to say chain mode was blocked on, now resolved
 *      the same way `PROC.RTM`/`ML.MEM`/`MAU.MEM`/`SHARE.MEM` already
 *      were. The real asm's own accompanying `mov ss,ax` (the running
 *      process's own stack also relocates onto the scratch block) has
 *      no C equivalent, same as every other real stack switch in this
 *      port (see `init.c`'s own header comment); the real asm's own
 *      `pushf!cli!...!popf` around the `p_uda` reassignment has no
 *      consumer here either (this port's own host is single-threaded).
 *      `load()` is then called with `load_chain=1` and `bx=cli_pd`
 *      (not `bx=0`) -- `load.c`'s own `cload_entry` already documents
 *      this exact `bx=PD` convention, just inlined here directly
 *      rather than going through that thin wrapper, same as the
 *      ordinary path already inlines `load_entry`'s own `bx=0`.
 *
 *   3. `clicmd_exit`'s own tail: if chain mode set `SD_CLI_TERM`
 *      (meaning the process should terminate on exit -- either
 *      because `cli_gopen` reached the real `load()` call, or because
 *      `cli_checkque` succeeded first), the real asm's own `jmp
 *      mpmif(f_terminate)` is a true tail jump, never returning to
 *      `clicmd_exit`'s own caller in the real system at all (the
 *      process is torn down). `f_terminate` is a boundary-stub no-op
 *      everywhere else in this project (nothing downstream ever reads
 *      its result), so under that same convention this port just
 *      clears the documented `p_flag` bits and returns a fixed
 *      `(bx,cx)=(0,0)` -- an "undefined artifact, fixed choice" (same
 *      category as `init.c`'s own `io_maxconsole`/`io_maxlist`), since
 *      no real caller ever actually observes this return value.
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
 * COMMAND.SUP only ever *reads* them, relying on `pcb_flnmptr` still
 * holding `CLI_CMDTAIL` (set by `clicmd_entry`'s own earlier
 * `cli_pfn` call) by the time `cli_gopen`'s own "2nd argument" parse
 * runs -- `shifttail` has already rewritten `CLI_CMDTAIL`'s own
 * *content* down to just the command's own arguments by then, so
 * re-parsing from the same pointer parses the first argument, not
 * the command name again. This port's own `cli_pfn`/`cli_checkque`
 * set both fields explicitly on every call instead of modeling that
 * one-time init, which is equivalent as long as nothing else ever
 * writes `pcb_fcbptr` to something other than `CLI_FCB` -- true
 * throughout this file.
 *
 * `fileopen`'s own `mpmif(f_fopen)` and `cli_gopen`'s own
 * `mpmif(f_load)` are both real calls now: traced through
 * `MODFUNC.DEF`/`BDOSIF.BDO`'s own `functab` (`f_fopen` = function
 * 15 = `bdos` module func#2 = `FILE4.BDO`'s own `func15`) and
 * `SUPIF.SUP`'s own `supfunc` (`f_load` = function 59 = `sup` module
 * func#4 = `LOAD.SUP`'s own `load_entry`) to routines already ported
 * elsewhere -- the same "boundary call turns out to resolve to an
 * already-ported routine once actually traced" pattern as `TMP.A86`/
 * `SUBMIT.A86`'s own `parsefilename`/`mpm_clicmd` and `LOAD.SUP`'s own
 * `ld_freeit` (see docs/src-c.md's own changelog entries for all).
 * `fileopen` replicates `BDOSIF.BDO`'s own shared dispatch convention
 * by hand around the direct call (`BD_INFO = DX` before, `BX =
 * BD_ARET` after -- see `fileopen`'s own header comment); `load_entry`
 * itself always forces `bx=0` regardless of its own caller's `bx`
 * (`sub bx,bx ! jmp load`), so `cli_gopen` calls `load.c`'s `load`
 * directly with `bx=0` rather than reproducing that -- `cli_pd` is
 * never passed into `load` at all on this path (see `cli_gopen`'s own
 * header comment for why).
 *
 * This makes `cli_fload`'s own real success path (a file that
 * genuinely opens, loads, and gets its own Base Page set up) reachable
 * for the first time. `f_createproc` (`sysent.c`'s `creat_proc_entry`,
 * whose own real tail is a dispatcher switch, not an ordinary
 * call/return -- see `dsptch.c`'s own header comment) and `f_setprior`
 * stay plain boundary stubs, same as `f_terminate` already was (in
 * `clicmd_exit`'s own `pf_ctlc` branch) -- all three are
 * "dispatch-adjacent," their own results never read by anything this
 * file does afterward, so this doesn't truncate the translation at
 * all (see `cli_gopen`'s own header comment for the detail).
 *
 * `cli_gopen`'s own real continuation (everything past `prfilnam`) has
 * no differential test of its own, for the same underlying reason
 * `func15` itself has none: reaching it requires `fileopen` to
 * genuinely succeed, which requires a real, matching directory entry
 * -- `FILE1-4.BDO`'s own directory/search machinery, which no test in
 * this project (including `func15`'s own) currently fakes convincingly
 * enough to drive from the outside, on top of which bundling
 * `LOAD.SUP`'s own real asm into `tests/src-c/fixtures/
 * command_wrapper.A86` would also need to switch that wrapper's own
 * reference run from non-segmented to `segmented=True` (`load_finish`'s
 * own UDA writes are genuinely segment-relative), re-verifying every
 * existing case there for safety first -- same shape of investment as
 * `test_memory.py`'s own `segmented=True` migration, just not spent
 * here. This translation was derived carefully by hand against the
 * real asm (field offsets, the default-FCB "duplicate the first 16
 * bytes into the next 16" trick, `pcb_flnmptr`'s own reuse across the
 * two argument parses) and reviewed for compile-time and logical
 * soundness, but treat it with the same caution as any large,
 * untested translation.
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
extern void func15(uint8_t *mem, uint16_t ds_bdos);
extern cmd_bxcx load(uint8_t *mem, uint16_t dx, uint16_t bx, uint16_t sysdat,
                      uint16_t user_retf_offset, uint16_t cs, int load_chain);

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
 * fileopen -- open cli_fcb (forcing read-only), a real call now into
 * `file4.c`'s already-ported `func15` (BDOS function 15, OPEN) --
 * see this file's own header comment for how `mpmif(f_fopen)` was
 * traced to it. `BDOSIF.BDO`'s own shared dispatch sets `BD_INFO =
 * DX` before calling a functab target, and its own shared `RETMON`
 * tail reads the result back as `BX = BD_ARET` afterward -- both
 * replicated here by hand around the direct call, same convention as
 * `LBDOSENT.A86`'s own `oldbdos`/`goback` (src-c/loader/lbdosent.c).
 */
fileopen_result fileopen(uint8_t *mem) {
    mem[(uint16_t)(CLI_FCB + fcb_name + 5)] |= 0x80; /* f6`= open read-only */
    mem_w16(mem, BD_INFO, CLI_FCB);
    func15(mem, 0); /* 0 -- same flat convention as this file's own BD_INFO/BD_ARET access */
    uint16_t aret = mem_r16(mem, BD_ARET);
    fileopen_result r = {(uint8_t)(aret & 0xFF), (uint8_t)(aret >> 8)};
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

static cmd_bxcx conasn(uint8_t *mem) {
    return conassign_entry(mem, CLI_ACB);
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
static cmd_bxcx cli_gopen(uint8_t *mem, uint16_t cli_pd, uint16_t sysdat,
                           uint16_t user_retf_offset, uint16_t cs);

/* cli_qful -- successful RSP-queue access (or a full MXCli queue on
 * the way there): in chain mode this process terminates on exit */
static cmd_bxcx cli_qful(uint8_t *mem) {
    if (mem[SD_CLI_CHAIN]) {
        mem[SD_CLI_TERM] = 0xFF; /* true -- SYSTEM.DEF: true equ 0ffffh */
    }
    return clicmd_exit(mem);
}

/*
 * cli_fload -- try to load cli_fcb (given type forced to 'CMD') for
 * execution. `fileopen` is a real call now (see this file's own
 * header comment), so every branch below -- which retry runs for a
 * given cli_dsk/srchdisk/cli_fcb combination, and whether the file is
 * ultimately found -- is genuinely reachable both ways.
 */
static cmd_bxcx cli_fload(uint8_t *mem, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
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
        return cli_gopen(mem, cli_pd, sysdat, user_retf_offset, cs);
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
                return cli_gopen(mem, cli_pd, sysdat, user_retf_offset, cs);
            }
            fileclose(mem);
            uint16_t rlr2 = mem_r16(mem, SD_RLR);
            if (mem[(uint16_t)(rlr2 + p_user)] != 0) {
                mem[(uint16_t)(rlr2 + p_user)] = 0;
                fileopen_result o3 = fileopen(mem);
                if (o3.bl != 0xFF) {
                    if (mem[CLI_FCB + fcb_type + 1] & 0x80) {
                        return cli_gopen(mem, cli_pd, sysdat, user_retf_offset, cs);
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
 * then (if cli_dfil) logging the filename via prfilnam, then (in
 * chain mode) the `cli_ld1` UDA hand-off (see this file's own header
 * comment), then loading the program (`load.c`'s own `load`, a real
 * call). Non-chain is always the `f_load`/`load_entry` path --
 * `load_entry`'s own real asm ignores its own caller's `bx` and
 * always forces `bx=0`, i.e. "don't init a PD", which is why `cli_pd`
 * itself isn't passed into `load` on that path -- `cli_pd`'s own
 * process gets created separately, below, by `f_createproc`. Chain
 * mode is always `f_cload`/`cload_entry`'s own `bx=PD` convention
 * instead (`load.c`'s own `cload_entry` documents it; inlined here
 * directly rather than going through that wrapper, same as the
 * non-chain path already inlines `load_entry`'s own `bx=0`). On
 * success, `cli_gload`'s own Base Page setup
 * (copy+uppercase the command tail into the new program's own DMA
 * buffer, build its own default FCB, and parse up to two filename
 * arguments from the tail into it) needs real segment-relative
 * addressing (`cli_bpage`, the Base Page segment `load` just
 * returned) -- the same already-established `mem_*_seg` pattern
 * `PROC.RTM`/`ML.MEM`/`MAU.MEM`/`SHARE.MEM`/`load.c` itself all use.
 *
 * `f_createproc`'s own real target, `sysent.c`'s `creat_proc_entry`,
 * ends in `jmp pdisp` -- a real dispatcher switch, not an ordinary
 * call/return (see `dsptch.c`'s own header comment on why this
 * port's single-threaded host has no equivalent for that). Luckily
 * nothing downstream ever reads its own BX/CX (the real asm's own
 * "mov cli_err,cx" right before `cli_exit` reads CX from `conasn`,
 * called *after* `f_createproc`, not from it) -- so it stays a true
 * no-op boundary stub here, same convention as `f_setprior`/
 * `f_terminate`, without truncating the translation at all: every
 * real instruction after it (console assignment via `conasn`, the
 * final `cli_err`/`cli_exit`) is still faithfully ported below.
 */
static cmd_bxcx cli_gopen(uint8_t *mem, uint16_t cli_pd, uint16_t sysdat,
                           uint16_t user_retf_offset, uint16_t cs) {
    uint16_t rlr = mem_r16(mem, SD_RLR);
    if (mem_r16(mem, (uint16_t)(rlr + p_flag)) & pf_ctlc) {
        cmd_bxcx result = {0xFFFF, e_abort};
        return result;
    }
    /* cli_ld1 -- chain mode hands the current process's own live UDA
     * off to cli_pd, relocating the current process onto a shared
     * scratch UDA block first (see this file's own header comment) */
    if (mem[SD_CLI_CHAIN]) {
        uint16_t old_uda = mem_r16(mem, (uint16_t)(rlr + p_uda)); /* es */
        mem_w16(mem, (uint16_t)(cli_pd + p_uda), old_uda);
        uint16_t scratch_seg = (uint16_t)((SYSDAT_INITUDA >> 4) + sysdat);
        for (uint16_t i = 0; i < ulen; i++) {
            mem_w8_seg(mem, scratch_seg, i, mem_r8_seg(mem, old_uda, i));
        }
        mem_w16(mem, (uint16_t)(rlr + p_uda), scratch_seg);
    }

    if (mem[SD_CLI_DFIL]) {
        prfilnam(mem);
    }

    /* noprfil/cli_ld -- f_cload (bx=cli_pd) in chain mode, f_load
     * (bx=0) otherwise */
    int chain = mem[SD_CLI_CHAIN] != 0;
    if (chain) {
        mem[SD_CLI_TERM] = 0xFF; /* true */
    }
    cmd_bxcx ld = load(mem, CLI_FCB, chain ? cli_pd : 0, sysdat, user_retf_offset, cs, chain);

    /* cli_cl */
    mem[UDA_ERROR_MODE] = 0;
    fileclose(mem);
    if (ld.cx != 0) {
        if (ld.cx != e_abort) {
            mem_w16(mem, SD_CLI_ERR, ld.cx); /* cli_lnab */
        }
        return cli_rmpd(mem, cli_pd);
    }

    /* cli_gload */
    uint16_t cli_bpage = ld.bx;
    mem_w16(mem, SD_CLI_BPAGE, cli_bpage);

    /* copy cmdtail to the user's own DMA buffer */
    for (uint16_t i = 0; i < 127; i++) {
        mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_dma + 1 + i), mem[(uint16_t)(CLI_CMDTAIL + i)]);
    }

    /* count cmd length, uppercase it */
    uint16_t di = bpg_dma + 1;
    uint8_t cl = 0;
    for (;;) {
        uint8_t ch = mem_r8_seg(mem, cli_bpage, di);
        if (ch == 0) break;
        if (ch >= 'a' && ch <= 'z') {
            mem_w8_seg(mem, cli_bpage, di, (uint8_t)(ch & 0x5F));
        }
        di++;
        cl++;
    }
    mem_w8_seg(mem, cli_bpage, bpg_dma, cl);

    /* init default fcb -- the first 16 bytes (disk=0, name/type=' '
     * x11, 2 zero words) duplicated verbatim into the next 16, same
     * as the real asm's own "rep stosb/stosw" + "rep movsw" pair */
    mem_w8_seg(mem, cli_bpage, bpg_lddsk, mem[SD_CLI_LDDSK]);
    mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb0 + 0), 0);
    for (int i = 0; i < 11; i++) {
        mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb0 + 1 + i), ' ');
    }
    for (int i = 0; i < 4; i++) {
        mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb0 + 12 + i), 0);
    }
    for (int i = 0; i < 16; i++) {
        mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb0 + 16 + i),
                   mem_r8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb0 + i)));
    }

    /* if cmdtail, parse up to two filename arguments out of it --
     * cli_pcb's own pcb_flnmptr is still CLI_CMDTAIL from clicmd_entry's
     * own earlier cli_pfn call; shifttail already rewrote CLI_CMDTAIL's
     * own content down to just the args, so re-parsing from the same
     * pointer now parses the first argument, not the command name */
    if (mem[CLI_CMDTAIL] != 0) {
        cmd_bxcx pr = cli_pfn(mem, CLI_CMDTAIL);
        if (pr.bx != 0xFFFF) {
            /* copy fcb to user fcb front */
            mem_w16_seg(mem, cli_bpage, bpg_pw1ptr, mem_r16(mem, (uint16_t)(CLI_FCB + fcb_pptr)));
            mem_w8_seg(mem, cli_bpage, bpg_pw1len, mem[CLI_FCB + fcb_plen]);
            for (int i = 0; i < 16; i++) {
                mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb0 + i), mem[(uint16_t)(CLI_FCB + i)]);
            }

            /* if more cmdtail, parse a second argument */
            if (pr.bx != 0) {
                cmd_bxcx pr2 = cli_pfn(mem, (uint16_t)(pr.bx + 1));
                if (pr2.bx != 0xFFFF) {
                    mem_w16_seg(mem, cli_bpage, bpg_pw2ptr, mem_r16(mem, (uint16_t)(CLI_FCB + fcb_pptr)));
                    mem_w8_seg(mem, cli_bpage, bpg_pw2len, mem[CLI_FCB + fcb_plen]);
                    for (int i = 0; i < 16; i++) {
                        mem_w8_seg(mem, cli_bpage, (uint16_t)(bpg_fcb1 + i), mem[(uint16_t)(CLI_FCB + i)]);
                    }
                }
            }
        }
    }

    /* 10. Create the process -- f_setprior (chain mode only) is
     * omitted as a no-op, same as f_createproc right below, since
     * nothing downstream ever reads either one's result (see header
     * comment); f_createproc itself stays a no-op too */

    /* 11. Assign Console to new process */
    if (mem[SD_CLI_DFIL]) {
        crlf(mem);
    }
    mem[CLI_ACB + acb_cns] = mem[SD_CLI_CNS];
    mem_w16(mem, (uint16_t)(CLI_ACB + acb_pd), cli_pd);
    mem[CLI_ACB + acb_match] = 1;
    cmd_bxcx ca = conasn(mem);
    mem_w16(mem, SD_CLI_ERR, ca.cx);
    return clicmd_exit(mem);
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
 * cli_err. If chain mode set `SD_CLI_TERM`, the real asm's own `jmp
 * mpmif(f_terminate)` right after the dsk/user/err_mode/dma restores
 * is a true tail jump that never returns to this function's own
 * caller in the real system at all -- modeled as a fixed `(bx,cx)=
 * (0,0)` return instead (see this file's own header comment for why).
 * Otherwise, `pf_ctlc`'s own separate `f_terminate` call (an ordinary
 * call/return in the real asm, not a tail jump) stays a boundary
 * stub -- same "dispatch-adjacent" exclusion `f_createproc`/
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

    if (mem[SD_CLI_CHAIN] && mem[SD_CLI_TERM]) {
        uint16_t mask = (uint16_t)~(uint16_t)(pf_keep | pf_sys | pf_tempkeep | pf_ctlc);
        mem_w16(mem, (uint16_t)(rlr + p_flag), (uint16_t)(mem_r16(mem, (uint16_t)(rlr + p_flag)) & mask));
        cmd_bxcx result = {0, 0}; /* undefined artifact, fixed choice -- see header comment */
        return result;
    }

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
cmd_bxcx clicmd_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat,
                       uint16_t user_retf_offset, uint16_t cs) {
    uint16_t wrkseg = mem_r16(mem, UDA_WRKSEG);

    readq_entry(mem, MXCLIQPB); /* mxread */

    uint16_t rlr = mem_r16(mem, SD_RLR);
    uint16_t pflag = mem_r16(mem, (uint16_t)(rlr + p_flag));
    mem_w16(mem, SD_CLI_PFLAG, pflag);
    pflag = (uint16_t)((pflag & (uint16_t)~(uint16_t)pf_keep) | pf_tempkeep);
    mem_w16(mem, (uint16_t)(rlr + p_flag), pflag);

    int chain = (dx == 0);
    mem[SD_CLI_CHAIN] = chain ? 0xFF : 0; /* true/false -- SYSTEM.DEF */
    if (chain) {
        mem[SD_CLI_TERM] = 0;
    }

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

    /* cli_copy -- chain mode copies 128 bytes from the calling
     * process's own DMA buffer instead of the caller's own CLICB-
     * shaped buffer, and never touches cli_net (see header comment) */
    if (chain) {
        uint16_t dma_seg = mem_r16(mem, SD_CLI_DMA_SEG);
        uint16_t dma_ofst = mem_r16(mem, SD_CLI_DMA_OFST);
        for (uint16_t i = 0; i < 128; i++) {
            mem[CLI_CMDTAIL + i] = mem_r8_seg(mem, dma_seg, (uint16_t)(dma_ofst + i));
        }
    } else {
        mem[SD_CLI_NET] = mem_r8_seg(mem, wrkseg, (uint16_t)(dx + cmdcb_net));
        for (uint16_t i = 0; i < cmdcblen - cmdcb_cmd; i++) {
            mem[CLI_CMDTAIL + i] = mem_r8_seg(mem, wrkseg, (uint16_t)(dx + cmdcb_cmd + i));
        }
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
            return cli_qful(mem);
        }
        if (qerr == e_q_full) {
            mem_w16(mem, SD_CLI_ERR, qerr);
            return cli_qful(mem);
        }
        /* any other cli_checkque error falls to cli_fload, same as
         * the real asm's own cli_ffload check */
    }
    return cli_fload(mem, sysdat, user_retf_offset, cs);
}

/* chain_entry -- `sub dx,dx` then fall into clicmd_entry (dx==0 is
 * clicmd_entry's own chain-mode signal) */
cmd_bxcx chain_entry(uint8_t *mem, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    return clicmd_entry(mem, 0, sysdat, user_retf_offset, cs);
}
