#!/usr/bin/env python3
"""Differential test for representative src-c/kernel/file1-3.c routines
against the real BDOS module with SYSDAT (the BDOS's DS) at a nonzero
segment -- see tests/src-c/bdosref.py. test_file1-3.py keep SYSDAT at
segment 0; this test is what shows the port addresses everything through
the BDOS's DS rather than assuming DS = 0, and that the four
cross-segment copies use their real segments:

- dirtouser: directory record (DS:buffa) -> the caller's DMA at
  parametersegment:dmaad (the asm's `mov es,parametersegment`)
- parsave33: the caller's FCB at parametersegment:info -> DS:loc_par_area
- copy_dma_in/copy_dma_out: DMABASE:dmaad <-> DS:common_dma

It also exercises the FCB/allocation-vector helpers through DATA.BDO's
own pointers (info, alloca) and `search` over a real directory buffer --
code that sits in DATA.BDO's dseg/cseg collision range in the flat
layout test_file1-3.py use, so it had no test before. Every case diffs
the whole image.
"""
import ctypes
import os
import sys

from bdosref import (BdosRef, Checker, BDOS_C, PD_RUN, SYSDAT, USER, build_lib, c_buffer, ds, us, w16, r16,
                     u8, u16, c_u8p, c_u16p)

BUFFA, FCB, ALLOC, CHECKV, DPB = 0x1400, 0x1500, 0x1600, 0x1700, 0x1800
DMA, INFO = 0x0080, 0x0050          # in the caller's segment (USER)


