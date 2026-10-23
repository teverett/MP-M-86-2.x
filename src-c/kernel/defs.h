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
#define e_bad_read         29 /* bad ret. from BDOS read */
#define e_abort            40 /* external termination occurred */

/* src/kernel/XIOSCB.DEF */
#define io_idle 0x17

/*
 * The fields below live in SYSDAT.FMT (System Data Area) or UDA.FMT (one
 * process's User Data Area) on real hardware -- each its OWN segment,
 * unrelated to the code segment or to each other. tools/i86sim.py and this
 * port both model memory as a single flat 64KB array with no real segment
 * registers (see i86sim's docstring), so these real hardware offsets
 * (small, e.g. SYSDAT.FMT's rlr=0x68) would collide with a routine's own
 * code if reused directly. Every Tier-1+ test instead places them at these
 * fixed, harness-chosen scratch addresses -- consistently, across every
 * ported routine that touches them, matching how they really are "the
 * same location" system-wide. The corresponding wrapper .A86 under
 * tests/src-c/fixtures/ declares each as a real DSEG label at the same
 * address (never a plain EQU, which would wrongly assemble "mov bx,rlr"
 * as an immediate load instead of a memory read) so the assembled
 * reference bytes and this port agree on what's a pointer-to and what's
 * the thing itself.
 */
#define SD_RLR        0xA000 /* word: Ready List Root -- running process's PD */
#define SD_DRL        0xA002 /* word: Dispatcher Ready List */
#define SD_DLR        0xA004 /* word: Delay List Root */
#define SD_NFLAGS     0xA006 /* byte: number of logical interrupt flags */
#define SD_FLAGS      0xA008 /* word: address of the Flag Table */
#define SD_VERNUM     0xA00A /* word: MP/M-86 w/BDOS version */
#define SD_MPMVERNUM  0xA00C /* word: MP/M-86 version */
#define UDA_DPARAM    0xA010 /* word: arg to dispatch */
#define UDA_WRKSEG    0xA012 /* word: current segment addr of buf */
#define UDA_RETSEG    0xA014 /* word: return segment for a far return */
#define SD_PLR        0xA01C /* word: Poll List Root */
#define SD_SLR        0xA01E /* word: Swap List Root */
#define SD_QLR        0xA020 /* word: root of the active Queue Descriptor list */
#define SD_QUL        0xA022 /* word: root of the unused-QD free list */
#define SD_QMAU       0xA024 /* word: MAU root queue buffers allocate from */
#define SD_ENDSEG     0xA026 /* word: segment just past the end of the system */
#define SD_MDUL       0xA028 /* word: root of the unused MD free list */
#define SD_MAL        0xA02A /* word: root of the active MAU list */
#define SD_MFL        0xA02C /* word: root of the Memory Free List */
#define SD_CCB        0xA030 /* word: base address of the CCB table */
#define SD_THRDRT     0xA032 /* word: Process Thread Root */
#define SD_NCNS       0xA034 /* byte: number of system console devices */
#define SD_NLSTDEV    0xA035 /* byte: number of XIOS list devices */
#define SD_NCONDEV    0xA036 /* byte: number of XIOS console devices */
#define SD_PUL        0xA038 /* word: Process Descriptor Unused List */
#define SD_MXMEMOWNER 0xA03A /* word: PD owning the MX memory queue mutex */
#define SD_MXMEMCOUNT 0xA03C /* byte: mutex nesting count */
#define SD_INDISP     0xAC00 /* byte: currently in dispatch? */
#define SD_INTFLAG    0xAC01 /* byte: if 0, interrupts not enabled */
#define SD_ES_SAV     0xAC02 /* word */
#define SD_BX_SAV     0xAC04 /* word */

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
#define fcb_modnum 0x0E /* byte: data module number (high bit = fwf) */
#define fcb_reccnt 0x0F /* byte: record count field */
#define fcb_dskmap 0x10 /* byte: disk map field */
#define fcb_nxtrec 0x20 /* byte: next record field (= fcblen) */
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
#define BD_PDADDR      0x0C3A /* word: PD address target for the current call */
#define BD_HIGH_EXT    0x0C34 /* byte: FCB high extent bits */
/* 0x0C35 (XFCB_READ_ONLY) unused by name */
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

