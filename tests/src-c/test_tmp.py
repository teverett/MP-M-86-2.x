#!/usr/bin/env python3
"""src-c/rsp/tmp.c against src-assem/rsp/TMP.A86, as whole programs (see
tests/src-c/rspref.py): TMP sets its console's defaults, prints the
version message, then loops -- prompt, read a command line, handle X:,
USER and PRINTER itself, hand anything else to the CLI and report its
errors. Each case types some lines (and scripts the CLI's answers); the
run ends when TMP reads one line more.
"""
import sys

from rspref import Checker, DS, Program, compare, pa

E_NO_MEMORY, E_NO_PD, E_Q_FULL, E_ILLDISK, E_BADFNAME, E_BAD_LOAD, E_BAD_OPEN, E_NULLCMD = 3, 12, 15, 23, 24, 28, 30, 31


def main():
    prog = Program('TMP', 'tmp_main', rsp=True, stack=(0x1C0, 0x1D8 + 6))
    ck = Checker()
    ffff = 0xFFFF

    def console(n):
        return lambda m: m.__setitem__(pa(DS, prog.v('defconsole')), n)

    for label, lines, cli, cns in (
            ("empty command", [b''], (), 0),
            ("';' comment, ignored", [b';xx'], (), 0),
            ("'A:' disk change", [b'A:'], (), 0),
            ("'b:' disk change", [b'b:'], (), 1),
            ("'P:', the last drive", [b'P:'], (), 0),
            ("'~:' out of range", [b'~:'], (), 0),
            ("2-char command, not a disk change", [b'ls'], (), 0),
            ("explicit drive -> CLI", [b'DIR A:'], (), 0),
            ("no drive, not a builtin -> CLI", [b'TYPE X.Y'], (), 0),
            ("USER, no argument", [b'USER'], (), 0),
            ("USER 5", [b'USER 5'], (), 2),
            ("USER 16 is invalid", [b'USER 16'], (), 0),
            ("PRINTER 12", [b'PRINTER 12'], (), 0),
            ("PRINTER 99 is refused", [b'PRINTER 99'], (), 0),
            ("bad parse -> clierror", [b'TOOLONGNAME.BAR X'], (), 0),
            ("all blanks -> CLI", [b'   '], (), 0),
            ("CLI: no free PD", [b'DIR'], ((ffff, E_NO_PD),), 0),
            ("CLI: not enough memory", [b'BIG'], ((ffff, E_NO_MEMORY),), 0),
            ("CLI: can't find command", [b'NOPE'], ((ffff, E_BAD_OPEN),), 0),
            ("CLI: load error", [b'BAD'], ((ffff, E_BAD_LOAD),), 0),
            ("CLI: queue full", [b'MPMSTAT'], ((ffff, E_Q_FULL),), 0),
            ("CLI: bad file name", [b'X'], ((ffff, E_BADFNAME),), 0),
            ("CLI: illegal disk", [b'X'], ((ffff, E_ILLDISK),), 0),
            ("CLI: null command", [b'X'], ((ffff, E_NULLCMD),), 0),
            ("CLI: some other error", [b'X'], ((ffff, 99),), 0),
            ("a session", [b'USER 3', b'B:', b'DIR *.*', b'', b'PRINTER 1', b'TYPE A.B', b'c:'],
             ((0, 0), (ffff, E_BAD_OPEN)), 0)):
        compare(ck, prog, label, {'lines': lines, 'cli': cli}, setup=console(cns),
                snap=lambda m, mpm: (mpm.disk, mpm.user, mpm.lst, mpm.console))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
