#!/usr/bin/env python3
"""src-c/utils/ed.c against src-assem/utils/ED.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): ED name edits a file -- commands come a line at
a time (conread), insert mode's text and the Y/N answers a character at
a time (conin) -- and E writes it back: the rest of the file to
name.$$$, the old file renamed to name.BAK, the $$$ to name. Each case
is a command (the CLI's fcbs and the tail), a disk and a script of
console input; the result is the files on the disk afterwards, names,
contents and attributes.

ED's text buffer is the memory past its data segment up to MAXB, the
data group's length LOAD leaves in the base page (DRI's ED.CMD asks for
the most there is, `gencmd ed data[... xfff]`: 0FFFh paragraphs). The
test machine is i86sim's 64K, so each case's MAXB is smaller: 1400h by
default (a 2385-byte text buffer, two-record source and destination
buffers), 2000h for one case (up to the UDA at 4000h: a 4773-byte text
buffer), and less for "NO MEMORY". ED's 14K of code doesn't fit
between rspref's CS (1000h) and DS (2000h), so here it is at 8000h.

The scripted MP/M (EdMpm) adds what ED needs to rspref's Mpm: console
input as a person types it (a key counts as pressed early -- seen by
console status, ED's break test -- only when the script says so), the
sequential read and write of a file's records through the fcb's extent
and current record, open's and make's fcb, the XFCB functions and set
attributes, and a disk that can fill up.
"""
import sys

import rspref
from rspref import (Checker, CpmFile, DS, LOADER_SS, Mpm, PD, P_NAME, P_UDA, Program, SYSDAT, SYSDAT_RLR,
                    S_MPMSEG, S_VERSION, UDA_SEG, U_INITCS, VERSION_TEXT, VSEG, DIR_ENTRIES, compare, dir_entry,
                    pa, w16)
from i86sim import I86Sim
import rsp_syms

ED_CS = 0x0800  # ED's code (0x3805 bytes), clear of its data and the UDA
CR, LF, EOF = 0x0D, 0x0A, 0x1A


def early(text):
    """keys pressed before ED looks: console status sees them"""
    return [0x100 | c for c in text]


