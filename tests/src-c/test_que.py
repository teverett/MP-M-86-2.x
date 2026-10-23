#!/usr/bin/env python3
"""Differential test for src-c/kernel/que1.c + que2.c against 4 of
src/kernel/QUE1.RTM's 10 routines (makeq_entry, openq_entry,
deleteq_entry, queverify) and 5 of src/kernel/QUE2.RTM's 6
(rlsmx, getqdaddr, remqd, qspace, qrelease).
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

STACK_TOP = 0xFFF0
SD_QLR, SD_QUL, SD_QMAU, SD_ENDSEG = 0xA020, 0xA022, 0xA024, 0xA026
UDA_WRKSEG = 0xA012
MPMIF_STUB = 0x9000

Q_LINK, Q_FLAGS, Q_NAME, Q_NMSGS, Q_DQ, Q_NQ, Q_MSGCNT, Q_MSGOUT, Q_BUF = 0, 4, 6, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A
QNAMSIZ = 8
QPB_QADDR, QPB_NAME = 2, 8
QF_MX, QF_TABLE = 0x01, 0x10
F_MAUALLOC = 0x309
E_NO_QD, E_NO_QUEUE, E_Q_INUSE, E_Q_NOTACTIVE = 7, 9, 0xA, 0xB

SYSDAT = 0x2000  # this test's chosen "cs:sysdat" value


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in (
        'makeq_entry', 'openq_entry', 'deleteq_entry', 'queverify',
        'rlsmx', 'getqdaddr', 'remqd', 'qspace', 'qrelease')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *QUE_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_ENDSEG, 0x5000)
    mem_w16(mem, UDA_WRKSEG, SYSDAT)  # by default, dx=0 maps straight into "MPM range"
    return mem


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def put_qname(mem, addr, name):
    padded = (name + b' ' * QNAMSIZ)[:QNAMSIZ]
    mem[addr + Q_NAME:addr + Q_NAME + QNAMSIZ] = padded


def run_reference(code, entry, mem, regs, sysdat_word=SYSDAT, stop_addrs=()):
    image = bytearray(mem)
    image[0:len(code)] = code
    image[0] = sysdat_word & 0xFF  # `sysdat dw 0` sits at cseg offset 0, same as other wrappers
    image[1] = (sysdat_word >> 8) & 0xFF
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry,
                 stop_addrs=set(stop_addrs), call_stub_addrs={MPMIF_STUB})
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.makeq_entry.restype = BxCx
        lib.makeq_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.openq_entry.restype = BxCx
        lib.openq_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.deleteq_entry.restype = BxCx
        lib.deleteq_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.queverify.restype = ctypes.c_uint16
        lib.queverify.argtypes = [c_u8p, ctypes.c_uint16]
        lib.rlsmx.argtypes = [c_u8p, ctypes.c_uint16]
        lib.getqdaddr.restype = ctypes.c_uint16
        lib.getqdaddr.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, c_u16p]
        lib.remqd.argtypes = [c_u8p, ctypes.c_uint16]
        lib.qspace.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]
        lib.qrelease.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def fmt(v):
            return f"{v[:24]!r}..." if isinstance(v, bytes) and len(v) > 24 else repr(v)

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {fmt(ref)} | port {fmt(port)}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- getqdaddr: already within MPM range --
        mem = base_memory()
        QD_SRC = 0x5000
        mem_w16(mem, QD_SRC + Q_BUF, 5)  # nonzero, so the "add off" branch is exercised
        ref_regs, ref_img = run_reference(code, entries['getqdaddr'], mem, {'dx': QD_SRC})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        si = lib.getqdaddr(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD_SRC), ctypes.c_uint16(SYSDAT), ctypes.byref(cx))
        ref = (ref_regs['dx'], ref_regs['cx'], mem_r16(ref_img, ref_regs['dx'] + Q_BUF))
        port = (si, cx.value, mem_r16(bytes(buf), si + Q_BUF))
        check("getqdaddr: already in MPM range", ref, port)

        # -- getqdaddr: outside MPM range -> grab from the qul free table --
        mem = base_memory()
        mem_w16(mem, UDA_WRKSEG, 0x0100)  # far below sysdat -> out of range
        QD_SRC = 0x6000
        put_qname(mem, QD_SRC, b'TESTQ')
        mem_w16(mem, QD_SRC + Q_FLAGS, 0)
        FREE_QD = 0x4000
        mem_w16(mem, FREE_QD + Q_LINK, 0)  # last entry on the free list
        mem_w16(mem, SD_QUL, FREE_QD)
        ref_regs, ref_img = run_reference(code, entries['getqdaddr'], mem, {'dx': QD_SRC})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        di = lib.getqdaddr(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD_SRC), ctypes.c_uint16(SYSDAT), ctypes.byref(cx))
        port_img = bytes(buf)
        ref = (ref_regs['dx'], ref_regs['cx'], mem_r16(ref_img, SD_QUL),
               ref_img[ref_regs['dx'] + Q_FLAGS] if False else mem_r16(ref_img, ref_regs['dx'] + Q_FLAGS))
        port = (di, cx.value, mem_r16(port_img, SD_QUL), mem_r16(port_img, di + Q_FLAGS))
        check("getqdaddr: from the qul free table", ref, port)

        # -- getqdaddr: qul exhausted -> e_no_qd --
        mem = base_memory()
        mem_w16(mem, UDA_WRKSEG, 0x0100)
        mem_w16(mem, SD_QUL, 0)
        ref_regs, ref_img = run_reference(code, entries['getqdaddr'], mem, {'dx': 0x6000})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        di = lib.getqdaddr(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x6000), ctypes.c_uint16(SYSDAT), ctypes.byref(cx))
        check("getqdaddr: qul exhausted", (ref_regs['dx'], ref_regs['cx']), (di, cx.value))

        # -- makeq_entry: name collision --
        mem = base_memory()
        QD_SRC = 0x5000
        mem_w16(mem, QD_SRC + Q_BUF, 0)
        put_qname(mem, QD_SRC, b'ECHO')
        EXISTING = 0x4100
        put_qname(mem, EXISTING, b'ECHO')
        mem_w16(mem, EXISTING + Q_LINK, 0)
        mem_w16(mem, SD_QLR, EXISTING)
        ref_regs, _ = run_reference(code, entries['makeq_entry'], mem, {'dx': QD_SRC})
        buf = to_ctypes(mem)
        r = lib.makeq_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD_SRC), ctypes.c_uint16(SYSDAT))
        check("makeq_entry: name collision", (ref_regs['bx'], ref_regs['cx']), (r.bx, r.cx))

        # -- makeq_entry: success --
        mem = base_memory()
        QD_SRC = 0x5000
        mem_w16(mem, QD_SRC + Q_BUF, 0)
        put_qname(mem, QD_SRC, b'NEWQ')
        OTHER = 0x4100
        put_qname(mem, OTHER, b'OTHER')
        mem_w16(mem, OTHER + Q_LINK, 0)
        mem_w16(mem, SD_QLR, OTHER)
        ref_regs, ref_img = run_reference(code, entries['makeq_entry'], mem, {'dx': QD_SRC})
        buf = to_ctypes(mem)
        r = lib.makeq_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD_SRC), ctypes.c_uint16(SYSDAT))
        port_img = bytes(buf)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, SD_QLR),
               mem_r16(ref_img, QD_SRC + Q_DQ), mem_r16(ref_img, QD_SRC + Q_LINK))
        port = ((r.bx, r.cx), mem_r16(port_img, SD_QLR),
                mem_r16(port_img, QD_SRC + Q_DQ), mem_r16(port_img, QD_SRC + Q_LINK))
        check("makeq_entry: success, links onto qlr", ref, port)

        # -- openq_entry: found / not found --
        for label, want_name, present in (("openq_entry: found", b'ECHO', True), ("openq_entry: not found", b'NOPE', True)):
            mem = base_memory()
            QD = 0x4100
            put_qname(mem, QD, b'ECHO')
            mem_w16(mem, QD + Q_LINK, 0)
            mem_w16(mem, SD_QLR, QD)
            QPB = 0x5000
            put_qname_at = QPB + QPB_NAME
            padded = (want_name + b' ' * QNAMSIZ)[:QNAMSIZ]
            mem[put_qname_at:put_qname_at + QNAMSIZ] = padded
            ref_regs, ref_img = run_reference(code, entries['openq_entry'], mem, {'dx': QPB})
            buf = to_ctypes(mem)
            r = lib.openq_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QPB))
            port_img = bytes(buf)
            ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, QPB + QPB_QADDR))
            port = ((r.bx, r.cx), mem_r16(port_img, QPB + QPB_QADDR))
            check(label, ref, port)

        # -- deleteq_entry: in use / not active / success --
        #
        # "not active" exercises the qd_noq path docs/bugs.md #100 fixed
        # (a real stack-corruption bug before that fix -- see que1.c's
        # own header comment); now that it's fixed, it's safe to run
        # against the real assembly like every other case here.
        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + Q_DQ, 1)
        mem_w16(mem, QD + Q_NQ, 0)
        QPB = 0x5000
        mem_w16(mem, QPB + QPB_QADDR, QD)
        ref_regs, _ = run_reference(code, entries['deleteq_entry'], mem, {'dx': QPB})
        buf = to_ctypes(mem)
        r = lib.deleteq_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QPB))
        check("deleteq_entry: in use", (ref_regs['bx'], ref_regs['cx']), (r.bx, r.cx))

        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + Q_DQ, 0)
        mem_w16(mem, QD + Q_NQ, 0)
        mem_w16(mem, QD + Q_FLAGS, 0)
        mem_w16(mem, SD_QLR, 0)  # qlr is empty -- QD isn't on it
        QPB = 0x5000
        mem_w16(mem, QPB + QPB_QADDR, QD)
        ref_regs, _ = run_reference(code, entries['deleteq_entry'], mem, {'dx': QPB})
        buf = to_ctypes(mem)
        r = lib.deleteq_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QPB))
        check("deleteq_entry: not active", (ref_regs['bx'], ref_regs['cx']), (r.bx, r.cx))

        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + Q_DQ, 0)
        mem_w16(mem, QD + Q_NQ, 0)
        mem_w16(mem, QD + Q_FLAGS, QF_TABLE)
        mem_w16(mem, QD + Q_LINK, 0)
        mem_w16(mem, SD_QLR, QD)
        mem_w16(mem, SD_QUL, 0x9999)
        QPB = 0x5000
        mem_w16(mem, QPB + QPB_QADDR, QD)
        ref_regs, ref_img = run_reference(code, entries['deleteq_entry'], mem, {'dx': QPB})
        buf = to_ctypes(mem)
        r = lib.deleteq_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QPB))
        port_img = bytes(buf)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, SD_QLR),
               mem_r16(ref_img, QD + Q_LINK), mem_r16(ref_img, SD_QUL))
        port = ((r.bx, r.cx), mem_r16(port_img, SD_QLR),
                mem_r16(port_img, QD + Q_LINK), mem_r16(port_img, SD_QUL))
        check("deleteq_entry: success (removes from qlr, calls remqd)", ref, port)

        # -- queverify: found / not found --
        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + Q_LINK, 0)
        mem_w16(mem, SD_QLR, QD)
        QPB = 0x5000
        mem_w16(mem, QPB + QPB_QADDR, QD)
        ref_regs, _ = run_reference(code, entries['queverify'], mem, {'dx': QPB})
        buf = to_ctypes(mem)
        cx = lib.queverify(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QPB))
        check("queverify: found", ref_regs['cx'], cx)

        mem_w16(mem, QPB + QPB_QADDR, 0x7777)  # not on the list
        ref_regs, _ = run_reference(code, entries['queverify'], mem, {'dx': QPB})
        buf = to_ctypes(mem)
        cx = lib.queverify(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QPB))
        check("queverify: not found", ref_regs['cx'], cx)

        # -- rlsmx: releases a matching MX queue, leaves a non-matching one alone --
        mem = base_memory()
        OWNER_PD = 0x8000
        MX_Q, OTHER_Q = 0x4100, 0x4200
        mem_w16(mem, MX_Q + Q_FLAGS, QF_MX)
        mem_w16(mem, MX_Q + Q_BUF, OWNER_PD)
        mem_w16(mem, MX_Q + Q_LINK, OTHER_Q)
        mem_w16(mem, OTHER_Q + Q_FLAGS, QF_MX)
        mem_w16(mem, OTHER_Q + Q_BUF, 0x9999)  # a different owner
        mem_w16(mem, OTHER_Q + Q_LINK, 0)
        mem_w16(mem, SD_QLR, MX_Q)
        ref_regs, ref_img = run_reference(code, entries['rlsmx'], mem, {'si': OWNER_PD})
        buf = to_ctypes(mem)
        lib.rlsmx(ctypes.cast(buf, c_u8p), ctypes.c_uint16(OWNER_PD))
        port_img = bytes(buf)
        ref = (mem_r16(ref_img, MX_Q + Q_BUF), mem_r16(ref_img, OTHER_Q + Q_BUF))
        port = (mem_r16(port_img, MX_Q + Q_BUF), mem_r16(port_img, OTHER_Q + Q_BUF))
        check("rlsmx: matching MX queue stubbed-called, other untouched", ref, port)

        # -- remqd: threads the freed QD onto qul (docs/bugs.md #99's fix) --
        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + Q_FLAGS, QF_TABLE)
        mem_w16(mem, QD + Q_BUF, 0)
        mem_w16(mem, SD_QUL, 0x5555)
        ref_regs, ref_img = run_reference(code, entries['remqd'], mem, {'si': QD})
        buf = to_ctypes(mem)
        lib.remqd(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD))
        port_img = bytes(buf)
        ref = (mem_r16(ref_img, QD + Q_LINK), mem_r16(ref_img, SD_QUL))
        port = (mem_r16(port_img, QD + Q_LINK), mem_r16(port_img, SD_QUL))
        check("remqd: qul updated to the freed QD", ref, port)

        # -- qspace: always reports an error under the boundary-stub convention --
        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + 0x0E, 10)  # q_msglen
        mem_w16(mem, QD + Q_NMSGS, 3)
        mem_w16(mem, QD + Q_BUF, 0)
        ref_regs, ref_img = run_reference(code, entries['qspace'], mem, {'si': QD})
        buf = to_ctypes(mem)
        cx = ctypes.c_uint16()
        lib.qspace(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD), ctypes.byref(cx))
        check("qspace: boundary-stub leaves an error code", ref_regs['cx'], cx.value)

        # -- qrelease: pure boundary stub, no observable effect --
        mem = base_memory()
        QD = 0x4100
        mem_w16(mem, QD + Q_BUF, 0x1234)
        ref_regs, ref_img = run_reference(code, entries['qrelease'], mem, {'si': QD})
        buf = to_ctypes(mem)
        lib.qrelease(ctypes.cast(buf, c_u8p), ctypes.c_uint16(QD))
        check("qrelease: no observable effect", bytes(ref_img[0x4000:0x4300]), bytes(buf)[0x4000:0x4300])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
