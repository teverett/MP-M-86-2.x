#!/usr/bin/env python3
"""mkdisk.py -- build src-c's bootable MP/M-86 floppy (build/mpm86c.img)
from src-c's own build output, nothing from src-assem:

  - the boot sector (build/boot/bootsect.bin), sector 0, its fields
    filled in (tools/bootsect.py) to load KERNEL.SYS;
  - a CP/M file system with the C XIOS's geometry (its DPB:
    src-c/gensys/sysdat.c -- tests/src-c/test_mkdisk.py checks these
    constants against it), holding:
      KERNEL.SYS   the kernel's flat image (build/boot/kernel.bin), in
                   contiguous blocks from block 1, read-only;
      SUBMIT.CMD,  CMD files for the C transients (TRANSIENTS: src-c/rsp's
      RANDOM.CMD,  and src-c/utils'): the data group is the C program's
      DIR.CMD, ... data image (from its compiled object, build/*/*.o),
                   the code group zeros of the original's length
                   (rsp_syms.h, utils_syms.h) -- the C kernel runs the C
                   main, LOAD only needs the groups;
      DEMO.SUB     a sample SUB file.

Run from src-c/ (the Makefile's `disk` target does).
"""
import argparse
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_C = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import bootsect  # noqa: E402

# the floppy: 80 cylinders, 2 heads, 18 512-byte sectors
SECTOR = 512
DISK_SIZE = 80 * 2 * 18 * SECTOR
# the CP/M file system on it (the XIOS's DPB): one CP/M track per
# physical track, 4 records a sector
SPT = 72          # records per track
BSH, BLM = 6, 63  # 8K blocks
DSM = 176         # last block
DRM = 63          # last directory entry
AL0 = 0x80        # the directory: block 0
OFF = 2           # reserved tracks
RECORD = 128
BLOCK = RECORD << BSH
DATA_START = OFF * SPT * RECORD
EXTENT_RECORDS = 128  # EXM = 0: one directory entry is a 16K extent
DIR_ENTRIES = DRM + 1

DEMO_SUB = (b"ECHO MP/M-86 on the C kernel, from a disk src-c built\r\n"
            b"ECHO argument 1: $1\r\n"
            b"USER 1\r\nUSER 0\r\n")


def block_offset(b):
    return DATA_START + b * BLOCK


class Disk:
    def __init__(self):
        self.img = bytearray(DISK_SIZE)
        self.img[block_offset(0):block_offset(0) + DIR_ENTRIES * 32] = b'\xe5' * (DIR_ENTRIES * 32)
        self.next_block = 1  # block 0 is the directory (AL0)
        self.next_entry = 0

    def add(self, name, ext, data, read_only=False):
        """a file in contiguous blocks; returns its first block"""
        data = data + b'\x1a' * ((-len(data)) % RECORD)
        records = len(data) // RECORD
        nblocks = (len(data) + BLOCK - 1) // BLOCK
        first = self.next_block
        if first + nblocks - 1 > DSM:
            raise SystemExit(f"mkdisk: {name}.{ext} doesn't fit")
        at = block_offset(first)
        self.img[at:at + len(data)] = data
        self.next_block += nblocks
        ext_bytes = bytearray(ext.ljust(3))
        if read_only:
            ext_bytes[0] |= 0x80  # t1': R/O
        per_extent = EXTENT_RECORDS * RECORD // BLOCK
        extent = 0
        while records > 0 or extent == 0:
            rc = min(EXTENT_RECORDS, records)
            blocks = list(range(first + extent * per_extent,
                                first + extent * per_extent + (rc * RECORD + BLOCK - 1) // BLOCK))
            if self.next_entry >= DIR_ENTRIES:
                raise SystemExit("mkdisk: directory full")
            entry = struct.pack('<B8s3sBBBB16B', 0, name.ljust(8), bytes(ext_bytes), extent & 0x1F, 0, extent >> 5,
                                rc, *(blocks + [0] * (16 - len(blocks))))
            off = block_offset(0) + self.next_entry * 32
            self.img[off:off + 32] = entry
            self.next_entry += 1
            records -= rc
            extent += 1
        return first


def c_image(obj, name):
    """the bytes of <name>_image in a compiled src-c/rsp object"""
    nm = subprocess.run(['i686-elf-nm', '-S', obj], capture_output=True, text=True, check=True).stdout
    for line in nm.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] == name + '_image':
            off, size = int(parts[0], 16), int(parts[1], 16)
            break
    else:
        raise SystemExit(f"mkdisk: no {name}_image in {obj}")
    out = obj + '.rodata'
    subprocess.run(['i686-elf-objcopy', '-O', 'binary', '-j', '.rodata', obj, out], check=True)
    data = open(out, 'rb').read()[off:off + size]
    os.remove(out)
    return data


# the transients: (name, the directory with their C and header)
TRANSIENTS = [('SUBMIT', 'rsp'), ('RANDOM', 'rsp'), ('DIR', 'utils'), ('TYPE', 'utils'), ('ERA', 'utils'),
              ('REN', 'utils')]


def code_len(name, where='rsp'):
    text = open(os.path.join(SRC_C, where, 'rsp_syms.h' if where == 'rsp' else 'utils_syms.h')).read()
    return int(re.search(rf'#define {name}_CODE_LEN (0x[0-9A-Fa-f]+)', text).group(1), 16)


def cmd_file(code_bytes, data):
    """a CMD file: the group descriptors (form, paragraphs, base, min, max)
    in a 128-byte header, then the code and data groups -- relocatable
    (base 0, min = its length)"""
    cparas = (code_bytes + 15) // 16
    dparas = (len(data) + 15) // 16
    header = struct.pack('<BHHHH BHHHH', 1, cparas, 0, cparas, 0, 2, dparas, 0, dparas, 0).ljust(RECORD, b'\0')
    return header + bytes(cparas * 16) + data.ljust(dparas * 16, b'\0')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('-o', '--output', default=os.path.join(SRC_C, 'build', 'mpm86c.img'))
    args = ap.parse_args()
    build = os.path.join(SRC_C, 'build')
    kernel = open(os.path.join(build, 'boot', 'kernel.bin'), 'rb').read()
    boot = open(os.path.join(build, 'boot', 'bootsect.bin'), 'rb').read()

    disk = Disk()
    first = disk.add(b'KERNEL', b'SYS', kernel, read_only=True)
    for name, where in TRANSIENTS:
        data = c_image(os.path.join(build, where, name.lower() + '.o'), name.lower())
        disk.add(name.encode(), b'CMD', cmd_file(code_len(name, where), data))
    disk.add(b'DEMO', b'SUB', DEMO_SUB)

    lba = block_offset(first) // SECTOR
    nsectors = (len(kernel) + SECTOR - 1) // SECTOR
    disk.img[0:SECTOR] = bootsect.patch(boot, lba, nsectors, bootsect.elf_entry(os.path.join(SRC_C, 'kernel.elf')))
    with open(args.output, 'wb') as f:
        f.write(disk.img)
    print(f"mkdisk: {os.path.relpath(args.output, SRC_C)}: KERNEL.SYS ({nsectors} sectors from LBA {lba}), "
          f"{', '.join(n + '.CMD' for n, _ in TRANSIENTS)}, DEMO.SUB; {disk.next_block - 1} of {DSM + 1} blocks, "
          f"{disk.next_entry} of {DIR_ENTRIES} directory entries")


if __name__ == '__main__':
    main()
