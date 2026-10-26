#!/usr/bin/env python3
"""Differential test for src-c/kernel/clock.c against src/rsp/CLOCK.A86's
`clockloop:` body, on real memory layout: CLOCK's code at its own
segment, DS = SYSDAT and BX = tod_offset (what `clock:` sets up), the
TOD structure in SYSDAT. The reference runs one pass, from clockloop
back to clockloop (every branch ends in `jmp clockloop`); its `int
mpmint` calls (flag wait/set) are no-op boundaries, as in clock.c.
Every case diffs the whole image (code and stack aside).
"""
import ctypes
import os
import sys

from memref import REPO_ROOT, KERNEL_C, build_lib, c_buffer, phys, w16, r16
from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

CLOCK_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'CLOCK.A86')
SYSDAT, CLOCK_CS = 0x0300, 0x0100
STACK_SEG, STACK_TOP = 0x0800, 0x0100
TOD = 0x7E


def main():
    asm = Assembler()
    asm.assemble(CLOCK_A86, [])
    code = asm.segment_bytes('cseg')
    clockloop = asm.symtab['clockloop'].value
    skip = set(range(phys(CLOCK_CS), phys(CLOCK_CS, len(code)))) | \
        set(range(phys(STACK_SEG, STACK_TOP - 8), phys(STACK_SEG, STACK_TOP)))
    lib_path = build_lib([os.path.join(KERNEL_C, 'clock.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.clockloop.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        lib.clockloop.restype = None
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        for label, day, hour, minute, sec in (
                ("plain second tick", 1, 0x10, 0x30, 0x45),
                ("second rolls over to minute", 1, 0x10, 0x30, 0x59),
                ("minute rolls over to hour", 1, 0x10, 0x59, 0x59),
                ("hour rolls over to day", 1, 0x23, 0x59, 0x59),
                ("day from a large value", 0x0099, 0x23, 0x59, 0x59)):
            mem = bytearray(0x10000)
            w16(mem, phys(SYSDAT, TOD), day)
            mem[phys(SYSDAT, TOD + 2):phys(SYSDAT, TOD + 5)] = bytes((hour, minute, sec))
            ref = bytearray(mem)
            ref[phys(CLOCK_CS):phys(CLOCK_CS, len(code))] = code
            sim = I86Sim(ref, {'cs': CLOCK_CS, 'ds': SYSDAT, 'es': SYSDAT, 'ss': STACK_SEG, 'sp': STACK_TOP,
                               'bx': TOD}, phys(CLOCK_CS, clockloop), segmented=True,
                         stop_addrs={phys(CLOCK_CS, clockloop)}, int_stub_nums={224})
            sim.step()      # entry = stop address: take the first step by hand
            sim.run()
            buf = c_buffer(mem)
            lib.clockloop(buf, SYSDAT)
            tod = lambda m: (r16(m, phys(SYSDAT, TOD)), bytes(m[phys(SYSDAT, TOD + 2):phys(SYSDAT, TOD + 5)]))  # noqa: E731
            check(label, tod(ref), tod(buf))
            check(f"{label}: no other bytes differ", [],
                  [hex(a) for a in range(0x10000) if a not in skip and ref[a] != buf[a]][:8])
        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
