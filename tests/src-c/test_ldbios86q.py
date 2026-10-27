#!/usr/bin/env python3
"""Differential test for src-c/loader/ldbios86q.c against the whole
of src-assem/loader/LDBIOS86Q.A86: the console-I/O subset (CONST, CONIN,
CONOUT, PUNCH, READER, GETIOBF), INIT, and every disk-I/O entry point
(SELDSK, HOME, SETTRK, SETSEC, SECTRAN, SETDMA, SETDMAB, GETSEGT,
READ, WRITE). LISTOUT/LISTST/SETIOBF are not ported -- see
ldbios86q.c's own header comment (bare `ret`, no observable effect).

READ/WRITE's own test cases run with `I86Sim(segmented=True)` (this
module's own `cs`/`ds`/`ss` pinned to 0 for the test -- see
ldbios86q.c's own header comment for why that's a valid, and the
only meaningfully testable, choice) so the real `dma_seg:dma_adr`
segment:offset copy gets genuine segment arithmetic on the reference
side too, not just the C port's own `mem_r8_seg`/`mem_w8_seg`.

Drives tests/src-c/fixtures/uart86q.py's `Uart86Q` and fdc86q.py's
`Fdc86Q` simulated devices from both sides: tools/i86sim.py's
`io_in_hook`/`io_out_hook` for the reference run, and ctypes
callbacks of the same shape for the C port.
"""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402
from uart86q import Uart86Q  # noqa: E402
from fdc86q import Fdc86Q  # noqa: E402
from iorecorder import IoRecorder  # noqa: E402

REAL_FILE = os.path.join(REPO_ROOT, 'src-assem', 'loader', 'LDBIOS86Q.A86')
LDBIOS86Q_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'ldbios86q.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0
CCP_OFFSET = 0x0103

LDB86Q_DISK, LDB86Q_TRK, LDB86Q_SECT = 0x1424, 0x1425, 0x1426
LDB86Q_DMA_ADR, LDB86Q_DMA_SEG = 0x1427, 0x1429
LDB86Q_DPE0, LDB86Q_SEGTABLE = 0x1660, 0x1650
LDB86Q_SECBUF = 0x1450


