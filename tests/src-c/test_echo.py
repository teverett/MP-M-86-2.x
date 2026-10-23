#!/usr/bin/env python3
"""Differential test for src-c/kernel/echo.c against src/rsp/ECHO.A86's
main:/loop: body.

ECHO.A86 assembles standalone (no wrapper fixture needed -- it has no
INCLUDEs at all). The reference run starts at `main` (so the one-time
setup also executes) and stops at the address of the final `jmps loop`
instruction -- the boundary this file's own header comment explains.
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

ECHO_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'ECHO.A86')
ECHO_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'echo.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
PD_ADDR = 0x1000
P_CNS = 0x20
JMPS_LOOP_ADDR = 0x83  # the one and only `jmps loop`, confirmed via ndisasm


def assemble_reference():
    asm = Assembler()
    asm.assemble(ECHO_A86, [])
    code = asm.segment_bytes('cseg')
    main = asm.symtab['main'].value
    mpm = asm.symtab['mpm'].value
    pdadr = asm.symtab['pdadr'].value
    cmdtail = asm.symtab['cmdtail'].value
    console = asm.symtab['console'].value
    return code, main, mpm, pdadr, cmdtail, console


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, ECHO_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory(pdadr, cmdtail, console_byte, tail_bytes):
    mem = bytearray(0x10000)
    mem_w16(mem, pdadr, PD_ADDR)
    mem[PD_ADDR + P_CNS] = console_byte
    mem[cmdtail:cmdtail + len(tail_bytes)] = tail_bytes
    return mem


def run_reference(code, main, mpm, mem):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {'sp': STACK_TOP}, main, stop_addrs={JMPS_LOOP_ADDR}, call_stub_addrs={mpm})
    sim.run()
    return image


def run_port(lib, mem):
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.echo_loop_once(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)))
    return bytes(buf)


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


CASES = [
    ("$ terminator mid-buffer", 7, b"HELLO$WORLD" + b" " * 120),
    ("NUL terminator mid-buffer", 3, b"DIR A:\x00" + b" " * 124),
    ("no terminator, runs off the end", 0, b"X" * 131 + b" " * 10),
    ("empty command tail", 9, b"\x00" + b" " * 130),
]


def main_test():
    code, main, mpm, pdadr, cmdtail, console = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.echo_loop_once.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        failures = []
        for label, console_byte, tail_bytes in CASES:
            mem = base_memory(pdadr, cmdtail, console_byte, tail_bytes)
            ref = run_reference(code, main, mpm, mem)
            port = run_port(lib, mem)
            ref_slice = (ref[console], ref[cmdtail:cmdtail + 140])
            port_slice = (port[console], port[cmdtail:cmdtail + 140])
            status = "ok" if ref_slice == port_slice else "FAIL"
            print(f"[{status}] {label}: reference console={ref_slice[0]} cmdtail={ref_slice[1][:20]!r} "
                  f"| port console={port_slice[0]} cmdtail={port_slice[1][:20]!r}")
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
    sys.exit(main_test())
