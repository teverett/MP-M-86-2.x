#!/usr/bin/env python3
"""Differential test for src-c/kernel/mau.c (maualloc_entry,
maufree_entry + internal mau_split/mau_insert/mau_release/mau_collapse)
against src/kernel/MAU.MEM, on real memory layout.

The reference is the whole real MEM module (src/kernel/MEM.A86); see
tests/src-c/memref.py. The MAU's MD is SYSDAT-relative, its SAT lives in
the MAU's own segment (MAU_SEG), and the MPB/MAF is in the running
process's u_wrkseg. Every case diffs the whole image.
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, build_lib, c_buffer, phys, sd, wk, w16, r16, SYSDAT

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
MD_ADDR = 0x2100
MAU_SEG = 0x0900
MPB, MAF = 0x0100, 0x0180


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib([p for p in MEM_C if p.endswith('mau.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.maualloc_entry.restype = BxCx
        lib.maualloc_entry.argtypes = [c_u8p, u16, u16, u16]
        lib.maufree_entry.restype = BxCx
        lib.maufree_entry.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)
        SATLEN = s['satlen']

        def base_memory(ents=4):
            mem = ref.base_memory()
            w16(mem, sd(MD_ADDR + s['m_start']), MAU_SEG)
            w16(mem, sd(MD_ADDR + s['m_length']), 0x1000)
            mem[phys(MAU_SEG)] = ents   # SAT0's ents
            return mem

        def put_sat(mem, n, start, length, nall):
            off = phys(MAU_SEG, n * SATLEN)
            w16(mem, off + s['sat_start'], start)
            w16(mem, off + s['sat_length'], length)
            mem[off + s['sat_nall']] = nall

        def put_mpb(mem, start, mn, mx, flags=0):
            for f, v in (('mpb_start', start), ('mpb_min', mn), ('mpb_max', mx), ('mpb_flags', flags)):
                w16(mem, wk(MPB + s[f]), v)

        def put_maf(mem, sat, start):
            for f, v in (('maf_mau', MD_ADDR), ('maf_sat', sat), ('maf_start', start)):
                w16(mem, wk(MAF + s[f]), v)

        sat_table = lambda img: bytes(img[phys(MAU_SEG):phys(MAU_SEG, 6 * SATLEN)])  # noqa: E731
        mpb = lambda img: tuple(r16(img, wk(MPB + s[f])) for f in ('mpb_start', 'mpb_min', 'mpb_max'))  # noqa: E731

        def alloc(label, mem):
            regs, img = ref.run('maualloc_entry', mem, {'bx': MD_ADDR, 'dx': MPB})
            buf = c_buffer(mem)
            r = lib.maualloc_entry(buf, SYSDAT, MD_ADDR, MPB)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx),
                       lambda i: (mpb(i), sat_table(i)))

        def free(label, mem):
            regs, img = ref.run('maufree_entry', mem, {'dx': MAF})
            buf = c_buffer(mem)
            r = lib.maufree_entry(buf, SYSDAT, MAF)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx), sat_table)

        # -- maualloc_entry --
        for label, args in (("maualloc_entry: non-absolute exact fit", (0, 5, 20)),
                            ("maualloc_entry: non-absolute, splits off the requested amount", (0, 5, 8)),
                            ("maualloc_entry: absolute address, exact match", (0x0A00, 0, 0)),
                            ("maualloc_entry: absolute address mid-entry (bug #102 regression)", (0x0A05, 0, 100)),
                            ("maualloc_entry: absolute address outside the MAU", (0x2000, 0, 5)),
                            ("maualloc_entry: no fit -> e_no_memory", (0, 30, 35))):
            mem = base_memory()
            put_sat(mem, 1, 0x0A00, 20, 0)
            put_sat(mem, 2, 0x0A14, 10, 1)
            put_mpb(mem, *args)
            alloc(label, mem)

        # split with a full table: mau_insert fails, so the entry is just
        # shortened ("create hole")
        mem = base_memory(ents=2)
        put_sat(mem, 1, 0x0A00, 20, 0)
        put_sat(mem, 2, 0x0A14, 10, 1)
        put_mpb(mem, 0, 5, 8)
        alloc("maualloc_entry: split with the SAT table full", mem)

        # -- maufree_entry --
        mem = base_memory()
        put_sat(mem, 1, 0x0A00, 20, 1)
        put_sat(mem, 2, 0x0A14, 10, 1)
        put_maf(mem, 0x0A00, 0x0A00)
        free("maufree_entry: whole-entry free", mem)

        mem = base_memory()
        put_sat(mem, 1, 0x0A00, 20, 1)
        put_sat(mem, 2, 0x0A14, 10, 1)
        put_maf(mem, 0x0A00, 0x0A10)
        free("maufree_entry: partial free splits the entry", mem)

        mem = base_memory()
        put_sat(mem, 1, 0x0A00, 20, 1)
        put_maf(mem, 0x0A00, 0x0A00)
        free("maufree_entry: freeing the only entry empties the MAU", mem)

        # docs/bugs.md #106: the last SAT in use is still allocated -- via
        # the next-slot-unused exit (above, "whole-entry free") and via the
        # table-full exit here
        mem = base_memory(ents=2)
        put_sat(mem, 1, 0x0A00, 20, 1)
        put_sat(mem, 2, 0x0A14, 10, 1)
        put_maf(mem, 0x0A00, 0x0A00)
        free("maufree_entry: full table, last SAT still allocated -> not empty", mem)

        mem = base_memory()
        put_sat(mem, 1, 0x0A00, 20, 1)
        put_sat(mem, 2, 0x0A14, 10, 0)
        put_sat(mem, 3, 0x0A1E, 10, 1)
        put_maf(mem, 0x0A00, 0x0A00)
        free("maufree_entry: freed entry merges with the free one after it", mem)

        mem = base_memory()
        put_sat(mem, 1, 0x0A00, 20, 1)
        put_maf(mem, 0x0B00, 0x0B00)
        free("maufree_entry: no such SAT -> e_no_memory", mem)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
