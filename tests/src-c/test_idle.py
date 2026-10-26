#!/usr/bin/env python3
"""Differential test for src-c/kernel/idle.c against src/kernel/IDLE.SUP's
`idle:`, on real memory layout -- see tests/src-c/supref.py. `idle:` ends
in `jmp xiosif` (the XIOS's idle routine); the reference's XIOS hook
captures the DS/ES/AX handed to it, which is what idle.c returns. `rlr`
and the PD are SYSDAT-relative.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, PD_RUN, SYSDAT, UDA, build_lib, c_buffer, sd, w16, KERNEL_C


class IdleResult(ctypes.Structure):
    _fields_ = [('ds', ctypes.c_uint16), ('es', ctypes.c_uint16), ('ax', ctypes.c_uint16)]


def main():
    ref = SysRef()
    lib_path = build_lib([os.path.join(KERNEL_C, 'idle.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.idle.restype = IdleResult
        lib.idle.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        ck = Checker(ref)
        for label, uda in (("idle: running PD's UDA at 0900h", UDA), ("idle: UDA at 1234h", 0x1234)):
            mem = ref.base_memory()
            w16(mem, sd(PD_RUN + ref.v('p_uda')), uda)
            seen = []
            ref.xios = lambda sim: seen.append((sim.regs['ds'], sim.regs['es'], sim.regs['ax']))
            _, img = ref.run('idle', mem, {}, ds=0x0123)
            buf = c_buffer(mem)
            r = lib.idle(buf, SYSDAT)
            ck.compare(label, img, buf, tuple(seen), ((r.ds, r.es, r.ax),))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
