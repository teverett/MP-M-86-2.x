/*
 * defs.h -- field offsets translated from the .DEF files under src/kernel/.
 *
 * /src-c is a routine-by-routine C port of /src's hand-written ASM86,
 * verified by differential testing (see docs/src-c.md), not a byte-for-byte
 * recompilation. Each offset here must match the value the real EQU chain
 * in the named .DEF file resolves to -- checked against
 * `python3 tools/asm86.py <file> --symbols` when a routine is ported, not
 * re-derived from scratch.
 *
 * Only fields a ported routine actually touches are listed; add more as
 * later files need them.
 */
#ifndef SRC_C_KERNEL_DEFS_H
#define SRC_C_KERNEL_DEFS_H

#include <stdint.h>

/*
 * get_xfcb_result -- src-c/kernel/file3.c's own `get_xfcb`/`get_xfcb0`
 * output (also used directly by src-c/kernel/file4.c's own `func15`):
 * al = the xfcb's own password mode (bits 5-7) with bit 0 set, found =
 * whether an xfcb was located at all (the real ZF), bx = .xfcb(extnum)
 * (`cmp_pw`'s own expected input).
 */
typedef struct {
    uint8_t al;
    uint8_t found;
    uint16_t bx;
} get_xfcb_result;

/* cio_bxcx -- the BX/CX pair most of CONSOLE.CIO/CHARIO.CIO's own
 * entries return (CIOIF.CIO's own `entry` dispatch just forwards
 * whatever's in them). Shared here (not a per-file local typedef)
 * so src-c/kernel/cioif.c's own `entry` dispatcher -- which calls
 * into both files -- can declare and return it without a type
 * mismatch once all three are linked into one real binary. */
typedef struct {
    uint16_t bx, cx;
} cio_bxcx;

/* chario.c's XIOS boundary hook: called with the asm's AL (io_*
 * function), CL and DL for every character-I/O call, returns the
 * XIOS's AX/BX. Unset, every call returns 0 (chario.c's header). */
typedef uint16_t (*cio_xios_fn)(uint8_t func, uint8_t cl, uint8_t dl);
void cio_set_xios(cio_xios_fn fn);

/* load.c's f_freadseq boundary: read the next sequential record of the
 * FCB at fcb_seg:fcb_off into dma_seg:dma_off; returns the BDOS's AL
 * (0 = ok, 1 = end of file, 0FFh = error). Unset, reads fail (0FFh). */
typedef uint8_t (*load_readseq_fn)(uint8_t *phys, uint16_t fcb_seg, uint16_t fcb_off,
                                   uint16_t dma_seg, uint16_t dma_off);
void load_set_readseq(load_readseq_fn fn);

/* command.c's BDOS boundary: f_fopen (15) / f_fclose (16) of the FCB at
 * fcb_seg:fcb_off, both getdiskmx functions (unported); returns the
 * BDOS's BX (aret). Unset, an open fails (00FFh) and a close returns 0. */
typedef uint16_t (*cli_bdos_fn)(uint8_t *phys, uint8_t func, uint16_t fcb_seg, uint16_t fcb_off);
void cli_set_bdos(cli_bdos_fn fn);

/*
 * get_dir_ext_result -- src-c/kernel/file2.c's own `get_dir_ext`
 * output (also used directly by src-c/kernel/file3.c's own `rseek`):
 * bx = .fcb(extnum), al = dir ext, dl = non-zero if no blocks were
 * found at all.
 */
typedef struct {
    uint16_t bx;
    uint8_t al;
    uint8_t dl;
} get_dir_ext_result;

/* src/kernel/SYSTEM.DEF */
#define pnamsiz 8  /* size of process/queue/file name fields */
#define qnamsiz pnamsiz /* src/kernel/SYSTEM.DEF: qnamsiz equ pnamsiz */
#define pdlen   0x30 /* size of one Process Descriptor (src/kernel/PD.DEF diagram) */

/* src/kernel/PD.DEF -- Process Descriptor field offsets (relative to a PD's
 * own address, same as the real EQU chain -- these never collide with
 * anything since the test harness places each PD at an address of its own
 * choosing and these are only ever used added to that base). */
#define p_link    0x00 /* word: System List link */
#define p_thread  0x02 /* word: Thread List link */
#define p_stat    0x04 /* byte: current activity (see ps_* below) */
#define p_prior   0x05 /* byte: priority */
#define p_flag    0x06 /* word: process state flags (see pf_* below) */
#define p_name    0x08 /* byte[pnamsiz]: process name */
#define p_uda     0x10 /* word: segment address of this PD's UDA */
/* src/kernel/UDA.FMT */
#define u_wrkseg  0x2E /* word: current segment addr of buf */
#define u_retseg  0x30 /* word: usr ES return */
/* src/kernel/RSP.DEF */
#define rsp_pd    0x10 /* byte: offset of the Process Descriptor within an RSP's own data segment */
#define rsp_link  0x00 /* word, es-relative: next RSP's own segment address (linked list) */
/* src/kernel/SYSTEM.DEF */
#define mpmint    224  /* int vec for mpm ent. */
/* src/kernel/MODFUNC.DEF */
#define bdosmod_bit 0x08
#define ciomod_bit  0x10
#define xiosmod_bit 0x20
/* src/kernel/MODTAB.DEF */
#define mod_init  4 /* dword offset of a module table entry's own "initialize" far pointer */
/* src/kernel/XIOSCB.DEF */
#define io_maxconsole 20
#define io_maxlist    21
#define p_mem     0x16 /* word: head of this PD's own MD list */
#define p_scratch 0x2E /* byte: scratch word (low byte) */
#define p_dsk     0x12 /* byte: current default disk */
#define p_user    0x13 /* byte: current default user number */
#define p_ldsk    0x14 /* byte: disk program loaded from */
#define p_luser   0x15 /* byte: user number loaded from */
#define p_cns     0x20 /* byte: default console */
#define p_lst     0x24 /* byte: default list device */
#define p_wait    0x1A /* word: ticks remaining before delay expires */
#define p_parent  0x1E /* word: process that created this process */

#define pf_sys      0x0001 /* system process */
#define pf_keep     0x0002 /* do not terminate */
#define pf_table    0x0010 /* from pd table */
#define pf_resource 0x0020 /* waiting for resource */
#define pf_raw      0x0040 /* raw console i/o */
#define pf_ctlc     0x0080 /* abort pending */
#define pf_tempkeep 0x0200 /* don't terminate yet... */
#define pf_ctld     0x0400 /* explicit detach occurred */
#define pf_childabort 0x0800 /* child process aborted abnormally */

#define ps_run      0x00
#define ps_poll     0x01
#define ps_delay    0x02
#define ps_swap     0x03
#define ps_term     0x04
#define ps_sleep    0x05
#define ps_dq       0x06
#define ps_nq       0x07
#define ps_flagwait 0x08
#define ps_ciowait  0x09

/* src/kernel/FLG.DEF -- one Flag Table entry */
#define flg_pd     0x00 /* word */
#define flg_ignore 0x02 /* byte */
#define flglen     3
#define flag_off   0xFFFF
#define flag_on    0xFFFE
#define flag_zig   0xFF

