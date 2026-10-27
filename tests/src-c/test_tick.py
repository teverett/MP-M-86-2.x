#!/usr/bin/env python3
"""Differential test for src-c/kernel/tick.c (`notick:`/`tick:`) against
src-assem/kernel/TICK.SUP, on real memory layout -- see tests/src-c/supref.py.
`dlr`/`drl` and the PDs are SYSDAT-relative; notick's `mov u_wrkseg,ds`
writes the running process's UDA.

tick_once is one pass of `tick:` after its flag wait (`int mpmint`, a
stub): the reference starts right after that INT and stops when the
code loops back to `tick` or falls into `notick`. notick_once is
`notick:` up to its fall-through into `tick:`, its XIOS stop-clock call
captured by the XIOS hook. Every case diffs the whole image.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, PD_RUN, SYSDAT, UDA, build_lib, c_buffer, phys, sd, w16, r16, KERNEL_C


def main():
    ref = SysRef()
    v = ref.v
    code = ref.sup.code
    after_int = next(i for i in range(v('tick'), v('tick') + 16) if code[i] == 0xCD) + 2
    lib_path = build_lib([os.path.join(KERNEL_C, 'tick.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.notick_once.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        lib.tick_once.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        ck = Checker(ref)

        # -- notick --
        mem = ref.base_memory()
        w16(mem, phys(UDA, v('u_wrkseg')), 0x4321)
        seen = []
        ref.xios = lambda sim: seen.append(sim.regs['ax'] & 0xFF)
        _, img = ref.run('notick', mem, {}, stop_addrs={v('tick')})
        buf = c_buffer(mem)
        lib.notick_once(buf, SYSDAT)
        ck.compare("notick: u_wrkseg = DS (SYSDAT), clock stopped", img, buf, (), (),
                   lambda i: r16(i, phys(UDA, v('u_wrkseg'))))
        ck.check("notick: XIOS io_stopclk", [v('io_stopclk')], seen)

        # -- tick --
        PDS = (0x2000, 0x2040, 0x2080)

        def delay_list(entries, drl=0x2999):
            mem = ref.base_memory()
            w16(mem, sd(v('drl')), drl)
            w16(mem, sd(v('dlr')), entries[0][0] if entries else 0)
            for addr, wait, link in entries:
                w16(mem, sd(addr + v('p_wait')), wait)
                w16(mem, sd(addr + v('p_link')), link)
                mem[sd(addr + v('p_stat'))] = 0xFF
            return mem

        cases = [
            ("empty delay list", []),
            ("head still waiting", [(PDS[0], 3, 0)]),
            ("head expires, list empties", [(PDS[0], 1, 0)]),
            ("head expires, next still waiting", [(PDS[0], 1, PDS[1]), (PDS[1], 2, 0)]),
            ("two expire on the same tick", [(PDS[0], 1, PDS[1]), (PDS[1], 0, PDS[2]), (PDS[2], 4, 0)]),
        ]
        for label, entries in cases:
            mem = delay_list(entries)
            _, img = ref.run(after_int, mem, {}, stop_addrs={v('tick'), v('notick')})
            buf = c_buffer(mem)
            lib.tick_once(buf, SYSDAT)
            ck.compare(f"tick: {label}", img, buf, (), (),
                       lambda i: (r16(i, sd(v('dlr'))), r16(i, sd(v('drl')))))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
