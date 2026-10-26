#!/usr/bin/env python3
"""Differential test for src-c/kernel/util_mem.c (getmd, freemd) against
src/kernel/UTIL.MEM, on real memory layout. The reference is the whole
real MEM module (src/kernel/MEM.A86) with SYSDAT at segment 100h; see
tests/src-c/memref.py. MDs and `mdul` are SYSDAT-relative; every case
diffs the whole image.
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, build_lib, c_buffer, sd, w16, r16, SYSDAT

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib([p for p in MEM_C if p.endswith('util_mem.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.getmd.restype = BxCx
        lib.getmd.argtypes = [c_u8p, u16]
        lib.freemd.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)
        MDUL = sd(s['mdul'])

        def md(addr, field):
            return sd(addr + s[field])

        # getmd: list empty -> e_no_umd
        mem = ref.base_memory()
        regs, img = ref.run('getmd', mem, {})
        buf = c_buffer(mem)
        r = lib.getmd(buf, SYSDAT)
        ck.compare("getmd: empty list", img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx))

        # getmd: pops the head, zeroes its fields, advances mdul
        MD1, MD2 = 0x2100, 0x2200
        mem = ref.base_memory()
        w16(mem, MDUL, MD1)
        w16(mem, md(MD1, 'm_link'), MD2)
        w16(mem, md(MD1, 'm_start'), 0x1234)
        w16(mem, md(MD1, 'm_length'), 0x5678)
        w16(mem, md(MD1, 'm_plist'), 0x9ABC)
        regs, img = ref.run('getmd', mem, {})
        buf = c_buffer(mem)
        r = lib.getmd(buf, SYSDAT)
        ck.compare("getmd: pops and clears the head MD", img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx),
                   lambda i: (r16(i, MDUL), bytes(i[sd(MD1):sd(MD1 + 8)])))

        # freemd: pushes onto the front of mdul
        mem = ref.base_memory()
        w16(mem, MDUL, 0x2777)
        MD = 0x2300
        regs, img = ref.run('freemd', mem, {'bx': MD})
        buf = c_buffer(mem)
        lib.freemd(buf, SYSDAT, MD)
        ck.compare("freemd: links onto mdul", img, buf, (), (),
                   lambda i: (r16(i, MDUL), r16(i, md(MD, 'm_link'))))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
