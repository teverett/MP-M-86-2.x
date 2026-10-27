#!/usr/bin/env python3
"""src-c/rsp/mpmstat.c (with src-c/utils/stscom.c) against
src-assem/utils/STSRSP.PLM (which includes STSCOM.PLM), compiled with
plmc --rsp MPMSTAT as src-assem's build makes MPMSTAT.RSP, as whole
programs (see tests/src-c/rspref.py): the MPMSTAT RSP makes and opens
its queue, then for each command the CLI writes to it -- the typing
process's PD, then the tail -- prints the status display on that
process's console and detaches it. The run ends when the queue is
empty.

The SYSDATs are test_stscmd.py's: the same running systems the
transient MPMSTAT.CMD is checked against.
"""
import struct
import sys

import rspref  # noqa: F401  (test_stscmd moves CS down: the RSP's code is 1761h bytes)
from rspref import Checker, Program, compare
import rsp_syms
from test_stscmd import CLOCK, DIRX, TMP0, TMP1, StsMpm, empty, system


def msg(pd, tail=b''):
    """a CLI message for an RSP queue: the sender's PD, then the tail"""
    return struct.pack('<H', pd) + tail + b'\0'


def main():
    prog = Program('STSRSP', 'mpmstat_main', rsp=True, plm=True, c_name=['mpmstat.c', '../utils/stscom.c'],
                   # the asm's stack: below the initial frame, which it starts
                   # past (rspref: sp = frame + 6), so its first push is there
                   stack=(rsp_syms.value('MPMSTAT_BP_TBUFF') + 128, rsp_syms.value('MPMSTAT_RSP_STACK_TOP') + 6))
    ck = Checker()

    def run(label, setup, expect=(), absent=(), **kw):
        ref, _ = compare(ck, prog, label, kw, setup=setup, mpm_class=StsMpm)
        out = bytes(ref.out)
        # the reference's output is MPMSTAT's real display, not a failure both share
        ck.check(f"{label}: shows what it should", [], [e for e in expect if e not in out])
        ck.check(f"{label}: and not what it shouldn't", [], [e for e in absent if e in out])
        ck.check(f"{label}: the consoles it displayed on",
                 [d for (c, *d) in ref.log if c == 148], [d for (c, *d) in _.log if c == 148])
        return ref

    full = [b'MP/M-86 V2.0 Test', b'****** Status Display - Values Shown In Hexadecimal *****',
            b'Number of Physical Consoles = 02', b'Number of Free Process Descriptors = 03',
            b'Number of Flags = 08', b' MPMSTAT [00]', b'[CliQ    ]   Tmp0    [00]',
            b'  02 -        CLOCK   [00]', b' MXdisk  [ABORT   ]', b'Control P - Console [00]',
            b'2000   0100  MPMSTAT', b'D000   1000  * FREE *', b'Consoles:', b'Printers:']
    for label, setup, kw, expect, absent in (
            ("no command yet: the queue made and opened", system(), {}, [], [b'Status']),
            ("one command, from console 0", system(), {'queue': [msg(TMP0)]}, full, []),
            ("from console 1", system(), {'queue': [msg(TMP1, b' P')]}, full, []),
            ("two commands: two displays", system(), {'queue': [msg(TMP0), msg(DIRX)]}, full, []),
            ("a wide ready list", system(wide=True), {'queue': [msg(CLOCK)]}, [b' USER2   [02]'], []),
            # one flag, not test_stscmd's none: with nflags 0 STSCOM walks 256
            # entries, and here some point past the test machine's 64K
            ("one flag, nothing ready, no partitions", system(nflags=1, ready=False, partitions=False),
             {'queue': [msg(TMP0)]}, [b'Number of Flags = 01'], [b'* FREE *']),
            ("no printers", system(nciodev=2), {'queue': [msg(TMP0)]}, [b'Consoles:'], [b'Printers:']),
            ("an empty system", empty, {'queue': [msg(TMP0)]}, [b'Number of Flags = 00'], [b'Consoles:'])):
        ref = run(label, setup, expect, absent, **kw)
        if label.startswith("two"):
            ck.check(f"{label}: displayed twice", 2, bytes(ref.out).count(b'****** Status Display'))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
