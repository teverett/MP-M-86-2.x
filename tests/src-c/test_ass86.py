#!/usr/bin/env python3
"""src-c/toolchain/ass86.c and asmtab.c against src-assem/toolchain/
ASS86.PLM and ASMTAB.PLM, DDT86's line assembler (its A command), as
tools/build_ddt86.py builds them into DDT86.CMD (see
tests/src-c/toolref.py): asm(seg:off) reads a console line, assembles
it at seg:off through ddtset and returns the offset past it; an error
prints "?" or "ambiguous operand" and goes back to DDT86 (goddt).

Each case is one line, assembled at USEG:off by the asm overlay in
i86sim and by the C on the host, from the same memory; toolref.compare
checks they made the same DDT86 calls, printed the same, returned the
same and left the same memory. Each case also says what the asm must
do -- the bytes it stores (so an instruction's encoding is checked, not
only that both sides agree), "?" (an error), "ambig" (the ambiguous
operand error) or "stop" (ddtset at 0FFFFh ends the A command) -- so a
case can't pass with both sides failing alike; and every mnemonic in
op$tab must be assembled by at least one case.

    python3 tests/src-c/test_ass86.py [-v]
"""
import os
import re
import sys

import toolref
from toolref import TSEG, USEG, pa
from rspref import Checker

VERBOSE = '-v' in sys.argv
SRC_C = os.path.join(toolref.REPO_ROOT, 'src-c', 'toolchain')


def syms():
    """toolchain_syms.h's constants"""
    text = open(os.path.join(SRC_C, 'toolchain_syms.h')).read()
    return {k: int(v, 16) for k, v in re.findall(r'#define (\w+) 0x([0-9A-F]+)', text)}


S = syms()
OFF = 0x200

