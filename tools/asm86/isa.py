"""8086 instruction encoder for the ASM86-dialect subset used in this tree.

Design notes (see tools/README.md for the full rationale):
  - Displacements are always encoded as disp16 when present (never the
    disp8 ModRM form), so instruction length never depends on a forward
    reference's resolved value -- pass 1 and pass 2 compute identical
    lengths.
  - JMP/CALL default to the near full-size encoding (E9/E8); only the
    explicit 'jmps' mnemonic uses the 2-byte short form (EB).
  - JMPS/Jcc/LOOP*/JCXZ (SHORT_JUMP_MNEMONICS below) *do* get real
    jump-size relaxation: assembler.py's Assembler._relax_jumps decides,
    per occurrence, whether the target fits the hardware 2-byte short
    form or needs growing, and tells these encoders which via
    EncodeCtx.jump_long. JMPS grows to a plain 3-byte near JMP; the
    conditional forms grow to an 8-byte trampoline (see _encode_jcc)
    since they can't just take a 16-bit displacement directly.
  - This assembler is not intended to produce byte-identical output to
    the original DRI ASM86 -- only functionally equivalent code.
"""
import struct
from .errors import AsmError
from .lex import REG16, REG8, SEGREG, MEM_COMBOS

SEG_PREFIX = {'es': 0x26, 'cs': 0x2E, 'ss': 0x36, 'ds': 0x3E}

BRANCH_MNEMONICS = {
    'jmp', 'jmps', 'jmpf', 'call', 'callf',
    'jz', 'je', 'jnz', 'jne', 'jb', 'jc', 'jnae', 'jnb', 'jae', 'jnc',
    'ja', 'jnbe', 'jbe', 'jna', 'jg', 'jnle', 'jge', 'jnl', 'jl', 'jnge',
    'jle', 'jng', 'jo', 'jno', 'js', 'jns', 'jp', 'jpe', 'jnp', 'jpo',
    'loop', 'loope', 'loopz', 'loopne', 'loopnz', 'jcxz',
    'ret', 'retf',
}

JCC = {
    'jz': 0x74, 'je': 0x74, 'jnz': 0x75, 'jne': 0x75,
    'jb': 0x72, 'jc': 0x72, 'jnae': 0x72, 'jnb': 0x73, 'jae': 0x73, 'jnc': 0x73,
    'ja': 0x77, 'jnbe': 0x77, 'jbe': 0x76, 'jna': 0x76,
    'jg': 0x7F, 'jnle': 0x7F, 'jge': 0x7D, 'jnl': 0x7D,
    'jl': 0x7C, 'jnge': 0x7C, 'jle': 0x7E, 'jng': 0x7E,
    'jo': 0x70, 'jno': 0x71, 'js': 0x78, 'jns': 0x79,
    'jp': 0x7A, 'jpe': 0x7A, 'jnp': 0x7B, 'jpo': 0x7B,
}
LOOP_OPS = {'loop': 0xE2, 'loope': 0xE1, 'loopz': 0xE1, 'loopne': 0xE0, 'loopnz': 0xE0}

# Mnemonics with a hardware-fixed 2-byte short (rel8) form: JMPS and
# every Jcc/LOOP*/JCXZ. assembler.py's relaxation pass (see
# Assembler._relax_jumps) decides, per occurrence, whether the target is
# in range for that 2-byte form or needs growing -- see EncodeCtx.jump_long
# and the encoders below.
SHORT_JUMP_MNEMONICS = set(JCC) | set(LOOP_OPS) | {'jcxz', 'jmps'}

ALU = {'add': 0, 'or': 1, 'adc': 2, 'sbb': 3, 'and': 4, 'sub': 5, 'xor': 6, 'cmp': 7}

F6_GROUP = {'not': 2, 'neg': 3, 'mul': 4, 'imul': 5, 'div': 6, 'idiv': 7}

SHIFT_GROUP = {'rol': 0, 'ror': 1, 'rcl': 2, 'rcr': 3, 'shl': 4, 'sal': 4, 'shr': 5, 'sar': 7}

