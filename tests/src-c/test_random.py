#!/usr/bin/env python3
"""src-c/rsp/random.c against src/rsp/RANDOM.A86 (DRI's sample
random-access program), as whole programs (see tests/src-c/rspref.py):
RANDOM opens (or makes) the file in its FCB, then takes commands -- nW
and nZ write a typed line to record n, nR prints record n, Q closes the
file and quits. Each case types commands and data; the run ends at Q
or when the input runs out.
"""
import sys

from rspref import Checker, DS, Program, compare, pa


def main():
    prog = Program('RANDOM', 'random_main', rsp=False, stack=(0x181, 0x1A1))
    ck = Checker()

    def fcb(name=b'R       DAT'):
        return lambda m: m.__setitem__(slice(pa(DS, 0x5C), pa(DS, 0x5C + 12)), b'\0' + name)

    def records(mpm):
        f = mpm.files.get(b'R       DAT')
        return None if f is None else {k: bytes(v).rstrip(b'\0') for k, v in sorted(f.recs.items())}

    old = {b'R       DAT': b'record zero'.ljust(128, b'\0') + b'record one'.ljust(128, b'\0')}
    for label, lines, chars, files in (
            ("new file: write, read back, quit", [b'3W', b'3R', b'Q'], b'hello there\r', None),
            ("existing file: read records 0 and 1", [b'0R', b'1r', b'Q'], b'', old),
            ("write zero fill, then read", [b'5Z', b'5R', b'Q'], b'five\r', None),
            ("unwritten record -> error", [b'9R', b'Q'], b'', None),
            ("bad commands -> error", [b'X', b'', b'12', b'Q'], b'', None),
            ("number too large -> error", [b'99999999W', b'Q'], b'', None),
            ("127 characters fill the record", [b'1W', b'1R', b'Q'], b'y' * 130 + b'\r', None),
            ("lower-case commands", [b'2w', b'2r', b'q'], b'abc\r', None),
            ("control characters aren't typed back", [b'6W', b'6R', b'Q'], b'a\tb\x07c\r', None),
            ("input runs out", [b'4W'], b'part', None)):
        compare(ck, prog, label, {'lines': lines, 'chars': chars, 'files': files}, setup=fcb(),
                snap=lambda m, mpm: records(mpm))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
