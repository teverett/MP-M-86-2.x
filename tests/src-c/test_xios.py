#!/usr/bin/env python3
"""Differential test for src-c/kernel/xios.c against src-assem/kernel/XIOS.A86's
portable function-set routines: SETTRK/SETSEC/SETDMA/SETDMAB/SECTRAN/
SELDSK/MAXCONSOLE/MAXLIST/GETSEGT/SELMEMORY/FLUSHBUF/STRTCLK/STOPCLK. See
xios.c's own header comment for why the rest of the file (real `in`/`out`
hardware I/O, the 8271 floppy driver, interrupt handlers, `init`) is not
ported.

XIOS.A86 assembles standalone (`include singles.lib`). It is the DRI
XIOS, which lives in the SYSDAT segment: it orgs past sysdat.lib's layout,
and its fields follow its code in a DSEG at the same segment (CS = DS).
The reference runs it that way at XSEG, segmented; the C gets real memory
and XSEG as its `sysdat`. Every case compares AX and diffs the whole
image.
"""
import ctypes
import os
import sys

from memref import REPO_ROOT, KERNEL_C, build_lib, c_buffer, phys, w16  # noqa: F401
from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

XIOS_A86 = os.path.join(REPO_ROOT, 'src-assem', 'kernel', 'XIOS.A86')
XSEG = 0x0080                       # the XIOS's segment (SYSDAT): CS = DS
STACK_SEG, STACK_TOP = 0x0F00, 0x0100
RET_IP = 0xFFF0
FUNCS = ('settrk', 'setsec', 'setdma', 'setdmab', 'sectran', 'seldsk',
         'maxconsole', 'maxlist', 'getsegt', 'selmemory', 'flushbuf',
         'strtclk', 'stopclk')
u8, u16 = ctypes.c_uint8, ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)


def main():
    asm = Assembler()
    asm.assemble(XIOS_A86, [os.path.join(REPO_ROOT, 'src-assem', 'kernel')])
    code, dseg = asm.segment_bytes('cseg'), asm.segment_bytes('dseg')
    sym = {k: v.value for k, v in asm.symtab.items()}
    stack = set(range(phys(STACK_SEG, STACK_TOP - 4), phys(STACK_SEG, STACK_TOP)))
    lib_path = build_lib([os.path.join(KERNEL_C, 'xios.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        for name in ('settrk', 'setsec'):
            getattr(lib, name).argtypes = [c_u8p, u16, u8]
        for name in ('setdma', 'setdmab'):
            getattr(lib, name).argtypes = [c_u8p, u16, u16]
        lib.sectran.argtypes = [c_u8p, u16, u16, u16]
        lib.sectran.restype = u16
        lib.seldsk.argtypes = [c_u8p, u16, u8]
        lib.seldsk.restype = u16
        lib.maxconsole.restype = lib.maxlist.restype = lib.getsegt.restype = u16
        lib.flushbuf.restype = u8
        lib.strtclk.argtypes = lib.stopclk.argtypes = [c_u8p, u16]
        for name in ('settrk', 'setsec', 'setdma', 'setdmab', 'strtclk', 'stopclk'):
            getattr(lib, name).restype = None
        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def image(mem=None):
            img = bytearray(0x10000)
            img[phys(XSEG):phys(XSEG, len(code))] = code
            img[phys(XSEG, len(code)):phys(XSEG, len(dseg))] = dseg[len(code):]
            for addr, val in (mem or {}).items():
                img[phys(XSEG, addr)] = val
            return img

        def run(label, func, regs, call, mem=None, result=lambda r: r['ax']):
            ref = image(mem)
            w16(ref, phys(STACK_SEG, STACK_TOP - 2), RET_IP)
            sim = I86Sim(ref, {**regs, 'cs': XSEG, 'ds': XSEG, 'es': XSEG, 'ss': STACK_SEG, 'sp': STACK_TOP - 2},
                         phys(XSEG, sym[func]), segmented=True, stop_addrs={phys(XSEG, RET_IP)})
            sim.run()
            port = image(mem)
            buf = c_buffer(port)
            got = call(buf)
            check(label, result(sim.regs), got)
            check(f"{label}: no other bytes differ", [],
                  [hex(a) for a in range(0x10000) if a not in stack and ref[a] != buf[a]][:8])

        for cl in (0, 5, 0xFF):
            run(f"SETTRK cl={cl:#x}", 'settrk', {'cx': cl}, lambda b, cl=cl: lib.settrk(b, XSEG, cl), result=lambda r: None)
            run(f"SETSEC cl={cl:#x}", 'setsec', {'cx': cl}, lambda b, cl=cl: lib.setsec(b, XSEG, cl), result=lambda r: None)
        for cx in (0, 0x1234, 0xFFFF):
            run(f"SETDMA cx={cx:#x}", 'setdma', {'cx': cx}, lambda b, cx=cx: lib.setdma(b, XSEG, cx),
                result=lambda r: None)
            run(f"SETDMAB cx={cx:#x}", 'setdmab', {'cx': cx}, lambda b, cx=cx: lib.setdmab(b, XSEG, cx),
                result=lambda r: None)
        run("SECTRAN no table (dx=0)", 'sectran', {'cx': 7, 'dx': 0}, lambda b: lib.sectran(b, XSEG, 7, 0))
        run("SECTRAN via table", 'sectran', {'cx': 3, 'dx': 0x2000}, lambda b: lib.sectran(b, XSEG, 3, 0x2000),
            mem={0x2003: 0x19})
        for cl in (0, 1, 2, 0x11):
            run(f"SELDSK cl={cl:#x}", 'seldsk', {'cx': cl}, lambda b, cl=cl: lib.seldsk(b, XSEG, cl))
        run("maxconsole", 'maxconsole', {}, lambda b: lib.maxconsole())
        run("maxlist", 'maxlist', {}, lambda b: lib.maxlist())
        run("getsegt", 'getsegt', {}, lambda b: lib.getsegt())
        run("flushbuf", 'flushbuf', {}, lambda b: lib.flushbuf(), result=lambda r: r['ax'] & 0xFF)
        run("STRTCLK", 'strtclk', {}, lambda b: lib.strtclk(b, XSEG), result=lambda r: None)
        run("STOPCLK", 'stopclk', {}, lambda b: lib.stopclk(b, XSEG), mem={sym['clockon']: 0xFF},
            result=lambda r: None)
        check("defs.h XIOS_* match XIOS.A86",
              [sym[n] for n in ('dma_adr', 'dma_seg', 'trk', 'sect', 'disk', 'dtrntbl', 'dpbase', 'clockon')],
              [0x16A4, 0x16A6, 0x16AB, 0x16AC, 0x16B4, 0x16B5, 0x16D1, 0x182A])
        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