NO_OPERAND = {
    'ret': 0xC3, 'retf': 0xCB, 'iret': 0xCF, 'nop': 0x90, 'hlt': 0xF4,
    'clc': 0xF8, 'cld': 0xFC, 'cli': 0xFA, 'stc': 0xF9, 'std': 0xFD, 'sti': 0xFB,
    'cmc': 0xF5, 'daa': 0x27, 'das': 0x2F, 'aaa': 0x37, 'aas': 0x3F,
    'cbw': 0x98, 'cwd': 0x99, 'pushf': 0x9C, 'popf': 0x9D,
    'sahf': 0x9E, 'lahf': 0x9F, 'xlat': 0xD7, 'wait': 0x9B,
    'movsb': ('movs', 1), 'movsw': ('movs', 2),
    'lodsb': ('lods', 1), 'lodsw': ('lods', 2),
    'stosb': ('stos', 1), 'stosw': ('stos', 2),
    'cmpsb': ('cmps', 1), 'cmpsw': ('cmps', 2),
    'scasb': ('scas', 1), 'scasw': ('scas', 2),
}

STRING_OPS = {'movs': 0xA4, 'lods': 0xAC, 'stos': 0xAA, 'cmps': 0xA6, 'scas': 0xAE}

REP_PREFIXES = {'rep': 0xF3, 'repe': 0xF3, 'repz': 0xF3, 'repne': 0xF2, 'repnz': 0xF2}


def modrm(mod, reg, rm):
    return ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7)


def u8(n):
    return struct.pack('<B', n & 0xFF)


def u16(n):
    return struct.pack('<H', n & 0xFFFF)


def encode_rm(op, reg_field, ctx):
    """Return bytes for ModRM + disp for a reg/mem operand -- NOT
    including any segment-override prefix (see `encode`'s own comment:
    that has to be the first byte of the whole instruction, before the
    opcode this ModRM+disp gets appended after, so it's computed and
    prepended once, centrally, in `encode` instead)."""
    if op.kind in ('reg', 'segreg'):
        return bytes([modrm(3, reg_field, op.reg)])
    if op.kind == 'mem':
        mem = op.mem
        base = mem['base']
        disp = mem.get('disp')
        if base is None:
            mb = modrm(0, reg_field, 6)
            d = u16(disp if disp is not None else 0)
        else:
            rm = MEM_COMBOS[base]
            if base == frozenset(['bp']):
                mb = modrm(2, reg_field, rm)
                d = u16(disp if disp is not None else 0)
            elif disp is None:
                mb = modrm(0, reg_field, rm)
                d = b''
            else:
                mb = modrm(2, reg_field, rm)
                d = u16(disp)
        return bytes([mb]) + d
    ctx.err("expected register or memory operand")


def _opsize(*ops):
    for o in ops:
        if o is not None and o.size is not None:
            return o.size
    return None


def _need_size(mnemonic, size, ctx):
    if size is None:
        ctx.err(f"ambiguous operand size for '{mnemonic}' -- use BYTE PTR or WORD PTR")
    return size


def _is_mem(op):
    return op is not None and op.kind == 'mem'


def _is_reg(op):
    return op is not None and op.kind in ('reg', 'segreg')


def _seg_prefix_byte(ops):
    """The one segment-override prefix byte (0x2E/0x26/0x36/0x3E) needed
    for this instruction, if any of its memory operands carries one, or
    b'' otherwise. Must be the very first byte of the whole encoded
    instruction -- see `encode`'s own comment on why this is computed
    centrally there instead of inside `encode_rm`."""
    for op in ops:
        if op is not None and op.kind == 'mem' and op.mem.get('seg'):
            return bytes([SEG_PREFIX[op.mem['seg']]])
    return b''