/* src/kernel/SYSDAT.FMT -- free_root, the olist/llist free-node list
 * root (src-c/kernel/file3.c's own delete_item/create_item). Unlike
 * every BD_* constant above (FILE1-4.BDO's own scratch area, which
 * resolves to 0xC00+ in the combined module), this is a real
 * SYSDAT.FMT field -- confirmed via `--symbols` against the same
 * wrapper: it resolves to 0x52, not a BD_*-range address. See
 * file3.c's own header comment for why this is safe to use directly
 * (not a new scratch choice) in this combined module's own test. */
#define BD_FREE_ROOT   0x0052 /* word: olist/llist free-node list root */

/* src-c/kernel/chario.c's own scratch addresses for the literal CS-
 * resident strings src/kernel/CHARIO.CIO embeds inline (ptrmsg,
 * abortmsg, attachmsg) -- this port has nowhere else to put them, so it
 * writes each one into one of these fixed, harness-chosen addresses
 * just before printing it (same "fixed scratch address" reasoning as
 * every other scratch constant above), rather than reproducing the
 * real segment-override dance CHARIO.CIO's prcsmsg/prname use to read a
 * CS- or DS-resident string transparently. */
#define CHARIO_PTRMSG    0xB200
#define CHARIO_ABORTMSG  0xB220
#define CHARIO_ATTACHMSG 0xB240

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

/* src/kernel/SYSDAT.FMT -- time-of-day bytes src-c/kernel/command.c's
 * own prtime reads, at fixed harness-chosen scratch addresses (same
 * reasoning as every other SD_* constant above) */
#define SD_TOD_HR  0xA040
#define SD_TOD_MIN 0xA041
#define SD_TOD_SEC 0xA042

/* src/rsp/CLOCK.A86's own TOD (time-of-day) structure in SYSDAT
 * (day:word, hour:byte, min:byte, sec:byte, real offsets 0/2/3/4) --
 * a separate harness-chosen scratch base from SD_TOD_HR/MIN/SEC above
 * (COMMAND.SUP's own, independently chosen, not confirmed to be the
 * same physical struct), same "fixed scratch address" reasoning */
#define SD_CLOCK_TOD_BASE 0xA044 /* word[1]+byte[3]: day/hour/min/sec */

/* src/kernel/SYSDAT.FMT's own `tod`/`tod_day`/`tod_hr`/`tod_min`/
 * `tod_sec` (real offsets 0/0/2/3/4, `tod` itself a zero-size alias
 * for `tod_day`) -- src/kernel/SYSFUNC.SUP's own `tod_entry` reads
 * this same real struct `rep movs`-style (`todlen`=5 bytes), but at
 * its own independently-chosen scratch base, same reasoning as
 * SD_CLOCK_TOD_BASE just above (not confirmed to be the same real
 * struct as either SD_TOD_HR/MIN/SEC or SD_CLOCK_TOD_BASE) */
#define SD_TOD     0xA049 /* word[1]+byte[3]: tod_day/tod_hr/tod_min/tod_sec */
#define todlen     5

/* src/kernel/SYSDAT.FMT -- src/kernel/COMMAND.SUP's own scratch
 * variables (the subset src-c/kernel/command.c's ported subroutines
 * touch), at fixed harness-chosen scratch addresses */
#define SD_CLI_CNS     0xA050
#define SD_CLI_USER    0xA051
#define SD_CLI_DSK     0xA052