/* src/kernel/ERR.DEF */
#define e_not_implemented 0x01
#define e_bad_entry        0x02
#define e_ill_flag         0x04
#define e_flag_ovrrun      0x05
#define e_flag_underrun    0x06
#define e_illdisk          0x17
#define e_badfname         0x18
#define e_badftype         0x19
#define e_ill_cns          0x13
#define e_ill_passwd       0x26
#define e_ill_lst          0x25
#define e_no_pdname        0x14
#define e_no_mimic         0x27
#define e_not_owner        0x20
#define e_no_attach        0x24
#define e_no_memory        0x03
#define e_active_pd        0x22
#define e_no_umd           0x12
#define e_no_pd            0x0C
#define e_no_cseg          33 /* no CSEG in load file */
#define e_pd_noterm        35 /* could not terminate process */
#define e_bad_load         28 /* bad ret. from BDOS load */
#define e_bad_read         29 /* bad ret. from BDOS read */
#define e_bad_open         30 /* bad ret. from BDOS open */
#define e_nullcmd          31 /* null command */
#define e_abort            40 /* external termination occurred */

/* src/kernel/XIOSCB.DEF */
#define io_idle 0x17

/* src/kernel/INIT.SUP's own `sysdat`/`supervisor` cseg cells -- the
 * module's own real, already-resolved cseg offsets, same
 * reasoning as the BD_* block in this file: INIT.SUP itself declares
 * them at its own cseg's very start, right after its own 2 `jmp`s
 * (`jmp init ! jmp entry`, 3 bytes each = 6), so `sysdat`=6 and
 * `supervisor`=8 (a 4-byte far pointer; its own segment half, the one
 * `init` actually writes, is 8+2=0Ah) are stable regardless of what
 * real module comes after INIT.SUP in the final SUP.A86 build --
 * confirmed via `--symbols` against tests/src-c/fixtures/
 * init_wrapper.A86, not assumed. */
#define INIT_SYSDAT          0x0006 /* word: INIT.SUP's own saved sysdat segment (cs:sysdat) */
#define INIT_SUPERVISOR_SEG  0x000A /* word: INIT.SUP's own `supervisor` far-ptr segment half */

/* src/kernel/CIOIF.CIO/MEMIF.MEM own the exact same cseg header shape
 * as INIT.SUP (`jmp init ! jmp entry ! sysdat dw 0 ! supervisor equ
 * (offset $) ! rw 2`), so `sysdat`=6/`supervisor`=8 land at the same
 * offsets -- confirmed directly against both real files, not just
 * assumed from the shared shape. Unlike INIT.SUP's own `init` (whose
 * own `supervisor` offset half is a fixed compile-time constant, its
 * own `entry` label -- see init.c's own header comment), these two
 * modules' own `init` copies *both* halves of `supervisor` from
 * SYSDAT's own module_table[SUP] entry (SYSDAT_SUPMOD),
 * since their own `supervisor` has to point at a genuinely different
 * module's own `entry` label, not a fixed offset within itself. */
#define CIOIF_SYSDAT          0x0006 /* word: CIOIF.CIO's own saved sysdat segment (cs:sysdat) */
#define CIOIF_SUPERVISOR_OFF  0x0008 /* word: CIOIF.CIO's own `supervisor` far-ptr offset half */
#define CIOIF_SUPERVISOR_SEG  0x000A /* word: CIOIF.CIO's own `supervisor` far-ptr segment half */
#define MEMIF_SYSDAT          0x0006 /* word: MEMIF.MEM's own saved sysdat segment (cs:sysdat) */
#define MEMIF_SUPERVISOR_OFF  0x0008 /* word: MEMIF.MEM's own `supervisor` far-ptr offset half */
#define MEMIF_SUPERVISOR_SEG  0x000A /* word: MEMIF.MEM's own `supervisor` far-ptr segment half */
#define RTMIF_SYSDAT          0x0006 /* word: RTMIF.RTM's own saved sysdat segment (cs:sysdat) */
#define RTMIF_SUPERVISOR_OFF  0x0008 /* word: RTMIF.RTM's own `supervisor` far-ptr offset half */
#define RTMIF_SUPERVISOR_SEG  0x000A /* word: RTMIF.RTM's own `supervisor` far-ptr segment half */
#define BDOSIF_SYSDAT          0x0006 /* word: BDOSIF.BDO's own saved sysdat segment (cs:sysdat) */
#define BDOSIF_SUPERVISOR_OFF  0x0008 /* word: BDOSIF.BDO's own `supervisor` far-ptr offset half */
#define BDOSIF_SUPERVISOR_SEG  0x000A /* word: BDOSIF.BDO's own `supervisor` far-ptr segment half */

/* src/kernel/SYSDAT.FMT -- real SYSDAT offsets (the SYSDAT segment is
 * every module's DS). `dispatcher`/`rtm_pdisp` are 4-byte far pointers,
 * set by RTMIF.RTM's own `init` to DSPTCH.RTM's own `fdisp`/`farpdisp`;
 * `idlepd`/`tickpd` are real PDs, `pdlen` bytes each. */
#define SYSDAT_SUPMOD     0x0000 /* word[2]: SYSDAT.FMT's own module_table[SUP] entry far ptr */
#define SYSDAT_DISPATCHER 0x0038 /* word[2]: far ptr to DSPTCH.RTM's own fdisp */
#define SYSDAT_RTM_PDISP  0x003C /* word[2]: far ptr to DSPTCH.RTM's own farpdisp */
#define SYSDAT_IDLEPD     0x09F0 /* byte[pdlen]: IDLE.SUP's own PD */
#define SYSDAT_TICKPD     0x0A20 /* byte[pdlen]: TICK.SUP's own PD */
#define SYSDAT_INITUDA    0x06C0 /* byte[ulen]: INIT.SUP's own init process's
    UDA (probed directly, same file as SYSDAT_SUPMOD/etc above) -- doubles as
    COMMAND.SUP's own chain-mode scratch UDA (`cli_ld1`'s own `offset inituda`
    relocation, `command.c`'s own `cli_gopen`) */
/* More real SYSDAT.FMT offsets, the fields INIT.SUP's own `init`
 * touches (init.c reads/writes them through real memory, see
 * far_ptr() below). tests/src-c/test_init.py checks each against the
 * real assembled SYSDAT.FMT symbol, not a hand-copied value. */
