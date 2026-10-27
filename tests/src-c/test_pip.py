#!/usr/bin/env python3
"""src-c/utils/pip.c against src-assem/utils/PIP.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): PIP copies files -- fast, a buffer at a time,
or a character at a time when a parameter needs it -- several at once
(B:=*.TXT), concatenated (C=A,B), to and from devices (CON:, LST:, PRN:,
EOF:, NUL:), with its bracketed parameters; with no command tail it
prompts * for one command a line until an empty one. Each case is a
command tail (or console lines) and a disk; the result is the disk
afterwards -- each drive's directory (names with their attribute bits,
extents) and each file's contents and password -- and what went to the
printer.

PIP needs more of the BDOS than rspref's Mpm has, so PipMpm, below,
models a CP/M-3 / MP/M-86 file system of its own: four drives of 64
directory entries (one 16K extent each), users, R/O, SYS and archive
attributes, passwords (with the XFCB PIP reads them from), the
multi-sector count, the sequential position (ex, s2, cr) and the random
record, a disk capacity, the printer.

PIP's buffers are past its image, sized from MAXB, the data group length
LOAD leaves in the base page (from the CMD header's maximum: DRI's is
980h paragraphs). This machine is 64K with the UDA at DS:2000h, so the
tests give PIP MAXB = 1A40h: 4K of buffers (SBUFF and DBUFF 2K each, 4K
together for a fast copy, so a fill is 4 multi-sector reads of 8).
"""
import sys

import rspref
from rspref import (Checker, DS, LOADER_SS, Mpm, Program, UDA_SEG, U_DCNT, U_SEARCHA, U_SEARCHABASE,
                    U_SEARCHL, BYTE_ARG, NO_ARG, compare, pa, r16, w16)
from i86sim import I86Sim
import rsp_syms

MAXB = 0x1A40
NDIR = 64
PIP_CS = 0x0800  # PIP's 17K of code doesn't fit below DS (rspref's CS): 8000h-C56Ch is free


def name11(name):
    base, _, ext = name.partition('.')
    return (base.ljust(8) + ext.ljust(3)).encode()


