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
MPMSEG_OFFSET = 0x40
NCCB_OFFSET = 0x49
NCNS_OFFSET = 0x47        # GENSYS's "# system console devices"
NLST_OFFSET = 0x48        # GENSYS's "# system list devices"
CCB_OFFSET = 0x54

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
PD_POOL_SLOTS = 8          # each baked-in RSP (TMP, CLOCK, ECHO, MPMSTAT)
                           # takes one (their PDs are copied out of their
                           # own data segments); the rest are for programs

# The system Lock List: "FREE_ROOT, LOCK_MAX, OPEN_MAX INITIALIZED BY
# SYSGEN" (DATA.BDO). Every file a process opens and every record it
# locks takes one 10-byte item off `free_root` (FILE3.BDO's
# CREATE_ITEM/SET_OLIST_ITEM: link, drive|attributes, 5 bytes, a word);
# left at the template's 0, the first open of anything fails with "No
# Room in System Lock List" (docs/bugs.md #113). GENSYS asks for the
# list size and the per-process open-file and locked-record limits.
FREE_ROOT_OFFSET = 0x52
SYSLREC_OFFSET = 0x86     # total locked records
SYSLFIL_OFFSET = 0x88     # total open files
LOCK_MAX_OFFSET = 0x8A    # locked records per process
OPEN_MAX_OFFSET = 0x8C    # open files per process
SYSLTOT_OFFSET = 0x8E     # total Lock List items
LOCK_ITEM_SIZE = 10
LOCK_ITEMS = 16

# Memory: `mdul` (unused Memory Descriptors) and `mfl` (Memory Free
# List) are GENSYS's too -- left empty, every f_malloc (a program load
# first of all) fails with e_no_memory (docs/bugs.md #113). A pool of
# MDs goes on mdul, and one MD on mfl describes the free memory: from
# the first paragraph after the system data area up to FREE_MEM_END
# (below the EBDA; MPMLDR, at 0800h, is dead by then). SYSDAT's own
# 4096 bytes are full, so the pool follows the XIOS's dseg tail.
#
# MP/M-86 2.0's memory model is fixed partitions, which GENSYS asks
# for: ML.MEM's `mlalloc` builds a process's MAU out of whole MFL
# entries (combining contiguous ones when a request needs more), and
# the MAU stays that process's until it frees everything in it. With
# all of free memory as one MFL entry, the first program to stay
# resident -- SPOOL, once it detaches -- owned all of it and every
# later load failed with "Not Enough Memory" (docs/bugs.md #119). So
# free memory is cut into PARTITION_SIZE-paragraph partitions, the
# last taking the remainder. Each partition is an MD on mfl; the MD
# pool holds those plus MD_SPARE for p_mem lists and multi-partition
# MAUs.
MFL_FIELD_OFFSET = 0x5A
MD_LEN = 10                # mdlen (MEM.DEF)
MD_SPARE = 40
PARTITION_SIZE = 0x400     # paragraphs: 16K
FREE_MEM_END = 0x9000

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
# separate segment -- each RSP's segments are computed purely *within*
# that same blob, the way each kernel module's own segment is (see
# BAKED_RSPS: TMP, then CLOCK).
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

# Each module (SUP/RTM/MEM/CIO/BDOS/XIOS) is assembled on its own, at
# origin 0, and gets its own code segment here (paragraph-aligned within
# the combined cseg), so its jump tables and code-label immediates need
# no relocation. An earlier version put every module in MPM_CODE_SEG at
# a byte offset and patched each table and immediate it knew about,
# which missed some (docs/bugs.md #112).
SYSDAT_DSEG_SIZE = 4096  # see XIOS86Q.A86's own comment on this number

