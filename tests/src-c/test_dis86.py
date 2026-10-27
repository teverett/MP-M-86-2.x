#!/usr/bin/env python3
"""src-c/toolchain/dis86.c + ins86.c against src-assem/toolchain/DIS86.PLM
+ INS86.PLM, DDT86's disassembler overlay, as tools/build_ddt86.py
builds it into DDT86.CMD (see tests/src-c/toolref.py): disem(seg:off)
prints the instruction there and returns the offset past it.

The cases: every first opcode byte; for the ModRM opcodes every mod/rm
form (register, the eight memory forms with no, an 8-bit and a 16-bit
displacement, and the direct address), with every reg field where it
picks the instruction or is printed (the alt-table groups 80-83, D0-D3,
F6/F7, FE/FF, the segment-register MOVs 8C/8E and ESC D8-DF: all 256
ModRM bytes); prefixes (segment overrides, LOCK, REP/REPNE), alone and
in chains of two to six (disem takes up to three, a fourth is printed as
an instruction); the undefined opcodes ("??="); and instructions
running off the end of their segment (at 0:FFFC-FFFF, the offset
wrapping as the 8086's does).

For speed the cases run in batches: each batch's instructions are laid
out in one memory, each side runs them one after another on its own
copy of it, and each case's services, output, result and the overlay's
data (its variables) are compared; the whole memory at the end of the
batch. Separately: dis86_install, from memory with the overlay's data
cleared, puts back exactly the asm build's bytes; and a set of known
instructions print the expected text, and every case's output is real
disassembly (a mnemonic or "??=" first), so the two can't agree by both
failing the same way.
"""
import os
import sys
import tempfile

import toolref
from toolref import TSEG, USEG, pa
from rspref import Checker

DIS_LO, DIS_HI = 0x25CD, 0x2A6E    # TC_DIS_CODE_END, TC_DIS_END: the overlay's data (INS86's, DIS86's)
INSTR_TABLE = 0x265D               # INS86_INSTRTABLE
MODRM_TYPES = {7, 12, 17, 20, 21, 22, 23, 24, 25, 26, 27, 28}   # dis86.c's cases that read a ModRM byte
PREFIXES = [0x26, 0x2E, 0x36, 0x3E, 0xF0, 0xF2, 0xF3]
UNDEFINED = {0x0F, *range(0x60, 0x70), 0xC0, 0xC1, 0xC8, 0xC9, 0xD6, 0xF1}
BATCH = 128
SLOT = 16                          # each case's room in a batch's memory

GOLDEN = [  # (bytes, text, length): checked against both sides
    ('b83412', 'MOV    AX,1234', 3),
    ('0000', 'ADD    [BX+SI],AL', 2),
    ('03472a', 'ADD    AX,2A[BX]', 3),
    ('8b8f3412', 'MOV    CX,1234[BX]', 4),
    ('8b4600', 'MOV    AX,[BP]', 3),
    ('c6060001ff', 'MOV    BYTE [0100],FF', 5),
    ('8346fe80', 'ADD    WORD FE[BP],FF80', 4),
    ('817f023412', 'CMP    WORD 02[BX],1234', 5),
    ('d3e0', 'SHL    AX,CL', 2),
    ('d1260001', 'SHL    WORD [0100],1', 4),
    ('f6c180', 'TEST   CL,80', 3),
    ('fe08', 'DEC    BYTE [BX+SI]', 2),
    ('ff1e3412', 'CALLF  [1234]', 4),
    ('8ed0', 'MOV    SS,AX', 2),
    ('8c38', '??=    8C', 1),
    ('e8fdff', 'CALL   0100', 3),
    ('7480', 'JZ     0082', 2),
    ('9a00103412', 'CALLF  1234:1000', 5),
    ('cc', 'INT    3', 1),
    ('d40a', 'AAM    ', 2),
    ('d40b', '??=    D4', 1),
    ('dfc1', 'ESC    38,CX', 2),
    ('26f30fa4', 'ES:    REP    ??=    0F', 3),
    ('f2ae', 'REPNE  SCASB  ', 2),
    ('f0262e36a5', 'LOCK   ES:    CS:    SS:    ', 4),
    ('e660', 'OUT    60,AL', 2),
    ('ec', 'IN     AL,DX', 1),
]


