#!/usr/bin/env python3
"""Differential test for src-c/kernel/command.c against src/kernel/
COMMAND.SUP's shifttail, fileopen, user_retf -- the only 3 routines in
that file this port covers. See command.c's own header comment for why
chain_entry/clicmd_entry (and everything only they call) are deferred:
a real segment-relative-addressing gap in cli_fload's own chain-mode
UDA copy, same class as PROC.RTM/ML.MEM.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'command_wrapper.A86')
COMMAND_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'command.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR = 0xA000
CLI_FCB, CLI_CMDTAIL = 0xB300, 0xB360
FCB_NAME = 0x01
P_FLAG = 0x06
PF_SYS, PF_KEEP, PF_RAW, PF_CTLC, PF_TEMPKEEP, PF_CTLD = 0x0001, 0x0002, 0x0040, 0x0080, 0x0200, 0x0400

PD = 0x5000
MPMIF_STUB = 0x9000


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in ('shifttail', 'fileopen', 'user_retf')}
    # user_retf ends in "int mpmint", not a near call -- tools/i86sim.py
    # has no INT support at all, so this is modeled the same way a tail
    # JMP into an unported routine is (docs/src-c.md's own convention):
    # a stop_addrs entry at the INT instruction's own address, found by
    # scanning for its opcode (0xCD) within user_retf's own byte range,
    # since nothing after it matters (it's the routine's last
    # instruction either way).
    chunk = code[entries['user_retf']:entries['user_retf'] + 40]
    int_offset = chunk.index(b'\xcd')
    entries['user_retf_int'] = entries['user_retf'] + int_offset
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, COMMAND_C, '-o', lib_path],
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
    return mem


def run_reference(code, entry, mem, regs, stop_addrs=()):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs={MPMIF_STUB})
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.shifttail.argtypes = [c_u8p, ctypes.c_uint16]
        lib.fileopen.argtypes = [c_u8p]
        lib.user_retf.argtypes = [c_u8p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- shifttail: ax == 0 -> clears cli_cmdtail --
        mem = base_memory()
        mem[CLI_CMDTAIL] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['shifttail'], mem, {'ax': 0})
        buf = to_ctypes(mem)
        lib.shifttail(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        check("shifttail: no tail -> cli_cmdtail cleared", ref_img[CLI_CMDTAIL], bytes(buf)[CLI_CMDTAIL])

        # -- shifttail: a real tail gets shifted down to the buffer start --
        mem = base_memory()
        TAIL_AT = CLI_CMDTAIL + 10
        mem[TAIL_AT:TAIL_AT + 6] = b'HELLO$'
        ref_regs, ref_img = run_reference(code, entries['shifttail'], mem, {'ax': TAIL_AT})
        buf = to_ctypes(mem)
        lib.shifttail(ctypes.cast(buf, c_u8p), ctypes.c_uint16(TAIL_AT))
        ref = bytes(ref_img[CLI_CMDTAIL:CLI_CMDTAIL + 6])
        port = bytes(buf)[CLI_CMDTAIL:CLI_CMDTAIL + 6]
        check("shifttail: tail shifted to buffer start", ref, port)

        # -- fileopen: sets the read-only bit on cli_fcb's own name[5] --
        mem = base_memory()
        mem[CLI_FCB + FCB_NAME + 5] = 0x41  # some pre-existing bits
        ref_regs, ref_img = run_reference(code, entries['fileopen'], mem, {})
        buf = to_ctypes(mem)
        lib.fileopen(ctypes.cast(buf, c_u8p))
        check("fileopen: sets the read-only bit", ref_img[CLI_FCB + FCB_NAME + 5], bytes(buf)[CLI_FCB + FCB_NAME + 5])

        # -- user_retf: clears pf_keep/pf_tempkeep/pf_ctlc/pf_sys, leaves
        # every other bit (including pf_raw/pf_ctld here) untouched --
        mem = base_memory()
        mem_w16(mem, PD + P_FLAG, PF_SYS | PF_KEEP | PF_RAW | PF_CTLC | PF_TEMPKEEP | PF_CTLD)
        ref_regs, ref_img = run_reference(code, entries['user_retf'], mem, {}, stop_addrs={entries['user_retf_int']})
        buf = to_ctypes(mem)
        lib.user_retf(ctypes.cast(buf, c_u8p))
        ref = mem_r16(ref_img, PD + P_FLAG)
        port = mem_r16(bytes(buf), PD + P_FLAG)
        check("user_retf: clears keep/tempkeep/ctlc/sys, not raw/ctld", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
