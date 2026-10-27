"""Shared harness for the src-c/rsp/ program tests (test_tmp.py,
test_echo.py, test_clock.py, test_submit.py, test_random.py): each runs a
whole program twice -- the real src/rsp/*.A86 in tools/i86sim.py, and its
C version (src-c/rsp/*.c) on the host -- against the same scripted
operating system, and compares what they did.

Both see the same memory layout: the program's code at CS, its data
segment (the assembled DSEG, which tests/src-c/test_rsp_images.py checks
the C image against) at DS, its PD at SYSDAT:PD with its UDA at UDA_SEG.
Each `int 224` -- the asm's, through i86sim's int_hooks, and the C's,
through fixtures/rt_host.c's rt_int224 -- goes to one `Mpm` object, a
small scripted MP/M: console output is collected, console input and
queue messages come from the script, there's a little CP/M file store
for the file functions, the CLI call records the command it's given,
and f_parsefilename is the real one (src-c/kernel/parse.c, on whichever
side's memory). A run ends when the program terminates or asks for input
the script doesn't have.

`compare` checks the two runs made the same system calls (function, DX,
DS, in order), printed the same text, and left the same bytes in memory,
except the asm's own stack (C has its own).
"""
import ctypes
import os
import struct
import subprocess
import sys
import tempfile

from memref import REPO_ROOT, KERNEL_C, phys, w16, r16  # noqa: F401
from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

SRC_C = os.path.join(REPO_ROOT, 'src-c')
RSP_A86 = os.path.join(REPO_ROOT, 'src', 'rsp')
FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')

MEM_SIZE = 0x10000  # i86sim's memory is 64K
CS = 0x0100        # the program's code
DS = 0x0200        # its data segment (a transient's base page is DS:0)
UDA_SEG = 0x0400   # its UDA
SYSDAT = 0x0500
VSEG = 0x0700      # where s_mpmseg:s_version points (TMP's version message)
LOADER_SS = 0x0F00  # a transient's starting stack (LOAD's), 0F00:0100 down
PD = 0x0200        # the running process's PD (a SYSDAT offset)
PARENT = 0x0240    # its parent's PD
SENDER = 0x0280    # a PD that sends ECHO a message

SYSDAT_RLR, S_MPMSEG, S_SYSDISK, S_VERSION, TOD = 0x68, 0x40, 0x4B, 0x78, 0x7E
P_PRIOR, P_FLAG, P_NAME, P_UDA, P_PARENT, P_CNS = 0x05, 0x06, 0x08, 0x10, 0x1E, 0x20
U_WRKSEG, U_INITCS = 0x2E, 0x50
VERSION_TEXT = b'MP/M-86 V2.0$'

# functions whose argument is DL, and ones with none (the MP/M-86
# Programmer's Guide): the log keeps what each function reads
BYTE_ARG = {2, 5, 14, 32, 44, 45, 132, 133, 143, 145, 148, 160}
NO_ARG = {0, 1, 11, 12, 25, 146, 147, 153, 156, 164}

c_u8p = ctypes.POINTER(ctypes.c_uint8)
HANDLER = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint16,
                           ctypes.POINTER(ctypes.c_uint16))
MAIN = ctypes.CFUNCTYPE(None, ctypes.c_uint16)


class ParseBxCx(ctypes.Structure):
    _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]


def compile_lib(sources, includes):
    lib = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unknown-warning-option',
                    '-Wno-unterminated-string-initialization', '-shared', '-fPIC',
                    *[f'-I{d}' for d in includes], *sources, '-o', lib], check=True)
    return lib


_parse_lib = None


def parse_lib():
    global _parse_lib
    if _parse_lib is None:
        path = compile_lib([os.path.join(KERNEL_C, 'parse.c')], [KERNEL_C])
        _parse_lib = ctypes.CDLL(path)
        os.remove(path)
        _parse_lib.parse_entry.restype = ParseBxCx
        _parse_lib.parse_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
    return _parse_lib


def pa(seg, off):
    return seg * 16 + off


