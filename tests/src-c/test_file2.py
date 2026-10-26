#!/usr/bin/env python3
"""Differential test for src-c/kernel/file2.c against a self-contained
subset of src/kernel/FILE2.BDO: GET_NALBS, getblock (+ internal
lefttst/righttst/rblok/rblok0), and now SWAP (`swap_sdcnt`), the first
routine in this file's own DATA.BDO cseg/dseg collision cluster to get
a real differential test, now that file1.c/file2.c/file3.c/file4.c's
own BD_* fields are genuinely segment-relative (see defs.h's own
`mem_seg_addr` and this file's own DS_BDOS/run_reference_segmented
header comment). `close`/`close1`/`search`/`searchn`/`open`/`open1`/
`opencopy`/`copy_dir0`-`2` are now unblocked in principle the same
way, but don't have their own new tests yet -- same "resolved in
principle, not yet written" precedent as several other clusters
elsewhere in this project (see docs/src-c.md).
On real memory: file*.c take (phys, ds_bdos) and address DATA.BDO's
fields, FCBs and directory buffers through the BDOS's DS (the combined
SYSDAT+DATA.BDO segment). This test keeps that segment at 0, so its flat
addresses are those DS offsets; tests/src-c/test_bdos_real.py covers
the same code with SYSDAT at a nonzero segment.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'file2_wrapper.A86')
FILE2_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file2.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file1.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file3.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file4.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

BD_MAXALL, BD_ALLOCA, BD_CURDSK = 0x0C4D, 0x0C46, 0x0C36
BD_SDCNT, BD_SDCNT0, BD_SDBLK, BD_SDBLK0 = 0x0C7D, 0x0C81, 0x0C7F, 0x0C83

BD_LRET, BD_INFO, BD_DCNT, BD_DBLK = 0x0C0F, 0x0C17, 0x0C25, 0x0C27
BD_CDRMAXA, BD_CHECKA, BD_CHKSIZ, BD_BUFFA = 0x0C3C, 0x0C44, 0x0C53, 0x0C40
BD_EXTMSK, BD_DIRMAX, BD_DPTR = 0x0C4C, 0x0C4F, 0x0CA9
BD_SEARCH_USER0, BD_FIND_XFCB, BD_SRCHA, BD_SRCHL = 0x0C12, 0x0C14, 0x0C19, 0x0C20
BD_FCB_EXISTS, BD_SAVE_XFCB, BD_DIRLOC = 0x0C6B, 0x0C6D, 0x0C99
BD_XDCNT, BD_USER_ZERO_PASS = 0x0CAA, 0x0CAE
BD_SECTPT = 0x0C48

# mpmif and its 9 XIOS-call helpers, plus XCRLF/XPRINT -- all real
# BDOSIF.BDO labels this combined bundle needs to assemble, even
# though nothing file2.c ports actually reaches them (see
# file1_wrapper.A86's own header comment).
# All 3 bytes lower than their own earlier values -- tools/asm86/
# isa.py's own `_encode_alu` now emits the real 8086's dedicated,
# shorter AL/AX,imm accumulator form instead of always the longer
# group1 r/m,imm form (see that function's own header comment),
# shrinking some `AND/OR/.../CMP AL,imm8`-shaped instruction earlier
# in this same combined BDOSIF.BDO+FILE1-4.BDO bundle by a net 3
# bytes -- every label after it shifts down by the same amount.
STUBS = {0x488, 0x4b5, 0x4ba, 0x4bf, 0x4c4, 0x4c9, 0x4ce, 0x4d3, 0x4d8, 0x4dd, 0x406, 0x413}


def _readf_writef_hook(sim):
    # readf/writef tail-jump into xiosif themselves, so a bare
    # call_stub_addrs no-op leaves AL as whatever it was *before* the
    # call (not readf/writef's own real "mov al,io_read"/"io_write"
    # result) -- diocomp's own real "or al,al!jz ret4" then branches on
    # that leftover value instead of the "always succeeds" result
    # file1.c's own `diocomp` already fixes at 0 for the C port. This
    # hook makes the reference run match that same fixed convention
    # instead of leaving AL an uncontrolled artifact.
    sim.regs['ax'] = sim.regs['ax'] & 0xFF00  # al=0 -- i86sim.py has no
                                                # separate 'al' register
                                                # key, only 'ax'


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    names = ('get_nalbs', 'getblock', 'swap', 'search', 'readf', 'writef', 'xiosif')
    entries = {name: asm.symtab[name].value for name in names}

    # BDOSIF.BDO's own shared `xiosif` trampoline (readf/writef/homef/
    # seldskf/settrkf/setsecf/setdmf/setdmbf/sectran/flush all tail-jump
    # into it) does one real indirect `callf dword ptr .xiosmod` --
    # stubbed as a no-op like every other unreached far call in this
    # project (tools/i86sim.py's `far_call_hooks` can model one when the
    # effect matters, as test_bdosif_init.py does for `mpmif`). Found
    # by scanning xiosif's own body for the first `FF /3` (CALL FAR
    # [mem]) byte pattern, not a hardcoded address -- this file's own
    # `AND AL,imm8` encoding fix (tools/asm86/isa.py's own `_encode_alu`)
    # shifts every address after the first such fix, so a literal
    # constant here would silently go stale the next time this wrapper
    # changes.
    xiosif = entries['xiosif']
    far_call_addr = None
    for i in range(xiosif, xiosif + 20):
        if code[i] == 0xFF and (code[i + 1] >> 3) & 7 == 3:
            far_call_addr = i
            break
    assert far_call_addr is not None, "xiosif's own far call not found"
    entries['far_call_stubs'] = {far_call_addr}
    return code, dseg, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *FILE2_C, '-o', lib_path],
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


def run_reference(code, dseg, entry, mem, regs, stop_addrs=()):
    # Unlike test_file1.py/test_file3.py, this one does NOT layer
    # dseg's own real field data over cseg: get_nalbs/getblock's own
    # code (this combined bundle's own FILE2.BDO) happens to land
    # inside DATA.BDO's dseg address range here, where file1.c's/
    # file3.c's own ported routines didn't -- blanket-copying dseg
    # would stomp getblock's own code. Nothing this test exercises
    # relies on an unset dseg default (BD_MAXALL/BD_ALLOCA are always
    # explicitly set below), so this is safe to skip entirely.
    del dseg
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # Without the dseg background, "only nonzero mem bytes win" isn't
    # enough for fields whose own high byte is legitimately 0 (e.g.
    # BD_MAXALL == 100) -- force every byte of the fields this test
    # actually drives unconditionally, same idea as test_file1.py's
    # own rlr/BD_CURDSK special-casing.
    image[BD_MAXALL:BD_MAXALL + 2] = mem[BD_MAXALL:BD_MAXALL + 2]
    image[BD_ALLOCA:BD_ALLOCA + 2] = mem[BD_ALLOCA:BD_ALLOCA + 2]
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs=STUBS)
    sim.run()
    return sim.regs, image


ALLOC_AT = 0x4400

# -- swap_sdcnt: real segmented addressing, proving the DATA.BDO
# cseg/dseg collision fix for real (see file2.c's own header comment
# and defs.h's own mem_seg_addr -- this is the first new differential
# test this project has for any routine in the collision cluster).
# `swap` itself (the real asm's own label) lands at 0xd1d, inside the
# combined wrapper's own dseg-sized [0, 0x1000) window that
# run_reference's own non-segmented convention can't use without
# corrupting this routine's own code -- so this one test alone needs
# `segmented=True` with a genuine, non-degenerate DS (DS_BDOS, chosen
# so its own physical base, DS_BDOS*16, clears this wrapper's entire
# ~0x2659-byte cseg) instead. `swap_sdcnt` touches only BD_SDCNT/
# BD_SDCNT0/BD_SDBLK/BD_SDBLK0 -- no SD_*/UDA_*/CLI_* field at all --
# so this doesn't need to reconcile with those fields' own harness-
# chosen flat scratch addresses used elsewhere in this project.
DS_BDOS = 0x0300


def phys(seg, offset):
    return (seg * 16 + offset) & 0xFFFF


def run_reference_segmented(code, entry, mem, ds_bdos, readf=None, writef=None, far_call_stubs=(),
                             stop_addrs=(), regs_extra=None):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # al=0 -- diocomp's own real "or al,al!jz ret4" (reached via home()/
    # rdbuff()'s own real `call readf`, stubbed as a no-op per STUBS,
    # which leaves AL as whatever it was *before* the call rather than
    # readf's own `mov al,io_read`) must see 0 for the same "always
    # succeeds" boundary-stub convention src-c/kernel/file1.c's own
    # `diocomp` already models for the C port -- any other value sends
    # the reference run down the real pererror/roderror branch instead.
    regs = {'ds': ds_bdos, 'sp': STACK_TOP, 'ax': 0}
    if regs_extra:
        regs.update(regs_extra)
    sim = I86Sim(image, regs, entry, stop_addrs=set(stop_addrs), call_stub_addrs=STUBS,
                 call_hooks={readf: _readf_writef_hook, writef: _readf_writef_hook},
                 far_call_stub_addrs=far_call_stubs, segmented=True)
    sim.run()
    return sim.regs, image


def main():
    code, dseg, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.get_nalbs.restype = ctypes.c_uint16
        lib.get_nalbs.argtypes = [c_u8p, ctypes.c_uint16]
        lib.getblock.restype = ctypes.c_uint16
        lib.getblock.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.swap_sdcnt.argtypes = [c_u8p, ctypes.c_uint16]
        lib.search.restype = ctypes.c_uint8
        lib.search.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            # physical memory for the real-memory C side; this test keeps
            # SYSDAT (the BDOS's DS) at segment 0, see the docstring
            buf = (ctypes.c_uint8 * 0x100000)()
            ctypes.memmove(buf, bytes(mem), len(mem))
            return buf

        # -- get_nalbs --
        mem = base_memory()
        mem_w16(mem, BD_MAXALL, 100)
        ref_regs, _ = run_reference(code, dseg, entries['get_nalbs'], mem, {})
        buf = to_ctypes(mem)
        got = lib.get_nalbs(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        check("get_nalbs", ref_regs['bx'], got)

        def setup(maxall, alloc_bytes):
            mem = base_memory()
            mem_w16(mem, BD_MAXALL, maxall)
            mem_w16(mem, BD_ALLOCA, ALLOC_AT)
            mem[ALLOC_AT:ALLOC_AT + len(alloc_bytes)] = alloc_bytes
            return mem

        # -- getblock: the starting bit itself is free --
        mem = setup(63, bytes([0x00] * 8))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 10})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(10))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 8]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 8]))
        check("getblock: starting bit free", ref, port)

        # -- getblock: bit 9 (the first one tried) and bit 11 (the first
        # righttst try) are both taken, forcing a second lefttst
        # iteration; bit 8 is free. (getallocbit's bit-within-byte order
        # is MSB-first as cx increases: within byte index 1 -- bits
        # 8-15 -- cx=8 is bit7 (0x80), cx=9 is bit6 (0x40), cx=10 is bit5
        # (0x20), cx=11 is bit4 (0x10), confirmed empirically by probing
        # getallocbit directly.)
        mem = setup(63, bytes([0x00, 0x50] + [0] * 6))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 10})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(10))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 8]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 8]))
        check("getblock: nearest free bit is 2 to the left (loops)", ref, port)

        # -- getblock: starting bit taken, left exhausted immediately
        # (cx==0 at entry), nearest free bit is to the right --
        mem = setup(63, bytes([0b00000001] + [0] * 7))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 0})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(0))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 8]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 8]))
        check("getblock: cx == 0, nearest free bit is to the right", ref, port)

        # -- getblock: every bit allocated, maxall small -> not found --
        mem = setup(7, bytes([0xFF]))
        ref_regs, ref_img = run_reference(code, dseg, entries['getblock'], mem, {'cx': 3})
        buf = to_ctypes(mem)
        got = lib.getblock(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(3))
        ref = (ref_regs['bx'], bytes(ref_img[ALLOC_AT:ALLOC_AT + 1]))
        port = (got, bytes(bytes(buf)[ALLOC_AT:ALLOC_AT + 1]))
        check("getblock: fully allocated, not found", ref, port)

        # -- swap_sdcnt: real segmented addressing, proving the DATA.BDO
        # collision fix for real (see this file's own DS_BDOS/
        # run_reference_segmented header comment) --
        mem = base_memory()
        p_sdcnt, p_sdcnt0 = phys(DS_BDOS, BD_SDCNT), phys(DS_BDOS, BD_SDCNT0)
        p_sdblk, p_sdblk0 = phys(DS_BDOS, BD_SDBLK), phys(DS_BDOS, BD_SDBLK0)
        mem_w16(mem, p_sdcnt, 0x1234)
        mem_w16(mem, p_sdcnt0, 0x5678)
        mem_w16(mem, p_sdblk, 0x9ABC)
        mem_w16(mem, p_sdblk0, 0xDEF0)
        ref_regs, ref_img = run_reference_segmented(code, entries['swap'], mem, DS_BDOS)
        buf = to_ctypes(mem)
        lib.swap_sdcnt(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_BDOS))
        port_img = bytes(buf)
        check("swap_sdcnt: sdcnt<->sdcnt0", (mem_r16(ref_img, p_sdcnt), mem_r16(ref_img, p_sdcnt0)),
              (mem_r16(port_img, p_sdcnt), mem_r16(port_img, p_sdcnt0)))
        check("swap_sdcnt: sdblk<->sdblk0", (mem_r16(ref_img, p_sdblk), mem_r16(ref_img, p_sdblk0)),
              (mem_r16(port_img, p_sdblk), mem_r16(port_img, p_sdblk0)))

        # -- search: an exact (non-wildcard) match against the one
        # directory entry present -- `search` itself lands at 0xbf5
        # in this combined bundle, deep inside the collision range.
        # Checksum work is disabled outright (BD_CHKSIZ=0, so
        # `checksum`'s own "past the vector's own size" early return
        # always fires) -- this test is about SEARCH's own directory-
        # scan logic, not FLUSH_FILE0's already-separately-tested
        # checksum-mismatch path. BD_DCNT starts wherever it likes:
        # `search1`'s own `setenddir`/`home` reset it to the "just
        # before record 0" state regardless.
        # `FCB_ADDR`/`DIR_ADDR`/`CDRMAX_ADDR` are externally-provided
        # pointer *values* (stored in BD_INFO/BD_BUFFA/BD_CDRMAXA, real
        # SYSDAT-resident cells -- segment-relative, via DS_BDOS, same
        # as every other BD_* field). The buffers those pointers lead
        # to are a different matter: under `segmented=True`, the
        # reference asm's own bare `[bx]`-style dereference of
        # *any* pointer value defaults to DS too (there's no way to
        # tell the simulator "only BD_*'s own cells are segment-
        # relative, not what they point to" -- real 8086 has no such
        # distinction either, it's DS for every unqualified access).
        # The C port, by design, always treats an externally-provided
        # pointer like `info`/`buffa` as flat (same convention as
        # every other already-ported file in this project). To keep
        # both sides agreeing, each such buffer's own content is
        # written twice: once at its own flat address (what the C
        # port reads) and once at `ref_phys` of that same address
        # (what the segmented reference run's own bare dereference
        # actually resolves to) -- same content, two physical
        # locations, each side reading the one it expects.
        FCB_ADDR, DIR_ADDR, CDRMAX_ADDR = 0x9000, 0x9100, 0x9200

        def ref_phys(addr):
            return (DS_BDOS * 16 + addr) & 0xFFFF

        def put_fcb15(mem, addr, name, typ, extnum=0, ubytes=0, modnum=0):
            for a in (addr, ref_phys(addr)):
                mem[a + 0] = 0  # dr
                mem[a + 1:a + 9] = (name.upper() + ' ' * 8)[:8].encode('ascii')
                mem[a + 9:a + 12] = (typ.upper() + ' ' * 3)[:3].encode('ascii')
                mem[a + 12] = extnum
                mem[a + 13] = ubytes
                mem[a + 14] = modnum

        def put_byte_both(mem, addr, val):
            mem[addr] = val
            mem[ref_phys(addr)] = val

        def put_word_both(mem, addr, val):
            mem_w16(mem, addr, val)
            mem_w16(mem, ref_phys(addr), val)

        def bw16(mem, bd_addr, val):
            mem_w16(mem, phys(DS_BDOS, bd_addr), val)

        def search_setup():
            mem = base_memory()
            bw16(mem, BD_DIRMAX, 100)
            bw16(mem, BD_CHKSIZ, 0)
            bw16(mem, BD_BUFFA, DIR_ADDR)
            bw16(mem, BD_CDRMAXA, CDRMAX_ADDR)
            put_word_both(mem, CDRMAX_ADDR, 999)
            bw16(mem, BD_INFO, FCB_ADDR)
            mem[phys(DS_BDOS, BD_EXTMSK)] = 0
            mem[phys(DS_BDOS, BD_SEARCH_USER0)] = 0
            mem[phys(DS_BDOS, BD_FIND_XFCB)] = 0
            mem[phys(DS_BDOS, BD_SAVE_XFCB)] = 0
            mem[phys(DS_BDOS, BD_USER_ZERO_PASS)] = 0
            bw16(mem, BD_SECTPT, 26)  # seekdir's own real "seek" tail does a
                                       # real `div sectpt` in the reference
                                       # asm (the C port's own boundary-stub
                                       # "seek" never divides at all) -- must
                                       # be nonzero
            return mem

        p_dirloc, p_lret = phys(DS_BDOS, BD_DIRLOC), phys(DS_BDOS, BD_LRET)

        mem = search_setup()
        put_fcb15(mem, FCB_ADDR, 'FOOBAR', 'TXT')
        put_fcb15(mem, DIR_ADDR, 'FOOBAR', 'TXT')
        ref_regs, ref_img = run_reference_segmented(code, entries['search'], mem, DS_BDOS, entries['readf'], entries['writef'], entries['far_call_stubs'], regs_extra={'cx': 15})
        buf = to_ctypes(mem)
        got = lib.search(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_BDOS), ctypes.c_uint8(15))
        port_img = bytes(buf)
        check("search: exact match -> return value", (ref_regs['cx'] >> 8) & 0xFF, got)
        check("search: exact match -> dirloc cleared", ref_img[p_dirloc], port_img[p_dirloc])
        check("search: exact match -> lret", ref_img[p_lret], port_img[p_lret])

        # -- search: no match anywhere -- an empty (0xE5) directory,
        # search runs off the end of the (small) directory and hits
        # srchfin --
        mem = search_setup()
        bw16(mem, BD_DIRMAX, 0)  # only entry 0 exists, and it's empty
        put_fcb15(mem, FCB_ADDR, 'FOOBAR', 'TXT')
        put_byte_both(mem, DIR_ADDR, 0xE5)  # bdos_empty
        ref_regs, ref_img = run_reference_segmented(code, entries['search'], mem, DS_BDOS, entries['readf'], entries['writef'], entries['far_call_stubs'], regs_extra={'cx': 15})
        buf = to_ctypes(mem)
        got = lib.search(ctypes.cast(buf, c_u8p), ctypes.c_uint16(DS_BDOS), ctypes.c_uint8(15))
        port_img = bytes(buf)
        check("search: no match -> return value", (ref_regs['cx'] >> 8) & 0xFF, got)
        check("search: no match -> lret", ref_img[p_lret], port_img[p_lret])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
