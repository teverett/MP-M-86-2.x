    #!/usr/bin/env python3
"""Combine the kernel modules into one loadable MPM.SYS-equivalent image.

This is the kernel-side counterpart to tools/build_mpmldr.py's loader
combiner. There is no GENSYS-86 source anywhere in this tree to copy
the real recipe from (see docs/orig.md's "Related DRI source: MP/M II"
for why -- it was one of the VAX-compiled utilities, never included in
this distribution), so this is a from-scratch replacement informed by
what *is* directly evidenced in the tree:

  - orig/mpm862sr/07/MPM86GEN.TEX says the kernel modules (SUP, RTM,
    MEM, CIO, BDOS, XIOS) are each assembled and GENCMD'd separately,
    then "combined" into MPM.SYS -- same two-stage shape as the loader,
    just without a surviving recipe for the combining step itself.
  - src/loader/LDMPM.A86's own comments describe the MPM.SYS file
    format it expects: a 128-byte header (type/length/absolute
    segment/min/max) for a combined CODE blob, then the code itself,
    then the same for a combined DATA blob -- i.e. every module's code
    concatenated into ONE code segment and every module's data into
    ONE data segment, not one segment per module.
  - src/kernel/SYSDAT.FMT's own "Module Table" (supmod/rtmmod/memmod/
    ciomod/bdosmod/xiosmod/netmod, each an 8-byte {entry far ptr,
    init far ptr} pair, "initialized by GENSYS") is how modules actually
    reach each other at runtime -- every "xiosif"/"mpmif"/"supif"-style
    trampoline in every module (CIOIF.CIO, RTMIF.RTM, SUPIF.SUP,
    MEMIF.MEM) calls through one of these far pointers, confirmed by
    reading their actual source. This script's real job is computing
    and writing those 6 far pointers (plus "dispatcher" and
    "module_map") correctly.

Given that, this script:
  1. Assembles SYSDAT.A86 (the one authoritative copy of the shared
     system data area -- every other module also includes sysdat.fmt,
     but purely to resolve its own label offsets against it; their own
     copy of those bytes is discarded, not loaded -- see the ESEG/DSEG
     investigation in tools/README.md for how this was confirmed) and
     SUP/RTM/MEM/CIO/BDOS/XIOS86Q.A86.
  2. Concatenates the six modules' CSEG back-to-back into one blob (the
     same "no fixed offsets, just lay them out and compute where things
     landed" approach tools/build_mpmldr.py already uses, and for the
     same reason: tools/asm86's larger code output makes any fixed
     budget unreliable to assume).
  3. Builds the combined DSEG as SYSDAT's own 4096-byte dseg.bin plus
     XIOS86Q's own private tail (the bytes from offset 4096 onward in
     its dseg.bin -- see XIOS86Q.A86's own comment on why its private
     data starts exactly there). SUP/RTM/MEM/CIO/BDOS's own dseg.bin
     output is the *same* 4096 bytes as SYSDAT's (confirmed directly --
     they have no private data of their own), so it's simply discarded.
  4. Patches the module table's entry/init far pointers (reading each
     module's real "init"/"entry" label offsets from `asm86 --symbols`,
     not assumed to be at any fixed offset within the module -- they
     aren't the same across modules) and `module_map` directly into the
     in-memory copy of SYSDAT's dseg.bin before writing it out.

KNOWN LIMITATIONS (also see tools/build_mpmldr.py and
src/kernel/XIOS86Q.A86's own "KNOWN LIMITATIONS" -- this inherits
theirs):
  - `dispatcher` (SYSDAT.FMT offset 0x38, used directly by XIOS86Q's
    tick ISR) is set to RTM's own general "entry" dispatch address,
    since no more specific "the real scheduler re-entry point" label
    was found anywhere in RTM.A86/DSPTCH.RTM -- an assumption, not
    confirmed against real GENSYS behavior.
  - Only ncondev/nlstdev/nciodev/tickspersec are set (to match
    XIOS86Q's own single-console, no-list-device, 60Hz configuration);
    every other "answers to GENSYS questions" field in SYSDAT.FMT keeps
    whatever DATA.BDO/SYSTEM.DAT initialized it to, which was written
    for the real XIOS.A86/BDOS configuration, not necessarily this one.
  - `netmod` is left unpopulated (no network module in this build).
  - This produces one flat image, not a real "MPM.SYS" file in
    LDMPM.A86's own expected on-disk format (two 128-byte headers plus
    code then data) -- that's one more step belonging to whatever
    eventually writes a bootable disk image, not this script.
  - Like everything else built this way, this has never actually run.

Usage: python3 tools/build_mpmsys.py [-o OUTPUT]
"""
import argparse
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL_DIR = os.path.join(ROOT, 'src', 'kernel')
ASM86 = [sys.executable, os.path.join(ROOT, 'tools', 'asm86.py')]

# Must match tools/build_disk.py's own MPM_CODE_SEG -- that script
# chooses where MPM.SYS's combined code segment actually gets loaded
# in memory, but the module table's far pointers (written here) need
# that same segment baked in *now*, since nothing downstream ever goes
# back to patch them. Missing that patch entirely (writing segment=0
# for every entry, as this script used to) wasn't just a different
# wrong address -- it was confirmed, by testing in QEMU, to make every
# single intermodule call (every "xiosif"/"mpmif"/"supif"-style
# trampoline, all the way down to `INIT.SUP`'s own first `callf dword
# ptr rtmmod[bx]`) jump to physical address `0x00000 + offset` instead
# of where the code actually lives -- straight into the IVT/BIOS data
# area, with no further diagnostic output at all.
MPM_CODE_SEG = 0x1000

