#!/usr/bin/env python3
"""Differential test for src-c/kernel/tmp.c against src/rsp/TMP.A86's
nextcommand:/clicall:/goodparse:/try_builtin:/not_builtin:/clierror:
body.

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
a listed address) can't be used here. This test uses `int_hooks`
instead: ONE hook on `INT mpmint` itself, which inspects CL (the real
function code) and, for `mpm_parse`/`mpm_clicmd` (152/150), makes a
REAL ctypes call into this SAME shared library's own `parse_entry`/
`clicmd_entry` (`src-c/kernel/parse.c`/`command.c`) against the SAME
`sim.mem` buffer (via `ctypes.c_uint8.from_buffer`, a zero-copy view --
writes the C call makes land directly in `sim.mem`) -- every other CL
value is a pure no-op, same as the old blanket `int_stub_nums={224}`
this replaces. This mirrors `src-c/loader/ldmpm.c`'s own real extern
call into `init_entry`, just driven from the Python side of a
differential test instead of from another C file.

That reflects a real correction, not just new coverage: `parsefilename`
was previously treated as an unidentified boundary stub whose own `cx`
came back deterministically as `mpm_parse` (152, pre-loaded right
before the no-op "call"), making `goodparse` and everything downstream
*provably unreachable* -- see the git history of this file's own
header comment for the full (now corrected) writeup. `parsefilename`'s
real target turned out to be `parse_entry`, already ported; `not_
builtin`'s own `mpm_clicmd` turned out to be `clicmd_entry`, already
ported too. Both are now called for real on both sides.

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

`not_builtin`'s own real `clicmd_entry` call needs the same supporting
fixture tests/src-c/test_command.py's own base_memory() already
establishes (SD_RLR/UDA_WRKSEG/an MXCli queue with one token, so
`mxread`'s own unconditional call doesn't retry forever under the
boundary-stubbed `mpmif(f_sleep)` convention) -- reused here verbatim,
and now needed for every case that reaches `clicall` (even the two
pre-existing "ordinary command" cases below), not just new ones.
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
TMP_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in
         ('tmp.c', 'command.c', 'que1.c', 'que2.c', 'console.c', 'chario.c', 'parse.c',
          'file1.c', 'file2.c', 'file3.c', 'file4.c',
          'load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
MPMINT = 224
MPM_PARSE = 152
MPM_CLICMD = 150
CSEG_BASE = 0x1000  # clear of every real DSEG address this routine touches

SD_TMP_CMDSENT = 0xA070
SD_TMP_READ_BLEN = 0xA071
SD_TMP_CLICB_CMD = 0xA100
SD_TMP_SAVEBUF = 0xA200

# command.c's own scratch fixture (tests/src-c/test_command.py's own
# base_memory(), reused verbatim -- see this file's own header comment)
SD_RLR = 0xA000
UDA_WRKSEG = 0xA012
SD_QLR = 0xA020
MXCLIQPB = 0xB400
Q_LINK, Q_FLAGS, Q_MSGLEN, Q_NMSGS, Q_MSGCNT = 0x00, 0x04, 0x0E, 0x10, 0x16
QF_MX = 0x01
QPB_QADDR = 0x02
MXQD = 0x9000
PD = 0x5000


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def add_command_fixture(mem):
    mem_w16(mem, SD_RLR, PD)
    mem_w16(mem, UDA_WRKSEG, 0)
    mem_w16(mem, SD_QLR, MXQD)
    mem_w16(mem, MXQD + Q_LINK, 0)
    mem_w16(mem, MXQD + Q_FLAGS, QF_MX)
    mem_w16(mem, MXQD + Q_MSGLEN, 0)
    mem_w16(mem, MXQD + Q_NMSGS, 1)
    mem_w16(mem, MXQD + Q_MSGCNT, 1)
    mem_w16(mem, MXCLIQPB + QPB_QADDR, MXQD)


# Real print-adjacent routines this file never used to reach (clicall
# used to dead-end at a fake, deterministic boundary-stub result --
# see this file's own header comment) -- stubbed out here (a true
# no-op, skipping their own body) now that goodparse/try_builtin are
# genuinely reachable: `print_ds_string`'s own `mov ax,cs ! mov
# ds,ax` prologue computes ITS OWN addressing basis from the live
# `cs` register, which this test's own CSEG_BASE shift does not (and
# cannot, under the flat/non-segmented addressing this whole file
# otherwise relies on) adjust to match -- letting it actually run
# sends its own string scan hunting for a `$` terminator that was
# never placed at the (unshifted) address it ends up reading from,
# spinning for a very long time instead of crashing outright. None
# of these has an observable memory effect anyway (same "boundary
# stub, nothing to model" class as every other print call in this
# file), so stubbing them outright costs nothing.
PRINT_STUBS = ('crlf', 'getlist', 'getuser', 'prchar', 'print_ds_string',
               'printstring', 'prnum', 'setlist', 'setuser')


def assemble_reference():
    asm = Assembler()
    asm.assemble(TMP_A86, [])
    code = asm.segment_bytes('cseg')
    nextcommand = asm.symtab['nextcommand'].value
    real_cmdsent = asm.symtab['cmdsent'].value
    real_read_blen = asm.symtab['read_blen'].value
    real_clicb_cmd = asm.symtab['clicb_cmd'].value
    real_savebuf = asm.symtab['savebuf'].value
    real_fcb = asm.symtab['fcb'].value
    print_stubs = {CSEG_BASE + asm.symtab[name].value for name in PRINT_STUBS}
    # usercmd/printercmd are cseg-resident DATA (not a scratch field),
    # so their own real content needs the same unshifted-address
    # treatment as cmdsent/read_blen/etc: `try_builtin`'s own `mov
    # di,offset usercmd` bakes in the real, UNSHIFTED address as an
    # immediate (same as any absolute same-cseg data reference -- see
    # this file's own header comment on why control-flow survives a
    # shift but an absolute data reference like this one doesn't),
    # so without this, the comparison reads whatever's actually at
    # that low address post-shift (nothing -- it's below CSEG_BASE),
    # never matches "USER    "/"PRINTER ", and always falls through
    # to not_builtin regardless of what was actually typed.
    usercmd_addr = asm.symtab['usercmd'].value
    printercmd_addr = asm.symtab['printercmd'].value
    real_names = {usercmd_addr: code[usercmd_addr:usercmd_addr + 8],
                  printercmd_addr: code[printercmd_addr:printercmd_addr + 8]}
    return (code, nextcommand, real_cmdsent, real_read_blen, real_clicb_cmd, real_savebuf, real_fcb,
            print_stubs, real_names)


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *TMP_C, '-o', lib_path],
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
    add_command_fixture(mem)
    return mem


def make_int_hook(lib, c_u8p, ParseBxCx, CmdBxCx):
    def hook(sim):
        cl = sim.regs['cx'] & 0xFF
        dx = sim.regs['dx']
        buf = (ctypes.c_uint8 * 0x10000).from_buffer(sim.mem)
        ptr = ctypes.cast(buf, c_u8p)
        if cl == MPM_PARSE:
            r = lib.parse_entry(ptr, ctypes.c_uint16(dx))
            sim.regs['bx'] = r.bx
            sim.regs['cx'] = r.cx
        elif cl == MPM_CLICMD:
            # sysdat/user_retf_offset/cs only matter on cli_gopen's own
            # real f_load path (load.c's load) -- never reached here
            # (no SD_PUL free-PD list set up), so 0 is inert.
            r = lib.clicmd_entry(ptr, ctypes.c_uint16(dx), ctypes.c_uint16(0),
                                  ctypes.c_uint16(0), ctypes.c_uint16(0))
            sim.regs['bx'] = r.bx
            sim.regs['cx'] = r.cx
        # else: every other real `mpm` call (print/console/disk-select/
        # setuser/setlist/...) stays a pure no-op, same as the old
        # blanket int_stub_nums={224} this replaces.
    return hook


def run_reference(code, nextcommand, real_addrs, cmdsent, read_blen, cmd_bytes, int_hook, print_stubs, real_names):
    real_cmdsent, real_read_blen, real_clicb_cmd, real_savebuf = real_addrs
    image = bytearray(0x10000)
    image[CSEG_BASE:CSEG_BASE + len(code)] = code
    image[real_cmdsent] = cmdsent
    image[real_read_blen] = read_blen
    image[real_clicb_cmd:real_clicb_cmd + len(cmd_bytes)] = cmd_bytes
    image[real_savebuf:real_savebuf + 129] = SAVEBUF_SENTINEL
    for addr, data in real_names.items():
        image[addr:addr + len(data)] = data
    add_command_fixture(image)
    entry = CSEG_BASE + nextcommand
    sim = I86Sim(image, {'sp': STACK_TOP}, entry,
                 stop_addrs={entry}, int_hooks={MPMINT: int_hook},
                 call_stub_addrs=print_stubs)
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
    ("ordinary command, explicit drive -> not_builtin -> clicmd_entry",
     0, 6, pad(b'DIR A:')),
    ("ordinary command, no drive, not a builtin -> not_builtin",
     1, 4, pad(b'TYPE')),
    ("USER builtin, no argument", 0, 4, pad(b'USER')),
    ("USER builtin, with a 1-digit argument", 0, 6, pad(b'USER 5')),
    ("PRINTER builtin, with a 2-digit argument", 0, 11, pad(b'PRINTER 12')),
]


def main():
    (code, nextcommand, cmdsent_a, read_blen_a, clicb_cmd_a, savebuf_a, fcb_a,
     print_stubs, real_names) = assemble_reference()
    real_addrs = (cmdsent_a, read_blen_a, clicb_cmd_a, savebuf_a)
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class ParseBxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        class CmdBxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.nextcommand_once.argtypes = [c_u8p]
        lib.parse_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.parse_entry.restype = ParseBxCx
        lib.clicmd_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
        lib.clicmd_entry.restype = CmdBxCx

        int_hook = make_int_hook(lib, c_u8p, ParseBxCx, CmdBxCx)

        failures = []
        for label, cmdsent, read_blen, cmd_bytes in CASES:
            mem = base_memory(cmdsent, read_blen, cmd_bytes)
            ref = run_reference(code, nextcommand, real_addrs, cmdsent, read_blen, cmd_bytes, int_hook,
                                 print_stubs, real_names)
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