/* src/kernel/UDA.FMT -- the subset of UDA fields src-c/kernel/file4.c's
 * own ported routines touch, at fixed harness-chosen scratch addresses
 * (same reasoning as every other UDA_ and SD_ constant above: UDA.FMT
 * declares its own `eseg`/`org 0`, a real separate hardware segment,
 * so its own real small offsets -- e.g. u_dma_ofst=2, u_retseg=0x30 in
 * the combined BDOS bundle -- collide with cseg's own early code once
 * collapsed into tools/i86sim.py's single flat 64KB array; same class
 * of gap as SYSDAT.FMT's own rlr=0x68, see that comment above) */
#define UDA_DMA_OFST    0xA060 /* word: BDOS dma offset */
#define UDA_DMA_SEG     0xA062 /* word: BDOS dma segment */
#define UDA_ERROR_MODE  0xA064 /* byte: BDOS error mode */
#define UDA_MULT_CNT    0xA065 /* byte: BDOS multi-sector count */

/* src/rsp/TMP.A86's own DSEG fields src-c/kernel/tmp.c touches, at
 * fixed harness-chosen scratch addresses -- TMP.A86's real `org
 * rsp_top`/`org rsp_pd`/`org rsp_bottom` DSEG offsets (e.g. clicb_cmd
 * at 0x1e0) land squarely inside its own 0x423-byte cseg once
 * collapsed into tools/i86sim.py's single flat 64KB array (confirmed
 * via `--symbols`), unlike ECHO.A86/CLOCK.A86 where this happened not
 * to collide; same class of gap as SYSDAT.FMT's own rlr=0x68, see that
 * comment above */
#define SD_TMP_CMDSENT    0xA070 /* byte: true once a CLI command has been sent */
#define SD_TMP_READ_BLEN  0xA071 /* byte: length of the last console-read command */
#define SD_TMP_CLICB_CMD  0xA100 /* byte[129]: the command text itself */
#define SD_TMP_SAVEBUF    0xA200 /* byte[128]: clicall's own copy, '$'-terminated */

/* src/rsp/SUBMIT.A86's own DSEG fields src-c/kernel/submit.c touches,
 * at fixed harness-chosen scratch addresses -- same collision as
 * TMP.A86 (its real DSEG offsets, e.g. cmdtail at 0x80, land inside
 * its own 0x665-byte cseg), see defs.h's comment on SD_TMP_* above. */
#define SD_SUBMIT_CMDTAIL    0xA300 /* byte[1+128]: length byte + raw command tail */
#define SD_SUBMIT_DEBLANKBUF 0xA400 /* byte[129]: deblanked command tail */
#define SD_SUBMIT_ARGTABLE   0xA500 /* byte[64]: each argument's start offset into DEBLANKBUF */
#define SD_SUBMIT_NUMARGS    0xA540 /* word: number of arguments found */
#define SD_SUBMIT_CMDBUF     0xA5F0 /* byte: readbuffer's own max-length byte */
#define SD_SUBMIT_BLEN       0xA5F1 /* byte: readbuffer's own count byte */
#define SD_SUBMIT_CMD        0xA5F2 /* byte[129]: one line read from the SUB file */
#define SD_SUBMIT_CLICMD     0xA700 /* byte[129]: cmd with $0-$9 args substituted in */
#define SD_SUBMIT_DMA        0xA800 /* byte[128]: the SUB file's current 128-byte DMA page */
#define SD_SUBMIT_BUF_PTR    0xA880 /* byte: current read position within SD_SUBMIT_DMA */
#define SD_SUBMIT_PDADR      0xA890 /* word: this process's own PD address */
#define SD_SUBMIT_SUBFILENAME 0xA8A0 /* byte[11]: the SUB file's own name, for the abort prompt */
#define SD_SUBMIT_FCB        0xA8B0 /* byte[36]: the SUB file's own FCB */
#define SD_SUBMIT_CMDSENT    0xA8E0 /* byte: true once a CLI command has been sent */
#define SD_SUBMIT_DEFDISK    0xA8E1 /* byte: current default disk */
#define SD_SUBMIT_DEFUSER    0xA8E2 /* byte: current default user number */
#define SD_SUBMIT_PROMPTUTENS 0xA8E3 /* byte: prompt's own user-number tens digit (or 13 if none) */
#define SD_SUBMIT_PROMPTUONES 0xA8E4 /* byte: prompt's own user-number ones digit */
#define SD_SUBMIT_PROMPTDISK  0xA8E5 /* byte: prompt's own disk-letter character */