# Module name -> (source file stem, SYSDAT.FMT module-table field offset)
MODULES = [
    ('SUP', 'SUP', 0x00),
    ('RTM', 'RTM', 0x08),
    ('MEM', 'MEM', 0x10),
    ('CIO', 'CIO', 0x18),
    ('BDOS', 'BDOS', 0x20),
    ('XIOS', 'XIOS86Q', 0x28),
]
DISPATCHER_FIELD_OFFSET = 0x38
MODULE_MAP_OFFSET = 0x46
NCONDEV_OFFSET = 0x83
NLSTDEV_OFFSET = 0x84
NCIODEV_OFFSET = 0x85
TICKSPERSEC_OFFSET = 0x51
SRCHDISK_OFFSET = 0x4b

# SYSDAT.FMT's "pul" (Process Descriptor Unused List head, 0x5c) -- a
# RUNTIME state variable (the free-PD-slot linked list RTM's own
# getpdadr/freepd maintain), not a GENSYS config answer, but its
# backing storage (SYSDAT.A86 -> DATA.BDO, real unmodified DRI source)
# still bakes in a stale non-zero value (0x0020) left over from
# whatever state the real system's template captured it in. "0" means
# "list is empty" (the correct state for a fresh boot, matching how
# every entry ever gets *added* to it, via `freepd`/`mov pul,si`) --
# left as 0x0020, `getpdadr`'s `gpd_bad`/`gpd_have` path (triggered the
# very first time RTM creates a process whose PD didn't already live
# in the sysdat-relative PD table -- i.e. immediately, for the first
# process RTM ever creates) trusted it as a real PD-table slot address
# and blindly `rep movsw`'d a 48-byte PD copy to absolute SYSDAT offset
# 0x20 -- confirmed with a QEMU hardware watchpoint on SYSDAT+0x28
# (the module table's `xiosmod` entry, squarely inside that 0x20-0x4f
# range): it fired on exactly this `rep movsw`, with SI/DI showing the
# copy ran from `idlepd` to offset 0x20. That silently clobbered
# `bdosmod`/`xiosmod`/`netmod`/`dispatcher`/`rtm_pdisp` in the module
# table with garbage, well after `INIT.SUP`'s own module-init calls
# (which only read the table, never got to see it broken) -- the first
# symptom was `INIT.SUP`'s very next XIOS call (`io_maxconsole`, via
# `xiosif`) silently jumping nowhere once `xiosmod`'s far pointer was
# gone.
#
# Checking every other "unused list"/"xxx root" field in SYSDAT.FMT
# the same way (dumping SYSDAT.A86's own raw assembled dseg.bin) found
# more of the same: `mdul` (Memory Descriptor Unused List, 0x58) is
# *also* a stale non-zero leftover, and `qlr` (Queue List Root, 0x74)
# holds a stale non-zero value too even though no queue has been
# created yet this boot. `rlr`/`thrdrt` (0x68/0x72) are left alone --
# both hold `initpd`'s own address, which really is correct for a
# fresh boot (the init process is what's running, and is on its own
# thread list).
#
# `qul` (QCB Unused List, 0x5e) needed more than just zeroing, though:
# unlike `pul` (whose only real callers here, `getpdadr`'s `gpd_bad`
# fallback, never actually trigger for RTM's own tick/idle process
# creation -- their PDs are already sysdat-resident, confirmed by
# re-deriving `getpdadr`'s address math by hand), `getqdaddr`'s own
# `gqd_bad` fallback (QUE2.RTM) *does* genuinely trigger here: BDOS's
# own init (BDOSIF.BDO) creates "mxdiskqd" by passing its *own cseg*
# address (it's declared via DATA.BDO inside BDOS's own module, not in
# sysdat), which `getqdaddr` always treats as "foreign" and copies
# into a real system-table slot pulled from `qul` -- a slot this tree
# never had, since there's no GENSYS equivalent anywhere here to have
# built one. Leaving `qul` zeroed just turns the corruption into a
# clean, immediate `e_no_qd` error return instead of a wild write --
# better, but it means queue creation can never actually succeed.
# Fixed by writing a small real QD pool straight into existing slack
# space in SYSDAT's own 4096-byte dseg (SYSDAT.A86's real assembled
# size has ~1.1KB of room after its last declared field, `mxmemcount`)
# and linking it through `qul` below -- no SYSDAT.FMT change needed:
# unlike the module table, nothing anywhere ever needs to reference
# this pool *by name* at compile time (`qul` is purely a runtime
# pointer RTM's own code follows, never a label other code assembles
# against), and SYSDAT.FMT doesn't drive SYSDAT.A86's real content
# anyway -- every *other* module includes it only to resolve its own
# label offsets, while SYSDAT.A86 gets its real bytes from SYSTEM.DAT/
# DATA.BDO instead (confirmed: adding a field there left SYSDAT.A86's
# own assembled dseg.bin size completely unchanged).
MDUL_FIELD_OFFSET = 0x58
QLR_FIELD_OFFSET = 0x74
PUL_FIELD_OFFSET = 0x5c
QUL_FIELD_OFFSET = 0x5e

# `pul` (like `qul` above) needs more than zeroing: zeroing it only
# stops the old DATA.BDO-template wild-write bug (confirmed via a QEMU
# watchpoint on SYSDAT's p_thread[tickpd]: `getpdadr`'s `gpd_bad` path,
# taken for *any* PD that doesn't already live sysdat-resident --
# i.e. every RSP's own PD, such as TMP's, which lives in TMP's own
# dseg -- reached `gpd_have` and `rep movsw`'d a 48-byte PD copy
# straight over live kernel state once `pul` held a stale non-zero
# DATA.BDO value) -- but with `pul` left at a permanent 0 (empty),
# every *non*-sysdat-resident PD allocation (any RSP's `f_createproc`,
# any CLI program load via COMMAND.SUP's `cli_fload`) fails outright
# with `e_no_pd`, forever: nothing ever adds slots to `pul` except
# `freepd`, and nothing is ever freed before something needs to be
# allocated. GENSYS would have built a real PD pool sized to the
# configured max process count and linked it through `pul`; this tree
# has no GENSYS equivalent, so (exactly like `qul`'s own QD pool fix
# above) a real pool is carved out of SYSDAT's own slack space and
# chained through `pul` as a standard singly-linked free list (each
# slot's own `p_link`, offset 0, points to the next slot; the last
# points to 0) -- matching exactly what `getpdadr`'s `gpd_have` path
# (PROC.RTM) already expects to find there.
PD_POOL_SLOT_SIZE = 0x30  # pdlen (PD.DEF)
PD_POOL_SLOTS = 4

