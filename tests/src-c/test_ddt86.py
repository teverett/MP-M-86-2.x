#!/usr/bin/env python3
"""src-c/toolchain/ddt86.c against src-assem/toolchain/DDT86.A86 (in
DDT86.CMD as tools/build_ddt86.py builds it, with its disassembler and
assembler overlays, whose C versions -- dis86.c, ins86.c, ass86.c,
asmtab.c -- the C side runs), as whole programs (tests/src-c/ddtref.py):
each case is a script of console lines (and keys for DDT86's break
test), a disk, and the user memory; both sides must make the same
system calls, print the same and leave the same memory and files.

Every command is covered but G, T and U, which run user code: the C
refuses each as an invalid command (ddt86.c's header comment), checked
against the asm refusing one (J).
"""
import sys

import ddtref
from ddtref import DSEG, USER_SEG, cmd_file, pa, sym
from rspref import Checker
from memref import r16

USER = pa(USER_SEG, 0)
CODE = bytes.fromhex('B83412' 'BB0001' 'B90200' 'CDE0' '90' '43' 'EBFE' 'C3')  # mov ax/bx/cx, int, nop, inc, jmp $, ret
TEXT = b'Hello, DDT86 world! 0123456789AB'


class ShortChecker(Checker):
    def check(self, label, ref, port):
        ok = ref == port
        r = repr(ref)
        print(f"[{'ok' if ok else 'FAIL'}] {label}" + ('' if ok else f": reference {r[:300]} | port {repr(port)[:300]}"))
        if not ok:
            self.failures.append(label)


def user_memory(mem):
    mem[USER:USER + 0x100] = bytes(range(256))
    mem[USER + 0x100:USER + 0x100 + len(TEXT)] = TEXT
    mem[USER + 0x200:USER + 0x200 + len(CODE)] = CODE


def lines(*ls):
    return [x if isinstance(x, bytes) else x.encode() for x in ls]


def expect(ck, label, out, *subs):
    for s in subs:
        ck.check(f"{label}: prints {s!r}", True, s in out)


