#!/usr/bin/env python3
"""The /src-assem offsets src-c keeps in its own headers, so its build doesn't
need /src-assem: src-c/rt/rt_syms.h (SUP/RTM code offsets the runtime hands
to the module inits), src-c/rsp/rsp_syms.h (the src-assem/rsp programs' data
and message offsets the C versions are laid out by) and
src-c/gensys/gensys_syms.h (SYSDAT's and the XIOS's data labels, the
modules' code lengths and init/entry offsets). Each constant is
`#define NAME 0xVALUE /* FILE.A86: label */` (or `FILE.A86: cseg
length` / `dseg length`); this assembles FILE.A86 (from src-assem/kernel or
src-assem/rsp) with tools/asm86 and checks VALUE, so a change on either side
fails here instead of silently drifting.
"""
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_C = os.path.join(REPO_ROOT, 'src-c')
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))
from asm86 import Assembler  # noqa: E402

HEADERS = [os.path.join(SRC_C, 'rt', 'rt_syms.h'), os.path.join(SRC_C, 'rsp', 'rsp_syms.h'),
           os.path.join(SRC_C, 'gensys', 'gensys_syms.h')]
SRC_DIRS = [os.path.join(REPO_ROOT, 'src-assem', 'kernel'), os.path.join(REPO_ROOT, 'src-assem', 'rsp')]
DEFINE_RE = re.compile(r'^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+)\s+/\*\s*(\S+\.A86):\s*(.+?)\s*\*/', re.MULTILINE)


def symbols(src):
    """{label (lower case): offset, 'cseg length': n, 'dseg length': n}"""
    path = next(os.path.join(d, src) for d in SRC_DIRS if os.path.exists(os.path.join(d, src)))
    asm = Assembler()
    asm.assemble(path, [os.path.dirname(path)])
    table = {k.lower(): v.value for k, v in asm.symtab.items() if v.kind == 'label'}
    table['cseg length'] = len(asm.segment_bytes('cseg'))
    table['dseg length'] = len(asm.segment_bytes('dseg'))
    return table


def main():
    failures = []
    checked = 0
    tables = {}
    for header in HEADERS:
        text = open(header).read()
        defines = DEFINE_RE.findall(text)
        bare = set(re.findall(r'^#define\s+(\w+)\s+0x', text, re.MULTILINE)) - {d[0] for d in defines}
        for name in sorted(bare):  # every value needs its source label
            print(f"[FAIL] {os.path.basename(header)}: {name} has no /* FILE.A86: label */ comment")
            failures.append(name)
        for name, value, src, label in defines:
            if src not in tables:
                tables[src] = symbols(src)
            got = tables[src].get(label.lower())
            ok = got == int(value, 16)
            checked += 1
            if not ok:
                print(f"[FAIL] {name} = {value}, but {src}'s {label} is "
                      f"{'missing' if got is None else f'0x{got:04X}'}")
                failures.append(name)
    print(f"{checked} constants checked against the assembled source.")
    if failures:
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
