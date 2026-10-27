#!/usr/bin/env python3
"""src-c/utils/stscmd.c (with stscom.c) against src-assem/utils/STSCMD.PLM
(which includes STSCOM.PLM; compiled as src-assem's build compiles
it), as whole programs (see tests/src-c/rspref.py): MPMSTAT walks
SYSDAT's lists -- the free PD/MD/QD lists, the queue buffer MAU's SAT,
the ready/delay/poll lists, each queue's DQ and NQ lists, the flags,
the CCBs, the thread list's memory partitions and the memory free list
-- and prints them; with P in the tail it repeats after each key.

Each case is a SYSDAT, laid out as src-assem/kernel's (SYSDAT.FMT,
PD.DEF, QD.DEF, CCB.DEF, FLG.DEF, MEM.DEF; src-c/kernel/defs.h), with
PDs, QDs, MDs, the CCB and flag tables in SYSDAT and the queue buffer
area's SAT in a segment of its own. The run ends at terminate (or at
the key the script doesn't have, with P).

MPMSTAT's code (186Ch bytes) doesn't fit below rspref's DS at CS =
0100h, so the test moves CS down to 0070h (nothing else is there).
"""
import struct
import sys

import rspref
from rspref import Checker, DS, Mpm, Program, SYSDAT, compare, pa, w16
import rsp_syms

rspref.CS = 0x0070  # see above: 0700h + 186Ch is below DS's 2000h

QBUF = 0x0680  # the queue buffer area's segment (the SAT at its start)

# SYSDAT.FMT
S_SUPMOD, S_NCNS, S_NLST, S_NCCB, S_NFLAGS, S_MMP = 0x00, 0x47, 0x48, 0x49, 0x4A, 0x4C
S_CCB, S_FLAGS, S_MDUL, S_MFL, S_PUL, S_QUL, S_QMAU = 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E, 0x60
S_RLR, S_DLR, S_PLR, S_THRDRT, S_QLR, S_VERSION = 0x68, 0x6A, 0x6E, 0x72, 0x74, 0x78
S_NCONDEV, S_NCIODEV = 0x83, 0x85
VERTEXT = 0x0F00  # the version string, in the SUP's code segment (VSEG)


class StsMpm(Mpm):
    """rspref's Mpm plus the two functions MPMSTAT makes that it lacks:
      142 dispatch      -- no argument; returns nothing (the dispatcher
                           runs, here nothing changes)
      154 get SYSDAT    -- no argument; ES:BX = SYSDAT:0
    (logged as functions with no argument, as rspref's NO_ARG would)"""

    def call(self, mem, ds, es, cl, dx):
        if cl in (142, 154):
            self.log.append((cl,))
            return (0, 0, SYSDAT) if cl == 154 else (0, 0, es)
        return super().call(mem, ds, es, cl, dx)


class Sysdat:
    """builds a SYSDAT in the scripted machine's memory"""

    def __init__(self, m):
        self.m = m
        self.next_md = 0x1100

    def w(self, off, val):
        w16(self.m, pa(SYSDAT, off), val)

    def b(self, off, val):
        self.m[pa(SYSDAT, off)] = val

    def pd(self, off, name, link=0, thread=0, mem=0, cns=0):
        e = bytearray(0x30)
        struct.pack_into('<HH', e, 0, link, thread)
        e[8:16] = name.ljust(8)
        struct.pack_into('<H', e, 0x16, mem)
        e[0x20] = cns
        self.m[pa(SYSDAT, off):pa(SYSDAT, off + 0x30)] = e

    def qd(self, off, name, link=0, flags=0, dq=0, nq=0, buffer=0):
        e = bytearray(0x1C)
        struct.pack_into('<HBBH', e, 0, link, 0, 0, flags)
        e[6:14] = name.ljust(8)
        struct.pack_into('<HHHHHHH', e, 0x0E, 0, 0, dq, nq, 0, 0, buffer)
        self.m[pa(SYSDAT, off):pa(SYSDAT, off + 0x1C)] = e

    def md(self, off, link=0, start=0, length=0, plist=0, unused=0):
        self.m[pa(SYSDAT, off):pa(SYSDAT, off + 10)] = struct.pack('<5H', link, start, length, plist, unused)

    def partition(self, pd, start, length):
        """pd's memory: an MS descriptor on its p_mem whose mau is an MD"""
        ms, md = self.next_md, self.next_md + 10
        self.next_md += 20
        self.md(md, 0, start, length)
        self.md(ms, 0, start, length, 0, md)  # link, start, length, flags, mau
        self.w(pd + 0x16, ms)

    def chain(self, offs, field=0):
        for a, b in zip(offs, offs[1:] + [0]):
            self.w(a + field, b)


