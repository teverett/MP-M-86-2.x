#!/usr/bin/env python3
"""src-c/rsp/echo.c against src-assem/rsp/ECHO.A86, as whole programs (see
tests/src-c/rspref.py): ECHO makes and opens its queue, then prints each
message the CLI writes to it -- the sender's PD, then the command tail
-- on the sender's console. Each case sends messages, and the run ends
when ECHO reads the queue once more.
"""
import struct
import sys

from rspref import Checker, Program, SENDER, SYSDAT, P_CNS, compare, pa


def msg(tail):
    return struct.pack('<H', SENDER) + tail


def main():
    prog = Program('ECHO', 'echo_main', rsp=True, stack=(0x25F, 0x27D + 6))
    ck = Checker()

    def cns(n):
        return lambda m: m.__setitem__(pa(SYSDAT, SENDER + P_CNS), n)

    for label, console, tails in (
            ("$ terminator mid-buffer", 7, [b"HELLO$WORLD" + b" " * 120]),
            ("NUL terminator mid-buffer", 3, [b"DIR A:\x00" + b" " * 124]),
            ("no terminator, runs off the end", 0, [b"X" * 129]),
            ("empty command tail", 9, [b"\x00"]),
            ("two messages", 2, [b"FIRST\x00", b"SECOND ONE\x00"])):
        compare(ck, prog, label, {'queue': [msg(t) for t in tails]}, setup=cns(console),
                snap=lambda m, mpm: mpm.console)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
