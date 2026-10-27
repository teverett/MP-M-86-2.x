#!/usr/bin/env python3
"""Differential test for src-c/kernel/rpl.c against src-assem/kernel/RPL.SUP's
rpl_entry, on real memory layout -- see tests/src-c/supref.py. The CPB is
in u_wrkseg. RPL's `mpmif(f_qopen)` reaches the RTM, which isn't loaded
here, so it's the no-op stub rpl.c models (CX stays nonzero), and the
routine always ends at rpl_err; its success path -- a hand-built far
return into the RSP -- has no C equivalent (see rpl.c's header). Every
case diffs the whole image.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, SYSDAT, build_lib, c_buffer, wk, w16, KERNEL_C

u16 = ctypes.c_uint16


def main():
    ref = SysRef()
    lib_path = build_lib([os.path.join(KERNEL_C, 'rpl.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.rpl_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), u16, u16,
                                  ctypes.POINTER(u16), ctypes.POINTER(u16)]
        ck = Checker(ref)
        for label, cpb, name, param in (("rpl_entry: ends at rpl_err (CPB #1)", 0x0100, b'SOMEQ   ', 0x1234),
                                        ("rpl_entry: ends at rpl_err (CPB #2)", 0x0180, b'OTHERQ  ', 0x5678)):
            mem = ref.base_memory()
            mem[wk(cpb):wk(cpb) + 8] = name
            w16(mem, wk(cpb + 8), param)
            regs, img = ref.run('rpl_entry', mem, {'dx': cpb})
            buf = c_buffer(mem)
            bx, cx = u16(), u16()
            lib.rpl_entry(buf, SYSDAT, cpb, ctypes.byref(bx), ctypes.byref(cx))
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (bx.value, cx.value))
            ck.check(f"{label}: went to the RTM for f_qopen", 1, len(ref.calls))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
