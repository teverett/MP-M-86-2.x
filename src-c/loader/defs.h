/*
 * defs.h -- field offsets and scratch addresses translated from the
 * .A86/.EQU files under src/loader/.
 *
 * /src-c is a routine-by-routine C port of /src's hand-written ASM86,
 * verified by differential testing (see docs/src-c.md), not a byte-for-byte
 * recompilation. Separate from src-c/kernel/defs.h -- the loader and the
 * kernel are different modules with their own symbol spaces, even though
 * both get ported the same way.
 *
 * Only fields a ported routine actually touches are listed; add more as
 * later files need them.
 */
#ifndef SRC_C_LOADER_DEFS_H
#define SRC_C_LOADER_DEFS_H

#include <stdint.h>

/* src/loader/LBDOSEQU.A86 -- control character constants LBDOSIO.A86 uses */
#define ctlc   0x03
#define ctlh   0x08
#define ctls   0x13
#define rubout 0x7F
#define tab    0x09
#define cr     0x0D
#define lf     0x0A

/* src/loader/LBDOSDAT.A86 -- LBDOSIO.A86's own scratch variables, at
 * fixed harness-chosen addresses (same reasoning as src-c/kernel/
 * defs.h's own SD_RLR et al: tools/i86sim.py has no real segment
 * registers, so every routine touching one of these needs to agree on a
 * single flat address for it). `aret`/`lret` (low byte of aret) are
 * shared with the still-unported LBDOSENT.A86 dispatch module. */
#define LD_KBCHAR  0xB000 /* byte: initial key char, 0 = none pending */
#define LD_COMPCOL 0xB001 /* byte: true if computing column position */
#define LD_COLUMN  0xB002 /* byte: column position */
#define LD_LISTCP  0xB003 /* byte: listing toggle */
#define LD_ARET    0xB004 /* word: dispatch return value; low byte = lret */

/* src/loader/LBDOSEQU.A86 -- FCB-layout equates src-c/loader/lbdosf1.c's
 * own ported routines touch. Same numeric values as src-c/kernel/
 * defs.h's own fcb_* constants (same real FCB layout, just a separate
 * symbol space -- see this file's own header comment). */
#define fcblen  32 /* file control block size */
#define extnum  12 /* extent number field */
#define modnum  14 /* data module number (high bit = fwf) */
#define fwfmsk  0x80 /* bit in modnum: file write flag */
#define reccnt  15 /* record count field */
#define dskmap  16 /* disk map field */
#define nxtrec  fcblen /* next record field */
#define rofile  9 /* high order bit of first type char: r/o attribute */
#define recsiz  128 /* directory record size */
#define dskshf  2 /* log2(dirrec), dirrec = recsiz/fcblen */
#define dskmsk  3 /* dirrec-1 */
#define fcbshf  5 /* log2(fcblen) */

/* src/loader/LBDOSDAT.A86 -- the subset of this module's own scratch
 * fields src-c/loader/lbdosf1.c's own ported routines touch, at fixed
 * harness-chosen scratch addresses (same reasoning as every other
 * scratch constant in this file: LBDOSDAT.A86 keeps this data in cseg,
 * immediately following this module's own code in the real combined
 * LDBDOS.A86 bundle -- see that file's own header comment -- a real
 * address this port has no reason to reproduce). */
