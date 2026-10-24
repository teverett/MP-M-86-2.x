#!/usr/bin/env python3
"""Differential test for src-c/kernel/supif.c against src/kernel/
SUPIF.SUP's `func1`. See supif.c's own header comment for what's not
ported (`user_entry`/`func`/`entry`/`mpmif`/`xiosif`) and why.

`func1` has a genuine "DS = sysdat segment" precondition (same class as
src-c/kernel/file1.c's own BD_* fields), so this test runs the reference
with `tools/i86sim.py`'s `segmented=True` mode and DS set to a real
segment value (DS_SYSDAT); `module_map`/`sysent` are written directly at
their real small SYSDAT.FMT offsets (0x46/0xA0) relative to that
segment's own physical base, not at the wrapper's own dseg placement of
those fields (which only exists to make the file assemble -- nothing in
that dseg blob is read by this test).

`func1` always stops at the same `inmod:` address regardless of which
branch it took to get there (ch != 0 jumps straight there; ch == 0 falls
through the network check, always taken per supif.c's own header comment
-- no network module exists anywhere in /src, so module_map's own
netmod_bit is always 0) -- AL (mod) and CL (func) at that point are
exactly this routine's own output.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'supif_wrapper.A86')
SUPIF_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'supif.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
DS_SYSDAT = 0x4321
SD_MODULE_MAP, SD_SYSENT, ENT_MOD, ENT_FUNC, ENTLEN = 0x46, 0xA0, 0, 1, 3


def phys(seg, offset=0):
    return (seg * 16 + offset) & 0xFFFF


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['func1'].value
    inmod = asm.symtab['inmod'].value
    return code, entry, inmod


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, SUPIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, inmod, mem, ch, cl):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {'cx': (ch << 8) | cl, 'ds': DS_SYSDAT, 'sp': STACK_TOP}, entry,
                 stop_addrs={inmod}, segmented=True)
    sim.run()
    return sim.regs


def base_memory(sysent_entries):
    mem = bytearray(0x10000)
    for func_num, mod, func in sysent_entries:
        si = phys(DS_SYSDAT, SD_SYSENT + func_num * ENTLEN)
        mem[si + ENT_MOD] = mod
        mem[si + ENT_FUNC] = func
    return mem


def run_port(lib, mem, ch, cl):
    buf = (ctypes.c_uint8 * 0x10000)(*mem)

    class ModFunc(ctypes.Structure):
        _fields_ = [('mod', ctypes.c_uint8), ('func', ctypes.c_uint8)]

    lib.func1.restype = ModFunc
    lib.func1.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
    r = lib.func1(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), DS_SYSDAT, ch, cl)
    return r.mod, r.func


CASES = [
    ("ch != 0 (explicit module/func pair)", 2, 7, []),
    ("ch == 0, user func 0 -> sysent lookup", 0, 0, [(0, 1, 5)]),
    ("ch == 0, user func 12 -> sysent lookup", 0, 12, [(12, 4, 9)]),
]


def main():
    code, entry, inmod = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        failures = []
        for label, ch, cl, sysent_entries in CASES:
            mem = base_memory(sysent_entries)
            ref_regs = run_reference(code, entry, inmod, mem, ch, cl)
            ref = (ref_regs['ax'] & 0xFF, ref_regs['cx'] & 0xFF)
            port = run_port(lib, mem, ch, cl)
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference (mod={ref[0]}, func={ref[1]}) | port (mod={port[0]}, func={port[1]})")
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
