#!/usr/bin/env python3
"""src-c/utils/dir.c against src-assem/utils/DIR.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): DIR lists the directory, four names a line, for
the current user or a [G n] one, system files only with [S]. Each case
is a command tail and a directory; the run ends when DIR terminates.
"""
import sys

from rspref import Checker, DS, Program, compare, pa, DIR_ENTRIES  # noqa: F401
import rsp_syms

FILES = {b'A       TXT': b'a', b'B       COM': b'b', b'HIDDEN  SYS': (b'h', {'sys': True}),
         b'OTHER   DAT': (b'o', {'user': 1})}
MANY = {f'F{n:02d}     TXT'.encode(): b'x' for n in range(10)}


def main():
    prog = Program('DIR', 'dir_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('DIR_STACK_BASE') - 1024, rsp_syms.value('DIR_STACK_BASE')))
    ck = Checker()

    def tail(t, fcb16=b' ' * 11):
        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 12)] = b'\0' + b' ' * 11  # the CLI's fcbs
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + fcb16
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    for label, t, files, kw in (
            ("no tail: every file of user 0", b'', FILES, {}),
            ("a pattern", b' *.COM', FILES, {}),
            ("two names", b' A.TXT B.COM', FILES, {}),
            ("[S]: system files too", b' [S]', FILES, {}),
            ("[G1]: user 1", b' [G1]', FILES, {}),
            ("[G12]: a two-digit user", b' [G12]', {b'TWELVE  TXT': (b't', {'user': 12})}, {}),
            ("not found", b' NOFILE.XYZ', FILES, {}),
            ("a bad entry", b' TOOLONGNAME.BAR', FILES, {}),
            ("only system files: they exist", b'', {b'HIDDEN  SYS': (b'h', {'sys': True})}, {}),
            ("ten files: two records, four a line", b'', MANY, {}),
            ("a key stops the listing", b'', MANY, {'chars': b'x'}),
            ("the heading for user 12", b'', {b'TWELVE  TXT': (b't', {'user': 12})}, {'user': 12})):
        compare(ck, prog, label, {'files': files, **kw}, setup=tail(t), snap=lambda m, mpm: mpm.user)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
