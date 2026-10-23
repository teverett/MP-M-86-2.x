#!/usr/bin/env python3
"""Differential test for src-c/kernel/ml.c against src/kernel/ML.MEM's
mlalloc_entry/mlfree_entry.

The first file this project ports using tools/i86sim.py's new opt-in
`segmented=True` mode (see that module's own docstring, and ml.c's own
header comment): mla_value reads its caller's MPB through a
dynamically-supplied segment (`u_wrkseg`'s own stored value), and
mla_initmau writes a brand-new MAU's own SAT through another
dynamically-supplied segment (`m_start[bx]`). Memory List elements
themselves are ordinary SYSDAT-resident addresses (same flat-model
scratch-address convention as every other RTM/SUP list this project
already ports).
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'ml_wrapper.A86')
ML_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'ml.c'),
        os.path.join(REPO_ROOT, 'src-c', 'kernel', 'util_mem.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
UDA_WRKSEG = 0xA012
SD_MDUL = 0xA028

M_LINK, M_START, M_LENGTH, M_PLIST = 0, 2, 4, 6
MPB_START, MPB_MIN, MPB_MAX = 0, 2, 4
E_NO_MEMORY = 3

ROOT, MD1, MD2, BIGMD, NEWMD = 0x5000, 0x5100, 0x5200, 0x5400, 0x5300
WRKSEG = 0x0040
MPB_ADDR = 0x0010  # offset within WRKSEG's own segment


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    mlalloc_entry = asm.symtab['mlalloc_entry'].value
    mlfree_entry = asm.symtab['mlfree_entry'].value
    return code, mlalloc_entry, mlfree_entry


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *ML_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[(addr + 1) & 0xFFFF] << 8)


def put_md(mem, addr, link, start, length, plist=0):
    mem_w16(mem, addr + M_LINK, link)
    mem_w16(mem, addr + M_START, start)
    mem_w16(mem, addr + M_LENGTH, length)
    mem_w16(mem, addr + M_PLIST, plist)


def put_mpb(mem, wrkseg_phys, offset, start, mn, mx):
    base = wrkseg_phys + offset
    mem_w16(mem, base + MPB_START, start)
    mem_w16(mem, base + MPB_MIN, mn)
    mem_w16(mem, base + MPB_MAX, mx)


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
    code, mlalloc_entry, mlfree_entry = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class MlBxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.mlalloc_entry.restype = MlBxCx
        lib.mlalloc_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.mlfree_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        wrkseg_phys = WRKSEG * 16

        def sat_bytes(mem, seg, n=16):
            base = seg * 16
            return bytes(mem[base:base + n])

        # -- mlalloc_entry: single element, perfect fit --
        mem = bytearray(0x10000)
        mem_w16(mem, ROOT + M_LINK, MD1)
        put_md(mem, MD1, 0, 0x0300, 20)
        put_mpb(mem, wrkseg_phys, MPB_ADDR, 0, 5, 20)
        mem_w16(mem, UDA_WRKSEG, WRKSEG)
        ref_regs, ref_img = run_reference(code, mlalloc_entry, mem,
                                           {'dx': MPB_ADDR, 'bx': ROOT})
        ref = (ref_regs['bx'], ref_regs['cx'],
               mem_r16(ref_img, ROOT + M_LINK),
               mem_r16(ref_img, MD1 + M_PLIST),
               sat_bytes(ref_img, 0x0300))
        buf = to_ctypes(mem)
        got = lib.mlalloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MPB_ADDR), ctypes.c_uint16(ROOT))
        port_img = bytes(buf)
        port = (got.bx, got.cx,
                mem_r16(port_img, ROOT + M_LINK),
                mem_r16(port_img, MD1 + M_PLIST),
                sat_bytes(port_img, 0x0300))
        check("mlalloc_entry: single element, perfect fit", ref, port)

        # -- mlalloc_entry: no fit at all -> e_no_memory --
        mem = bytearray(0x10000)
        mem_w16(mem, ROOT + M_LINK, MD1)
        put_md(mem, MD1, 0, 0x0300, 5)
        put_mpb(mem, wrkseg_phys, MPB_ADDR, 0, 20, 25)
        mem_w16(mem, UDA_WRKSEG, WRKSEG)
        ref_regs, ref_img = run_reference(code, mlalloc_entry, mem,
                                           {'dx': MPB_ADDR, 'bx': ROOT})
        ref = (ref_regs['bx'], ref_regs['cx'], mem_r16(ref_img, ROOT + M_LINK))
        buf = to_ctypes(mem)
        got = lib.mlalloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MPB_ADDR), ctypes.c_uint16(ROOT))
        port_img = bytes(buf)
        port = (got.bx, got.cx, mem_r16(port_img, ROOT + M_LINK))
        check("mlalloc_entry: no window satisfies the MPB", ref, port)
        if got.cx != E_NO_MEMORY:
            print(f"    (expected cx == e_no_memory == {E_NO_MEMORY})")

        # -- mlalloc_entry: two contiguous elements must merge --
        mem = bytearray(0x10000)
        mem_w16(mem, ROOT + M_LINK, MD1)
        put_md(mem, MD1, MD2, 0x0300, 5)
        put_md(mem, MD2, 0, 0x0305, 15)
        put_mpb(mem, wrkseg_phys, MPB_ADDR, 0, 8, 25)
        mem_w16(mem, UDA_WRKSEG, WRKSEG)
        mem_w16(mem, SD_MDUL, NEWMD)
        mem_w16(mem, NEWMD + M_LINK, 0)
        ref_regs, ref_img = run_reference(code, mlalloc_entry, mem,
                                           {'dx': MPB_ADDR, 'bx': ROOT})
        ref = (ref_regs['bx'], ref_regs['cx'],
               mem_r16(ref_img, ROOT + M_LINK),
               mem_r16(ref_img, NEWMD + M_PLIST),
               mem_r16(ref_img, NEWMD + M_LENGTH),
               mem_r16(ref_img, NEWMD + M_START),
               mem_r16(ref_img, SD_MDUL),
               sat_bytes(ref_img, 0x0300))
        buf = to_ctypes(mem)
        got = lib.mlalloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MPB_ADDR), ctypes.c_uint16(ROOT))
        port_img = bytes(buf)
        port = (got.bx, got.cx,
                mem_r16(port_img, ROOT + M_LINK),
                mem_r16(port_img, NEWMD + M_PLIST),
                mem_r16(port_img, NEWMD + M_LENGTH),
                mem_r16(port_img, NEWMD + M_START),
                mem_r16(port_img, SD_MDUL),
                sat_bytes(port_img, 0x0300))
        check("mlalloc_entry: two contiguous elements merge into one MAU", ref, port)

        # -- mlfree_entry: plain (non-partitioned) MAU, empty list --
        mem = bytearray(0x10000)
        mem_w16(mem, ROOT + M_LINK, 0)
        put_md(mem, MD1, 0, 0x0300, 20, plist=0)
        ref_regs, ref_img = run_reference(code, mlfree_entry, mem, {'dx': MD1, 'bx': ROOT})
        ref = (mem_r16(ref_img, ROOT + M_LINK), mem_r16(ref_img, MD1 + M_LINK))
        buf = to_ctypes(mem)
        lib.mlfree_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MD1), ctypes.c_uint16(ROOT))
        port_img = bytes(buf)
        port = (mem_r16(port_img, ROOT + M_LINK), mem_r16(port_img, MD1 + M_LINK))
        check("mlfree_entry: plain MAU reinserted into an empty list", ref, port)

        # -- mlfree_entry: partitioned MAU, each underlying MD reinserted + MAU freed --
        mem = bytearray(0x10000)
        mem_w16(mem, ROOT + M_LINK, 0)
        put_md(mem, MD1, MD2, 0x0300, 5)
        put_md(mem, MD2, 0, 0x0400, 15)
        put_md(mem, BIGMD, 0, 0x0300, 20, plist=MD1)
        mem_w16(mem, SD_MDUL, 0)
        ref_regs, ref_img = run_reference(code, mlfree_entry, mem, {'dx': BIGMD, 'bx': ROOT})
        ref = (mem_r16(ref_img, ROOT + M_LINK), mem_r16(ref_img, MD1 + M_LINK),
               mem_r16(ref_img, MD2 + M_LINK), mem_r16(ref_img, SD_MDUL),
               mem_r16(ref_img, BIGMD + M_LINK))
        buf = to_ctypes(mem)
        lib.mlfree_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BIGMD), ctypes.c_uint16(ROOT))
        port_img = bytes(buf)
        port = (mem_r16(port_img, ROOT + M_LINK), mem_r16(port_img, MD1 + M_LINK),
                mem_r16(port_img, MD2 + M_LINK), mem_r16(port_img, SD_MDUL),
                mem_r16(port_img, BIGMD + M_LINK))
        check("mlfree_entry: partitioned MAU splits back into the list + freemd", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
