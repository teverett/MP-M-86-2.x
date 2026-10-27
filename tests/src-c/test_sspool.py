#!/usr/bin/env python3
"""src-c/utils/sspool.c against src-assem/utils/SSPOOL.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): SSPOOL (STOPSPLR) aborts every process named
SPOOL on its console, or on the console 0-253 its argument names, and
says whether there was one. Each case is the argument (the CLI's fcb),
the console it runs on and the processes running; the result is the
processes left, which each case also checks against the ones SSPOOL
should have aborted, so the two sides can't agree on aborting nothing.
"""
import sys

from rspref import Checker, DS, Mpm, Program, compare, pa, r16
import rsp_syms


class SspoolMpm(Mpm):
    """Mpm, plus abort specified process (157) as src-assem/kernel's
    ABORT.RTM does it: DX is the APB (pd, term, cns, net, pname(8)); with
    pd = 0 the process is the first in `procs` -- (name, console) pairs --
    whose name and console both match (FINDPD.RTM's findpdnc). It's gone
    before the call returns ("force abort to happen before we return"):
    BX = 0. No match: BX = 0FFFFh, CX = 20 (e_no_pdname). A pd that isn't
    0 isn't modelled (SSPOOL always passes 0)."""

    def __init__(self, procs=(), **kw):
        super().__init__(**kw)
        self.procs = list(procs)

    def call(self, mem, ds, es, cl, dx):
        if cl == 157:
            self.log.append((cl, dx, ds))
            apb = pa(ds, dx)
            assert r16(mem, apb) == 0, "abort by PD isn't modelled"
            want = (bytes(mem[apb + 6:apb + 14]), mem[apb + 4])
            if want in self.procs:
                self.procs.remove(want)
                return 0, 0, es
            return 0xFFFF, 20, es
        return super().call(mem, ds, es, cl, dx)


SPOOL = b'SPOOL   '


def main():
    prog = Program('SSPOOL', 'sspool_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('SSPOOL_STACK_BASE') - 1024, rsp_syms.value('SSPOOL_STACK_BASE')))
    ck = Checker()

    def arg(a):
        """SSPOOL [a]: the CLI's fcb and tail"""
        t = b' ' + a if a else b''

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 36)] = b'\0' + a.ljust(11)[:11] + bytes(24)
            m[pa(DS, 0x6C):pa(DS, 0x6C + 12)] = b'\0' + b' ' * 11
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
        return run

    others = [(b'CLOCK   ', 0), (SPOOL, 1), (b'TMP0    ', 0)]
    for label, a, kw, left in (
            ("this console: one spooler", b'', {'procs': others + [(SPOOL, 0)]}, others),
            ("this console: two spoolers", b'', {'procs': [(SPOOL, 0)] + others + [(SPOOL, 0)]}, others),
            ("this console (2)", b'', {'console': 2, 'procs': [(SPOOL, 2), (SPOOL, 0)]}, [(SPOOL, 0)]),
            ("not running", b'', {'procs': others}, others),
            ("console 1", b'1', {'procs': others}, [(b'CLOCK   ', 0), (b'TMP0    ', 0)]),
            ("console 1: not running there", b'1', {'procs': [(SPOOL, 0)]}, [(SPOOL, 0)]),
            ("console 12", b'12', {'procs': [(SPOOL, 12), (SPOOL, 1)]}, [(SPOOL, 1)]),
            ("console 253", b'253', {'procs': [(SPOOL, 253)]}, []),
            ("console 007: leading zeros", b'007', {'procs': [(SPOOL, 7)]}, []),
            ("four digits: the first three, 123", b'1234', {'procs': [(SPOOL, 123)]}, []),
            ("console 254: illegal", b'254', {'procs': [(SPOOL, 254)]}, [(SPOOL, 254)]),
            ("console 999: illegal", b'999', {'procs': [(SPOOL, 0)]}, [(SPOOL, 0)]),
            ("not a number: illegal", b'1A', {'procs': [(SPOOL, 1)]}, [(SPOOL, 1)]),
            ("a letter: illegal", b'X', {'procs': [(SPOOL, 0)]}, [(SPOOL, 0)]),
            ("a character below '0': illegal", b'#', {'procs': [(SPOOL, 0)]}, [(SPOOL, 0)]),
            ("the wrong MP/M (2.1)", b'', {'version': 0x1131, 'procs': [(SPOOL, 0)]}, [(SPOOL, 0)]),
            ("the wrong MP/M (CP/M-86)", b'', {'version': 0x0022, 'procs': [(SPOOL, 0)]}, [(SPOOL, 0)])):
        ref, _ = compare(ck, prog, label, kw, setup=arg(a), snap=lambda m, mpm: mpm.procs, mpm_class=SspoolMpm)
        ck.check(f"{label}: what SSPOOL should leave running", left, ref.procs)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
