#!/usr/bin/env python3
"""Combine LDMPM.A86 + LDBDOS.A86 + LDBIOS86Q.A86 into one MPMLDR image.

LDBIOS86Q.A86 is this project's QEMU-targeted replacement for the real
LDBIOS.A86 (standard PC UART + NEC765 floppy controller instead of the
iSBC 86/12's i8251/i8271 hardware -- see that file's own header comment
for the rationale, which mirrors src-assem/kernel/XIOS86Q.A86's).

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

LDBDOS.A86 and LDBIOS86Q.A86 both need the same fix as each other now
(an earlier version of this script gave LDBDOS.A86 the *opposite*
fix -- zeroing its self-positioning `org` -- which traded one addressing
bug for another; see LDBDOS.A86's own header comment for the story):
every plain `offset <label>` reference to a module's own local data (or
plain direct `jmp`/`call` to another module's absolute position) is
only correct if that module's `cseg` is really `org`'d at its own real
position in the concatenated MPMLDR.bin (`ldbdos_offset`/
`mldbios_offset` respectively) rather than at 0 -- see LDBIOS86Q.A86's
own header comment for how this was first found (a stack pointer
landing bytes too low, corrupting another module's code). So this
script keeps both modules' internal self-positioning `org`, and after
patching that module's own offset constant to match, strips the
resulting leading zero-padding (asm86 pads a raw binary from 0 up to
any `org` target) before concatenating its real content.

Usage: python3 tools/build_mpmldr.py [-o OUTPUT]
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOADER_DIR = os.path.join(ROOT, 'src-assem', 'loader')
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
BDOSOFFSET_RE = re.compile(
    r'^(bdosoffset\s+equ\s+)([0-9a-fA-F]+[hH])(\s*;.*)?$',
    re.MULTILINE,
)


def assemble(name, out_dir):
    """Assemble src-assem/loader/<name>.A86, returning its .cseg.bin path and size."""
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


def strip_org_padding(path, offset, label):
    """Read the assembled file at `path` and strip the `offset` leading
    zero-padding bytes asm86 emits for a module whose cseg starts with
    `org <self-position>` instead of `org 0` -- see LDBDOS.A86's and
    LDBIOS86Q.A86's own header comments for why they need this."""
    with open(path, 'rb') as f:
        data = f.read()
    assert len(data) > offset, (
        f"{label} assembled to {len(data)} bytes, not even past its "
        f"own org target ({offset:#x})")
    assert data[:offset] == b'\0' * offset, (
        f"{label}'s leading org-padding wasn't all zero -- something "
        "is emitting bytes before its own org target")
    return data[offset:]


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

    out_dir = os.path.join(ROOT, 'src-assem', 'build', 'loader')
    os.makedirs(out_dir, exist_ok=True)

    # LDMPM's own assembled size doesn't depend on the *value* of
    # ldbdos_offset/mldbios_offset (they're used as a stored data value
    # and a dangling, nothing-emitted org-jump respectively -- see the
    # comment above LDMPM.A86's own declaration of them), so it's safe
    # to assemble it first and learn ldbdos_offset (LDBDOS.A86's own
    # real position) before LDBDOS.A86 itself is known to be correct.
    ldmpm_path, ldmpm_size = assemble('LDMPM', out_dir)
    ldbdos_offset = ldmpm_size

    # Patch LDBDOS.A86's own bdosoffset (its self-positioning `org`
    # target -- see that file's header comment) and biosoffset (an
    # absolute CALL/JMP target into LDBIOS86Q's jump table) to match,
    # assemble, and strip the resulting leading padding to learn its
    # real content size. bdosoffset itself doesn't affect that size
    # (it's only ever used positionally, plus one dead/commented-out
    # line) -- but biosoffset can: tools/asm86 does real jump-size
    # relaxation (docs/bugs.md #6), so a CALL/JMP to biosoffset can
    # encode shorter or longer depending on its actual value, which
    # changes mldbios_offset (biosoffset's own target), in a loop. Keep
    # patching both against the latest guess and re-measuring until it
    # stops moving -- confirmed to need a second pass in practice, so
    # don't assume one pass is enough here the way LDMPM.A86's patch
    # above safely does.
    mldbios_offset = ldbdos_offset  # first guess, corrected below
    for _ in range(5):
        patch_offsets('LDBDOS.A86', [
            (BDOSOFFSET_RE, ldbdos_offset, 'bdosoffset'),
            (BIOSOFFSET_RE, mldbios_offset, 'biosoffset'),
        ])
        ldbdos_path, _ = assemble('LDBDOS', out_dir)
        ldbdos_bytes = strip_org_padding(ldbdos_path, ldbdos_offset, 'LDBDOS.A86')
        new_mldbios_offset = ldbdos_offset + len(ldbdos_bytes)
        if new_mldbios_offset == mldbios_offset:
            break
        mldbios_offset = new_mldbios_offset
    else:
        raise SystemExit(
            "LDBDOS.A86's content size kept changing after patching "
            "bdosoffset/biosoffset -- didn't converge in 5 passes")
    ldbdos_size = len(ldbdos_bytes)

    if patch_offsets('LDMPM.A86', [
        (OFFSET_RE, ldbdos_offset, 'ldbdos_offset'),
        (MLDBIOS_RE, mldbios_offset, 'mldbios_offset'),
    ]):
        print(f"build_mpmldr: patched LDMPM.A86's ldbdos_offset="
              f"{ldbdos_offset:#x}, mldbios_offset={mldbios_offset:#x} "
              "to match the current assembled sizes")
        ldmpm_path, ldmpm_size = assemble('LDMPM', out_dir)

    # LDBIOS86Q.A86's own mldbios_offset (see that file's header
    # comment) is now known -- patch it to match, assemble, and strip
    # its leading padding the same way.
    patch_offsets('LDBIOS86Q.A86', [
        (MLDBIOS_RE, mldbios_offset, 'mldbios_offset'),
    ])
    ldbios_path, _ = assemble('LDBIOS86Q', out_dir)
    ldbios_bytes = strip_org_padding(ldbios_path, mldbios_offset, 'LDBIOS86Q.A86')

    out_path = args.output or os.path.join(out_dir, 'MPMLDR.bin')
    with open(out_path, 'wb') as out:
        with open(ldmpm_path, 'rb') as f:
            ldmpm_data = f.read()
        assert len(ldmpm_data) == ldmpm_size
        out.write(ldmpm_data)
        print(f"build_mpmldr: {'LDMPM':8s} {ldmpm_size:6d} bytes "
              f"@ {out.tell() - ldmpm_size:#06x}")
        out.write(ldbdos_bytes)
        print(f"build_mpmldr: {'LDBDOS':8s} {len(ldbdos_bytes):6d} bytes "
              f"@ {out.tell() - len(ldbdos_bytes):#06x}")
        out.write(ldbios_bytes)
        print(f"build_mpmldr: LDBIOS86Q {len(ldbios_bytes):6d} bytes "
              f"@ {out.tell() - len(ldbios_bytes):#06x}")

    total = os.path.getsize(out_path)
    print(f"build_mpmldr: wrote {out_path} ({total} bytes)")


if __name__ == '__main__':
    main()
