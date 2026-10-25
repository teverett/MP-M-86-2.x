#!/usr/bin/env python3
"""Differential test for src-c/kernel/bdosif.c against src/kernel/
BDOSIF.BDO's `entry` (its own BDOS function-table dispatch). See
bdosif.c's own header comment for the full design: only the 10 real
functions whose own `btab_flag` is 0 (func24/25/26/29/32/44/45/51/52/
107, BDOS function #s 11/12/13/16/19/31/32/36/37/45) skip `getdiskmx`
(the real critical-section-switch+SHELL+CALL_BDOS mechanism, not
ported here) -- every other function # is a documented boundary
(`reached_getdiskmx`), stubbed the same way `tests/src-c/
test_file4.py`'s own `STUBS` set already stubs other BDOSIF.BDO/
FILE1-4.BDO far-call-like boundaries.

Reuses tests/src-c/fixtures/file4_wrapper.A86 verbatim (the same
combined BDOSIF.BDO+FILE1-4.BDO+UDA.FMT+SYSDAT.FMT+DATA.BDO bundle
src/kernel/BDOS.A86 itself assembles) -- `entry`/`functab`/`nomx`/
`exit`/`getdiskmx` are all real labels already present in it, nothing
new needed there. Same "no dseg layering, force-copy each field"
reference-run convention as test_file4.py; see that file's own header
comment for why (this combined bundle's own dseg collides with cseg
at these tiny real offsets).
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
BDOSIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f)
            for f in ('bdosif.c', 'file1.c', 'file2.c', 'file3.c', 'file4.c')]
BDOSIF_C.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'bdosif_init_stubs.c'))
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

REAL_RLR, REAL_DLOG, REAL_RODSK, REAL_RETURNSEG = 0x68, 0xC03, 0xC01, 0xCBB
REAL_U_DMA_OFST, REAL_U_DMA_SEG = 0x2, 0x4
REAL_U_ERROR_MODE, REAL_U_MULT_CNT, REAL_U_RETSEG = 0x10, 0x11, 0x30

SD_RLR = 0xA000
UDA_DMA_OFST, UDA_DMA_SEG = 0xA060, 0xA062
UDA_ERROR_MODE, UDA_MULT_CNT, UDA_RETSEG = 0xA064, 0xA065, 0xA014

PD_ADDR = 0x4000

# Same stub set test_file4.py already needs for this combined bundle
# to run its own already-ported code paths (mpmif and friends -- see
# that file's own comment), plus `getdiskmx` itself: the one real
# boundary *this* test deliberately stops short of (bdosif.c's own
# `entry` reports reaching it rather than modeling it).
STUBS = {0x488, 0x4b5, 0x4ba, 0x4bf, 0x4c4, 0x4c9, 0x4ce, 0x4d3, 0x4d8, 0x4dd, 0x406, 0x413}


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['entry'].value
    exit_addr = asm.symtab['exit'].value
    getdiskmx = asm.symtab['getdiskmx'].value
    return code, entry, exit_addr, getdiskmx


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *BDOSIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD_ADDR)
    return mem


def run_reference(code, entry, exit_addr, getdiskmx, mem, cl, dx, bx, ds):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    image[REAL_RLR:REAL_RLR + 2] = mem[SD_RLR:SD_RLR + 2]
    image[REAL_U_DMA_OFST:REAL_U_DMA_OFST + 2] = mem[UDA_DMA_OFST:UDA_DMA_OFST + 2]
    image[REAL_U_DMA_SEG:REAL_U_DMA_SEG + 2] = mem[UDA_DMA_SEG:UDA_DMA_SEG + 2]
    image[REAL_U_ERROR_MODE] = mem[UDA_ERROR_MODE]
    image[REAL_U_MULT_CNT] = mem[UDA_MULT_CNT]
    image[REAL_U_RETSEG:REAL_U_RETSEG + 2] = mem[UDA_RETSEG:UDA_RETSEG + 2]
    stubs = STUBS | {getdiskmx}
    sim = I86Sim(image, {'cx': cl, 'dx': dx, 'bx': bx, 'ds': ds, 'sp': STACK_TOP}, entry,
                 stop_addrs={exit_addr}, call_stub_addrs=stubs)
    sim.run()
    return sim.regs, image


class EntryResult(ctypes.Structure):
    _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16), ('reached_getdiskmx', ctypes.c_uint8)]


def run_port(lib, mem, cl, dx, bx, ds):
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.entry.restype = EntryResult
    lib.entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8, ctypes.c_uint16,
                           ctypes.c_uint16, ctypes.c_uint16]
    return lib.entry(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), cl, dx, bx, ds)


AMBIENT_BX = 0x55AA

# (label, cl, dx, ds, seed_fn) -- seed_fn(mem) plants whatever this
# function # needs for a non-trivial check.
DIRECT_CASES = [
    ("func24 (11): login vector", 11, 0, 0, lambda m: mem_w16(m, REAL_DLOG, 0x1234)),
    ("func25 (12): current disk", 12, 0, 0, lambda m: m.__setitem__(PD_ADDR + 1, 3)),  # p_dsk = offset 1
    ("func26 (13): set dma address", 13, 0x5678, 0, lambda m: None),
    ("func29 (16): r/o vector", 16, 0, 0, lambda m: mem_w16(m, REAL_RODSK, 0x4321)),
    ("func32 (19): interrogate user code", 19, 0x00FF, 0, lambda m: None),
    ("func32 (19): set user code", 19, 0x001D, 0, lambda m: None),
    ("func44 (31): valid multi-sector count", 31, 0x0010, 0, lambda m: None),
    ("func44 (31): zero is out of range", 31, 0x0000, 0, lambda m: None),
    ("func45 (32): set bdos error mode", 32, 0x00FF, 0, lambda m: None),
    ("func51 (36): set dma base", 36, 0x9ABC, 0, lambda m: None),
    ("func52 (37): get dma", 37, 0, 0x1111, lambda m: None),
    ("func107 (45): FUNCRET no-op", 45, 0x1234, 0, lambda m: None),
]

# Every other function # (0-10,14,15,17,18,20-30,33-35,38-44,46) goes
# through getdiskmx -- a handful of representative samples, not all
# 37, since they all take the exact same documented-boundary path.
BOUNDARY_CASES = [0, 7, 14, 20, 28, 38, 46]


def main():
    code, entry, exit_addr, getdiskmx = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        for label, cl, dx, ds, seed_fn in DIRECT_CASES:
            mem = base_memory()
            seed_fn(mem)
            bx = AMBIENT_BX
            ref_regs, _ = run_reference(code, entry, exit_addr, getdiskmx, mem, cl, dx, bx, ds)
            port = run_port(lib, mem, cl, dx, bx, ds)
            ref = (ref_regs['bx'] & 0xFFFF, ref_regs['cx'] & 0xFFFF)
            got = (port.bx, port.cx)
            check(label, ref, got)
            check(f"{label}: reached_getdiskmx", False, bool(port.reached_getdiskmx))

        for cl in BOUNDARY_CASES:
            mem = base_memory()
            bx = AMBIENT_BX
            ref_regs, _ = run_reference(code, entry, exit_addr, getdiskmx, mem, cl, 0, bx, 0)
            port = run_port(lib, mem, cl, 0, bx, 0)
            ref = ref_regs['bx'] & 0xFFFF
            check(f"func #{cl}: reaches getdiskmx, bx stays ambient", AMBIENT_BX, ref)
            check(f"func #{cl}: reached_getdiskmx", True, bool(port.reached_getdiskmx))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {2 * len(DIRECT_CASES) + 2 * len(BOUNDARY_CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
