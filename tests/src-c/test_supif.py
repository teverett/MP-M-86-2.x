#!/usr/bin/env python3
"""Differential test for src-c/kernel/supif.c against src/kernel/
SUPIF.SUP's `func1` -- now including its real `mod == sup` dispatch-
through into the 15-entry `supfunc` table, not just the (mod,func)
resolution that used to be this test's whole job. See supif.c's own
header comment for what's still not ported (`user_entry`/`func`/
`entry`/`mpmif`/`xiosif`) and why.

`func1` has a genuine "DS = sysdat segment" precondition (same class as
src-c/kernel/file1.c's own BD_* fields), so this test runs the reference
with `tools/i86sim.py`'s `segmented=True` mode and DS set to a real
segment value (DS_SYSDAT); `module_map`/`sysent` are written directly at
their real small SYSDAT.FMT offsets (0x46/0xA0) relative to that
segment's own physical base, not at the wrapper's own dseg placement of
those fields (which only exists to make the file assemble -- nothing in
that dseg blob is read by this test).

The 15 real supfunc targets are marker stubs in
tests/src-c/fixtures/supif_wrapper.A86 (see its own header comment),
not the real routines -- each target is already independently
differential-tested elsewhere (test_sysfunc.py/test_load.py/
test_command.py/test_rpl.py/test_parse.py). This test's own job is
`func1`'s real dispatch logic itself: for every (mod,func) that
resolves to `sup`, does it land on the *right* slot, and does `bx`
arrive forced (load_entry=0, cload_entry=1) or ambient (every other
slot, including bare `load`) exactly as supif.c's own header comment
claims. A stub leaves `bx` untouched and returns a slot-specific `cx`
marker (`0x9000 + slot #`), so both checks come from one (bx,cx) pair.

`mod != sup` (the real cross-module far call, `gofar`) has no C target
at all -- supif.c's own `func1` just returns (mod,func) without
dispatching, same as before this test's own expansion; still covered
by the `ch != 0` case below (mod=2, picked so it's provably != sup).
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'supif_wrapper.A86')
# func1 now dispatches for real into these targets too (see supif.c's
# own header comment) -- link all five, even though the targets' own
# full behavior is independently tested elsewhere (see this file's
# own module docstring).
SUPIF_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'supif.c'),
           os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'supif_test_stubs.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
DS_SYSDAT = 0x4321
SD_MODULE_MAP, SD_SYSENT, ENT_MOD, ENT_FUNC, ENTLEN = 0x46, 0xA0, 0, 1, 3
SUP_MODULE = 1

SUPFUNC_LABELS = [
    'not_implemented', 'bad_entry', 'version_entry', 'callbios_entry', 'load_entry',
    'clicmd_entry', 'rpl_entry', 'parse_entry', 'sysdat_entry', 'tod_entry',
    'load', 'mpmversion_entry', 'chain_entry', 'serial_entry', 'cload_entry',
]


def phys(seg, offset=0):
    return (seg * 16 + offset) & 0xFFFF


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    func1 = asm.symtab['func1'].value
    inmod = asm.symtab['inmod'].value
    load_ret = asm.symtab['load'].value + 4
    assert code[load_ret] == 0xC3, f"expected ret at load+4 ({hex(load_ret)})"
    stub_ret = {'load': load_ret, 'load_entry': load_ret, 'cload_entry': load_ret}
    for label in SUPFUNC_LABELS:
        if label in stub_ret:
            continue
        addr = asm.symtab[label].value
        assert code[addr + 4] == 0xC3, f"expected ret at {label}+4 ({hex(addr + 4)})"
        stub_ret[label] = addr + 4
    return code, func1, inmod, stub_ret


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *SUPIF_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, func1, stop_addr, mem, ch, cl, dx=0, bx=0):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {'cx': (ch << 8) | cl, 'dx': dx, 'bx': bx, 'ds': DS_SYSDAT, 'sp': STACK_TOP},
                 func1, stop_addrs={stop_addr}, segmented=True)
    sim.run()
    return sim.regs


def base_memory(sysent_entries=()):
    mem = bytearray(0x10000)
    for func_num, mod, func in sysent_entries:
        si = phys(DS_SYSDAT, SD_SYSENT + func_num * ENTLEN)
        mem[si + ENT_MOD] = mod
        mem[si + ENT_FUNC] = func
    return mem


class ModFuncBxCx(ctypes.Structure):
    _fields_ = [('mod', ctypes.c_uint8), ('func', ctypes.c_uint8),
                ('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16),
                ('reached_xios', ctypes.c_uint8),
                ('al', ctypes.c_uint8), ('ah', ctypes.c_uint8),
                ('cl', ctypes.c_uint8), ('dl', ctypes.c_uint8)]


def run_port(lib, mem, ch, cl, dx=0, bx=0):
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.func1.restype = ModFuncBxCx
    lib.func1.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8,
                           ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
    r = lib.func1(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), DS_SYSDAT, ch, cl, dx, bx,
                  0, 0, 0)
    return r


# (label, ch, cl, dx, bx, sysent_entries, expect_slot_label, expect_bx)
# expect_bx=None means "ambient bx (the bx this case passed in)".
RESOLUTION_CASES = [
    ("ch != 0, mod != sup (gofar, no dispatch)", 2, 7, 0x1111, 0x2222, (), None, None),
    ("ch == 0, sysent[0] -> (mod=sup, func=0)", 0, 0, 0, 0, [(0, SUP_MODULE, 0)], 'not_implemented', None),
    ("ch == 0, sysent[12] -> (mod=sup, func=11)", 0, 12, 0, 0, [(12, SUP_MODULE, 11)], 'mpmversion_entry', None),
]

DISPATCH_CASES = [
    ("func 0: not_implemented", 0, 'not_implemented', None),
    ("func 1: bad_entry", 1, 'bad_entry', None),
    ("func 2: version_entry", 2, 'version_entry', None),
    ("func 3: callbios_entry", 3, 'callbios_entry', None),
    ("func 4: load_entry (bx forced to 0)", 4, 'load_entry', 0),
    ("func 5: clicmd_entry", 5, 'clicmd_entry', None),
    ("func 6: rpl_entry", 6, 'rpl_entry', None),
    ("func 7: parse_entry", 7, 'parse_entry', None),
    ("func 8: sysdat_entry", 8, 'sysdat_entry', None),
    ("func 9: tod_entry", 9, 'tod_entry', None),
    ("func 10: load (ambient bx)", 10, 'load', None),
    ("func 11: mpmversion_entry", 11, 'mpmversion_entry', None),
    ("func 12: chain_entry", 12, 'chain_entry', None),
    ("func 13: serial_entry", 13, 'serial_entry', None),
    ("func 14: cload_entry (bx forced to 1)", 14, 'cload_entry', 1),
]

AMBIENT_BX = 0x55AA
AMBIENT_DX = 0x1234


def main():
    code, func1, inmod, stub_ret = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        for label, ch, cl, dx, bx, sysent_entries, slot, expect_bx in RESOLUTION_CASES:
            mem = base_memory(sysent_entries)
            stop_addr = stub_ret[slot] if slot else inmod
            ref_regs = run_reference(code, func1, stop_addr, mem, ch, cl, dx, bx)
            port = run_port(lib, mem, ch, cl, dx, bx)
            ref = (ref_regs['ax'] & 0xFF, ref_regs['cx'] & 0xFF)
            check(f"{label} (mod,func)", ref, (port.mod, port.func))

        for label, func, slot, expect_bx in DISPATCH_CASES:
            mem = base_memory()
            bx = AMBIENT_BX
            ref_regs = run_reference(code, func1, stub_ret[slot], mem, SUP_MODULE, func, AMBIENT_DX, bx)
            port = run_port(lib, mem, SUP_MODULE, func, AMBIENT_DX, bx)
            ref_bx = ref_regs['bx'] & 0xFFFF
            ref_cx = ref_regs['cx'] & 0xFFFF
            check(f"{label}: slot marker (cx)", ref_cx, port.cx)
            check(f"{label}: bx", ref_bx, port.bx)
            if expect_bx is not None:
                # also pin down what "forced" actually means here, not
                # just "reference == port"
                check(f"{label}: bx is forced to {expect_bx}", expect_bx, port.bx)
            else:
                check(f"{label}: bx stays ambient ({hex(AMBIENT_BX)})", AMBIENT_BX, port.bx)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {len(RESOLUTION_CASES) + len(DISPATCH_CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
