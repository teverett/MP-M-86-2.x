#!/usr/bin/env python3
"""Differential test for src-c/kernel/util_mem.c against src/kernel/
UTIL.MEM's getmd and freemd."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'util_mem_wrapper.A86')
UTIL_MEM_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'util_mem.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_MDUL = 0xA028
M_LINK, M_START, M_LENGTH, M_PLIST = 0, 2, 4, 6
E_NO_UMD = 0x12


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in ('getmd', 'freemd')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, UTIL_MEM_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def run_reference(code, entry, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry)
    sim.run()
    return (sim.regs['bx'], sim.regs['cx']), image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.getmd.restype = BxCx
        lib.getmd.argtypes = [c_u8p]
        lib.freemd.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # getmd: list empty -> e_no_umd
        mem = bytearray(0x10000)
        mem_w16(mem, SD_MDUL, 0)
        ref_regs, _ = run_reference(code, entries['getmd'], mem, {})
        buf = to_ctypes(mem)
        r = lib.getmd(ctypes.cast(buf, c_u8p))
        check("getmd: empty list", ref_regs, (r.bx, r.cx))

        # getmd: pops the head, zeroes its fields, advances mdul
        MD1 = 0x4100
        MD2 = 0x4200
        mem = bytearray(0x10000)
        mem_w16(mem, SD_MDUL, MD1)
        mem_w16(mem, MD1 + M_LINK, MD2)
        mem_w16(mem, MD1 + M_START, 0x1234)
        mem_w16(mem, MD1 + M_LENGTH, 0x5678)
        mem_w16(mem, MD1 + M_PLIST, 0x9ABC)
        ref_regs, ref_img = run_reference(code, entries['getmd'], mem, {})
        buf = to_ctypes(mem)
        r = lib.getmd(ctypes.cast(buf, c_u8p))
        port_img = bytes(buf)
        ref = (ref_regs, mem_r16(ref_img, SD_MDUL), ref_img[MD1:MD1 + 8])
        port = ((r.bx, r.cx), mem_r16(port_img, SD_MDUL), port_img[MD1:MD1 + 8])
        check("getmd: pops and clears the head MD", ref, port)

        # freemd: pushes onto the front of mdul
        mem = bytearray(0x10000)
        mem_w16(mem, SD_MDUL, 0x7777)
        MD = 0x4300
        ref_regs, ref_img = run_reference(code, entries['freemd'], mem, {'bx': MD})
        buf = to_ctypes(mem)
        lib.freemd(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD))
        port_img = bytes(buf)
        ref = (mem_r16(ref_img, SD_MDUL), mem_r16(ref_img, MD + M_LINK))
        port = (mem_r16(port_img, SD_MDUL), mem_r16(port_img, MD + M_LINK))
        check("freemd: links onto mdul", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
