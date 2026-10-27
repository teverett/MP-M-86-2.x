#!/usr/bin/env python3
"""The C versions of src-assem/rsp/'s programs (src-c/rsp/*.c) each carry
their data segment as one initialized C object, laid out as the .A86's
DSEG (src-c/rsp/rsp.h). This compares each of those objects, as
i686-elf-gcc compiled it (the symbol's bytes in the .o's .rodata), with
the assembled data segment: the .A86 assembled with tools/asm86 here,
or for CACHE (no source) the data group of src-assem/rsp/CACHE.RSP.

The only bytes allowed to differ are an RSP's start IP (the first word
of its initial stack frame, at the UDA's u_stack_sp): a code address
only the 8086 code has, left 0 in the C image (rsp.h). Everything else,
base pages and stacks included, must be identical.

Needs i686-elf-gcc/i686-elf-objcopy/i686-elf-nm (as test_boot.py does);
skips with a message (exit 0) if they're missing.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_C_DIR = os.path.join(REPO_ROOT, 'src-c')
RSP_DIR = os.path.join(REPO_ROOT, 'src-assem', 'rsp')
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

REQUIRED_TOOLS = ('i686-elf-gcc', 'i686-elf-objcopy', 'i686-elf-nm')

# (C file / symbol prefix, its source, RSP?)
PROGRAMS = (
    ('tmp', 'TMP.A86', True),
    ('clock', 'CLOCK.A86', True),
    ('echo', 'ECHO.A86', True),
    ('cache', 'CACHE.RSP', True),
    ('submit', 'SUBMIT.A86', False),
    ('random', 'RANDOM.A86', False),
)


def c_image(name):
    """The bytes of <name>_image in build/rsp/<name>.o."""
    obj = os.path.join(SRC_C_DIR, 'build', 'rsp', name + '.o')
    nm = subprocess.run(['i686-elf-nm', '-S', obj], capture_output=True, text=True, check=True).stdout
    for line in nm.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] == name + '_image':
            off, size = int(parts[0], 16), int(parts[1], 16)
            break
    else:
        raise RuntimeError(f'{name}_image not in {obj}')
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, 'rodata.bin')
        subprocess.run(['i686-elf-objcopy', '-O', 'binary', '-j', '.rodata', obj, out], check=True)
        with open(out, 'rb') as f:
            return f.read()[off:off + size]


def asm_dseg(source, tmp):
    """The assembled data segment of src-assem/rsp/<source>."""
    if source.upper().endswith('.RSP'):
        from build_mpmsys import binary_rsp
        return binary_rsp(os.path.join(RSP_DIR, source))[1]
    stem = os.path.join(tmp, os.path.splitext(source)[0])
    subprocess.run([sys.executable, os.path.join(REPO_ROOT, 'tools', 'asm86.py'),
                    os.path.join(RSP_DIR, source), '-o', stem], check=True, capture_output=True)
    with open(stem + '.dseg.bin', 'rb') as f:
        return f.read()


def main():
    missing = [t for t in REQUIRED_TOOLS if shutil.which(t) is None]
    if missing:
        print(f"skip: missing tool(s) {', '.join(missing)}")
        return 0
    objs = [os.path.join('build', 'rsp', name + '.o') for name, _, _ in PROGRAMS]
    subprocess.run(['make'] + objs, cwd=SRC_C_DIR, check=True, stdout=subprocess.DEVNULL)

    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        for name, source, is_rsp in PROGRAMS:
            c = c_image(name)
            a = asm_dseg(source, tmp)
            allowed = set()
            if is_rsp:
                uda = struct.unpack_from('<H', a, 0x10 + 0x10)[0] * 16  # the PD's p_uda
                frame = struct.unpack_from('<H', a, uda + 0x34)[0]     # u_stack_sp
                allowed = {frame, frame + 1}
            diffs = [i for i in range(min(len(a), len(c))) if a[i] != c[i] and i not in allowed]
            problems = []
            if len(a) != len(c):
                problems.append(f'length {len(c):#x}, assembled {len(a):#x}')
            if diffs:
                problems.append(f'{len(diffs)} byte(s) differ, first at {diffs[0]:#06x} '
                                f'(C {c[diffs[0]]:#04x}, assembled {a[diffs[0]]:#04x})')
            status = 'FAIL' if problems else 'ok'
            note = '; '.join(problems) if problems else (
                f'{len(c):#x} bytes identical' + (f' (start IP at {min(allowed):#x}: C 0, '
                                                  f'assembled {struct.unpack_from("<H", a, min(allowed))[0]:#x})'
                                                  if allowed else ''))
            print(f'[{status}] {name}_image vs {source}: {note}')
            if problems:
                failures.append(name)
    if failures:
        print(f'\n{len(failures)} case(s) failed: {", ".join(failures)}')
        return 1
    print(f'\nall {len(PROGRAMS)} case(s) match.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
