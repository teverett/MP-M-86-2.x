#!/usr/bin/env python3
"""Differential test for src-c/kernel/submit.c against a self-contained
subset of src/rsp/SUBMIT.A86: deblank+fill_argtable (unlabeled code
right after `submit:`'s own `call openfile`), expand_command
(`nextchar`/.../`echocommand`), readbuffer, readchar, and (now)
topofcmdloop_once's own real `goodparse`/`try_builtin`/`notbuiltin`
dispatch. See submit.c's own header comment for why `$INCLUDE`'s own
real body and `submit:`'s own one-time setup are still deferred.

SUBMIT.A86 assembles standalone (no wrapper fixture needed -- it has
no INCLUDEs at all), after fixing all 3 of docs/bugs.md #3's bugs
directly in src/rsp/SUBMIT.A86 (same RSP-unpatched-policy question
already asked and answered for CLOCK.A86/TMP.A86).

SUBMIT.A86's own real DSEG offsets (e.g. `cmdtail` at 0x80) land
inside its own 0x665-byte cseg once collapsed into the flat memory
model, same as TMP.A86 -- the *reference* run loads cseg at a shifted
base (CSEG_BASE) for the same reason test_tmp.py's own run_reference
does (every control-flow instruction this file's own reachable path
uses is relative, so shifting the whole block by a constant preserves
every internal branch; `deblank`/`fill_argtable`/`expand_command`
never reference an absolute same-cseg address for anything other than
boundary-stub print calls).

`topofcmdloop_once`'s own `goodparse`/`try_builtin`/`notbuiltin` were
previously believed provably unreachable (same claim `tmp.c`'s own
`goodparse` made, and just as wrong -- see submit.c's own header
comment for the correction: `parsefilename`/`mpm_clicmd` are real
calls into `parse.c`'s `parse_entry`/`command.c`'s `clicmd_entry`).
Exercising them for real needs the exact same two fixes
test_tmp.py's own run_reference needed: `call_stub_addrs` for every
print-adjacent routine (`getuser`/`setuser`/`getlist`/`setlist`/
`prnum`, alongside the already-stubbed `con_write`/`crlf`) to avoid
`print_ds_string`-class `cs`-relative addressing breaking under the
`CSEG_BASE` shift, and an unshifted-real-address override for
`usercmd`/`printercmd`/`includecmd` (cseg-resident DATA `try_builtin`
compares against by absolute address, same class of problem as
`usercmd`/`printercmd` in `tmp.c`'s own test). `notbuiltin`'s own
real `clicmd_entry` call needs `command.c`'s usual supporting fixture
too (`SD_RLR`/`UDA_WRKSEG`/an MXCli queue with one token) -- see
test_command.py's own base_memory(), reused here via the same
`int_hooks`-driven real `ctypes` call test_tmp.py's own
`make_int_hook` already established (not reproduced in Python).
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

SUBMIT_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'SUBMIT.A86')
SUBMIT_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in
            ('submit.c', 'command.c', 'que1.c', 'que2.c', 'console.c', 'chario.c', 'parse.c',
             'file1.c', 'file2.c', 'file3.c', 'file4.c',
             'load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
CSEG_BASE = 0x1000  # clear of every real DSEG address this routine touches

SD_SUBMIT_CMDTAIL = 0xA300
SD_SUBMIT_DEBLANKBUF = 0xA400
SD_SUBMIT_ARGTABLE = 0xA500
SD_SUBMIT_NUMARGS = 0xA540
SD_SUBMIT_CMDBUF = 0xA5F0
SD_SUBMIT_BLEN = 0xA5F1
SD_SUBMIT_CMD = 0xA5F2
SD_SUBMIT_CLICMD = 0xA700
SD_SUBMIT_DMA = 0xA800
SD_SUBMIT_BUF_PTR = 0xA880
SD_SUBMIT_PDADR = 0xA890
SD_SUBMIT_SUBFILENAME = 0xA8A0
SD_SUBMIT_FCB = 0xA8B0
SD_SUBMIT_CMDSENT = 0xA8E0
SD_SUBMIT_DEFDISK = 0xA8E1
SD_SUBMIT_DEFUSER = 0xA8E2
SD_SUBMIT_PROMPTUTENS = 0xA8E3
SD_SUBMIT_PROMPTUONES = 0xA8E4
SD_SUBMIT_PROMPTDISK = 0xA8E5

P_FLAG = 0x06
PF_CHILDABORT = 0x0800
PF_CTLC = 0x0080
PD_ADDR = 0x9000

MPMINT = 224
MPM_PARSE = 152
MPM_CLICMD = 150

# command.c's own scratch fixture (tests/src-c/test_command.py's own
# base_memory(), reused verbatim -- see this file's own header
# comment)
SD_RLR = 0xA000
UDA_WRKSEG = 0xA012
SD_QLR = 0xA020
MXCLIQPB = 0xB400
Q_LINK, Q_FLAGS, Q_MSGLEN, Q_NMSGS, Q_MSGCNT = 0x00, 0x04, 0x0E, 0x10, 0x16
QF_MX = 0x01
QPB_QADDR = 0x02
MXQD = 0x9400  # clear of PD_ADDR (0x9000) above

# Print-adjacent routines genuinely reachable now that goodparse/
# try_builtin run for real -- stubbed outright, same reasoning as
# test_tmp.py's own PRINT_STUBS.
PRINT_STUBS = ('con_write', 'crlf', 'getuser', 'setuser', 'getlist', 'setlist', 'prnum')


def add_command_fixture(mem):
    mem_w16(mem, SD_RLR, PD_ADDR)
    mem_w16(mem, UDA_WRKSEG, 0)
    mem_w16(mem, SD_QLR, MXQD)
    mem_w16(mem, MXQD + Q_LINK, 0)
    mem_w16(mem, MXQD + Q_FLAGS, QF_MX)
    mem_w16(mem, MXQD + Q_MSGLEN, 0)
    mem_w16(mem, MXQD + Q_NMSGS, 1)
    mem_w16(mem, MXQD + Q_MSGCNT, 1)
    mem_w16(mem, MXCLIQPB + QPB_QADDR, MXQD)


def make_int_hook(lib, c_u8p):
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
        # else: every other real `mpm` call stays a pure no-op, same
        # as the old blanket int_stub_nums={224} this replaces.
    return hook


def assemble_reference():
    asm = Assembler()
    asm.assemble(SUBMIT_A86, [])
    code = asm.segment_bytes('cseg')
    names = ('submit', 'mpm', 'openfile', 'readfile', 'filldone', 'crlf', 'nextchar', 'echocommand',
             'readbuffer', 'readchar', 'cmdtail', 'deblankbuf', 'argtable', 'numargs',
             'cmdbuf', 'blen', 'cmd', 'clicmd', 'dma', 'buf_ptr',
             'command_user', 'command_disk', 'defuser',
             'topofcmdloop', 'submitexit', 'getnxtcmd', 'charin', 'con_write',
             'pdadr', 'fcb', 'subfilename', 'cmdsent', 'defdisk',
             'promptutens', 'promptuones', 'promptdisk')
    entries = {name: asm.symtab[name].value for name in names}
    print_stubs = {CSEG_BASE + asm.symtab[name].value for name in PRINT_STUBS}
    # usercmd/printercmd/includecmd are DSEG-resident DATA (SUBMIT.A86
    # switches to `dseg` partway through, unlike TMP.A86 -- confirmed
    # by grep for `^\s*dseg` -- so these three, like every other real
    # DSEG field this file has, are NOT part of the shifted `cseg`
    # blob at all) that `try_builtin` compares against by absolute
    # address -- same "needs its own real content restored post-
    # shift" treatment as test_tmp.py's own usercmd/printercmd,
    # just sourced from `dseg` instead of `code` (an earlier version
    # of this fix read them from `code`/cseg by analogy with
    # test_tmp.py, which doesn't apply here -- caught because the
    # bytes that came back weren't the expected text at all).
    dseg = asm.segment_bytes('dseg')
    real_names = {}
    for name in ('usercmd', 'printercmd', 'includecmd'):
        addr = asm.symtab[name].value
        real_names[addr] = dseg[addr:addr + 8]
    return code, entries, print_stubs, real_names


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *SUBMIT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def run_reference(code, entry, mem, regs=None, stop_addrs=(), call_stub_addrs=()):
    image = bytearray(0x10000)
    image[CSEG_BASE:CSEG_BASE + len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {**(regs or {}), 'sp': STACK_TOP},
                 CSEG_BASE + entry,
                 stop_addrs={(CSEG_BASE + a) for a in stop_addrs},
                 call_stub_addrs={(CSEG_BASE + a) for a in call_stub_addrs})
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, e, print_stubs, real_names = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        lib.deblank.argtypes = [c_u8p]
        lib.fill_argtable.argtypes = [c_u8p]
        lib.expand_command.argtypes = [c_u8p]
        lib.readbuffer.restype = ctypes.c_uint16
        lib.readbuffer.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
        lib.readchar.restype = ctypes.c_uint16
        lib.readchar.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
        lib.topofcmdloop_once.restype = ctypes.c_int
        lib.topofcmdloop_once.argtypes = [c_u8p, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]

        class ParseBxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        class CmdBxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.parse_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.parse_entry.restype = ParseBxCx
        lib.clicmd_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
        lib.clicmd_entry.restype = CmdBxCx
        int_hook = make_int_hook(lib, c_u8p)

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref!r} | port {port!r}")
            if status == "FAIL":
                failures.append(label)

        # -- deblank + fill_argtable: run the real submit: preamble through filldone --
        def run_deblank_case(label, tail_text):
            mem = bytearray(0x10000)
            tail = tail_text.encode()
            mem[e['cmdtail']] = len(tail)
            mem[e['cmdtail'] + 1:e['cmdtail'] + 1 + len(tail)] = tail
            mem[e['cmdtail'] + 1 + len(tail)] = 0  # NUL terminator deblank scans for
            ref_regs, ref_img = run_reference(
                code, e['submit'], mem, {'ax': 0},
                stop_addrs={e['crlf']}, call_stub_addrs={e['mpm'], e['openfile']})
            ref = (bytes(ref_img[e['deblankbuf']:e['deblankbuf'] + 20]),
                   bytes(ref_img[e['argtable']:e['argtable'] + 8]),
                   mem_r16(ref_img, e['numargs']))

            port_mem = bytearray(0x10000)
            port_mem[SD_SUBMIT_CMDTAIL] = len(tail)
            port_mem[SD_SUBMIT_CMDTAIL + 1:SD_SUBMIT_CMDTAIL + 1 + len(tail)] = tail
            port_mem[SD_SUBMIT_CMDTAIL + 1 + len(tail)] = 0
            buf = to_ctypes(port_mem)
            lib.deblank(ctypes.cast(buf, c_u8p))
            lib.fill_argtable(ctypes.cast(buf, c_u8p))
            port_img = bytes(buf)
            port = (bytes(port_img[SD_SUBMIT_DEBLANKBUF:SD_SUBMIT_DEBLANKBUF + 20]),
                    bytes(port_img[SD_SUBMIT_ARGTABLE:SD_SUBMIT_ARGTABLE + 8]),
                    mem_r16(port_img, SD_SUBMIT_NUMARGS))
            check(f"deblank+fill_argtable: {label}", ref, port)

        run_deblank_case("simple args", " foo  bar baz")
        run_deblank_case("leading tabs/spaces, embedded tab", "\t  one\ttwo")
        # fill_argtable's own `skiparg` scans forward for a space that
        # terminates the *current* argument -- guaranteed to exist by
        # deblank's own "always end with ' \0'" guarantee, but only
        # when there's at least one real (non-whitespace) character.
        # A genuinely empty/all-whitespace cmdtail (deblankbuf = " \0")
        # has no such space to find and the real asm scans off the end
        # of the buffer -- not tested, since this never happens for a
        # real invocation (argument 0 is always the .SUB filename
        # itself; "single argument" below is the real minimum).
        run_deblank_case("single argument", " onlyone")

        # -- expand_command --
        def run_expand_case(label, cmd_text, args):
            mem = bytearray(0x10000)
            cmdb = cmd_text.encode() + b'\x00'
            mem[e['cmd']:e['cmd'] + len(cmdb)] = cmdb
            deblank_text = (' ' + ' '.join(args) + ' ').encode() if args else b' '
            mem[e['deblankbuf']:e['deblankbuf'] + len(deblank_text)] = deblank_text
            mem_w16(mem, e['numargs'], len(args))
            pos = 0
            for i, a in enumerate(args):
                pos += 1  # the leading space
                mem[e['argtable'] + i] = pos
                pos += len(a)
            ref_regs, ref_img = run_reference(
                code, e['nextchar'], mem, {'si': e['cmd'], 'bp': e['clicmd'], 'di': 0},
                stop_addrs={e['echocommand']})
            ref = bytes(ref_img[e['clicmd']:e['clicmd'] + 24])

            port_mem = bytearray(0x10000)
            port_mem[SD_SUBMIT_CMD:SD_SUBMIT_CMD + len(cmdb)] = cmdb
            port_mem[SD_SUBMIT_DEBLANKBUF:SD_SUBMIT_DEBLANKBUF + len(deblank_text)] = deblank_text
            mem_w16(port_mem, SD_SUBMIT_NUMARGS, len(args))
            pos = 0
            for i, a in enumerate(args):
                pos += 1
                port_mem[SD_SUBMIT_ARGTABLE + i] = pos
                pos += len(a)
            buf = to_ctypes(port_mem)
            lib.expand_command(ctypes.cast(buf, c_u8p))
            port = bytes(bytes(buf)[SD_SUBMIT_CLICMD:SD_SUBMIT_CLICMD + 24])
            check(f"expand_command: {label}", ref, port)

        run_expand_case("no substitution", "DIR A:", [])
        run_expand_case("single-digit arg", "TYPE $1.TXT", ["FOO"])
        run_expand_case("two-digit arg", "ECHO $10", [str(i) for i in range(11)])
        run_expand_case("arg out of range", "TYPE $5.TXT", ["FOO"])
        run_expand_case("literal $ not followed by digit", "PIP X=$Y", [])

        # -- readchar: no refill needed --
        mem = bytearray(0x10000)
        mem[e['buf_ptr']] = 5
        mem[e['dma'] + 5] = ord('Q')
        ref_regs, ref_img = run_reference(code, e['readchar'], mem, {'si': 0x4000})
        ref = (ref_regs['bx'], ref_img[0x4000], ref_img[e['buf_ptr']])
        port_mem = bytearray(0x10000)
        port_mem[SD_SUBMIT_BUF_PTR] = 5
        port_mem[SD_SUBMIT_DMA + 5] = ord('Q')
        buf = to_ctypes(port_mem)
        got = lib.readchar(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x4000),
                            ctypes.c_uint8(0), ctypes.c_uint8(0))
        port_img = bytes(buf)
        port = (got, port_img[0x4000], port_img[SD_SUBMIT_BUF_PTR])
        check("readchar: no refill, ordinary char", ref, port)

        # -- readchar: EOF marker --
        mem = bytearray(0x10000)
        mem[e['buf_ptr']] = 10
        mem[e['dma'] + 10] = 0x1A
        ref_regs, ref_img = run_reference(code, e['readchar'], mem, {'si': 0x4000})
        ref = ref_regs['bx']
        port_mem = bytearray(0x10000)
        port_mem[SD_SUBMIT_BUF_PTR] = 10
        port_mem[SD_SUBMIT_DMA + 10] = 0x1A
        buf = to_ctypes(port_mem)
        got = lib.readchar(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x4000),
                            ctypes.c_uint8(0), ctypes.c_uint8(0))
        check("readchar: EOF marker", ref, got)

        # -- readchar: refill needed, openfile fails --
        mem = bytearray(0x10000)
        mem[e['buf_ptr']] = 128
        mem[e['command_disk']] = 1
        ref_regs, ref_img = run_reference(code, e['readchar'], mem, {'si': 0x4000, 'ax': 0xFF00},
                                           call_stub_addrs={e['mpm'], e['openfile'], e['readfile']})
        ref = ref_regs['bx']
        port_mem = bytearray(0x10000)
        port_mem[SD_SUBMIT_BUF_PTR] = 128
        buf = to_ctypes(port_mem)
        got = lib.readchar(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x4000),
                            ctypes.c_uint8(0xFF), ctypes.c_uint8(0))
        check("readchar: refill, openfile fails", ref, got)

        # -- readchar: refill needed, succeeds --
        mem = bytearray(0x10000)
        mem[e['buf_ptr']] = 128
        mem[e['dma']] = ord('Z')
        ref_regs, ref_img = run_reference(code, e['readchar'], mem, {'si': 0x4000, 'ax': 0x0000},
                                           call_stub_addrs={e['mpm'], e['openfile'], e['readfile']})
        ref = (ref_regs['bx'], ref_img[0x4000], ref_img[e['buf_ptr']])
        port_mem = bytearray(0x10000)
        port_mem[SD_SUBMIT_BUF_PTR] = 128
        port_mem[SD_SUBMIT_DMA] = ord('Z')
        buf = to_ctypes(port_mem)
        got = lib.readchar(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0x4000),
                            ctypes.c_uint8(0), ctypes.c_uint8(0))
        port_img = bytes(buf)
        port = (got, port_img[0x4000], port_img[SD_SUBMIT_BUF_PTR])
        check("readchar: refill, succeeds", ref, port)

        # -- readbuffer: one short line --
        def run_readbuffer_case(label, dma_text, maxlen=32):
            mem = bytearray(0x10000)
            mem[e['cmdbuf']] = maxlen
            dma = dma_text.encode()
            mem[e['dma']:e['dma'] + len(dma)] = dma
            ref_regs, ref_img = run_reference(code, e['readbuffer'], mem, {'dx': e['cmdbuf']})
            ref = (ref_regs['bx'], ref_img[e['blen']], bytes(ref_img[e['cmd']:e['cmd'] + 16]))

            port_mem = bytearray(0x10000)
            port_mem[SD_SUBMIT_CMDBUF] = maxlen
            port_mem[SD_SUBMIT_DMA:SD_SUBMIT_DMA + len(dma)] = dma
            buf = to_ctypes(port_mem)
            got = lib.readbuffer(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SD_SUBMIT_CMDBUF),
                                  ctypes.c_uint8(0), ctypes.c_uint8(0))
            port_img = bytes(buf)
            port = (got, port_img[SD_SUBMIT_BLEN], bytes(port_img[SD_SUBMIT_CMD:SD_SUBMIT_CMD + 16]))
            check(f"readbuffer: {label}", ref, port)

        run_readbuffer_case("CRLF-terminated line", "DIR A:\r\n")
        run_readbuffer_case("LF-only line", "ECHO HI\n")
        run_readbuffer_case("skips a leading blank line", "\r\nDIR B:\r\n")
        run_readbuffer_case("buffer fills before any terminator", "X" * 40 + "\r\n", maxlen=10)

        # -- topofcmdloop_once --
        def run_topofcmdloop_case(label, childabort, al, cmd_text,
                                   cmdsent=False, fcb_name=b'FILENAME123'):
            """al is the single shared AL value that persists through
            whichever of charin/mpm_getdefdisk/mpm_usercode actually run:
            under the no-op boundary-stub convention a `call charin` (no
            sentinel pre-loaded) leaves AL exactly as it already was, so
            charin's own "result" can't be independently injected via
            call_stub_addrs -- it's mechanically forced to equal whatever
            AL already held. Same reasoning as defdisk_al/defuser_al
            needing to match each other (see submit.c's own header
            comment on topofcmdloop_once)."""
            mem = bytearray(0x10000)
            mem_w16(mem, e['pdadr'], PD_ADDR)
            flags = PF_CHILDABORT if childabort else 0
            mem_w16(mem, PD_ADDR + P_FLAG, flags)
            mem[e['fcb'] + 1:e['fcb'] + 1 + 11] = fcb_name[:11].ljust(11, b' ')
            mem[e['cmdsent']] = 1 if cmdsent else 0
            cmdb = cmd_text.encode() + b'\x00'
            mem[e['cmd']:e['cmd'] + len(cmdb)] = cmdb
            mem[e['blen']] = len(cmd_text)
            mem_w16(mem, e['numargs'], 0)
            add_command_fixture(mem)
            image = bytearray(0x10000)
            image[CSEG_BASE:CSEG_BASE + len(code)] = code
            for addr, val in enumerate(mem):
                if val != 0:
                    image[addr] = val
            for addr, data in real_names.items():
                image[addr:addr + len(data)] = data
            sim = I86Sim(image, {'ax': (al << 8) | al, 'sp': STACK_TOP},
                         CSEG_BASE + e['topofcmdloop'],
                         stop_addrs={CSEG_BASE + e['submitexit'], CSEG_BASE + e['getnxtcmd']},
                         call_stub_addrs={CSEG_BASE + e['mpm'], CSEG_BASE + e['charin']} | print_stubs,
                         int_hooks={MPMINT: int_hook})
            sim.run()
            ref_img = image
            ref_outcome = 1 if sim.ip - CSEG_BASE == e['submitexit'] else 0
            ref = (ref_outcome,
                   mem_r16(ref_img, PD_ADDR + P_FLAG),
                   bytes(ref_img[e['subfilename']:e['subfilename'] + 11]),
                   ref_img[e['cmdsent']],
                   ref_img[e['defdisk']], ref_img[e['defuser']],
                   ref_img[e['promptutens']], ref_img[e['promptuones']], ref_img[e['promptdisk']],
                   bytes(ref_img[e['clicmd']:e['clicmd'] + 16]))

            port_mem = bytearray(0x10000)
            mem_w16(port_mem, SD_SUBMIT_PDADR, PD_ADDR)
            mem_w16(port_mem, PD_ADDR + P_FLAG, flags)
            port_mem[SD_SUBMIT_FCB + 1:SD_SUBMIT_FCB + 1 + 11] = fcb_name[:11].ljust(11, b' ')
            port_mem[SD_SUBMIT_CMDSENT] = 1 if cmdsent else 0
            port_mem[SD_SUBMIT_CMD:SD_SUBMIT_CMD + len(cmdb)] = cmdb
            port_mem[SD_SUBMIT_BLEN] = len(cmd_text)
            mem_w16(port_mem, SD_SUBMIT_NUMARGS, 0)
            add_command_fixture(port_mem)
            buf = to_ctypes(port_mem)
            # if the control-C path runs, the real asm's own `and al,5fh`
            # (upper-casing charin's answer) is what actually persists
            # into defdisk/defuser afterward, not the raw al
            persisted_al = (al & 0x5F) if childabort else al
            got = lib.topofcmdloop_once(ctypes.cast(buf, c_u8p), ctypes.c_uint8(al),
                                         ctypes.c_uint8(persisted_al), ctypes.c_uint8(persisted_al))
            port_img = bytes(buf)
            port = (got,
                    mem_r16(port_img, PD_ADDR + P_FLAG),
                    bytes(port_img[SD_SUBMIT_SUBFILENAME:SD_SUBMIT_SUBFILENAME + 11]),
                    port_img[SD_SUBMIT_CMDSENT],
                    port_img[SD_SUBMIT_DEFDISK], port_img[SD_SUBMIT_DEFUSER],
                    port_img[SD_SUBMIT_PROMPTUTENS], port_img[SD_SUBMIT_PROMPTUONES],
                    port_img[SD_SUBMIT_PROMPTDISK],
                    bytes(port_img[SD_SUBMIT_CLICMD:SD_SUBMIT_CLICMD + 16]))
            check(f"topofcmdloop_once: {label}", ref, port)

        run_topofcmdloop_case("no control-C, ordinary command -> submitexit",
                               False, 7, "DIR A:")
        run_topofcmdloop_case("no control-C, empty command -> getnxtcmd",
                               False, 3, "")
        run_topofcmdloop_case("no control-C, ';'-prefixed -> getnxtcmd",
                               False, 12, ";ignored")
        run_topofcmdloop_case("no control-C, disk shortcut 'C:' -> getnxtcmd",
                               False, 0, "C:")
        run_topofcmdloop_case("no control-C, disk shortcut out of range -> submitexit",
                               False, 0, "~:")
        run_topofcmdloop_case("control-C pending, answer 'N' -> clears flag, continues",
                               True, ord('n'), "")
        run_topofcmdloop_case("control-C pending, answer 'Y' -> sets pf_ctlc, submitexit",
                               True, ord('Y'), "DIR A:")
        run_topofcmdloop_case("USER builtin, no argument -> getnxtcmd",
                               False, 4, "USER")
        run_topofcmdloop_case("USER builtin, with a 1-digit argument -> getnxtcmd",
                               False, 4, "USER 5")
        run_topofcmdloop_case("PRINTER builtin, with a 2-digit argument -> getnxtcmd",
                               False, 4, "PRINTER 12")

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
