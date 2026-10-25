#!/usr/bin/env python3
"""Differential test for src-c/kernel/rtmif.c's own `entry` (RTMIF.RTM's
inter-module RTM dispatch) against the real src/kernel/RTMIF.RTM,
SYSENT.RTM, PROC.RTM, DSPTCH.RTM, FLAG.RTM, QUE1.RTM, QUE2.RTM,
FINDPD.RTM, and ABORT.RTM combined exactly the way src/kernel/RTM.A86
itself does. See rtmif.c's own header comment for the register-
convention mapping this test checks, and
tests/src-c/fixtures/rtmif_entry_wrapper.A86's own header comment for
why every target's own internal `mpmif` calls are plain no-ops here
(same convention test_cioif_entry.py's/test_memif_entry.py's own
wrappers use) -- this test verifies `entry`'s own dispatch wiring, not
each target's full internal behavior, already covered by
test_sysent.py/test_proc.py/test_dsptch.py/test_flag.py/test_que.py/
test_findpd.py/test_abort.py.

`readq_entry`/`creadq_entry`/`writeq_entry`/`cwriteq_entry` (cases
7-10) each have a real infinite-loop risk under a no-op `mpmif`: with
an empty/full queue they retry forever waiting for a wakeup that can
never come (see que1.c's own header comment, and
tests/src-c/fixtures/que_wrapper.A86's own deferral of exactly these
four for the same reason). This test avoids the loop by giving each
one a queue that already satisfies it on the first pass (one buffered
0-length message already queued, or room for one more) -- the
retry path itself stays untested here, same scope test_que.py already
settled on.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'rtmif_entry_wrapper.A86')
RTMIF_ENTRY_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f)
                  for f in ('rtmif.c', 'sysent.c', 'proc.c', 'dsptch.c', 'flag.c',
                            'que1.c', 'que2.c', 'findpd.c', 'abort.c')]
# proc.c/dsptch.c are linked (rtmif.c's own entry() calls into
# creat_proc_entry, which calls real proc_creat/pdisp) even though the
# REFERENCE side's own wrapper stubs dsptch/pdisp/proc_creat as plain
# addresses (see rtmif_entry_wrapper.A86's own header comment for why
# that asymmetry is unavoidable here) -- case 14 below is marked
# check_result=False for exactly that reason.
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, SD_DRL, SD_DLR = 0xA000, 0xA002, 0xA004
SD_NFLAGS, SD_FLAGS = 0xA006, 0xA008
UDA_DPARAM, UDA_WRKSEG, UDA_RETSEG = 0xA010, 0xA012, 0xA014
SD_PLR, SD_SLR = 0xA01C, 0xA01E
SD_QLR, SD_QUL = 0xA020, 0xA022
SD_ENDSEG, SD_CCB, SD_THRDRT = 0xA026, 0xA030, 0xA032
SD_PUL, SD_MXMEMOWNER, SD_MXMEMCOUNT = 0xA038, 0xA03A, 0xA03C
SD_INDISP = 0xAC00

P_LINK, P_THREAD, P_STAT, P_PRIOR, P_FLAG, P_NAME, P_UDA, P_SCRATCH = 0, 2, 4, 5, 6, 8, 0x10, 0x2E
Q_LINK, Q_FLAGS, Q_MSGLEN, Q_NMSGS, Q_MSGCNT, Q_MSGOUT, Q_BUF = 0, 4, 0x0E, 0x10, 0x16, 0x18, 0x1A
QPB_QADDR, QPB_BUFFPTR = 2, 6
PD_A = 0x5000
QD_A = 0x6000
QPB_A = 0x7000


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    entry = asm.symtab['entry'].value
    mpmif = asm.symtab['mpmif'].value
    xiosif = asm.symtab['xiosif'].value
    rtm_ret = asm.symtab['rtm_ret'].value
    assert code[rtm_ret] == 0xCB, f"expected retf at {hex(rtm_ret)}"
    return code, dseg, entry, rtm_ret, mpmif, xiosif


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *RTMIF_ENTRY_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, dseg, entry, stop_addr, mpmif, xiosif, mem, regs, max_steps=20000):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    image[len(code):len(dseg)] = dseg[len(code):]
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # dsptch/pdisp/proc_creat (0x9000/0x9001/0x9002, see
    # rtmif_entry_wrapper.A86's own header comment) are plain-EQU stub
    # addresses, not real labels -- stubbed the same way mpmif/xiosif
    # are, so a real `jmp dsptch`/`call proc_creat` doesn't try to
    # execute whatever garbage happens to sit at an unmapped address.
    stubs = {mpmif, xiosif, 0x9000, 0x9001, 0x9002}
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry,
                 stop_addrs={stop_addr}, call_stub_addrs=stubs,
                 jmp_stub_addrs=stubs)
    sim.run(max_steps=max_steps)
    return sim.regs, image


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD_A)
    return mem


def setup_qd(mem, msgcnt, nmsgs, flags=0):
    mem_w16(mem, SD_QLR, QD_A)
    mem_w16(mem, QD_A + Q_LINK, 0)
    mem_w16(mem, QD_A + Q_FLAGS, flags)
    mem_w16(mem, QD_A + Q_MSGLEN, 0)  # 0-length messages -- skip the byte-copy loop
    mem_w16(mem, QD_A + Q_NMSGS, nmsgs)
    mem_w16(mem, QD_A + Q_MSGCNT, msgcnt)
    mem_w16(mem, QD_A + Q_MSGOUT, 0)
    mem_w16(mem, QPB_A + QPB_QADDR, QD_A)
    mem_w16(mem, QPB_A + QPB_BUFFPTR, 0)


def main():
    code, dseg, entry, stop_addr, mpmif, xiosif = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class EntryResult(ctypes.Structure):
            _fields_ = [('ax', ctypes.c_uint16), ('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16),
                        ('dx', ctypes.c_uint16), ('si', ctypes.c_uint16), ('di', ctypes.c_uint16),
                        ('bp', ctypes.c_uint16)]

        lib.entry.restype = EntryResult
        lib.entry.argtypes = [c_u8p, ctypes.c_uint8, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16,
                               ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16,
                               ctypes.c_uint16, ctypes.c_uint16, c_u8p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def run_case(label, cl, mem, dx=0, ax=0, bx=0, cx=0, si=0, di=0, bp=0, ds=0, sysdat=0,
                     fields=('bx', 'cx'), check_result=True):
            ref_regs, ref_img = run_reference(code, dseg, entry, stop_addr, mpmif, xiosif, mem,
                                               {'cx': cl, 'dx': dx, 'ax': ax, 'bx': bx,
                                                'si': si, 'di': di, 'bp': bp})
            buf = to_ctypes(mem)
            was_indisp = ctypes.c_uint8(0)
            r = lib.entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(cl), ctypes.c_uint16(dx),
                           ctypes.c_uint16(ax), ctypes.c_uint16(bx), ctypes.c_uint16(cx),
                           ctypes.c_uint16(si), ctypes.c_uint16(di), ctypes.c_uint16(bp),
                           ctypes.c_uint16(ds), ctypes.c_uint16(sysdat), ctypes.byref(was_indisp))
            if check_result:
                ref = tuple(ref_regs[f] & 0xFFFF for f in fields)
                port = tuple(getattr(r, f) for f in fields)
                check(label, ref, port)
            return ref_img, bytes(buf)

        # 0: sysreset_entry
        mem = base_memory()
        run_case("0 sysreset_entry", 0, mem, fields=('bx',))

        # 1: poll_entry
        mem = base_memory()
        run_case("1 poll_entry", 1, mem, dx=0, fields=())

        # 2: flag_wait_entry -- bad flag number -> deterministic error
        mem = base_memory()
        mem[SD_NFLAGS] = 1
        mem_w16(mem, SD_FLAGS, 0x4000)
        run_case("2 flag_wait_entry (bad flag#)", 2, mem, dx=5)

        # 3: flag_set_entry -- same bad flag number
        mem = base_memory()
        mem[SD_NFLAGS] = 1
        mem_w16(mem, SD_FLAGS, 0x4000)
        run_case("3 flag_set_entry (bad flag#)", 3, mem, dx=5)

        # 4: makeq_entry -- empty unused-QD list -> e_no_qd
        mem = base_memory()
        run_case("4 makeq_entry (no unused QD)", 4, mem, dx=QD_A)

        # 5: openq_entry -- queue not found
        mem = base_memory()
        mem_w16(mem, QPB_A + QPB_QADDR, QD_A)
        run_case("5 openq_entry (not found)", 5, mem, dx=QPB_A)

        # 6: deleteq_entry -- queue not found. dx is a QPB (qpb_qaddr
        # -> the QD to delete), not the QD address directly -- an
        # earlier version of this case passed dx=QD_A raw, which
        # made `di` (read from a QPB that was never set up) land on
        # 0, reading dq/nq from real code bytes near address 0 instead
        # of a real QD's own fields, a test bug (not a dispatch bug)
        # that happened to produce a different, but still genuinely
        # matching-in-kind, error code on each side.
        mem = base_memory()
        mem_w16(mem, QPB_A + QPB_QADDR, QD_A)
        run_case("6 deleteq_entry (not found)", 6, mem, dx=QPB_A)

        # 7: readq_entry -- one 0-length message already queued
        mem = base_memory()
        setup_qd(mem, msgcnt=1, nmsgs=4)
        run_case("7 readq_entry (message ready)", 7, mem, dx=QPB_A)

        # 8: creadq_entry -- empty queue, conditional -> e_q_empty (no loop risk either way)
        mem = base_memory()
        setup_qd(mem, msgcnt=0, nmsgs=4)
        run_case("8 creadq_entry (empty, conditional)", 8, mem, dx=QPB_A)

        # 9: writeq_entry -- room for one more message
        mem = base_memory()
        setup_qd(mem, msgcnt=0, nmsgs=4)
        run_case("9 writeq_entry (room available)", 9, mem, dx=QPB_A)

        # 10: cwriteq_entry -- full queue, conditional -> e_q_full
        mem = base_memory()
        setup_qd(mem, msgcnt=4, nmsgs=4)
        run_case("10 cwriteq_entry (full, conditional)", 10, mem, dx=QPB_A)

        # 11: delay_entry
        mem = base_memory()
        run_case("11 delay_entry", 11, mem, dx=100, fields=())

        # 12: dispatch_entry -- no-op boundary stub both sides
        mem = base_memory()
        run_case("12 dispatch_entry", 12, mem, fields=())

        # 13: terminate_entry
        mem = base_memory()
        run_case("13 terminate_entry", 13, mem, dx=0)

        # 14: creat_proc_entry -- not independently verified here:
        # the C port's own creat_proc_entry calls the REAL proc_creat/
        # pdisp (proc.c/dsptch.c), but the reference side's own wrapper
        # stubs `proc_creat`/`pdisp` as plain addresses (see
        # rtmif_entry_wrapper.A86's own header comment for why that
        # asymmetry can't be avoided once ABORT.RTM's own `jmp dsptch`
        # tail needs the opposite treatment in the SAME combined
        # module) -- same "known, documented gap" class as
        # test_memif_entry.py's own cases 0-5/10-12. Just confirms the
        # dispatch itself doesn't crash or hang.
        mem = base_memory()
        mem[SD_INDISP] = 0
        run_case("14 creat_proc_entry (dispatch only, not independently verified)", 14, mem,
                  dx=0, bp=0x1234, check_result=False)

        # 15: set_prior_entry
        mem = base_memory()
        ref_img, port = run_case("15 set_prior_entry", 15, mem, dx=7, fields=())
        check("15: p_prior side effect", ref_img[PD_A + P_PRIOR], port[PD_A + P_PRIOR])

        # 16: pd_entry
        mem = base_memory()
        run_case("16 pd_entry", 16, mem, ds=0x1234, fields=('bx', 'cx'))

        # 17: abort_spec_entry -- APB with pd=0 (nothing to abort)
        APB = 0x7200
        mem = base_memory()
        mem_w16(mem, UDA_WRKSEG, 0)
        run_case("17 abort_spec_entry (pd=0)", 17, mem, dx=APB)

        # 18: sleep_entry
        mem = base_memory()
        ref_img, port = run_case("18 sleep_entry", 18, mem, dx=0x99, fields=())
        check("18: p_stat side effect", ref_img[PD_A + P_STAT], port[PD_A + P_STAT])

        # 19: wakeup_entry -- empty System List (no-op)
        SYSLIST = 0x7300
        mem = base_memory()
        mem_w16(mem, SYSLIST, 0)
        run_case("19 wakeup_entry (empty list)", 19, mem, dx=SYSLIST, fields=())

        # 20: findpdname_entry -- empty thread list -> not found
        NAME = 0x7400
        mem = base_memory()
        for i, ch in enumerate(b'NOBODY\x00\x00'):
            mem[NAME + i] = ch
        run_case("20 findpdname_entry (not found)", 20, mem, dx=NAME, bx=PD_A - P_THREAD, fields=('bx', 'cx'))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