class PFile:
    """a file: its user, name (11 bytes, the attributes in the high
    bits), records, password (8 bytes or None) and password mode"""

    def __init__(self, user, name, data=b'', password=None, mode=0x80):
        self.user, self.name = user, bytearray(name)
        data = data + b'\x1a' * ((-len(data)) % 128)
        self.recs = [bytearray(data[i:i + 128]) for i in range(0, len(data), 128)]
        self.password, self.mode = password, mode

    def key(self):
        return bytes(b & 0x7F for b in self.name)

    def extents(self):
        return max(1, (len(self.recs) + 127) // 128)


def attr_name(name, ro=False, sys=False, arc=False, f1=False):
    n = bytearray(name)
    if f1:
        n[0] |= 0x80
    if ro:
        n[8] |= 0x80
    if sys:
        n[9] |= 0x80
    if arc:
        n[10] |= 0x80
    return n


class PipMpm(Mpm):
    """rspref's Mpm with a file system for PIP:
      disk -- {'D:NAME.TYP' or 'D:NAME.TYP;u': data, or (data, {'ro', 'sys',
              'arc', 'f1', 'password'})} (u: the user, default 0)
      capacity -- {drive: records} a drive holds (default: plenty)
      full -- fill drive A's free directory entries with user 15 files,
              leaving `full` free (True: none)
      typeahead -- console status sees the script's characters (else
              they are only there when PIP reads; the user types after
              the prompt)
      lst_busy -- the printer is attached to another process
      reverse -- drive A's directory entries in the reverse order (a
              file's later extents before its first)"""

    def __init__(self, disk=None, capacity=None, full=False, typeahead=False, lst_busy=False, reverse=False,
                 **kw):
        super().__init__(**kw)
        self.slots = {d: [None] * NDIR for d in range(4)}  # [file, extent] or None
        self.capacity = capacity or {}
        self.typeahead, self.lst_busy = typeahead, lst_busy
        self.mcnt = 1
        self.lst_out = bytearray()
        for spec_name, spec in (disk or {}).items():
            data, attrs = spec if isinstance(spec, tuple) else (spec, {})
            drive, rest = spec_name.split(':')
            fname, _, user = rest.partition(';')
            f = PFile(int(user or 0), attr_name(name11(fname), attrs.get('ro'), attrs.get('sys'),
                                                 attrs.get('arc'), attrs.get('f1')), data,
                      attrs['password'].ljust(8) if 'password' in attrs else None)
            assert self._grow(ord(drive) - ord('A'), f)
        if reverse:
            self.slots[0].reverse()
        if full:
            n = 0
            while sum(s is None for s in self.slots[0]) > (0 if full is True else full):
                assert self._grow(0, PFile(15, b'FILL%04d   ' % n))
                n += 1

    # -- the directory
    def _free(self, d):
        return next((n for n in range(NDIR) if self.slots[d][n] is None), None)

    def _grow(self, d, f, records=None):
        """give f the directory entries `records` records need"""
        need = max(1, ((len(f.recs) if records is None else records) + 127) // 128)
        have = sum(1 for s in self.slots[d] if s and s[0] is f)
        while have < need:
            n = self._free(d)
            if n is None:
                return False
            self.slots[d][n] = [f, have]
            have += 1
        return True

    def _files(self, d):
        seen = []
        for s in self.slots[d]:
            if s and s[0] not in seen:
                seen.append(s[0])
        return seen

    def _entry(self, s):
        if s is None:
            return b'\xe5' * 32
        f, ext = s
        e = bytearray(32)
        e[0] = f.user
        e[1:12] = f.name
        e[12], e[14] = ext & 0x1F, ext >> 5
        e[15] = max(0, min(128, len(f.recs) - ext * 128))
        e[16] = 1 + ext
        return bytes(e)

    def _drive(self, b0):
        return self.disk if b0 in (0, ord('?')) else (b0 - 1) & 0x0F

    @staticmethod
    def _like(pat, name):
        return all((p & 0x7F) == ord('?') or (p & 0x7F) == (c & 0x7F) for p, c in zip(pat, name))

    def _find(self, d, name, user=None, sys0=False):
        """the file `name` (masked) of the user; with sys0, else user 0's
        if it is a SYS file (as MP/M's open does) -- (file, from user 0)"""
        user = self.user if user is None else user
        for f in self._files(d):
            if f.user == user and f.key() == name:
                return f, False
        if sys0 and user != 0:
            for f in self._files(d):
                if f.user == 0 and f.key() == name and f.name[9] & 0x80:
                    return f, True
        return None, False

    def _matching(self, d, pat):
        return [f for f in self._files(d) if f.user == self.user and self._like(pat, f.name)]

    def _pw_ok(self, mem, ds, f):
        d = self._dma(ds)
        return f.password is None or f.password == bytes(mem[d:d + 8])

    # -- the sequential position: cr, ex (fcb 12), s2 (fcb 14)
    @staticmethod
    def _pos(mem, ds, dx):
        ex, s2, cr = mem[pa(ds, dx + 12)] & 0x1F, mem[pa(ds, dx + 14)] & 0x3F, mem[pa(ds, dx + 32)]
        return (s2 * 32 + ex) * 128 + cr

    @staticmethod
    def _set_pos(mem, ds, dx, rec, after):
        """rec accessed; after: cr past it (sequential), else at it (random)"""
        ext = rec // 128
        mem[pa(ds, dx + 12)] = ext & 0x1F
        mem[pa(ds, dx + 14)] = ext >> 5
        mem[pa(ds, dx + 32)] = rec % 128 + (1 if after else 0)

    def _used(self, d):
        return sum(len(f.recs) for f in self._files(d))

    def _search(self, mem, ds, dx, start):
        pat = bytes(mem[pa(ds, dx):pa(ds, dx) + 15])
        d = self._drive(pat[0])
        for n in range(start, NDIR):
            s = self.slots[d][n]
            if pat[0] == ord('?'):
                hit = True
            elif s is None or s[0].user != self.user or not self._like(pat[1:12], s[0].name):
                hit = False
            else:  # the extent: ? any, else ex (its low 5 bits) with s2 = 0
                hit = pat[12] == ord('?') or (s[1] == pat[12] & 0x1F)
            if hit:
                dma = self._dma(ds)
                rec = n & ~3
                mem[dma:dma + 128] = b''.join(self._entry(self.slots[d][rec + k]) for k in range(4))
                w16(mem, pa(UDA_SEG, U_DCNT), n)
                return n & 3
        w16(mem, pa(UDA_SEG, U_DCNT), 0xFFFF)
        return 0xFF

    def call(self, mem, ds, es, cl, dx):
        if cl not in (5, 11, 15, 16, 17, 18, 19, 20, 21, 22, 23, 30, 33, 36, 44, 102, 161):
            return super().call(mem, ds, es, cl, dx)
        self.log.append((cl,) if cl in NO_ARG else (cl, dx & 0xFF) if cl in BYTE_ARG else (cl, dx, ds))
        dl = dx & 0xFF
        fcb = lambda i: pa(ds, dx + i)  # noqa: E731
        name = bytes(b & 0x7F for b in mem[fcb(1):fcb(12)])
        d = self._drive(mem[fcb(0)])
        bx = 0
        if cl == 5:                       # list output
            self.lst_out.append(dl)
        elif cl == 11:                    # console status
            bx = 0xFF if self.typeahead and self.chars else 0
        elif cl == 161:                   # conditional attach list
            bx = 0xFF if self.lst_busy else 0
        elif cl == 44:                    # set multi-sector count
            self.mcnt = dl
        elif cl == 17:                    # search first
            mem[pa(UDA_SEG, U_SEARCHL)] = 15
            w16(mem, pa(UDA_SEG, U_SEARCHA), dx)
            w16(mem, pa(UDA_SEG, U_SEARCHABASE), ds)
            bx = self._search(mem, ds, dx, 0)
        elif cl == 18:                    # search next: the UDA's place, its fcb as it is now
            dcnt = r16(mem, pa(UDA_SEG, U_DCNT))
            bx = 0xFF if dcnt == 0xFFFF else self._search(mem, r16(mem, pa(UDA_SEG, U_SEARCHABASE)),
                                                          r16(mem, pa(UDA_SEG, U_SEARCHA)), dcnt + 1)
        elif cl == 15:                    # open: the fcb gets the extent's entry
            f, from0 = self._find(d, name, sys0=True)
            ext = (mem[fcb(14)] & 0x3F) * 32 + (mem[fcb(12)] & 0x1F)
            if b'?' in name:
                bx = 0x09FF
            elif f is None or ext >= f.extents():
                bx = 0x00FF
            elif not self._pw_ok(mem, ds, f):
                bx = 0x07FF
            else:
                n = next(n for n in range(NDIR) if self.slots[d][n] and self.slots[d][n] == [f, ext])
                e = self._entry(self.slots[d][n])
                mem[fcb(1):fcb(32)] = e[1:32]
                for i in range(5, 9):      # f5'-f8': the interface attributes
                    mem[fcb(i)] &= 0x7F
                if from0:                  # f8': user 0's SYS file
                    mem[fcb(8)] |= 0x80
                bx = n & 3
        elif cl == 16:                    # close
            bx = 0 if self._find(d, name, sys0=True)[0] else 0xFF
        elif cl in (20, 21):              # read / write sequential: mcnt records
            f = self._find(d, name, sys0=True)[0]
            if f is None:
                return bx | 9, 0, es
            if cl == 21 and f.name[8] & 0x80:
                return 0x03FF, 0, es
            for k in range(self.mcnt):
                rec = self._pos(mem, ds, dx)
                dma = self._dma(ds) + 128 * k
                if cl == 20:
                    if rec >= len(f.recs):
                        bx = 1 | k << 12
                        break
                    mem[dma:dma + 128] = f.recs[rec]
                else:
                    if rec >= len(f.recs):
                        if not self._grow(d, f, rec + 1):
                            bx = 1 | k << 12  # no directory space
                            break
                        if self._used(d) + rec + 1 - len(f.recs) > self.capacity.get(d, 1 << 20):
                            bx = 2 | k << 12  # no data block
                            break
                        f.recs += [bytearray(128) for _ in range(rec + 1 - len(f.recs))]
                    f.recs[rec] = bytearray(mem[dma:dma + 128])
                    f.name[10] &= 0x7F        # written: not archived
                self._set_pos(mem, ds, dx, rec, True)
        elif cl == 33:                    # read random: the position is left at the record
            f = self._find(d, name, sys0=True)[0]
            rec = r16(mem, fcb(33)) | mem[fcb(35)] << 16
            if f is None:
                bx = 9
            elif rec >= len(f.recs):
                bx = 1
            else:
                dma = self._dma(ds)
                mem[dma:dma + 128] = f.recs[rec]
                self._set_pos(mem, ds, dx, rec, False)
        elif cl == 36:                    # set random record
            rec = self._pos(mem, ds, dx)
            w16(mem, fcb(33), rec & 0xFFFF)
            mem[fcb(35)] = rec >> 16
        elif cl == 19:                    # delete: every extent; R/O, password
            hits = self._matching(d, mem[fcb(1):fcb(12)])
            if not hits:
                bx = 0x00FF
            elif any(f.name[8] & 0x80 for f in hits):
                bx = 0x03FF
            elif not all(self._pw_ok(mem, ds, f) for f in hits):
                bx = 0x07FF
            else:
                self.slots[d] = [None if s and s[0] in hits else s for s in self.slots[d]]
        elif cl == 22:                    # make: f6' gives it the DMA's password and mode
            n = self._free(d)
            if b'?' in name:
                bx = 0x09FF
            elif self._find(d, name)[0]:
                bx = 0x08FF
            elif n is None:
                bx = 0x00FF
            else:
                nm = bytearray(mem[fcb(1):fcb(12)])
                for i in range(4, 8):
                    nm[i] &= 0x7F
                f = PFile(self.user, nm)
                if mem[fcb(6)] & 0x80:
                    dma = self._dma(ds)
                    f.password, f.mode = bytes(mem[dma:dma + 8]), mem[dma + 8]
                self.slots[d][n] = [f, 0]
                mem[fcb(15):fcb(32)] = bytes(17)
                bx = n & 3
        elif cl == 23:                    # rename: the new name at fcb+16; attributes kept
            f = self._find(d, name)[0]
            new = bytes(b & 0x7F for b in mem[fcb(17):fcb(28)])
            if self._find(d, new)[0]:
                bx = 0x08FF
            elif f is None:
                bx = 0x00FF
            elif f.name[8] & 0x80:
                bx = 0x03FF
            elif not self._pw_ok(mem, ds, f):
                bx = 0x07FF
            else:
                f.name = bytearray(c | (a & 0x80) for c, a in zip(new, f.name))
        elif cl == 30:                    # set file attributes: f1'-f4', t1'-t3'
            hits = self._matching(d, mem[fcb(1):fcb(12)])
            if not hits:
                bx = 0x00FF
            for f in hits:
                for i in (0, 1, 2, 3, 8, 9, 10):
                    f.name[i] = (f.name[i] & 0x7F) | (mem[fcb(1 + i)] & 0x80)
        elif cl == 102:                   # read XFCB: mode, key, the password encoded
            f = self._find(d, name)[0]
            if f is None or f.password is None:
                bx = 0xFF
            else:
                key = sum(f.password) & 0xFF
                mem[fcb(12)], mem[fcb(13)] = f.mode, key
                for i in range(8):
                    mem[fcb(23 - i)] = f.password[i] ^ key
        return bx & 0xFFFF, 0, es

    def snapshot(self):
        dirs = {chr(65 + d): [(n, s[0].user, bytes(s[0].name).hex(), s[1]) for n, s in enumerate(self.slots[d]) if s]
                for d in range(4)}
        files = {f'{chr(65 + d)}:{f.user}:{f.key().decode()}': (b''.join(f.recs), f.password)
                 for d in range(4) for f in self._files(d) if f.user != 15}
        return dirs, files, bytes(self.lst_out)


class PipProgram(Program):
    """PIP.PLM's code at PIP_CS, and its asm run with room for its
    character-at-a-time copies; main_bp, where the compiled PL/M keeps
    the main program's BP for GO TO RETRY (the C's is its jmp_buf), is
    left as the image has it"""

    def memory(self, setup=None):
        cs, rspref.CS = rspref.CS, PIP_CS
        try:
            return super().memory(setup)
        finally:
            rspref.CS = cs

    def run_asm(self, mem, mpm):
        mem = bytearray(mem)

        def hook(sim):
            r = sim.regs
            res = mpm.call(mem, r['ds'], r['es'], r['cx'] & 0xFF, r['dx'])
            if res is None:
                sim.halted = True
                return
            r['ax'] = r['bx'] = res[0]
            r['cx'] = res[1]
            r['es'] = res[2]

        regs = {'cs': PIP_CS, 'ds': DS, 'es': DS, 'ss': LOADER_SS, 'sp': 0x100}
        sim = I86Sim(mem, regs, pa(PIP_CS, 0), segmented=True, int_hooks={224: hook})
        sim.run(max_steps=60000000)
        assert sim.halted, f"{self.name}: the asm run didn't finish"
        w16(mem, pa(DS, rsp_syms.value('PIP_MAIN_BP')), 0)
        return mem


TEXT = b'Line one\r\n\tTabbed line, lower case\r\nMIXED Case\x0cform feed\r\n' \
       b'high\xc1\xc2 bits\r\nthe end\r\n\x1a'
LONG = b''.join(b'line %02d of the text\r\n' % i for i in range(60)) + b'\x1a'   # 1.3K
BIG = bytes(range(256)) * 160                                                   # 40K: 3 extents
HEX = b':0400000001020304F2\r\n:02001000AABB89\r\n:00000001FF\r\n\x1a'
BADHEX = b':0400000001020304F3\r\n:00000001FF\r\n\x1a'
OBJ = bytes([1, 2, 0x0C, 0x1A, 3, 0x0C, 4]) * 20

DISK = {'A:A.TXT': TEXT, 'A:B.TXT': b'second file\r\n\x1a', 'A:LONG.TXT': LONG, 'A:BIG.DAT': BIG,
        'A:C.DOC': b'a doc\r\n\x1a', 'A:A.TXT;1': b'user one\r\n\x1a', 'A:SYS.TXT': (b'system\r\n\x1a', {'sys': True}),
        'A:RO.TXT': (b'read only\r\n\x1a', {'ro': True}), 'A:PW.TXT': (b'secret\r\n\x1a', {'password': b'KEY'}),
        'A:Y.HEX': HEX, 'A:BAD.HEX': BADHEX, 'A:Y.BIN': OBJ, 'A:Y.CMD': OBJ,
        'A:ARC.TXT': (b'archived\r\n\x1a', {'arc': True, 'f1': True}), 'B:OLD.TXT': b'on b\r\n\x1a'}


def data(mpm, path):
    """a file's contents, 'A:0:NAME    TXT'"""
    return mpm.snapshot()[1].get(path, (None,))[0]


def main():
    prog = PipProgram('PIP', 'pip_main', rsp=False, plm=True,
                      stack=(rsp_syms.value('PIP_STACK_BASE') - 1024, rsp_syms.value('PIP_STACK_BASE')))
    ck = Checker()

    def tail(t):
        t = b' ' + t if t else b''

        def run(m):
            w16(m, pa(DS, 6), MAXB)  # MAXB: the data group's length (and its high byte)
            m[pa(DS, 8)] = 0
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = b'\0' + b' ' * 11 + bytes(21)  # the CLI's fcb
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    A = 'A:0:'
    padded = lambda b: b + b'\x1a' * ((-len(b)) % 128)  # noqa: E731
    cases = (
        # copies
        ("a file to a file", b'X.TXT=A.TXT', {},
         lambda m: data(m, A + 'X       TXT') == padded(TEXT)),
        ("a file to another drive", b'B:=A.TXT', {},
         lambda m: data(m, 'B:0:A       TXT') == padded(TEXT)),
        ("a file to another drive, renamed", b'B:NEW.TXT=A:A.TXT', {},
         lambda m: data(m, 'B:0:NEW     TXT') == padded(TEXT)),
        ("a file from another drive", b'OLD.TXT=B:', {},
         lambda m: data(m, A + 'OLD     TXT') == padded(b'on b\r\n\x1a')),
        ("over an existing file", b'B.TXT=A.TXT', {},
         lambda m: data(m, A + 'B       TXT') == padded(TEXT)),
        ("a 40K file: three extents, fast copy refills", b'B:=BIG.DAT', {},
         lambda m: data(m, 'B:0:BIG     DAT') == BIG),
        ("a 40K file, [V] verified", b'B:=BIG.DAT[V]', {},
         lambda m: data(m, 'B:0:BIG     DAT') == BIG),
        ("a character copy refilling the buffers ([U])", b'X.TXT=LONG.TXT[U]', {},
         lambda m: data(m, A + 'X       TXT') == padded(LONG.upper())),
        ("from user 1 ([G1])", b'X.TXT=A.TXT[G1]', {},
         lambda m: data(m, A + 'X       TXT') == padded(b'user one\r\n\x1a')),
        ("to user 2 ([G2] on the destination)", b'A.TXT[G2]=A.TXT', {},
         lambda m: data(m, 'A:2:A       TXT') == padded(TEXT)),
        ("from user 0 to user 3, as user 3", b'X.TXT=A.TXT[G0]', {'user': 3},
         lambda m: data(m, 'A:3:X       TXT') == padded(TEXT)),
        ("wildcards: several files, one failing (a password)", b'B:=*.TXT', {},
         lambda m: all(data(m, 'B:0:' + n) for n in ('A       TXT', 'B       TXT', 'LONG    TXT', 'ARC     TXT'))
         and b'INVALID PASSWORD - A:PW.TXT' in m.out and data(m, 'B:0:SYS     TXT') is None),
        ("wildcards, from user 1", b'B:=A:*.*[G1]', {},
         lambda m: data(m, 'B:0:A       TXT') == padded(b'user one\r\n\x1a')),
        ("wildcards, [K] no names shown", b'B:=?.TXT[K]', {},
         lambda m: data(m, 'B:0:B       TXT') is not None and b'COPYING' not in m.out),
        ("wildcards, [A] archive: only unarchived files", b'B:=*.TXT[A]', {},
         lambda m: data(m, 'B:0:ARC     TXT') is None and data(m, 'B:0:A       TXT') is not None),
        ("wildcards, an archived file whose first extent isn't first", b'B:=*.DAT',
         {'disk': {'A:BIG.DAT': (BIG, {'arc': True})}, 'reverse': True},
         lambda m: data(m, 'B:0:BIG     DAT') == BIG),
        ("wildcards, the same drive and user", b'A:=*.TXT', {},
         lambda m: b'INVALID FORMAT' in m.out),
        ("concatenation", b'X.TXT=A.TXT,B.TXT', {},
         lambda m: data(m, A + 'X       TXT') == padded(TEXT[:-1] + b'second file\r\n\x1a')),
        ("concatenation with EOF: and NUL:", b'X.TXT=NUL:,B.TXT,EOF:', {},
         lambda m: data(m, A + 'X       TXT').startswith(bytes(40) + b'second file\r\n\x1a\x1a')),
        ("interactive: two commands, then an empty line", b'',
         {'lines': [b'X.TXT=A.TXT', b'B:=B.TXT', b'']},
         lambda m: b'*' in m.out and data(m, 'B:0:B       TXT') and data(m, A + 'X       TXT')),
        ("interactive: an error, then a command", b'',
         {'lines': [b'X.TXT=NONE.TXT', b'Y.TXT=B.TXT', b'']},
         lambda m: b'FILE NOT FOUND' in m.out and data(m, A + 'Y       TXT')),
        ("interactive: a wildcard copy", b'', {'lines': [b'C:=*.DOC', b'']},
         lambda m: data(m, 'C:0:C       DOC')),
        # devices
        ("to the console", b'CON:=A.TXT', {}, lambda m: b'Tabbed' in m.out),
        ("to the printer (LST:)", b'LST:=A.TXT', {}, lambda m: b'MIXED' in m.lst_out),
        ("to the printer (PRN:): numbers, tabs, pages", b'PRN:=LONG.TXT', {},
         lambda m: b'    60: line 59' in m.lst_out and m.lst_out.count(b'\x0c') == 1),
        ("the printer busy", b'LST:=A.TXT', {'lst_busy': True}, lambda m: b'PRINTER BUSY' in m.out),
        ("from the console", b'X.TXT=CON:', {'chars': b'typed in\r\n\x1a'},
         lambda m: data(m, A + 'X       TXT') == padded(b'typed in\r\n\x1a')),
        ("console to printer", b'LST:=CON:', {'chars': b'abc\x1a'}, lambda m: m.lst_out == b'abc'),
        ("an invalid destination (PUN:)", b'PUN:=A.TXT', {}, lambda m: b'INVALID DESTINATION' in m.out),
        ("an invalid source (RDR:)", b'CON:=RDR:', {}, lambda m: b'INVALID SOURCE' in m.out),
        ("an invalid device", b'XYZ:=A.TXT', {}, lambda m: b'ERROR' in m.out),
        # parameters
        ("[E] echo", b'X.TXT=B.TXT[E]', {}, lambda m: b'second file' in m.out),
        ("[N] line numbers", b'X.TXT=A.TXT[N]', {},
         lambda m: data(m, A + 'X       TXT').startswith(b'     1: Line one')),
        # past 99: the hundreds digit is DEC(C2 PLUS 0)'s carry (until
        # plmc's PLUS added it, 100 came out as "  1": docs/bugs.md #131)
        ("[N] past line 99", b'X.TXT=M.TXT[N]',
         {'disk': {**DISK, 'A:M.TXT': b''.join(b'l%d\r\n' % i for i in range(1, 106)) + b'\x1a'}},
         lambda m: b'   100: l100\r\n   101: l101' in data(m, A + 'X       TXT')),
        ("[N2] numbers with zeros, a tab", b'X.TXT=B.TXT[N2]', {},
         lambda m: data(m, A + 'X       TXT').startswith(b'000001\tsecond')),
        ("[T8] tabs", b'X.TXT=A.TXT[T8]', {},
         lambda m: b'\r\n        Tabbed' in data(m, A + 'X       TXT')),
        ("[U] upper case", b'X.TXT=A.TXT[U]', {}, lambda m: b'LOWER CASE' in data(m, A + 'X       TXT')),
        ("[L] lower case", b'X.TXT=A.TXT[L]', {}, lambda m: b'mixed case' in data(m, A + 'X       TXT')),
        ("[Z] zero parity", b'X.TXT=A.TXT[Z]', {}, lambda m: b'highAB bits' in data(m, A + 'X       TXT')),
        ("[S..^Z] start string", b'X.TXT=LONG.TXT[Sline 58\x1a]', {},
         lambda m: data(m, A + 'X       TXT') == padded(b'line 58 of the text\r\nline 59 of the text\r\n\x1a')),
        ("[Q..^Z] quit string", b'X.TXT=LONG.TXT[Qline 01\x1a]', {},
         lambda m: data(m, A + 'X       TXT') == padded(b'line 00 of the text\r\nline 01\r\n\x1a')),
        ("[S..^ZQ..^Z] both", b'CON:=LONG.TXT[Sline 10\x1aQline 12\x1a]', {},
         lambda m: m.out.endswith(b'line 10 of the text\r\nline 11 of the text\r\nline 12\r\n')),
        ("[S] not found", b'X.TXT=B.TXT[Sxyz\x1a]', {}, lambda m: b'START NOT FOUND' in m.out),
        ("[Q] not found", b'X.TXT=B.TXT[Qxyz\x1a]', {}, lambda m: b'QUIT NOT FOUND' in m.out),
        ("[P5] pages", b'X.TXT=LONG.TXT[P5]', {},
         lambda m: data(m, A + 'X       TXT').count(b'\x0c') == 13),
        ("[F] form feeds removed", b'X.TXT=A.TXT[F]', {}, lambda m: b'\x0c' not in data(m, A + 'X       TXT')),
        ("[D10] delete past column 10", b'X.TXT=LONG.TXT[D10]', {},
         lambda m: data(m, A + 'X       TXT').startswith(b'line 00 of\r\n')),
        ("[O] object file: past its ^Zs", b'X.BIN=Y.BIN[O],B.TXT', {},
         lambda m: data(m, A + 'X       BIN') == padded(padded(OBJ) + b'second file\r\n\x1a')),
        ("[O] file to file: a fast copy", b'X.BIN=Y.BIN[O]', {},
         lambda m: data(m, A + 'X       BIN') == padded(OBJ)),
        ("a .CMD file: past its ^Zs", b'X.BIN=Y.CMD[F]', {},
         lambda m: data(m, A + 'X       BIN').startswith(bytes([1, 2, 0x1A, 3, 4, 1, 2]))),
        ("a text file stops at its ^Z", b'X.BIN=Y.BIN[F]', {},
         lambda m: data(m, A + 'X       BIN') == padded(bytes([1, 2, 0x1A]))),
        ("[H] hex records", b'X.HEX=Y.HEX[H]', {},
         lambda m: data(m, A + 'X       HEX') == padded(HEX + b'\r\n')),
        ("[I] hex, the :00 record left out", b'X.HEX=Y.HEX[I]', {},
         lambda m: data(m, A + 'X       HEX') == padded(HEX[:-14] + b'\r\n\x1a')),
        ("[H] a bad checksum, then ^Z", b'X.HEX=BAD.HEX[H]', {'chars': b'\x1a'},
         lambda m: b'CHECKSUM ERROR' in m.out),
        ("to an .H86 file", b'X.H86=Y.HEX', {}, lambda m: data(m, A + 'X       H86') is not None),
        ("[B] block mode, an XOFF", b'X.TXT=B.TXT[B]', {'disk': {**DISK, 'A:B.TXT': b'ab\x13cd\r\n\x1a'}},
         lambda m: data(m, A + 'X       TXT') == padded(b'abcd\r\n\x1a')),
        ("[R] a SYS file", b'X.TXT=SYS.TXT[R]', {},
         lambda m: data(m, A + 'X       TXT') == padded(b'system\r\n\x1a')),
        ("a SYS file without [R]", b'X.TXT=SYS.TXT', {}, lambda m: b'FILE NOT FOUND' in m.out),
        ("user 0's SYS file from user 4 ([R])", b'X.TXT=SYS.TXT[R]', {'user': 4},
         lambda m: data(m, 'A:4:X       TXT') == padded(b'system\r\n\x1a')),
        ("the source's attributes go with it", b'B:=ARC.TXT', {},
         lambda m: m.snapshot()[0]['B'][1][2] == bytes(attr_name(name11('ARC.TXT'), f1=True)).hex()),
        # a named destination takes the source's password, as a drive does
        # (until bugs.md #134 any destination name cleared getpw)
        ("a password: given", b'X.TXT=PW.TXT;KEY', {},
         lambda m: data(m, A + 'X       TXT') == padded(b'secret\r\n\x1a')
         and m.snapshot()[1][A + 'X       TXT'][1] == b'KEY     '),
        ("a password: the destination's own wins", b'X.TXT;NEW=PW.TXT;KEY', {},
         lambda m: m.snapshot()[1][A + 'X       TXT'][1] == b'NEW     '),
        ("no password, none given to the destination", b'X.TXT=A.TXT', {},
         lambda m: m.snapshot()[1][A + 'X       TXT'][1] is None),
        ("a password: missing", b'X.TXT=PW.TXT', {}, lambda m: b'INVALID PASSWORD' in m.out),
        ("a password: kept, to a drive", b'B:=PW.TXT;KEY', {},
         lambda m: m.snapshot()[1]['B:0:PW      TXT'][1] == b'KEY     '),
        ("a password on the destination", b'X.TXT;NEW=B.TXT', {},
         lambda m: m.snapshot()[1][A + 'X       TXT'][1] == b'NEW     '),
        # errors
        ("no file", b'X.TXT=NONE.TXT', {}, lambda m: b'ERROR - FILE NOT FOUND  - A:NONE.TXT' in m.out),
        ("no file for a wildcard", b'B:=*.ZZZ', {}, lambda m: b'FILE NOT FOUND' in m.out),
        ("a bad parameter", b'X.TXT=A.TXT[$]', {}, lambda m: b'BAD PARAMETER' in m.out),
        ("a bad user number", b'X.TXT=A.TXT[G40]', {}, lambda m: b'INVALID USER NUMBER' in m.out),
        # drives are A..P (until bugs.md #134 Q: got through to the BDOS)
        ("drive Q: isn't one", b'Q:X.TXT=A.TXT', {}, lambda m: b'INVALID FORMAT' in m.out and not any(
            k.startswith('Q:') for k in m.snapshot()[1])),
        ("an invalid format", b'X.TXT A.TXT', {}, lambda m: b'INVALID FORMAT' in m.out),
        ("an invalid separator", b'X.TXT=A.TXT B.TXT', {}, lambda m: b'INVALID SEPARATOR' in m.out),
        ("an ambiguous destination", b'*.TXT=A.TXT', {}, lambda m: b'INVALID FORMAT' in m.out),
        ("the disk full", b'B:=BIG.DAT', {'capacity': {1: 100}},
         lambda m: b'DISK WRITE' in m.out and data(m, 'B:0:BIG     DAT') is None),
        ("the disk full, a character copy", b'B:=LONG.TXT[U]', {'capacity': {1: 5}},
         lambda m: b'DISK WRITE' in m.out),
        ("the directory full", b'X.TXT=A.TXT', {'full': True}, lambda m: b'NO DIRECTORY SPACE' in m.out),
        ("the directory full for a big file's second extent", b'B.TXT=BIG.DAT', {'full': 1, 'disk':
         {'A:BIG.DAT': BIG, 'A:B.TXT': b'b\x1a'}},
         lambda m: b'DISK WRITE  - A:B.$$$' in m.out and data(m, A + 'B       TXT') == padded(b'b\x1a')),
        ("the directory full in a wildcard copy: retried for each file", b'A:=B:*.TXT',
         {'full': True, 'disk': {'B:A.TXT': b'a\x1a', 'B:B.TXT': b'b\x1a'}},
         lambda m: m.out.count(b'NO DIRECTORY SPACE') == 2),
        ("a R/O destination: delete it", b'RO.TXT=B.TXT', {'chars': b'y'},
         lambda m: data(m, A + 'RO      TXT') == padded(b'second file\r\n\x1a')),
        ("a R/O destination: keep it", b'RO.TXT=B.TXT', {'chars': b'n'},
         lambda m: b'NOT DELETED' in m.out and data(m, A + 'RO      TXT') == padded(b'read only\r\n\x1a')),
        ("a R/O destination, [W]", b'RO.TXT=B.TXT[W]', {},
         lambda m: b'R/O' not in m.out and data(m, A + 'RO      TXT') == padded(b'second file\r\n\x1a')),
        ("a R/O source keeps its attribute", b'B:=RO.TXT', {},
         lambda m: m.snapshot()[0]['B'][1][2] == bytes(attr_name(name11('RO.TXT'), ro=True)).hex()),
        ("a key stops a copy", b'CON:=LONG.TXT', {'chars': b'x', 'typeahead': True},
         lambda m: b'USER ABORTED' in m.out),
        ("^Z ends a copy early", b'X.TXT=LONG.TXT[U]', {'chars': b'\x1a', 'typeahead': True},
         lambda m: data(m, A + 'X       TXT') == padded(b'LINE 00 OF THE TEXT\r\x1a')),
        ("the wrong MP/M", b'X.TXT=A.TXT', {'version': 0x1022}, lambda m: b'REQUIRES MP/M-86' in m.out),
    )
    for label, t, kw, expect in cases:
        args = {'disk': DISK, **kw}
        ref, _ = compare(ck, prog, label, args, setup=tail(t), snap=lambda m, mpm: mpm.snapshot(),
                         mpm_class=PipMpm)
        ck.check(f"{label}: does what PIP does", True, bool(expect(ref)))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
