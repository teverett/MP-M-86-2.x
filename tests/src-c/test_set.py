#!/usr/bin/env python3
"""src-c/utils/set.c against src-assem/utils/SET.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): SET sets file attributes (RO/RW, SYS/DIR,
ARCHIVE, F1-F4), file passwords, protection modes and time stamping
(the XFCB), the directory label (name, password, protection, stamps,
make XFCBs), the default password, and drive R/O-R/W status. Each case
is a command tail and a disk; the result is the directory afterwards
(entries, label, XFCBs), the drive's label byte and R/O vector, and the
default passwords in this process's UDA and its parent's.

rspref's scripted MP/M doesn't have the label, XFCB, attribute and
drive functions, so SetMpm adds them, after the MP/M-86 BDOS
(src-assem/kernel/FILE2-4.BDO): directory labels are entries with
user byte 20h, XFCBs entries with 10h + user, passwords are stored
reversed and XORed with their checksum (SET_PW), and checked against
the DMA or the default password (CMP_PW). The functions that search
the directory themselves (30, 100, 102, 103) leave the UDA's search
place behind them as the BDOS does, so a wildcard that didn't restore
it would stop. Console status is ready only for characters typed
ahead ('typed'); 'chars' are typed when SET waits for them, after its
prompt.

Each case also names text the reference's output must (or, with a
leading '!', mustn't) have, so a case can't pass with both sides
failing alike. Some of it is the compiled SET's own behaviour: plmc
gives cmd(27) only its 13 initial bytes, so each parse's fcb runs into
passwd and a password given before another option, or typed for the
label, is lost (see set.c). SET's code is longer than the 4K rspref
leaves below its data, so SetProgram runs the asm's code at CODE_SEG.
-v prints every case's output and result.
"""
import sys

from rspref import (Checker, DS, LOADER_SS, Mpm, NO_ARG, Program, SYSDAT, TOD, UDA_SEG, PD, PARENT, P_PARENT, P_UDA,
                    VSEG, U_DCNT, U_INITCS, U_SEARCHA, U_SEARCHL, DIR_ENTRIES, compare, pa, w16)
import rsp_syms
from i86sim import I86Sim  # noqa: E402 (rspref puts tools/ on the path)

DPB_OFF = 0x40      # the DPB function 31 returns, at VSEG:DPB_OFF
PARENT_UDA = 0x0480  # the parent process's UDA
U_DF_PASSWORD = 0x12
TOD_BYTES = b'\x34\x12\x10\x30'
VERBOSE = '-v' in sys.argv


def encrypt(pw):
    """the BDOS's SET_PW: (password bytes 16-23, checksum, null?)"""
    pw = pw.ljust(8)[:8]
    ch = sum(pw) & 0xFF
    e = bytearray(8)
    for k in range(8):
        e[7 - k] = pw[k] ^ ch
    return bytes(e), ch, ch == 0 and all(b in (0, 0x20) for b in pw)


