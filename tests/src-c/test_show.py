#!/usr/bin/env python3
"""src-c/utils/show.c against src-assem/utils/SHOW.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): SHOW [d:][SPACE | USERS | LABEL | DRIVE | HELP]
shows each logged-in drive's R/O status and free space (or the named
drive's), the active user and the users with files, the directory label
and its stamps, and the disk parameter block. Each case is a command
tail and a system of drives; besides the two runs agreeing, each case
checks that the reference printed what those drives really hold.

rspref's Mpm has one directory and no disk parameters, so ShowMpm adds
the disk system SHOW asks about: drives, each with its DPB, allocation
vector, 64-entry directory and label; and functions 14 (select: logs
the drive in, an unknown drive ends the run), 24 (login vector), 27
(allocation vector address), 29 (R/O vector), 31 (DPB address, ES:BX),
46 (free space) and 101 (label data).

SHOW's code (0x2995 bytes) is too long for rspref's CS (0100h): it
would run into the data segment at 0200h, so this test moves the code
to 0800h.
"""
import datetime
import sys

import rspref
from rspref import Checker, DS, SYSDAT, Mpm, Program, compare, pa, w16, DIR_ENTRIES
import rsp_syms

rspref.CS = 0x0800  # see above: 8000h-A995h, clear of the data, UDA, SYSDAT and VSEG

DPB_OFF = 0x0600  # each drive's DPB, in SYSDAT (the BDOS's data, as MP/M-86 returns it)
ALV_OFF = 0x0C00  # each drive's allocation vector for function 27 (a pre-3.0 BDOS's, in DS)


