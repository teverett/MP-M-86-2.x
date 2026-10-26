#!/usr/bin/env python3
"""Differential test for src-c/kernel/sysfunc.c (all 8 SYSFUNC.SUP
entries) against the real SUP module, on real memory layout -- see
tests/src-c/supref.py. vernum/mpmvernum/tod/rlr/PDs are SYSDAT-relative,
the caller's buffers are in u_wrkseg, u_retseg is the running UDA.
callbios_entry's two tails are captured where they leave the SUP: the
XIOS hook records the AL/AH/CL/DL handed to `xiosif`, and BOOT/WBOOT's
`mpmif(f_terminate)` shows up as a call into the RTM (not loaded here,
so a stub). Every case diffs the whole image.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, PD_RUN, SYSDAT, UDA, WRKSEG, build_lib, c_buffer, phys, sd, wk, w16, r16, KERNEL_C

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
c_u16p = ctypes.POINTER(ctypes.c_uint16)
BUF = 0x0100


class CallbiosResult(ctypes.Structure):
    _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16), ('reached_xios', ctypes.c_uint8),
                ('al', ctypes.c_uint8), ('ah', ctypes.c_uint8), ('cl', ctypes.c_uint8), ('dl', ctypes.c_uint8)]


def main():
    ref = SysRef()
    v = ref.v
    lib_path = build_lib([os.path.join(KERNEL_C, 'sysfunc.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        for n in ('not_implemented', 'bad_entry'):
            getattr(lib, n).restype = u16
            getattr(lib, n).argtypes = [c_u16p]
        for n in ('version_entry', 'mpmversion_entry', 'sysdat_entry'):
            getattr(lib, n).restype = u16
            getattr(lib, n).argtypes = [c_u8p, u16, c_u16p]
        for n in ('tod_entry', 'serial_entry'):
            getattr(lib, n).restype = u16
            getattr(lib, n).argtypes = [c_u8p, u16, u16, c_u16p]
        lib.callbios_entry.restype = CallbiosResult
        lib.callbios_entry.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)

        def run(label, entry, mem, call, regs=None, snap=lambda i: ()):
            ref_regs, img = ref.run(entry, mem, regs or {})
            buf = c_buffer(mem)
            cx = ctypes.c_uint16()
            bx = call(buf, cx)
            ck.compare(label, img, buf, (ref_regs['bx'], ref_regs['cx']), (bx, cx.value), snap)

        run("not_implemented", 'not_implemented', ref.base_memory(), lambda b, cx: lib.not_implemented(ctypes.byref(cx)))
        run("bad_entry", 'bad_entry', ref.base_memory(), lambda b, cx: lib.bad_entry(ctypes.byref(cx)))
        mem = ref.base_memory()
        w16(mem, sd(v('vernum')), 0x1130)
        w16(mem, sd(v('mpmvernum')), 0x1120)
        run("version_entry", 'version_entry', mem, lambda b, cx: lib.version_entry(b, SYSDAT, ctypes.byref(cx)))
        run("mpmversion_entry", 'mpmversion_entry', mem,
            lambda b, cx: lib.mpmversion_entry(b, SYSDAT, ctypes.byref(cx)))
        run("sysdat_entry: u_retseg = SYSDAT", 'sysdat_entry', ref.base_memory(),
            lambda b, cx: lib.sysdat_entry(b, SYSDAT, ctypes.byref(cx)),
            snap=lambda i: r16(i, phys(UDA, v('u_retseg'))))
        mem = ref.base_memory()
        mem[sd(v('tod')):sd(v('tod')) + 5] = b'\x34\x12\x09\x45\x30'
        run("tod_entry: copies tod to u_wrkseg", 'tod_entry', mem,
            lambda b, cx: lib.tod_entry(b, SYSDAT, BUF, ctypes.byref(cx)), {'dx': BUF},
            snap=lambda i: bytes(i[wk(BUF):wk(BUF) + 5]))
        run("serial_entry: copies '654321' to u_wrkseg", 'serial_entry', ref.base_memory(),
            lambda b, cx: lib.serial_entry(b, SYSDAT, BUF, ctypes.byref(cx)), {'dx': BUF},
            snap=lambda i: bytes(i[wk(BUF):wk(BUF) + 6]))

        # -- callbios --
        def callbios(label, al, cns=0, lst=0, block_cl=0x77):
            mem = ref.base_memory()
            mem[wk(BUF)] = al
            mem[wk(BUF) + 1] = block_cl
            mem[sd(PD_RUN + v('p_cns'))] = cns
            mem[sd(PD_RUN + v('p_lst'))] = lst
            seen = []
            ref.xios = lambda sim: seen.append((sim.regs['ax'] & 0xFF, sim.regs['ax'] >> 8,
                                               sim.regs['cx'] & 0xFF, sim.regs['dx'] & 0xFF))
            ref_regs, img = ref.run('callbios_entry', mem, {'dx': BUF})
            buf = c_buffer(mem)
            r = lib.callbios_entry(buf, SYSDAT, BUF)
            if r.reached_xios == 2:
                got, want = [(r.al, r.ah, r.cl, r.dl)], seen
            elif r.reached_xios == 1:
                ent = v('sysent') + (v('f_terminate') & 0xFF) * 3   # sysent: module, function, flags
                got, want = [(ref.data[ent], ref.data[ent + 1])], list(ref.calls)
            else:
                got, want = (r.bx, r.cx), (ref_regs['bx'], ref_regs['cx'])
            ck.compare(label, img, buf, want, got)

        for al in (8, 16, 200):
            callbios(f"callbios_entry: al={al} -> error", al)
        for al in (0, 1):
            callbios(f"callbios_entry: al={al} (boot) -> mpmif(f_terminate) into the RTM", al)
        for al in (2, 3):
            callbios(f"callbios_entry: al={al} -> xiosif, cl <- p_cns", al, cns=9)
        callbios("callbios_entry: al=4 -> xiosif, cl <- block byte", 4, cns=9)
        callbios("callbios_entry: al=6 -> xiosif, dl <- p_lst", 6, lst=3)
        callbios("callbios_entry: al=15 -> xiosif, cl <- p_lst", 15, lst=3)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
