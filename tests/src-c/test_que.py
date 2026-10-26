#!/usr/bin/env python3
"""Differential test for src-c/kernel/que1.c + que2.c against all 10 of
src/kernel/QUE1.RTM's routines (makeq_entry, openq_entry,
deleteq_entry, queverify, readq_entry, creadq_entry, readq,
writeq_entry, cwriteq_entry, writeq) and 5 of src/kernel/QUE2.RTM's 6
(rlsmx, getqdaddr, remqd, qspace, qrelease), on real memory layout.

que1.c/que2.c address real memory (defs.h's far_ptr), so the reference
runs with tools/i86sim.py's segmented=True: tests/src-c/fixtures/
que_wrapper.A86 includes the real SYSDAT.FMT/UDA.FMT, SYSDAT is at
segment 100h (DS) with QDs, the qlr/qul lists and PDs at SYSDAT-relative
offsets, the running process's UDA is the reference's ES and its PD's
p_uda, and QPBs/caller buffers sit in that UDA's u_wrkseg. Field
offsets come from the assembled symbol table, and every case diffs the
whole image to catch stray writes.

readq's/writeq's own real "wait forever" retry path (unconditional,
queue empty/full) is deliberately not exercised here -- see que1.c's
own header comment for why.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'que_wrapper.A86')
QUE_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'que1.c'),
         os.path.join(REPO_ROOT, 'src-c', 'kernel', 'que2.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
STACK_TOP = 0xFFF0
MPMIF_STUB = 0x9000

SYSDAT = 0x0100     # DS; QDs/PDs below are SYSDAT offsets
UDA = 0x0600        # running process's UDA segment (ES)
WRKSEG = 0x0700     # its u_wrkseg; QPBs/buffers below are offsets in it
ENDSEG = 0x0F00
PD_RUN = 0x2800     # rlr

Q_LINK, Q_FLAGS, Q_NAME, Q_MSGLEN, Q_NMSGS, Q_DQ, Q_NQ, Q_MSGCNT, Q_MSGOUT, Q_BUF = \
    0, 4, 6, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A
QNAMSIZ = 8
QPB_QADDR, QPB_BUFFPTR, QPB_NAME = 2, 6, 8
QF_MX, QF_TABLE = 0x01, 0x10
P_STAT, P_UDA = 0x04, 0x10

ENTRIES = ('makeq_entry', 'openq_entry', 'deleteq_entry', 'queverify',
           'readq_entry', 'creadq_entry', 'writeq_entry', 'cwriteq_entry',
           'rlsmx', 'getqdaddr', 'remqd', 'qspace', 'qrelease')


def phys(seg, off=0):
    return seg * 16 + off


def sd(off):
    return phys(SYSDAT, off)


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def pad(name):
    return (name + b' ' * QNAMSIZ)[:QNAMSIZ]


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    sym = {name: asm.symtab[name].value for name in
           ENTRIES + ('sysdat', 'rlr', 'qlr', 'qul', 'endseg', 'u_wrkseg')}
    assert sym['sysdat'] == 0
    assert len(code) < sd(0), "code must not overlap SYSDAT"
    return code, sym


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *QUE_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def main():
    code, sym = assemble_reference()
    skip = set(range(len(code))) | set(range(0xFF00, SIM_MEM))  # code, harness stack (SS=0)
    QLR, QUL = sd(sym['qlr']), sd(sym['qul'])

    def base_memory(wrkseg=WRKSEG):
        mem = bytearray(SIM_MEM)
        w16(mem, sd(sym['endseg']), ENDSEG)
        w16(mem, sd(sym['rlr']), PD_RUN)
        w16(mem, sd(PD_RUN + P_UDA), UDA)
        w16(mem, phys(UDA, sym['u_wrkseg']), wrkseg)
        return mem

    def run_reference(entry, mem, regs):
        image = bytearray(mem)
        image[0:len(code)] = code
        w16(image, sym['sysdat'], SYSDAT)
        sim = I86Sim(image, {**regs, 'ds': SYSDAT, 'es': UDA, 'ss': 0, 'sp': STACK_TOP, 'cs': 0},
                     sym[entry], call_stub_addrs={MPMIF_STUB}, segmented=True)
        sim.run()
        return sim.regs, image

    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)
        u16 = ctypes.c_uint16

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        for name in ('makeq_entry', 'openq_entry', 'deleteq_entry', 'readq_entry',
                     'creadq_entry', 'writeq_entry', 'cwriteq_entry'):
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [c_u8p, u16, u16]
        lib.queverify.restype = u16
        lib.queverify.argtypes = [c_u8p, u16, u16]
        lib.rlsmx.argtypes = [c_u8p, u16, u16]
        lib.getqdaddr.restype = u16
        lib.getqdaddr.argtypes = [c_u8p, u16, u16, c_u16p]
        lib.remqd.argtypes = [c_u8p, u16, u16]
        lib.qspace.argtypes = [c_u8p, u16, u16, c_u16p]
        lib.qrelease.argtypes = [c_u8p, u16, u16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def call_port(entry, buf, val):
            fn = getattr(lib, entry)
            if entry in ('getqdaddr', 'qspace'):
                cx = ctypes.c_uint16()
                ret = fn(buf, SYSDAT, val, ctypes.byref(cx))
                return (ret, cx.value) if entry == 'getqdaddr' else (cx.value,)
            r = fn(buf, SYSDAT, val)
            if entry == 'queverify':
                return (r,)
            if entry in ('rlsmx', 'remqd', 'qrelease'):
                return ()
            return (r.bx, r.cx)

        def ref_result(entry, regs):
            if entry == 'getqdaddr':
                return (regs['dx'], regs['cx'])
            if entry in ('queverify', 'qspace'):
                return (regs['cx'],)
            if entry in ('rlsmx', 'remqd', 'qrelease'):
                return ()
            return (regs['bx'], regs['cx'])

        def run_case(label, entry, mem, val, snap=lambda img: ()):
            reg = 'si' if entry in ('rlsmx', 'remqd', 'qspace', 'qrelease') else 'dx'
            ref_regs, ref = run_reference(entry, mem, {reg: val})
            buf = (ctypes.c_uint8 * PHYS_MEM)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            port_r = call_port(entry, buf, val)
            port = bytes(buf)
            check(label, (ref_result(entry, ref_regs), snap(ref)), (port_r, snap(port)))
            diffs = [hex(a) for a in range(SIM_MEM) if a not in skip and ref[a] != port[a]]
            check(f"{label}: no other bytes differ", [], diffs[:8])
            check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

        def wk(off):
            return phys(WRKSEG, off)

        # -- getqdaddr: already within MPM range --
        # u_wrkseg 180h is inside SYSDAT..endseg, so wrkseg:1800h is
        # SYSDAT:2000h (off = (180h-100h)<<4 = 800h) and q_buf gets off added.
        mem = base_memory(wrkseg=0x0180)
        w16(mem, sd(0x2000 + Q_BUF), 5)
        run_case("getqdaddr: already in MPM range", 'getqdaddr', mem, 0x1800,
                 lambda img: r16(img, sd(0x2000 + Q_BUF)))

        # -- getqdaddr: outside MPM range -> grab from the qul free table --
        mem = base_memory(wrkseg=ENDSEG)  # wrkseg >= endseg -> out of range
        src = phys(ENDSEG, 0x100)
        mem[src + Q_NAME:src + Q_NAME + QNAMSIZ] = pad(b'TESTQ')
        w16(mem, src + Q_MSGLEN, 4)
        FREE_QD = 0x2000
        w16(mem, sd(FREE_QD + Q_LINK), 0x2100)  # next free entry
        w16(mem, QUL, FREE_QD)
        run_case("getqdaddr: from the qul free table", 'getqdaddr', mem, 0x100,
                 lambda img: (r16(img, QUL), r16(img, sd(FREE_QD + Q_FLAGS)),
                              bytes(img[sd(FREE_QD):sd(FREE_QD + 0x1C)])))

        # -- getqdaddr: qul exhausted -> e_no_qd --
        mem = base_memory(wrkseg=ENDSEG)
        run_case("getqdaddr: qul exhausted", 'getqdaddr', mem, 0x100)

        # -- makeq_entry: name collision / success --
        # default u_wrkseg 700h is inside MPM range: off = 6000h
        QD_DX = 0x0100
        QD_SD = QD_DX + ((WRKSEG - SYSDAT) << 4)
        for label, name in (("makeq_entry: name collision", b'ECHO'),
                            ("makeq_entry: success, links onto qlr", b'NEWQ')):
            mem = base_memory()
            mem[sd(QD_SD + Q_NAME):sd(QD_SD + Q_NAME) + QNAMSIZ] = pad(name)
            w16(mem, sd(QD_SD + Q_DQ), 0x1111)
            w16(mem, sd(QD_SD + Q_MSGCNT), 0x2222)
            EXISTING = 0x2100
            mem[sd(EXISTING + Q_NAME):sd(EXISTING + Q_NAME) + QNAMSIZ] = pad(b'ECHO')
            w16(mem, QLR, EXISTING)
            run_case(label, 'makeq_entry', mem, QD_DX,
                     lambda img: (r16(img, QLR), r16(img, sd(QD_SD + Q_LINK)), r16(img, sd(QD_SD + Q_DQ))))

        # -- openq_entry: found / not found --
        for label, want in (("openq_entry: found", b'ECHO'), ("openq_entry: not found", b'NOPE')):
            mem = base_memory()
            QD = 0x2100
            mem[sd(QD + Q_NAME):sd(QD + Q_NAME) + QNAMSIZ] = pad(b'OTHER')
            w16(mem, sd(QD + Q_LINK), 0x2200)
            mem[sd(0x2200 + Q_NAME):sd(0x2200 + Q_NAME) + QNAMSIZ] = pad(b'ECHO')
            w16(mem, QLR, QD)
            QPB = 0x0200
            mem[wk(QPB + QPB_NAME):wk(QPB + QPB_NAME) + QNAMSIZ] = pad(want)
            run_case(label, 'openq_entry', mem, QPB, lambda img: r16(img, wk(QPB + QPB_QADDR)))

        # -- deleteq_entry: in use / not active / success --
        #
        # "not active" exercises the qd_noq path docs/bugs.md #100 fixed
        # (a real stack-corruption bug before that fix -- see que1.c's
        # own header comment).
        QD, QPB = 0x2100, 0x0200
        mem = base_memory()
        w16(mem, sd(QD + Q_DQ), 1)
        w16(mem, wk(QPB + QPB_QADDR), QD)
        run_case("deleteq_entry: in use", 'deleteq_entry', mem, QPB)

        mem = base_memory()
        w16(mem, wk(QPB + QPB_QADDR), QD)  # qlr empty -- QD isn't on it
        run_case("deleteq_entry: not active", 'deleteq_entry', mem, QPB)

        mem = base_memory()
        w16(mem, sd(QD + Q_FLAGS), QF_TABLE)
        w16(mem, sd(0x2200 + Q_LINK), QD)
        w16(mem, sd(QD + Q_LINK), 0x2300)
        w16(mem, QLR, 0x2200)
        w16(mem, QUL, 0x2400)
        w16(mem, wk(QPB + QPB_QADDR), QD)
        run_case("deleteq_entry: success (unlinks from qlr, remqd)", 'deleteq_entry', mem, QPB,
                 lambda img: (r16(img, sd(0x2200 + Q_LINK)), r16(img, sd(QD + Q_LINK)), r16(img, QUL)))

        # -- queverify: found / not found --
        for label, qaddr in (("queverify: found", 0x2200), ("queverify: not found", 0x2777)):
            mem = base_memory()
            w16(mem, QLR, 0x2100)
            w16(mem, sd(0x2100 + Q_LINK), 0x2200)
            w16(mem, wk(QPB + QPB_QADDR), qaddr)
            run_case(label, 'queverify', mem, QPB)

        def queue_memory(msgcnt, msglen, nmsgs=2, msgout=0, flags=0, qbuf=0x2300):
            mem = base_memory()
            w16(mem, QLR, QD)
            w16(mem, sd(QD + Q_MSGCNT), msgcnt)
            w16(mem, sd(QD + Q_MSGLEN), msglen)
            w16(mem, sd(QD + Q_NMSGS), nmsgs)
            w16(mem, sd(QD + Q_MSGOUT), msgout)
            w16(mem, sd(QD + Q_FLAGS), flags)
            w16(mem, sd(QD + Q_BUF), qbuf)
            w16(mem, wk(QPB + QPB_QADDR), QD)
            w16(mem, wk(QPB + QPB_BUFFPTR), 0x0400)
            return mem

        # -- readq_entry: a real message in the buffer, with wraparound --
        mem = queue_memory(msgcnt=1, msglen=4, msgout=1)
        mem[sd(0x2304):sd(0x2308)] = b'\x11\x22\x33\x44'  # slot 1's message
        run_case("readq_entry: copies the message, msgout wraps to 0", 'readq_entry', mem, QPB,
                 lambda img: (bytes(img[wk(0x400):wk(0x404)]), r16(img, sd(QD + Q_MSGOUT)),
                              r16(img, sd(QD + Q_MSGCNT))))

        # -- readq_entry: msglen==0, an MX queue -> BUF becomes the reader's own PD --
        mem = queue_memory(msgcnt=1, msglen=0, flags=QF_MX, qbuf=0)
        run_case("readq_entry: msglen==0, MX queue -> BUF=reader's PD", 'readq_entry', mem, QPB,
                 lambda img: (r16(img, sd(QD + Q_BUF)), r16(img, sd(QD + Q_MSGCNT))))

        # -- readq_entry: QD not on qlr -> e_no_queue --
        mem = queue_memory(msgcnt=1, msglen=4)
        w16(mem, QLR, 0)
        run_case("readq_entry: unknown queue", 'readq_entry', mem, QPB)

        # -- creadq_entry: empty queue, conditional -> immediate e_q_empty --
        run_case("creadq_entry: empty queue -> e_q_empty, no wait", 'creadq_entry',
                 queue_memory(msgcnt=0, msglen=4), QPB)

        # -- writeq_entry: room available, with wraparound --
        mem = queue_memory(msgcnt=1, msglen=4, msgout=1)  # slot (1+1)%2 = 0
        mem[wk(0x400):wk(0x404)] = b'\xAA\xBB\xCC\xDD'
        run_case("writeq_entry: copies into slot 0 (wrapped), msgcnt bumped", 'writeq_entry', mem, QPB,
                 lambda img: (bytes(img[sd(0x2300):sd(0x2304)]), r16(img, sd(QD + Q_MSGCNT))))

        # -- writeq_entry: msglen==0, an MX queue -> BUF cleared --
        mem = queue_memory(msgcnt=0, msglen=0, nmsgs=1, flags=QF_MX, qbuf=0x2999)
        run_case("writeq_entry: msglen==0, MX queue -> BUF cleared", 'writeq_entry', mem, QPB,
                 lambda img: (r16(img, sd(QD + Q_BUF)), r16(img, sd(QD + Q_MSGCNT))))

        # -- cwriteq_entry: full queue, conditional -> immediate e_q_full --
        run_case("cwriteq_entry: full queue -> e_q_full, no wait", 'cwriteq_entry',
                 queue_memory(msgcnt=2, msglen=4), QPB)

        # -- rlsmx: walks qlr; matching MX queue gets the (stubbed) f_cqwrite --
        mem = base_memory()
        OWNER = 0x2A00
        w16(mem, sd(0x2100 + Q_FLAGS), QF_MX)
        w16(mem, sd(0x2100 + Q_BUF), OWNER)
        w16(mem, sd(0x2100 + Q_LINK), 0x2200)
        w16(mem, sd(0x2200 + Q_FLAGS), QF_MX)
        w16(mem, sd(0x2200 + Q_BUF), 0x2B00)
        w16(mem, QLR, 0x2100)
        run_case("rlsmx: walks qlr", 'rlsmx', mem, OWNER)

        # -- remqd: threads the freed QD onto qul (docs/bugs.md #99's fix) --
        mem = base_memory()
        w16(mem, sd(QD + Q_FLAGS), QF_TABLE)
        w16(mem, QUL, 0x2555)
        run_case("remqd: qul updated to the freed QD", 'remqd', mem, QD,
                 lambda img: (r16(img, sd(QD + Q_LINK)), r16(img, QUL)))

        mem = base_memory()
        w16(mem, QUL, 0x2555)
        run_case("remqd: not a table QD -> untouched", 'remqd', mem, QD)

        # -- qspace: always reports an error under the boundary-stub convention --
        mem = base_memory()
        w16(mem, sd(QD + Q_MSGLEN), 10)
        w16(mem, sd(QD + Q_NMSGS), 3)
        run_case("qspace: boundary stub leaves an error code", 'qspace', mem, QD)

        # -- qrelease: pure boundary stub, no observable effect --
        mem = base_memory()
        w16(mem, sd(QD + Q_BUF), 0x1234)
        run_case("qrelease: no observable effect", 'qrelease', mem, QD)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
