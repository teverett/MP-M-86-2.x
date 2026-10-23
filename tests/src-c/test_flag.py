#!/usr/bin/env python3
"""Differential test for src-c/kernel/flag.c against src/kernel/FLAG.RTM's
flag_set_entry and flag_wait_entry (and, through them, their shared
internal flag_valid/flag_exit/flag_bexit)."""
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

STACK_TOP = 0xFFF0
SD_RLR, SD_DRL, SD_NFLAGS, SD_FLAGS, UDA_DPARAM = 0xA000, 0xA002, 0xA006, 0xA008, 0xA010
FLAG_TABLE = 0x2000   # flglen (3) bytes per entry: word flg_pd, byte flg_ignore
FLG_PD, FLG_IGNORE, FLGLEN = 0x00, 0x02, 3
FLAG_OFF, FLAG_ON, FLAG_ZIG = 0xFFFF, 0xFFFE, 0xFF
PD_ADDR = 0x1000
P_LINK, P_STAT = 0x00, 0x04
DSPTCH_STUB = 0x9000
NFLAGS = 4


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in ('flag_set_entry', 'flag_wait_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, FLAG_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory(flag_entry_value=FLAG_OFF, flag_ignore=FLAG_ZIG):
    mem = bytearray(0x10000)
    mem[SD_NFLAGS] = NFLAGS
    mem_w16(mem, SD_FLAGS, FLAG_TABLE)
    for i in range(NFLAGS):
        mem_w16(mem, FLAG_TABLE + i * FLGLEN + FLG_PD, flag_entry_value)
        mem[FLAG_TABLE + i * FLGLEN + FLG_IGNORE] = flag_ignore
    mem_w16(mem, SD_RLR, PD_ADDR)
    mem_w16(mem, SD_DRL, 0x9999)
    return mem


def run_reference(code, entry, mem, dl):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {'dx': dl, 'sp': STACK_TOP}, entry, call_stub_addrs={DSPTCH_STUB})
    sim.run()
    return (sim.regs['bx'], sim.regs['cx']), image


def run_port(lib, fn_name, mem, dl):
    fn = getattr(lib, fn_name)

    class BxCx(ctypes.Structure):
        _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

    fn.restype = BxCx
    fn.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    r = fn(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), ctypes.c_uint8(dl))
    return (r.bx, r.cx), bytes(buf)


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # -- flag_set_entry --

        # invalid flag number -> e_ill_flag
        mem = base_memory()
        (ref_regs, _) = run_reference(code, entries['flag_set_entry'], mem, NFLAGS)
        (port_regs, _) = run_port(lib, 'flag_set_entry', mem, NFLAGS)
        check("flag_set_entry: invalid flag number", ref_regs, port_regs)

        # flag already on -> e_flag_ovrrun
        mem = base_memory(flag_entry_value=FLAG_ON)
        (ref_regs, _) = run_reference(code, entries['flag_set_entry'], mem, 1)
        (port_regs, _) = run_port(lib, 'flag_set_entry', mem, 1)
        check("flag_set_entry: already on", ref_regs, port_regs)

        # flag off -> turns on
        mem = base_memory(flag_entry_value=FLAG_OFF)
        (ref_regs, ref_img) = run_reference(code, entries['flag_set_entry'], mem, 1)
        (port_regs, port_img) = run_port(lib, 'flag_set_entry', mem, 1)
        ref = (ref_regs, mem_r16(ref_img, FLAG_TABLE + 1 * FLGLEN + FLG_PD))
        port = (port_regs, mem_r16(port_img, FLAG_TABLE + 1 * FLGLEN + FLG_PD))
        check("flag_set_entry: off -> on", ref, port)

        # a PD is waiting on this flag -> wake it, flag table entry -> off
        mem = base_memory(flag_entry_value=PD_ADDR)
        mem[PD_ADDR + P_LINK] = 0
        mem[PD_ADDR + P_LINK + 1] = 0
        mem[PD_ADDR + P_STAT] = 0xEE
        (ref_regs, ref_img) = run_reference(code, entries['flag_set_entry'], mem, 2)
        (port_regs, port_img) = run_port(lib, 'flag_set_entry', mem, 2)
        ref = (ref_regs, mem_r16(ref_img, FLAG_TABLE + 2 * FLGLEN + FLG_PD),
               ref_img[PD_ADDR + P_STAT], mem_r16(ref_img, SD_DRL))
        port = (port_regs, mem_r16(port_img, FLAG_TABLE + 2 * FLGLEN + FLG_PD),
                port_img[PD_ADDR + P_STAT], mem_r16(port_img, SD_DRL))
        check("flag_set_entry: wakes a waiting PD", ref, port)

        # ignore count nonzero -> just decrements, flag entry untouched
        mem = base_memory(flag_entry_value=FLAG_ON, flag_ignore=5)
        (ref_regs, ref_img) = run_reference(code, entries['flag_set_entry'], mem, 3)
        (port_regs, port_img) = run_port(lib, 'flag_set_entry', mem, 3)
        ref = (ref_regs, ref_img[FLAG_TABLE + 3 * FLGLEN + FLG_IGNORE])
        port = (port_regs, port_img[FLAG_TABLE + 3 * FLGLEN + FLG_IGNORE])
        check("flag_set_entry: ignore count decrements", ref, port)

        # -- flag_wait_entry --

        # invalid flag number -> e_ill_flag
        mem = base_memory()
        (ref_regs, _) = run_reference(code, entries['flag_wait_entry'], mem, NFLAGS)
        (port_regs, _) = run_port(lib, 'flag_wait_entry', mem, NFLAGS)
        check("flag_wait_entry: invalid flag number", ref_regs, port_regs)

        # flag already on -> consume it, flag table entry -> off
        mem = base_memory(flag_entry_value=FLAG_ON)
        (ref_regs, ref_img) = run_reference(code, entries['flag_wait_entry'], mem, 1)
        (port_regs, port_img) = run_port(lib, 'flag_wait_entry', mem, 1)
        ref = (ref_regs, mem_r16(ref_img, FLAG_TABLE + 1 * FLGLEN + FLG_PD))
        port = (port_regs, mem_r16(port_img, FLAG_TABLE + 1 * FLGLEN + FLG_PD))
        check("flag_wait_entry: on -> consumed", ref, port)

        # flag off -> this process blocks (p_stat=ps_flagwait, u_dparam=si)
        mem = base_memory(flag_entry_value=FLAG_OFF)
        (ref_regs, ref_img) = run_reference(code, entries['flag_wait_entry'], mem, 2)
        (port_regs, port_img) = run_port(lib, 'flag_wait_entry', mem, 2)
        ref = (ref_regs, ref_img[PD_ADDR + P_STAT], mem_r16(ref_img, UDA_DPARAM))
        port = (port_regs, port_img[PD_ADDR + P_STAT], mem_r16(port_img, UDA_DPARAM))
        check("flag_wait_entry: off -> this process waits", ref, port)

        # someone else already waiting -> e_flag_underrun
        mem = base_memory(flag_entry_value=PD_ADDR)
        (ref_regs, _) = run_reference(code, entries['flag_wait_entry'], mem, 3)
        (port_regs, _) = run_port(lib, 'flag_wait_entry', mem, 3)
        check("flag_wait_entry: already a waiter -> underrun", ref_regs, port_regs)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


if __name__ == '__main__':
    sys.exit(main())
