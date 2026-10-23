#!/usr/bin/env python3
"""Differential test for src-c/kernel/tick.c against src/kernel/TICK.SUP's
notick:/tick: body.

TICK.SUP needs tests/src-c/fixtures/tick_wrapper.A86 (no DSEG/EQUs of
its own -- every field it touches is declared by other files in the
real build). `rlr`/`drl`/`dlr`/`u_wrkseg` sit at the same scratch
addresses src-c/kernel/defs.h uses (SD_RLR/SD_DRL/SD_DLR/UDA_WRKSEG).

Two separate reference runs, matching tick.c's own two functions (see
its header comment for why `notick:`/`tick:` can't share one pass the
way CLOCK.A86/ECHO.A86's single loop body does): `notick_once` starts
at `notick` and stops at `tick` (always a straight fallthrough, no
ambiguity); `tick_once` starts at `tick` and stops at {`tick`,
`notick`} (both are real loop-back targets for "one tick's worth of
work" -- same stop_addrs-equals-entry workaround as test_clock.py's
own clockloop: force one real step first).
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

TICK_SUP = os.path.join(REPO_ROOT, 'src', 'kernel', 'TICK.SUP')
TICK_WRAPPER = os.path.join(REPO_ROOT, 'tests', 'src-c', 'fixtures', 'tick_wrapper.A86')
TICK_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'tick.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR = 0xA000
SD_DRL = 0xA002
SD_DLR = 0xA004
UDA_WRKSEG = 0xA012
P_WAIT = 0x1A
P_LINK = 0x00
P_STAT = 0x04
PS_RUN = 0x00


def assemble_reference():
    asm = Assembler()
    asm.assemble(TICK_WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    notick = asm.symtab['notick'].value
    tick = asm.symtab['tick'].value
    xiosif = asm.symtab['xiosif'].value
    return code, notick, tick, xiosif


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, TICK_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


# --- notick_once ---

def run_notick_reference(code, notick, tick, xiosif, ds):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    mem_w16(image, SD_RLR, 0x2000)  # some PD address, unused beyond p_uda read
    sim = I86Sim(image, {'sp': STACK_TOP, 'ds': ds}, notick, stop_addrs={tick},
                 call_stub_addrs={xiosif})
    sim.run()
    return mem_r16(image, UDA_WRKSEG)


def run_notick_port(lib, ds):
    mem = bytearray(0x10000)
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.notick_once(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), ctypes.c_uint16(ds))
    return mem_r16(bytes(buf), UDA_WRKSEG)


NOTICK_CASES = [
    ("ds = 0x1234", 0x1234),
    ("ds = 0", 0),
    ("ds = 0xFFFF", 0xFFFF),
]


# --- tick_once ---

def build_delay_list(mem, entries):
    """entries: list of (pd_addr, wait, link_to_next_pd_or_0). First
    entry's address becomes dlr."""
    if not entries:
        mem_w16(mem, SD_DLR, 0)
        return
    mem_w16(mem, SD_DLR, entries[0][0])
    for addr, wait, link in entries:
        mem_w16(mem, addr + P_WAIT, wait)
        mem_w16(mem, addr + P_LINK, link)
        mem[addr + P_STAT] = 0xFF  # sentinel, overwritten with ps_run if moved


def run_tick_reference(code, notick, tick, drl, entries):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    mem_w16(image, SD_DRL, drl)
    build_delay_list(image, entries)
    sim = I86Sim(image, {'sp': STACK_TOP}, tick, stop_addrs={tick, notick},
                 int_stub_nums={224})
    sim.step()
    sim.run()
    return (mem_r16(image, SD_DLR), mem_r16(image, SD_DRL),
            [(a, mem_r16(image, a + P_WAIT), mem_r16(image, a + P_LINK), image[a + P_STAT])
             for a, _, _ in entries])


def run_tick_port(lib, drl, entries):
    mem = bytearray(0x10000)
    mem_w16(mem, SD_DRL, drl)
    build_delay_list(mem, entries)
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.tick_once(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)))
    image = bytes(buf)
    return (mem_r16(image, SD_DLR), mem_r16(image, SD_DRL),
            [(a, mem_r16(image, a + P_WAIT), mem_r16(image, a + P_LINK), image[a + P_STAT])
             for a, _, _ in entries])


TICK_CASES = [
    ("empty delay list", 0, 0, []),
    ("single entry, not yet expired", 0, 0, [(0x3000, 5, 0)]),
    ("single entry, expires this tick, empty ready list", 0, 0, [(0x3000, 1, 0)]),
    ("single entry, expires this tick, non-empty ready list", 0x4000, 0,
     [(0x3000, 1, 0)]),
    ("two entries, head not yet expired", 0, 0, [(0x3000, 3, 0x3100), (0x3100, 0, 0)]),
    ("two entries, head expires, next doesn't (delta > 0)", 0, 0,
     [(0x3000, 1, 0x3100), (0x3100, 4, 0)]),
    ("two entries, both expire same tick (delta 0)", 0, 0,
     [(0x3000, 1, 0x3100), (0x3100, 0, 0)]),
    ("three entries, first two expire same tick, third doesn't", 0, 0,
     [(0x3000, 1, 0x3100), (0x3100, 0, 0x3200), (0x3200, 2, 0)]),
]


def main():
    code, notick, tick, xiosif = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.notick_once.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        lib.tick_once.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        failures = []

        for label, ds in NOTICK_CASES:
            ref = run_notick_reference(code, notick, tick, xiosif, ds)
            port = run_notick_port(lib, ds)
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] notick_once: {label}: reference {ref:#06x} | port {port:#06x}")
            if status == "FAIL":
                failures.append(label)

        for label, drl, dlr_unused, entries in TICK_CASES:
            ref = run_tick_reference(code, notick, tick, drl, entries)
            port = run_tick_port(lib, drl, entries)
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] tick_once: {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {len(NOTICK_CASES) + len(TICK_CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