def encode(mnemonic, ops, ctx):
    """ops: list of Operand (already parsed); ctx: EncodeCtx (see assembler.py).
    Returns the encoded instruction bytes.

    A segment-override prefix (`mov cs:foo,al`, say) must be the FIRST
    byte of the instruction, before the opcode -- but `encode_rm` is
    called by every mnemonic-specific encoder below *after* that
    encoder has already emitted its own opcode byte(s), so a prefix
    `encode_rm` returned inline used to land stuck in the middle of the
    instruction instead (opcode, then prefix, then ModRM/disp),
    corrupting it and everything decoded after it. Confirmed by testing
    in QEMU: `mov cs:outbyte_tmp,al` (src/loader/LDBIOS86Q.A86's own
    fdc_out_byte) silently became `MOV [outbyte_tmp],CH` plus a stray
    extra byte feeding into whatever came next. Fixed by having
    `encode_rm` leave the prefix out entirely and computing/prepending
    it once, here, for every mnemonic uniformly, regardless of which
    operand position the memory operand is in."""
    prefix = _seg_prefix_byte(ops)
    return prefix + _encode_body(mnemonic, ops, ctx)


def _encode_body(mnemonic, ops, ctx):
    m = mnemonic.lower()

    if m == 'ret' and ops:
        return bytes([0xC2]) + u16(ops[0].mem['value'])
    if m == 'retf' and ops:
        return bytes([0xCA]) + u16(ops[0].mem['value'])

    if m in NO_OPERAND:
        v = NO_OPERAND[m]
        if isinstance(v, tuple):
            base, size = v
            return _string_op(base, size, ctx)
        return bytes([v])

    if m in REP_PREFIXES:
        inner = ctx.rest_mnemonic
        inner_ops = ctx.rest_operands
        body = encode(inner, inner_ops, ctx)
        return bytes([REP_PREFIXES[m]]) + body

    if m in ('movs', 'lods', 'stos', 'cmps', 'scas'):
        size = _opsize(*ops) or 1
        for o in ops:
            if o.kind == 'reg' and o.reg == REG16['ax'] and o.size == 2:
                size = 2
        return _string_op(m, size, ctx)

    if m == 'int':
        v = ops[0].mem['value']
        return bytes([0xCD]) + u8(v)

    if m in ('jmp',):
        return _encode_jmp(ops, ctx)
    if m == 'jmps':
        return _encode_jmps(ops, ctx)
    if m == 'jmpf':
        return _encode_jmpf(ops, ctx)
    if m == 'call':
        return _encode_call(ops, ctx)
    if m == 'callf':
        return _encode_callf(ops, ctx)
    if m in JCC:
        return _encode_jcc(JCC[m], ops, ctx)
    if m in LOOP_OPS:
        return _encode_jcc(LOOP_OPS[m], ops, ctx)
    if m == 'jcxz':
        return _encode_jcc(0xE3, ops, ctx)
    if m == 'mov':
        return _encode_mov(ops, ctx)
    if m in ALU:
        return _encode_alu(m, ops, ctx)
    if m == 'test':
        return _encode_test(ops, ctx)
    if m == 'xchg':
        return _encode_xchg(ops, ctx)
    if m == 'lea':
        dst, src = ops
        if dst.kind != 'reg' or dst.size != 2:
            ctx.err("LEA destination must be a 16-bit register")
        if src.kind != 'mem':
            ctx.err("LEA source must be a memory operand")
        return bytes([0x8D]) + encode_rm(src, dst.reg, ctx)
    if m in ('push', 'pop'):
        return _encode_pushpop(m, ops, ctx)
    if m in ('inc', 'dec'):
        return _encode_incdec(m, ops, ctx)
    if m in F6_GROUP:
        return _encode_f6(m, ops, ctx)
    if m in SHIFT_GROUP:
        return _encode_shift(m, ops, ctx)
    if m == 'in':
        return _encode_in(ops, ctx)
    if m == 'out':
        return _encode_out(ops, ctx)

    ctx.err(f"unsupported mnemonic '{mnemonic}'")


def _string_op(base, size, ctx):
    opcode = STRING_OPS[base] + (1 if size == 2 else 0)
    return bytes([opcode])


def _rel_target(op, ctx):
    if op.kind != 'imm':
        ctx.err("expected a branch target address")
    return op.mem['value']


