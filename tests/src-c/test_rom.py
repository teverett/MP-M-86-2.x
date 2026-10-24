#!/usr/bin/env python3
"""Differential test for src-c/loader/rom.c against the whole of
src/loader/ROM.A86: the real SBC86/12 + iSBC 204 boot ROM. See rom.c's
own header comment for exactly what's not ported (console I/O and its
only caller, the fatal-error retry tail; the final far jump) and why.

The reference run starts right after `cld` (address 0x25, the real
boundary between the file's own very first block and everything
after it -- there's no label there, so this is found by single-
stepping once and recording where it lands, not guessed), skipping
only that first block: an EPROM-to-RAM copy (`rep movs`) of the FDC
command tables this routine sends, which `rom.c` doesn't model either
(it uses the already-copied VALUES directly as C constants, see that
file's own header comment), so there's nothing to lose by skipping
it. There's also a real reason to: `mov SI,drombegin` (`drombegin equ
offset $`, used bare, no `offset` keyword at the use site) assembles
here as a direct MEMORY READ (`8B 36 <addr>`) instead of an immediate
load (`BE <addr>`) -- confirmed by single-stepping tools/i86sim.py and
watching SI stay 0 past that instruction -- contradicting this
project's own documented EQU semantics elsewhere (e.g. `init_wrapper
.A86`'s own comment: "an EQU would wrongly assemble a read of the
field... as an immediate load instead of a memory read", i.e. EQU is
supposed to always be the immediate side of that question). Possibly
a genuine, previously-uncaught tools/asm86 bug for a bare address-
valued EQU used without `offset` -- investigated (a targeted fix in
`is_addr`'s own computation, gating it on a real label symbol rather
than `v.is_offset` alone, cleanly fixes `mov SI,drombegin`/`mov
bx,sector1` below) but NOT kept: it also flips `LBDOSENT.A86`'s own
`lea sp,endstack` (a bare "offset $"-rooted EQU used as LEA's address
operand, which genuinely needs the mem-shaped operand `is_addr`
provides, unlike a plain `MOV reg,EQU`) from a 'mem' to an 'imm'
operand, which `isa.py`'s own LEA validation then rejects outright --
confirmed by re-running `make` in `src/` after the change. Properly
distinguishing "bare EQU address constant, MOV wants it as an
immediate" from "bare EQU address constant, LEA wants it as a memory-
shaped operand" needs LEA's own operand parsing to force that shape
regardless of `is_addr`, not a one-line change to `is_addr` itself --
out of scope here, so this port doesn't need the copy it breaks;
starting right after it, with the command tables pre-seeded directly,
and the one later `mov bx,sector1` (same bug, same workaround) patched
up by hand mid-run, sidesteps it without depending on a fix. (The
UART/8253/8259 init right after `cld` is real, assembled-in code --
see rom.c's own header comment -- so it's included here, unlike the
copy.)

Uses its own minimal synthetic i8271/8257 device (same shape as
tests/src-c/test_ldbios.py's own `Device`): `fdc_stat` always reports
"not busy, not full, interrupt-request set" so every poll succeeds
immediately.

The actual DMA byte transfer isn't an instruction `tools/i86sim.py`
can trap (unlike `INT 13h`, which `rom86q.c`'s own test hooks via
`int_hooks`) -- so, for the one 5-byte CMD header this routine's own
control flow genuinely reads back (Ty/Len/Abs, at the real `sector1`
RAM address), the reference run just pre-seeds that address directly
before `sim.run()` (same observable effect as "the DMA already
happened," since nothing else ever touches those bytes), while the
port side gets the identical bytes through its own `disk_read`
callback.
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

REAL_FILE = os.path.join(REPO_ROOT, 'src', 'loader', 'ROM.A86')
ROM_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'rom.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0
FDC_STAT = 0xA0
SECTOR1 = 0x032A


class Device:
    def __init__(self):
        self.sent = []
        self.dma_writes = []

    def in8(self, port):
        if port == FDC_STAT:
            return 0x08  # not busy, not full, interrupt-request set
        if port == 0xA1:  # fdc_rslt (shares its port number with fdc_parm)
            return 0x00
        raise ValueError(f"Device: unexpected IN from port 0x{port:02X}")

    def out8(self, port, value):
        value &= 0xFF
        if port in (0xA0, 0xA1):
            self.sent.append(value)
        elif port in (0xA4, 0xA5, 0xA8, 0xAA):
            self.dma_writes.append((port, value))
        elif port in (0xA2, 0xAF, 0xC0, 0xC2, 0xDA, 0xD4, 0xD6):
            pass  # FDC/204 reset, 8259 PIC, 8251 UART, 8253 timer init -- not independently verified here
        else:
            raise ValueError(f"Device: unexpected OUT to port 0x{port:02X}")


def assemble_reference():
    asm = Assembler()
    asm.assemble(REAL_FILE, [])
    code = asm.segment_bytes('cseg')
    entry = 0x25  # right after `cld`, the real end of the EPROM-to-RAM copy block
    # The RAM-resident command tables the EPROM-to-RAM copy (skipped,
    # see this file's own header comment) would otherwise have
    # populated -- each (destination, source) pair, read directly
    # from the EPROM's own cseg-resident copy so the values are
    # exactly what the real copy would have produced, not retyped.
    tables = [(asm.symtab[dst].value, asm.symtab[src].value, length)
              for dst, src, length in (
                  ('read0', 'creadstring', 4), ('read1', 'creadtrk0', 5),
                  ('read2', 'creadtrk1', 5), ('home', 'chome0', 3),
                  ('specs1', 'cspecs1', 6), ('specs2', 'cspecs2', 6),
                  ('specs3', 'cspecs3', 6))]
    # the one `jmpf dword ptr leap_offset` -- scan for `FF /5` (JMP FAR
    # r/m), same technique as test_rom86q.py's own far-jmp scan. Unlike
    # that file, this scan isn't instruction-aligned, so it also finds
    # one false positive, byte-coincidental match inside send_com's
    # own encoding (confirmed by disassembling both hits by hand: only
    # the lower address decodes to a real `FF 2E <leap_offset>`
    # instruction immediately after `mov leap_offset,0`'s own 6 bytes
    # -- the real control flow's own straight-line order, with no
    # backward jump that could ever reach the higher, spurious one).
    far_jmps = [addr for addr in range(len(code) - 1)
                if code[addr] == 0xFF and ((code[addr + 1] >> 3) & 7) == 5]
    assert len(far_jmps) == 2, f"expected 2 far-jmp byte matches (1 real + 1 false positive), found {far_jmps}"
    return code, entry, min(far_jmps), tables


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, ROM_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def run_reference(code, entry, stop_addr, device, header, tables):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    image[SECTOR1:SECTOR1 + 5] = header
    for dst, src, length in tables:
        image[dst:dst + length] = code[src:src + length]

    def io_in_hook(sim, port, width):
        assert width == 8
        return device.in8(port)

    def io_out_hook(sim, port, width, value):
        assert width == 8
        device.out8(port, value)

    # `mov bx,sector1` (the same bare-"offset $"-rooted-EQU encoding
    # quirk as `drombegin`, see this file's own header comment) is a
    # direct memory read at runtime, not the immediate load it's
    # obviously meant to be -- stop right there, patch bx by hand,
    # and skip past its 4-byte encoding (confirmed via `--symbols`/
    # direct disassembly, not guessed) before resuming.
    sector1_mov_addr = 0x84
    sim = I86Sim(image, {'sp': STACK_TOP}, entry, stop_addrs={sector1_mov_addr},
                 io_in_hook=io_in_hook, io_out_hook=io_out_hook)
    sim.run()
    assert sim.ip == sector1_mov_addr
    sim.regs['bx'] = SECTOR1
    sim.ip = sector1_mov_addr + 4

    sim.stop_addrs = {stop_addr}
    sim.halted = False
    sim.run()
    return sim.regs


def main():
    code, entry, far_jmp_addr, tables = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        IN8_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.c_uint16)
        OUT8_FN = ctypes.CFUNCTYPE(None, ctypes.c_uint16, ctypes.c_uint8)
        DISK_READ_FN = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_uint8))
        lib.start_entry.restype = None
        lib.start_entry.argtypes = [IN8_FN, OUT8_FN, DISK_READ_FN, ctypes.POINTER(ctypes.c_uint16)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # Ty=1 (arbitrary), Len=0x1234 (body length, unused by this
        # routine's own observable behavior), Abs=0x2000 (load segment)
        header = bytes([1, 0x34, 0x12, 0x00, 0x20])

        ref_device = Device()
        ref_regs = run_reference(code, entry, far_jmp_addr, ref_device, header, tables)

        c_device = Device()
        in8 = IN8_FN(lambda port: c_device.in8(port))
        out8 = OUT8_FN(lambda port, value: c_device.out8(port, value))

        def disk_read(buf_ptr):
            for i, b in enumerate(header):
                buf_ptr[i] = b

        disk_read_cb = DISK_READ_FN(disk_read)
        entry_seg = ctypes.c_uint16(0)
        lib.start_entry(in8, out8, disk_read_cb, ctypes.byref(entry_seg))

        check("entry segment (leap_segment)", ref_regs['es'], entry_seg.value)
        check("FDC command bytes sent", ref_device.sent, c_device.sent)
        check("DMA register writes", ref_device.dma_writes, c_device.dma_writes)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
