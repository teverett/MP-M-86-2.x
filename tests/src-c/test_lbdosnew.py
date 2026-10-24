#!/usr/bin/env python3
"""Differential test for src-c/loader/lbdosnew.c against src/loader/
LBDOSNEW.A86's func51 (set segment base for disk I/O).
func50/func52/func53/func54/func55/func56/func57 are not ported -- see
lbdosnew.c's own header comment for why (each is just a separate entry
point for the same bare `ret`, no code of its own)."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'lbdosnew_wrapper.A86')
LBDOSNEW_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'lbdosnew.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0
LD_DMABASE = 0xB043


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'loader')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in ('func51',)}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, LBDOSNEW_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    return bytearray(0x10000)


def run_reference(code, entry, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry)
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.func51.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- func51: sets dmabase from dx --
        mem = base_memory()
        mem[LD_DMABASE:LD_DMABASE + 2] = (0, 0)
        ref_regs, ref_img = run_reference(code, entries['func51'], mem, {'dx': 0x1234})
        buf = to_ctypes(mem)
        lib.func51(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x1234))
        check("func51: sets dmabase", ref_img[LD_DMABASE:LD_DMABASE + 2], bytes(buf)[LD_DMABASE:LD_DMABASE + 2])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