def _encode_jmp(ops, ctx):
    op = ops[0]
    if op.kind == 'imm':
        target = op.mem['value']
        rel = (target - (ctx.addr + 3)) & 0xFFFF
        return bytes([0xE9]) + u16(rel)
    size = op.size or 2
    if size != 2:
        ctx.err("indirect JMP target must be word-sized (near pointer)")
    return bytes([0xFF]) + encode_rm(op, 4, ctx)


def _encode_jmps(ops, ctx):
    op = ops[0]
    if ctx.jump_long:
        # Relaxed: JMPS is already unconditional, so growing it just
        # means "use the real near JMP form" -- no trampoline needed
        # (contrast _encode_jcc below, which can't do this because its
        # jump is conditional).
        if ctx.tentative:
            return bytes(3)
        target = _rel_target(op, ctx)
        rel = (target - (ctx.addr + 3)) & 0xFFFF
        return bytes([0xE9]) + u16(rel)
    if ctx.tentative:
        return bytes(2)
    target = _rel_target(op, ctx)
    rel = target - (ctx.addr + 2)
    if not (-128 <= rel <= 127):
        # The relaxation pass (assembler.py) should have already caught
        # this and set ctx.jump_long -- reaching here means relaxation
        # didn't converge on this site, a relaxation bug, not a normal
        # out-of-range source branch.
        ctx.err(f"JMPS target out of short-jump range (rel={rel}) after relaxation")
    return bytes([0xEB]) + u8(rel & 0xFF)


def _encode_jmpf(ops, ctx):
    op = ops[0]
    if op.kind == 'imm':
        target = op.mem['value']
        return bytes([0xEA]) + u16(target) + u16(0)
    size = op.size or 2
    return bytes([0xFF]) + encode_rm(op, 5, ctx)


def _encode_call(ops, ctx):
    op = ops[0]
    if op.kind == 'imm':
        target = op.mem['value']
        rel = (target - (ctx.addr + 3)) & 0xFFFF
        return bytes([0xE8]) + u16(rel)
    return bytes([0xFF]) + encode_rm(op, 2, ctx)


def _encode_callf(ops, ctx):
    op = ops[0]
    if op.kind == 'imm':
        target = op.mem['value']
        return bytes([0x9A]) + u16(target) + u16(0)
    return bytes([0xFF]) + encode_rm(op, 3, ctx)


def _encode_jcc(opcode, ops, ctx):
    if ctx.jump_long:
        # Relaxed form: a conditional/LOOP/JCXZ opcode can't take a
        # 16-bit displacement at all on the 8086, and (unlike JMPS) its
        # condition means it can't simply be re-encoded as some other
        # opcode with a longer displacement either. Use the standard
        # trampoline instead -- it works uniformly for every mnemonic in
        # SHORT_JUMP_MNEMONICS without needing a per-opcode "inverse
        # condition" table:
        #
        #   addr+0: <opcode> 03         ; short jump, always in range:
        #                                 always exactly 3 bytes forward,
        #                                 landing on the second JMP below
        #   addr+2: E9 <rel to addr+8>  ; near JMP, taken when the
        #                                 original condition was FALSE;
        #                                 skips over the real-target JMP
        #   addr+5: E9 <rel to target>  ; near JMP to the real target,
        #                                 only reached via the short jump
        #                                 at addr+0 (condition TRUE)
        #   addr+8: (falls through here when the condition was FALSE)
        if ctx.tentative:
            return bytes(8)
        target = _rel_target(ops[0], ctx)
        over_rel = 3  # addr+2's JMP always skips exactly the JMP at addr+5
        target_rel = (target - (ctx.addr + 8)) & 0xFFFF
        return (bytes([opcode, 0x03, 0xE9]) + u16(over_rel)
                + bytes([0xE9]) + u16(target_rel))
    if ctx.tentative:
        return bytes(2)
    target = _rel_target(ops[0], ctx)
    rel = target - (ctx.addr + 2)
    if not (-128 <= rel <= 127):
        # See the matching comment in _encode_jmps: the relaxation pass
        # should have already grown this site to the long form above.
        ctx.err(f"conditional/loop branch out of range (rel={rel}) after relaxation")
    return bytes([opcode]) + u8(rel & 0xFF)


