#!/usr/bin/env python3
"""Assemble a bootable QEMU floppy image from this tree's other build
outputs: ROM86Q.bin (tools/build_bootsect.py), MPMLDR.bin
(tools/build_mpmldr.py), and MPM.bin.{cseg,dseg} (tools/build_mpmsys.py).

Those three scripts each produce one piece; nothing until now has
combined them into an actual disk image, or built the one piece none
of them produce at all: a real on-disk `MPM.SYS` file, in the format
`src/loader/LDMPM.A86` documents and checks for itself (read directly
from its own code, not just its header comment -- see the `perr:`/
`hdrok:` logic around line 140 of that file):

    one 128-byte header record:
        ctype  db 1          ;codetype -- LDMPM checks this is exactly 1
        clen   dw <code len>
        cldseg dw <code segment>
        cmin   dw 0          ;unused by LDMPM.A86's own logic
        cmax   dw 0          ;unused by LDMPM.A86's own logic
        dtype  db 2          ;datatype -- LDMPM checks this is exactly 2
        dlen   dw <data len>
        dldseg dw <data segment>
        dmin   dw 0
        dmax   dw 0
        (padded with zeros to 128 bytes)
    followed immediately (continuing the same sequential BDOS read
    LDMPM.A86 performs) by <code len> bytes of code, then <data len>
    bytes of data.

`MPM.SYS` has to exist as a real file in a real CP/M directory on the
disk, found by `LDMPM.A86`'s own `openfnc`/FCB-based open -- there is
no shortcut here, unlike the loader-level header `ROM86Q.A86` reads
(that one really is just raw bytes at a fixed disk location, no
filesystem involved). This script builds that CP/M directory (and the
whole disk image around it) by hand, from the exact DPB both
`src/kernel/XIOS86Q.A86` and `src/loader/LDBIOS86Q.A86` already carry
(SPT=72 128-byte logical records/CP/M-track, 8192-byte/64-record
allocation blocks, DSM=176, DRM=63 (64 directory entries), AL0=80h/
AL1=0 (block 0 reserved for the directory), CKS=16, OFF=2 (2 reserved
CP/M tracks) -- EXM=0, meaning one directory entry (one "extent")
covers at most 128 128-byte records (16KB); a file bigger than that
needs more than one directory entry, handled below.

Disk layout this produces (1,474,560 bytes: 80 cylinders * 2 heads *
18 sectors/track * 512 bytes/sector):

    physical sector 1 (byte 0):      ROM86Q.bin, the boot sector.
    physical sector 2 (byte 512):    the 9-byte loader header
                                      ROM86Q.A86 itself reads (Ty/Len/
                                      Abs/Min/Max -- see that file's
                                      own header comment), padded to
                                      512 bytes.
    physical sector 3 (byte 1024):   MPMLDR.bin.
    byte 18432 (OFF=2 reserved CP/M tracks in): the CP/M directory
                                      (block 0, 8192 bytes, 64
                                      32-byte entries -- unused
                                      entries marked E5h, the
                                      standard CP/M "available"
                                      marker).
    byte 26624 (block 1 onward):     MPM.SYS's own body (the 128-byte
                                      header above, then its code,
                                      then its data).
    everything else:                 zero-filled, unallocated.

Both MPMLDR.bin and MPM.SYS's own code/data are loaded at fixed real-
mode segments chosen here (not computed from anything else -- there's
no constraint from LDMPM.A86's own code beyond "don't overlap each
other or the boot sector", per its own header comment: "MPMLDR may be
read into any segment that does not overlap the desired system load
segment"): MPMLDR at 0800h (physical 8000h, clear of the boot sector
and its stack, which both stay below 7C00h), MPM.SYS's code at 1000h,
its data immediately after at a paragraph-aligned offset.

After MPM.SYS's own blocks, each of the 19 src/utils/ binaries the
Makefile's own PLM_UTILS list builds, plus src/rsp/ECHO.A86 (the only
RSP that assembles cleanly -- see RSP_OK in the Makefile), is written
as its own CP/M directory entry: <NAME>.CMD for the utilities, ECHO.RSP
for ECHO -- plain 8.3 names matching the Makefile's own basenames, not
the docs/build.md section-5 renames (STSCMD/STSRSP -> STAT.CMD/
MPMSTAT.RSP): that mapping disagrees with the STSCMD.PLM/STSRSP.PLM
header comments themselves (mpmstat.cmd=stscmd.cmd, mpmstat.rsp=
stsrsp.cmd) on which file becomes which name, so STSCMD/STSCOM/STSRSP
are kept as plain <NAME>.CMD like everything else rather than guessing.

Each of these files -- like MPM.SYS -- actually assembles to *two*
pieces, a `.cseg.bin` and a `.dseg.bin` (the latter holding real
initialized data: e.g. TYPE.PLM's own error-message strings, or
ECHO.A86's 794 bytes of working storage); the Makefile only tracks the
`.cseg.bin` as each utility/RSP's build product, but dropping the data
segment would silently discard that real data. So each one gets the
same two-segment header MPM.SYS gets (see `build_two_segment_body`),
at a shared placeholder code segment (CMD_RSP_LOAD_SEG below) -- a
placeholder because no CCP/process loader exists anywhere in this tree
yet to actually read a `.CMD`/`.RSP` file and run it (`docs/build.md`'s
own "minimum useful command set" section 5 is still just a target
list), so there is no real convention here to match yet, only MPM.SYS's
own confirmed-by-LDMPM.A86 header format to stay consistent with.

KNOWN LIMITATIONS:
  - No SUBMIT.CMD (src/rsp/SUBMIT.A86 has 3 genuine, deliberately
    unpatched 1981 source bugs -- docs/bugs.md #3 -- and doesn't
    assemble) or the other RSPs (CLOCK/TMP, docs/bugs.md #1/#2, same
    reason). Only ECHO.RSP is placed, since it's the only one in
    RSP_OK.
  - Nothing in this tree yet actually loads a `.CMD`/`.RSP` file from
    disk and runs it -- these are placed in the filesystem in a format
    modeled on MPM.SYS's own, but unconfirmed against any real loader.
  - Actually tested in QEMU (`make run`): boots through SeaBIOS, the
    boot sector loads MPMLDR.bin and jumps into it (see docs/bugs.md
    #12/#13 for the two real bugs that testing caught and fixed).
    MPMLDR/LDBDOS.A86/LDBIOS86Q.A86 running correctly past that jump
    hasn't been confirmed yet -- no console output was observed from
    that point before this testing session ended.

Usage: python3 tools/build_disk.py [-o OUTPUT]
"""
import argparse
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PYTHON = sys.executable

