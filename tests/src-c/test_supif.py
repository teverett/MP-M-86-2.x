#!/usr/bin/env python3
"""Differential test for src-c/kernel/supif.c's func1 (SUPIF.SUP: resolve a
function through sysent -- or an explicit (module, function) in CH/CL --
and dispatch a SUP function through `supfunc`) against the real system,
on real memory layout -- see tests/src-c/supref.py.

The reference is the real SUP with the real RTM, MEM and BDOS beside it,
so a dispatched SUP function runs for real on both sides: the cheap ones
(version, sysdat, tod, serial, parse, rpl, not_implemented, bad_entry)
and the load/CLI family on a path that fails early (load: the header
read fails; clicmd/chain: no free PD) -- enough to show the right target
ran with the right dx/bx: load_entry forces bx = 0, `load` (SUP function
10) and cload_entry (14) take the caller's bx as the PD, which lands in
SYSDAT's load_pd. A function in another module (`gofar`) is a far call in
the asm and, in the C, just the resolved (module, function); the
reference logs that call. Every dispatched case compares bx/cx and diffs
the whole image.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, MOD_NUM, PD_RUN, SYSDAT, UDA, WRKSEG, ULEN, build_lib, c_buffer, phys, sd, wk, w16, r16, KERNEL_C

SUPIF_C = [os.path.join(KERNEL_C, f) for f in
           ('supif.c', 'sysfunc.c', 'rpl.c', 'parse.c', 'command.c', 'load.c', 'que1.c', 'que2.c', 'console.c',
            'chario.c', 'findpd.c', 'sysent.c', 'proc.c', 'dsptch.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c',
            'cpmmem.c', 'file1.c', 'file2.c', 'file3.c', 'file4.c')]
u16, u8 = ctypes.c_uint16, ctypes.c_uint8
c_u8p = ctypes.POINTER(ctypes.c_uint8)
RET_IP = 0xFFF0              # func1's return address: where the reference stops
DX = 0x0040                  # a buffer in u_wrkseg
NEW_PD = 0x1400


class Func1Result(ctypes.Structure):
    _fields_ = [('mod', u8), ('func', u8), ('bx', u16), ('cx', u16),
                ('reached_xios', u8), ('al', u8), ('ah', u8), ('cl', u8), ('dl', u8)]


def main():
    ref = SysRef(('SUP', 'RTM', 'MEM', 'BDOS'))
    v = ref.v
    user_retf = v('user_retf')
    lib_path = build_lib(SUPIF_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.func1.restype = Func1Result
        lib.func1.argtypes = [c_u8p, u16, u8, u8, u16, u16, u16, u16]
        ck = Checker(ref)
        cli = lambda name: sd(v(name))  # noqa: E731

        def base_memory():
            mem = ref.base_memory()
            mem[cli('dayfile')] = 0
            w16(mem, cli('pul'), 0)
            w16(mem, phys(UDA, ULEN - 2), RET_IP)
            for qd, nxt in ((v('mxcliqd'), v('mxloadqd')), (v('mxloadqd'), 0)):
                w16(mem, sd(qd + v('q_link')), nxt)
                w16(mem, sd(qd + v('q_msgcnt')), 1)
            w16(mem, cli('qlr'), v('mxcliqd'))
            mem[cli('tod'):cli('tod') + 5] = b'\x34\x12\x08\x30\x59'
            mem[wk(DX):wk(DX) + 16] = b'\0A:FOO.TXT\0' + bytes(5)    # a CLICB / a parse string
            w16(mem, wk(0x60), DX + 1)                               # a PCB: string, FCB
            w16(mem, wk(0x62), 0x80)
            mem[phys(UDA, v('u_dma_ofst'))] = 0x90
            w16(mem, phys(UDA, v('u_dma_seg')), WRKSEG)
            mem[wk(0x90):wk(0x95)] = b'NOPE\0'                       # a chain's command
            return mem

        def bad_read(sim):
            sim.regs['ax'] = sim.regs['bx'] = 0xFF
        ref.handlers = {(MOD_NUM['BDOS'], 7): bad_read}

        def run(label, ch, cl, dx=0, bx=0x55AA, gofar=None):
            mem = base_memory()
            regs, img = ref.run('func1', mem, {'cx': (ch << 8) | cl, 'dx': dx, 'bx': bx}, stop_addrs={RET_IP},
                                sp=ULEN - 2)
            calls = list(ref.calls)
            buf = c_buffer(mem)
            r = lib.func1(buf, SYSDAT, ch, cl, dx, bx, user_retf, 0)
            if gofar:
                ck.check(f"{label}: resolves to", gofar, (r.mod, r.func))
                ck.check(f"{label}: the reference's far call", [gofar], calls)
                return
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx),
                       snap=lambda i: r16(i, cli('load_pd')))

        # through sysent (CH = 0)
        run("fn 12 -> sup 2: version", 0, 12)
        run("fn 163 -> sup 11: MP/M version", 0, 163)
        run("fn 154 -> sup 8: sysdat address", 0, 154)
        run("fn 155 -> sup 9: tod", 0, 155, dx=DX)
        run("fn 107 -> sup 13: serial number", 0, 107, dx=DX)
        run("fn 152 -> sup 7: parse filename", 0, 152, dx=0x60)
        run("fn 151 -> sup 6: rpl", 0, 151, dx=DX)
        run("fn 200 -> sup 1: bad function", 0, 200)
        run("fn 59 -> sup 4: load_entry (bx forced to 0)", 0, 59, dx=0x80)
        run("fn 47 -> sup 12: chain (no free PD)", 0, 47)
        run("fn 150 -> sup 5: CLI command (no free PD)", 0, 150, dx=DX)
        # an explicit module (CH)
        run("sup 0: not implemented", MOD_NUM['SUP'], 0)
        run("sup 10: load (the caller's bx is the PD)", MOD_NUM['SUP'], 10, dx=0x80, bx=NEW_PD)
        run("sup 14: cload_entry (the caller's bx is the PD)", MOD_NUM['SUP'], 14, dx=0x80, bx=NEW_PD)
        run("fn 2 -> cio 1: gofar", 0, 2, gofar=(MOD_NUM['CIO'], 1))
        run("rtm 1: gofar", MOD_NUM['RTM'], 1, gofar=(MOD_NUM['RTM'], 1))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