# PDs (SYSDAT offsets; rspref's own running PD is at 0200h)
TMP0, TMP1, CLOCK, ECHO, PIN0, PIN1, SPOOL, ABORT, DIRX, TYPEX = (0x0C00 + 0x30 * i for i in range(10))
FREEPD = [0x0E00, 0x0E30, 0x0E60]
EXTRA = [0x1200, 0x1230, 0x1260]  # more ready PDs, for a wide list
CCBS, FLAGS, QDS = 0x0E90, 0x0F30, 0x1000
FLAGS256 = 0x1300  # a 256-entry flag table (300h bytes), for nflags 0
MXDISK, MXLIST, CLIQ, ECHOQ, SPOOLQ = (QDS + 0x1C * i for i in range(5))
FREEQD = [QDS + 0x1C * 5, QDS + 0x1C * 6]


def system(nflags=8, ncondev=2, nciodev=3, ready=True, partitions=True, delay_loop=False, wide=False):
    """a running MP/M: setup(m) for compare"""
    def setup(m):
        s = Sysdat(m)
        s.w(S_SUPMOD + 2, rspref.VSEG)  # supmod(1): the SUP's code segment
        s.w(S_VERSION, VERTEXT)
        v = b'\r\n\nMP/M-86 V2.0 \xd4est\r\n$'  # the high bit is masked off
        m[pa(rspref.VSEG, VERTEXT):pa(rspref.VSEG, VERTEXT) + len(v)] = v
        s.b(S_NCNS, 2)
        s.b(S_NLST, 1)
        s.b(S_NCCB, 5)  # two consoles, a list device, two virtual consoles
        s.b(S_NFLAGS, nflags)
        s.w(S_MMP, 0x2000)
        s.b(S_NCONDEV, ncondev)
        s.b(S_NCIODEV, nciodev)

        names = {TMP0: b'Tmp0', TMP1: b'Tmp1', CLOCK: b'CLOCK', ECHO: b'ECHO', PIN0: b'PIN', PIN1: b'PIN',
                 SPOOL: b'SPOOL', ABORT: b'ABORT', DIRX: b'DIR', TYPEX: b'TYPE'}
        for i, (off, name) in enumerate(names.items()):
            s.pd(off, name, cns=i % 2)
        s.w(TMP1 + 0x16, 0xFF)  # pd.mem 0FFh: its console isn't shown
        for off in FREEPD:
            s.pd(off, b'')
        for i, off in enumerate(EXTRA):
            s.pd(off, b'USER%d' % i, cns=2)
        s.chain(FREEPD)
        s.w(S_PUL, FREEPD[0])

        # the lists: the running PD first on the ready list
        rl = [rspref.PD, DIRX, TYPEX, TMP1, ABORT] + (EXTRA if wide else [])
        if ready:
            s.chain(rl)
            s.w(S_RLR, rspref.PD)
        else:
            s.w(S_RLR, 0)
        s.w(S_DLR, CLOCK)
        if delay_loop:
            s.w(CLOCK, CLOCK)  # a broken (circular) list: display$links stops at 64
        s.chain([PIN0, PIN1])
        s.w(S_PLR, PIN0)

        # flags: off (0FFFFh), set (0FFFEh), or the PD waiting on it.
        # With nflags 0 STSCOM walks all 256 (its byte count wraps), so
        # that table is a whole one, in room of its own (FLAGS256)
        fl = [0xFFFF, 0xFFFE, CLOCK, 0xFFFE, 0xFFFF, SPOOL, 0xFFFE, 0xFFFF] + [0xFFFE] * 30
        table = FLAGS
        if nflags == 0:
            table = FLAGS256
            fl = fl[:8] + [0xFFFF] * (255 - 8) + [CLOCK]
        s.w(S_FLAGS, table)
        for i, pd in enumerate(fl):
            m[pa(SYSDAT, table + 3 * i):pa(SYSDAT, table + 3 * i + 3)] = struct.pack('<HB', pd, 0)

        # queues: MX ones with and without an owner, one with readers
        # waiting (DQ), one with writers (NQ)
        s.qd(MXDISK, b'MXdisk', MXLIST, flags=0x01 | 0x08, buffer=ABORT)
        s.qd(MXLIST, b'MXlist', CLIQ, flags=0x01)
        s.qd(CLIQ, b'CliQ', ECHOQ, flags=0x02, dq=TMP0, buffer=0x0400)
        s.qd(ECHOQ, b'ECHO', SPOOLQ, nq=ECHO, buffer=0x0500)
        s.qd(SPOOLQ, b'SPOOL', 0, flags=0x08, dq=SPOOL)
        s.w(S_QLR, MXDISK)
        for off in FREEQD:
            s.qd(off, b'')
        s.chain(FREEQD)
        s.w(S_QUL, FREEQD[0])

        # CCBs: console 0 attached to Tmp0 with two waiting; console 1
        # in ^P (to console 0); the printer unattached, a PD waiting
        s.w(S_CCB, CCBS)
        ccbs = [(TMP0, ABORT), (0xFFFF, 0), (0, SPOOL), (TYPEX, 0), (0, 0)]
        for i, (att, q) in enumerate(ccbs):
            e = bytearray(12)
            struct.pack_into('<HH', e, 0, att, q)
            e[9] = 0 if att == 0xFFFF else 0x55
            m[pa(SYSDAT, CCBS + 12 * i):pa(SYSDAT, CCBS + 12 * i + 12)] = e

        # memory: the thread list through every PD, partitions for some
        thread = [rspref.PD, TMP0, TMP1, CLOCK, ECHO, PIN0, PIN1, SPOOL, ABORT, DIRX, TYPEX]
        s.chain(thread, field=2)
        s.w(S_THRDRT, thread[0] if partitions else 0)
        if partitions:
            s.partition(rspref.PD, 0x2000, 0x0100)
            s.partition(TMP0, 0x3A00, 0x0480)
            s.partition(SPOOL, 0x4000, 0x0200)
            s.partition(DIRX, 0x8000, 0x1000)
            s.partition(TYPEX, 0x9000, 0x0800)
            s.w(ECHO + 0x16, 8)  # pd.mem 8 is skipped too
        free = [0x1180, 0x118A, 0x1194]
        for off, (start, length) in zip(free, [(0xA000, 0x2000), (0xC800, 0x0400), (0xD000, 0x1000)]):
            s.md(off, 0, start, length)
        s.chain(free)
        s.w(S_MFL, free[0])
        unused = [0x11B0, 0x11BA, 0x11C4, 0x11CE]
        for off in unused:
            s.md(off)
        s.chain(unused)
        s.w(S_MDUL, unused[0])

        # the queue buffer MAU (qmau: link, start, length, plist) and its SAT
        m[pa(SYSDAT, S_QMAU):pa(SYSDAT, S_QMAU + 8)] = struct.pack('<4H', 0, QBUF, 0x0040, 0)
        sat = [(0x0003, 0x0003, 0), (0x0001, 0x0040, 0), (0x0041, 0x0020, 2), (0x0061, 0x0080, 0), (0, 0, 0)]
        for i, (st, ln, na) in enumerate(sat):
            m[pa(QBUF, 5 * i):pa(QBUF, 5 * i + 5)] = struct.pack('<HHB', st, ln, na)
    return setup