#define SYSDAT_RSPSEG     0x0042 /* word: segment of first RSP (linked list head) */
#define SYSDAT_VERSION    0x0078 /* word: addr. version str in SUP code segment */
#define SYSDAT_NCONDEV    0x0083 /* byte: # XIOS consoles */
#define SYSDAT_NLSTDEV    0x0084 /* byte: # XIOS list devices */
#define SYSDAT_NCNS       0x0047 /* byte: # system consoles */
#define SYSDAT_TOD        0x007E /* tod_day word + tod_hr/min/sec bytes */
#define SYSDAT_VERNUM     0x007A /* word: MP/M-86 w/BDOS version */
#define SYSDAT_MPMVERNUM  0x007C /* word: MP/M-86 version */
#define SYSDAT_SYSENT     0x00A0 /* the system-entry (function -> module) table */
#define SYSDAT_SRCHDISK   0x004B /* byte: default search disk */
#define SYSDAT_DAYFILE    0x004F /* byte: 0xFF -- display command info */
/* LOAD.SUP's variables (SYSDAT.FMT) */
#define SYSDAT_MXLOADQPB      0x03BC /* the MXLoad queue's QPB (pre-opened) */
#define SYSDAT_MXCLIQPB       0x03E8 /* the MXCli queue's QPB (pre-opened) */
#define SYSDAT_LOAD_DMA       0x0424 /* byte[128]: load's record buffer */
#define SYSDAT_LOAD_UDA       0x04A4 /* word */
#define SYSDAT_LOAD_LSTK      0x04A6 /* word */
#define SYSDAT_LOAD_BASEP     0x04A8 /* word */
#define SYSDAT_LOAD_8080      0x04AA /* byte */
#define SYSDAT_LOAD_NRELSEGS  0x04AB /* byte */
#define SYSDAT_LOAD_NLDTABENTS 0x04AC /* word */
#define SYSDAT_LOAD_PD        0x04AE /* word: pd of load prog */
#define SYSDAT_LOAD_FCB       0x04B0 /* word: addr of open FCB in u_wrkseg */
#define SYSDAT_LOAD_MAXWANTED 0x04B2 /* word */
#define SYSDAT_LOAD_MINWANTED 0x04B4 /* word */
#define SYSDAT_LOAD_INDMA     0x04B6 /* word */
#define SYSDAT_LOAD_CHAIN     0x04B8 /* byte: true (0FFh) on a chain load */
#define SYSDAT_LDTAB          0x04B9 /* ldtab[], ldtablen bytes per entry */
#define ldtabsiz              0x00AA /* LDTAB.DEF: 10 entries */
/* src/kernel/XIOSCB.DEF's character I/O function numbers (xiosif's AL) */
#define io_const   0
#define io_conin   1
#define io_conout  2
#define io_list    3
#define SYSDAT_NCIODEV    0x0085 /* byte: # character i/o devices */
#define SYSDAT_INITPD     0x09C0 /* byte[pdlen]: INIT.SUP's own init process's PD */
#define SYSDAT_IDLE_TOS   0x0A7A /* word: IDLE.SUP's own initial top-of-stack (its entry) */
#define SYSDAT_TICK_TOS   0x0AAA /* word: TICK.SUP's own initial top-of-stack (its entry) */
#define SYSDAT_INTFLAG    0x0B5F /* byte: if 0, interrupts not enabled */
#define INIT_VER          0x0037 /* INIT.SUP's own `ver` string, offset in the SUP code segment */
/* Real SYSDAT.FMT offsets for the process lists and dispatcher state
 * proc.c/dsptch.c/sysent.c use on real memory (from RTM.A86's
 * --symbols). PD addresses on these lists are offsets within the
 * SYSDAT segment, since the real asm reaches every PD DS-relative. */
#define SYSDAT_ENDSEG     0x0044 /* word: 1st paragraph outside MP/M */
#define SYSDAT_NFLAGS     0x004A /* byte: number of system flags */
#define SYSDAT_FLAGS      0x0056 /* word: address of the Flag Table (in SYSDAT) */
#define SYSDAT_PUL        0x005C /* word: Process Descriptor Unused List */
#define SYSDAT_RLR        0x0068 /* word: Ready List Root -- running process's PD */
#define SYSDAT_DLR        0x006A /* word: Delay List Root */
#define SYSDAT_DRL        0x006C /* word: Dispatcher Ready List */
#define SYSDAT_PLR        0x006E /* word: Poll List Root */
#define SYSDAT_SLR        0x0070 /* word: Swap List Root */
#define SYSDAT_THRDRT     0x0072 /* word: Process Thread Root */
#define SYSDAT_INDISP     0x0B5E /* byte: currently in dispatch? */
#define SYSDAT_MXMEMOWNER 0x0B64 /* word: PD owning the MX memory queue mutex */
#define SYSDAT_MXMEMCOUNT 0x0B66 /* byte: mutex nesting count */
#define SYSDAT_CCB        0x0054 /* word: address of the CCB table (in SYSDAT) */
#define SYSDAT_QUL        0x005E /* word: root of the unused-QD free list */
#define SYSDAT_QMAU       0x0060 /* word[]: MAU queue buffers allocate from */
#define SYSDAT_QLR        0x0074 /* word: root of the active Queue Descriptor list */
#define SYSDAT_MDUL       0x0058 /* word: root of the unused MD free list */
#define SYSDAT_MFL        0x005A /* word: root of the Memory Free List */
#define SYSDAT_MAL        0x0076 /* word: root of the active MAU list */
/* Fixed spots in the running process's UDA stack area for the parameter
 * blocks the MEM asm builds on its own stack and hands to `mpmif` with
 * DS = SS. During a system call SS is the running UDA and SP starts at
 * ulen (SUPIF.SUP's user_entry: `mov ss,es ! mov sp,ulen`), so the real
 * blocks live in the UDA's stack area too (above its last field, u_stat_sav
 * at 61h); the C port has no 8086 stack, so it uses these fixed offsets,
 * kept apart because free_memory's MFPB is still live during mfree_it. */
#define UDA_STK_MAF       0x0090 /* word[3]: memory.c mfree_it's MAF.DEF block */
#define UDA_STK_MFPB      0x0098 /* word[2]: cpmmem.c free_memory's MFPB.DEF block */
#define UDA_STK_MPB       0x00A0 /* word[5]: cpmmem.c getmemory's MPB.DEF block */
#define UDA_STK_LOAD_MCB  0x00B0 /* byte[5]: load.c ld_freeit's MCB (f_freemem) */
#define UDA_STK_LOAD_MFPB 0x00B8 /* word[2]: load.c's MFPB (f_memfree) */
/* src/kernel/DATA.BDO's mxdiskqd/mxdiskqpb, BDOS module data in the
 * combined SYSDAT+DATA.BDO data segment (BDOS.A86's --symbols) */
#define SYSDAT_MXDISKQD   0x0F92 /* byte[qdlen]: the MX disk queue's QD */
#define SYSDAT_MXDISKQPB  0x0FAE /* byte[qpblen]: its QPB */

/* src/kernel/CHAR.DEF */
#define ctlc   0x03
#define ctld   0x04
#define ctle   0x05
#define bell   0x07
#define ctlh   0x08
#define tab 0x09
#define ctlp   0x10
#define ctlq   0x11
#define ctlr   0x12
#define ctls   0x13
#define ctlu   0x15
#define ctlx   0x18
#define ctl    0x5E
#define rubout 0x7F
#define cr  0x0D
#define lf  0x0A

/* src/kernel/PCB.DEF -- Parse Control Block (parse_entry's own input) */
#define pcb_flnmptr 0x00 /* word: offset of string to parse */
#define pcb_fcbptr  0x02 /* word: offset of FCB in u_wrkseg */

/* src/kernel/FCB.DEF -- File Control Block. Only the fields PARSE.SUP
 * touches are listed. */
#define fcb_dr     0x00 /* byte: drive, 0=default, 1=A, 2=B, ... */
#define fcb_name   0x01 /* byte[fnamsiz] */
#define fcb_type   0x09 /* byte[ftypsiz] */
#define fcb_passwd 0x10 /* byte[8] */
#define fcb_plen   0x1A /* byte: password length */
#define fcb_extnum 0x0C /* byte: extent number field */
#define fcb_ubytes 0x0D /* byte: unfilled bytes field */
#define fcb_modnum 0x0E /* byte: data module number (high bit = fwf) */
#define fcb_reccnt 0x0F /* byte: record count field */
#define fcb_dskmap 0x10 /* byte: disk map field */
#define fcb_nxtrec 0x20 /* byte: next record field (= fcblen) */
#define fcblen 0x20 /* size of one FCB (src/kernel/EQU.BDO) */
#define namlen 15 /* name+type length, for a SEARCH's own CL (src/kernel/EQU.BDO) */
#define fcb_ranrec 0x21 /* word: random record field */
#define fwfmsk     0x80 /* bit in fcb_modnum: file write flag */
/* src/kernel/EQU.BDO: rofile equ 9 -- same offset as fcb_type above;
 * the read-only attribute bit lives in the high bit of fcb_type[0] */

/* src/kernel/QD.DEF -- Queue Descriptor */
#define q_link    0x00 /* word */
#define q_flags   0x04 /* word */
#define q_name    0x06 /* byte[qnamsiz] */
#define q_msglen  0x0E /* word */
#define q_nmsgs   0x10 /* word */
#define q_dq      0x12 /* word */
#define q_nq      0x14 /* word */
#define q_msgcnt  0x16 /* word */
#define q_msgout  0x18 /* word */
#define q_buf     0x1A /* word */
#define qdlen     0x1C

