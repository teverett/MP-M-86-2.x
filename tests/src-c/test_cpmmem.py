#!/usr/bin/env python3
"""Differential test for src-c/kernel/cpmmem.c against src/kernel/
CPMMEM.MEM's maxmem_entry, absmax_entry, cpmalloc_entry, cpmabsa_entry,
cpmfree_entry (+ internal getmemory, free_memory). freeall_entry is not
tested -- see cpmmem.c's own header comment for why it's deferred, not
ported.

getmemory's own mpmif(f_malloc) and free_memory's own mpmif(f_memfree)
are both real calls now, into memory.c's malloc_entry/mfree_entry
(bundled here the same way test_memory.py's own memory_mem_wrapper.A86
bundles MAU.MEM/ML.MEM/UTIL.MEM) -- see cpmmem.c's own header comment.
Needs tools/i86sim.py's opt-in `segmented=True` mode now too, for the
same reason test_memory.py does (mfree_it's own real call into
maufree_entry reaches a genuinely dynamic MAU segment)."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'cpmmem_wrapper.A86')
CPMMEM_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'cpmmem.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'memory.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'util_mem.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'mau.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'ml.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR, SD_MDUL, SD_MAL, SD_MFL, SD_THRDRT = 0xA000, 0xA028, 0xA02A, 0xA02C, 0xA032
UDA_WRKSEG = 0xA012
MPMIF_STUB = 0x9000

MCB_BASE, MCB_LENGTH, MCB_EXT = 0, 2, 4
MS_LINK, MS_START, MS_LENGTH, MS_FLAGS, MS_MAU = 0, 2, 4, 6, 8
M_LINK = 0
P_MEM = 0x16
MF_LOAD = 0x01
F_MALLOC = 0x80
SYSDAT = 0x2000

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


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in (
        'maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *CPMMEM_C, '-o', lib_path],
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
    mem_w16(mem, SD_RLR, 0x8000)  # the running process's own PD
    mem_w16(mem, UDA_WRKSEG, SYSDAT)
    mem_w16(mem, SD_MDUL, 0)  # no unused MDs -- getmd fails with e_no_umd
    return mem


def run_reference(code, entry, mem, regs, sysdat_word=SYSDAT):
    image = bytearray(mem)
    image[0:len(code)] = code
    image[0] = sysdat_word & 0xFF  # `sysdat dw 0` sits at cseg offset 0
    image[1] = (sysdat_word >> 8) & 0xFF
    # mpmif is a real dispatcher now (see the wrapper's own comment) --
    # f_malloc/f_memfree both tail into the real MEMORY.MEM/MAU.MEM/
    # ML.MEM/UTIL.MEM.
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

        for name in ('maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry'):
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def call(name, mem, dx):
            buf = to_ctypes(mem)
            r = getattr(lib, name)(ctypes.cast(buf, c_u8p), ctypes.c_uint16(dx))
            return (r.bx, r.cx), bytes(buf)

        # -- every allocator entry now makes a real call into malloc_entry
        # (memory.c) via getmemory; with no unused MDs available, getmd
        # fails for real with e_no_umd (0x12) -- a genuinely different,
        # meaningful error code, not the old hardcoded f_malloc sentinel --
        for name, setup in (
            ('maxmem_entry', lambda mem, MCB: mem_w16(mem, MCB + MCB_LENGTH, 0x40)),
            ('absmax_entry', lambda mem, MCB: (mem_w16(mem, MCB + MCB_BASE, 0x1000), mem_w16(mem, MCB + MCB_LENGTH, 0x40))),
            ('cpmalloc_entry', lambda mem, MCB: mem_w16(mem, MCB + MCB_LENGTH, 0x40)),
            ('cpmabsa_entry', lambda mem, MCB: (mem_w16(mem, MCB + MCB_BASE, 0x1000), mem_w16(mem, MCB + MCB_LENGTH, 0x40))),
        ):
            mem = base_memory()
            MCB = 0x5000
            setup(mem, MCB)
            ref_regs, _ = run_reference(code, entries[name], mem, {'dx': MCB})
            port, _ = call(name, mem, MCB)
            check(f"{name}: real malloc_entry call, no unused MDs -> e_no_umd", (ref_regs['bx'], ref_regs['cx']), port)

        # -- cpmalloc_entry: one unused MD available, this PD's own
        # p_mem list empty (mall_ml path), MFL root itself empty too --
        # mlalloc_entry's own loop sees m_link[SD_MFL]==0 and fails for
        # real with e_no_memory, matching memory.c's own established
        # "mlalloc genuinely fails (empty MFL)" precedent (test_memory.py)
        # -- getmd's new_md is freed back via freemd on this path --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        FREE_MD = 0x4400
        mem_w16(mem, SD_MDUL, FREE_MD)
        mem_w16(mem, FREE_MD + MS_LINK, 0)  # one unused MD, nothing chained after
        mem_w16(mem, RLR + P_MEM + MS_LINK, 0)  # this PD owns no MAUs yet -> mall_ml
        mem_w16(mem, SD_MFL, 0)  # mlalloc_entry's own MFL list root, empty
        MCB = 0x5000
        mem_w16(mem, MCB + MCB_LENGTH, 0x10)
        ref_regs, ref_img = run_reference(code, entries['cpmalloc_entry'], mem, {'dx': MCB})
        port, port_img = call('cpmalloc_entry', mem, MCB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, SD_MDUL))
        got = (port, mem_r16(port_img, SD_MDUL))
        check("cpmalloc_entry: real mall_ml path, mlalloc genuinely fails, MD freed back", ref, got)

        # -- cpmfree_entry: mcb_ext != 0xFF -- tail-jumps straight into
        # free_memory, which now makes a real mfree_entry call; with no
        # p_mem list entries at all (this PD owns nothing), the real
        # walk fails for real with e_no_memory --
        mem = base_memory()
        MCB = 0x5000
        mem_w16(mem, MCB + MCB_BASE, 0x3000)
        mem[MCB + MCB_EXT] = 0x00
        ref_regs, _ = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB, 'bx': 0})
        port, _ = call('cpmfree_entry', mem, MCB)
        check("cpmfree_entry: single-segment free, real mfree_entry call, e_no_memory", (ref_regs['bx'], ref_regs['cx']), port)

        # -- cpmfree_entry: mcb_ext != 0xFF, real success -- a matching MD
        # on this PD's own p_mem list, its MAU still in use afterward --
        # exercises free_memory's own bx passthrough (no longer forced
        # to a fixed 0, see cpmmem.c's own header comment) --
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
        MCB = 0x5000
        mem_w16(mem, MCB + MCB_BASE, 0x300)
        mem[MCB + MCB_EXT] = 0x00
        ref_regs, ref_img = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB})
        port, port_img = call('cpmfree_entry', mem, MCB)
        ref = ((ref_regs['bx'], ref_regs['cx']), ref_img[phys(MAU_SEG) + 1 * SATLEN + SAT_NALL])
        got = (port, port_img[phys(MAU_SEG) + 1 * SATLEN + SAT_NALL])
        check("cpmfree_entry: single-segment free, real success, MAU still in use", ref, got)

        # -- cpmfree_entry: mcb_ext == 0xFF, empty p_mem list -- cpmf_next
        # immediately hits si==0 -> success, no segment visited --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        mem_w16(mem, RLR + P_MEM + MS_LINK, 0)
        MCB = 0x5000
        mem[MCB + MCB_EXT] = 0xFF
        ref_regs, _ = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB})
        port, _ = call('cpmfree_entry', mem, MCB)
        check("cpmfree_entry: free-all, empty p_mem list", (ref_regs['bx'], ref_regs['cx']), port)

        # -- cpmfree_entry: mcb_ext == 0xFF, one Load segment on the list --
        # try_seg's "and ms_flags[si],mf_load" clobbers ms_flags down to
        # just the mf_load bit, then skips it (mov bx,si) and moves on to
        # the next (empty) link -- success, but ms_flags visibly changed --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4400
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_FLAGS, MF_LOAD | 0x08)  # a Load segment, plus some other bit
        mem_w16(mem, MD + MS_LINK, 0)
        MCB = 0x5000
        mem[MCB + MCB_EXT] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB})
        port, port_img = call('cpmfree_entry', mem, MCB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, MD + MS_FLAGS))
        got = (port, mem_r16(port_img, MD + MS_FLAGS))
        check("cpmfree_entry: free-all skips a Load segment, clobbering its flags", ref, got)

        # -- cpmfree_entry: mcb_ext == 0xFF, one non-Load segment on the
        # list -- try_seg's own mask leaves ms_flags at 0 (not Load), so
        # free_seg runs for real: free_memory now genuinely succeeds
        # (the MD's own MAU has no other allocation, so it's released
        # to the MFL too, same mechanics as test_memory.py's own "MAU
        # now empty" case), cpmf_root restarts the walk, and the
        # now-empty p_mem list ends the loop with a real success --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4400
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_START, 0x300)
        mem_w16(mem, MD + MS_LENGTH, 0x10)
        mem_w16(mem, MD + MS_FLAGS, 0)  # not a Load segment
        mem_w16(mem, MD + MS_LINK, 0)
        mem_w16(mem, MD + MS_MAU, MAU_REC)
        mem_w16(mem, MAU_REC + MS_START, MAU_SEG)
        mem_w16(mem, MAU_REC + MS_LENGTH, 0x1000)
        put_sat(mem, MAU_SEG, 1, 0x300, 0x10, 1, ents=2)  # the MAU's only real entry
        mem_w16(mem, SD_MAL + M_LINK, MAU_REC)
        mem_w16(mem, MAU_REC + M_LINK, 0)
        mem_w16(mem, SD_MFL + M_LINK, 0)
        MCB = 0x5000
        mem[MCB + MCB_EXT] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB})
        port, port_img = call('cpmfree_entry', mem, MCB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, SD_MAL), mem_r16(ref_img, SD_MFL))
        got = (port, mem_r16(port_img, SD_MAL), mem_r16(port_img, SD_MFL))
        check("cpmfree_entry: free-all, one non-Load segment, real success, MAU released to MFL", ref, got)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
