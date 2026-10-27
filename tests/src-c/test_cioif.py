#!/usr/bin/env python3
"""Differential test for src-c/kernel/cioif.c's cioif_init against
src-assem/kernel/CIOIF.CIO's `init`, on real memory layout -- see
tests/src-c/cioref.py. (`entry` is covered by test_cioif_entry.py.)
`init` saves DS (SYSDAT) in cs:sysdat and copies SYSDAT's
module_table[SUP] far pointer (`supmod`) into cs:supervisor; the code
segment is at 0, so those cells are compared explicitly.
"""
import ctypes
import os
import sys

from cioref import CioRef, Checker, CIO_C, SYSDAT, build_lib, c_buffer, sd, w16, r16, u16, c_u8p

SUPMOD = (0x1234, 0x5678)


def main():
    ref = CioRef()
    s = ref.s
    lib_path = build_lib(CIO_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.cioif_init.argtypes = [c_u8p, u16, u16]
        ck = Checker(ref)
        retf = [i for i in range(s['init'], s['init'] + 0x20) if ref.code[i] == 0xCB][0]

        mem = ref.base_memory()
        w16(mem, sd(s['supmod']), SUPMOD[0])
        w16(mem, sd(s['supmod'] + 2), SUPMOD[1])
        _, img = ref.run('init', mem, {}, stop_addrs={retf})
        buf = c_buffer(mem)
        lib.cioif_init(buf, SYSDAT, 0)
        cells = lambda i: (r16(i, s['sysdat']), r16(i, s['supervisor']), r16(i, s['supervisor'] + 2))  # noqa: E731
        ck.check("init: cs:sysdat / cs:supervisor", cells(img), cells(bytes(buf)))
        ck.check("init: values", cells(bytes(buf)), (SYSDAT,) + SUPMOD)
        ck.compare("init", img, buf, (), ())
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
