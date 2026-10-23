#!/usr/bin/env python3
"""Differential test for src-c/kernel/tmp.c against src/rsp/TMP.A86's
nextcommand:/clicall:/clierror: body.

TMP.A86 assembles standalone (no wrapper fixture needed -- it has no
INCLUDEs at all). The reference run starts at `nextcommand` (skipping
`tmp:`'s own one-time setup, moot under the flat memory model -- see
tmp.c's own header comment) and stops at that same address -- every
real path through this pass ends in `jmp nextcommand`, same one-pass-
through-the-loop convention as test_echo.py's own `loop:`.

TMP.A86's own dispatcher calls reach `mpm:`'s `int mpmint ! ret` via a
tail-JMP chain through one of its own small per-function trampolines
(e.g. `prchar: mov cl,mpm_conout ! jmp mpm1` / `mpm1: jmp mpm`), unlike
ECHO.A86/CLOCK.A86's own direct `call mpm` at every site -- so
`call_stub_addrs` (which only intercepts a near CALL landing exactly on
a listed address) can't be used here. Instead this test stubs the `INT
mpmint` instruction itself via `int_stub_nums`, added to tools/
i86sim.py for this file: the no-op stub resumes at `mpm`'s own `ret`,
which unwinds correctly back to the original CALL site no matter how
many tail JMPs preceded it.

TMP.A86's own DSEG offsets (e.g. `clicb_cmd` at 0x1e0) land inside its
own 0x423-byte cseg once collapsed into the flat memory model, unlike
ECHO.A86/CLOCK.A86 where this happened not to collide. Poking test
input directly at those low addresses the way test_echo.py/test_clock.py
do would corrupt the real code itself, so the *reference* run instead
loads `cseg` at a shifted base (CSEG_BASE, well above every real DSEG
address this routine touches) and leaves the real DSEG fields at their
own unshifted, real addresses. This is safe because every control-flow
instruction TMP.A86 uses (`jmp`/`call`/`je`/...) is relative (assembled
as a displacement from the next instruction, confirmed via tools/asm86's
own near-jump/call encoding) so shifting the whole cseg block by a
constant preserves every internal branch; the only *absolute* same-cseg
address references this routine's own reachable path bakes in are
`offset prompt`/`offset catcherr`-style string addresses fed to
`printstring`/`print_ds_string`, both boundary-stub no-ops this port
never dereferences anyway. `tmp.c` itself still uses the separate
SD_TMP_* scratch addresses from defs.h (its only option, since it has
no "real code region" of its own to collide with).
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

TMP_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'TMP.A86')
TMP_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'tmp.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
MPMINT = 224
CSEG_BASE = 0x1000  # clear of every real DSEG address this routine touches

SD_TMP_CMDSENT = 0xA070
SD_TMP_READ_BLEN = 0xA071
SD_TMP_CLICB_CMD = 0xA100
SD_TMP_SAVEBUF = 0xA200


def assemble_reference():
    asm = Assembler()
    asm.assemble(TMP_A86, [])
    code = asm.segment_bytes('cseg')
    nextcommand = asm.symtab['nextcommand'].value
    real_cmdsent = asm.symtab['cmdsent'].value
    real_read_blen = asm.symtab['read_blen'].value
    real_clicb_cmd = asm.symtab['clicb_cmd'].value
    real_savebuf = asm.symtab['savebuf'].value
    return code, nextcommand, real_cmdsent, real_read_blen, real_clicb_cmd, real_savebuf


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, TMP_C, '-o', lib_path],
        check=True,
    )
    return lib_path


SAVEBUF_SENTINEL = bytes((i & 0xFF) for i in range(129))


def base_memory(cmdsent, read_blen, cmd_bytes):
    """Build the port's own memory image (SD_TMP_* scratch addresses).

    savebuf is pre-filled with a sentinel pattern (rather than left
    zeroed) so a case that never reaches clicall -- and so never
    touches savebuf on either side -- compares equal to the reference's
    own untouched content, instead of comparing a cleared scratch
    region against whatever unrelated cseg bytes happen to sit at
    savebuf's real (colliding) address there.
    """
    mem = bytearray(0x10000)
    mem[SD_TMP_CMDSENT] = cmdsent
    mem[SD_TMP_READ_BLEN] = read_blen
    mem[SD_TMP_CLICB_CMD:SD_TMP_CLICB_CMD + len(cmd_bytes)] = cmd_bytes
    mem[SD_TMP_SAVEBUF:SD_TMP_SAVEBUF + 129] = SAVEBUF_SENTINEL
    return mem


def run_reference(code, nextcommand, real_addrs, cmdsent, read_blen, cmd_bytes):
    real_cmdsent, real_read_blen, real_clicb_cmd, real_savebuf = real_addrs
    image = bytearray(0x10000)
    image[CSEG_BASE:CSEG_BASE + len(code)] = code
    image[real_cmdsent] = cmdsent
    image[real_read_blen] = read_blen
    image[real_clicb_cmd:real_clicb_cmd + len(cmd_bytes)] = cmd_bytes
    image[real_savebuf:real_savebuf + 129] = SAVEBUF_SENTINEL
    entry = CSEG_BASE + nextcommand
    sim = I86Sim(image, {'sp': STACK_TOP}, entry,
                 stop_addrs={entry}, int_stub_nums={MPMINT})
    sim.step()
    sim.run()
    return (image[real_cmdsent],
            bytes(image[real_clicb_cmd:real_clicb_cmd + 129]),
            bytes(image[real_savebuf:real_savebuf + 129]))


def run_port(lib, mem):
    buf = (ctypes.c_uint8 * 0x10000)(*mem)
    lib.nextcommand_once(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)))
    image = bytes(buf)
    return (image[SD_TMP_CMDSENT],
            image[SD_TMP_CLICB_CMD:SD_TMP_CLICB_CMD + 129],
            image[SD_TMP_SAVEBUF:SD_TMP_SAVEBUF + 129])


def pad(cmd):
    return cmd + b' ' * (128 - len(cmd))


CASES = [
    ("empty command, cmdsent clear", 0, 0, pad(b'')),
    ("empty command, cmdsent set (cleared)", 1, 0, pad(b'')),
    ("';'-prefixed command, ignored", 0, 3, pad(b';xx')),
    ("valid disk-change shortcut 'A:'", 0, 2, pad(b'A:')),
    ("disk-change shortcut, lowercase 'b:'", 0, 2, pad(b'b:')),
    ("disk-change shortcut, out of range '~:'", 0, 2, pad(b'~:')),
    ("2-char command, not a shortcut (no ':')", 0, 2, pad(b'ls')),
    ("ordinary command -> clicall -> catch-all error", 0, 6, pad(b'DIR A:')),
    ("ordinary command, cmdsent was set", 1, 4, pad(b'TYPE')),
]


def main():
    code, nextcommand, cmdsent_a, read_blen_a, clicb_cmd_a, savebuf_a = assemble_reference()
    real_addrs = (cmdsent_a, read_blen_a, clicb_cmd_a, savebuf_a)
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.nextcommand_once.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        failures = []
        for label, cmdsent, read_blen, cmd_bytes in CASES:
            mem = base_memory(cmdsent, read_blen, cmd_bytes)
            ref = run_reference(code, nextcommand, real_addrs, cmdsent, read_blen, cmd_bytes)
            port = run_port(lib, mem)
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}")
            if status == "FAIL":
                print(f"    reference cmdsent={ref[0]} clicb_cmd={ref[1][:10]!r} savebuf={ref[2][:10]!r}")
                print(f"    port      cmdsent={port[0]} clicb_cmd={port[1][:10]!r} savebuf={port[2][:10]!r}")
                failures.append(label)
        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print(f"\nall {len(CASES)} cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