# (line, expect[, off[, seg]]): expect is the bytes stored (hex), '?', 'ambig' or 'stop'. An
# offset past 0EFFFh is in segment 0 (toolref's memory is 64K: USEG:0F000h is past it; 0:0FFF8h is
# above the stack and the host's argument block)
CASES = [
    # type 1: no operands
    ('aaa', '37'), ('aas', '3f'), ('cbw', '98'), ('clc', 'f8'), ('cld', 'fc'), ('cli', 'fa'), ('cmc', 'f5'),
    ('cmpsb', 'a6'), ('cmpsw', 'a7'), ('cwd', '99'), ('daa', '27'), ('das', '2f'), ('hlt', 'f4'),
    ('into', 'ce'), ('iret', 'cf'), ('lahf', '9f'), ('lodsb', 'ac'), ('lodsw', 'ad'), ('movsb', 'a4'),
    ('movsw', 'a5'), ('nop', '90'), ('popf', '9d'), ('pushf', '9c'), ('sahf', '9e'), ('scasb', 'ae'),
    ('scasw', 'af'), ('stc', 'f9'), ('std', 'fd'), ('sti', 'fb'), ('stosb', 'aa'), ('stosw', 'ab'),
    ('wait', '9b'), ('xlat', 'd7'), ('nop ax', '?'), ('nop nop', '?'), ('   nop', '90'), ('NOP', '90'),
    # type 2: aad aam
    ('aad', 'd50a'), ('aam', 'd40a'), ('aad 5', '?'),
    # type 3: short jumps, at 200h: 210h is +0Eh, the range 182h-281h
    ('ja 210', '770e'), ('jae 210', '730e'), ('jb 210', '720e'), ('jbe 210', '760e'), ('jc 210', '720e'),
    ('jcxz 210', 'e30e'), ('je 210', '740e'), ('jg 210', '7f0e'), ('jge 210', '7d0e'), ('jl 210', '7c0e'),
    ('jle 210', '7e0e'), ('jmps 210', 'eb0e'), ('jna 210', '760e'), ('jnae 210', '720e'), ('jnb 210', '730e'),
    ('jnbe 210', '770e'), ('jnc 210', '730e'), ('jne 210', '750e'), ('jng 210', '7e0e'), ('jnge 210', '7c0e'),
    ('jnl 210', '7d0e'), ('jnle 210', '7f0e'), ('jno 210', '710e'), ('jnp 210', '7b0e'), ('jns 210', '790e'),
    ('jnz 210', '750e'), ('jo 210', '700e'), ('jp 210', '7a0e'), ('jpe 210', '7a0e'), ('jpo 210', '7b0e'),
    ('js 210', '780e'), ('jz 210', '740e'), ('loop 1f0', 'e2ee'), ('loope 202', 'e100'), ('loopne 210', 'e00e'),
    ('loopnz 210', 'e00e'), ('loopz 210', 'e10e'),
    ('jz 182', '7480'), ('jz 281', '747f'), ('jz 181', '?'), ('jz 282', '?'), ('jz bx', '?'), ('jz', '?'),
    ('jmps 10', 'eb16', 0xFFF8, 0), ('jz ff90', '748e', 0x0000),
    # type 4: shifts and rotates
    ('rol ax,1', 'd1c0'), ('ror cl,1', 'd0c9'), ('rcl dx,cl', 'd3d2'), ('rcr byte [si],cl', 'd21c'),
    ('sal bl,1', 'd0e3'), ('shl bl,cl', 'd2e3'), ('shr word [1234],1', 'd12e3412'), ('sar word 4[bx],1', 'd17f04'),
    ('rol [si],1', 'ambig'), ('ror ax,2', '?'), ('rol ax,dl', '?'), ('sal 5,1', '?'), ('shl ax', '?'),
    # type 5: one operand, modrm
    ('mul bx', 'f7e3'), ('imul cl', 'f6e9'), ('div word [si]', 'f734'), ('idiv byte 1234[bp]', 'f6be3412'),
    ('not dx', 'f7d2'), ('neg byte [bx+di]', 'f619'), ('neg [bx]', 'ambig'), ('mul 5', '?'), ('div', '?'),
    # type 6: les lds lea
    ('lea bx,[si]', '8d1c'), ('lds ax,4[bp+di]', 'c54304'), ('les di,[1234]', 'c43e3412'), ('lea ax,bx', '?'),
    ('lea al,[si]', '?'), ('lea bx,byte [si]', '?'), ('lds es,[si]', '?'),
    # type 7: inc dec
    ('inc ax', '40'), ('dec di', '4f'), ('inc cl', 'fec1'), ('dec byte [bx]', 'fe0f'), ('inc word 5[di]', 'ff4505'),
    ('dec [bx]', 'ambig'), ('inc 5', '?'), ('inc es', '?'),
    # type 8, 8a: push pop
    ('push ax', '50'), ('push es', '06'), ('push cs', '0e'), ('push ss', '16'), ('push ds', '1e'), ('push [bx]', 'ff37'),
    ('push word 4[si]', 'ff7404'), ('push al', '?'), ('push 5', '?'),
    ('pop ax', '58'), ('pop si', '5e'), ('pop es', '07'), ('pop ds', '1f'), ('pop ss', '17'), ('pop cs', '?'),
    ('pop [1234]', '8f063412'), ('pop bl', '?'),
    # type 9: add adc sbb sub cmp
    ('add al,5', '0405'), ('add ax,1234', '053412'), ('add ax,5', '050500'), ('sub ax,ff80', '2d80ff'),
    ('adc bl,5', '82d305'), ('sub bx,5', '83eb05'), ('cmp bx,1234', '81fb3412'), ('sbb bx,ff80', '83db80'),
    ('add byte [bx],5', '820705'), ('add [bx],5', 'ambig'), ('add [bx],1234', '81073412'), ('add word [si],1', '830401'),
    ('add bl,1234', '?'), ('add ax,bx', '03c3'), ('add [si],ax', '0104'), ('cmp ax,[si]', '3b04'), ('adc al,[si]', '1204'),
    ('sbb cl,dl', '1aca'), ('add [si],[di]', '?'), ('add al,bx', '?'), ('add ax,12345', '054523'), ('add 5,ax', '?'),
    # type 10: and or xor
    ('and al,f', '240f'), ('or ax,1234', '0d3412'), ('xor bl,5', '80f305'), ('and bx,5', '81e30500'),
    ('or byte [bx],80', '800f80'), ('xor [bx],5', 'ambig'), ('and cx,dx', '23ca'), ('or [di],bl', '081d'),
    ('xor dx,[bp]', '335600'), ('and al,100', '?'),
    # type 11: call jmp
    ('call 1234', 'e83110'), ('jmp 200', 'e9fdff'), ('call bx', 'ffd3'), ('call [bx]', 'ff17'),
    ('jmp word 4[si]', 'ff6404'), ('jmp al', '?'), ('jmp 1234:5', '?'), ('call 10', 'e81500', 0xFFF8, 0),
    # 17: callf jmpf
    ('callf 1234:5678', '9a78563412'), ('jmpf 0:100', 'ea00010000'), ('callf [bx]', 'ff1f'), ('jmpf 4[di]', 'ff6d04'),
    ('jmpf es:[bx]', '26ff2f'), ('callf 1234', '?'), ('jmpf 1234:bx', '?'), ('callf al', '?'),
    # 12: int
    ('int 3', 'cc'), ('int 21', 'cd21'), ('int ff', 'cdff'), ('int 100', '?'), ('int 1ff', '?'), ('int ax', '?'),
    # 13: esc
    ('esc 5,[bx]', 'd82f'), ('esc 3f,4[si]', 'df7c04'), ('esc 1,ax', 'd8c8'), ('esc 9,cl', 'd9c9'), ('esc 40,[bx]', '?'),
    ('esc ax,[bx]', '?'), ('esc 5,6', '?'),
    # 14: ret retf
    ('ret', 'c3'), ('retf', 'cb'), ('ret 4', 'c20400'), ('retf 1234', 'ca3412'), ('ret ax', '?'),
    # 15, 16: in out (IN AX,port was E4 -- ASS86 left its W bit out --
    # until bugs.md #137)
    ('in al,dx', 'ec'), ('in ax,dx', 'ed'), ('in al,40', 'e440'), ('in ax,40', 'e540'), ('in bl,dx', '?'),
    ('in al,100', '?'), ('in dx,al', '?'),
    ('out dx,al', 'ee'), ('out dx,ax', 'ef'), ('out 40,al', 'e640'), ('out 40,ax', 'e740'), ('out dx,bl', '?'),
    ('out 100,al', '?'), ('out bx,al', '?'),
    # 18: test
    ('test al,5', 'a805'), ('test ax,1234', 'a93412'), ('test bl,5', 'f6c305'), ('test word [bx],5', 'f7070500'),
    ('test [bx],5', 'ambig'), ('test ax,bx', '85c3'), ('test [si],al', '8404'), ('test al,[si]', '8404'),
    ('test al,300', '?'), ('test 5,al', '?'),
    # 19: xchg
    ('xchg ax,bx', '93'), ('xchg ax,ax', '90'), ('xchg al,bl', '86c3'), ('xchg [si],ax', '8704'), ('xchg bx,ax', '87d8'),
    ('xchg al,ax', '?'), ('xchg ax,5', '?'),
    # 20: mov
    ('mov ds,ax', '8ed8'), ('mov es,[bx]', '8e07'), ('mov cs,ax', '8ec8'), ('mov ds,al', '?'), ('mov ax,ds', '8cd8'),
    ('mov [bx],es', '8c07'), ('mov al,es', '?'), ('mov al,[1234]', 'a03412'), ('mov ax,[1234]', 'a13412'),
    ('mov [1234],al', 'a23412'), ('mov [1234],ax', 'a33412'), ('mov al,5', 'b005'), ('mov bx,1234', 'bb3412'),
    ('mov al,1234', 'b034'), ('mov byte [bx],5', 'c60705'), ('mov [bx],5', 'ambig'),
    ('mov word 4[bp],1234', 'c746043412'), ('mov ax,bx', '8bc3'), ('mov [si],cl', '880c'), ('mov cl,[si]', '8a0c'),
    ('mov [bx],[si]', '?'), ('mov bx,4[si]', '8b5c04'), ('mov bx,1234[bx+si]', '8b983412'),
    ('mov dl,80[bp+di]', '8a938000'), ('mov dl,7f[bp+di]', '8a537f'), ('mov ax,[bp]', '8b4600'),
    ('mov ax,4[bp]', '8b4604'), ('mov ax,[di]', '8b05'), ('mov ax,[bp+si]', '8b02'), ('mov ax,[bx+di]', '8b01'),
    ('mov ax,word [si]', '8b04'), ('mov al,word [si]', '?'), ('mov byte al,5', '?'), ('mov word [bx],word 5', 'c7070500'), ('mov dx,ffff[si]', '8b94ffff'),
    ('MOV\tAX,\t5', 'b80500'), ('mov ax,5:6', '?'), ('mov ,ax', '?'), ('mov ax,bx,cx', '?'),
    # segment overrides
    ('mov ax,es:[bx]', '268b07'), ('mov cs:4[si],al', '2e884404'), ('ss: mov ax,[bp]', '368b4600'), ('ds:nop', '3e90'),
    ('es: mov ax,ds:[bx]', '?'), ('mov ax,ss:bx', '368bc3'), ('cs:', '2e'), ('mov es:ax,ds:bx', '?'),
    # LOCK, REP
    ('lock xchg ax,[bx]', 'f08707'), ('rep movsb', 'f3a4'), ('repe cmpsb', 'f3a6'), ('repz cmpsw', 'f3a7'),
    ('repne scasb', 'f2ae'), ('repnz scasw', 'f2af'), ('lock', 'f0'), ('rep', 'f3'), ('lock rep movsw', 'f0f3a5'),
    ('rep es: movsb', 'f326a4'), ('lock es: inc byte [bx]', 'f026fe07'), ('lock lock nop', '?'),
    ('rep repne movsb', '?'),
    # BYTE / WORD
    ('inc byte word [bx]', '?'), ('inc byte byte [bx]', '?'), ('byte', '?'), ('inc byte foo', '?'),
    ('inc word es:[bx]', '26ff07'),
    # errors
    ('foo', '?'), ('x', '?'), ('movsbxx', '?'), ('mov ax', '?'), ('mov ax,', '?'), ('mov ax,[si+4]', '?'),
    ('mov ax,5[bx+si', '?'), ('mov ax,4[si]]', '?'), ('mov ax,[bx+bp]', '?'), ('mov ax,[si+di]', '?'),
    ('cs', '?'), ('a' * 40, '?'), ('mov ax,' + '1' * 40, '?'), (',', '?'), ('mov ax,[]', '?'),
    # no opcode: the A command's end
    ('', ''), ('   ', ''), ('.', ''), ('.nop', ''),
    # the offset wrapping
    ('nop', 'stop', 0xFFFF, 0), ('mov ax,1234', 'stop', 0xFFFE, 0), ('nop', '90', 0xFFFE, 0),
]

