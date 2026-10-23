#!/usr/bin/env python3
"""Differential test for src-c/kernel/file2.c against a self-contained
subset of src/kernel/FILE2.BDO: GET_NALBS, getblock (+ internal
lefttst/righttst/rblok/rblok0). See file2.c's own header comment for
why the rest of the file is deferred.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'file2_wrapper.A86')
FILE2_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file2.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file1.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file3.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

BD_MAXALL, BD_ALLOCA, BD_CURDSK = 0x0C4D, 0x0C46, 0x0C36

# mpmif and its 9 XIOS-call helpers, plus XCRLF/XPRINT -- all real
# BDOSIF.BDO labels this combined bundle needs to assemble, even
# though nothing file2.c ports actually reaches them (see
# file1_wrapper.A86's own header comment).
STUBS = {0x48b, 0x4b8, 0x4bd, 0x4c2, 0x4c7, 0x4cc, 0x4d1, 0x4d6, 0x4db, 0x4e0, 0x409, 0x416}


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    names = ('get_nalbs', 'getblock')
    entries = {name: asm.symtab[name].value for name in names}
    return code, dseg, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *FILE2_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    return bytearray(0x10000)


def run_reference(code, dseg, entry, mem, regs, stop_addrs=()):
    # Unlike test_file1.py/test_file3.py, this one does NOT layer
    # dseg's own real field data over cseg: get_nalbs/getblock's own
    # code (this combined bundle's own FILE2.BDO) happens to land
    # inside DATA.BDO's dseg address range here, where file1.c's/
    # file3.c's own ported routines didn't -- blanket-copying dseg
    # would stomp getblock's own code. Nothing this test exercises
    # relies on an unset dseg default (BD_MAXALL/BD_ALLOCA are always
    # explicitly set below), so this is safe to skip entirely.
    del dseg
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # Without the dseg background, "only nonzero mem bytes win" isn't
    # enough for fields whose own high byte is legitimately 0 (e.g.
    # BD_MAXALL == 100) -- force every byte of the fields this test
    # actually drives unconditionally, same idea as test_file1.py's
    # own rlr/BD_CURDSK special-casing.
    image[BD_MAXALL:BD_MAXALL + 2] = mem[BD_MAXALL:BD_MAXALL + 2]
    image[BD_ALLOCA:BD_ALLOCA + 2] = mem[BD_ALLOCA:BD_ALLOCA + 2]
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs=STUBS)
    sim.run()
    return sim.regs, image


ALLOC_AT = 0x4400


def main():
    code, dseg, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.get_nalbs.restype = ctypes.c_uint16
        lib.get_nalbs.argtypes = [c_u8p]
        lib.getblock.restype = ctypes.c_uint16
        lib.getblock.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- get_nalbs --
        mem = base_memory()
        mem_w16(mem, BD_MAXALL, 100)
        ref_regs, _ = run_reference(code, dseg, entries['get_nalbs'], mem, {})
        buf = to_ctypes(mem)
        got = lib.get_nalbs(ctypes.cast(buf, c_u8p))
        check("get_nalbs", ref_regs['bx'], got)

        def setup(maxall, alloc_bytes):
            mem = base_memory()
            mem_w16(mem, BD_MAXALL, maxall)
            mem_w16(mem, BD_ALLOCA, ALLOC_AT)
            mem[ALLOC_AT:ALLOC_AT + len(alloc_bytes)] = alloc_bytes
            return mem

        # -- getblock: the starting bit itself is free --
        mem = setup(63, bytes([0x00] * 8))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 10})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(10))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 8]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 8]))
        check("getblock: starting bit free", ref, port)

        # -- getblock: bit 9 (the first one tried) and bit 11 (the first
        # righttst try) are both taken, forcing a second lefttst
        # iteration; bit 8 is free. (getallocbit's bit-within-byte order
        # is MSB-first as cx increases: within byte index 1 -- bits
        # 8-15 -- cx=8 is bit7 (0x80), cx=9 is bit6 (0x40), cx=10 is bit5
        # (0x20), cx=11 is bit4 (0x10), confirmed empirically by probing
        # getallocbit directly.)
        mem = setup(63, bytes([0x00, 0x50] + [0] * 6))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 10})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(10))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 8]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 8]))
        check("getblock: nearest free bit is 2 to the left (loops)", ref, port)

        # -- getblock: starting bit taken, left exhausted immediately
        # (cx==0 at entry), nearest free bit is to the right --
        mem = setup(63, bytes([0b00000001] + [0] * 7))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 0})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 8]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 8]))
        check("getblock: cx == 0, nearest free bit is to the right", ref, port)

        # -- getblock: every bit allocated, maxall small -> not found --
        mem = setup(7, bytes([0xFF]))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 3})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(3))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 1]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 1]))
        check("getblock: fully allocated, not found", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
