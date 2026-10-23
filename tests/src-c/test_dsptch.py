#!/usr/bin/env python3
"""Differential test for src-c/kernel/dsptch.c against src/kernel/
DSPTCH.RTM. Written as a reading/debugging aid for the live
scheduler-hang investigation (docs/bugs.md #83-98 and onward), per
the user's own request -- see dsptch.c's own header comment for what
this file is (and isn't): there is no C equivalent for a real
interrupt return or an actual context switch's own register/stack
resumption, so those are modeled as explicit input/output parameters
instead (see dsptch.c's own header comment for the exact mapping),
and `int_disp_exit`'s/`fdisp`'s own trailing `iret` is treated as an
implicit stop point (the sim has no IRET opcode at all -- an
unimplemented-opcode exception at that exact instruction is the
expected, successful outcome, not a real error).

Bundled with src/kernel/PROC.RTM (for `freepd`, called by `term_act`
as an ordinary internal call). Needs `segmented=True` (dynamically-
computed UDA segments) and a CSEG_BASE shift (this routine reads/
writes the real absolute interrupt vector table at segment 0, same
reason src-c/kernel/proc.c's own test needed one).
"""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim, I86SimError  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'dsptch_wrapper.A86')
DSPTCH_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'dsptch.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'proc.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0
CSEG_BASE = 0x0800
CS_REG = CSEG_BASE >> 4

SD_RLR, SD_DRL, SD_DLR, SD_PLR, SD_SLR = 0xA000, 0xA002, 0xA004, 0xA01C, 0xA01E
SD_THRDRT, SD_PUL, SD_MXMEMOWNER, SD_INDISP = 0xA032, 0xA038, 0xA03A, 0xAC00
UDA_WRKSEG = 0xA012

P_LINK, P_THREAD, P_STAT, P_PRIOR, P_FLAG = 0, 2, 4, 5, 6
P_UDA, P_MEM, P_CNS, P_LST, P_WAIT, P_SCRATCH, P_PARENT = 0x10, 0x16, 0x20, 0x24, 0x1A, 0x2E, 0x1E
PF_RESOURCE = 0x20
PS_RUN = 0

U_DPARAM, U_IN_INT, U_SP, U_SS = 0, 0x1B, 0x1C, 0x1E
U_AX, U_BX, U_CX, U_DX, U_DI, U_SI, U_BP = 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C
U_IVECTORS, U_STAT_SAV = 0x38, 0x61
FLG_PD, FLAG_ON, FLAG_OFF = 0, 0xFFFE, 0xFFFF

PD_CUR, PD_B = 0x5000, 0x5100
UDA_CUR, UDA_B = 0x0200, 0x0300


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
               ('dsptch', 'fdisp', 'pdisp')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *DSPTCH_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD_CUR)
    mem_w16(mem, PD_CUR + P_LINK, PD_B)
    mem_w16(mem, PD_CUR + P_UDA, UDA_CUR)
    mem_w16(mem, PD_B + P_LINK, 0)
    mem_w16(mem, PD_B + P_UDA, UDA_B)
    mem[PD_B + P_STAT] = PS_RUN
    # PD_B's own UDA: the process we're about to switch to
    base = phys(UDA_B)
    mem_w16(mem, base + U_AX, 0x1111)
    mem_w16(mem, base + U_BX, 0x2222)
    mem_w16(mem, base + U_CX, 0x3333)
    mem_w16(mem, base + U_DX, 0x4444)
    mem_w16(mem, base + U_SI, 0x5555)
    mem_w16(mem, base + U_DI, 0x6666)
    mem_w16(mem, base + U_BP, 0x7777)
    # switch_'s own "mov ss,u_ss!mov sp,u_sp" genuinely switches to this
    # process's own real stack before the final ret -- so, exactly like
    # the harness's own initial STACK_TOP/RET_SENTINEL convention, that
    # stack needs a real return address waiting at [u_ss:u_sp] or the
    # ret pops garbage and the sim wildly jumps (was landing on a
    # coincidental 0xCF byte near the start of the code, not a real
    # IRET at all). RET_SENTINEL (0xFFFF, see tools/i86sim.py) is what
    # the sim's own run() recognizes as "halt cleanly".
    u_ss, u_sp = 0x0900, 0x0100
    mem_w16(mem, base + U_SS, u_ss)
    mem_w16(mem, base + U_SP, u_sp)
    mem_w16(mem, (u_ss * 16 + u_sp) & 0xFFFF, 0xFFFF)
    for i in range(10):
        mem_w16(mem, base + U_IVECTORS + 2 * i, 0x8100 + i)
    for i in range(4):
        mem_w16(mem, base + U_IVECTORS + 20 + 2 * i, 0x8200 + i)  # i_mpm_ip block
    return mem


