#!/usr/bin/env python3
"""Differential test for src-c/kernel/findpd.c against src/kernel/FINDPD.RTM.

Tier 0 proof-of-concept for the /src-c plan (docs/src-c.md): assembles the
real routine with tools/asm86.py, runs the resulting bytes through
tools/i86sim.py, compiles the C port and calls it (via ctypes) against the
*same* synthetic memory image, and diffs the outcome -- not the bytes. See
docs/src-c.md for why.

Usage: python3 tests/src-c/test_findpd.py
"""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'findpd_wrapper.A86')
FINDPD_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'findpd.c')
FINDPD_H_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

CODE_ADDR = 0x0000
STACK_TOP = 0xFFF0
PDTAB_BASE = 0x1000
PD_LEN = 0x30
P_THREAD = 0x02
P_NAME = 0x08
NAME_ADDR = 0x4000
THREAD_ROOT_ADDR = 0x5000  # arbitrary word holding the head-of-list pointer


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['findpdname_entry'].value
    if entry != 0:
        raise AssertionError(f"expected findpdname_entry at 0 in the isolated build, got {entry:#x}")
    return code


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', FINDPD_H_DIR, FINDPD_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def make_memory(names_in_list, name_to_find):
    """Builds a flat 64KB image: a linked PD thread-list of the given names,
    plus a name buffer holding name_to_find. Returns (mem, bx_root, name_addr).
    """
    mem = bytearray(0x10000)

    pd_addrs = [PDTAB_BASE + i * PD_LEN for i in range(len(names_in_list))]
    for i, (addr, name) in enumerate(zip(pd_addrs, names_in_list)):
        next_addr = pd_addrs[i + 1] if i + 1 < len(pd_addrs) else 0
        mem[addr + P_THREAD] = next_addr & 0xFF
        mem[addr + P_THREAD + 1] = (next_addr >> 8) & 0xFF
        padded = (name + b'\x00' * 8)[:8]
        mem[addr + P_NAME:addr + P_NAME + 8] = padded

    head = pd_addrs[0] if pd_addrs else 0
    mem[THREAD_ROOT_ADDR] = head & 0xFF
    mem[THREAD_ROOT_ADDR + 1] = (head >> 8) & 0xFF
    bx_root = (THREAD_ROOT_ADDR - P_THREAD) & 0xFFFF

    padded_find = (name_to_find + b'\x00' * 8)[:8]
    mem[NAME_ADDR:NAME_ADDR + 8] = padded_find

    return mem, bx_root, NAME_ADDR


def run_reference(code, mem, bx_root, name_addr):
    image = bytearray(mem)
    image[CODE_ADDR:CODE_ADDR + len(code)] = code
    sim = I86Sim(image, {'bx': bx_root, 'dx': name_addr, 'sp': STACK_TOP}, CODE_ADDR)
    sim.run()
    return sim.regs['bx'], sim.regs['cx']


def run_port(lib_path, mem, bx_root, name_addr):
    lib = ctypes.CDLL(lib_path)
    lib.findpdname_entry.restype = ctypes.c_uint16
    lib.findpdname_entry.argtypes = [
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16,
        ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint16),
    ]
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    mem_ptr = ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8))
    name_ptr = ctypes.cast(ctypes.byref(buf, name_addr), ctypes.POINTER(ctypes.c_uint8))
    cx = ctypes.c_uint16()
    bx = lib.findpdname_entry(mem_ptr, ctypes.c_uint16(bx_root), name_ptr, ctypes.byref(cx))
    return bx, cx.value


CASES = [
    ("empty list", [], b"ECHO"),
    ("single entry, found", [b"ECHO"], b"ECHO"),
    ("single entry, not found", [b"ECHO"], b"TMP"),
    ("middle of list, found", [b"TMP", b"ECHO", b"CLOCK"], b"ECHO"),
    ("end of list, found", [b"TMP", b"CLOCK", b"ECHO"], b"ECHO"),
    ("not found, falls off list", [b"TMP", b"CLOCK"], b"ECHO"),
    ("bit-7-only difference still matches", [b"ECHO"], bytes([c | 0x80 for c in b"ECHO"])),
]


def main():
    code = assemble_reference()
    lib_path = build_c_library()
    try:
        failures = []
        for label, names_in_list, name_to_find in CASES:
            mem, bx_root, name_addr = make_memory(names_in_list, name_to_find)
            ref_bx, ref_cx = run_reference(code, mem, bx_root, name_addr)
            port_bx, port_cx = run_port(lib_path, mem, bx_root, name_addr)
            status = "ok" if (ref_bx, ref_cx) == (port_bx, port_cx) else "FAIL"
            print(f"[{status}] {label}: reference bx={ref_bx:#06x} cx={ref_cx:#06x} "
                  f"| port bx={port_bx:#06x} cx={port_cx:#06x}")
            if status == "FAIL":
                failures.append(label)
        if failures:
            print(f"\n{len(failures)}/{len(CASES)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {len(CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