# RSPs baked into the image: (name, its initial-stack-frame label,
# source). An .A86 source is src/rsp/<name>.A86; a .PLM one is compiled
# by plmc as an RSP (its RSPASM.A86-style shell, PD named <name>) plus
# the mon1-4 stub. MPMSTAT is DRI's status RSP (STSRSP.PLM, sharing
# STSCOM.PLM with the transient STSCMD): typing MPMSTAT queues the
# command to it, as ECHO does.
#
# A .RSP source is a DRI-built binary RSP (GENCMD output: a 128-byte
# header, then the code and data groups). Its start frame and UDA are
# read from the image (stack label None). CACHE.RSP (orig/lastdri/22,
# no source anywhere in orig/) is a write-through directory cache: its
# init code hooks SYSDAT's XIOS entry (xiosmod, 28h) and terminates,
# leaving a handler that serves SELDSK/HOME/READ/WRITE for records
# 0-15 (the 64-entry directory) of drives A and B from its own data
# segment -- the BDOS passes the absolute record in CH:DX
# (FILE1.BDO's PASS_ARECORD), as the cache expects.
BAKED_RSPS = [('TMP', 'stack_top', None), ('CLOCK', 'stack_tos', None), ('ECHO', 'stack_tos', None),
              ('MPMSTAT', 'rsp_stack_top', os.path.join('src', 'utils', 'STSRSP.PLM')),
              ('CACHE', None, os.path.join('src', 'rsp', 'CACHE.RSP'))]
PLMC = [sys.executable, os.path.join(ROOT, 'tools', 'plmc.py')]
PLM_STUB = os.path.join(ROOT, 'tools', 'plm', 'runtime_stub.A86')


def binary_rsp(path):
    """(code, data) of a GENCMD-format RSP: group descriptors (form,
    length in paragraphs, base, min, max) in the first record, the
    groups' bytes after it in order."""
    with open(path, 'rb') as f:
        body = f.read()
    groups, pos = {}, 128
    for i in range(8):
        form, paras = struct.unpack_from('<BH', body, i * 9)
        if form == 0:
            break
        groups[form] = bytearray(body[pos:pos + paras * 16])
        pos += paras * 16
    return groups[1], groups[2]


