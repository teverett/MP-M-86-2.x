#!/usr/bin/env python3
"""Differential test for src-c/kernel/sysfunc.c against the 4 self-contained
entries in src/kernel/SYSFUNC.SUP (not_implemented, bad_entry,
version_entry, mpmversion_entry)."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'sysfunc_wrapper.A86')
SYSFUNC_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'sysfunc.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_VERNUM = 0xA00A
SD_MPMVERNUM = 0xA00C


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value
               for name in ('not_implemented', 'bad_entry', 'version_entry', 'mpmversion_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, SYSFUNC_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, mem_template):
    image = bytearray(mem_template)
    image[0:len(code)] = code
    sim = I86Sim(image, {'sp': STACK_TOP}, entry)
    sim.run()
    return sim.regs['bx'], sim.regs['cx']


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.not_implemented.restype = ctypes.c_uint16
        lib.not_implemented.argtypes = [ctypes.POINTER(ctypes.c_uint16)]
        lib.bad_entry.restype = ctypes.c_uint16
        lib.bad_entry.argtypes = [ctypes.POINTER(ctypes.c_uint16)]
        lib.version_entry.restype = ctypes.c_uint16
        lib.version_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint16)]
        lib.mpmversion_entry.restype = ctypes.c_uint16
        lib.mpmversion_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint16)]

        mem_template = bytearray(0x10000)
        mem_template[SD_VERNUM] = 0x34
        mem_template[SD_VERNUM + 1] = 0x12
        mem_template[SD_MPMVERNUM] = 0x01
        mem_template[SD_MPMVERNUM + 1] = 0x01

        buf = (ctypes.c_uint8 * 0x10000)(*mem_template)
        mem_ptr = ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8))

        failures = []

        def check(label, ref_bx_cx, port_bx_cx):
            status = "ok" if ref_bx_cx == port_bx_cx else "FAIL"
            print(f"[{status}] {label}: reference bx={ref_bx_cx[0]:#06x} cx={ref_bx_cx[1]:#06x} "
                  f"| port bx={port_bx_cx[0]:#06x} cx={port_bx_cx[1]:#06x}")
            if status == "FAIL":
                failures.append(label)

        ref = run_reference(code, entries['not_implemented'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.not_implemented(ctypes.byref(cx))
        check("not_implemented", ref, (bx, cx.value))

        ref = run_reference(code, entries['bad_entry'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.bad_entry(ctypes.byref(cx))
        check("bad_entry", ref, (bx, cx.value))

        ref = run_reference(code, entries['version_entry'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.version_entry(mem_ptr, ctypes.byref(cx))
        check("version_entry", ref, (bx, cx.value))

        ref = run_reference(code, entries['mpmversion_entry'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.mpmversion_entry(mem_ptr, ctypes.byref(cx))
        check("mpmversion_entry", ref, (bx, cx.value))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall 4 cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