class EdMpm(Mpm):
    """rspref's Mpm and what ED needs besides:
      chars    -- ints; c | 100h is a key already pressed (console
                  status reports it), a plain c is typed when asked for
      capacity -- the records the disk holds (write sequential then
                  returns 2, no data block)"""

    def __init__(self, capacity=None, **kw):
        kw['chars'] = list(kw.get('chars', b''))
        super().__init__(**kw)
        self.capacity = capacity
        self.modes = {}  # password modes (the XFCB's)
        for name in self.passwords:
            self.modes[name] = 0x80  # read: protected

    def _entry(self, name, extent=0):
        """the directory entry for name's extent"""
        for n in range(DIR_ENTRIES):
            e = self.dir[n]
            if e[0] == self.user and bytes(b & 0x7F for b in e[1:12]) == name and e[12] == extent:
                return n
        return None

    def _record(self, mem, ds, dx):
        """the fcb's record number: extent, current record"""
        return (mem[pa(ds, dx + 12)] & 0x1F) * 128 + mem[pa(ds, dx + 32)]

    def _next_record(self, mem, ds, dx):
        cr = mem[pa(ds, dx + 32)] + 1
        if cr == 128:  # on to the next extent
            cr = 0
            mem[pa(ds, dx + 12)] = (mem[pa(ds, dx + 12)] + 1) & 0x1F
        mem[pa(ds, dx + 32)] = cr

    def call(self, mem, ds, es, cl, dx):
        dl = dx & 0xFF
        if cl == 1:      # console input, echoed (graphic, CR, LF, tab, backspace)
            self.log.append((cl,))
            if not self.chars:
                return None
            c = self.chars.pop(0) & 0xFF
            if c >= 0x20 or c in (CR, LF, 0x09, 0x08):
                self.out.append(c)
            return c, 0, es
        if cl == 6 and dl == 0xFD:  # raw input, waiting: no echo
            self.log.append((cl, dl))
            if not self.chars:
                return None
            return self.chars.pop(0) & 0xFF, 0, es
        if cl == 11:     # console status: a key pressed early?
            self.log.append((cl,))
            return (0xFF if self.chars and self.chars[0] >= 0x100 else 0), 0, es
        if cl not in (15, 20, 21, 22, 30, 102, 103):
            return super().call(mem, ds, es, cl, dx)
        self.log.append((cl, dx, ds))
        name = self._fname(mem, ds, dx)
        bx = 0
        if cl == 15:     # open: Mpm's checks, then the extent-0 entry into fcb 1-31
            self.log.pop()
            res = super().call(mem, ds, es, cl, dx)
            n = self._entry(name)
            if res[0] == 0 and n is not None:
                mem[pa(ds, dx + 1):pa(ds, dx + 32)] = self.dir[n][1:32]
            return res
        elif cl == 20:   # read sequential: the record at ex, cr
            f = self.files.get(name)
            rec = self._record(mem, ds, dx)
            if f is None or rec not in f.recs:
                bx = 1   # reading unwritten data / end of file
            else:
                d = self._dma(ds)
                mem[d:d + 128] = f.recs[rec]
                self._next_record(mem, ds, dx)
        elif cl == 21:   # write sequential
            f = self.files.get(name)
            used = sum(len(x.recs) for x in self.files.values())
            if f is None:
                bx = 9   # invalid fcb
            elif self.capacity is not None and used >= self.capacity:
                bx = 2   # no data block
            else:
                d = self._dma(ds)
                f.recs[self._record(mem, ds, dx)] = bytearray(mem[d:d + 128])
                self._next_record(mem, ds, dx)
        elif cl == 22:   # make: a new entry; fcb s1, rc and map cleared
            free = next((n for n in range(DIR_ENTRIES) if self.dir[n][0] == 0xE5), None)
            if name in self.files:
                bx = 0x08FF  # file exists
            elif free is None:
                bx = 0xFF    # directory full
            else:
                self.files[name] = CpmFile()
                self.dir[free] = dir_entry(self.user, name, 0, mem[pa(ds, dx + 12)] & 0x1F)
                mem[pa(ds, dx + 13)] = 0
                mem[pa(ds, dx + 15):pa(ds, dx + 32)] = bytes(17)
        elif cl == 30:   # set file attributes: f1'-t3' from the fcb
            hits = [n for n in range(DIR_ENTRIES) if self.dir[n][0] == self.user
                    and bytes(b & 0x7F for b in self.dir[n][1:12]) == name]
            if not hits:
                bx = 0xFF
            for n in hits:
                for i in range(1, 12):
                    self.dir[n][i] = (self.dir[n][i] & 0x7F) | (mem[pa(ds, dx + i)] & 0x80)
        elif cl == 102:  # read XFCB: the password mode into fcb(ex)
            if name in self.passwords:
                mem[pa(ds, dx + 12)] = self.modes.get(name, 0x80)
            else:
                bx = 0xFF    # no XFCB
        elif cl == 103:  # write XFCB: the new password at DMA+8, mode = fcb(ex)
            if name in self.files:
                d = self._dma(ds)
                self.passwords[name] = bytes(mem[d + 8:d + 16])
                self.modes[name] = mem[pa(ds, dx + 12)]
            else:
                bx = 0xFF
        return bx & 0xFFFF, 0, es


class EdProgram(Program):
    """ED, its code at ED_CS, and a longer run than rspref's"""

    def memory(self, setup=None):
        mem = bytearray(rspref.MEM_SIZE)
        mem[pa(ED_CS, 0):pa(ED_CS, len(self.code))] = self.code
        mem[pa(DS, 0):pa(DS, len(self.data))] = self.data
        w16(mem, pa(SYSDAT, SYSDAT_RLR), PD)
        w16(mem, pa(SYSDAT, PD + P_UDA), UDA_SEG)
        w16(mem, pa(UDA_SEG, U_INITCS), ED_CS)
        w16(mem, pa(SYSDAT, S_MPMSEG), VSEG)
        w16(mem, pa(SYSDAT, S_VERSION), 0x10)
        mem[pa(VSEG, 0x10):pa(VSEG, 0x10) + len(VERSION_TEXT)] = VERSION_TEXT
        mem[pa(SYSDAT, PD + P_NAME):pa(SYSDAT, PD + P_NAME + 8)] = b'ED      '
        if setup:
            setup(mem)
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

        regs = {'cs': ED_CS, 'ds': DS, 'es': DS, 'ss': LOADER_SS, 'sp': 0x100}
        sim = I86Sim(mem, regs, pa(ED_CS, 0), segmented=True, int_hooks={224: hook})
        sim.run(max_steps=60000000)
        assert sim.halted, "ED: the asm run didn't finish"
        return mem