def main():
    ck = ShortChecker()
    n = 0

    def case(label, ls=(), **kw):
        nonlocal n
        n += 1
        kw.setdefault('setup', user_memory)
        return ddtref.compare(ck, label, lines(*ls), **kw)

    # start-up: the sign-on, the BDOS vector copied to 225, the old vectors saved
    ref, mpm, port, _ = case("start-up")
    ck.check("start-up: the sign-on and prompt", b'DDT86 1.1\r\n-', bytes(mpm.out))
    ck.check("start-up: vector 225 is 224's", [r16(ref, 0x380), r16(ref, 0x382)],
             [r16(port, 0x384), r16(port, 0x386)])
    ck.check("start-up: errmode on (MP/M)", 1, port[pa(DSEG, sym('errmode'))])
    ck.check("start-up: vectorsave", bytes(ddtref.machine()[4:16]),
             bytes(port[pa(DSEG, sym('vectorsave')):pa(DSEG, sym('vectorsave')) + 12]))
    ck.check("start-up: the user's SP, flags", [0x100, DSEG, 0x0200],
             [r16(port, pa(DSEG, sym('usersp'))), r16(port, pa(DSEG, sym('userss'))),
              r16(port, pa(DSEG, sym('userfl')))])
    _, mpm, _, _ = case("CP/M-86 BDOS (no error mode)", ["X"], mpm_args={'version': 0x0022})
    ck.check("CP/M-86 BDOS: no function 45", [], [c for c in mpm.log if c[0] == 45])

    # errors: bad commands, comments, empty lines
    _, mpm, _, _ = case("bad commands", ["Q", "1", "", ";comment", "C", "Z", "J", "[", "q", "N5"])
    ck.check("bad commands: each refused", 8, bytes(mpm.out).count(b'\r\n?'))

    # G, T and U: refused as an invalid command is
    gtu = ["G", "G100", "GA00:200,205", "T", "TS3", "U5", "g"]
    conbuff = set(range(pa(DSEG, sym('conbuff')), pa(DSEG, sym('conbuff') + 65)))
    _, mpm, _, pm = ddtref.compare(ck, "G, T, U", lines(*gtu), ref_lines=lines(*['J' + g[1:] for g in gtu]),
                                   setup=user_memory, skip=conbuff)
    n += 1
    ck.check("G, T, U: each refused", len(gtu), bytes(pm.out).count(b'\r\n?'))

    # H
    _, mpm, _, _ = case("H", ["H1234,4321", "H5,10", "HFFFF,1", "H0,0", "Hg,1", "H1:2", "H", "H12345,1",
                              "H1 2", "H1,2,3"])
    expect(ck, "H", bytes(mpm.out), b'H1234,4321\r\r\n5555 CF13', b'0015 FFF5', b'0000 FFFE', b'2346 2344')

    # D, DW
    _, mpm, _, _ = case("D", ["DA00:0", "D", "DA00:100,11F", "DWA00:0,1F", "DW", "D0:FFC0", "DA00:F0,10F",
                              "DDS:380,38F", "DX:0", "DA00:10,5", "DA00:", "DA00:0,G", "DW0:FFE0", "DW0:FFF8,FFFE"])
    expect(ck, "D", bytes(mpm.out),
           b'0A00:0000 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F ................',
           b'0A00:0100 48 65 6C 6C 6F 2C 20 44 44 54 38 36 20 77 6F 72 Hello, DDT86 wor',
           b'0A00:0000 0100 0302 0504', b'0000:FFF0 ')

    # the 40-column patch: S sets DDT86's own COL40
    col40 = f"S{DSEG:X}:{sym('col40'):X}"
    _, mpm, _, _ = case("40 columns", [col40, "1", ".", "DA00:0", "DWA00:0", "X", "XAX", "."])
    expect(ck, "40 columns", bytes(mpm.out), b'0A00:0000 00 01 02 03 04 05 ......\r\n0A00:0006',
           b'0A00:0000 0100 0302 0504 0706 ........\r\n')

    # the break test: any key stops a display, ^S holds it for a key
    _, mpm, _, _ = case("^C", ["DA00:0", "LA00:200", "H1,1"], mpm_args={'chars': b'\x03\x03'})
    ck.check("^C: nothing displayed", False, b'0A00:0000' in bytes(mpm.out))
    _, mpm, _, _ = case("^S", ["DA00:0,3F", "H1,1"], mpm_args={'chars': b'\x13 '})
    expect(ck, "^S", bytes(mpm.out), b'0A00:0030', b'0002 0000')

    # F, FW
    ref, mpm, _, _ = case("F", ["FA00:0,F,AA", "FWA00:10,1F,1234", "FA00:20,2F,100", "FA00:10,0,1", "FA00:0,F",
                                "FA00:30,3F,G", "F0:FFFE,FFFF,55", "FW0:FFF0,FFFF,ABCD", "DA00:0,3F"])
    ck.check("F: filled", bytes([0xAA] * 16) + b'\x34\x12' * 8, bytes(ref[USER:USER + 0x20]))

    # M, B
    ref, mpm, _, _ = case("M, B", ["MA00:0,F,A00:300", "BA00:0,F,A00:300", "SA00:305", "77", ".",
                                   "BA00:0,F,A00:300", "BA00:0,F", "MA00:10,0,A00:300", "M", "B1,2,3,4",
                                   "MA00:100,11F,A00:310", "DA00:300,32F", "MDS:0,3,A00:340"])
    ck.check("M: moved", bytes(range(16)), bytes(ref[USER + 0x300:USER + 0x305]) + b'\x05' +
             bytes(ref[USER + 0x306:USER + 0x310]))
    expect(ck, "B", bytes(mpm.out), b'0A00:0005 05  0A00:0305 77')

    # S, SW
    ref, mpm, _, _ = case("S", ["SA00:400", "12", "", "34", "123", "SA00:400", "x", "SWA00:410", "1234", "ABCD",
                                ".", "S", "S0:FFFE", "5A", "", "SW0:FFFE", "BEEF", "SDS:380", "."])
    ck.check("S: set", bytes([0x12, 0x00, 0x34]), bytes(ref[USER + 0x400:USER + 0x403]))
    ck.check("SW: set", bytes([0x34, 0x12, 0xCD, 0xAB]), bytes(ref[USER + 0x410:USER + 0x414]))

    # X: registers and flags
    ref, mpm, port, _ = case("X", ["X", "XAX", "1234", "", "5678", ".", "XCS", "A00", "A00", "", "", "200",
                                   "XC", "1", "XZ", "1", "XO", "", "XS", "1", "XQ", "XAB", "XAXY", "XC", "2",
                                   "XD", "X1", "XBX", "1G", "X", "LA00:200,20A"])
    expect(ck, "X", bytes(mpm.out),
           b'           AX   BX   CX   DX   SP   BP   SI   DI   CS   DS   SS   ES   IP   \r\n'
           b'--I------ 0000 0000 0000 0000 0100 0000 0000 0000 0000 0000 0410 0000 0000 \r\n',
           b'--I-SZ--C 1234 0000 5678 0000 0100 ', b'0A00 0A00 0410 0000 0200 \r\nMOV    AX,1234')
    ck.check("X: the flags", 0x0200 | 0x01 | 0x40 | 0x80, r16(port, pa(DSEG, sym('userfl'))) & 0x0FFF)

    # L: the disassembler overlay; without it (DISEMPRESENT patched) L is refused and X prints a byte
    dis = f"S{DSEG:X}:{sym('disempresent'):X}"
    _, mpm, _, _ = case("L", ["LA00:200", "L", "LA00:200,208", "LA00:0", "L,", "LA00:200,", "XCS", "A00", ".",
                              "X", dis, "0", ".", "L", "X"])
    expect(ck, "L", bytes(mpm.out), b'0A00:0200 MOV    AX,1234\r\n0A00:0203 MOV    BX,0100',
           b'INT    E0', b'JMPS   020D', b'0A00:000C ')

    # A: the assembler overlay, its errors (asment), and its end at the top of a segment (plmset)
    asm_p = f"S{DSEG:X}:{sym('assempresent'):X}"
    ref, mpm, _, _ = case("A", ["AA00:500", "MOV AX,1234", "NOP", "FOO BAR", "INC BX", "MOV CX,[BX+SI+10]", "",
                                "LA00:500,50A", "A", "A0:FFFE", "NOP", "NOP", "INC AX", "X",
                                asm_p, "0", ".", "AA00:600"])
    ck.check("A: assembled", bytes.fromhex('B83412' '90' '43') + bytes(3), bytes(ref[USER + 0x500:USER + 0x508]))
    expect(ck, "A", bytes(mpm.out), b'0A00:0504 INC    BX')

    # I: the fcbs and the command tail (DDT86's base page)
    ref, mpm, _, _ = case("I", ["IB:FOO.TXT C:BAR.DAT", "D410:5C,AF", "I*.COM", "ITOOLONGNAME.TXT", "I",
                                "IA.B;C:D.E", "D410:5C,AF"])
    expect(ck, "I", bytes(mpm.out), b'0410:005C 02 46 4F 4F 20 20 20 20 20 54 58 54 00',
           b'0410:006C 03 42 41 52 20 20 20 20 20 44 41 54 00', b'13 42 3A 46 4F 4F 2E 54 58 54 20 43')
    base = pa(DSEG, 0)
    ck.check("I: the last fcbs", [b'\x00A       B  ', b'\x03D       E  '],
             [bytes(ref[base + 0x5C:base + 0x68]), bytes(ref[base + 0x6C:base + 0x78])])

    # R, V, W: read a file into user memory, write memory back
    data = bytes((i * 7) & 0xFF for i in range(300))
    files = {b'TEST    DAT': data, b'RO      DAT': (b'x', {'ro': True})}
    ref, mpm, port, pm = case("R, V, W", ["V", "RTEST.DAT", "V", "D", "L", "WOUT.DAT", "WOUT2.DAT,A00:0,A00:FF",
                                          "R", "R*.DAT", "RTEST.DAT X", "RNONE.DAT", "RA:TEST.DAT", "V"],
                              mpm_args={'files': files})
    expect(ck, "R, V, W", bytes(mpm.out), b'\r\n?', b'  START      END\r\n0A00:0000 0A00:017F',
           b'0A00:0000 00 07 0E 15', b'NO FILE')
    ck.check("R: read", data, bytes(ref[USER:USER + 300]))
    out = {k: b''.join(bytes(f.recs[i]) for i in sorted(f.recs)) for k, f in pm.files.items()}
    ck.check("W: the file read, written back", data + b'\x1a' * 84, out.get(b'OUT     DAT'))
    ck.check("W: a range", bytes(ref[USER:USER + 0x100]), out.get(b'OUT2    DAT'))
    case("R: no memory", ["RTEST.DAT", "V"], mpm_args={'files': files, 'mem_paras': 0})
    _, mpm, _, _ = case("R: not enough memory", ["RTEST.DAT", "V"], mpm_args={'files': files, 'mem_paras': 0x10})
    expect(ck, "R: not enough memory", bytes(mpm.out), b'INSUFFICIENT MEMORY')
    _, mpm, _, _ = case("W errors", ["W", "WX.DAT", "WX.DAT,A00:10,A00:0", "W*.DAT,A00:0,A00:1",
                                     "WRO.DAT,A00:0,A00:7F", "WX.DAT,A00:0", "WX.DAT,A00:0,A00:7F,1"],
                        mpm_args={'files': files})
    ck.check("W errors: each refused", 7, bytes(mpm.out).count(b'\r\n?'))
    _, mpm, _, _ = case("W: R/O, CP/M-86 BDOS", ["WRO.DAT,A00:0,A00:7F"],
                        mpm_args={'files': dict(files), 'version': 0x0022})
    _, mpm, _, _ = case("W: no directory space", ["WNEW.DAT,A00:0,A00:7F"], mpm_args={'make_fail': True})
    expect(ck, "W: no directory space", bytes(mpm.out), b'NO SPACE')
    _, mpm, _, _ = case("W: disk full", ["WNEW.DAT,A00:0,A00:FF"], mpm_args={'capacity': 1})
    expect(ck, "W: disk full", bytes(mpm.out), b'DISK WRITE ERROR')

    # E: load a program (function 59), its regions (V), its registers (X)
    prog80 = cmd_file([(1, bytes(0x100) + CODE, None)])
    prog86 = cmd_file([(1, CODE, None), (2, bytes(0x100) + TEXT, 0x30), (3, b'\xEE' * 16, None),
                       (4, bytes(0x40), None)])
    disk = {b'PROG80  CMD': prog80, b'PROG    CMD': prog86, b'DATA    BIN': data}
    ref, mpm, port, _ = case("E: 8080 model", ["EPROG80", "X", "V", "L", "IARG1 ARG2", "D", "XIP", "103",
                                               "L"], mpm_args={'files': disk})
    expect(ck, "E: 8080 model", bytes(mpm.out), b'     START      END\r\nCS 0A00:0000 0A00:010F',
           b'0A00 0A00 0410 0A00 0100 \r\nMOV    AX,1234', b'0A00:0103 MOV    BX,0100')
    ck.check("E: 8080 model: the tail in the program's base page", b'\x09ARG1 ARG2\x00',
             bytes(ref[USER + 0x80:USER + 0x8B]))
    ref, mpm, port, _ = case("E: 8086 model", ["EPROG.CMD", "X", "V", "L", "D", "DES:0,F", "IA.B C.D"],
                             mpm_args={'files': disk})
    expect(ck, "E: 8086 model", bytes(mpm.out), b'CS 0A00:0000 0A00:000F', b'DS 0A01:0000 0A01:02FF',
           b'ES 0A31:0000 0A31:000F', b'SS 0A32:0000 0A32:003F', b'0A00 0A01 0A32 0A31 0000 \r\nMOV    AX,1234')
    _, mpm, _, _ = case("E: from the command tail", ["X", "I"], tail=b' PROG80', mpm_args={'files': disk})
    expect(ck, "E: from the command tail", bytes(mpm.out), b'CS 0A00:0000')
    _, mpm, _, _ = case("E errors", ["E", "ENONE", "EP*", "EPROG X", "EDATA.BIN", "V"],
                        mpm_args={'files': disk, 'load_fail': True})
    expect(ck, "E errors", bytes(mpm.out), b'NO FILE', b'INSUFFICIENT MEMORY')

    print(f"\n{n} cases")
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
