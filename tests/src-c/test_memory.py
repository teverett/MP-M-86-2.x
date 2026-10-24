#!/usr/bin/env python3
"""Differential test for src-c/kernel/memory.c against src/kernel/
MEMORY.MEM's malloc_entry and mfree_entry (+ internal mall_try_mau,
mfree_it, both static in the C port and exercised only indirectly).

malloc_entry's own mall_try_mau/mall_ml and mfree_entry's own mfree_it
now all make real mpmif(f_maualloc)/mpmif(f_mlalloc)/mpmif(f_maufree)/
mpmif(f_mlfree) calls into the real MAU.MEM/ML.MEM (bundled into this
test's own wrapper, with mpmif itself a small real dispatcher -- see
the wrapper's own header comment and memory.c's own header comment).
Needs tools/i86sim.py's opt-in `segmented=True` mode (see test_mau.py's
own docstring) now that mfree_it's own real call into maufree_entry
reaches MAU.MEM's own dynamically-computed MAU segment -- confirmed
safe to enable for every case here, not just the new ones (plain
unprefixed accesses behave identically either way).
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'memory_mem_wrapper.A86')
MEMORY_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'memory.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'util_mem.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'mau.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'ml.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, SD_THRDRT, SD_MDUL, SD_MAL, SD_MFL = 0xA000, 0xA032, 0xA028, 0xA02A, 0xA02C
UDA_WRKSEG = 0xA012

MPB_START, MPB_MIN, MPB_MAX, MPB_PDADR, MPB_FLAGS = 0, 2, 4, 6, 8
MFPB_START, MFPB_PD = 0, 2
MS_LINK, MS_START, MS_LENGTH, MS_FLAGS, MS_MAU = 0, 2, 4, 6, 8
M_LINK = 0
P_THREAD = 2
P_MEM = 0x16
E_NO_UMD, E_ACTIVE_PD, E_NO_MEMORY = 0x12, 0x22, 0x03
F_MAUALLOC, F_MAUFREE, F_MLALLOC = 0x309, 0x30A, 0x30B

SAT_START, SAT_LENGTH, SAT_NALL, SATLEN = 0, 2, 4, 5
MAU_SEG = 0x0100
MAU_REC = 0x4600


def phys(seg):
    return (seg * 16) & 0xFFFF


def put_sat(mem, seg, entry_no, start, length, nall, ents=4):
    base = phys(seg)
    mem[base] = ents
    off = base + entry_no * SATLEN
    mem_w16(mem, off + SAT_START, start)
    mem_w16(mem, off + SAT_LENGTH, length)
    mem[off + SAT_NALL] = nall

SYSDAT = 0x2000


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in ('malloc_entry', 'mfree_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *MEMORY_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, SYSDAT)  # dx maps straight into "MPM range"
    mem_w16(mem, SD_RLR, 0x8000)  # the running process's own PD, by default
    return mem


def run_reference(code, entry, mem, regs, sysdat_word=SYSDAT):
    image = bytearray(mem)
    image[0:len(code)] = code
    image[0] = sysdat_word & 0xFF  # `sysdat dw 0` sits at cseg offset 0
    image[1] = (sysdat_word >> 8) & 0xFF
    # mpmif is a real dispatcher now (see the wrapper's own comment),
    # not a flat call_stub_addrs target -- f_maualloc/f_mlalloc tail
    # into the real MAU.MEM/ML.MEM, everything else just ret's.
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, segmented=True)
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.malloc_entry.restype = BxCx
        lib.malloc_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.mfree_entry.restype = BxCx
        lib.mfree_entry.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def call_malloc(mem, dx):
            buf = to_ctypes(mem)
            r = lib.malloc_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(dx))
            return (r.bx, r.cx), bytes(buf)

        def call_mfree(mem, dx):
            buf = to_ctypes(mem)
            r = lib.mfree_entry(ctypes.cast(buf, c_u8p), ctypes.c_uint16(dx))
            return (r.bx, r.cx), bytes(buf)

        # -- malloc_entry: MD free list empty -> e_no_umd propagates straight through --
        mem = base_memory()
        mem_w16(mem, SD_MDUL, 0)
        MPB = 0x5000
        mem_w16(mem, MPB + MPB_PDADR, 0)
        ref_regs, _ = run_reference(code, entries['malloc_entry'], mem, {'dx': MPB})
        port, _ = call_malloc(mem, MPB)
        check("malloc_entry: no unused MDs", (ref_regs['bx'], ref_regs['cx']), port)

        # -- malloc_entry: requested PD is on the thread list and active, but
        # isn't the caller -- e_active_pd (exercises the correctly-written
        # p_thread walk malloc_entry always had) --
        mem = base_memory()
        MD = 0x4400
        mem_w16(mem, SD_MDUL, MD)
        OTHER_PD = 0x8100
        mem_w16(mem, SD_THRDRT, OTHER_PD)
        mem_w16(mem, OTHER_PD + P_THREAD, 0)
        MPB = 0x5000
        mem_w16(mem, MPB + MPB_PDADR, OTHER_PD)
        ref_regs, _ = run_reference(code, entries['malloc_entry'], mem, {'dx': MPB})
        port, _ = call_malloc(mem, MPB)
        check("malloc_entry: active PD other than the caller", (ref_regs['bx'], ref_regs['cx']), port)

        # -- malloc_entry: PD has no MDs of its own yet -- falls straight to
        # mall_ml, whose mlalloc boundary stub always "fails" (cx=f_mlalloc) --
        mem = base_memory()
        MD = 0x4400
        mem_w16(mem, SD_MDUL, MD)
        mem_w16(mem, SD_THRDRT, 0)
        PD = 0x8000
        mem_w16(mem, PD + P_MEM + MS_LINK, 0)  # this PD's p_mem list is empty
        MPB = 0x5000
        mem_w16(mem, MPB + MPB_PDADR, PD)
        mem_w16(mem, MPB + MPB_START, 0x30)
        mem_w16(mem, MPB + MPB_MIN, 0x10)
        mem_w16(mem, MPB + MPB_FLAGS, 0x04)
        ref_regs, ref_img = run_reference(code, entries['malloc_entry'], mem, {'dx': MPB})
        port, port_img = call_malloc(mem, MPB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, SD_MDUL))
        got = (port, mem_r16(port_img, SD_MDUL))
        check("malloc_entry: new-MAU, mlalloc genuinely fails (empty MFL), MD freed back", ref, got)

        # -- mfree_entry: explicit pdadr equal to the caller's own rlr --
        # (mfree_chkpd's "bx==0" fast path is covered by the next two cases,
        # which leave MFPB_PD at 0; this one takes the PD-active-check branch
        # with bx==rlr, landing straight on mfree_gotpd without walking the
        # thread list at all)
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        mem_w16(mem, SD_THRDRT, 0)
        mem_w16(mem, RLR + P_MEM + MS_LINK, 0)  # empty p_mem list -> e_no_memory
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, RLR)
        mem_w16(mem, MFPB + MFPB_START, 0x30)
        ref_regs, _ = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, _ = call_mfree(mem, MFPB)
        check("mfree_entry: explicit pdadr == rlr, empty list -> e_no_memory", (ref_regs['bx'], ref_regs['cx']), port)

        # -- mfree_entry: active PD other than the caller -- e_active_pd.
        # This is the exact walk docs/src-c.md's own fix (p_link -> p_thread)
        # targets: before that fix this test would desync from the real
        # assembly (also just fixed) the moment OTHER_PD's thread-list entry
        # didn't happen to collide with p_link's offset by coincidence. --
        mem = base_memory()
        OTHER_PD = 0x8100
        mem_w16(mem, SD_THRDRT, OTHER_PD)
        mem_w16(mem, OTHER_PD + P_THREAD, 0)
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, OTHER_PD)
        mem_w16(mem, MFPB + MFPB_START, 0x30)
        ref_regs, _ = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, _ = call_mfree(mem, MFPB)
        check("mfree_entry: active PD other than the caller", (ref_regs['bx'], ref_regs['cx']), port)

        # -- mfree_entry: bx==0 (free from the calling process), no MD covers
        # the requested start paragraph -> e_no_memory --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4500
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_START, 0x40)
        mem_w16(mem, MD + MS_LENGTH, 0x10)
        mem_w16(mem, MD + MS_LINK, 0)
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, 0)
        mem_w16(mem, MFPB + MFPB_START, 0x30)  # below every MD's own range
        ref_regs, _ = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, _ = call_mfree(mem, MFPB)
        check("mfree_entry: no MD covers the target paragraph", (ref_regs['bx'], ref_regs['cx']), port)

        # -- mfree_entry: exact-start match -> tail-calls mfree_it, whose own
        # real maufree_entry call genuinely fails here (ms_mau names a
        # bogus, nonexistent MAU record -- maufree_entry's own "ents"
        # read off that garbage segment comes back 0, failing the very
        # first entry-count check) -- mfree_entry returns mfree_it's
        # result directly without checking cx (same as the real jmps);
        # the MD's own length is correctly left untouched on this
        # failure path --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4500
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_START, 0x30)
        mem_w16(mem, MD + MS_LENGTH, 0x10)
        mem_w16(mem, MD + MS_LINK, 0)
        mem_w16(mem, MD + MS_MAU, 0x9999)
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, 0)
        mem_w16(mem, MFPB + MFPB_START, 0x30)
        ref_regs, ref_img = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, port_img = call_mfree(mem, MFPB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, MD + MS_LENGTH))
        got = (port, mem_r16(port_img, MD + MS_LENGTH))
        check("mfree_entry: exact-start match, bogus MAU -> maufree_entry genuinely fails", ref, got)

        # -- mfree_entry: target falls strictly inside one MD's range (not
        # exact start) -- real maufree_entry call genuinely succeeds (a
        # real SAT split, via mau_split), so mfree_it's own decrease-
        # length path runs for real: ms_length[si] shrinks by
        # (target - start), independently of whatever mau_split did to
        # the MAU's own internal SAT bookkeeping --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4500
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_START, 0x30)
        mem_w16(mem, MD + MS_LENGTH, 0x20)
        mem_w16(mem, MD + MS_LINK, 0)
        mem_w16(mem, MD + MS_MAU, MAU_REC)
        mem_w16(mem, MAU_REC + MS_START, MAU_SEG)
        mem_w16(mem, MAU_REC + MS_LENGTH, 0x1000)
        put_sat(mem, MAU_SEG, 1, 0x30, 0x20, 1)  # allocated, covers [0x30, 0x50)
        put_sat(mem, MAU_SEG, 2, 0x50, 0x10, 1)  # sentinel/boundary entry
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, 0)
        mem_w16(mem, MFPB + MFPB_START, 0x38)  # inside [0x30, 0x50), not exact start
        ref_regs, ref_img = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, port_img = call_mfree(mem, MFPB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, MD + MS_LENGTH))
        got = (port, mem_r16(port_img, MD + MS_LENGTH))
        check("mfree_entry: target inside a larger MD, real maufree_entry split, length decreased", ref, got)

        # -- mfree_entry: exact-start match, real maufree_entry call --
        # MAU still has another allocated SAT entry, so mau_collapse's
        # own scan finds it and reports "not empty" (mfree_exit) --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4500
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_START, 0x300)
        mem_w16(mem, MD + MS_LENGTH, 0x10)
        mem_w16(mem, MD + MS_LINK, 0)
        mem_w16(mem, MD + MS_MAU, MAU_REC)
        mem_w16(mem, MAU_REC + MS_START, MAU_SEG)
        mem_w16(mem, MAU_REC + MS_LENGTH, 0x1000)
        put_sat(mem, MAU_SEG, 1, 0x300, 0x10, 1)  # the target, currently allocated
        put_sat(mem, MAU_SEG, 2, 0x310, 0x10, 1)  # a neighbor that stays allocated
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, 0)
        mem_w16(mem, MFPB + MFPB_START, 0x300)
        ref_regs, ref_img = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, port_img = call_mfree(mem, MFPB)
        ref = ((ref_regs['bx'], ref_regs['cx']), ref_img[phys(MAU_SEG) + 1 * SATLEN + SAT_NALL])
        got = (port, port_img[phys(MAU_SEG) + 1 * SATLEN + SAT_NALL])
        check("mfree_entry: real maufree_entry call, MAU still in use", ref, got)

        # -- mfree_entry: exact-start match, real maufree_entry call --
        # the freed SAT is the MAU's only real entry, so mau_collapse
        # reports "empty" -- the MAU is then unlinked from the active
        # MAU list (SD_MAL) and handed back to the Memory Free List
        # (SD_MFL) via a real mlfree_entry call --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4500
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_START, 0x300)
        mem_w16(mem, MD + MS_LENGTH, 0x10)
        mem_w16(mem, MD + MS_LINK, 0)
        mem_w16(mem, MD + MS_MAU, MAU_REC)
        mem_w16(mem, MAU_REC + MS_START, MAU_SEG)
        mem_w16(mem, MAU_REC + MS_LENGTH, 0x1000)
        put_sat(mem, MAU_SEG, 1, 0x300, 0x10, 1, ents=2)  # the MAU's only real entry
        mem_w16(mem, SD_MAL + M_LINK, MAU_REC)
        mem_w16(mem, MAU_REC + M_LINK, 0)
        mem_w16(mem, SD_MFL + M_LINK, 0)
        MFPB = 0x5100
        mem_w16(mem, MFPB + MFPB_PD, 0)
        mem_w16(mem, MFPB + MFPB_START, 0x300)
        ref_regs, ref_img = run_reference(code, entries['mfree_entry'], mem, {'dx': MFPB})
        port, port_img = call_mfree(mem, MFPB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, SD_MAL), mem_r16(ref_img, SD_MFL))
        got = (port, mem_r16(port_img, SD_MAL), mem_r16(port_img, SD_MFL))
        check("mfree_entry: real maufree_entry call, MAU now empty, released to MFL", ref, got)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
