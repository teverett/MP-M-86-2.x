#!/usr/bin/env python3
"""Differential test for src-c/kernel/proc.c against src/kernel/
PROC.RTM's freepd/proc_creat (+ internal getpdadr).

Needs tools/i86sim.py's opt-in `segmented=True` mode (see src-c/
kernel/ml.c's own header comment) -- proc_creat dynamically switches
DS/ES to a brand-new child process's own UDA segment and, later, to
that same child's own user-stack segment. It also needs the real
assembled code shifted away from absolute address 0 (CSEG_BASE, same
technique as tests/src-c/test_tmp.py's own docstring explains) since
proc_creat genuinely reads the real, absolute interrupt vector table
at segment 0 -- unlike every other file ported so far, which only
ever read scratch/SYSDAT-relative fields that could be placed
anywhere. Segment 0 can't be relocated, so the routine's own code has
to move instead, and `cs:sysdat`'s own segmented-mode effective
address has to be corrected for that shift by setting the `cs`
register explicitly (see base_regs() below).
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'proc_wrapper.A86')
PROC_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'proc.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
CSEG_BASE = 0x0800
CS_REG = CSEG_BASE >> 4
# `mpmif equ 9000h` in the wrapper is resolved at *assemble* time assuming
# cseg loads at absolute 0; since this wrapper's cseg is actually loaded at
# CSEG_BASE (see run_reference's own comment), the CALL's encoded rel16
# displacement lands on 9000h+CSEG_BASE at runtime, not bare 9000h.
MPMIF_STUB = 0x9000 + CSEG_BASE

SD_RLR, SD_DRL, UDA_WRKSEG, SD_ENDSEG = 0xA000, 0xA002, 0xA012, 0xA026
SD_THRDRT, SD_NLSTDEV, SD_NCONDEV, SD_PUL = 0xA032, 0xA035, 0xA036, 0xA038

P_LINK, P_THREAD, P_FLAG, P_UDA, P_MEM, P_LST, P_PARENT = 0, 2, 6, 0x10, 0x16, 0x24, 0x1E
PF_TABLE, PF_RESOURCE, PF_CHILDABORT = 0x10, 0x20, 0x800
PDLEN = 0x30
E_NO_PD = 0x0C
MS_LINK = 0

U_DPARAM, U_DMA_SEG, U_ERROR_MODE, U_MULT_CNT = 0, 4, 0x10, 0x11
U_DF_PASSWORD, U_PD_CNT, U_IN_INT, U_SP, U_SS = 0x12, 0x1A, 0x1B, 0x1C, 0x1E
U_DS_SAV, U_STACK_SP, U_IVECTORS, U_ES_SAV, U_FLAG_SAV = 0x32, 0x34, 0x38, 0x4C, 0x4E
U_INITCS, U_INITDS, U_INITES, U_INITSS, U_MPM_IP, U_INSYS = 0x50, 0x52, 0x54, 0x56, 0x58, 0x60


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
    entries = {name: asm.symtab[name].value for name in ('freepd', 'proc_creat')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *PROC_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry_off, mem, regs, sysdat_word):
    image = bytearray(0x10000)
    image[CSEG_BASE:CSEG_BASE + len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    image[CSEG_BASE] = sysdat_word & 0xFF       # `sysdat dw 0` sits at cseg offset 0
    image[CSEG_BASE + 1] = (sysdat_word >> 8) & 0xFF
    entry = CSEG_BASE + entry_off
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP, 'cs': CS_REG}, entry,
                 call_stub_addrs={MPMIF_STUB}, segmented=True)
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.freepd.argtypes = [c_u8p, ctypes.c_uint16]
        lib.proc_creat.restype = ctypes.c_uint16
        lib.proc_creat.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # ================= freepd =================

        # -- freepd: pf_table NOT set -> just unlinked from the thread list --
        mem = bytearray(0x10000)
        PD_X, PD_Y = 0x5000, 0x5100
        mem_w16(mem, SD_THRDRT, PD_X)
        mem_w16(mem, PD_X + P_THREAD, PD_Y)
        mem_w16(mem, PD_Y + P_THREAD, 0)
        mem_w16(mem, PD_Y + P_FLAG, 0)
        mem_w16(mem, SD_PUL, 0)
        ref_regs, ref_img = run_reference(code, entries['freepd'], mem, {'si': PD_Y}, 0x1000)
        ref = (mem_r16(ref_img, SD_THRDRT), mem_r16(ref_img, PD_X + P_THREAD), mem_r16(ref_img, SD_PUL))
        buf = to_ctypes(mem)
        lib.freepd(ctypes.cast(buf, c_u8p), ctypes.c_uint16(PD_Y))
        port_img = bytes(buf)
        port = (mem_r16(port_img, SD_THRDRT), mem_r16(port_img, PD_X + P_THREAD), mem_r16(port_img, SD_PUL))
        check("freepd: pf_table not set -> unlinked only", ref, port)

        # -- freepd: pf_table set -> also pushed onto pul --
        mem = bytearray(0x10000)
        mem_w16(mem, SD_THRDRT, PD_X)
        mem_w16(mem, PD_X + P_THREAD, PD_Y)
        mem_w16(mem, PD_Y + P_THREAD, 0)
        mem_w16(mem, PD_Y + P_FLAG, PF_TABLE)
        mem_w16(mem, SD_PUL, 0x4000)
        ref_regs, ref_img = run_reference(code, entries['freepd'], mem, {'si': PD_Y}, 0x1000)
        ref = (mem_r16(ref_img, SD_THRDRT), mem_r16(ref_img, PD_X + P_THREAD),
               mem_r16(ref_img, SD_PUL), mem_r16(ref_img, PD_Y + P_LINK))
        buf = to_ctypes(mem)
        lib.freepd(ctypes.cast(buf, c_u8p), ctypes.c_uint16(PD_Y))
        port_img = bytes(buf)
        port = (mem_r16(port_img, SD_THRDRT), mem_r16(port_img, PD_X + P_THREAD),
                mem_r16(port_img, SD_PUL), mem_r16(port_img, PD_Y + P_LINK))
        check("freepd: pf_table set -> also pushed onto pul", ref, port)

        # ================= proc_creat =================

        # -- proc_creat: dx == 0 -> nothing to do --
        mem = bytearray(0x10000)
        ref_regs, ref_img = run_reference(code, entries['proc_creat'], mem, {'dx': 0}, 0x1000)
        buf = to_ctypes(mem)
        bx = lib.proc_creat(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(0x1000), None)
        check("proc_creat: dx==0", ref_regs['bx'], bx)

        # -- proc_creat: getpdadr fails (pul exhausted) -> bx=0xFFFF --
        mem = bytearray(0x10000)
        SYSDAT = 0x3100
        mem_w16(mem, UDA_WRKSEG, 0x0100)  # far below sysdat -> out of range
        mem_w16(mem, SD_ENDSEG, 0x7000)
        mem_w16(mem, SD_PUL, 0)
        ref_regs, ref_img = run_reference(code, entries['proc_creat'], mem, {'dx': 0x5100}, SYSDAT)
        buf = to_ctypes(mem)
        bx = lib.proc_creat(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x5100), ctypes.c_uint16(SYSDAT), None)
        check("proc_creat: getpdadr fails -> 0xFFFF", ref_regs['bx'], bx)

        # -- proc_creat: single, full success --
        def build_success_mem():
            mem = bytearray(0x10000)
            SYSDAT = WRKSEG = 0x3100
            mem_w16(mem, UDA_WRKSEG, WRKSEG)
            mem_w16(mem, SD_ENDSEG, 0x7000)
            mem_w16(mem, SD_NLSTDEV, 4)
            mem_w16(mem, SD_NCONDEV, 2)
            mem[SD_NLSTDEV] = 4
            mem[SD_NCONDEV] = 2

            PARENT_PD = 0x5000
            CHILD_PD = 0x5100  # dx -- wrkseg==sysdat so getpdadr's si == dx
            UDA_SEG = 0x3300
            PARENT_UDA_SEG = 0x3600
            MS1 = 0x5400

            mem_w16(mem, SD_RLR, PARENT_PD)
            mem_w16(mem, PARENT_PD + P_FLAG, PF_CHILDABORT)
            mem_w16(mem, PARENT_PD + P_UDA, PARENT_UDA_SEG)
            mem_w16(mem, PARENT_PD + P_MEM, MS1)  # exercises the share loop
            mem_w16(mem, MS1 + MS_LINK, 0)

            mem_w16(mem, CHILD_PD + P_LINK, 0)  # end of the creation chain
            mem_w16(mem, CHILD_PD + P_UDA, (UDA_SEG - WRKSEG) & 0xFFFF)  # relative, per real asm
            mem[CHILD_PD + P_LST] = 1  # < nlstdev -> kept, then + ncondev
            mem_w16(mem, CHILD_PD + P_MEM, 0)

            for i in range(8):
                mem[phys(PARENT_UDA_SEG) + U_DF_PASSWORD + i] = 0x30 + i
            mem_w16(mem, phys(UDA_SEG) + U_STACK_SP, 0x00F0)
            # every u_init* field starts 0 -> defaults to wrkseg

            for i in range(10):
                mem_w16(mem, 2 * i, 0x1100 + i)  # IVT ints 0-4, recognizable pattern
            for i in range(4):
                mem_w16(mem, 0x380 + 2 * i, 0x2200 + i)  # i_mpm_ip block

            return mem, SYSDAT, CHILD_PD, UDA_SEG, WRKSEG, PARENT_UDA_SEG

        def success_snapshot(img, child_pd, uda_seg, wrkseg):
            ub = phys(uda_seg)
            # the IRET-frame step's own `di`/`ax` are read via the
            # ambient ES (uda_seg), not the just-switched user-stack
            # segment -- see src-c/kernel/proc.c's own header comment
            # on this; only the final frame *write* uses wrkseg
            di = mem_r16(img, ub + U_SP)
            frame = phys(wrkseg) + di
            return (
                mem_r16(img, child_pd + P_UDA),
                mem_r16(img, child_pd + P_PARENT),
                img[child_pd + P_LST],
                mem_r16(img, child_pd + P_FLAG),
                bytes(img[ub + U_DF_PASSWORD:ub + U_DF_PASSWORD + 8]),
                mem_r16(img, ub + U_INITCS), mem_r16(img, ub + U_INITDS),
                mem_r16(img, ub + U_INITSS), mem_r16(img, ub + U_INITES),
                [mem_r16(img, ub + U_IVECTORS + 2 * i) for i in range(10)],
                [mem_r16(img, ub + U_MPM_IP + 2 * i) for i in range(4)],
                mem_r16(img, ub + U_SS), mem_r16(img, ub + U_SP),
                img[ub + U_ERROR_MODE], img[ub + U_PD_CNT], img[ub + U_MULT_CNT],
                img[ub + U_IN_INT], img[ub + U_INSYS],
                mem_r16(img, ub + U_DS_SAV), mem_r16(img, ub + U_DMA_SEG),
                mem_r16(img, ub + U_ES_SAV), mem_r16(img, ub + U_DPARAM),
                mem_r16(img, ub + U_FLAG_SAV),
                mem_r16(img, frame + 2), mem_r16(img, frame + 4),
                mem_r16(img, SD_THRDRT), mem_r16(img, child_pd + P_THREAD),
                mem_r16(img, SD_DRL), mem_r16(img, child_pd + P_LINK),
            )

        mem, SYSDAT, CHILD_PD, UDA_SEG, WRKSEG, PARENT_UDA_SEG = build_success_mem()
        ref_regs, ref_img = run_reference(code, entries['proc_creat'], mem, {'dx': CHILD_PD, 'es': PARENT_UDA_SEG}, SYSDAT)
        buf = to_ctypes(mem)
        bx = lib.proc_creat(ctypes.cast(buf, c_u8p), ctypes.c_uint16(CHILD_PD), ctypes.c_uint16(SYSDAT), None)
        port_img = bytes(buf)
        ref = (ref_regs['bx'],) + success_snapshot(ref_img, CHILD_PD, UDA_SEG, WRKSEG)
        port = (bx,) + success_snapshot(port_img, CHILD_PD, UDA_SEG, WRKSEG)
        check("proc_creat: single success (defaults, password, ivectors, iret frame, lists)", ref, port)

        # -- proc_creat: a 2-long creation chain (tail-loop drains both) --
        mem, SYSDAT, CHILD_PD, UDA_SEG, WRKSEG, PARENT_UDA_SEG = build_success_mem()
        CHILD2_PD = 0x5300
        UDA_SEG2 = 0x3500
        mem_w16(mem, CHILD_PD + P_LINK, CHILD2_PD)  # chain to a second creation
        mem_w16(mem, CHILD2_PD + P_LINK, 0)
        mem_w16(mem, CHILD2_PD + P_UDA, (UDA_SEG2 - WRKSEG) & 0xFFFF)
        mem_w16(mem, CHILD2_PD + P_MEM, 0)
        mem[CHILD2_PD + P_LST] = 9  # >= nlstdev -> reset to 0, then + ncondev

        ref_regs, ref_img = run_reference(code, entries['proc_creat'], mem, {'dx': CHILD_PD, 'es': PARENT_UDA_SEG}, SYSDAT)
        buf = to_ctypes(mem)
        bx = lib.proc_creat(ctypes.cast(buf, c_u8p), ctypes.c_uint16(CHILD_PD), ctypes.c_uint16(SYSDAT), None)
        port_img = bytes(buf)

        def chain_snapshot(img):
            return (
                mem_r16(img, SD_THRDRT),
                mem_r16(img, CHILD2_PD + P_THREAD), mem_r16(img, CHILD_PD + P_THREAD),
                mem_r16(img, SD_DRL),
                mem_r16(img, CHILD2_PD + P_LINK), mem_r16(img, CHILD_PD + P_LINK),
                img[CHILD2_PD + P_LST],
                mem_r16(img, CHILD_PD + P_UDA), mem_r16(img, CHILD2_PD + P_UDA),
            )

        check("proc_creat: 2-long chain", (ref_regs['bx'],) + chain_snapshot(ref_img),
              (bx,) + chain_snapshot(port_img))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