SECTOR_SIZE = 512
CYLINDERS = 80
HEADS = 2
SECTORS_PER_TRACK = 18
DISK_SIZE = CYLINDERS * HEADS * SECTORS_PER_TRACK * SECTOR_SIZE

# DPB (see docs/build.md's "What's actually needed to boot under
# QEMU" item 1/2, and src/kernel/XIOS86Q.A86's/src/loader/
# LDBIOS86Q.A86's own identical DPBs).
RECORDS_PER_TRACK = 72          # SPT: 128-byte logical records/CP/M track
BLOCK_SIZE = 8192                # BSH=6, BLM=63
RECORDS_PER_BLOCK = BLOCK_SIZE // 128  # 64
TOTAL_BLOCKS = 177               # DSM+1
DIR_ENTRIES = 64                 # DRM+1
DIR_BLOCKS = 1                   # AL0=80h, AL1=0
RESERVED_TRACKS = 2              # OFF
RECORDS_PER_CPM_TRACK = RECORDS_PER_TRACK  # one CP/M track == one physical
                                            # track here (nheads=2 means
                                            # each physical head side is
                                            # its own CP/M track)
BYTES_PER_CPM_TRACK = RECORDS_PER_CPM_TRACK * 128  # 9216, == one physical
                                                    # track's byte size