class SetMpm(Mpm):
    """Mpm plus the functions SET uses:
      28  write protect: the current disk joins the R/O vector (0FFFFh if deny)
      30  set file indicators: f1'-f4', t1'-t3' from the fcb into every
          extent; error 9 for a wildcard, 7 for a protected file whose
          password isn't at the DMA (passwords on in the label); 0FFh
          when there's no file
      31  the DPB (ES:BX), with extent mask exm
      37  reset drive: the vector leaves the R/O vector (0FFFFh if deny)
      100 write label: makes one (0FFh with no directory space), checks
          its password (DMA 0-7), takes the fcb's name and data byte
          (| 1), and a new password (DMA 8-15) if bit 0; error 2 on an
          R/O drive. The fcb's byte 0 comes back as 20h.
      101 the label's data byte (0, no label)
      102 read XFCB into the fcb (32 bytes), 0FFh when none
      103 write XFCB: makes one if the label exists and the file does
          (else 0FFh), checks the password of a protected one, sets the
          mode (fcb(12) and 0E0h, or 80h) and with bit 0 a password
      106 default password, into the UDA (encrypted, as the BDOS keeps it)
    label  -- (name, data byte, password) or None
    xfcbs  -- {name: (mode, password)}
    attrs  -- {name: byte offsets whose attribute bit is set}
    full   -- fill the empty directory entries
    typed  -- characters typed ahead (console status is ready)"""

    def __init__(self, label=None, xfcbs=None, attrs=None, rodsk=0, deny=False, full=False, typed=b'', dpb_exm=0,
                 **kw):
        super().__init__(**kw)
        for name, offs in (attrs or {}).items():
            for e in self.dir:
                if e[0] != 0xE5 and bytes(b & 0x7F for b in e[1:12]) == name:
                    for i in offs:
                        e[i] |= 0x80
        self.drvlbl = 0
        if label:
            name, data, pw = label
            e = bytearray(32)
            e[0], e[1:12], e[12] = 0x20, name, data | 1
            if pw:
                e[16:24], e[13], _ = encrypt(pw)
            self.dir[self._free()] = e
            self.drvlbl = e[12]
        for name, (mode, pw) in (xfcbs or {}).items():
            e = bytearray(32)
            e[0], e[1:12], e[12] = 0x10 | self.user, name, mode
            if pw:
                e[16:24], e[13], _ = encrypt(pw)
            self.dir[self._free()] = e
        if full:
            for n in range(DIR_ENTRIES):
                if self.dir[n][0] == 0xE5:
                    self.dir[n] = bytearray(b'\x05' + b'FILLER  %03d' % n + bytes(20))
        self.rodsk, self.deny, self.typed, self.dpb_exm = rodsk, deny, list(typed), dpb_exm

    def _free(self):
        return next((n for n in range(DIR_ENTRIES) if self.dir[n][0] == 0xE5), None)

    @staticmethod
    def _key(e):
        return bytes(b & 0x7F for b in e[1:12])

    def _xfcb(self, name):
        return next((n for n in range(DIR_ENTRIES) if self.dir[n][0] == 0x10 | self.user
                     and self._key(self.dir[n]) == name), None)

    def _cmp_pw(self, mem, e, cur):
        """CMP_PW: a null password, the DMA's, or the default one"""
        ch = e[13]
        if ch == 0 and all(b in (0, 0x20) for b in e[16:24]):
            return True
        if all((e[23 - k] ^ ch) == cur[k] for k in range(8)):
            return True
        return bytes(e[16:24]) == bytes(mem[pa(UDA_SEG, U_DF_PASSWORD):pa(UDA_SEG, U_DF_PASSWORD) + 8])

    @staticmethod
    def _set_pw(e, new):
        e[16:24], e[13], null = encrypt(bytes(new))
        if null:
            e[12] = 0  # SET_PW: a null password zeroes the mode

    def _searched(self, mem, dx, length):
        """the BDOS's own search left the UDA's place behind"""
        mem[pa(UDA_SEG, U_SEARCHL)] = length
        w16(mem, pa(UDA_SEG, U_SEARCHA), dx)
        w16(mem, pa(UDA_SEG, U_DCNT), 0xFFFF)

    def call(self, mem, ds, es, cl, dx):
        dl = dx & 0xFF
        if cl == 11:                  # console status: typed ahead?
            self.log.append((cl,))
            return (0xFF if self.typed else 0), 0, es
        if self.typed and (cl == 1 or (cl == 6 and dl in (0xFD, 0xFF))):
            self.log.append((cl,) if cl in NO_ARG else (cl, dl))
            return self.typed.pop(0), 0, es
        if cl in (28, 31):            # no argument
            self.log.append((cl,))
            if cl == 31:
                return DPB_OFF, 0, VSEG
            if self.deny:
                return 0xFFFF, 0, es
            self.rodsk |= 1 << self.disk
            return 0, 0, es
        if cl == 101:                 # DL: the drive
            self.log.append((cl, dl))
            return self.drvlbl, 0, es
        if cl not in (30, 37, 100, 102, 103, 106):
            return super().call(mem, ds, es, cl, dx)
        self.log.append((cl, dx, ds))
        f = pa(ds, dx)
        d = self._dma(ds)
        cur, new = bytes(mem[d:d + 8]), bytes(mem[d + 8:d + 16])
        name = bytes(b & 0x7F for b in mem[f + 1:f + 12])
        if cl == 37:                  # reset drive
            if self.deny:
                return 0xFFFF, 0, es
            self.rodsk &= ~dx & 0xFFFF
            return 0, 0, es
        if cl == 106:                 # default password
            df, _, _ = encrypt(bytes(mem[f:f + 8]))
            mem[pa(UDA_SEG, U_DF_PASSWORD):pa(UDA_SEG, U_DF_PASSWORD) + 8] = df
            return 0, 0, es
        if cl == 100:                 # write label
            if self.rodsk >> self.disk & 1:
                return 0x02FF, 0, es
            mem[f] = 0x20
            n = next((n for n in range(DIR_ENTRIES) if self.dir[n][0] == 0x20), None)
            if n is None:
                n = self._free()
                if n is None:
                    return 0x00FF, 0, es
                e = bytearray(32)
                e[0], e[1:12], e[24:28] = 0x20, mem[f + 1:f + 12], TOD_BYTES
                self.dir[n] = e
            e = bytearray(self.dir[n])
            e[28:32] = mem[pa(SYSDAT, TOD):pa(SYSDAT, TOD) + 4]
            self._searched(mem, dx, 1)
            if not self._cmp_pw(mem, e, cur):
                return 0x07FF, 0, es
            e[0], e[1:12] = 0x20, mem[f + 1:f + 12]
            e[12] = self.drvlbl = mem[f + 12] | 1
            if mem[f + 12] & 1:
                self._set_pw(e, new)
            self.dir[n] = e
            return 0, 0, es
        if ord('?') in name:          # 30, 102, 103: not a wildcard
            return 0x09FF, 0, es
        if cl == 30:                  # set file indicators
            if self.drvlbl & 0x80:
                x = self._xfcb(name)
                if x is not None and self.dir[x][12] & 0xE0 and not self._cmp_pw(mem, self.dir[x], cur):
                    return 0x07FF, 0, es
            hits = [n for n in range(DIR_ENTRIES) if self.dir[n][0] == self.user and self._key(self.dir[n]) == name]
            self._searched(mem, dx, 12)
            if not hits:
                return 0x00FF, 0, es
            for n in hits:
                for i in (1, 2, 3, 4, 9, 10, 11):
                    self.dir[n][i] = (self.dir[n][i] & 0x7F) | (mem[f + i] & 0x80)
            return hits[-1] & 3, 0, es
        x = self._xfcb(name)
        self._searched(mem, dx, 12)
        if cl == 102:                 # read XFCB
            if x is None:
                return 0x00FF, 0, es
            mem[f:f + 32] = self.dir[x]
            return 0, 0, es
        # 103: write XFCB
        if x is None:
            if self.drvlbl == 0 or not any(self.dir[n][0] == self.user and self._key(self.dir[n]) == name
                                           for n in range(DIR_ENTRIES)):
                return 0x00FF, 0, es
            x = self._free()
            if x is None:
                return 0x00FF, 0, es
            e = bytearray(32)
            e[0], e[1:12] = 0x10 | self.user, mem[f + 1:f + 12]
            self.dir[x] = e
        e = bytearray(self.dir[x])
        if e[12] & 0xE0 and not self._cmp_pw(mem, e, cur):
            return 0x07FF, 0, es
        if e[12] != 0 or mem[f + 12] & 1:
            e[12] = (mem[f + 12] & 0xE0) or 0x80
            if mem[f + 12] & 1:
                self._set_pw(e, new)
        self.dir[x] = e
        return 0, 0, es


