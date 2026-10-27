#!/usr/bin/env python3
"""Differential test for src-c/loader/ldmpm.c against src-assem/loader/
LDMPM.A86's `start:` (ported as `start_entry`), the loader's own main
orchestration routine.

`jmp mldbios` -- a near jump into src-assem/loader/LDBIOS86Q.A86's own
`INIT`, whose own tail jumps straight back -- is modeled via
tools/i86sim.py's new `jmp_hooks` on the reference side, replaying
the exact same UART/FDC register-write sequence
tests/src-c/test_ldbios86q.py's own INIT case already verifies
(this test doesn't re-verify INIT itself, just that the call site
wires up correctly); the C port instead makes a REAL extern call into
src-c/loader/ldbios86q.c's own `init_entry` -- both .c files are
compiled into one shared library for this test, the first such
multi-file link in this port.

Every other real effect in LDMPM.A86 goes through its own synthetic
`int 224`/`sys_vec` (NOT the real MP/M-86 kernel's `int mpmint`, same
number but a completely different, loader-local convention -- see
ldmpm.c's own header comment) -- modeled via `tools/i86sim.py`'s
`int_hooks` on the reference side and three separate C callback
parameters (`bdos_fn`/`msg_fn`/`read_fn`) on the port side, backed by
tests/src-c/fixtures/mpmsysfile.py's `MpmSysFile` (the synthetic
MPM.SYS file content) on both.
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
from iorecorder import IoRecorder  # noqa: E402
from mpmsysfile import MpmSysFile  # noqa: E402

REAL_FILE = os.path.join(REPO_ROOT, 'src-assem', 'loader', 'LDMPM.A86')
LDMPM_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'ldmpm.c')
LDBIOS86Q_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'ldbios86q.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0
CS = 0x0000  # this module's own real code segment -- ambient/unconstrained,
             # see ldmpm.c's own header comment; 0 keeps every bare/DS-
             # relative access in this file flat-equivalent for the test

COUTF, PSTRF, SELDSK, OPENF, READSF, DMAF, DMABF = 2, 9, 14, 15, 20, 26, 51


def assemble_reference():
    asm = Assembler()
    asm.assemble(REAL_FILE, [])
    code = asm.segment_bytes('cseg')
    entry = asm.symtab['start'].value
    mldbios = asm.symtab['mldbios'].value
    stop2 = asm.symtab['stop2'].value

    # the one `jmpf cs:mpm` instruction (the success path's own real
    # exit) -- scan for `0xFF` with a ModRM reg field of 5 (JMP FAR
    # r/m) between gompm and stop, same technique as
    # tests/src-c/test_rom86q.py's own scan. Unlike ROM86Q.A86's own
    # `jmpf dword ptr leap_off`, this one needs a `cs:` segment-
    # override PREFIX byte (0x2E, see ldmpm.c's own header comment
    # on why) -- `far_jmp_stub_addrs` is keyed by the instruction's
    # own start address, which `step()` captures BEFORE consuming
    # the prefix, so the registered address is the PREFIX byte's own
    # position, one earlier than the `0xFF` opcode itself.
    gompm = asm.symtab['gompm'].value
    stop = asm.symtab['stop'].value
    far_jmps = [addr - 1 for addr in range(gompm, stop)
                if code[addr] == 0xFF and ((code[addr + 1] >> 3) & 7) == 5
                and code[addr - 1] == 0x2E]
    assert len(far_jmps) == 1, f"expected 1 far jmp site, found {far_jmps}"

    return code, entry, mldbios, stop2, far_jmps[0]


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, LDMPM_C, LDBIOS86Q_C, '-o', lib_path],
        check=True,
    )
    return lib_path


class BdosLog:
    def __init__(self):
        self.msgs = []
        self.console = []
        self.calls = []


# This module's own real `start:` prologue, right after the `jmp
# mldbios` this hook stands in for (`xor ax,ax ! mov ds,ax ! mov
# bdos_offset,offset ldbdos_offset ! mov bdos_segment,cs ! mov ax,cs
# ! mov ss,ax ! mov ds,ax ! mov es,ax ! mov sp,offset(stack)`) is
# ENTIRELY skipped by jumping straight to 0x11D (`mov dx,offset
# signon`, confirmed by single-stepping the real assembled bytes) --
# not just the stack/segment switch (no C equivalent, same as every
# other real OS entry point in this port), but ALSO the two
# `bdos_offset`/`bdos_segment` writes themselves: those are a bare,
# unprefixed write to an ABSOLUTE address (0x380/0x382) meant for a
# genuinely different, real `dseg 0`/`org 0` segment (the real
# interrupt vector table) that happens to collide with THIS module's
# OWN cseg-resident data (`lenmsg` et al, nearby in this same flat
# model) purely because this test's own ambient-flat convention
# never segments dseg away from cseg the way real two-segment
# hardware would -- confirmed by watching it actually happen
# (`lenmsg`'s own string got clobbered) before adding this skip.
# ldmpm.c's own header comment covers why neither write is modeled.
START_SIGNON = 0x11D  # mov dx,offset signon (right after "mldbios returns here: 103H")


def make_mldbios_jmp_hook(io_recorder):
    def hook(sim):
        # replays the exact byte sequence src-c/loader/ldbios86q.c's
        # own init_entry (and the real LDBIOS86Q.A86 INIT it ports)
        # produces -- see this file's own header comment
        io_recorder.out8(0x3FB, 0x80)
        io_recorder.out8(0x3F8, 0x0C)
        io_recorder.out8(0x3F9, 0x00)
        io_recorder.out8(0x3FB, 0x03)
        io_recorder.out8(0x3F9, 0x00)
        io_recorder.out8(0x3FC, 0x03)
        io_recorder.out8(0x3F2, 0x04 | 0x10 | 0x08 | 0x00)
        for b in (0x03, 0xDF, 0x02):
            io_recorder.out8(0x3F5, b)
        sim.ip = START_SIGNON  # skip the prologue -- see module comment above
    return hook


def make_bdos_int_hook(mpmfile, log, open_fail, dma_state):
    def hook(sim):
        cl = sim.regs['cx'] & 0xFF
        dx = sim.regs['dx']
        if cl == PSTRF:
            chars = []
            addr = dx
            while True:
                b = sim.mem[addr & 0xFFFF]
                if b == 0x24:  # '$'
                    break
                chars.append(b)
                addr = (addr + 1) & 0xFFFF
            log.msgs.append(bytes(chars).decode('ascii'))
        elif cl == SELDSK:
            log.calls.append(('seldsk', dx))
        elif cl == OPENF:
            log.calls.append(('openf', dx))
            al = 0xFF if open_fail else 0x00
            sim.regs['ax'] = (sim.regs['ax'] & 0xFF00) | al
        elif cl == DMAF:
            dma_state['offset'] = dx
            log.calls.append(('dmaf', dx))
        elif cl == DMABF:
            dma_state['seg'] = dx
            log.calls.append(('dmabf', dx))
        elif cl == COUTF:
            log.console.append(dx & 0xFF)
        elif cl == READSF:
            al, rec = mpmfile.read_next()
            dma_state['read_count'] = dma_state.get('read_count', 0) + 1
            if dma_state['read_count'] == 1:
                # only the very first readsf call (the CMD header,
                # read via `perr`'s own "mov dx,cs ! call setdmab" /
                # "mov dx,offset sec1 ! call setdma") is written back
                # to a real, SAFE flat address (sec1, inside this
                # module's own small data area) -- every later body
                # read's own real target is `dmab:dx`, a genuine
                # segment:offset pair this flat 64KB model can't
                # represent faithfully for an arbitrary load segment
                # (e.g. cldseg=0x3000 means a real address of
                # 0x30000+, wrapping mod 0x10000 right back over this
                # module's own low-memory code) -- same "DMA transfer
                # itself not modeled, nothing reads it back" scope
                # src-c/loader/ldbios86q.c's own header comment uses
                # for READ/WRITE, just applied by skipping the write
                # entirely rather than via mem_r8_seg/mem_w8_seg
                addr = dma_state['offset']
                for i, b in enumerate(rec):
                    sim.mem[(addr + i) & 0xFFFF] = b
            sim.regs['ax'] = (sim.regs['ax'] & 0xFF00) | al
            log.calls.append(('readsf', al))
        else:
            raise ValueError(f"unexpected bdos cl={cl}")
    return hook


def run_reference(code, entry, mldbios, stop2, far_jmp_addr, mpmfile, open_fail):
    image = bytearray(0x10000)
    image[0:len(code)] = code

    io_recorder = IoRecorder()
    log = BdosLog()
    dma_state = {'offset': 0, 'seg': 0}

    sim = I86Sim(image, {'cs': CS, 'sp': STACK_TOP}, entry,
                 stop_addrs={stop2}, far_jmp_stub_addrs={far_jmp_addr},
                 jmp_hooks={mldbios: make_mldbios_jmp_hook(io_recorder)},
                 int_hooks={224: make_bdos_int_hook(mpmfile, log, open_fail, dma_state)})
    sim.run()
    return sim.regs, sim.halted, io_recorder, log


def header_record(ctype, clen, cldseg, dtype, dlen, dldseg):
    rec = bytearray(128)
    rec[0] = ctype
    rec[1:3] = clen.to_bytes(2, 'little')
    rec[3:5] = cldseg.to_bytes(2, 'little')
    rec[9] = dtype
    rec[10:12] = dlen.to_bytes(2, 'little')
    rec[12:14] = dldseg.to_bytes(2, 'little')
    return bytes(rec)


def main():
    code, entry, mldbios, stop2, far_jmp_addr = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)

        IN8_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.c_uint16)
        OUT8_FN = ctypes.CFUNCTYPE(None, ctypes.c_uint16, ctypes.c_uint8)
        BDOS_FN = ctypes.CFUNCTYPE(ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint16)
        MSG_FN = ctypes.CFUNCTYPE(None, ctypes.c_char_p)
        READ_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.POINTER(ctypes.c_uint8))

        lib.start_entry.restype = ctypes.c_int
        lib.start_entry.argtypes = [ctypes.c_uint16, IN8_FN, OUT8_FN, BDOS_FN, MSG_FN, READ_FN,
                                     ctypes.POINTER(ctypes.c_uint16)]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def make_c_callbacks(mpmfile, log, open_fail):
            dma_state = {'offset': 0, 'seg': 0}

            def bdos_cb(cl, dx):
                if cl == SELDSK:
                    log.calls.append(('seldsk', dx))
                elif cl == OPENF:
                    log.calls.append(('openf', dx))
                    return 0xFF if open_fail else 0x00
                elif cl == DMAF:
                    dma_state['offset'] = dx
                    log.calls.append(('dmaf', dx))
                elif cl == DMABF:
                    dma_state['seg'] = dx
                    log.calls.append(('dmabf', dx))
                elif cl == COUTF:
                    log.console.append(dx & 0xFF)
                else:
                    raise ValueError(f"unexpected bdos cl={cl}")
                return 0

            def msg_cb(s):
                log.msgs.append(s.decode('ascii'))

            def read_cb(buf_ptr):
                al, rec = mpmfile.read_next()
                ctypes.memmove(buf_ptr, bytes(rec), 128)
                log.calls.append(('readsf', al))
                return al

            io_recorder = IoRecorder()
            in8 = IN8_FN(lambda port: io_recorder.in8(port))
            out8 = OUT8_FN(lambda port, value: io_recorder.out8(port, value))
            bdos = BDOS_FN(bdos_cb)
            msg = MSG_FN(msg_cb)
            readf = READ_FN(read_cb)
            return io_recorder, in8, out8, bdos, msg, readf

        # -- successful boot: 1-record body (1 code "paragraph", 1
        # data "paragraph", so clen=1,dlen=1 exactly covers it) --
        hdr = header_record(1, 1, 0x3000, 2, 1, 0x3001)
        ref_file = MpmSysFile([hdr, bytes([0xAA]) * 128])
        ref_regs, ref_halted, ref_io, ref_log = run_reference(code, entry, mldbios, stop2, far_jmp_addr, ref_file, open_fail=False)

        c_file = MpmSysFile([hdr, bytes([0xAA]) * 128])
        c_log = BdosLog()
        c_io, in8, out8, bdos, msg, readf = make_c_callbacks(c_file, c_log, open_fail=False)
        mpmcseg = ctypes.c_uint16(0)
        rc = lib.start_entry(ctypes.c_uint16(CS), in8, out8, bdos, msg, readf, ctypes.byref(mpmcseg))

        check("1-record body: returns ok", 0, rc)
        check("1-record body: mpmcseg", 0x3000, mpmcseg.value)
        check("1-record body: INIT register writes", ref_io.writes, c_io.writes)
        check("1-record body: bdos call sequence", ref_log.calls, c_log.calls)
        check("1-record body: console (phex digits)", ref_log.console, c_log.console)
        check("1-record body: messages", ref_log.msgs, c_log.msgs)

        # -- bad header (code+data regions don't line up) --
        hdr = header_record(1, 1, 0x3000, 2, 1, 0x4000)  # dldseg wrong
        ref_file = MpmSysFile([hdr])
        ref_regs, ref_halted, ref_io, ref_log = run_reference(code, entry, mldbios, stop2, far_jmp_addr, ref_file, open_fail=False)

        c_file = MpmSysFile([hdr])
        c_log = BdosLog()
        c_io, in8, out8, bdos, msg, readf = make_c_callbacks(c_file, c_log, open_fail=False)
        mpmcseg = ctypes.c_uint16(0xBEEF)
        rc = lib.start_entry(ctypes.c_uint16(CS), in8, out8, bdos, msg, readf, ctypes.byref(mpmcseg))

        check("bad header: returns fail", 1, rc)
        check("bad header: messages", ref_log.msgs, c_log.msgs)

        # -- file not found (openfnc returns al=255) --
        ref_file = MpmSysFile([])
        ref_regs, ref_halted, ref_io, ref_log = run_reference(code, entry, mldbios, stop2, far_jmp_addr, ref_file, open_fail=True)

        c_file = MpmSysFile([])
        c_log = BdosLog()
        c_io, in8, out8, bdos, msg, readf = make_c_callbacks(c_file, c_log, open_fail=True)
        mpmcseg = ctypes.c_uint16(0xBEEF)
        rc = lib.start_entry(ctypes.c_uint16(CS), in8, out8, bdos, msg, readf, ctypes.byref(mpmcseg))

        check("file not found: returns fail", 1, rc)
        check("file not found: messages", ref_log.msgs, c_log.msgs)

        # -- read error partway through the body --
        hdr = header_record(1, 2, 0x5000, 2, 2, 0x5002)
        ref_file = MpmSysFile([hdr, bytes([1]) * 128, bytes([2]) * 128, bytes([3]) * 128])
        ref_file.fail_at = 2  # fail on the 2nd body record (0-based call index 2)
        ref_regs, ref_halted, ref_io, ref_log = run_reference(code, entry, mldbios, stop2, far_jmp_addr, ref_file, open_fail=False)

        c_file = MpmSysFile([hdr, bytes([1]) * 128, bytes([2]) * 128, bytes([3]) * 128])
        c_file.fail_at = 2
        c_log = BdosLog()
        c_io, in8, out8, bdos, msg, readf = make_c_callbacks(c_file, c_log, open_fail=False)
        mpmcseg = ctypes.c_uint16(0xBEEF)
        rc = lib.start_entry(ctypes.c_uint16(CS), in8, out8, bdos, msg, readf, ctypes.byref(mpmcseg))

        check("read error: returns fail", 1, rc)
        check("read error: bdos call sequence", ref_log.calls, c_log.calls)
        check("read error: messages", ref_log.msgs, c_log.msgs)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