def tail(k, n=6):
    """the bytes after an opcode (or ModRM byte) for case k"""
    return bytes((0x5A + 0x3B * k + 0x47 * i) & 0xFF for i in range(n))


def cases():
    """[(label, bytes, sweep)] at USEG (sweep: the first-byte sweep's), checking the opcode/ModRM coverage"""
    body = toolref.image()[0]
    out, k = [], 0
    modrm_ops = [op for op in range(256) if body[INSTR_TABLE + 2 * op + 1] in MODRM_TYPES]
    assert len(modrm_ops) == 68, len(modrm_ops)
    full = {*range(0x80, 0x84), *range(0xD0, 0xD4), 0xF6, 0xF7, 0xFE, 0xFF, 0x8C, 0x8E, *range(0xD8, 0xE0)}
    seen = set()
    for op in range(256):
        if op in modrm_ops:
            forms = range(256) if op in full else [(mod << 6) | (((rm * 3 + mod) & 7) << 3) | rm
                                                    for mod in range(4) for rm in range(8)]
            for m in forms:
                out.append((f"{op:02X} {m:02X}", bytes([op, m]) + tail(k), True))
                seen.add((op, m >> 6, m & 7))
                k += 1
            for m in (0x46, 0x47, 0x40):          # disp8 0: [BP] prints none, the others 00
                out.append((f"{op:02X} {m:02X} 00", bytes([op, m, 0]) + tail(k, 4), True))
                k += 1
        else:
            for t in (tail(k), bytes([0x0A, 0x80, 0xFF, 0x7F, 0x00, 0x01]), bytes([0x80, 0x00, 0xFF, 0xFF, 0, 0])):
                out.append((f"{op:02X} {t[:2].hex()}", bytes([op]) + t, True))
                k += 1
    for op in modrm_ops:
        assert all((op, mod, rm) in seen for mod in range(4) for rm in range(8)), op
    follow = [bytes.fromhex(h) for h in ('8b4712', 'a4', 'a5', 'ae', '90', '0f', 'ff38', 'c6060001ff', '8346fe80')]
    for p in PREFIXES:
        for f in follow:
            out.append((f"{p:02X} {f.hex()}", bytes([p]) + f, False))
    for p in PREFIXES:
        for q in PREFIXES:
            out.append((f"{p:02X}{q:02X} a5", bytes([p, q, 0xA5]), False))
            for r in PREFIXES:
                out.append((f"{p:02X}{q:02X}{r:02X} 8b8f3412", bytes([p, q, r, 0x8B, 0x8F, 0x34, 0x12]), False))
    for n in (4, 5, 6):
        for i in range(len(PREFIXES)):
            chain = bytes(PREFIXES[(i + j) % len(PREFIXES)] for j in range(n))
            out.append((f"{chain.hex()} a5", chain + b'\xa5', False))
    return out


def straddle_cases():
    """[(label, bytes, off)] at 0:off, running past 0:FFFF"""
    out = []
    for h in ('b83412', '9a00103412', '817f023412', 'c6060001ff', 'e8fdff', '7480', '26f3a5', 'f0262e8b8f3412',
              '8b8f3412', 'd40a', 'ff1e3412', 'cd21', '0f'):
        b = bytes.fromhex(h)
        for off in range(0x10000 - min(len(b), 4), 0x10000):
            out.append((f"0:{off:04X} {h}", b, off))
    return out


def lib():
    return toolref.CLib(['ins86.c', 'dis86.c'])


