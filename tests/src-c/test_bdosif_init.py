#!/usr/bin/env python3
"""Differential test for src-c/kernel/bdosif.c's bdosif_init against
src/kernel/BDOSIF.BDO's `init`, on real memory layout.
(tests/src-c/test_bdosif.py covers `entry`.)

tests/src-c/fixtures/bdosif_init_wrapper.A86 assembles the whole real
BDOS module plus QUE1.RTM/QUE2.RTM, so the reference runs the real
`init` and the real `mpmif`. SYSDAT (DS) is at segment 300h, holding
the combined SYSDAT.FMT+DATA.BDO data segment's real assembled bytes
(DATA.BDO's mxdiskqd/mxdiskqpb at their real F92h/FAEh offsets, with
their real initial values). The running process's PD (rlr) is in
SYSDAT and its UDA is its p_uda.

`init` makes three `mpmif` calls (f_qmake/f_qopen/f_qwrite). BDOSIF's
`mpmif` sets ES = p_uda[rlr] and then does `callf cs:dword ptr
.supervisor` into SUPIF.SUP; that far call is modeled with
tools/i86sim.py's `far_call_hooks`. The hook applies SUPIF's own mpmif
protocol (u_wrkseg = caller's DS around the call, DS = SYSDAT) and runs
the real QUE1.RTM entry in a nested sim on the same memory. Any other
function (writeq's own f_wakeup) is the usual no-op boundary stub.
bdosif.c's direct calls into que1.c do the same u_wrkseg save/set/restore.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'bdosif_init_wrapper.A86')
BDOSIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f)
            for f in ('bdosif.c', 'file1.c', 'file2.c', 'file3.c', 'file4.c', 'que1.c', 'que2.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
SYSDAT = 0x0300
UDA = 0x0500
PD_RUN = 0x1100      # rlr (SYSDAT offset, past the 1000h-byte data segment)
OTHER_QD = 0x1200    # a queue already on qlr
CALLER_WRKSEG = 0x0777
ENDSEG = 0x0800
SUPMOD = (0x1234, 0x5678)

Q_LINK, Q_NAME, Q_MSGCNT, Q_BUF = 0, 6, 0x16, 0x1A
QPB_QADDR = 2
P_UDA = 0x10


def phys(seg, off=0):
    return seg * 16 + off


def sd(off):
    return phys(SYSDAT, off)


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    data = asm.segment_bytes('dseg')
    sym = {name: asm.symtab[name].value for name in
           ('init', 'entry', 'mpmif', 'sysdat', 'supervisor', 'supmod', 'rlr', 'qlr', 'endseg',
            'mxdiskqd', 'mxdiskqpb', 'u_wrkseg', 'makeq_entry', 'openq_entry', 'writeq_entry',
            'f_qmake', 'f_qopen', 'f_qwrite', 'f_wakeup')}
    assert (sym['mxdiskqd'], sym['mxdiskqpb']) == (0xF92, 0xFAE), "DATA.BDO layout moved"
    assert len(code) <= sd(0), "code must not overlap SYSDAT"
    assert len(data) <= PD_RUN, "PD must sit past the data segment"
    retf = [i for i in range(sym['init'], sym['entry']) if code[i] == 0xCB]
    assert len(retf) == 1, f"expected exactly 1 retf in init, found {retf}"
    # mpmif's `callf cs:dword ptr .supervisor`: 2E FF /3 with a disp16 operand
    callf = [i for i in range(sym['mpmif'], sym['mpmif'] + 0x20)
             if code[i] == 0xFF and (code[i + 1] >> 3) & 7 == 3]
    assert len(callf) == 1, f"expected one callf in mpmif, found {callf}"
    return code, data, sym, retf[0], callf[0]


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *BDOSIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def main():
    code, data, sym, retf_addr, callf_addr = assemble_reference()
    # the callf's own address: back up over the cs: prefix byte if present
    callf_insn = callf_addr - 1 if code[callf_addr - 1] == 0x2E else callf_addr
    skip = set(range(len(code))) | set(range(0xFE00, SIM_MEM))  # code, harness stacks (SS=0)
    targets = {sym['f_qmake']: 'makeq_entry', sym['f_qopen']: 'openq_entry',
               sym['f_qwrite']: 'writeq_entry'}
    calls = []

    def supif(sim):
        """SUPIF.SUP's entry -> mpmif -> func, for the queue functions."""
        target = targets.get(sim.regs['cx'])
        calls.append((sim.regs['cx'], sim.regs['dx']))
        if target is None:
            return  # f_wakeup etc.: boundary stub
        wrk = phys(sim.regs['es'], sym['u_wrkseg'])
        saved = r16(sim.mem, wrk)
        w16(sim.mem, wrk, sim.regs['ds'])
        inner = I86Sim(sim.mem, {'dx': sim.regs['dx'], 'ds': SYSDAT, 'es': sim.regs['es'],
                                 'ss': sim.regs['ss'], 'sp': sim.regs['sp'] - 0x80, 'cs': 0},
                       sym[target], segmented=True, far_call_hooks={callf_insn: supif})
        inner.run()
        w16(sim.mem, wrk, saved)
        sim.regs['bx'], sim.regs['cx'] = inner.regs['bx'], inner.regs['cx']

    def base_memory():
        mem = bytearray(SIM_MEM)
        mem[sd(0):sd(len(data))] = data
        w16(mem, sd(sym['supmod']), SUPMOD[0])
        w16(mem, sd(sym['supmod'] + 2), SUPMOD[1])
        w16(mem, sd(sym['endseg']), ENDSEG)
        w16(mem, sd(sym['rlr']), PD_RUN)
        w16(mem, sd(PD_RUN + P_UDA), UDA)
        w16(mem, phys(UDA, sym['u_wrkseg']), CALLER_WRKSEG)
        w16(mem, sd(sym['qlr']), OTHER_QD)
        mem[sd(OTHER_QD + Q_NAME):sd(OTHER_QD + Q_NAME + 8)] = b'OTHER   '
        return mem

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.bdosif_init.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        mem = base_memory()
        ref = bytearray(mem)
        ref[0:len(code)] = code
        sim = I86Sim(ref, {'ds': SYSDAT, 'es': 0x0999, 'ss': 0, 'sp': STACK_TOP, 'cs': 0},
                     sym['init'], segmented=True, stop_addrs={retf_addr},
                     far_call_hooks={callf_insn: supif})
        sim.run()
        check("init: mpmif calls (expected | reference)",
              [(hex(sym[f]), hex(dx)) for f, dx in (('f_qmake', sym['mxdiskqd']),
                                                   ('f_qopen', sym['mxdiskqpb']),
                                                   ('f_qwrite', sym['mxdiskqpb']))] + [hex(sym['f_wakeup'])],
              [(hex(c), hex(dx)) for c, dx in calls[:3]] + [hex(c) for c, _ in calls[3:]])

        buf = (ctypes.c_uint8 * PHYS_MEM)()
        ctypes.memmove(buf, bytes(mem), len(mem))
        lib.bdosif_init(buf, SYSDAT, 0)
        port = bytes(buf)

        cells = lambda img: (r16(img, sym['sysdat']), r16(img, sym['supervisor']),  # noqa: E731
                             r16(img, sym['supervisor'] + 2))
        check("init: cs:sysdat / cs:supervisor", cells(ref), cells(port))
        check("init: supervisor = supmod", cells(ref)[1:], SUPMOD)
        qd, qpb = sym['mxdiskqd'], sym['mxdiskqpb']
        state = lambda img: (r16(img, sd(sym['qlr'])), r16(img, sd(qd + Q_LINK)),  # noqa: E731
                             r16(img, sd(qpb + QPB_QADDR)), r16(img, sd(qd + Q_MSGCNT)),
                             r16(img, sd(qd + Q_BUF)), r16(img, phys(UDA, sym['u_wrkseg'])))
        check("init: qlr/q_link/qpb_qaddr/msgcnt/q_buf/u_wrkseg", state(ref), state(port))
        check("init: mxdiskqd made, opened, written once; u_wrkseg restored", state(port),
              (qd, OTHER_QD, qd, 1, 0, CALLER_WRKSEG))
        diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and ref[a] != port[a]]
        check("init: no other bytes differ", [], diffs[:8])
        check("init: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