#define LD_CURDSK  0xB010 /* byte: current disk number */
#define LD_INFO    0xB011 /* word: current FCB address */
#define LD_RODSK   0xB013 /* word: read-only disk vector */
#define LD_DMAAD   0xB015 /* word: dma offset */
#define LD_CDRMAXA 0xB017 /* word: pointer to current dir max value */
#define LD_CURTRKA 0xB019 /* word: pointer to current track word */
#define LD_CURRECA 0xB01B /* word: current record address */
#define LD_BUFFA   0xB01D /* word: pointer to directory dma address */
#define LD_DPBADDR 0xB01F /* word: current disk parameter block address */
#define LD_CHECKA  0xB021 /* word: current checksum vector address */
#define LD_ALLOCA  0xB023 /* word: current allocation vector address */
#define LD_SECTPT  0xB025 /* word: sectors per track */
#define LD_BLKSHF  0xB027 /* byte: block shift factor */
#define LD_BLKMSK  0xB028 /* byte: block mask */
#define LD_EXTMSK  0xB029 /* byte: extent mask */
#define LD_MAXALL  0xB02A /* word: maximum allocation number */
#define LD_DIRMAX  0xB02C /* word: largest directory number */
#define LD_CHKSIZ  0xB02E /* word: size of checksum vector */
#define LD_OFFSETV 0xB030 /* word: offset tracks at beginning */
#define LD_TRANV   0xB032 /* word: address of translate vector */
#define LD_SEQIO   0xB034 /* byte: 1 if sequential i/o */
#define LD_SINGLE  0xB035 /* byte: true if single-byte allocation map */
#define LD_RCOUNT  0xB036 /* byte: record count in current fcb */
#define LD_EXTVAL  0xB037 /* byte: extent number and extmsk */
#define LD_VRECORD 0xB038 /* word: current virtual record; low byte = lvrecord */
#define LD_ARECORD 0xB03A /* word: current actual record */
#define LD_ABLOCK  0xB03C /* word: current actual block# * blkmsk */
#define LD_DPTR    0xB03E /* byte: directory pointer 0,1,2,3 */
#define LD_DCNT    0xB03F /* word: directory counter; low byte = ldcnt */
#define LD_DREC    0xB041 /* word: directory record */
#define LD_DMABASE 0xB043 /* word: segment base for disk i/o */
#define LD_PARAMETERSEGMENT 0xB045 /* word: user parameter segment */

/* src/loader/LBDOSDAT.A86 -- the subset src-c/loader/lbdosf2.c's own
 * ported routines touch, same reasoning/method as the LD_* block
 * above (checked against tests/src-c/fixtures/lbdosf2_wrapper.A86) */
#define LD_DIRBLK    0xB047 /* word: reserved allocation bits for the directory */
#define LD_USRCODE   0xB049 /* byte: current user number */
#define LD_DIRLOC    0xB04A /* byte: directory-location flag, set by srch */
#define LD_SRCHL     0xB04B /* byte: srch's own search length */
#define LD_SRCHA     0xB04C /* word: srch's own search address */
#define LD_RMF       0xB04E /* byte: read-mode flag for openreel */
#define LD_FCBCOPIED 0xB04F /* byte: true if copyfcb was actually called */
#define LD_EFCB      0xB050 /* byte: 0xE5 (empty directory entry), same
                                value as `empty` below -- a distinct
                                real memory location (FILE1.BDO's own
                                "make" temporarily points LD_INFO here) */

/* src/loader/LBDOSDAT.A86 -- the subset src-c/loader/lbdosf3.c's own
 * ported routines touch, same reasoning/method as the LD_* block
 * above (checked against tests/src-c/fixtures/lbdosf3_wrapper.A86) */
#define LD_DLOG      0xB051 /* word: logged-in disks vector */
#define LD_RESEL     0xB053 /* byte: reselection-occurred flag */
#define LD_FCBDSK    0xB054 /* byte: drive code named in the fcb at entry */
#define LD_OLDDSK    0xB055 /* byte: curdsk saved across a reselect */
#define LD_LINFO     0xB056 /* byte: normalized drive code (0-30 or 255) */
#define LD_PARCOPFL  0xB057 /* byte: true if a parameter block was copied in */
#define LD_PARLG     0xB058 /* byte: length of the copied parameter block */
#define LD_INFOSAVE  0xB059 /* word: LD_INFO's own value before parsave */
#define LD_LOC_PAR_AREA 0xB05B /* byte[258]: local parameter-block copy */

/* src/loader/LBDOSEQU.A86 -- more FCB-layout/BDOS equates src-c/
 * loader/lbdosf2.c's own ported routines touch, same reasoning as the
 * fcblen/extnum/etc. block above */
#define empty  0xE5 /* empty directory entry marker */
#define ubytes 13   /* unfilled bytes field */
#define maxext 31   /* largest extent number */
#define maxmod 15   /* largest module number */
#define namlen 15   /* name length */

