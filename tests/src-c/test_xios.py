#!/usr/bin/env python3
"""Differential test for src-c/kernel/xios.c against src/kernel/XIOS.A86's
portable function-set routines: SETTRK/SETSEC/SETDMA/SETDMAB/SECTRAN/
SELDSK/MAXCONSOLE/MAXLIST/GETSEGT/SELMEMORY/FLUSHBUF/STRTCLK/STOPCLK. See
xios.c's own header comment for why the rest of the file (real `in`/`out`
hardware I/O, the 8271 floppy driver, interrupt handlers, `init`) is not
ported.

XIOS.A86 assembles standalone (`include singles.lib`, same include dir as
every other src/kernel/*.A86 file) now that docs/bugs.md #7's `dp_base`/
`dpbase` typo is fixed. It switches from `cseg` to a real `DSEG` partway
through (right after `endcode`, just before the function table) for
`trk`/`sect`/`dma_adr`/`dma_seg`/`disk`/`dtrntbl`/`stoppoll`/`clockon`.
Both of `tools/asm86.py`'s `segment_bytes('cseg'/'dseg')` buffers are
indexed by the same absolute address from 0 (not a from-the-start-of-
this-segment relative offset), so they're combined by writing `cseg`
into a flat image first and then `dseg` over it (same order and reason
as `test_file1.py`'s own `run_reference`: dseg's own low region, below
its real DSEG switch point, is unused zero padding, so it never clobbers
cseg's code when written second).
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

XIOS_A86 = os.path.join(REPO_ROOT, 'src', 'kernel', 'XIOS.A86')
XIOS_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'xios.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
FUNCS = ('settrk', 'setsec', 'setdma', 'setdmab', 'sectran', 'seldsk',
         'maxconsole', 'maxlist', 'getsegt', 'selmemory', 'flushbuf',
         'strtclk', 'stopclk')
FIELDS = ('trk', 'sect', 'dma_adr', 'dma_seg', 'disk', 'clockon', 'dpbase')

# xios.c's own defs.h scratch addresses -- a fixed, independently-chosen
# base, same reasoning as every other SD_*/XIOS_* constant in defs.h (not
# the same numeric address as the real file's own `trk`/`sect`/etc.,
# which only the reference run below ever uses).
PORT_TRK, PORT_SECT, PORT_DMA_ADR, PORT_DMA_SEG, PORT_DISK, PORT_CLOCKON = (
    0xAC00, 0xAC01, 0xAC02, 0xAC04, 0xAC06, 0xAC07)
PORT_DPBASE = 0xAC20


def assemble_reference():
    asm = Assembler()
    asm.assemble(XIOS_A86, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    entries = {name: asm.symtab[name].value for name in FUNCS}
    fields = {name: asm.symtab[name].value for name in FIELDS}
    return code, dseg, entries, fields


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, XIOS_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, dseg, entry, mem, regs):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    # XIOS.A86's own DSEG switch happens right at `endcode` (cseg's own
    # length) -- everything in dseg below that is unused zero padding
    # that must NOT overwrite cseg's real code, same reasoning as
    # test_file1.py's own 0xC00 cutoff for DATA.BDO.
    image[len(code):len(dseg)] = dseg[len(code):]
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry)
    sim.run()
    return sim.regs, image


def main():
    code, dseg, entries, fields = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        lib.settrk.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
        lib.setsec.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
        lib.setdma.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        lib.setdmab.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16]
        lib.sectran.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint16]
        lib.sectran.restype = ctypes.c_uint16
        lib.seldsk.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
        lib.seldsk.restype = ctypes.c_uint16
        lib.maxconsole.restype = ctypes.c_uint16
        lib.maxlist.restype = ctypes.c_uint16
        lib.getsegt.restype = ctypes.c_uint16
        lib.flushbuf.restype = ctypes.c_uint8
        lib.strtclk.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        lib.stopclk.argtypes = [ctypes.POINTER(ctypes.c_uint8)]

        failures = []

        def check(label, ref_ax, port_ax, ref_mem=None, port_mem=None, watch=()):
            ok = ref_ax == port_ax
            if ok and ref_mem is not None:
                for addr in watch:
                    if ref_mem[addr] != port_mem[addr]:
                        ok = False
            status = "ok" if ok else "FAIL"
            print(f"[{status}] {label}: reference {ref_ax} | port {port_ax}")
            if not ok:
                failures.append(label)

        # SETTRK / SETSEC: store CL into trk/sect (8 bits only)
        for cl in (0, 5, 0xFF):
            mem = bytearray(0x10000)
            buf = (ctypes.c_uint8 * 0x10000)(*mem)
            lib.settrk(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), cl)
            _, ref_mem = run_reference(code, dseg, entries['settrk'], mem, {'cx': cl})
            check(f"SETTRK cl={cl:#x}", ref_mem[fields['trk']], buf[PORT_TRK])

            mem = bytearray(0x10000)
            buf = (ctypes.c_uint8 * 0x10000)(*mem)
            lib.setsec(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), cl)
            _, ref_mem = run_reference(code, dseg, entries['setsec'], mem, {'cx': cl})
            check(f"SETSEC cl={cl:#x}", ref_mem[fields['sect']], buf[PORT_SECT])

        # SETDMA / SETDMAB: store full CX into dma_adr/dma_seg
        for cx in (0, 0x1234, 0xFFFF):
            mem = bytearray(0x10000)
            buf = (ctypes.c_uint8 * 0x10000)(*mem)
            lib.setdma(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), cx)
            _, ref_mem = run_reference(code, dseg, entries['setdma'], mem, {'cx': cx})
            ref_val = ref_mem[fields['dma_adr']] | (ref_mem[fields['dma_adr'] + 1] << 8)
            port_val = buf[PORT_DMA_ADR] | (buf[PORT_DMA_ADR + 1] << 8)
            check(f"SETDMA cx={cx:#x}", ref_val, port_val)

            mem = bytearray(0x10000)
            buf = (ctypes.c_uint8 * 0x10000)(*mem)
            lib.setdmab(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), cx)
            _, ref_mem = run_reference(code, dseg, entries['setdmab'], mem, {'cx': cx})
            ref_val = ref_mem[fields['dma_seg']] | (ref_mem[fields['dma_seg'] + 1] << 8)
            port_val = buf[PORT_DMA_SEG] | (buf[PORT_DMA_SEG + 1] << 8)
            check(f"SETDMAB cx={cx:#x}", ref_val, port_val)

        # SECTRAN: passthrough when dx==0, else table lookup through dx
        mem = bytearray(0x10000)
        buf = (ctypes.c_uint8 * 0x10000)(*mem)
        ref_regs, _ = run_reference(code, dseg, entries['sectran'], mem, {'cx': 7, 'dx': 0})
        port_ax = lib.sectran(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), 7, 0)
        check("SECTRAN no table (dx=0)", ref_regs['ax'], port_ax)

        TRAN_TABLE = 0x3000
        mem = bytearray(0x10000)
        mem[TRAN_TABLE + 3] = 0x19
        buf = (ctypes.c_uint8 * 0x10000)(*mem)
        ref_regs, _ = run_reference(code, dseg, entries['sectran'], mem, {'cx': 3, 'dx': TRAN_TABLE})
        port_ax = lib.sectran(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), 3, TRAN_TABLE)
        check("SECTRAN via table", ref_regs['ax'], port_ax)

        # SELDSK: A (0) and B (1) are valid, C (2) is not. The returned
        # address is each world's own DPH table base (real `dpbase` vs
        # this port's own XIOS_DPBASE, deliberately different numeric
        # bases, same as every other "table base" scratch constant in
        # defs.h) plus 16 * the physical drive number -- compared
        # structurally (offset from each one's own base), not by raw
        # value, along with `disk`'s own saved physical drive number.
        for cl in (0, 1, 2):
            mem = bytearray(0x10000)
            buf = (ctypes.c_uint8 * 0x10000)(*mem)
            port_ax = lib.seldsk(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), cl)
            ref_regs, ref_mem = run_reference(code, dseg, entries['seldsk'], mem, {'cx': cl})
            ref_offset = 0 if ref_regs['ax'] == 0 else ref_regs['ax'] - fields['dpbase']
            port_offset = 0 if port_ax == 0 else port_ax - PORT_DPBASE
            check(f"SELDSK cl={cl}", (ref_offset, ref_mem[fields['disk']]),
                  (port_offset, buf[PORT_DISK]))

        # The fixed-answer functions
        mem = bytearray(0x10000)
        ref_regs, _ = run_reference(code, dseg, entries['maxconsole'], mem, {})
        check("maxconsole", ref_regs['ax'], lib.maxconsole())

        ref_regs, _ = run_reference(code, dseg, entries['maxlist'], mem, {})
        check("maxlist", ref_regs['ax'], lib.maxlist())

        ref_regs, _ = run_reference(code, dseg, entries['getsegt'], mem, {})
        check("getsegt", ref_regs['ax'], lib.getsegt())

        ref_regs, _ = run_reference(code, dseg, entries['flushbuf'], mem, {})
        check("flushbuf", ref_regs['ax'] & 0xFF, lib.flushbuf())

        # STRTCLK / STOPCLK: clockon true/false (real code uses `true`/
        # `false` EQUs -- confirm their real values via the reference run)
        mem = bytearray(0x10000)
        buf = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.strtclk(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)))
        _, ref_mem = run_reference(code, dseg, entries['strtclk'], mem, {})
        check("STRTCLK", ref_mem[fields['clockon']], buf[PORT_CLOCKON])

        mem = bytearray(0x10000)
        mem[fields['clockon']] = 0xff
        buf = (ctypes.c_uint8 * 0x10000)(*mem)
        buf[PORT_CLOCKON] = 0xff
        lib.stopclk(ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)))
        _, ref_mem = run_reference(code, dseg, entries['stopclk'], mem, {})
        check("STOPCLK", ref_mem[fields['clockon']], buf[PORT_CLOCKON])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
