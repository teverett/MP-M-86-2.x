"""Harness for tests/src-c/test_ddt86.py: DDT86 as a whole program,
run twice from the same machine and script -- the real DDT86.CMD (as
tools/build_ddt86.py builds it: DDT86.A86 with its two PL/M overlays) in
tools/i86sim.py, and src-c/toolchain/ddt86.c (with dis86.c, ins86.c,
ass86.c, asmtab.c) on the host -- against one scripted MP/M.

The machine (i86sim's 64K):
  0000-03FF  the interrupt vectors; 224 (the BDOS's) is set, so
             DDT86's copy of it to 225 shows
  4000-40FF  rspref's UDA (its Mpm keeps the search place there)
  4100-      DDT86 (DSEG): the CMD image from offset 0 -- base page,
             code from 100h, data from endcs, the overlays -- run as an
             8080-model program is: CS = DS = SS = DSEG, IP = 100h; the
             CCP's stack is DSEG:100h down (ddt86.c's DDT86_CCP_SP)
  A000-FFFF  user memory: what R reads into (function 53's block) and E
             loads (function 59)

The asm makes its BDOS calls through int 225 (setbdosint's copy of
224): both interrupts are i86sim int_hooks into the Mpm; the C's calls
are rt_int224s through fixtures/rt_host.c. DdtMpm is rspref's Mpm plus
what DDT86 needs: write sequential, make failing, the memory functions
(53, 56, 57) and program load (59, which lays a CMD file's groups out
from LOAD_SEG and writes the base page as the C kernel's load.c does).

The asm runs with i86sim's IF tracking on (track_if), interrupts
enabled, as LOAD starts a program (ddt86.c's DDT86_SYSFLAGS): DDT86's
start-up tests IF in its `pushf; pop ax`.

`compare` checks the two made the same system calls (function, DX, DS),
printed the same, and left the same memory, except: the asm's stack
(DDT86's 96 bytes below stackp), the CCP's stack (DSEG:FE-FF, the
asm's pushf), DDT86's code (setbdosint patches its int 225 into it --
the C's must be untouched), and the single-step and breakpoint vectors
(0:4-0:0Fh), which the C doesn't set (ddt86.c's header).
"""
import ctypes
import os
import struct
import sys

import rspref
import toolref
from memref import w16, r16
from rspref import Mpm, CpmFile, compile_lib, pa, FIXTURES, SRC_C, KERNEL_C, HANDLER
from i86sim import I86Sim

MEM_SIZE = 0x10000
C_MEM_SIZE = 0x110000  # the C's buffer: real memory's 1M (it doesn't wrap at 64K)
DSEG = 0x0410
CCP_SP = 0x0100
USER_SEG = 0x0A00      # user memory, to FFFF
USER_PARAS = 0x0600
LOAD_SEG = 0x0A00      # where function 59 loads a program
BDOS_VECTOR = (0x1234, 0xF000)
C_SOURCES = ['ddt86.c', 'dis86.c', 'ins86.c', 'ass86.c', 'asmtab.c']


def image():
    return toolref.image()


def sym(label):
    return toolref.sym('ddt86', label)


def layout():
    return image()[1]


