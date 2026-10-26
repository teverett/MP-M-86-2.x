#!/usr/bin/env python3
"""Differential test for src-c/kernel/file4.c's directly-dispatched BDOS
functions (func24/25/26/29/32/44/45/51/52 -- the ones BDOSIF's `entry`
calls without getdiskmx) against the real BDOS module, on real memory
layout with SYSDAT at a nonzero segment -- see tests/src-c/bdosref.py.

DATA.BDO's fields (dlog, rodsk, returnseg) and the running PD are DS
(SYSDAT)-relative; the u_* fields (u_dma_ofst/u_dma_seg/u_mult_cnt/
u_error_mode/u_retseg) are the running process's UDA, reached through ES
in the asm. Every case compares bx/cx and diffs the whole image.
"""
import ctypes
import os
import sys

from bdosref import (BdosRef, Checker, BDOS_C, PD_RUN, SYSDAT, UDA, build_lib, c_buffer, ds, phys, w16,
                     r16, u8, u16, c_u8p, c_u16p)


def main():
    ref = BdosRef()
    v = ref.v
    lib_path = build_lib(BDOS_C)
    try:
        lib = ctypes.CDLL(lib_path)
        for name, extra, res in (('func24_entry', [], u16), ('func25_entry', [], u16), ('func29_entry', [], u16),
                                 ('func32_entry', [u8], u16), ('func52_entry', [], u16)):
            fn = getattr(lib, name)
            fn.restype = res
            fn.argtypes = [c_u8p, u16] + extra + [c_u16p]
        for name, extra in (('func26_entry', [u16]), ('func51_entry', [u16]), ('func44_entry', [u8]),
                            ('func45_entry', [u8])):
            getattr(lib, name).argtypes = [c_u8p, u16] + extra + [c_u16p, c_u16p]
        ck = Checker(ref)

        def run(label, func, mem, dx=0, args=(), returns='bx', snap=lambda img: ()):
            regs, img = ref.run(func, mem, {'dx': dx, 'bx': 0x55AA, 'cx': 0x66BB}, es_uda=True)
            buf = c_buffer(mem)
            bx, cx = ctypes.c_uint16(), ctypes.c_uint16()
            fn = getattr(lib, func + '_entry')
            if returns == 'bx':
                got = (fn(buf, SYSDAT, *args, ctypes.byref(cx)), cx.value)
            else:
                fn(buf, SYSDAT, *args, ctypes.byref(bx), ctypes.byref(cx))
                got = (bx.value, cx.value)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), got, snap)

        uda = lambda f: phys(UDA, v(f))  # noqa: E731

        mem = ref.base_memory()
        w16(mem, ds(v('dlog')), 0x1234)
        run("func24: return the login vector", 'func24', mem)
        mem = ref.base_memory()
        mem[ds(PD_RUN + v('p_dsk'))] = 3
        run("func25: return the current disk", 'func25', mem)
        mem = ref.base_memory()
        w16(mem, ds(v('rodsk')), 0x4321)
        run("func29: return the r/o vector", 'func29', mem)
        mem = ref.base_memory()
        mem[ds(PD_RUN + v('p_user'))] = 7
        run("func32: interrogate the user code", 'func32', mem, 0xFF, (0xFF,))
        run("func32: set the user code (masked to 4 bits)", 'func32', ref.base_memory(), 0x1D, (0x1D,),
            snap=lambda i: i[ds(PD_RUN + v('p_user'))])
        run("func26: set the dma offset", 'func26', ref.base_memory(), 0x5678, (0x5678,), 'bxcx',
            lambda i: r16(i, uda('u_dma_ofst')))
        run("func51: set the dma base", 'func51', ref.base_memory(), 0x9ABC, (0x9ABC,), 'bxcx',
            lambda i: r16(i, uda('u_dma_seg')))
        for dl in (0x10, 0x00, 0x11):
            run(f"func44: multi-sector count {dl:#x}", 'func44', ref.base_memory(), dl, (dl,), 'bxcx',
                lambda i: i[uda('u_mult_cnt')])
        run("func45: set the error mode", 'func45', ref.base_memory(), 0xFE, (0xFE,), 'bxcx',
            lambda i: i[uda('u_error_mode')])
        mem = ref.base_memory()
        w16(mem, uda('u_dma_ofst'), 0x0080)
        w16(mem, uda('u_dma_seg'), 0x2345)
        run("func52: get dma (returnseg = DS, u_retseg = u_dma_seg)", 'func52', mem,
            snap=lambda i: (r16(i, ds(v('returnseg'))), r16(i, uda('u_retseg'))))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
