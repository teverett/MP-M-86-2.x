#!/usr/bin/env python3
"""Differential test for src-c/kernel/echo.c against src/rsp/ECHO.A86's
`loop:` body, on real memory layout: ECHO's code at its own segment, DS
= its data segment, ES = SYSDAT (`main:`'s `mov es,sdatseg`). The
reference runs from `main` to the `jmps loop` at the end of one pass;
every `int mpmint` (make/open/read queue, set priority, set console,
write, detach) is a no-op boundary, as in echo.c, so the queue message
-- the sending process's PD, then the command tail -- is preset in
ECHO's qpb buffer (pdadr/cmdtail). Every case diffs the whole image.
"""
import ctypes
import os
import sys

from memref import REPO_ROOT, KERNEL_C, build_lib, c_buffer, phys, w16
from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

ECHO_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'ECHO.A86')
SYSDAT, ECHO_CS, ECHO_DS = 0x0300, 0x0100, 0x0200
STACK_SEG, STACK_TOP = 0x0800, 0x0100
PD = 0x0900                 # the sender's PD (a SYSDAT offset)
P_CNS = 0x20


def main():
    asm = Assembler()
    asm.assemble(ECHO_A86, [])
    code, data = asm.segment_bytes('cseg'), asm.segment_bytes('dseg')
    s = {k: v.value for k, v in asm.symtab.items()}
    jmps_loop = [i for i in range(s['loop'], len(code) - 1)
                 if code[i] == 0xEB and (i + 2 + (code[i + 1] - 256 if code[i + 1] > 127 else code[i + 1])) == s['loop']]
    assert len(jmps_loop) == 1, jmps_loop
    skip = set(range(phys(ECHO_CS), phys(ECHO_CS, len(code)))) | \
        set(range(phys(STACK_SEG, STACK_TOP - 8), phys(STACK_SEG, STACK_TOP)))
    lib_path = build_lib([os.path.join(KERNEL_C, 'echo.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.echo_loop_once.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        lib.echo_loop_once.restype = None
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        for label, cns, tail in (("$ terminator mid-buffer", 7, b"HELLO$WORLD" + b" " * 120),
                                 ("NUL terminator mid-buffer", 3, b"DIR A:\x00" + b" " * 124),
                                 ("no terminator, runs off the end", 0, b"X" * 131 + b" " * 10),
                                 ("empty command tail", 9, b"\x00" + b" " * 130)):
            mem = bytearray(0x10000)
            mem[phys(ECHO_DS):phys(ECHO_DS, len(data))] = data
            w16(mem, phys(ECHO_DS, s['sdatseg']), SYSDAT)
            w16(mem, phys(ECHO_DS, s['pdadr']), PD)
            mem[phys(SYSDAT, PD + P_CNS)] = cns
            mem[phys(ECHO_DS, s['cmdtail']):phys(ECHO_DS, s['cmdtail'] + len(tail))] = tail
            ref = bytearray(mem)
            ref[phys(ECHO_CS):phys(ECHO_CS, len(code))] = code
            sim = I86Sim(ref, {'cs': ECHO_CS, 'ds': ECHO_DS, 'es': ECHO_DS, 'ss': STACK_SEG, 'sp': STACK_TOP},
                         phys(ECHO_CS, s['main']), segmented=True, stop_addrs={phys(ECHO_CS, jmps_loop[0])},
                         int_stub_nums={224})
            sim.run()
            buf = c_buffer(mem)
            lib.echo_loop_once(buf, ECHO_DS)
            snap = lambda m: (m[phys(ECHO_DS, s['console'])],  # noqa: E731
                              bytes(m[phys(ECHO_DS, s['cmdtail']):phys(ECHO_DS, s['cmdtail'] + 24)]))
            check(label, snap(ref), snap(buf))
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