def run_batch(ck, name, c_lib, batch):
    """batch: [(label, bytes, off, seg)] -> [(label, ref text, ref result)]"""
    mem = toolref.memory()
    for _, b, off, seg in batch:
        for i, x in enumerate(b):
            mem[pa(seg, (off + i) & 0xFFFF)] = x
    ref, port = bytearray(mem), bytearray(mem)
    bad, outs = [], []
    lo, hi = pa(TSEG, DIS_LO), pa(TSEG, DIS_HI)
    for label, _, off, seg in batch:
        rd, pd = toolref.Ddt(), toolref.Ddt()
        rres, ref = toolref.asm_run('pl_disem', ref, rd, off, seg)
        pres, port = c_lib.run('tc_disem_main', port, pd, off, seg)
        if (rd.log, rd.out, rres) != (pd.log, pd.out, pres) or ref[lo:hi] != port[lo:hi]:
            bad.append((label, bytes(rd.out), hex(rres), bytes(pd.out), hex(pres)))
        outs.append((label, bytes(rd.out).decode('latin-1'), rres))
    ck.check(f"{name}: {len(batch)} cases agree", [], bad[:4])
    stack = range(pa(TSEG, toolref.SP - 0x400), pa(TSEG, toolref.SP))
    diffs = [] if ref == port else [hex(a) for a in range(len(ref)) if a not in stack and ref[a] != port[a]]
    ck.check(f"{name}: no other bytes differ", [], diffs[:8])
    return outs


def mnemonics():
    body = toolref.image()[0]
    names = set()
    for size, (a, b) in zip(range(2, 7), ((0x285D, 0x2875), (0x2875, 0x2944), (0x2944, 0x29A8), (0x29A8, 0x29F8),
                                          (0x29F8, 0x2A04))):
        names |= {body[i:i + size].decode().ljust(7) for i in range(a, b, size)}
    return names


def main():
    ck = Checker()
    c_lib = lib()

    # dis86_install puts the overlay's data back
    body = toolref.image()[0]
    mem = toolref.memory()
    mem[pa(TSEG, DIS_LO):pa(TSEG, DIS_HI)] = bytes(DIS_HI - DIS_LO)
    _, mem = c_lib.run('dis86_install', mem, toolref.Ddt(), 0)
    ck.check("dis86_install: the asm build's data", body[DIS_LO:DIS_HI].hex(),
             bytes(mem[pa(TSEG, DIS_LO):pa(TSEG, DIS_HI)]).hex())
    ck.check("dis86_install: the data is the tables", True, b'LOOPNELOOPNZ' in body[DIS_LO:DIS_HI]
             and body[INSTR_TABLE + 2 * 0xB8 + 1] == 18)

    # known instructions, on both sides
    gold = [(h, bytes.fromhex(h), 0x100, USEG + 0x20 * i) for i, (h, _, _) in enumerate(GOLDEN)]  # each at 100h
    outs = run_batch(ck, "golden", c_lib, gold)
    ck.check("golden: the expected text", [(t, 0x100 + n) for _, t, n in GOLDEN],
             [(t, r) for _, t, r in outs])

    all_outs, sweep = [], {}
    cs = cases()
    ck.check("every first opcode byte covered", 256, len({b[0] for _, b, _ in cs}))
    for n in range(0, len(cs), BATCH):
        part = cs[n:n + BATCH]
        batch = [(label, b, 0x100 + SLOT * i, USEG) for i, (label, b, _) in enumerate(part)]
        outs = run_batch(ck, f"batch {n // BATCH} ({batch[0][0]} ..)", c_lib, batch)
        all_outs += outs
        for (_, b, sw), (_, t, _) in zip(part, outs):
            if sw:
                sweep.setdefault(b[0], []).append(t == f"??=    {b[0]:02X}")
    st = straddle_cases()
    for i, case in enumerate(st):  # one a memory: they share 0:FFFC-FFFF
        outs = run_batch(ck, f"segment end {i}: {case[0]}", c_lib, [case + (0,)])
        all_outs += outs
        ck.check(f"segment end {i}: the offset wraps", True, outs[0][2] < 0x10)

    # real disassembly: a mnemonic (or a prefix's) or "??=" first; the undefined opcodes say so
    names = mnemonics()
    junk = [(lb, t) for lb, t, _ in all_outs if not (t[:7] in names or t.startswith('??=    '))]
    ck.check("every output is a mnemonic or ??=", [], junk[:4])
    ck.check("the undefined opcodes print ??=", sorted(UNDEFINED), sorted(op for op, v in sweep.items() if all(v)))
    print(f"\n{len(gold) + len(cs) + len(st)} cases")
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
