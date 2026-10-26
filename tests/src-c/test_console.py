#!/usr/bin/env python3
"""Differential test for src-c/kernel/console.c (all of CONSOLE.CIO +
CHARIO.CIO's getccbadr) against the real CIO module, on real memory
layout -- see tests/src-c/cioref.py. CCBs, PDs and the device counts
are SYSDAT-relative, conassign's ACB is in the running process's
u_wrkseg, and conassign's name search runs the real FINDPD.RTM on the
reference side (findpd.c on the C side). Every case diffs the whole
image.
"""
import ctypes
import os
import sys

from cioref import (CioRef, Checker, CIO_C, NCONDEV, PD_RUN, SYSDAT, BxCx, build_lib, c_buffer,
                    sd, wk, w16, r16, u8, u16, c_u8p)

PD_B, PD_C = 0x2100, 0x2200
ACB = 0x0100


def main():
    ref = CioRef()
    s = ref.s
    lib_path = build_lib([p for p in CIO_C if p.endswith(('console.c', 'findpd.c'))])
    try:
        lib = ctypes.CDLL(lib_path)
        sigs = {'conattach_entry': [], 'cconattch_entry': [], 'lstattach_entry': [], 'clstattch_entry': [],
                'condetach_entry': [], 'lstdetach_entry': [], 'getdefcon_entry': [], 'getdeflst_entry': [],
                'setdefcon_entry': [u8], 'setdeflst_entry': [u8], 'conassign_entry': [u16],
                'mimic_entry': [u8, u8], 'unmimic_entry': [u8]}
        for name, extra in sigs.items():
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [c_u8p, u16] + extra
        ck = Checker(ref)

        def run(label, entry, mem, regs=None, args=(), snap=lambda img: ()):
            regs = regs or {}
            ref_regs, img = ref.run(entry, mem, regs)
            buf = c_buffer(mem)
            r = getattr(lib, entry)(buf, SYSDAT, *args)
            ck.compare(label, img, buf, (ref_regs['bx'], ref_regs['cx']), (r.bx, r.cx), snap)

        attach = lambda n: lambda img: r16(img, ref.ccb(n, 'c_attach'))  # noqa: E731

        # -- attach --
        for label, initial in (("conattach_entry: free -> attach", 0), ("conattach_entry: already owner", PD_RUN)):
            mem = ref.base_memory()
            w16(mem, ref.ccb(0, 'c_attach'), initial)
            run(label, 'conattach_entry', mem, snap=attach(0))

        mem = ref.base_memory()
        w16(mem, ref.ccb(0, 'c_attach'), PD_B)
        run("cconattch_entry: busy, conditional -> error", 'cconattch_entry', mem, snap=attach(0))

        mem = ref.base_memory()
        w16(mem, ref.ccb(0, 'c_attach'), 0xFFFF)   # console 0 mimics console 1, which we own
        mem[ref.ccb(0, 'c_msource')] = 1
        w16(mem, ref.ccb(1, 'c_attach'), PD_RUN)
        run("conattach_entry: mimic device, source already owned", 'conattach_entry', mem, snap=attach(0))

        run("lstattach_entry: free -> attach", 'lstattach_entry', ref.base_memory(), snap=attach(NCONDEV))

        mem = ref.base_memory()
        w16(mem, ref.ccb(1, 'c_attach'), PD_B)
        mem[ref.pd(PD_RUN, 'p_lst')] = 1
        run("clstattch_entry: busy, conditional -> error", 'clstattch_entry', mem)

        # -- detach --
        mem = ref.base_memory()
        w16(mem, ref.ccb(NCONDEV, 'c_attach'), PD_RUN)
        w16(mem, ref.ccb(NCONDEV, 'c_queue'), PD_B)
        run("lstdetach_entry: owner detaches, next waiter takes over", 'lstdetach_entry', mem,
            snap=attach(NCONDEV))

        mem = ref.base_memory()
        w16(mem, ref.ccb(0, 'c_attach'), PD_B)
        run("condetach_entry: not owner -> error", 'condetach_entry', mem)
        run("condetach_entry: not attached -> ok", 'condetach_entry', ref.base_memory())

        # -- defaults --
        mem = ref.base_memory()
        mem[ref.pd(PD_RUN, 'p_cns')] = 1
        run("getdefcon_entry", 'getdefcon_entry', mem)
        mem = ref.base_memory()
        mem[ref.pd(PD_RUN, 'p_lst')] = NCONDEV + 1
        run("getdeflst_entry", 'getdeflst_entry', mem)

        run("setdefcon_entry: illegal console", 'setdefcon_entry', ref.base_memory(), {'dx': NCONDEV}, (NCONDEV,))
        mem = ref.base_memory()
        w16(mem, ref.ccb(0, 'c_attach'), PD_RUN)
        run("setdefcon_entry: switch console, detaches old", 'setdefcon_entry', mem, {'dx': 1}, (1,),
            lambda i: (r16(i, ref.ccb(0, 'c_attach')), i[ref.pd(PD_RUN, 'p_cns')]))
        run("setdeflst_entry: illegal list device", 'setdeflst_entry', ref.base_memory(), {'dx': 2}, (2,))
        mem = ref.base_memory()
        w16(mem, ref.ccb(NCONDEV, 'c_attach'), PD_RUN)
        run("setdeflst_entry: switch list device, detaches old", 'setdeflst_entry', mem, {'dx': 1}, (1,),
            lambda i: (r16(i, ref.ccb(NCONDEV, 'c_attach')), i[ref.pd(PD_RUN, 'p_lst')]))

        # -- conassign --
        def acb(mem, cns, match=0, pd=0, name=b''):
            mem[wk(ACB + s['acb_cns'])] = cns
            mem[wk(ACB + s['acb_match'])] = match
            w16(mem, wk(ACB + s['acb_pd']), pd)
            mem[wk(ACB + s['acb_name']):wk(ACB + s['acb_name']) + 8] = (name + b' ' * 8)[:8]
            return mem

        def threaded(mem):
            """thread list: RUNNER -> ECHO (console 0) -> ECHO (console 1)"""
            w16(mem, sd(s['thrdrt']), PD_RUN)
            w16(mem, ref.pd(PD_RUN, 'p_thread'), PD_B)
            w16(mem, ref.pd(PD_B, 'p_thread'), PD_C)
            for p, cns in ((PD_B, 0), (PD_C, 1)):
                mem[ref.pd(p, 'p_name'):ref.pd(p, 'p_name') + 8] = b'ECHO    '
                mem[ref.pd(p, 'p_cns')] = cns
            return mem

        def assign(label, mem, snap=attach(1)):
            run(label, 'conassign_entry', mem, {'dx': ACB}, (ACB,), snap)

        assign("conassign_entry: illegal console", acb(ref.base_memory(), 3))
        mem = acb(ref.base_memory(), 1, pd=PD_B)
        w16(mem, ref.ccb(1, 'c_attach'), PD_C)
        assign("conassign_entry: not owner", mem)
        mem = acb(ref.base_memory(), 1, pd=PD_B)
        w16(mem, ref.ccb(1, 'c_queue'), PD_C)      # PD_B waits in console 1's queue
        w16(mem, ref.pd(PD_C, 'p_link'), PD_B)
        w16(mem, ref.pd(PD_B, 'p_link'), 0x2300)
        assign("conassign_entry: direct PD, taken out of the queue", mem,
               lambda i: (r16(i, ref.ccb(1, 'c_attach')), r16(i, ref.ccb(1, 'c_queue')),
                          r16(i, ref.pd(PD_C, 'p_link')), r16(i, ref.pd(PD_B, 'p_link'))))
        assign("conassign_entry: by name, first match", threaded(acb(ref.base_memory(), 1, name=b'ECHO')))
        assign("conassign_entry: by name, console must match -> second ECHO",
               threaded(acb(ref.base_memory(), 1, match=1, name=b'ECHO')))
        assign("conassign_entry: by name, not found", threaded(acb(ref.base_memory(), 1, name=b'NOBODY')))
        assign("conassign_entry: direct PD, console mismatch -> search after it",
               threaded(acb(ref.base_memory(), 1, match=1, pd=PD_B, name=b'ECHO')))

        # -- mimic --
        mem = ref.base_memory()
        w16(mem, ref.ccb(0, 'c_attach'), PD_RUN)
        mimic_snap = lambda i: (r16(i, ref.ccb(2, 'c_attach')), i[ref.ccb(2, 'c_msource')],  # noqa: E731
                                i[ref.ccb(0, 'c_mimic')])
        run("mimic_entry: success", 'mimic_entry', mem, {'dx': 0x0002}, (0, 2), mimic_snap)
        run("mimic_entry: not owner -> error", 'mimic_entry', ref.base_memory(), {'dx': 0x0002}, (0, 2))

        # device 3 (list 1) mimics console 0
        mem = ref.base_memory()
        w16(mem, ref.ccb(3, 'c_attach'), 0xFFFF)
        mem[ref.ccb(3, 'c_msource')] = 0
        mem[ref.ccb(0, 'c_mimic')] = 3
        run("unmimic_entry: success", 'unmimic_entry', mem, {'dx': 3}, (3,),
            lambda i: (r16(i, ref.ccb(3, 'c_attach')), i[ref.ccb(3, 'c_msource')], i[ref.ccb(0, 'c_mimic')],
                       i[ref.pd(PD_RUN, 'p_lst')]))
        run("unmimic_entry: not a mimic -> error", 'unmimic_entry', ref.base_memory(), {'dx': 2}, (2,))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
