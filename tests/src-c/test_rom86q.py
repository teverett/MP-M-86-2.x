#!/usr/bin/env python3
"""Differential test for src-c/loader/rom86q.c against the whole of
src/loader/ROM86Q.A86: `start:` (ported as `start_entry`), plus its
internal subroutines `advance_chs`, `read_sector`, `pmsg`, `putc`.

Drives tests/src-c/fixtures/uart86q.py's `Uart86Q` (console output)
and bootdisk.py's `BootDisk` (the synthetic floppy image behind
`INT 13h`) from both sides: tools/i86sim.py's `io_in_hook`/
`io_out_hook`/`int_hooks` for the reference run, and ctypes callbacks
of the same shape for the C port.
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
from bootdisk import BootDisk  # noqa: E402

REAL_FILE = os.path.join(REPO_ROOT, 'src', 'loader', 'ROM86Q.A86')
ROM86Q_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'rom86q.c')

STACK_TOP = 0x7C00
BOOTDRV = 0x00


def assemble_reference():
    asm = Assembler()
    asm.assemble(REAL_FILE, [])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['start'].value
    jump_in = asm.symtab['jump_in'].value
    hang = asm.symtab['hang'].value

    # the one `jmpf dword ptr leap_off` instruction -- scan for `0xFF`
    # with a ModRM reg field of 5 (JMP FAR r/m) between jump_in and
    # hang, same technique as tests/src-c/test_init.py's own scan for
    # CALL FAR sites.
    far_jmps = []
    for addr in range(jump_in, hang):  # code[] is indexed by absolute address directly
        if code[addr] == 0xFF and ((code[addr + 1] >> 3) & 7) == 5:
            far_jmps.append(addr)
    assert len(far_jmps) == 1, f"expected 1 far jmp site, found {far_jmps}"

    return code, entry, hang, far_jmps[0]


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         ROM86Q_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, hang_addr, far_jmp_addr, uart, disk):
    image = bytearray(0x10000)
    image[0:len(code)] = code

    def io_in_hook(sim, port, width):
        assert width == 8
        return uart.in8(port)

    def io_out_hook(sim, port, width, value):
        assert width == 8
        uart.out8(port, value)

    def int13_hook(sim):
        ah = (sim.regs['ax'] >> 8) & 0xFF
        al = sim.regs['ax'] & 0xFF
        ch = (sim.regs['cx'] >> 8) & 0xFF
        cl = sim.regs['cx'] & 0xFF
        dh = (sim.regs['dx'] >> 8) & 0xFF
        dl = sim.regs['dx'] & 0xFF
        cf, data = disk.read_chs(ah, al, ch, cl, dh, dl)
        bx = sim.regs['bx']
        for i, b in enumerate(data):
            sim.mem[(bx + i) & 0xFFFF] = b
        sim.cf = cf

    sim = I86Sim(image, {'dl': 0, 'dx': BOOTDRV, 'sp': STACK_TOP}, entry,
                 stop_addrs={hang_addr}, far_jmp_stub_addrs={far_jmp_addr},
                 io_in_hook=io_in_hook, io_out_hook=io_out_hook,
                 int_hooks={0x13: int13_hook})
    sim.run()
    return sim.regs, sim.halted


def main():
    code, entry, hang_addr, far_jmp_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)

        IN8_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.c_uint16)
        OUT8_FN = ctypes.CFUNCTYPE(None, ctypes.c_uint16, ctypes.c_uint8)
        INT13_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
                                     ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
                                     ctypes.c_uint8, ctypes.POINTER(ctypes.c_uint8))

        lib.start_entry.restype = ctypes.c_int
        lib.start_entry.argtypes = [ctypes.c_uint8, INT13_FN, IN8_FN, OUT8_FN,
                                     ctypes.POINTER(ctypes.c_uint16)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def make_c_int13(disk):
            def cb(ah, al, ch, cl, dh, dl, buf_ptr):
                cf, data = disk.read_chs(ah, al, ch, cl, dh, dl)
                ctypes.memmove(buf_ptr, bytes(data), 512)
                return cf
            return INT13_FN(cb)

        def header_sector(body_len, abs_seg):
            data = bytearray(512)
            data[0] = 1  # Ty (unchecked by this routine)
            data[1:3] = body_len.to_bytes(2, 'little')
            data[3:5] = abs_seg.to_bytes(2, 'little')
            return bytes(data)

        # -- successful boot: 1-sector body --
        ref_disk = BootDisk()
        ref_disk.set_sector(0, 0, 2, header_sector(100, 0x1000))
        ref_disk.set_sector(0, 0, 3, bytes(range(256)) * 2)
        ref_uart = Uart86Q()
        ref_regs, ref_halted = run_reference(code, entry, hang_addr, far_jmp_addr, ref_uart, ref_disk)

        c_disk = BootDisk()
        c_disk.set_sector(0, 0, 2, header_sector(100, 0x1000))
        c_disk.set_sector(0, 0, 3, bytes(range(256)) * 2)
        c_uart = Uart86Q()
        in8 = IN8_FN(lambda port: c_uart.in8(port))
        out8 = OUT8_FN(lambda port, value: c_uart.out8(port, value))
        int13 = make_c_int13(c_disk)
        entry_seg = ctypes.c_uint16(0)
        rc = lib.start_entry(ctypes.c_uint8(BOOTDRV), int13, in8, out8, ctypes.byref(entry_seg))

        check("1-sector body: halted/returns ok", True, ref_halted and rc == 0)
        check("1-sector body: entry_seg", 0x1000, entry_seg.value)
        check("1-sector body: INT13 call sequence", ref_disk.calls, c_disk.calls)
        check("1-sector body: console output", ref_uart.tx, c_uart.tx)

        # -- successful boot: 3-sector body (crosses a track boundary's
        # worth of advance_chs calls, same geometry both sides) --
        ref_disk = BootDisk()
        ref_disk.set_sector(0, 0, 2, header_sector(1200, 0x2000))
        for i, (c, h, s) in enumerate([(0, 0, 3), (0, 0, 4), (0, 0, 5)]):
            ref_disk.set_sector(c, h, s, bytes([i]) * 512)
        ref_uart = Uart86Q()
        ref_regs, ref_halted = run_reference(code, entry, hang_addr, far_jmp_addr, ref_uart, ref_disk)

        c_disk = BootDisk()
        c_disk.set_sector(0, 0, 2, header_sector(1200, 0x2000))
        for i, (c, h, s) in enumerate([(0, 0, 3), (0, 0, 4), (0, 0, 5)]):
            c_disk.set_sector(c, h, s, bytes([i]) * 512)
        c_uart = Uart86Q()
        in8 = IN8_FN(lambda port: c_uart.in8(port))
        out8 = OUT8_FN(lambda port, value: c_uart.out8(port, value))
        int13 = make_c_int13(c_disk)
        entry_seg = ctypes.c_uint16(0)
        rc = lib.start_entry(ctypes.c_uint8(BOOTDRV), int13, in8, out8, ctypes.byref(entry_seg))

        check("3-sector body: halted/returns ok", True, ref_halted and rc == 0)
        check("3-sector body: entry_seg", 0x2000, entry_seg.value)
        check("3-sector body: INT13 call sequence", ref_disk.calls, c_disk.calls)
        check("3-sector body: console output (3 dots)", ref_uart.tx, c_uart.tx)

        # -- header read fails every attempt -- boot failed, no entry_seg --
        ref_disk = BootDisk()
        ref_disk.fail_at.add((0, 0, 2))
        ref_uart = Uart86Q()
        ref_regs, ref_halted = run_reference(code, entry, hang_addr, far_jmp_addr, ref_uart, ref_disk)

        c_disk = BootDisk()
        c_disk.fail_at.add((0, 0, 2))
        c_uart = Uart86Q()
        in8 = IN8_FN(lambda port: c_uart.in8(port))
        out8 = OUT8_FN(lambda port, value: c_uart.out8(port, value))
        int13 = make_c_int13(c_disk)
        entry_seg = ctypes.c_uint16(0xBEEF)
        rc = lib.start_entry(ctypes.c_uint8(BOOTDRV), int13, in8, out8, ctypes.byref(entry_seg))

        check("header read failure: halted/returns fail", True, ref_halted and rc == 1)
        check("header read failure: INT13 retries (4 reads + 3 resets)",
              len(ref_disk.calls), len(c_disk.calls))
        check("header read failure: INT13 call sequence", ref_disk.calls, c_disk.calls)
        check("header read failure: console output", ref_uart.tx, c_uart.tx)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
