#!/usr/bin/env python3
"""Produce a real 512-byte boot sector from src/loader/ROM86Q.A86.

ROM86Q.A86 (see its own header comment) has to assemble as if loaded at
its real runtime address, 0000:7C00h -- `org 7c00h` -- so that every
absolute memory reference its own code makes (e.g. `mov bootdrv,dl`)
encodes the correct address. tools/asm86 implements `org` as literal
zero-byte padding from file offset 0 (the same behavior that caused the
LDBDOS.A86 bug fixed in docs/bugs.md #11), so asm86's own .cseg.bin for
this file is 0x7e00 (32256) bytes: 0x7c00 bytes of leading zero padding
followed by the real 512-byte sector, ending in the 55h/AAh boot
signature `org 7c00h+510` places at the very end.

This script assembles ROM86Q.A86 and slices off just that trailing
512-byte sector -- the only part with real content -- verifying the
boot signature survived the slice, and writes it out as a real,
directly-`dd`-able-to-a-floppy-image boot sector.

Usage: python3 tools/build_bootsect.py [-o OUTPUT]
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOADER_DIR = os.path.join(ROOT, 'src', 'loader')
ASM86 = [sys.executable, os.path.join(ROOT, 'tools', 'asm86.py')]

SECTOR_SIZE = 512
LOAD_ADDR = 0x7c00


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('-o', '--output', default=None,
                     help='output path for the 512-byte boot sector '
                          '(default: build/loader/ROM86Q.bin)')
    args = ap.parse_args()

    out_dir = os.path.join(ROOT, 'build', 'loader')
    os.makedirs(out_dir, exist_ok=True)

    src = os.path.join(LOADER_DIR, 'ROM86Q.A86')
    out_base = os.path.join(out_dir, 'ROM86Q')
    result = subprocess.run(ASM86 + [src, '-o', out_base],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit("asm86 failed assembling ROM86Q.A86")

    with open(out_base + '.cseg.bin', 'rb') as f:
        padded = f.read()

    expected_padded_size = LOAD_ADDR + SECTOR_SIZE
    if len(padded) != expected_padded_size:
        raise SystemExit(
            f"ROM86Q.A86 assembled to {len(padded)} bytes, expected "
            f"exactly {expected_padded_size} ({LOAD_ADDR:#x} bytes of "
            f"org-padding + one {SECTOR_SIZE}-byte sector) -- its own "
            "code+data no longer fits in one boot sector, or the `org "
            "7c00h+510` signature placement has drifted; see this "
            "script's own docstring")
    if any(padded[:LOAD_ADDR]):
        raise SystemExit(
            "ROM86Q.A86's leading org-padding isn't all zero -- some "
            "real content landed before the sector boundary at 0x7c00")

    sector = padded[LOAD_ADDR:]
    if sector[-2:] != b'\x55\xaa':
        raise SystemExit(
            f"ROM86Q.A86's last two bytes are {sector[-2:].hex()}, not "
            "the required 55h/AAh boot signature")

    out_path = args.output or os.path.join(out_dir, 'ROM86Q.bin')
    with open(out_path, 'wb') as f:
        f.write(sector)
    print(f"build_bootsect: wrote {out_path} ({len(sector)} bytes)")


if __name__ == '__main__':
    main()
