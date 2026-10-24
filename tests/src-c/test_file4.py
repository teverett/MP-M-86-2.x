#!/usr/bin/env python3
"""Differential test for src-c/kernel/file4.c against a self-contained
subset of src/kernel/FILE4.BDO: func24, func25, func29, func32,
FUNC44, FUNC45, func51, func52. See file4.c's own header comment for
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'file4_wrapper.A86')
FILE4_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file4.c')
FILE1_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file1.c')
FILE2_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file2.c')
FILE3_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file3.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

# This combined bundle's own real small addresses for the UDA fields
# these routines touch -- collide with cseg's own early code, same
# class of gap as rlr=0x68 (see file1.c's/file4.c's own comments).
REAL_RLR, REAL_DLOG, REAL_RODSK, REAL_RETURNSEG = 0x68, 0xC03, 0xC01, 0xCBB
REAL_U_DMA_OFST, REAL_U_DMA_SEG = 0x2, 0x4
REAL_U_ERROR_MODE, REAL_U_MULT_CNT, REAL_U_RETSEG = 0x10, 0x11, 0x30

# src-c/kernel/defs.h's own harness-chosen scratch addresses the port
# itself reads/writes for the same fields.
SD_RLR = 0xA000
UDA_DMA_OFST, UDA_DMA_SEG = 0xA060, 0xA062
UDA_ERROR_MODE, UDA_MULT_CNT, UDA_RETSEG = 0xA064, 0xA065, 0xA014

PD_ADDR = 0x4000  # harness-chosen scratch PD address, well clear of cseg/dseg

# mpmif and its 9 XIOS-call helpers, plus XCRLF/XPRINT -- all real
# BDOSIF.BDO labels this combined bundle needs to assemble, even
# though nothing file4.c ports actually reaches them (see
# file1_wrapper.A86's own header comment).
STUBS = {0x48b, 0x4b8, 0x4bd, 0x4c2, 0x4c7, 0x4cc, 0x4d1, 0x4d6, 0x4db, 0x4e0, 0x409, 0x416}


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    names = ('func24', 'func25', 'func29', 'func32', 'func44', 'func45', 'func51', 'func52')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, FILE1_C, FILE2_C, FILE3_C, FILE4_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD_ADDR)
    return mem


def run_reference(code, entry, mem, regs, stop_addrs=()):
    # Same reasoning as test_file2.py's own run_reference: this
    # combined bundle's own dseg would stomp code in this address
    # range once layered, so it's dropped entirely; every field this
    # test drives is instead force-copied explicitly below.
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # Force-copy every field unconditionally (same reasoning as
    # test_file1.py's own rlr/BD_CURDSK special-casing): the port's
    # own scratch addresses (SD_RLR etc.) never collide with code, so
    # this only has to remap each one to its real bundle address for
    # the reference run.
    image[REAL_RLR:REAL_RLR + 2] = mem[SD_RLR:SD_RLR + 2]
    image[REAL_DLOG:REAL_DLOG + 2] = mem[REAL_DLOG:REAL_DLOG + 2]
    image[REAL_RODSK:REAL_RODSK + 2] = mem[REAL_RODSK:REAL_RODSK + 2]
    image[REAL_RETURNSEG:REAL_RETURNSEG + 2] = mem[REAL_RETURNSEG:REAL_RETURNSEG + 2]
    image[REAL_U_DMA_OFST:REAL_U_DMA_OFST + 2] = mem[UDA_DMA_OFST:UDA_DMA_OFST + 2]
    image[REAL_U_DMA_SEG:REAL_U_DMA_SEG + 2] = mem[UDA_DMA_SEG:UDA_DMA_SEG + 2]
    image[REAL_U_ERROR_MODE] = mem[UDA_ERROR_MODE]
    image[REAL_U_MULT_CNT] = mem[UDA_MULT_CNT]
    image[REAL_U_RETSEG:REAL_U_RETSEG + 2] = mem[UDA_RETSEG:UDA_RETSEG + 2]
    # The running PD's own p_dsk/p_user bytes, at the PD address this
    # test chose -- PD_ADDR is well clear of cseg/dseg, so a plain
    # "nonzero wins" copy already carried these; nothing further
    # needed here.
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs=STUBS)
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)

        lib.func24_entry.restype = ctypes.c_uint16
        lib.func24_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]
        lib.func25_entry.restype = ctypes.c_uint16
        lib.func25_entry.argtypes = [c_u8p, c_u16p]
        lib.func29_entry.restype = ctypes.c_uint16
        lib.func29_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]
        lib.func32_entry.restype = ctypes.c_uint16
        lib.func32_entry.argtypes = [c_u8p, ctypes.c_uint8, c_u16p]
        lib.func44_entry.argtypes = [c_u8p, ctypes.c_uint8, c_u16p, c_u16p]
        lib.func45_entry.argtypes = [c_u8p, ctypes.c_uint8, c_u16p, c_u16p]
        lib.func51_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p, c_u16p]
        lib.func52_entry.restype = ctypes.c_uint16
        lib.func52_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, c_u16p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- func24: return the login vector --
        mem = base_memory()
        mem_w16(mem, REAL_DLOG, 0x1234)
        ref_regs, _ = run_reference(code, entries['func24'], mem, {})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        got = lib.func24_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.byref(cx))
        check("func24: return the login vector", (ref_regs['bx'], ref_regs['cx']), (got, cx.value))

        # -- func25: return selected disk number --
        mem = base_memory()
        mem[PD_ADDR + 0x12] = 3  # p_dsk
        ref_regs, _ = run_reference(code, entries['func25'], mem, {})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        got = lib.func25_entry(ctypes.cast(buf, c_u8p), ctypes.byref(cx))
        check("func25: return selected disk number", (ref_regs['bx'], ref_regs['cx']), (got, cx.value))

        # -- func29: return r/o bit vector --
        mem = base_memory()
        mem_w16(mem, REAL_RODSK, 0x0005)
        ref_regs, _ = run_reference(code, entries['func29'], mem, {})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        got = lib.func29_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.byref(cx))
        check("func29: return r/o bit vector", (ref_regs['bx'], ref_regs['cx']), (got, cx.value))

        # -- func32: interrogate current user code (dl == 0xFF) --
        mem = base_memory()
        mem[PD_ADDR + 0x13] = 7  # p_user
        ref_regs, _ = run_reference(code, entries['func32'], mem, {'dx': 0x00FF})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        got = lib.func32_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0xFF), ctypes.byref(cx))
        check("func32: interrogate current user code", (ref_regs['bx'], ref_regs['cx']), (got, cx.value))

        # -- func32: set a new user code (masked to 4 bits) --
        mem = base_memory()
        mem[PD_ADDR + 0x13] = 7  # p_user
        ref_regs, ref_img = run_reference(code, entries['func32'], mem, {'dx': 0x001D})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        got = lib.func32_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x1D), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], ref_img[PD_ADDR + 0x13])
        port = (got, cx.value, bytes(buf)[PD_ADDR + 0x13])
        check("func32: set a new user code", ref, port)

        # -- func44: valid multi-sector count --
        mem = base_memory()
        ref_regs, ref_img = run_reference(code, entries['func44'], mem, {'dx': 0x0010})
        buf = to_ctypes(mem)
        bx = ctypes.c_uint16()
        cx = ctypes.c_uint16()
        lib.func44_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x10), ctypes.byref(bx), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], ref_img[REAL_U_MULT_CNT])
        port = (bx.value, cx.value, bytes(buf)[UDA_MULT_CNT])
        check("func44: valid multi-sector count", ref, port)

        # -- func44: zero is out of range --
        mem = base_memory()
        mem[UDA_MULT_CNT] = 0x09
        ref_regs, ref_img = run_reference(code, entries['func44'], mem, {'dx': 0x0000})
        buf = to_ctypes(mem)
        bx = ctypes.c_uint16()
        cx = ctypes.c_uint16()
        lib.func44_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x00), ctypes.byref(bx), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], ref_img[REAL_U_MULT_CNT])
        port = (bx.value, cx.value, bytes(buf)[UDA_MULT_CNT])
        check("func44: zero is out of range", ref, port)

        # -- func44: 17 is out of range --
        mem = base_memory()
        mem[UDA_MULT_CNT] = 0x09
        ref_regs, ref_img = run_reference(code, entries['func44'], mem, {'dx': 0x0011})
        buf = to_ctypes(mem)
        bx = ctypes.c_uint16()
        cx = ctypes.c_uint16()
        lib.func44_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x11), ctypes.byref(bx), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], ref_img[REAL_U_MULT_CNT])
        port = (bx.value, cx.value, bytes(buf)[UDA_MULT_CNT])
        check("func44: 17 is out of range", ref, port)

        # -- func45: set bdos error mode --
        mem = base_memory()
        ref_regs, ref_img = run_reference(code, entries['func45'], mem, {'dx': 0x00FF})
        buf = to_ctypes(mem)
        bx = ctypes.c_uint16()
        cx = ctypes.c_uint16()
        lib.func45_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0xFF), ctypes.byref(bx), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], ref_img[REAL_U_ERROR_MODE])
        port = (bx.value, cx.value, bytes(buf)[UDA_ERROR_MODE])
        check("func45: set bdos error mode", ref, port)

        # -- func51: set dma base --
        mem = base_memory()
        ref_regs, ref_img = run_reference(code, entries['func51'], mem, {'dx': 0x5678})
        buf = to_ctypes(mem)
        bx = ctypes.c_uint16()
        cx = ctypes.c_uint16()
        lib.func51_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x5678), ctypes.byref(bx), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], mem_r16(ref_img, REAL_U_DMA_SEG))
        port = (bx.value, cx.value, mem_r16(bytes(buf), UDA_DMA_SEG))
        check("func51: set dma base", ref, port)

        # -- func52: get dma --
        mem = base_memory()
        mem_w16(mem, UDA_DMA_SEG, 0x4321)
        mem_w16(mem, UDA_DMA_OFST, 0x0099)
        ref_regs, ref_img = run_reference(code, entries['func52'], mem, {'ds': 0x1111})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        got = lib.func52_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(0x1111), ctypes.byref(cx))
        ref = (ref_regs['bx'], ref_regs['cx'], mem_r16(ref_img, REAL_U_RETSEG), mem_r16(ref_img, REAL_RETURNSEG))
        port = (got, cx.value, mem_r16(bytes(buf), UDA_RETSEG), mem_r16(bytes(buf), REAL_RETURNSEG))
        check("func52: get dma", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
