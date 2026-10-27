#!/usr/bin/env python3
"""src-c/utils/attach.c against src-assem/utils/ATTACH.PLM (compiled as
src-assem's build compiles it), as whole programs (see
tests/src-c/rspref.py): ATTACH name gives this program's console to the
process called name whose own console is the same, through the ACB and
function 149.

rspref's Mpm has no processes or consoles, so AttachMpm adds a thread
list and conassign_entry's semantics (CONSOLE.CIO). Each case is a
command and a thread list; the result is who has which console, and
each case also checks what that should be. conassign_entry fails with
BX = 0FFFFh and the code in CX; ATTACH reads it with mon5 (the
interface stub's CX-returning call), so no such process is "Can't Find
PD" and any other failure "Attach Failed" (docs/bugs.md #133).
"""
import sys

from rspref import Checker, DS, Mpm, Program, compare, pa
import rsp_syms

E_ILL_CNS, E_NO_PDNAME, E_NOT_OWNER = 19, 20, 32  # ERR.DEF


class AttachMpm(Mpm):
    """Mpm with a thread list and console owners, for function 149
    (console assign).
      pds      -- the processes, in thread order: dicts with name (8
                  bytes) and cns; the caller, the program's own PD, is first
      ncns     -- SYSDAT's ncns: conassign_entry refuses a console above it
      attached -- {console: name} of consoles some process has (the
                  caller doesn't have its own: the CLI gave it, detached)
    The ACB (DS:DX): cns, match (nonzero: the PD's console must be cns),
    pd (0: find by name), name. A failure is BX = 0FFFFh, CX = the code."""

    def __init__(self, pds=(), ncns=3, attached=None, **kw):
        super().__init__(**kw)
        self.pds = [{'name': b'ATTACH  ', 'cns': self.console}] + [dict(p) for p in pds]
        self.ncns, self.attached = ncns, dict(attached or {})

    @staticmethod
    def _same(a, b):  # findpdname_entry's compare ignores bit 7
        return all(((x - y) & 0x7F) == 0 for x, y in zip(a, b))

    def _fail(self, code, es):
        return 0xFFFF, code, es

    def call(self, mem, ds, es, cl, dx):
        if cl != 149:
            return super().call(mem, ds, es, cl, dx)
        self.log.append((cl, dx, ds))
        acb = pa(ds, dx)
        cns, match, pd, name = mem[acb], mem[acb + 1], mem[acb + 2] | mem[acb + 3] << 8, bytes(mem[acb + 4:acb + 12])
        if cns > self.ncns:  # `cmp ah,ncns ! jbe as_gcns`
            return self._fail(E_ILL_CNS, es)
        if self.attached.get(cns, self.pds[0]['name']) != self.pds[0]['name']:
            return self._fail(E_NOT_OWNER, es)
        if pd:  # a PD address: the n-th on the thread (ATTACH never passes one)
            cands = self.pds[pd - 1:pd]
        else:
            cands = [p for p in self.pds if self._same(p['name'], name)]
        hit = next((p for p in cands if not match or p['cns'] == cns), None)
        if hit is None:
            return self._fail(E_NO_PDNAME, es)
        self.attached[cns] = (hit['name'], self.pds.index(hit))
        return 0, 0, es


def main():
    prog = Program('ATTACH', 'attach_main', rsp=False, plm=True,
                   stack=(rsp_syms.value('ATTACH_STACK_BASE') - 1024, rsp_syms.value('ATTACH_STACK_BASE')))
    ck = Checker()

    def command(name):
        """ATTACH name: what the CLI leaves in the base page"""
        tail = b' ' + name

        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 36)] = b'\0' + name.ljust(8)[:8] + b'   ' + bytes(24)
            m[pa(DS, 0x80)] = len(tail)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(tail) + 1)] = tail + b'\0'
        return run

    PDS = [{'name': b'ED      ', 'cns': 1}, {'name': b'PIP     ', 'cns': 2},
           {'name': b'TWIN    ', 'cns': 2}, {'name': b'TWIN    ', 'cns': 1}]
    FAILED, CANT = b'\r\nAttach Failed ', b"\r\nCan't Find PD "

    for label, cmd, kw, attached, out in (
            ("a process on this console", command(b'ED'), {'console': 1}, {1: (b'ED      ', 1)}, b''),
            # (none of that name on this console: findpdname's e_no_pdname)
            ("one on another console", command(b'PIP'), {'console': 1}, {}, CANT),
            ("no such process", command(b'NONE'), {'console': 1}, {}, CANT),
            ("no name", command(b''), {'console': 1}, {}, CANT),
            ("two of a name: the one on this console", command(b'TWIN'), {'console': 1},
             {1: (b'TWIN    ', 4)}, b''),
            ("console 3 = ncns: let through", command(b'PIP'), {'console': 3, 'pds': PDS + [{'name': b'PIP     ',
                                                                                          'cns': 3}]},
             {3: (b'PIP     ', 5)}, b''),
            ("console 4 > ncns", command(b'PIP'), {'console': 4, 'pds': PDS + [{'name': b'PIP     ', 'cns': 4}]},
             {}, FAILED),
            ("another process has the console", command(b'ED'), {'console': 1, 'attached': {1: b'PIP     '}},
             {1: b'PIP     '}, FAILED),
            ("itself", command(b'ATTACH'), {'console': 1}, {1: (b'ATTACH  ', 0)}, b''),
            ("the wrong MP/M", command(b'ED'), {'console': 1, 'version': 0x1131}, {}, b'\r\nRequires MP/M-86 ')):
        ref, port = compare(ck, prog, label, {'pds': PDS, **kw}, setup=cmd, mpm_class=AttachMpm,
                            snap=lambda m, mpm: mpm.attached)
        ck.check(f"{label}: expected", (attached, out), (port.attached, bytes(port.out)))
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
