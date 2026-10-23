#!/usr/bin/env python3
"""Differential test for src-c/kernel/chario.c against src/kernel/
CHARIO.CIO's 9 entry points: conin_entry, conout_entry, rconin_entry,
rconout_entry, listout_entry, constat_entry, dirio_entry,
conwrite_entry, conprint_entry, conread_entry.

constf/coninf's own boundary-stub result is fixed at 0 under the
convention (see chario.c's own header comment), which makes a few real
paths (control_s's ctl-S wait loop, chkctl's "echo answer" block)
unreachable; they're translated faithfully in chario.c but not
exercised here, same reasoning as QUE1.RTM's cioattach.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'chario_wrapper.A86')
CHARIO_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'console.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'chario.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, SD_CCB, SD_THRDRT = 0xA000, 0xA030, 0xA032
SD_NCNS, SD_NLSTDEV, SD_NCONDEV = 0xA034, 0xA035, 0xA036
MPMIF_STUB = 0x9000
XIOSIF_STUB = 0x9010

C_ATTACH, C_FLAG, C_STRTCOL, C_COLUMN, C_NCHAR, C_MIMIC, C_MSOURCE, CCBLEN = 0, 4, 5, 6, 7, 8, 9, 12
P_FLAG, P_CNS, P_LST, P_SCRATCH = 6, 0x20, 0x24, 0x2E
CF_COMPC = 0x02

PD = 0x5000
CCB_BASE = 0x6000


def ccb_addr(dev):
    return CCB_BASE + dev * CCBLEN


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    names = ('conin_entry', 'conout_entry', 'rconin_entry', 'rconout_entry',
             'listout_entry', 'constat_entry', 'dirio_entry', 'conwrite_entry',
             'conprint_entry', 'conread_entry')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *CHARIO_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD)
    mem_w16(mem, SD_CCB, CCB_BASE)
    mem_w16(mem, SD_THRDRT, 0)
    mem[SD_NCNS] = 4
    mem[SD_NLSTDEV] = 4
    mem[SD_NCONDEV] = 2
    mem[PD + P_CNS] = 0  # default console 0
    mem[PD + P_LST] = 2  # default list device (ncondev + 0)
    mem_w16(mem, ccb_addr(0) + C_ATTACH, PD)  # already attached, avoids
    mem_w16(mem, ccb_addr(2) + C_ATTACH, 0)   # the attach-message cascade
    return mem


def run_reference(code, entry, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry,
                 call_stub_addrs={MPMIF_STUB, XIOSIF_STUB})
    sim.run(max_steps=20000)
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.conin_entry.restype = ctypes.c_uint8
        lib.conin_entry.argtypes = [c_u8p]
        lib.conout_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.rconin_entry.restype = ctypes.c_uint8
        lib.rconin_entry.argtypes = [c_u8p]
        lib.rconout_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.listout_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.constat_entry.restype = ctypes.c_uint8
        lib.constat_entry.argtypes = [c_u8p]
        lib.dirio_entry.restype = ctypes.c_uint8
        lib.dirio_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.conwrite_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.conprint_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
        lib.conread_entry.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- conin_entry: a buffered, non-ctrl character --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('Q')
        mem[ccb_addr(0) + C_COLUMN] = 3
        ref_regs, ref_img = run_reference(code, entries['conin_entry'], mem, {})
        buf = to_ctypes(mem)
        al = lib.conin_entry(ctypes.cast(buf, c_u8p))
        ref = (ref_regs['bx'] & 0xFF, ref_img[ccb_addr(0) + C_COLUMN])
        port = (al, bytes(buf)[ccb_addr(0) + C_COLUMN])
        check("conin_entry: buffered graphic char", ref, port)

        # -- conin_entry: ctl-D detaches (chkctl's own ctld branch is
        # reachable/deterministic, but conin_entry's "je conin_entry"
        # retry loop that follows it calls conin/conbrk a second time,
        # and under the convention their result is whatever bx happens
        # to be at that point -- a real artifact of the full attach/
        # conattach_entry/message-printing call chain the reference
        # assembly runs through there, not something this port's own
        # fixed-at-0 choice can be expected to reproduce (see header
        # comment). Only the deterministic p_flag side effect is
        # checked here, not the final returned character. --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = 0x04  # ctld
        ref_regs, ref_img = run_reference(code, entries['conin_entry'], mem, {})
        buf = to_ctypes(mem)
        lib.conin_entry(ctypes.cast(buf, c_u8p))
        ref = mem_r16(ref_img, PD + P_FLAG)
        port = mem_r16(bytes(buf), PD + P_FLAG)
        check("conin_entry: ctl-D detaches (p_flag side effect)", ref, port)

        # -- conout_entry: a plain character advances column --
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 10
        ref_regs, ref_img = run_reference(code, entries['conout_entry'], mem, {'dx': ord('Z')})
        buf = to_ctypes(mem)
        lib.conout_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(ord('Z')))
        check("conout_entry: column advances", ref_img[ccb_addr(0) + C_COLUMN], bytes(buf)[ccb_addr(0) + C_COLUMN])

        # -- rconin_entry: reads and clears c_nchar, raw (no chkctl) --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = 0x04  # ctld -- raw mode skips chkctl entirely
        ref_regs, ref_img = run_reference(code, entries['rconin_entry'], mem, {})
        buf = to_ctypes(mem)
        al = lib.rconin_entry(ctypes.cast(buf, c_u8p))
        ref = (ref_regs['bx'] & 0xFF, ref_img[ccb_addr(0) + C_NCHAR])
        port = (al, bytes(buf)[ccb_addr(0) + C_NCHAR])
        check("rconin_entry: raw read, ctld passed through untouched", ref, port)

        # -- rconout_entry: line feed resets column --
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 7
        ref_regs, ref_img = run_reference(code, entries['rconout_entry'], mem, {'dx': 0x0A})
        buf = to_ctypes(mem)
        lib.rconout_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x0A))
        check("rconout_entry: lf resets column", ref_img[ccb_addr(0) + C_COLUMN], bytes(buf)[ccb_addr(0) + C_COLUMN])

        # -- listout_entry: attaches the calling process to its list device --
        mem = base_memory()
        ref_regs, ref_img = run_reference(code, entries['listout_entry'], mem, {'dx': ord('X')})
        buf = to_ctypes(mem)
        lib.listout_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(ord('X')))
        ref = mem_r16(ref_img, ccb_addr(2) + C_ATTACH)
        port = mem_r16(bytes(buf), ccb_addr(2) + C_ATTACH)
        check("listout_entry: attaches to the list device", ref, port)

        # -- constat_entry: attached -> defers to conbrk --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('Q')
        ref_regs, _ = run_reference(code, entries['constat_entry'], mem, {})
        buf = to_ctypes(mem)
        bl = lib.constat_entry(ctypes.cast(buf, c_u8p))
        check("constat_entry: attached, char buffered", ref_regs['bx'] & 0xFF, bl)

        # -- constat_entry: not attached -> boundary stub, always 0 --
        mem = base_memory()
        mem_w16(mem, ccb_addr(0) + C_ATTACH, 0x9999)  # some other PD
        ref_regs, _ = run_reference(code, entries['constat_entry'], mem, {})
        buf = to_ctypes(mem)
        bl = lib.constat_entry(ctypes.cast(buf, c_u8p))
        check("constat_entry: not attached", ref_regs['bx'] & 0xFF, bl)

        # -- dirio_entry: read (0xFD) --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('M')
        ref_regs, ref_img = run_reference(code, entries['dirio_entry'], mem, {'dx': 0xFD})
        buf = to_ctypes(mem)
        al = lib.dirio_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0xFD))
        check("dirio_entry: read (0xFD)", ref_regs['bx'] & 0xFF, al)

        # -- dirio_entry: write (< 0xFD) --
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 0
        ref_regs, ref_img = run_reference(code, entries['dirio_entry'], mem, {'dx': ord('K')})
        buf = to_ctypes(mem)
        lib.dirio_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(ord('K')))
        check("dirio_entry: write, column advances", ref_img[ccb_addr(0) + C_COLUMN], bytes(buf)[ccb_addr(0) + C_COLUMN])

        # -- dirio_entry: status (0xFE) --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('N')  # ready
        ref_regs, _ = run_reference(code, entries['dirio_entry'], mem, {'dx': 0xFE})
        buf = to_ctypes(mem)
        al = lib.dirio_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0xFE))
        check("dirio_entry: status (0xFE), ready", ref_regs['bx'] & 0xFF, al)

        # -- dirio_entry: readif (0xFF), nothing buffered -> boundary stub, dispatches --
        mem = base_memory()
        ref_regs, _ = run_reference(code, entries['dirio_entry'], mem, {'dx': 0xFF})
        buf = to_ctypes(mem)
        al = lib.dirio_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0xFF))
        check("dirio_entry: readif (0xFF), nothing buffered", ref_regs['bx'] & 0xFF, al)

        # -- conwrite_entry: prints up to '$' --
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 0
        BUF = 0x4000
        msg = b'HEY$'
        mem[BUF:BUF + len(msg)] = msg
        ref_regs, ref_img = run_reference(code, entries['conwrite_entry'], mem, {'dx': BUF})
        buf = to_ctypes(mem)
        lib.conwrite_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUF))
        check("conwrite_entry: column reflects 3 chars", ref_img[ccb_addr(0) + C_COLUMN], bytes(buf)[ccb_addr(0) + C_COLUMN])

        # -- conprint_entry: a max-length cap stops it early --
        mem = base_memory()
        mem[ccb_addr(0) + C_COLUMN] = 0
        BUF = 0x4000
        mem[BUF:BUF + 5] = b'ABCDE'
        ref_regs, ref_img = run_reference(code, entries['conprint_entry'], mem, {'dx': BUF, 'bx': 0x0300})  # bh=3,bl=0
        buf = to_ctypes(mem)
        lib.conprint_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUF), ctypes.c_uint8(3), ctypes.c_uint8(0))
        check("conprint_entry: stops at max (3 chars)", ref_img[ccb_addr(0) + C_COLUMN], bytes(buf)[ccb_addr(0) + C_COLUMN])

        # -- conread_entry: cr as the very first character ends the line immediately --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = 0x0D  # cr
        BUF = 0x4000
        mem[BUF] = 10  # max length
        ref_regs, ref_img = run_reference(code, entries['conread_entry'], mem, {'dx': BUF})
        buf = to_ctypes(mem)
        lib.conread_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUF))
        ref = (ref_img[BUF + 1], ref_img[ccb_addr(0) + C_COLUMN])
        port = (bytes(buf)[BUF + 1], bytes(buf)[ccb_addr(0) + C_COLUMN])
        check("conread_entry: cr ends an empty line", ref, port)

        # -- conread_entry: a max-length-1 buffer accepts exactly one
        # character before readen fires on its own -- avoids needing a
        # second conin() read, which would only ever return the fixed
        # NUL artifact (see header comment) --
        mem = base_memory()
        mem[ccb_addr(0) + C_NCHAR] = ord('Q')
        BUF = 0x4000
        mem[BUF] = 1  # max length
        ref_regs, ref_img = run_reference(code, entries['conread_entry'], mem, {'dx': BUF})
        buf = to_ctypes(mem)
        lib.conread_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUF))
        ref = (ref_img[BUF + 1], ref_img[BUF + 2], ref_img[ccb_addr(0) + C_COLUMN])
        port = (bytes(buf)[BUF + 1], bytes(buf)[BUF + 2], bytes(buf)[ccb_addr(0) + C_COLUMN])
        check("conread_entry: one character fills a max-length-1 buffer", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
