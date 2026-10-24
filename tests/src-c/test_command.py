#!/usr/bin/env python3
"""Differential test for src-c/kernel/command.c against src/kernel/
COMMAND.SUP's shifttail, fileopen, user_retf, and clicmd_entry (the
non-chain path -- see command.c's own header comment for why
chain_entry/chain mode is excluded).

clicmd_entry's own reference run is against command_wrapper.A86, which
bundles QUE1.RTM (openq_entry/readq_entry/writeq_entry/cwriteq_entry),
CONSOLE.CIO+CHARIO.CIO (cconattch_entry/conassign_entry/conout_entry/
conprint_entry), and PARSE.SUP (parse_entry) for real, via a real
`mpmif` dispatcher assembled right into the wrapper (see its own
header comment) -- not a flat Python stub like the other 3 routines'
own simpler reference run still uses.

Run entirely non-segmented (the project-wide default, same as
que_wrapper.A86/console_wrapper.A86/parse_wrapper.A86's own tests):
every "mov ds,u_wrkseg"/"mov es,u_wrkseg" in the bundled real asm is a
no-op under i86sim.py's own non-segmented convention (only the offset
matters, the segment register's value is ignored), which is exactly
the simplification src-c/kernel/*.c's own flat SD_*/CLI_* addressing
throughout this whole project already assumes. UDA_WRKSEG is set to 0
below for the same reason: clicmd_entry's own `cli_copy` step is the
one place in this file that does genuine segment arithmetic
(`mem_r8_seg(mem, wrkseg, ...)`), and with wrkseg=0 that's
`seg*16+off == off`, matching the non-segmented reference run's own
flat read of the exact same bytes.
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
COMMAND_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in
             ('command.c', 'que1.c', 'que2.c', 'console.c', 'chario.c', 'parse.c',
              'file1.c', 'file2.c', 'file3.c', 'file4.c',
              'load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR = 0xA000
UDA_WRKSEG = 0xA012
SD_QLR = 0xA020
SD_PUL = 0xA038
SD_DAYFILE, SD_SRCHDISK = 0xA068, 0xA069
SD_CLI_ERR = 0xA058

CLI_FCB, CLI_CUSPQPB, CLI_CMDTAIL = 0xB300, 0xB320, 0xB360
CLI_PCB = 0xB350
MXCLIQPB = 0xB400
PCB_FLNMPTR, PCB_FCBPTR = 0x00, 0x02
FCB_DR, FCB_NAME, FCB_TYPE, FCB_PLEN = 0x00, 0x01, 0x09, 0x1A

Q_LINK, Q_FLAGS, Q_NAME, Q_MSGLEN, Q_NMSGS = 0x00, 0x04, 0x06, 0x0E, 0x10
Q_DQ, Q_NQ, Q_MSGCNT, Q_MSGOUT, Q_BUF = 0x12, 0x14, 0x16, 0x18, 0x1A
QF_MX, QF_RSP = 0x01, 0x08
QPB_QADDR, QPB_NMSGS, QPB_BUFFPTR, QPB_NAME = 0x02, 0x04, 0x06, 0x08

P_LINK, P_FLAG, P_DSK, P_USER, P_CNS, P_LST = 0x00, 0x06, 0x12, 0x13, 0x20, 0x24

PD = 0x5000
FREE_PD = 0x5100
MXQD = 0x9000
QD1 = 0x9100
CMDBUF = 0x9800
CMDCB_CMD = 1

e_q_full, e_no_queue, e_bad_open = 0x0F, 0x09, 30


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def put_name(mem, addr, name):
    padded = (name.upper() + ' ' * 8)[:8].encode('ascii')
    mem[addr:addr + 8] = padded


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in
               ('shifttail', 'fileopen', 'user_retf', 'clicmd_entry')}
    chunk = code[entries['user_retf']:entries['user_retf'] + 40]
    int_offset = chunk.index(b'\xcd')
    entries['user_retf_int'] = entries['user_retf'] + int_offset
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *COMMAND_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, PD)
    mem_w16(mem, UDA_WRKSEG, 0)
    # MXCli semaphore queue: one token available, so the unconditional
    # mxread (readq_entry) in clicmd_entry's own prologue, and the
    # unconditional mxwrite (writeq_entry) in clicmd_exit, each succeed
    # on their first attempt without retrying (see this file's own
    # header comment, and que1.c's own header comment on why that
    # matters: both calls would otherwise retry forever under the
    # boundary-stubbed mpmif(f_sleep) convention).
    mem_w16(mem, SD_QLR, MXQD)
    mem_w16(mem, MXQD + Q_LINK, 0)
    mem_w16(mem, MXQD + Q_FLAGS, QF_MX)
    mem_w16(mem, MXQD + Q_MSGLEN, 0)
    mem_w16(mem, MXQD + Q_NMSGS, 1)
    mem_w16(mem, MXQD + Q_MSGCNT, 1)
    mem_w16(mem, MXCLIQPB + QPB_QADDR, MXQD)
    # cli_pcb's own pcb_flnmptr/pcb_fcbptr are never written anywhere
    # in the real COMMAND.SUP source (command.c's own header comment)
    # -- the real system's own boot-time init sets them once, and
    # since cli_copy always lands the raw command text at the same
    # fixed CLI_CMDTAIL address, they're fixed boot-time constants
    # here too (cli_pfn, command.c's own stand-in for that same
    # boot-time init, sets them again on every call -- redundant with
    # this, but harmless).
    mem_w16(mem, CLI_PCB + PCB_FLNMPTR, CLI_CMDTAIL)
    mem_w16(mem, CLI_PCB + PCB_FCBPTR, CLI_FCB)
    return mem


def put_command(mem, text):
    """Write `text` as the command name into CMDBUF's own CLICB.DEF-
    shaped buffer (clicb_net byte + clicb_cmd text), the same shape
    clicmd_entry's own `dx` argument (in the caller's own, here
    wrkseg=0, flat-addressed) points at."""
    mem[CMDBUF] = 0  # clicb_net
    body = text.encode('ascii')
    mem[CMDBUF + CMDCB_CMD:CMDBUF + CMDCB_CMD + len(body)] = body


def run_reference(code, entry, mem, regs, stop_addrs=(), call_stub_addrs=()):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs=set(call_stub_addrs))
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class FileopenResult(ctypes.Structure):
            _fields_ = [('bl', ctypes.c_uint8), ('bh', ctypes.c_uint8)]

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.shifttail.argtypes = [c_u8p, ctypes.c_uint16]
        lib.fileopen.argtypes = [c_u8p]
        lib.fileopen.restype = FileopenResult
        lib.user_retf.argtypes = [c_u8p]
        lib.clicmd_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
        lib.clicmd_entry.restype = BxCx
        # sysdat/user_retf_offset/cs only matter on cli_gopen's own real
        # f_load path (load.c's load), never reached by either case
        # below (RSP dispatch succeeds before cli_fload; the no-RSP-match
        # case fails at fileopen before reaching cli_gopen) -- 0 is inert.

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

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

        # -- fileopen: sets the read-only bit on cli_fcb's own name[5].
        # fileopen is a real call into file4.c's func15 now (see
        # command.c's own header comment) -- the wrapper's own
        # reference run still uses f_fopen's old fixed-stub result
        # (BL=0xFF/BH=0), since bundling the real BDOS.A86 module here
        # would just hit the same, already-documented FILE2.BDO
        # cseg/dseg collision test_file4.py's own func15 ports around
        # (see file2.c's own header comment) -- but with no matching
        # directory entry anywhere in this empty fixture, func15's own
        # real logic genuinely, deterministically fails the same way
        # (confirmed directly: bl=0xff/bh=0), so the two sides still
        # agree, just for a real reason on the port side now instead
        # of a coincidence --
        mem = base_memory()
        mem[CLI_FCB + FCB_NAME + 5] = 0x41  # some pre-existing bits
        mem[CLI_FCB + FCB_NAME:CLI_FCB + FCB_NAME + 8] = b'FOOBAR  '
        ref_regs, ref_img = run_reference(code, entries['fileopen'], mem, {})
        buf = to_ctypes(mem)
        got = lib.fileopen(ctypes.cast(buf, c_u8p))
        check("fileopen: sets the read-only bit", ref_img[CLI_FCB + FCB_NAME + 5], bytes(buf)[CLI_FCB + FCB_NAME + 5])
        check("fileopen: real func15 call, no matching dir entry -> BL/BH", (0xFF, 0), (got.bl, got.bh))

        # -- user_retf: clears pf_keep/pf_tempkeep/pf_ctlc/pf_sys, leaves
        # every other bit (including pf_raw/pf_ctld here) untouched --
        PF_SYS, PF_KEEP, PF_RAW, PF_CTLC, PF_TEMPKEEP, PF_CTLD = 0x0001, 0x0002, 0x0040, 0x0080, 0x0200, 0x0400
        mem = base_memory()
        mem_w16(mem, PD + P_FLAG, PF_SYS | PF_KEEP | PF_RAW | PF_CTLC | PF_TEMPKEEP | PF_CTLD)
        ref_regs, ref_img = run_reference(code, entries['user_retf'], mem, {}, stop_addrs={entries['user_retf_int']})
        buf = to_ctypes(mem)
        lib.user_retf(ctypes.cast(buf, c_u8p))
        ref = mem_r16(ref_img, PD + P_FLAG)
        port = mem_r16(bytes(buf), PD + P_FLAG)
        check("user_retf: clears keep/tempkeep/ctlc/sys, not raw/ctld", ref, port)

        # -- clicmd_entry: a typed command matching a registered RSP
        # queue dispatches to it (cli_checkque's own success path) --
        mem = base_memory()
        mem_w16(mem, MXQD + Q_LINK, QD1)
        mem_w16(mem, QD1 + Q_LINK, 0)
        mem_w16(mem, QD1 + Q_FLAGS, QF_RSP)
        put_name(mem, QD1 + Q_NAME, 'FOOBAR')
        mem_w16(mem, QD1 + Q_MSGLEN, 0)
        mem_w16(mem, QD1 + Q_NMSGS, 1)
        mem_w16(mem, QD1 + Q_MSGCNT, 0)
        put_command(mem, 'FOOBAR')
        ref_regs, ref_img = run_reference(code, entries['clicmd_entry'], mem, {'dx': CMDBUF})
        buf = to_ctypes(mem)
        got = lib.clicmd_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(CMDBUF),
                                ctypes.c_uint16(0), ctypes.c_uint16(0), ctypes.c_uint16(0))
        port_img = bytes(buf)
        check("clicmd_entry: RSP dispatch -> bx/cx",
              (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))
        check("clicmd_entry: RSP dispatch -> cli_err",
              mem_r16(ref_img, SD_CLI_ERR), mem_r16(port_img, SD_CLI_ERR))
        check("clicmd_entry: RSP dispatch -> parsed fcb name",
              bytes(ref_img[CLI_FCB + FCB_NAME:CLI_FCB + FCB_NAME + 8]),
              bytes(port_img[CLI_FCB + FCB_NAME:CLI_FCB + FCB_NAME + 8]))
        check("clicmd_entry: RSP dispatch -> queue got the message",
              mem_r16(ref_img, QD1 + Q_MSGCNT), mem_r16(port_img, QD1 + Q_MSGCNT))

        # -- clicmd_entry: a typed command matching no RSP queue falls
        # through to cli_fload, whose own real fileopen call fails for
        # real (no matching directory entry in this empty fixture,
        # same as the dedicated fileopen case above) -- e_bad_open --
        mem = base_memory()
        put_command(mem, 'NOPE')
        mem_w16(mem, SD_PUL, FREE_PD)
        mem_w16(mem, FREE_PD + P_LINK, 0)
        ref_regs, ref_img = run_reference(code, entries['clicmd_entry'], mem, {'dx': CMDBUF})
        buf = to_ctypes(mem)
        got = lib.clicmd_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(CMDBUF),
                                ctypes.c_uint16(0), ctypes.c_uint16(0), ctypes.c_uint16(0))
        port_img = bytes(buf)
        check("clicmd_entry: no RSP match -> bx/cx",
              (ref_regs['bx'], ref_regs['cx']), (got.bx, got.cx))
        check("clicmd_entry: no RSP match -> cli_err == e_bad_open",
              mem_r16(ref_img, SD_CLI_ERR), mem_r16(port_img, SD_CLI_ERR))
        check("clicmd_entry: no RSP match -> PD released back onto pul",
              mem_r16(ref_img, SD_PUL), mem_r16(port_img, SD_PUL))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
