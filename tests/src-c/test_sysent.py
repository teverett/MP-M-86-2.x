#!/usr/bin/env python3
"""Differential test for src-c/kernel/sysent.c against 7 of
src/kernel/SYSENT.RTM's 8 entries (creat_proc_entry is deferred).

poll_entry/delay_entry/dispatch_entry/set_prior_entry/sleep_entry tail-jump
into the (unported) dispatcher and never return -- per docs/src-c.md's
boundary-stub convention, this test stops the simulator right there and
compares the PD fields each one wrote (not a return value). pd_entry and
wakeup_entry really return; those compare bx/cx or memory as applicable.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'sysent_wrapper.A86')
SYSENT_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'sysent.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
PD_ADDR = 0x1000
P_STAT, P_PRIOR, P_LINK, P_SCRATCH = 0x04, 0x05, 0x00, 0x2E
SD_RLR, SD_DRL = 0xA000, 0xA002
UDA_DPARAM, UDA_RETSEG = 0xA010, 0xA014
DSPTCH_STUB, PDISP_STUB = 0x9000, 0x9010

SYSLIST_ROOT = 0x1100  # a word elsewhere, holding the System List head pointer
WAKE_PD_ADDR = 0x1200  # the PD wakeup_entry moves from the System List to DRL


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in (
        'poll_entry', 'delay_entry', 'dispatch_entry', 'set_prior_entry',
        'pd_entry', 'sleep_entry', 'wakeup_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, SYSENT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem[SD_RLR] = PD_ADDR & 0xFF
    mem[SD_RLR + 1] = (PD_ADDR >> 8) & 0xFF
    return mem


def run_reference(code, entry, mem, regs, stop_addrs=()):
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
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)
        lib.poll_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.delay_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.dispatch_entry.argtypes = []
        lib.set_prior_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.sleep_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.wakeup_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.pd_entry.restype = ctypes.c_uint16
        lib.pd_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # poll_entry: dx = device number -> p_stat=ps_poll, u_dparam=dx
        mem = base_memory()
        regs, image = run_reference(code, entries['poll_entry'], mem, {'dx': 7}, {DSPTCH_STUB})
        ref = (image[PD_ADDR + P_STAT], mem_r16(image, UDA_DPARAM))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.poll_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(7))
        port = (port_mem[PD_ADDR + P_STAT], mem_r16(bytes(port_mem), UDA_DPARAM))
        check("poll_entry", ref, port)

        # delay_entry: dx = ticks -> p_stat=ps_delay, u_dparam=dx
        mem = base_memory()
        regs, image = run_reference(code, entries['delay_entry'], mem, {'dx': 42}, {DSPTCH_STUB})
        ref = (image[PD_ADDR + P_STAT], mem_r16(image, UDA_DPARAM))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.delay_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(42))
        port = (port_mem[PD_ADDR + P_STAT], mem_r16(bytes(port_mem), UDA_DPARAM))
        check("delay_entry", ref, port)

        # dispatch_entry: bx=cx=0 at the pdisp boundary, no memory effects
        mem = base_memory()
        regs, image = run_reference(code, entries['dispatch_entry'], mem, {}, {PDISP_STUB})
        ref = (regs['bx'], regs['cx'])
        lib.dispatch_entry()
        check("dispatch_entry", ref, (0, 0))

        # set_prior_entry: dl = priority -> p_prior=dl
        mem = base_memory()
        regs, image = run_reference(code, entries['set_prior_entry'], mem, {'dx': 0x0009}, {PDISP_STUB})
        ref = image[PD_ADDR + P_PRIOR]
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.set_prior_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint8(9))
        port = port_mem[PD_ADDR + P_PRIOR]
        check("set_prior_entry", ref, port)

        # pd_entry: ds -> u_retseg, returns bx=rlr, cx=0 (a real RET)
        mem = base_memory()
        regs, image = run_reference(code, entries['pd_entry'], mem, {'ds': 0x5EED})
        ref = (regs['bx'], regs['cx'], mem_r16(image, UDA_RETSEG))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        cx = ctypes.c_uint16()
        bx = lib.pd_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(0x5EED), ctypes.byref(cx))
        port = (bx, cx.value, mem_r16(bytes(port_mem), UDA_RETSEG))
        check("pd_entry", ref, port)

        # sleep_entry: dx = wait param -> u_dparam=dx, p_scratch=old p_stat, p_stat=ps_sleep
        mem = base_memory()
        mem[PD_ADDR + P_STAT] = 0x03  # some prior status, to prove it gets copied to p_scratch
        regs, image = run_reference(code, entries['sleep_entry'], mem, {'dx': 99}, {DSPTCH_STUB})
        ref = (image[PD_ADDR + P_STAT], image[PD_ADDR + P_SCRATCH], mem_r16(image, UDA_DPARAM))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.sleep_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(99))
        port = (port_mem[PD_ADDR + P_STAT], port_mem[PD_ADDR + P_SCRATCH], mem_r16(bytes(port_mem), UDA_DPARAM))
        check("sleep_entry", ref, port)

        # wakeup_entry, two cases: a process on the System List, and an empty list.
        for label, list_head in (("wakeup_entry, process waiting", WAKE_PD_ADDR), ("wakeup_entry, empty list", 0)):
            mem = base_memory()
            mem_w16(mem, SYSLIST_ROOT, list_head)
            mem_w16(mem, SD_DRL, 0x9999)  # a recognizable prior DRL head
            if list_head:
                mem[WAKE_PD_ADDR + P_LINK] = 0
                mem[WAKE_PD_ADDR + P_LINK + 1] = 0
                mem[WAKE_PD_ADDR + P_STAT] = 0xEE  # anything but ps_run, to prove it changes
            regs, image = run_reference(code, entries['wakeup_entry'], mem, {'dx': SYSLIST_ROOT}, {PDISP_STUB})
            ref = (mem_r16(image, SYSLIST_ROOT), mem_r16(image, SD_DRL),
                   image[WAKE_PD_ADDR + P_STAT] if list_head else None,
                   mem_r16(image, WAKE_PD_ADDR + P_LINK) if list_head else None)
            port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
            lib.wakeup_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(SYSLIST_ROOT))
            port_bytes = bytes(port_mem)
            port = (mem_r16(port_bytes, SYSLIST_ROOT), mem_r16(port_bytes, SD_DRL),
                    port_mem[WAKE_PD_ADDR + P_STAT] if list_head else None,
                    mem_r16(port_bytes, WAKE_PD_ADDR + P_LINK) if list_head else None)
            check(label, ref, port)

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
