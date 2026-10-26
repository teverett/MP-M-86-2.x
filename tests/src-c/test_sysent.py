#!/usr/bin/env python3
"""Differential test for src-c/kernel/sysent.c against all 8 of
src/kernel/SYSENT.RTM's entries, both on real memory layout.

poll_entry/delay_entry/dispatch_entry/set_prior_entry/sleep_entry tail-
jump into the dispatcher and never return -- per docs/src-c.md's
boundary-stub convention the simulator stops there and the test
compares what each one wrote. pd_entry and wakeup_entry really return.

sysent.c addresses real memory (defs.h's far_ptr), so the references
run with tools/i86sim.py's segmented=True: SYSDAT at segment 100h with
PDs and list roots at SYSDAT-relative offsets (DS-relative, as the
real asm reaches them), and the running process's UDA as the
reference's ES and its PD's p_uda (sysent.c's u_dparam/u_retseg go
there). Field offsets come from the assembled symbol table, and every
case diffs the whole image to catch stray writes.

creat_proc_entry runs all the way through a real proc_creat + pdisp
(tests/src-c/fixtures/creat_proc_wrapper.A86 bundles the real PROC.RTM
and DSPTCH.RTM); see sysent.c's header comment for why its saved
ax/bx/cx/si/di aren't meaningful.
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

FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')
SYSENT_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in ('sysent.c', 'proc.c', 'dsptch.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
HARNESS_STACK = set(range(0xFF00, SIM_MEM))  # SS=0

SYSDAT = 0x0100
P_LINK, P_STAT, P_PRIOR, P_FLAG, P_UDA, P_MEM, P_LST, P_PARENT, P_SCRATCH = (
    0, 4, 5, 6, 0x10, 0x16, 0x24, 0x1E, 0x2E)
PS_RUN = 0
DSPTCH_STUB, PDISP_STUB = 0x9000, 0x9010

PD_ADDR, UDA_CUR = 0x2000, 0x0400          # running process (rlr) and its UDA
SYSLIST_ROOT, WAKE_PD = 0x2100, 0x2200     # SYSDAT offsets


def phys(seg, off=0):
    return seg * 16 + off


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def sd(off):
    return phys(SYSDAT, off)


def assemble(name, fields):
    asm = Assembler()
    asm.assemble(os.path.join(FIXTURES, name), [KERNEL_DIR])
    return asm.segment_bytes('cseg'), {f: asm.symtab[f].value for f in fields}


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *SYSENT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def to_phys(mem):
    buf = (ctypes.c_uint8 * PHYS_MEM)()
    ctypes.memmove(buf, bytes(mem), len(mem))
    return buf


def main():
    code, sym = assemble('sysent_wrapper.A86', (
        'poll_entry', 'delay_entry', 'dispatch_entry', 'set_prior_entry', 'pd_entry',
        'sleep_entry', 'wakeup_entry', 'rlr', 'drl', 'u_dparam', 'u_retseg'))
    cp_fields = ('creat_proc_entry', 'rlr', 'drl', 'pul', 'thrdrt', 'endseg', 'ncondev', 'nlstdev',
                 'dsptchtos', 'i_mpm_ip', 'u_stack_sp', 'u_wrkseg', 'u_ss', 'u_sp', 'u_flag_sav',
                 'u_ax', 'u_bx', 'u_cx', 'u_si', 'u_di')
    cp_code, cp = assemble('creat_proc_wrapper.A86', cp_fields)
    code_range = set(range(len(code)))

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        u8p = ctypes.POINTER(ctypes.c_uint8)
        u16 = ctypes.c_uint16
        for name in ('poll_entry', 'delay_entry', 'sleep_entry', 'wakeup_entry'):
            getattr(lib, name).argtypes = [u8p, u16, u16]
        lib.dispatch_entry.argtypes = []
        lib.set_prior_entry.argtypes = [u8p, u16, ctypes.c_uint8]
        lib.pd_entry.restype = u16
        lib.pd_entry.argtypes = [u8p, u16, u16, ctypes.POINTER(u16)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def check_images(label, ref, port, skip):
            diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        def base_memory():
            mem = bytearray(SIM_MEM)
            w16(mem, sd(sym['rlr']), PD_ADDR)
            w16(mem, sd(PD_ADDR + P_UDA), UDA_CUR)
            return mem

        def run_reference(entry, mem, regs, stop_addrs=()):
            image = bytearray(mem)
            image[0:len(code)] = code
            sim = I86Sim(image, {'ds': SYSDAT, 'es': UDA_CUR, 'ss': 0, 'sp': STACK_TOP, 'cs': 0, **regs},
                         sym[entry], stop_addrs=set(stop_addrs), segmented=True)
            sim.run()
            return sim.regs, image

        def run_case(label, entry, mem, regs, stop, call_port, snap):
            ref_regs, ref = run_reference(entry, mem, regs, stop)
            buf = to_phys(mem)
            result = call_port(buf)
            port = bytes(buf)
            check(label, snap(ref, ref_regs, None), snap(port, None, result))
            check_images(label, ref, port, code_range | HARNESS_STACK)

        dparam = phys(UDA_CUR, sym['u_dparam'])
        stat = sd(PD_ADDR + P_STAT)

        # poll_entry: dx = device -> p_stat=ps_poll, u_dparam=dx
        run_case("poll_entry", 'poll_entry', base_memory(), {'dx': 7}, {DSPTCH_STUB},
                 lambda b: lib.poll_entry(b, SYSDAT, 7),
                 lambda img, r, p: (img[stat], r16(img, dparam)))

        # delay_entry: dx = ticks -> p_stat=ps_delay, u_dparam=dx
        run_case("delay_entry", 'delay_entry', base_memory(), {'dx': 42}, {DSPTCH_STUB},
                 lambda b: lib.delay_entry(b, SYSDAT, 42),
                 lambda img, r, p: (img[stat], r16(img, dparam)))

        # dispatch_entry: bx=cx=0 at the pdisp boundary, no memory effects
        regs, _ = run_reference('dispatch_entry', base_memory(), {}, {PDISP_STUB})
        lib.dispatch_entry()
        check("dispatch_entry", (regs['bx'], regs['cx']), (0, 0))

        # set_prior_entry: dl = priority -> p_prior=dl
        run_case("set_prior_entry", 'set_prior_entry', base_memory(), {'dx': 0x0009}, {PDISP_STUB},
                 lambda b: lib.set_prior_entry(b, SYSDAT, 9),
                 lambda img, r, p: img[sd(PD_ADDR + P_PRIOR)])

        # pd_entry: u_retseg = ds (sysdat here: the asm reaches rlr
        # through DS), returns bx=rlr, cx=0 (a real RET)
        retseg = phys(UDA_CUR, sym['u_retseg'])

        def pd_port(b):
            cx = u16()
            bx = lib.pd_entry(b, SYSDAT, SYSDAT, ctypes.byref(cx))
            return bx, cx.value

        run_case("pd_entry", 'pd_entry', base_memory(), {}, (), pd_port,
                 lambda img, r, p: ((r['bx'], r['cx']) if r else p) + (r16(img, retseg),))

        # sleep_entry: dx = wait param -> u_dparam=dx, p_scratch=old p_stat, p_stat=ps_sleep
        mem = base_memory()
        mem[stat] = 0x03  # some prior status, to prove it gets copied to p_scratch
        run_case("sleep_entry", 'sleep_entry', mem, {'dx': 99}, {DSPTCH_STUB},
                 lambda b: lib.sleep_entry(b, SYSDAT, 99),
                 lambda img, r, p: (img[stat], img[sd(PD_ADDR + P_SCRATCH)], r16(img, dparam)))

        # wakeup_entry: a process on the System List, and an empty list
        for label, head in (("wakeup_entry, process waiting", WAKE_PD), ("wakeup_entry, empty list", 0)):
            mem = base_memory()
            w16(mem, sd(SYSLIST_ROOT), head)
            w16(mem, sd(sym['drl']), 0x2999)  # a recognizable prior DRL head
            if head:
                w16(mem, sd(WAKE_PD + P_LINK), 0)
                mem[sd(WAKE_PD + P_STAT)] = 0xEE  # anything but ps_run, to prove it changes
            run_case(label, 'wakeup_entry', mem, {'dx': SYSLIST_ROOT}, {PDISP_STUB},
                     lambda b: lib.wakeup_entry(b, SYSDAT, SYSLIST_ROOT),
                     lambda img, r, p: (r16(img, sd(SYSLIST_ROOT)), r16(img, sd(sym['drl'])),
                                        img[sd(WAKE_PD + P_STAT)], r16(img, sd(WAKE_PD + P_LINK))))

        # ================= creat_proc_entry =================
        # A real end-to-end case: creates one child PD (proc_creat),
        # then really dispatches (pdisp -> dsptch -> schedule ->
        # switch), landing on whichever PD ends up first on RLR. Code
        # relocated to 800h (the dispatcher and proc_creat touch the
        # interrupt vectors at segment 0).
        CSEG_BASE = 0x0800
        PARENT_PD, CHILD_PD = 0x2000, 0x2100
        PARENT_UDA, CHILD_UDA = 0x0400, 0x0500

        class SwitchResult(ctypes.Structure):
            _fields_ = [('ax', u16), ('bx', u16), ('cx', u16), ('dx', u16),
                        ('si', u16), ('di', u16), ('bp', u16), ('new_ss', u16),
                        ('new_sp', u16), ('went_to_int_disp_exit', ctypes.c_uint8)]

        lib.creat_proc_entry.restype = SwitchResult
        lib.creat_proc_entry.argtypes = [u8p] + [u16] * 8 + [ctypes.POINTER(ctypes.c_uint8)]

        mem = bytearray(SIM_MEM)
        w16(mem, sd(cp['endseg']), 0x0F00)
        mem[sd(cp['nlstdev'])] = 4
        mem[sd(cp['ncondev'])] = 2
        w16(mem, sd(cp['rlr']), PARENT_PD)
        w16(mem, sd(PARENT_PD + P_LINK), 0)  # only entry on RLR before the call
        mem[sd(PARENT_PD + P_PRIOR)] = 0
        w16(mem, sd(PARENT_PD + P_UDA), PARENT_UDA)
        mem[sd(PARENT_PD + P_STAT)] = 7  # overwritten to ps_run by pdisp itself
        w16(mem, phys(PARENT_UDA, cp['u_wrkseg']), SYSDAT)
        w16(mem, sd(CHILD_PD + P_LINK), 0)  # end of the (one-long) creation chain
        w16(mem, sd(CHILD_PD + P_UDA), CHILD_UDA - SYSDAT)  # relative to the creator's wrkseg
        mem[sd(CHILD_PD + P_LST)] = 1
        mem[sd(CHILD_PD + P_PRIOR)] = 0xFF
        w16(mem, phys(CHILD_UDA, cp['u_stack_sp']), 0x00F0)
        for i in range(10):
            w16(mem, 2 * i, 0x1100 + i)
        for i in range(4):
            w16(mem, cp['i_mpm_ip'] + 2 * i, 0x2200 + i)
        # insert_process sorts lower p_prior first, so the parent (0)
        # stays ahead of the new child (FFh): switch dispatches right
        # back into the parent. dsptch's own `mov u_ss,ss ! mov u_sp,sp`
        # saved the parent's real ss:sp first, so switch's final ret
        # pops the harness's own RET_SENTINEL.

        REGS_IN = dict(ax=0xAAAA, bx=0xBBBB, cx=0xCCCC, si=0x1234, di=0x5678, bp=0x9ABC,
                       dx=CHILD_PD, es=PARENT_UDA)
        image = bytearray(mem)
        image[CSEG_BASE:CSEG_BASE + len(cp_code)] = cp_code
        w16(image, CSEG_BASE, SYSDAT)  # `sysdat dw 0` at cseg offset 0
        sim = I86Sim(image, {'ds': SYSDAT, 'ss': 0, 'sp': STACK_TOP, 'cs': CSEG_BASE >> 4, **REGS_IN},
                     CSEG_BASE + cp['creat_proc_entry'], call_stub_addrs={CSEG_BASE + 0x9000},
                     segmented=True)
        sim.run()
        ref_regs, ref = sim.regs, image

        buf = to_phys(mem)
        flag = ctypes.c_uint8(0xFF)
        got = lib.creat_proc_entry(buf, SYSDAT, CHILD_PD, REGS_IN['ax'], REGS_IN['bx'], REGS_IN['cx'],
                                   REGS_IN['si'], REGS_IN['di'], REGS_IN['bp'], ctypes.byref(flag))
        port = bytes(buf)

        def cp_snapshot(img):
            return (r16(img, sd(cp['rlr'])), r16(img, sd(PARENT_PD + P_LINK)), r16(img, sd(cp['drl'])),
                    r16(img, sd(cp['thrdrt'])), r16(img, sd(CHILD_PD + P_PARENT)),
                    r16(img, sd(CHILD_PD + P_UDA)), img[sd(CHILD_PD + P_LST)])

        check("creat_proc_entry: creates the child, dispatches back into the parent",
              cp_snapshot(ref), cp_snapshot(port))
        # ax/bx/cx/si/di are proc_creat's leftover scratch by the time
        # dsptch saves them (see sysent.c's header comment); bp is the
        # one register proc_creat never touches, so it's preserved.
        check("creat_proc_entry: switch_result's bp matches", ref_regs['bp'], got.bp)
        skip = set(range(CSEG_BASE, CSEG_BASE + len(cp_code))) | HARNESS_STACK
        skip |= set(range(sd(cp['dsptchtos']) - 0x40, sd(cp['dsptchtos'])))  # dispatcher stack
        for name in ('u_ss', 'u_sp', 'u_flag_sav', 'u_ax', 'u_bx', 'u_cx', 'u_si', 'u_di'):
            a = phys(PARENT_UDA, cp[name])
            skip |= {a, a + 1}
        check_images("creat_proc_entry", ref, port, skip)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
