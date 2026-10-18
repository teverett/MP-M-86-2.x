"""Preprocessing (comments, $ compiler-control lines, $INCLUDE) and
tokenizing for the PL/M-86 dialect subset used in this source tree."""
import os
import re
from .errors import PlmError

KEYWORDS = {
    'declare', 'dcl', 'procedure', 'end', 'if', 'then', 'else', 'do',
    'while', 'case', 'to', 'by', 'return', 'call', 'goto', 'go', 'literally',
    'structure', 'based', 'byte', 'address', 'pointer', 'word', 'external',
    'public', 'reentrant', 'interrupt', 'and', 'or', 'not', 'xor', 'mod',
    'label', 'disable', 'enable', 'initial', 'data', 'at',
}

TOKEN_RE = re.compile(r"""
    (?P<ws>[ \t\r\n]+)
  | (?P<num>[0-9][0-9A-Za-z$]*)
  | (?P<ident>[A-Za-z][A-Za-z0-9$]*)
  | (?P<assign>:=)
  | (?P<ne>[<][>])
  | (?P<le>[<][=])
  | (?P<ge>[>][=])
  | (?P<punct>[-+*/=<>().,;:@])
""", re.VERBOSE)


def _strip_zeof(text):
    return text.split('\x1a', 1)[0]


def _strip_comments(text):
    out = []
    i = 0
    n = len(text)
    in_str = False
    while i < n:
        c = text[i]
        if in_str:
            out.append(c)
            if c == "'":
                in_str = False
            i += 1
            continue
        if c == "'":
            in_str = True
            out.append(c)
            i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j == -1:
                raise PlmError("unterminated /* comment */")
            out.append('\n' * text.count('\n', i, j + 2))  # keep line numbers aligned
            i = j + 2
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def _find_include(name, search_dirs):
    for d in search_dirs:
        try:
            entries = os.listdir(d)
        except OSError:
            continue
        for e in entries:
            if e.lower() == name.lower():
                return os.path.join(d, e)
    return None


class Tok:
    __slots__ = ('kind', 'val', 'lineno')

    def __init__(self, kind, val, lineno):
        self.kind = kind
        self.val = val
        self.lineno = lineno

    def __repr__(self):
        return f"Tok({self.kind!r},{self.val!r}@{self.lineno})"


def preprocess(path, search_dirs, _stack=None):
    """Returns a flat list of (lineno, code_text) for one file, with
    comments stripped and $INCLUDE expanded. Other $ directives (TITLE,
    COMPACT, EJECT, OPTIMIZE, DATE, ...) are recognized and dropped --
    they only affect compiler listings, not code generation."""
    if _stack is None:
        _stack = []
    path = os.path.abspath(path)
    if path in _stack:
        raise PlmError(f"circular $INCLUDE of {path}")
    with open(path, encoding='latin-1') as f:
        raw = f.read()
    raw = _strip_zeof(raw)
    raw = _strip_comments(raw)
    out = []
    my_dir = os.path.dirname(path)
    dirs = [my_dir] + [d for d in search_dirs if d != my_dir]
    for lineno, line in enumerate(raw.split('\n'), 1):
        stripped = line.strip()
        if stripped.startswith('$'):
            body = stripped[1:].strip()
            m = re.match(r'([A-Za-z]+)\s*\(?\s*([^)]*)\)?', body)
            directive = m.group(1).lower() if m else ''
            arg = m.group(2).strip() if m else ''
            if directive == 'include':
                fname = arg.split()[0] if arg else ''
                inc_path = _find_include(fname, dirs)
                if inc_path is None:
                    raise PlmError(f"$INCLUDE file not found: {fname}", os.path.basename(path), lineno)
                out.extend(preprocess(inc_path, search_dirs, _stack + [path]))
                continue
            # TITLE/COMPACT/EJECT/OPTIMIZE/DATE/NOLIST/... : listing-only, ignore.
            continue
        out.append((os.path.basename(path), lineno, line))
    return out


