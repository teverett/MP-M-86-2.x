"""Expression evaluation and instruction-operand parsing."""
import re
from .errors import AsmError
from .lex import tokenize_expr, parse_number, SEGREG, REG16, REG8, MEM_COMBOS


class Symbol:
    __slots__ = ('name', 'kind', 'value', 'size')

    def __init__(self, name, kind, value, size=None):
        self.name = name       # original-case name
        self.kind = kind       # 'equ' or 'label'
        self.value = value     # resolved integer
        self.size = size       # 1, 2, or None


class Value:
    __slots__ = ('n', 'size', 'symbol', 'is_offset')

    def __init__(self, n, size=None, symbol=None, is_offset=False):
        self.n = n & 0xFFFF if n >= 0 else n
        self.size = size
        self.symbol = symbol  # the Symbol this value came from, if a bare ref
        # True if this value went through the OFFSET operator: forces an
        # instruction operand to be classified as an immediate address,
        # even though a bare reference to the same symbol would otherwise
        # be treated as a direct memory operand (see parse_operand below).
        self.is_offset = is_offset


class ExprCtx:
    def __init__(self, symtab, cur_addr, filename, lineno, tentative=False, pending=None,
                 presize=None, prekind=None):
        self.symtab = symtab
        self.cur_addr = cur_addr
        self.filename = filename
        self.lineno = lineno
        # Pass 1 only needs instruction *lengths*, which never depend on a
        # forward-referenced symbol's value (see tools/README.md). When
        # tentative, an unresolved name is assumed to be a forward-referenced
        # data label (the only case this tree actually relies on) instead of
        # raising, so pass 1 can still classify the operand as memory/size-2.
        self.tentative = tentative
        self.pending = pending
        # name(lower) -> 1/2, learned by a cheap pre-scan of every DB/DW/RB/RW
        # label in the file, so a forward reference still gets the right
        # byte/word size (which affects instruction length, e.g. CMP imm8
        # vs imm16) even before its defining line has been reached.
        self.presize = presize or {}
        # name(lower) -> ('equ'|'label', size) for every EQU in the file,
        # learned the same way (see _prescan_sizes in assembler.py).
        self.prekind = prekind or {}

    def err(self, msg):
        raise AsmError(msg, self.filename, self.lineno)

    def lookup(self, name):
        key = name.lower()
        sym = self.symtab.get(key)
        if sym is None and key.startswith('.'):
            # DRI ASM86 convention: a leading '.' on a reference to an
            # offset-style EQU constant is cosmetic, not part of the name
            # (e.g. "mov dx,.s_version" referencing "s_version equ ...").
            sym = self.symtab.get(key[1:])
            if sym is not None:
                key = key[1:]
        if sym is None:
            if self.tentative:
                kind, size = self.prekind.get(key, ('label', self.presize.get(key)))
                sym = Symbol(name, kind, 0, size)
                self.symtab[key] = sym
                if self.pending is not None:
                    self.pending.add(key)
                return sym
            self.err(f"undefined symbol '{name}'")
        return sym