def assemble_reference():
    asm = Assembler()
    asm.assemble(REAL_FILE, [])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name.lower()].value
               for name in ('CONST', 'CONIN', 'CONOUT', 'PUNCH', 'READER', 'GETIOBF',
                             'INIT', 'SELDSK', 'HOME', 'SETTRK', 'SETSEC', 'SECTRAN',
                             'SETDMA', 'SETDMAB', 'GETSEGT', 'READ', 'WRITE')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, LDBIOS86Q_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, regs, device, stop_addrs=None, overlay=None, segmented=False):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    if overlay is not None:
        for addr, data in overlay.items():
            image[addr:addr + len(data)] = data

    def io_in_hook(sim, port, width):
        assert width == 8
        return device.in8(port)

    def io_out_hook(sim, port, width, value):
        assert width == 8
        device.out8(port, value)

    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry,
                 stop_addrs=stop_addrs or frozenset(), segmented=segmented,
                 io_in_hook=io_in_hook, io_out_hook=io_out_hook)
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
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
        lib.init_entry.restype = None
        lib.init_entry.argtypes = [IN8_FN, OUT8_FN]
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
        lib.read_entry.argtypes = [c_u8p, IN8_FN, OUT8_FN, ctypes.c_uint16]
        lib.write_entry.restype = ctypes.c_uint8
        lib.write_entry.argtypes = [c_u8p, IN8_FN, OUT8_FN, ctypes.c_uint16]

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

        # -- CONST: not ready --
        ref_regs, _ = run_reference(code, entries['CONST'], {}, Uart86Q(rx_bytes=b''))
        in8, _ = make_c_hooks(Uart86Q(rx_bytes=b''))
        check("CONST: not ready", ref_regs['ax'] & 0xFF, lib.const_entry(in8))

        # -- CONST: ready --
        ref_regs, _ = run_reference(code, entries['CONST'], {}, Uart86Q(rx_bytes=b'A'))
        in8, _ = make_c_hooks(Uart86Q(rx_bytes=b'A'))
        check("CONST: ready", ref_regs['ax'] & 0xFF, lib.const_entry(in8))

        # -- CONIN: character already buffered --
        ref_regs, _ = run_reference(code, entries['CONIN'], {}, Uart86Q(rx_bytes=b'Q'))
        in8, _ = make_c_hooks(Uart86Q(rx_bytes=b'Q'))
        check("CONIN: reads buffered char", ref_regs['ax'] & 0xFF, lib.conin_entry(in8))

        # -- CONOUT: sends a character --
        uart = Uart86Q()
        run_reference(code, entries['CONOUT'], {'cx': 0x58}, uart)
        c_uart = Uart86Q()
        in8, out8 = make_c_hooks(c_uart)
        lib.conout_entry(in8, out8, ctypes.c_uint8(0x58))
        check("CONOUT: transmits char", uart.tx, c_uart.tx)

        # -- PUNCH / READER: always EOF (0x1A), no I/O needed --
        ref_regs, _ = run_reference(code, entries['PUNCH'], {}, Uart86Q())
        check("PUNCH: EOF", ref_regs['ax'] & 0xFF, lib.punch_entry())
        ref_regs, _ = run_reference(code, entries['READER'], {}, Uart86Q())
        check("READER: EOF", ref_regs['ax'] & 0xFF, lib.reader_entry())

        # -- GETIOBF: always 0 --
        ref_regs, _ = run_reference(code, entries['GETIOBF'], {}, Uart86Q())
        check("GETIOBF: zero", ref_regs['ax'] & 0xFF, lib.getiobf_entry())

        # -- GETSEGT: constant segment-table address --
        ref_regs, _ = run_reference(code, entries['GETSEGT'], {}, Uart86Q())
        check("GETSEGT: segtable address", ref_regs['bx'], lib.getsegt_entry())
        assert ref_regs['bx'] == LDB86Q_SEGTABLE

        # -- SELDSK: drive 0 -> dpe0 --
        ref_regs, _ = run_reference(code, entries['SELDSK'], {'cx': 0x0000}, Uart86Q())
        check("SELDSK: drive 0", ref_regs['bx'], lib.seldsk_entry(to_ctypes(bytearray(0x10000)), ctypes.c_uint8(0)))
        assert ref_regs['bx'] == LDB86Q_DPE0

        # -- SELDSK: unsupported drive -> 0 --
        ref_regs, _ = run_reference(code, entries['SELDSK'], {'cx': 0x0001}, Uart86Q())
        check("SELDSK: drive 1 (unsupported)",
              ref_regs['bx'], lib.seldsk_entry(to_ctypes(bytearray(0x10000)), ctypes.c_uint8(1)))

        # -- SETTRK / SETSEC --
        ref_regs, ref_img = run_reference(code, entries['SETTRK'], {'cx': 0x002A}, Uart86Q())
        buf = to_ctypes(bytearray(0x10000))
        lib.settrk_entry(buf, ctypes.c_uint8(0x2A))
        check("SETTRK: sets trk", ref_img[LDB86Q_TRK], bytes(buf)[LDB86Q_TRK])

        ref_regs, ref_img = run_reference(code, entries['SETSEC'], {'cx': 0x0007}, Uart86Q())
        buf = to_ctypes(bytearray(0x10000))
        lib.setsec_entry(buf, ctypes.c_uint8(0x07))
        check("SETSEC: sets sect", ref_img[LDB86Q_SECT], bytes(buf)[LDB86Q_SECT])

        # -- SETDMA / SETDMAB --
        ref_regs, ref_img = run_reference(code, entries['SETDMA'], {'cx': 0x1234}, Uart86Q())
        buf = to_ctypes(bytearray(0x10000))
        lib.setdma_entry(buf, ctypes.c_uint16(0x1234))
        check("SETDMA: sets dma_adr", ref_img[LDB86Q_DMA_ADR:LDB86Q_DMA_ADR + 2],
              bytes(buf)[LDB86Q_DMA_ADR:LDB86Q_DMA_ADR + 2])

        ref_regs, ref_img = run_reference(code, entries['SETDMAB'], {'cx': 0x5678}, Uart86Q())
        buf = to_ctypes(bytearray(0x10000))
        lib.setdmab_entry(buf, ctypes.c_uint16(0x5678))
        check("SETDMAB: sets dma_seg", ref_img[LDB86Q_DMA_SEG:LDB86Q_DMA_SEG + 2],
              bytes(buf)[LDB86Q_DMA_SEG:LDB86Q_DMA_SEG + 2])

        # -- SECTRAN: no translation (dx=0) --
        ref_regs, _ = run_reference(code, entries['SECTRAN'], {'cx': 0x0009, 'dx': 0x0000}, Uart86Q())
        check("SECTRAN: no translation", ref_regs['ax'],
              lib.sectran_entry(to_ctypes(bytearray(0x10000)), ctypes.c_uint16(9), ctypes.c_uint16(0)))

        # -- SECTRAN: real table lookup (dx = table base) --
        table_base = 0x2000
        table_data = bytes([10, 20, 30, 40])
        ref_regs, _ = run_reference(code, entries['SECTRAN'], {'cx': 0x0002, 'dx': table_base}, Uart86Q(),
                                     overlay={table_base: table_data})
        c_mem_bytes = bytearray(0x10000)
        c_mem_bytes[table_base:table_base + len(table_data)] = table_data
        c_mem = to_ctypes(c_mem_bytes)
        check("SECTRAN: table lookup", ref_regs['ax'],
              lib.sectran_entry(c_mem, ctypes.c_uint16(2), ctypes.c_uint16(table_base)))

        # -- INIT: UART/FDC hardware setup, stop at the jump back
        # into LDMPM.A86 (ccp_offset) --
        recorder = IoRecorder()
        run_reference(code, entries['INIT'], {'cs': 0x1000}, recorder, stop_addrs={CCP_OFFSET})
        c_recorder = IoRecorder()
        in8, out8 = make_c_hooks(c_recorder)
        lib.init_entry(in8, out8)
        check("INIT: UART/FDC register writes", recorder.writes, c_recorder.writes)

        # -- HOME: recalibrate to track 0 --
        ref_fdc = Fdc86Q()
        ref_regs, ref_img = run_reference(code, entries['HOME'], {}, ref_fdc)
        c_fdc = Fdc86Q()
        in8, out8 = make_c_hooks(c_fdc)
        buf = to_ctypes(bytearray(0x10000))
        lib.home_entry(buf, in8, out8)
        check("HOME: trk reset to 0", ref_img[LDB86Q_TRK], bytes(buf)[LDB86Q_TRK])
        check("HOME: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)

        # -- READ: trk=0/sect=0 -> secbuf+0, copied out to dma_seg:dma_adr
        # (genuine segment:offset -- see this file's own header comment
        # for why segmented=True with cs=ds=ss=0 is used here) --
        dma_seg, dma_adr = 0x0100, 0x0020
        dma_phys = (dma_seg * 16 + dma_adr) & 0xFFFF
        secbuf_data = bytes(range(128))
        overlay = {
            LDB86Q_TRK: bytes([0]),
            LDB86Q_SECT: bytes([0]),
            LDB86Q_DMA_ADR: dma_adr.to_bytes(2, 'little'),
            LDB86Q_DMA_SEG: dma_seg.to_bytes(2, 'little'),
            LDB86Q_SECBUF: secbuf_data,
        }
        ref_fdc = Fdc86Q()
        ref_regs, ref_img = run_reference(code, entries['READ'], {'cs': 0, 'ds': 0, 'ss': 0}, ref_fdc,
                                           overlay=overlay, segmented=True)
        c_mem = bytearray(0x10000)
        for addr, data in overlay.items():
            c_mem[addr:addr + len(data)] = data
        c_buf = to_ctypes(c_mem)
        c_fdc = Fdc86Q()
        in8, out8 = make_c_hooks(c_fdc)
        al = lib.read_entry(c_buf, in8, out8, ctypes.c_uint16(0))
        check("READ: al (status)", ref_regs['ax'] & 0xFF, al)
        check("READ: copies secbuf to dma_seg:dma_adr",
              ref_img[dma_phys:dma_phys + 128], bytes(c_buf)[dma_phys:dma_phys + 128])
        check("READ: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)
        assert bytes(c_buf)[dma_phys:dma_phys + 128] == secbuf_data

        # -- WRITE: trk=0/sect=0, caller's dma_seg:dma_adr buffer
        # spliced into secbuf+0 (the DMA-to-disk half isn't modeled --
        # see this file's own header comment) --
        caller_data = bytes((200 + i) & 0xFF for i in range(128))
        overlay = {
            LDB86Q_TRK: bytes([0]),
            LDB86Q_SECT: bytes([0]),
            LDB86Q_DMA_ADR: dma_adr.to_bytes(2, 'little'),
            LDB86Q_DMA_SEG: dma_seg.to_bytes(2, 'little'),
            dma_phys: caller_data,
        }
        ref_fdc = Fdc86Q()
        ref_regs, ref_img = run_reference(code, entries['WRITE'], {'cs': 0, 'ds': 0, 'ss': 0}, ref_fdc,
                                           overlay=overlay, segmented=True)
        c_mem = bytearray(0x10000)
        for addr, data in overlay.items():
            c_mem[addr:addr + len(data)] = data
        c_buf = to_ctypes(c_mem)
        c_fdc = Fdc86Q()
        in8, out8 = make_c_hooks(c_fdc)
        al = lib.write_entry(c_buf, in8, out8, ctypes.c_uint16(0))
        check("WRITE: al (status)", ref_regs['ax'] & 0xFF, al)
        check("WRITE: splices dma_seg:dma_adr into secbuf",
              ref_img[LDB86Q_SECBUF:LDB86Q_SECBUF + 128], bytes(c_buf)[LDB86Q_SECBUF:LDB86Q_SECBUF + 128])
        check("WRITE: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)
        assert bytes(c_buf)[LDB86Q_SECBUF:LDB86Q_SECBUF + 128] == caller_data

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
