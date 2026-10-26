#!/usr/bin/env python3
"""Differential test for src-c/kernel/chario.c (CHARIO.CIO's entry points
and internals) against the real CIO module, on real memory layout --
see tests/src-c/cioref.py. CCBs/PDs are SYSDAT-relative, print/read
buffers are in the running process's u_wrkseg, and the three CS-resident
messages (attach/abort/printer-busy) print from the CIO code segment on
the reference side and from chario.c's constants on the C side. Both
sides get the same scripted XIOS (cioref.Xios: typed input in, output
recorded), so each case compares the call's result, the whole stream of
XIOS calls (every character printed, every status poll and read), and
every byte of state left behind (columns, attach/detach, flags, the
buffers) -- a whole-image diff.
"""
import ctypes
import os
import sys

from cioref import (CioRef, Checker, CIO_C, NCONDEV, PD_RUN, SYSDAT, XIOS_FN, build_lib, c_buffer, c_xios,
                    sd, wk, w16, r16, u8, u16, c_u8p)

PD_B = 0x2100
BUF = 0x0100
CTLC, CTLD, CTLE, CTLH, CTLP, CTLR, CTLU, CTLX, TAB, CR, LF, RUBOUT = \
    3, 4, 5, 8, 0x10, 0x12, 0x15, 0x18, 9, 13, 10, 0x7F