def tail(t, then=None):
    def run(m):
        if then:
            then(m)
        m[pa(DS, 0x80)] = len(t)
        m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
    return run


def empty(m):
    """a SYSDAT with nothing on any list, no flags, no consoles"""
    w16(m, pa(SYSDAT, S_RLR), 0)
    w16(m, pa(SYSDAT, S_SUPMOD + 2), rspref.VSEG)
    w16(m, pa(SYSDAT, S_VERSION), VERTEXT)
    v = b'\r\n\nMP/M-86 V2.0$'
    m[pa(rspref.VSEG, VERTEXT):pa(rspref.VSEG, VERTEXT) + len(v)] = v


def main():
    prog = Program('STSCMD', 'stscmd_main', rsp=False, plm=True, c_name=['stscmd.c', 'stscom.c'],
                   stack=(rsp_syms.value('STSCMD_STACK_BASE') - 1024, rsp_syms.value('STSCMD_STACK_BASE')))
    ck = Checker()

    def run(label, setup, expect=(), absent=(), **kw):
        ref, _ = compare(ck, prog, label, kw, setup=setup, mpm_class=StsMpm)
        out = bytes(ref.out)
        # the reference's output is MPMSTAT's real display, not a failure both share
        ck.check(f"{label}: shows what it should", [], [e for e in expect if e not in out])
        ck.check(f"{label}: and not what it shouldn't", [], [e for e in absent if e in out])
        return out

    full = [b'MP/M-86 V2.0 Test', b'Number of Physical Consoles = 02', b'Number of Virtual Consoles = 02',
            b'Number of Free Process Descriptors = 03', b'Number of Free Memory Descriptors = 04',
            b'Number of Free Queue Control Blocks = 02', b'Free Queue Buffer Area = 00C0',
            b'Number of Flags = 08', b'Maximum Paragraphs Per Process = 2000',
            b' STSCMD  [00]', b' Tmp1        ', b'[CliQ    ]   Tmp0    [00]', b'[ECHO    ]   ECHO    [01]',
            b'  02 -        CLOCK   [00]', b'  01  03  06', b' MXdisk  [ABORT   ]', b' MXlist            ',
            b'Control P - Console [00]', b'Unattached', b' [00]      - Tmp0', b' [00]      - ABORT       ',
            b'2000   0100  STSCMD', b'D000   1000  * FREE *', b'Printers:']
    for label, setup, kw, expect, absent in (
            ("a running system", system(), {}, full, [b'Hit a Key']),
            ("a wide ready list wraps", system(wide=True), {}, [b'ABORT   [01]', b' USER2   [02]'], []),
            ("[P]: again after each key", tail(b' P', system()), {'chars': b'xy'}, full + [b'Hit a Key'], []),
            ("p, lower case, later in the tail", tail(b' xyz p', system()), {'chars': b'x'}, [b'Hit a Key'], []),
            ("a tail without P", tail(b' Q', system()), {}, full, [b'Hit a Key']),
            ("a circular delay list stops at 64", system(delay_loop=True), {}, [b' CLOCK   [00]' * 3], []),
            ("no flags, nothing ready, no partitions", system(nflags=0, ready=False, partitions=False), {},
             [b'Number of Flags = 00', b'  FF -       ', b'Ready Process(es): \r\nProcess(es) DQing: '],
             [b'* FREE *']),  # no thread list: display$memory returns before the free list
            ("no printers", system(nciodev=2), {}, [b'Consoles:'], [b'Printers:']),
            ("fewer devices than consoles: the count wraps", system(ncondev=3, nciodev=2), {},
             [b'Printers:', b' [FE]      - Unattached'], []),
            ("many flags set: the line wraps", system(nflags=38), {}, [b'  25'], []),
            ("an empty system", empty, {}, [b'Number of Flags = 00'], [b'Consoles:']),
            ("the wrong MP/M", system(), {'version': 0x1022}, [b'Requires MP/M-86 2.0'], [b'Status']),
            ("CP/M-86's version", system(), {'version': 0x0030}, [b'Requires MP/M-86 2.0'], [b'Status'])):
        run(label, setup, expect, absent, **kw)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