def _encode_mov(ops, ctx):
    dst, src = ops
    if dst.kind == 'segreg' or src.kind == 'segreg':
        if dst.kind == 'segreg' and src.kind in ('reg', 'mem'):
            return bytes([0x8E]) + encode_rm(src, dst.reg, ctx)
        if src.kind == 'segreg' and dst.kind in ('reg', 'mem'):
            return bytes([0x8C]) + encode_rm(dst, src.reg, ctx)
        ctx.err("unsupported MOV form with segment register")
    if src.kind == 'imm':
        size = _need_size('mov', _opsize(dst, src), ctx)
        opcode = 0xC6 if size == 1 else 0xC7
        imm = u8(src.mem['value']) if size == 1 else u16(src.mem['value'])
        return bytes([opcode]) + encode_rm(dst, 0, ctx) + imm
    if dst.kind == 'reg':
        return bytes([0x8A if dst.size == 1 else 0x8B]) + encode_rm(src, dst.reg, ctx)
    if dst.kind == 'mem' and src.kind == 'reg':
        return bytes([0x88 if src.size == 1 else 0x89]) + encode_rm(dst, src.reg, ctx)
    ctx.err("unsupported MOV operand combination (mem,mem not allowed)")


def _encode_alu(name, ops, ctx):
    dst, src = ops
    idx = ALU[name]
    if src.kind == 'imm':
        size = _need_size(name, _opsize(dst, src), ctx)
        opcode = 0x80 if size == 1 else 0x81
        imm = u8(src.mem['value']) if size == 1 else u16(src.mem['value'])
        return bytes([opcode]) + encode_rm(dst, idx, ctx) + imm
    if dst.kind == 'reg':
        opcode = idx * 8 + 2 + (0 if dst.size == 1 else 1)
        return bytes([opcode]) + encode_rm(src, dst.reg, ctx)
    if dst.kind == 'mem' and src.kind == 'reg':
        opcode = idx * 8 + 0 + (0 if src.size == 1 else 1)
        return bytes([opcode]) + encode_rm(dst, src.reg, ctx)
    ctx.err(f"unsupported operand combination for '{name}' (mem,mem not allowed)")


def _encode_test(ops, ctx):
    dst, src = ops
    if src.kind == 'imm':
        size = _need_size('test', _opsize(dst, src), ctx)
        opcode = 0xA8 if (dst.kind == 'reg' and dst.reg == REG8['al'] and size == 1) else (0xF6 if size == 1 else 0xF7)
        if opcode in (0xF6, 0xF7):
            imm = u8(src.mem['value']) if size == 1 else u16(src.mem['value'])
            return bytes([opcode]) + encode_rm(dst, 0, ctx) + imm
        return bytes([opcode]) + u8(src.mem['value'])
    if dst.kind == 'reg':
        opcode = 0x84 if dst.size == 1 else 0x85
        return bytes([opcode]) + encode_rm(src, dst.reg, ctx)
    if src.kind == 'reg':
        opcode = 0x84 if src.size == 1 else 0x85
        return bytes([opcode]) + encode_rm(dst, src.reg, ctx)
    ctx.err("TEST requires at least one register operand")


def _encode_xchg(ops, ctx):
    a, b = ops
    if a.kind == 'reg':
        return bytes([0x86 if a.size == 1 else 0x87]) + encode_rm(b, a.reg, ctx)
    if b.kind == 'reg':
        return bytes([0x86 if b.size == 1 else 0x87]) + encode_rm(a, b.reg, ctx)
    ctx.err("XCHG requires at least one register operand")