class _Parser:
    def __init__(self, toks, ctx):
        self.toks = toks
        self.i = 0
        self.ctx = ctx

    def peek(self):
        return self.toks[self.i]

    def next(self):
        t = self.toks[self.i]
        self.i += 1
        return t

    def expect(self, kind):
        t = self.next()
        if t.kind != kind:
            self.ctx.err(f"expected {kind}, got {t.kind} ({t.val!r})")
        return t

    # Precedence (low to high): OR > XOR > AND > (+ -) > (* /) > unary
    def parse(self):
        v = self.parse_or()
        return v

    def parse_or(self):
        v = self.parse_xor()
        while self.peek().kind == 'ID' and self.peek().val.lower() == 'or':
            self.next()
            r = self.parse_xor()
            v = Value(v.n | r.n, v.size or r.size)
        return v

    def parse_xor(self):
        v = self.parse_and()
        while self.peek().kind == 'ID' and self.peek().val.lower() == 'xor':
            self.next()
            r = self.parse_and()
            v = Value(v.n ^ r.n, v.size or r.size)
        return v

    def parse_and(self):
        v = self.parse_add()
        while self.peek().kind == 'ID' and self.peek().val.lower() == 'and':
            self.next()
            r = self.parse_add()
            v = Value(v.n & r.n, v.size or r.size)
        return v

    def parse_add(self):
        v = self.parse_mul()
        while self.peek().kind in ('+', '-'):
            op = self.next().kind
            r = self.parse_mul()
            n = v.n + r.n if op == '+' else v.n - r.n
            v = Value(n, v.size or r.size)
        return v

    def parse_mul(self):
        v = self.parse_unary()
        while True:
            t = self.peek()
            if t.kind == '*':
                self.next()
                r = self.parse_unary()
                v = Value(v.n * r.n, v.size or r.size)
            elif t.kind == '/':
                self.next()
                r = self.parse_unary()
                v = Value(v.n // r.n, v.size or r.size)
            elif t.kind == 'ID' and t.val.lower() == 'mod':
                self.next()
                r = self.parse_unary()
                v = Value(v.n % r.n, v.size or r.size)
            elif t.kind == 'ID' and t.val.lower() == 'shl':
                self.next()
                r = self.parse_unary()
                v = Value((v.n << r.n) & 0xFFFF, v.size or r.size)
            elif t.kind == 'ID' and t.val.lower() == 'shr':
                self.next()
                r = self.parse_unary()
                v = Value((v.n & 0xFFFF) >> r.n, v.size or r.size)
            else:
                break
        return v

    def parse_unary(self):
        t = self.peek()
        if t.kind == '-':
            self.next()
            v = self.parse_unary()
            return Value(-v.n, v.size)
        if t.kind == 'ID' and t.val.lower() == 'not':
            self.next()
            v = self.parse_unary()
            return Value((~v.n) & 0xFFFF, v.size)
        if t.kind == 'ID' and t.val.lower() == 'offset':
            self.next()
            v = self.parse_unary()
            return Value(v.n, v.size, v.symbol, is_offset=True)
        if t.kind == 'ID' and t.val.lower() in ('byte', 'word'):
            size = 1 if t.val.lower() == 'byte' else 2
            save = self.i
            self.next()
            if self.peek().kind == 'ID' and self.peek().val.lower() == 'ptr':
                self.next()
                v = self.parse_unary()
                return Value(v.n, size)
            self.i = save  # 'byte'/'word' used as a plain identifier instead
        return self.parse_primary()

    def parse_primary(self):
        t = self.next()
        if t.kind == 'NUM':
            return Value(parse_number(t.val))
        if t.kind == 'STR':
            s = t.val
            if len(s) == 0:
                return Value(0, 1)
            if len(s) == 1:
                return Value(ord(s), 1)
            if len(s) == 2:
                return Value(ord(s[0]) | (ord(s[1]) << 8), 2)
            self.ctx.err(f"character literal too long: '{s}'")
        if t.kind == 'DOLLAR':
            return Value(self.ctx.cur_addr, 2)
        if t.kind == '(':
            v = self.parse()
            self.expect(')')
            return v
        if t.kind == 'ID':
            sym = self.ctx.lookup(t.val)
            return Value(sym.value, sym.size, symbol=sym)
        self.ctx.err(f"unexpected token {t.kind} ({t.val!r}) in expression")


def eval_tokens(toks, ctx):
    p = _Parser(toks, ctx)
    v = p.parse()
    if p.peek().kind != 'EOF':
        ctx.err(f"trailing tokens in expression: {p.peek().val!r}")
    return v


def eval_text(text, ctx):
    return eval_tokens(tokenize_expr(text), ctx)


# ---------------------------------------------------------------------
# Instruction operand parsing
# ---------------------------------------------------------------------

class Operand:
    __slots__ = ('kind', 'reg', 'size', 'mem')
    # kind in {'reg', 'segreg', 'imm', 'mem'}

    def __init__(self, kind, reg=None, size=None, mem=None):
        self.kind = kind
        self.reg = reg
        self.size = size
        self.mem = mem  # dict(base=frozenset|None, disp=int|None, seg=str|None, dispsize)

    def __repr__(self):
        return f"Operand({self.kind},{self.reg},{self.size},{self.mem})"


_BRACKET_RE = re.compile(r'^(.*)\[(.*)\]$')
_BRACKET_INNER_RE = re.compile(r'^\s*([A-Za-z]+)\s*(.*)$')
_REGS8086 = frozenset(['bx', 'bp', 'si', 'di'])
_SEGOVR_RE = re.compile(r'^\s*(cs|ds|es|ss)\s*:\s*(.*)$', re.IGNORECASE)
_CAST_RE = re.compile(r'^\s*(byte|word)\s+ptr\s*(.*)$', re.IGNORECASE)


def _parse_bracket_inside(inside, ctx):
    """Parse the contents of [...]: a register, a register pair
    ("bx+si"), or a register plus a signed displacement expression
    ("bp-2", "bp+4"). Returns (regs: frozenset, disp_text: str|None)."""
    m = _BRACKET_INNER_RE.match(inside)
    if not m:
        ctx.err(f"malformed memory operand: [{inside}]")
    first = m.group(1).lower()
    rest = m.group(2).strip()
    if first not in _REGS8086:
        ctx.err(f"'{first}' is not a valid base register for 8086 memory addressing")
    if rest == '':
        regs = frozenset([first])
        disp_text = None
    elif rest[0] in '+-':
        maybe_reg = rest[1:].strip().lower()
        if rest[0] == '+' and maybe_reg in _REGS8086:
            regs = frozenset([first, maybe_reg])
            disp_text = None
        else:
            regs = frozenset([first])
            disp_text = rest
    else:
        ctx.err(f"malformed memory operand: [{inside}]")
    if regs not in MEM_COMBOS:
        ctx.err(f"invalid 8086 memory addressing combo: [{inside}]")
    return regs, disp_text


def parse_operand(text, ctx, branch_context=False):
    text = text.strip()
    if text == '':
        return None

    seg_override = None
    m = _SEGOVR_RE.match(text)
    if m:
        seg_override = m.group(1).lower()
        text = m.group(2).strip()

    explicit_size = None
    m = _CAST_RE.match(text)
    if m:
        explicit_size = 1 if m.group(1).lower() == 'byte' else 2
        text = m.group(2).strip()

    low = text.lower()
    if seg_override is None and explicit_size is None:
        if low in REG16:
            return Operand('reg', REG16[low], 2)
        if low in REG8:
            return Operand('reg', REG8[low], 1)
        if low in SEGREG:
            return Operand('segreg', SEGREG[low], 2)

    m = _BRACKET_RE.match(text)
    if m:
        prefix = m.group(1).strip()
        inside = m.group(2).strip()
        regs, disp_text = _parse_bracket_inside(inside, ctx)
        disp = None
        dsize = None
        # A prefix before the bracket (DRI's own convention, e.g.
        # "tod_sec[bx]") and a +/- displacement inside it (needed for
        # frame-relative addressing, e.g. "[bp-2]") both contribute to
        # the total displacement if both happen to be present.
        if prefix:
            v = eval_text(prefix, ctx)
            disp = v.n
            dsize = v.size
        if disp_text is not None:
            v = eval_text(disp_text, ctx)
            disp = (disp or 0) + v.n
            dsize = dsize or v.size
        size = explicit_size or dsize
        return Operand('mem', size=size,
                        mem={'base': regs, 'disp': disp, 'seg': seg_override})

    # No brackets: bare expression.
    v = eval_text(text, ctx)
    sym = v.symbol
    if branch_context:
        return Operand('imm', size=explicit_size or 2, mem={'value': v.n})
    if v.is_offset:
        return Operand('imm', size=explicit_size or 2, mem={'value': v.n})
    if sym is not None and (sym.kind == 'label' or (sym.kind == 'equ' and sym.size)):
        size = explicit_size or sym.size
        return Operand('mem', size=size,
                        mem={'base': None, 'disp': v.n, 'seg': seg_override})
    return Operand('imm', size=explicit_size or v.size, mem={'value': v.n})