def run_reference(code, entry, mem, regs):
    image = bytearray(0x10000)
    image[CSEG_BASE:CSEG_BASE + len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # mpmif/xiosif (term_act's own console/list detach loop, schedule's own
    # poll loop) are unported boundary stubs, same convention as every
    # other wrapper's own `call_stub_addrs` (e.g. abort_wrapper.A86's) --
    # but unlike those (which don't shift CSEG_BASE), the `call mpmif`
    # instruction's own rel16 displacement was assembled assuming cs
    # itself (not just the offset) would land exactly on 0x9000, so the
    # actual absolute target under this shift is CSEG_BASE + 0x9000.
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP, 'cs': CS_REG}, CSEG_BASE + entry, segmented=True,
                 call_stub_addrs={(CSEG_BASE + 0x9000) & 0xFFFF, (CSEG_BASE + 0x9001) & 0xFFFF})
    try:
        sim.run()
    except I86SimError as e:
        if 'unimplemented opcode 0xCF' not in str(e):
            raise
        # the real asm's own IRET -- no sim support, and none needed:
        # regs are already the pre-IRET state, exactly what we want
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class SwitchResult(ctypes.Structure):
            _fields_ = [('ax', ctypes.c_uint16), ('bx', ctypes.c_uint16),
                        ('cx', ctypes.c_uint16), ('dx', ctypes.c_uint16),
                        ('si', ctypes.c_uint16), ('di', ctypes.c_uint16),
                        ('bp', ctypes.c_uint16), ('new_ss', ctypes.c_uint16),
                        ('new_sp', ctypes.c_uint16), ('went_to_int_disp_exit', ctypes.c_uint8)]

        argt = [c_u8p] + [ctypes.c_uint16] * 7 + [ctypes.POINTER(ctypes.c_uint8)]
        lib.dsptch.restype = SwitchResult
        lib.dsptch.argtypes = argt
        lib.pdisp.restype = SwitchResult
        lib.pdisp.argtypes = argt
        lib.fdisp.restype = SwitchResult
        lib.fdisp.argtypes = argt

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # `es` is the system-wide ambient "current process's own UDA"
        # register every file in this project assumes is already set
        # correctly on entry (see e.g. UDA.FMT's own header comment) --
        # dsptch/pdisp/fdisp all read/write bare UDA fields relying on
        # it, so the reference run needs it set explicitly (nothing
        # upstream of this entry point sets it for us, unlike a real
        # syscall dispatch).
        REGS_IN = dict(ax=0xAAAA, bx=0xBBBB, cx=0xCCCC, dx=0xDDDD, si=0x1234, di=0x5678, bp=0x9ABC,
                       es=UDA_CUR)

        def call_c(fn, mem):
            buf = to_ctypes(mem)
            flag = ctypes.c_uint8(0xFF)
            got = fn(ctypes.cast(buf, c_u8p), ctypes.c_uint16(REGS_IN['ax']), ctypes.c_uint16(REGS_IN['bx']),
                     ctypes.c_uint16(REGS_IN['cx']), ctypes.c_uint16(REGS_IN['dx']),
                     ctypes.c_uint16(REGS_IN['si']), ctypes.c_uint16(REGS_IN['di']),
                     ctypes.c_uint16(REGS_IN['bp']), ctypes.byref(flag))
            return got, flag.value, bytes(buf)

        def switch_snapshot(img):
            uda = phys(UDA_CUR)
            return (mem_r16(img, SD_RLR), mem_r16(img, PD_CUR + P_LINK),
                    mem_r16(img, uda + U_AX), mem_r16(img, uda + U_BX),
                    mem_r16(img, uda + U_CX), mem_r16(img, uda + U_DX),
                    mem_r16(img, uda + U_SI), mem_r16(img, uda + U_DI),
                    mem_r16(img, uda + U_BP), mem_r16(img, 0), mem_r16(img, 2),
                    mem_r16(img, 0x380), img[SD_INDISP])

        # -- dsptch: already in dispatch -> no-op short-circuit --
        mem = base_memory()
        mem[SD_INDISP] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        check("dsptch: already in dispatch -> no-op", (ref_img[SD_INDISP],), (port_img[SD_INDISP],))
        check("dsptch: already-in-dispatch flag set", 1, flag)

        # -- dsptch: stat=0 (run) -> disp_act, switches to PD_B --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 0
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        ref = switch_snapshot(ref_img)
        port = switch_snapshot(port_img)
        check("dsptch: disp_act -> switches to PD_B", ref, port)
        check("dsptch: disp_act -- switch_result matches",
              (ref_regs['ax'], ref_regs['bx'], ref_regs['cx'], ref_regs['dx'],
               ref_regs['si'], ref_regs['di'], ref_regs['bp']),
              (got.ax, got.bx, got.cx, got.dx, got.si, got.di, got.bp))

        # -- dsptch: stat=5 (sleep) -> sleep_act --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 5
        mem_w16(mem, phys(UDA_CUR) + U_DPARAM, (SD_DLR - P_LINK) & 0xFFFF)
        mem[PD_CUR + P_SCRATCH] = 2  # ps_delay
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        ref = (mem_r16(ref_img, SD_DLR), ref_img[PD_CUR + P_STAT], ref_img[PD_CUR + P_FLAG] & PF_RESOURCE)
        port = (mem_r16(port_img, SD_DLR), port_img[PD_CUR + P_STAT], port_img[PD_CUR + P_FLAG] & PF_RESOURCE)
        check("dsptch: sleep_act", ref, port)

        # -- dsptch: stat=3 (swap) -> swap_act --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 3
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        check("dsptch: swap_act", mem_r16(ref_img, SD_SLR), mem_r16(port_img, SD_SLR))

        # -- dsptch: stat=2 (delay) -> delay_act, inserted into an
        # already-populated delay list --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 2
        mem_w16(mem, phys(UDA_CUR) + U_DPARAM, 5)
        OTHER = 0x5400
        mem_w16(mem, SD_DLR, OTHER)
        mem_w16(mem, OTHER + P_LINK, 0)
        mem_w16(mem, OTHER + P_WAIT, 10)
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        ref = (mem_r16(ref_img, SD_DLR), mem_r16(ref_img, PD_CUR + P_WAIT),
               mem_r16(ref_img, OTHER + P_WAIT))
        port = (mem_r16(port_img, SD_DLR), mem_r16(port_img, PD_CUR + P_WAIT),
                mem_r16(port_img, OTHER + P_WAIT))
        check("dsptch: delay_act, inserted ahead of an existing entry", ref, port)

        # -- dsptch: stat=8 (flag), flag not yet set -> flag_act waits --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 8
        FLAG_ENTRY = 0x5500
        mem_w16(mem, phys(UDA_CUR) + U_DPARAM, FLAG_ENTRY)
        mem_w16(mem, FLAG_ENTRY + FLG_PD, 0)  # not yet set
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        check("dsptch: flag_act, waits", mem_r16(ref_img, FLAG_ENTRY + FLG_PD),
              mem_r16(port_img, FLAG_ENTRY + FLG_PD))

        # -- dsptch: stat=8 (flag), flag already set -> flag_act -> disp_act --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 8
        mem_w16(mem, phys(UDA_CUR) + U_DPARAM, FLAG_ENTRY)
        mem_w16(mem, FLAG_ENTRY + FLG_PD, FLAG_ON)
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        ref = (mem_r16(ref_img, FLAG_ENTRY + FLG_PD), ref_img[PD_CUR + P_STAT])
        port = (mem_r16(port_img, FLAG_ENTRY + FLG_PD), port_img[PD_CUR + P_STAT])
        check("dsptch: flag_act, already set -> disp_act", ref, port)

        # -- dsptch: stat=4 (term) -> term_act, frees the PD via freepd --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 4
        mem_w16(mem, PD_CUR + P_MEM, 0)
        mem_w16(mem, SD_THRDRT, PD_CUR)
        mem_w16(mem, PD_CUR + P_THREAD, 0)
        mem_w16(mem, SD_PUL, 0)
        ref_regs, ref_img = run_reference(code, entries['dsptch'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.dsptch, mem)
        ref = (mem_r16(ref_img, SD_MXMEMOWNER), mem_r16(ref_img, SD_PUL))
        port = (mem_r16(port_img, SD_MXMEMOWNER), mem_r16(port_img, SD_PUL))
        check("dsptch: term_act -> frees the terminating PD", ref, port)

        # -- pdisp: sets p_stat=ps_run on rlr, then dispatches --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 7  # would be overwritten by pdisp's own prep
        ref_regs, ref_img = run_reference(code, entries['pdisp'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.pdisp, mem)
        check("pdisp: switches to PD_B same as dsptch", switch_snapshot(ref_img), switch_snapshot(port_img))

        # -- fdisp: already in dispatch -> no-op short-circuit --
        mem = base_memory()
        mem[SD_INDISP] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['fdisp'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.fdisp, mem)
        check("fdisp: already in dispatch -> no-op", flag, 1)

        # -- fdisp: normal entry, sets u_in_int/u_stat_sav on rlr, then
        # switches to PD_B (whose own u_in_int was already true, so
        # switch_ goes through int_disp_exit) --
        mem = base_memory()
        mem[PD_CUR + P_STAT] = 9  # ciowait -- no real table entry, falls to disp_act
        mem[phys(UDA_B) + U_IN_INT] = 0xFF
        mem[PD_B + P_STAT] = 0x42  # arbitrary pre-interrupt status to restore
        mem[phys(UDA_B) + U_STAT_SAV] = 0x42
        ref_regs, ref_img = run_reference(code, entries['fdisp'], mem, REGS_IN)
        got, flag, port_img = call_c(lib.fdisp, mem)
        ref = (mem_r16(ref_img, phys(UDA_CUR) + U_IN_INT) & 0xFF, ref_img[PD_CUR + P_STAT],
               ref_img[SD_INDISP], ref_img[PD_B + P_STAT])
        port = (mem_r16(port_img, phys(UDA_CUR) + U_IN_INT) & 0xFF, port_img[PD_CUR + P_STAT],
                port_img[SD_INDISP], port_img[PD_B + P_STAT])
        check("fdisp: sets in_int/stat_sav, switches through int_disp_exit", ref, port)
        check("fdisp: went_to_int_disp_exit flag", True, got.went_to_int_disp_exit == 1)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
