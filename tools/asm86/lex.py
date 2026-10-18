"""Line/statement/expression lexing for the ASM86-dialect assembler.

Covers the syntax actually used by the MP/M-86 2.x source tree:
  - ';' line comments
  - '!' used to separate multiple statements on one physical line
  - labels with or without a trailing ':'
  - numeric literals with H/O/Q/B suffixes, decimal default
  - character literals 'x' / 'xy' (packed into a byte/word)
  - EQU, title/name/cseg/dseg/aseg/org/db/dw/rb/rw/end/include/if/endif
  - operators + - * / MOD SHL SHR AND OR XOR NOT, unary -, OFFSET,
    BYTE PTR / WORD PTR casts, '$' (current location counter)
  - memory operands: SEG: SYMBOL[REG(+REG)]  (register combos valid on 8086)
"""
import re
from .errors import AsmError

REG16 = {'ax': 0, 'cx': 1, 'dx': 2, 'bx': 3, 'sp': 4, 'bp': 5, 'si': 6, 'di': 7}
REG8 = {'al': 0, 'cl': 1, 'dl': 2, 'bl': 3, 'ah': 4, 'ch': 5, 'dh': 6, 'bh': 7}
SEGREG = {'es': 0, 'cs': 1, 'ss': 2, 'ds': 3}
ALL_REGS = set(REG16) | set(REG8) | set(SEGREG)

# Registers/combos legal inside [ ... ] on the 8086.
MEM_COMBOS = {
    frozenset(['bx', 'si']): 0,
    frozenset(['bx', 'di']): 1,
    frozenset(['bp', 'si']): 2,
    frozenset(['bp', 'di']): 3,
    frozenset(['si']): 4,
    frozenset(['di']): 5,
    frozenset(['bp']): 6,  # forced disp16 (mod=00/rm=110 is reserved for direct addr)
    frozenset(['bx']): 7,
}

IDENT_RE = re.compile(r'[A-Za-z_.$?][A-Za-z0-9_.$?]*')
NUMBER_RE = re.compile(r'[0-9][0-9A-Za-z]*')


def strip_comment(line):
    """Remove a trailing ';' comment (outside of quotes)."""
    in_q = False
    for i, ch in enumerate(line):
        if ch == "'":
            in_q = not in_q
        elif ch == ';' and not in_q:
            return line[:i]
    return line


def split_statements(line):
    """Split a (comment-stripped) physical line on top-level '!'."""
    parts = []
    buf = []
    in_q = False
    for ch in line:
        if ch == "'":
            in_q = not in_q
            buf.append(ch)
        elif ch == '!' and not in_q:
            parts.append(''.join(buf))
            buf = []
        else:
            buf.append(ch)
    parts.append(''.join(buf))
    return [p.strip() for p in parts]


def split_top_commas(text):
    """Split operand text on top-level commas (not inside [] () or quotes)."""
    parts = []
    buf = []
    depth = 0
    in_q = False
    for ch in text:
        if ch == "'":
            in_q = not in_q
            buf.append(ch)
        elif in_q:
            buf.append(ch)
        elif ch in '([':
            depth += 1
            buf.append(ch)
        elif ch in ')]':
            depth -= 1
            buf.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(buf))
            buf = []
        else:
            buf.append(ch)
    parts.append(''.join(buf))
    return [p.strip() for p in parts if p.strip() != '']


def _first_token(text):
    m = IDENT_RE.match(text)
    if not m:
        return None, text
    return m.group(0), text[m.end():].strip()


# Everything the lexer must recognise as "not a label" when scanning the
# first word of a label-less statement.
DIRECTIVES = {
    'title', 'name', 'cseg', 'dseg', 'aseg', 'org', 'equ', 'db', 'byte',
    'dw', 'word', 'dd', 'rb', 'rw', 'rs', 'end', 'include', 'if', 'endif', 'eject',
    'page', 'public', 'extrn', 'global',
}


def parse_statement(text, mnemonics):
    """Split a statement into (label, mnemonic, operand_text).

    label and/or mnemonic may be None for blank/label-only statements.
    """
    text = text.strip()
    if not text:
        return None, None, ''

    m = re.match(r'^([A-Za-z_.$?][A-Za-z0-9_.$?]*)\s*:\s*(.*)$', text)
    if m:
        label = m.group(1)
        rest = m.group(2).strip()
        if not rest:
            return label, None, ''
        mnem, operand_text = _first_token(rest)
        return label, mnem, operand_text

    first, rest = _first_token(text)
    if first is None:
        raise AsmError(f"cannot parse statement: {text!r}")
    low = first.lower()
    if low in DIRECTIVES or low in mnemonics:
        return None, first, rest

    # No colon, first token isn't a keyword -> it's a label; next token
    # is the directive/mnemonic (e.g. "sysdat dw 0,0,0").
    mnem, operand_text = _first_token(rest)
    return first, mnem, operand_text


class Tok:
    __slots__ = ('kind', 'val')

    def __init__(self, kind, val):
        self.kind = kind
        self.val = val

    def __repr__(self):
        return f"Tok({self.kind!r},{self.val!r})"


def tokenize_expr(text):
    toks = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c.isspace():
            i += 1
            continue
        if c == "'":
            j = text.index("'", i + 1) if "'" in text[i + 1:] else -1
            if j == -1:
                raise AsmError(f"unterminated character literal in: {text!r}")
            toks.append(Tok('STR', text[i + 1:j]))
            i = j + 1
            continue
        if c.isdigit():
            m = NUMBER_RE.match(text, i)
            toks.append(Tok('NUM', m.group(0)))
            i = m.end()
            continue
        if c.isalpha() or c in '_.$?':
            if c == '$' and (i + 1 >= n or not (text[i + 1].isalnum() or text[i + 1] in '_.$?')):
                toks.append(Tok('DOLLAR', '$'))
                i += 1
                continue
            m = IDENT_RE.match(text, i)
            word = m.group(0)
            if word[0] == '.' and word[1:].isdigit():
                # DRI ASM86 convention: a leading '.' is cosmetic even on
                # a plain number, not just a symbol name (e.g. "mov .0,
                # offset int_trap" in XIOS.A86, meaning absolute address/
                # immediate 0). Emit it as a NUM, not an identifier to
                # look up.
                toks.append(Tok('NUM', word[1:]))
            else:
                toks.append(Tok('ID', word))
            i = m.end()
            continue
        if c in '+-*/()[]:':
            toks.append(Tok(c, c))
            i += 1
            continue
        if c == ',':
            toks.append(Tok(',', ','))
            i += 1
            continue
        raise AsmError(f"unexpected character {c!r} in expression: {text!r}")
    toks.append(Tok('EOF', None))
    return toks


def parse_number(tok):
    s = tok
    low = s.lower()
    if low.endswith('h'):
        return int(s[:-1], 16)
    if low.endswith('o') or low.endswith('q'):
        return int(s[:-1], 8)
    if low.endswith('b') and all(ch in '01' for ch in s[:-1]) and len(s) > 1:
        return int(s[:-1], 2)
    if all(ch in '0123456789' for ch in s):
        return int(s, 10)
    # Fallback: digit-string with trailing hex digits but no 'h' suffix
    # shouldn't occur in well-formed ASM86 source; try hex as last resort.
    return int(s, 16)
