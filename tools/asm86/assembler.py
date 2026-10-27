"""Two-pass ASM86-dialect assembler driver."""
import os
import re
import struct
from .errors import AsmError
from .lex import strip_comment, split_statements, split_top_commas, parse_statement, tokenize_expr
from .expr import ExprCtx, eval_tokens, eval_text, Symbol, parse_operand
from . import isa

DIRECTIVE_NAMES = {
    'title', 'name', 'cseg', 'dseg', 'aseg', 'eseg', 'org', 'equ', 'db', 'byte',
    'dw', 'word', 'dd', 'rb', 'rw', 'rs', 'end', 'include', 'if', 'endif', 'eject',
    'page', 'public', 'extrn', 'global',
}
ALL_MNEMONICS = DIRECTIVE_NAMES | isa.ALL_MNEMONICS


class EncodeCtx:
    """Adapter handed to isa.encode(); carries current address + error()."""

    def __init__(self, addr, filename, lineno, tentative=False):
        self.addr = addr
        self.filename = filename
        self.lineno = lineno
        self.rest_mnemonic = None
        self.rest_operands = None
        # True during pass 1: forward-referenced symbols may still be
        # placeholders, so value-dependent checks (e.g. "is this branch in
        # rel8 range?") must not be enforced yet -- only instruction
        # *length* (fixed per mnemonic, see isa.py) matters in pass 1.
        self.tentative = tentative
        # For isa.SHORT_JUMP_MNEMONICS only: whether this specific
        # occurrence has been relaxed to its long form. Decided by
        # Assembler._relax_jumps before pass 1/2 ever run; see isa.py's
        # _encode_jcc/_encode_jmps for what each mnemonic's long form is.
        self.jump_long = False

    def err(self, msg):
        raise AsmError(msg, self.filename, self.lineno)


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


def flatten(path, search_dirs, _stack=None):
    """Expand INCLUDE directives into a flat (filename, lineno, text) list."""
    if _stack is None:
        _stack = []
    path = os.path.abspath(path)
    if path in _stack:
        raise AsmError(f"circular INCLUDE of {path}")
    out = []
    my_dir = os.path.dirname(path)
    dirs = [my_dir] + [d for d in search_dirs if d != my_dir]
    with open(path, encoding='latin-1') as f:
        content = f.read()
    content = content.split('\x1a', 1)[0]  # CP/M ^Z end-of-file padding
    lines = content.splitlines()
    for lineno, raw in enumerate(lines, 1):
        text = strip_comment(raw)
        kept = []
        for stmt_text in split_statements(text):
            stripped = stmt_text.strip()
            low = stripped.lower()
            if low == 'end':
                # DRI ASM86: END is scoped to the file it appears in, not
                # the whole assembly -- an INCLUDEd fragment commonly ends
                # with its own "END" (e.g. FILE1.BDO's "end bdos
                # filesystem part 1" / END, with FILE2-4.BDO still to
                # come after it in BDOS.A86's own include list). Stop
                # flattening *this* file here and return to whichever
                # file included it (or, for the top-level file, this is
                # simply its own real end).
                if kept:
                    out.append((os.path.basename(path), lineno, ' ! '.join(kept)))
                return out
            if low.startswith('include'):
                rest = stripped[len('include'):].strip()
                if rest and (stripped.split()[0].lower() == 'include'):
                    fname = rest.split()[0]
                    inc_path = _find_include(fname, dirs)
                    if inc_path is None:
                        raise AsmError(f"INCLUDE file not found: {fname}", os.path.basename(path), lineno)
                    # Flush any statements seen so far on this line first,
                    # so their relative order (e.g. "eject ! include x")
                    # is preserved, then splice the include's own lines in.
                    if kept:
                        out.append((os.path.basename(path), lineno, ' ! '.join(kept)))
                        kept = []
                    out.extend(flatten(inc_path, search_dirs, _stack + [path]))
                    continue
            kept.append(stmt_text)
        if kept:
            out.append((os.path.basename(path), lineno, ' ! '.join(kept)))
    return out


