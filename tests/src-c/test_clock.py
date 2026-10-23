#!/usr/bin/env python3
"""Differential test for src-c/kernel/clock.c against src/rsp/
CLOCK.A86's clockloop: body.

CLOCK.A86 assembles standalone (no wrapper fixture needed -- it has no
INCLUDEs at all), after fixing docs/bugs.md #1 (clock_loop/clockloop)
directly in src/rsp/CLOCK.A86. The reference run starts at `clockloop`
itself (skipping `clock:`'s own `mov ds,sysdat ! mov bx,tod_offset`
setup, moot under the flat memory model -- see clock.c's own header
comment) with bx preset to this test's own scratch TOD base address,
and stops at that same clockloop address -- every branch in the real
asm ends in `jmp clockloop`, same one-pass-through-the-loop convention
as test_echo.py's own `loop:`.
"""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

CLOCK_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'CLOCK.A86')
CLOCK_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'clock.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_CLOCK_TOD_BASE = 0xA044
TOD_DAY, TOD_HOUR, TOD_MIN, TOD_SEC = SD_CLOCK_TOD_BASE, SD_CLOCK_TOD_BASE + 2, SD_CLOCK_TOD_BASE + 3, SD_CLOCK_TOD_BASE + 4


def assemble_reference():
    asm = Assembler()
    asm.assemble(CLOCK_A86, [])
    code = asm.segment_bytes('cseg')
    clockloop = asm.symtab['clockloop'].value
    mpm = asm.symtab['mpm'].value
    return code, clockloop, mpm


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, CLOCK_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory(day, hour, minute, sec):
    mem = bytearray(0x10000)
    mem_w16(mem, TOD_DAY, day)
    mem[TOD_HOUR] = hour
    mem[TOD_MIN] = minute
    mem[TOD_SEC] = sec
    return mem


def run_reference(code, clockloop, mpm, mem):
    # Entry and stop address are the same here (every branch loops back
    # to clockloop itself) -- I86Sim.run() checks stop_addrs *before*
    # executing anything, so starting there too would halt instantly
    # with nothing run. Force one real step first so the loop body
    # actually executes, then run() normally until it loops back.
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {'sp': STACK_TOP, 'bx': SD_CLOCK_TOD_BASE}, clockloop,
                 stop_addrs={clockloop}, call_stub_addrs={mpm})
    sim.step()
    sim.run()
    return image


def run_port(lib, mem):
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.clockloop(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)))
    return bytes(buf)


CASES = [
    ("plain second tick", 1, 10, 30, 0x45),
    ("second rolls over to minute (0x59 -> 0x00, minute bumps)", 1, 10, 30, 0x59),
    ("minute rolls over to hour (0x59:0x59 -> hour bumps)", 1, 10, 59, 0x59),
    ("hour rolls over to day (0x23:0x59:0x59 -> day bumps)", 1, 0x23, 0x59, 0x59),
    ("day wraps from a large BCD value", 0x0099, 0x23, 0x59, 0x59),
]


def main():
    code, clockloop, mpm = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.clockloop.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        failures = []
        for label, day, hour, minute, sec in CASES:
            mem = base_memory(day, hour, minute, sec)
            ref = run_reference(code, clockloop, mpm, mem)
            port = run_port(lib, mem)
            ref_slice = (mem_r16(ref, TOD_DAY), ref[TOD_HOUR], ref[TOD_MIN], ref[TOD_SEC])
            port_slice = (mem_r16(port, TOD_DAY), port[TOD_HOUR], port[TOD_MIN], port[TOD_SEC])
            status = "ok" if ref_slice == port_slice else "FAIL"
            print(f"[{status}] {label}: reference {ref_slice} | port {port_slice}")
            if status == "FAIL":
                failures.append(label)
        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {len(CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