DATA_AREA_START = RESERVED_TRACKS * BYTES_PER_CPM_TRACK  # 18432
DIR_START = DATA_AREA_START
RECORDS_PER_EXTENT = 128         # EXM=0: one directory entry == 16KB

# Where the loader and the boot sector place things.
BOOTSECT_OFFSET = 0
LOADER_HEADER_OFFSET = SECTOR_SIZE
MPMLDR_OFFSET = 2 * SECTOR_SIZE
MPMLDR_LOAD_SEG = 0x0800         # physical 8000h -- clear of the boot
                                 # sector (7C00h-7E00h) and its stack

MPM_CODE_SEG = 0x1000            # physical 10000h
MPM_HEADER_TYPE_CODE = 1         # LDMPM.A86's own `codetype equ 1`
MPM_HEADER_TYPE_DATA = 2         # LDMPM.A86's own `datatype equ 2`

# Same 19 names as the Makefile's own PLM_UTILS list (src/utils/*.PLM
# that assemble cleanly) -- kept here as a plain literal rather than
# parsed out of the Makefile, same reasoning as every other hand-kept
# constant in this script.
UTIL_NAMES = ['ABORT', 'ATTACH', 'CONS', 'DIR', 'DSKRST', 'ED', 'ERA',
              'ERAQ', 'PIP', 'REN', 'SET', 'SHOW', 'SPOOL', 'SSPOOL',
              'STSCMD', 'STSCOM', 'STSRSP', 'TOD', 'TYPE']

# Same reasoning as RSP_OK in the Makefile -- only ECHO.A86 assembles
# cleanly (CLOCK/TMP/SUBMIT all have genuine, deliberately unpatched
# 1981 source bugs -- docs/bugs.md #1-#3).
RSP_NAMES = ['ECHO']

# Placeholder load segment shared by every utility/RSP body below --
# see this script's own docstring for why there's no real convention
# yet to pick a better one.
CMD_RSP_LOAD_SEG = 0x2000


def run(script, *args):
    result = subprocess.run([PYTHON, os.path.join(ROOT, 'tools', script), *args],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"{script} failed")
    sys.stdout.write(result.stdout)


def block_to_offset(block_num):
    return DATA_AREA_START + block_num * BLOCK_SIZE


def build_two_segment_body(code, data, code_seg):
    """The 128-byte header record LDMPM.A86 itself checks for MPM.SYS,
    followed by the code then the data -- see this script's own
    docstring. Reused as-is for every utility/RSP body too, as the
    only two-segment-with-header convention this tree has."""
    data_seg = code_seg + (len(code) + 15) // 16  # paragraph-align
    header = struct.pack(
        '<BHHHH BHHHH',
        MPM_HEADER_TYPE_CODE, len(code), code_seg, 0, 0,
        MPM_HEADER_TYPE_DATA, len(data), data_seg, 0, 0,
    )
    header = header.ljust(128, b'\0')
    return header + code + data, data_seg