#define qf_mx    0x01 /* Mutual Exclusion */
#define qf_table 0x10 /* from the qd table (vs. caller-supplied) */

/* src/kernel/QD.DEF -- Queue Parameter Block */
#define qpb_qaddr   0x02 /* word: address of QD */
#define qpb_nmsgs   0x04 /* word: number of messages to read/write */
#define qpb_buffptr 0x06 /* word */
#define qpb_name    0x08 /* byte[qnamsiz] */

/* src/kernel/MPB.DEF -- Memory Parameter Block (only the field
 * qspace/qrelease touch) */
#define mpb_start 0x00 /* word */

/* src/kernel/ERR.DEF */
#define e_no_qd        0x07
#define e_q_empty      0x0E
#define e_q_full       0x0F
#define e_no_queue     0x09
#define e_q_inuse      0x0A
#define e_q_notactive  0x0B

/* src/kernel/MODFUNC.DEF -- mpmif function codes QUE1.RTM/QUE2.RTM use.
 * (user*0x100)+140, (rtm*0x100)+18/19, (mem*0x100)+9/10 resolved via
 * `python3 tools/asm86.py tests/src-c/fixtures/que_wrapper.A86 -I
 * src/kernel --symbols`.) */
#define f_cqwrite    0x8C
#define f_maualloc   0x309
#define f_maufree    0x30A
#define f_mlalloc    0x30B
#define f_mlfree     0x30C
#define f_findpdname 0x214
#define f_share      0x308

/* src/kernel/CCB.DEF -- Console Control Block */
#define c_attach  0x00 /* word */
#define c_queue   0x02 /* word */
#define c_flag    0x04 /* byte: run-time flags (see cf_* below) */
#define c_strtcol 0x05 /* byte: starting column, line editing */
#define c_column  0x06 /* byte: current column, line editing */
#define c_nchar   0x07 /* byte: 1-char read-ahead for CTRL chars */
#define c_mimic   0x08 /* byte */
#define c_msource 0x09 /* byte */
#define ccblen    0x0C

#define cf_listcp 0x01 /* control P toggle */
#define cf_compc  0x02 /* suppress output */

/* src/kernel/ACB.DEF -- Assign Console Control Block */
#define acb_cns   0x00 /* byte */
#define acb_match 0x01 /* byte */
#define acb_pd    0x02 /* word */
#define acb_name  0x04 /* byte[pnamsiz] */

/* src/kernel/MPB.DEF -- the rest of the fields src-c/kernel/que2.c's
 * own mpb_start (0x00) didn't already need */
#define mpb_min   0x02 /* word */
#define mpb_max   0x04 /* word */
#define mpb_pdadr 0x06 /* word */
#define mpb_flags 0x08 /* word */

/* src/kernel/MPB.DEF -- Memory Free Parameter Block */
#define mfpb_start 0x00 /* word */
#define mfpb_pd    0x02 /* word */

/* src/kernel/MEM.DEF -- one Memory Descriptor, under two different names
 * depending which list it's on. m_* while on the MFL/MAL (free MAUs);
 * ms_* while linked onto a PD's own p_mem list (an allocated memory
 * segment) -- same 5-word layout either way, just an aliased 4th/5th
 * field (ms_flags/ms_mau are m_plist/m_unused under another name). */
#define m_link    0x00 /* word */
#define m_start   0x02 /* word */
#define m_length  0x04 /* word */
#define m_plist   0x06 /* word */
#define m_unused  0x08 /* word */
#define mdlen     0x0A
/* src/kernel/MEM.DEF -- Share Parameter Block (share_entry's own input,
 * reached through u_wrkseg same as every other *PB above) */
#define spb_opd   0x00 /* word: Owner PD */
#define spb_rpd   0x02 /* word: Requestor PD */
#define spb_start 0x04 /* word: start paragraph */
#define spblen    0x06

#define ms_link   0x00 /* word */
#define ms_start  0x02 /* word */
#define ms_length 0x04 /* word */
#define ms_flags  0x06 /* word (same bits as mpb_flags) */
#define ms_mau    0x08 /* word */

/* src/kernel/CPMMEM.MEM's own Memory Control Block (MCB), the CP/M-86
 * compatibility counterpart to MPB.DEF's MPB above */
#define mcb_base   0x00 /* word */
#define mcb_length 0x02 /* word */
#define mcb_ext    0x04 /* byte */
#define mcblen     0x05

/* src/kernel/MPB.DEF */
#define mf_load 0x01 /* bit in ms_flags/mpb_flags: loaded from a disk file */
#define mf_share 0x02 /* bit in mpb_flags: share an existing allocation */
#define mf_code  0x04 /* bit in mpb_flags/ms_flags: a Code segment */

/* src/kernel/CMDH.DEF -- Command Header Entry Format, src-c/kernel/
 * load.c's own get_ch_info */
#define ch_form   0x00 /* byte */
#define ch_length 0x01 /* word */
#define ch_base   0x03 /* word */
#define ch_min    0x05 /* word */
#define ch_max    0x07 /* word */
#define chlen     0x09
#define ch_entmax 8

/* src/kernel/MPB.DEF -- Memory Alloc Free parameter block (maufree_entry's
 * own input, reached through u_wrkseg same as every other *PB above) */
#define maf_mau   0x00 /* word: MD address of the MAU to free from */
#define maf_sat   0x02 /* word: the SAT entry's own sat_start value */
#define maf_start 0x04 /* word: start of the region actually being freed */
#define maflen    0x06

/* src/kernel/MODFUNC.DEF -- mpmif function codes CPMMEM.MEM's own
 * getmemory/free_memory use ((user*0x100)+128/130, user=0) */
#define f_malloc    0x80
#define f_memfree   0x82
#define f_terminate 0x8F

/* src/kernel/DATA.BDO -- the subset of BDOS-wide scratch fields
 * src-c/kernel/file1.c's own ported routines touch. Unlike every
 * other scratch constant in this file, these are NOT independently
 * chosen addresses -- tests/src-c/fixtures/file1_wrapper.A86 includes
 * the real DATA.BDO (along with FILE1-4.BDO/BDOSIF.BDO, the same
 * bundle src/kernel/BDOS.A86 itself assembles, since FILE1.BDO isn't
 * self-contained -- see file1.c's own header comment), so these are
 * DATA.BDO's own real resulting addresses, checked against `python3
 * tools/asm86.py tests/src-c/fixtures/file1_wrapper.A86 -I src/kernel
 * --symbols`. `PD_CNT`/`HIGH_EXT`/`XFCB_READ_ONLY`/`CURDSK` land as 4
 * contiguous bytes (0xC33-0xC36), matching CHKSUM_FCB's own 4-byte
 * checksum read -- confirmed, not just assumed. */