# `endseg` (SYSTEM.DAT: "dw 0 ;1st parag. outside of MP/M") is GENSYS-
# computed data in the original 1981 build -- this tree's own
# SYSTEM.DAT just declares it statically as 0 and nothing in src/
# kernel/ ever writes to it afterward (confirmed: grep -rn "endseg,"
# across the whole tree turns up nothing). `PROC.RTM`'s `getpdadr`
# (used by `proc_creat` to validate a PD address, e.g. when creating
# the tick/idle processes during RTM's own `init:`) and `QUE2.RTM`'s
# `getqdaddr` both do `cmp ax,sysdat ! jb bad ! cmp ax,endseg ! jae
# bad` before a *second*, separate, tighter "within 64K of sysdat"
# check -- so with `endseg` stuck at 0, literally every real SYSDAT
# address (always >= sysdat, which is always > 0) fails the "below
# endseg" half unconditionally, regardless of how valid it actually
# is. Confirmed via a serial-trace probe: `getpdadr` returned
# `e_no_pd` (`CX=000Ch`) for `idlepd`'s own, perfectly valid address
# during RTM's `init:`'s two `proc_creat` calls (chained via
# `p_link`, one call creates both tick and idle), silently failing to
# ever link either process onto the dispatcher ready list (`drl`) --
# which is why `rlr` read back `0000h` (an empty ready list) at the
# very first `switch:` this tree ever reached with no process besides
# the caller yielding. Fixed the same way as `pul`/`mdul`/`qlr`/`qul`/
# `qmau` above: patched in at build time, standing in for GENSYS.
# `0xFFFF` (the largest representable 16-bit paragraph address, just
# under the 1MB real-mode ceiling) is deliberately generous -- neither
# check this guards is really meant to be the *binding* constraint
# (the second, tighter "within 64K of sysdat" check always is), so
# there's no need to compute a tight, true end-of-RAM value here.
ENDSEG_FIELD_OFFSET = 0x44

# `flags` (SYSTEM.DAT: "dw 0 ;addr. Flag Table") is the base pointer to
# the logical-interrupt-flag table (`FLAG.RTM`'s own header comment:
# "GENSYS initializes the flag table with 0ffh's") -- another field
# GENSYS computed and backfilled in the original build that this
# tree's `tools/build_mpmsys.py` never patched, same class of gap as
# `pul`/`mdul`/`qlr`/`qul`/`qmau`/`endseg` above. With `flags` stuck at
# its static source value (0), `flag_valid`'s own `mov si,flags ! add
# si,ax` (FLAG.RTM) resolves to whatever real SYSDAT field happens to
# sit at that small byte offset (not physical address 0 like the
# `qmau`/`m_start` bug -- `flags` is read bare, i.e. DS-relative, and
# DS is sysdat here -- but garbage all the same: it's some *other*
# unrelated field, reinterpreted as flag-table entries). Confirmed via
# a serial-trace probe: `TICK.SUP`'s own `tick:` loop (`mov dx,
# flag_tick ! mov cx,f_flagwait ! int mpmint`, meant to block until the
# next real timer tick) looped at a wildly higher rate than 60Hz with
# no throttling at all -- `flag_wait_entry` (`FLAG.RTM`) reads back
# neither `flag_on` (0FFFEh) nor `flag_off` (0FFFFh) from the garbage
# table entry, takes its `fw_noff: mov cx,e_flag_underrun` error exit
# instead of ever calling `dsptch` to actually block, and `TICK.SUP`
# doesn't check that return code at all, so it just spins straight
# back round to `tick:` and tries again immediately, forever, burning
# CPU and flooding the serial trace with a probe-dump for every single
# spin. Fixed the same way as the others: a real, zero-initialized-
# then-flag_off-filled table is carved out of SYSDAT's own slack space
# (right after the `qmau` buffer) and `flags` is pointed at it.
FLAGS_FIELD_OFFSET = 0x56
NFLAGS = 0x20          # SYSTEM.DAT's own static "nflags db 20h" -- the
                       # table's entry *count* was always correctly
                       # declared; only the base pointer and the
                       # entries' own contents were never initialized.
FLGLEN = 3             # flg_pd (word, FLG.DEF) + flg_ignore (byte)

# `rspseg` (SYSTEM.DAT: "dw 0 ;segment of first RSP") is the last
# GENSYS-era gap: `INIT.SUP`'s own `nrsp:` loop already walks a chain
# of RSPs starting here (`cl,f_createproc` once per entry, chained via
# each RSP's own header `LINK` field doubling as "next RSP segment"
# before `INIT.SUP` overwrites it with sysdat's own segment, as the
# real MP/M-86 Programmer's Guide describes) and was never exercised
# because nothing ever set it -- `src/rsp/TMP.A86` (the real MP/M-86
# Terminal Message Process, i.e. its console command-line shell; see
# docs/bugs.md #2, now fixed) exists in this tree already but was
# never wired up to anything (confirmed: `tools/build_disk.py`'s own
# comments -- "no CCP/process loader exists anywhere in this tree yet
# to actually read a .CMD/.RSP file and run it"). Baked in here the
# same way XIOS86Q's private dseg tail is: TMP is an "8080 Model" RSP
# (its own header comment; the MP/M-86 Programmer's Guide section 5.2
# describes this as CS=DS=ES=SS all the same segment), so its code and
# data are appended, back to back, onto this SAME combined `cseg` blob
# (right after the real kernel modules') rather than loaded as a
# separate segment -- `tmp_data_seg` is computed as an offset purely
# *within* that same blob, exactly the way `MPM_CODE_SEG`+`module_base`
# already works for SUP/RTM/MEM/CIO/BDOS/XIOS.
RSPSEG_FIELD_OFFSET = 0x42
QD_POOL_SLOT_SIZE = 0x1c  # qdlen (QD.DEF) -- first word of each slot is
                          # its own q_link, matching getqdaddr's own
                          # free-list convention (QUE2.RTM).
