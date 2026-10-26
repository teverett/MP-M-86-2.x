#!/usr/bin/env python3
"""Differential test for src-c/kernel/load.c (LOAD.SUP's load/cload_entry,
diskread, load_group and the load_finish tail) against the real system,
on real memory layout -- see tests/src-c/supref.py.

The reference is the real SUP with the real RTM, MEM and BDOS loaded
beside it, so load's own `mpmif` calls run the real code: f_qread/f_qwrite
on the MXLoad queue (RTM), f_malloc/f_memfree/f_freemem (MEM, re-entering
the SUP and itself as the real system does), f_setdma/f_setdmab (BDOS
func26/51, setting the running UDA's u_dma_*). f_freadseq, a read through
the BDOS's unported getdiskmx path, is a boundary: the reference's handler
and load.c's `load_set_readseq` hook read the same scripted file records
into the UDA's DMA address. Every case compares bx/cx and diffs the
whole image (code and the UDA's stack area aside).
"""
import ctypes
import os
import sys

from supref import (SysRef, Checker, MOD_NUM, PD_RUN, SYSDAT, UDA, WRKSEG, BxCx, build_lib, c_buffer, phys, sd,
                    wk, w16, r16, KERNEL_C)

LOAD_C = [os.path.join(KERNEL_C, f) for f in
          ('load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c', 'que1.c', 'que2.c',
           'file1.c', 'file2.c', 'file3.c', 'file4.c')]
u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
FCB = 0x0100                 # the open FCB, in u_wrkseg
NEW_PD = 0x2200              # a PD to initialize
MD_FREE = (0x2400, 0x2410, 0x2420, 0x2430, 0x2440)
MFL_MD = 0x2600
CALLER_SEGS = (0x0B00, 0x0B40)    # memory the caller owns (chain load)
MFL_SEG = 0x0C00             # the Memory Free List's one region
READSEQ_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, c_u8p, u16, u16, u16, u16)


class Disk:
    """A file of 128-byte records, read sequentially through the FCB's
    nxtrec -- the same model on both sides."""
    def __init__(self, records, fail=False):
        self.records, self.fail = records, fail

    def read(self, getb, setb, fcb, dma):
        if self.fail:
            return 0xFF
        nxt = getb(fcb + 32)
        if nxt >= len(self.records):
            return 1
        for i, b in enumerate(self.records[nxt]):
            setb(dma + i, b)
        setb(fcb + 32, nxt + 1)
        return 0


def cmd_header(entries):
    rec = bytearray(128)
    for i, (form, length, base, mn, mx) in enumerate(entries):
        rec[i * 9] = form
        for j, val in enumerate((length, base, mn, mx)):
            w16(rec, i * 9 + 1 + 2 * j, val)
    return bytes(rec)


