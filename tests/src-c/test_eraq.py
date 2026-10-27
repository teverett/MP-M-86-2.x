#!/usr/bin/env python3
"""src-c/utils/eraq.c against src-assem/utils/ERAQ.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): ERAQ collects the current user's directory
entries a name matches (128 at most), then names each one -- "A: NAME.TYP ?"
-- and erases it on a Y, naming the ones it can't (R/O, a password) and
asking for the password. Each case is a command (the CLI's fcbs and the
tail), a disk and the answers; the result is the files left, which each
case also checks against what ERAQ should have done, so the two sides
can't agree on doing nothing.
"""
import sys

import rspref
from rspref import Checker, DS, Program, compare, pa
import rsp_syms

FILES = {b'A       TXT': b'a', b'B       TXT': b'b', b'C       COM': b'c', b'OTHER   TXT': (b'o', {'user': 1})}
RO = {**FILES, b'RO      TXT': (b'r', {'ro': True})}
PW = {**FILES, b'PW      TXT': (b'p', {'password': b'KEY'})}
MANY = {f'F{n:03d}    TXT'.encode(): b'x' for n in range(129)}


def fcb11(name):
    base, _, ext = name.partition(b'.')
    return base.ljust(8) + ext.ljust(3)


def pattern(name):
    """the CLI's fcb name for name: a * fills its field with ?s"""
    base, ext = fcb11(name)[:8], fcb11(name)[8:]
    if b'*' in base:
        base = base.split(b'*')[0].ljust(8, b'?')
    if b'*' in ext:
        ext = ext.split(b'*')[0].ljust(3, b'?')
    return base + ext


def main():
    prog = Program('ERAQ', 'eraq_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('ERAQ_STACK_BASE') - 1024, rsp_syms.value('ERAQ_STACK_BASE')))
    ck = Checker()

    def command(name, option=b'', drive=0, password=b''):
        """ERAQ [d:]name[;password] [option]: what the CLI leaves in the
        base page (len0 is the password's length)"""
        tail = b' ' + (b'%c:' % (64 + drive) if drive else b'') + name + (b';' + password if password else b'') + \
            (b' [' + option + b']' if option else b'')

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = bytes([drive]) + pattern(name) + bytes(21)
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + option.ljust(11)
            m[pa(DS, 0x53)] = len(password)
            m[pa(DS, 0x80)] = len(tail)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(tail) + 1)] = tail + b'\0'
        return run

    def gone(files, *names):
        return sorted(set(files) - set(names))

    for label, cmd, files, kw, left in (
            ("a file: yes", command(b'A.TXT'), FILES, {'chars': b'y'}, gone(FILES, b'A       TXT')),
            ("a file: no", command(b'A.TXT'), FILES, {'chars': b'n'}, gone(FILES)),
            ("a pattern: Y, n, then input runs out", command(b'*.TXT'), FILES, {'chars': b'Yn'},
             gone(FILES, b'A       TXT')),
            ("*.*: every answer", command(b'*.*'), FILES, {'chars': b'yYx'},
             gone(FILES, b'A       TXT', b'B       TXT')),
            ("no file", command(b'NONE.TXT'), FILES, {}, gone(FILES)),
            ("another user's files", command(b'*.TXT'), FILES, {'user': 1, 'chars': b'y'},
             gone(FILES, b'OTHER   TXT')),
            ("a drive: B:", command(b'A.TXT', drive=2), FILES, {'chars': b'y'}, gone(FILES, b'A       TXT')),
            ("the default drive: C:", command(b'A.TXT'), FILES, {'disk': 2, 'chars': b'y'},
             gone(FILES, b'A       TXT')),
            ("[X]: the XFCBs only", command(b'A.TXT', b'X'), FILES, {'chars': b'y'}, gone(FILES)),
            ("an invalid parameter", command(b'A.TXT', b'Q'), FILES, {'chars': b'y'}, gone(FILES)),
            ("a read-only file: not erased", command(b'*.TXT'), RO, {'chars': b'yyy'},
             gone(RO, b'A       TXT', b'B       TXT')),
            ("a password: given", command(b'PW.TXT'), PW, {'chars': b'ykey\r'}, gone(PW, b'PW      TXT')),
            ("a password: 8 characters, no return", command(b'PW.TXT'), PW, {'chars': b'yKEYxxxxx'}, gone(PW)),
            ("a password: edited (^X, backspace)", command(b'PW.TXT'), PW, {'chars': b'yzz\x18q\x08\x08kex\x08y\r'},
             gone(PW, b'PW      TXT')),
            ("a password: wrong", command(b'*.TXT'), PW, {'chars': b'nyylock\r'}, gone(PW, b'B       TXT')),
            ("a password: ^C", command(b'*.TXT'), PW, {'chars': b'nnyk\x03'}, gone(PW)),
            ("a password on the command line", command(b'PW.TXT', password=b'KEY'), PW, {'chars': b'y'},
             gone(PW, b'PW      TXT')),
            ("128 entries: the table full", command(b'*.TXT'), dict(list(MANY.items())[:128]), {'chars': b'n' * 128},
             None),
            ("129 entries: too many", command(b'*.TXT'), MANY, {'chars': b'y'}, sorted(MANY)),
            ("the wrong MP/M", command(b'A.TXT'), FILES, {'version': 0x1022, 'chars': b'y'}, gone(FILES)),
            ("MP/M-80 2.0 (CP/M 3.0, MP/M): passes", command(b'A.TXT'), FILES, {'version': 0x0130, 'chars': b'y'},
             gone(FILES, b'A       TXT'))):
        if len(files) > rspref.DIR_ENTRIES:  # a directory big enough (Mpm sizes its own by this)
            rspref.DIR_ENTRIES = 256
        ref, _ = compare(ck, prog, label, {'files': files, **kw}, setup=cmd, snap=lambda m, mpm: sorted(mpm.files))
        rspref.DIR_ENTRIES = 64
        if left is not None:
            ck.check(f"{label}: what ERAQ should leave", left, sorted(ref.files))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
