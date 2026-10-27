#!/usr/bin/env python3
"""src-c/utils/dskrst.c against src-assem/utils/DSKRST.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): DSKRST (DSKRESET.CMD on the disk) resets the
drives its command tail names -- every letter A to P in it -- or all
sixteen with no tail, through function 37 (reset drive). Mpm answers 37
with 0, and DSKRST doesn't look; the result is the drive vector it
passes, which each case also checks against what it should be.
"""
import sys

from rspref import Checker, DS, Program, compare, pa
import rsp_syms


def main():
    prog = Program('DSKRST', 'dskrst_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('DSKRST_STACK_BASE') - 1024, rsp_syms.value('DSKRST_STACK_BASE')))
    ck = Checker()

    def tail(t):
        """the command tail the CLI leaves at tbuff: its length, then it"""
        def run(m):
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    def vector(mpm):
        return [hex(e[1]) for e in mpm.log if e[0] == 37]

    def bits(letters):
        return hex(sum(1 << (ord(c) - ord('A')) for c in set(letters)))

    for label, t, kw, want in (
            ("no tail: every drive", b'', {}, ['0xffff']),
            ("A:", b' A:', {}, [bits('A')]),
            ("B:", b' B:', {}, [bits('B')]),
            ("P:, the last", b' P:', {}, [bits('P')]),
            ("A: B: P:", b' A: B: P:', {}, [bits('ABP')]),
            ("a list without colons", b' C,D,E', {}, [bits('CDE')]),
            ("a letter twice", b' A: A:', {}, [bits('A')]),
            ("Q: beyond P: nothing", b' Q:', {}, ['0x0']),
            ("lower case: nothing", b' a:', {}, ['0x0']),
            ("@ and [ around A..P", b' @[A', {}, [bits('A')]),
            ("A to P", b' ABCDEFGHIJKLMNOP', {}, ['0xffff']),
            ("a long tail", b' ' + b'H' * 125, {}, [bits('H')]),
            ("only spaces: a tail, so nothing", b'   ', {}, ['0x0']),
            ("the wrong product", b' A:', {'version': 0x1030}, []),
            ("the wrong version", b' A:', {'version': 0x1120}, [])):
        ref, port = compare(ck, prog, label, kw, setup=tail(t), snap=lambda m, mpm: vector(mpm))
        out = b'\r\nRequires MP/M-86 2.0' if 'version' in kw else b''
        ck.check(f"{label}: expected", (want, out), (vector(port), bytes(port.out)))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
