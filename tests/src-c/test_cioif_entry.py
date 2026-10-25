#!/usr/bin/env python3
"""Differential test for src-c/kernel/cioif.c's own `entry` (CIOIF.CIO's
inter-module CIO dispatch) against the real src/kernel/CIOIF.CIO,
CONSOLE.CIO, and CHARIO.CIO combined exactly the way src/kernel/CIO.A86
itself does (`include cioif.cio!include console.cio!include chario.cio`).
See cioif.c's own header comment for the register-convention mapping
this test is really checking: does `cl` route to the right target with
the right subset of dx/bh/bl, with the right field(s) of the result
coming back.

Reuses tests/src-c/test_chario.py's own `base_memory()`/CCB/PD scratch
conventions (same addresses, so the two tests' own setup stays directly
comparable) -- see that file's own header comment for why `base_memory`
pre-attaches both console 0 and list device 2.

`entry` is purely dispatch, so each of the 23 cases below just needs
ONE representative scenario per target (already covered more deeply
by test_console.py/test_chario.py's own dedicated tests) -- the thing
actually being verified here is the wiring itself, not each target's
own internal correctness.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'cioif_entry_wrapper.A86')
CIOIF_ENTRY_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in ('cioif.c', 'console.c', 'chario.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, SD_CCB, SD_THRDRT = 0xA000, 0xA030, 0xA032
SD_NCNS, SD_NLSTDEV, SD_NCONDEV = 0xA034, 0xA035, 0xA036

C_ATTACH, C_FLAG, C_STRTCOL, C_COLUMN, C_NCHAR, C_MIMIC, C_MSOURCE, CCBLEN = 0, 4, 5, 6, 7, 8, 9, 12
P_FLAG, P_CNS, P_LST, P_SCRATCH = 6, 0x20, 0x24, 0x2E

PD = 0x5000
CCB_BASE = 0x6000


def ccb_addr(dev):
    return CCB_BASE + dev * CCBLEN


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD)
    mem_w16(mem, SD_CCB, CCB_BASE)
    mem_w16(mem, SD_THRDRT, 0)
    mem[SD_NCNS] = 4
    mem[SD_NLSTDEV] = 4
    mem[SD_NCONDEV] = 2
    mem[PD + P_CNS] = 0
    mem[PD + P_LST] = 2
    mem_w16(mem, ccb_addr(0) + C_ATTACH, PD)
    mem_w16(mem, ccb_addr(2) + C_ATTACH, 0)
    return mem


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['entry'].value
    mpmif = asm.symtab['mpmif'].value
    xiosif = asm.symtab['xiosif'].value
    # `entry` is `mov ch,0!shl cx,1!mov si,cx!call cs:functab[si]!retf`
    # -- the one retf is the single 0xCB byte right after that call,
    # confirmed by direct disassembly, not guessed.
    retf_candidates = [a for a in range(entry, entry + 20) if code[a] == 0xCB]
    assert len(retf_candidates) == 1, f"expected 1 retf, found {retf_candidates}"
    return code, entry, retf_candidates[0], mpmif, xiosif


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *CIOIF_ENTRY_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, stop_addr, mpmif, xiosif, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry,
                 stop_addrs={stop_addr}, call_stub_addrs={mpmif, xiosif})
    sim.run(max_steps=20000)
    return sim.regs, image


def main():
    code, entry, stop_addr, mpmif, xiosif = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class EntryResult(ctypes.Structure):
            _fields_ = [('ax', ctypes.c_uint16), ('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.entry.restype = EntryResult
        lib.entry.argtypes = [c_u8p, ctypes.c_uint8, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # These targets only ever define BL (not BH) and never touch CX
        # at all -- same "only compare what's actually defined" choice
        # test_chario.py's own conin_entry/etc. tests already make (BH/
        # CX hold whatever garbage the internal call chain left them
        # as, not something meaningful to reproduce).
        BL_ONLY = {0, 2, 5, 8}

        def run_case(label, cl, mem, dx=0, bh=0, bl=0, check_bx_cx=True):
            ref_regs, ref_img = run_reference(code, entry, stop_addr, mpmif, xiosif, mem,
                                               {'cx': cl, 'dx': dx, 'bx': (bh << 8) | bl})
            buf = to_ctypes(mem)
            r = lib.entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(cl), ctypes.c_uint16(dx),
                           ctypes.c_uint8(bh), ctypes.c_uint8(bl))
            if check_bx_cx:
                if cl in BL_ONLY:
                    check(label, ref_regs['bx'] & 0xFF, r.bx & 0xFF)
                else:
                    check(label, (ref_regs['bx'] & 0xFFFF, ref_regs['cx'] & 0xFFFF), (r.bx, r.cx))
            return ref_img, bytes(buf)

        # 0: conin_entry -- buffered graphic char
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('Q')
        run_case("0 conin_entry", 0, mem)

        # 1: conout_entry -- column side effect (no defined return)
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 10
        ref_img, port = run_case("1 conout_entry", 1, mem, dx=ord('Z'), check_bx_cx=False)
        check("1 conout_entry: column advances", ref_img[ccb_addr(0) + C_COLUMN], port[ccb_addr(0) + C_COLUMN])

        # 2: rconin_entry -- raw, reads/clears c_nchar
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = 0x04
        run_case("2 rconin_entry", 2, mem)

        # 3: rconout_entry -- side effect only
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 5
        ref_img, port = run_case("3 rconout_entry", 3, mem, dx=ord('A'), check_bx_cx=False)
        check("3 rconout_entry: column advances", ref_img[ccb_addr(0) + C_COLUMN], port[ccb_addr(0) + C_COLUMN])

        # 4: listout_entry -- side effect only (lstattach_entry's own call)
        mem = base_memory()
        run_case("4 listout_entry", 4, mem, dx=ord('X'), check_bx_cx=False)

        # 5: dirio_entry -- conin path (dl=0xFD)
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('Q')
        run_case("5 dirio_entry", 5, mem, dx=0xFD)

        # 6: conwrite_entry -- writes a '$'-terminated buffer
        BUF = 0x7000
        mem = base_memory()
        for i, ch in enumerate(b'HI$'):
            mem[BUF + i] = ch
        ref_img, port = run_case("6 conwrite_entry", 6, mem, dx=BUF, check_bx_cx=False)
        check("6 conwrite_entry: column side effect", ref_img[ccb_addr(0) + C_COLUMN], port[ccb_addr(0) + C_COLUMN])

        # 7: conread_entry -- CR immediately ends the line
        mem = base_memory()
        mem[BUF] = 10  # max length
        mem[ccb_addr(0) + C_NCHAR] = 13  # cr
        ref_img, port = run_case("7 conread_entry", 7, mem, dx=BUF, check_bx_cx=False)
        check("7 conread_entry: length byte", ref_img[BUF + 1], port[BUF + 1])

        # 8: constat_entry
        mem = base_memory()
        run_case("8 constat_entry", 8, mem)

        # 9: conattach_entry -- free console -> attach
        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0)
        run_case("9 conattach_entry", 9, mem)

        # 10: condetach_entry
        mem = base_memory()
        run_case("10 condetach_entry", 10, mem)

        # 11: setdefcon_entry
        mem = base_memory()
        run_case("11 setdefcon_entry", 11, mem, dx=0)

        # 12: conassign_entry
        ACB = 0x7100
        mem = base_memory()
        run_case("12 conassign_entry", 12, mem, dx=ACB)

        # 13: getdefcon_entry
        mem = base_memory()
        run_case("13 getdefcon_entry", 13, mem)

        # 14: conprint_entry -- dollar-delimited buffer
        mem = base_memory()
        for i, ch in enumerate(b'HI$'):
            mem[BUF + i] = ch
        ref_img, port = run_case("14 conprint_entry", 14, mem, dx=BUF, bh=0, bl=ord('$'), check_bx_cx=False)
        check("14 conprint_entry: column side effect", ref_img[ccb_addr(0) + C_COLUMN], port[ccb_addr(0) + C_COLUMN])

        # 15: lstattach_entry
        mem = base_memory()
        run_case("15 lstattach_entry", 15, mem)

        # 16: lstdetach_entry
        mem = base_memory()
        run_case("16 lstdetach_entry", 16, mem)

        # 17: setdeflst_entry
        mem = base_memory()
        run_case("17 setdeflst_entry", 17, mem, dx=0)

        # 18: clstattch_entry
        mem = base_memory()
        run_case("18 clstattch_entry", 18, mem)

        # 19: cconattch_entry
        mem = base_memory()
        run_case("19 cconattch_entry", 19, mem)

        # 20: getdeflst_entry
        mem = base_memory()
        run_case("20 getdeflst_entry", 20, mem)

        # 21: mimic_entry -- dh=source(1), dl=mimic device(2) (encoded
        # into dx: dh=dx>>8, dl=dx&0xFF, matching cioif.c's own case 21)
        mem = base_memory()
        mem_w16(mem, ccb_addr(1) + C_ATTACH, PD)
        run_case("21 mimic_entry", 21, mem, dx=(1 << 8) | 2)

        # 22: unmimic_entry
        mem = base_memory()
        run_case("22 unmimic_entry", 22, mem, dx=0)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