#define BD_RESEL       0x0C0E /* byte: reselection flag */
#define BD_ARET        0x0C0F /* word: return value; low byte = BD_LRET */
#define BD_LRET        0x0C0F /* byte: low(BD_ARET) */
#define BD_COMP_FCB_CKS 0x0C11 /* byte: compute-fcb-checksum flag */
#define BD_SELDSK      0x0C16 /* byte: selected disk number */
#define BD_INFO        0x0C17 /* word: address of the current FCB */
#define BD_DMAAD       0x0C1B /* word: dma offset */
#define BD_DMABASE     0x0C1D /* word: dma base */
#define BD_FX          0x0C1F /* byte: BDOS function # (for error msgs) */
#define BD_DCNT        0x0C25 /* word: directory counter (low byte = ldcnt) */
#define BD_DBLK        0x0C27 /* word: directory block */
#define BD_ERROR_MODE  0x0C29 /* byte */
#define BD_PD_CNT      0x0C33 /* byte: BDOS process count */
#define BD_CHECK_FCB_RET 0x0C75 /* byte: see check_fcb1's own header comment in file1.c */
#define BD_PDADDR      0x0C3A /* word: PD address target for the current call */
#define BD_HIGH_EXT    0x0C34 /* byte: FCB high extent bits */
#define BD_XFCB_READ_ONLY 0x0C35 /* byte: xfcb read-only flag, set by reselect */
#define BD_CURDSK      0x0C36 /* byte: current disk */
#define BD_CDRMAXA     0x0C3C /* word: pointer to current dir max value */
#define BD_CHECKA      0x0C44 /* word: current checksum vector addr */
#define BD_CHKSIZ      0x0C53 /* word: size of checksum vector */
#define BD_DRVLBLA     0x0C3E /* word: drive label data byte address */
#define BD_BUFFA       0x0C40 /* word: pointer to directory dma address */
#define BD_DPBADDR     0x0C42 /* word: current disk parameter block address */
#define BD_ALLOCA      0x0C46 /* word: current allocation vector address */
#define BD_SECTPT      0x0C48 /* word: sectors per track */
#define BD_BLKSHF      0x0C4A /* byte: block shift factor */
#define BD_BLKMSK      0x0C4B /* byte: block mask */
#define BD_EXTMSK      0x0C4C /* byte: extent mask */
#define BD_MAXALL      0x0C4D /* word: maximum allocation number */
#define BD_DIRMAX      0x0C4F /* word: largest directory number */
#define BD_OFFSETV     0x0C55 /* word: offset tracks at beginning */
#define BD_TRANV       0x0C94 /* word: address of translate vector */
#define BD_WFLAG       0x0C98 /* byte: XIOS/BIOS write flag */
#define BD_SEQIO       0x0C9A /* byte: 1 if sequential i/o */
#define BD_SINGLE      0x0C9F /* byte: true if single-byte alloc map */
#define BD_RCOUNT      0x0CA1 /* byte: record count in current fcb */
#define BD_EXTVAL      0x0CA2 /* byte: extent number and extmsk */
#define BD_VRECORD     0x0CA3 /* byte: current virtual record */
#define BD_ARECORD     0x0CA4 /* word: current actual record, low word */
#define BD_ARECORD_HI  0x0CA6 /* byte: current actual record, high byte */
#define BD_ABLOCK      0x0CA7 /* word: current actual block# * blkmsk */
#define BD_DPTR        0x0CA9 /* byte: directory pointer 0,1,2,3 */
#define BD_PARAMETERSEGMENT 0x0CB9 /* word: user parameter segment */
#define BD_RODSK       0x0C01 /* word: read-only disk vector */
#define BD_RETURN_FFFF 0x0C69 /* byte: select-error flag for functions 27 & 31 */
#define BD_REM_DRV     0x0C0B /* byte: removable-drive flag (no observable
                                  effect on any routine file1.c ports --
                                  only ever OR'd into a boundary-stub's
                                  own input, see wrbuff's header comment) */

/* src/kernel/DATA.BDO -- the subset of BDOS-wide scratch fields
 * src-c/kernel/file4.c's own ported routines touch, at their own real
 * resulting addresses (same reasoning/method as the BD_* block above) */
#define BD_DLOG        0x0C03 /* word: disk login vector */
#define BD_RLOG        0x0C05 /* word: removable-drive login vector */
#define BD_TLOG        0x0C07 /* word: "just logged in this call" vector */
#define BD_DIRBLK      0x0C51 /* word: reserved directory block count */
#define BD_RETURNSEG   0x0CBB /* word: user return segment (func27/31/52) */

/* src/kernel/DATA.BDO -- the subset of BDOS-wide scratch fields
 * src-c/kernel/file3.c's own ported routines touch, at their own real
 * resulting addresses (same reasoning/method as the BD_* block above --
 * checked against the same wrapper, tests/src-c/fixtures/
 * file1_wrapper.A86, since it's the same combined bundle). PACKED_DCNT
 * (3 bytes) lands immediately after BD_CURDSK, and PDADDR immediately
 * after that -- confirmed, not just assumed -- matching search_listo's
 * own "compare ch bytes starting right after curdsk" trick. */
#define BD_SDCNT       0x0C7D /* word: saved dcnt of file's 1st fcb */
#define BD_SDBLK       0x0C7F /* word: saved dblk of file's 1st fcb */
#define BD_PACKED_DCNT 0x0C37 /* byte[3]: packed dblk(15 bits)+dcnt(9 bits) */
#define BD_FLUSHED     0x0C8D /* byte: lock-list open-file flush flag */
#define BD_OPEN_ROOT   0x0C90 /* word: open-file list root */
#define BD_LOCK_ROOT   0x0C92 /* word: locked-record list root */
#define BD_CUR_POS     0x0C79 /* word: current position in olist/llist */
#define BD_PRV_POS     0x0C7B /* word: previous position in olist/llist */

/* src/kernel/DATA.BDO -- the subset of BDOS-wide scratch fields
 * src-c/kernel/file2.c's own `srch`/`SEARCH`/`SEARCHN` port touches,
 * at their own real resulting addresses (same reasoning/method as the
 * BD_* block above, checked against the same `file1_wrapper.A86`
 * bundle) */
#define BD_SEARCH_USER0   0x0C12 /* byte */
#define BD_FIND_XFCB      0x0C14 /* byte */
#define BD_SRCHA          0x0C19 /* word: search address (start of FCB being searched for) */
#define BD_SRCHL          0x0C20 /* byte: search length */
#define BD_FCB_EXISTS     0x0C6B /* byte */
#define BD_SAVE_XFCB      0x0C6D /* byte */
#define BD_ACTUAL_RC      0x0C6C /* byte: SET_RC's own output, read by RESTORE_RC */
#define BD_EFCB           0x0C00 /* byte: an empty (all-0xE5) fcb template, for MAKE's own "search for free space" pass */
#define BD_DF_PASSWORD    0x0C2B /* byte[8]: the system-wide default password */
#define BD_XDMAAD         0x0C67 /* word: the xfcb's own dma address (user-supplied password buffer) */
#define BD_MAKE_XFCB      0x0C13 /* byte: true if MAKE is creating an extended fcb */
#define BD_SAVE_EXT       0x0C6F /* byte: CLOSE1's own saved fcb(ext) value */
#define BD_DONT_CLOSE     0x0C85 /* byte: skip CLOSE_FCB's own final rewrite if true */
#define BD_ATTRIBUTES     0x0C70 /* byte */
#define BD_SDCNT0         0x0C81 /* word */
#define BD_SDBLK0         0x0C83 /* word */
#define BD_DIRLOC         0x0C99 /* byte: directory flag in rename, etc. */
#define BD_XDCNT          0x0CAA /* word */
#define BD_XDBLK          0x0CAC /* word */
#define BD_USER_ZERO_PASS 0x0CAE /* byte */
#define BD_SAVE_MOD       0x0C6E /* byte: openreel's own saved module# */
#define BD_RMF            0x0C97 /* byte: openreel's own "read mode" flag */
#define BD_FILE_ID        0x0C88 /* word: func16's own saved olist item addr */
#define BD_SRCHABASE      0x0C23 /* word: func17's own saved user parameter segment */
#define BD_SRCHAOFST      0x0C21 /* word: func17's own saved user fcb offset */
#define BD_DMINX          0x0C9C /* byte: diskwrite's own saved disk-map index */
#define BD_LOCK_SP        0x0C72 /* word: LOCK's own saved real SP -- never read anywhere in src/, see lock_'s own header comment */
#define BD_LOCK_SHELL     0x0C74 /* byte: LOCK's own shell flag -- never read anywhere in src/, see lock_'s own header comment */
#define BD_LOCK_UNLOCK    0x0C76 /* byte: true=lock, false=unlock (func42/func43's own input) */
#define BD_SHELL_RR       0x0CB4 /* byte[3]: BDOSIF.BDO's own SAVE_RR/RESET_RR scratch for fcb_ranrec */
#define BD_NTLOG          0x0C09 /* word: diskreset's own "newly logged in" accumulator */
#define BD_SET_RO_FLAG    0x0C8B /* byte: diskreset's own forced-read-only flag */
#define BD_CHECK_DISK     0x0C8C /* byte: diskreset's own per-disk removable/ro flag */
#define BD_DENIEDDRV      0x0DF0 /* byte: diskreset's own denied-process error message drive letter */
#define BD_DENIEDCNS      0x0DFB /* byte: diskreset's own denied-process error message console # */
#define BD_DENIEDPRC      0x0E05 /* byte[8]: diskreset's own denied-process error message process name */

