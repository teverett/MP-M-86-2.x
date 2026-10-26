#!/usr/bin/env python3
"""Differential test for src-c/kernel/xios86q.c against the whole of
src/kernel/XIOS86Q.A86: the from-scratch QEMU XIOS actually combined
into the real bootable MPM.bin (not DRI source -- see that file's own
header comment). See xios86q.c's own header comment for exactly what's
not ported (`entry`/`supif`, `idle`, `tickint`'s own ISR shell, the
"yield via supif" tail of `rtm_poll`, and `init`'s own far-pointer
copies/IVT write/`retf`) and why.

XIOS86Q.A86 has no real DRI include dependencies beyond the standard
system.def/modfunc.def/xioscb.def/pd.def/sysdat.fmt chain and assembles
to one fixed layout on its own (confirmed: `python3 tools/asm86.py
src/kernel/XIOS86Q.A86 -I src/kernel --symbols` needs no other wrapper),
so this test assembles it directly, no fixture wrapper needed.

Reuses tests/src-c/fixtures/uart86q.py's `Uart86Q` and fdc86q.py's
`Fdc86Q` -- the exact same COM1/NEC765 hardware model
src/loader/LDBIOS86Q.A86's own test already established, since this
file targets identical QEMU hardware -- plus a small local `PicPit`
recorder for the PIT (0x40/0x43) and PIC (0x20/0x21) ports `init_entry`
also touches, which neither existing fixture models. `Device` below
just routes each port to whichever of the three actually owns it.
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

REAL_FILE = os.path.join(REPO_ROOT, 'src', 'kernel', 'XIOS86Q.A86')
XIOS86Q_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'xios86q.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

XSEG = 0x0100        # the XIOS's DS: SYSDAT (its data follows SYSDAT's own, from 1000h)
XBASE = XSEG * 16
CSEG = 0x0600        # its CS: the combined kernel code segment, not SYSDAT
CBASE = CSEG * 16
STACK_SEG, STACK_TOP = 0x0800, 0x0FF0

XQ_TICK_COUNT, XQ_CLOCKON = 0x1000, 0x1001
XQ_DISK, XQ_TRK, XQ_SECT = 0x1004, 0x1005, 0x1006
XQ_DMA_ADR, XQ_DMA_SEG = 0x1007, 0x1009
XQ_SECBUF = 0x1040
XQ_DPH0 = 0x1240


class PicPit:
    """Records PIT (0x40/0x43) and PIC (0x20/0x21) writes; PIC mask
    reads return whatever was last written (starting all-masked,
    0xFF, the real post-reset default), so init_entry's own 3
    independent read-modify-write sequences compose correctly."""

    def __init__(self):
        self.pit_writes = []
        self.pic_writes = []
        self.pic_mask = 0xFF

    def in8(self, port):
        if port == 0x21:
            return self.pic_mask
        raise ValueError(f"PicPit: unexpected IN from port 0x{port:04X}")

    def out8(self, port, value):
        value &= 0xFF
        if port in (0x40, 0x43):
            self.pit_writes.append((port, value))
        elif port == 0x21:
            self.pic_mask = value
            self.pic_writes.append((port, value))
        elif port == 0x20:
            self.pic_writes.append((port, value))
        else:
            raise ValueError(f"PicPit: unexpected OUT to port 0x{port:04X}")


class Device:
    def __init__(self, uart=None, fdc=None, picpit=None):
        self.uart = uart or Uart86Q()
        self.fdc = fdc or Fdc86Q()
        self.picpit = picpit or PicPit()

    def _route(self, port):
        if port in (0x3F8, 0x3F9, 0x3FB, 0x3FC, 0x3FD):
            return self.uart
        if port in (0x40, 0x43, 0x20, 0x21):
            return self.picpit
        return self.fdc

    def in8(self, port):
        return self._route(port).in8(port)

    def out8(self, port, value):
        self._route(port).out8(port, value)


def assemble_reference():
    asm = Assembler()
    asm.assemble(REAL_FILE, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    entries = {name: asm.symtab[name.lower()].value
               for name in ('const', 'conin', 'conout', 'plist', 'punch', 'reader', 'listst',
                             'maxconsole', 'maxlist', 'disk_home', 'seldsk', 'settrk', 'setsec',
                             'setdma', 'setdmab', 'sectran', 'diskread', 'diskwrite', 'getsegt',
                             'selmemory', 'flushbuf', 'strtclk', 'stopclk', 'polldev', 'init',
                             'entry')}
    return code, dseg, entries


def assemble_symbols():
    asm = Assembler()
    asm.assemble(REAL_FILE, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    return {k: v.value for k, v in asm.symtab.items()}


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, XIOS86Q_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def xios_image(code, dseg, mem=None):
    """the XIOS's code at CSEG and its DSEG image at XSEG (SYSDAT: the
    XIOS's own fields start at 4096); then a case's own bytes (physical
    addresses)"""
    image = bytearray(0x10000)
    image[CBASE:CBASE + len(code)] = code
    image[XBASE:XBASE + len(dseg)] = dseg
    if mem is not None:
        for addr, val in enumerate(mem):
            if val != 0:
                image[addr] = val
    return image


def run_reference(code, dseg, entry, regs, device, mem=None, stop_addrs=None, max_steps=100000):
    image = xios_image(code, dseg, mem)

    def io_in_hook(sim, port, width):
        assert width == 8
        return device.in8(port)

    def io_out_hook(sim, port, width, value):
        assert width == 8
        device.out8(port, value)

    sim = I86Sim(image, {**regs, 'cs': CSEG, 'ds': XSEG, 'es': XSEG, 'ss': STACK_SEG, 'sp': STACK_TOP},
                 CBASE + entry, segmented=True,
                 stop_addrs={CBASE + a for a in stop_addrs or ()},
                 io_in_hook=io_in_hook, io_out_hook=io_out_hook)
    sim.run(max_steps=max_steps)
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
        lib.xios_conin_entry.restype = ctypes.c_uint8
        lib.xios_conin_entry.argtypes = [IN8_FN, OUT8_FN]
        lib.xios_conout_entry.restype = None
        lib.xios_conout_entry.argtypes = [IN8_FN, OUT8_FN, ctypes.c_uint8]
        lib.punch_entry.restype = ctypes.c_uint8
        lib.punch_entry.argtypes = []
        lib.reader_entry.restype = ctypes.c_uint8
        lib.reader_entry.argtypes = []
        lib.listst_entry.restype = ctypes.c_uint8
        lib.listst_entry.argtypes = []
        lib.maxconsole_entry.restype = ctypes.c_uint16
        lib.maxconsole_entry.argtypes = []
        lib.maxlist_entry.restype = ctypes.c_uint16
        lib.maxlist_entry.argtypes = []
        lib.disk_home_entry.restype = None
        lib.disk_home_entry.argtypes = [c_u8p, ctypes.c_uint16, IN8_FN, OUT8_FN]
        lib.seldsk_entry.restype = ctypes.c_uint16
        lib.seldsk_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.settrk_entry.restype = None
        lib.settrk_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.setsec_entry.restype = None
        lib.setsec_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.setdma_entry.restype = None
        lib.setdma_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.setdmab_entry.restype = None
        lib.setdmab_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.sectran_entry.restype = ctypes.c_uint16
        lib.sectran_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
        lib.diskread_entry.restype = ctypes.c_uint8
        lib.diskread_entry.argtypes = [c_u8p, ctypes.c_uint16, IN8_FN, OUT8_FN]
        lib.diskwrite_entry.restype = ctypes.c_uint8
        lib.diskwrite_entry.argtypes = [c_u8p, ctypes.c_uint16, IN8_FN, OUT8_FN]
        lib.getsegt_entry.restype = ctypes.c_uint16
        lib.getsegt_entry.argtypes = []
        lib.flushbuf_entry.restype = ctypes.c_uint8
        lib.flushbuf_entry.argtypes = []
        lib.strtclk_entry.restype = None
        lib.strtclk_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.stopclk_entry.restype = None
        lib.stopclk_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.polldev_entry.restype = ctypes.c_uint8
        lib.polldev_entry.argtypes = [IN8_FN, ctypes.c_uint8]
        lib.init_entry.restype = None
        lib.init_entry.argtypes = [c_u8p, ctypes.c_uint16, IN8_FN, OUT8_FN]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def make_c_hooks(device):
            in8 = IN8_FN(lambda port: device.in8(port))
            out8 = OUT8_FN(lambda port, value: device.out8(port, value))
            return in8, out8

        # -- CONST: not ready / ready --
        ref_regs, _ = run_reference(code, dseg, entries['const'], {}, Device(uart=Uart86Q(rx_bytes=b'')))
        check("CONST: not ready", ref_regs['ax'] & 0xFF, lib.const_entry(make_c_hooks(Device(uart=Uart86Q(rx_bytes=b'')))[0]))
        ref_regs, _ = run_reference(code, dseg, entries['const'], {}, Device(uart=Uart86Q(rx_bytes=b'A')))
        check("CONST: ready", ref_regs['ax'] & 0xFF, lib.const_entry(make_c_hooks(Device(uart=Uart86Q(rx_bytes=b'A')))[0]))

        # -- CONIN --
        ref_regs, _ = run_reference(code, dseg, entries['conin'], {}, Device(uart=Uart86Q(rx_bytes=b'Q')))
        in8, out8 = make_c_hooks(Device(uart=Uart86Q(rx_bytes=b'Q')))
        check("CONIN", ref_regs['ax'] & 0xFF, lib.xios_conin_entry(in8, out8))

        # -- CONOUT --
        d1 = Device()
        run_reference(code, dseg, entries['conout'], {'cx': 0x58}, d1)
        d2 = Device()
        in8, out8 = make_c_hooks(d2)
        lib.xios_conout_entry(in8, out8, ctypes.c_uint8(0x58))
        check("CONOUT", d1.uart.tx, d2.uart.tx)

        # -- PUNCH/READER/LISTST --
        ref_regs, _ = run_reference(code, dseg, entries['punch'], {}, Device())
        check("PUNCH", ref_regs['ax'] & 0xFF, lib.punch_entry())
        ref_regs, _ = run_reference(code, dseg, entries['reader'], {}, Device())
        check("READER", ref_regs['ax'] & 0xFF, lib.reader_entry())
        ref_regs, _ = run_reference(code, dseg, entries['listst'], {}, Device())
        check("LISTST", ref_regs['ax'] & 0xFF, lib.listst_entry())

        # -- MAXCONSOLE/MAXLIST --
        ref_regs, _ = run_reference(code, dseg, entries['maxconsole'], {}, Device())
        check("MAXCONSOLE", ref_regs['ax'], lib.maxconsole_entry())
        ref_regs, _ = run_reference(code, dseg, entries['maxlist'], {}, Device())
        check("MAXLIST", ref_regs['ax'], lib.maxlist_entry())

        # -- GETSEGT/FLUSHBUF --
        ref_regs, _ = run_reference(code, dseg, entries['getsegt'], {}, Device())
        check("GETSEGT", ref_regs['ax'], lib.getsegt_entry())
        ref_regs, _ = run_reference(code, dseg, entries['flushbuf'], {}, Device())
        check("FLUSHBUF", ref_regs['ax'] & 0xFF, lib.flushbuf_entry())

        # -- SELDSK: disk 0 / out-of-range --
        for cl in (0, 1, 2):
            ref_regs, _ = run_reference(code, dseg, entries['seldsk'], {'cx': cl}, Device())
            port_ax = lib.seldsk_entry(to_ctypes(bytearray(0x10000)), XSEG, ctypes.c_uint8(cl))
            check(f"SELDSK cl={cl}", ref_regs['ax'], port_ax)
        assert run_reference(code, dseg, entries['seldsk'], {'cx': 0}, Device())[0]['ax'] == XQ_DPH0

        # -- SETTRK/SETSEC --
        ref_regs, ref_img = run_reference(code, dseg, entries['settrk'], {'cx': 0x2A}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.settrk_entry(buf, XSEG, ctypes.c_uint8(0x2A))
        check("SETTRK", ref_img[XBASE + XQ_TRK], bytes(buf)[XBASE + XQ_TRK])

        ref_regs, ref_img = run_reference(code, dseg, entries['setsec'], {'cx': 0x07}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.setsec_entry(buf, XSEG, ctypes.c_uint8(0x07))
        check("SETSEC", ref_img[XBASE + XQ_SECT], bytes(buf)[XBASE + XQ_SECT])

        # -- SETDMA/SETDMAB --
        ref_regs, ref_img = run_reference(code, dseg, entries['setdma'], {'cx': 0x1234}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.setdma_entry(buf, XSEG, ctypes.c_uint16(0x1234))
        check("SETDMA", ref_img[XBASE + XQ_DMA_ADR:XBASE + XQ_DMA_ADR + 2], bytes(buf)[XBASE + XQ_DMA_ADR:XBASE + XQ_DMA_ADR + 2])

        ref_regs, ref_img = run_reference(code, dseg, entries['setdmab'], {'cx': 0x5678}, Device())
        buf = to_ctypes(bytearray(0x10000))
        lib.setdmab_entry(buf, XSEG, ctypes.c_uint16(0x5678))
        check("SETDMAB", ref_img[XBASE + XQ_DMA_SEG:XBASE + XQ_DMA_SEG + 2], bytes(buf)[XBASE + XQ_DMA_SEG:XBASE + XQ_DMA_SEG + 2])

        # -- SECTRAN --
        ref_regs, _ = run_reference(code, dseg, entries['sectran'], {'cx': 9, 'dx': 0}, Device())
        check("SECTRAN: no translation", ref_regs['ax'],
              lib.sectran_entry(to_ctypes(bytearray(0x10000)), XSEG, ctypes.c_uint16(9), ctypes.c_uint16(0)))
        table_base = 0x2000
        table_data = bytes([10, 20, 30, 40])
        mem = bytearray(0x10000)
        mem[XBASE + table_base:XBASE + table_base + len(table_data)] = table_data
        ref_regs, _ = run_reference(code, dseg, entries['sectran'], {'cx': 2, 'dx': table_base}, Device(), mem=mem)
        check("SECTRAN: table lookup", ref_regs['ax'],
              lib.sectran_entry(to_ctypes(mem), XSEG, ctypes.c_uint16(2), ctypes.c_uint16(table_base)))

        # -- POLLDEV --
        for label, cl, uart in (("c0indev not ready", 0x00, Uart86Q(rx_bytes=b'')),
                                 ("c0indev ready", 0x00, Uart86Q(rx_bytes=b'x')),
                                 ("c0outdev", 0x04, Uart86Q()),
                                 ("flpy_poll_dev", 0x09, Uart86Q())):
            ref_regs, _ = run_reference(code, dseg, entries['polldev'], {'cx': cl}, Device(uart=uart))
            port_ax = lib.polldev_entry(make_c_hooks(Device(uart=uart))[0], ctypes.c_uint8(cl))
            check(f"POLLDEV {label}", ref_regs['ax'] & 0xFF, port_ax)

        # -- DISK_HOME --
        ref_fdc = Fdc86Q()
        ref_regs, ref_img = run_reference(code, dseg, entries['disk_home'], {}, Device(fdc=ref_fdc))
        c_fdc = Fdc86Q()
        in8, out8 = make_c_hooks(Device(fdc=c_fdc))
        buf = to_ctypes(bytearray(0x10000))
        lib.disk_home_entry(buf, XSEG, in8, out8)
        check("DISK_HOME: trk reset to 0", ref_img[XBASE + XQ_TRK], bytes(buf)[XBASE + XQ_TRK])
        check("DISK_HOME: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)

        # -- DISKREAD/DISKWRITE: trk=5/sect=3, the caller's buffer at
        # dma_seg:dma_adr; the 8237 gets secbuf's physical address
        # (XSEG:secbuf) --
        DMA_SEG, DMA_ADR = 0x0500, 0x0010
        for name, entry_name, c_fn in (("DISKREAD", 'diskread', lib.diskread_entry),
                                        ("DISKWRITE", 'diskwrite', lib.diskwrite_entry)):
            mem = bytearray(0x10000)
            mem[XBASE + XQ_TRK] = 5
            mem[XBASE + XQ_SECT] = 3
            mem[XBASE + XQ_DMA_ADR] = DMA_ADR & 0xFF
            mem[XBASE + XQ_DMA_ADR + 1] = DMA_ADR >> 8
            mem[XBASE + XQ_DMA_SEG] = DMA_SEG & 0xFF
            mem[XBASE + XQ_DMA_SEG + 1] = DMA_SEG >> 8
            # the sector as the 8237 left it in secbuf (DMA isn't modeled)
            for i in range(512):
                mem[XBASE + XQ_SECBUF + i] = (0x80 + i) & 0xFF
            if name == "DISKWRITE":
                phys = (DMA_SEG * 16 + DMA_ADR) & 0xFFFF
                for i in range(128):
                    mem[(phys + i) & 0xFFFF] = (i + 1) & 0xFF
            ref_fdc = Fdc86Q()
            # rw_setup's own motor_delay is a genuine 65536-iteration
            # bounded loop (see xios86q.c's own comment) -- needs a
            # higher step budget than this interpreter's own default.
            ref_regs, ref_img = run_reference(code, dseg, entries[entry_name], {}, Device(fdc=ref_fdc),
                                               mem=mem, max_steps=400000)
            c_fdc = Fdc86Q()
            in8, out8 = make_c_hooks(Device(fdc=c_fdc))
            c_mem = to_ctypes(xios_image(code, dseg, mem))
            port_al = c_fn(c_mem, XSEG, in8, out8)
            check(f"{name}: AL result", ref_regs['ax'] & 0xFF, port_al)
            check(f"{name}: FDC command bytes sent", ref_fdc.sent, c_fdc.sent)
            check(f"{name}: DMA register writes", ref_fdc.dma_writes, c_fdc.dma_writes)
            check(f"{name}: no other bytes differ", [],
                  [hex(i) for i in range(0x10000) if not CBASE <= i < CBASE + len(code)
                   and not STACK_SEG * 16 <= i < STACK_SEG * 16 + STACK_TOP and ref_img[i] != c_mem[i]][:8])
            if name == "DISKREAD":
                phys = (DMA_SEG * 16 + DMA_ADR) & 0xFFFF
                ref_bytes = bytes(ref_img[(phys + i) & 0xFFFF] for i in range(128))
                port_bytes = bytes(c_mem[(phys + i) & 0xFFFF] for i in range(128))
                check(f"{name}: 128-byte copy", ref_bytes, port_bytes)

        # -- STRTCLK/STOPCLK --
        mem = bytearray(0x10000)
        ref_regs, ref_img = run_reference(code, dseg, entries['strtclk'], {}, Device(), mem=mem)
        buf = to_ctypes(bytearray(0x10000))
        lib.strtclk_entry(buf, XSEG)
        check("STRTCLK", ref_img[XBASE + XQ_CLOCKON], bytes(buf)[XBASE + XQ_CLOCKON])

        mem = bytearray(0x10000)
        mem[XBASE + XQ_CLOCKON] = 0xff
        ref_regs, ref_img = run_reference(code, dseg, entries['stopclk'], {}, Device(), mem=mem)
        buf = to_ctypes(mem)
        lib.stopclk_entry(buf, XSEG)
        check("STOPCLK", ref_img[XBASE + XQ_CLOCKON], bytes(buf)[XBASE + XQ_CLOCKON])

        # -- INIT: UART/PIT/PIC/FDC arming + signon, stop right at
        # the far-pointer-copy section this port doesn't model (the
        # real `init:`'s own first instruction after the two `mov
        # cs:[si],ax`-style copies -- found the same way other tests
        # in this project locate an untested boundary: scanning
        # forward from `init` for the next real label, `entry`, and
        # using the byte right after the copies as the stop point).
        # Not reachable here anyway: this port's own `init_entry`
        # starts straight at the UART setup, same as the real file
        # does right after those copies.
        init_start = entries['init']
        # Run the reference FROM the first UART-setup instruction
        # ("mov al,80h"), skipping only the far-pointer copies this
        # port doesn't model (see xios86q.c's own header comment).
        # Found by scanning for the first `mov al,80h` encoding (the
        # 3-byte `C6 C0 80` form this assembler emits here) after
        # `init_start`, confirmed unique and immediately followed by
        # `mov dx,uart_lcr ! out dx,al` by disassembly, not guessed.
        candidates = [a for a in range(init_start, init_start + 120)
                      if code[a:a + 3] == b'\xc6\xc0\x80']
        assert len(candidates) == 1, f"expected exactly 1 'mov al,80h', found {candidates}"
        uart_setup_start = candidates[0]

        # signon's own real, null-terminated string content, already
        # present in `dseg` (its own static data) -- read directly so
        # the port's own copy (which has no equivalent preloaded
        # image) gets the exact same bytes, not just a truncated stand-in.
        XQ_SIGNON = 0x134A
        signon_end = XQ_SIGNON
        while dseg[signon_end] != 0:
            signon_end += 1
        signon_bytes = bytes(dseg[XQ_SIGNON:signon_end + 1])

        ref_device = Device()
        # stop right at the real `retf` (unimplemented in
        # tools/i86sim.py, same as every other real OS-exit point in
        # this project) -- one byte before `entry`, confirmed by
        # disassembly (opcode 0xCB right there), not guessed.
        ref_regs, ref_img = run_reference(code, dseg, uart_setup_start, {}, ref_device,
                                           stop_addrs={entries['entry'] - 1})
        c_device = Device()
        c_mem = bytearray(0x10000)
        c_mem[XBASE + XQ_SIGNON:XBASE + XQ_SIGNON + len(signon_bytes)] = signon_bytes
        c_buf = to_ctypes(c_mem)
        in8, out8 = make_c_hooks(c_device)
        lib.init_entry(c_buf, XSEG, in8, out8)
        check("INIT: UART setup writes", ref_device.uart.setup_writes, c_device.uart.setup_writes)
        check("INIT: PIT writes", ref_device.picpit.pit_writes, c_device.picpit.pit_writes)
        check("INIT: PIC writes", ref_device.picpit.pic_writes, c_device.picpit.pic_writes)
        check("INIT: FDC specify command sent", ref_device.fdc.sent, c_device.fdc.sent)
        check("INIT: signon printed", ref_device.uart.tx, c_device.uart.tx)

        # -- TICKINT_ONCE: the whole real ISR, interrupting code whose
        # DS/SS are neither the XIOS's CS nor SYSDAT, from `tickint` to
        # its final `jmpf cs:dword ptr mydispatch` (the dispatcher tail
        # tickint_once doesn't model). Each `call supif` (f_flagset) is a
        # boundary, counted. Besides tick_count/clockon, the ISR may only
        # write its CS save cells (code), its own stack in SYSDAT and the
        # interrupted stack -- anything else is a stray write. --
        lib.tickint_once.argtypes = [c_u8p, ctypes.c_uint16]

        class TickResult(ctypes.Structure):
            _fields_ = [('sec_flag_set', ctypes.c_uint8), ('tick_flag_set', ctypes.c_uint8)]

        lib.tickint_once.restype = TickResult
        xsym = lambda name: assemble_symbols()[name]  # noqa: E731
        TICKINT, SUPIF, TOS = xsym('tickint'), xsym('supif'), xsym('tickint_tos')
        JMPF = code.index(b'\x2e\xff\x2e', xsym('tick_2'))   # jmpf cs:dword ptr mydispatch
        USER_DS, USER_SS, USER_SP = 0x0A00, 0x0C00, 0x0100
        isr_stack = set(range(XBASE + TOS - 64, XBASE + TOS))
        # the interrupted stack: i86sim's own start word, then the ISR's push ds (and its push cs ! pop ds)
        user_stack = set(range(USER_SS * 16 + USER_SP - 8, USER_SS * 16 + USER_SP))

        def run_tickint_once(mem):
            image = xios_image(code, dseg, mem)
            supif_calls = []
            sim = I86Sim(image, {'cs': CSEG, 'ds': USER_DS, 'es': USER_DS, 'ss': USER_SS, 'sp': USER_SP,
                                 'ax': 0x1234}, CBASE + TICKINT, segmented=True, stop_addrs={CBASE + JMPF},
                         call_stub_addrs={CBASE + SUPIF}, call_hooks={CBASE + SUPIF: lambda sim: supif_calls.append(1)},
                         io_out_hook=lambda sim, port, width, value: None)
            w16 = lambda a, v: (image.__setitem__(a, v & 0xFF), image.__setitem__(a + 1, v >> 8))  # noqa: E731
            w16(CBASE + xsym('sysdat'), XSEG)       # init's `mov cs:sysdat,ds`
            sim.run()
            regs = (sim.regs['ds'], sim.regs['ss'], sim.regs['sp'], sim.regs['ax'])
            return image, len(supif_calls), regs

        for label, tick_count, clockon in (
                ("mid-countdown, clock off", 30, 0x00),
                ("rolls over to a new second, clock off", 1, 0x00),
                ("mid-countdown, clock on (tick flag too)", 30, 0xff),
                ("rolls over AND clock on (both flags)", 1, 0xff)):
            mem = bytearray(0x10000)
            mem[XBASE + XQ_TICK_COUNT] = tick_count
            mem[XBASE + XQ_CLOCKON] = clockon
            ref_img, ref_calls, ref_regs = run_tickint_once(mem)
            c_mem = xios_image(code, dseg, mem)
            buf = to_ctypes(c_mem)
            r = lib.tickint_once(buf, XSEG)
            port_calls = int(r.sec_flag_set) + int(r.tick_flag_set)
            check(f"TICKINT_ONCE {label}", (ref_img[XBASE + XQ_TICK_COUNT], ref_img[XBASE + XQ_CLOCKON], ref_calls),
                  (bytes(buf)[XBASE + XQ_TICK_COUNT], bytes(buf)[XBASE + XQ_CLOCKON], port_calls))
            check(f"TICKINT_ONCE {label}: interrupted DS/SS/SP/AX restored", (USER_DS, USER_SS, USER_SP - 2, 0x1234),
                  ref_regs)
            check(f"TICKINT_ONCE {label}: no stray writes", [],
                  [hex(i) for i in range(0x10000) if not CBASE <= i < CBASE + len(code) and i not in isr_stack
                   and i not in user_stack and ref_img[i] != bytes(buf)[i]][:8])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
