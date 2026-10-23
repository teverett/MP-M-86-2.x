#!/usr/bin/env python3
"""Differential test for src-c/kernel/parse.c against src/kernel/PARSE.SUP's
parse_entry (and, through it, its internal setfld/delim/deblank/fcbinit)."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'parse_wrapper.A86')
PARSE_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'parse.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
UDA_WRKSEG = 0xA012
PCB, NAMEPTR, FCBPTR = 0x3000, 0x4000, 0x5000
FCB_LEN = 32


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['parse_entry'].value
    return code, entry


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, PARSE_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory(name_bytes):
    mem = bytearray(0x10000)
    mem[UDA_WRKSEG] = 0x34
    mem[UDA_WRKSEG + 1] = 0x12
    mem[PCB:PCB + 2] = NAMEPTR.to_bytes(2, 'little')
    mem[PCB + 2:PCB + 4] = FCBPTR.to_bytes(2, 'little')
    mem[NAMEPTR:NAMEPTR + len(name_bytes)] = name_bytes
    mem[FCBPTR:FCBPTR + FCB_LEN] = b'\xcc' * FCB_LEN  # poison, to prove fcbinit really runs
    return mem


def run_reference(code, entry, mem):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {'dx': PCB, 'sp': STACK_TOP}, entry)
    sim.run()
    return (sim.regs['bx'], sim.regs['cx']), bytes(image[FCBPTR:FCBPTR + FCB_LEN])


def run_port(lib, mem):
    class BxCx(ctypes.Structure):
        _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

    lib.parse_entry.restype = BxCx
    lib.parse_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    r = lib.parse_entry(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), ctypes.c_uint16(PCB))
    return (r.bx, r.cx), bytes(buf)[FCBPTR:FCBPTR + FCB_LEN]


CASES = [
    ("simple name.type", b"FOO.BAR\x00"),
    ("name only, NUL-terminated", b"FOO\x00"),
    ("name only, CR-terminated", b"FOO\r"),
    ("leading blanks", b"   FOO.BAR\x00"),
    ("with drive letter", b"B:FOO.BAR\x00"),
    ("lowercase gets upper-cased", b"foo.bar\x00"),
    ("wildcard name", b"*.BAR\x00"),
    ("wildcard type", b"FOO.*\x00"),
    ("with password", b"FOO.BAR;SECRET\x00"),
    ("more to parse (comma-separated)", b"FOO.BAR,BAZ.QUX\x00"),
    ("empty string", b"\x00"),
    ("illegal drive letter", b"9:FOO\x00"),
    ("drive letter out of range", b"[:FOO\x00"),
    ("name too long", b"TOOLONGNAME.BAR\x00"),
    ("type too long", b"FOO.TOOLONG\x00"),
    ("illegal character", b"FOO\x01BAR\x00"),
    ("drive then illegal delim", b"A:.FOO\x00"),
]


def main():
    code, entry = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        failures = []
        for label, name_bytes in CASES:
            mem = base_memory(name_bytes)
            ref_regs, ref_fcb = run_reference(code, entry, mem)
            port_regs, port_fcb = run_port(lib, mem)
            status = "ok" if (ref_regs, ref_fcb) == (port_regs, port_fcb) else "FAIL"
            print(f"[{status}] {label}: reference bx={ref_regs[0]:#06x} cx={ref_regs[1]:#06x} "
                  f"| port bx={port_regs[0]:#06x} cx={port_regs[1]:#06x}")
            if ref_fcb != port_fcb:
                print(f"    FCB mismatch: ref={ref_fcb!r} port={port_fcb!r}")
            if status == "FAIL":
                failures.append(label)
        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {len(CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