def fcb11(name):
    base, _, ext = name.partition(b'.')
    return base.ljust(8) + ext.ljust(3)


def lines(n, fmt=b'line %d alpha\r\n'):
    return b''.join(fmt % i for i in range(1, n + 1))


TEXT = lines(10)
BIG = lines(150, b'this is line %03d of the big one\r\n')  # 5100 bytes: more than a buffer
FILES = {b'A       TXT': TEXT + b'\x1a', b'BIG     TXT': BIG + b'\x1a',
         b'M       LIB': b'from the library\r\n\x1a', b'OLD     BAK': b'stale',
         b'RO      TXT': (b'read only\r\n\x1a', {'ro': True}),
         b'SYS     TXT': (b'system file\r\n\x1a', {'sys': True}),
         b'PW      TXT': (b'secret\r\n\x1a', {'password': b'KEY'})}


def text(f):
    """a file's contents up to its ^Z"""
    return f.split(b'\x1a')[0]


def command(name, dest=b'', maxb=0x1400, password=b''):
    """ED name [dest] as the CLI and LOAD leave the base page: the
    data group's length (MAXB), the fcbs, the password, the tail"""
    tail = b' ' + name + (b';' + password if password else b'') + (b' ' + dest if dest else b'')
    drive, _, fname = name.rpartition(b':')
    ddrive = dest.rstrip(b':')

    def run(m):
        m[pa(DS, 6):pa(DS, 9)] = maxb.to_bytes(3, 'little')
        w16(m, pa(DS, 9), DS)
        m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = bytes([drive[0] - 64 if drive else 0]) + fcb11(fname) + bytes(21)
        m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = bytes([ddrive[0] - 64 if ddrive else 0]) + b' ' * 11
        if password:
            m[pa(DS, 0x6C + 1):pa(DS, 0x6C + 9)] = password.ljust(8)
            w16(m, pa(DS, 0x51), 0x81 + tail.index(b';') + 1)
            m[pa(DS, 0x53)] = len(password)
        m[pa(DS, 0x80)] = len(tail)
        m[pa(DS, 0x81):pa(DS, 0x81 + len(tail) + 1)] = tail + b'\0'
    return run


def disk(m, mpm):
    """the files: name, contents (every record written), R/O and SYS,
    password"""
    out = []
    for name in sorted(mpm.files):
        f = mpm.files[name]
        e = next((e for e in mpm.dir if e[0] != 0xE5 and bytes(b & 0x7F for b in e[1:12]) == name), None)
        attrs = (bool(e[9] & 0x80), bool(e[10] & 0x80)) if e else None
        out.append((name.decode(), b''.join(bytes(f.recs[r]) for r in sorted(f.recs)), attrs,
                    mpm.passwords.get(name)))
    return out


def got(mpm, name):
    """a file's records, or None"""
    f = mpm.files.get(fcb11(name))
    return None if f is None else b''.join(bytes(f.recs[r]) for r in sorted(f.recs))


def sys_attr(mpm, name):
    return any(e[0] != 0xE5 and bytes(b & 0x7F for b in e[1:12]) == fcb11(name) and e[10] & 0x80 for e in mpm.dir)


def edited(d, name=b'A.TXT'):
    """name's text, up to its ^Z (b"" if there is no name)"""
    return text(got(d, name) or b'\x1a')


