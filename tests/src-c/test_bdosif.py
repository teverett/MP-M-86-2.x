#!/usr/bin/env python3
"""Differential test for src-c/kernel/bdosif.c's bdosif_entry (BDOSIF.BDO's
`entry`, the BDOS function-table dispatch) against the real BDOS module,
on real memory layout with SYSDAT at a nonzero segment -- see
tests/src-c/bdosref.py and bdosif.c's header.

The 10 functions whose `btab_flag` is 0 (func24/25/26/29/32/44/45/51/52/
107, BDOS function #s 11/12/13/16/19/31/32/36/37/45) are dispatched
directly (`nomx`). Every other function goes through getdiskmx: the
MXdisk queue, the UDA <-> BDOS copies, the DMA reload, CALL_BDOS and
BDOS_RETURN. Those cases use functions that need no disk (FUNC104/105/
106), plus getdiskmx's own branches: a DMA that differs from the BDOS's
copy (setdata's XIOS calls), a ^C already pending (the function is
skipped and the process terminates), pf_tempkeep already on. Both sides'
mpmif/xiosif calls are logged and compared (fixtures/bdos_hooks.c on the
C side). For each case bx/cx and the whole image are compared, except the
asm's own BDOS stack in SYSDAT (SAVE_SP .. bdosstack), which the C, on
the process's stack, doesn't use.
"""
import ctypes
import os
import sys

from bdosref import (BdosRef, Checker, BDOS_C, PD_RUN, SYSDAT, UDA, USER, build_lib, c_buffer, ds, phys, us, w16,
                     u8, u16, c_u8p)

HOOKS_C = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'bdos_hooks.c')

AMBIENT_BX = 0x55AA


class EntryResult(ctypes.Structure):
    _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]


def ref_calls(ref):
    return [(ord('m') << 16) | v if k == 'mpm' else (ord('x') << 16) | v for k, v in ref.calls]


def port_calls(lib):
    buf = (ctypes.c_uint32 * 256)()
    n = lib.bdos_test_calls(buf, 256)
    return list(buf[:n])


def main():
    ref = BdosRef()
    v = ref.v
    exit_retf = [i for i in range(v('exit'), v('exit') + 8) if ref.code[i] == 0xCB][0]
    lib_path = build_lib(BDOS_C + [HOOKS_C])
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

        # through getdiskmx
        lib.bdos_test_install()
        ref.skip |= set(range(ds(v('save_sp')), ds(v('bdosstack') + 1)))  # the asm's own BDOS stack
        PARAM = 0x0040  # the caller's parameter block, in its u_wrkseg (USER)
        flag = lambda m: ds(PD_RUN + v('p_flag'))  # noqa: E731

        def caller(m):
            w16(m, uda('u_wrkseg'), USER)
            w16(m, uda('u_retseg'), 0x4444)
            # the UDA's DMA equals the BDOS's copy, unless a case changes it
            w16(m, uda('u_dma_ofst'), m[ds(v('dmaad'))] | m[ds(v('dmaad')) + 1] << 8)
            w16(m, uda('u_dma_seg'), m[ds(v('dmabase'))] | m[ds(v('dmabase')) + 1] << 8)

        def tod(m):
            m[ds(v('tod')):ds(v('tod')) + 5] = bytes([0x34, 0x12, 0x09, 0x41, 0x27])

        def user_bytes(data):
            return lambda m: m.__setitem__(slice(us(PARAM), us(PARAM) + len(data)), data)

        def seq(*fs):
            def run(m):
                for f in fs:
                    f(m)
            return run

        def newdma(m):
            w16(m, uda('u_dma_ofst'), 0x0080)
            w16(m, uda('u_dma_seg'), USER)

        def ctlc(m):
            w16(m, flag(m), 0x0080)  # pf_ctlc

        def tempkeep(m):
            w16(m, flag(m), 0x0200)  # pf_tempkeep

        mx_cases = [
            ("FUNC105 (43): get date and time", 43, seq(caller, tod)),
            ("FUNC104 (42): set date and time", 42, seq(caller, user_bytes(bytes([0x78, 0x56, 0x13, 0x59])))),
            ("FUNC106 (44): set default password", 44, seq(caller, user_bytes(b'SECRET  '))),
            ("FUNC106 (44): null default password", 44, seq(caller, user_bytes(b'        '))),
            ("getdiskmx: a new dma -> setdata", 43, seq(caller, tod, newdma)),
            ("getdiskmx: ^C pending -> no function, terminate", 43, seq(caller, tod, ctlc)),
            ("getdiskmx: pf_tempkeep already on stays on", 43, seq(caller, tod, tempkeep)),
        ]
        for label, cl, seed in mx_cases:
            mem = ref.base_memory()
            seed(mem)
            regs, img = ref.run('entry', mem, {'cx': cl, 'dx': PARAM, 'bx': AMBIENT_BX}, es_uda=True,
                                stop_addrs={exit_retf})
            buf = c_buffer(mem)
            lib.bdos_test_install()
            r = lib.bdosif_entry(buf, SYSDAT, cl, PARAM, AMBIENT_BX)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx))
            ck.check(f"{label}: same mpmif/xiosif calls", ref_calls(ref), port_calls(lib))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
