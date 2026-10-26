#!/usr/bin/env python3
"""Differential test for src-c/kernel/abort.c against src/kernel/
ABORT.RTM's sysreset_entry/terminate_entry/abort_spec_entry (+
internal helpers, including every abort_tab handler). Bundled with
src/kernel/QUE2.RTM (rlsmx) and src/kernel/FINDPD.RTM
(findpdname_entry), both called as ordinary internal calls, not
boundary stubs.

abort.c addresses real memory (defs.h's far_ptr), so the reference runs
with tools/i86sim.py's segmented=True: tests/src-c/fixtures/
abort_wrapper.A86 includes the real SYSDAT.FMT/UDA.FMT, SYSDAT is at
segment 100h (DS) with PDs, every process list, QDs, the flag and CCB
tables at SYSDAT-relative offsets, the running process's UDA is the
reference's ES and its PD's p_uda, the APB sits in that UDA's
u_wrkseg, and an aborted process's UDA is its own p_uda. Field offsets
come from the assembled symbol table, and every case diffs the whole
image to catch stray writes.

Needs tools/i86sim.py's `CALL r/m16`/`JMP r/m16` support (`call
cs:abort_tab[di]`). `jmp dsptch`/`jmp pdisp` are boundary tail-jumps
(`stop_addrs`).
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

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
MPMIF, DSPTCH, PDISP = 0x9000, 0x9001, 0x9002

SYSDAT = 0x0100      # DS; PDs/QDs/tables below are SYSDAT offsets
UDA_RUN = 0x0600     # running process's UDA (ES)
WRKSEG = 0x0700      # its u_wrkseg; the APB lives here
UDA_ABT = 0x0800     # aborted process's own UDA
APB_ADDR = 0x0010
CCB_TABLE = 0x3000   # contents of SYSDAT's `ccb` word
FLAG_TABLE = 0x3400  # contents of SYSDAT's `flags` word

PD_A, PD_B, PD_C, PARENT = 0x2000, 0x2100, 0x2200, 0x2300

FIELDS = ('p_link', 'p_thread', 'p_stat', 'p_prior', 'p_flag', 'p_name', 'p_uda',
          'p_cns', 'p_lst', 'p_wait', 'p_parent',
          'pf_keep', 'pf_tempkeep', 'pf_sys', 'pf_ctlc', 'pf_childabort',
          'apb_pd', 'apb_term', 'apb_cns', 'apb_pdname',
          'q_link', 'q_dq', 'q_nq', 'c_queue', 'ccblen', 'flg_pd', 'flg_ignore', 'flglen',
          'rlr', 'drl', 'dlr', 'plr', 'slr', 'qlr', 'thrdrt', 'ccb', 'nflags', 'flags',
          'mxmemowner', 'mxmemcount', 'u_wrkseg', 'u_error_mode', 'u_dparam',
          'u_sp', 'u_ss', 'u_in_int', 'ulen',
          'sysreset_entry', 'terminate_entry', 'abort_spec_entry', 'terminate', 'sysdat')


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
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in FIELDS}
    assert len(code) < sd(0), "code must not overlap SYSDAT"
    return code, sym


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *ABORT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def main():
    code, s = assemble_reference()
    skip = set(range(len(code))) | set(range(0xFF00, SIM_MEM))  # code, harness stack (SS=0)

    def pd(addr, field):
        return sd(addr + s[field])

    def base_memory(running=PD_B):
        mem = bytearray(SIM_MEM)
        w16(mem, sd(s['rlr']), running)
        for p in (PD_A, PD_B, PD_C, PARENT):
            w16(mem, pd(p, 'p_uda'), UDA_RUN if p == running else UDA_ABT)
        w16(mem, phys(UDA_RUN, s['u_wrkseg']), WRKSEG)
        w16(mem, sd(s['ccb']), CCB_TABLE)
        w16(mem, sd(s['flags']), FLAG_TABLE)
        mem[sd(s['nflags'])] = 4
        for i in range(4):
            w16(mem, sd(FLAG_TABLE + i * s['flglen'] + s['flg_pd']), 0xFFFF)
        w16(mem, sd(s['thrdrt']), PD_A)  # thread: A -> B -> C -> PARENT
        w16(mem, pd(PD_A, 'p_thread'), PD_B)
        w16(mem, pd(PD_B, 'p_thread'), PD_C)
        w16(mem, pd(PD_C, 'p_thread'), PARENT)
        return mem

    def put_apb(mem, pd_addr, term, cns, name=b''):
        base = phys(WRKSEG, APB_ADDR)
        w16(mem, base + s['apb_pd'], pd_addr)
        w16(mem, base + s['apb_term'], term)
        mem[base + s['apb_cns']] = cns
        mem[base + s['apb_pdname']:base + s['apb_pdname'] + 8] = (name + b'\x00' * 8)[:8]

    def run_reference(entry, mem, dx):
        image = bytearray(mem)
        image[0:len(code)] = code
        w16(image, s['sysdat'], SYSDAT)
        sim = I86Sim(image, {'dx': dx, 'ds': SYSDAT, 'es': UDA_RUN, 'ss': 0, 'sp': STACK_TOP, 'cs': 0},
                     s[entry], segmented=True, call_stub_addrs={MPMIF}, stop_addrs={DSPTCH, PDISP})
        sim.run()
        return sim.regs, image

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        u16 = ctypes.c_uint16

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.sysreset_entry.restype = u16
        lib.sysreset_entry.argtypes = [c_u8p, u16]
        lib.terminate_entry.restype = BxCx
        lib.terminate_entry.argtypes = [c_u8p, u16, u16]
        lib.abort_spec_entry.restype = BxCx
        lib.abort_spec_entry.argtypes = [c_u8p, u16, u16, u16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def run_case(label, entry, mem, dx=0, snap=lambda img: ()):
            ref_regs, ref = run_reference(entry, mem, dx)
            buf = (ctypes.c_uint8 * PHYS_MEM)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            if entry == 'sysreset_entry':
                port_r, ref_r = lib.sysreset_entry(buf, SYSDAT), ref_regs['bx']
            elif entry == 'terminate_entry':
                r = lib.terminate_entry(buf, SYSDAT, dx)
                port_r, ref_r = (r.bx, r.cx), (ref_regs['bx'], ref_regs['cx'])
            else:
                r = lib.abort_spec_entry(buf, SYSDAT, dx, s['terminate'])
                port_r, ref_r = (r.bx, r.cx), (ref_regs['bx'], ref_regs['cx'])
            port = bytes(buf)
            check(label, (ref_r, snap(ref)), (port_r, snap(port)))
            diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        # -- terminate_entry: success, reparents a child, propagates
        # pf_ctlc to the parent as pf_childabort --
        def term_memory():
            mem = base_memory(running=PD_A)
            w16(mem, pd(PD_C, 'p_parent'), PD_A)
            w16(mem, pd(PD_B, 'p_parent'), PARENT)
            w16(mem, pd(PD_A, 'p_flag'), s['pf_ctlc'])
            w16(mem, pd(PD_A, 'p_parent'), PARENT)
            return mem

        term_snap = lambda img: (r16(img, pd(PD_C, 'p_parent')), r16(img, pd(PD_B, 'p_parent')),  # noqa: E731
                                 r16(img, pd(PD_A, 'p_flag')), r16(img, pd(PARENT, 'p_flag')),
                                 r16(img, sd(s['mxmemowner'])), img[sd(s['mxmemcount'])],
                                 img[pd(PD_A, 'p_stat')], img[phys(UDA_RUN, s['u_error_mode'])])
        run_case("terminate_entry: success, reparents child, flags parent", 'terminate_entry',
                 term_memory(), 0, term_snap)
        run_case("sysreset_entry: terminates the caller", 'sysreset_entry', term_memory(), 0, term_snap)

        mem = base_memory(running=PD_A)
        w16(mem, pd(PD_A, 'p_flag'), s['pf_keep'] | s['pf_ctlc'])
        run_case("terminate_entry: pf_keep -> e_pd_noterm, pf_ctlc cleared", 'terminate_entry', mem, 0,
                 lambda img: r16(img, pd(PD_A, 'p_flag')))

        mem = base_memory(running=PD_A)
        w16(mem, pd(PD_A, 'p_flag'), s['pf_tempkeep'])
        run_case("terminate_entry: pf_tempkeep -> deferred, pf_ctlc set", 'terminate_entry', mem, 0,
                 lambda img: r16(img, pd(PD_A, 'p_flag')))

        mem = base_memory(running=PD_A)
        w16(mem, pd(PD_A, 'p_flag'), s['pf_sys'])
        run_case("terminate_entry: pf_sys, code 0 -> e_pd_noterm", 'terminate_entry', mem, 0)
        run_case("terminate_entry: pf_sys, code FFh -> terminates", 'terminate_entry', mem, 0x00FF,
                 lambda img: img[pd(PD_A, 'p_stat')])

        # -- abort_spec_entry --
        uda_snap = lambda img: (r16(img, sd(s['drl'])), r16(img, pd(PD_A, 'p_link')),  # noqa: E731
                                img[pd(PD_A, 'p_prior')], r16(img, pd(PD_A, 'p_flag')),
                                r16(img, phys(UDA_ABT, s['u_sp'])), r16(img, phys(UDA_ABT, s['u_ss'])),
                                img[phys(UDA_ABT, s['u_in_int'])],
                                r16(img, phys(UDA_ABT, s['ulen'] - 2)),
                                r16(img, phys(UDA_ABT, s['u_dparam'])))

        # abtrun: PD_A is next on the ready list after the running PD_B
        mem = base_memory()
        w16(mem, pd(PD_B, 'p_link'), PD_A)
        w16(mem, pd(PD_A, 'p_link'), PD_C)
        w16(mem, pd(PD_A, 'p_flag'), s['pf_ctlc'])
        w16(mem, sd(s['drl']), 0x2700)
        put_apb(mem, PD_A, 0x1234, 5)
        run_case("abort_spec_entry: ready list (abtrun)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (uda_snap(img), r16(img, pd(PD_B, 'p_link'))))

        # abtrun falls back to drl
        mem = base_memory()
        w16(mem, sd(s['drl']), PD_C)
        w16(mem, pd(PD_C, 'p_link'), PD_A)
        put_apb(mem, PD_A, 0x0055, 0)
        run_case("abort_spec_entry: dispatcher ready list (abtrun -> drl)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (uda_snap(img), r16(img, pd(PD_C, 'p_link'))))

        # not on any list for its status -> abt_tab_err
        mem = base_memory()
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: not on its status list -> e_no_pdname", 'abort_spec_entry', mem, APB_ADDR)

        # not on the thread list at all
        mem = base_memory()
        put_apb(mem, 0x2777, 0, 0)
        run_case("abort_spec_entry: PD not on thread -> e_no_pdname", 'abort_spec_entry', mem, APB_ADDR)

        # aborting self delegates to terminate_entry
        mem = base_memory(running=PD_A)
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: self-abort delegates to terminate_entry", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: img[pd(PD_A, 'p_stat')])

        # keep -> e_pd_noterm
        mem = base_memory()
        w16(mem, pd(PD_A, 'p_flag'), s['pf_keep'])
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: pf_keep -> e_pd_noterm", 'abort_spec_entry', mem, APB_ADDR)

        # tempkeep -> deferred
        mem = base_memory()
        w16(mem, pd(PD_A, 'p_flag'), s['pf_tempkeep'])
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: pf_tempkeep -> deferred", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: r16(img, pd(PD_A, 'p_flag')))

        # by name + console, on the poll list (abtpol)
        mem = base_memory()
        for p, name, cns in ((PD_A, b'ECHO', 1), (PD_C, b'ECHO', 3)):
            mem[pd(p, 'p_name'):pd(p, 'p_name') + 8] = (name + b'\x00' * 8)[:8]
            mem[pd(p, 'p_cns')] = cns
        w16(mem, pd(PD_C, 'p_uda'), UDA_ABT)
        mem[pd(PD_C, 'p_stat')] = 1  # ps_poll
        w16(mem, sd(s['plr']), PD_C)
        put_apb(mem, 0, 0x0042, 3, b'ECHO')
        run_case("abort_spec_entry: by name+console, poll list (abtpol)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (r16(img, sd(s['plr'])), r16(img, sd(s['drl'])), r16(img, phys(UDA_ABT, s['u_dparam']))))

        # name matches but console doesn't
        mem = base_memory()
        mem[pd(PD_A, 'p_name'):pd(PD_A, 'p_name') + 8] = b'ECHO\x00\x00\x00\x00'
        mem[pd(PD_A, 'p_cns')] = 1
        put_apb(mem, 0, 0, 2, b'ECHO')
        run_case("abort_spec_entry: name ok, console mismatch -> e_no_pdname", 'abort_spec_entry', mem, APB_ADDR)

        # abtdly: fixes up the next PD's own p_wait
        mem = base_memory()
        w16(mem, sd(s['dlr']), PD_C)
        w16(mem, pd(PD_C, 'p_link'), PD_A)
        w16(mem, pd(PD_A, 'p_link'), PARENT)
        w16(mem, pd(PD_A, 'p_wait'), 5)
        w16(mem, pd(PARENT, 'p_wait'), 10)
        mem[pd(PD_A, 'p_stat')] = 2  # ps_delay
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: delay list (abtdly)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (r16(img, pd(PD_C, 'p_link')), r16(img, pd(PARENT, 'p_wait'))))

        # abtswp
        mem = base_memory()
        w16(mem, sd(s['slr']), PD_A)
        mem[pd(PD_A, 'p_stat')] = 3  # ps_swap
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: swap list (abtswp)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: r16(img, sd(s['slr'])))

        # abtdq / abtnq: on a QD's DQ / NQ list (second QD on qlr)
        for stat, root, label in ((6, 'q_dq', 'abtdq'), (7, 'q_nq', 'abtnq')):
            mem = base_memory()
            QD1, QD2 = 0x2800, 0x2900
            w16(mem, sd(s['qlr']), QD1)
            w16(mem, sd(QD1 + s['q_link']), QD2)
            w16(mem, sd(QD2 + s[root]), PD_C)
            w16(mem, pd(PD_C, 'p_link'), PD_A)
            mem[pd(PD_A, 'p_stat')] = stat
            put_apb(mem, PD_A, 0, 0)
            run_case(f"abort_spec_entry: queue list ({label})", 'abort_spec_entry', mem, APB_ADDR,
                     lambda img: r16(img, pd(PD_C, 'p_link')))

        # abtflg: waiting on flag 2
        mem = base_memory()
        f2 = sd(FLAG_TABLE + 2 * s['flglen'])
        w16(mem, f2 + s['flg_pd'], PD_A)
        mem[f2 + s['flg_ignore']] = 7
        mem[pd(PD_A, 'p_stat')] = 8  # ps_flagwait
        put_apb(mem, PD_A, 0, 0)
        run_case("abort_spec_entry: flag wait (abtflg)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (r16(img, f2 + s['flg_pd']), img[f2 + s['flg_ignore']]))

        # abtcns: waiting on console 1's c_queue -- gccba must index the
        # table `ccb` *points to* (CCB_TABLE), not the `ccb` word's address
        def ccb_queue(n):
            return sd(CCB_TABLE + n * s['ccblen'] + s['c_queue'])

        mem = base_memory()
        mem[pd(PD_A, 'p_stat')] = 9  # ps_ciowait
        mem[pd(PD_A, 'p_cns')] = 1
        mem[pd(PD_A, 'p_lst')] = 2
        w16(mem, ccb_queue(1), PD_C)
        w16(mem, pd(PD_C, 'p_link'), PD_A)
        put_apb(mem, PD_A, 0, 1)
        run_case("abort_spec_entry: console wait (abtcns)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (r16(img, ccb_queue(1)), r16(img, pd(PD_C, 'p_link'))))

        # abtcns -> abtlst: not on the console's queue, on list device 2's
        mem = base_memory()
        mem[pd(PD_A, 'p_stat')] = 9
        mem[pd(PD_A, 'p_cns')] = 1
        mem[pd(PD_A, 'p_lst')] = 2
        w16(mem, ccb_queue(1), PD_C)
        w16(mem, ccb_queue(2), PD_A)
        w16(mem, pd(PD_A, 'p_link'), PARENT)
        put_apb(mem, PD_A, 0, 1)
        run_case("abort_spec_entry: list-device wait (abtcns -> abtlst)", 'abort_spec_entry', mem, APB_ADDR,
                 lambda img: (r16(img, ccb_queue(1)), r16(img, ccb_queue(2))))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