def directory_entries_for_file(name, ext, body_len, first_block):
    """One CP/M directory entry per 128-record (16KB) extent -- EXM=0
    means that's the most one entry can span, regardless of block
    size (see this script's own docstring)."""
    total_records = (body_len + 127) // 128
    entries = []
    block = first_block
    records_left = total_records
    extent = 0
    while records_left > 0:
        rc = min(RECORDS_PER_EXTENT, records_left)
        blocks_needed = (rc * 128 + BLOCK_SIZE - 1) // BLOCK_SIZE
        al = list(range(block, block + blocks_needed)) + [0] * (16 - blocks_needed)
        entry = struct.pack(
            '<B8s3sBBBB16B',
            0, name.ljust(8, b' '), ext.ljust(3, b' '),
            extent, 0, 0, rc, *al,
        )
        assert len(entry) == 32
        entries.append(entry)
        block += blocks_needed
        records_left -= rc
        extent += 1
    return entries, block  # block is now the next free block


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('-o', '--output', default=None,
                     help='output path for the disk image '
                          '(default: build/disk/mpm86q.img)')
    args = ap.parse_args()

    out_dir = os.path.join(ROOT, 'build', 'disk')
    os.makedirs(out_dir, exist_ok=True)

    run('build_bootsect.py')
    run('build_mpmldr.py')
    run('build_mpmsys.py')

    # The 19 src/utils/ utilities have no single build_*.py of their
    # own (the Makefile compiles+assembles each one directly) -- ask
    # `make` for each build/utils/<NAME>.cseg.bin target the same way
    # the three build_*.py calls above are themselves `make` targets.
    util_targets = [f'build/utils/{name}.cseg.bin' for name in UTIL_NAMES]
    rsp_targets = [f'build/rsp/{name}.cseg.bin' for name in RSP_NAMES]
    result = subprocess.run(['make', *util_targets, *rsp_targets], cwd=ROOT,
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit("make (utils/rsp) failed")
    sys.stdout.write(result.stdout)

    with open(os.path.join(ROOT, 'build', 'loader', 'ROM86Q.bin'), 'rb') as f:
        bootsect = f.read()
    with open(os.path.join(ROOT, 'build', 'loader', 'MPMLDR.bin'), 'rb') as f:
        mpmldr = f.read()
    with open(os.path.join(ROOT, 'build', 'kernel', 'MPM.bin.cseg'), 'rb') as f:
        mpm_code = f.read()
    with open(os.path.join(ROOT, 'build', 'kernel', 'MPM.bin.dseg'), 'rb') as f:
        mpm_data = f.read()

    assert len(bootsect) == SECTOR_SIZE
    if MPMLDR_OFFSET + len(mpmldr) > DATA_AREA_START:
        raise SystemExit(
            f"MPMLDR.bin ({len(mpmldr)} bytes) no longer fits in the "
            f"{RESERVED_TRACKS} reserved CP/M tracks -- grew past "
            f"{DATA_AREA_START - MPMLDR_OFFSET} bytes of room")

    image = bytearray(DISK_SIZE)

    # -- boot sector + loader header + MPMLDR --
    image[BOOTSECT_OFFSET:BOOTSECT_OFFSET + len(bootsect)] = bootsect

    loader_header = struct.pack('<BHHHH', 1, len(mpmldr), MPMLDR_LOAD_SEG,
                                 len(mpmldr), len(mpmldr))
    image[LOADER_HEADER_OFFSET:LOADER_HEADER_OFFSET + len(loader_header)] = \
        loader_header

    image[MPMLDR_OFFSET:MPMLDR_OFFSET + len(mpmldr)] = mpmldr

    # -- CP/M directory (block 0): all entries start "available" (E5h) --
    dir_bytes = bytearray(b'\xe5' * BLOCK_SIZE)

    mpm_sys_body, mpm_data_seg = build_two_segment_body(
        mpm_code, mpm_data, MPM_CODE_SEG)
    entries, next_free_block = directory_entries_for_file(
        b'MPM', b'SYS', len(mpm_sys_body), first_block=DIR_BLOCKS)
    if next_free_block > TOTAL_BLOCKS:
        raise SystemExit(
            f"MPM.SYS ({len(mpm_sys_body)} bytes) needs more blocks "
            f"than this disk's DPB has ({TOTAL_BLOCKS - DIR_BLOCKS} "
            "available for data)")

    # -- the 19 src/utils/ utilities (as <NAME>.CMD) and ECHO.RSP,
    # each code+data combined the same way as MPM.SYS above, one
    # directory entry per file (more if > 16KB, same multi-extent
    # logic), continuing on from MPM.SYS's own entries/blocks --
    util_bodies = []
    for name, ext, build_dir in (
        [(n, 'CMD', 'utils') for n in UTIL_NAMES] +
        [(n, 'RSP', 'rsp') for n in RSP_NAMES]
    ):
        stem = os.path.join(ROOT, 'build', build_dir, name)
        with open(f'{stem}.cseg.bin', 'rb') as f:
            code = f.read()
        with open(f'{stem}.dseg.bin', 'rb') as f:
            data = f.read()
        body, data_seg = build_two_segment_body(code, data, CMD_RSP_LOAD_SEG)
        util_bodies.append((name, ext, body, len(code), len(data),
                             data_seg, next_free_block))
        util_entries, next_free_block = directory_entries_for_file(
            name.encode('ascii'), ext.encode('ascii'), len(body),
            first_block=next_free_block)
        entries.extend(util_entries)

    if next_free_block > TOTAL_BLOCKS:
        raise SystemExit(
            f"MPM.SYS + utilities/RSPs need more blocks than this "
            f"disk's DPB has ({TOTAL_BLOCKS - DIR_BLOCKS} available "
            "for data)")
    if len(entries) > DIR_ENTRIES:
        raise SystemExit(
            f"MPM.SYS + utilities/RSPs need {len(entries)} directory "
            f"entries, more than this disk's DPB allows ({DIR_ENTRIES})")
    for i, entry in enumerate(entries):
        dir_bytes[i * 32:(i + 1) * 32] = entry

    image[DIR_START:DIR_START + BLOCK_SIZE] = dir_bytes

    # -- MPM.SYS's own body, in the blocks its directory entries name --
    body_start = block_to_offset(DIR_BLOCKS)
    image[body_start:body_start + len(mpm_sys_body)] = mpm_sys_body

    # -- each utility/RSP's body, at the block its own directory
    # entries named above (recorded as each file was allocated, so
    # order and block numbers always match regardless of how MPM.SYS
    # sized) --
    for name, ext, body, code_len, data_len, data_seg, first_block in util_bodies:
        util_start = block_to_offset(first_block)
        image[util_start:util_start + len(body)] = body

    out_path = args.output or os.path.join(out_dir, 'mpm86q.img')
    with open(out_path, 'wb') as f:
        f.write(image)

    print(f"build_disk: boot sector {len(bootsect)} bytes @ "
          f"{BOOTSECT_OFFSET:#x}")
    print(f"build_disk: loader header @ {LOADER_HEADER_OFFSET:#x} "
          f"(Len={len(mpmldr):#x}, Abs={MPMLDR_LOAD_SEG:04x}h)")
    print(f"build_disk: MPMLDR.bin {len(mpmldr)} bytes @ "
          f"{MPMLDR_OFFSET:#x}")
    print(f"build_disk: CP/M directory @ {DIR_START:#x}")
    print(f"build_disk: MPM.SYS body {len(mpm_sys_body)} bytes @ "
          f"{body_start:#x} (code@{MPM_CODE_SEG:04x}h "
          f"len={len(mpm_code):#x}, data@{mpm_data_seg:04x}h "
          f"len={len(mpm_data):#x})")
    for name, ext, body, code_len, data_len, data_seg, first_block in util_bodies:
        print(f"build_disk: {name}.{ext} {len(body)} bytes @ "
              f"{block_to_offset(first_block):#x} "
              f"(code@{CMD_RSP_LOAD_SEG:04x}h len={code_len:#x}, "
              f"data@{data_seg:04x}h len={data_len:#x})")
    print(f"build_disk: directory has {len(entries)} entries used of "
          f"{DIR_ENTRIES}")
    print(f"build_disk: wrote {out_path} ({len(image)} bytes)")


if __name__ == '__main__':
    main()
