#!/usr/bin/env python3
"""Differential test for src-c/kernel/bdosif.c's bdosif_entry (BDOSIF.BDO's
`entry`, the BDOS function-table dispatch) against the real BDOS module,
on real memory layout with SYSDAT at a nonzero segment -- see
tests/src-c/bdosref.py and bdosif.c's header.

Only the 10 functions whose `btab_flag` is 0 (func24/25/26/29/32/44/45/
51/52/107, BDOS function #s 11/12/13/16/19/31/32/36/37/45) are dispatched
directly (`nomx`); for those, bx/cx and the whole image are compared.
Every other function goes through getdiskmx, which bdosif.c reports
(`reached_getdiskmx`) rather than running; the reference stubs
getdiskmx, and a few representative function #s check that both sides
take that path.
"""
import ctypes
import os
import sys

from bdosref import BdosRef, Checker, BDOS_C, PD_RUN, SYSDAT, UDA, build_lib, c_buffer, ds, phys, w16, u8, u16, c_u8p

AMBIENT_BX = 0x55AA


class EntryResult(ctypes.Structure):
    _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16), ('reached_getdiskmx', ctypes.c_uint8)]


def main():
    ref = BdosRef()
    v = ref.v
    exit_retf = [i for i in range(v('exit'), v('exit') + 8) if ref.code[i] == 0xCB][0]
    lib_path = build_lib(BDOS_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.bdosif_entry.restype = EntryResult
        lib.bdosif_entry.argtypes = [c_u8p, u16, u8, u16, u16]
        ck = Checker(ref)
        uda = lambda f: phys(UDA, v(f))  # noqa: E731

        cases = [
            ("func24 (11): login vector", 11, 0, lambda m: w16(m, ds(v('dlog')), 0x1234)),
            ("func25 (12): current disk", 12, 0, lambda m: m.__setitem__(ds(PD_RUN + v('p_dsk')), 3)),
            ("func26 (13): set dma address", 13, 0x5678, None),
            ("func29 (16): r/o vector", 16, 0, lambda m: w16(m, ds(v('rodsk')), 0x4321)),
            ("func32 (19): interrogate user code", 19, 0x00FF, None),
            ("func32 (19): set user code", 19, 0x001D, None),
            ("func44 (31): valid multi-sector count", 31, 0x0010, None),
            ("func44 (31): zero is out of range", 31, 0x0000, None),
            ("func45 (32): set bdos error mode", 32, 0x00FF, None),
            ("func51 (36): set dma base", 36, 0x9ABC, None),
            ("func52 (37): get dma", 37, 0, lambda m: w16(m, uda('u_dma_seg'), 0x2345)),
            ("func107 (45): FUNCRET no-op", 45, 0x1234, None),
        ]
        for label, cl, dx, seed in cases:
            mem = ref.base_memory()
            if seed:
                seed(mem)
            regs, img = ref.run('entry', mem, {'cx': cl, 'dx': dx, 'bx': AMBIENT_BX}, es_uda=True,
                                stop_addrs={exit_retf})
            buf = c_buffer(mem)
            r = lib.bdosif_entry(buf, SYSDAT, cl, dx, AMBIENT_BX)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx))
            ck.check(f"{label}: dispatched directly", False, bool(r.reached_getdiskmx))

        # through getdiskmx: a few representative function #s
        for cl in (0, 7, 14, 20, 28, 38, 46):
            regs, _ = ref.run('entry', ref.base_memory(), {'cx': cl, 'dx': 0, 'bx': AMBIENT_BX}, es_uda=True,
                              stop_addrs={exit_retf}, stubs={v('getdiskmx')})
            r = lib.bdosif_entry(c_buffer(ref.base_memory()), SYSDAT, cl, 0, AMBIENT_BX)
            ck.check(f"func #{cl}: reference goes to getdiskmx (bx untouched)", AMBIENT_BX, regs['bx'])
            ck.check(f"func #{cl}: reached_getdiskmx", True, bool(r.reached_getdiskmx))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