/* src/kernel/DATA.BDO -- the subset of BDOS-wide scratch fields
 * src-c/kernel/file3.c's own `reselect`/`parsave`/`COUNT_OPENS`/
 * `CREATE_OLIST_ITEM` and src-c/kernel/file2.c's own `open`/`OPEN1`/
 * `opencopy` port touches, at their own real resulting addresses
 * (same reasoning/method as the BD_* blocks above, checked against
 * the same `file1_wrapper.A86`/`file3_wrapper.A86` bundles) */
#define BD_FCBDSK      0x0C0C /* byte: OPEN's own saved disk# for the fcb being opened */
#define BD_PARCOPFL    0x0C0D /* byte: parsave's own "copy succeeded" flag */
#define BD_USRCODE     0x0C15 /* byte: user code of the process making this call */
#define BD_MULT_CNT    0x0C2A /* byte: real local copy of u_mult_cnt (see UDA_MULT_CNT) */
#define BD_OPEN_CNT    0x0C86 /* byte: COUNT_OPENS' own running open-file count */
#define BD_LOCK_CNT    0x0C87 /* byte: COUNT_OPENS' own running locked-record count */
#define BD_MAKE_FLAG   0x0C6A /* byte: true if the current open is really a MAKE */
#define BD_LINFO       0x0C9B /* byte: reselect's own saved disk# (from SET_SELDSK) */
#define BD_OLDDSK      0x0CA0 /* byte: reselect's own previously-selected disk# */
#define BD_SHELL_FLAG  0x0CB3 /* byte: parsave's own "command shell" flag */
#define BD_INFOSAVE    0x0CB7 /* word: parsave's own saved BD_INFO across the copy */
#define BD_PARLG       0x0CBD /* byte: parsave's own copy length */
#define BD_COMMON_DMA  0x0C57 /* byte: parsave33's own "use the common dma area" flag */
#define BD_LOC_PAR_AREA 0x0E0E /* byte: parsave's own local (ds-relative) parameter-area scratch byte */

/* src/kernel/SYSDAT.FMT -- free_root/open_max, system-wide constants
 * src-c/kernel/file3.c's own olist/llist and COUNT_OPENS code touches
 * (free_root: delete_item/create_item; open_max: COUNT_OPENS' own
 * limit check). Unlike every BD_* constant above (FILE1-4.BDO's own
 * scratch area, which resolves to 0xC00+ in the combined module),
 * these are real SYSDAT.FMT fields -- confirmed via `--symbols`
 * against the same wrapper: they resolve to 0x52/0x8C, not BD_*-range
 * addresses. See file3.c's own header comment for why this is safe to
 * use directly (not a new scratch choice) in this combined module's
 * own test. */
#define BD_FREE_ROOT   0x0052 /* word: olist/llist free-node list root */
#define BD_OPEN_MAX    0x008C /* byte: max simultaneous open files/locks allowed */
#define BD_LOCK_MAX    0x008A /* byte: max simultaneous locked records allowed */

static inline uint16_t mem_r16(const uint8_t *mem, uint16_t addr) {
    return (uint16_t)(mem[addr] | (mem[(uint16_t)(addr + 1)] << 8));
}

static inline void mem_w16(uint8_t *mem, uint16_t addr, uint16_t val) {
    mem[addr] = (uint8_t)(val & 0xFF);
    mem[(uint16_t)(addr + 1)] = (uint8_t)(val >> 8);
}

/* A real segment:offset read/write -- for the first time, a port
 * genuinely needs this (src-c/kernel/ml.c's own mla_value, reading its
 * caller's MPB through a dynamically-supplied segment, `u_wrkseg`'s
 * own stored value -- not a fixed scratch address the way every other
 * ES/DS-switch elsewhere in this project turned out to be moot).
 * tools/i86sim.py's own `segmented=True` mode performs the identical
 * `(seg * 16 + offset) & 0xFFFF` computation for the real asm
 * reference run -- see that module's own docstring. */
static inline uint16_t mem_r16_seg(const uint8_t *mem, uint16_t seg, uint16_t offset) {
    return mem_r16(mem, (uint16_t)((uint16_t)(seg * 16) + offset));
}

static inline void mem_w16_seg(uint8_t *mem, uint16_t seg, uint16_t offset, uint16_t val) {
    mem_w16(mem, (uint16_t)((uint16_t)(seg * 16) + offset), val);
}

static inline uint8_t mem_r8_seg(const uint8_t *mem, uint16_t seg, uint16_t offset) {
    return mem[(uint16_t)((uint16_t)(seg * 16) + offset)];
}

static inline void mem_w8_seg(uint8_t *mem, uint16_t seg, uint16_t offset, uint8_t val) {
    mem[(uint16_t)((uint16_t)(seg * 16) + offset)] = val;
}

/* Real-memory addressing -- what every src-c/kernel file runs on. (The
 * flat 64KB mem_* helpers above are for src-c/loader, which still uses
 * that model.)
 * `phys` is the base of the real 8086 physical address space: the
 * linked kernel passes the start of physical memory, a test passes a
 * host buffer standing in for it. seg:off is the real 8086 address
 * seg*16 + off -- 20 bits, no 16-bit wrap, real offsets throughout. */
static inline uint8_t *far_ptr(uint8_t *phys, uint16_t seg, uint16_t off) {
    return phys + ((uint32_t)seg << 4) + off;
}

static inline uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

/* Byte/word at `off` within a segment, given the segment's base
 * (from far_ptr). `off` is uint16_t, so `si + p_link`-style arithmetic
 * passed straight in wraps at 64KB the way a real 8086 offset does. */
static inline uint8_t seg_r8(const uint8_t *seg, uint16_t off) {
    return seg[off];
}

static inline void seg_w8(uint8_t *seg, uint16_t off, uint8_t v) {
    seg[off] = v;
}

static inline uint16_t seg_r16(const uint8_t *seg, uint16_t off) {
    return rd16(seg + off);
}

static inline void seg_w16(uint8_t *seg, uint16_t off, uint16_t v) {
    wr16(seg + off, v);
}

/* The running process's UDA segment. The real asm reaches u_* fields
 * through its ambient ES, which always holds this while a process is
 * running (fdisp sets it explicitly: `mov es,p_uda[rlr]`). */
