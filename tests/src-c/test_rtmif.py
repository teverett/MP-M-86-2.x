#!/usr/bin/env python3
"""Differential test for src-c/kernel/rtmif.c against src/kernel/
RTMIF.RTM's `init`. `entry` (RTMIF.RTM's own inter-module RTM
dispatch) is now ported too -- see tests/src-c/test_rtmif_entry.py
for its own dedicated differential test. `proc_creat` is a boundary
stub in the reference
wrapper (see tests/src-c/fixtures/rtmif_wrapper.A86's own header
comment for why) -- the C port still makes the real call into
proc.c's own already-verified `proc_creat`, so only the parts of
`init` genuinely under test here (sysdat/supervisor copy,
dispatcher/rtm_pdisp far pointers, tick/idle UDA setup) are checked.

Needs tools/i86sim.py's opt-in `segmented=True` mode: `init` itself
runs with a genuinely non-zero `DS=sysdat` throughout (per its own
"assume DS = Sysdat Segment" precondition), and switches `ES` to a
dynamically-computed segment for the tick/idle UDA writes."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'rtmif_wrapper.A86')
# rtmif.c's own `entry` now calls into sysent.c/dsptch.c/flag.c/
# que1.c/que2.c/findpd.c/abort.c too -- link all nine, even though
# this test only exercises `init` (same build
# tests/src-c/test_rtmif_entry.py uses for `entry` itself).
RTMIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f)
           for f in ('rtmif.c', 'proc.c', 'sysent.c', 'dsptch.c', 'flag.c',
                     'que1.c', 'que2.c', 'findpd.c', 'abort.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

SYSDAT_SUPMOD, SYSDAT_DISPATCHER, SYSDAT_RTM_PDISP = 0x0000, 0x0038, 0x003C
SYSDAT_IDLEPD, SYSDAT_TICKPD = 0x09F0, 0x0A20
P_UDA = 0x10
U_INITCS, U_INITSS = 0x50, 0x56
RTMIF_SYSDAT, RTMIF_SUPERVISOR_OFF, RTMIF_SUPERVISOR_SEG = 0x0006, 0x0008, 0x000A


def phys(seg):
    return (seg * 16) & 0xFFFF


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['init'].value
    entry_label = asm.symtab['entry'].value
    retf_addrs = [i for i in range(entry, entry_label) if code[i] == 0xCB]
    assert len(retf_addrs) == 1, f"expected exactly 1 retf, found {retf_addrs}"
    fdisp = asm.symtab['fdisp'].value
    farpdisp = asm.symtab['farpdisp'].value
    return code, entry, retf_addrs[0], fdisp, farpdisp


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *RTMIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    return bytearray(0x10000)


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    addr &= 0xFFFF
    return mem[addr] | (mem[(addr + 1) & 0xFFFF] << 8)


def run_reference(code, entry, stop_addr, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs={stop_addr}, segmented=True)
    sim.run()
    return sim.regs, image


def main():
    code, entry, stop_addr, fdisp, farpdisp = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.rtmif_init.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # DS_SYSDAT is a genuine, non-degenerate segment (its own
        # physical base, DS_SYSDAT*16, must land clear of this
        # wrapper's own cseg bytes -- dispatcher's own raw offset
        # alone, 0x38, sits inside init's own body if sysdat were
        # ever flat-0) -- phys(DS_SYSDAT) = 0x2340, comfortably past
        # this wrapper's own ~0x300-byte cseg.
        DS_SYSDAT = 0x1234
        CS = 0x0000  # drives real instruction fetch under segmented=True
        IDLE_P_UDA = 0x0010
        TICK_P_UDA = 0x0020

        mem = base_memory()
        mem_w16(mem, phys(DS_SYSDAT) + SYSDAT_SUPMOD, 0x0718)
        mem_w16(mem, phys(DS_SYSDAT) + SYSDAT_SUPMOD + 2, 0x1000)
        mem_w16(mem, phys(DS_SYSDAT) + SYSDAT_IDLEPD + P_UDA, IDLE_P_UDA)
        mem_w16(mem, phys(DS_SYSDAT) + SYSDAT_TICKPD + P_UDA, TICK_P_UDA)

        ref_regs, ref_img = run_reference(code, entry, stop_addr, mem, {'ds': DS_SYSDAT, 'cs': CS})
        buf = to_ctypes(mem)
        lib.rtmif_init(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_SYSDAT), ctypes.c_uint16(CS),
                 ctypes.c_uint16(fdisp), ctypes.c_uint16(farpdisp))
        port = bytes(buf)

        check("init: sysdat", ref_img[RTMIF_SYSDAT:RTMIF_SYSDAT + 2], port[RTMIF_SYSDAT:RTMIF_SYSDAT + 2])
        check("init: supervisor offset", ref_img[RTMIF_SUPERVISOR_OFF:RTMIF_SUPERVISOR_OFF + 2],
              port[RTMIF_SUPERVISOR_OFF:RTMIF_SUPERVISOR_OFF + 2])
        check("init: supervisor segment", ref_img[RTMIF_SUPERVISOR_SEG:RTMIF_SUPERVISOR_SEG + 2],
              port[RTMIF_SUPERVISOR_SEG:RTMIF_SUPERVISOR_SEG + 2])

        disp_addr = phys(DS_SYSDAT) + SYSDAT_DISPATCHER
        rtmp_addr = phys(DS_SYSDAT) + SYSDAT_RTM_PDISP
        check("init: dispatcher far ptr", ref_img[disp_addr:disp_addr + 4], port[disp_addr:disp_addr + 4])
        check("init: rtm_pdisp far ptr", ref_img[rtmp_addr:rtmp_addr + 4], port[rtmp_addr:rtmp_addr + 4])

        idle_uda = (IDLE_P_UDA + DS_SYSDAT) & 0xFFFF
        tick_uda = (TICK_P_UDA + DS_SYSDAT) & 0xFFFF
        for name, uda in (("idle", idle_uda), ("tick", tick_uda)):
            ref = (mem_r16(ref_img, phys(uda) + U_INITCS), mem_r16(ref_img, phys(uda) + U_INITSS))
            got = (mem_r16(port, phys(uda) + U_INITCS), mem_r16(port, phys(uda) + U_INITSS))
            check(f"init: {name}'s own u_initcs/u_initss", ref, got)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