def rsp_source(name, plm_src, out_dir):
    """The .A86 to assemble for a baked-in RSP."""
    if plm_src is None:
        return os.path.join(ROOT, 'src', 'rsp', name + '.A86')
    gen = os.path.join(out_dir, name + '.gen.A86')
    result = subprocess.run(PLMC + [os.path.join(ROOT, plm_src), '--rsp', name, '-o', gen],
                            capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"plmc failed compiling {plm_src} as an RSP")
    combined = os.path.join(out_dir, name + '.rsp.A86')
    with open(gen) as f, open(combined, 'w') as o:
        o.writelines(l for l in f if l.strip().lower() != 'end')
        o.write(open(PLM_STUB).read())
    return combined

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
        # Each module gets its own code segment, starting on a
        # paragraph boundary, so every code offset it was assembled with
        # (jump tables, `offset label` immediates, pushed return
        # addresses) is right as is. Modules reach each other only
        # through far pointers (the module table, `supervisor`,
        # `dispatcher`, the IVT), all built as (offset, cs). This
        # replaces the per-site relocations an earlier version of this
        # script did with every module sharing MPM_CODE_SEG, which
        # missed sites (docs/bugs.md #112).
        cseg += b'\0' * (-len(cseg) % 16)
        module_base = len(cseg)
        module_seg = MPM_CODE_SEG + module_base // 16

        cseg += code
        init_off, entry_off = get_init_entry_offsets(stem)
        module_offsets[name] = module_seg
        print(f"build_mpmsys: {name:5s} code @ {module_base:#06x} seg {module_seg:#06x} "
              f"({len(code)} bytes), init={init_off:#x} entry={entry_off:#x}")
        if field_offset is not None:
            # Field layout (SYSDAT.FMT): entry far ptr (offset,segment),
            # then init far ptr (offset,segment), in the module's own
            # code segment.
            struct.pack_into('<HH', dseg, field_offset, entry_off, module_seg)
            struct.pack_into('<HH', dseg, field_offset + 4, init_off, module_seg)

    # "dispatcher": no dedicated label found anywhere for this -- see
    # KNOWN LIMITATIONS above. Points at RTM's own general entry point.
    rtm_seg = module_offsets['RTM']
    rtm_init, rtm_entry = get_init_entry_offsets('RTM')
    struct.pack_into('<HH', dseg, DISPATCHER_FIELD_OFFSET, rtm_entry, rtm_seg)

    # module_map: which modules are present (MODFUNC.DEF's *mod_bit
    # values) -- every module built here except netmod.
    dseg[MODULE_MAP_OFFSET] = 0x3F  # sup|rtm|mem|bdos|cio|xios, not net

    # A few "answers to GENSYS questions" fields that matter for
    # XIOS86Q's own specific configuration (one console, one list
    # device, 60 ticks/sec) -- see KNOWN LIMITATIONS for what's *not*
    # set here. (INIT.SUP re-reads the device counts from the XIOS's
    # maxconsole/maxlist anyway.)
    dseg[NCONDEV_OFFSET] = 1
    dseg[NLSTDEV_OFFSET] = 1
    dseg[NCIODEV_OFFSET] = 2
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

    # mpmseg ("begin MPM segment", SYSDAT.FMT offset 0x40) -- never set
    # by GENSYS here either (same unconfigured-field bug class as
    # srchdisk/ccb/pul/qul above), but TMP.A86's own init code reads it
    # directly as the *segment* half of the version-string far pointer
    # it prints at startup ("mov ds,sysdatseg ! mov dx,.s_version ! mov
    # ds,.s_mpmseg ! call print_ds_string"): `.s_version` correctly
    # reads SYSDAT's own "version" field (INIT.SUP's `ver` string's
    # *offset*, within the shared code segment), but with `mpmseg` left
    # at its stock 0, `print_ds_string` then reads that offset through
    # DS=0 -- the IVT/BIOS data area -- instead of the real code
    # segment, printing whatever garbage sits there as if it were the
    # version banner. Confirmed live: this is exactly the unreadable
    # text every boot prints immediately after XIOS86Q's own signon,
    # before TMP otherwise runs and idles normally.
    struct.pack_into('<H', dseg, MPMSEG_OFFSET, MPM_CODE_SEG)

    # nccb ("# character control blocks", SYSDAT.FMT) must match the
    # number of CCB-backed devices this build actually has --
    # ncondev+nlstdev = 1+1 = 2, not SYSTEM.DAT's stock 5 (written for
    # the original multi-device target). See CCB_OFFSET's own patch
    # below for why `ccb` (where that/those CCB(s) actually live)
    # matters far more than this count does, but keep them consistent.
    dseg[NCCB_OFFSET] = 2
    # ncns/nlst (the GENSYS answers, beside the XIOS's own ncondev/
    # nlstdev counts) were SYSTEM.DAT's stock 4 and 1. CONSOLE.CIO checks
    # console numbers against ncns, and MPMSTAT showed 4 physical and
    # nccb - (nlst + ncns) = FDh virtual consoles (docs/bugs.md #122).
    dseg[NCNS_OFFSET] = 1
    dseg[NLST_OFFSET] = 1

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
    # SYSDAT's 4096 bytes are SYSTEM.DAT's fields up to `mxmemcount`, then
    # DATA.BDO (from `efcb`: the BDOS's variables, its internal stack and
    # mxdiskqd/mxdiskqpb, up to FBFh). Only the gap between is free --
    # the other pools go past the XIOS's data tail (docs/bugs.md #117).
    data_bdo_start = get_symbol('SYSDAT', 'efcb')
    if qd_pool_end > data_bdo_start:
        raise SystemExit(
            f"QD pool (offset {qd_pool_start:#x}..{qd_pool_end:#x}) runs "
            f"into DATA.BDO (from {data_bdo_start:#x})")
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
    # RSPs baked into the image, in the order INIT.SUP creates them (its
    # `nrsp:` loop follows each RSP's `rsp_link`, the first word of its
    # data segment, starting from SYSDAT's `rspseg`). TMP is the console's
    # command processor; CLOCK advances SYSDAT's time of day, waiting on
    # the seconds flag XIOS86Q's tick ISR sets (docs/bugs.md #117).
    rsp_data_segs = []
    rsp_images = []
    for name, stack_label, rsp_src in BAKED_RSPS:
        if rsp_src is not None and rsp_src.upper().endswith('.RSP'):
            rsp_cseg, rsp_dseg = binary_rsp(os.path.join(ROOT, rsp_src))
            uda = struct.unpack_from('<H', rsp_dseg, 0x10 + 0x10)[0] * 16   # PD's p_uda
            frame = struct.unpack_from('<H', rsp_dseg, uda + 0x34)[0]      # u_sp
            sym = {'uda': uda, 'frame': frame}
            stack_label = 'frame'
        else:
            src = rsp_source(name, rsp_src, out_dir)
            out = os.path.join(out_dir, name)
            result = subprocess.run(ASM86 + [src, '-o', out], capture_output=True, text=True)
            if result.returncode != 0:
                sys.stderr.write(result.stdout)
                sys.stderr.write(result.stderr)
                raise SystemExit(f"asm86 failed assembling {src}")
            with open(out + '.cseg.bin', 'rb') as f:
                rsp_cseg = bytearray(f.read())
            with open(out + '.dseg.bin', 'rb') as f:
                rsp_dseg = bytearray(f.read())
            syms = subprocess.run(ASM86 + [src, '--symbols'], capture_output=True, text=True).stdout
            sym = {k.lower(): int(v, 16) for k, v in ANY_SYM_RE.findall(syms)}

        # Pad `cseg` itself up to the next paragraph boundary before
        # appending the RSP: its code segment is computed from the
        # rounded length, so the bytes have to start exactly there (a
        # 1-byte gap once shifted every address inside TMP by one byte).
        cseg += b'\0' * ((-len(cseg)) % 16)
        code_seg = MPM_CODE_SEG + len(cseg) // 16
        cseg_paras = (len(rsp_cseg) + 15) // 16
        rsp_cseg = rsp_cseg.ljust(cseg_paras * 16, b'\0')
        data_seg = code_seg + cseg_paras

        # The RSP's code segment, GENSYS-filled in the real build (the
        # templates leave `unknown`, 0): the 2nd word of the initial
        # stack frame (offset, segment, flags) and `u_initcs` (UDA offset
        # 50h). "Small Model" (MP/M-86 Programmer's Guide 5.3.2): CS set
        # explicitly, DS/ES/SS left 0 so PROC.RTM's `cp_uda` fills them
        # from `u_wrkseg`, which INIT.SUP's `nrsp:` sets to the RSP's data
        # segment.
        struct.pack_into('<H', rsp_dseg, sym[stack_label] + 2, code_seg)
        struct.pack_into('<H', rsp_dseg, sym['uda'] + 0x50, code_seg)
        dseg_paras = (len(rsp_dseg) + 15) // 16
        rsp_dseg = rsp_dseg.ljust(dseg_paras * 16, b'\0')
        cseg += rsp_cseg
        rsp_images.append((len(cseg), rsp_dseg))   # where its dseg will go
        cseg += rsp_dseg
        rsp_data_segs.append(data_seg)
        print(f"build_mpmsys: {name:7s} code @ {code_seg:#06x}, "
              f"data @ {data_seg:#06x} ({len(rsp_cseg)}+{len(rsp_dseg)} bytes)")
    # chain them: each `rsp_link` (dseg offset 0) names the next one's
    # data segment, the last 0
    for (off, _), nxt in zip(rsp_images, rsp_data_segs[1:] + [0]):
        struct.pack_into('<H', cseg, off, nxt)

    sysdat_segment = MPM_CODE_SEG + (len(cseg) + 15) // 16
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

    # The rest of the system tables go after the XIOS's data tail (the
    # dseg grows; everything here is a SYSDAT offset): the queue
    # buffer's MAU (paragraph-aligned -- its m_start is a segment), the
    # flag table, the PD pool and the Lock List.
    def alloc(n, align=1):
        dseg.extend(bytes((-len(dseg)) % align))
        off = len(dseg)
        dseg.extend(bytes(n))
        return off

    qmau_buf_off = alloc(QMAU_BUF_SIZE, 16)
    satlen = 5  # sat_start(2) + sat_length(2) + sat_nall(1)
    sat_table_len = satlen * (QMAU_SAT_ENTRIES + 1)  # +1 for SAT0 itself
    dseg[qmau_buf_off] = QMAU_SAT_ENTRIES  # SAT0.ents = capacity
    # SAT1.start is a SYSDAT offset, not one relative to m_start: the
    # queue code stores maualloc's mpb_start straight into q_buf and
    # writeq/readq index it with DS = SYSDAT (QUE1.RTM's qw_move, and
    # getqdaddr's own SYSDAT-relative q_buf for in-system QDs). Relative
    # to the buffer, the first table-copied queue with a message buffer
    # (ECHO's) got q_buf = 37h and wrote over SYSDAT's own header
    # (docs/bugs.md #118).
    struct.pack_into('<HHB', dseg, qmau_buf_off + satlen,
                      qmau_buf_off + sat_table_len,    # SAT1.start
                      QMAU_BUF_SIZE - sat_table_len,    # SAT1.length
                      0)                                # SAT1.nall (free)
    struct.pack_into('<HHHH', dseg, QMAU_FIELD_OFFSET,
                      0, sysdat_segment + (qmau_buf_off >> 4),
                      QMAU_BUF_SIZE, 0)
    struct.pack_into('<H', dseg, QUL_FIELD_OFFSET, qd_pool_start)
    struct.pack_into('<H', dseg, ENDSEG_FIELD_OFFSET, 0xFFFF)
    struct.pack_into('<H', dseg, RSPSEG_FIELD_OFFSET, rsp_data_segs[0])

    # flag table -- see FLAGS_FIELD_OFFSET's own comment.
    flagtab_start = alloc(NFLAGS * FLGLEN)
    for i in range(NFLAGS):
        off = flagtab_start + i * FLGLEN
        struct.pack_into('<HB', dseg, off, 0xFFFF, 0xFF)  # flag_off, flag_zig
    struct.pack_into('<H', dseg, FLAGS_FIELD_OFFSET, flagtab_start)

    # PD pool: real free PD slots -- see PUL_FIELD_OFFSET's own comment
    # for why zeroing `pul` alone isn't enough.
    pd_pool_start = alloc(PD_POOL_SLOTS * PD_POOL_SLOT_SIZE, 2)
    for slot in range(PD_POOL_SLOTS):
        slot_off = pd_pool_start + slot * PD_POOL_SLOT_SIZE
        next_off = (slot_off + PD_POOL_SLOT_SIZE
                    if slot < PD_POOL_SLOTS - 1 else 0)
        struct.pack_into('<H', dseg, slot_off, next_off)  # p_link
    struct.pack_into('<H', dseg, PUL_FIELD_OFFSET, pd_pool_start)

    # Lock List: LOCK_ITEMS free items, linked through their first word
    # -- see FREE_ROOT_OFFSET's own comment.
    lock_start = alloc(LOCK_ITEMS * LOCK_ITEM_SIZE, 2)
    for i in range(LOCK_ITEMS):
        off = lock_start + i * LOCK_ITEM_SIZE
        struct.pack_into('<H', dseg, off, off + LOCK_ITEM_SIZE if i < LOCK_ITEMS - 1 else 0)
    struct.pack_into('<H', dseg, FREE_ROOT_OFFSET, lock_start)
    for field in (SYSLREC_OFFSET, SYSLFIL_OFFSET, SYSLTOT_OFFSET):
        struct.pack_into('<H', dseg, field, LOCK_ITEMS)
    dseg[LOCK_MAX_OFFSET] = LOCK_ITEMS
    dseg[OPEN_MAX_OFFSET] = LOCK_ITEMS

    # ccb ("addr. Console Ctrl Blk Table", SYSDAT.FMT) -- real GENSYS
    # computes this; SYSTEM.DAT's stock value is the unconfigured
    # placeholder `0`, which routes every console attach/detach/status
    # call at CHARIO.CIO's own `getccbadr` (ccb + console#*ccblen) to
    # flat address 0 instead of a real CCB, with disastrous results
    # (see `ccbtab0`'s own comment in XIOS86Q.A86 for the exact hang
    # this caused: TMP.A86's first real console attach reads garbage
    # at address 0 as `c_attach`, finds it non-zero, and sleeps
    # forever). Point it at the real, zero-initialized CCB storage
    # `ccbtab0` reserves in XIOS86Q.A86's own private dseg tail.
    ccbtab0_off = get_symbol('XIOS86Q', 'ccbtab0')
    struct.pack_into('<H', dseg, CCB_OFFSET, ccbtab0_off)

    # ccbtab0's own bytes are all zero (XIOS86Q.A86 just reserves them
    # with `rb 12`), but CCB.DEF's own layout comment says byte offset
    # 8 (`c_mimic`, "cio dev that mimics us") uses 0 as a perfectly
    # valid *real* device number, not a sentinel -- "0ffh means no
    # mimic device" is the actual no-op value, same GENSYS-answer-never-
    # configured bug class as `pul`/`qul`/this file's own CCB_OFFSET
    # fix just above. Left at the zero default, CHARIO.CIO's own
    # `conout` (`cmp c_mimic[si],0ffh ! je compout`) always takes the
    # *mimic* branch -- for this build's single console, device 0
    # mimicking device 0 -- so every character written to the console
    # gets a second, redundant `stdout`/`conoutf` call to the exact
    # same device, printing itself twice. Confirmed live: XIOS86Q's own
    # low-level `conout:` (the one real `out dx,al` to the UART) fires
    # twice per character, back-to-back, both returns landing on the
    # identical caller address -- not wild execution, just this one
    # mis-defaulted byte.
    # Both CCBs: console 0 and list 0 (CCB index ncondev + 0, which
    # is what every PD's p_lst holds). Without the list's, a program
    # attaching the printer (SPOOL) waits on a c_queue past the end of
    # the table, where nothing ever wakes it and ABORT can't find it
    # (docs/bugs.md #119).
    for ccb_index in range(2):
        struct.pack_into('<BB', dseg, ccbtab0_off + 12 * ccb_index + 8,
                         0xFF, 0xFF)  # c_mimic, c_msource: none

    # Memory Descriptor pool and the Memory Free List -- see
    # MFL_FIELD_OFFSET's own comment.
    # Enough MDs for the partitions whatever the pool's own size moves
    # free_start to: count them from the pool's start, an upper bound.
    md_pool = len(dseg)
    nparts = (FREE_MEM_END - (sysdat_segment + md_pool // 16)) // PARTITION_SIZE
    md_count = MD_SPARE + nparts
    dseg += bytes(md_count * MD_LEN)
    free_start = sysdat_segment + (len(dseg) + 15) // 16
    starts = list(range(free_start, FREE_MEM_END, PARTITION_SIZE))
    if len(starts) > 1 and FREE_MEM_END - starts[-1] < PARTITION_SIZE:
        starts.pop()                          # fold the remainder into the last
    assert len(starts) <= nparts
    ends = starts[1:] + [FREE_MEM_END]
    mds = [md_pool + i * MD_LEN for i in range(md_count)]
    spare, parts = mds[:md_count - len(starts)], mds[md_count - len(starts):]
    for i, off in enumerate(spare):           # mdul
        struct.pack_into('<H', dseg, off, spare[i + 1] if i + 1 < len(spare) else 0)
    struct.pack_into('<H', dseg, MDUL_FIELD_OFFSET, spare[0])
    for i, off in enumerate(parts):           # mfl, in memory order
        struct.pack_into('<HHHH', dseg, off, parts[i + 1] if i + 1 < len(parts) else 0,
                         starts[i], ends[i] - starts[i], 0)
    struct.pack_into('<H', dseg, MFL_FIELD_OFFSET, parts[0])
    print(f"build_mpmsys: free memory {free_start:#06x}-{FREE_MEM_END:#06x} in "
          f"{len(parts)} partitions, {len(spare)} free MDs, {LOCK_ITEMS} Lock List items")

    out_path = args.output or os.path.join(out_dir, 'MPM.bin')
    with open(out_path + '.cseg', 'wb') as f:
        f.write(cseg)
    with open(out_path + '.dseg', 'wb') as f:
        f.write(dseg)
    print(f"build_mpmsys: wrote {out_path}.cseg ({len(cseg)} bytes), "
          f"{out_path}.dseg ({len(dseg)} bytes)")


if __name__ == '__main__':
    main()