FILES = {b'A       TXT': b'a', b'B       TXT': b'b', b'C       COM': b'c', b'RO      TXT': (b'r', {'ro': True}),
         b'SYS     COM': (b's', {'sys': True}), b'OTHER   TXT': (b'o', {'user': 1})}
LABEL = (b'MYDISK     ', 0x00, b'')
LABEL_PW = (b'MYDISK     ', 0x80, b'KEY')        # passwords on, the label's own is KEY
LABEL_TS = (b'STAMPS     ', 0x70, b'')           # access, update, make XFCBs
PROTECTED = {b'A       TXT': (0x80, b'KEY')}     # A.TXT: read protected, KEY
WRITEPROT = {b'A       TXT': (0x40, b'KEY')}


class SetProgram(Program):
    """SET's code (280Fh bytes) is longer than the 4K rspref leaves
    between its CS (100h) and DS (200h), where the data would overwrite
    it: the asm's code goes at CODE_SEG instead."""
    CODE_SEG = 0x0900

    def memory(self, setup=None):
        mem = super().memory(setup)
        mem[pa(self.CODE_SEG, 0):pa(self.CODE_SEG, len(self.code))] = self.code
        w16(mem, pa(UDA_SEG, U_INITCS), self.CODE_SEG)
        return mem

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

        regs = {'cs': self.CODE_SEG, 'ds': DS, 'es': DS, 'ss': LOADER_SS, 'sp': 0x100}  # a transient
        sim = I86Sim(mem, regs, pa(self.CODE_SEG, 0), segmented=True, int_hooks={224: hook})
        sim.run(max_steps=3000000)
        assert sim.halted, f"{self.name}: the asm run didn't finish"
        return mem


