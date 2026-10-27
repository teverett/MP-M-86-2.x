"""Shared harness for src-c/toolchain/'s module tests: one of DDT86's
PL/M overlay entries -- disem (src-assem/toolchain/DIS86.PLM, with
INS86) or asm (ASS86.PLM, with ASMTAB) -- run twice from the same
memory: the real thing in tools/i86sim.py, inside DDT86.CMD as
tools/build_ddt86.py builds it, and its C version (src-c/toolchain/) on
the host.

DDT86 is an 8080-model program: the whole image is one segment, TSEG
here (CS = DS = SS), the stack at TSEG:SP. The far pointer an entry
takes (DDT86 pushes the segment, then the offset) points into USEG,
where a test puts the instruction bytes to disassemble or reads the
assembled ones from.

The overlays call back into DDT86 through its vectors (103h conin,
106h plmconout, 109h plmgetline, 10Ch asment, 10Fh plmset), reached
through their link stubs (pl_conin, pl_conout, pl_ddtgetline,
pl_goddt, pl_ddtset). Both sides' calls go to one `Ddt` (below), which
does what DDT86's routines do, on the image's memory: getline reads a
scripted console line into DDT86's conbuff (BDOS function 10's, then
an eol, conptr = 0), conin takes the next character from it (upper
case), conout collects output, set stores the byte at type1seg:off
(DDT86's A-command segment), asment (goddt) ends the run. On the asm
side they're i86sim call_hooks on the stubs; on the C side, the ddt_*
services tests/src-c/fixtures/toolchain_host.c makes into rt_int224
calls with pseudo function numbers.

`compare` checks both made the same service calls, printed the same,
returned the same and left the same memory (but the asm's stack).
"""
import ctypes
import os
import sys
import tempfile

from memref import REPO_ROOT, w16, r16  # noqa: F401
from rspref import compile_lib
from i86sim import I86Sim  # noqa: E402

sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))
import build_ddt86  # noqa: E402

SRC_C = os.path.join(REPO_ROOT, 'src-c')
FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')
MEM_SIZE = 0x10000
TSEG = 0x0600       # DDT86's segment (image at TSEG:0, up to TC_DDT86_IMAGE_LEN)
USEG = 0x0100       # the user memory the far pointers point into
SP = 0x9F00         # the stack, in TSEG, past the image
ARGS = 0x9FF0       # toolchain_host.c's TC_HOST_ARGS
SVC = {0xE0: 'conin', 0xE1: 'conout', 0xE2: 'getline', 0xE3: 'asment', 0xE4: 'set'}

_image = None


def image():
    """(the DDT86 image bytes, layout, {piece: (Assembler, publics)})"""
    global _image
    if _image is None:
        cmd, layout, parts = build_ddt86.build(parts=True)
        _image = (cmd[128:], layout, parts)
    return _image


def sym(piece, label):
    asm = image()[2][piece][0]
    return next(v.value for k, v in asm.symtab.items() if k.lower() == label.lower())


def pa(seg, off):
    return seg * 16 + off


class Ddt:
    """DDT86's services, for one run; `lines`: the console lines getline
    reads, in order"""

    def __init__(self, lines=()):
        self.lines = [bytes(ln) for ln in lines]
        self.log, self.out = [], bytearray()

    def _ddt(self, name):
        return sym('ddt86', name)

    def call(self, mem, svc, arg=0, b=0):
        """returns the AX/BX value, or None to stop the run"""
        self.log.append((svc, arg, b) if svc == 'set' else (svc, arg) if svc == 'conout' else (svc,))
        if svc == 'conout':
            self.out.append(arg & 0xFF)
        elif svc == 'getline':
            if not self.lines:
                return None
            line = self.lines.pop(0)[:64]
            buf = pa(TSEG, self._ddt('conbuff'))
            mem[pa(TSEG, self._ddt('conbuffcnt'))] = len(line)
            mem[buf:buf + len(line)] = line
            mem[buf + len(line)] = 0x0D               # eol
            w16(mem, pa(TSEG, self._ddt('conptr')), 0)
        elif svc == 'conin':
            ptr = r16(mem, pa(TSEG, self._ddt('conptr')))
            c = mem[pa(TSEG, self._ddt('conbuff') + ptr)]
            w16(mem, pa(TSEG, self._ddt('conptr')), (ptr + 1) & 0xFFFF)
            return c - 0x20 if ord('a') <= c <= ord('z') else c
        elif svc == 'set':
            seg = r16(mem, pa(TSEG, self._ddt('type1seg')))
            mem[pa(seg, arg)] = b
            if arg == 0xFFFF:                         # plmset: the offset wrapped: out of A
                return None
        elif svc == 'asment':
            return None
        return 0