def main():
    ref = CioRef()
    s = ref.s
    lib_path = build_lib(CIO_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.cio_set_xios.argtypes = [XIOS_FN]
        sigs = {'conin_entry': (u8, []), 'rconin_entry': (u8, []), 'constat_entry': (u8, []),
                'dirio_entry': (u8, [u8]), 'conout_entry': (None, [u8]), 'rconout_entry': (None, [u8]),
                'listout_entry': (None, [u8]), 'conwrite_entry': (None, [u16]),
                'conread_entry': (None, [u16]), 'conprint_entry': (None, [u16, u8, u8])}
        for name, (res, extra) in sigs.items():
            fn = getattr(lib, name)
            fn.restype = res
            fn.argtypes = [c_u8p, u16] + extra
        ck = Checker(ref)

        def base(attached=True, nchar=0, column=0):
            mem = ref.base_memory()
            if attached:
                w16(mem, ref.ccb(0, 'c_attach'), PD_RUN)
            mem[ref.ccb(0, 'c_nchar')] = nchar
            mem[ref.ccb(0, 'c_column')] = column
            return mem

        def run(label, entry, mem, regs=None, args=(), result=None, snap=None, typed=b''):
            ref_regs, img = ref.run(entry, mem, regs or {}, xios_input=typed)
            buf = c_buffer(mem)
            x, keep = c_xios(lib, typed)
            got = getattr(lib, entry)(buf, SYSDAT, *args)
            ck.check(f"{label}: XIOS calls", ref.xios.log, x.log)
            want = () if result is None else (ref_regs['bx'] & 0xFF,)
            port = () if result is None else (got,)
            snap = snap or (lambda i: (i[ref.ccb(0, 'c_column')], i[ref.ccb(0, 'c_nchar')],
                                       r16(i, ref.ccb(0, 'c_attach')), r16(i, ref.pd(PD_RUN, 'p_flag'))))
            ck.compare(label, img, buf, want, port, snap)

        # -- conin_entry --
        run("conin_entry: buffered graphic char, echoed", 'conin_entry', base(nchar=ord('A')), result=True)
        run("conin_entry: ctl-D detaches, then reads NUL", 'conin_entry', base(nchar=CTLD), result=True)
        run("conin_entry: ctl-C prints the abort prompt", 'conin_entry', base(nchar=CTLC, column=5), result=True)
        mem = base(attached=False, nchar=ord('x'))
        w16(mem, ref.pd(PD_RUN, 'p_flag'), s['pf_ctld'])
        run("conin_entry: re-attach after ctl-D prints 'Attach: <name>'", 'conin_entry', mem, result=True)

        # input from the XIOS rather than already buffered
        run("conin_entry: typed char read through the XIOS", 'conin_entry', base(), result=True, typed=b'a')
        run("conin_entry: ctl-S ... ctl-Q pauses, then reads", 'conin_entry', base(), result=True,
            typed=b'\x13zz\x11x')
        run("conin_entry: ctl-C answered Y sets pf_ctlc", 'conin_entry', base(column=3), result=True,
            typed=b'\x03y')
        run("conin_entry: ctl-C answered n, then reads on", 'conin_entry', base(), result=True, typed=b'\x03nq')
        run("conin_entry: ctl-C, ctl-C again re-prompts", 'conin_entry', base(), result=True, typed=b'\x03\x03n')
        run("rconin_entry: raw, ctl-C read as data", 'rconin_entry', base(), result=True, typed=b'\x03')

        # -- conout / rconout --
        for label, ch, col in (("conout_entry: column advances", ord('Z'), 3),
                               ("conout_entry: tab expands to the next stop", TAB, 3),
                               ("conout_entry: cr resets the column", CR, 7),
                               ("conout_entry: backspace backs up", CTLH, 7),
                               ("conout_entry: not attached -> attaches first", ord('Z'), 0)):
            run(label, 'conout_entry', base(attached=col != 0, column=col), {'dx': ch}, (ch,))
        run("rconin_entry: raw read, ctl-D passed through", 'rconin_entry', base(nchar=CTLD), result=True)
        run("rconout_entry: lf leaves the column", 'rconout_entry', base(column=4), {'dx': LF}, (LF,))

        mem = base()
        mem[ref.ccb(0, 'c_mimic')] = NCONDEV + 1   # list device 1 echoes console 0
        run("conout_entry: echoed to the mimicking list device", 'conout_entry', mem, {'dx': ord('m')},
            (ord('m'),))
        mem = base()
        mem[ref.ccb(0, 'c_mimic')] = 1             # console 1 echoes console 0
        run("conout_entry: echoed to the mimicking console", 'conout_entry', mem, {'dx': ord('m')}, (ord('m'),))

        # -- listout --
        run("listout_entry: attaches to the list device", 'listout_entry', base(), {'dx': ord('X')}, (ord('X'),),
            snap=lambda i: r16(i, ref.ccb(NCONDEV, 'c_attach')))

        # -- constat / dirio --
        run("constat_entry: attached, char buffered", 'constat_entry', base(nchar=ord('A')), result=True)
        run("constat_entry: not attached", 'constat_entry', base(attached=False), result=True)
        run("dirio_entry: read (0xFD)", 'dirio_entry', base(nchar=ord('q')), {'dx': 0xFD}, (0xFD,), result=True)
        run("dirio_entry: write, column advances", 'dirio_entry', base(column=2), {'dx': ord('w')}, (ord('w'),))
        run("dirio_entry: status (0xFE), ready", 'dirio_entry', base(nchar=ord('q')), {'dx': 0xFE}, (0xFE,),
            result=True)
        run("dirio_entry: status (0xFE), not ready", 'dirio_entry', base(), {'dx': 0xFE}, (0xFE,), result=True)
        run("dirio_entry: readif (0xFF), char buffered", 'dirio_entry', base(nchar=ord('r')), {'dx': 0xFF},
            (0xFF,), result=True)
        run("dirio_entry: readif (0xFF), nothing buffered", 'dirio_entry', base(), {'dx': 0xFF}, (0xFF,),
            result=True)

        # -- conwrite / conprint (buffer in u_wrkseg) --
        mem = base()
        mem[wk(BUF):wk(BUF) + 6] = b'ab\tc$x'
        run("conwrite_entry: stops at '$', tab expanded", 'conwrite_entry', mem, {'dx': BUF}, (BUF,))
        mem = base()
        mem[wk(BUF):wk(BUF) + 6] = b'abcdef'
        run("conprint_entry: stops at max (3 chars)", 'conprint_entry', mem, {'dx': BUF, 'bx': 0x0300},
            (BUF, 3, 0))

        # -- conread (buffer in u_wrkseg) --
        def reader(max_len, nchar, column=0):
            mem = base(nchar=nchar, column=column)
            mem[wk(BUF)] = max_len
            return mem

        buf_snap = lambda i: (bytes(i[wk(BUF):wk(BUF) + 8]), i[ref.ccb(0, 'c_column')],  # noqa: E731
                              i[ref.ccb(0, 'c_strtcol')], i[ref.ccb(0, 'c_flag')])
        for label, max_len, ch in (("conread_entry: cr ends an empty line", 4, CR),
                                   ("conread_entry: one char fills a 1-byte buffer", 1, ord('k')),
                                   ("conread_entry: char then NULs to max length", 3, ord('k')),
                                   ("conread_entry: ctl-H at the start is ignored", 2, CTLH),
                                   ("conread_entry: rubout at the start is ignored", 2, RUBOUT),
                                   ("conread_entry: ctl-E resets the columns", 2, CTLE),
                                   ("conread_entry: ctl-X with nothing typed", 2, CTLX),
                                   ("conread_entry: ctl-U redraws from the start column", 2, CTLU),
                                   ("conread_entry: ctl-R retypes the line", 2, CTLR)):
            run(label, 'conread_entry', reader(max_len, ch, column=6), {'dx': BUF}, (BUF,), snap=buf_snap)

        # typed lines, with real line editing
        for label, typed in (("conread_entry: typed line ended by cr", b'abc\r'),
                             ("conread_entry: backspace (ctl-H) edits", b'ab\x08c\r'),
                             ("conread_entry: rubout echoes the deleted char", b'ab\x7fc\r'),
                             ("conread_entry: ctl-X erases the line", b'abc\x18d\r'),
                             ("conread_entry: ctl-U starts over", b'ab\x15cd\r'),
                             ("conread_entry: ctl-R retypes, then continues", b'ab\x12c\r'),
                             ("conread_entry: control chars echo as ^X", b'a\x01b\r'),
                             ("conread_entry: fills to max length", b'abcdefgh')):
            run(label, 'conread_entry', reader(5, 0, column=2), {'dx': BUF}, (BUF,), snap=buf_snap, typed=typed)

        # ctl-P: turns on printer echo (mimic the list device)
        mem = reader(2, CTLP)
        run("conread_entry: ctl-P starts mimicking the list device", 'conread_entry', mem, {'dx': BUF}, (BUF,),
            snap=lambda i: (buf_snap(i), r16(i, ref.ccb(NCONDEV, 'c_attach')), i[ref.ccb(0, 'c_mimic')]))
        mem = reader(2, CTLP)
        w16(mem, ref.ccb(NCONDEV, 'c_attach'), PD_B)
        run("conread_entry: ctl-P with the printer busy", 'conread_entry', mem, {'dx': BUF}, (BUF,), snap=buf_snap)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
