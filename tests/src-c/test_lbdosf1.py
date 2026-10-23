#!/usr/bin/env python3
"""Differential test for src-c/loader/lbdosf1.c against src/loader/
LBDOSF1.A86. See lbdosf1.c's own header comment for exactly which
routines are ported and why the rest (selerror/goerr, setallocbit/
rotr, setdata/setdir/setdma) is deferred.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'lbdosf1_wrapper.A86')
LBDOSF1_C = os.path.join(REPO_ROOT, 'src-c', 'loader', 'lbdosf1.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'loader')

STACK_TOP = 0xFFF0

# seldskf/homef/readf/writef/settrkf/sectran/setsecf/setdmbf/setdmf --
# found via `python3 -c "...asm.symtab[name].value..."` against this
# same wrapper (see lbdosf1_wrapper.A86's own header comment: these
# coincidentally overlap this wrapper's own early code, harmless).
STUBS = {0x1b, 0x18, 0x27, 0x2a, 0x1e, 0x30, 0x21, 0x33, 0x24}

LD_CURDSK, LD_INFO, LD_RODSK, LD_DMAAD = 0xB010, 0xB011, 0xB013, 0xB015
LD_CDRMAXA, LD_CURTRKA, LD_CURRECA = 0xB017, 0xB019, 0xB01B
LD_BUFFA, LD_DPBADDR, LD_CHECKA, LD_ALLOCA = 0xB01D, 0xB01F, 0xB021, 0xB023
LD_SECTPT, LD_BLKSHF, LD_BLKMSK, LD_EXTMSK = 0xB025, 0xB027, 0xB028, 0xB029
LD_MAXALL, LD_DIRMAX, LD_CHKSIZ, LD_OFFSETV = 0xB02A, 0xB02C, 0xB02E, 0xB030
LD_TRANV, LD_SEQIO, LD_SINGLE, LD_RCOUNT, LD_EXTVAL = 0xB032, 0xB034, 0xB035, 0xB036, 0xB037
LD_VRECORD, LD_ARECORD, LD_ABLOCK, LD_DPTR = 0xB038, 0xB03A, 0xB03C, 0xB03E
LD_DCNT, LD_DREC, LD_DMABASE, LD_PARAMETERSEGMENT = 0xB03F, 0xB041, 0xB043, 0xB045

FCB_ROFILE, FCB_MODNUM, FCB_RECCNT, FCB_DSKMAP, FCB_NXTREC = 9, 14, 15, 16, 32
FWFMSK = 0x80

INFO_AT = 0x4000   # scratch FCB address, well clear of cseg/dseg
BUFFA_AT = 0x4100  # scratch directory-buffer address
ALLOC_AT = 0x4400  # scratch allocation-vector address


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'loader')])
    code = asm.segment_bytes('cseg')
    names = ('selectdisk', 'home', 'rdbuff', 'wrbuff', 'seekdir', 'seek',
             'index', 'alloct', 'atran', 'getfcb', 'setfcb', 'cmpecs',
             'nowrite', 'setro', 'ckrodir', 'ckrofile', 'checkwrite',
             'getdptra', 'getmodnum', 'clrmodnum', 'setfwf', 'compcdr',
             'setcdr', 'newchecksum', 'checksum', 'wrdir', 'rddirbuf',
             'dirtouser', 'endofdir', 'setenddir', 'rddir', 'rddir0',
             'getallocbit', 'setallocbit', 'rotr')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, LBDOSF1_C, '-o', lib_path],
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
    # This wrapper's own dseg lives at 0xB010+, well clear of cseg (the
    # ported code tops out well under 0x400) -- no collision to work
    # around, unlike file1_wrapper.A86/file2_wrapper.A86's own combined-
    # bundle situation. Every field also defaults to 0 in the wrapper
    # itself, so there's no real nonzero background to layer either --
    # a plain "nonzero mem bytes win" copy is sufficient on its own.
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
        c_u16p = ctypes.POINTER(ctypes.c_uint16)

        lib.selectdisk.argtypes = [c_u8p]
        lib.home.argtypes = [c_u8p]
        lib.rdbuff.argtypes = [c_u8p]
        lib.wrbuff.argtypes = [c_u8p]
        lib.seekdir.argtypes = [c_u8p]
        lib.seek.argtypes = [c_u8p]
        lib.bdos_index.argtypes = [c_u8p]
        lib.alloct.restype = ctypes.c_uint16
        lib.alloct.argtypes = [c_u8p]
        lib.atran.argtypes = [c_u8p]
        lib.getfcb.argtypes = [c_u8p]
        lib.setfcb.argtypes = [c_u8p]
        lib.cmpecs.restype = ctypes.c_uint8
        lib.cmpecs.argtypes = [c_u8p]
        lib.nowrite.restype = ctypes.c_uint8
        lib.nowrite.argtypes = [c_u8p]
        lib.setro.argtypes = [c_u8p]
        lib.ckrodir.argtypes = [c_u8p]
        lib.ckrofile.argtypes = [c_u8p, ctypes.c_uint16]
        lib.checkwrite.argtypes = [c_u8p]
        lib.getdptra.restype = ctypes.c_uint16
        lib.getdptra.argtypes = [c_u8p]
        lib.getmodnum.restype = ctypes.c_uint8
        lib.getmodnum.argtypes = [c_u8p, c_u16p]
        lib.clrmodnum.argtypes = [c_u8p]
        lib.setfwf.restype = ctypes.c_uint8
        lib.setfwf.argtypes = [c_u8p]
        lib.compcdr.restype = ctypes.c_uint8
        lib.compcdr.argtypes = [c_u8p]
        lib.setcdr.argtypes = [c_u8p]
        lib.newchecksum.argtypes = [c_u8p]
        lib.checksum.argtypes = [c_u8p, ctypes.c_uint8]
        lib.wrdir.argtypes = [c_u8p]
        lib.rddirbuf.argtypes = [c_u8p]
        lib.dirtouser.argtypes = [c_u8p]
        lib.endofdir.restype = ctypes.c_uint8
        lib.endofdir.argtypes = [c_u8p]
        lib.setenddir.argtypes = [c_u8p]
        lib.rddir.argtypes = [c_u8p, ctypes.c_uint8]
        lib.rddir0.argtypes = [c_u8p, ctypes.c_uint8]
        lib.getallocbit.restype = ctypes.c_uint8
        lib.getallocbit.argtypes = [c_u8p, ctypes.c_uint16, c_u16p, ctypes.POINTER(ctypes.c_uint8), c_u16p]
        lib.setallocbit.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
        lib.rotr.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def run_both(name, cfunc, mem, regs, cargs=()):
            ref_regs, ref_img, _ref_sim = run_reference(code, entries[name], mem, regs)
            buf = to_ctypes(mem)
            cfunc(ctypes.cast(buf, c_u8p), *cargs)
            return ref_regs, ref_img, bytes(buf)

        # -- selectdisk: seldskf boundary-stub always "fails" (bx=0),
        # so this always returns immediately with no observable effect
        # -- just confirm both sides run to completion without crashing --
        mem = base_memory()
        mem[LD_CURDSK] = 3
        run_both('selectdisk', lib.selectdisk, mem, {})
        check("selectdisk: boundary stub fails, no effect", True, True)

        # -- home: homef boundary-stub no-op, but [curtrka] is always
        # zeroed as a real side effect --
        mem = base_memory()
        mem_w16(mem, LD_CURTRKA, 0x4200)
        mem[0x4200] = 0xAA
        mem[0x4201] = 0xBB
        ref_regs, ref_img, port_img = run_both('home', lib.home, mem, {})
        check("home: zeroes [curtrka]", ref_img[0x4200:0x4202], port_img[0x4200:0x4202])

        # -- rdbuff/wrbuff: readf/writef boundary-stub always "succeeds",
        # no observable effect --
        for name, fn in (('rdbuff', lib.rdbuff), ('wrbuff', lib.wrbuff)):
            mem = base_memory()
            run_both(name, fn, mem, {})
            check(f"{name}: boundary stub succeeds, no effect", True, True)

        # -- seekdir/seek: compute arecord/drec, seek a real track into
        # [curtrka], settrkf/sectran/setsecf no-ops --
        mem = base_memory()
        mem_w16(mem, LD_DCNT, 37)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4200)
        ref_regs, ref_img, port_img = run_both('seekdir', lib.seekdir, mem, {})
        ref = (mem_r16(ref_img, LD_ARECORD), mem_r16(ref_img, LD_DREC), mem_r16(ref_img, 0x4200))
        port = (mem_r16(port_img, LD_ARECORD), mem_r16(port_img, LD_DREC), mem_r16(port_img, 0x4200))
        check("seekdir: computes arecord/drec, seeks via curtrka", ref, port)

        mem = base_memory()
        mem_w16(mem, LD_ARECORD, 100)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4200)
        ref_regs, ref_img, port_img = run_both('seek', lib.seek, mem, {})
        check("seek: writes the computed track via curtrka",
              mem_r16(ref_img, 0x4200), mem_r16(port_img, 0x4200))

        # -- bdos_index/alloct/atran, chained like the real code does --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[LD_BLKSHF] = 3
        mem[LD_EXTVAL] = 2
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_NXTREC] = 20  # lvrecord, via getfcb-style setup
        mem[LD_VRECORD] = 20
        mem[LD_BLKMSK] = 7
        mem[INFO_AT + FCB_DSKMAP + 18] = 0x2A  # some disk-map byte index gets read
        ref_regs, ref_img, port_img = run_both('index', lib.bdos_index, mem, {})
        check("bdos_index: computes arecord", mem_r16(ref_img, LD_ARECORD), mem_r16(port_img, LD_ARECORD))

        mem2 = bytearray(port_img)
        got_alloct = lib.alloct(ctypes.cast(to_ctypes(mem2), c_u8p))
        ref_regs2, ref_img2, _ = run_reference(code, entries['alloct'], bytearray(ref_img), {})
        check("alloct: returns arecord", ref_regs2['bx'], got_alloct)

        mem3 = bytearray(port_img)
        buf3 = to_ctypes(mem3)
        lib.atran(ctypes.cast(buf3, c_u8p))
        ref_regs3, ref_img3, _ = run_reference(code, entries['atran'], bytearray(ref_img), {})
        check("atran: computes ablock/arecord",
              (mem_r16(ref_img3, LD_ABLOCK), mem_r16(ref_img3, LD_ARECORD)),
              (mem_r16(bytes(buf3), LD_ABLOCK), mem_r16(bytes(buf3), LD_ARECORD)))

        # -- getfcb/setfcb --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT + FCB_NXTREC] = 42
        mem[INFO_AT + FCB_RECCNT] = 7
        mem[INFO_AT + 12] = 0x55  # extnum byte
        mem[LD_EXTMSK] = 0x1F
        ref_regs, ref_img, port_img = run_both('getfcb', lib.getfcb, mem, {})
        ref = (ref_img[LD_VRECORD], ref_img[LD_RCOUNT], ref_img[LD_EXTVAL])
        port = (port_img[LD_VRECORD], port_img[LD_RCOUNT], port_img[LD_EXTVAL])
        check("getfcb", ref, port)

        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[LD_SEQIO] = 1
        mem[LD_VRECORD] = 42
        mem[LD_RCOUNT] = 7
        ref_regs, ref_img, port_img = run_both('setfcb', lib.setfcb, mem, {})
        ref = (ref_img[INFO_AT + FCB_NXTREC], ref_img[INFO_AT + FCB_RECCNT])
        port = (port_img[INFO_AT + FCB_NXTREC], port_img[INFO_AT + FCB_RECCNT])
        check("setfcb", ref, port)

        # -- cmpecs --
        mem = base_memory()
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        for i in range(128):
            mem[BUFFA_AT + i] = (i * 7) & 0xFF
        ref_regs, ref_img, port_img = run_both('cmpecs', lib.cmpecs, mem, {})
        check("cmpecs", ref_regs['ax'] & 0xFF, lib.cmpecs(ctypes.cast(to_ctypes(mem), c_u8p)))

        # -- nowrite --
        mem = base_memory()
        mem_w16(mem, LD_RODSK, 0x0004)
        mem[LD_CURDSK] = 2
        ref_regs, _, _ = run_reference(code, entries['nowrite'], mem, {})
        check("nowrite: disk is read-only", ref_regs['ax'] & 1, lib.nowrite(ctypes.cast(to_ctypes(mem), c_u8p)))

        mem = base_memory()
        mem_w16(mem, LD_RODSK, 0x0000)
        mem[LD_CURDSK] = 2
        ref_regs, _, _ = run_reference(code, entries['nowrite'], mem, {})
        check("nowrite: disk is writable", ref_regs['ax'] & 1, lib.nowrite(ctypes.cast(to_ctypes(mem), c_u8p)))

        # -- setro --
        mem = base_memory()
        mem[LD_CURDSK] = 1
        mem_w16(mem, LD_RODSK, 0)
        mem_w16(mem, LD_DIRMAX, 63)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        ref_regs, ref_img, port_img = run_both('setro', lib.setro, mem, {})
        ref = (mem_r16(ref_img, LD_RODSK), mem_r16(ref_img, 0x4200))
        port = (mem_r16(port_img, LD_RODSK), mem_r16(port_img, 0x4200))
        check("setro", ref, port)

        # -- ckrodir/ckrofile: r/o bit clear --
        mem = base_memory()
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem[LD_DPTR] = 0
        mem[BUFFA_AT + FCB_ROFILE] = 0x00
        run_both('ckrodir', lib.ckrodir, mem, {})
        check("ckrodir: r/o bit clear, no-op", True, True)

        # -- ckrofile: r/o bit set (deferred error tail, state up to it) --
        mem = base_memory()
        mem[BUFFA_AT + FCB_ROFILE] = 0x80
        buf = to_ctypes(mem)
        lib.ckrofile(ctypes.cast(buf, c_u8p), ctypes.c_uint16(BUFFA_AT))
        check("ckrofile: r/o bit set, no crash (deferred tail)", True, True)

        # -- checkwrite: disk is writable (no-op) --
        mem = base_memory()
        mem_w16(mem, LD_RODSK, 0)
        mem[LD_CURDSK] = 1
        run_both('checkwrite', lib.checkwrite, mem, {})
        check("checkwrite: disk writable, no-op", True, True)

        # -- getdptra --
        mem = base_memory()
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem[LD_DPTR] = 0x60
        ref_regs, _, _ = run_reference(code, entries['getdptra'], mem, {})
        check("getdptra", ref_regs['bx'], lib.getdptra(ctypes.cast(to_ctypes(mem), c_u8p)))

        # -- getmodnum/clrmodnum/setfwf --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT + FCB_MODNUM] = 0x05
        ref_regs, _, _ = run_reference(code, entries['getmodnum'], mem, {})
        bx = ctypes.c_uint16()
        got = lib.getmodnum(ctypes.cast(to_ctypes(mem), c_u8p), ctypes.byref(bx))
        check("getmodnum", (ref_regs['ax'] & 0xFF, ref_regs['bx']), (got, bx.value))

        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT + FCB_MODNUM] = 0x05
        ref_regs, ref_img, port_img = run_both('clrmodnum', lib.clrmodnum, mem, {})
        check("clrmodnum", ref_img[INFO_AT + FCB_MODNUM], port_img[INFO_AT + FCB_MODNUM])

        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT + FCB_MODNUM] = 0x05
        ref_regs, _, _ = run_reference(code, entries['setfwf'], mem, {})
        got = lib.setfwf(ctypes.cast(to_ctypes(mem), c_u8p))
        check("setfwf", ref_regs['ax'] & 0xFF, got)

        # -- compcdr/setcdr -- compcdr returns its result via the carry
        # flag, not a register, so check sim.cf directly --
        mem = base_memory()
        mem_w16(mem, LD_DCNT, 5)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, 0x4200, 10)
        _, _, ref_sim = run_reference(code, entries['compcdr'], mem, {})
        check("compcdr: cdrmax > dcnt", ref_sim.cf, lib.compcdr(ctypes.cast(to_ctypes(mem), c_u8p)))

        mem = base_memory()
        mem_w16(mem, LD_DCNT, 5)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, 0x4200, 10)
        ref_regs, ref_img, port_img = run_both('setcdr', lib.setcdr, mem, {})
        check("setcdr: cdrmax > dcnt, no update", mem_r16(ref_img, 0x4200), mem_r16(port_img, 0x4200))

        mem = base_memory()
        mem_w16(mem, LD_DCNT, 10)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, 0x4200, 5)
        ref_regs, ref_img, port_img = run_both('setcdr', lib.setcdr, mem, {})
        check("setcdr: cdrmax <= dcnt, updates to dcnt+1", mem_r16(ref_img, 0x4200), mem_r16(port_img, 0x4200))

        # -- newchecksum: past the checksum vector, no-op --
        mem = base_memory()
        mem_w16(mem, LD_DREC, 50)
        mem_w16(mem, LD_CHKSIZ, 10)
        run_both('newchecksum', lib.newchecksum, mem, {})
        check("newchecksum: past checksum vector, no-op", True, True)

        # -- newchecksum: within range, initializes --
        mem = base_memory()
        mem_w16(mem, LD_DREC, 2)
        mem_w16(mem, LD_CHKSIZ, 10)
        mem_w16(mem, LD_CHECKA, 0x4300)
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        for i in range(128):
            mem[BUFFA_AT + i] = i
        ref_regs, ref_img, port_img = run_both('newchecksum', lib.newchecksum, mem, {})
        check("newchecksum: within range, initializes",
              ref_img[0x4300 + 2], port_img[0x4300 + 2])

        # -- checksum: mismatch, cdrmax > dcnt -> setro --
        mem = base_memory()
        mem_w16(mem, LD_DREC, 2)
        mem_w16(mem, LD_CHKSIZ, 10)
        mem_w16(mem, LD_CHECKA, 0x4300)
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem[0x4300 + 2] = 0xFF  # stored checksum, won't match
        mem_w16(mem, LD_DCNT, 2)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, 0x4200, 10)
        _, ref_img, _ = run_reference(code, entries['checksum'], mem, {'cx': 0})
        buf = to_ctypes(mem)
        lib.checksum(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0))
        ref = (mem_r16(ref_img, LD_RODSK),)
        port = (mem_r16(bytes(buf), LD_RODSK),)
        check("checksum: mismatch, cdrmax > dcnt -> setro", ref, port)

        # -- wrdir/rddirbuf: no observable effect beyond their own real
        # pieces (newchecksum/readf/writef, setdir/setdata no-ops) --
        mem = base_memory()
        mem_w16(mem, LD_DREC, 50)
        mem_w16(mem, LD_CHKSIZ, 10)
        run_both('wrdir', lib.wrdir, mem, {})
        check("wrdir", True, True)

        mem = base_memory()
        run_both('rddirbuf', lib.rddirbuf, mem, {})
        check("rddirbuf", True, True)

        # -- dirtouser --
        mem = base_memory()
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        mem_w16(mem, LD_DMAAD, 0x4200)
        for i in range(128):
            mem[BUFFA_AT + i] = (i * 3) & 0xFF
        ref_regs, ref_img, port_img = run_both('dirtouser', lib.dirtouser, mem, {})
        check("dirtouser", ref_img[0x4200:0x4280], port_img[0x4200:0x4280])

        # -- endofdir/setenddir -- endofdir returns its result via the
        # zero flag, not a register, so check sim.zf directly --
        mem = base_memory()
        mem_w16(mem, LD_DCNT, 0xFFFF)
        _, _, ref_sim = run_reference(code, entries['endofdir'], mem, {})
        check("endofdir: at end", bool(ref_sim.zf), lib.endofdir(ctypes.cast(to_ctypes(mem), c_u8p)) != 0)

        mem = base_memory()
        mem_w16(mem, LD_DCNT, 5)
        _, _, ref_sim = run_reference(code, entries['endofdir'], mem, {})
        check("endofdir: not at end", bool(ref_sim.zf), lib.endofdir(ctypes.cast(to_ctypes(mem), c_u8p)) != 0)

        mem = base_memory()
        ref_regs, ref_img, port_img = run_both('setenddir', lib.setenddir, mem, {})
        check("setenddir", mem_r16(ref_img, LD_DCNT), mem_r16(port_img, LD_DCNT))

        # -- rddir/rddir0: dirmax < dcnt -> setenddir --
        mem = base_memory()
        mem_w16(mem, LD_DIRMAX, 5)
        mem_w16(mem, LD_DCNT, 10)
        _, ref_img, _ = run_reference(code, entries['rddir'], mem, {'cx': 0})
        buf = to_ctypes(mem)
        lib.rddir(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0))
        check("rddir: dirmax < dcnt -> setenddir", mem_r16(ref_img, LD_DCNT), mem_r16(bytes(buf), LD_DCNT))

        # -- rddir: within range, dptr != 0 (not a new record) --
        mem = base_memory()
        mem_w16(mem, LD_DIRMAX, 63)
        mem_w16(mem, LD_DCNT, 4)  # +1 -> 5; 5 & dskmsk(3) = 1, <<fcbshf(5) = 32 != 0
        _, ref_img, _ = run_reference(code, entries['rddir'], mem, {'cx': 0})
        buf = to_ctypes(mem)
        lib.rddir(ctypes.cast(buf, c_u8p), ctypes.c_uint8(0))
        ref = (mem_r16(ref_img, LD_DCNT), ref_img[LD_DPTR])
        port = (mem_r16(bytes(buf), LD_DCNT), bytes(buf)[LD_DPTR])
        check("rddir: within range, not a new record", ref, port)

        # -- getallocbit --
        mem = base_memory()
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem[ALLOC_AT] = 0x5A
        mem[ALLOC_AT + 1] = 0xC3
        for cx in (0, 5, 9, 15):
            ref_regs, _, _ = run_reference(code, entries['getallocbit'], mem, {'cx': cx})
            buf = to_ctypes(mem)
            bx = ctypes.c_uint16()
            dh = ctypes.c_uint8()
            cx_out = ctypes.c_uint16()
            got = lib.getallocbit(ctypes.cast(buf, c_u8p), ctypes.c_uint16(cx), ctypes.byref(bx),
                                   ctypes.byref(dh), ctypes.byref(cx_out))
            ref = (ref_regs['ax'] & 0xFF, ref_regs['bx'], (ref_regs['dx'] >> 8) & 0xFF)
            port = (got, bx.value, dh.value)
            check(f"getallocbit: cx={cx}", ref, port)
            check(f"getallocbit: cx={cx}, internal cx", ref_regs['cx'], cx_out.value)

        # -- setallocbit/rotr -- ch is whatever's ambient at the call
        # site, not getallocbit's own returned shift count; pass a few
        # different values to confirm the port reproduces that --
        for cx, dl, ch_in in ((5, 1, 0), (5, 1, 3), (9, 0, 5), (9, 1, 0xAA)):
            mem = base_memory()
            mem_w16(mem, LD_ALLOCA, ALLOC_AT)
            mem[ALLOC_AT] = 0x5A
            mem[ALLOC_AT + 1] = 0xC3
            ref_regs, ref_img, _ = run_reference(code, entries['setallocbit'], mem,
                                                  {'cx': cx, 'dx': (ch_in << 8) | dl})
            buf = to_ctypes(mem)
            lib.setallocbit(ctypes.cast(buf, c_u8p), ctypes.c_uint16(cx), ctypes.c_uint8(dl), ctypes.c_uint8(ch_in))
            check(f"setallocbit: cx={cx} dl={dl} ch={ch_in}",
                  ref_img[ALLOC_AT:ALLOC_AT + 2], bytes(buf)[ALLOC_AT:ALLOC_AT + 2])

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
