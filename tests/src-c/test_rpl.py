#!/usr/bin/env python3
"""Differential test for src-c/kernel/rpl.c against src/kernel/
RPL.SUP's rpl_entry. See rpl.c's own header comment for why this
collapses to the one deterministic `rpl_err` outcome under this
project's own boundary-stub convention, regardless of what CPB it's
given -- the two cases below exist to confirm exactly that (different
CPBs, same result), not to exercise any branch.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'rpl_wrapper.A86')
RPL_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'rpl.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
UDA_WRKSEG, UDA_RETSEG = 0xA012, 0xA014
MPMIF_STUB = 0x9000
CPB_NAME, CPB_PARAM = 0, 8


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['rpl_entry'].value
    return code, entry


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, RPL_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, 0x4000)  # arbitrary -- never actually read along rpl_err
    mem_w16(mem, UDA_RETSEG, 0x4100)  # ditto
    return mem


def run_reference(code, entry, mem, dx):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {'dx': dx, 'sp': STACK_TOP}, entry, call_stub_addrs={MPMIF_STUB})
    sim.run()
    return sim.regs, image


def main():
    code, entry = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)
        lib.rpl_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p, c_u16p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def call_c(dx, mem):
            buf = to_ctypes(mem)
            bx, cx = ctypes.c_uint16(0xDEAD), ctypes.c_uint16(0xDEAD)
            lib.rpl_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(dx), ctypes.byref(bx), ctypes.byref(cx))
            return bx.value, cx.value

        # -- rpl_entry: a CPB naming some arbitrary queue -- mpmif(f_qopen)'s
        # own no-op-return convention leaves cx == f_qopen (never 0), so
        # the real routine always falls straight into rpl_err.
        mem = base_memory()
        CPB = 0x5000
        mem[CPB + CPB_NAME:CPB + CPB_NAME + 8] = b'SOMEQ   '
        mem_w16(mem, CPB + CPB_PARAM, 0x1234)
        ref_regs, ref_img = run_reference(code, entry, mem, CPB)
        got = call_c(CPB, mem)
        check("rpl_entry: always falls through to rpl_err (CPB #1)",
              (ref_regs['bx'], ref_regs['cx']), got)

        # -- same, with a different CPB (confirms invariance, not a branch) --
        mem = base_memory()
        CPB2 = 0x5100
        mem[CPB2 + CPB_NAME:CPB2 + CPB_NAME + 8] = b'OTHERQ  '
        mem_w16(mem, CPB2 + CPB_PARAM, 0x5678)
        ref_regs, ref_img = run_reference(code, entry, mem, CPB2)
        got = call_c(CPB2, mem)
        check("rpl_entry: always falls through to rpl_err (CPB #2)",
              (ref_regs['bx'], ref_regs['cx']), got)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