ERR = b'\r\n?'
AMBIG = b'\r\nambiguous operand'


def op_tab_names(body):
    """op$tab's mnemonics, by DIS86's tables (tab$ptrs, nops, opn$in)"""
    names = {}
    for n in range(5):
        base = int.from_bytes(body[S['DIS86_TABPTRS'] + 2 * n:S['DIS86_TABPTRS'] + 2 * n + 2], 'little')
        for i in range(body[S['DIS86_NOPS'] + n]):
            names[body[S['DIS86_OPNIN'] + n] + i] = body[base + (n + 2) * i:base + (n + 2) * (i + 1)].decode()
    opt = S['ASMTAB_OPTAB']
    return {names[body[opt + i]] for i in range(0, 480, 4)}


def lib():
    srcs = ['dis86.c', 'ins86.c', 'asmtab.c', 'ass86.c']
    missing = [s for s in srcs[:2] if not os.path.exists(os.path.join(SRC_C, s))]
    assert not missing, f"needs {missing}"
    return toolref.CLib(srcs)


def main():
    ck = Checker()
    c_lib = lib()
    body = toolref.image()[0]
    lo, hi = S['ASMTAB_OPTAB'], S['TC_ASS_END']

    # ass86_install puts op$tab and ASS86's data back
    mem = toolref.memory()
    mem[pa(TSEG, lo):pa(TSEG, hi)] = bytes(hi - lo)
    _, mem = c_lib.run('ass86_install', mem, toolref.Ddt(), 0)
    ck.check("ass86_install: the asm build's data", body[lo:hi].hex(), bytes(mem[pa(TSEG, lo):pa(TSEG, hi)]).hex())
    ck.check("ass86_install: the data is the tables", True,
             b'BXBPSIDI' in body[lo:hi] and b'ambiguous operand$' in body[lo:hi] and body[lo + 2] == 0x37)

    mem = toolref.memory()
    done, bad = set(), []
    for case in CASES:
        line, expect = case[0], case[1]
        off = case[2] if len(case) > 2 else OFF
        seg = case[3] if len(case) > 3 else USEG
        label = f"{line!r} at {seg:04X}:{off:04X}"
        ok = expect not in ('?', 'ambig', 'stop')
        d, _, res = toolref.compare(ck, label, mem, 'pl_asm', c_lib, 'tc_asm_main', lines=[line.encode()], off=off,
                                    seg=seg, result=ok)
        stored = bytes(b for s, *a in d.log if s == 'set' for b in a[1:])
        head = b'\r\n%04X:%04X ' % (seg, off)
        if expect == '?':
            got = (d.out == head + ERR and d.log[-1] == ('asment',))
        elif expect == 'ambig':
            got = (d.out == head + AMBIG and d.log[-1] == ('asment',))
        elif expect == 'stop':
            got = d.log[-1][:2] == ('set', 0xFFFF)
        else:
            want = bytes.fromhex(expect)
            got = (d.out == head and stored == want and res == (off + len(want)) & 0xFFFF
                   and [a for s, a, *_ in (x for x in d.log if x[0] == 'set')] ==
                   [(off + i) & 0xFFFF for i in range(len(want))])
            if got and line.split():
                done.add(line.split()[0].upper())
        if not got:
            bad.append((line, expect, bytes(d.out), stored.hex(), hex(res)))
        if VERBOSE:
            print(f"    {label}: {bytes(d.out)!r} {stored.hex()} -> {res:04X}")
    ck.check("the asm does what each case expects", [], bad)
    names = op_tab_names(body)
    ck.check("op$tab has 120 mnemonics", 120, len(names))
    ck.check("every op$tab mnemonic assembled", [], sorted(names - done))

    # the segment DDT86 prints (and stores into) is the far pointer's
    d, _, _ = toolref.compare(ck, "'nop' at 0200:0300", mem, 'pl_asm', c_lib, 'tc_asm_main', lines=[b'nop'],
                              off=0x300, seg=0x0200)
    ck.check("'nop' at 0200:0300: prints the segment", b'\r\n0200:0300 ', bytes(d.out))
    print(f"\n{len(CASES) + 1} cases")
    return ck.finish()


if __name__ == '__main__':
    sys.exit(main())
