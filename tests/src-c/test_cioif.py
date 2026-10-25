#!/usr/bin/env python3
"""Differential test for src-c/kernel/cioif.c against src/kernel/
CIOIF.CIO's `init`. `entry` (CIOIF.CIO's own inter-module CIO
dispatch) is now ported too -- see tests/src-c/test_cioif_entry.py
for its own dedicated differential test (it needs CONSOLE.CIO/
CHARIO.CIO linked in for real, unlike this file's own init-only
wrapper)."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'cioif_wrapper.A86')
# cioif.c's own `entry` now calls into console.c/chario.c -- link all
# three, even though this test only exercises `init` (same 3-file
# build tests/src-c/test_cioif_entry.py uses for `entry` itself).
CIOIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in ('cioif.c', 'console.c', 'chario.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

SD_SUPMOD_OFF, SD_SUPMOD_SEG = 0xA88D, 0xA88F
CIOIF_SYSDAT, CIOIF_SUPERVISOR_OFF, CIOIF_SUPERVISOR_SEG = 0x0006, 0x0008, 0x000A


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['init'].value
    entry_label = asm.symtab['entry'].value
    # init's own body has no call/callf/jmp of any kind -- a straight
    # 7-instruction run ending in a far RET -- so the stop point is
    # just that RETF's own address, found the same way this project
    # already scans for other fixed opcodes (0xCB = RETF).
    retf_addrs = [i for i in range(entry, entry_label) if code[i] == 0xCB]
    assert len(retf_addrs) == 1, f"expected exactly 1 retf, found {retf_addrs}"
    return code, entry, retf_addrs[0]


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *CIOIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    return bytearray(0x10000)


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def run_reference(code, entry, stop_addr, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs={stop_addr})
    sim.run()
    return sim.regs, image


def main():
    code, entry, stop_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.cioif_init.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        DS_SYSDAT = 0x1234

        # -- init: copies ds into its own sysdat cell, and both halves
        # of SYSDAT's own module_table[SUP] entry pointer into its own
        # supervisor cell --
        mem = base_memory()
        mem_w16(mem, SD_SUPMOD_OFF, 0x0718)
        mem_w16(mem, SD_SUPMOD_SEG, 0x1000)
        ref_regs, ref_img = run_reference(code, entry, stop_addr, mem, {'ds': DS_SYSDAT})
        buf = to_ctypes(mem)
        lib.cioif_init(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_SYSDAT))
        port = bytes(buf)
        check("init: sysdat", ref_img[CIOIF_SYSDAT:CIOIF_SYSDAT + 2], port[CIOIF_SYSDAT:CIOIF_SYSDAT + 2])
        check("init: supervisor offset", ref_img[CIOIF_SUPERVISOR_OFF:CIOIF_SUPERVISOR_OFF + 2],
              port[CIOIF_SUPERVISOR_OFF:CIOIF_SUPERVISOR_OFF + 2])
        check("init: supervisor segment", ref_img[CIOIF_SUPERVISOR_SEG:CIOIF_SUPERVISOR_SEG + 2],
              port[CIOIF_SUPERVISOR_SEG:CIOIF_SUPERVISOR_SEG + 2])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
