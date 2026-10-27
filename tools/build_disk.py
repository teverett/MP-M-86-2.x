#!/usr/bin/env python3
"""Assemble a bootable QEMU floppy image from this tree's other build
outputs: ROM86Q.bin (tools/build_bootsect.py), MPMLDR.bin
(tools/build_mpmldr.py), and MPM.bin.{cseg,dseg} (tools/build_mpmsys.py).

Those three scripts each produce one piece; nothing until now has
combined them into an actual disk image, or built the one piece none
of them produce at all: a real on-disk `MPM.SYS` file, in the format
`src-assem/loader/LDMPM.A86` documents and checks for itself (read directly
from its own code, not just its header comment -- see the `perr:`/
`hdrok:` logic around line 140 of that file):

    one 128-byte header record:
        ctype  db 1          ;codetype -- LDMPM checks this is exactly 1
        clen   dw <code len, in PARAGRAPHS (16-byte units), not bytes>
        cldseg dw <code segment>
        cmin   dw 0          ;unused by LDMPM.A86's own logic
        cmax   dw 0          ;unused by LDMPM.A86's own logic
        dtype  db 2          ;datatype -- LDMPM checks this is exactly 2
        dlen   dw <data len, in PARAGRAPHS too>
        dldseg dw <data segment>
        dmin   dw 0
        dmax   dw 0
        (padded with zeros to 128 bytes)
    followed immediately (continuing the same sequential BDOS read
    LDMPM.A86 performs) by <code len> bytes of code, then <data len>
    bytes of data (the *real* byte lengths here, even though the
    header's own clen/dlen fields are paragraph counts).

    clen/dlen being paragraph counts, not byte counts, isn't just a
    docstring detail: LDMPM.A86's own header-sanity check computes
    `cldseg + clen` and expects that to equal `dldseg` exactly (i.e.
    "the data segment starts immediately after the code segment, in
    paragraph units") -- both sides of that comparison are segment
    (paragraph) values, so clen has to be paragraphs too for the
    arithmetic to mean anything. Confirmed the hard way: writing raw
    byte lengths here (this script's own original approach) assembled
    and built fine, with no error anywhere in this toolchain, but
    LDMPM.A86 rejected the real on-disk MPM.SYS as a "Bad Header
    Record" every time once booting in QEMU actually got far enough to
    read it (docs/bugs.md #22's own continuation) -- `cldseg + clen`
    (a segment plus a raw byte count) came out nowhere near `dldseg`.
    LDMPM.A86's own later length check (`lenok:`) confirms the same
    convention independently: it shifts the *actual* bytes-read count
    right by 4 (i.e. converts it to paragraphs) before comparing
    against `clen + dlen`.

`MPM.SYS` has to exist as a real file in a real CP/M directory on the
disk, found by `LDMPM.A86`'s own `openfnc`/FCB-based open -- there is
no shortcut here, unlike the loader-level header `ROM86Q.A86` reads
(that one really is just raw bytes at a fixed disk location, no
filesystem involved). This script builds that CP/M directory (and the
whole disk image around it) by hand, from the exact DPB both
`src-assem/kernel/XIOS86Q.A86` and `src-assem/loader/LDBIOS86Q.A86` already carry
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

After MPM.SYS's own blocks, each of the 18 src-assem/utils/ binaries the
Makefile's own PLM_UTILS list builds, plus TMP/ECHO/CLOCK/SUBMIT.A86
(see RSP_OK in the Makefile) and MPMSTAT, is written as its own CP/M
directory entry: <NAME>.CMD for the utilities (and for SUBMIT),
<NAME>.RSP for TMP/ECHO/CLOCK/MPMSTAT -- plain 8.3 names
matching the Makefile's own basenames. MPMSTAT.RSP is STSRSP.PLM built
as an RSP (`plmc --rsp MPMSTAT`), following STSRSP.PLM's own header
comment ("ren mpmstat.rsp=stsrsp.cmd"); STSRSP isn't placed as a .CMD,
since run as a program it makes its queue and waits on it forever,
hanging the console (docs/bugs.md #125). STSCMD goes on the disk
as MPMSTAT.CMD, the name its header gives it (DISK_NAMES); STSCOM
stays a plain <NAME>.CMD.

Each of these files -- like MPM.SYS -- actually assembles to *two*
pieces, a `.cseg.bin` and a `.dseg.bin` (the latter holding real
initialized data: e.g. TYPE.PLM's own error-message strings, or
ECHO.A86's 794 bytes of working storage); the Makefile only tracks the
`.cseg.bin` as each utility/RSP's build product, but dropping the data
segment would silently discard that real data. So each one gets the
same two-segment header MPM.SYS gets (see `build_two_segment_body`),
but with relocatable groups (base 0, min = length), the way GENCMD
writes a .CMD, so LOAD.SUP puts each one wherever memory is free.

KNOWN LIMITATIONS:
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
# QEMU" item 1/2, and src-assem/kernel/XIOS86Q.A86's/src-assem/loader/
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

# Same 18 names as the Makefile's own PLM_UTILS list (src-assem/utils/*.PLM
# that assemble cleanly) -- kept here as a plain literal rather than
# parsed out of the Makefile, same reasoning as every other hand-kept
# constant in this script.
UTIL_NAMES = ['ABORT', 'ATTACH', 'CONS', 'DIR', 'DSKRST', 'ED', 'ERA',
              'ERAQ', 'PIP', 'REN', 'SET', 'SHOW', 'SPOOL', 'SSPOOL',
              'STSCMD', 'STSCOM', 'TOD', 'TYPE']

# Same reasoning as RSP_OK in the Makefile -- all 4 of ECHO/CLOCK/TMP/
# SUBMIT.A86 assemble cleanly now that docs/bugs.md #1-#3 are fixed.
# TMP/ECHO/CLOCK become <NAME>.RSP; SUBMIT becomes SUBMIT.CMD instead
# (docs/build.md's own mapping -- it's invoked as a CLI command, not
# attached to a console the way the RSPs are). MPMSTAT is the
# Makefile's src-assem/build/rsp/MPMSTAT (STSRSP.PLM as an RSP). build_mpmsys.py
# bakes TMP, ECHO, CLOCK and MPMSTAT into MPM.SYS itself (standing in
# for GENSYS, which would read these files); the disk carries them as
# DRI's distributions do (TMP.RSP, ECHO.RSP, ...), unpatched.
# RANDOM (DRI's sample random-access program) is a .CMD like SUBMIT.
RSP_NAMES = ['TMP', 'ECHO', 'CLOCK', 'SUBMIT', 'MPMSTAT', 'RANDOM']

# Data-group maximums (paragraphs) DRI's own build gave GENCMD for the
# utilities that size a buffer from the memory they're loaded with
# (base page MAXB, the data group's real length): "gencmd ed data[b1a5
# m253 xfff]" and "gencmd pip data[b1c3 m374 x980]" (orig/mpm862sr's
# submit files). Everything else is built with no options, max = min.
# Without these, ED had no room past its own data: "NO MEMORY"
# (docs/bugs.md #120).
DATA_MAX = {'ED': 0xFFF, 'PIP': 0x980}
RSP_EXT = {'TMP': 'RSP', 'ECHO': 'RSP', 'CLOCK': 'RSP', 'SUBMIT': 'CMD', 'MPMSTAT': 'RSP',
           'RANDOM': 'CMD'}

# Files already in GENCMD form, copied to the disk byte for byte: DRI's
# CACHE.RSP (orig/lastdri/22; no source exists -- docs/bugs.md #124),
# which build_mpmsys.py also bakes into MPM.SYS.
RAW_FILES = [('CACHE', 'RSP', os.path.join('src-assem', 'rsp', 'CACHE.RSP'))]

# Disk names that differ from the build's: STSCMD.PLM's own header
# ("ren mpmstat.cmd=stscmd.cmd") -- with the MPMSTAT RSP resident,
# typing MPMSTAT reaches the RSP's queue first; MPMSTAT.CMD runs only on
# a system built without it (or as A:MPMSTAT) -- and DSKRESET/CONSOLE,
# the names the MP/M-86 user guide (and DRI's distributions) give
# DSKRST.PLM's and CONS.PLM's commands.
DISK_NAMES = {'STSCMD': 'MPMSTAT', 'DSKRST': 'DSKRESET', 'CONS': 'CONSOLE'}


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


def build_two_segment_body(code, data, code_seg=None, data_max=0):
    """The 128-byte header record LDMPM.A86 itself checks for MPM.SYS,
    followed by the code then the data -- see this script's own
    docstring (including why clen/dlen are paragraph counts, not byte
    counts). Reused as-is for every utility/RSP body too, as the only
    two-segment-with-header convention this tree has.

    The code (and data) bytes written here are padded out to a full
    paragraph each, not left at their raw lengths: LDMPM.A86 loads the
    whole body as one continuous sequential read (code immediately
    followed by data, exactly as stored on disk -- see its own
    `readit1`/`readit2` loop), so data only actually lands at the
    `dldseg` this header promises if the code portion on disk is
    exactly `clen` paragraphs long, padding included. Skipping the pad
    (writing only the raw `len(code)` bytes) would also make LDMPM's
    own later length check (`lenok:`) fail falsely whenever a segment
    isn't already a whole number of paragraphs, since it compares
    `clen + dlen` (now exact paragraph counts) against the real
    bytes-read count shifted right 4 (i.e. floor-divided) -- the two
    would quietly disagree by a paragraph or two.

    With no `code_seg` the groups are relocatable, the way GENCMD
    writes a .CMD: base 0 and min = the group's own length, so LOAD.SUP
    mallocs them wherever memory is free. A fixed base made every
    program load at the same address, which held only while nothing
    stayed resident: once SPOOL detached there, every later load failed
    with "Not Enough Memory" (docs/bugs.md #119). `data_max` is the data
    group's max (paragraphs; 0 = just min), see DATA_MAX.
    """
    code_paras = (len(code) + 15) // 16  # paragraph-align
    code = code.ljust(code_paras * 16, b'\0')
    data_paras = (len(data) + 15) // 16
    data = data.ljust(data_paras * 16, b'\0')
    if code_seg is None:
        data_seg = None
        groups = (0, code_paras, 0, 0, data_paras, max(data_max, data_paras) if data_max else 0)
    else:
        data_seg = code_seg + code_paras
        groups = (code_seg, 0, 0, data_seg, 0, 0)
    header = struct.pack(
        '<BHHHH BHHHH',
        MPM_HEADER_TYPE_CODE, code_paras, *groups[:3],
        MPM_HEADER_TYPE_DATA, data_paras, *groups[3:],
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
                          '(default: src-assem/build/disk/mpm86q.img)')
    args = ap.parse_args()

    out_dir = os.path.join(ROOT, 'src-assem', 'build', 'disk')
    os.makedirs(out_dir, exist_ok=True)

    run('build_bootsect.py')
    run('build_mpmldr.py')
    run('build_mpmsys.py')

    # The 18 src-assem/utils/ utilities have no single build_*.py of their
    # own (the Makefile compiles+assembles each one directly) -- ask
    # `src-assem/Makefile` owns the utility/RSP rules and uses paths relative
    # to src-assem/, so invoke it there, with its build directory, src-assem/build/.
    util_targets = [f'build/utils/{name}.cseg.bin' for name in UTIL_NAMES]
    rsp_targets = [f'build/rsp/{name}.cseg.bin' for name in RSP_NAMES]
    result = subprocess.run(['make', *util_targets, *rsp_targets],
                             cwd=os.path.join(ROOT, 'src-assem'),
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit("make (utils/rsp) failed")
    sys.stdout.write(result.stdout)

    with open(os.path.join(ROOT, 'src-assem', 'build', 'loader', 'ROM86Q.bin'), 'rb') as f:
        bootsect = f.read()
    with open(os.path.join(ROOT, 'src-assem', 'build', 'loader', 'MPMLDR.bin'), 'rb') as f:
        mpmldr = f.read()
    with open(os.path.join(ROOT, 'src-assem', 'build', 'kernel', 'MPM.bin.cseg'), 'rb') as f:
        mpm_code = f.read()
    with open(os.path.join(ROOT, 'src-assem', 'build', 'kernel', 'MPM.bin.dseg'), 'rb') as f:
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

    # -- the 18 src-assem/utils/ utilities (as <NAME>.CMD) and ECHO.RSP/
    # CLOCK.RSP/SUBMIT.CMD, each code+data combined the same way as
    # MPM.SYS above, one directory entry per file (more if > 16KB,
    # same multi-extent logic), continuing on from MPM.SYS's own
    # entries/blocks --
    util_bodies = []
    for name, ext, build_dir in (
        [(n, 'CMD', 'utils') for n in UTIL_NAMES] +
        [(n, RSP_EXT[n], 'rsp') for n in RSP_NAMES]
    ):
        stem = os.path.join(ROOT, 'src-assem', 'build', build_dir, name)
        with open(f'{stem}.cseg.bin', 'rb') as f:
            code = f.read()
        with open(f'{stem}.dseg.bin', 'rb') as f:
            data = f.read()
        body, data_seg = build_two_segment_body(code, data, data_max=DATA_MAX.get(name, 0))
        name = DISK_NAMES.get(name, name)
        util_bodies.append((name, ext, body, len(code), len(data),
                             data_seg, next_free_block))
        util_entries, next_free_block = directory_entries_for_file(
            name.encode('ascii'), ext.encode('ascii'), len(body),
            first_block=next_free_block)
        entries.extend(util_entries)
    for name, ext, path in RAW_FILES:
        with open(os.path.join(ROOT, path), 'rb') as f:
            body = f.read()
        util_bodies.append((name, ext, body, None, None, None, next_free_block))
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
        what = (f"relocatable, code len={code_len:#x}, data len={data_len:#x}"
                if code_len is not None else "copied as is")
        print(f"build_disk: {name}.{ext} {len(body)} bytes @ "
              f"{block_to_offset(first_block):#x} ({what})")
    print(f"build_disk: directory has {len(entries)} entries used of "
          f"{DIR_ENTRIES}")
    print(f"build_disk: wrote {out_path} ({len(image)} bytes)")


if __name__ == '__main__':
    main()