class CpmFile:
    def __init__(self, data=b''):
        data = data + b'\x1a' * ((-len(data)) % 128)
        self.recs = {i: bytearray(data[i * 128:(i + 1) * 128]) for i in range(len(data) // 128)}


class Mpm:
    """A scripted MP/M for one run.
      lines -- console lines conread returns, in order
      chars -- characters conin returns
      queue -- messages qread delivers (bytes, written at the QPB's buffer)
      files -- {b'NAME    EXT': bytes}
      cli   -- CLI results (bx, cx), in order; then (0, 0)
      flagwaits -- how many flag waits return before the run stops
    The run stops at terminate (0, 143) or when input runs out."""

    def __init__(self, lines=(), chars=b'', queue=(), files=None, cli=(), flagwaits=0, disk=0, user=0,
                 lst=0, console=0):  # noqa: E129
        self.lines = list(lines)
        self.chars = list(chars)
        self.queue = list(queue)
        self.files = {k: CpmFile(v) for k, v in (files or {}).items()}
        self.cli = list(cli)
        self.flagwaits = flagwaits
        self.disk, self.user, self.lst, self.console = disk, user, lst, console
        self.dma_off = 0x80
        self.dma_seg = None
        self.log = []
        self.out = bytearray()
        self.commands = []

    def _string(self, mem, seg, off):
        s = bytearray()
        while mem[pa(seg, off)] != ord('$') and len(s) < 512:
            s.append(mem[pa(seg, off)])
            off = (off + 1) & 0xFFFF
        return bytes(s)

    @staticmethod
    def _fname(mem, ds, fcb):
        """name and type, without the attribute bits (as the BDOS matches)"""
        return bytes(b & 0x7F for b in mem[pa(ds, fcb + 1):pa(ds, fcb + 12)])

    def _dma(self, ds):
        return pa(self.dma_seg if self.dma_seg is not None else ds, self.dma_off)

    def call(self, mem, ds, es, cl, dx):
        """returns None to stop, else (bx, cx, es)"""
        # what the function reads: nothing, DL alone (the asm often leaves
        # something in DX/DH), or DX with DS
        self.log.append((cl,) if cl in NO_ARG else (cl, dx & 0xFF) if cl in BYTE_ARG else (cl, dx, ds))
        dl = dx & 0xFF
        bx, cx = 0, 0
        if cl in (0, 143):            # system reset, terminate
            return None
        if cl == 1:                   # conin
            if not self.chars:
                return None
            bx = self.chars.pop(0)
        elif cl == 2:                 # conout
            self.out.append(dl)
        elif cl == 9:                 # conwrite
            self.out += self._string(mem, ds, dx)
        elif cl == 10:                # conread
            if not self.lines:
                return None
            line = self.lines.pop(0)[:mem[pa(ds, dx)]]
            mem[pa(ds, dx + 1)] = len(line)
            mem[pa(ds, dx + 2):pa(ds, dx + 2) + len(line)] = line
            self.out += line + b'\r'
        elif cl == 12:                # version
            bx = 0x1130
        elif cl == 14:                # select disk
            self.disk = dl
        elif cl == 25:                # get default disk
            bx = self.disk
        elif cl == 32:                # get/set user code
            if dl == 0xFF:
                bx = self.user
            else:
                self.user = dl & 0x0F
        elif cl == 26:                # set dma
            self.dma_off = dx
        elif cl == 51:                # set dma base
            self.dma_seg = dx
        elif cl == 153:               # get default console
            bx = self.console
        elif cl == 148:               # set default console
            self.console = dl
        elif cl == 156:               # get PD address
            bx, es = PD, SYSDAT
        elif cl == 160:               # set default list device
            if dl > 15:
                bx, cx = 0xFFFF, 0x0C
            else:
                self.lst = dl
        elif cl == 164:               # get default list device
            bx = self.lst
        elif cl == 132:               # flag wait
            if self.flagwaits == 0:
                return None
            self.flagwaits -= 1
        elif cl == 137:               # queue read
            if not self.queue:
                return None
            msg = self.queue.pop(0)
            buf = r16(mem, pa(ds, dx + 6))
            mem[pa(ds, buf):pa(ds, buf) + len(msg)] = msg
        elif cl == 150:               # CLI
            net = pa(ds, dx)
            cmd = bytes(mem[net + 1:net + 130]).split(b'\0')[0]
            self.commands.append(cmd)
            bx, cx = self.cli.pop(0) if self.cli else (0, 0)
        elif cl == 152:               # parse filename: the real one
            w16(mem, pa(UDA_SEG, U_WRKSEG), ds)
            buf = (ctypes.c_uint8 * MEM_SIZE).from_buffer(mem)
            r = parse_lib().parse_entry(buf, SYSDAT, dx)
            del buf
            bx, cx = r.bx, r.cx
        elif cl == 15:                # open
            name = self._fname(mem, ds, dx)
            bx = 0 if name in self.files else 0xFF
        elif cl == 16:                # close
            bx = 0 if self._fname(mem, ds, dx) in self.files else 0xFF
        elif cl == 22:                # make
            self.files.setdefault(self._fname(mem, ds, dx), CpmFile())
        elif cl == 20:                # read sequential: record = fcb(cr)
            f = self.files.get(self._fname(mem, ds, dx))
            rec = mem[pa(ds, dx + 32)]
            if f is None or rec not in f.recs:
                bx = 1
            else:
                d = self._dma(ds)
                mem[d:d + 128] = f.recs[rec]
                mem[pa(ds, dx + 32)] = rec + 1
        elif cl in (33, 34, 40):      # read / write / write zero-fill random
            f = self.files.get(self._fname(mem, ds, dx))
            rec = r16(mem, pa(ds, dx + 33)) | mem[pa(ds, dx + 35)] << 16
            d = self._dma(ds)
            if f is None:
                bx = 9
            elif cl == 33:
                if rec in f.recs:
                    mem[d:d + 128] = f.recs[rec]
                else:
                    bx = 1
            else:
                f.recs[rec] = bytearray(mem[d:d + 128])
        # everything else (attach, detach, free drive, set priority, queue
        # make/open, flag set, ...) just returns 0
        return bx & 0xFFFF, cx & 0xFFFF, es


class Program:
    """One src/rsp program: its assembled code and data, and its C build."""

    def __init__(self, name, c_main, rsp, stack):
        self.name = name
        asm = Assembler()
        asm.assemble(os.path.join(RSP_A86, name + '.A86'), [])
        self.code, self.data = asm.segment_bytes('cseg'), asm.segment_bytes('dseg')
        self.sym = {k.lower(): v.value for k, v in asm.symtab.items()}
        self.rsp = rsp
        self.stack = stack  # (first, last) DS offsets of the asm's stack
        subprocess.run(['make', 'obj/rsp/rsp_syms.h'], cwd=SRC_C, check=True, stdout=subprocess.DEVNULL)
        path = compile_lib([os.path.join(SRC_C, 'rsp', name.lower() + '.c'), os.path.join(FIXTURES, 'rt_host.c')],
                           [KERNEL_C, os.path.join(SRC_C, 'rt'), os.path.join(SRC_C, 'rsp'),
                            os.path.join(SRC_C, 'obj', 'rsp')])
        self.lib = ctypes.CDLL(path)
        os.remove(path)
        self.lib.rt_host_run.argtypes = [c_u8p, ctypes.c_uint16, HANDLER, ctypes.c_void_p, ctypes.c_uint16]
        self.lib.rt_host_log.restype = ctypes.c_char_p
        self.c_main = ctypes.cast(getattr(self.lib, c_main), ctypes.c_void_p)

    def v(self, label):
        return self.sym[label.lower()]

    def memory(self, setup=None):
        """the machine both runs start from"""
        mem = bytearray(MEM_SIZE)
        mem[pa(CS, 0):pa(CS, len(self.code))] = self.code
        mem[pa(DS, 0):pa(DS, len(self.data))] = self.data
        if self.rsp:
            w16(mem, pa(DS, 0), SYSDAT)  # rsp_link: INIT gives the RSP SYSDAT
        w16(mem, pa(SYSDAT, SYSDAT_RLR), PD)
        w16(mem, pa(SYSDAT, PD + P_UDA), UDA_SEG)
        w16(mem, pa(UDA_SEG, U_INITCS), CS)
        w16(mem, pa(SYSDAT, S_MPMSEG), VSEG)
        w16(mem, pa(SYSDAT, S_VERSION), 0x10)
        mem[pa(VSEG, 0x10):pa(VSEG, 0x10) + len(VERSION_TEXT)] = VERSION_TEXT
        name = self.name.encode() if not self.rsp else bytes(self.data[0x18:0x20])
        mem[pa(SYSDAT, PD + P_NAME):pa(SYSDAT, PD + P_NAME + 8)] = name.ljust(8)[:8]
        if setup:
            setup(mem)
        return mem

    def run_asm(self, mem, mpm):
        mem = bytearray(mem)

        def hook(sim):
            r = sim.regs
            res = mpm.call(mem, r['ds'], r['es'], r['cx'] & 0xFF, r['dx'])
            if res is None:
                sim.halted = True
                return
            r['ax'] = r['bx'] = res[0]
            r['cx'] = res[1]
            r['es'] = res[2]

        if self.rsp:  # start at the initial frame (the UDA's u_stack_sp): IP, CS, flags
            frame = r16(mem, pa(DS, 0x40 + 0x34))
            ip, sp = r16(mem, pa(DS, frame)), frame + 6
            regs = {'cs': CS, 'ds': DS, 'es': DS, 'ss': DS, 'sp': sp}
        else:         # a transient: CS:0, the base page in DS (and ES, SS)
            ip = 0
            regs = {'cs': CS, 'ds': DS, 'es': DS, 'ss': LOADER_SS, 'sp': 0x100}
        sim = I86Sim(mem, regs, pa(CS, ip), segmented=True, int_hooks={224: hook})
        sim.run(max_steps=3000000)
        assert sim.halted, f"{self.name}: the asm run didn't finish"
        return mem

    def run_c(self, mem, mpm):
        buf = (ctypes.c_uint8 * MEM_SIZE).from_buffer_copy(mem)
        view = memoryview(buf).cast('B')

        @HANDLER
        def handler(ds, es, cl, dx, out):
            res = mpm.call(view, ds, es, cl, dx)
            if res is None:
                return 1
            out[0], out[1], out[2] = res
            return 0

        stopped = self.lib.rt_host_run(buf, SYSDAT, handler, self.c_main, DS)
        del view
        return bytearray(buf), stopped, self.lib.rt_host_log().decode()


class Checker:
    def __init__(self):
        self.failures = []

    def check(self, label, ref, port):
        ok = ref == port
        print(f"[{'ok' if ok else 'FAIL'}] {label}: reference {ref!r} | port {port!r}")
        if not ok:
            self.failures.append(label)

    def finish(self):
        if self.failures:
            print(f"\n{len(self.failures)} case(s) failed: {', '.join(self.failures)}")
            return 1
        print("\nall cases match.")
        return 0


def compare(ck, prog, label, mpm_args, setup=None, snap=None):
    """run both sides from the same machine and script; compare"""
    mem = prog.memory(setup)
    ref_mpm, port_mpm = Mpm(**mpm_args), Mpm(**mpm_args)
    ref = prog.run_asm(mem, ref_mpm)
    port, _, _ = prog.run_c(mem, port_mpm)
    ck.check(f"{label}: same calls", ref_mpm.log, port_mpm.log)
    ck.check(f"{label}: same output", bytes(ref_mpm.out), bytes(port_mpm.out))
    ck.check(f"{label}: same CLI commands", ref_mpm.commands, port_mpm.commands)
    if snap:
        ck.check(f"{label}: result", snap(ref, ref_mpm), snap(port, port_mpm))
    skip = set(range(pa(DS, prog.stack[0]), pa(DS, prog.stack[1]))) | set(range(pa(LOADER_SS, 0), pa(LOADER_SS, 0x100)))
    diffs = [hex(a) for a in range(MEM_SIZE) if a not in skip and ref[a] != port[a]]
    ck.check(f"{label}: no other bytes differ", [], diffs[:8])
    return ref_mpm, port_mpm
