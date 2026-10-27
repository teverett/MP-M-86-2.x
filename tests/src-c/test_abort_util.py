#!/usr/bin/env python3
"""src-c/utils/abort.c against src-assem/utils/ABORT.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): ABORT name [n] aborts the first process called
name on console n -- this one's own console without n -- through the
Abort Parameter Block and function 157, and says when it can't. (The
kernel's ABORT.RTM has its own test, test_abort.py.)

rspref's Mpm has no processes, so AbortMpm adds a thread list and
abort_spec_entry's semantics (ABORT.RTM): find the PD by name and
console, fail on keep, defer on tempkeep, refuse a system process unless
the termination code's low byte is 0FFh; aborting the caller is a
terminate. Each case is a command (the CLI's two fcbs and the tail) and
a thread list; the result is which processes were aborted, and each
case also checks what that should be.
"""
import sys

from rspref import Checker, DS, Mpm, Program, compare, pa
import rsp_syms

E_NO_PDNAME, E_PD_NOTERM = 20, 35  # ERR.DEF


class AbortMpm(Mpm):
    """Mpm with a thread list for function 157 (abort process).
      pds -- the other processes, in thread order: dicts with name (8
             bytes), cns, and optionally sys/keep/tempkeep. The caller --
             the program's own PD, its name, on Mpm's console -- is first.
    The APB (DS:DX): pd (0: find by name and console), term, cns, net,
    pname. Name match ignores bit 7, as findpdname_entry does."""

    def __init__(self, pds=(), **kw):
        super().__init__(**kw)
        self.pds = [{'name': b'ABORT   ', 'cns': self.console, 'self': True}] + [dict(p) for p in pds]
        self.aborted = []

    @staticmethod
    def _same(a, b):
        return all(((x - y) & 0x7F) == 0 for x, y in zip(a, b))

    def call(self, mem, ds, es, cl, dx):
        if cl != 157:
            return super().call(mem, ds, es, cl, dx)
        self.log.append((cl, dx, ds))
        apb = pa(ds, dx)
        pd, term = mem[apb] | mem[apb + 1] << 8, mem[apb + 2] | mem[apb + 3] << 8
        cns, name = mem[apb + 4], bytes(mem[apb + 6:apb + 14])
        if pd:  # a PD address: the n-th on the thread (ABORT never passes one)
            hit = self.pds[pd - 1] if pd <= len(self.pds) else None
        else:
            hit = next((p for p in self.pds if self._same(p['name'], name) and p['cns'] == cns), None)
        if hit is None:
            return 0xFFFF, E_NO_PDNAME, es
        if hit.get('self'):
            self.aborted.append(hit['name'])
            return None  # terminate_entry: the caller ends
        if hit.get('keep') or (hit.get('sys') and term & 0xFF != 0xFF):
            hit['ctlc'] = False
            return 0xFFFF, E_PD_NOTERM, es
        if hit.get('tempkeep'):  # abort pending, at its next ^C check
            hit['ctlc'] = True
            return 0, 0, es
        self.pds.remove(hit)
        self.aborted.append(hit['name'])
        return 0, 0, es


def fcb11(name):
    return name.ljust(8)[:8] + b'   '


def main():
    prog = Program('ABORT', 'abort_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('ABORT_STACK_BASE') - 1024, rsp_syms.value('ABORT_STACK_BASE')))
    ck = Checker()

    def command(name, cns=b''):
        """ABORT name [cns]: what the CLI leaves in the base page"""
        tail = b' ' + name + (b' ' + cns if cns else b'')

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 16)] = b'\0' + fcb11(name) + bytes(4)
            m[pa(DS, 0x6C):pa(DS, 0x6C + 16)] = b'\0' + fcb11(cns) + bytes(4)
            m[pa(DS, 0x80)] = len(tail)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(tail) + 1)] = tail + b'\0'
        return run

    EDIT = {'name': b'ED      ', 'cns': 2}
    PDS = [EDIT, {'name': b'PIP     ', 'cns': 1}, {'name': b'PIP     ', 'cns': 12},
           {'name': b'ED      ', 'cns': 253}, {'name': b'KEPT    ', 'cns': 2, 'keep': True},
           {'name': b'TEMP    ', 'cns': 2, 'tempkeep': True}, {'name': b'CLOCK   ', 'cns': 2, 'sys': True},
           {'name': b'TWIN    ', 'cns': 2}, {'name': b'TWIN    ', 'cns': 2},
           {'name': b'ED      ', 'cns': 123}]
    OK, FAILED = b'', b'\r\nAbort Failed.'
    ILLEGAL = b'\r\nIllegal Console, Use 0-253 '

    for label, cmd, kw, aborted, out in (
            ("its own console", command(b'ED'), {'console': 2}, [b'ED      '], OK),
            ("another console's process: not ours", command(b'PIP'), {'console': 2}, [], FAILED),
            ("console 1", command(b'PIP', b'1'), {'console': 2}, [b'PIP     '], OK),
            ("console 12: two digits", command(b'PIP', b'12'), {'console': 2}, [b'PIP     '], OK),
            ("console 253: three digits", command(b'ED', b'253'), {'console': 2}, [b'ED      '], OK),
            ("console 254: illegal", command(b'ED', b'254'), {'console': 2}, [], ILLEGAL),
            ("console 999: illegal", command(b'ED', b'999'), {'console': 2}, [], ILLEGAL),
            ("console 1234: the fourth digit ignored", command(b'ED', b'1234'), {'console': 2}, [b'ED      '], OK),
            ("console 12A: not a number", command(b'PIP', b'12A'), {'console': 2}, [], ILLEGAL),
            ("console X: not a number", command(b'ED', b'X'), {'console': 2}, [], ILLEGAL),
            ("console -1: below '0' wraps", command(b'ED', b'-1'), {'console': 2}, [], ILLEGAL),
            ("console 02: a leading zero", command(b'ED', b'02'), {'console': 0}, [b'ED      '], OK),
            ("no such process", command(b'NONE'), {'console': 2}, [], FAILED),
            ("no name", command(b''), {'console': 2}, [], FAILED),
            ("a keep process", command(b'KEPT'), {'console': 2}, [], FAILED),
            ("a tempkeep process: pending", command(b'TEMP'), {'console': 2}, [], OK),
            ("a system process: aborted all the same", command(b'CLOCK'), {'console': 2}, [b'CLOCK   '], OK),
            ("two of a name: the first", command(b'TWIN'), {'console': 2}, [b'TWIN    '], OK),
            ("itself: a terminate", command(b'ABORT'), {'console': 2}, [b'ABORT   '], OK),
            ("itself, on another console: not found", command(b'ABORT', b'3'), {'console': 2}, [], FAILED),
            ("the wrong MP/M", command(b'ED'), {'console': 2, 'version': 0x1022}, [], b'\r\nRequires MP/M-86')):
        ref, port = compare(ck, prog, label, {'pds': PDS, **kw}, setup=cmd, mpm_class=AbortMpm,
                            snap=lambda m, mpm: (mpm.aborted, [(p['name'], p['cns'], p.get('ctlc'))
                                                               for p in mpm.pds]))
        ck.check(f"{label}: expected", (aborted, out), (port.aborted, bytes(port.out)))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