def snap(mem, mpm):
    if mem is None:
        return [bytes(e).hex() for e in mpm.dir if e[0] != 0xE5], mpm.drvlbl, mpm.rodsk, mpm.disk
    entries = [bytes(e).hex() for e in mpm.dir if e[0] != 0xE5]
    df = lambda seg: bytes(mem[pa(seg, U_DF_PASSWORD):pa(seg, U_DF_PASSWORD) + 8]).hex()  # noqa: E731
    return entries, mpm.drvlbl, mpm.rodsk, mpm.disk, df(UDA_SEG), df(PARENT_UDA)


def main():
    stack_base = rsp_syms.value('SET_STACK_BASE')
    prog = SetProgram('SET', 'set_main', rsp=False, plm=True, stack=(stack_base - 1024, stack_base))
    ck = Checker()

    def tail(t, exm=0):
        t = b' ' + t if t else b''

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = b'\0' + b' ' * 11 + bytes(21)  # the CLI's fcbs
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + b' ' * 11
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
            # the DPB (exm), the parent and its UDA, the time of day
            m[pa(VSEG, DPB_OFF):pa(VSEG, DPB_OFF + 15)] = bytes([26, 0, 3, 7, exm, 242, 0, 63, 0, 0xC0, 0, 16, 0, 2, 0])
            w16(m, pa(SYSDAT, PD + P_PARENT), PARENT)
            w16(m, pa(SYSDAT, PARENT + P_UDA), PARENT_UDA)
            m[pa(SYSDAT, TOD):pa(SYSDAT, TOD + 4)] = TOD_BYTES
        return run

    L = {'label': LABEL}
    ROW = b'A:MYDISK  .       '  # showlbl's row: the label, then passwords, make, create, access, update
    for label, t, kw, expect in (
            # file attributes
            ("RO", b'A.TXT [RO]', {}, (b'\r\nA:A       .TXT  set to directory (DIR), read only (RO)\t\r\n',)),
            ("RW on a read-only file", b'RO.TXT [RW]', {}, (b'A:RO      .TXT  set to directory (DIR), read write (RW)',)),
            ("SYS", b'A.TXT [SYS]', {}, (b'A:A       .TXT  set to system (SYS), read write (RW)',)),
            ("DIR on a system file", b'SYS.COM [DIR]', {}, (b'A:SYS     .COM  set to directory (DIR)',)),
            ("two attributes", b'A.TXT [RO,SYS]', {}, (b'set to system (SYS), read only (RO)',)),
            ("archive and F1-F4 on", b'A.TXT [ARCHIVE=ON,F1=ON,F2=ON,F3=ON,F4=ON]', {}, (b'read write (RW)\tA1234',)),
            ("F1-F4 off", b'B.TXT [F1=OFF,F2=OFF,F3=ON,F4=OFF,ARCHIVE=OFF]',
             {'attrs': {b'B       TXT': (1, 2, 4, 11)}}, (b'read write (RW)\t3\r\n',)),
            ("a wildcard: every file it matches", b'*.TXT [RO]', {},
             (b'A:A       .TXT  set to', b'A:B       .TXT  set to', b'A:RO      .TXT  set to', b'!OTHER')),
            ("a wildcard, two options", b'*.COM [SYS,RO]', {},
             (b'A:C       .COM  set to system (SYS), read only (RO)', b'A:SYS     .COM  set to system (SYS), read only')),
            ("a wildcard matching nothing", b'*.XYZ [RO]', {}, (b'\r\n File not found',)),
            ("a file that isn't there", b'NONE.TXT [RO]', {}, (b' File not found',)),
            ("two files", b'A.TXT [RO] B.TXT [SYS]', {},
             (b'A:A       .TXT  set to directory (DIR), read only', b'A:B       .TXT  set to system (SYS), read write')),
            ("a drive's file", b'B:A.TXT [RO]', {}, (b'B:A       .TXT  set to',)),
            ("user 1's files", b'*.TXT [SYS]', {'user': 1}, (b'A:OTHER   .TXT  set to system', b'!A:A ')),
            ("a file of two extents", b'A.TXT [RO]', {'files': {b'A       TXT': b'x' * 0x4100}}, (b'read only (RO)',)),
            ("from drive B", b'A.TXT [RO]', {'disk': 1}, (b'B:A       .TXT  set to',)),
            # errors and bad options
            ("an attribute value that isn't ON/OFF", b'A.TXT [ARCHIVE=MAYBE]', {},
             (b'ERROR: =MAYBE] ?\r\nInvalid Value, Use ON or OFF',)),
            ("an unknown attribute", b'A.TXT [FX]', {}, (b'ERROR: [FX] ?\r\nInvalid Attribute',)),
            ("an unknown option", b'A.TXT [ZZ]', {}, (b'Invalid Parameter',)),
            ("a label option on a file", b'A.TXT [UPDATE=ON]', {}, (b'Invalid Parameter',)),
            ("an invalid file name", b'TOOLONGNAME.X [RO]', {}, (b'Invalid File',)),
            ("no options", b'A.TXT', {}, (b'SET 2.0\r\nERROR: Parameter Required, try SET [HELP]',)),
            ("no tail", b'', {}, (b'Parameter Required',)),
            ("a value missing", b'A.TXT [ARCHIVE]', {}, (b'Invalid Value',)),
            ("help", b'[HELP]', {}, (b'SET EXAMPLES', b'(Drive Status)')),
            ("version", b'[V]', {}, (b'\r\nSET 2.0',)),
            ("typed ahead: aborted", b'A.TXT [RO]', {'typed': b'x'}, (b'Aborted', b'!set to')),
            ("the wrong MP/M: CP/M 2.2", b'A.TXT [RO]', {'version': 0x0022}, (b'Requires MP/M 2.0', b'!set to')),
            ("the wrong MP/M: CP/M 3", b'A.TXT [RO]', {'version': 0x0030}, (b'Requires MP/M 2.0', b'!set to')),
            # passwords and protection
            ("a password", b'A.TXT [PASSWORD=KEY]', L, (b'A:A       .TXT  Protection = READ, Password = KEY',)),
            ("a password: no label", b'A.TXT [PASSWORD=KEY]', {}, (b'ERROR:  First SET A:[protect=on]',)),
            # [password=]: a null password (until bugs.md #132 its ] was left
            # for an empty option: "ERROR: ] ?"); clearing it takes the old one
            ("a null password: =]", b'A.TXT [PASSWORD=]', {**L, 'xfcbs': PROTECTED, 'chars': b'key\r'},
             (b'Password ? ', b'!ERROR: ] ?')),
            ("a null password: no value", b'A.TXT [PASSWORD,RO]', {**L, 'xfcbs': PROTECTED, 'chars': b'key\r'},
             (b'read only (RO)', b'Protection = READ')),
            ("a password for a file that isn't there", b'NONE.TXT [PASSWORD=KEY]', L,
             (b'A:NONE    .TXT   File not found or no directory space',)),
            # (the passwords after another option come out blank: plmc's
            # 13-byte cmd, see set.c)
            ("password and protection", b'A.TXT [PASSWORD=KEY,PROTECT=WRITE]', L, (b'Protection = WRITE',)),
            ("protect=delete", b'A.TXT [PASSWORD=KEY,PROTECT=DELETE]', L, (b'Protection = DELETE',)),
            ("protect=read: no password", b'A.TXT [PROTECT=READ]', L, (b'ERROR:  Assign a password to this file.',)),
            ("protect=none", b'A.TXT [PROTECT=NONE]', {**L, 'xfcbs': PROTECTED, 'chars': b'key\r'},
             (b'Protection = NONE',)),
            ("protect=off", b'A.TXT [PROTECT=OFF]', {**L, 'xfcbs': WRITEPROT, 'chars': b'key\r'},
             (b'Protection = NONE',)),
            ("protect=on", b'A.TXT [PASSWORD=K,PROTECT=ON]', L, (b'Protection = READ',)),
            ("a bad protection mode", b'A.TXT [PROTECT=MAYBE]', L, (b'Invalid Mode',)),
            ("protect: no label", b'A.TXT [PROTECT=READ]', {}, (b'First SET A:[protect=on]',)),
            ("a protected file: password given", b'A.TXT [RO]',
             {'label': LABEL_PW, 'xfcbs': PROTECTED, 'chars': b'key\r'},
             (b'ERROR: Wrong Password\r\n\r\nPassword ? \r\n\r\nA:A       .TXT  set to directory (DIR), read only (RO)',)),
            ("a protected file: wrong password", b'A.TXT [RO]',
             {'label': LABEL_PW, 'xfcbs': PROTECTED, 'chars': b'lock\r'},
             (b'Password ? \r\n\r\nA:A       .TXT  ERROR: Wrong Password', b'!set to')),
            ("a protected file: password edited", b'A.TXT [RO]',
             {'label': LABEL_PW, 'xfcbs': PROTECTED, 'chars': b'x\x18kx\x08ey\r'}, (b'read only (RO)',)),
            ("a protected file: ^C", b'A.TXT [RO]', {'label': LABEL_PW, 'xfcbs': PROTECTED, 'chars': b'\x03'},
             (b'Password ? ', b'!set to')),
            ("a protected file: password on the command line", b'A.TXT;KEY [RO]',
             {'label': LABEL_PW, 'xfcbs': PROTECTED, 'chars': b'key\r'}, (b'read only (RO)',)),
            ("a protected file, passwords off in the label", b'A.TXT [RO]', {'label': LABEL, 'xfcbs': PROTECTED},
             (b'read only (RO)', b'!Password ?')),
            ("a wildcard with a password", b'*.TXT [PASSWORD=KEY]', L,
             (b'A:A       .TXT  Protection = READ, Password = KEY', b'A:B       .TXT  Protection = READ',
              b'A:RO      .TXT  Protection = READ')),
            ("a protected XFCB: password given", b'A.TXT [PROTECT=WRITE]',
             {**L, 'xfcbs': PROTECTED, 'chars': b'key\r'}, (b'Protection = WRITE',)),
            ("the default password opens a protected file", b'[DEFAULT=KEY] A.TXT [RO]',
             {'label': LABEL_PW, 'xfcbs': PROTECTED}, (b'Default Password set to KEY', b'read only (RO)',
                                                       b'!Password ?')),
            # time stamps
            ("time stamps", b'A.TXT [TIME]', {'label': LABEL_TS}, (b'A:A       .TXT  Time Stamps ON',)),
            ("time stamps: [X]", b'A.TXT [X]', {'label': LABEL_TS}, (b'Time Stamps ON',)),
            ("time stamps: no stamping in the label", b'A.TXT [TIME]', L,
             (b'ERROR:  First SET A:[access=on, update=on]',)),
            ("time stamps and a password", b'A.TXT [TIME,PASSWORD=KEY]', {'label': LABEL_TS},
             (b'Protection = READ, Time Stamps ON, Password = KEY',)),
            # labels
            ("a label password: no label yet", b'[PASSWORD=XYZ]', {},
             (b'Label for drive A:', b'A:Label   .       off      off      off      off      off',
              b'Password = XYZ')),
            ("a label password", b'[PASSWORD=XYZ]', L, (ROW, b'Password = XYZ')),
            ("the label's name", b'[NAME=DISK1]', L, (b'A:DISK1   .       off',)),
            ("protect=on in the label", b'[PROTECT=ON]', L, (ROW + b'on       off      off      off      off',)),
            ("protect=off in the label", b'[PROTECT=OFF]', {'label': (b'MYDISK     ', 0x80, b'')},
             (ROW + b'off      off',)),
            ("update=on", b'[UPDATE=ON]', L, (ROW + b'off      on       on       off      on',)),
            ("update=off", b'[UPDATE=OFF]', {'label': LABEL_TS},
             (b'A:STAMPS  .       off      on       off      on       off',)),
            ("access=on", b'[ACCESS=ON]', L, (ROW + b'off      on       off      on       off',)),
            ("access=off", b'[ACCESS=OFF]', {'label': LABEL_TS},
             (b'A:STAMPS  .       off      on       on       off      on',)),
            ("create=on", b'[CREATE=ON]', L, (ROW + b'off      on       on       off      off',)),
            ("create=off", b'[CREATE=OFF]', L, (ROW + b'off      off      off      on       off',)),
            ("make=on", b'[MAKE=ON]', L, (ROW + b'off      on       on       off      off',)),
            ("make=off", b'[MAKE=OFF]', {'label': LABEL_TS},
             (b'A:STAMPS  .       off      off      off      on       on',)),
            ("several label options", b'[NAME=STAMPED,UPDATE=ON,CREATE=ON,PROTECT=ON,PASSWORD=PW]', L,
             (b'A:STAMPED .       on       on       on       off      on', b'Password = PW')),
            ("a label value that isn't ON/OFF", b'[UPDATE=MAYBE]', L, (b'Invalid Value', b'!Label for')),
            # (asked twice: the password typed for readlabel is lost to the
            # next parse -- plmc's 13-byte cmd)
            ("a protected label: password given", b'[UPDATE=ON]', {'label': LABEL_PW, 'chars': b'key\rkey\r'},
             (b'Label for drive A:', ROW + b'on       on       on       off      on')),
            ("a protected label: wrong password", b'[UPDATE=ON]', {'label': LABEL_PW, 'chars': b'no\rno\r'},
             (b'Directory Label ERROR: Wrong Password', b'!Label for')),
            ("a label on an R/O drive", b'[NAME=X]', {**L, 'rodsk': 1}, (b'ERROR: A: Drive read only',)),
            ("a label: no directory space", b'[NAME=X]', {'full': True}, (b'Unsuccessful Function',)),
            ("the default password", b'[DEFAULT=XYZ]', {}, (b'Default Password set to XYZ',)),
            # drives
            ("the current drive R/O", b'[RO]', {}, (b'Drive A: set to read only (RO)',)),
            ("the current drive R/W", b'[RW]', {'rodsk': 1}, (b'Drive A: set to read write (RW)',)),
            ("drive B R/O", b'B:[RO]', {}, (b'Drive B: set to read only (RO)',)),
            ("two drives", b'A:[RW], B:[RO]', {'rodsk': 1},
             (b'Drive A: set to read write (RW)\r\nDrive B: set to read only (RO)',)),
            ("a drive reset denied", b'[RW]', {'deny': True}, (b'Unsuccessful Function',)),
            ("write protect denied", b'[RO]', {'deny': True}, (b'Unsuccessful Function',)),
            # SET's own: match's message for the two drive values is past RW, RO
            ("a bad drive status", b'[RX]', {}, (b'\r\nInvalid Attribute\r\n',))):  # bugs.md #132
        args = {'files': FILES, **kw}
        try:
            ref, port = compare(ck, prog, label, args, setup=tail(t), snap=snap, mpm_class=SetMpm)
        except Exception as e:  # a run that didn't finish
            ck.check(f"{label}: runs", '', repr(e))
            continue
        out = bytes(ref.out)
        missing = [x for x in expect if (x[1:] in out) == (x[:1] == b'!')]
        ck.check(f"{label}: does what it should", [], missing or [])
        if missing or VERBOSE:
            print(f"    {out!r}\n    {snap(None, ref)!r}")
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
