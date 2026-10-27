#!/usr/bin/env python3
"""src-c/utils/spool.c against src-assem/utils/SPOOL.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): SPOOL opens each file the tail names, takes the
largest memory block it can get (or uses its own 512-byte buffer),
detaches from the console, and copies each file to the list device up
to its ^Z, a buffer-load at a time, expanding tabs; a key stops it.

Each case is a command tail, a disk and the memory MP/M has free; the
result is what reached the printer, which each case also checks against
the files' text (tabs expanded), so the two sides can't agree on
printing nothing.
"""
import sys

from rspref import Checker, DS, Mpm, Program, compare, pa, w16, r16
import rsp_syms

FREE_SEG = 0x0800  # free memory in the test machine: 0800:0 up to LOADER_SS


class SpoolMpm(Mpm):
    """Mpm, plus what SPOOL needs (the MP/M-86 Programmer's Guide, and
    src-assem/kernel's CPMMEM.MEM and the CIO's list functions):
      5   list output: the character (DL) goes to self.printed -- if the
          list device is attached (158), else self.unattached
      39  free drives: nothing to free here; returns 0
      53  get max mem: allocates the largest free block up to the MCB's
          length (paragraphs): writes base, length and ext = 1 (as
          CPMMEM.MEM's maxmem_entry does), BX = 0; none free -> BX =
          0FFFFh, CX = 3 (e_no_memory). `free` = (base, paragraphs) or None
      57  free mem: records the MCB's base in self.freed
      147 detach (the console): self.detached
      158 attach list, 159 detach list: self.lst_attached; each logged in
          self.events"""

    def __init__(self, free=None, **kw):
        super().__init__(**kw)
        self.free = free
        self.printed = bytearray()
        self.unattached = bytearray()
        self.freed = []
        self.detached = False
        self.lst_attached = False
        self.events = []

    def call(self, mem, ds, es, cl, dx):
        if cl == 5:
            self.log.append((cl, dx & 0xFF))
            (self.printed if self.lst_attached else self.unattached).append(dx & 0xFF)
            return 0, 0, es
        if cl == 53:
            self.log.append((cl, dx, ds))
            mcb = pa(ds, dx)
            if self.free is None:
                return 0xFFFF, 3, es
            base, paras = self.free
            length = min(r16(mem, mcb + 2), paras)
            w16(mem, mcb, base)
            w16(mem, mcb + 2, length)
            mem[mcb + 4] = 1
            self.free = None  # allocated: none left
            return 0, 0, es
        if cl == 57:
            self.log.append((cl, dx, ds))
            self.freed.append(r16(mem, pa(ds, dx)))
            return 0, 0, es
        if cl == 147:
            self.detached = True
            self.events.append('detach')
        elif cl == 158:
            self.lst_attached = True
            self.events.append('attach list')
        elif cl == 159:
            self.lst_attached = False
            self.events.append('detach list')
        return super().call(mem, ds, es, cl, dx)


def expand(text):
    """what SPOOL prints of text: to its ^Z, tabs to columns of 8 (a
    column counts printable characters, an LF starts it again)"""
    out, col = bytearray(), 0
    for c in text.split(b'\x1a')[0]:
        n = 8 - (col & 7) if c == 9 else 1
        c = 32 if c == 9 else c
        for _ in range(n):
            if c >= 32:
                col = (col + 1) & 0xFF
            if c == 10:
                col = 0
            out.append(c)
    return bytes(out)


SHORT = b'hello, printer\r\n\x1a'
TABS = b'a\tb\tc\r\n\tindented\r\n12345678\tnine\r\n\x1a'
LONG = b''.join(b'line %03d: the quick brown fox\r\n' % n for n in range(60)) + b'\x1a'  # 15 records
NOEOF = bytes(range(32, 96)) * 30  # exactly 15 records, no ^Z
FILES = {b'SHORT   TXT': SHORT, b'TABS    TXT': TABS, b'LONG    TXT': LONG, b'NOEOF   TXT': NOEOF,
         b'PW      TXT': (b'secret\r\n\x1a', {'password': b'KEY'}), b'EMPTY   TXT': b''}
ROOM = (FREE_SEG, 0x0400)    # 16K free: 128 buffers
SMALL = (FREE_SEG, 0x0030)   # 48 paragraphs: 6 buffers
TINY = (FREE_SEG, 0x0020)    # under 40 paragraphs: the local buffer


def main():
    prog = Program('SPOOL', 'spool_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('SPOOL_STACK_BASE') - 1024, rsp_syms.value('SPOOL_STACK_BASE')))
    ck = Checker()

    def tail(t):
        t = b' ' + t if t else b''

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 36)] = b'\0' + b' ' * 11 + bytes(24)
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    def snap(m, mpm):
        return bytes(mpm.printed), bytes(mpm.unattached), mpm.events, mpm.freed, mpm.dma_seg

    for label, t, kw, printed in (
            ("a file", b'SHORT.TXT', {'free': ROOM}, expand(SHORT)),
            ("tabs expanded", b'TABS.TXT', {'free': ROOM}, expand(TABS)),
            ("two files", b'SHORT.TXT TABS.TXT', {'free': ROOM}, expand(SHORT) + expand(TABS)),
            ("a comma between them", b'SHORT.TXT,TABS.TXT', {'free': ROOM}, expand(SHORT) + expand(TABS)),
            ("15 records in one buffer-load", b'LONG.TXT', {'free': ROOM}, expand(LONG)),
            ("15 records, 6 buffers: three loads", b'LONG.TXT', {'free': SMALL}, expand(LONG)),
            ("no ^Z: to the end, 6 buffers", b'NOEOF.TXT', {'free': SMALL}, expand(NOEOF)),
            ("too little memory: the local buffer", b'LONG.TXT', {'free': TINY}, expand(LONG)),
            ("no memory: the local buffer", b'NOEOF.TXT', {}, expand(NOEOF)),
            ("an empty file", b'EMPTY.TXT SHORT.TXT', {'free': ROOM}, expand(SHORT)),
            ("a file that can't be opened", b'SHORT.TXT NONE.TXT', {'free': ROOM}, b''),
            ("a password: can't be opened", b'PW.TXT', {'free': ROOM}, b''),
            ("an illegal file name", b'SHORT.TXT TOOLONGNAME.TXT', {'free': ROOM}, b''),
            ("no file named", b'', {'free': ROOM}, b''),
            ("a key stops it", b'LONG.TXT', {'free': ROOM, 'chars': b'x'}, expand(LONG)[:1]),
            ("the wrong MP/M", b'SHORT.TXT', {'free': ROOM, 'version': 0x0130}, b'')):
        ref, _ = compare(ck, prog, label, {'files': FILES, **kw}, setup=tail(t), snap=snap, mpm_class=SpoolMpm)
        ck.check(f"{label}: what SPOOL should print", printed, bytes(ref.printed))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
