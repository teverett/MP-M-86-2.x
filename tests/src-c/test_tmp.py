#!/usr/bin/env python3
"""Differential test for src-c/kernel/tmp.c (src/rsp/TMP.A86: one pass of
the command loop, `nextcommand:` back to `nextcommand:`) against the real
TMP running as an RSP in the real system, on real memory layout -- see
tests/src-c/supref.py.

TMP's code is at TMP_CS and its data segment (the real DSEG image) at
TMP_DS. Its `int mpmint` goes to SysRef.user_call for the two calls
tmp.c makes for real -- f_parsefilename and the CLI call -- which run the
real SUP func1 with u_wrkseg = TMP's DS (and, for the CLI call, the real
COMMAND.SUP over the real RTM's queues). Every other function (console
I/O, attach, disk/user/list get/set, free drive) is a no-op boundary on
both sides; conread's result is the command preset in TMP's read buffer.
Every case diffs the whole image and checks the same calls were made.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, PD_RUN, SYSDAT, UDA, build_lib, c_buffer, phys, sd, w16, r16, KERNEL_C, REPO_ROOT
from asm86 import Assembler  # noqa: E402

TMP_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'TMP.A86')
TMP_C = [os.path.join(KERNEL_C, f) for f in
         ('tmp.c', 'command.c', 'que1.c', 'que2.c', 'console.c', 'chario.c', 'findpd.c', 'parse.c', 'sysent.c',
          'proc.c', 'dsptch.c', 'load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c',
          'file1.c', 'file2.c', 'file3.c', 'file4.c')]
TMP_DS = 0x0B00
TMP_CS = 0x0C00
u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)


def main():
    ref = SysRef(('SUP', 'RTM'))
    v = ref.v
    asm = Assembler()
    asm.assemble(TMP_A86, [])
    tcode, tdata, tsym = asm.segment_bytes('cseg'), asm.segment_bytes('dseg'), asm.symtab
    t = lambda name: tsym[name].value  # noqa: E731
    td = lambda name: phys(TMP_DS, t(name))  # noqa: E731
    nextcommand = phys(TMP_CS, t('nextcommand'))
    first_call = tcode.index(0xE8, t('nextcommand'))     # past `mov dx,0ffffh`: call freedrive
    ref.skip |= set(range(phys(TMP_CS), phys(TMP_CS, len(tcode))))
    ref.skip |= set(range(td('stack_top') - 12, td('stack_top')))    # TMP's own stack
    lib_path = build_lib(TMP_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.nextcommand_once.argtypes = [c_u8p, u16, u16, u16, u16]
        ck = Checker(ref)

        def base_memory(cmdsent, text):
            mem = ref.base_memory()
            mem[phys(TMP_DS):phys(TMP_DS, len(tdata))] = tdata
            mem[phys(TMP_CS):phys(TMP_CS, len(tcode))] = tcode
            mem[sd(v('dayfile'))] = 0
            w16(mem, sd(v('pul')), 0)
            qd = v('mxcliqd')
            w16(mem, sd(v('qlr')), qd)
            w16(mem, sd(qd + v('q_link')), 0)
            w16(mem, sd(qd + v('q_msgcnt')), 1)
            mem[td('cmdsent')] = cmdsent
            mem[td('read_blen')] = len(text)
            mem[td('clicb_cmd'):td('clicb_cmd') + 128] = text + b' ' * (128 - len(text))
            mem[td('savebuf'):td('savebuf') + 128] = bytes(range(128))
            return mem

        def run(label, cmdsent, text):
            mem = base_memory(cmdsent, text)

            def int_hook(sim):
                if sim.regs['cx'] & 0xFF in (150, 152):
                    ref.user_call(sim)
                else:
                    ref.calls.append(('stub', sim.regs['cx'] & 0xFF))
            # one pass: from nextcommand (past its `mov dx,0ffffh`, which
            # only feeds freedrive) until it gets back there
            regs, img = ref.run(first_call, mem, {}, cs=TMP_CS, ds=TMP_DS, es=TMP_DS, ss=TMP_DS,
                                sp=t('stack_top'), int_hook=int_hook, stop_addrs={nextcommand})
            ref_calls = [c for c in ref.calls if c[0] == 'user']
            buf = c_buffer(mem)
            lib.nextcommand_once(buf, SYSDAT, TMP_DS, v('user_retf'), 0)
            ck.compare(label, img, buf, (), (),
                       snap=lambda i: (i[td('cmdsent')], bytes(i[td('savebuf'):td('savebuf') + 16])))
            return ref_calls

        cases = [
            ("empty command", 0, b''),
            ("empty command, cmdsent set -> cleared", 0xFF, b''),
            ("';' comment, ignored", 0, b';xx'),
            ("'A:' disk change", 0, b'A:'),
            ("'b:' disk change", 0, b'b:'),
            ("'~:' out of range", 0, b'~:'),
            ("2-char command, not a disk change", 0, b'ls'),
            ("explicit drive -> CLI call (no free PD)", 0, b'DIR A:'),
            ("no drive, not a builtin -> CLI call", 0xFF, b'TYPE X.Y'),
            ("USER, no argument", 0, b'USER'),
            ("USER 5", 0, b'USER 5'),
            ("PRINTER 12", 0, b'PRINTER 12'),
            ("bad parse -> clierror", 0, b'TOOLONGNAME.BAR X'),
            ("all blanks -> CLI call", 0xFF, b"   "),
        ]
        for label, cmdsent, text in cases:
            calls = run(label, cmdsent, text)
            print(f"    reference system calls: {calls}")
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
