#!/usr/bin/env python3
"""Differential test for src-c/kernel/parse.c against src/kernel/PARSE.SUP's
parse_entry, on real memory layout -- see tests/src-c/supref.py. The PCB,
the string and the FCB are all in the running process's u_wrkseg. Every
case compares bx/cx and diffs the whole image (the FCB bytes included).
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, SYSDAT, BxCx, build_lib, c_buffer, wk, w16, KERNEL_C

PCB, NAME, FCB = 0x0100, 0x0140, 0x0200

CASES = [
    ("simple name.type", b"FOO.BAR\x00"),
    ("name only, NUL-terminated", b"FOO\x00"),
    ("name only, CR-terminated", b"FOO\r"),
    ("leading blanks", b"   FOO.BAR\x00"),
    ("with drive letter", b"B:FOO.BAR\x00"),
    ("lowercase gets upper-cased", b"foo.bar\x00"),
    ("wildcard name", b"*.BAR\x00"),
    ("wildcard type", b"FOO.*\x00"),
    ("with password", b"FOO.BAR;SECRET\x00"),
    ("more to parse (comma-separated)", b"FOO.BAR,BAZ.QUX\x00"),
    ("empty string", b"\x00"),
    ("illegal drive letter", b"9:FOO\x00"),
    ("drive letter out of range", b"[:FOO\x00"),
    ("name too long", b"TOOLONGNAME.BAR\x00"),
    ("type too long", b"FOO.TOOLONG\x00"),
    ("illegal character", b"FOO\x01BAR\x00"),
    ("drive then illegal delim", b"A:.FOO\x00"),
]


def main():
    ref = SysRef()
    lib_path = build_lib([os.path.join(KERNEL_C, 'parse.c')])
    try:
        lib = ctypes.CDLL(lib_path)
        lib.parse_entry.restype = BxCx
        lib.parse_entry.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint16]
        ck = Checker(ref)
        for label, name in CASES:
            mem = ref.base_memory()
            w16(mem, wk(PCB), NAME)
            w16(mem, wk(PCB + 2), FCB)
            mem[wk(NAME):wk(NAME) + len(name)] = name
            mem[wk(FCB):wk(FCB) + 36] = b'\xEE' * 36
            regs, img = ref.run('parse_entry', mem, {'dx': PCB})
            buf = c_buffer(mem)
            r = lib.parse_entry(buf, SYSDAT, PCB)
            ck.compare(label, img, buf, (regs['bx'], regs['cx']), (r.bx, r.cx),
                       lambda i: bytes(i[wk(FCB):wk(FCB) + 36]))
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
