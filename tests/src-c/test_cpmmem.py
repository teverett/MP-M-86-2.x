#!/usr/bin/env python3
"""Differential test for src-c/kernel/cpmmem.c (maxmem_entry,
absmax_entry, cpmalloc_entry, cpmabsa_entry, cpmfree_entry + internal
getmemory/free_memory) against src-assem/kernel/CPMMEM.MEM, on real memory
layout.

The reference is the whole real MEM module (src-assem/kernel/MEM.A86); see
tests/src-c/memref.py. getmemory/free_memory build their MPB/MFPB on
the stack (SS = the running UDA) and call `mpmif` with DS = SS; the
SUPIF hook turns that into u_wrkseg = the UDA and re-enters MEM's real
`entry`, so malloc/mfree run for real on both sides. The MCB is in the
running process's u_wrkseg. Every case diffs the whole image (except
the UDA's stack area, where both sides keep those blocks).
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, PD_RUN, UDA, build_lib, c_buffer, phys, sd, wk, w16, r16, SYSDAT

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
MD_FREE1, MD_FREE2 = 0x2400, 0x2410
MS1, MS2 = 0x2500, 0x2510
MAU1, MFL_MD = 0x2600, 0x2700
MAU_SEG = 0x0900
MCB = 0x0100
MCB_BASE, MCB_LENGTH, MCB_EXT = 0, 2, 4


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib([p for p in MEM_C if p.endswith(('cpmmem.c', 'memory.c', 'mau.c', 'ml.c', 'util_mem.c'))])
    try:
        lib = ctypes.CDLL(lib_path)
        names = ('maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry')
        for name in names:
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)
        SATLEN = s['satlen']

        def f(addr, field):
            return sd(addr + s[field])

        def base_memory(mfl=True):
            mem = ref.base_memory()
            w16(mem, sd(s['thrdrt']), PD_RUN)
            w16(mem, sd(s['mdul']), MD_FREE1)
            w16(mem, f(MD_FREE1, 'm_link'), MD_FREE2)
            if mfl:
                w16(mem, sd(s['mfl']), MFL_MD)
                w16(mem, f(MFL_MD, 'm_start'), 0x0A00)
                w16(mem, f(MFL_MD, 'm_length'), 0x40)
            return mem

        def put_mcb(mem, base, length, ext=0):
            w16(mem, wk(MCB + MCB_BASE), base)
            w16(mem, wk(MCB + MCB_LENGTH), length)
            mem[wk(MCB + MCB_EXT)] = ext

        def owned(mem, segs, sats, ents=4):
            """the caller's p_mem: segs = [(ms, start, length, flags)] on MAU1"""
            w16(mem, sd(s['mal']), MAU1)
            w16(mem, f(MAU1, 'm_start'), MAU_SEG)
            w16(mem, f(MAU1, 'm_length'), 0x100)
            mem[phys(MAU_SEG)] = ents
            for n, (start, length, nall) in enumerate(sats, 1):
                w16(mem, phys(MAU_SEG, n * SATLEN + s['sat_start']), start)
                w16(mem, phys(MAU_SEG, n * SATLEN + s['sat_length']), length)
                mem[phys(MAU_SEG, n * SATLEN + s['sat_nall'])] = nall
            w16(mem, f(PD_RUN, 'p_mem'), segs[0][0])
            for i, (ms, start, length, flags) in enumerate(segs):
                for fld, v in (('ms_link', segs[i + 1][0] if i + 1 < len(segs) else 0), ('ms_start', start),
                               ('ms_length', length), ('ms_flags', flags), ('ms_mau', MAU1)):
                    w16(mem, f(ms, fld), v)

        def run(label, entry, mem, snap=lambda img: ()):
            regs, img = ref.run(entry, mem, {'dx': MCB})
            buf = c_buffer(mem)
            r = getattr(lib, entry)(buf, SYSDAT, MCB)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx),
                       lambda i: (bytes(i[wk(MCB):wk(MCB + 5)]),) + snap(i))
            # u_wrkseg is back to the caller's after the inner mpmif calls
            ck.check(f"{label}: u_wrkseg restored", r16(img, phys(UDA, s['u_wrkseg'])),
                     r16(bytes(buf), phys(UDA, s['u_wrkseg'])))

        lists = lambda i: (r16(i, sd(s['mdul'])), r16(i, sd(s['mal'])), r16(i, sd(s['mfl'])),  # noqa: E731
                           r16(i, f(PD_RUN, 'p_mem')))

        # -- the allocators: each goes through getmemory -> malloc_entry --
        for entry, base, length in (('maxmem_entry', 0, 0x10), ('absmax_entry', 0x0A12, 0x10),
                                    ('cpmalloc_entry', 0, 0x10), ('cpmabsa_entry', 0x0A12, 0x10)):
            mem = base_memory()
            put_mcb(mem, base, length)
            run(f"{entry}: new MAU off the MFL, success", entry, mem, lists)
        mem = base_memory(mfl=False)
        put_mcb(mem, 0, 0x10)
        run("cpmalloc_entry: empty MFL -> fails, MD freed back", 'cpmalloc_entry', mem, lists)
        mem = base_memory()
        put_mcb(mem, 0, 0x80)
        run("maxmem_entry: more than the MFL holds -> gets what is there", 'maxmem_entry', mem, lists)

        # -- cpmfree_entry: one segment (mcb_ext != 0FFh) --
        mem = base_memory()
        put_mcb(mem, 0x0910, 0)
        run("cpmfree_entry: single-segment free, nothing owned -> e_no_memory", 'cpmfree_entry', mem)

        mem = base_memory()
        owned(mem, [(MS1, 0x0910, 0x10, 0), (MS2, 0x0930, 0x20, 0)],
              [(0x0910, 0x10, 1), (0x0920, 0x10, 0), (0x0930, 0x20, 1)])
        put_mcb(mem, 0x0910, 0)
        run("cpmfree_entry: single-segment free, MAU still in use", 'cpmfree_entry', mem, lists)

        # -- cpmfree_entry: free all but Load memory (mcb_ext == 0FFh) --
        mem = base_memory()
        put_mcb(mem, 0, 0, 0xFF)
        run("cpmfree_entry: free-all, empty p_mem list", 'cpmfree_entry', mem)

        mem = base_memory()
        owned(mem, [(MS1, 0x0910, 0x10, s['mf_load'] | s['mf_code'])], [(0x0910, 0x10, 1)], ents=2)
        put_mcb(mem, 0, 0, 0xFF)
        run("cpmfree_entry: free-all keeps a Load segment and its flags (bug #107)", 'cpmfree_entry', mem,
            lambda i: (lists(i), r16(i, f(MS1, 'ms_flags'))))

        mem = base_memory()
        owned(mem, [(MS1, 0x0910, 0x10, s['mf_load']), (MS2, 0x0930, 0x20, s['mf_code'])],
              [(0x0910, 0x10, 1), (0x0930, 0x20, 1)])
        put_mcb(mem, 0, 0, 0xFF)
        run("cpmfree_entry: free-all frees the non-Load segment, keeps the MAU", 'cpmfree_entry', mem,
            lambda i: (lists(i), r16(i, f(MS1, 'ms_link'))))

        # MS1's SAT is missing, so freeing it fails: the walk must step
        # past it and still free MS2 (it used to retry MS1 forever, bug #107)
        mem = base_memory()
        owned(mem, [(MS1, 0x0910, 0x10, 0), (MS2, 0x0930, 0x20, 0)],
              [(0x0920, 0x10, 1), (0x0930, 0x20, 1)])
        put_mcb(mem, 0, 0, 0xFF)
        run("cpmfree_entry: free-all skips a segment that fails to free (bug #107)", 'cpmfree_entry', mem,
            lambda i: (lists(i), r16(i, f(MS1, 'ms_link'))))

        mem = base_memory(mfl=False)
        owned(mem, [(MS1, 0x0910, 0x10, 0)], [(0x0910, 0x10, 1)], ents=2)
        put_mcb(mem, 0, 0, 0xFF)
        run("cpmfree_entry: free-all, last segment -> MAU released to the MFL", 'cpmfree_entry', mem, lists)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