class Assembler:
    def __init__(self):
        self.symtab = {}          # lower(name) -> Symbol
        self.segs = {}            # seg name -> {offset: byte}
        self.seg_order = []
        self.cur_seg = None
        self.loc = {}             # seg name -> int location counter
        self.ended = False
        self.if_stack = []        # list of bool (assembling?)
        self.pending = set()      # tentative (unresolved-in-pass-1) symbol keys
        self.tentative = False
        self.presize = {}         # name(lower) -> 1/2, from _prescan_sizes()
        self.prekind = {}         # name(lower) -> ('equ'|'label', size), from _prescan_sizes()
        self.preseg = {}          # name(lower) -> 'cseg'/'dseg'/'eseg'/'aseg'/None, from _prescan_sizes()
        # Jump-size relaxation state (see _relax_jumps). jump_long is
        # keyed by a sequential index assigned to each SHORT_JUMP_MNEMONICS
        # occurrence, in file order -- stable across repeated passes
        # because every pass walks the exact same `lines` in the exact
        # same order. jump_sites is only populated during a tentative
        # pass, to be checked once that pass's symtab is fully resolved.
        self.jump_long = {}
        self.jump_sites = []
        self._jump_idx = 0
        # name(lower) -> total emitted byte length of that DB/DW/DD/
        # RB/RW/RS declaration, for ASM86's LENGTH operator (see
        # _do_statement and ExprCtx.lookup_length in expr.py).
        self.lengths = {}

    # -- segment / symbol helpers -----------------------------------
    def switch_seg(self, name):
        if name not in self.segs:
            self.segs[name] = {}
            self.loc[name] = 0
            self.seg_order.append(name)
        self.cur_seg = name

    def cur_addr(self):
        if self.cur_seg is None:
            self.switch_seg('cseg')
        return self.loc[self.cur_seg]

    def emit(self, data):
        if self.cur_seg is None:
            self.switch_seg('cseg')
        seg = self.segs[self.cur_seg]
        addr = self.loc[self.cur_seg]
        for i, b in enumerate(data):
            seg[addr + i] = b & 0xFF
        self.loc[self.cur_seg] = addr + len(data)

    def define_symbol(self, name, kind, value, size, filename, lineno, allow_redefine=False,
                       is_addr=False):
        key = name.lower()
        existing = self.symtab.get(key)
        if existing is not None and not allow_redefine:
            if existing.kind != kind or existing.value != value:
                raise AsmError(f"symbol '{name}' redefined (was {existing.kind}={existing.value}, "
                                f"now {kind}={value})", filename, lineno)
        seg = self.cur_seg if kind == 'label' else None
        moved = getattr(self, '_moved', None)
        if (moved is not None and kind == 'label' and existing is not None and existing.kind == 'label'
                and existing.value != value):
            moved.append((name, existing.value, value, (filename, lineno)))
        self.symtab[key] = Symbol(name, kind, value, size, seg, is_addr)
        self.pending.discard(key)

    def active(self):
        return all(self.if_stack) if self.if_stack else True

    # -- main entry ---------------------------------------------------
    def assemble(self, path, search_dirs):
        lines = flatten(path, search_dirs)
        self._prescan_sizes(lines)
        self._relax_jumps(lines)
        if self.pending:
            names = ', '.join(sorted(self.symtab[k].name for k in self.pending))
            raise AsmError(f"undefined symbol(s), never defined anywhere in the file: {names}")
        # Pass 1 (the last iteration _relax_jumps ran) leaves self.symtab
        # fully populated with every label's final address -- lengths no
        # longer depend on a forward reference's *value* (only on
        # self.jump_long, which is now stable) -- so pass 2 reuses it
        # verbatim and only resets the segment/location state before
        # re-emitting bytes.
        self.segs = {}
        self.seg_order = []
        self.loc = {}
        self.cur_seg = None
        self.if_stack = []
        self.ended = False
        self._jump_idx = 0
        self._moved = []
        self._run_pass(lines, is_final=True)
        # every reference used pass 1's addresses: a label the final pass
        # puts elsewhere (an instruction sized differently between the
        # passes) means a wrong binary, so it's an error, not silent
        if self._moved:
            name, was, now, where = self._moved[0]
            raise AsmError(f"label '{name}' moved between passes ({was:#06x} -> {now:#06x}; "
                           f"{len(self._moved)} label(s) in all): an instruction before it assembled to a "
                           f"different size in the final pass (an assembler bug)", *where)

    def _relax_jumps(self, lines, max_iters=50):
        """Iterate pass 1 until every SHORT_JUMP_MNEMONICS occurrence's
        jump_long decision is stable.

        Growing a jump to its long form only ever *increases* the
        distance other jumps have to cover (the long forms are strictly
        bigger than the short ones they replace), never decreases it --
        so jump_long only ever grows monotonically across iterations and
        this is guaranteed to converge; max_iters is just a sanity cap in
        case a future change breaks that invariant.
        """
        for _ in range(max_iters):
            self.jump_sites = []
            self._jump_idx = 0
            self.symtab = {}
            self.segs = {}
            self.seg_order = []
            self.loc = {}
            self.cur_seg = None
            self.if_stack = []
            self.ended = False
            self.pending = set()
            self._run_pass(lines, is_final=False)
            if not self._grow_out_of_range_jumps():
                return
        raise AsmError(f"jump-size relaxation did not converge after {max_iters} iterations "
                        "(this would be an assembler bug, not a source issue)")

    def _grow_out_of_range_jumps(self):
        """Re-check every recorded short-jump site against the symtab
        this (now-complete) tentative pass just resolved. Returns True if
        any site needed growing (so the caller should re-run pass 1)."""
        grown = False
        for site in self.jump_sites:
            idx = site['idx']
            if self.jump_long.get(idx, False):
                continue
            ctx = ExprCtx(self.symtab, site['addr'], site['filename'], site['lineno'])
            target = eval_text(site['operand_text'], ctx).n
            rel = target - (site['addr'] + 2)
            if not (-128 <= rel <= 127):
                self.jump_long[idx] = True
                grown = True
        return grown

    def _run_pass(self, lines, is_final):
        self.tentative = not is_final
        for filename, lineno, text in lines:
            if self.ended:
                break
            for stmt_text in split_statements(text):
                if self.ended:
                    break
                self._do_statement(stmt_text, filename, lineno, is_final)

    def _ectx(self, filename, lineno, addr=None):
        return ExprCtx(self.symtab, addr if addr is not None else self.cur_addr(),
                        filename, lineno, tentative=self.tentative, pending=self.pending,
                        presize=self.presize, prekind=self.prekind, preseg=self.preseg,
                        lengths=self.lengths)

    _CAST_PREFIX_RE = re.compile(r'^\s*(byte|word)\s+ptr\b', re.IGNORECASE)
    _OFFSET_KEYWORD_RE = re.compile(r'\boffset\b', re.IGNORECASE)
    # "offset A - offset B" (parenthesized or not): a distance, a plain
    # number -- as the final evaluation makes it (expr.py, docs/bugs.md
    # #137); prescanning it as an address sized the instruction using it
    # differently in the tentative pass than in the final one
    _DISTANCE_RE = re.compile(r'^\s*\(?\s*offset\s+[\w$@?]+\s*\)?\s*-\s*\(?\s*offset\s+[\w$@?]+\s*\)?\s*$',
                              re.IGNORECASE)
    # A cast's remaining text is a *bare* reference (just one optionally
    # dot-prefixed identifier, e.g. "aret" in "lret equ byte ptr aret")
    # rather than an arithmetic expression (e.g. "p_parent + word" in
    # PD.DEF's "p_cns equ byte ptr p_parent + word") -- see this method's
    # own is_addr comment below for why that distinction matters.
    _BARE_REF_RE = re.compile(r'^\s*\.?[A-Za-z_][A-Za-z0-9_]*\s*$')

    def _prescan_sizes(self, lines):
        """Learn every DB/DW/RB/RW/EQU symbol's kind+size (and, for the
        former, which segment it'll be declared under) before pass 1, so
        a forward reference to it still classifies correctly (mem vs
        imm), gets the right size, and -- for ESEG labels specifically,
        see parse_operand's own comment on ESEG -- gets the right
        default segment override too (see ExprCtx.presize/prekind/
        preseg). `prescan_seg` tracks CSEG/DSEG/ESEG/ASEG switches the
        same way `switch_seg` does during a real pass, just without
        touching any other assembler state."""
        prescan_seg = None
        for filename, lineno, text in lines:
            for stmt_text in split_statements(text):
                label, mnem, operand_text = parse_statement(stmt_text, ALL_MNEMONICS)
                if mnem is not None:
                    low = mnem.lower()
                    if low in ('cseg', 'dseg', 'eseg', 'aseg'):
                        prescan_seg = low
                if label is None or mnem is None:
                    continue
                low = mnem.lower()
                if low in ('db', 'byte', 'rb', 'rs'):
                    self.presize[label.lower()] = 1
                    self.prekind[label.lower()] = ('label', 1, True)
                    self.preseg[label.lower()] = prescan_seg
                elif low in ('dw', 'word', 'rw'):
                    self.presize[label.lower()] = 2
                    self.prekind[label.lower()] = ('label', 2, True)
                    self.preseg[label.lower()] = prescan_seg
                elif low == 'dd':
                    self.presize[label.lower()] = 4
                    self.prekind[label.lower()] = ('label', 4, True)
                    self.preseg[label.lower()] = prescan_seg
                elif low == 'equ':
                    has_offset = (self._OFFSET_KEYWORD_RE.search(operand_text)
                                  or operand_text.strip() == '$')
                    m = self._CAST_PREFIX_RE.match(operand_text)
                    if m:
                        size = 1 if m.group(1).lower() == 'byte' else 2
                        # is_addr mirrors Symbol.is_addr's own rule (see
                        # expr.py): a cast of a *bare* label reference
                        # (e.g. "lret equ byte ptr aret") denotes a real
                        # address, same as an "offset"-rooted expression;
                        # a cast of an *arithmetic* expression (e.g.
                        # PD.DEF's "p_cns equ byte ptr p_parent + word",
                        # a structure-field OFFSET CONSTANT) does not.
                        # This is only a forward-reference guess (no real
                        # symbol table to check "aret" against yet) --
                        # the final pass always re-evaluates the real
                        # expression and gets the authoritative answer;
                        # this just has to *size* the tentative encoding
                        # the same way the final one will.
                        rest = self._CAST_PREFIX_RE.sub('', operand_text, count=1)
                        is_addr = bool(has_offset or self._BARE_REF_RE.match(rest))
                    elif self._DISTANCE_RE.match(operand_text):
                        size = None
                        is_addr = False
                    elif has_offset:
                        # "label equ offset <expr>" / "label equ $" (and
                        # arithmetic on these, e.g. SYSDAT.FMT's own
                        # "netmod equ (offset $)" or PARSE.SUP's "ndelims
                        # equ (offset $)-(offset delims)") always yield a
                        # 16-bit word value -- an address or an address
                        # difference, both within one 64KB segment --
                        # same as an explicit `word ptr` cast would.
                        # Without this, a forward reference to such a
                        # label (e.g. a `callf dword ptr .netmod`-style
                        # indirect far call, SUPIF.SUP) prescans as
                        # size=None, misclassifying the operand as an
                        # immediate instead of a memory operand during
                        # the tentative pass -- confirmed by testing:
                        # that encoded differently (and shorter) than
                        # the real, final encoding once the symbol
                        # actually resolved, throwing off every later
                        # address in the file by that one instruction's
                        # size difference -- the jump-size relaxation
                        # pass never caught this because it only watches
                        # jump *ranges* going out of bounds, not
                        # instruction sizes being unstable between
                        # passes in general.
                        size = 2
                        is_addr = True
                    else:
                        size = None
                        is_addr = False
                    self.prekind[label.lower()] = ('equ', size, is_addr)

    def _do_statement(self, stmt_text, filename, lineno, is_final):
        label, mnem, operand_text = parse_statement(stmt_text, ALL_MNEMONICS)
        low = mnem.lower() if mnem else None

        # Conditional assembly is tracked even while "inactive" so ENDIF/IF
        # nesting stays correct, but everything else is skipped.
        if low == 'if':
            ctx = self._ectx(filename, lineno)
            cond = True
            if self.active():
                cond = bool(eval_text(operand_text, ctx).n)
            self.if_stack.append(cond)
            return
        if low == 'endif':
            if not self.if_stack:
                raise AsmError("ENDIF without matching IF", filename, lineno)
            self.if_stack.pop()
            return
        if not self.active():
            return
        if label is None and mnem is None:
            return

        if low in ('eject', 'page', 'title', 'name', 'public', 'extrn', 'global'):
            return

        if low == 'end':
            self.ended = True
            return

        size_hint = None
        if low in ('db', 'byte', 'rb', 'rs'):
            size_hint = 1
        elif low in ('dw', 'word', 'rw'):
            size_hint = 2
        elif low == 'dd':
            size_hint = 4

        if label is not None and low != 'equ':
            self.define_symbol(label, 'label', self.cur_addr(), size_hint, filename, lineno,
                                allow_redefine=True)

        if low is None:
            return

        if low == 'equ':
            ctx = self._ectx(filename, lineno)
            v = eval_text(operand_text, ctx)
            # is_addr tells parse_operand this EQU's own value is a genuine
            # address -- either via "offset $"/"offset LABEL" (v.is_offset)
            # or a bare alias of a real label (v.symbol.kind == 'label',
            # e.g. "lret equ byte ptr aret") -- apart from a structure-field
            # offset constant that merely carries a byte/word/dword ptr
            # cast for sizing a *bracketed* use (e.g. "p_cns equ byte ptr
            # p_parent + word"). See Symbol.is_addr's own comment in
            # expr.py.
            is_addr = v.is_offset or (v.symbol is not None and v.symbol.kind == 'label')
            self.define_symbol(label, 'equ', v.n, v.size, filename, lineno, allow_redefine=True,
                                is_addr=is_addr)
            return

        if low == 'cseg':
            self.switch_seg('cseg')
            return
        if low == 'dseg':
            self.switch_seg('dseg')
            return
        if low == 'aseg':
            self.switch_seg('aseg')
            return
        if low == 'eseg':
            # ASM86's third real segment type (alongside CSEG/DSEG),
            # used by UDA.FMT for the User Data Area, which lives at
            # the ES-relative offset documented in its own header
            # comment ("the Extra Segment register points to the
            # beginning of the User Data Area") -- not a directive
            # this assembler recognized at all before. Without this,
            # "eseg" fell through to being parsed as a harmless bare
            # label, and the "org 0" immediately following it (see
            # UDA.FMT) then reset whatever segment was *actually*
            # active at that point -- corrupting the start of CSEG in
            # BDOS.A86/MEM.A86/CIO.A86 (all three include UDA.FMT),
            # confirmed by their assembled output literally starting
            # with the zero bytes UDA.FMT's own RB reservations wrote
            # over the real "JMP init / JMP entry" header with.
            self.switch_seg('eseg')
            return

        if low == 'org':
            ctx = self._ectx(filename, lineno)
            v = eval_text(operand_text, ctx)
            if self.cur_seg is None:
                self.switch_seg('cseg')
            self.loc[self.cur_seg] = v.n
            if label is not None:
                self.define_symbol(label, 'label', v.n, size_hint, filename, lineno, allow_redefine=True)
            return

        if low in ('db', 'byte', 'dw', 'word', 'dd', 'rb', 'rs', 'rw'):
            # Track each declaration's total emitted byte length, keyed
            # by its label, for ASM86's LENGTH operator (see
            # expr.py/eval_text's leading-LENGTH handling) -- e.g.
            # ROM.A86's "er0 rb length cer0", where cer0 is a DB string
            # declared elsewhere in the same file.
            addr_before = self.cur_addr()
            if low in ('db', 'byte'):
                self._do_db(operand_text, filename, lineno)
            elif low in ('dw', 'word'):
                self._do_dw(operand_text, filename, lineno)
            elif low == 'dd':
                self._do_dd(operand_text, filename, lineno)
            elif low in ('rb', 'rs'):
                # DRI ASM86: 'rs' reserves N raw bytes, the same as 'rb'
                # -- e.g. DATA.BDO's "endlist rs 0" is a zero-size marker
                # label, exactly the 'rb 0' idiom used elsewhere here.
                ctx = self._ectx(filename, lineno)
                n = eval_text(operand_text, ctx).n
                self.emit(bytes(n))
            else:  # rw
                ctx = self._ectx(filename, lineno)
                n = eval_text(operand_text, ctx).n
                self.emit(bytes(n * 2))
            if label is not None:
                self.lengths[label.lower()] = self.cur_addr() - addr_before
            return

        # Otherwise: a CPU instruction.
        self._do_instruction(mnem, operand_text, filename, lineno, is_final)

    def _do_db(self, operand_text, filename, lineno):
        out = bytearray()
        for part in split_top_commas(operand_text):
            part = part.strip()
            if len(part) >= 2 and part.startswith("'") and part.endswith("'"):
                out.extend(part[1:-1].replace("''", "'").encode('latin-1'))  # '' = one quote
            else:
                v = eval_text(part, self._ectx(filename, lineno, self.cur_addr() + len(out)))
                out.append(v.n & 0xFF)
        self.emit(bytes(out))

    def _do_dw(self, operand_text, filename, lineno):
        out = bytearray()
        for part in split_top_commas(operand_text):
            part = part.strip()
            v = eval_text(part, self._ectx(filename, lineno, self.cur_addr() + len(out)))
            out += struct.pack('<H', v.n & 0xFFFF)
        self.emit(bytes(out))

    def _do_dd(self, operand_text, filename, lineno):
        # DD (used once in this tree, LDMPM.A86's "mpm dd abs_zero", a
        # placeholder immediately overlaid by two DWs via "org offset
        # mpm") emits a 4-byte far pointer: offset word, then segment
        # word. Like JMPF/CALLF elsewhere in this assembler, the segment
        # half is always written as 0000 -- this tree never resolves a
        # real segment value at single-file assembly time (see
        # "Design choices that trade size for simplicity" in
        # tools/README.md).
        out = bytearray()
        for part in split_top_commas(operand_text):
            part = part.strip()
            v = eval_text(part, self._ectx(filename, lineno, self.cur_addr() + len(out)))
            out += struct.pack('<HH', v.n & 0xFFFF, 0)
        self.emit(bytes(out))

    def _do_instruction(self, mnem, operand_text, filename, lineno, is_final):
        m = mnem.lower()
        addr = self.cur_addr()
        ectx = EncodeCtx(addr, filename, lineno, tentative=self.tentative)

        if m in isa.SHORT_JUMP_MNEMONICS:
            idx = self._jump_idx
            self._jump_idx += 1
            ectx.jump_long = self.jump_long.get(idx, False)
            if self.tentative:
                # Target isn't evaluated here -- a forward reference may
                # not be defined yet at this point in a linear pass 1
                # scan. _grow_out_of_range_jumps() re-evaluates this text
                # once the whole pass (and so the symtab) is complete.
                self.jump_sites.append({
                    'idx': idx, 'addr': addr, 'operand_text': operand_text,
                    'filename': filename, 'lineno': lineno,
                })

        if m in isa.REP_PREFIXES:
            inner_label, inner_mnem, inner_rest = parse_statement(operand_text, ALL_MNEMONICS)
            if inner_mnem is None:
                raise AsmError(f"'{mnem}' requires a string-instruction operand", filename, lineno)
            ectx.rest_mnemonic = inner_mnem
            ectx.rest_operands = self._parse_operands(inner_mnem, inner_rest, ectx)
            data = isa.encode(m, [], ectx)
        else:
            operands = self._parse_operands(mnem, operand_text, ectx)
            data = isa.encode(m, operands, ectx)
        self.emit(data)

    def _parse_operands(self, mnem, operand_text, ectx):
        # JMPF/CALLF are in BRANCH_MNEMONICS (they do transfer control),
        # but unlike every other entry there, their operand is never a
        # relative branch-target label -- it's either a far pointer in
        # memory ("jmpf dword ptr mem", or a bare `dd`-declared label
        # like LDMPM.A86's own "jmpf mpm") or a direct seg:off immediate.
        # branch_context's "bare label -> relative imm target" shortcut
        # (parse_operand, expr.py) doesn't apply here and must not take
        # over a `dword ptr <label>`/bare-dd-label operand before the
        # normal mem-operand resolution gets a chance to -- confirmed by
        # testing in QEMU: it silently produced a *direct* far jump
        # (opcode 0xEA) to a hardcoded, wrong target (the label's own
        # address, segment 0) instead of the intended indirect jump
        # through the far pointer stored there.
        branch = mnem.lower() in isa.BRANCH_MNEMONICS and mnem.lower() not in ('jmpf', 'callf')
        ctx = self._ectx(ectx.filename, ectx.lineno, ectx.addr)
        ops = []
        for part in split_top_commas(operand_text):
            ops.append(parse_operand(part, ctx, branch_context=branch))
        return ops

    # -- output ---------------------------------------------------------
    def segment_bytes(self, name):
        seg = self.segs.get(name, {})
        if not seg:
            return b''
        size = max(seg) + 1
        buf = bytearray(size)
        for off, b in seg.items():
            buf[off] = b
        return bytes(buf)
