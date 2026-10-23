#!/usr/bin/env python3
"""Differential test for src-c/kernel/console.c against all of
src/kernel/CONSOLE.CIO plus src/kernel/CHARIO.CIO's getccbadr.

cioattach's real "actually must wait" path is deliberately not exercised
-- see console.c's own header comment and docs/src-c.md.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'console_wrapper.A86')
CONSOLE_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'console.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, SD_CCB, SD_THRDRT = 0xA000, 0xA030, 0xA032
SD_NCNS, SD_NLSTDEV, SD_NCONDEV = 0xA034, 0xA035, 0xA036
UDA_WRKSEG = 0xA012
MPMIF_STUB = 0x9000

C_ATTACH, C_QUEUE, C_MIMIC, C_MSOURCE, CCBLEN = 0, 2, 8, 9, 12
P_CNS, P_LST, P_STAT, P_LINK = 0x20, 0x24, 0x04, 0x00
ACB_CNS, ACB_MATCH, ACB_PD, ACB_NAME = 0, 1, 2, 4
PS_CIOWAIT = 9
E_ILL_CNS, E_ILL_LST, E_NOT_OWNER, E_NO_ATTACH, E_NO_MIMIC = 0x13, 0x25, 0x20, 0x24, 0x27
F_FINDPDNAME = 0x214

PD = 0x5000
CCB_BASE = 0x6000


def ccb_addr(dev):
    return CCB_BASE + dev * CCBLEN


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    names = ('lstattach_entry', 'clstattch_entry', 'cconattch_entry', 'conattach_entry',
             'cioattach', 'lstdetach_entry', 'condetach_entry', 'ciodetach',
             'conassign_entry', 'getdefcon_entry', 'getdeflst_entry',
             'setdefcon_entry', 'setdeflst_entry', 'mimic_entry', 'unmimic_entry')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, CONSOLE_C, '-o', lib_path],
        check=True,
    )
    return lib_path


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
    return mem


def run_reference(code, entry, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, call_stub_addrs={MPMIF_STUB})
    sim.run(max_steps=20000)
    return (sim.regs['bx'], sim.regs['cx']), image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        for name in ('lstattach_entry', 'clstattch_entry', 'cconattch_entry', 'conattach_entry',
                     'lstdetach_entry', 'condetach_entry', 'getdefcon_entry', 'getdeflst_entry'):
            getattr(lib, name).restype = BxCx
            getattr(lib, name).argtypes = [c_u8p]
        lib.cioattach.restype = BxCx
        lib.cioattach.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.ciodetach.restype = BxCx
        lib.ciodetach.argtypes = [c_u8p, ctypes.c_uint16]
        lib.setdefcon_entry.restype = BxCx
        lib.setdefcon_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.setdeflst_entry.restype = BxCx
        lib.setdeflst_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.conassign_entry.restype = BxCx
        lib.conassign_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.mimic_entry.restype = BxCx
        lib.mimic_entry.argtypes = [c_u8p, ctypes.c_uint8, ctypes.c_uint8]
        lib.unmimic_entry.restype = BxCx
        lib.unmimic_entry.argtypes = [c_u8p, ctypes.c_uint8]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- conattach_entry: free console -> attach; already-owner -> ok --
        for label, initial_attach in (("conattach_entry: free -> attach", 0), ("conattach_entry: already owner", PD)):
            mem = base_memory()
            mem_w16(mem, ccb_addr(0) + C_ATTACH, initial_attach)
            mem[PD + P_CNS] = 0
            ref_regs, ref_img = run_reference(code, entries['conattach_entry'], mem, {})
            buf = to_ctypes(mem)
            r = lib.conattach_entry(ctypes.cast(buf, c_u8p))
            port_img = bytes(buf)
            ref = (ref_regs, mem_r16(ref_img, ccb_addr(0) + C_ATTACH))
            port = ((r.bx, r.cx), mem_r16(port_img, ccb_addr(0) + C_ATTACH))
            check(label, ref, port)

        # -- cconattch_entry: busy + conditional -> e_no_attach --
        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0x9999)
        mem[PD + P_CNS] = 0
        ref_regs, _ = run_reference(code, entries['cconattch_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.cconattch_entry(ctypes.cast(buf, c_u8p))
        check("cconattch_entry: busy, conditional -> error", ref_regs, (r.bx, r.cx))

        # -- conattach_entry: mimic device, source already owns it -> ok --
        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0xFFFF)  # console 0 is a mimic of console 1
        mem[ccb_addr(0) + C_MSOURCE] = 1
        mem_w16(mem, ccb_addr(1) + C_ATTACH, PD)  # the calling PD already owns the source
        mem[PD + P_CNS] = 0
        ref_regs, _ = run_reference(code, entries['conattach_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.conattach_entry(ctypes.cast(buf, c_u8p))
        check("conattach_entry: mimic device, source already owned", ref_regs, (r.bx, r.cx))

        # -- lstattach_entry / lstdetach_entry --
        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0)
        mem[PD + P_LST] = 0
        ref_regs, ref_img = run_reference(code, entries['lstattach_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.lstattach_entry(ctypes.cast(buf, c_u8p))
        check("lstattach_entry: free -> attach", ref_regs, (r.bx, r.cx))

        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, PD)
        mem_w16(mem, ccb_addr(0) + C_QUEUE, 0x7777)
        mem[PD + P_LST] = 0
        ref_regs, ref_img = run_reference(code, entries['lstdetach_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.lstdetach_entry(ctypes.cast(buf, c_u8p))
        port_img = bytes(buf)
        ref = (ref_regs, mem_r16(ref_img, ccb_addr(0) + C_ATTACH))
        port = ((r.bx, r.cx), mem_r16(port_img, ccb_addr(0) + C_ATTACH))
        check("lstdetach_entry: owner detaches", ref, port)

        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0x9999)  # someone else owns it
        mem[PD + P_LST] = 0
        ref_regs, _ = run_reference(code, entries['condetach_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.condetach_entry(ctypes.cast(buf, c_u8p))
        check("condetach_entry: not owner -> error", ref_regs, (r.bx, r.cx))

        # -- getdefcon_entry / getdeflst_entry --
        mem = base_memory()
        mem[PD + P_CNS] = 3
        ref_regs, _ = run_reference(code, entries['getdefcon_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.getdefcon_entry(ctypes.cast(buf, c_u8p))
        check("getdefcon_entry", ref_regs, (r.bx, r.cx))

        mem = base_memory()
        mem[PD + P_LST] = 3  # p_lst (3) - ncondev (2) = 1
        ref_regs, _ = run_reference(code, entries['getdeflst_entry'], mem, {})
        buf = to_ctypes(mem)
        r = lib.getdeflst_entry(ctypes.cast(buf, c_u8p))
        check("getdeflst_entry", ref_regs, (r.bx, r.cx))

        # -- setdefcon_entry: illegal / same / different (detaches old) --
        mem = base_memory()
        ref_regs, _ = run_reference(code, entries['setdefcon_entry'], mem, {'dx': 9})
        buf = to_ctypes(mem)
        r = lib.setdefcon_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(9))
        check("setdefcon_entry: illegal console", ref_regs, (r.bx, r.cx))

        mem = base_memory()
        mem[PD + P_CNS] = 0
        mem_w16(mem, ccb_addr(0) + C_ATTACH, PD)
        ref_regs, ref_img = run_reference(code, entries['setdefcon_entry'], mem, {'dx': 1})
        buf = to_ctypes(mem)
        r = lib.setdefcon_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(1))
        port_img = bytes(buf)
        ref = (ref_regs, ref_img[PD + P_CNS], mem_r16(ref_img, ccb_addr(0) + C_ATTACH))
        port = ((r.bx, r.cx), port_img[PD + P_CNS], mem_r16(port_img, ccb_addr(0) + C_ATTACH))
        check("setdefcon_entry: switch console, detaches old", ref, port)

        # -- setdeflst_entry --
        mem = base_memory()
        ref_regs, _ = run_reference(code, entries['setdeflst_entry'], mem, {'dx': 9})
        buf = to_ctypes(mem)
        r = lib.setdeflst_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(9))
        check("setdeflst_entry: illegal list device", ref_regs, (r.bx, r.cx))

        # -- conassign_entry: illegal console / not owner / direct PD, success --
        mem = base_memory()
        ACB = 0x7000
        mem[ACB + ACB_CNS] = 9  # > ncns
        ref_regs, _ = run_reference(code, entries['conassign_entry'], mem, {'dx': ACB})
        buf = to_ctypes(mem)
        r = lib.conassign_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(ACB))
        check("conassign_entry: illegal console", ref_regs, (r.bx, r.cx))

        mem = base_memory()
        mem[ACB + ACB_CNS] = 0
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0x9999)  # owned by someone else
        ref_regs, _ = run_reference(code, entries['conassign_entry'], mem, {'dx': ACB})
        buf = to_ctypes(mem)
        r = lib.conassign_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(ACB))
        check("conassign_entry: not owner", ref_regs, (r.bx, r.cx))

        TARGET_PD = 0x5100
        mem = base_memory()
        mem[ACB + ACB_CNS] = 0
        mem[ACB + ACB_MATCH] = 0
        mem_w16(mem, ACB + ACB_PD, TARGET_PD)
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0)  # free
        mem_w16(mem, ccb_addr(0) + C_QUEUE, 0)   # no one waiting
        ref_regs, ref_img = run_reference(code, entries['conassign_entry'], mem, {'dx': ACB})
        buf = to_ctypes(mem)
        r = lib.conassign_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(ACB))
        port_img = bytes(buf)
        ref = (ref_regs, mem_r16(ref_img, ccb_addr(0) + C_ATTACH))
        port = ((r.bx, r.cx), mem_r16(port_img, ccb_addr(0) + C_ATTACH))
        check("conassign_entry: direct PD, success", ref, port)

        mem = base_memory()
        mem[ACB + ACB_CNS] = 0
        mem_w16(mem, ACB + ACB_PD, 0)  # no direct PD -> name search
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0)
        ref_regs, _ = run_reference(code, entries['conassign_entry'], mem, {'dx': ACB})
        buf = to_ctypes(mem)
        r = lib.conassign_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(ACB))
        check("conassign_entry: name search (always 'fails' under the stub)", ref_regs, (r.bx, r.cx))

        # -- mimic_entry / unmimic_entry --
        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, PD)  # caller owns the source
        mem[ccb_addr(0) + C_MIMIC] = 0xFF
        mem_w16(mem, ccb_addr(1) + C_ATTACH, 0)  # mimic device free
        mem[ccb_addr(1) + C_MIMIC] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['mimic_entry'], mem, {'dx': (0 << 8) | 1})
        buf = to_ctypes(mem)
        r = lib.mimic_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0), ctypes.c_uint8(1))
        port_img = bytes(buf)
        ref = (ref_regs, mem_r16(ref_img, ccb_addr(1) + C_ATTACH), ref_img[ccb_addr(1) + C_MSOURCE],
               ref_img[ccb_addr(0) + C_MIMIC])
        port = ((r.bx, r.cx), mem_r16(port_img, ccb_addr(1) + C_ATTACH), port_img[ccb_addr(1) + C_MSOURCE],
                port_img[ccb_addr(0) + C_MIMIC])
        check("mimic_entry: success", ref, port)

        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0x9999)  # caller doesn't own the source
        ref_regs, _ = run_reference(code, entries['mimic_entry'], mem, {'dx': (0 << 8) | 1})
        buf = to_ctypes(mem)
        r = lib.mimic_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0), ctypes.c_uint8(1))
        check("mimic_entry: not owner -> error", ref_regs, (r.bx, r.cx))

        mem = base_memory()
        mem_w16(mem, ccb_addr(1) + C_ATTACH, 0xFFFF)  # device 1 is a mimic
        mem[ccb_addr(1) + C_MSOURCE] = 0
        mem[ccb_addr(0) + C_MIMIC] = 1
        mem[PD + P_LST] = 5
        ref_regs, ref_img = run_reference(code, entries['unmimic_entry'], mem, {'dx': 1})
        buf = to_ctypes(mem)
        r = lib.unmimic_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(1))
        port_img = bytes(buf)
        ref = (ref_regs, mem_r16(ref_img, ccb_addr(1) + C_ATTACH), ref_img[PD + P_LST],
               ref_img[ccb_addr(0) + C_MIMIC])
        port = ((r.bx, r.cx), mem_r16(port_img, ccb_addr(1) + C_ATTACH), port_img[PD + P_LST],
                port_img[ccb_addr(0) + C_MIMIC])
        check("unmimic_entry: success", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