class Drive:
    """a drive: its DPB (spt, bsh, blm, exm, dsm, drm, al0, al1, cks,
    off), how many blocks files hold (after the directory's), its
    directory entries beyond the files, and its label (mode, name11,
    create and update stamps as datetimes) or None"""

    def __init__(self, dpb, used, label=None, entries=()):
        self.spt, self.bsh, self.blm, self.exm, self.dsm, self.drm, self.al0, self.al1, self.cks, self.off = dpb
        self.used = used
        self.label = label
        self.entries = list(entries)

    def dpb_bytes(self):
        b = bytearray(17)
        w16(b, 0, self.spt)
        b[2], b[3], b[4] = self.bsh, self.blm, self.exm
        w16(b, 5, self.dsm)
        w16(b, 7, self.drm)
        b[9], b[10] = self.al0, self.al1
        w16(b, 11, self.cks)
        w16(b, 13, self.off)
        b[15], b[16] = 0, 0  # psh, phm (128-byte sectors)
        return bytes(b)

    def dir_blocks(self):
        return bin((self.al0 << 8) | self.al1).count('1')

    def alv(self):
        """the allocation vector: the directory's blocks (AL0/AL1), then
        the files', one bit a block, high bit first"""
        v = bytearray(self.dsm // 8 + 1)
        for n in range(self.dir_blocks() + self.used):
            v[n >> 3] |= 0x80 >> (n & 7)
        return bytes(v)

    def free_records(self):
        return (self.dsm + 1 - self.dir_blocks() - self.used) << self.bsh


def stamp(t):
    """a 4-byte date and time stamp: days from 1 Jan 1978 = 1, BCD
    hours and minutes"""
    days = (t.date() - datetime.date(1977, 12, 31)).days
    return bytes([days & 0xFF, days >> 8, int(f'{t.hour:02d}', 16), int(f'{t.minute:02d}', 16)])


def label_entry(mode, name11, create, update):
    e = bytearray(32)
    e[0] = 0x20
    e[1:12] = name11
    e[12] = mode
    e[16:24] = b' ' * 8  # no password
    e[24:28] = stamp(create)
    e[28:32] = stamp(update)
    return e


class ShowMpm(Mpm):
    """Mpm with drives (see the module docstring): drives = {n: Drive};
    drive 0's directory is Mpm's (its files), plus its own entries;
    login, ro -- the login and R/O vectors"""

    def __init__(self, drives=None, login=1, ro=0, **kw):
        super().__init__(**kw)
        self.drives = drives or {}
        self.login, self.ro = login, ro
        self.dirs = {}
        for n, d in self.drives.items():
            dr = self.dir if n == 0 else [bytearray(b'\xe5' * 32) for _ in range(DIR_ENTRIES)]
            extra = [bytearray(e) for e in d.entries]
            if d.label:
                extra.append(label_entry(*d.label))
            for e in extra:
                dr[next(k for k in range(DIR_ENTRIES) if dr[k][0] == 0xE5)] = e
            self.dirs[n] = dr
        if self.disk in self.dirs:
            self.dir = self.dirs[self.disk]

    def call(self, mem, ds, es, cl, dx):
        dl = dx & 0xFF
        if cl in (24, 27, 29, 31):    # no argument
            self.log.append((cl,))
            if cl == 24:              # get login vector
                return self.login, 0, es
            if cl == 29:              # get R/O vector
                return self.ro, 0, es
            d = self.drives[self.disk]
            if cl == 31:              # get DPB address: ES:BX, in the BDOS's segment
                off = DPB_OFF + 0x20 * self.disk
                mem[pa(SYSDAT, off):pa(SYSDAT, off) + 17] = d.dpb_bytes()
                return off, 0, SYSDAT
            off = ALV_OFF + 0x200 * self.disk  # get allocation vector address
            v = d.alv()
            mem[pa(ds, off):pa(ds, off) + len(v)] = v
            return off, 0, ds
        if cl in (46, 101):           # DL = the drive
            self.log.append((cl, dl))
            if dl not in self.drives:
                return None
            self.login |= 1 << dl     # the BDOS logs it in
            d = self.drives[dl]
            if cl == 46:              # get free space: 3-byte record count at the DMA
                r = d.free_records()
                a = self._dma(ds)
                mem[a:a + 3] = bytes([r & 0xFF, (r >> 8) & 0xFF, (r >> 16) & 0xFF])
                return 0, 0, es
            return (d.label[0] if d.label else 0), 0, es  # get label data
        if cl == 14:                  # select disk: logs it in; no such drive -> BDOS error, ended
            if dl not in self.drives:
                self.log.append((cl, dl))
                return None
            self.login |= 1 << dl
            self.dir = self.dirs[dl]
        if cl in (17, 18):            # search the current drive's directory
            self.dir = self.dirs[self.disk]
        return super().call(mem, ds, es, cl, dx)


# the drives: A is the src-c floppy (src-c/tools/mkdisk.py), B an 8"
# single-density floppy, C a 64M hard disk, D a 5" double-density floppy, not
# logged in
FLOPPY = (72, 6, 63, 0, 176, 63, 0x80, 0, 16, 2)
SSSD = (26, 3, 7, 0, 242, 63, 0xC0, 0, 16, 2)
HARD = (128, 7, 127, 7, 4095, 1023, 0xC0, 0, 0, 1)
SMALL = (32, 4, 15, 1, 155, 63, 0x80, 0, 16, 2)

FILES = {b'MPM     SYS': b'm' * 300, b'SHOW    CMD': b's' * 200, b'NOTES   TXT': (b'n', {'user': 1}),
         b'PROG    A86': (b'p', {'user': 3}), b'LAST    TXT': (b'l', {'user': 15})}
XFCB3 = bytes([0x13]) + b'PROG    A86' + bytes(20)  # user 3's XFCB (a password on PROG.A86)
WHEN = (datetime.datetime(1981, 8, 4, 9, 5), datetime.datetime(1982, 2, 28, 17, 45))
LEAP = (datetime.datetime(1980, 2, 29, 23, 59), datetime.datetime(1980, 12, 31, 0, 0))
Y2K = (datetime.datetime(1999, 12, 31, 12, 30), datetime.datetime(2001, 3, 1, 8, 0))


def system(label_a=None, label_c=None):
    return {0: Drive(FLOPPY, 20, label_a, [XFCB3]), 1: Drive(SSSD, 50), 2: Drive(HARD, 10, label_c),
            3: Drive(SMALL, 7)}


# what SHOW should print, from the drives (independent of the PL/M)
def kb(n):
    return f'{n:,}'.rjust(9)


def space(n, d, ro, old=False):
    k = d.free_records() >> 3
    return f"\r\n{chr(65 + n)}: R{'O' if ro else 'W'}, Space: {k if old else kb(k)}k".encode()


def drive_text(n, d):
    recs = (d.dsm + 1) << d.bsh
    rows = [(recs, '128 Byte Record Capacity'), (recs >> 3, 'Kilobyte Drive  Capacity'),
            (d.drm + 1, '32 Byte  Directory Entries'), ((d.cks << 2) & 0xFFFF, 'Checked  Directory Entries'),
            ((d.exm + 1) * 128, 'Records / Directory Entry'), (1 << d.bsh, 'Records / Block'),
            (d.spt, 'Sectors / Track'), (d.off, 'Reserved  Tracks')]
    return (f"\r\n        {chr(65 + n)}: Drive Characteristics" +
            ''.join(f"\r\n{kb(v)}: {t}" for v, t in rows)).encode()


def users_text(n, user, dirs):
    have = sorted({e[0] & 0x0F for e in dirs if e[0] != 0xE5})
    return (f"\r\n{chr(65 + n)}: Active User :{user:3d}\r\n{chr(65 + n)}: Active Files:" +
            ''.join(f'{u:3d}' for u in have)).encode()


def ts(t):
    return f'{t.month:02d}/{t.day:02d}/{t.year % 100:02d} {t.hour:02d}:{t.minute:02d}'


def label_text(n, mode, name11, create, update):
    name = name11[:8] + b'.' + name11[8:]
    make = mode & 0x10
    row = (name.decode() + ('    on   ' if mode & 0x80 else '    off  ') + ('   on  ' if make else '   off ') +
           ('   on   ' if mode & 0x40 or make else '   off  ') + ('   on ' if mode & 0x20 else '   off') +
           '    ' + ts(create) + '  ' + ts(update))
    head = 'Access' if mode & 0x40 else 'Create'
    return (f"\r\nLabel for drive {chr(65 + n)}: \r\n" +
            "\r\nDirectory     Passwds  Make   Stamp   Stamp" +
            "\r\nLabel         Reqd     XFCBs  " + head + "  Update  Label Created   Label Updated" +
            "\r\n------------  -------  -----  ------  ------  --------------  --------------\r\n" +
            row + "\r\n").encode()


HELP = (b"\r\nDrive Status    : SHOW DRIVE:  SHOW d:DRIVE:\r\nUser Status     : SHOW USERS:  SHOW d:USERS:"
        b"\r\nDirectory Label : SHOW LABEL:  SHOW d:LABEL:\r\nFree Disk Space : SHOW SPACE:  SHOW d:SPACE:\r\n")
BAD = b"\r\nSHOW 2.0\r\n\r\nInvalid Option, use the following:\r\n" + HELP


def main():
    prog = Program('SHOW', 'show_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('SHOW_STACK_BASE') - 1024, rsp_syms.value('SHOW_STACK_BASE')))
    ck = Checker()

    def tail(t):
        t = b' ' + t if t else b''

        def run(m):
            n = len(prog.code)  # what LOAD leaves in the base page: the code group's length
            m[pa(DS, 0):pa(DS, 3)] = bytes([n & 0xFF, (n >> 8) & 0xFF, n >> 16])
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = b'\0' + b' ' * 11 + bytes(21)  # the CLI's fcbs
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + b' ' * 11
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    s = system()
    la = (0x61, b'SYSDISK MPM', *WHEN)   # access and update stamps
    lc = (0xB1, b'HARDDISK   ', *LEAP)   # passwords, make XFCBs, update stamps
    ly = (0x31, b'Y2K     LBL', *Y2K)    # make XFCBs, update stamps: the years past 99
    sl = system(la, lc)
    users_a = [e for e in ShowMpm(drives=system(), files=FILES).dirs[0]]
    std = {'files': FILES, 'drives': s, 'login': 0b0111, 'ro': 0b0010}
    # MP/M 1.x (cp/m 2.2's BDOS): free space from the allocation vector,
    # a bit at a time (not the hard disk: too many steps for i86sim)
    old = {**std, 'version': 0x0122, 'login': 0b1011}

    cases = (
        ("no tail: every logged-in drive's space", b'', std,
         [space(0, s[0], 0), space(1, s[1], 1), space(2, s[2], 0)]),
        ("SPACE", b'SPACE', std, [space(0, s[0], 0) + space(1, s[1], 1) + space(2, s[2], 0)]),
        ("[SPACE]: the global option form", b'[SPACE]', std, [space(1, s[1], 1)]),
        ("B: just that drive", b'B:', std, [space(1, s[1], 1)]),
        ("C:SPACE: the hard disk, over 64K records", b'C:SPACE', std, [space(2, s[2], 0)]),
        ("D: not logged in: selecting logs it in", b'D:', std, [space(3, s[3], 0)]),
        ("SPACE USERS: two items, USERS on the last drive SPACE selected", b'SPACE USERS', std,
         [space(2, s[2], 0) + users_text(2, 0, [])]),
        ("USERS: users 0, 1, 3, 15 have files", b'USERS', std, [users_text(0, 0, users_a)]),
        ("B:USERS, user 5 active, no files", b'B:USERS', {**std, 'user': 5}, [users_text(1, 5, [])]),
        ("LABEL: access and update stamps", b'LABEL', {**std, 'drives': sl}, [label_text(0, *la)]),
        ("C:LABEL: passwords, XFCBs, a leap day", b'C:LABEL', {**std, 'drives': sl}, [label_text(2, *lc)]),
        ("LABEL: stamps past 1999", b'LABEL', {**std, 'drives': system(ly)}, []),
        ("B:LABEL: no label", b'B:LABEL', std, [b"\r\nNo Directory Label exists\r\n"]),
        ("DRIVE: every logged-in drive", b'DRIVE', std, [drive_text(0, s[0]) + b'\r\n', drive_text(1, s[1]),
                                                         drive_text(2, s[2])]),
        ("A:DRIVE: the src-c floppy", b'A:DRIVE', std, [drive_text(0, s[0])]),
        ("C:DRIVE: the hard disk", b'C:DRIVE:', std, [drive_text(2, s[2])]),
        ("HELP", b'HELP', std, [HELP]),
        ("OPEN: not yet implemented", b'OPEN', std, [b"\r\nNot yet implemented"]),
        ("LOCKED: not yet implemented", b'LOCKED', std, [b"\r\nNot yet implemented"]),
        ("a bad option", b'XYZ', std, [BAD]),
        ("a bad L option", b'LX', std, [BAD]),
        ("a bad entry", b'TOOLONGNAME', std, [BAD]),
        ("Q: past P: a parse error", b'Q:', std, BAD),
        ("E: no such drive: the select error ends it", b'E:', std, b''),
        ("a key stops it at the first line", b'DRIVE', {**std, 'chars': b'x'}, b"\r\n"),
        ("MP/M 1.x: space from the allocation vector", b'', old,
         [space(0, s[0], 0, True) + space(1, s[1], 1, True) + space(3, s[3], 0, True)]),
        # pvalue's zeros: until plmc let its local ZERO hide the flag,
        # the 0s inside a number were dropped (docs/bugs.md #131)
        ("MP/M 1.x: a number with zeros inside", b'A:', {**old, 'drives': {**s, 0: Drive(FLOPPY, 26)}},
         [space(0, Drive(FLOPPY, 26), 0, True), b'\r\nA: RW, Space: 1200k']),
        ("MP/M 1.x: LABEL requires MP/M 2.0", b'LABEL', old, [b"\r\nRequires MP/M 2.0"]),
        ("MP/M 1.x: OPEN requires MP/M 2.0", b'OPEN', old, [b"\r\nRequires MP/M 2.0"]),
        ("CP/M 1.4: requires CP/M 2.0", b'', {**std, 'version': 0x0014}, [b"Requires CP/M 2.0"]),
    )
    for label, t, kw, expect in cases:
        ref, _ = compare(ck, prog, label, kw, setup=tail(t), snap=lambda m, mpm: (mpm.disk, mpm.login),
                         mpm_class=ShowMpm)
        if isinstance(expect, bytes):  # all it prints
            ck.check(f"{label}: prints just that", expect, bytes(ref.out))
        else:
            ck.check(f"{label}: shows what the drives hold", [], [e for e in expect if e not in bytes(ref.out)])
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