def _encode_pushpop(name, ops, ctx):
    op = ops[0]
    if op.kind == 'segreg':
        if name == 'push':
            table = {SEGREG['es']: 0x06, SEGREG['cs']: 0x0E, SEGREG['ss']: 0x16, SEGREG['ds']: 0x1E}
        else:
            table = {SEGREG['es']: 0x07, SEGREG['ss']: 0x17, SEGREG['ds']: 0x1F}
        if op.reg not in table:
            ctx.err(f"cannot POP CS")
        return bytes([table[op.reg]])
    if op.kind == 'reg':
        base = 0x50 if name == 'push' else 0x58
        return bytes([base + op.reg])
    if op.kind == 'mem':
        if name == 'push':
            return bytes([0xFF]) + encode_rm(op, 6, ctx)
        return bytes([0x8F]) + encode_rm(op, 0, ctx)
    ctx.err(f"unsupported operand for {name}")


def _encode_incdec(name, ops, ctx):
    op = ops[0]
    size = _need_size(name, op.size, ctx)
    opcode = 0xFE if size == 1 else 0xFF
    reg_field = 0 if name == 'inc' else 1
    return bytes([opcode]) + encode_rm(op, reg_field, ctx)


def _encode_f6(name, ops, ctx):
    op = ops[0]
    size = _need_size(name, op.size, ctx)
    opcode = 0xF6 if size == 1 else 0xF7
    return bytes([opcode]) + encode_rm(op, F6_GROUP[name], ctx)


def _encode_shift(name, ops, ctx):
    dst, cnt = ops
    size = _need_size(name, dst.size, ctx)
    opcode = (0xD0 if size == 1 else 0xD1)
    if cnt.kind == 'reg' and cnt.reg == REG8['cl'] and cnt.size == 1:
        opcode = 0xD2 if size == 1 else 0xD3
    elif cnt.kind == 'imm' and cnt.mem['value'] == 1:
        pass
    else:
        ctx.err(f"{name}: 8086 only supports a shift count of 1 or CL")
    return bytes([opcode]) + encode_rm(dst, SHIFT_GROUP[name], ctx)


def _check_port_imm(value, ctx):
    # The immediate IN/OUT forms (0xE4-0xE7) only have an 8-bit port
    # field -- silently truncating a larger value (e.g. 0x3F8 -> 0xF8)
    # was a real, previously-undiscovered bug here: it assembled
    # clean and produced working-looking code that silently talked to
    # the wrong I/O port at runtime. A port >= 256 must go through DX
    # (`mov dx,port ! in/out dx,al`), the only 8086 encoding that can
    # actually address it.
    if value > 0xFF:
        ctx.err(f"IN/OUT port {value:#x} is >= 256 and needs DX "
                 "(the immediate IN/OUT form only has an 8-bit port "
                 "field, e.g. `mov dx,{:#x} ! in al,dx`)".format(value))


def _encode_in(ops, ctx):
    dst, src = ops
    if dst.kind != 'reg' or dst.reg != 0:
        ctx.err("IN destination must be AL or AX")
    wide = dst.size == 2
    if src.kind == 'reg' and src.size == 2 and src.reg == REG16['dx']:
        return bytes([0xEC if not wide else 0xED])
    if src.kind == 'imm':
        _check_port_imm(src.mem['value'], ctx)
        return bytes([0xE4 if not wide else 0xE5]) + u8(src.mem['value'])
    ctx.err("IN source must be DX or an immediate port number")


def _encode_out(ops, ctx):
    dst, src = ops
    if src.kind != 'reg' or src.reg != 0:
        ctx.err("OUT source must be AL or AX")
    wide = src.size == 2
    if dst.kind == 'reg' and dst.size == 2 and dst.reg == REG16['dx']:
        return bytes([0xEE if not wide else 0xEF])
    if dst.kind == 'imm':
        _check_port_imm(dst.mem['value'], ctx)
        return bytes([0xE6 if not wide else 0xE7]) + u8(dst.mem['value'])
    ctx.err("OUT destination must be DX or an immediate port number")


ALL_MNEMONICS = (set(NO_OPERAND) | set(REP_PREFIXES) | {'movs', 'lods', 'stos', 'cmps', 'scas'}
                  | {'int'} | BRANCH_MNEMONICS | {'mov'} | set(ALU) | {'test', 'xchg', 'lea'}
                  | {'push', 'pop', 'inc', 'dec'} | set(F6_GROUP) | set(SHIFT_GROUP)
                  | {'in', 'out'})
