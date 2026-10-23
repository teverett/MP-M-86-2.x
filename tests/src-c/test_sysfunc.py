#!/usr/bin/env python3
"""Differential test for src-c/kernel/sysfunc.c against all 8 entries
in src/kernel/SYSFUNC.SUP (not_implemented, bad_entry, version_entry,
mpmversion_entry, sysdat_entry, tod_entry, serial_entry,
callbios_entry).

callbios_entry's own two tail-jump paths (into mpmif(f_terminate) for
BOOT/WBOOT, into xiosif otherwise) are boundary stubs, same convention
as SYSENT.RTM's own already-ported poll_entry/delay_entry/etc: the
test stops the simulator right at mpmif's/xiosif's own call site and
compares the register state there, not a return value, for those two
paths.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'sysfunc_wrapper.A86')
SYSFUNC_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'sysfunc.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_VERNUM = 0xA00A
SD_MPMVERNUM = 0xA00C
SD_RLR = 0xA000
UDA_WRKSEG, UDA_RETSEG = 0xA012, 0xA014
SD_TOD = 0xA049
P_CNS, P_LST = 0x20, 0x24
MPMIF_STUB, XIOSIF_STUB = 0x9000, 0x9010


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value
               for name in ('not_implemented', 'bad_entry', 'version_entry', 'mpmversion_entry',
                             'sysdat_entry', 'tod_entry', 'serial_entry', 'callbios_entry')}
    return code, entries


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, SYSFUNC_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, mem_template):
    image = bytearray(mem_template)
    image[0:len(code)] = code
    sim = I86Sim(image, {'sp': STACK_TOP}, entry)
    sim.run()
    return sim.regs['bx'], sim.regs['cx']


def run_reference_full(code, entry, mem, regs, stop_addrs=()):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs))
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.not_implemented.restype = ctypes.c_uint16
        lib.not_implemented.argtypes = [ctypes.POINTER(ctypes.c_uint16)]
        lib.bad_entry.restype = ctypes.c_uint16
        lib.bad_entry.argtypes = [ctypes.POINTER(ctypes.c_uint16)]
        lib.version_entry.restype = ctypes.c_uint16
        lib.version_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint16)]
        lib.mpmversion_entry.restype = ctypes.c_uint16
        lib.mpmversion_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint16)]
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)
        lib.sysdat_entry.restype = ctypes.c_uint16
        lib.sysdat_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]
        lib.tod_entry.restype = ctypes.c_uint16
        lib.tod_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]
        lib.serial_entry.restype = ctypes.c_uint16
        lib.serial_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]

        class CallbiosResult(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16), ('reached_xios', ctypes.c_uint8),
                        ('al', ctypes.c_uint8), ('ah', ctypes.c_uint8), ('cl', ctypes.c_uint8),
                        ('dl', ctypes.c_uint8)]

        lib.callbios_entry.restype = CallbiosResult
        lib.callbios_entry.argtypes = [c_u8p, ctypes.c_uint16]

        mem_template = bytearray(0x10000)
        mem_template[SD_VERNUM] = 0x34
        mem_template[SD_VERNUM + 1] = 0x12
        mem_template[SD_MPMVERNUM] = 0x01
        mem_template[SD_MPMVERNUM + 1] = 0x01

        buf = (ctypes.c_uint8 * 0x10000)(*mem_template)
        mem_ptr = ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8))

        failures = []

        def check(label, ref_bx_cx, port_bx_cx):
            status = "ok" if ref_bx_cx == port_bx_cx else "FAIL"
            print(f"[{status}] {label}: reference bx={ref_bx_cx[0]:#06x} cx={ref_bx_cx[1]:#06x} "
                  f"| port bx={port_bx_cx[0]:#06x} cx={port_bx_cx[1]:#06x}")
            if status == "FAIL":
                failures.append(label)

        ref = run_reference(code, entries['not_implemented'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.not_implemented(ctypes.byref(cx))
        check("not_implemented", ref, (bx, cx.value))

        ref = run_reference(code, entries['bad_entry'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.bad_entry(ctypes.byref(cx))
        check("bad_entry", ref, (bx, cx.value))

        ref = run_reference(code, entries['version_entry'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.version_entry(mem_ptr, ctypes.byref(cx))
        check("version_entry", ref, (bx, cx.value))

        ref = run_reference(code, entries['mpmversion_entry'], mem_template)
        cx = ctypes.c_uint16()
        bx = lib.mpmversion_entry(mem_ptr, ctypes.byref(cx))
        check("mpmversion_entry", ref, (bx, cx.value))

        def check_bxcx(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- sysdat_entry: u_retseg <- ds --
        mem = bytearray(0x10000)
        ref_regs, ref_img = run_reference_full(code, entries['sysdat_entry'], mem, {'ds': 0x1234})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        bx = lib.sysdat_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x1234), ctypes.byref(cx))
        check_bxcx("sysdat_entry", (ref_regs['bx'], ref_regs['cx'], mem_r16(ref_img, UDA_RETSEG)),
                   (bx, cx.value, mem_r16(bytes(buf), UDA_RETSEG)))

        # -- tod_entry: copies the system's own tod struct --
        mem = bytearray(0x10000)
        mem_w16(mem, SD_TOD, 0x1234)  # tod_day
        mem[SD_TOD + 2] = 13  # tod_hr
        mem[SD_TOD + 3] = 45  # tod_min
        mem[SD_TOD + 4] = 59  # tod_sec
        mem_w16(mem, UDA_WRKSEG, 0)
        BUF_ADDR = 0x5000
        ref_regs, ref_img = run_reference_full(code, entries['tod_entry'], mem, {'dx': BUF_ADDR})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        bx = lib.tod_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUF_ADDR), ctypes.byref(cx))
        check_bxcx("tod_entry", (ref_regs['bx'], ref_regs['cx'], bytes(ref_img[BUF_ADDR:BUF_ADDR + 5])),
                   (bx, cx.value, bytes(buf)[BUF_ADDR:BUF_ADDR + 5]))

        # -- serial_entry: copies RTMIF.RTM's own fixed '654321' --
        mem = bytearray(0x10000)
        mem_w16(mem, UDA_WRKSEG, 0)
        ref_regs, ref_img = run_reference_full(code, entries['serial_entry'], mem, {'dx': BUF_ADDR})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        bx = lib.serial_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUF_ADDR), ctypes.byref(cx))
        check_bxcx("serial_entry", (ref_regs['bx'], ref_regs['cx'], bytes(ref_img[BUF_ADDR:BUF_ADDR + 6])),
                   (bx, cx.value, bytes(buf)[BUF_ADDR:BUF_ADDR + 6]))

        # -- callbios_entry: al outside {0..7,15} -> immediate bx=cx=0xFFFF --
        BLOCK_ADDR = 0x5100
        for al in (8, 16, 200):
            mem = bytearray(0x10000)
            mem_w16(mem, UDA_WRKSEG, 0)
            mem[BLOCK_ADDR] = al
            ref_regs, _ = run_reference_full(code, entries['callbios_entry'], mem, {'dx': BLOCK_ADDR})
            buf = to_ctypes(mem)
            got = lib.callbios_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BLOCK_ADDR))
            check_bxcx(f"callbios_entry: al={al} -> immediate error",
                       (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))

        # -- callbios_entry: al<=1 (BOOT/WBOOT) -> tail-jumps into mpmif(f_terminate) --
        for al in (0, 1):
            mem = bytearray(0x10000)
            mem_w16(mem, UDA_WRKSEG, 0)
            mem[BLOCK_ADDR] = al
            ref_regs, _ = run_reference_full(code, entries['callbios_entry'], mem, {'dx': BLOCK_ADDR},
                                              stop_addrs={MPMIF_STUB})
            buf = to_ctypes(mem)
            got = lib.callbios_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BLOCK_ADDR))
            check_bxcx(f"callbios_entry: al={al} -> mpmif(f_terminate) tail", True, got.reached_xios == 1)

        def callbios_case(label, al, p_cns_val=None, p_lst_val=None, block_cl=0x77):
            mem = bytearray(0x10000)
            mem_w16(mem, UDA_WRKSEG, 0)
            mem[BLOCK_ADDR] = al
            mem[BLOCK_ADDR + 1] = block_cl  # the block's own 2nd field, low byte
            PD_ADDR = 0x6000
            mem_w16(mem, SD_RLR, PD_ADDR)
            if p_cns_val is not None:
                mem[PD_ADDR + P_CNS] = p_cns_val
            if p_lst_val is not None:
                mem[PD_ADDR + P_LST] = p_lst_val
            ref_regs, _ = run_reference_full(code, entries['callbios_entry'], mem, {'dx': BLOCK_ADDR},
                                              stop_addrs={XIOSIF_STUB})
            buf = to_ctypes(mem)
            got = lib.callbios_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BLOCK_ADDR))
            check_bxcx(label,
                       (ref_regs['ax'] & 0xFF, (ref_regs['ax'] >> 8) & 0xFF, ref_regs['cx'] & 0xFF, ref_regs['dx'] & 0xFF),
                       (got.al, got.ah, got.cl, got.dl))

        # al in {2,3} (console in/aux) -> xiosif tail, cl<-p_cns
        for al in (2, 3):
            callbios_case(f"callbios_entry: al={al} -> xiosif tail (cl<-p_cns)", al, p_cns_val=9)
        # al==4 (console output) -> xiosif tail, cl stays the block's own byte
        callbios_case("callbios_entry: al=4 -> xiosif tail (cl<-block byte 1)", 4, p_cns_val=9)
        # al in {5,6,7} (list devices) -> xiosif tail, dl<-p_lst, cl stays the block's own byte
        callbios_case("callbios_entry: al=6 -> xiosif tail (dl<-p_lst, cl<-block byte 1)", 6, p_lst_val=3)
        # al==15 (list status) -> xiosif tail, cl<-p_lst
        callbios_case("callbios_entry: al=15 -> xiosif tail (cl<-p_lst)", 15, p_lst_val=3)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
