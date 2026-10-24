#!/usr/bin/env python3
"""Differential test for src-c/kernel/bdosif.c against src/kernel/
BDOSIF.BDO's `init`. `entry` is not ported -- see bdosif.c's own
header comment.

`init`'s own three `call mpmif` sites (create/open/write the MX disk
queue) are real calls into QUE1.RTM's own `makeq_entry`/`openq_entry`/
`writeq_entry` (already independently verified, tests/src-c/
test_que.py) -- wired via the fresh near-dispatch `mpmif` convention
tests/src-c/fixtures/bdosif_wrapper.A86's own header comment explains
(BDOSIF.BDO's own real `mpmif` is a genuine indirect far call with no
real-redirect capability in tools/i86sim.py, so it can't be driven for
real the way RTMIF.RTM's own single near `call proc_creat` could).
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'bdosif_wrapper.A86')
BDOSIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in ('bdosif.c', 'que1.c', 'que2.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

SD_SUPMOD_OFF, SD_SUPMOD_SEG = 0xA88D, 0xA88F
BDOSIF_SYSDAT, BDOSIF_SUPERVISOR_OFF, BDOSIF_SUPERVISOR_SEG = 0x0006, 0x0008, 0x000A

SD_RLR, SD_QLR, SD_QUL, SD_QMAU, SD_ENDSEG = 0xA000, 0xA020, 0xA022, 0xA024, 0xA026
UDA_WRKSEG = 0xA012
SD_MXDISKQD, SD_MXDISKQPB = 0xA0C0, 0xA0E0

Q_LINK, Q_FLAGS, Q_NAME, Q_MSGLEN, Q_NMSGS, Q_DQ, Q_NQ, Q_MSGCNT, Q_MSGOUT, Q_BUF = \
    0, 4, 6, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A
QPB_FLGS, QPB_QADDR, QPB_NMSGS, QPB_BUFFPTR, QPB_NAME = 0, 2, 4, 6, 8
QNAMSIZ = 8
QF_MX = 0x01

DS_SYSDAT = 0x1234  # arbitrary tag -- init is tested non-segmented, see
                    # bdosif.c's own header comment


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['init'].value
    entry_label = asm.symtab['entry'].value
    retf_addrs = [i for i in range(entry, entry_label) if code[i] == 0xCB]
    assert len(retf_addrs) == 1, f"expected exactly 1 retf, found {retf_addrs}"
    return code, entry, retf_addrs[0]


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *BDOSIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_ENDSEG, 0x5000)
    mem_w16(mem, UDA_WRKSEG, DS_SYSDAT)  # same flat-passthrough convention
                                          # test_que.py's own base_memory uses
    mem_w16(mem, SD_SUPMOD_OFF, 0x0718)
    mem_w16(mem, SD_SUPMOD_SEG, 0x1000)

    # src/kernel/DATA.BDO's own real initial mxdiskqd/mxdiskqpb values
    mem_w16(mem, SD_MXDISKQD + Q_LINK, 0)
    mem_w16(mem, SD_MXDISKQD + Q_FLAGS, QF_MX)
    mem[SD_MXDISKQD + Q_NAME:SD_MXDISKQD + Q_NAME + QNAMSIZ] = b'MXdisk  '
    mem_w16(mem, SD_MXDISKQD + Q_MSGLEN, 0)
    mem_w16(mem, SD_MXDISKQD + Q_NMSGS, 1)
    mem_w16(mem, SD_MXDISKQD + Q_DQ, 0)
    mem_w16(mem, SD_MXDISKQD + Q_NQ, 0)
    mem_w16(mem, SD_MXDISKQD + Q_MSGCNT, 0)
    mem_w16(mem, SD_MXDISKQD + Q_MSGOUT, 0)
    mem_w16(mem, SD_MXDISKQD + Q_BUF, 0)

    mem[SD_MXDISKQPB + QPB_FLGS] = 0
    mem_w16(mem, SD_MXDISKQPB + QPB_QADDR, 0)
    mem_w16(mem, SD_MXDISKQPB + QPB_NMSGS, 1)
    mem_w16(mem, SD_MXDISKQPB + QPB_BUFFPTR, 0)
    mem[SD_MXDISKQPB + QPB_NAME:SD_MXDISKQPB + QPB_NAME + QNAMSIZ] = b'MXdisk  '
    return mem


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def run_reference(code, entry, stop_addr, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs={stop_addr})
    sim.run()
    return sim.regs, image


def main():
    code, entry, stop_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.init.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        mem = base_memory()
        ref_regs, ref_img = run_reference(code, entry, stop_addr, mem, {'ds': DS_SYSDAT})
        buf = to_ctypes(mem)
        lib.init(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_SYSDAT))
        port = bytes(buf)

        check("init: sysdat", ref_img[BDOSIF_SYSDAT:BDOSIF_SYSDAT + 2], port[BDOSIF_SYSDAT:BDOSIF_SYSDAT + 2])
        check("init: supervisor offset", ref_img[BDOSIF_SUPERVISOR_OFF:BDOSIF_SUPERVISOR_OFF + 2],
              port[BDOSIF_SUPERVISOR_OFF:BDOSIF_SUPERVISOR_OFF + 2])
        check("init: supervisor segment", ref_img[BDOSIF_SUPERVISOR_SEG:BDOSIF_SUPERVISOR_SEG + 2],
              port[BDOSIF_SUPERVISOR_SEG:BDOSIF_SUPERVISOR_SEG + 2])

        # makeq_entry threaded mxdiskqd onto SD_QLR
        check("init: qlr -> mxdiskqd", mem_r16(ref_img, SD_QLR), mem_r16(port, SD_QLR))
        check("init: mxdiskqd's own q_link", mem_r16(ref_img, SD_MXDISKQD + Q_LINK),
              mem_r16(port, SD_MXDISKQD + Q_LINK))

        # openq_entry filled in mxdiskqpb's own qpb_qaddr
        check("init: mxdiskqpb's own qpb_qaddr", mem_r16(ref_img, SD_MXDISKQPB + QPB_QADDR),
              mem_r16(port, SD_MXDISKQPB + QPB_QADDR))

        # writeq_entry incremented mxdiskqd's own q_msgcnt (nmsgs=1, so
        # this is the "not full" real path, not the unconditional
        # retry-forever one -- see que1.c's own header comment)
        check("init: mxdiskqd's own q_msgcnt", mem_r16(ref_img, SD_MXDISKQD + Q_MSGCNT),
              mem_r16(port, SD_MXDISKQD + Q_MSGCNT))
        check("init: mxdiskqd's own q_buf (MX queue, no owner)", mem_r16(ref_img, SD_MXDISKQD + Q_BUF),
              mem_r16(port, SD_MXDISKQD + Q_BUF))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
