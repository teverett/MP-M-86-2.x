#!/usr/bin/env python3
"""Differential test for src-c/loader/ldbios.c against the whole of
src/loader/LDBIOS.A86: the real iSBC 86/12 + iSBC 204 driver. See
ldbios.c's own header comment for exactly what's not modeled (`INIT`'s
own no-op hardware setup, `WBOOT`, `LISTOUT`/`LISTST`/`SETIOBF`, the
error-retry-with-console-interaction tail) and why.

Uses its own minimal synthetic i8251/8271/8257 device (`Device` below,
same "deliberately narrow, not a real controller model" philosophy as
tests/src-c/fixtures/fdc86q.py/iorecorder.py): `fdc_stat` always reports
"not busy, not full, interrupt-request set" (0x08) so every poll in
`execute`/`send_com` -- whichever bit it's actually waiting on --
succeeds on the first try; `csts` always reports both RDA and TBE set.
Backs both tools/i86sim.py's `io_in_hook`/`io_out_hook` and ldbios.c's
own `io_in8`/`io_out8` C callbacks, same "one real model, two call
sites" shape as every other hardware-I/O test in this port.
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

REAL_FILE = os.path.join(REPO_ROOT, 'src', 'loader', 'LDBIOS.A86')
LDBIOS_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'ldbios.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0
CSTS, CDATA = 0xDA, 0xD8
FDC_STAT, FDC_COM, FDC_PARM = 0xA0, 0xA0, 0xA1
DMAC_ADR, DMAC_CONT, DMAC_MODE, FDC_SEGMENT = 0xA4, 0xA5, 0xA8, 0xAA

LDB_TRK, LDB_SECT = 0x1541, 0x1542
LDB_DMA_ADR, LDB_DMA_SEG = 0x153A, 0x153C
LDB_SEL_MASK = 0x153E
LDB_DPBASE, LDB_SEGTABLE = 0x154D, 0x1548


class Device:
    def __init__(self, rx_byte=0x00):
        self.sent = []       # every FDC command/parameter byte written
        self.dma_writes = []  # every 8257 register write
        self.tx = []         # every console char transmitted
        self.rx_byte = rx_byte

    def in8(self, port):
        if port == CSTS:
            return 0x03  # RDA + TBE both set
        if port == CDATA:
            return self.rx_byte
        if port == FDC_STAT:
            return 0x08  # not busy, not full, interrupt-request set
        if port == 0xA1:  # fdc_rslt (shares its port number with fdc_parm)
            return 0x00  # no error bits set
        raise ValueError(f"Device: unexpected IN from port 0x{port:02X}")

    def out8(self, port, value):
        value &= 0xFF
        if port == CDATA:
            self.tx.append(value)
        elif port in (FDC_COM, FDC_PARM):
            self.sent.append(value)
        elif port in (DMAC_ADR, DMAC_CONT, DMAC_MODE, FDC_SEGMENT):
            self.dma_writes.append((port, value))
        else:
            raise ValueError(f"Device: unexpected OUT to port 0x{port:02X}")


def assemble_reference():
    asm = Assembler()
    asm.assemble(REAL_FILE, [])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    entries = {name: asm.symtab[name.lower()].value
               for name in ('CONST', 'CONIN', 'CONOUT', 'PUNCH', 'READER', 'GETIOBF',
                             'SELDSK', 'HOME', 'SETTRK', 'SETSEC', 'SECTRAN',
                             'SETDMA', 'SETDMAB', 'GETSEGT', 'READ', 'WRITE')}
    return code, dseg, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, LDBIOS_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, dseg, entry, regs, device, mem=None):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    # LDBIOS.A86's own DSEG switch happens right at `data_offset`
    # (cseg's own length) -- same technique as src-c/kernel/xios.c's
    # own differential test uses for XIOS.A86's own real DSEG switch.
    image[len(code):len(dseg)] = dseg[len(code):]
    if mem is not None:
        for addr, val in enumerate(mem):
            if val != 0:
                image[addr] = val

    def io_in_hook(sim, port, width):
        assert width == 8
        return device.in8(port)

    def io_out_hook(sim, port, width, value):
        assert width == 8
        device.out8(port, value)

    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry,
                 io_in_hook=io_in_hook, io_out_hook=io_out_hook)
    sim.run()
    return sim.regs, image


def main():
    code, dseg, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        IN8_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.c_uint16)
        OUT8_FN = ctypes.CFUNCTYPE(None, ctypes.c_uint16, ctypes.c_uint8)

        lib.const_entry.restype = ctypes.c_uint8
        lib.const_entry.argtypes = [IN8_FN]
        lib.conin_entry.restype = ctypes.c_uint8
        lib.conin_entry.argtypes = [IN8_FN]
        lib.conout_entry.restype = None
        lib.conout_entry.argtypes = [IN8_FN, OUT8_FN, ctypes.c_uint8]
        lib.punch_entry.restype = ctypes.c_uint8
        lib.punch_entry.argtypes = []
        lib.reader_entry.restype = ctypes.c_uint8
        lib.reader_entry.argtypes = []
        lib.getiobf_entry.restype = ctypes.c_uint8
        lib.getiobf_entry.argtypes = []
        lib.seldsk_entry.restype = ctypes.c_uint16
        lib.seldsk_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.home_entry.restype = None
        lib.home_entry.argtypes = [c_u8p, IN8_FN, OUT8_FN]
        lib.settrk_entry.restype = None
        lib.settrk_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.setsec_entry.restype = None
        lib.setsec_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.sectran_entry.restype = ctypes.c_uint16
        lib.sectran_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.setdma_entry.restype = None
        lib.setdma_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.setdmab_entry.restype = None
        lib.setdmab_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.getsegt_entry.restype = ctypes.c_uint16
        lib.getsegt_entry.argtypes = []
        lib.read_entry.restype = ctypes.c_uint8
        lib.read_entry.argtypes = [c_u8p, IN8_FN, OUT8_FN]
        lib.write_entry.restype = ctypes.c_uint8
        lib.write_entry.argtypes = [c_u8p, IN8_FN, OUT8_FN]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def make_c_hooks(device):
            in8 = IN8_FN(lambda port: device.in8(port))
            out8 = OUT8_FN(lambda port, value: device.out8(port, value))
            return in8, out8

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- CONST: not ready / ready --
        ref_regs, _ = run_reference(code, dseg, entries['CONST'], {}, Device())
        check("CONST", ref_regs['ax'] & 0xFF, lib.const_entry(make_c_hooks(Device())[0]))

        # -- CONIN: reads the buffered char --
        ref_regs, _ = run_reference(code, dseg, entries['CONIN'], {}, Device(rx_byte=0xD1))
        check("CONIN", ref_regs['ax'] & 0xFF, lib.conin_entry(make_c_hooks(Device(rx_byte=0xD1))[0]))

        # -- CONOUT: transmits a character --
        d1 = Device()
        run_reference(code, dseg, entries['CONOUT'], {'cx': 0x58}, d1)
        d2 = Device()
        in8, out8 = make_c_hooks(d2)
        lib.conout_entry(in8, out8, ctypes.c_uint8(0x58))
        check("CONOUT", d1.tx, d2.tx)

        # -- PUNCH / READER: always EOF --
        ref_regs, _ = run_reference(code, dseg, entries['PUNCH'], {}, Device())
        check("PUNCH: EOF", ref_regs['ax'] & 0xFF, lib.punch_entry())
        ref_regs, _ = run_reference(code, dseg, entries['READER'], {}, Device())
        check("READER: EOF", ref_regs['ax'] & 0xFF, lib.reader_entry())

        # -- GETIOBF --
        ref_regs, _ = run_reference(code, dseg, entries['GETIOBF'], {}, Device())
        check("GETIOBF", ref_regs['ax'] & 0xFF, lib.getiobf_entry())

        # -- GETSEGT --
        ref_regs, _ = run_reference(code, dseg, entries['GETSEGT'], {}, Device())
        check("GETSEGT", ref_regs['bx'], lib.getsegt_entry())
        assert ref_regs['bx'] == LDB_SEGTABLE

        # -- SELDSK: drive 0 / drive 1 / out-of-range. For an invalid
        # drive, sel_mask is untouched -- its own real DSEG default
        # (0x40, `sel_mask db 40h`) shows through on the reference
        # side once there's no SELDSK(0 or 1) before it to overwrite
        # it, so that one case only checks bx, not sel_mask (a static-
        # data artifact, not something SELDSK itself computes). --
        for cl in (0, 1, 2):
            mem = bytearray(0x10000)
            buf = to_ctypes(mem)
            port_bx = lib.seldsk_entry(buf, ctypes.c_uint8(cl))
            ref_regs, ref_img = run_reference(code, dseg, entries['SELDSK'], {'cx': cl}, Device())
            if cl == 2:
                check(f"SELDSK cl={cl}", ref_regs['bx'], port_bx)
            else:
                check(f"SELDSK cl={cl}", (ref_regs['bx'], ref_img[LDB_SEL_MASK]),
                      (port_bx, bytes(buf)[LDB_SEL_MASK]))
        assert run_reference(code, dseg, entries['SELDSK'], {'cx': 0}, Device())[0]['bx'] == LDB_DPBASE
        assert run_reference(code, dseg, entries['SELDSK'], {'cx': 1}, Device())[0]['bx'] == LDB_DPBASE + 16

        # -- SETTRK / SETSEC --
        ref_regs, ref_img = run_reference(code, dseg, entries['SETTRK'], {'cx': 0x2A}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.settrk_entry(buf, ctypes.c_uint8(0x2A))
        check("SETTRK", ref_img[LDB_TRK], bytes(buf)[LDB_TRK])

        ref_regs, ref_img = run_reference(code, dseg, entries['SETSEC'], {'cx': 0x07}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.setsec_entry(buf, ctypes.c_uint8(0x07))
        check("SETSEC", ref_img[LDB_SECT], bytes(buf)[LDB_SECT])

        # -- SETDMA / SETDMAB --
        ref_regs, ref_img = run_reference(code, dseg, entries['SETDMA'], {'cx': 0x1234}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.setdma_entry(buf, ctypes.c_uint16(0x1234))
        check("SETDMA", ref_img[LDB_DMA_ADR:LDB_DMA_ADR + 2], bytes(buf)[LDB_DMA_ADR:LDB_DMA_ADR + 2])

        ref_regs, ref_img = run_reference(code, dseg, entries['SETDMAB'], {'cx': 0x5678}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.setdmab_entry(buf, ctypes.c_uint16(0x5678))
        check("SETDMAB", ref_img[LDB_DMA_SEG:LDB_DMA_SEG + 2], bytes(buf)[LDB_DMA_SEG:LDB_DMA_SEG + 2])

        # -- SECTRAN: always a table lookup (no dx==0 shortcut in this
        # real file, unlike LDBIOS86Q.A86/XIOS.A86), result in BX --
        mem = bytearray(0x10000)
        mem[9] = 0x42
        ref_regs, _ = run_reference(code, dseg, entries['SECTRAN'], {'cx': 9, 'dx': 0}, Device(), mem=mem)
        check("SECTRAN: dx=0", ref_regs['bx'],
              lib.sectran_entry(to_ctypes(mem), ctypes.c_uint16(9), ctypes.c_uint16(0)))

        table_base = 0x2000
        table_data = bytes([10, 20, 30, 40])
        mem = bytearray(0x10000)
        mem[table_base:table_base + len(table_data)] = table_data
        ref_regs, _ = run_reference(code, dseg, entries['SECTRAN'], {'cx': 2, 'dx': table_base}, Device(), mem=mem)
        c_mem = to_ctypes(mem)
        check("SECTRAN: table lookup", ref_regs['bx'],
              lib.sectran_entry(c_mem, ctypes.c_uint16(2), ctypes.c_uint16(table_base)))

        # -- HOME: recalibrate to track 0. sel_mask set explicitly on
        # both sides (0x40, drive 0) rather than relying on either
        # side's own default (the real DSEG's own `sel_mask db 40h`
        # vs. this port's own all-zero mem) -- see the SELDSK test's
        # own comment for why that default isn't something to assert
        # on, let alone depend on silently here. --
        mem = bytearray(0x10000)
        mem[LDB_SEL_MASK] = 0x40
        ref_fdc = Device()
        ref_regs, ref_img = run_reference(code, dseg, entries['HOME'], {}, ref_fdc, mem=mem)
        c_fdc = Device()
        in8, out8 = make_c_hooks(c_fdc)
        buf = to_ctypes(mem)
        lib.home_entry(buf, in8, out8)
        check("HOME: trk reset to 0", ref_img[LDB_TRK], bytes(buf)[LDB_TRK])
        check("HOME: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)

        # -- READ / WRITE: command bytes + DMA setup, given drive 0
        # (sel_mask=0x40) and a chosen trk/sect/dma_adr/dma_seg --
        for name, entry_name, c_fn in (("READ", 'READ', lib.read_entry), ("WRITE", 'WRITE', lib.write_entry)):
            mem = bytearray(0x10000)
            mem[LDB_SEL_MASK] = 0x40
            mem[LDB_TRK] = 5
            mem[LDB_SECT] = 3
            mem[LDB_DMA_ADR] = 0x00
            mem[LDB_DMA_ADR + 1] = 0x20
            mem[LDB_DMA_SEG] = 0x34
            mem[LDB_DMA_SEG + 1] = 0x12
            ref_fdc = Device()
            ref_regs, ref_img = run_reference(code, dseg, entries[entry_name], {}, ref_fdc, mem=mem)
            c_fdc = Device()
            in8, out8 = make_c_hooks(c_fdc)
            c_mem = to_ctypes(mem)
            port_al = c_fn(c_mem, in8, out8)
            check(f"{name}: AL result", ref_regs['ax'] & 0xFF, port_al)
            check(f"{name}: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)
            check(f"{name}: DMA register writes", ref_fdc.dma_writes, c_fdc.dma_writes)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
