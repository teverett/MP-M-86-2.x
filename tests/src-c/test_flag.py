#!/usr/bin/env python3
"""Differential test for src-c/kernel/flag.c against src/kernel/FLAG.RTM's
flag_set_entry and flag_wait_entry (and, through them, their shared
internal flag_valid/flag_exit/flag_bexit), both on real memory layout.

flag.c addresses real memory (defs.h's far_ptr), so the reference runs
with tools/i86sim.py's segmented=True: tests/src-c/fixtures/
flag_wrapper.A86 includes the real SYSDAT.FMT/UDA.FMT, SYSDAT is at
segment 100h with the flag table and PDs at SYSDAT-relative offsets
(DS-relative, as the real asm reaches them), and the running process's
UDA is the reference's ES and its PD's p_uda (flag.c's u_dparam goes
there). Field offsets come from the assembled symbol table, and every
case diffs the whole image to catch stray writes.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'flag_wrapper.A86')
FLAG_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'flag.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
SYSDAT = 0x0100
FLAG_TABLE = 0x2000   # SYSDAT offset; flglen (3) bytes per entry: word flg_pd, byte flg_ignore
FLG_PD, FLG_IGNORE, FLGLEN = 0x00, 0x02, 3
FLAG_OFF, FLAG_ON, FLAG_ZIG = 0xFFFF, 0xFFFE, 0xFF
PD_ADDR, UDA_CUR = 0x2100, 0x0400  # running process (SYSDAT offset) and its UDA
WAITER_PD = 0x2200
P_LINK, P_STAT, P_UDA = 0x00, 0x04, 0x10
DSPTCH_STUB = 0x9000
NFLAGS = 4


def phys(seg, off=0):
    return seg * 16 + off


def sd(off):
    return phys(SYSDAT, off)


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in
           ('flag_set_entry', 'flag_wait_entry', 'rlr', 'drl', 'nflags', 'flags', 'u_dparam')}
    return code, sym


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, FLAG_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def main():
    code, sym = assemble_reference()
    skip = set(range(len(code))) | set(range(0xFF00, SIM_MEM))  # code, harness stack (SS=0)
    entry_addr = lambda n: sd(FLAG_TABLE + n * FLGLEN)  # noqa: E731

    def base_memory(flag_entry_value=FLAG_OFF, flag_ignore=FLAG_ZIG):
        mem = bytearray(SIM_MEM)
        mem[sd(sym['nflags'])] = NFLAGS
        w16(mem, sd(sym['flags']), FLAG_TABLE)
        for i in range(NFLAGS):
            w16(mem, entry_addr(i) + FLG_PD, flag_entry_value)
            mem[entry_addr(i) + FLG_IGNORE] = flag_ignore
        w16(mem, sd(sym['rlr']), PD_ADDR)
        w16(mem, sd(PD_ADDR + P_UDA), UDA_CUR)
        w16(mem, sd(sym['drl']), 0x2999)
        return mem

    def run_reference(entry, mem, dl):
        image = bytearray(mem)
        image[0:len(code)] = code
        sim = I86Sim(image, {'dx': dl, 'ds': SYSDAT, 'es': UDA_CUR, 'ss': 0, 'sp': STACK_TOP, 'cs': 0},
                     sym[entry], call_stub_addrs={DSPTCH_STUB}, segmented=True)
        sim.run()
        return (sim.regs['bx'], sim.regs['cx']), image

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        for name in ('flag_set_entry', 'flag_wait_entry'):
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint8]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def run_case(label, entry, mem, dl, snap=lambda img: ()):
            ref_regs, ref = run_reference(entry, mem, dl)
            buf = (ctypes.c_uint8 * PHYS_MEM)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            r = getattr(lib, entry)(buf, SYSDAT, dl)
            port = bytes(buf)
            check(label, (ref_regs, snap(ref)), ((r.bx, r.cx), snap(port)))
            diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        # -- flag_set_entry --
        run_case("flag_set_entry: invalid flag number", 'flag_set_entry', base_memory(), NFLAGS)
        run_case("flag_set_entry: already on", 'flag_set_entry', base_memory(FLAG_ON), 1)
        run_case("flag_set_entry: off -> on", 'flag_set_entry', base_memory(FLAG_OFF), 1,
                 lambda img: r16(img, entry_addr(1) + FLG_PD))

        mem = base_memory(flag_entry_value=WAITER_PD)
        w16(mem, sd(WAITER_PD + P_LINK), 0)
        mem[sd(WAITER_PD + P_STAT)] = 0xEE
        run_case("flag_set_entry: wakes a waiting PD", 'flag_set_entry', mem, 2,
                 lambda img: (r16(img, entry_addr(2) + FLG_PD), img[sd(WAITER_PD + P_STAT)],
                              r16(img, sd(WAITER_PD + P_LINK)), r16(img, sd(sym['drl']))))

        run_case("flag_set_entry: ignore count decrements", 'flag_set_entry',
                 base_memory(FLAG_ON, flag_ignore=5), 3,
                 lambda img: img[entry_addr(3) + FLG_IGNORE])

        # -- flag_wait_entry --
        run_case("flag_wait_entry: invalid flag number", 'flag_wait_entry', base_memory(), NFLAGS)
        run_case("flag_wait_entry: on -> consumed", 'flag_wait_entry', base_memory(FLAG_ON), 1,
                 lambda img: r16(img, entry_addr(1) + FLG_PD))
        run_case("flag_wait_entry: off -> this process waits", 'flag_wait_entry', base_memory(FLAG_OFF), 2,
                 lambda img: (img[sd(PD_ADDR + P_STAT)], r16(img, phys(UDA_CUR, sym['u_dparam']))))
        run_case("flag_wait_entry: already a waiter -> underrun", 'flag_wait_entry',
                 base_memory(WAITER_PD), 3)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