/* src/loader/LDBIOS86Q.A86's own scratch/data cells and tables, at
 * their own REAL resolved addresses (not harness-chosen scratch
 * ones, unlike the LD_* block above): this file is fully self-
 * contained (no `INCLUDE`s at all, confirmed by grep) and always
 * assembled standalone, so `python3 tools/asm86.py src/loader/
 * LDBIOS86Q.A86 --symbols` gives one real, deterministic layout,
 * same reasoning as src-c/kernel/defs.h's own BD_* block. */
/* All of these (through LDB86Q_SEGTABLE below) are probed directly via
 * `--symbols` against src/loader/LDBIOS86Q.A86, and shift whenever
 * tools/asm86.py's own encoding choices change (last: `_encode_alu`'s
 * own accumulator-immediate short form, see that function's own
 * header comment) -- not a uniform shift across all of them, since
 * the fix only shrinks specific instructions wherever they occur. */
#define LDB86Q_DISK        0x144B /* byte: current disk number */
#define LDB86Q_TRK         0x144C /* byte: current track */
#define LDB86Q_SECT        0x144D /* byte: current sector */
#define LDB86Q_DMA_ADR     0x144E /* word: dma offset */
#define LDB86Q_DMA_SEG     0x1450 /* word: dma segment */
#define LDB86Q_CUR_CYL     0x1452 /* byte: physical cylinder */
#define LDB86Q_CUR_HEAD    0x1453 /* byte: physical head */
#define LDB86Q_CUR_PSEC    0x1454 /* byte: physical (512-byte) sector, 1-based */
#define LDB86Q_CUR_BUFOFF  0x1455 /* word: offset of this logical record within secbuf */
#define LDB86Q_SEEK_PARMS  0x145F /* byte[4]: FDC SEEK command table */
#define LDB86Q_READ_PARMS  0x1463 /* byte[10]: FDC READ DATA command table */
#define LDB86Q_WRITE_PARMS 0x146D /* byte[10]: FDC WRITE DATA command table */
#define LDB86Q_SECBUF      0x1477 /* byte[fd_secsize=512]: one physical sector's worth of scratch buffer */
#define LDB86Q_DPE0        0x1680 /* word: SELDSK's own DPH address (content unconsumed, not modeled -- see init.c-style precedent) -- probed via `--symbols`; shifts whenever tools/asm86.py's own encoding choices change (last: `_encode_alu`'s own accumulator-immediate short form, see that function's own header comment) */
#define LDB86Q_SEGTABLE    0x1677 /* word: GETSEGT's own segment-table address (static cseg data, content unconsumed by anything ported so far) -- probed via `--symbols`; shifts whenever tools/asm86.py's own encoding choices change (last: `_encode_alu`'s own accumulator-immediate short form, see that function's own header comment) */

static inline uint16_t mem_r16(const uint8_t *mem, uint16_t addr) {
    return (uint16_t)(mem[addr] | (mem[(uint16_t)(addr + 1)] << 8));
}

static inline void mem_w16(uint8_t *mem, uint16_t addr, uint16_t val) {
    mem[addr] = (uint8_t)(val & 0xFF);
    mem[(uint16_t)(addr + 1)] = (uint8_t)(val >> 8);
}

/* A real segment:offset read/write -- src-c/loader/ldbios86q.c's own
 * READ/WRITE, transferring a 128-byte logical record to/from their
 * caller's `dma_seg:dma_adr` buffer -- same reasoning/idiom as
 * src-c/kernel/defs.h's own mem_r16_seg/mem_w16_seg/mem_r8_seg/
 * mem_w8_seg (that file's own header comment has the detail). */
static inline uint8_t mem_r8_seg(const uint8_t *mem, uint16_t seg, uint16_t offset) {
    return mem[(uint16_t)((uint16_t)(seg * 16) + offset)];
}

static inline void mem_w8_seg(uint8_t *mem, uint16_t seg, uint16_t offset, uint8_t val) {
    mem[(uint16_t)((uint16_t)(seg * 16) + offset)] = val;
}

#endif
