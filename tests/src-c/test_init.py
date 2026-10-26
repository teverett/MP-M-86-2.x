#!/usr/bin/env python3
"""Differential test for src-c/kernel/init.c (`sup_init`) against
src/kernel/INIT.SUP's `init`, both running against real memory layout.

sup_init addresses real memory (defs.h's far_ptr(): seg*16 + off at
real SYSDAT/IVT/SUP-cseg offsets), so the reference runs with
tools/i86sim.py's segmented=True and a real, nonzero SYSDAT segment,
and tests/src-c/fixtures/init_wrapper.A86 includes the real
SYSDAT.FMT/VEC.FMT/UDA.FMT. Every field address below comes from the
assembled symbol table, not a hand-copied constant, so a wrong
SYSDAT_* offset in defs.h shows up as a mismatch at the real address.

CS is 0: i86sim fetches instructions from the physical IP and ignores
CS, so the code image has to sit at physical 0 for its own `cs:`
accesses (cs:sysdat etc.) to land on it. The IVT at 380h is clear of
the 17Dh-byte code image.

Beyond the per-field checks, the two images are diffed byte for byte
(outside the code image and the reference's own stack pushes) to
catch a stray write on either side.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'init_wrapper.A86')
INIT_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'init.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000     # i86sim's segmented mode wraps at 64KB
PHYS_MEM = 0x100000   # sup_init's view: the real 1MB 8086 address space

CS = 0x0000
DS_SYSDAT = 0x0400    # SYSDAT at physical 4000h-4B60h
RSP_SEGS = (0x0600, 0x0700)
NOTICK_OFS, IDLE_OFS, USER_ENTRY_OFS = 0x0179, 0x017A, 0x017B
SUP_SEG = 0x1000      # module_table[SUP].entry segment half, copied to supervisor+2
XIOS_INIT_SEG = 0xBEEF  # sentinel at SYSDAT 2Eh -- the bare `mov u_wrkseg,ds` bug's target


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in (
        'init', 'rsp_out', 'xiosif', 'sysdat', 'supervisor', 'ver', 'version',
        'tick_tos', 'idle_tos', 'intflag', 'ncondev', 'nciodev', 'nlstdev',
        'rspseg', 'initpd', 'inituda', 'init_tos', 'supmod', 'i_mpm_ip', 'i_mpm_cs',
        'p_uda', 'u_wrkseg', 'rsp_link')}
    # the 5 `callf dword ptr <...>mod[bx]` -- target only known at
    # run time, so stubbed by instruction address (0xFF /3, CALL FAR r/m)
    far_calls = [i for i in range(sym['init'], len(code) - 1)
                 if code[i] == 0xFF and ((code[i + 1] >> 3) & 7) == 3]
    assert len(far_calls) == 5, f"expected 5 callf sites, found {far_calls}"
    stop_addr = sym['rsp_out'] + 6  # past the final int mpmint
    return code, sym, far_calls, stop_addr


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, INIT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def phys(seg, off):
    return seg * 16 + off


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def initial_memory(sym, rsp_segs):
    mem = bytearray(SIM_MEM)
    w16(mem, phys(DS_SYSDAT, sym['supmod'] + 2), SUP_SEG)
    w16(mem, phys(DS_SYSDAT, 0x2E), XIOS_INIT_SEG)
    w16(mem, phys(DS_SYSDAT, sym['rspseg']), rsp_segs[0] if rsp_segs else 0)
    for seg, nxt in zip(rsp_segs, list(rsp_segs[1:]) + [0]):
        w16(mem, phys(seg, sym['rsp_link']), nxt)
    return mem


def run_reference(code, sym, far_calls, stop_addr, mem):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(
        image, {'cs': CS, 'ds': DS_SYSDAT, 'ss': DS_SYSDAT, 'sp': 0xFFF0}, sym['init'],
        stop_addrs={stop_addr},
        call_stub_addrs={sym['xiosif']},
        int_stub_nums={224},  # mpmint
        far_call_stub_addrs=set(far_calls),
        segmented=True,
    )
    sim.run()
    return image


def run_port(lib, code, mem):
    buf = (ctypes.c_uint8 * PHYS_MEM)()
    ctypes.memmove(buf, bytes(mem), len(mem))
    ctypes.memmove(buf, bytes(code), len(code))
    lib.sup_init(buf, ctypes.c_uint16(DS_SYSDAT), ctypes.c_uint16(CS),
                 ctypes.c_uint16(NOTICK_OFS), ctypes.c_uint16(IDLE_OFS),
                 ctypes.c_uint16(USER_ENTRY_OFS))
    return bytes(buf)


def main():
    code, sym, far_calls, stop_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.sup_init.argtypes = [ctypes.POINTER(ctypes.c_uint8)] + [ctypes.c_uint16] * 5
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        for case, rsp_segs in (("no RSPs", ()), ("two-RSP chain", RSP_SEGS)):
            mem = initial_memory(sym, rsp_segs)
            ref = run_reference(code, sym, far_calls, stop_addr, mem)
            port = run_port(lib, code, mem)

            def both16(addr):
                return r16(ref, addr), r16(port, addr)

            sd = lambda off: phys(DS_SYSDAT, off)  # noqa: E731
            uda_seg = (sym['inituda'] >> 4) + DS_SYSDAT

            check(f"{case}: cs:sysdat", *both16(phys(CS, sym['sysdat'])))
            check(f"{case}: cs:supervisor segment", *both16(phys(CS, sym['supervisor'] + 2)))
            check(f"{case}: version", *both16(sd(sym['version'])))
            check(f"{case}: tick_tos", *both16(sd(sym['tick_tos'])))
            check(f"{case}: idle_tos", *both16(sd(sym['idle_tos'])))
            check(f"{case}: i_mpm_ip", *both16(phys(0, sym['i_mpm_ip'])))
            check(f"{case}: i_mpm_cs", *both16(phys(0, sym['i_mpm_cs'])))
            check(f"{case}: initpd.p_uda", *both16(sd(sym['initpd'] + sym['p_uda'])))
            check(f"{case}: p_uda is inituda's own paragraph", uda_seg, r16(port, sd(sym['initpd'] + sym['p_uda'])))
            check(f"{case}: inituda.u_wrkseg", *both16(phys(uda_seg, sym['u_wrkseg'])))
            check(f"{case}: SYSDAT 2Eh untouched (u_wrkseg bug fix)", XIOS_INIT_SEG, r16(port, sd(0x2E)))
            check(f"{case}: intflag", ref[sd(sym['intflag'])], port[sd(sym['intflag'])])
            check(f"{case}: ncondev", ref[sd(sym['ncondev'])], port[sd(sym['ncondev'])])
            check(f"{case}: nciodev", ref[sd(sym['nciodev'])], port[sd(sym['nciodev'])])
            check(f"{case}: nlstdev", ref[sd(sym['nlstdev'])], port[sd(sym['nlstdev'])])
            check(f"{case}: rspseg (list consumed)", *both16(sd(sym['rspseg'])))
            for seg in rsp_segs:
                check(f"{case}: RSP {seg:04X}h rsp_link = sysdat", *both16(phys(seg, sym['rsp_link'])))

            # whole-image diff, outside the code image, the reference's
            # own stack pushes just below init_tos, and the return
            # sentinel I86Sim pushes onto the initial stack at entry
            stack = set(range(sd(sym['init_tos']) - 0x10, sd(sym['init_tos'])))
            entry_sp = (DS_SYSDAT * 16 + 0xFFF0 - 2) & 0xFFFF
            stack |= {entry_sp, entry_sp + 1}
            diffs = [hex(a) for a in range(len(code), SIM_MEM)
                     if a not in stack and ref[a] != port[a]]
            check(f"{case}: no other bytes differ", [], diffs[:8])
            check(f"{case}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
