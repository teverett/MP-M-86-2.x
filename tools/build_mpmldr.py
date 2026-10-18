#!/usr/bin/env python3
"""Combine LDMPM.A86 + LDBDOS.A86 + LDBIOS86Q.A86 into one MPMLDR image.

LDBIOS86Q.A86 is this project's QEMU-targeted replacement for the real
LDBIOS.A86 (standard PC UART + NEC765 floppy controller instead of the
iSBC 86/12's i8251/i8271 hardware -- see that file's own header comment
for the rationale, which mirrors src/kernel/XIOS86Q.A86's).

The real DRI build (orig/mpm862sr/04/MKALL.SUB) does this as:

    pip mpmldr.h86=ldmpm.h86,ldbdos.h86,ldbios.h86
    gencmd mpmldr 8080 code[a0400]

i.e. concatenate the three modules' assembled hex at DRI's own ASM86's
fixed offsets (LDBDOS at 0406h, LDBIOS at 1200h, both hard-coded in
LDMPM.A86 and used there to patch an interrupt-table entry and to reach
LDBIOS's own init code) and run GENCMD. There is no GENCMD here, and
more importantly those two fixed offsets assume DRI's own ASM86's more
compact code -- tools/asm86 (always disp16, always near JMP/CALL; see
tools/README.md) assembles LDBDOS.A86 bigger than the 3578-byte budget
DRI's fixed layout left for it.

This script does the same job a different way: assemble all three
modules, lay them out *sequentially* instead of at fixed offsets, and
keep LDMPM.A86's own ldbdos_offset/mldbios_offset constants (the only
place those fixed numbers are actually used) in sync with wherever the
pieces actually land -- rewriting that one source file in place if the
sizes have drifted since it was last correct (e.g. after a future
tools/asm86 change), then re-assembling it, before concatenating.

LDBDOS.A86 turned out to need the exact same treatment, for a related
but distinct reason: unlike LDMPM.A86, it used to compute its *own*
absolute position via an internal `org bdosoffset` (DRI's original
fixed value, 400h) -- harmless under DRI's own Intel-hex-based build,
but under tools/asm86's raw-binary output this padded LDBDOS.cseg.bin
with literal zero bytes from file offset 0 up to bdosoffset, which this
script's sequential placement then stacked *on top of*, landing the
real entry code 0x400 bytes past where LDMPM's own patched
ldbdos_offset actually pointed (confirmed by inspecting the combined
MPMLDR.bin -- see docs/bugs.md's LDBDOS entry). LDBDOS.A86's own
bdosoffset is now permanently 0 (no more internal self-positioning,
sequencing is this script's job alone); its biosoffset (an absolute
CALL/JMP target into LDBIOS86Q.A86's jump table) and bdoscodesize (used
to place LDBDOS's own DSEG data right after its real CSEG) are
self-corrected here the same way LDMPM.A86's constants are.

Usage: python3 tools/build_mpmldr.py [-o OUTPUT]
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOADER_DIR = os.path.join(ROOT, 'src', 'loader')
ASM86 = [sys.executable, os.path.join(ROOT, 'tools', 'asm86.py')]

OFFSET_RE = re.compile(
    r'^(ldbdos_offset\s+equ\s+)([0-9a-fA-F]+h)(\s*;.*)?$',
    re.MULTILINE,
)
MLDBIOS_RE = re.compile(
    r'^(mldbios_offset\s+equ\s+)([0-9a-fA-F]+h)(\s*;.*)?$',
    re.MULTILINE,
)
BIOSOFFSET_RE = re.compile(
    r'^(biosoffset\s+equ\s+)([0-9a-fA-F]+[hH])(\s*;.*)?$',
    re.MULTILINE,
)
BDOSCODESIZE_RE = re.compile(
    r'^(bdoscodesize\s+equ\s+)([0-9a-fA-F]+[hH])(\s*;.*)?$',
    re.MULTILINE,
)


def assemble(name, out_dir):
    """Assemble src/loader/<name>.A86, returning its .cseg.bin path and size."""
    src = os.path.join(LOADER_DIR, name + '.A86')
    out_base = os.path.join(out_dir, name)
    result = subprocess.run(ASM86 + [src, '-o', out_base],
                             capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"asm86 failed assembling {name}.A86")
    cseg_path = out_base + '.cseg.bin'
    size = os.path.getsize(cseg_path) if os.path.exists(cseg_path) else 0
    return cseg_path, size


def hexlit(value):
    """Format `value` as an ASM86 hex literal, with the leading '0' a
    value whose first hex digit is a letter (e.g. 0xe8f) needs so the
    assembler doesn't try to parse it as an identifier starting with
    'e8fh' instead of a number."""
    digits = f'{value:x}'
    if digits[0] in 'abcdef':
        digits = '0' + digits
    return digits + 'h'


def patch_offsets(filename, patches):
    """Rewrite the named loader source's EQU constants in place if they
    don't already match the real, current values. `patches` is a list
    of (pattern, new_value_int, label) triples. Returns True if a
    change was made (meaning the file needs re-assembling)."""
    # newline='' disables universal-newline translation on both read and
    # write -- these files are CRLF (CP/M-era convention) and a plain
    # text-mode round-trip would otherwise silently rewrite every line
    # ending to LF, turning a tiny constant change into a whole-file diff.
    path = os.path.join(LOADER_DIR, filename)
    with open(path, encoding='latin-1', newline='') as f:
        text = f.read()

    changed = False
    for pattern, new_value, label in patches:
        new_val = hexlit(new_value)
        m = pattern.search(text)
        if not m:
            raise SystemExit(f"could not find {label} in {filename}")
        if m.group(2).lower() != new_val.lower():
            changed = True
            text = pattern.sub(lambda mm: mm.group(1) + new_val + (mm.group(3) or ''),
                                text, count=1)

    if changed:
        with open(path, 'w', encoding='latin-1', newline='') as f:
            f.write(text)
    return changed


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('-o', '--output', default=None,
                     help='output path for the combined image '
                          '(default: build/loader/MPMLDR.bin)')
    args = ap.parse_args()

    out_dir = os.path.join(ROOT, 'build', 'loader')
    os.makedirs(out_dir, exist_ok=True)

    # Pass 1: assemble LDBDOS/LDBIOS to learn their real current sizes
    # (these don't depend on LDMPM's offsets at all, so this is safe to
    # do before LDMPM is known to be correct).
    ldbdos_path, ldbdos_size = assemble('LDBDOS', out_dir)
    ldbios_path, ldbios_size = assemble('LDBIOS86Q', out_dir)

    # LDMPM's own assembled size doesn't depend on the *value* of
    # ldbdos_offset/mldbios_offset either (they're used as a stored
    # data value and a dangling, nothing-emitted org-jump respectively
    # -- see the comment above LDMPM.A86's own declaration of them) --
    # so it's safe to assemble it once, check/patch, and only
    # re-assemble if a patch was actually needed.
    ldmpm_path, ldmpm_size = assemble('LDMPM', out_dir)

    ldbdos_offset = ldmpm_size
    mldbios_offset = ldmpm_size + ldbdos_size

    if patch_offsets('LDMPM.A86', [
        (OFFSET_RE, ldbdos_offset, 'ldbdos_offset'),
        (MLDBIOS_RE, mldbios_offset, 'mldbios_offset'),
    ]):
        print(f"build_mpmldr: patched LDMPM.A86's ldbdos_offset="
              f"{ldbdos_offset:#x}, mldbios_offset={mldbios_offset:#x} "
              "to match the current assembled sizes")
        ldmpm_path, ldmpm_size = assemble('LDMPM', out_dir)

    # LDBDOS.A86's own biosoffset (an absolute CALL/JMP target into
    # LDBIOS86Q's jump table) and bdoscodesize (where its own DSEG data
    # starts, right after its real CSEG) don't affect its assembled
    # CSEG size -- they're baked in as fixed-width immediate operands
    # regardless of value -- so ldbdos_size above is already final; one
    # patch-and-reassemble pass is enough, same as LDMPM.A86 above.
    if patch_offsets('LDBDOS.A86', [
        (BIOSOFFSET_RE, mldbios_offset, 'biosoffset'),
        (BDOSCODESIZE_RE, ldbdos_size, 'bdoscodesize'),
    ]):
        print(f"build_mpmldr: patched LDBDOS.A86's biosoffset="
              f"{mldbios_offset:#x}, bdoscodesize={ldbdos_size:#x} "
              "to match the current assembled sizes")
        ldbdos_path, ldbdos_size_new = assemble('LDBDOS', out_dir)
        assert ldbdos_size_new == ldbdos_size, (
            "LDBDOS.A86's assembled size changed after patching "
            "biosoffset/bdoscodesize -- it shouldn't (see comment above)")

    out_path = args.output or os.path.join(out_dir, 'MPMLDR.bin')
    with open(out_path, 'wb') as out:
        for path, size, name in (
            (ldmpm_path, ldmpm_size, 'LDMPM'),
            (ldbdos_path, ldbdos_size, 'LDBDOS'),
            (ldbios_path, ldbios_size, 'LDBIOS86Q'),
        ):
            with open(path, 'rb') as f:
                data = f.read()
            assert len(data) == size
            out.write(data)
            print(f"build_mpmldr: {name:8s} {size:6d} bytes "
                  f"@ {out.tell() - size:#06x}")

    total = os.path.getsize(out_path)
    print(f"build_mpmldr: wrote {out_path} ({total} bytes)")


if __name__ == '__main__':
    main()
