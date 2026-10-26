#!/usr/bin/env python3
"""Differential test for src-c/kernel/proc.c against src/kernel/
PROC.RTM's freepd/proc_creat (+ internal getpdadr), both on real
memory layout.

proc.c addresses real memory (defs.h's far_ptr), so the reference runs
with tools/i86sim.py's segmented=True and real segments:
tests/src-c/fixtures/proc_wrapper.A86 includes the real SYSDAT.FMT/
UDA.FMT/VEC.FMT, SYSDAT is at segment 100h with PDs at SYSDAT-relative
offsets (DS-relative, as the real asm reaches them), the creator's UDA
is the reference's ES and its PD's p_uda (the C side's rlr_uda), and
the child UDAs/user stacks are real segments. Field offsets come from
the assembled symbol table.

The code is relocated to physical 800h (CS=80h) because proc_creat
reads the real interrupt vectors at segment 0; i86sim fetches from the
physical IP, and near calls/jumps are relative, so only `cs:sysdat`
needs the CS register to match. Every case also diffs the whole image
(outside the code and the reference's stack) to catch stray writes.
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

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
STACK = range(0xFF00, 0x10000)  # reference's own stack (SS=0)
CSEG_BASE = 0x0800
CS_REG = CSEG_BASE >> 4
# `mpmif equ 9000h` assembles assuming cseg at 0; relocated, its call
# lands at 9000h+CSEG_BASE
MPMIF_STUB = 0x9000 + CSEG_BASE

SYSDAT = 0x0100
ENDSEG = 0x0F00

P_LINK, P_THREAD, P_FLAG, P_UDA, P_MEM, P_LST, P_PARENT = 0, 2, 6, 0x10, 0x16, 0x24, 0x1E
PF_TABLE, PF_CHILDABORT = 0x10, 0x800
PDLEN = 0x30
MS_LINK = 0

FIELDS = ('freepd', 'proc_creat', 'rlr', 'drl', 'pul', 'thrdrt', 'endseg', 'ncondev', 'nlstdev',
          'i_mpm_ip', 'u_wrkseg', 'u_sp', 'u_ss', 'u_stack_sp', 'u_df_password', 'u_ivectors',
          'u_mpm_ip', 'u_initcs', 'u_initds', 'u_initss', 'u_inites', 'u_error_mode', 'u_mult_cnt',
          'u_pd_cnt', 'u_in_int', 'u_insys', 'u_ds_sav', 'u_dma_seg', 'u_es_sav', 'u_dparam',
          'u_flag_sav')


def phys(seg, off=0):
    return seg * 16 + off


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in FIELDS}
    return code, sym


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *PROC_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry_off, mem, regs):
    image = bytearray(mem)
    image[CSEG_BASE:CSEG_BASE + len(code)] = code
    w16(image, CSEG_BASE, SYSDAT)  # `sysdat dw 0` sits at cseg offset 0
    sim = I86Sim(image, {'ds': SYSDAT, 'ss': 0, 'sp': STACK_TOP, 'cs': CS_REG, **regs},
                 CSEG_BASE + entry_off, call_stub_addrs={MPMIF_STUB}, segmented=True)
    sim.run()
    return sim.regs, image


def run_port(lib, fn, mem, *args):
    buf = (ctypes.c_uint8 * PHYS_MEM)()
    ctypes.memmove(buf, bytes(mem), len(mem))
    result = fn(buf, ctypes.c_uint16(SYSDAT), *[ctypes.c_uint16(a) for a in args],
                *([None] if fn.__name__ == 'proc_creat' else []))
    return result, bytes(buf)


def main():
    code, sym = assemble_reference()
    code_range = range(CSEG_BASE, CSEG_BASE + len(code))
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        u8p = ctypes.POINTER(ctypes.c_uint8)
        lib.freepd.argtypes = [u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.proc_creat.restype = ctypes.c_uint16
        lib.proc_creat.argtypes = [u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def check_images(label, ref, port):
            diffs = [hex(a) for a in range(SIM_MEM)
                     if a not in code_range and a not in STACK and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        sd = lambda off: phys(SYSDAT, off)  # noqa: E731

        # ================= freepd =================
        PD_X, PD_Y = 0x2000, 0x2100
        for label, flag, pul in (("freepd: pf_table not set -> unlinked only", 0, 0),
                                 ("freepd: pf_table set -> also pushed onto pul", PF_TABLE, 0x2600)):
            mem = bytearray(SIM_MEM)
            w16(mem, sd(sym['thrdrt']), PD_X)
            w16(mem, sd(PD_X + P_THREAD), PD_Y)
            w16(mem, sd(PD_Y + P_THREAD), 0)
            w16(mem, sd(PD_Y + P_FLAG), flag)
            w16(mem, sd(sym['pul']), pul)
            _, ref = run_reference(code, sym['freepd'], mem, {'si': PD_Y})
            _, port = run_port(lib, lib.freepd, mem, PD_Y)
            snap = lambda img: (r16(img, sd(sym['thrdrt'])), r16(img, sd(PD_X + P_THREAD)),  # noqa: E731
                                r16(img, sd(sym['pul'])), r16(img, sd(PD_Y + P_LINK)))
            check(label, snap(ref), snap(port))
            check_images(label, ref, port)

        # ================= proc_creat =================

        # -- dx == 0 -> nothing to do --
        mem = bytearray(SIM_MEM)
        regs, ref = run_reference(code, sym['proc_creat'], mem, {'dx': 0})
        bx, port = run_port(lib, lib.proc_creat, mem, 0)
        check("proc_creat: dx==0", regs['bx'], bx)
        check_images("proc_creat: dx==0", ref, port)

        PARENT_PD, PARENT_UDA = 0x2000, 0x0400
        MS1 = 0x2400

        def creator_mem(wrkseg, pul=0):
            mem = bytearray(SIM_MEM)
            w16(mem, sd(sym['endseg']), ENDSEG)
            mem[sd(sym['nlstdev'])] = 4
            mem[sd(sym['ncondev'])] = 2
            w16(mem, sd(sym['pul']), pul)
            w16(mem, sd(sym['rlr']), PARENT_PD)
            w16(mem, sd(PARENT_PD + P_FLAG), PF_CHILDABORT)
            w16(mem, sd(PARENT_PD + P_UDA), PARENT_UDA)
            w16(mem, sd(PARENT_PD + P_MEM), MS1)  # exercises the share loop
            w16(mem, sd(MS1 + MS_LINK), 0)
            w16(mem, phys(PARENT_UDA, sym['u_wrkseg']), wrkseg)
            for i in range(8):
                mem[phys(PARENT_UDA, sym['u_df_password'] + i)] = 0x30 + i
            for i in range(10):
                w16(mem, 2 * i, 0x1100 + i)  # IVT ints 0-4, recognizable pattern
            for i in range(4):
                w16(mem, sym['i_mpm_ip'] + 2 * i, 0x2200 + i)
            return mem

        def child_pd(mem, wrkseg, pd_addr, uda_seg, lst, link=0, initss=0):
            w16(mem, pd_addr + P_LINK, link)
            w16(mem, pd_addr + P_UDA, (uda_seg - wrkseg) & 0xFFFF)  # relative, per real asm
            mem[pd_addr + P_LST] = lst
            w16(mem, pd_addr + P_MEM, 0)
            w16(mem, phys(uda_seg, sym['u_stack_sp']), 0x00F0)
            w16(mem, phys(uda_seg, sym['u_initss']), initss)

        def uda_snapshot(img, pd, uda_seg):
            u = lambda name, k=0: phys(uda_seg, sym[name] + k)  # noqa: E731
            stack_seg = r16(img, u('u_ss'))
            frame = phys(stack_seg, r16(img, u('u_sp')))
            return (
                r16(img, sd(pd + P_UDA)), r16(img, sd(pd + P_PARENT)), img[sd(pd + P_LST)],
                r16(img, sd(pd + P_FLAG)),
                bytes(img[u('u_df_password'):u('u_df_password', 8)]),
                [r16(img, u(f)) for f in ('u_initcs', 'u_initds', 'u_initss', 'u_inites')],
                [r16(img, u('u_ivectors', 2 * i)) for i in range(10)],
                [r16(img, u('u_mpm_ip', 2 * i)) for i in range(4)],
                r16(img, u('u_ss')), r16(img, u('u_sp')),
                [img[u(f)] for f in ('u_error_mode', 'u_pd_cnt', 'u_mult_cnt', 'u_in_int', 'u_insys')],
                [r16(img, u(f)) for f in ('u_ds_sav', 'u_dma_seg', 'u_es_sav', 'u_dparam', 'u_flag_sav')],
                r16(img, frame + 2), r16(img, frame + 4),
            )

        def lists_snapshot(img, pds):
            return ([r16(img, sd(sym[n])) for n in ('thrdrt', 'drl', 'pul')] +
                    [(r16(img, sd(pd + P_THREAD)), r16(img, sd(pd + P_LINK))) for pd in pds] +
                    [r16(img, sd(PARENT_PD + P_FLAG))])

        def run_case(label, mem, dx, pds_udas):
            regs, ref = run_reference(code, sym['proc_creat'], mem, {'dx': dx, 'es': PARENT_UDA})
            bx, port = run_port(lib, lib.proc_creat, mem, dx)
            check(f"{label}: bx", regs['bx'], bx)
            for pd, uda_seg in pds_udas:
                check(f"{label}: PD {pd:04X}h + UDA {uda_seg:04X}h",
                      uda_snapshot(ref, pd, uda_seg), uda_snapshot(port, pd, uda_seg))
            pds = [pd for pd, _ in pds_udas]
            check(f"{label}: thread/ready/unused lists", lists_snapshot(ref, pds), lists_snapshot(port, pds))
            check_images(label, ref, port)
            return port

        # -- getpdadr fails: u_wrkseg + dx/16 = 0020h, below SYSDAT, and
        # the PD-unused list is empty --
        mem = creator_mem(wrkseg=0x0010)
        regs, ref = run_reference(code, sym['proc_creat'], mem, {'dx': 0x0100, 'es': PARENT_UDA})
        bx, port = run_port(lib, lib.proc_creat, mem, 0x0100)
        check("proc_creat: getpdadr fails -> 0xFFFF", regs['bx'], bx)
        check("proc_creat: getpdadr fails -> 0xFFFF (expected value)", 0xFFFF, bx)
        check_images("proc_creat: getpdadr fails", ref, port)

        # -- single success: u_init* defaults (initss kept), password,
        # ivectors, iret frame on the user stack, lists --
        CHILD_PD, UDA_SEG = 0x2100, 0x0500
        mem = creator_mem(wrkseg=SYSDAT)
        child_pd(mem, SYSDAT, sd(CHILD_PD), UDA_SEG, lst=1, initss=0x0700)
        run_case("proc_creat: single success", mem, CHILD_PD, [(CHILD_PD, UDA_SEG)])

        # -- 2-long creation chain (tail loop drains both) --
        CHILD2_PD, UDA_SEG2 = 0x2300, 0x0600
        mem = creator_mem(wrkseg=SYSDAT)
        child_pd(mem, SYSDAT, sd(CHILD_PD), UDA_SEG, lst=1, link=CHILD2_PD, initss=0x0700)
        child_pd(mem, SYSDAT, sd(CHILD2_PD), UDA_SEG2, lst=9)  # >= nlstdev -> 0, then + ncondev
        run_case("proc_creat: 2-long chain", mem, CHILD_PD, [(CHILD_PD, UDA_SEG), (CHILD2_PD, UDA_SEG2)])

        # -- getpdadr copies an out-of-range PD into the PD table --
        WRKSEG_OUT = 0x0F80          # >= endseg -> outside MP/M
        SRC_DX = 0x0010              # PD at wrkseg:0010h
        TABLE_PD, TABLE_NEXT = 0x2600, 0x2700
        UDA_SEG3 = 0x0800
        mem = creator_mem(wrkseg=WRKSEG_OUT, pul=TABLE_PD)
        w16(mem, sd(TABLE_PD + P_LINK), TABLE_NEXT)
        child_pd(mem, WRKSEG_OUT, phys(WRKSEG_OUT, SRC_DX), UDA_SEG3, lst=1)
        w16(mem, phys(UDA_SEG3, sym['u_stack_sp']), 0x0040)
        port = run_case("proc_creat: PD copied into table", mem, SRC_DX, [(TABLE_PD, UDA_SEG3)])
        check("proc_creat: copied PD flagged pf_table", PF_TABLE, r16(port, sd(TABLE_PD + P_FLAG)) & PF_TABLE)
        check("proc_creat: pul advanced", TABLE_NEXT, r16(port, sd(sym['pul'])))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
