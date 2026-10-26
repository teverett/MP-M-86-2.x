#!/usr/bin/env python3
"""Differential test for src-c/kernel/memif.c's memif_entry (MEMIF.MEM's
inter-module MEM dispatch, wrapped in its binary-semaphore acquire/
release) against the real MEM module, on real memory layout.

The reference is src/kernel/MEM.A86 itself; see tests/src-c/memref.py.
The SUPIF hook re-enters `entry` for the targets' own nested mpmif calls
(recursively, as the real system does -- the caller already owns the
mutex by then), and treats `entry`'s f_qread/f_qwrite (acquire/release
of mxmemqpb) and f_terminate (pending ctl-C) as no-op boundary stubs.
So every one of the 13 functions is checked end to end -- result, the
mutex state (`mxmemowner`/`mxmemcount`), the caller's pf_tempkeep, and
the whole image -- and the reference's boundary calls are checked
against what memif.c's header says the C side models as no-ops.
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, PD_RUN, build_lib, c_buffer, phys, sd, wk, w16, r16, SYSDAT

u16 = ctypes.c_uint16
MD_FREE1, MD_FREE2 = 0x2400, 0x2410
MS1, MAU1, MFL_MD = 0x2500, 0x2600, 0x2700
MAU_SEG = 0x0900
PB = 0x0100        # the parameter block (MCB/MPB/MFPB/SPB/MAF) in u_wrkseg


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib(MEM_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.memif_entry.restype = BxCx
        lib.memif_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), u16, ctypes.c_uint8, u16, u16]
        ck = Checker(ref)
        SATLEN = s['satlen']
        F_QREAD, F_QWRITE, F_TERMINATE = 137, 139, 143

        def f(addr, field):
            return sd(addr + s[field])

        def base_memory(owner=0, count=0, pflag=0):
            """the caller (PD_RUN) with one MAU (MAU1 on mal, one
            allocated segment MS1) and a free MFL region"""
            mem = ref.base_memory()
            w16(mem, sd(s['mxmemowner']), owner)
            mem[sd(s['mxmemcount'])] = count
            w16(mem, f(PD_RUN, 'p_flag'), pflag)
            w16(mem, sd(s['thrdrt']), PD_RUN)
            w16(mem, sd(s['mdul']), MD_FREE1)
            w16(mem, f(MD_FREE1, 'm_link'), MD_FREE2)
            w16(mem, sd(s['mfl']), MFL_MD)
            w16(mem, f(MFL_MD, 'm_start'), 0x0A00)
            w16(mem, f(MFL_MD, 'm_length'), 0x40)
            w16(mem, sd(s['mal']), MAU1)
            w16(mem, f(MAU1, 'm_start'), MAU_SEG)
            w16(mem, f(MAU1, 'm_length'), 0x100)
            mem[phys(MAU_SEG)] = 4
            for n, (start, length, nall) in enumerate(((0x0910, 0x10, 1), (0x0920, 0x40, 0)), 1):
                w16(mem, phys(MAU_SEG, n * SATLEN + s['sat_start']), start)
                w16(mem, phys(MAU_SEG, n * SATLEN + s['sat_length']), length)
                mem[phys(MAU_SEG, n * SATLEN + s['sat_nall'])] = nall
            w16(mem, f(PD_RUN, 'p_mem'), MS1)
            for fld, v in (('ms_start', 0x0910), ('ms_length', 0x10), ('ms_mau', MAU1)):
                w16(mem, f(MS1, fld), v)
            return mem

        def pb(mem, *words):
            for i, v in enumerate(words):
                w16(mem, wk(PB + 2 * i), v)
            return mem

        state = lambda i: (r16(i, sd(s['mxmemowner'])), i[sd(s['mxmemcount'])],  # noqa: E731
                           r16(i, f(PD_RUN, 'p_flag')), r16(i, sd(s['mdul'])), r16(i, sd(s['mal'])),
                           r16(i, sd(s['mfl'])), r16(i, f(PD_RUN, 'p_mem')))

        def run(label, cl, mem, dx=PB, bx=0, boundary=None):
            regs, img = ref.run('entry', mem, {'cx': cl, 'dx': dx, 'bx': bx}, stop_addrs={ref.entry_retf})
            calls = [c for c in ref.calls if c not in ref.funcs]
            buf = c_buffer(mem)
            r = lib.memif_entry(buf, SYSDAT, cl, dx, bx)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx), state)
            if boundary is not None:
                ck.check(f"{label}: reference boundary calls", boundary, calls)

        acquire_release = [F_QREAD, F_QWRITE]

        # every function, entered without the mutex: acquire, run, release
        run("0 maxmem_entry", 0, pb(base_memory(), 0, 0x10, 0), boundary=acquire_release)
        run("1 absmax_entry", 1, pb(base_memory(), 0x0A12, 0x10, 0), boundary=acquire_release)
        run("2 cpmalloc_entry", 2, pb(base_memory(), 0, 0x10, 0), boundary=acquire_release)
        run("3 cpmabsa_entry", 3, pb(base_memory(), 0x0A12, 0x10, 0), boundary=acquire_release)
        run("4 cpmfree_entry (one segment)", 4, pb(base_memory(), 0x0910, 0, 0), boundary=acquire_release)
        run("5 freeall_entry", 5, base_memory(), boundary=[F_QREAD, F_TERMINATE, F_QWRITE])
        run("6 malloc_entry", 6, pb(base_memory(), 0, 8, 8, 0, 0), boundary=acquire_release)
        run("7 mfree_entry", 7, pb(base_memory(), 0x0910, 0), boundary=acquire_release)
        run("8 share_entry (with self)", 8, pb(base_memory(), 0, 0, 0x0910), boundary=acquire_release)
        run("9 maualloc_entry", 9, pb(base_memory(), 0, 8, 8, 0, 0), bx=MAU1, boundary=acquire_release)
        run("10 maufree_entry", 10, pb(base_memory(), MAU1, 0x0910, 0x0910), boundary=acquire_release)
        run("11 mlalloc_entry", 11, pb(base_memory(), 0, 8, 8, 0, 0), bx=s['mfl'] - s['m_link'],
            boundary=acquire_release)
        run("12 mlfree_entry", 12, base_memory(), dx=MAU1, bx=s['mfl'] - s['m_link'], boundary=acquire_release)

        # already the owner (a nested call): no acquire/release at all
        run("6 malloc_entry (already owner)", 6, pb(base_memory(PD_RUN, 1), 0, 8, 8, 0, 0), boundary=[])
        # pf_tempkeep was already set: release leaves it set
        run("2 cpmalloc_entry (pf_tempkeep already set)", 2,
            pb(base_memory(pflag=s['pf_tempkeep']), 0, 0x10, 0), boundary=acquire_release)
        # ctl-C arrived meanwhile: release, then f_terminate
        run("2 cpmalloc_entry (ctl-C pending)", 2, pb(base_memory(pflag=s['pf_ctlc']), 0, 0x10, 0),
            boundary=[F_QREAD, F_QWRITE, F_TERMINATE])
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