static inline uint16_t rlr_uda(const uint8_t *sysdat) {
    return seg_r16(sysdat, (uint16_t)(seg_r16(sysdat, SYSDAT_RLR) + p_uda));
}

/* SUPIF.SUP's user_entry, as an RSP's `int mpmint` reaches a function:
 * u_wrkseg = the caller's DS (left that way afterwards, as user_entry
 * does on a call from user level). Its stack switch, u_insys count and
 * u_retseg/u_stack_* saves aren't modeled. */
static inline void user_enter(uint8_t *phys, uint16_t sysdat, uint16_t ds) {
    seg_w16(far_ptr(phys, rlr_uda(far_ptr(phys, sysdat, 0)), 0), u_wrkseg, ds);
}

/* src/kernel/MAU.MEM's own SAT (suballocation table) header offsets,
 * needed by src-c/kernel/ml.c's own mla_initmau (the first entry of
 * a brand-new MAU's own SAT, at offset `satlen` within the MAU's own
 * segment -- see that file's own header comment). */
#define sat_start  0 /* word */
#define sat_length 2 /* word */
#define sat_nall   4 /* byte */
#define satlen     5

/* src/kernel/QD.DEF -- the one bit src-c/kernel/command.c's own
 * cli_checkque touches (ms_flags/mpb_flags' own q_flags alias) */
#define qf_rsp 0x08

/* src/kernel/UDA.FMT -- real field offsets (NOT fixed scratch
 * addresses -- unlike UDA_WRKSEG/UDA_DMA_SEG/etc. above, which stand
 * in for "the one ambient current process's own UDA field" when a
 * routine never needs more than one UDA at a time). src-c/kernel/
 * proc.c's own proc_creat writes through a brand-new child process's
 * own dynamically-computed UDA segment -- a genuinely different
 * segment each call, the same real infrastructure gap src-c/kernel/
 * ml.c's own sat_start/sat_length/sat_nall above exist for -- so these
 * are used with defs.h's mem_r16_seg/mem_w16_seg/mem_r8_seg/mem_w8_seg
 * helpers against that dynamic segment, never as a fixed address. */
#define u_dparam      0x00 /* word: arg to dispatch */
#define u_dma_ofst    0x02 /* word: BDOS dma offset */
#define u_dma_seg     0x04 /* word: BDOS dma segment */
#define u_error_mode  0x10 /* byte: BDOS error mode */
#define u_mult_cnt    0x11 /* byte: BDOS multi-sector count */
#define u_df_password 0x12 /* byte[8]: BDOS default password */
#define u_pd_cnt      0x1A /* byte: BDOS process count */
#define u_in_int      0x1B /* byte */
#define u_sp          0x1C /* word: save register area -- sp */
#define u_ss          0x1E /* word: save register area -- ss */
#define u_ax          0x20 /* word */
#define u_bx          0x22 /* word */
#define u_cx          0x24 /* word */
#define u_dx          0x26 /* word */
#define u_di          0x28 /* word */
#define u_si          0x2A /* word */
#define u_bp          0x2C /* word */
#define u_ds_sav      0x32 /* word */
#define u_stack_sp    0x34 /* word: usr stack pointer */
#define u_ivectors    0x38 /* word[10]: save int 0-4 */
#define u_es_sav      0x4C /* word */
#define u_flag_sav    0x4E /* word */
#define u_initcs      0x50 /* word */
#define u_initds      0x52 /* word */
#define u_inites      0x54 /* word */
#define u_initss      0x56 /* word */
#define u_mpm_ip      0x58 /* word: MPM vec save (+ mpm_cs/debug_ip/
                               debug_cs immediately following, copied
                               together as one 4-word block) */
#define u_insys       0x60 /* byte: # times through user_entry */
#define u_stat_sav    0x61 /* byte: p_stat save, used during interrupts */

/* src/kernel/VEC.FMT -- i_mpm_ip, overlaying the real (absolute,
 * segment-0) interrupt vector table at mpmint*4 (src/kernel/
 * SYSTEM.DEF: mpmint equ 224) -- see that file's own header comment
 * for why this doesn't just equal mpmint*4 when assembled inline (it
 * does here: this is the already-resolved real address, confirmed
 * against `--symbols` when VEC.FMT is assembled on its own, org 0) */
#define i_mpm_ip      0x380
#define i_mpm_cs      0x382 /* word: immediately follows i_mpm_ip -- src/kernel/
                               INIT.SUP's own `init` writes both as one real
                               interrupt-vector-table entry (ip then cs) */

/* src/kernel/MODFUNC.DEF -- mpmif function codes src-c/kernel/command.c
 * uses ((user*0x100)+n unless noted) */
#define f_conout         0x02
#define f_fopen          0x0F
#define f_fclose         0x10
#define f_qopen          0x87
#define f_qread          0x89
#define f_qwrite         0x8B
#define f_createproc     0x90
#define f_setprior       0x91
#define f_conassign      0x95
#define f_parsefilename  0x98
#define f_cconattch      0xA2
#define f_load           0x10A /* (sup*0x100)+10 */
#define f_cload          0x10E /* (sup*0x100)+14 */
#define f_conprint       0x40E /* (cio*0x100)+14 */

/* src/kernel/SYSDAT.FMT -- the tod structure's length (tod_entry's
 * copy); the structure itself is at SYSDAT_TOD */
#define todlen     5

/* src/kernel/SYSTEM.DEF */
#define ulen    0x100 /* size of uda */
#define lstklen 96    /* size of load stack */

/* src/kernel/EQU.BDO -- fixed build-time constants (not scratch) */
#define fcbshf  5 /* log2(fcblen) */
#define dskmsk  3 /* dirrec-1 = (recsiz/fcblen)-1 = directory elements per record, minus 1 */
#define bdos_empty  0xE5 /* empty directory entry marker (EQU.BDO's own `empty` -- renamed
                             here since a bare `empty` is too generic a macro name to export
                             into every file that includes defs.h) */
#define maxext  31 /* largest extent number (EQU.BDO) */
#define maxmod  0x3F /* largest module number (EQU.BDO) */
#define lstrec  0x7F /* last record number in an extent (EQU.BDO) */

/* src/kernel/LDTAB.DEF -- one Load Table entry (src-c/kernel/load.c's
 * own load_finish) */
#define ldtab_start  0x00 /* word */
#define ldtab_min    0x02 /* word */
#define ldtab_max    0x04 /* word */
#define ldtab_pd     0x06 /* word */
#define ldtab_attrib 0x08 /* word */
#define ldtab_fstrt  0x0A /* word */
#define ldtab_flen   0x0C /* word */
#define ldtab_type   0x0E /* byte */
#define ldtab_id     0x0F /* word */
#define ldtablen     0x11

/* src/kernel/BASEP.FMT -- Base Page Format offsets (real, small,
 * dynamically-segment-relative like UDA.FMT's own u_* block above --
 * reached through load_basep's own dynamically-computed segment) */
#define bpg_cseg 0x03 /* word */
#define bpg_8080 0x05 /* byte */
#define bpg_eseg 0x0F /* word */
#define bpg_lddsk   0x50 /* byte */
#define bpg_pw1ptr  0x51 /* word */
#define bpg_pw1len  0x53 /* byte */
#define bpg_pw2ptr  0x54 /* word */
#define bpg_pw2len  0x56 /* byte */
#define bpg_fcb0    0x5C /* byte[16] */
#define bpg_fcb1    0x6C /* byte[16] */
#define bpg_dma  0x80 /* offset constant, not a field read/written here */

/* src/kernel/LSTK.FMT -- Load Stack Format offsets, relative to the
 * same dynamically-computed segment as u_initss/load_lstk (ls_sp is
 * zero-width, coinciding with ls_offset's own start -- see src-c/
 * kernel/load.c's own header comment for how this ties into src-c/
 * kernel/proc.c's own u_sp-relative IRET-frame write) */
