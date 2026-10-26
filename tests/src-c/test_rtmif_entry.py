#!/usr/bin/env python3
"""Differential test for src-c/kernel/rtmif.c's own `entry` (RTMIF.RTM's
inter-module RTM dispatch) against the real RTM module, on real memory
layout. See rtmif.c's own header comment for the register-convention
mapping this test checks.

The reference is src/kernel/RTM.A86 itself -- the whole real module
(RTMIF/SYSENT/PROC/DSPTCH/FLAG/QUE1/QUE2/FINDPD/ABORT + the real
UDA.FMT/SYSDAT.FMT), so every field offset and code address is the
genuine one, including ABORT.RTM's `terminate:` (rtmif.c's
RTMIF_ABORT_TERMINATE_OFFSET, asserted below). It runs with
tools/i86sim.py's segmented=True: DS = SYSDAT (segment 100h), ES = the
running process's UDA, with PDs/QDs/flag table/lists SYSDAT-relative and
QPBs/names/APBs in that UDA's u_wrkseg -- the same environment SUPIF's
mpmif sets up before calling into the RTM.

`mpmif`/`xiosif` and the dispatcher (`dsptch`/`pdisp`) are boundary
stubs at their real addresses (call and jmp alike): this test verifies
`entry`'s own dispatch wiring into each real target, whose full
internal behavior is covered by test_sysent.py/test_proc.py/
test_dsptch.py/test_flag.py/test_que.py/test_findpd.py/test_abort.py.
`proc_creat` is stubbed too, which leaves case 14 (creat_proc_entry,
`call proc_creat ! jmp pdisp`) dispatch-only: rtmif.c's side runs the
real proc.c/dsptch.c, which a stubbed reference can't match.

readq/creadq/writeq/cwriteq (cases 7-10) would retry forever under a
no-op `mpmif` with an empty/full queue (see que1.c's own header
comment), so each gets a queue that satisfies it on the first pass, or
is the conditional variant that returns immediately.

Every case diffs the whole image (except code and the harness stack).
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

RTM_A86 = os.path.join(REPO_ROOT, 'src', 'kernel', 'RTM.A86')
RTMIF_ENTRY_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f)
                 for f in ('rtmif.c', 'sysent.c', 'proc.c', 'dsptch.c', 'flag.c',
                           'que1.c', 'que2.c', 'findpd.c', 'abort.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
RTMIF_ABORT_TERMINATE_OFFSET = 0x0C68  # rtmif.c's #define

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
SYSDAT = 0x0100
UDA_RUN = 0x0500
UDA_ABT = 0x0600
WRKSEG = 0x0700
ENDSEG = 0x0F00

PD_A, PD_B = 0x2000, 0x2100        # SYSDAT offsets; PD_A is running
QD_A, QD_B = 0x2400, 0x2440
FLAG_TABLE, SYSLIST = 0x2600, 0x2700
QPB, NAME, APB, QD_SRC = 0x0100, 0x0200, 0x0300, 0x0400   # u_wrkseg offsets
FLAG_OFF, FLAG_ON = 0xFFFF, 0xFFFE

SYMS = ('entry', 'rtm_ret', 'mpmif', 'xiosif', 'dsptch', 'pdisp', 'proc_creat', 'terminate',
        'sysdat', 'rlr', 'drl', 'qlr', 'qul', 'endseg', 'thrdrt', 'nflags', 'flags', 'indisp',
        'u_wrkseg', 'u_dparam', 'u_retseg',
        'p_link', 'p_thread', 'p_stat', 'p_prior', 'p_name', 'p_uda', 'p_scratch',
        'q_link', 'q_name', 'q_msglen', 'q_nmsgs', 'q_msgcnt', 'q_buf',
        'qpb_qaddr', 'qpb_buffptr', 'qpb_name', 'apb_pd', 'apb_term')


def phys(seg, off=0):
    return seg * 16 + off


def sd(off):
    return phys(SYSDAT, off)


def wk(off):
    return phys(WRKSEG, off)


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(RTM_A86, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    s = {name: asm.symtab[name].value for name in SYMS}
    assert code[s['rtm_ret']] == 0xCB, f"expected retf at {hex(s['rtm_ret'])}"
    assert s['sysdat'] == 6
    assert s['terminate'] == RTMIF_ABORT_TERMINATE_OFFSET, \
        f"re-probe rtmif.c's RTMIF_ABORT_TERMINATE_OFFSET: terminate is now {s['terminate']:#x}"
    assert len(code) < sd(0), "code must not overlap SYSDAT"
    return code, s


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *RTMIF_ENTRY_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def main():
    code, s = assemble_reference()
    skip = set(range(len(code))) | set(range(0xFF00, SIM_MEM))  # code, harness stack (SS=0)
    stubs = {s['mpmif'], s['xiosif'], s['dsptch'], s['pdisp'], s['proc_creat']}

    def pd(addr, field):
        return sd(addr + s[field])

    def base_memory():
        mem = bytearray(SIM_MEM)
        w16(mem, sd(s['rlr']), PD_A)
        w16(mem, pd(PD_A, 'p_uda'), UDA_RUN)
        w16(mem, pd(PD_B, 'p_uda'), UDA_ABT)
        w16(mem, phys(UDA_RUN, s['u_wrkseg']), WRKSEG)
        w16(mem, sd(s['endseg']), ENDSEG)
        w16(mem, sd(s['thrdrt']), PD_A)            # thread: A -> B
        w16(mem, pd(PD_A, 'p_thread'), PD_B)
        mem[pd(PD_A, 'p_name'):pd(PD_A, 'p_name') + 8] = b'RUNNER  '
        mem[pd(PD_B, 'p_name'):pd(PD_B, 'p_name') + 8] = b'ECHO    '
        return mem

    def run_reference(mem, regs):
        image = bytearray(mem)
        image[0:len(code)] = code
        w16(image, s['sysdat'], SYSDAT)
        sim = I86Sim(image, {**regs, 'ds': SYSDAT, 'es': UDA_RUN, 'ss': 0, 'sp': STACK_TOP, 'cs': 0},
                     s['entry'], segmented=True, stop_addrs={s['rtm_ret']},
                     call_stub_addrs=stubs, jmp_stub_addrs=stubs)
        sim.run(max_steps=20000)
        return sim.regs, image

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        u16 = ctypes.c_uint16

        class EntryResult(ctypes.Structure):
            _fields_ = [(r, ctypes.c_uint16) for r in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp')]

        lib.rtmif_entry.restype = EntryResult
        lib.rtmif_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8] + [u16] * 9 + \
            [ctypes.POINTER(ctypes.c_uint8)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def run_case(label, cl, mem, dx=0, bx=0, fields=('bx', 'cx'), snap=lambda img: (),
                     dispatch_only=False):
            regs = {'cx': cl, 'dx': dx, 'ax': 0x1111, 'bx': bx, 'si': 0x2222, 'di': 0x3333, 'bp': 0x4444}
            ref_regs, ref = run_reference(mem, regs)
            buf = (ctypes.c_uint8 * PHYS_MEM)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            was_indisp = ctypes.c_uint8(0)
            r = lib.rtmif_entry(buf, cl, dx, regs['ax'], bx, cl, regs['si'], regs['di'], regs['bp'],
                                SYSDAT, SYSDAT, ctypes.byref(was_indisp))
            port = bytes(buf)
            if dispatch_only:
                print(f"[--] {label}: dispatched (not compared, see header)")
                return
            check(label, (tuple(ref_regs[f] for f in fields), snap(ref)),
                  (tuple(getattr(r, f) for f in fields), snap(port)))
            diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        uda = lambda f: phys(UDA_RUN, s[f])  # noqa: E731

        # 0: sysreset_entry -- terminates the running PD
        run_case("0 sysreset_entry", 0, base_memory(), fields=('bx',),
                 snap=lambda img: img[pd(PD_A, 'p_stat')])

        # 1: poll_entry
        run_case("1 poll_entry", 1, base_memory(), dx=7, fields=(),
                 snap=lambda img: (img[pd(PD_A, 'p_stat')], r16(img, uda('u_dparam'))))

        # 2/3: flag_wait_entry/flag_set_entry -- one flag starting `val`
        def flag_memory(val):
            mem = base_memory()
            mem[sd(s['nflags'])] = 1
            w16(mem, sd(s['flags']), FLAG_TABLE)
            w16(mem, sd(FLAG_TABLE), val)
            mem[sd(FLAG_TABLE + 2)] = 0xFF
            return mem

        flag = lambda img: r16(img, sd(FLAG_TABLE))  # noqa: E731
        run_case("2 flag_wait_entry (bad flag#)", 2, flag_memory(FLAG_OFF), dx=5)
        run_case("2 flag_wait_entry (on -> consumed)", 2, flag_memory(FLAG_ON), dx=0, snap=flag)
        run_case("3 flag_set_entry (bad flag#)", 3, flag_memory(FLAG_OFF), dx=5)
        run_case("3 flag_set_entry (off -> on)", 3, flag_memory(FLAG_OFF), dx=0, snap=flag)

        def queue_memory(msgcnt=0, nmsgs=4, msglen=0, name=b'MXQ'):
            mem = base_memory()
            w16(mem, sd(s['qlr']), QD_A)
            w16(mem, sd(QD_A + s['q_link']), QD_B)
            mem[sd(QD_A + s['q_name']):sd(QD_A + s['q_name']) + 8] = (name + b' ' * 8)[:8]
            w16(mem, sd(QD_A + s['q_msglen']), msglen)
            w16(mem, sd(QD_A + s['q_nmsgs']), nmsgs)
            w16(mem, sd(QD_A + s['q_msgcnt']), msgcnt)
            w16(mem, sd(QD_A + s['q_buf']), 0x2800)
            w16(mem, wk(QPB + s['qpb_qaddr']), QD_A)
            w16(mem, wk(QPB + s['qpb_buffptr']), 0x0500)
            return mem

        # 4: makeq_entry -- QD in u_wrkseg, inside MPM range
        for label, name in (("4 makeq_entry (success)", b'NEWQ'), ("4 makeq_entry (name in use)", b'MXQ')):
            mem = queue_memory()
            mem[wk(QD_SRC + s['q_name']):wk(QD_SRC + s['q_name']) + 8] = (name + b' ' * 8)[:8]
            run_case(label, 4, mem, dx=QD_SRC, snap=lambda img: r16(img, sd(s['qlr'])))

        # 5: openq_entry
        for label, name in (("5 openq_entry (found)", b'MXQ'), ("5 openq_entry (not found)", b'NOPE')):
            mem = queue_memory()
            w16(mem, wk(QPB + s['qpb_qaddr']), 0)
            mem[wk(QPB + s['qpb_name']):wk(QPB + s['qpb_name']) + 8] = (name + b' ' * 8)[:8]
            run_case(label, 5, mem, dx=QPB, snap=lambda img: r16(img, wk(QPB + s['qpb_qaddr'])))

        # 6: deleteq_entry
        run_case("6 deleteq_entry (success)", 6, queue_memory(), dx=QPB,
                 snap=lambda img: r16(img, sd(s['qlr'])))
        mem = queue_memory()
        w16(mem, wk(QPB + s['qpb_qaddr']), 0x2480)  # not on qlr
        run_case("6 deleteq_entry (not active)", 6, mem, dx=QPB)

        # 7-10: read/write
        mem = queue_memory(msgcnt=1, msglen=4)
        mem[sd(0x2800):sd(0x2804)] = b'\x01\x02\x03\x04'
        run_case("7 readq_entry (message ready)", 7, mem, dx=QPB,
                 snap=lambda img: (bytes(img[wk(0x500):wk(0x504)]), r16(img, sd(QD_A + s['q_msgcnt']))))
        run_case("8 creadq_entry (empty, conditional)", 8, queue_memory(msgcnt=0), dx=QPB)
        mem = queue_memory(msgcnt=0, msglen=4)
        mem[wk(0x500):wk(0x504)] = b'\xAA\xBB\xCC\xDD'
        run_case("9 writeq_entry (room available)", 9, mem, dx=QPB,
                 snap=lambda img: (bytes(img[sd(0x2800):sd(0x2804)]), r16(img, sd(QD_A + s['q_msgcnt']))))
        run_case("10 cwriteq_entry (full, conditional)", 10, queue_memory(msgcnt=4), dx=QPB)

        # 11: delay_entry
        run_case("11 delay_entry", 11, base_memory(), dx=100, fields=(),
                 snap=lambda img: (img[pd(PD_A, 'p_stat')], r16(img, uda('u_dparam'))))

        # 12: dispatch_entry -- jmp pdisp, boundary stub both sides
        run_case("12 dispatch_entry", 12, base_memory(), fields=())

        # 13: terminate_entry
        run_case("13 terminate_entry", 13, base_memory(), dx=0,
                 snap=lambda img: img[pd(PD_A, 'p_stat')])

        # 14: creat_proc_entry -- dispatch only (see header)
        run_case("14 creat_proc_entry", 14, base_memory(), dx=0, dispatch_only=True)

        # 15: set_prior_entry
        run_case("15 set_prior_entry", 15, base_memory(), dx=7, fields=(),
                 snap=lambda img: img[pd(PD_A, 'p_prior')])

        # 16: pd_entry -- u_retseg = caller's DS (SYSDAT, as SUPIF leaves it)
        run_case("16 pd_entry", 16, base_memory(),
                 snap=lambda img: r16(img, uda('u_retseg')))

        # 17: abort_spec_entry -- PD_B on the dispatcher ready list, by
        # address; and pd=0 with a name nobody has
        mem = base_memory()
        w16(mem, sd(s['drl']), PD_B)
        w16(mem, wk(APB + s['apb_pd']), PD_B)
        w16(mem, wk(APB + s['apb_term']), 0x0077)
        run_case("17 abort_spec_entry (success, real terminate offset)", 17, mem, dx=APB,
                 snap=lambda img: (r16(img, sd(s['drl'])), r16(img, phys(UDA_ABT, 0xFE))))
        mem = base_memory()
        mem[wk(APB + 6):wk(APB + 14)] = b'NOBODY\x00\x00'
        run_case("17 abort_spec_entry (pd=0, no such name)", 17, mem, dx=APB)

        # 18: sleep_entry
        mem = base_memory()
        mem[pd(PD_A, 'p_stat')] = 3  # prior status, copied to p_scratch
        run_case("18 sleep_entry", 18, mem, dx=0x99, fields=(),
                 snap=lambda img: (img[pd(PD_A, 'p_stat')], img[pd(PD_A, 'p_scratch')],
                                   r16(img, uda('u_dparam'))))

        # 19: wakeup_entry -- empty list, and one sleeper moved to drl
        run_case("19 wakeup_entry (empty list)", 19, base_memory(), dx=SYSLIST, fields=())
        mem = base_memory()
        w16(mem, sd(SYSLIST), PD_B)
        w16(mem, sd(s['drl']), 0x2999)
        run_case("19 wakeup_entry (one sleeper)", 19, mem, dx=SYSLIST, fields=(),
                 snap=lambda img: (r16(img, sd(SYSLIST)), r16(img, sd(s['drl'])),
                                   r16(img, pd(PD_B, 'p_link'))))

        # 20: findpdname_entry -- name in u_wrkseg, bx = thread root - p_thread
        root = s['thrdrt'] - s['p_thread']
        for label, name in (("20 findpdname_entry (found)", b'ECHO    '),
                            ("20 findpdname_entry (not found)", b'NOBODY  ')):
            mem = base_memory()
            mem[wk(NAME):wk(NAME) + 8] = name
            run_case(label, 20, mem, dx=NAME, bx=root)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