def main():
    ref = SysRef(('SUP', 'RTM', 'MEM', 'BDOS'))
    v = ref.v
    user_retf = v('user_retf')
    lib_path = build_lib(LOAD_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.load_set_readseq.argtypes = [READSEQ_FN]
        lib.diskread.restype = BxCx
        lib.diskread.argtypes = [c_u8p, u16, u16, u16]
        lib.load_group.restype = u16
        lib.load_group.argtypes = [c_u8p, u16, u16]
        lib.load_finish.restype = BxCx
        lib.load_finish.argtypes = [c_u8p, u16, u16, u16]
        lib.load.restype = BxCx
        lib.load.argtypes = [c_u8p, u16, u16, u16, u16, u16, ctypes.c_int]
        lib.cload_entry.restype = BxCx
        lib.cload_entry.argtypes = [c_u8p, u16, u16, u16, u16, u16]
        ck = Checker(ref)
        LDT = v('ldtab')
        LDTABLEN = 17
        f = lambda addr, name: sd(addr + v(name))  # noqa: E731

        def base_memory():
            mem = ref.base_memory()
            w16(mem, sd(v('mxmemowner')), PD_RUN)      # MEM's mutex: already ours
            mem[sd(v('mxmemcount'))] = 1
            # MXLoad: its QD on qlr with its one token
            qd = v('mxloadqd')
            w16(mem, sd(v('qlr')), qd)
            w16(mem, sd(qd + v('q_link')), 0)
            w16(mem, sd(qd + v('q_msgcnt')), 1)
            # free MDs, one Memory Free List region
            w16(mem, sd(v('mdul')), MD_FREE[0])
            for i, md in enumerate(MD_FREE):
                w16(mem, sd(md + v('m_link')), MD_FREE[i + 1] if i + 1 < len(MD_FREE) else 0)
            w16(mem, sd(v('mfl')), MFL_MD)
            w16(mem, sd(MFL_MD + v('m_start')), MFL_SEG)
            w16(mem, sd(MFL_MD + v('m_length')), 0x0300)
            w16(mem, sd(v('thrdrt')), PD_RUN)
            return mem

        def run(label, entry, mem, regs, call, disk, result=lambda r: (r['bx'], r['cx']),
                port=lambda r: (r.bx, r.cx), snap=lambda i: (), stop=()):
            def handler(sim):
                getb = lambda a: sim.mem[a]  # noqa: E731
                setb = lambda a, b: sim.mem.__setitem__(a, b)  # noqa: E731
                fcb = phys(r16(sim.mem, phys(UDA, v('u_wrkseg'))), sim.regs['dx'])
                dma = phys(r16(sim.mem, phys(UDA, v('u_dma_seg'))), r16(sim.mem, phys(UDA, v('u_dma_ofst'))))
                sim.regs['ax'] = sim.regs['bx'] = disk.read(getb, setb, fcb, dma)
            ref.handlers = {(MOD_NUM['BDOS'], 7): handler}
            ref_regs, img = ref.run(entry, mem, regs, stop_addrs=stop)
            buf = c_buffer(mem)

            def c_read(p, fseg, foff, dseg, doff):
                return disk.read(lambda a: p[a], lambda a, b: p.__setitem__(a, b),
                                 phys(fseg, foff), phys(dseg, doff))
            hook = READSEQ_FN(c_read)
            lib.load_set_readseq(hook)
            got = call(buf)
            ck.compare(label, img, buf, result(ref_regs), port(got), snap)

        def fcb_memory(nxtrec=0):
            mem = base_memory()
            mem[wk(FCB + 32)] = nxtrec
            w16(mem, sd(v('load_fcb')), FCB)
            return mem

        recs = [bytes([0x11] * 128), bytes([0x22] * 128), bytes([0x33] * 128)]

        # -- diskread: dx = dma offset, cx = dma segment --
        for label, nxt, disk, pf in (("diskread: record 0", 0, Disk(recs), 0),
                                     ("diskread: record 1", 1, Disk(recs), 0),
                                     ("diskread: end of file", 3, Disk(recs), 0),
                                     ("diskread: read error -> e_bad_read", 0, Disk(recs, fail=True), 0),
                                     ("diskread: pf_ctlc -> e_abort", 0, Disk(recs), v('pf_ctlc'))):
            mem = fcb_memory(nxt)
            w16(mem, f(PD_RUN, 'p_flag'), pf)
            run(label, 'diskread', mem, {'dx': 0x10, 'cx': 0x0D00},
                lambda b: lib.diskread(b, SYSDAT, 0x10, 0x0D00), disk,
                snap=lambda i: (bytes(i[phys(0x0D00, 0x10):phys(0x0D00, 0x90)]),
                                r16(i, phys(UDA, v('u_dma_seg'))), r16(i, phys(UDA, v('u_dma_ofst')))))

        # -- load_group: si = an ldtab entry --
        def put_ldtab(mem, n, start, flen, fstrt, mn=0, mx=0, type_=1):
            e = sd(LDT + n * LDTABLEN)
            for name, val in (('ldtab_start', start), ('ldtab_flen', flen), ('ldtab_fstrt', fstrt),
                              ('ldtab_min', mn), ('ldtab_max', mx)):
                w16(mem, e + v(name), val)
            mem[e + v('ldtab_type')] = type_

        for label, flen, fstrt, indma, preload in (
                ("load_group: one full record, straight to memory", 8, 0, 0x100, False),
                ("load_group: two full records", 16, 0, 0x100, False),
                ("load_group: a partial record, via load_dma", 3, 0, 0x100, False),
                ("load_group: already in load_dma, no read", 3, 2, 0, True)):
            mem = fcb_memory(0)
            put_ldtab(mem, 0, 0x0D00, flen, fstrt)
            w16(mem, sd(v('load_indma')), indma)
            if preload:
                mem[sd(v('load_dma')):sd(v('load_dma')) + 128] = bytes(range(128))
            run(label, 'load_group', mem, {'si': LDT}, lambda b: lib.load_group(b, SYSDAT, LDT), Disk(recs),
                result=lambda r: (r['cx'],), port=lambda r: (r,),
                snap=lambda i: bytes(i[phys(0x0D00):phys(0x0D00, 0x100)]))

        # -- load_finish (lb_more onward): si = ldtab, cx = load_nldtabents --
        def finish(label, groups, pd=0, fcb_dr=0):
            mem = fcb_memory()
            for n, (start, mn, type_) in enumerate(groups):
                put_ldtab(mem, n, start, 0, 0, mn, mn, type_)
            w16(mem, sd(v('load_nldtabents')), len(groups))
            w16(mem, sd(v('load_pd')), pd)
            mem[wk(FCB)] = fcb_dr
            mem[f(NEW_PD, 'p_user')] = 3
            mem[f(NEW_PD, 'p_dsk')] = 2
            stops = {v('load_giveup'), v('load_exit')}
            # an error exit is `jmp load_giveup`, where the reference stops
            # (before giveup sets bx): compare cx there
            run(label, 'lb_more', mem, {'si': LDT, 'cx': len(groups)},
                lambda b: lib.load_finish(b, SYSDAT, user_retf, 0), Disk(recs), stop=stops,
                result=lambda r: (r['bx'], 0) if r['cx'] == 0 else ('giveup', r['cx']),
                port=lambda r: (r.bx, 0) if r.cx == 0 else ('giveup', r.cx))

        uda_grp = (0x0D00, 0x16, 0)
        finish("load_finish: data-group .CMD, PD initialized", [uda_grp, (0x0E00, 8, 1), (0x0E80, 0x10, 2)], NEW_PD)
        finish("load_finish: FCB disk letter overrides p_ldsk", [uda_grp, (0x0E00, 8, 1), (0x0E80, 0x10, 2)], NEW_PD,
               fcb_dr=5)
        finish("load_finish: 8080-model (code only)", [uda_grp, (0x0E00, 8, 1)], NEW_PD)
        finish("load_finish: no PD -> base page only", [(0x0E80, 0x10, 2)])
        finish("load_finish: no code/data group -> e_no_cseg", [(0x0E80, 0x10, 3)])
        finish("load_finish: data group without a code group -> e_no_cseg", [uda_grp, (0x0E80, 0x10, 2)], NEW_PD)

        # -- load, end to end --
        def full(label, header_entries, content, pd=0, chain=False, mem=None, disk=None):
            mem = mem or fcb_memory(5)   # load resets nxtrec itself
            disk = disk or Disk([cmd_header(header_entries)] + content)
            snap = lambda i: (r16(i, sd(v('mdul'))), r16(i, sd(v('mal'))), r16(i, sd(v('mfl'))),  # noqa: E731
                              r16(i, f(PD_RUN, 'p_mem')), r16(i, f(NEW_PD, 'p_mem')),
                              r16(i, sd(v('mxloadqd') + v('q_msgcnt'))))
            if chain:
                run(label, 'cload_entry', mem, {'dx': FCB, 'bx': pd},
                    lambda b: lib.cload_entry(b, SYSDAT, FCB, pd, user_retf, 0), disk, snap=snap)
            else:
                run(label, 'load', mem, {'dx': FCB, 'bx': pd},
                    lambda b: lib.load(b, SYSDAT, FCB, pd, user_retf, 0, 0), disk, snap=snap)

        full("load: one relative code group (8080 model), no PD", [(1, 8, 0, 8, 8)], [bytes([0xCC] * 128)])
        full("load: code + data groups, PD initialized", [(1, 8, 0, 8, 8), (2, 8, 0, 0x10, 0x10)],
             [bytes([0xCC] * 128), bytes([0xDD] * 128)], pd=NEW_PD)
        full("load: not enough memory -> e_no_memory, nothing kept", [(1, 8, 0, 0x400, 0x400)], [bytes(128)])
        full("load: header read fails -> e_bad_read", [], [], disk=Disk([], fail=True))
        full("load: no code group -> e_no_cseg, memory released", [(3, 8, 0, 8, 8)], [bytes(128)])

        # chain load: the caller owns two segments; the new PD keeps its UDA
        mem = fcb_memory(5)
        w16(mem, f(NEW_PD, 'p_uda'), CALLER_SEGS[0])
        MS1, MS2, MAU1 = 0x2700, 0x2710, 0x2800
        w16(mem, f(PD_RUN, 'p_mem'), MS1)
        for ms, link, seg in ((MS1, MS2, CALLER_SEGS[0]), (MS2, 0, CALLER_SEGS[1])):
            for name, val in (('ms_link', link), ('ms_start', seg), ('ms_length', 0x40),
                              ('ms_flags', v('mf_load')), ('ms_mau', MAU1)):
                w16(mem, f(ms, name), val)
        w16(mem, sd(v('mal')), MAU1)
        w16(mem, f(MAU1, 'm_start'), 0x0AF0)
        w16(mem, f(MAU1, 'm_length'), 0x100)
        mem[phys(0x0AF0)] = 4
        for n, (start, length, nall) in enumerate(((CALLER_SEGS[0], 0x40, 1), (CALLER_SEGS[1], 0x40, 1)), 1):
            e = phys(0x0AF0, n * 5)
            w16(mem, e, start)
            w16(mem, e + 2, length)
            mem[e + 4] = nall
        full("cload_entry: chain load frees the caller's memory, keeps the UDA",
             [(1, 8, 0, 8, 8)], [bytes([0xCC] * 128)], pd=NEW_PD, chain=True, mem=mem)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
