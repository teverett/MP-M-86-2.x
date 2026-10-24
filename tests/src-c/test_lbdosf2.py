#!/usr/bin/env python3
"""Differential test for src-c/loader/lbdosf2.c against src/loader/
LBDOSF2.A86. See lbdosf2.c's own header comment for the full story on
`ch` threading (scndm/srch/srchn/srchnext/delet) and getblock's own
unrelated use of rotr.

KNOWN BROKEN as of tools/asm86/isa.py's own `_encode_alu` fix (real
8086 accumulator-immediate short form for AL/AX,imm, see that
function's own header comment) -- NOT yet root-caused or fixed. The
`initialize` test case's own reference run now runs off the end of
this wrapper's own assembled code (falls through zero-filled memory
for thousands of steps after a `JMP [BX+0]` -- a jump-table dispatch,
`LEA BX,[6]` then indirect through it -- lands somewhere wrong) and
eventually hits a genuinely unimplemented opcode far from any real
code. Every other instance of this same encoding fix's fallout this
session turned out to be a stale hardcoded probed address (see
test_file1.py/test_file2.py/test_file3.py/test_file4.py/
test_ldbios86q.py/test_ldmpm.py/test_load.py's own fixes, and
test_load.py's own FCB_ADDR/cseg-collision fix specifically) -- this
one is likely the same general class (a jump-table entry whose own
real raw position moved) but wasn't tracked down before time ran out
on this pass. Needs its own dedicated look.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'lbdosf2_wrapper.A86')
LBDOSF2_C = [os.path.join(REPO_ROOT, 'src-c', 'loader', f) for f in ('lbdosf2.c', 'lbdosf1.c', 'lbdosio.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0

LD_CURDSK, LD_INFO, LD_RODSK, LD_DMAAD = 0xB010, 0xB011, 0xB013, 0xB015
LD_CDRMAXA, LD_CURTRKA, LD_CURRECA = 0xB017, 0xB019, 0xB01B
LD_BUFFA, LD_DPBADDR, LD_CHECKA, LD_ALLOCA = 0xB01D, 0xB01F, 0xB021, 0xB023
LD_SECTPT, LD_BLKSHF, LD_BLKMSK, LD_EXTMSK = 0xB025, 0xB027, 0xB028, 0xB029
LD_MAXALL, LD_DIRMAX, LD_CHKSIZ, LD_OFFSETV = 0xB02A, 0xB02C, 0xB02E, 0xB030
LD_TRANV, LD_SEQIO, LD_SINGLE, LD_RCOUNT, LD_EXTVAL = 0xB032, 0xB034, 0xB035, 0xB036, 0xB037
LD_VRECORD, LD_ARECORD, LD_ABLOCK, LD_DPTR = 0xB038, 0xB03A, 0xB03C, 0xB03E
LD_DCNT, LD_DREC, LD_DMABASE, LD_PARAMETERSEGMENT = 0xB03F, 0xB041, 0xB043, 0xB045
LD_DIRBLK, LD_USRCODE, LD_DIRLOC = 0xB047, 0xB049, 0xB04A
LD_SRCHL, LD_SRCHA, LD_RMF, LD_FCBCOPIED, LD_EFCB = 0xB04B, 0xB04C, 0xB04E, 0xB04F, 0xB050
LD_ARET = 0xB004

FCB_EXTNUM, FCB_MODNUM, FCB_RECCNT, FCB_DSKMAP, FCB_NXTREC = 12, 14, 15, 16, 32
EMPTY = 0xE5

INFO_AT = 0x4000
BUFFA_AT = 0x4100
ALLOC_AT = 0x4400

STUBS = {0x1b, 0x18, 0x27, 0x2a, 0x1e, 0x30, 0x21, 0x33, 0x24}


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'loader')])
    code = asm.segment_bytes('cseg')
    names = ('scndm', 'initialize', 'ret8b', 'cpydirloc', 'srch', 'srchn', 'srchfin', 'endsearch',
             'delet', 'getblock', 'copyfcb', 'rename', 'indicators', 'open', 'close', 'make',
             'openreel', 'openmod', 'openr0', 'openr1', 'openr2', 'openerr')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *LBDOSF2_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    return bytearray(0x10000)


def run_reference(code, entry, mem, regs, stop_addrs=()):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs=STUBS)
    sim.run()
    return sim.regs, image, sim


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.scndm.argtypes = [c_u8p, ctypes.c_uint8, ctypes.c_uint8]
        lib.initialize.argtypes = [c_u8p]
        lib.cpydirloc.argtypes = [c_u8p]
        lib.srch.restype = ctypes.c_uint8
        lib.srch.argtypes = [c_u8p, ctypes.c_uint8, ctypes.c_uint8]
        lib.srchn.restype = ctypes.c_uint8
        lib.srchn.argtypes = [c_u8p, ctypes.c_uint8]
        lib.delet.argtypes = [c_u8p]
        lib.getblock.restype = ctypes.c_uint16
        lib.getblock.argtypes = [c_u8p, ctypes.c_uint16]
        lib.copyfcb.argtypes = [c_u8p]
        lib.rename_.argtypes = [c_u8p]
        lib.indicators.argtypes = [c_u8p]
        lib.open_.argtypes = [c_u8p]
        lib.close_.argtypes = [c_u8p]
        lib.make.argtypes = [c_u8p]
        lib.openreel.argtypes = [c_u8p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def setup_search(mem, usrcode=0, entries_data=None):
            """Populate a 128-byte directory record at BUFFA_AT with
            up to 4 32-byte entries (entries_data), the rest empty."""
            mem_w16(mem, LD_BUFFA, BUFFA_AT)
            mem[LD_USRCODE] = usrcode
            for i in range(4):
                base = BUFFA_AT + i * 32
                if entries_data and i < len(entries_data):
                    mem[base:base + len(entries_data[i])] = entries_data[i]
                else:
                    mem[base] = EMPTY

        # -- scndm: single-byte map, one nonzero entry --
        mem = base_memory()
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem[LD_DPTR] = 0
        mem[LD_SINGLE] = 1
        mem_w16(mem, LD_MAXALL, 20)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem[BUFFA_AT + FCB_DSKMAP] = 5  # block 5
        ref_regs, ref_img, ref_sim = run_reference(code, entries['scndm'], mem, {'cx': 0x0001})
        buf = to_ctypes(mem)
        lib.scndm(ctypes.cast(buf, c_u8p), ctypes.c_uint8(1), ctypes.c_uint8(0))
        check("scndm: single-byte map", ref_img[ALLOC_AT:ALLOC_AT + 64], bytes(buf)[ALLOC_AT:ALLOC_AT + 64])

        # -- scndm: double-byte map, one nonzero entry --
        mem = base_memory()
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem[LD_DPTR] = 0
        mem[LD_SINGLE] = 0
        mem_w16(mem, LD_MAXALL, 300)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem_w16(mem, BUFFA_AT + FCB_DSKMAP, 200)
        ref_regs, ref_img, ref_sim = run_reference(code, entries['scndm'], mem, {'cx': 0x0301})
        buf = to_ctypes(mem)
        lib.scndm(ctypes.cast(buf, c_u8p), ctypes.c_uint8(1), ctypes.c_uint8(3))
        check("scndm: double-byte map", ref_img[ALLOC_AT:ALLOC_AT + 64], bytes(buf)[ALLOC_AT:ALLOC_AT + 64])

        # -- initialize: one real entry (block 5), rest empty --
        mem = base_memory()
        mem_w16(mem, LD_MAXALL, 20)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DIRBLK, 0)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[LD_USRCODE] = 0
        mem[LD_SINGLE] = 1
        setup_search(mem, usrcode=0, entries_data=[bytes([0] * 16 + [5] + [0] * 15)])
        ref_regs, ref_img, ref_sim = run_reference(code, entries['initialize'], mem, {}, stop_addrs={entries['ret8b']})
        buf = to_ctypes(mem)
        lib.initialize(ctypes.cast(buf, c_u8p))
        check("initialize", ref_img[ALLOC_AT:ALLOC_AT + 4], bytes(buf)[ALLOC_AT:ALLOC_AT + 4])

        # -- cpydirloc --
        mem = base_memory()
        mem[LD_DIRLOC] = 0x42
        mem_w16(mem, LD_ARET, 0x9900)
        ref_regs, ref_img, _ = run_reference(code, entries['cpydirloc'], mem, {})
        buf = to_ctypes(mem)
        lib.cpydirloc(ctypes.cast(buf, c_u8p))
        check("cpydirloc", ref_img[LD_ARET], bytes(buf)[LD_ARET])

        # -- srch/srchn: successful match on the 1st entry --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        setup_search(mem, usrcode=0, entries_data=[b'TESTFILE TXT' + bytes(20)])
        ref_regs, ref_img, _ = run_reference(code, entries['srch'], mem, {'cx': 12})
        buf = to_ctypes(mem)
        got = lib.srch(ctypes.cast(buf, c_u8p), ctypes.c_uint8(12), ctypes.c_uint8(0))
        ref = (ref_img[LD_ARET], ref_img[LD_DIRLOC])
        port = (bytes(buf)[LD_ARET], bytes(buf)[LD_DIRLOC])
        check("srch: successful match", ref, port)

        # -- srch: no match, end of directory --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'NOPE    TXT '
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        setup_search(mem, usrcode=0, entries_data=[b'TESTFILE TXT' + bytes(20)])
        ref_regs, ref_img, _ = run_reference(code, entries['srch'], mem, {'cx': 12})
        buf = to_ctypes(mem)
        lib.srch(ctypes.cast(buf, c_u8p), ctypes.c_uint8(12), ctypes.c_uint8(0))
        check("srch: no match", ref_img[LD_ARET], bytes(buf)[LD_ARET])

        # -- delet: one matching entry --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem_w16(mem, LD_MAXALL, 20)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem[LD_SINGLE] = 1
        setup_search(mem, usrcode=0, entries_data=[b'TESTFILE TXT' + bytes(4) + bytes([7]) + bytes(15)])
        ref_regs, ref_img, _ = run_reference(code, entries['delet'], mem, {})
        buf = to_ctypes(mem)
        lib.delet(ctypes.cast(buf, c_u8p))
        ref = (ref_img[BUFFA_AT], ref_img[ALLOC_AT:ALLOC_AT + 2])
        port = (bytes(buf)[BUFFA_AT], bytes(buf)[ALLOC_AT:ALLOC_AT + 2])
        check("delet", ref, port)

        # -- getblock: starting bit free --
        mem = base_memory()
        mem_w16(mem, LD_MAXALL, 63)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem[ALLOC_AT:ALLOC_AT + 8] = bytes(8)
        ref_regs, ref_img, _ = run_reference(code, entries['getblock'], mem, {'cx': 10})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(10))
        ref = (ref_regs['bx'], ref_img[ALLOC_AT:ALLOC_AT + 8])
        port = (got, bytes(buf)[ALLOC_AT:ALLOC_AT + 8])
        check("getblock: starting bit free", ref, port)

        # -- getblock: fully allocated --
        mem = base_memory()
        mem_w16(mem, LD_MAXALL, 7)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem[ALLOC_AT] = 0xFF
        ref_regs, ref_img, _ = run_reference(code, entries['getblock'], mem, {'cx': 3})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(3))
        ref = (ref_regs['bx'], ref_img[ALLOC_AT])
        port = (got, bytes(buf)[ALLOC_AT])
        check("getblock: fully allocated", ref, port)

        # -- copyfcb -- has no RET of its own, falls through into
        # seekdir+wrdir too (see lbdosf2.c's own header comment) --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem[LD_DPTR] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem_w16(mem, LD_DREC, 50)  # past chksiz -- checksum is a no-op
        for i in range(32):
            mem[INFO_AT + i] = (i * 5) & 0xFF
        ref_regs, ref_img, _ = run_reference(code, entries['copyfcb'], mem, {})
        buf = to_ctypes(mem)
        lib.copyfcb(ctypes.cast(buf, c_u8p))
        check("copyfcb", ref_img[BUFFA_AT:BUFFA_AT + 32], bytes(buf)[BUFFA_AT:BUFFA_AT + 32])

        # -- rename: one matching entry --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem[INFO_AT + 16:INFO_AT + 28] = b'NEWNAME TXT '
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        setup_search(mem, usrcode=0, entries_data=[b'TESTFILE TXT' + bytes(20)])
        ref_regs, ref_img, _ = run_reference(code, entries['rename'], mem, {})
        buf = to_ctypes(mem)
        lib.rename_(ctypes.cast(buf, c_u8p))
        check("rename", ref_img[BUFFA_AT:BUFFA_AT + 32], bytes(buf)[BUFFA_AT:BUFFA_AT + 32])

        # -- indicators: one matching entry --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        setup_search(mem, usrcode=0, entries_data=[b'TESTFILE TXT' + bytes(20)])
        ref_regs, ref_img, _ = run_reference(code, entries['indicators'], mem, {})
        buf = to_ctypes(mem)
        lib.indicators(ctypes.cast(buf, c_u8p))
        check("indicators", ref_img[BUFFA_AT:BUFFA_AT + 12], bytes(buf)[BUFFA_AT:BUFFA_AT + 12])

        # -- open: found --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 15] = b'TESTFILE TXT' + bytes(3)
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        entry_bytes = bytearray(32)
        entry_bytes[0:12] = b'TESTFILE TXT'
        entry_bytes[FCB_EXTNUM] = 0
        entry_bytes[FCB_RECCNT] = 42
        setup_search(mem, usrcode=0, entries_data=[bytes(entry_bytes)])
        ref_regs, ref_img, _ = run_reference(code, entries['open'], mem, {})
        buf = to_ctypes(mem)
        lib.open_(ctypes.cast(buf, c_u8p))
        check("open: found", ref_img[INFO_AT:INFO_AT + 32], bytes(buf)[INFO_AT:INFO_AT + 32])

        # -- close: single-byte map, matching disk map, fcb reccnt wins --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_MODNUM] = 0  # fwf clear -> proceed
        mem[INFO_AT + FCB_EXTNUM] = 3
        mem[INFO_AT + FCB_RECCNT] = 99
        for i in range(16):
            mem[INFO_AT + FCB_DSKMAP + i] = i + 1  # nonzero disk map
        entry_bytes = bytearray(32)
        entry_bytes[0:12] = b'TESTFILE TXT'
        entry_bytes[FCB_EXTNUM] = 2
        entry_bytes[FCB_RECCNT] = 10
        for i in range(16):
            entry_bytes[FCB_DSKMAP + i] = i + 1  # matches fcb's own disk map
        setup_search(mem, usrcode=0, entries_data=[bytes(entry_bytes)])
        ref_regs, ref_img, _ = run_reference(code, entries['close'], mem, {})
        buf = to_ctypes(mem)
        lib.close_(ctypes.cast(buf, c_u8p))
        ref = (ref_img[LD_ARET], ref_img[BUFFA_AT:BUFFA_AT + 32])
        port = (bytes(buf)[LD_ARET], bytes(buf)[BUFFA_AT:BUFFA_AT + 32])
        check("close: single-byte map, merge ok", ref, port)

        # -- close: disk map mismatch -> mergerr --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_MODNUM] = 0
        mem[INFO_AT + FCB_DSKMAP] = 5
        entry_bytes = bytearray(32)
        entry_bytes[0:12] = b'TESTFILE TXT'
        entry_bytes[FCB_DSKMAP] = 9  # mismatch (neither is 0)
        setup_search(mem, usrcode=0, entries_data=[bytes(entry_bytes)])
        ref_regs, ref_img, _ = run_reference(code, entries['close'], mem, {})
        buf = to_ctypes(mem)
        lib.close_(ctypes.cast(buf, c_u8p))
        check("close: disk map mismatch -> mergerr", ref_img[LD_ARET], bytes(buf)[LD_ARET])

        # -- make: free slot found --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'NEWFILE TXT '
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, 0x4200, 10)
        setup_search(mem, usrcode=0, entries_data=None)  # all 4 slots empty
        ref_regs, ref_img, _ = run_reference(code, entries['make'], mem, {})
        buf = to_ctypes(mem)
        lib.make(ctypes.cast(buf, c_u8p))
        ref = (ref_img[BUFFA_AT:BUFFA_AT + 32], mem_r16(ref_img, 0x4200))
        port = (bytes(buf)[BUFFA_AT:BUFFA_AT + 32], mem_r16(bytes(buf), 0x4200))
        check("make: free slot found", ref, port)

        # -- openreel: extent increments within the same module, fcb
        # was copied -> same-group copy path --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT:INFO_AT + 12] = b'TESTFILE TXT'
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[LD_SINGLE] = 1
        mem[LD_EXTMSK] = 0x00  # every extent is "in the same group"
        mem[INFO_AT + FCB_MODNUM] = 0x80  # fwf already set -> close is a no-op past nowrite/getmodnum
        mem[INFO_AT + FCB_EXTNUM] = 0
        entry_bytes = bytearray(32)
        entry_bytes[0:12] = b'TESTFILE TXT'
        setup_search(mem, usrcode=0, entries_data=[bytes(entry_bytes)])
        ref_regs, ref_img, _ = run_reference(code, entries['openreel'], mem, {})
        buf = to_ctypes(mem)
        lib.openreel(ctypes.cast(buf, c_u8p))
        ref = (ref_img[LD_ARET], ref_img[INFO_AT + FCB_EXTNUM])
        port = (bytes(buf)[LD_ARET], bytes(buf)[INFO_AT + FCB_EXTNUM])
        check("openreel: same-group extent bump", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
