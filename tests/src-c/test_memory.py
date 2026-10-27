#!/usr/bin/env python3
"""Differential test for src-c/kernel/memory.c (malloc_entry +
mall_try_mau, mfree_entry + mfree_it) against src-assem/kernel/MEMORY.MEM, on
real memory layout.

The reference is the whole real MEM module (src-assem/kernel/MEM.A86); see
tests/src-c/memref.py. Its `mpmif` calls (f_maualloc/f_mlalloc/
f_maufree/f_mlfree) re-enter MEM's real `entry` through the SUPIF hook,
so allocation/free run through the real MAU.MEM/ML.MEM end to end on
both sides. PDs, p_mem lists, MDs, `mal`/`mfl` are SYSDAT-relative; the
MPB/MFPB is in the running process's u_wrkseg; mfree_it's on-stack MAF
lands in the running UDA's stack area (SS = the UDA). Every case diffs
the whole image.
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, PD_RUN, build_lib, c_buffer, phys, sd, wk, w16, r16, SYSDAT

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
OTHER = 0x2100               # another PD on the thread list
MD_FREE1, MD_FREE2, MD_FREE3 = 0x2400, 0x2410, 0x2420   # unused MDs
MS1, MS2 = 0x2500, 0x2510    # the caller's memory segments
MAU1 = 0x2600                # an existing MAU's MD
MFL_MD = 0x2700              # one Memory Free List region
MAU_SEG = 0x0900
MPB, MFPB = 0x0100, 0x0180


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib([p for p in MEM_C if p.endswith(('memory.c', 'mau.c', 'ml.c', 'util_mem.c'))])
    try:
        lib = ctypes.CDLL(lib_path)
        for name in ('malloc_entry', 'mfree_entry'):
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)
        SATLEN = s['satlen']

        def f(addr, field):
            return sd(addr + s[field])

        def base_memory(mdul=(MD_FREE1, MD_FREE2, MD_FREE3)):
            mem = ref.base_memory()
            w16(mem, sd(s['thrdrt']), PD_RUN)
            w16(mem, f(PD_RUN, 'p_thread'), OTHER)
            w16(mem, sd(s['mdul']), mdul[0] if mdul else 0)
            for i, md in enumerate(mdul):
                w16(mem, f(md, 'm_link'), mdul[i + 1] if i + 1 < len(mdul) else 0)
            return mem

        def put_mfl(mem, start=0x0A00, length=0x100):
            w16(mem, sd(s['mfl']), MFL_MD)
            w16(mem, f(MFL_MD, 'm_start'), start)
            w16(mem, f(MFL_MD, 'm_length'), length)

        def put_mau(mem, sats, ents=4):
            """MAU1 on `mal`, its SAT in MAU_SEG: sats = [(start, length, nall)]"""
            w16(mem, sd(s['mal']), MAU1)
            w16(mem, f(MAU1, 'm_start'), MAU_SEG)
            w16(mem, f(MAU1, 'm_length'), 0x100)
            mem[phys(MAU_SEG)] = ents
            for n, (start, length, nall) in enumerate(sats, 1):
                w16(mem, phys(MAU_SEG, n * SATLEN + s['sat_start']), start)
                w16(mem, phys(MAU_SEG, n * SATLEN + s['sat_length']), length)
                mem[phys(MAU_SEG, n * SATLEN + s['sat_nall'])] = nall

        def put_ms(mem, ms, link, start, length, mau=MAU1, flags=0):
            for fld, v in (('ms_link', link), ('ms_start', start), ('ms_length', length),
                           ('ms_flags', flags), ('ms_mau', mau)):
                w16(mem, f(ms, fld), v)

        def put_mpb(mem, start, mn, mx, pdadr=0, flags=0):
            for fld, v in (('mpb_start', start), ('mpb_min', mn), ('mpb_max', mx),
                           ('mpb_pdadr', pdadr), ('mpb_flags', flags)):
                w16(mem, wk(MPB + s[fld]), v)

        def put_mfpb(mem, start, pd=0):
            w16(mem, wk(MFPB + s['mfpb_start']), start)
            w16(mem, wk(MFPB + s['mfpb_pd']), pd)

        def run(label, entry, mem, dx, snap=lambda img: ()):
            regs, img = ref.run(entry, mem, {'dx': dx})
            buf = c_buffer(mem)
            r = getattr(lib, entry)(buf, SYSDAT, dx)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx), snap)

        lists = lambda i: (r16(i, sd(s['mdul'])), r16(i, sd(s['mal'])), r16(i, sd(s['mfl'])),  # noqa: E731
                           r16(i, f(PD_RUN, 'p_mem')))
        mpb = lambda i: tuple(r16(i, wk(MPB + s[x])) for x in ('mpb_start', 'mpb_min', 'mpb_pdadr'))  # noqa: E731

        # -- malloc_entry --
        mem = base_memory(mdul=())
        put_mpb(mem, 0, 4, 8)
        run("malloc_entry: no unused MDs -> e_no_umd", 'malloc_entry', mem, MPB)

        mem = base_memory()
        put_mpb(mem, 0, 4, 8, pdadr=OTHER)
        run("malloc_entry: active PD other than the caller -> e_active_pd", 'malloc_entry', mem, MPB)

        mem = base_memory()
        put_mpb(mem, 0, 4, 8)
        run("malloc_entry: no MAU, empty MFL -> fails, MD freed back", 'malloc_entry', mem, MPB,
            lambda i: (lists(i), mpb(i)))

        mem = base_memory()
        put_mfl(mem)
        put_mpb(mem, 0, 4, 8, flags=s['mf_code'])
        run("malloc_entry: new MAU off the MFL, then allocated from it", 'malloc_entry', mem, MPB,
            lambda i: (lists(i), mpb(i), bytes(i[f(MD_FREE1, 'ms_link'):f(MD_FREE1, 'ms_link') + 10]),
                       bytes(i[phys(0x0A00):phys(0x0A00, 16)])))

        # the caller already uses MAU1, which has room: allocate there, and
        # link the new MS right after the one that pointed at MAU1
        mem = base_memory()
        put_mau(mem, [(0x0910, 0x10, 1), (0x0920, 0x40, 0)])
        w16(mem, f(PD_RUN, 'p_mem'), MS1)
        put_ms(mem, MS1, 0, 0x0910, 0x10)
        put_mpb(mem, 0, 8, 8, pdadr=PD_RUN)
        run("malloc_entry: allocated from a MAU the PD already uses", 'malloc_entry', mem, MPB,
            lambda i: (lists(i), mpb(i), r16(i, f(MS1, 'ms_link')),
                       bytes(i[phys(MAU_SEG):phys(MAU_SEG, 4 * SATLEN)])))

        # -- mfree_entry --
        mem = base_memory()
        put_mfpb(mem, 0x0910, pd=PD_RUN)
        run("mfree_entry: explicit pdadr == rlr, empty list -> e_no_memory", 'mfree_entry', mem, MFPB)

        mem = base_memory()
        put_mfpb(mem, 0x0910, pd=OTHER)
        run("mfree_entry: active PD other than the caller -> e_active_pd", 'mfree_entry', mem, MFPB)

        def owned(sats, ents=4):
            mem = base_memory()
            put_mau(mem, sats, ents)
            w16(mem, f(PD_RUN, 'p_mem'), MS1)
            put_ms(mem, MS1, MS2, 0x0910, 0x10)
            put_ms(mem, MS2, 0, 0x0930, 0x20)
            return mem

        mem = owned([(0x0910, 0x10, 1), (0x0920, 0x10, 0), (0x0930, 0x20, 1)])
        put_mfpb(mem, 0x0905)
        run("mfree_entry: no MD covers the target paragraph -> e_no_memory", 'mfree_entry', mem, MFPB)

        mem = owned([(0x0910, 0x10, 1), (0x0920, 0x10, 0), (0x0930, 0x20, 1)])
        put_mfpb(mem, 0x0938)
        run("mfree_entry: target inside an MS -> SAT split, length decreased", 'mfree_entry', mem, MFPB,
            lambda i: (lists(i), r16(i, f(MS2, 'ms_length')), bytes(i[phys(MAU_SEG):phys(MAU_SEG, 5 * SATLEN)])))

        mem = owned([(0x0910, 0x10, 1), (0x0920, 0x10, 0), (0x0930, 0x20, 1)])
        put_mfpb(mem, 0x0910)
        run("mfree_entry: exact start, MAU still in use", 'mfree_entry', mem, MFPB,
            lambda i: (lists(i), r16(i, f(MS1, 'ms_link')), bytes(i[phys(MAU_SEG):phys(MAU_SEG, 5 * SATLEN)])))

        # the freed SAT was the MAU's last allocation: MAU off `mal`, back
        # to the MFL
        mem = base_memory()
        put_mau(mem, [(0x0910, 0x10, 1)], ents=2)
        w16(mem, f(PD_RUN, 'p_mem'), MS1)
        put_ms(mem, MS1, 0, 0x0910, 0x10)
        put_mfpb(mem, 0x0910)
        run("mfree_entry: exact start, MAU now empty -> released to the MFL", 'mfree_entry', mem, MFPB,
            lambda i: (lists(i), r16(i, f(MAU1, 'm_link'))))

        # docs/bugs.md #106: freeing the first of two allocated segments
        # must leave the MAU on `mal` (it used to be released while the
        # second was still allocated)
        mem = owned([(0x0910, 0x10, 1), (0x0930, 0x20, 1)])
        put_mfpb(mem, 0x0910)
        run("mfree_entry: last SAT still allocated -> MAU kept (bug #106)", 'mfree_entry', mem, MFPB, lists)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
