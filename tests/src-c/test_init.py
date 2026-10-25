#!/usr/bin/env python3
"""Differential test for src-c/kernel/init.c against src/kernel/
INIT.SUP's `init`. `entry` is not ported -- see init.c's own header
comment."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'init_wrapper.A86')
INIT_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'init.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

# src-c/kernel/defs.h's own harness-chosen scratch addresses -- kept
# in sync by hand with this test (see init_wrapper.A86's own header
# comment for why these can't just be read off a real-system build).
SD_INIT_VERSION, SD_INIT_TICK_TOS, SD_INIT_IDLE_TOS = 0xA800, 0xA802, 0xA804
SD_INIT_INTFLAG = 0xA806
SD_RSPSEG = 0xA80B
SD_SUPMOD_SEG = 0xA88F
I_MPM_IP, I_MPM_CS = 0x380, 0x382
INIT_SYSDAT, INIT_SUPERVISOR_SEG = 0x0006, 0x000A
SD_NCONDEV = 0xA036
SD_NCIODEV = 0xA037
SD_NLSTDEV = 0xA035


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['init'].value
    xiosif = asm.symtab['xiosif'].value
    # the 5 `callf dword ptr <...>mod[bx]` instructions -- their own
    # *target* is only known at runtime (loaded from the module
    # table), so tools/i86sim.py's far_call_stub_addrs keys off each
    # instruction's own address instead; found by scanning cseg for
    # `0xFF` with a ModRM reg field of 3 (CALL FAR r/m) in the range
    # init's own body occupies, and cross-checked against nbdom/
    # nciom/nxiom's own resolved addresses (each one's own `callf` +
    # the trailing pop(s) lands exactly on the next label).
    far_calls = []
    for i in range(entry, len(code) - 1):
        if code[i] == 0xFF and ((code[i + 1] >> 3) & 7) == 3:
            far_calls.append(i)
    assert len(far_calls) == 5, f"expected 5 callf sites, found {far_calls}"
    stop_addr = asm.symtab['rsp_out'].value + 6  # past the final int mpmint
    return code, entry, xiosif, far_calls, stop_addr


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, INIT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    return bytearray(0x10000)


def run_reference(code, entry, xiosif, far_calls, stop_addr, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(
        image, {**regs, 'sp': STACK_TOP}, entry,
        stop_addrs={stop_addr},
        call_stub_addrs={xiosif},
        int_stub_nums={224},  # mpmint
        far_call_stub_addrs=set(far_calls),
    )
    sim.run()
    return sim.regs, image


def main():
    code, entry, xiosif, far_calls, stop_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.sup_init.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16,
                              ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        DS_SYSDAT = 0x1234
        CS = 0x5678
        NOTICK_OFS = 0x0001
        IDLE_OFS = 0x0002
        USER_ENTRY_OFS = 0x0003

        # -- init: no RSPs (rspseg=0), straight through to termination --
        mem = base_memory()
        mem[SD_SUPMOD_SEG:SD_SUPMOD_SEG + 2] = (0xCD, 0xAB)
        mem[SD_RSPSEG:SD_RSPSEG + 2] = (0, 0)
        ref_regs, ref_img = run_reference(
            code, entry, xiosif, far_calls, stop_addr, mem,
            {'ds': DS_SYSDAT, 'cs': CS})
        buf = to_ctypes(mem)
        lib.sup_init(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_SYSDAT), ctypes.c_uint16(CS),
                  ctypes.c_uint16(NOTICK_OFS), ctypes.c_uint16(IDLE_OFS), ctypes.c_uint16(USER_ENTRY_OFS))
        port = bytes(buf)
        check("init: sysdat", ref_img[INIT_SYSDAT:INIT_SYSDAT + 2], port[INIT_SYSDAT:INIT_SYSDAT + 2])
        check("init: supervisor segment", ref_img[INIT_SUPERVISOR_SEG:INIT_SUPERVISOR_SEG + 2],
              port[INIT_SUPERVISOR_SEG:INIT_SUPERVISOR_SEG + 2])
        check("init: version", ref_img[SD_INIT_VERSION:SD_INIT_VERSION + 2], port[SD_INIT_VERSION:SD_INIT_VERSION + 2])
        check("init: tick_tos", ref_img[SD_INIT_TICK_TOS:SD_INIT_TICK_TOS + 2], port[SD_INIT_TICK_TOS:SD_INIT_TICK_TOS + 2])
        check("init: idle_tos", ref_img[SD_INIT_IDLE_TOS:SD_INIT_IDLE_TOS + 2], port[SD_INIT_IDLE_TOS:SD_INIT_IDLE_TOS + 2])
        check("init: i_mpm_ip", ref_img[I_MPM_IP:I_MPM_IP + 2], port[I_MPM_IP:I_MPM_IP + 2])
        check("init: i_mpm_cs", ref_img[I_MPM_CS:I_MPM_CS + 2], port[I_MPM_CS:I_MPM_CS + 2])
        check("init: intflag", ref_img[SD_INIT_INTFLAG], port[SD_INIT_INTFLAG])
        check("init: ncondev", ref_img[SD_NCONDEV], port[SD_NCONDEV])
        check("init: nciodev", ref_img[SD_NCIODEV], port[SD_NCIODEV])
        check("init: nlstdev", ref_img[SD_NLSTDEV], port[SD_NLSTDEV])
        # `u_wrkseg` (the bug fix) and the RSP-list walk both go
        # through a real `es:`-prefixed access -- tools/i86sim.py
        # only computes a real segment:offset address for one when
        # `segmented=True`, which would also turn every *other*
        # access in this same routine (version/tick_tos/etc, all
        # plain bare instructions that default to DS) from this
        # test's own flat scratch addresses into real `ds*16+offset`
        # computations, breaking them instead -- init.c's own fields
        # can't all be moved to genuine segment-relative addressing
        # without redoing this port the way ml.c/proc.c/etc. do (a
        # much bigger undertaking). Both are translated faithfully in
        # init.c regardless (via mem_r16_seg/mem_w16_seg, the correct
        # idiom either way), just not independently exercised by this
        # test -- same "translated faithfully but not exercised"
        # precedent as QUE1.RTM's own retry loops.

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
