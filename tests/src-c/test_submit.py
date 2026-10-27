#!/usr/bin/env python3
"""src-c/rsp/submit.c against src-assem/rsp/SUBMIT.A86, as whole programs (see
tests/src-c/rspref.py): SUBMIT reads its .SUB file a line at a time,
puts the command tail's arguments in for $1..$99, prints each command,
does X:, USER, PRINTER and $INCLUDE itself and hands the rest to the
CLI. Each case is a SUB file (and the command tail, the CLI's answers,
the PD's flags); the run ends when SUBMIT exits.
"""
import struct
import sys

from rspref import Checker, DS, PARENT, PD, Program, SYSDAT, P_FLAG, P_PARENT, P_PRIOR, compare, pa

PF_CHILDABORT = 0x800
E_NO_PD, E_NULLCMD = 12, 31


def main():
    prog = Program('SUBMIT', 'submit_main', rsp=False, stack=(0x100, 0x13C))
    ck = Checker()

    def setup(tail, flags=0, parent=True, name=b'T       '):
        def run(m):
            m[pa(DS, 0x5C):pa(DS, 0x5C + 12)] = b'\0' + name + b'   '        # the CLI's FCB: the SUB file
            t = tail.upper()
            m[pa(DS, 0x80)] = len(t)
            m[pa(DS, 0x81):pa(DS, 0x81 + len(t) + 1)] = t + b'\0'
            struct.pack_into('<H', m, pa(SYSDAT, PD + P_FLAG), flags)
            struct.pack_into('<H', m, pa(SYSDAT, PD + P_PARENT), PARENT if parent else 0)
            m[pa(SYSDAT, PARENT + P_PRIOR)] = 198
        return run

    def case(label, sub, tail=b' T', files=None, **kw):
        setup_kw = {k: kw.pop(k) for k in ('flags', 'parent') if k in kw}
        f = {b'T       SUB': sub} if sub is not None else {}
        f.update(files or {})
        ref, _ = compare(ck, prog, label, {'files': f, **kw}, setup=setup(tail, **setup_kw),
                         snap=lambda m, mpm: (mpm.disk, mpm.user, mpm.lst))
        if sub is not None:  # the run got past opening the file
            ck.check(f"{label}: the SUB file was read", True, b"No 'SUB'" not in ref.out and any(
                c[0] == 20 for c in ref.log))
        return ref

    case("no SUB file", None)
    case("simple args", b'TYPE $1\r\nDIR $2\r\n', tail=b' T  foo  bar')
    case("leading tabs/spaces, embedded tab", b'X $1 $2 $3\r\n', tail=b'\t  T\tone\ttwo  ')
    case("two-digit arg", b'ECHO $10 $11\r\n', tail=b' T ' + b' '.join(b'A%d' % i for i in range(11)))
    case("arg out of range", b'TYPE $5.TXT\r\n', tail=b' T FOO')
    case("arg just past the last one", b'TYPE $1 $2\r\n', tail=b' T FOO')
    case("literal $ not followed by a digit", b'PIP X=$Y\r\n')
    case("line fills the buffer", b'X' * 140 + b'\r\n')
    case("LF-only lines, a leading blank line", b'\r\nDIR B:\nTYPE A\n')
    case("';' comment and empty lines", b';ignored\r\n\r\n\r\nDIR\r\n')
    case("'C:' disk change", b'C:\r\nDIR\r\n')
    case("'~:' out of range -> exit", b'~:\r\nDIR\r\n')
    case("2 chars, no ':' -> builtin check on the last parse", b'DIR\r\nls\r\n')
    case("USER, no argument; USER 5; PRINTER 12", b'USER\r\nUSER 5\r\nPRINTER 12\r\nUSER 16\r\n')
    case("$INCLUDE file -> nested submit through the CLI", b'$INCLUDE NEXT\r\n')
    case("$INCLUDE, no file -> CLI as is", b'$INCLUDE\r\n')
    case("$INCLUDE, bad file -> exit", b'$INCLUDE A:B:C\r\nDIR\r\n')
    case("bad parse -> exit", b'TOOLONGNAME.BAR X\r\nDIR\r\n')
    case("CLI error -> exit", b'DIR\r\nTYPE\r\n', cli=((0xFFFF, E_NO_PD),))
    case("CLI null command -> next", b'DIR\r\nTYPE\r\n', cli=((0xFFFF, E_NULLCMD),))
    case("EOF marker mid-file", b'DIR\r\n\x1aTYPE\r\n')
    case("two records", b''.join(b'ECHO LINE %02d\r\n' % i for i in range(20)))
    case("prompt for user 12, disk B", b'DIR\r\n', user=12, disk=1)
    case("ctl-C pending, answer 'n' -> continues", b'DIR\r\nTYPE\r\n', flags=PF_CHILDABORT, chars=b'n')
    case("ctl-C pending, answer 'Y' -> exit", b'DIR\r\nTYPE\r\n', flags=PF_CHILDABORT, chars=b'Y')
    case("no parent -> priority 197", b'DIR\r\n', parent=False)
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
