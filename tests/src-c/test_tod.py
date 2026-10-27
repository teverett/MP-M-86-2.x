#!/usr/bin/env python3
"""src-c/utils/tod.c against src-assem/utils/TOD.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): TOD shows SYSDAT's clock (its tod at 7Eh: the
day number from 1/1/78 = 1, then BCD hours, minutes, seconds) as "Day
mm/dd/yy hh:mm:ss"; with a time and date in the tail it parses them,
waits for a key and sets the clock; with P it shows the clock until a
key. Each case is a tail and a clock; the result is the clock after.

The scripted MP/M gets a model of what TOD needs that rspref's lacks:
function 154 (SYSDAT's address), and a clock that runs: console status
(11) finds no key for the first `polls` calls, the clock ticking a
second each time, then reports the script's keys as rspref's does.
"""
import datetime
import sys

from rspref import Checker, DS, Mpm, Program, SYSDAT, TOD, compare, pa
import rsp_syms

BASE = datetime.date(1977, 12, 31)  # day 1 is 1/1/78, a Sunday


def bcd(n):
    return (n // 10) << 4 | n % 10


def unbcd(b):
    return (b >> 4) * 10 + (b & 0xF)


class TodMpm(Mpm):
    """rspref's Mpm plus:
      154 get SYSDAT -- no argument; ES:BX = SYSDAT:0 (logged as no-argument)
      11 console status -- 0 for the first `polls` calls, each a second
         of the SYSDAT clock going by; then 0FFh if a key is waiting --
         and if none is, the run stops (TOD would poll forever)"""

    def __init__(self, polls=0, **kw):
        super().__init__(**kw)
        self.polls = polls

    def tick(self, mem):
        t = pa(SYSDAT, TOD)
        day = mem[t] | mem[t + 1] << 8
        h, m, s = unbcd(mem[t + 2]), unbcd(mem[t + 3]), unbcd(mem[t + 4])
        s += 1
        if s == 60:
            s, m = 0, m + 1
        if m == 60:
            m, h = 0, h + 1
        if h == 24:
            h, day = 0, day + 1
        mem[t:t + 5] = bytes([day & 0xFF, day >> 8, bcd(h), bcd(m), bcd(s)])

    def call(self, mem, ds, es, cl, dx):
        if cl == 154:
            self.log.append((cl,))
            return 0, 0, SYSDAT
        if cl == 11:
            if self.polls:
                self.log.append((cl,))
                self.polls -= 1
                self.tick(mem)
                return 0, 0, es
            if not self.chars:
                self.log.append((cl,))
                return None
        return super().call(mem, ds, es, cl, dx)


def clock(date=None, h=0, m=0, s=0, day=None):
    day = (date - BASE).days if day is None else day
    return bytes([day & 0xFF, day >> 8, bcd(h), bcd(m), bcd(s)])


def shown(date, h, m, s):
    """what TOD should show for a date and time"""
    wd = 'Sun Mon Tue Wed Thu Fri Sat'.split()[(date.weekday() + 1) % 7]
    return f"{wd} {date.month:02d}/{date.day:02d}/{date.year % 100:02d} {h:02d}:{m:02d}:{s:02d}".encode()


def main():
    prog = Program('TOD', 'tod_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('TOD_STACK_BASE') - 1024, rsp_syms.value('TOD_STACK_BASE')))
    ck = Checker()

    def setup(t, now):
        def run(m):
            m[pa(SYSDAT, TOD):pa(SYSDAT, TOD + 5)] = now
            word = t.split()[0][:1].upper() if t.split() else b' '
            m[pa(DS, 0x5C):pa(DS, 0x5C + 12)] = b'\0' + word.ljust(11)  # the CLI's fcb
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    def snap(m, mpm):
        return bytes(m[pa(SYSDAT, TOD):pa(SYSDAT, TOD + 5)]).hex()

    def run(label, t, now, expect=None, **kw):
        ref, _ = compare(ck, prog, label, kw, setup=setup(t, now), snap=snap, mpm_class=TodMpm)
        out = bytes(ref.out)
        # the reference's run is TOD's real behaviour, not a failure both share
        if expect is not None:
            ck.check(f"{label}: shows what it should", True, expect in out)
        return out

    # displays: dates across the leap-year rules and the century
    for date, (h, m, s) in ((datetime.date(1978, 1, 1), (0, 0, 0)), (datetime.date(1978, 12, 31), (23, 59, 59)),
                            (datetime.date(1980, 2, 28), (12, 0, 0)), (datetime.date(1980, 2, 29), (12, 0, 1)),
                            (datetime.date(1980, 3, 1), (1, 2, 3)), (datetime.date(1984, 1, 31), (9, 30, 0)),
                            (datetime.date(1984, 12, 31), (10, 11, 12)), (datetime.date(1985, 3, 1), (7, 7, 7)),
                            (datetime.date(1999, 12, 31), (23, 59, 59)), (datetime.date(1996, 2, 29), (0, 0, 59))):
        run(f"shows {date} {h:02d}:{m:02d}:{s:02d}", b'', clock(date, h, m, s), expect=shown(date, h, m, s))
    # out of range: day 0 is "1/0/78", a Monday ((0 - 1) mod 7 in 16 bits);
    # from 2000 the year's tens digit is past '9'; and the year byte wraps
    run("day 0", b'', clock(day=0), expect=b'Mon 01/00/78')
    run("2000 and after: the year goes past 99", b'', clock(datetime.date(2001, 6, 15), 8, 0, 0),
        expect=b'Fri 06/15/:1 08:00:00')
    run("day 0FFFFh: the year wraps", b'', clock(day=0xFFFF), expect=b'Sun 06/04/01')

    # setting the clock
    now = clock(datetime.date(1985, 6, 1), 1, 2, 3)

    def set_case(label, t, want=None, **kw):
        kw.setdefault('chars', b'x')
        mem = {}

        def keep(m, mpm):
            mem['tod'] = snap(m, mpm)
            return mem['tod']
        r, _ = compare(ck, prog, label, kw, setup=setup(t, now), snap=keep, mpm_class=TodMpm)
        out = bytes(r.out)
        if want is None:
            ck.check(f"{label}: an error", True, b'Illegal time/date specification.' in out)
        else:
            d, h, m, s = want
            ck.check(f"{label}: asks for the key", True, b'Strike key to set time' in out)
            ck.check(f"{label}: then shows the new time", True, shown(d, h, m, s) in out)
            ck.check(f"{label}: the clock is set", clock(d, h, m, s).hex(), mem['tod'])
        return out

    D = datetime.date
    set_case("set a time", b' 10/26/99 12:34:56', (D(1999, 10, 26), 12, 34, 56))
    set_case("set 1/1/78 0:0:0", b' 1/1/78 0:0:0', (D(1978, 1, 1), 0, 0, 0))
    set_case("set feb 29 of a leap year", b' 2/29/80 23:59:59', (D(1980, 2, 29), 23, 59, 59))
    set_case("set feb 28 of a leap year", b' 2/28/84 1:1:1', (D(1984, 2, 28), 1, 1, 1))
    set_case("set mar 1 of a leap year", b' 3/1/84 1:1:1', (D(1984, 3, 1), 1, 1, 1))
    set_case("set jan 31 of a leap year", b' 1/31/88 13:00:00', (D(1988, 1, 31), 13, 0, 0))
    set_case("set dec 31 1999", b' 12/31/99 23:59:59', (D(1999, 12, 31), 23, 59, 59))
    set_case("blanks around the fields", b' 1/ 2 /85  3: 4 : 5', (D(1985, 1, 2), 3, 4, 5))
    set_case("leading zeros", b' 0012/0031/0099 0023:0059:0059', None)  # 21 characters: cut off at 20
    set_case("leading zeros, short", b' 012/031/099 07:08:09', (D(1999, 12, 31), 7, 8, 9))
    set_case("the 21st character is the last", b'     12/31/99 23:59:59', (D(1999, 12, 31), 23, 59, 5))
    set_case("feb 29 of a year that isn't leap", b' 2/29/81 0:0:0')
    set_case("month 13", b' 13/1/80 0:0:0')
    set_case("month 0", b' 0/1/80 0:0:0')
    set_case("april 31", b' 4/31/80 0:0:0')
    set_case("day 0", b' 4/0/80 0:0:0')
    set_case("year 77", b' 4/1/77 0:0:0')
    set_case("hour 24", b' 4/1/80 24:0:0')
    set_case("minute 60", b' 4/1/80 1:60:0')
    set_case("second 60", b' 4/1/80 1:1:60')
    set_case("no seconds", b' 4/1/80 1:1')
    set_case("a dash for a slash", b' 4-1-80 1:1:1')
    set_case("a letter", b' 4/x/80 1:1:1')
    set_case("a three-digit day: too big before the multiply", b' 1/320/80 1:1:1')
    set_case("a three-digit hour", b' 1/1/80 123:1:1')
    # a number past 255 sets CARRY (byte adds): an error (until plmc
    # added bytes in 8 bits, its TOD took 260 as 4: docs/bugs.md #131)
    set_case("260: past a byte at the multiply", b' 1/1/80 1:260:0')
    set_case("256: past a byte at the add", b' 1/1/80 1:256:0')
    run("set, but no key: it waits", b' 10/26/99 12:34:56', now, expect=b'Strike key to set time')

    # P: the clock until a key
    run("P: a key at once", b' P', now, expect=shown(D(1985, 6, 1), 1, 2, 3), chars=b'x')
    run("P: three seconds, then a key", b' P', clock(D(1985, 6, 1), 23, 59, 58),
        expect=shown(D(1985, 6, 2), 0, 0, 1), chars=b'x', polls=3)
    run("P: no key: it runs on", b' P', now, polls=2)

    run("the wrong MP/M", b'', now, expect=b'Requires MP/M-86 2.0', version=0x1022)
    run("CP/M-86's version", b' P', now, expect=b'Requires MP/M-86 2.0', version=0x0022)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