/* src/kernel/SYSTEM.DEF */
#define ulen    0x100 /* size of uda */
#define lstklen 96    /* size of load stack */

/* src/kernel/EQU.BDO -- fixed build-time constants (not scratch) */
#define fcbshf  5 /* log2(fcblen) */
#define dskmsk  3 /* dirrec-1 = (recsiz/fcblen)-1 = directory elements per record, minus 1 */

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

/* src/kernel/SYSDAT.FMT -- "Data Used by Load Program", the subset
 * src-c/kernel/load.c's own load_finish touches, at fixed
 * harness-chosen scratch addresses (same reasoning as every other
 * SD_* constant above) */
#define SD_LOAD_PD          0xA900 /* word: pd of load prog */
#define SD_LOAD_FCB         0xA902 /* word: addr of open FCB in u_wrkseg */
#define SD_LOAD_NLDTABENTS  0xA904 /* word */
#define SD_LOAD_BASEP       0xA906 /* word */
#define SD_LOAD_8080        0xA908 /* byte */
#define SD_LOAD_UDA         0xA909 /* word */
#define SD_LOAD_LSTK        0xA90B /* word */
#define SD_LOAD_LDTAB       0xA910 /* ldtab[], ldtablen bytes per entry (up to
                                       10 entries, wrapper reserves 200 bytes
                                       -- ends at 0xA9D8) */
#define SD_LOAD_INDMA       0xA9E7 /* word: starting paragraph # currently
                                       sitting in SD_LOAD_DMA (tests/src-c/
                                       fixtures/load_wrapper.A86's own real
                                       `load_indma`, confirmed via --symbols) */
#define SD_LOAD_DMA         0xAB00 /* 128-byte (8-paragraph) shared record
                                       buffer, src-c/kernel/load.c's own
                                       load_group/load_finish (load_wrapper.
                                       A86's own real `load_dma`) */

/* `diskread`'s/`load_group`'s own real `mpmif(f_freadseq)` dependency
 * (see src-c/kernel/load.c's own header comment): a deliberately
 * simplified single-extent "virtual disk" standing in for a real
 * sequential BDOS file read. `SD_LOAD_DISK` is laid out as
 * consecutive `SD_LOAD_DISK_RECS`-many 128-byte CP/M records,
 * starting at `fcb_nxtrec`'s own real record 0; this port supports
 * only a single extent (no disk-map/extent-switch logic), which every
 * file this project's own tests load is small enough to stay within
 * -- real multi-extent handling is out of scope here, same spirit as
 * every other "deliberate simplification, documented" case elsewhere
 * in this tree. Both scratch addresses are this port's own (no real
 * SYSDAT.FMT equivalent -- the real BDOS keeps no such flat array). */
#define SD_LOAD_DISK_RECS   0xA9E9 /* word: # of 128-byte records in SD_LOAD_DISK */
#define SD_LOAD_DISK        0xB000 /* record 0 starts here, 128 bytes/record */

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

#define CLI_FCB        0xB300 /* byte[fcblen+1] */
#define CLI_CUSPQPB    0xB320 /* byte[16] */
#define CLI_ACB        0xB340 /* byte[12] */
#define CLI_PCB        0xB350 /* word[2] */
#define CLI_CMDTAIL    0xB360 /* byte[130] */
#define MXCLIQPB       0xB400 /* byte[16] */

#endif
