#!/usr/bin/env python3
"""src-c/utils/ren.c against src-assem/utils/REN.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): REN new=old renames a file, or with matching ?s
every file the old name matches, one at a time -- saving and restoring
the UDA's search place around each rename -- offering to delete a new
name that exists and asking for passwords. Each case is a command tail
and a disk; the result is the files there afterwards.
"""
import sys

from rspref import Checker, DS, Program, compare, pa
import rsp_syms

FILES = {b'OLD     TXT': b'o', b'A       DOC': b'a', b'B       DOC': b'b', b'EXISTS  TXT': b'e',
         b'RO      TXT': (b'r', {'ro': True}), b'PW      TXT': (b'p', {'password': b'KEY'})}


def main():
    prog = Program('REN', 'ren_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('REN_STACK_BASE') - 1024, rsp_syms.value('REN_STACK_BASE')))
    ck = Checker()

    def tail(t):
        t = b' ' + t

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 33)] = b'\0' + b' ' * 11 + bytes(21)
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    for label, t, kw in (
            ("a file", b'NEW.TXT=OLD.TXT', {}),
            ("no such file", b'NEW.TXT=NONE.TXT', {}),
            ("the new name exists: delete it", b'EXISTS.TXT=OLD.TXT', {'chars': b'y'}),
            ("the new name exists: keep it", b'EXISTS.TXT=OLD.TXT', {'chars': b'n'}),
            ("a wildcard", b'*.TXT=*.DOC', {}),
            ("a wildcard, a new name existing", b'?.TXT=?.DOC', {'files': {**FILES, b'A       TXT': b'x'},
                                                                 'chars': b'n'}),
            ("a bad wildcard", b'A.TXT=*.DOC', {}),
            ("a read-only file", b'NEW.TXT=RO.TXT', {}),
            ("a password: asked for", b'NEW.TXT=PW.TXT', {'chars': b'key\r'}),
            ("a password: wrong", b'NEW.TXT=PW.TXT', {'chars': b'lock\r'}),
            ("a password: edited", b'NEW.TXT=PW.TXT', {'chars': b'x\x18k\x08\x08key\r'}),
            ("a password on the command line", b'NEW.TXT=PW.TXT;KEY', {}),
            ("an invalid name", b'TOOLONGNAME.X=OLD.TXT', {}),
            ("no old name", b'NEW.TXT', {}),
            ("different drives", b'B:NEW.TXT=A:OLD.TXT', {}),
            ("the new name's drive", b'A:NEW.TXT=OLD.TXT', {}),
            ("the wrong MP/M", b'NEW.TXT=OLD.TXT', {'version': 0x0022})):
        compare(ck, prog, label, {'files': FILES, **kw}, setup=tail(t), snap=lambda m, mpm: sorted(mpm.files))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