QD_POOL_SLOTS = 4

# `qmau` (SYSDAT.FMT offset 0x60, "MAU for queue buffer") is a raw MD
# {link,start,length,plist} struct, same "never initialized, stale
# DATA.BDO template" bug as pul/mdul/qlr/qul above: its `m_start` is
# 0, i.e MAU.MEM's own `maualloc_entry` ends up doing `mov
# ds,m_start[bx]` -> `mov ds,0`, then walking/allocating-from (and, on
# success, incrementing an alloc count *into*) whatever happens to sit
# at absolute segment 0 -- the real-mode interrupt vector table --
# confirmed by a probe dump: completely unrelated SYSDAT fields read
# as corrupted immediately after the first `f_maualloc` call this tree
# ever makes (BDOSIF.BDO's own "create disk mx queue" asking for its
# message buffer). Unlike pul/mdul/qlr/qul (small free-list heads
# living entirely within SYSDAT's own segment), `m_start` is read as a
# real, independent *segment value* -- so this needs actual backing
# memory with its own paragraph-aligned address, not just a SYSDAT-
# relative offset. Fixed by carving `QMAU_BUF_SIZE` bytes out of
# SYSDAT's own slack space (right after the QD pool, paragraph-
# aligned) and pointing `qmau` at it with a from-scratch, minimal
# Suballocation Table (MAU.MEM's own header comment on SAT0/SAT1
# describes the format) -- one real entry covering the whole buffer
# past the table's own 10 bytes, marked free (`nall`=0).
QMAU_FIELD_OFFSET = 0x60
QMAU_BUF_SIZE = 0x200  # matches the stale template's own m_length
QMAU_SAT_ENTRIES = 10  # generous capacity -- see the fix's own comment
                       # on why this must exceed the real entry count

# Every module (SUP/RTM/MEM/CIO/BDOS/XIOS) dispatches its own "entry:"
# function calls through a local jump table ("function"/"functab"),
# built by each module's own standalone `asm86` run as a `dw label`
# (or, for BDOS, `dw label ! db flag`) array -- near-call targets,
# computed relative to offset 0 (that module's own assembly-time
# origin), NOT relative to wherever this script ends up concatenating
# that module's code within the combined cseg. SUP happens to load at
# offset 0 (the first module here), so its own table's entries are
# already correct by coincidence; every other module's table entries
# are wrong by exactly that module's own `module_base` once
# concatenated -- confirmed with a QEMU hardware breakpoint set at
# RTM's real (combined-cseg) `makeq_entry` address: it never fired
# when BDOS's init created its "mxdiskqd" queue (`f_qmake`, dispatched
# correctly all the way through SUP's own module table into RTM's
# `entry:` -- confirmed with a breakpoint *there*, which did fire,
# with CX holding the right local function number, 4) -- and a direct
# memory read of RTM's own `function[8]` (the jump-table slot for
# function 4) showed it still held `0x0834`, `makeq_entry`'s offset
# from RTM's own standalone assembly, not `0x1d40`, where
# `makeq_entry` really lives once RTM is concatenated at its own
# `module_base`. The resulting wild near-call (jumping into whatever
# code/data happens to sit at that same raw offset within *SUP's* own
# module, since every module shares the one `MPM_CODE_SEG`) is almost
# certainly why RTM's own queue functions, and likely every other
# module's own `entry:`-dispatched functions, silently hang somewhere
# unrelated the first time anything actually calls through them --
# every module-*init* call (through the already-correctly-patched
# module table) worked fine up to now because none of them are
# dispatched through a module's own *internal* function table at all.
#
# Fixed by patching every (non-SUP) module's own table entries here,
# right after concatenating that module's code, adding its own
# `module_base` to each one -- entry counts/strides below were
# counted directly from each module's own source (BDOS's own table
# entries are 3 bytes: a 2-byte offset plus a 1-byte flag that must be
# left alone).
FUNCTION_TABLES = {
    'RTM': ('function', 21, 2),
    'MEM': ('functab', 13, 2),
    'CIO': ('functab', 16, 2),
    'BDOS': ('functab', 47, 3),
    'XIOS': ('functab', 25, 2),
}

# The opcode+modrm bytes (everything up to the table-offset displacement)
# of each module's own "entry:" dispatcher instruction that locates its
# FUNCTION_TABLES table -- see that relocation's own comment just above
# where this is used. RTM/MEM/CIO: `call word ptr cs:[si+table]`. BDOS:
# `mov si,offset functab` (an immediate load, not a memory operand, but
# the same bare un-relocated value). XIOS: `call word ptr cs:[bx+table]`.
DISPATCH_PATCHES = {
    'RTM': b'\x2e\xff\x94',
    'MEM': b'\x2e\xff\x94',
    'CIO': b'\x2e\xff\x94',
    'BDOS': b'\xc7\xc6',
    'XIOS': b'\x2e\xff\x97',
}

SYSDAT_DSEG_SIZE = 4096  # see XIOS86Q.A86's own comment on this number

SYM_RE = re.compile(r'^\s*(init|entry)\s+label\s+([0-9A-Fa-f]+)h', re.MULTILINE)
ANY_SYM_RE = re.compile(r'^\s*(\S+)\s+(?:label|equ)\s+([0-9A-Fa-f]+)h', re.MULTILINE)


def get_symbol(src_stem, name):
    """Look up one arbitrary symbol's value via `asm86 --symbols` -- used
    to place the QD pool (see QUL_FIELD_OFFSET's own comment) right after
    SYSDAT.FMT's own last declared field (`mxmemcount`), instead of
    hardcoding that boundary, since it moves whenever SYSDAT.FMT does."""
    src = os.path.join(KERNEL_DIR, src_stem + '.A86')
    result = subprocess.run(ASM86 + [src, '--symbols'],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"asm86 --symbols failed for {src_stem}.A86")
    for sym_name, value in ANY_SYM_RE.findall(result.stdout):
        if sym_name.lower() == name.lower():
            return int(value, 16)
    raise SystemExit(f"{src_stem}.A86: could not find symbol {name!r}")