def main():
    ref = BdosRef()
    v = ref.v
    lib_path = build_lib(BDOS_C)
    try:
        lib = ctypes.CDLL(lib_path)
        for name in ('dirtouser', 'parsave33', 'getfcb', 'setfcb'):
            getattr(lib, name).argtypes = [c_u8p, u16]
        for name in ('copy_dma_in', 'copy_dma_out'):
            getattr(lib, name).argtypes = [c_u8p, u16, u8]
        lib.getallocbit.restype = u8
        lib.getallocbit.argtypes = [c_u8p, u16, u16, c_u16p, ctypes.POINTER(ctypes.c_uint8)]
        lib.setallocbit.argtypes = [c_u8p, u16, u16, u8]
        lib.search.restype = u8
        lib.search.argtypes = [c_u8p, u16, u8]
        ck = Checker(ref)
        bd = lambda name: ds(v(name))  # noqa: E731

        def base():
            mem = ref.base_memory()
            w16(mem, bd('buffa'), BUFFA)
            w16(mem, bd('alloca'), ALLOC)
            w16(mem, bd('parametersegment'), USER)
            w16(mem, bd('dmabase'), USER)
            w16(mem, bd('dmaad'), DMA)
            return mem

        def run(label, entry, mem, regs=None, call=None, result=lambda regs: (), snap=lambda img: ()):
            ref_regs, img = ref.run(entry, mem, regs or {})
            buf = c_buffer(mem)
            got = call(buf)
            ck.compare(label, img, buf, result(ref_regs), got, snap)

        # -- the cross-segment copies --
        mem = base()
        mem[ds(BUFFA):ds(BUFFA + 128)] = bytes(range(128))
        run("dirtouser: DS:buffa -> parametersegment:dmaad", 'dirtouser', mem,
            call=lambda b: (lib.dirtouser(b, SYSDAT), ())[1], snap=lambda i: bytes(i[us(DMA):us(DMA + 128)]))

        mem = base()
        w16(mem, bd('info'), INFO)
        mem[us(INFO):us(INFO + 33)] = b'\x01FILENAMETYP' + bytes(range(1, 22))
        run("parsave33: parametersegment:info -> DS:loc_par_area", 'parsave33', mem,
            call=lambda b: (lib.parsave33(b, SYSDAT), ())[1],
            snap=lambda i: (r16(i, bd('info')), bytes(i[bd('loc_par_area'):bd('loc_par_area') + 33])))

        mem = base()
        mem[us(DMA):us(DMA + 16)] = b'0123456789ABCDEF'
        run("copy_dma_in: DMABASE:dmaad -> DS:common_dma", 'copy_dma_in', mem, {'cx': 16},
            call=lambda b: (lib.copy_dma_in(b, SYSDAT, 16), ())[1],
            snap=lambda i: bytes(i[bd('common_dma'):bd('common_dma') + 16]))

        mem = base()
        mem[bd('common_dma'):bd('common_dma') + 16] = b'fedcba9876543210'
        run("copy_dma_out: DS:common_dma -> DMABASE:dmaad", 'copy_dma_out', mem, {'cx': 16},
            call=lambda b: (lib.copy_dma_out(b, SYSDAT, 16), ())[1],
            snap=lambda i: bytes(i[us(DMA):us(DMA + 16)]))

        # -- FCB fields through DATA.BDO's info pointer --
        mem = base()
        w16(mem, bd('info'), FCB)
        mem[ds(FCB + 12)] = 5       # fcb_extnum
        mem[ds(FCB + 15)] = 0x22    # fcb_reccnt
        mem[ds(FCB + 32)] = 7       # fcb_nxtrec
        run("getfcb: info's ext/rc/nxtrec into DATA.BDO", 'getfcb', mem,
            call=lambda b: (lib.getfcb(b, SYSDAT), ())[1],
            snap=lambda i: (i[bd('vrecord')], i[bd('rcount')], i[bd('extval')]))
        mem = base()
        w16(mem, bd('info'), FCB)
        mem[bd('vrecord')] = 9
        mem[bd('rcount')] = 0x33
        mem[bd('seqio')] = 1
        run("setfcb: DATA.BDO's record state back into info's fcb", 'setfcb', mem,
            call=lambda b: (lib.setfcb(b, SYSDAT), ())[1],
            snap=lambda i: (i[ds(FCB + 32)], i[ds(FCB + 15)]))

        # -- allocation vector through alloca --
        mem = base()
        mem[ds(ALLOC):ds(ALLOC + 4)] = b'\x81\x42\x00\xFF'
        for blk in (0, 7, 9, 26):
            def call(b, blk=blk):
                bx, dh = ctypes.c_uint16(), ctypes.c_uint8()
                return (lib.getallocbit(b, SYSDAT, blk, ctypes.byref(bx), ctypes.byref(dh)) & 1,)
            run(f"getallocbit: block {blk}", 'getallocbit', mem, {'cx': blk}, call=call,
                result=lambda r: (r['ax'] & 1,))
        for blk, bit in ((3, 1), (7, 0), (25, 1)):
            run(f"setallocbit: block {blk} <- {bit}", 'setallocbit', mem, {'cx': blk, 'dx': bit},
                call=lambda b, blk=blk, bit=bit: (lib.setallocbit(b, SYSDAT, blk, bit), ())[1],
                snap=lambda i: bytes(i[ds(ALLOC):ds(ALLOC + 4)]))

        # -- search over a real directory buffer --
        def directory(name, entries=((0, b'OTHER   TXT'), (0, b'FILE    DAT'), (0xE5, b'        '),
                                      (1, b'FILE    DAT'))):
            mem = base()
            w16(mem, bd('info'), FCB)
            mem[ds(FCB):ds(FCB + 12)] = b'\x00' + name
            w16(mem, bd('dirmax'), 3)          # 4 entries, one record
            w16(mem, bd('sectpt'), 26)         # seek divides by it
            w16(mem, bd('checka'), CHECKV)
            w16(mem, bd('chksiz'), 0x8000)     # permanent drive: no checksums
            w16(mem, bd('cdrmaxa'), DPB)
            w16(mem, ds(DPB), 4)
            for n, (user, ent) in enumerate(entries):
                rec = ds(BUFFA + 32 * n)
                mem[rec] = user
                mem[rec + 1:rec + 1 + len(ent)] = ent
            return mem

        snap = lambda i: (r16(i, bd('dcnt')), i[bd('lret')], i[bd('dptr')])  # noqa: E731
        for label, name in (("search: finds FILE.DAT, user 0", b'FILE    DAT'),
                            ("search: no such file", b'NOPE    DAT')):
            run(label, 'search', directory(name), {'cx': 12},
                call=lambda b: (lib.search(b, SYSDAT, 12), ())[1], snap=snap)

        # an XFCB on the way (open/make search with FIND_XFCB set): a
        # same-user XFCB sets SAVE_XFCB for that entry only, and a whole
        # match on one is noted (XDCNT) but searched past -- until
        # bugs.md #135 the C cleared SAVE_XFCB only at empty entries and
        # returned the XFCB as the match
        xsnap = lambda i: (snap(i), i[bd('save_xfcb')], r16(i, bd('xdcnt')))  # noqa: E731
        for label, entries in (
                ("search: past another file's XFCB", ((0x10, b'OTHER   TXT'), (0, b'FILE    DAT'), (0xE5, b''),
                                                      (0xE5, b''))),
                ("search: the file's own XFCB first", ((0x10, b'FILE    DAT'), (0, b'FILE    DAT'), (0xE5, b''),
                                                       (0xE5, b'')))):
            mem = directory(b'FILE    DAT', entries)
            mem[bd('find_xfcb')] = 0xFF
            w16(mem, bd('xdcnt'), 0xFE00)  # init_xfcb_search's "none yet"
            run(label, 'search', mem, {'cx': 12}, call=lambda b: (lib.search(b, SYSDAT, 12), ())[1], snap=xsnap)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