#define ls_sp       0x56 /* offset constant: lstklen-10 */
#define ls_offset   0x56 /* word */
#define ls_cseg     0x58 /* word (filled in later, by proc_creat) */
#define ls_flags    0x5A /* word */
#define ls_roffset  0x5C /* word */
#define ls_rcseg    0x5E /* word */

/* src/kernel/APB.DEF -- Abort Parameter Block (abort_spec_entry's own
 * input, reached through u_wrkseg same as every other *PB above) */
#define abt_prior   32
#define apb_pd      0x00 /* word */
#define apb_term    0x02 /* word */
#define apb_cns     0x04 /* byte */
#define apb_net     0x05 /* byte */
#define apb_pdname  0x06 /* byte[pnamsiz] */
#define apb_len     0x0E

/* src/kernel/MODFUNC.DEF -- mpmif function code src-c/kernel/abort.c's
 * own terminate_entry uses (bdos*0x100)+46 */
#define f_bdosterm 0x12E

#define fcb_pptr 0x18 /* word: password-length word's own address, src/kernel/FCB.DEF */

/* src/kernel/SYSDAT.FMT -- COMMAND.SUP's cli_* fields (real SYSDAT
 * offsets). SYSTEM.DAT initializes cli_pcb (flnmptr = cli_cmdtail,
 * fcbptr = cli_fcb) and cli_cuspqpb (buffptr = cli_ppd: an RSP's queue
 * message is the parent PD followed by the command tail); COMMAND.SUP
 * itself never sets either, except pcb_flnmptr around the second
 * argument parse. */
#define SYSDAT_CLI_DMA      0x0563 /* byte[dskrecl] */
#define SYSDAT_CLI_DMA_OFST 0x05E3 /* word: the caller's saved u_dma_ofst */
#define SYSDAT_CLI_DMA_SEG  0x05E5 /* word: the caller's saved u_dma_seg */
#define SYSDAT_CLI_CHAIN    0x05E7 /* byte: true for a chain */
#define SYSDAT_CLI_TERM     0x05E8 /* byte: true: terminate on exit */
#define SYSDAT_CLI_PFLAG    0x05E9 /* word: the caller's saved p_flag */
#define SYSDAT_CLI_NET      0x05EB /* byte */
#define SYSDAT_CLI_PPD      0x05EC /* word: parent PD */
#define SYSDAT_CLI_CMDTAIL  0x05EE /* byte[130] */
#define SYSDAT_CLI_FCB      0x0670 /* byte[fcblen+1] */
#define SYSDAT_CLI_CUSPQPB  0x0691 /* byte[16] */
#define SYSDAT_CLI_ACB      0x06A1 /* byte[12] */
#define SYSDAT_CLI_PCB      0x06AD /* word[2] */
#define SYSDAT_CLI_PD       0x06B1 /* word: PD of the loaded program */
#define SYSDAT_CLI_ERR      0x06B3 /* word */
#define SYSDAT_CLI_BPAGE    0x06B5 /* word */
#define SYSDAT_CLI_LDDSK    0x06B7 /* byte */
#define SYSDAT_CLI_CNS      0x06B8 /* byte */
#define SYSDAT_CLI_USER     0x06B9 /* byte */
#define SYSDAT_CLI_DSK      0x06BA /* byte */
#define SYSDAT_CLI_ERR_MODE 0x06BB /* byte */
#define SYSDAT_CLI_DFIL     0x06BC /* byte: true if showing dayfile info */

/* src/kernel/XIOS.A86's fields: real offsets in its segment (XIOS.A86
 * includes sysdat.lib and orgs past it -- the DRI XIOS lives in the
 * SYSDAT segment, CS = DS = SYSDAT) */
#define XIOS_DMA_ADR 0x16A4 /* word: current DMA offset (SETDMA) */
#define XIOS_DMA_SEG 0x16A6 /* word: current DMA segment (SETDMAB) */
#define XIOS_TRK     0x16AB /* byte: current track (SETTRK) */
#define XIOS_SECT    0x16AC /* byte: current sector (SETSEC) */
#define XIOS_DISK    0x16B4 /* byte: physical disk selected (SELDSK) */
#define XIOS_DTRNTBL 0x16B5 /* byte[16]: logical -> physical disk (0FFh = none) */
#define XIOS_DPBASE  0x16D1 /* the Disk Parameter Header table (singles.lib) */
#define XIOS_CLOCKON 0x182A /* byte: tick flag enable (STRTCLK/STOPCLK) */

/* src/kernel/ENTTAB.DEF -- one `sysent` entry's own field layout */
#define ENT_MOD  0 /* byte */
#define ENT_FUNC 1 /* byte */
#define ENT_FLAG 2 /* byte */
#define entlen   3

/* src/kernel/CLICB.DEF -- the UDA_WRKSEG-relative command buffer
 * clicmd_entry's own `cli_copy` reads from (via mem_r8_seg). Named
 * cmdcb_* (not clicb_*) because tmp.c already uses `clicb_cmd` as a
 * local pointer variable name of its own (TMP.A86's own, unrelated,
 * DSEG-offset constant of the same name -- see SD_TMP_CLICB_CMD). */
#define cmdcb_net  0 /* byte */
#define cmdcb_cmd  1 /* byte */
#define cmdcblen   130 /* byte: cmdcb_cmd + 129 */

/* src/kernel/XIOS86Q.A86 -- not DRI source (a from-scratch QEMU
 * driver this project wrote), but the file actually combined into
 * the real bootable MPM.bin, unlike the real XIOS.A86. It has no
 * `INCLUDE`s beyond the standard system.def/modfunc.def/xioscb.def/
 * pd.def/sysdat.fmt chain and assembles to one fixed layout, so
 * these are real, probed addresses (`python3 tools/asm86.py
 * src/kernel/XIOS86Q.A86 -I src/kernel --symbols`), same reasoning
 * as src-c/loader/defs.h's own LDB86Q_ block -- not harness-chosen
 * scratch addresses. */
#define XQ_TICK_COUNT    0x1000 /* byte */
#define XQ_CLOCKON       0x1001 /* byte */
#define XQ_STOPPOLL      0x1002 /* word */
#define XQ_DISK          0x1004 /* byte: selected physical drive (0 only) */
#define XQ_TRK           0x1005 /* byte */
#define XQ_SECT          0x1006 /* byte */
#define XQ_DMA_ADR       0x1007 /* word */
#define XQ_DMA_SEG       0x1009 /* word */
#define XQ_CUR_CYL       0x100B /* byte */
#define XQ_CUR_HEAD      0x100C /* byte */
#define XQ_CUR_PSEC      0x100D /* byte */
#define XQ_CUR_BUFOFF    0x100E /* word */
#define XQ_OUTBYTE_TMP   0x1010 /* byte: fdc_out_byte's scratch */
#define XQ_DTRNTBL       0x1011 /* byte[4] */
#define XQ_SPECIFY_PARMS 0x1021 /* byte[4] */
#define XQ_RECAL_PARMS   0x1025 /* byte[3] */
#define XQ_SEEK_PARMS    0x1028 /* byte[4] */
#define XQ_READ_PARMS    0x102C /* byte[10] */
#define XQ_WRITE_PARMS   0x1036 /* byte[10] */
#define XQ_SECBUF        0x1040 /* byte[fd_secsize=512] */
#define XQ_DPH0          0x1240 /* word: SELDSK's own DPH address */
#define XQ_SIGNON        0x134A /* byte[...]: init's own signon message */

#endif