def main():
    prog = EdProgram('ED', 'ed_main', rsp=False, plm=True,
                     stack=(rsp_syms.value('ED_MAIN_BP'), rsp_syms.value('ED_STACK_BASE')))
    ck = Checker()
    kept = lambda d: edited(d) == TEXT and edited(d, b'A.BAK') == TEXT  # noqa: E731
    # each: label, command, script, and what the reference must show
    # (o: its output, d: its disk) -- so a match is real behaviour, not
    # both sides failing alike. The stack (asm) excluded from the memory
    # compare is from pl_main_bp, the PL/M's own saved frame pointer.
    cases = (
        ("a new file: insert, end", command(b'NEW.TXT'),
         {'lines': [b'i', b'e'], 'chars': b'hello\rworld\r\x1a'},
         lambda o, d: b'NEW FILE' in o and edited(d, b'NEW.TXT') == b'hello\r\nworld\r\n'
         and got(d, b'NEW.BAK') == b'' and got(d, b'NEW.$$$') is None),
        ("append all, type all, end", command(b'A.TXT'), {'lines': [b'#a', b'b#t', b'e']},
         lambda o, d: b'    5:  line 5 alpha\r\n' in o and kept(d) and got(d, b'OLD.BAK') == b'stale'.ljust(128, b'\x1a')
         and got(d, b'A.$$$') is None),
        ("append some, lines, type, page, quit", command(b'A.TXT'),
         {'lines': [b'4a', b'2l', b't', b'-t', b'0t', b'b', b'3:t', b':5t', b'', b'-', b'0p', b'-b', b'q'],
          'chars': b'y'},
         lambda o, d: b'    3:  line 3 alpha\r\n    3: *-t\r\n    2:  line 2 alpha' in o and edited(d) == TEXT
         and got(d, b'A.$$$') is None and got(d, b'A.BAK') is None),
        ("insert a line, from the command line", command(b'A.TXT'),
         {'lines': [b'#a', b'3l', b'iinserted', b'iTWO\x1a0lt', b'b#t', b'e']},
         lambda o, d: edited(d).split(b'\r\n')[3:5] == [b'inserted', b'TWOline 4 alpha']),
        ("insert mode: lower and upper case, ^L, tab", command(b'A.TXT'),
         {'lines': [b'#a', b'I', b'i', b'#t', b'e'], 'chars': b'up\tper\x0cnext\r\x1alower\r\x1a'},
         lambda o, d: edited(d).startswith(b'UP\tPER\r\nNEXT\r\nlower\r\nline 1')),
        ("insert mode: backspace, rubout, ^U, ^X, ^R", command(b'A.TXT'),
         {'lines': [b'#a', b'i', b'#t', b'e'],
          'chars': b'abx\x08c\rdeq\x7ff\rgone\x15kept\rzap\x18x\x12\r\x1a'},
         lambda o, d: edited(d).startswith(b'abc\r\ndef\r\nkept\r\nx\r\nline 1')),
        ("substitute", command(b'A.TXT'), {'lines': [b'#a', b'#salpha\x1abeta', b'b#t', b'e']},
         lambda o, d: edited(d) == TEXT.replace(b'alpha', b'beta') and b'BREAK "#" AT a' in o),
        ("substitute, ^L: two lines", command(b'A.TXT'), {'lines': [b'#a', b'2s alpha\x1a\x0c', b'e']},
         lambda o, d: edited(d).startswith(b'line 1\r\n\r\nline 2\r\n\r\nline 3 alpha')),
        ("find, delete characters, kill lines", command(b'A.TXT'),
         {'lines': [b'#a', b'fline 5', b'-6d', b'0lt', b'2k', b'-k', b'5c', b'2d', b'b#t', b'e']},
         lambda o, d: edited(d) == lines(3) + b'line alpha\r\n' + lines(10)[len(lines(7)):]),
        ("find: not there", command(b'A.TXT'), {'lines': [b'#a', b'fnothing', b'e']},
         lambda o, d: b'BREAK "#" AT ' in o and kept(d)),
        ("a macro", command(b'A.TXT'), {'lines': [b'#a', b'mfalpha\x1a-5diBETA\x1a0lt', b'b#t', b'e']},
         lambda o, d: edited(d) == TEXT.replace(b'alpha', b'BETA') and b'BREAK "#" AT ^Z' in o),
        ("a macro, three times", command(b'A.TXT'), {'lines': [b'#a', b'3mfalpha\x1a-5diX\x1a', b'b#t', b'e']},
         lambda o, d: edited(d) == lines(3, b'line %d X\r\n') + lines(10)[len(lines(3)):]),
        ("a macro, a key stops it", command(b'A.TXT'),
         {'lines': [b'#a', b'mfalpha\x1a', b'e'], 'chars': early(b' ')},
         lambda o, d: b'BREAK "#"' in o and kept(d)),
        ("juxtapose, twice (the fixed nJ)", command(b'A.TXT'),
         {'lines': [b'#a', b'2jline\x1a #\x1aalpha', b'b#t', b'e']},
         lambda o, d: edited(d) == b'line #alpha\r\n' * 2 + lines(10)[len(lines(2)):]),
        ("read a library", command(b'A.TXT'), {'lines': [b'#a', b'2l', b'rm', b'b#t', b'e']},
         lambda o, d: edited(d).split(b'\r\n')[2] == b'from the library'),
        ("read: no library", command(b'A.TXT'), {'lines': [b'#a', b'rnone', b'e']},
         lambda o, d: b'BREAK "O"' in o and kept(d)),
        ("read: a name too long", command(b'A.TXT'), {'lines': [b'#a', b'rninechars', b'e']},
         lambda o, d: b'BREAK "#"' in o and kept(d)),
        ("transfer lines, read them back, clear", command(b'A.TXT'),
         {'lines': [b'#a', b'b', b'3x', b'#l', b'r', b'0x', b'r', b'e']},
         lambda o, d: edited(d) == TEXT + lines(3) and b'BREAK "O"' in o
         and got(d, b'X$$$$$$$.LIB') is None),
        ("transfer more than a record", command(b'A.TXT'), {'lines': [b'#a', b'b', b'#x', b'#l', b'r', b'e']},
         lambda o, d: edited(d) == TEXT + TEXT),
        ("transfer, quit: the xfer file is deleted", command(b'A.TXT'), {'lines': [b'#a', b'5x', b'q'], 'chars': b'y'},
         lambda o, d: got(d, b'X$$$$$$$.LIB') is None and edited(d) == TEXT and got(d, b'A.BAK') is None),
        ("next: through a big file", command(b'BIG.TXT'), {'lines': [b'n140 of', b'0lt', b'e']},
         lambda o, d: b'  140:  this is line 140 of' in o and edited(d, b'BIG.TXT') == BIG),
        ("next: not there", command(b'BIG.TXT'), {'lines': [b'nnowhere', b'e']},
         lambda o, d: b'BREAK "#"' in o and edited(d, b'BIG.TXT') == BIG),
        ("a big file: half a buffer, write, append", command(b'BIG.TXT'),
         {'lines': [b'a', b'0a', b'0v', b'30w', b'0a', b'0v', b'#a', b'b10t', b'e']},
         lambda o, d: o.count(b' 1195/ 2417') == 2 and b'BREAK ">"' in o and edited(d, b'BIG.TXT') == BIG),
        ("a big file, all of memory", command(b'BIG.TXT', maxb=0x2000),
         {'lines': [b'#a', b'0v', b'b', b'k', b'e']},
         lambda o, d: edited(d, b'BIG.TXT') == BIG[BIG.index(b'\n') + 1:]),
        ("overflow: no room", command(b'BIG.TXT', maxb=0x0F00), {'lines': [b'#a', b'#a', b'e']},
         lambda o, d: b'BREAK ">"' in o and edited(d, b'BIG.TXT') == BIG),
        ("head: write and start again", command(b'A.TXT'),
         {'lines': [b'#a', b'itop', b'h', b'#a', b'iagain', b'e']},
         lambda o, d: edited(d) == b'again\r\ntop\r\n' + TEXT and edited(d, b'A.BAK') == b'top\r\n' + TEXT),
        ("old file again: yes", command(b'A.TXT'), {'lines': [b'#a', b'#k', b'o', b'#a', b'e'], 'chars': b'y'},
         lambda o, d: kept(d) and b'O-(Y/N)?y' in o),
        ("old file again: no", command(b'A.TXT'), {'lines': [b'#a', b'#k', b'o', b'e'], 'chars': b'n'},
         lambda o, d: edited(d) == b'' and edited(d, b'A.BAK') == TEXT),
        ("quit: no", command(b'A.TXT'), {'lines': [b'#a', b'#k', b'q', b'e'], 'chars': b'n'},
         lambda o, d: edited(d) == b''),
        ("upper case, verify", command(b'A.TXT'),
         {'lines': [b'u', b'#a', b'-u', b'-v', b'0v', b'v', b'b2t', b'e']},
         lambda o, d: edited(d) == TEXT.upper() and b'*0v\r\n 2275/ 2417\r\n' in o),
        ("bad commands", command(b'A.TXT'), {'lines': [b'#a', b'y', b'-a', b'5q', b'-#f', b'e']},
         lambda o, d: o.count(b'BREAK "?"') == 4 and kept(d)),
        ("a key stops typing", command(b'A.TXT'), {'lines': [b'#a', b'b#t', b'e'], 'chars': early(b'x')},
         lambda o, d: b'BREAK "#" AT T' in o and b'line 2' not in o),
        ("sleep: a key", command(b'A.TXT'), {'lines': [b'0z', b'2z', b'e'], 'chars': [ord('k')] + early(b'x')},
         lambda o, d: b'BREAK' in o),
        ("page down", command(b'BIG.TXT'), {'lines': [b'#a', b'b', b'p', b'e']},
         lambda o, d: b'this is line 024' in o and b'this is line 046' in o and b'this is line 047' not in o),
        ("the destination on another disk: the file is there", command(b'A.TXT', b'B:'), {'lines': [b'e']},
         lambda o, d: b'FILE EXISTS, ERASE IT' in o and edited(d) == TEXT and got(d, b'A.BAK') is None),
        ("a read-only file", command(b'RO.TXT'), {'lines': [b'e']},
         lambda o, d: b'** FILE IS READ/ONLY **' in o and got(d, b'RO.$$$') is None),
        ("a system file stays one", command(b'SYS.TXT'), {'lines': [b'#a', b'e']},
         lambda o, d: edited(d, b'SYS.TXT') == b'system file\r\n' and sys_attr(d, b'SYS.TXT')),
        # PASSWORD is plmc's 1 byte (utils_syms.h): the password typed (or
        # given) overwrites DFCB, so the edit goes to a Y.$$$ -- as the
        # compiled ED.PLM does it
        ("a password: asked for", command(b'PW.TXT'), {'lines': [b'#a', b'#t', b'e'], 'chars': b'key\r'},
         lambda o, d: b'Password ?' in o and b'secret' in o and edited(d, b'PW.TXT') == b'secret\r\n'),
        ("a password on the command line", command(b'PW.TXT', password=b'KEY'),
         {'lines': [b'#a', b'e'], 'chars': b'key\r'},
         lambda o, d: b'Password ?' not in o and edited(d, b'PW.TXT') == b'secret\r\n'),
        # the edited file keeps its password: ED gives the new one (made
        # as the $$$) the old one's XFCB
        ("a protected file keeps its password", command(b'PW.TXT'),
         {'lines': [b'#a', b'b', b'iedited\x1a', b'e'], 'chars': b'key\r'},
         lambda o, d: edited(d, b'PW.TXT') == b'editedsecret\r\n' and d.passwords.get(b'PW      TXT', b'')[:3] == b'KEY'),
        ("the disk fills up", command(b'BIG.TXT'), {'lines': [b'#a', b'e'], 'capacity': 70},
         lambda o, d: b'DISK OR DIRECTORY FULL' in o and edited(d, b'BIG.TXT') == BIG),
        ("no file name", command(b''), {'lines': [b'e']},
         lambda o, d: b'Filename Required' in o),
        ("no memory", command(b'A.TXT', maxb=0x0D00), {'lines': [b'e']},
         lambda o, d: o == b'\r\nNO MEMORY'),
        ("the wrong MP/M", command(b'A.TXT'), {'lines': [b'e'], 'version': 0x0022},
         lambda o, d: o == b'\r\nRequires MP/M 2.0'),
    )
    for label, cmd, script, real in cases:
        ref, _ = compare(ck, prog, label, {'files': FILES, **script}, setup=cmd, snap=disk, mpm_class=EdMpm)
        ok = bool(real(bytes(ref.out), ref))
        ck.check(f"{label}: the reference does it", True, ok)
        if not ok:
            print(f"    the reference's output: {bytes(ref.out)!r}\n    its disk: {disk(None, ref)!r}")
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