def asm_run(entry, mem, ddt, off, seg=USEG, max_steps=2000000):
    """the asm entry (a dis/ass label: pl_disem, pl_asm) -> (result, mem)"""
    mem = bytearray(mem)
    sp = SP
    for word in (seg, off):                       # DDT86: push seg, push off
        sp -= 2
        w16(mem, pa(TSEG, sp), word)
    piece = 'dis' if entry == 'pl_disem' else 'ass'

    def hook(name, nargs):
        def run(sim):
            r = sim.regs
            st = pa(r['ss'], r['sp'])             # no return address: the hook replaces the call
            args = [r16(mem, st + 2 * i) for i in range(nargs)]
            res = ddt.call(mem, name, *([args[1], args[0]] if name == 'set' else args))
            r['sp'] = (r['sp'] + 2 * nargs) & 0xFFFF  # the callee pops its parameters
            if res is None:
                sim.halted = True
                return
            r['ax'] = (r['ax'] & 0xFF00) | (res & 0xFF) if name == 'conin' else r['ax']
        return run

    hooks = {}
    for stub, name, nargs in (('pl_conin', 'conin', 0), ('pl_conout', 'conout', 1), ('pl_ddtgetline', 'getline', 0),
                              ('pl_goddt', 'asment', 0), ('pl_ddtset', 'set', 2)):
        p = 'dis' if stub in ('pl_conin', 'pl_conout') else 'ass'
        try:
            hooks[pa(TSEG, sym(p, stub))] = hook(name, nargs)  # (segmented i86sim: linear addresses)
        except StopIteration:
            pass
    regs = {'cs': TSEG, 'ds': TSEG, 'es': TSEG, 'ss': TSEG, 'sp': sp}
    sim = I86Sim(mem, regs, pa(TSEG, sym(piece, entry)), segmented=True, call_hooks=hooks)
    sim.run(max_steps=max_steps)
    assert sim.halted, "the asm run didn't finish"
    return sim.regs['ax'], mem


HANDLER = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint16,
                           ctypes.POINTER(ctypes.c_uint16))


class CLib:
    """src-c/toolchain/'s C, built for the host with toolchain_host.c"""

    def __init__(self, sources):
        tc = os.path.join(SRC_C, 'toolchain')
        path = compile_lib([os.path.join(tc, s) for s in sources] + [os.path.join(FIXTURES, 'toolchain_host.c'),
                                                                     os.path.join(FIXTURES, 'rt_host.c')],
                         [os.path.join(SRC_C, d) for d in ('kernel', 'rt', 'toolchain')])
        self.lib = ctypes.CDLL(path)
        os.remove(path)
        self.lib.rt_host_run.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, HANDLER, ctypes.c_void_p,
                                         ctypes.c_uint16]

    def run(self, main, mem, ddt, off, seg=USEG):
        """the C entry's test main (tc_disem_main, tc_asm_main) -> (result, mem)"""
        buf = (ctypes.c_uint8 * MEM_SIZE).from_buffer_copy(mem)
        view = memoryview(buf).cast('B')
        w16(view, pa(TSEG, ARGS), off)
        w16(view, pa(TSEG, ARGS + 2), seg)

        @HANDLER
        def handler(ds, es, cl, dx, out):
            name = SVC.get(cl)
            assert name, f"unexpected int 224 function {cl}"
            res = ddt.call(view, name, dx, es & 0xFF)
            if res is None:
                return 1
            out[0], out[1], out[2] = res, 0, es
            return 0

        self.lib.rt_host_run(buf, 0, handler, ctypes.cast(getattr(self.lib, main), ctypes.c_void_p), TSEG)
        res = r16(view, pa(TSEG, ARGS + 4))
        del view
        mem = bytearray(buf)
        for i in range(6):                        # the host's argument block isn't the asm's
            mem[pa(TSEG, ARGS) + i] = 0
        return res, mem


def memory(setup=None):
    """the machine both runs start from: DDT86's image at TSEG"""
    body = image()[0]
    mem = bytearray(MEM_SIZE)
    mem[pa(TSEG, 0):pa(TSEG, len(body))] = body
    if setup:
        setup(mem)
    return mem


def compare(ck, label, mem, entry, c_lib, c_main, lines=(), off=0, seg=USEG, result=True):
    """run both sides; check the services, output, result and memory"""
    ref_ddt, port_ddt = Ddt(lines), Ddt(lines)
    if entry == 'pl_asm':  # DDT86's A command: plmset stores at type1seg:off
        mem = bytearray(mem)
        w16(mem, pa(TSEG, sym('ddt86', 'type1seg')), seg)
        w16(mem, pa(TSEG, sym('ddt86', 'type1loc')), off)
    ref_res, ref = asm_run(entry, mem, ref_ddt, off, seg)
    port_res, port = c_lib.run(c_main, mem, port_ddt, off, seg)
    ck.check(f"{label}: same services", ref_ddt.log, port_ddt.log)
    ck.check(f"{label}: same output", bytes(ref_ddt.out), bytes(port_ddt.out))
    if result:
        ck.check(f"{label}: same result", ref_res, port_res)
    stack = set(range(pa(TSEG, SP - 0x400), pa(TSEG, SP)))
    diffs = [hex(a) for a in range(MEM_SIZE) if a not in stack and ref[a] != port[a]]
    ck.check(f"{label}: no other bytes differ", [], diffs[:8])
    return ref_ddt, ref, ref_res
