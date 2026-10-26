#!/usr/bin/env python3
"""Differential test for src-c/kernel/memif.c's memif_init against
src/kernel/MEMIF.MEM's `init`, on real memory layout. (`entry` is
covered by tests/src-c/test_memif_entry.py.)

The reference is the whole real MEM module (src/kernel/MEM.A86); see
tests/src-c/memref.py. `init` saves DS (SYSDAT) in cs:sysdat and copies
SYSDAT's module_table[SUP] far pointer (`supmod`) into cs:supervisor.
The code segment is at 0 here, so those cells are the first bytes of
the image and are compared explicitly.
"""
import ctypes
import os
import sys

from memref import Checker, MemRef, MEM_C, build_lib, c_buffer, sd, w16, r16, SYSDAT

SUPMOD = (0x1234, 0x5678)


def main():
    ref = MemRef()
    s = ref.s
    lib_path = build_lib(MEM_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.memif_init.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint16]
        ck = Checker(ref)
        retf = [i for i in range(s['init'], s['init'] + 0x20) if ref.code[i] == 0xCB][0]

        mem = ref.base_memory()
        w16(mem, sd(s['supmod']), SUPMOD[0])
        w16(mem, sd(s['supmod'] + 2), SUPMOD[1])
        _, img = ref.run('init', mem, {}, stop_addrs={retf})
        buf = c_buffer(mem)
        lib.memif_init(buf, SYSDAT, 0)
        cells = lambda i: (r16(i, s['sysdat']), r16(i, s['supervisor']), r16(i, s['supervisor'] + 2))  # noqa: E731
        ck.check("init: cs:sysdat / cs:supervisor", cells(img), cells(bytes(buf)))
        ck.check("init: values", cells(bytes(buf)), (SYSDAT,) + SUPMOD)
        ck.compare("init", img, buf, (), ())
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
