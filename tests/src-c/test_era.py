#!/usr/bin/env python3
"""src-c/utils/era.c against src-assem/utils/ERA.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): ERA erases the files a name matches, asking
first for *.*; when that fails for some (read-only, a password) it
erases them one at a time -- saving and restoring the UDA's search
place around each delete -- naming the ones it can't and asking for
passwords. Each case is a command (the CLI's fcbs and the tail) and a
disk; the result is the files left.
"""
import sys

from rspref import Checker, DS, Program, compare, pa
import rsp_syms

FILES = {b'A       TXT': b'a', b'B       TXT': b'b', b'C       COM': b'c', b'OTHER   TXT': (b'o', {'user': 1})}
RO = {**FILES, b'RO      TXT': (b'r', {'ro': True})}
PW = {**FILES, b'PW      TXT': (b'p', {'password': b'KEY'})}


def fcb11(name):
    base, _, ext = name.partition(b'.')
    return base.ljust(8) + ext.ljust(3)


def main():
    prog = Program('ERA', 'era_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('ERA_STACK_BASE') - 1024, rsp_syms.value('ERA_STACK_BASE')))
    ck = Checker()

    def command(name, option=b''):
        """ERA name [option]: what the CLI leaves in the base page"""
        tail = b' ' + name + (b' [' + option + b']' if option else b'')
        pat = fcb11(name).replace(b'*', b'?')
        pat = pat[:8].split(b'?')[0].ljust(8, b'?') if b'?' in pat[:8] else pat[:8]
        pat += fcb11(name)[8:].split(b'*')[0].ljust(3, b'?') if b'*' in name.partition(b'.')[2] else fcb11(name)[8:]

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = b'\0' + pat + bytes(21)
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + option.ljust(11)
            m[pa(DS, 0x80)] = len(tail)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(tail) + 1)] = tail + b'\0'
        return run

    for label, cmd, files, kw in (
            ("a file", command(b'A.TXT'), FILES, {}),
            ("a pattern", command(b'*.TXT'), FILES, {}),
            ("no file", command(b'NONE.TXT'), FILES, {}),
            ("*.*: confirmed", command(b'*.*'), FILES, {'chars': b'Y'}),
            ("*.*: confirmed, lower case", command(b'*.*'), FILES, {'chars': b'y'}),
            ("*.*: not confirmed", command(b'*.*'), FILES, {'chars': b'n'}),
            ("*.* [X]: no question", command(b'*.*', b'X'), FILES, {}),
            ("an invalid parameter", command(b'A.TXT', b'Q'), FILES, {}),
            ("a read-only file among them: one at a time", command(b'*.TXT'), RO, {}),
            ("a read-only file", command(b'RO.TXT'), RO, {}),
            ("a password: given", command(b'*.TXT'), PW, {'chars': b'key\r'}),
            ("a password: wrong", command(b'*.TXT'), PW, {'chars': b'lock\r'}),
            ("a password: ^C", command(b'*.TXT'), PW, {'chars': b'\x03'}),
            ("user 1's files", command(b'*.TXT'), FILES, {'user': 1}),
            ("the wrong MP/M", command(b'A.TXT'), FILES, {'version': 0x1022})):
        compare(ck, prog, label, {'files': files, **kw}, setup=cmd, snap=lambda m, mpm: sorted(mpm.files))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
