#!/usr/bin/env python3
"""Differential test for src-c/kernel/share.c against src/kernel/
SHARE.MEM's share_entry.

Needs tools/i86sim.py's opt-in `segmented=True` mode (see src-c/
kernel/ml.c's own header comment): the SAT scan near the end switches
DS to the underlying MAU's own dynamically-supplied segment.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'share_wrapper.A86')
SHARE_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'share.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'util_mem.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, UDA_WRKSEG, SD_MDUL = 0xA000, 0xA012, 0xA028

P_MEM = 0x16
MS_LINK, MS_START, MS_LENGTH, MS_FLAGS, MS_MAU = 0, 2, 4, 6, 8
M_LINK, M_START = 0, 2
SPB_OPD, SPB_RPD, SPB_START = 0, 2, 4
SAT_START, SAT_NALL, SATLEN = 0, 4, 5
E_NO_MEMORY = 3

WRKSEG = 0x0040
SPB_ADDR = 0x0010  # offset within WRKSEG's own segment
OWNER_PD, RPD, FREE_MD = 0x5000, 0x5100, 0x5200
OWNER_MS, MAU_MD = 0x5300, 0x5400
MAU_SEG = 0x0100


def phys(seg):
    return (seg * 16) & 0xFFFF


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    addr &= 0xFFFF
    return mem[addr] | (mem[(addr + 1) & 0xFFFF] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['share_entry'].value
    return code, entry


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *SHARE_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, WRKSEG)
    mem_w16(mem, SD_MDUL, FREE_MD)
    mem_w16(mem, FREE_MD + M_LINK, 0)
    return mem


def put_spb(mem, opd, rpd, start):
    base = phys(WRKSEG) + SPB_ADDR
    mem_w16(mem, base + SPB_OPD, opd)
    mem_w16(mem, base + SPB_RPD, rpd)
    mem_w16(mem, base + SPB_START, start)


def put_owner_ms(mem, pd, ms_addr, start, length, flags, mau):
    mem_w16(mem, pd + P_MEM, ms_addr)
    mem_w16(mem, ms_addr + MS_LINK, 0)
    mem_w16(mem, ms_addr + MS_START, start)
    mem_w16(mem, ms_addr + MS_LENGTH, length)
    mem_w16(mem, ms_addr + MS_FLAGS, flags)
    mem_w16(mem, ms_addr + MS_MAU, mau)


def put_sat(mem, seg, entry_no, start, nall, ents=4):
    base = phys(seg)
    mem[base] = ents
    off = base + entry_no * SATLEN
    mem_w16(mem, off + SAT_START, start)
    mem[off + SAT_NALL] = nall


def run_reference(code, entry, mem, regs):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, segmented=True)
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entry = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.share_entry.restype = BxCx
        lib.share_entry.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def snapshot(mem):
            return (
                mem_r16(mem, RPD + P_MEM),
                mem_r16(mem, FREE_MD + MS_START), mem_r16(mem, FREE_MD + MS_LENGTH),
                mem_r16(mem, FREE_MD + MS_FLAGS), mem_r16(mem, FREE_MD + MS_MAU),
                mem_r16(mem, FREE_MD + MS_LINK),
                mem[phys(MAU_SEG) + SATLEN + SAT_NALL],
                mem_r16(mem, SD_MDUL),
            )

        # -- share_entry: basic share, requestor's p_mem list empty --
        mem = base_memory()
        put_spb(mem, OWNER_PD, RPD, 0x0300)
        put_owner_ms(mem, OWNER_PD, OWNER_MS, 0x0300, 20, 0x02, MAU_MD)
        mem_w16(mem, RPD + P_MEM, 0)
        mem_w16(mem, MAU_MD + M_START, MAU_SEG)
        put_sat(mem, MAU_SEG, 1, 0x0300, 1)
        ref_regs, ref_img = run_reference(code, entry, mem, {'dx': SPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.share_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SPB_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx']) + snapshot(ref_img)
        port = (got.bx, got.cx) + snapshot(port_img)
        check("share_entry: basic share onto an empty p_mem list", ref, port)

        # -- share_entry: requestor already has an entry for this MAU --
        mem = base_memory()
        put_spb(mem, OWNER_PD, RPD, 0x0300)
        put_owner_ms(mem, OWNER_PD, OWNER_MS, 0x0300, 20, 0x02, MAU_MD)
        EXISTING_MS = 0x5500
        mem_w16(mem, RPD + P_MEM, EXISTING_MS)
        mem_w16(mem, EXISTING_MS + MS_LINK, 0)
        mem_w16(mem, EXISTING_MS + MS_MAU, MAU_MD)
        mem_w16(mem, MAU_MD + M_START, MAU_SEG)
        put_sat(mem, MAU_SEG, 1, 0x0300, 1)
        ref_regs, ref_img = run_reference(code, entry, mem, {'dx': SPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.share_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SPB_ADDR))
        port_img = bytes(buf)

        def snap2(m):
            return snapshot(m) + (mem_r16(m, EXISTING_MS + MS_LINK),)

        ref = (ref_regs['bx'], ref_regs['cx']) + snap2(ref_img)
        port = (got.bx, got.cx) + snap2(port_img)
        check("share_entry: inserted next to an existing same-MAU entry", ref, port)

        # -- share_entry: owner/requestor default to rlr, and sharing with
        # self is a trivial no-op success --
        mem = base_memory()
        put_spb(mem, 0, 0, 0x0300)
        mem_w16(mem, SD_RLR, OWNER_PD)
        ref_regs, ref_img = run_reference(code, entry, mem, {'dx': SPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.share_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SPB_ADDR))
        check("share_entry: owner==requestor (both default to rlr) -> no-op success",
              (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))

        # -- share_entry: owner has no MS at the requested start -> e_no_memory --
        mem = base_memory()
        put_spb(mem, OWNER_PD, RPD, 0x0300)
        put_owner_ms(mem, OWNER_PD, OWNER_MS, 0x0999, 20, 0, MAU_MD)
        ref_regs, ref_img = run_reference(code, entry, mem, {'dx': SPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.share_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SPB_ADDR))
        check("share_entry: no matching owner MS -> e_no_memory",
              (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))

        # -- share_entry: owner MS found, but MAU's own SAT has no matching
        # entry -> e_no_memory, and the freshly-gotten MD is freed back --
        mem = base_memory()
        put_spb(mem, OWNER_PD, RPD, 0x0300)
        put_owner_ms(mem, OWNER_PD, OWNER_MS, 0x0300, 20, 0, MAU_MD)
        mem_w16(mem, MAU_MD + M_START, MAU_SEG)
        put_sat(mem, MAU_SEG, 1, 0x0999, 0)  # no entry starts at 0x0300
        ref_regs, ref_img = run_reference(code, entry, mem, {'dx': SPB_ADDR})
        buf = to_ctypes(mem)
        got = lib.share_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SPB_ADDR))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], mem_r16(ref_img, SD_MDUL))
        port = (got.bx, got.cx, mem_r16(port_img, SD_MDUL))
        check("share_entry: no matching SAT entry -> e_no_memory, MD freed", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
