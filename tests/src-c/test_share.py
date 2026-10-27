#!/usr/bin/env python3
"""Differential test for src-c/kernel/share.c (share_entry) against
src-assem/kernel/SHARE.MEM, on real memory layout.

The reference is the whole real MEM module (src-assem/kernel/MEM.A86); see
tests/src-c/memref.py. PDs, their p_mem MS lists and the MD pool are
SYSDAT-relative, the SPB is in the running process's u_wrkseg, and the
SAT scan is in the shared MAU's own segment. Every case diffs the
whole image, which also catches the MD leaks docs/bugs.md #105 fixed
(an unfreed MD stays off `mdul`).
"""
import ctypes
import os
import sys

from memref import BxCx, Checker, MemRef, MEM_C, PD_RUN, build_lib, c_buffer, phys, sd, wk, w16, r16, SYSDAT

u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
OWNER, REQ = 0x2100, 0x2200          # PDs (PD_RUN is the caller)
OWNER_MS, REQ_MS, NEW_MD = 0x2400, 0x2410, 0x2420
MAU_MD = 0x2500
MAU_SEG = 0x0900
SPB = 0x0100
START = 0x0A00


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib([p for p in MEM_C if p.endswith(('share.c', 'util_mem.c'))])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.share_entry.restype = BxCx
        lib.share_entry.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)
        SATLEN = s['satlen']

        def f(addr, field):
            return sd(addr + s[field])

        def base_memory(opd=OWNER, rpd=REQ, sat_start=START):
            mem = ref.base_memory()
            w16(mem, sd(s['mdul']), NEW_MD)
            w16(mem, wk(SPB + s['spb_opd']), opd)
            w16(mem, wk(SPB + s['spb_rpd']), rpd)
            w16(mem, wk(SPB + s['spb_start']), START)
            owner = PD_RUN if opd == 0 else opd
            w16(mem, f(owner, 'p_mem'), OWNER_MS)
            w16(mem, f(OWNER_MS, 'ms_start'), START)
            w16(mem, f(OWNER_MS, 'ms_length'), 0x20)
            w16(mem, f(OWNER_MS, 'ms_flags'), s['mf_share'] | s['mf_code'])
            w16(mem, f(OWNER_MS, 'ms_mau'), MAU_MD)
            w16(mem, f(MAU_MD, 'm_start'), MAU_SEG)
            mem[phys(MAU_SEG)] = 3
            w16(mem, phys(MAU_SEG, SATLEN + s['sat_start']), 0x0990)
            w16(mem, phys(MAU_SEG, 2 * SATLEN + s['sat_start']), sat_start)
            mem[phys(MAU_SEG, 2 * SATLEN + s['sat_nall'])] = 1
            return mem

        def run(label, mem, snap=lambda img: ()):
            regs, img = ref.run('share_entry', mem, {'dx': SPB})
            buf = c_buffer(mem)
            r = lib.share_entry(buf, SYSDAT, SPB)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx),
                       lambda i: (r16(i, sd(s['mdul'])),) + snap(i))

        req_list = lambda i: (r16(i, f(REQ, 'p_mem')), r16(i, f(NEW_MD, 'ms_link')),  # noqa: E731
                              bytes(i[sd(NEW_MD):sd(NEW_MD + 10)]),
                              i[phys(MAU_SEG, 2 * SATLEN + s['sat_nall'])])

        run("share_entry: basic share onto an empty p_mem list", base_memory(), req_list)

        # requestor already has an MS on the same MAU: the new one goes in
        # right before it
        mem = base_memory()
        w16(mem, f(REQ, 'p_mem'), 0x2430)
        w16(mem, f(0x2430, 'ms_link'), REQ_MS)
        w16(mem, f(0x2430, 'ms_mau'), 0x2999)
        w16(mem, f(REQ_MS, 'ms_mau'), MAU_MD)
        run("share_entry: inserted next to an existing same-MAU entry", mem,
            lambda i: (r16(i, f(0x2430, 'ms_link')),) + req_list(i))

        # owner 0 defaults to rlr
        run("share_entry: owner defaults to rlr", base_memory(opd=0), req_list)

        # requestor 0 defaults to rlr: owner == requestor, nothing to do --
        # the new MD goes straight back on mdul (docs/bugs.md #105)
        run("share_entry: sharing with self -> success, MD freed", base_memory(opd=0, rpd=0))

        # owner has no MS at the requested start -> e_no_memory, MD freed
        # (docs/bugs.md #105)
        mem = base_memory()
        w16(mem, f(OWNER_MS, 'ms_start'), 0x0B00)
        run("share_entry: no matching owner MS -> e_no_memory, MD freed", mem)

        # owner MS found, but the MAU's SAT has no such entry
        run("share_entry: no matching SAT entry -> e_no_memory, MD freed", base_memory(sat_start=0x0A80))

        # no free MD at all
        mem = base_memory()
        w16(mem, sd(s['mdul']), 0)
        run("share_entry: no free MD -> e_no_umd", mem)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