def run_asm86(src_stem, out_dir, extra_args=()):
    src = os.path.join(KERNEL_DIR, src_stem + '.A86')
    out_base = os.path.join(out_dir, src_stem)
    result = subprocess.run(ASM86 + [src, '-o', out_base, *extra_args],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"asm86 failed assembling {src_stem}.A86")
    return out_base


def get_init_entry_offsets(src_stem):
    src = os.path.join(KERNEL_DIR, src_stem + '.A86')
    result = subprocess.run(ASM86 + [src, '--symbols'],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"asm86 --symbols failed for {src_stem}.A86")
    offsets = {}
    for m in SYM_RE.finditer(result.stdout):
        offsets[m.group(1)] = int(m.group(2), 16)
    if 'init' not in offsets or 'entry' not in offsets:
        raise SystemExit(f"{src_stem}.A86: could not find init/entry symbols")
    return offsets['init'], offsets['entry']


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('-o', '--output', default=None,
                     help='output path for the combined image '
                          '(default: build/kernel/MPM.bin)')
    args = ap.parse_args()

    out_dir = os.path.join(ROOT, 'build', 'kernel')
    os.makedirs(out_dir, exist_ok=True)

    sysdat_base = run_asm86('SYSDAT', out_dir)
    with open(sysdat_base + '.dseg.bin', 'rb') as f:
        dseg = bytearray(f.read())
    if len(dseg) != SYSDAT_DSEG_SIZE:
        raise SystemExit(f"SYSDAT.A86's dseg.bin is {len(dseg)} bytes, "
                          f"expected {SYSDAT_DSEG_SIZE} -- the hard-coded "
                          "offsets in this script are no longer valid; "
                          "see its own comments")

    cseg = bytearray()
    module_offsets = {}
    for name, stem, field_offset in MODULES:
        base = run_asm86(stem, out_dir)
        with open(base + '.cseg.bin', 'rb') as f:
            code = bytearray(f.read())
        module_base = len(cseg)

        if name == 'RTM':
            # RTMIF.RTM's own init: writes two more near-pointers
            # (`dispatcher`/`rtm_pdisp`, SYSDAT fields read by XIOS86Q's
            # tick ISR and by other modules' intermodule calls) using
            # `mov word ptr[bx],offset fdisp`/`offset farpdisp` -- same
            # unrelocated-immediate bug as FUNCTION_TABLES above, just
            # baked into a MOV instruction's own operand bytes instead
            # of a jump table: `offset fdisp` is fdisp's *standalone*
            # RTM.A86 offset, assembled with no knowledge of this
            # module's final module_base, so without this the real
            # runtime write ends up (fdisp's bare offset, cs) instead
            # of (module_base + fdisp's offset, cs) -- a wild far
            # pointer landing wherever that bare offset falls inside
            # the *combined* cseg (typically inside SUP, since
            # module_base is always larger than these small standalone
            # offsets), fired the moment anything actually calls
            # through `dispatcher`/`rtm_pdisp` (a timer tick, or any
            # other module's own intermodule call).
            for sym in ('fdisp', 'farpdisp'):
                off = get_symbol(stem, sym)
                needle = b'\xc7\x07' + struct.pack('<H', off)
                idx = code.find(needle)
                if idx == -1:
                    raise SystemExit(
                        f"RTM.A86: couldn't find the "
                        f"'mov word ptr[bx],offset {sym}' immediate "
                        f"to relocate (expected {needle.hex()})")
                if code.find(needle, idx + 1) != -1:
                    raise SystemExit(
                        f"RTM.A86: '{sym}' immediate pattern isn't "
                        "unique -- relocation would be ambiguous")
                struct.pack_into('<H', code, idx + 2,
                                  (module_base + off) & 0xFFFF)

        cseg += code
        init_off, entry_off = get_init_entry_offsets(stem)
        module_offsets[name] = module_base
        print(f"build_mpmsys: {name:5s} code @ {module_base:#06x} "
              f"({len(code)} bytes), init={init_off:#x} entry={entry_off:#x}")
        if field_offset is not None:
            # Field layout (SYSDAT.FMT): entry far ptr (offset,segment),
            # then init far ptr (offset,segment) -- same segment
            # (MPM_CODE_SEG) for every module here, since they all
            # share the one combined code segment.
            struct.pack_into('<HH', dseg, field_offset,
                              (module_base + entry_off) & 0xFFFF, MPM_CODE_SEG)
            struct.pack_into('<HH', dseg, field_offset + 4,
                              (module_base + init_off) & 0xFFFF, MPM_CODE_SEG)

        # relocate this module's own internal function-dispatch table
        # -- see FUNCTION_TABLES' own comment for why this is needed.
        table_info = FUNCTION_TABLES.get(name)
        if table_info is not None:
            table_sym, entry_count, stride = table_info
            table_off = get_symbol(stem, table_sym)
            for i in range(entry_count):
                entry_addr = module_base + table_off + i * stride
                target = struct.unpack_from('<H', cseg, entry_addr)[0]
                struct.pack_into('<H', cseg, entry_addr,
                                  (target + module_base) & 0xFFFF)

            # The table's own CONTENTS are now correctly relocated above,
            # but the "entry:" dispatcher's own instruction that *locates*
            # the table (`call cs:TABLE[si]`, or BDOS's `mov si,offset
            # TABLE`) bakes TABLE's bare *standalone* offset directly into
            # its own operand bytes -- same unrelocated-immediate bug as
            # everywhere else in FUNCTION_TABLES/the dispatcher fix above,
            # just encoded as part of a CALL/MOV instruction's own
            # displacement instead of a jump-table entry or another MOV's
            # immediate. Left unfixed, EVERY module but SUP ends up
            # dispatching through CS:[bare table_off] -- i.e. through
            # whatever unrelated bytes happen to sit at that same small
            # offset inside *SUP* (since module_base is 0 there) -- which
            # is why no non-SUP module's own internal dispatch has ever
            # worked even with the table contents already fixed above.
            dispatch_prefix = DISPATCH_PATCHES[name]
            needle = dispatch_prefix + struct.pack('<H', table_off)
            idx = cseg.find(needle, module_base, module_base + len(code))
            if idx == -1:
                raise SystemExit(
                    f"{stem}.A86: couldn't find the dispatcher's own "
                    f"'{table_sym}'-locating instruction to relocate "
                    f"(expected {needle.hex()} somewhere in this module)")
            if cseg.find(needle, idx + 1, module_base + len(code)) != -1:
                raise SystemExit(
                    f"{stem}.A86: '{table_sym}' dispatch-instruction "
                    "pattern isn't unique -- relocation would be ambiguous")
            struct.pack_into('<H', cseg, idx + len(dispatch_prefix),
                              (module_base + table_off) & 0xFFFF)

        if name == 'RTM':
            # DSPTCH.RTM's own `dsp_table` (the process-status-code ->
            # handler jump table `dsptch` uses to decide what to do
            # with the process it just took off the Ready list) is a
            # *second*, completely separate internal jump table in
            # this same module -- not covered by FUNCTION_TABLES/
            # DISPATCH_PATCHES above, which only ever knew about
            # RTM's "function" (its *external*, intermodule entry
            # table). Same bug, same fix, on both the table's own
            # entries and the `jmp cs:dsp_table[bx]` instruction's own
            # embedded (unrelocated) displacement -- found only once
            # something actually exercised RTM's scheduler itself
            # (`poll_entry`'s `jmp dsptch`) for the first time this
            # whole tree's ever managed to.
            dsp_table_off = get_symbol(stem, 'dsp_table')
            dsp_table_entries = 9
            for i in range(dsp_table_entries):
                entry_addr = module_base + dsp_table_off + i * 2
                target = struct.unpack_from('<H', cseg, entry_addr)[0]
                struct.pack_into('<H', cseg, entry_addr,
                                  (target + module_base) & 0xFFFF)
            needle = b'\x2e\xff\xa7' + struct.pack('<H', dsp_table_off)
            idx = cseg.find(needle, module_base, module_base + len(code))
            if idx == -1:
                raise SystemExit(
                    "RTM.A86: couldn't find the 'jmp cs:dsp_table[bx]' "
                    f"instruction to relocate (expected {needle.hex()})")
            if cseg.find(needle, idx + 1, module_base + len(code)) != -1:
                raise SystemExit(
                    "RTM.A86: 'dsp_table' dispatch-instruction pattern "
                    "isn't unique -- relocation would be ambiguous")
            struct.pack_into('<H', cseg, idx + 3,
                              (module_base + dsp_table_off) & 0xFFFF)

    # "dispatcher": no dedicated label found anywhere for this -- see
    # KNOWN LIMITATIONS above. Points at RTM's own general entry point.
    rtm_base = module_offsets['RTM']
    rtm_init, rtm_entry = get_init_entry_offsets('RTM')
    struct.pack_into('<HH', dseg, DISPATCHER_FIELD_OFFSET,
                      (rtm_base + rtm_entry) & 0xFFFF, MPM_CODE_SEG)

    # module_map: which modules are present (MODFUNC.DEF's *mod_bit
    # values) -- every module built here except netmod.
    dseg[MODULE_MAP_OFFSET] = 0x3F  # sup|rtm|mem|bdos|cio|xios, not net

    # A few "answers to GENSYS questions" fields that matter for
    # XIOS86Q's own specific configuration (one console, no list
    # device, 60 ticks/sec) -- see KNOWN LIMITATIONS for what's *not*
    # set here.
    dseg[NCONDEV_OFFSET] = 1
    dseg[NLSTDEV_OFFSET] = 0
    dseg[NCIODEV_OFFSET] = 1
    dseg[TICKSPERSEC_OFFSET] = 60

    # srchdisk ("default search disk", SYSDAT.FMT) is also the disk
    # TMP.A86's own init code reads (as "s_sysdisk") to pick its initial
    # default drive -- SYSTEM.DAT's stock value (1 = drive B:) was
    # written for the original multi-drive Intel SBC target. This
    # single-floppy QEMU build's XIOS86Q.A86 dtrntbl only has drive 0
    # (A:) wired up, so leaving srchdisk=1 makes TMP's very first
    # setdisk() call select a nonexistent drive: BDOS's selectdisk
    # silently no-ops (seldskf returns bx=0) without ever filling in
    # sectpt/the rest of the DPB copy, and the first real file read's
    # "DIV SECTPT" then faults on a divisor of 0 with no handler
    # installed, hanging forever. Patch it to drive A: to match this
    # build's actual disk configuration.
    dseg[SRCHDISK_OFFSET] = 0

    # mdul/qlr: reset every stale "unused list"/"queue list root" head
    # to empty -- see PUL_FIELD_OFFSET's own comment for why their
    # inherited DATA.BDO values are actively wrong for a fresh boot, not
    # just unused. `pul` itself is linked to a real PD pool further
    # down (see PUL_FIELD_OFFSET's own comment) instead of just zeroed.
    struct.pack_into('<H', dseg, MDUL_FIELD_OFFSET, 0)
    struct.pack_into('<H', dseg, QLR_FIELD_OFFSET, 0)

    # qul: link a real pool of free QD slots instead -- see its own
    # comment above for why zeroing it (like the others) isn't enough.
    # Placed right after SYSDAT.FMT's own last declared field so it
    # can't collide with anything a real field offset ever resolves
    # to; chained as a standard singly-linked free list (each slot's
    # first word, its own q_link, points to the next slot; the last
    # points to 0), matching exactly what getqdaddr's gqd_have path
    # (QUE2.RTM) already expects to find there.
    qd_pool_start = get_symbol('RTM', 'mxmemcount') + 1
    qd_pool_end = qd_pool_start + QD_POOL_SLOTS * QD_POOL_SLOT_SIZE
    if qd_pool_end > SYSDAT_DSEG_SIZE:
        raise SystemExit(
            f"QD pool (offset {qd_pool_start:#x}..{qd_pool_end:#x}) no "
            f"longer fits in SYSDAT's {SYSDAT_DSEG_SIZE}-byte dseg -- "
            "SYSDAT.FMT grew too close to the end")
    for slot in range(QD_POOL_SLOTS):
        slot_off = qd_pool_start + slot * QD_POOL_SLOT_SIZE
        next_off = (slot_off + QD_POOL_SLOT_SIZE
                    if slot < QD_POOL_SLOTS - 1 else 0)
        struct.pack_into('<H', dseg, slot_off, next_off)

    # qmau: point it at a real, paragraph-aligned backing buffer with a
    # from-scratch Suballocation Table instead of the stale (m_start=0)
    # template -- see QMAU_FIELD_OFFSET's own comment for why. Placed
    # right after the QD pool, rounded up to the next paragraph so
    # `m_start`'s segment:0 lands exactly on this buffer's first byte.
    # `sysdat_segment` is MPM_CODE_SEG plus however many paragraphs the
    # combined cseg takes up -- the same arithmetic build_disk.py's own
    # `build_two_segment_body` uses to place SYSDAT right after the
    # code on disk, so this always agrees with where the loader will
    # actually put it at runtime.
    #
    # SAT0's `ents` is NOT "how many real entries exist" -- it's the
    # table's own *capacity*. `maualloc_entry`'s own scan loop
    # (QUE1.RTM... er, MAU.MEM's `maua_nsat`) stops at the first
    # zero-`sat_start` entry regardless of `ents`, and `mau_insert`
    # (used whenever an allocation needs to *split* a free entry into
    # an allocated piece + a smaller leftover-free piece) only
    # succeeds if the entry at offset `ents*satlen` is still all-zero
    # -- i.e. `ents` must reserve real *slack* beyond however many
    # entries are actually filled in, or every single allocation that
    # doesn't consume an entry's *entire* remaining length fails outright
    # (confirmed directly: `mau_insert` returns CX=0FFFFh otherwise,
    # which `mau_split` treats as an error). `QMAU_SAT_ENTRIES` reserves
    # generous room for that; only the first slot starts non-zero (one
    # real entry covering the whole buffer past the table itself).
    # Bake in TMP (see RSPSEG_FIELD_OFFSET's own comment for what/why)
    # -- assembled standalone since it lives in src/rsp/, not src/
    # kernel/ (run_asm86 hard-codes KERNEL_DIR), and its code+data
    # appended onto `cseg` *before* `sysdat_segment` is computed below,
    # not after: `sysdat_segment`'s own formula (and build_disk.py's
    # independent recomputation of the same value, per its comment
    # just above) are both driven entirely by `len(cseg)` at the point
    # each one runs, so TMP's bytes have to already be counted by the
    # time either one adds up `cseg`'s length, or the two computations
    # would disagree about where SYSDAT actually loads -- confirmed
    # the hard way: appending TMP *after* this line (tried first, then
    # reverted) left `sysdat_segment` here looking only at the
    # original kernel-only `cseg`, while build_disk.py's own
    # `build_two_segment_body`, re-deriving the same value from the
    # *larger*, TMP-inclusive `MPM.bin.cseg` file it reads straight off
    # disk, placed SYSDAT somewhere else entirely -- the real kernel
    # code ran with every `module_table`/`qmau`/etc. pointer correct
    # for one segment while the loader actually put SYSDAT at another,
    # corrupting everything within the first few dispatches.
    tmp_src = os.path.join(ROOT, 'src', 'rsp', 'TMP.A86')
    tmp_out = os.path.join(out_dir, 'TMP')
    result = subprocess.run(ASM86 + [tmp_src, '-o', tmp_out],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit("asm86 failed assembling src/rsp/TMP.A86")
    with open(tmp_out + '.cseg.bin', 'rb') as f:
        tmp_cseg = bytearray(f.read())
    with open(tmp_out + '.dseg.bin', 'rb') as f:
        tmp_dseg = bytearray(f.read())

    # Pad `cseg` itself up to the next paragraph boundary *before*
    # appending TMP -- `tmp_code_seg` below rounds `len(cseg)` UP to
    # compute TMP's own segment, but without this padding the actual
    # `cseg += tmp_cseg` a few lines down still appends at the
    # *unrounded* current length, leaving every byte of TMP's own
    # code+data physically offset from where `tmp_code_seg` (and every
    # address computed relative to it -- "offset tmp" in TMP.A86's own
    # dseg, the two build-time patches just below, etc.) claims it
    # starts. Confirmed directly this session: the combined kernel
    # cseg's real length (29023 bytes) isn't itself a multiple of 16,
    # a 1-byte gap that silently shifted every single address inside
    # TMP by one byte.
    cseg += b'\0' * ((-len(cseg)) % 16)
    tmp_code_seg = MPM_CODE_SEG + len(cseg) // 16
    tmp_cseg_paras = (len(tmp_cseg) + 15) // 16
    tmp_cseg = tmp_cseg.ljust(tmp_cseg_paras * 16, b'\0')
    tmp_data_seg = tmp_code_seg + tmp_cseg_paras

    # Patch TMP's own dseg: `stack_top`'s 2nd word (its "code seg" slot,
    # GENSYS-filled in the real build -- TMP.A86's own template leaves
    # it as the placeholder `unknown` equ, 0) and `u_initcs` (UDA-
    # relative offset 50h, confirmed via --symbols against a real
    # kernel file that includes UDA.FMT) both need the real code
    # segment -- "Small Model" semantics (MP/M-86 Programmer's Guide
    # 5.3.2): CS set explicitly, DS/ES/SS left 0 so PROC.RTM's own
    # `cp_uda` auto-fills them from `ax`=`u_wrkseg` (which `INIT.SUP`'s
    # `nrsp:` loop already sets to `rspseg`, i.e. `tmp_data_seg`, via
    # `mov ds,cx` right before `int mpmint`).
    struct.pack_into('<H', tmp_dseg, 0x1DA, tmp_code_seg)  # stack_top+2
    struct.pack_into('<H', tmp_dseg, 0x90, tmp_code_seg)   # u_initcs
    tmp_dseg_paras = (len(tmp_dseg) + 15) // 16
    tmp_dseg = tmp_dseg.ljust(tmp_dseg_paras * 16, b'\0')

    cseg += tmp_cseg
    cseg += tmp_dseg
    print(f"build_mpmsys: TMP   code @ {tmp_code_seg:#06x}, "
          f"data @ {tmp_data_seg:#06x} ({len(tmp_cseg)}+{len(tmp_dseg)} "
          "bytes)")

    sysdat_segment = MPM_CODE_SEG + (len(cseg) + 15) // 16
    qmau_buf_off = (qd_pool_end + 15) & ~0xF
    qmau_buf_end = qmau_buf_off + QMAU_BUF_SIZE
    if qmau_buf_end > SYSDAT_DSEG_SIZE:
        raise SystemExit(
            f"qmau buffer (offset {qmau_buf_off:#x}..{qmau_buf_end:#x}) "
            f"no longer fits in SYSDAT's {SYSDAT_DSEG_SIZE}-byte dseg -- "
            "SYSDAT.FMT grew too close to the end")
    satlen = 5  # sat_start(2) + sat_length(2) + sat_nall(1)
    sat_table_len = satlen * (QMAU_SAT_ENTRIES + 1)  # +1 for SAT0 itself
    for i in range(sat_table_len):
        dseg[qmau_buf_off + i] = 0  # whole table starts zeroed/unused
    dseg[qmau_buf_off] = QMAU_SAT_ENTRIES  # SAT0.ents = capacity
    struct.pack_into('<HHB', dseg, qmau_buf_off + satlen,
                      sat_table_len,                   # SAT1.start
                      QMAU_BUF_SIZE - sat_table_len,    # SAT1.length
                      0)                                # SAT1.nall (free)
    struct.pack_into('<HHHH', dseg, QMAU_FIELD_OFFSET,
                      0, sysdat_segment + (qmau_buf_off >> 4),
                      QMAU_BUF_SIZE, 0)
    struct.pack_into('<H', dseg, QUL_FIELD_OFFSET, qd_pool_start)
    struct.pack_into('<H', dseg, ENDSEG_FIELD_OFFSET, 0xFFFF)
    struct.pack_into('<H', dseg, RSPSEG_FIELD_OFFSET, tmp_data_seg)

    # flag table: real, zero-initialized-then-filled entries right
    # after the qmau buffer -- see FLAGS_FIELD_OFFSET's own comment.
    flagtab_start = qmau_buf_end
    flagtab_end = flagtab_start + NFLAGS * FLGLEN
    if flagtab_end > SYSDAT_DSEG_SIZE:
        raise SystemExit(
            f"flag table (offset {flagtab_start:#x}..{flagtab_end:#x}) "
            f"no longer fits in SYSDAT's {SYSDAT_DSEG_SIZE}-byte dseg -- "
            "SYSDAT.FMT grew too close to the end")
    for i in range(NFLAGS):
        off = flagtab_start + i * FLGLEN
        struct.pack_into('<HB', dseg, off, 0xFFFF, 0xFF)  # flag_off, flag_zig
    struct.pack_into('<H', dseg, FLAGS_FIELD_OFFSET, flagtab_start)

    # PD pool: real free PD slots right after the flag table -- see
    # PUL_FIELD_OFFSET's own comment for why zeroing `pul` alone isn't
    # enough.
    pd_pool_start = flagtab_end
    pd_pool_end = pd_pool_start + PD_POOL_SLOTS * PD_POOL_SLOT_SIZE
    if pd_pool_end > SYSDAT_DSEG_SIZE:
        raise SystemExit(
            f"PD pool (offset {pd_pool_start:#x}..{pd_pool_end:#x}) no "
            f"longer fits in SYSDAT's {SYSDAT_DSEG_SIZE}-byte dseg -- "
            "SYSDAT.FMT grew too close to the end")
    for slot in range(PD_POOL_SLOTS):
        slot_off = pd_pool_start + slot * PD_POOL_SLOT_SIZE
        next_off = (slot_off + PD_POOL_SLOT_SIZE
                    if slot < PD_POOL_SLOTS - 1 else 0)
        struct.pack_into('<H', dseg, slot_off, next_off)  # p_link
    struct.pack_into('<H', dseg, PUL_FIELD_OFFSET, pd_pool_start)

    # XIOS86Q's own private tail: the bytes from SYSDAT_DSEG_SIZE
    # onward in its own dseg.bin (see its own comment on why its
    # private data starts exactly there).
    xios_base = run_asm86('XIOS86Q', out_dir)
    with open(xios_base + '.dseg.bin', 'rb') as f:
        xios_dseg = f.read()
    if len(xios_dseg) < SYSDAT_DSEG_SIZE:
        raise SystemExit("XIOS86Q.A86's dseg.bin is smaller than the "
                          "shared system data area -- its own 'org "
                          f"{SYSDAT_DSEG_SIZE}' must not have taken effect")
    dseg += xios_dseg[SYSDAT_DSEG_SIZE:]

    out_path = args.output or os.path.join(out_dir, 'MPM.bin')
    with open(out_path + '.cseg', 'wb') as f:
        f.write(cseg)
    with open(out_path + '.dseg', 'wb') as f:
        f.write(dseg)
    print(f"build_mpmsys: wrote {out_path}.cseg ({len(cseg)} bytes), "
          f"{out_path}.dseg ({len(dseg)} bytes)")


if __name__ == '__main__':
    main()
