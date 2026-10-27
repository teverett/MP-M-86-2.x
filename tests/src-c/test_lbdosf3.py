#!/usr/bin/env python3
"""Differential test for src-c/loader/lbdosf3.c against src-assem/loader/
LBDOSF3.A86. See lbdosf3.c's own header comment for exactly what's
ported (including the borrowed LBDOSENT.A86 parsave33/parunsave) and
why slect's own CALL selerror branch is deferred.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'lbdosf3_wrapper.A86')
LBDOSF3_C = [os.path.join(REPO_ROOT, 'src-c', 'loader', f) for f in
             ('lbdosf3.c', 'lbdosf2.c', 'lbdosf1.c', 'lbdosio.c')]
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
LD_DLOG, LD_RESEL, LD_FCBDSK, LD_OLDDSK, LD_LINFO = 0xB051, 0xB053, 0xB054, 0xB055, 0xB056
LD_PARCOPFL, LD_PARLG, LD_INFOSAVE, LD_LOC_PAR_AREA = 0xB057, 0xB058, 0xB059, 0xB05B
LD_ARET = 0xB004

FCB_EXTNUM, FCB_MODNUM, FCB_RECCNT, FCB_DSKMAP, FCB_NXTREC = 12, 14, 15, 16, 32

INFO_AT = 0x4000
BUFFA_AT = 0x4100
ALLOC_AT = 0x4400

STUBS = {0x1b, 0x18, 0x27, 0x2a, 0x1e, 0x30, 0x21, 0x33, 0x24}
DIOCOMP_ADDR = None  # set in main(), once the reference is assembled
RET4_ADDR = None  # likewise -- selectdisk's own shared early-return point


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src-assem', 'loader')])
    code = asm.segment_bytes('cseg')
    names = ('sqread', 'diskread', 'diskeof', 'slect', 'curselect', 'reselect',
             'func13', 'parsave33', 'parunsave', 'func15', 'func20', 'func26', 'goback',
             'diocomp', 'ret4', 'setdata')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *LBDOSF3_C, '-o', lib_path],
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
    # seek's own real asm ends in `jmp setsecf`, a tail JMP (not a near
    # CALL), so call_stub_addrs can't intercept it -- it coincidentally
    # overlaps real code elsewhere in this combined bundle (same class
    # of coincidence as lbdosf1_wrapper.A86's own BIOS-table comment),
    # and in THIS bundle's own byte layout that landing spot doesn't
    # happen to fall through to a clean RET on its own. Since setsecf
    # really is expected to return normally to seek's own caller, this
    # treats a JMP landing on any BIOS stub address the same way
    # call_stub_addrs treats a CALL to one: pop the return address
    # (still on the stack from whoever originally called `seek`) and
    # resume there, rather than executing whatever real bytes happen
    # to sit at that coincidental address.
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    # diocomp's own "or al,al" branches on readf/writef's real result,
    # an undefined artifact under the boundary-stub convention (see
    # src-c/loader/lbdosf1.c's own header comment) -- fixed at 0
    # (success) there, same as every other port in this file. Nothing
    # zeroes AL automatically on a stubbed CALL (it's simply left
    # unchanged), so landing on diocomp itself forces it here instead.
    # selectdisk's own early "jz ret4" (bx==0 under file1.c's own
    # convention) leaves ZF=1 (from its own "cmp bx,0"), which would
    # make slect's own "JNZ L_67" always fall through to the still-
    # deferred selerror if replayed mechanically -- but "what flags an
    # undefined-artifact convention leaves behind for a DIFFERENT
    # caller to observe" is itself just as undefined as the original
    # artifact, so this forces ZF=0 here instead, same reasoning as
    # every other undefined-artifact choice in this project: pick the
    # value that keeps the routine's own real job (disk selection)
    # reachable, see src-c/loader/lbdosf3.c's own header comment.
    halt_addrs = set(stop_addrs) | STUBS | {DIOCOMP_ADDR, RET4_ADDR}
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=halt_addrs,
                 call_stub_addrs=STUBS)
    for _ in range(1000):
        sim.run()
        if sim.ip in STUBS:
            ret_addr = image[sim.regs['sp']] | (image[(sim.regs['sp'] + 1) & 0xFFFF] << 8)
            sim.regs['sp'] = (sim.regs['sp'] + 2) & 0xFFFF
            sim.ip = ret_addr
            sim.halted = False
        elif sim.ip == DIOCOMP_ADDR:
            sim.regs['ax'] = sim.regs['ax'] & 0xFF00
            sim.halted = False
            sim.step()
        elif sim.ip == RET4_ADDR:
            sim.zf = 0
            sim.halted = False
            sim.step()
        else:
            break
    return sim.regs, image, sim


def main():
    global DIOCOMP_ADDR, RET4_ADDR
    code, entries = assemble_reference()
    DIOCOMP_ADDR = entries['diocomp']
    RET4_ADDR = entries['ret4']
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.diskread.argtypes = [c_u8p]
        lib.sqread.argtypes = [c_u8p]
        lib.slect.argtypes = [c_u8p]
        lib.curselect.argtypes = [c_u8p]
        lib.reselect.argtypes = [c_u8p]
        lib.func13.argtypes = [c_u8p]
        lib.parsave33.argtypes = [c_u8p]
        lib.parunsave.argtypes = [c_u8p]
        lib.func15.argtypes = [c_u8p]
        lib.func20.argtypes = [c_u8p]
        lib.func26.argtypes = [c_u8p, ctypes.c_uint16]
        lib.goback.restype = ctypes.c_uint16
        lib.goback.argtypes = [c_u8p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def run_both(name, cfunc, mem, regs, cargs=(), stop_addrs=()):
            ref_regs, ref_img, ref_sim = run_reference(code, entries[name], mem, regs, stop_addrs)
            buf = to_ctypes(mem)
            cfunc(ctypes.cast(buf, c_u8p), *cargs)
            return ref_regs, ref_img, bytes(buf), ref_sim

        # -- diskread: enough records in extent, block allocated --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[INFO_AT + FCB_NXTREC] = 5
        mem[INFO_AT + FCB_RECCNT] = 10
        mem[LD_BLKSHF] = 3
        mem[LD_EXTVAL] = 0
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_DSKMAP] = 7  # block 7 (nonzero -> allocated)
        ref_regs, ref_img, port_img, _ = run_both('diskread', lib.diskread, mem, {})
        check("diskread: record ok, block allocated", ref_img[LD_ARET], port_img[LD_ARET])

        # -- diskread: block not allocated -> diskeof --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[INFO_AT + FCB_NXTREC] = 5
        mem[INFO_AT + FCB_RECCNT] = 10
        mem[LD_BLKSHF] = 3
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_DSKMAP] = 0  # unallocated
        ref_regs, ref_img, port_img, _ = run_both('diskread', lib.diskread, mem, {})
        check("diskread: block not allocated -> diskeof", ref_img[LD_ARET], port_img[LD_ARET])

        # -- sqread --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[INFO_AT + FCB_NXTREC] = 5
        mem[INFO_AT + FCB_RECCNT] = 10
        mem[LD_BLKSHF] = 3
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_DSKMAP] = 7
        ref_regs, ref_img, port_img, _ = run_both('sqread', lib.sqread, mem, {})
        check("sqread", (ref_img[LD_SEQIO], ref_img[LD_ARET]), (port_img[LD_SEQIO], port_img[LD_ARET]))

        # -- slect: not yet logged in (bit clear) -> setcdisk + initialize --
        mem = base_memory()
        mem[LD_CURDSK] = 2
        mem_w16(mem, LD_DLOG, 0x0000)
        mem_w16(mem, LD_MAXALL, 20)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DIRBLK, 0)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[LD_SINGLE] = 1
        mem[LD_USRCODE] = 0
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        for i in range(4):
            mem[BUFFA_AT + i * 32] = 0xE5
        ref_regs, ref_img, port_img, _ = run_both('slect', lib.slect, mem, {})
        check("slect: not logged in -> setcdisk+initialize",
              mem_r16(ref_img, LD_DLOG), mem_r16(port_img, LD_DLOG))

        # -- slect: already logged in (bit set) -> no-op --
        mem = base_memory()
        mem[LD_CURDSK] = 0
        mem_w16(mem, LD_DLOG, 0x0001)  # bit 0 set
        ref_regs, ref_img, port_img, _ = run_both('slect', lib.slect, mem, {})
        check("slect: already logged in, no-op",
              mem_r16(ref_img, LD_DLOG), mem_r16(port_img, LD_DLOG))

        # -- curselect: already logged in (bit 5, matching linfo=5
        # after rotation), avoids re-exercising initialize --
        mem = base_memory()
        mem[LD_LINFO] = 5
        mem_w16(mem, LD_DLOG, 0x0020)
        ref_regs, ref_img, port_img, _ = run_both('curselect', lib.curselect, mem, {})
        check("curselect", ref_img[LD_CURDSK], port_img[LD_CURDSK])

        # -- reselect: auto-select drive --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT] = 0x03  # drive code 3 -> auto-select (al&0x1f=3, dec->2, <30)
        mem[LD_CURDSK] = 7
        mem[LD_USRCODE] = 0
        mem_w16(mem, LD_DLOG, 0x0004)  # bit 2 set -> already logged in on drive 2, avoids re-exercising initialize
        ref_regs, ref_img, port_img, _ = run_both('reselect', lib.reselect, mem, {})
        ref = (ref_img[LD_OLDDSK], ref_img[LD_FCBDSK], ref_img[INFO_AT])
        port = (port_img[LD_OLDDSK], port_img[LD_FCBDSK], port_img[INFO_AT])
        check("reselect: auto-select", ref, port)

        # -- reselect: no auto-select (drive code 0 -> normalized 255) --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT] = 0x00
        mem[LD_USRCODE] = 0x02
        ref_regs, ref_img, port_img, _ = run_both('reselect', lib.reselect, mem, {})
        check("reselect: no auto-select", ref_img[INFO_AT], port_img[INFO_AT])

        # -- func13 --
        mem = base_memory()
        mem_w16(mem, LD_RODSK, 0x00FF)
        mem_w16(mem, LD_DLOG, 0x00FF)
        mem[LD_CURDSK] = 5
        mem_w16(mem, LD_MAXALL, 20)
        mem_w16(mem, LD_ALLOCA, ALLOC_AT)
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_CDRMAXA, 0x4200)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[LD_SINGLE] = 1
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        for i in range(4):
            mem[BUFFA_AT + i * 32] = 0xE5
        ref_regs, ref_img, port_img, _ = run_both('func13', lib.func13, mem, {})
        ref = (ref_img[LD_CURDSK], mem_r16(ref_img, LD_RODSK), mem_r16(ref_img, LD_DLOG))
        port = (port_img[LD_CURDSK], mem_r16(port_img, LD_RODSK), mem_r16(port_img, LD_DLOG))
        check("func13", ref, port)

        # -- parsave33/parunsave --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        for i in range(33):
            mem[INFO_AT + i] = (i * 3) & 0xFF
        ref_regs, ref_img, port_img, _ = run_both('parsave33', lib.parsave33, mem, {'cx': 0})
        ref = (mem_r16(ref_img, LD_INFO), ref_img[LD_LOC_PAR_AREA:LD_LOC_PAR_AREA + 33])
        port = (mem_r16(port_img, LD_INFO), port_img[LD_LOC_PAR_AREA:LD_LOC_PAR_AREA + 33])
        check("parsave33", ref, port)

        mem2 = bytearray(port_img)
        buf2 = to_ctypes(mem2)
        lib.parunsave(ctypes.cast(buf2, c_u8p))
        ref_regs2, ref_img2, _ = run_reference(code, entries['parunsave'], bytearray(ref_img), {})
        check("parunsave", mem_r16(ref_img2, LD_INFO), mem_r16(bytes(buf2), LD_INFO))

        # -- func15: open file --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT] = 0x00  # drive code 0 -> no auto-select
        mem[INFO_AT + 1:INFO_AT + 12] = b'TESTFILE TX'
        mem[INFO_AT + 12] = ord('T')
        mem[LD_USRCODE] = 0
        mem_w16(mem, LD_DIRMAX, 3)
        mem_w16(mem, LD_DCNT, 0xFFFF)
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem_w16(mem, LD_BUFFA, BUFFA_AT)
        entry_bytes = bytearray(32)
        entry_bytes[0:12] = b'TESTFILE TXT'
        entry_bytes[FCB_RECCNT] = 7
        mem[BUFFA_AT:BUFFA_AT + 32] = entry_bytes
        for i in range(1, 4):
            mem[BUFFA_AT + i * 32] = 0xE5
        ref_regs, ref_img, port_img, _ = run_both('func15', lib.func15, mem, {'cx': 0})
        check("func15: open file", ref_img[INFO_AT:INFO_AT + 32], port_img[INFO_AT:INFO_AT + 32])

        # -- func20: read a file --
        mem = base_memory()
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[INFO_AT] = 0x00
        mem[LD_USRCODE] = 0
        mem_w16(mem, LD_SECTPT, 9)
        mem_w16(mem, LD_CURTRKA, 0x4300)
        mem[INFO_AT + FCB_NXTREC] = 5
        mem[INFO_AT + FCB_RECCNT] = 10
        mem[LD_BLKSHF] = 3
        mem[LD_SINGLE] = 1
        mem[INFO_AT + FCB_DSKMAP] = 7
        ref_regs, ref_img, port_img, _ = run_both('func20', lib.func20, mem, {'cx': 0})
        check("func20: read a file", ref_img[LD_ARET], port_img[LD_ARET])

        # -- func26 -- tail-jumps into setdata (no observable effect,
        # see lbdosf1.c's own comment); stop there rather than letting
        # the reference fall through setdata's own further tail jump
        # into setdmf with no real caller left on the stack to return
        # to (this test's own entry point, unlike every other test
        # here, has no nested call chain above it) --
        mem = base_memory()
        ref_regs, ref_img, port_img, _ = run_both(
            'func26', lib.func26, mem, {'dx': 0x1234}, cargs=(ctypes.c_uint16(0x1234),),
            stop_addrs={entries['setdata']})
        check("func26", mem_r16(ref_img, LD_DMAAD), mem_r16(port_img, LD_DMAAD))

        # -- goback: resel set, fcbdsk nonzero -> restore + curselect --
        mem = base_memory()
        mem[LD_RESEL] = 0xFF
        mem_w16(mem, LD_INFO, INFO_AT)
        mem[LD_FCBDSK] = 0x03
        mem[LD_OLDDSK] = 4
        mem_w16(mem, LD_DLOG, 0x0010)  # bit 4 set -> already logged in on drive 4, avoids re-exercising initialize
        mem[LD_PARCOPFL] = 0
        mem_w16(mem, LD_ARET, 0x1234)
        ref_regs, ref_img, port_img, ref_sim = run_both('goback', lib.goback, mem, {})
        got = lib.goback(ctypes.cast(to_ctypes(mem), c_u8p))
        ref = (ref_img[INFO_AT], ref_img[LD_CURDSK])
        port = (port_img[INFO_AT], port_img[LD_CURDSK])
        check("goback: resel + fcbdsk -> curselect", ref, port)

        # -- goback: parcopfl set -> parunsave --
        mem = base_memory()
        mem[LD_RESEL] = 0
        mem[LD_PARCOPFL] = 0xFF
        mem[LD_PARLG] = 5
        mem_w16(mem, LD_INFOSAVE, INFO_AT)
        for i in range(5):
            mem[LD_LOC_PAR_AREA + i] = i + 1
        mem_w16(mem, LD_ARET, 0x5678)
        ref_regs, ref_img, port_img, _ = run_both('goback', lib.goback, mem, {})
        ref = (mem_r16(ref_img, LD_INFO), ref_img[INFO_AT:INFO_AT + 5])
        port = (mem_r16(port_img, LD_INFO), port_img[INFO_AT:INFO_AT + 5])
        check("goback: parcopfl -> parunsave", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
