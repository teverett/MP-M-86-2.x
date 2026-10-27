#!/usr/bin/env python3
"""src-c/rsp/clock.c against src-assem/rsp/CLOCK.A86, as whole programs (see
tests/src-c/rspref.py): CLOCK waits on the seconds flag and adds a second
to SYSDAT's time of day each time, BCD, setting the minute flag as each
minute ends. Each case starts from a time of day and lets some seconds
go by; the run ends at the next flag wait. The times are valid BCD, as
SYSDAT's always is: tools/i86sim.py doesn't track AF, so its DAA is
exact only for BCD (a non-BCD 4Fh would differ from the real 8086, and
from clock.c, which follows the 8086).
"""
import struct
import sys

from rspref import Checker, Program, SYSDAT, TOD, compare, pa


def main():
    prog = Program('CLOCK', 'clock_main', rsp=True, stack=(0x1C0, 0x1DE + 6))
    ck = Checker()

    def tod(day, hour, minute, second):
        return lambda m: m.__setitem__(slice(pa(SYSDAT, TOD), pa(SYSDAT, TOD) + 5),
                                       struct.pack('<HBBB', day, hour, minute, second))

    def snap(m, mpm):
        return bytes(m[pa(SYSDAT, TOD):pa(SYSDAT, TOD) + 5]).hex(' ')

    for label, start, seconds in (
            ("plain second tick", (1, 0x10, 0x30, 0x45), 1),
            ("second rolls over to minute", (1, 0x10, 0x30, 0x59), 1),
            ("minute rolls over to hour", (1, 0x10, 0x59, 0x59), 1),
            ("hour rolls over to day", (1, 0x23, 0x59, 0x59), 1),
            ("day from a large value", (0x0099, 0x23, 0x59, 0x59), 1),
            ("two and a half minutes", (5, 0x09, 0x58, 0x31), 150)):
        compare(ck, prog, label, {'flagwaits': seconds}, setup=tod(*start), snap=snap)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
