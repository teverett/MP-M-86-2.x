#!/usr/bin/env python3
"""src-c/utils/cons.c against src-assem/utils/CONS.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): CONS (CONSOLE.CMD on the disk) prints its
console's number, right-aligned in three places by p$decimal's leading
zero suppression. Each case is a console number (function 153's answer)
or an MP/M version; each also checks the text it should print.
"""
import sys

from rspref import Checker, Program, compare
import rsp_syms


def main():
    prog = Program('CONS', 'cons_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('CONS_STACK_BASE') - 1024, rsp_syms.value('CONS_STACK_BASE')))
    ck = Checker()
    HEAD = b'\r\nConsole = '
    WRONG = b'\r\nRequires MP/M-86 2.0'

    for label, kw, out in (
            ("console 0: zeros suppressed but the last", {'console': 0}, HEAD + b'  0'),
            ("console 1", {'console': 1}, HEAD + b'  1'),
            ("console 9", {'console': 9}, HEAD + b'  9'),
            ("console 10: an inner zero", {'console': 10}, HEAD + b' 10'),
            ("console 99", {'console': 99}, HEAD + b' 99'),
            ("console 100: zeros after a digit", {'console': 100}, HEAD + b'100'),
            ("console 207", {'console': 207}, HEAD + b'207'),
            ("console 255", {'console': 255}, HEAD + b'255'),
            ("the wrong product (CP/M-86 2.0's 0022h)", {'version': 0x0022}, WRONG),
            ("the wrong version (MP/M-86 2.1's 1131h)", {'version': 0x1131}, WRONG),
            ("the MP/M product bits in the low byte", {'version': 0x3011}, WRONG)):
        ref, port = compare(ck, prog, label, kw)
        ck.check(f"{label}: expected", out, bytes(port.out))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
