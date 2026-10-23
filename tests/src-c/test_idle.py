#!/usr/bin/env python3
"""Differential test for src-c/kernel/idle.c against src/kernel/IDLE.SUP.

`idle:` never returns in the real system -- it tail-jumps into the XIOS
module (`jmp xiosif`), which isn't ported. Per docs/src-c.md's boundary-stub
convention, the test stops the simulator the instant it reaches that jump
target (tools/i86sim.py's `stop_addrs`) and compares the register state it
was about to hand off, rather than a return value.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'idle_wrapper.A86')
IDLE_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'idle.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
PD_ADDR = 0x1000
P_UDA = 0x10
SD_RLR = 0xA000
XIOSIF_STUB = 0x9000


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['idle'].value
    return code, entry


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, IDLE_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def make_memory(uda_es):
    mem = bytearray(0x10000)
    mem[SD_RLR] = PD_ADDR & 0xFF
    mem[SD_RLR + 1] = (PD_ADDR >> 8) & 0xFF
    mem[PD_ADDR + P_UDA] = uda_es & 0xFF
    mem[PD_ADDR + P_UDA + 1] = (uda_es >> 8) & 0xFF
    return mem


def run_reference(code, entry, mem, sysdat_word):
    image = bytearray(mem)
    image[0:len(code)] = code
    image[0] = sysdat_word & 0xFF  # `sysdat dw 0` sits at cseg offset 0
    image[1] = (sysdat_word >> 8) & 0xFF
    sim = I86Sim(image, {'sp': STACK_TOP}, entry, stop_addrs={XIOSIF_STUB})
    sim.run()
    return sim.regs['ds'], sim.regs['es'], sim.regs['ax']


def run_port(lib_path, mem, sysdat_word):
    lib = ctypes.CDLL(lib_path)

    class IdleResult(ctypes.Structure):
        _fields_ = [('ds', ctypes.c_uint16), ('es', ctypes.c_uint16), ('ax', ctypes.c_uint16)]

    lib.idle.restype = IdleResult
    lib.idle.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    mem_ptr = ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8))
    r = lib.idle(mem_ptr, ctypes.c_uint16(sysdat_word))
    return r.ds, r.es, r.ax


CASES = [
    ("typical", 0x1234, 0x5678),
    ("zero uda segment", 0x1234, 0x0000),
    ("zero sysdat segment", 0x0000, 0x5678),
]


def main():
    code, entry = assemble_reference()
    lib_path = build_c_library()
    try:
        failures = []
        for label, sysdat_word, uda_es in CASES:
            mem = make_memory(uda_es)
            ref = run_reference(code, entry, mem, sysdat_word)
            port = run_port(lib_path, mem, sysdat_word)
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference ds={ref[0]:#06x} es={ref[1]:#06x} ax={ref[2]:#06x} "
                  f"| port ds={port[0]:#06x} es={port[1]:#06x} ax={port[2]:#06x}")
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
