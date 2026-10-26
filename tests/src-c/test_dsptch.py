#!/usr/bin/env python3
"""Differential test for src-c/kernel/dsptch.c against src/kernel/
DSPTCH.RTM, both on real memory layout. dsptch.c was first written as
a reading/debugging aid for the scheduler-hang investigation (docs/
bugs.md #83-98); see its header comment for what has no C equivalent
(a real interrupt return, the context switch's own register/stack
resumption -- modeled as explicit input/output parameters). The sim
has no IRET opcode, so an unimplemented-opcode exception at
int_disp_exit's/fdisp's own trailing `iret` is the expected stop point.

dsptch.c addresses real memory (defs.h's far_ptr), so the reference
runs with tools/i86sim.py's segmented=True and real segments:
tests/src-c/fixtures/dsptch_wrapper.A86 includes the real SYSDAT.FMT/
UDA.FMT/VEC.FMT; SYSDAT is at segment 100h with PDs and the flag entry
at SYSDAT-relative offsets; the outgoing process's UDA is the
reference's ES and its PD's p_uda. The code is relocated to physical
800h (CS=80h) because the dispatcher saves/restores the interrupt
vectors at segment 0.

Every case also diffs the whole image, leaving out the code, both
stacks (the harness's at SS=0 and the dispatcher's own below SYSDAT's
dsptchtos), and exactly the fields dsptch.c documents as having no C
equivalent: the outgoing UDA's u_ss/u_sp/u_flag_sav (the real stack
and flags save) and fdisp's u_ds_sav/u_es_sav/es_sav/bx_sav.
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

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
CSEG_BASE = 0x0800
CS_REG = CSEG_BASE >> 4

SYSDAT = 0x0100

P_LINK, P_THREAD, P_STAT, P_PRIOR, P_FLAG = 0, 2, 4, 5, 6
P_UDA, P_MEM, P_WAIT, P_SCRATCH = 0x10, 0x16, 0x1A, 0x2E
PF_RESOURCE = 0x20
PS_RUN = 0
FLG_PD, FLAG_ON = 0, 0xFFFE

PD_CUR, PD_B, OTHER, FLAG_ENTRY = 0x2000, 0x2100, 0x2400, 0x2500  # SYSDAT offsets
UDA_CUR, UDA_B = 0x0400, 0x0500
B_STACK = (0x0900, 0x0100)  # PD_B's own u_ss:u_sp

FIELDS = ('dsptch', 'fdisp', 'pdisp', 'rlr', 'drl', 'dlr', 'plr', 'slr', 'thrdrt', 'pul',
          'mxmemowner', 'indisp', 'dsptchtos', 'es_sav', 'bx_sav', 'i_mpm_ip',
          'u_dparam', 'u_in_int', 'u_sp', 'u_ss', 'u_ax', 'u_bx', 'u_cx', 'u_dx', 'u_di', 'u_si',
          'u_bp', 'u_ivectors', 'u_mpm_ip', 'u_stat_sav', 'u_flag_sav', 'u_ds_sav', 'u_es_sav')


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
         '-I', INCLUDE_DIR, *DSPTCH_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def main():
    code, sym = assemble_reference()
    sd = lambda off: phys(SYSDAT, off)  # noqa: E731
    ucur = lambda name, k=0: phys(UDA_CUR, sym[name] + k)  # noqa: E731
    ub = lambda name, k=0: phys(UDA_B, sym[name] + k)  # noqa: E731

    def base_memory():
        mem = bytearray(SIM_MEM)
        w16(mem, sd(sym['rlr']), PD_CUR)
        w16(mem, sd(PD_CUR + P_LINK), PD_B)
        w16(mem, sd(PD_CUR + P_UDA), UDA_CUR)
        w16(mem, sd(PD_B + P_LINK), 0)
        w16(mem, sd(PD_B + P_UDA), UDA_B)
        mem[sd(PD_B + P_STAT)] = PS_RUN
        # PD_B's own UDA: the process we're about to switch to
        for name, val in (('u_ax', 0x1111), ('u_bx', 0x2222), ('u_cx', 0x3333), ('u_dx', 0x4444),
                          ('u_si', 0x5555), ('u_di', 0x6666), ('u_bp', 0x7777)):
            w16(mem, ub(name), val)
        # switch's `mov ss,u_ss ! mov sp,u_sp` really switches to PD_B's
        # stack before the final ret, so a return address has to be
        # waiting there: RET_SENTINEL (0xFFFF), which i86sim's run()
        # treats as "halt cleanly"
        w16(mem, ub('u_ss'), B_STACK[0])
        w16(mem, ub('u_sp'), B_STACK[1])
        w16(mem, phys(*B_STACK), 0xFFFF)
        for i in range(10):
            w16(mem, ub('u_ivectors', 2 * i), 0x8100 + i)
        for i in range(4):
            w16(mem, ub('u_mpm_ip', 2 * i), 0x8200 + i)
        return mem

    # Not modeled by dsptch.c (no C equivalent, see its header comment)
    unmodeled = set(range(CSEG_BASE, CSEG_BASE + len(code)))           # code
    unmodeled |= set(range(0xFF00, SIM_MEM))                           # harness stack (SS=0)
    unmodeled |= set(range(sd(sym['dsptchtos']) - 0x40, sd(sym['dsptchtos'])))  # dispatcher stack
    for name in ('u_ss', 'u_sp', 'u_flag_sav', 'u_ds_sav', 'u_es_sav'):
        unmodeled |= {ucur(name), ucur(name) + 1}
    for name in ('es_sav', 'bx_sav'):
        unmodeled |= {sd(sym[name]), sd(sym[name]) + 1}

    def run_reference(entry, mem, regs):
        image = bytearray(mem)
        image[CSEG_BASE:CSEG_BASE + len(code)] = code
        w16(image, CSEG_BASE, SYSDAT)  # `sysdat dw 0` sits at cseg offset 0
        # `call mpmif` assembles assuming cseg at 0, so relocated it
        # lands at CSEG_BASE + 9000h (xiosif is a real stub, see wrapper)
        sim = I86Sim(image, {'ds': SYSDAT, 'ss': 0, 'sp': STACK_TOP, 'cs': CS_REG, **regs},
                     CSEG_BASE + entry, segmented=True,
                     call_stub_addrs={CSEG_BASE + 0x9000})
        try:
            sim.run()
        except I86SimError as e:
            if 'unimplemented opcode 0xCF' not in str(e):
                raise
            # the real asm's own IRET -- regs are already the pre-IRET state
        return sim.regs, image

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)

        class SwitchResult(ctypes.Structure):
            _fields_ = [('ax', ctypes.c_uint16), ('bx', ctypes.c_uint16),
                        ('cx', ctypes.c_uint16), ('dx', ctypes.c_uint16),
                        ('si', ctypes.c_uint16), ('di', ctypes.c_uint16),
                        ('bp', ctypes.c_uint16), ('new_ss', ctypes.c_uint16),
                        ('new_sp', ctypes.c_uint16), ('went_to_int_disp_exit', ctypes.c_uint8)]

        argt = [ctypes.POINTER(ctypes.c_uint8)] + [ctypes.c_uint16] * 8 + [ctypes.POINTER(ctypes.c_uint8)]
        for fn in (lib.dsptch, lib.pdisp, lib.fdisp):
            fn.restype = SwitchResult
            fn.argtypes = argt

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # es = the outgoing process's own UDA, the ambient register the
        # real asm reaches every u_* field through
        REGS_IN = dict(ax=0xAAAA, bx=0xBBBB, cx=0xCCCC, dx=0xDDDD, si=0x1234, di=0x5678, bp=0x9ABC,
                       es=UDA_CUR)

        def call_c(fn, mem):
            buf = (ctypes.c_uint8 * PHYS_MEM)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            flag = ctypes.c_uint8(0xFF)
            got = fn(buf, SYSDAT, REGS_IN['ax'], REGS_IN['bx'], REGS_IN['cx'], REGS_IN['dx'],
                     REGS_IN['si'], REGS_IN['di'], REGS_IN['bp'], ctypes.byref(flag))
            return got, flag.value, bytes(buf)

        def run_case(label, entry, fn, mem):
            regs, ref = run_reference(sym[entry], mem, REGS_IN)
            got, flag, port = call_c(fn, mem)
            diffs = [hex(a) for a in range(SIM_MEM) if a not in unmodeled and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))
            return regs, ref, got, flag, port

        def switch_snapshot(img):
            return ([r16(img, sd(sym['rlr'])), r16(img, sd(PD_CUR + P_LINK))] +
                    [r16(img, ucur(n)) for n in ('u_ax', 'u_bx', 'u_cx', 'u_dx', 'u_si', 'u_di', 'u_bp')] +
                    [r16(img, 0), r16(img, 2), r16(img, sym['i_mpm_ip']), img[sd(sym['indisp'])]])

        # -- dsptch: already in dispatch -> no-op short-circuit --
        mem = base_memory()
        mem[sd(sym['indisp'])] = 0xFF
        _, ref, _, flag, port = run_case("dsptch: already in dispatch", 'dsptch', lib.dsptch, mem)
        check("dsptch: already in dispatch -> no-op", ref[sd(sym['indisp'])], port[sd(sym['indisp'])])
        check("dsptch: already-in-dispatch flag set", 1, flag)

        # -- dsptch: stat=0 (run) -> disp_act, switches to PD_B --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 0
        regs, ref, got, _, port = run_case("dsptch: disp_act", 'dsptch', lib.dsptch, mem)
        check("dsptch: disp_act -> switches to PD_B", switch_snapshot(ref), switch_snapshot(port))
        check("dsptch: disp_act -- switch_result matches",
              tuple(regs[r] for r in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp')),
              (got.ax, got.bx, got.cx, got.dx, got.si, got.di, got.bp))
        check("dsptch: disp_act -- new ss:sp is PD_B's", B_STACK, (got.new_ss, got.new_sp))

        # -- dsptch: stat=5 (sleep) -> sleep_act, onto the list u_dparam names --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 5
        w16(mem, ucur('u_dparam'), (sym['dlr'] - P_LINK) & 0xFFFF)
        mem[sd(PD_CUR + P_SCRATCH)] = 2  # ps_delay
        _, ref, _, _, port = run_case("dsptch: sleep_act", 'dsptch', lib.dsptch, mem)
        snap = lambda img: (r16(img, sd(sym['dlr'])), img[sd(PD_CUR + P_STAT)],  # noqa: E731
                            img[sd(PD_CUR + P_FLAG)] & PF_RESOURCE)
        check("dsptch: sleep_act", snap(ref), snap(port))

        # -- dsptch: stat=3 (swap) -> swap_act --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 3
        _, ref, _, _, port = run_case("dsptch: swap_act", 'dsptch', lib.dsptch, mem)
        check("dsptch: swap_act", r16(ref, sd(sym['slr'])), r16(port, sd(sym['slr'])))

        # -- dsptch: stat=1 (poll) -> poll_act, onto the poll list --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 1
        w16(mem, ucur('u_dparam'), 0x0007)
        _, ref, _, _, port = run_case("dsptch: poll_act", 'dsptch', lib.dsptch, mem)
        snap = lambda img: (r16(img, sd(sym['plr'])), r16(img, sd(PD_CUR + P_WAIT)))  # noqa: E731
        check("dsptch: poll_act", snap(ref), snap(port))

        # -- dsptch: stat=2 (delay) -> delay_act, ahead of an existing entry --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 2
        w16(mem, ucur('u_dparam'), 5)
        w16(mem, sd(sym['dlr']), OTHER)
        w16(mem, sd(OTHER + P_LINK), 0)
        w16(mem, sd(OTHER + P_WAIT), 10)
        _, ref, _, _, port = run_case("dsptch: delay_act", 'dsptch', lib.dsptch, mem)
        snap = lambda img: (r16(img, sd(sym['dlr'])), r16(img, sd(PD_CUR + P_WAIT)),  # noqa: E731
                            r16(img, sd(OTHER + P_WAIT)))
        check("dsptch: delay_act, inserted ahead of an existing entry", snap(ref), snap(port))

        # -- dsptch: stat=8 (flag), flag not yet set -> flag_act waits --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 8
        w16(mem, ucur('u_dparam'), FLAG_ENTRY)
        w16(mem, sd(FLAG_ENTRY + FLG_PD), 0)
        _, ref, _, _, port = run_case("dsptch: flag_act waits", 'dsptch', lib.dsptch, mem)
        check("dsptch: flag_act, waits", r16(ref, sd(FLAG_ENTRY + FLG_PD)), r16(port, sd(FLAG_ENTRY + FLG_PD)))

        # -- dsptch: stat=8 (flag), flag already set -> flag_act -> disp_act --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 8
        w16(mem, ucur('u_dparam'), FLAG_ENTRY)
        w16(mem, sd(FLAG_ENTRY + FLG_PD), FLAG_ON)
        _, ref, _, _, port = run_case("dsptch: flag_act set", 'dsptch', lib.dsptch, mem)
        snap = lambda img: (r16(img, sd(FLAG_ENTRY + FLG_PD)), img[sd(PD_CUR + P_STAT)])  # noqa: E731
        check("dsptch: flag_act, already set -> disp_act", snap(ref), snap(port))

        # -- dsptch: stat=4 (term) -> term_act, frees the PD via freepd --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 4
        w16(mem, sd(PD_CUR + P_MEM), 0)
        w16(mem, sd(sym['thrdrt']), PD_CUR)
        w16(mem, sd(PD_CUR + P_THREAD), 0)
        w16(mem, sd(sym['pul']), 0)
        w16(mem, sd(sym['mxmemowner']), PD_CUR)
        _, ref, _, _, port = run_case("dsptch: term_act", 'dsptch', lib.dsptch, mem)
        snap = lambda img: (r16(img, sd(sym['mxmemowner'])), r16(img, sd(sym['pul'])),  # noqa: E731
                            r16(img, sd(sym['thrdrt'])))
        check("dsptch: term_act -> frees the terminating PD", snap(ref), snap(port))

        # -- pdisp: sets p_stat=ps_run on rlr, then dispatches --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 7  # overwritten by pdisp's own prep
        _, ref, _, _, port = run_case("pdisp", 'pdisp', lib.pdisp, mem)
        check("pdisp: switches to PD_B same as dsptch", switch_snapshot(ref), switch_snapshot(port))

        # -- fdisp: already in dispatch -> no-op short-circuit --
        mem = base_memory()
        mem[sd(sym['indisp'])] = 0xFF
        _, _, _, flag, _ = run_case("fdisp: already in dispatch", 'fdisp', lib.fdisp, mem)
        check("fdisp: already in dispatch -> no-op", 1, flag)

        # -- fdisp: normal entry, sets u_in_int/u_stat_sav on rlr, then
        # switches to PD_B (whose own u_in_int was already true, so
        # switch goes through int_disp_exit) --
        mem = base_memory()
        mem[sd(PD_CUR + P_STAT)] = 9  # ciowait -- no real table entry, falls to disp_act
        mem[ub('u_in_int')] = 0xFF
        mem[sd(PD_B + P_STAT)] = 0x42
        mem[ub('u_stat_sav')] = 0x42
        _, ref, got, _, port = run_case("fdisp", 'fdisp', lib.fdisp, mem)
        snap = lambda img: (img[ucur('u_in_int')], img[ucur('u_stat_sav')], img[sd(PD_CUR + P_STAT)],  # noqa: E731
                            img[sd(sym['indisp'])], img[sd(PD_B + P_STAT)], img[ub('u_in_int')])
        check("fdisp: sets in_int/stat_sav, switches through int_disp_exit", snap(ref), snap(port))
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