class DdtMpm(Mpm):
    """rspref's Mpm and what DDT86 needs besides:
      mem_paras -- the user memory function 53 offers (0: none)
      make_fail -- make returns 0FFh (no directory space)
      capacity  -- records write sequential takes before it fails (2)
      load_fail -- program load returns 0FFFFh"""

    def __init__(self, mem_paras=USER_PARAS, make_fail=False, capacity=None, load_fail=False, **kw):
        super().__init__(**kw)
        self.mem_paras, self.make_fail, self.capacity, self.load_fail = mem_paras, make_fail, capacity, load_fail
        self.written = 0

    def call(self, mem, ds, es, cl, dx):
        if cl == 21:                                    # write sequential: the record at cr
            self.log.append((cl, dx, ds))
            f = self.files.get(self._fname(mem, ds, dx))
            if f is None:
                return 9, 0, es
            if self.capacity is not None and self.written >= self.capacity:
                return 2, 0, es
            rec = mem[pa(ds, dx + 32)]
            d = self._dma(ds)
            f.recs[rec] = bytearray(mem[d:d + 128])
            mem[pa(ds, dx + 32)] = (rec + 1) & 0xFF
            self.written += 1
            return 0, 0, es
        if cl == 22 and self.make_fail:
            self.log.append((cl, dx, ds))
            return 0xFF, 0, es
        if cl == 53:                                    # get max mem: M_BASE, M_LENGTH
            self.log.append((cl, dx, ds))
            if not self.mem_paras:
                return 0xFF, 0, es
            w16(mem, pa(ds, dx), USER_SEG)
            w16(mem, pa(ds, dx + 2), min(r16(mem, pa(ds, dx + 2)), self.mem_paras))
            return 0, 0, es
        if cl in (56, 57):                              # alloc abs mem, free mem
            self.log.append((cl, dx, ds))
            return 0, 0, es
        if cl == 59:                                    # program load
            self.log.append((cl, dx, ds))
            return self._load(mem, ds, dx), 0, es
        return super().call(mem, ds, es, cl, dx)

    def _load(self, mem, ds, dx):
        f = self.files.get(self._fname(mem, ds, dx))
        if f is None or self.load_fail:
            return 0xFFFF
        data = b''.join(bytes(f.recs[i]) for i in sorted(f.recs))
        groups = []
        for i in range(8):
            form, paras, base, mn, mx = struct.unpack_from('<BHHHH', data, 9 * i)
            if form:
                groups.append((form, paras, max(mn, paras)))
        seg, pos, placed = LOAD_SEG, 128, []
        for form, paras, mn in groups:                  # the groups, one after another
            mem[pa(seg, 0):pa(seg, paras * 16)] = data[pos:pos + paras * 16].ljust(paras * 16, b'\0')
            placed.append((form, seg, mn))
            pos += paras * 16
            seg += mn
        data_g = [p for p in placed if p[0] == 2] or [p for p in placed if p[0] == 1]
        basep = data_g[0][1]
        bp = pa(basep, 0)
        mem[bp:bp + 0x5B] = bytes(0x5B)
        is8080 = not any(p[0] == 2 for p in placed)
        mem[bp + 5] = 1 if is8080 else 0
        for form, s, mn in placed:                      # each group's length and base
            slot = 6 * (form - 1)
            w16(mem, bp + slot, ((mn << 4) - 1) & 0xFFFF)
            mem[bp + slot + 2] = (mn >> 12) & 0xFF
            w16(mem, bp + slot + 3, s)
        if is8080:
            mem[bp + 6:bp + 12] = mem[bp:bp + 6]
        return basep


def cmd_file(groups):
    """a CMD file: [(form, bytes, min paragraphs or None)]"""
    hdr, body = bytearray(128), b''
    for i, (form, data, mn) in enumerate(groups):
        paras = (len(data) + 15) // 16
        struct.pack_into('<BHHHH', hdr, 9 * i, form, paras, 0, mn or paras, 0)
        body += data.ljust(paras * 16, b'\0')
    return bytes(hdr) + body


def machine(tail=b'', setup=None):
    """the memory both runs start from"""
    body = image()[0]
    mem = bytearray(MEM_SIZE)
    mem[pa(DSEG, 0):pa(DSEG, len(body))] = body
    w16(mem, 224 * 4, BDOS_VECTOR[0])
    w16(mem, 224 * 4 + 2, BDOS_VECTOR[1])
    for i in range(1, 4):                               # something for savevect to save
        w16(mem, 4 * i, 0x1100 + i)
        w16(mem, 4 * i + 2, 0x2200 + i)
    if tail:                                            # the command tail (and as the CCP leaves it)
        mem[pa(DSEG, 0x80)] = len(tail)
        mem[pa(DSEG, 0x81):pa(DSEG, 0x81 + len(tail))] = tail
    if setup:
        setup(mem)
    return mem


def run_asm(mem, mpm, max_steps=30000000):
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

    regs = {'cs': DSEG, 'ds': DSEG, 'es': DSEG, 'ss': DSEG, 'sp': CCP_SP + 2}
    sim = I86Sim(mem, regs, pa(DSEG, 0x100), segmented=True, int_hooks={224: hook, 225: hook},
                 track_if=True, iflag=1)               # interrupts on, as LOAD starts a program
    sim.regs['sp'] = CCP_SP                             # no return sentinel (DDT86 never returns): its
    mem[pa(DSEG, CCP_SP):pa(DSEG, CCP_SP + 2)] = image()[0][CCP_SP:CCP_SP + 2]  # 2 bytes back
    sim.run(max_steps=max_steps)
    assert sim.halted, "the asm run didn't finish"
    return mem


