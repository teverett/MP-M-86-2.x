#!/usr/bin/env python3
"""Differential test for src-c/kernel/rtmif.c's `rtmif_init` against
src/kernel/RTMIF.RTM's `init`, both running against real memory layout.
`entry` has its own test, tests/src-c/test_rtmif_entry.py.

rtmif_init addresses real memory (defs.h's far_ptr(): seg*16 + off at
real SYSDAT/UDA/RTM-cseg offsets), so the reference runs with
tools/i86sim.py's segmented=True and a real, nonzero SYSDAT segment,
and tests/src-c/fixtures/rtmif_wrapper.A86 includes the real
SYSDAT.FMT/UDA.FMT. Every field address below comes from the assembled
symbol table, so a wrong offset in defs.h shows up as a mismatch at
the real address.

CS is 0: i86sim fetches instructions from the physical IP and ignores
CS, so the code image has to sit at physical 0 for its own `cs:`
accesses (cs:sysdat, cs:supervisor) to land on it.

`init` ends with a real `call proc_creat` (PROC.RTM linked in, proc.c
also on real memory), which drains the idle -> tick creation chain:
the creator is the init process (rlr = initpd, its UDA as the
reference's ES with u_wrkseg = SYSDAT, as INIT.SUP leaves it). Beyond
the per-field checks, the two images are diffed byte for byte, outside
the code image and the reference's own stack, so everything
proc_creat writes is compared too.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'rtmif_wrapper.A86')
# rtmif.c's own `entry` calls into sysent.c/dsptch.c/flag.c/que1.c/
# que2.c/findpd.c/abort.c too -- link them all, even though this test
# only exercises `rtmif_init`.
RTMIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f)
           for f in ('rtmif.c', 'proc.c', 'sysent.c', 'dsptch.c', 'flag.c',
                     'que1.c', 'que2.c', 'findpd.c', 'abort.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000     # i86sim's segmented mode wraps at 64KB
PHYS_MEM = 0x100000   # rtmif_init's view: the real 1MB 8086 address space
STACK_TOP = 0xFFF0    # SS=0, so the reference's stack is physical FFxxh

CS = 0x0000
DS_SYSDAT = 0x0400    # SYSDAT at physical 4000h-4B60h
SUPMOD = (0x0718, 0x1000)      # module_table[SUP] entry far ptr (off, seg)
TICK_P_UDA, IDLE_P_UDA = 0x0100, 0x0110  # SYSDAT-relative paragraphs -> UDAs at 500h/510h
CREATOR_UDA = 0x0600  # the init process's UDA (rlr's p_uda, and ES)
ENDSEG = 0x0F00
TICK_SP, IDLE_SP = 0x0F00, 0x0F80  # u_stack_sp -> IRET frames on the SYSDAT segment (u_initss)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in (
        'init', 'entry', 'fdisp', 'farpdisp', 'sysdat', 'supervisor', 'supmod',
        'dispatcher', 'rtm_pdisp', 'tickpd', 'idlepd', 'p_uda', 'u_initcs', 'u_initss',
        'initpd', 'rlr', 'drl', 'thrdrt', 'endseg', 'p_link', 'p_thread', 'u_wrkseg', 'u_stack_sp')}
    retf_addrs = [i for i in range(sym['init'], sym['entry']) if code[i] == 0xCB]
    assert len(retf_addrs) == 1, f"expected exactly 1 retf, found {retf_addrs}"
    return code, sym, retf_addrs[0]


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *RTMIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def phys(seg, off):
    return seg * 16 + off


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def initial_memory(sym):
    mem = bytearray(SIM_MEM)
    w16(mem, phys(DS_SYSDAT, sym['supmod']), SUPMOD[0])
    w16(mem, phys(DS_SYSDAT, sym['supmod'] + 2), SUPMOD[1])
    w16(mem, phys(DS_SYSDAT, sym['tickpd'] + sym['p_uda']), TICK_P_UDA)
    w16(mem, phys(DS_SYSDAT, sym['idlepd'] + sym['p_uda']), IDLE_P_UDA)
    # creator: the init process, as INIT.SUP leaves it before callf rtmmod
    w16(mem, phys(DS_SYSDAT, sym['rlr']), sym['initpd'])
    w16(mem, phys(DS_SYSDAT, sym['initpd'] + sym['p_uda']), CREATOR_UDA)
    w16(mem, phys(CREATOR_UDA, sym['u_wrkseg']), DS_SYSDAT)
    w16(mem, phys(DS_SYSDAT, sym['endseg']), ENDSEG)
    # creation chain: idlepd -> tickpd
    w16(mem, phys(DS_SYSDAT, sym['idlepd'] + sym['p_link']), sym['tickpd'])
    w16(mem, phys(DS_SYSDAT, sym['tickpd'] + sym['p_link']), 0)
    w16(mem, phys(DS_SYSDAT + TICK_P_UDA, sym['u_stack_sp']), TICK_SP)
    w16(mem, phys(DS_SYSDAT + IDLE_P_UDA, sym['u_stack_sp']), IDLE_SP)
    return mem


def run_reference(code, sym, stop_addr, mem):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {'cs': CS, 'ds': DS_SYSDAT, 'es': CREATOR_UDA, 'ss': 0, 'sp': STACK_TOP},
                 sym['init'],
                 stop_addrs={stop_addr}, segmented=True)
    sim.run()
    return image


def run_port(lib, code, sym, mem):
    buf = (ctypes.c_uint8 * PHYS_MEM)()
    ctypes.memmove(buf, bytes(mem), len(mem))
    ctypes.memmove(buf, bytes(code), len(code))
    lib.rtmif_init(buf, ctypes.c_uint16(DS_SYSDAT), ctypes.c_uint16(CS),
                   ctypes.c_uint16(sym['fdisp']), ctypes.c_uint16(sym['farpdisp']))
    return bytes(buf)


def main():
    code, sym, stop_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.rtmif_init.argtypes = [ctypes.POINTER(ctypes.c_uint8)] + [ctypes.c_uint16] * 4
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        mem = initial_memory(sym)
        ref = run_reference(code, sym, stop_addr, mem)
        port = run_port(lib, code, sym, mem)

        def both16(addr):
            return r16(ref, addr), r16(port, addr)

        sd = lambda off: phys(DS_SYSDAT, off)  # noqa: E731

        check("cs:sysdat", *both16(phys(CS, sym['sysdat'])))
        check("cs:supervisor offset", *both16(phys(CS, sym['supervisor'])))
        check("cs:supervisor segment", *both16(phys(CS, sym['supervisor'] + 2)))
        for name in ('dispatcher', 'rtm_pdisp'):
            check(f"{name} offset", *both16(sd(sym[name])))
            check(f"{name} segment", *both16(sd(sym[name] + 2)))
        for name, p_uda in (('tick', TICK_P_UDA), ('idle', IDLE_P_UDA)):
            uda_seg = DS_SYSDAT + p_uda
            check(f"{name} UDA u_initcs", *both16(phys(uda_seg, sym['u_initcs'])))
            check(f"{name} UDA u_initss", *both16(phys(uda_seg, sym['u_initss'])))
            check(f"{name} UDA u_initcs = supervisor segment", SUPMOD[1],
                  r16(port, phys(uda_seg, sym['u_initcs'])))
            check(f"{name} UDA u_initss = sysdat", DS_SYSDAT, r16(port, phys(uda_seg, sym['u_initss'])))
            check(f"{name}pd p_uda (made absolute by proc_creat)", *both16(sd(sym[f'{name}pd'] + sym['p_uda'])))
            check(f"{name}pd p_uda = its UDA segment", uda_seg, r16(port, sd(sym[f'{name}pd'] + sym['p_uda'])))
        for name in ('drl', 'thrdrt'):
            check(f"{name} (tick and idle created)", *both16(sd(sym[name])))
        for name in ('idlepd', 'tickpd'):
            links = lambda img: (r16(img, sd(sym[name] + sym['p_link'])),  # noqa: E731
                                 r16(img, sd(sym[name] + sym['p_thread'])))
            check(f"{name} p_link/p_thread", links(ref), links(port))

        # whole-image diff, outside the code image and the reference's
        # own stack (I86Sim's entry return sentinel, init's `push es`,
        # proc_creat's pushes -- all just below STACK_TOP)
        stack = set(range(0xFF00, SIM_MEM))
        diffs = [hex(a) for a in range(len(code), SIM_MEM)
                 if a not in stack and ref[a] != port[a]]
        check("no other bytes differ", [], diffs[:8])
        check("nothing written past 64KB", True, not any(port[SIM_MEM:]))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
