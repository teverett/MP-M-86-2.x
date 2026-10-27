#!/usr/bin/env python3
"""src-c/toolchain/gentab.c, the C version of src-assem/toolchain/
GENTAB.PLI: built for the host (it's a host tool, as GENTAB.CPM was),
run on src-assem/toolchain/OP86.DAT, it must write OPTAB.DAT and
OPTAB.LIT byte for byte as the ones in src-assem/toolchain/ are --
DRI's GENTAB's own output (orig/mpm862sr/06), which DIS86.PLM, INS86.PLM
and ASMTAB.PLM include. (GENTAB.CPM itself, PL/I-80 code for CP/M-80,
has nothing here to run on.)

Also: the console's per-length counts, and an OP86.DAT with a mnemonic
added, whose tables and literals move as GENTAB.PLI's loops say.
"""
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLCHAIN = os.path.join(REPO_ROOT, 'src-assem', 'toolchain')
GENTAB_C = os.path.join(REPO_ROOT, 'src-c', 'toolchain', 'gentab.c')


def main():
    failures = []

    def check(label, ok, detail=''):
        print(f"[{'ok' if ok else 'FAIL'}] {label}{': ' + detail if detail else ''}")
        if not ok:
            failures.append(label)

    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, 'gentab')
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-o', exe, GENTAB_C], check=True)

        def gentab(op86):
            dat, lit = os.path.join(tmp, 'OPTAB.DAT'), os.path.join(tmp, 'OPTAB.LIT')
            out = subprocess.run([exe, op86, dat, lit], check=True, capture_output=True, text=True).stdout
            return open(dat, 'rb').read(), open(lit, 'rb').read(), out

        dat, lit, out = gentab(os.path.join(TOOLCHAIN, 'OP86.DAT'))
        for name, got in (('OPTAB.DAT', dat), ('OPTAB.LIT', lit)):
            want = open(os.path.join(TOOLCHAIN, name), 'rb').read()
            check(f"{name} as DRI's GENTAB wrote it", got == want, f"{len(got)} bytes, {len(want)} expected")
        check("the counts on the console", out.split() == ['12', '2-character', 'opcodes', '69', '3-character',
                                                           'opcodes', '25', '4-character', 'opcodes', '16',
                                                           '5-character', 'opcodes', '2', '6-character', 'opcodes'],
              repr(out))

        # a 5-character mnemonic added before XCHG: ops5 grows, ops6 and
        # the literals after it move along
        op86 = open(os.path.join(TOOLCHAIN, 'OP86.DAT'), 'rb').read().replace(b'XCHG\r\n', b'XYZZY\r\nXCHG\r\n')
        path = os.path.join(tmp, 'OP86X.DAT')
        open(path, 'wb').write(op86)
        dat, lit, _ = gentab(path)
        check("an added mnemonic: nops", b'nops (5) byte public initial (12, 69, 25, 17, 2);' in dat)
        check("an added mnemonic: opn$in", b'initial (0, 12, 81, 106, 123, 255);' in dat)
        check("an added mnemonic: its literal", b"\tXYZZY$in literally 'op5$in + 16'," in lit)
        check("an added mnemonic: op6$in moves", b"\top6$in literally '123';" in lit)
        check("an added mnemonic: whole records", len(dat) % 128 == 0 and len(lit) % 128 == 0)
    if failures:
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
