#!/usr/bin/env python3
"""Differential test for src-c/kernel/ml.c (mlalloc_entry + internal
mla_value/mla_makemau/mla_initmau, mlfree_entry + internal ml_insert)
against src/kernel/ML.MEM, on real memory layout.

The reference is the whole real MEM module (src/kernel/MEM.A86); see
tests/src-c/memref.py. The Memory List (rooted at SYSDAT's real `mfl`)
and its MDs are SYSDAT-relative, the MPB is in the running process's
u_wrkseg, and a new MAU's SAT is written in that MAU's own segment.
Every case diffs the whole image.
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, build_lib, c_buffer, phys, sd, wk, w16, r16, SYSDAT

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
MD1, MD2, MD3, NEWMD, BIGMD = 0x2100, 0x2110, 0x2120, 0x2130, 0x2140
MPB = 0x0100
E_NO_MEMORY = 3


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib([p for p in MEM_C if p.endswith(('ml.c', 'util_mem.c'))])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.mlalloc_entry.restype = BxCx
        lib.mlalloc_entry.argtypes = [c_u8p, u16, u16, u16]
        lib.mlfree_entry.restype = BxCx
        lib.mlfree_entry.argtypes = [c_u8p, u16, u16, u16]
        ck = Checker(ref)
        ROOT = s['mfl'] - s['m_link']   # the real Memory Free List root

        def put_md(mem, addr, link, start, length, plist=0):
            w16(mem, sd(addr + s['m_link']), link)
            w16(mem, sd(addr + s['m_start']), start)
            w16(mem, sd(addr + s['m_length']), length)
            w16(mem, sd(addr + s['m_plist']), plist)

        def put_mpb(mem, start, mn, mx):
            w16(mem, wk(MPB + s['mpb_start']), start)
            w16(mem, wk(MPB + s['mpb_min']), mn)
            w16(mem, wk(MPB + s['mpb_max']), mx)

        def ml(mem, *mds):
            """thread MDs (addr, start, length) onto the list in order"""
            w16(mem, sd(ROOT + s['m_link']), mds[0][0] if mds else 0)
            for i, (addr, start, length) in enumerate(mds):
                put_md(mem, addr, mds[i + 1][0] if i + 1 < len(mds) else 0, start, length)

        def alloc(label, mem, snap=lambda img: ()):
            regs, img = ref.run('mlalloc_entry', mem, {'dx': MPB, 'bx': ROOT})
            buf = c_buffer(mem)
            r = lib.mlalloc_entry(buf, SYSDAT, MPB, ROOT)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx), snap)

        def free(label, mem, dx, snap=lambda img: ()):
            regs, img = ref.run('mlfree_entry', mem, {'dx': dx, 'bx': ROOT})
            buf = c_buffer(mem)
            r = lib.mlfree_entry(buf, SYSDAT, dx, ROOT)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx), snap)

        root_link = lambda img: r16(img, sd(ROOT + s['m_link']))  # noqa: E731

        # -- mlalloc_entry --
        mem = ref.base_memory()
        ml(mem, (MD1, 0x0900, 20))
        put_mpb(mem, 0, 5, 20)
        alloc("mlalloc_entry: single element, perfect fit", mem,
              lambda i: (root_link(i), bytes(i[phys(0x0900):phys(0x0900, 16)])))

        mem = ref.base_memory()
        ml(mem, (MD1, 0x0900, 5))
        put_mpb(mem, 0, 20, 25)
        alloc("mlalloc_entry: no window satisfies the MPB", mem, root_link)

        mem = ref.base_memory()
        ml(mem, (MD1, 0x0900, 5), (MD2, 0x0905, 15))
        put_mpb(mem, 0, 8, 25)
        w16(mem, sd(s['mdul']), NEWMD)
        alloc("mlalloc_entry: two contiguous elements merge into one MAU", mem,
              lambda i: (root_link(i), bytes(i[sd(NEWMD):sd(NEWMD + 8)]), r16(i, sd(s['mdul']))))

        mem = ref.base_memory()
        ml(mem, (MD1, 0x0900, 5), (MD2, 0x0905, 15))
        put_mpb(mem, 0, 8, 25)
        alloc("mlalloc_entry: merge needed but no free MD", mem, root_link)

        # best score wins: 200 and 100 paragraphs score (len - 30) >> 4 = 10/4,
        # the 30-paragraph element (20 + the 10-paragraph SAT header) scores 0
        mem = ref.base_memory()
        ml(mem, (MD1, 0x0900, 200), (MD2, 0x0A00, 100), (MD3, 0x0B00, 30))
        put_mpb(mem, 0, 10, 20)
        alloc("mlalloc_entry: picks the best-scoring element", mem,
              lambda i: (root_link(i), r16(i, sd(MD2 + s['m_link']))))

        # absolute request: must fall inside an element, past its own SAT
        # header (m_start + 2*satlen)
        for label, start in (("mlalloc_entry: absolute request inside the 2nd element", 0x0A10),
                             ("mlalloc_entry: absolute request in no element", 0x0C00)):
            mem = ref.base_memory()
            ml(mem, (MD1, 0x0900, 20), (MD2, 0x0A00, 20))
            put_mpb(mem, start, 5, 10)
            alloc(label, mem, root_link)

        # -- mlfree_entry --
        mem = ref.base_memory()
        put_md(mem, MD1, 0, 0x0900, 20)
        free("mlfree_entry: plain MAU into an empty list", mem, MD1,
             lambda i: (root_link(i), r16(i, sd(MD1 + s['m_link']))))

        mem = ref.base_memory()
        ml(mem, (MD2, 0x0800, 4), (MD3, 0x0A00, 4))
        put_md(mem, MD1, 0, 0x0900, 20)
        free("mlfree_entry: plain MAU sorted into the middle", mem, MD1,
             lambda i: (r16(i, sd(MD2 + s['m_link'])), r16(i, sd(MD1 + s['m_link']))))

        mem = ref.base_memory()
        put_md(mem, MD1, MD2, 0x0900, 5)
        put_md(mem, MD2, 0, 0x0A00, 15)
        put_md(mem, BIGMD, 0, 0x0900, 20, plist=MD1)
        free("mlfree_entry: partitioned MAU splits back into the list + freemd", mem, BIGMD,
             lambda i: (root_link(i), r16(i, sd(MD1 + s['m_link'])), r16(i, sd(MD2 + s['m_link'])),
                        r16(i, sd(s['mdul']))))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
