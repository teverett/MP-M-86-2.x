#!/usr/bin/env python3
"""Differential test for src-c/kernel/abort.c against src/kernel/
ABORT.RTM's sysreset_entry/terminate_entry/abort_spec_entry (+
internal helpers). Bundled with src/kernel/QUE2.RTM (rlsmx) and
src/kernel/FINDPD.RTM (findpdname_entry), both called as ordinary
internal calls, not boundary stubs.

Needs tools/i86sim.py's `CALL r/m16`/`JMP r/m16` support (`call
cs:abort_tab[di]`) and `segmented=True` (the aborted process's own
dynamically-computed UDA segment, same class as ml.c/proc.c/mau.c).
`jmp dsptch`/`jmp pdisp` are boundary tail-jumps (`stop_addrs`), same
convention as idle_wrapper.A86's own `xiosif`.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'abort_wrapper.A86')
ABORT_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'abort.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'que2.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'findpd.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
DSPTCH, PDISP = 0x9001, 0x9002

SD_RLR, SD_DRL, SD_DLR, SD_PLR, SD_THRDRT = 0xA000, 0xA002, 0xA004, 0xA01C, 0xA032
SD_MXMEMOWNER, SD_MXMEMCOUNT = 0xA03A, 0xA03C
UDA_WRKSEG = 0xA012

P_LINK, P_THREAD, P_STAT, P_PRIOR, P_FLAG = 0, 2, 4, 5, 6
P_NAME, P_UDA, P_MEM, P_CNS, P_LST, P_WAIT, P_PARENT = 8, 0x10, 0x16, 0x20, 0x24, 0x1A, 0x1E
PF_KEEP, PF_TEMPKEEP, PF_SYS, PF_CTLC, PF_CHILDABORT = 1, 0x200, 1, 0x80, 0x800
PS_RUN, PS_TERM = 0, 4

APB_PD, APB_TERM, APB_CNS, APB_PDNAME = 0, 2, 4, 6
E_NO_PDNAME, E_PD_NOTERM = 0x14, 35

U_SP, U_SS, U_IN_INT = 0x1C, 0x1E, 0x1B

WRKSEG = 0x0100  # phys 0x1000 -- clear of the combined wrapper's own
                 # cseg (ABORT.RTM+QUE2.RTM+FINDPD.RTM, ~1.1KB)
APB_ADDR = 0x0010


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
    entries = {name: asm.symtab[name].value for name in
               ('abt_chk', 'terminate_entry', 'abort_spec_entry')}
    terminate_offset = asm.symtab['terminate'].value
    return code, entries, terminate_offset


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *ABORT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, WRKSEG)
    return mem


def put_apb(mem, pd, term, cns, name=b''):
    base = phys(WRKSEG) + APB_ADDR
    mem_w16(mem, base + APB_PD, pd)
    mem_w16(mem, base + APB_TERM, term)
    mem[base + APB_CNS] = cns
    padded = (name + b'\x00' * 8)[:8]
    mem[base + APB_PDNAME:base + APB_PDNAME + 8] = padded


def run_reference(code, entry, mem, regs):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, segmented=True,
                 call_stub_addrs={0x9000}, stop_addrs={DSPTCH, PDISP})
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entries, terminate_offset = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.abt_chk.restype = ctypes.c_uint16
        lib.abt_chk.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.terminate_entry.restype = BxCx
        lib.terminate_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.abort_spec_entry.restype = BxCx
        lib.abort_spec_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        PD_A, PD_B, PD_C, PARENT = 0x5000, 0x5100, 0x5200, 0x5300

        # -- abt_chk: pf_keep set -> not ok --
        mem = base_memory()
        mem_w16(mem, PD_A + P_FLAG, PF_KEEP | PF_CTLC)
        ref_regs, ref_img = run_reference(code, entries['abt_chk'], mem, {'bx': PD_A, 'dx': 0})
        buf = to_ctypes(mem)
        got = lib.abt_chk(ctypes.cast(buf, c_u8p), ctypes.c_uint16(PD_A), ctypes.c_uint8(0))
        ref = (ref_regs['cx'], mem_r16(ref_img, PD_A + P_FLAG))
        port_img = bytes(buf)
        port = (got, mem_r16(port_img, PD_A + P_FLAG))
        check("abt_chk: pf_keep -> not ok, pf_ctlc cleared", ref, port)

        # -- abt_chk: neither keep nor tempkeep, not sys -> ok --
        mem = base_memory()
        mem_w16(mem, PD_A + P_FLAG, 0)
        ref_regs, ref_img = run_reference(code, entries['abt_chk'], mem, {'bx': PD_A, 'dx': 0})
        buf = to_ctypes(mem)
        got = lib.abt_chk(ctypes.cast(buf, c_u8p), ctypes.c_uint16(PD_A), ctypes.c_uint8(0))
        check("abt_chk: ordinary process -> ok", ref_regs['cx'], got)

        # -- terminate_entry: success, reparents a child, propagates
        # pf_ctlc to the parent --
        mem = base_memory()
        mem_w16(mem, SD_RLR, PD_A)
        mem_w16(mem, SD_THRDRT, PD_C)
        mem_w16(mem, PD_C + P_THREAD, 0)
        mem_w16(mem, PD_C + P_PARENT, PD_A)
        mem_w16(mem, PD_A + P_FLAG, PF_CTLC)
        mem_w16(mem, PD_A + P_PARENT, PARENT)
        mem_w16(mem, PARENT + P_FLAG, 0)
        ref_regs, ref_img = run_reference(code, entries['terminate_entry'], mem, {'dx': 0})
        buf = to_ctypes(mem)
        got = lib.terminate_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        port_img = bytes(buf)

        def term_snapshot(img):
            return (mem_r16(img, PD_C + P_PARENT), mem_r16(img, PD_A + P_FLAG),
                    mem_r16(img, PARENT + P_FLAG), mem_r16(img, SD_MXMEMOWNER),
                    img[SD_MXMEMCOUNT], img[PD_A + P_STAT])

        ref = (ref_regs['bx'], ref_regs['cx']) + term_snapshot(ref_img)
        port = (got.bx, got.cx) + term_snapshot(port_img)
        check("terminate_entry: success, reparents child, flags parent", ref, port)

        # -- terminate_entry: pf_keep -> e_pd_noterm --
        mem = base_memory()
        mem_w16(mem, SD_RLR, PD_A)
        mem_w16(mem, PD_A + P_FLAG, PF_KEEP)
        ref_regs, ref_img = run_reference(code, entries['terminate_entry'], mem, {'dx': 0})
        buf = to_ctypes(mem)
        got = lib.terminate_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        check("terminate_entry: pf_keep -> e_pd_noterm", (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))

        # -- abort_spec_entry: abort a PD on the ready list (abtrun) --
        mem = base_memory()
        mem_w16(mem, SD_RLR, PD_B)  # someone else is running
        mem_w16(mem, SD_THRDRT, PD_A)
        mem_w16(mem, PD_A + P_THREAD, 0)
        mem_w16(mem, SD_RLR, PD_B)
        mem_w16(mem, PD_B + P_LINK, PD_A)  # PD_A is next on the ready list
        mem_w16(mem, PD_A + P_LINK, 0)
        mem_w16(mem, PD_A + P_STAT, PS_RUN)
        mem_w16(mem, PD_A + P_FLAG, PF_CTLC)
        mem_w16(mem, PD_A + P_UDA, 0x0200)  # dynamically-computed UDA segment
        put_apb(mem, PD_A, 0x1234, 5)
        ref_regs, ref_img = run_reference(code, entries['abort_spec_entry'], mem,
                                           {'dx': APB_ADDR})
        buf = to_ctypes(mem)
        got = lib.abort_spec_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(APB_ADDR),
                                    ctypes.c_uint16(terminate_offset))
        port_img = bytes(buf)

        def spec_snapshot(img):
            uda = phys(0x0200)
            return (mem_r16(img, SD_DRL), mem_r16(img, PD_A + P_LINK),
                    mem_r16(img, PD_B + P_LINK), img[PD_A + P_PRIOR],
                    mem_r16(img, PD_A + P_FLAG),
                    mem_r16(img, uda + U_SP), mem_r16(img, uda + U_SS),
                    img[uda + U_IN_INT], mem_r16(img, uda + 0x00FE),
                    mem_r16(img, uda + 0x0000))

        ref = (ref_regs['bx'], ref_regs['cx']) + spec_snapshot(ref_img)
        port = (got.bx, got.cx) + spec_snapshot(port_img)
        check("abort_spec_entry: aborts a PD on the ready list", ref, port)

        # -- abort_spec_entry: target not found -> e_no_pdname --
        mem = base_memory()
        mem_w16(mem, SD_THRDRT, 0)
        put_apb(mem, PD_A, 0, 0)
        ref_regs, ref_img = run_reference(code, entries['abort_spec_entry'], mem,
                                           {'dx': APB_ADDR})
        buf = to_ctypes(mem)
        got = lib.abort_spec_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(APB_ADDR),
                                    ctypes.c_uint16(terminate_offset))
        check("abort_spec_entry: target not found -> e_no_pdname",
              (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))

        # -- abort_spec_entry: aborting self delegates to terminate_entry --
        mem = base_memory()
        mem_w16(mem, SD_RLR, PD_A)
        mem_w16(mem, SD_THRDRT, PD_A)
        mem_w16(mem, PD_A + P_THREAD, 0)
        mem_w16(mem, PD_A + P_FLAG, 0)
        put_apb(mem, PD_A, 0, 0)
        ref_regs, ref_img = run_reference(code, entries['abort_spec_entry'], mem,
                                           {'dx': APB_ADDR})
        buf = to_ctypes(mem)
        got = lib.abort_spec_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(APB_ADDR),
                                    ctypes.c_uint16(terminate_offset))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], ref_img[PD_A + P_STAT])
        port = (got.bx, got.cx, port_img[PD_A + P_STAT])
        check("abort_spec_entry: self-abort delegates to terminate_entry", ref, port)

        # -- abort_spec_entry: abort a PD on the Delay List (abtdly, fixes
        # up the next PD's own p_wait) --
        mem = base_memory()
        mem_w16(mem, SD_RLR, PD_B)
        mem_w16(mem, SD_THRDRT, PD_A)
        mem_w16(mem, PD_A + P_THREAD, 0)
        mem_w16(mem, SD_DLR, PD_A)
        mem_w16(mem, PD_A + P_LINK, PD_C)
        mem_w16(mem, PD_A + P_WAIT, 5)
        mem_w16(mem, PD_C + P_LINK, 0)
        mem_w16(mem, PD_C + P_WAIT, 10)
        mem_w16(mem, PD_A + P_STAT, 2)  # ps_delay
        mem_w16(mem, PD_A + P_FLAG, 0)
        mem_w16(mem, PD_A + P_UDA, 0x0300)
        put_apb(mem, PD_A, 0, 0)
        ref_regs, ref_img = run_reference(code, entries['abort_spec_entry'], mem,
                                           {'dx': APB_ADDR})
        buf = to_ctypes(mem)
        got = lib.abort_spec_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(APB_ADDR),
                                    ctypes.c_uint16(terminate_offset))
        port_img = bytes(buf)
        ref = (ref_regs['bx'], ref_regs['cx'], mem_r16(ref_img, SD_DLR),
               mem_r16(ref_img, PD_C + P_WAIT))
        port = (got.bx, got.cx, mem_r16(port_img, SD_DLR), mem_r16(port_img, PD_C + P_WAIT))
        check("abort_spec_entry: aborts a PD on the Delay List", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
