#!/usr/bin/env python3
"""src-c/utils/type.c against src-assem/utils/TYPE.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): TYPE types a file up to its ^Z, asks for the
password of a protected one, and pauses every 24 (or nn) lines with
[P] ([Pnn]). Each case is a file name, its second fcb (the option), and
a disk; the run ends when TYPE terminates or waits for a key the script
doesn't have.
"""
import sys

from rspref import Checker, DS, Program, compare, pa
import rsp_syms

LINES = b''.join(b'line %d\r\n' % n for n in range(30)) + b'\x1a'
FILES = {b'A       TXT': b'hello, world\r\n\x1a', b'LONG    TXT': LINES,
         b'NOEOF   TXT': b'x' * 200, b'SECRET  TXT': (b'the secret\r\n\x1a', {'password': b'OPEN'})}


def main():
    prog = Program('TYPE', 'type_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('TYPE_STACK_BASE') - 1024, rsp_syms.value('TYPE_STACK_BASE')))
    ck = Checker()

    def setup(name, fcb16=b' ' * 11, tail=b'', len0=0):
        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = b'\0' + name + bytes(21)  # the CLI's fcbs
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + fcb16
            m[pa(DS, 0x53)] = len0
            m[pa(DS, 0x80)] = len(tail)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(tail) + 1)] = tail + b'\0'
        return run

    for label, s, kw in (
            ("a file", setup(b'A       TXT'), {}),
            ("several records, to the ^Z", setup(b'LONG    TXT'), {}),
            ("no ^Z: to the end of the file", setup(b'NOEOF   TXT'), {}),
            ("no file", setup(b'NOFILE  TXT'), {}),
            ("a key stops it", setup(b'LONG    TXT'), {'chars': b'x'}),
            ("[P]: waits after 24 lines", setup(b'LONG    TXT', b'P          '), {}),
            ("[PA]: 24 too", setup(b'LONG    TXT', b'PA         '), {}),
            ("[P05]: after 5", setup(b'LONG    TXT', b'P05        '), {}),
            ("a password: asked for", setup(b'SECRET  TXT'), {'chars': b'open\r'}),
            ("a password: edited (^X, backspace)", setup(b'SECRET  TXT'), {'chars': b'zz\x18o\x08\x08opex\x08n\r'}),
            ("a password: wrong", setup(b'SECRET  TXT'), {'chars': b'shut\r'}),
            ("a password: 8 characters, no return", setup(b'SECRET  TXT'), {'chars': b'abcdefgh'}),
            ("a password: ^C", setup(b'SECRET  TXT'), {'chars': b'o\x03'}),
            ("a password on the command line", setup(b'SECRET  TXT', tail=b' SECRET.TXT;OPEN', len0=4), {}),
            ("the wrong MP/M", setup(b'A       TXT'), {'version': 0x0122})):
        compare(ck, prog, label, {'files': FILES, **kw}, setup=s)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
