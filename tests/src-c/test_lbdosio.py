#!/usr/bin/env python3
"""Differential test for src-c/loader/lbdosio.c against src/loader/
LBDOSIO.A86's conbrk, conout, tabout, crlf, print, func9, staret,
funcret, setlret1. func1/func3/func6/func7/func8/func10/func11 (and
func2, an alias for tabout itself) are not ported -- see lbdosio.c's
own header comment for why."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'lbdosio_wrapper.A86')
LBDOSIO_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'lbdosio.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0
LD_KBCHAR, LD_COMPCOL, LD_COLUMN, LD_LISTCP, LD_ARET = 0xB000, 0xB001, 0xB002, 0xB003, 0xB004


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'loader')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in (
        'conbrk', 'conout', 'tabout', 'crlf', 'print', 'func9', 'staret', 'funcret', 'setlret1')}
    stubs = {asm.symtab[name].value for name in ('constf', 'coninf', 'conouf', 'listf', 'wbootf')}
    return code, entries, stubs


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, LBDOSIO_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    return bytearray(0x10000)


def run_reference(code, entry, mem, regs, stubs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, call_stub_addrs=stubs)
    sim.run()
    return sim.regs, image


def main():
    code, entries, stubs = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.conbrk.restype = ctypes.c_uint8
        lib.conbrk.argtypes = [c_u8p]
        lib.conout.argtypes = [c_u8p, ctypes.c_uint8]
        lib.tabout.argtypes = [c_u8p, ctypes.c_uint8]
        lib.crlf.argtypes = [c_u8p]
        lib.print.argtypes = [c_u8p, ctypes.c_uint16]
        lib.func9.argtypes = [c_u8p, ctypes.c_uint16]
        lib.staret.argtypes = [c_u8p, ctypes.c_uint8]
        lib.funcret.argtypes = [c_u8p]
        lib.setlret1.argtypes = [c_u8p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- conbrk: a buffered character is already ready --
        mem = base_memory()
        mem[LD_KBCHAR] = ord('X')
        ref_regs, ref_img = run_reference(code, entries['conbrk'], mem, {}, stubs)
        buf = to_ctypes(mem)
        al = lib.conbrk(ctypes.cast(buf, c_u8p))
        check("conbrk: already-buffered character", ((ref_regs['ax'] & 0xFF), ref_img[LD_KBCHAR]), (al, bytes(buf)[LD_KBCHAR]))

        # -- conbrk: nothing buffered -- always "not ready" under the
        # boundary-stub convention (constf's own no-op leaves AL at 0) --
        mem = base_memory()
        mem[LD_KBCHAR] = 0
        ref_regs, _ = run_reference(code, entries['conbrk'], mem, {}, stubs)
        buf = to_ctypes(mem)
        al = lib.conbrk(ctypes.cast(buf, c_u8p))
        check("conbrk: nothing buffered, boundary stub -> not ready", (ref_regs['ax'] & 0xFF), al)

        # -- conout: a plain graphic character, compcol set (measure-only,
        # no real write side effects to check) --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 5
        ref_regs, ref_img = run_reference(code, entries['conout'], mem, {'cx': ord('Q')}, stubs)
        buf = to_ctypes(mem)
        lib.conout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(ord('Q')))
        check("conout: graphic char, column advances", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- conout: backspace (ctlh) decrements column twice total --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 5
        ref_regs, ref_img = run_reference(code, entries['conout'], mem, {'cx': 0x08}, stubs)
        buf = to_ctypes(mem)
        lib.conout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x08))
        check("conout: backspace", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- conout: line feed resets column to 0 --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 5
        ref_regs, ref_img = run_reference(code, entries['conout'], mem, {'cx': 0x0A}, stubs)
        buf = to_ctypes(mem)
        lib.conout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x0A))
        check("conout: line feed resets column", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- conout: rubout -- no column change at all --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 5
        ref_regs, ref_img = run_reference(code, entries['conout'], mem, {'cx': 0x7F}, stubs)
        buf = to_ctypes(mem)
        lib.conout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x7F))
        check("conout: rubout, no column change", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- conout: real write path (compcol clear) -- conbrk/conouf/
        # listf calls have no observable effect, column still updates --
        mem = base_memory()
        mem[LD_COMPCOL] = 0
        mem[LD_COLUMN] = 0
        mem[LD_LISTCP] = 1
        ref_regs, ref_img = run_reference(code, entries['conout'], mem, {'cx': ord('A')}, stubs)
        buf = to_ctypes(mem)
        lib.conout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(ord('A')))
        check("conout: real write path, column still advances", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- tabout: non-tab character is a direct tail-call to conout --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 3
        ref_regs, ref_img = run_reference(code, entries['tabout'], mem, {'cx': ord('Z')}, stubs)
        buf = to_ctypes(mem)
        lib.tabout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(ord('Z')))
        check("tabout: non-tab tail-calls conout", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- tabout: a tab from column 3 advances to the next multiple of 8 --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 3
        ref_regs, ref_img = run_reference(code, entries['tabout'], mem, {'cx': 0x09}, stubs)
        buf = to_ctypes(mem)
        lib.tabout(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x09))
        check("tabout: tab advances to next multiple of 8", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- crlf: cr then lf -- column ends at 0 either way --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 7
        ref_regs, ref_img = run_reference(code, entries['crlf'], mem, {}, stubs)
        buf = to_ctypes(mem)
        lib.crlf(ctypes.cast(buf, c_u8p))
        check("crlf: column ends at 0", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- print: scans a message until '$', each char via tabout --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 0
        MSG = 0x4000
        msg = b'HI$'
        mem[MSG:MSG + len(msg)] = msg
        ref_regs, ref_img = run_reference(code, entries['print'], mem, {'cx': MSG}, stubs)
        buf = to_ctypes(mem)
        lib.print(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MSG))
        check("print: scans to '$', column reflects 2 chars", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- func9: same as print, via dx --
        mem = base_memory()
        mem[LD_COMPCOL] = 1
        mem[LD_COLUMN] = 0
        MSG = 0x4000
        msg = b'OK$'
        mem[MSG:MSG + len(msg)] = msg
        ref_regs, ref_img = run_reference(code, entries['func9'], mem, {'dx': MSG}, stubs)
        buf = to_ctypes(mem)
        lib.func9(ctypes.cast(buf, c_u8p), ctypes.c_uint16(MSG))
        check("func9: prints via dx", ref_img[LD_COLUMN], bytes(buf)[LD_COLUMN])

        # -- staret: sets lret (aret's low byte) without touching the high byte --
        mem = base_memory()
        mem[LD_ARET] = 0x00
        mem[LD_ARET + 1] = 0x12
        ref_regs, ref_img = run_reference(code, entries['staret'], mem, {'ax': 0x34}, stubs)
        buf = to_ctypes(mem)
        lib.staret(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0x34))
        check("staret: sets lret, high byte untouched", ref_img[LD_ARET:LD_ARET + 2], bytes(buf)[LD_ARET:LD_ARET + 2])

        # -- funcret: bare return, no observable effect --
        mem = base_memory()
        mem[LD_ARET] = 0x55
        ref_regs, ref_img = run_reference(code, entries['funcret'], mem, {}, stubs)
        buf = to_ctypes(mem)
        lib.funcret(ctypes.cast(buf, c_u8p))
        check("funcret: no observable effect", ref_img[LD_ARET], bytes(buf)[LD_ARET])

        # -- setlret1: sets lret = 1 --
        mem = base_memory()
        mem[LD_ARET] = 0x00
        mem[LD_ARET + 1] = 0x99
        ref_regs, ref_img = run_reference(code, entries['setlret1'], mem, {}, stubs)
        buf = to_ctypes(mem)
        lib.setlret1(ctypes.cast(buf, c_u8p))
        check("setlret1: lret = 1", ref_img[LD_ARET:LD_ARET + 2], bytes(buf)[LD_ARET:LD_ARET + 2])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
