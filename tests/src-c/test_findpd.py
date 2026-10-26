#!/usr/bin/env python3
"""Differential test for src-c/kernel/findpd.c against src/kernel/FINDPD.RTM,
on real memory layout.

findpd.c addresses real memory (defs.h's far_ptr), so the reference runs
with tools/i86sim.py's segmented=True: tests/src-c/fixtures/
findpd_wrapper.A86 includes the real SYSDAT.FMT/UDA.FMT, SYSDAT is at
segment 100h (DS) with the PDs and the real thread root (`thrdrt`) at
SYSDAT-relative offsets, the running process's UDA is the reference's ES
and its PD's p_uda, and the name being looked up sits in that UDA's
u_wrkseg. Every case diffs the whole image (the routine writes nothing).

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
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
SYSDAT = 0x0100
UDA = 0x0600
WRKSEG = 0x0700
PD_RUN = 0x2800      # rlr (SYSDAT offset)
PDTAB_BASE = 0x2000  # thread-list PDs (SYSDAT offsets)
PD_LEN = 0x30
P_THREAD, P_NAME, P_UDA = 0x02, 0x08, 0x10
NAME_OFF = 0x0100    # name being looked up, in u_wrkseg


def phys(seg, off=0):
    return seg * 16 + off


def sd(off):
    return phys(SYSDAT, off)


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in ('findpdname_entry', 'rlr', 'thrdrt', 'u_wrkseg')}
    if sym['findpdname_entry'] != 0:
        raise AssertionError(f"expected findpdname_entry at 0, got {sym['findpdname_entry']:#x}")
    return code, sym


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, FINDPD_C, '-o', lib_path],
        check=True,
    )
    return lib_path


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
    code, sym = assemble_reference()
    skip = set(range(len(code))) | set(range(0xFF00, SIM_MEM))  # code, harness stack (SS=0)

    def make_memory(names_in_list, name_to_find):
        mem = bytearray(SIM_MEM)
        w16(mem, sd(sym['rlr']), PD_RUN)
        w16(mem, sd(PD_RUN + P_UDA), UDA)
        w16(mem, phys(UDA, sym['u_wrkseg']), WRKSEG)
        pd_addrs = [PDTAB_BASE + i * PD_LEN for i in range(len(names_in_list))]
        for i, (addr, name) in enumerate(zip(pd_addrs, names_in_list)):
            w16(mem, sd(addr + P_THREAD), pd_addrs[i + 1] if i + 1 < len(pd_addrs) else 0)
            mem[sd(addr + P_NAME):sd(addr + P_NAME) + 8] = (name + b'\x00' * 8)[:8]
        w16(mem, sd(sym['thrdrt']), pd_addrs[0] if pd_addrs else 0)
        mem[phys(WRKSEG, NAME_OFF):phys(WRKSEG, NAME_OFF) + 8] = (name_to_find + b'\x00' * 8)[:8]
        return mem

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.findpdname_entry.restype = ctypes.c_uint16
        lib.findpdname_entry.argtypes = [
            ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint16,
            ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16),
        ]
        bx_root = (sym['thrdrt'] - P_THREAD) & 0xFFFF
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        for label, names_in_list, name_to_find in CASES:
            mem = make_memory(names_in_list, name_to_find)
            image = bytearray(mem)
            image[0:len(code)] = code
            sim = I86Sim(image, {'bx': bx_root, 'dx': NAME_OFF, 'ds': SYSDAT, 'es': UDA,
                                 'ss': 0, 'sp': STACK_TOP, 'cs': 0},
                         0, segmented=True)
            sim.run()
            buf = (ctypes.c_uint8 * PHYS_MEM)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            cx = ctypes.c_uint16()
            bx = lib.findpdname_entry(buf, SYSDAT, bx_root, NAME_OFF, ctypes.byref(cx))
            port = bytes(buf)
            check(label, (hex(sim.regs['bx']), hex(sim.regs['cx'])), (hex(bx), hex(cx.value)))
            diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and image[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
