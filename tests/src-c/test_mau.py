#!/usr/bin/env python3
"""Differential test for src-c/kernel/mau.c against src/kernel/
MAU.MEM's maualloc_entry/maufree_entry (+ internal mau_split,
mau_insert, mau_release, mau_collapse).

Needs tools/i86sim.py's opt-in `segmented=True` mode (see src-c/
kernel/ml.c's own header comment): every SAT field lives inside a
MAU's own dynamically-computed segment (`m_start[bx]`).

Also exercises docs/bugs.md #102's fix directly (the "absolute address
splits a free SAT that isn't exactly at its start" case) -- before the
fix, `src/kernel/MAU.MEM` clobbered `BX` with the split length right
before calling `mau_split`, corrupting the SAT table instead of
splitting it correctly.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'mau_wrapper.A86')
MAU_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'mau.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
UDA_WRKSEG = 0xA012

M_START, M_LENGTH = 2, 4
MPB_START, MPB_MIN, MPB_MAX, MPB_FLAGS = 0, 2, 4, 8
MAF_MAU, MAF_SAT, MAF_START = 0, 2, 4
SAT_START, SAT_LENGTH, SAT_NALL, SATLEN = 0, 2, 4, 5
E_NO_MEMORY = 3

WRKSEG = 0x0040
MAU_SEG = 0x0100
MPB_ADDR = 0x0010  # offset within WRKSEG's own segment
MAF_ADDR = 0x0020
MD_ADDR = 0x5000


def phys(seg):
    return (seg * 16) & 0xFFFF


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    addr &= 0xFFFF
    return mem[addr] | (mem[(addr + 1) & 0xFFFF] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in ('maualloc_entry', 'maufree_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *MAU_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, WRKSEG)
    mem_w16(mem, MD_ADDR + M_START, MAU_SEG)
    mem_w16(mem, MD_ADDR + M_LENGTH, 0x1000)
    return mem


def put_sat(mem, seg, entry_no, start, length, nall, ents=4):
    base = phys(seg)
    mem[base] = ents  # SAT0's own ents field
    off = base + entry_no * SATLEN
    mem_w16(mem, off + SAT_START, start)
    mem_w16(mem, off + SAT_LENGTH, length)
    mem[off + SAT_NALL] = nall


def put_mpb(mem, start, mn, mx, flags=0):
    base = phys(WRKSEG) + MPB_ADDR
    mem_w16(mem, base + MPB_START, start)
    mem_w16(mem, base + MPB_MIN, mn)
    mem_w16(mem, base + MPB_MAX, mx)
    mem_w16(mem, base + MPB_FLAGS, flags)


def put_maf(mem, mau, sat, start):
    base = phys(WRKSEG) + MAF_ADDR
    mem_w16(mem, base + MAF_MAU, mau)
    mem_w16(mem, base + MAF_SAT, sat)
    mem_w16(mem, base + MAF_START, start)


def run_reference(code, entry, mem, regs):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, segmented=True)
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class MauBxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.maualloc_entry.restype = MauBxCx
        lib.maualloc_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.maufree_entry.restype = MauBxCx
        lib.maufree_entry.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def sat_table(mem, n=40):
            base = phys(MAU_SEG)
            return bytes(mem[base:base + n])

        def mpb_fields(mem):
            base = phys(WRKSEG) + MPB_ADDR
            return (mem_r16(mem, base + MPB_START), mem_r16(mem, base + MPB_MIN),
                    mem_r16(mem, base + MPB_MAX))

        # -- maualloc_entry: non-absolute, exact fit (no split) --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 0)
        put_sat(mem, MAU_SEG, 2, 0x0400, 10, 1)
        put_mpb(mem, 0, 5, 20)
        ref_regs, ref_img = run_reference(code, entries['maualloc_entry'], mem, {'bx': MD_ADDR, 'dx': MPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.maualloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD_ADDR), ctypes.c_uint16(MPB_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], mpb_fields(ref_img), sat_table(ref_img))
        port = (got.bx, got.cx, mpb_fields(port_img), sat_table(port_img))
        check("maualloc_entry: non-absolute exact fit", ref, port)

        # -- maualloc_entry: non-absolute, needs a split --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 0)
        put_sat(mem, MAU_SEG, 2, 0x0400, 10, 1)
        put_mpb(mem, 0, 5, 8)
        ref_regs, ref_img = run_reference(code, entries['maualloc_entry'], mem, {'bx': MD_ADDR, 'dx': MPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.maualloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD_ADDR), ctypes.c_uint16(MPB_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], mpb_fields(ref_img), sat_table(ref_img))
        port = (got.bx, got.cx, mpb_fields(port_img), sat_table(port_img))
        check("maualloc_entry: non-absolute, splits off the requested amount", ref, port)

        # -- maualloc_entry: absolute address, exact match (no split) --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 0)
        put_sat(mem, MAU_SEG, 2, 0x0400, 10, 1)
        put_mpb(mem, 0x0300, 0, 0)
        ref_regs, ref_img = run_reference(code, entries['maualloc_entry'], mem, {'bx': MD_ADDR, 'dx': MPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.maualloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD_ADDR), ctypes.c_uint16(MPB_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], mpb_fields(ref_img), sat_table(ref_img))
        port = (got.bx, got.cx, mpb_fields(port_img), sat_table(port_img))
        check("maualloc_entry: absolute address, exact match", ref, port)

        # -- maualloc_entry: absolute address NOT at a SAT's own start --
        # (docs/bugs.md #102's own regression case)
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 0)
        put_sat(mem, MAU_SEG, 2, 0x0400, 10, 1)
        put_mpb(mem, 0x0305, 0, 100)
        ref_regs, ref_img = run_reference(code, entries['maualloc_entry'], mem, {'bx': MD_ADDR, 'dx': MPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.maualloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD_ADDR), ctypes.c_uint16(MPB_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], mpb_fields(ref_img), sat_table(ref_img))
        port = (got.bx, got.cx, mpb_fields(port_img), sat_table(port_img))
        check("maualloc_entry: absolute address mid-entry (bug #102 regression)", ref, port)

        # -- maualloc_entry: no fit -> e_no_memory --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 5, 1)
        put_sat(mem, MAU_SEG, 2, 0x0305, 10, 1)
        put_mpb(mem, 0, 20, 25)
        ref_regs, ref_img = run_reference(code, entries['maualloc_entry'], mem, {'bx': MD_ADDR, 'dx': MPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.maualloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD_ADDR), ctypes.c_uint16(MPB_ADDR))
        check("maualloc_entry: no fit -> e_no_memory", (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))

        # -- maufree_entry: whole-entry free (exact match) --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 1)
        put_sat(mem, MAU_SEG, 2, 0x0400, 10, 1)
        put_maf(mem, MD_ADDR, 0x0300, 0x0300)
        ref_regs, ref_img = run_reference(code, entries['maufree_entry'], mem, {'dx': MAF_ADDR})
        buf = to_ctypes(mem)
        got = lib.maufree_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MAF_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], sat_table(ref_img))
        port = (got.bx, got.cx, sat_table(port_img))
        check("maufree_entry: whole-entry free", ref, port)

        # -- maufree_entry: partial free, needs a split --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 1)
        put_sat(mem, MAU_SEG, 2, 0x0400, 10, 1)
        put_maf(mem, MD_ADDR, 0x0300, 0x0310)
        ref_regs, ref_img = run_reference(code, entries['maufree_entry'], mem, {'dx': MAF_ADDR})
        buf = to_ctypes(mem)
        got = lib.maufree_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MAF_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], sat_table(ref_img))
        port = (got.bx, got.cx, sat_table(port_img))
        check("maufree_entry: partial free splits the entry", ref, port)

        # -- maufree_entry: freeing the last allocated entry empties the MAU --
        mem = base_memory()
        put_sat(mem, MAU_SEG, 1, 0x0300, 20, 1)
        put_maf(mem, MD_ADDR, 0x0300, 0x0300)
        ref_regs, ref_img = run_reference(code, entries['maufree_entry'], mem, {'dx': MAF_ADDR})
        buf = to_ctypes(mem)
        got = lib.maufree_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MAF_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], sat_table(ref_img))
        port = (got.bx, got.cx, sat_table(port_img))
        check("maufree_entry: freeing the only entry empties the MAU", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
