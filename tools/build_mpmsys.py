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

SYSDAT_DSEG_SIZE = 4096  # see XIOS86Q.A86's own comment on this number

SYM_RE = re.compile(r'^\s*(init|entry)\s+label\s+([0-9A-Fa-f]+)h', re.MULTILINE)


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
            code = f.read()
        module_base = len(cseg)
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
