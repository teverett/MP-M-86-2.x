#!/usr/bin/env python3
"""Differential test for src-c/kernel/cioif.c's cioif_entry (CIOIF.CIO's
inter-module CIO dispatch) against the real CIO module, on real memory
layout -- see tests/src-c/cioref.py. This checks the wiring: does `cl`
route to the right target with the right subset of dx/bh/bl, with the
right part of the result coming back, with the same XIOS calls (both
sides get an XIOS with no input; see cioref.Xios). Each of the 23 functions gets one
representative scenario (test_console.py/test_chario.py go deeper), and
every case diffs the whole image.

Only what a target defines is compared: BL for the four byte-returning
functions (conin/rconin/dirio/constat -- BH/CX are whatever the call
chain left), BX:CX for the BX/CX ones, nothing for the void ones (their
effect shows up in the image).
"""
import ctypes
import os
import sys

from cioref import (CioRef, Checker, CIO_C, NCONDEV, PD_RUN, SYSDAT, XIOS_FN, build_lib, c_buffer, c_xios,
                    sd, wk, w16, r16, u8, u16, c_u8p)

BUF, ACB = 0x0100, 0x0180
PD_B = 0x2100
BL_ONLY = {0, 2, 5, 8}
VOID = {1, 3, 4, 6, 7, 14}


class EntryResult(ctypes.Structure):
    _fields_ = [('ax', ctypes.c_uint16), ('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]


def main():
    ref = CioRef()
    s = ref.s
    lib_path = build_lib(CIO_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.cioif_entry.restype = EntryResult
        lib.cioif_entry.argtypes = [c_u8p, u16, u8, u16, u8, u8]
        lib.cio_set_xios.argtypes = [XIOS_FN]
        ck = Checker(ref)

        def base(nchar=0):
            mem = ref.base_memory()
            w16(mem, ref.ccb(0, 'c_attach'), PD_RUN)
            w16(mem, ref.ccb(NCONDEV, 'c_attach'), PD_RUN)
            mem[ref.ccb(0, 'c_nchar')] = nchar
            return mem

        def run(label, cl, mem, dx=0, bh=0, bl=0, snap=None):
            regs, img = ref.run('entry', mem, {'cx': cl, 'dx': dx, 'bx': (bh << 8) | bl},
                                stop_addrs={ref.entry_retf})
            buf = c_buffer(mem)
            x, keep = c_xios(lib)
            r = lib.cioif_entry(buf, SYSDAT, cl, dx, bh, bl)
            ck.check(f"{label}: XIOS calls", ref.xios.log, x.log)
            if cl in BL_ONLY:
                want, got = (regs['bx'] & 0xFF,), (r.bx & 0xFF,)
            elif cl in VOID:
                want, got = (), ()
            else:
                want, got = (regs['bx'], regs['cx']), (r.bx, r.cx)
            ck.compare(label, img, buf, want, got,
                       snap or (lambda i: (i[ref.ccb(0, 'c_column')], r16(i, ref.ccb(0, 'c_attach')))))

        def text(mem, data):
            mem[wk(BUF):wk(BUF) + len(data)] = data
            return mem

        run("0 conin_entry", 0, base(ord('Q')))
        run("1 conout_entry", 1, base(), dx=ord('Z'))
        run("2 rconin_entry", 2, base(ord('R')))
        run("3 rconout_entry", 3, base(), dx=ord('A'))
        run("4 listout_entry", 4, base(), dx=ord('X'))
        run("5 dirio_entry", 5, base(ord('d')), dx=0xFD)
        run("6 conwrite_entry", 6, text(base(), b'hey$'), dx=BUF)
        run("7 conread_entry", 7, text(base(ord('k')), b'\x02'), dx=BUF)
        # docs/bugs.md #108: through `entry`, CX is 2*cl going in, so
        # mimic_entry's success has to clear it for control_p to see it
        mem = base(0x10)  # ctl-P
        w16(mem, ref.ccb(NCONDEV, 'c_attach'), 0)
        run("7 conread_entry: ctl-P turns printer echo on (bug #108)", 7, text(mem, b'\x02'), dx=BUF,
            snap=lambda i: (i[ref.ccb(0, 'c_flag')] & s['cf_listcp'], i[ref.ccb(0, 'c_mimic')],
                            i[ref.ccb(0, 'c_column')]))
        run("8 constat_entry", 8, base(ord('s')))
        run("9 conattach_entry", 9, ref.base_memory())
        run("10 condetach_entry", 10, base())
        run("11 setdefcon_entry", 11, base(), dx=1)
        mem = base()
        mem[wk(ACB + s['acb_cns'])] = 1
        w16(mem, wk(ACB + s['acb_pd']), PD_B)
        run("12 conassign_entry", 12, mem, dx=ACB)
        run("13 getdefcon_entry", 13, base())
        run("14 conprint_entry", 14, text(base(), b'abcdef'), dx=BUF, bh=3, bl=0)
        run("15 lstattach_entry", 15, ref.base_memory())
        run("16 lstdetach_entry", 16, base())
        run("17 setdeflst_entry", 17, base(), dx=1)
        mem = ref.base_memory()
        w16(mem, ref.ccb(NCONDEV, 'c_attach'), PD_B)
        run("18 clstattch_entry", 18, mem)
        mem = ref.base_memory()
        w16(mem, ref.ccb(0, 'c_attach'), PD_B)
        run("19 cconattch_entry", 19, mem)
        run("20 getdeflst_entry", 20, base())
        run("21 mimic_entry", 21, base(), dx=(0 << 8) | 3)
        mem = base()
        w16(mem, ref.ccb(3, 'c_attach'), 0xFFFF)
        mem[ref.ccb(3, 'c_msource')] = 0
        mem[ref.ccb(0, 'c_mimic')] = 3
        run("22 unmimic_entry", 22, mem, dx=3)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