def tokenize(lines):
    """lines: list of (filename, lineno, text). Returns list[Tok].

    A string literal is allowed to span multiple physical lines (real
    PL/M-86 source does this for long LITERALLY text, e.g. a structure
    template split across several DECLARE lines) -- so the scan below
    can pull in subsequent entries of `lines` while still inside a
    still-open quote.
    """
    toks = []
    li = 0
    nlines = len(lines)
    filename, lineno, text = (lines[0] if lines else ('', 0, ''))
    i = 0
    n = len(text)
    start_lineno = lineno
    while li < nlines:
        if i >= n:
            li += 1
            if li >= nlines:
                break
            filename, lineno, text = lines[li]
            i = 0
            n = len(text)
            continue
        c = text[i]
        if c.isspace():
            i += 1
            continue
        if c == "'":
            start_lineno = lineno
            j = i + 1
            buf = []
            while True:
                if j >= n:
                    buf.append('\n')
                    li += 1
                    if li >= nlines:
                        raise PlmError("unterminated string literal", filename, start_lineno)
                    filename, lineno, text = lines[li]
                    j = 0
                    n = len(text)
                    continue
                if text[j] == "'":
                    if j + 1 < n and text[j + 1] == "'":
                        buf.append("'")
                        j += 2
                        continue
                    break
                buf.append(text[j])
                j += 1
            toks.append(Tok('STR', ''.join(buf), start_lineno))
            i = j + 1
            continue
        m = TOKEN_RE.match(text, i)
        if not m or m.lastgroup == 'ws':
            if m and m.lastgroup == 'ws':
                i = m.end()
                continue
            raise PlmError(f"unexpected character {c!r}", filename, lineno)
        kind = m.lastgroup
        val = m.group()
        i = m.end()
        if kind == 'num':
            # '$' is an insignificant readability separator in PL/M, in
            # numbers (e.g. "0110$0000b") just as in identifiers.
            toks.append(Tok('NUM', val.replace('$', ''), lineno))
        elif kind == 'ident':
            canon = val.replace('$', '')
            low = canon.lower()
            if low in KEYWORDS:
                toks.append(Tok(low.upper(), canon, lineno))
            else:
                toks.append(Tok('IDENT', canon, lineno))
        elif kind == 'assign':
            toks.append(Tok(':=', ':=', lineno))
        elif kind == 'ne':
            toks.append(Tok('<>', '<>', lineno))
        elif kind == 'le':
            toks.append(Tok('<=', '<=', lineno))
        elif kind == 'ge':
            toks.append(Tok('>=', '>=', lineno))
        else:
            toks.append(Tok(val, val, lineno))
    toks.append(Tok('EOF', None, lines[-1][1] if lines else 0))
    return toks


def expand_literally(toks):
    """Real PL/M LITERALLY is textual macro substitution, not just a named
    numeric constant: 'NAME LITERALLY '<text>'' defines NAME to expand,
    wherever it is referenced afterward, to '<text>' re-tokenized (and
    recursively macro-expanded itself, using whatever is already defined).
    This tree's own COMLIT.LIT bootstraps the abbreviation 'lit' for
    'literally' this way (and, redundantly but harmlessly, 'dcl' for
    'declare'), so this step runs on the raw token stream before any
    keyword meaning is assumed, and applies to every token (not just
    IDENTs), since a macro can shadow an already-keyworded spelling."""
    macros = {}

    def pull(queue):
        while queue:
            t = queue[0]
            if t.kind not in ('STR', 'NUM', 'EOF') and t.val is not None:
                m = macros.get(t.val.lower())
                if m is not None:
                    queue.pop(0)
                    queue[0:0] = m
                    continue
            return queue.pop(0)
        return None

    def run(queue):
        out = []
        pending = None
        while True:
            t = pending if pending is not None else pull(queue)
            pending = None
            if t is None or t.kind == 'EOF':
                if t is not None:
                    out.append(t)
                break
            if t.kind not in ('STR', 'NUM'):
                t2 = pull(queue)
                if t2 is not None and t2.kind == 'LITERALLY':
                    t3 = pull(queue)
                    if t3 is None or t3.kind != 'STR':
                        raise PlmError(f"LITERALLY for '{t.val}' must be followed by a "
                                       "quoted string", getattr(t, 'filename', None), t.lineno)
                    frag = tokenize([('<literally>', t3.lineno, t3.val)])
                    frag = frag[:-1]  # drop the fragment's own EOF
                    macros[t.val.lower()] = run(frag)
                    # A stripped definition leaves a dangling ',' behind in
                    # a DECLARE list; remove whichever side has one so the
                    # remaining list stays well-formed.
                    if out and out[-1].kind == ',':
                        out.pop()
                    else:
                        t4 = pull(queue)
                        if t4 is not None and t4.kind != ',':
                            pending = t4
                    continue
                pending = t2
            out.append(t)
        return out

    expanded = run(list(toks))
    # A DECLARE statement that turned out to consist entirely of LITERALLY
    # definitions is now just "DECLARE ;" (or "DCL ;"); drop both tokens.
    cleaned = []
    i = 0
    while i < len(expanded):
        if (expanded[i].kind in ('DECLARE', 'DCL') and i + 1 < len(expanded)
                and expanded[i + 1].kind == ';'):
            i += 2
            continue
        cleaned.append(expanded[i])
        i += 1
    return cleaned


def parse_number(s):
    low = s.lower()
    if low.endswith('h'):
        return int(s[:-1], 16)
    if low.endswith('o') or low.endswith('q'):
        return int(s[:-1], 8)
    if low.endswith('b') and len(s) > 1 and all(ch in '01' for ch in s[:-1]):
        return int(s[:-1], 2)
    return int(s, 10)