_lib = None


def c_lib():
    global _lib
    if _lib is None:
        tc = os.path.join(SRC_C, 'toolchain')
        path = compile_lib([os.path.join(tc, s) for s in C_SOURCES] + [os.path.join(FIXTURES, 'rt_host.c')],
                           [KERNEL_C, os.path.join(SRC_C, 'rt'), tc])
        _lib = ctypes.CDLL(path)
        os.remove(path)
        _lib.rt_host_run.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, HANDLER, ctypes.c_void_p,
                                     ctypes.c_uint16]
    return _lib


def run_c(mem, mpm):
    lib = c_lib()
    buf = (ctypes.c_uint8 * C_MEM_SIZE)()
    ctypes.memmove(buf, bytes(mem), MEM_SIZE)
    view = memoryview(buf).cast('B')

    @HANDLER
    def handler(ds, es, cl, dx, out):
        res = mpm.call(view, ds, es, cl, dx)
        if res is None:
            return 1
        out[0], out[1], out[2] = res
        return 0

    lib.rt_host_run(buf, 0, handler, ctypes.cast(lib.ddt86_main, ctypes.c_void_p), DSEG)
    del view
    out = bytes(buf)
    assert out[MEM_SIZE:] == bytes(C_MEM_SIZE - MEM_SIZE), "the C wrote past 64K"
    return bytearray(out[:MEM_SIZE])


def excluded():
    stackp = layout()['ddt_data_end']
    skip = set(range(pa(DSEG, stackp - 96), pa(DSEG, stackp)))      # the asm's stack
    skip |= set(range(pa(DSEG, CCP_SP - 2), pa(DSEG, CCP_SP)))       # the CCP's (pushf)
    skip |= set(range(pa(DSEG, 0x100), pa(DSEG, sym('userax'))))     # DDT86's code
    skip |= set(range(4, 16))                                        # the int 1 and 3 vectors
    return skip


_skip = None


def compare(ck, label, lines=(), tail=b'', setup=None, check=None, mpm_args=None, ref_lines=None, skip=()):
    """both runs; ref_lines: the asm's lines, when they differ (G, T, U),
    skip: more addresses not to compare"""
    global _skip
    if _skip is None:
        _skip = excluded()
    mpm_args = dict(mpm_args or {})
    mem = machine(tail, setup)
    ref_mpm = DdtMpm(lines=list(ref_lines if ref_lines is not None else lines), **mpm_args)
    port_mpm = DdtMpm(lines=list(lines), **mpm_args)
    ref = run_asm(mem, ref_mpm)
    port = run_c(mem, port_mpm)
    ck.check(f"{label}: same calls", ref_mpm.log, port_mpm.log)
    out_ref = bytes(ref_mpm.out)
    if ref_lines is not None:                           # the echoed lines are the port's
        for a, b in zip(ref_lines, lines):
            out_ref = out_ref.replace(a + b'\r', b + b'\r', 1)
    ck.check(f"{label}: same output", out_ref, bytes(port_mpm.out))
    diffs = [hex(a) for a in range(MEM_SIZE) if a not in _skip and a not in skip and ref[a] != port[a]]
    ck.check(f"{label}: no other bytes differ", [], diffs[:8])
    code = (pa(DSEG, 0x100), pa(DSEG, sym('userax')))
    ck.check(f"{label}: the C leaves DDT86's code alone", True, port[code[0]:code[1]] == mem[code[0]:code[1]])
    files = {k: b''.join(bytes(f.recs[i]) for i in sorted(f.recs)) for k, f in ref_mpm.files.items()}
    pfiles = {k: b''.join(bytes(f.recs[i]) for i in sorted(f.recs)) for k, f in port_mpm.files.items()}
    ck.check(f"{label}: same files", files, pfiles)
    if check:
        check(ck, label, ref, ref_mpm, port, port_mpm)
    return ref, ref_mpm, port, port_mpm
