"""Code generator: PL/M AST -> ASM86-dialect text (fed to tools/asm86).

Calling convention (PL/M-86's: DRI's MCD.A86 mon1..mon4 stub, which
every utility is linked with, reads `func` from [bp+6] and `info` from
[bp+4] -- docs/bugs.md #115):
  - Arguments are pushed left to right, so the last declared parameter
    ends up at [bp+4] and the first deepest (old BP at [bp+0], return
    address at [bp+2]).
  - Every parameter and local variable occupies a full 16-bit stack slot,
    regardless of its declared BYTE/ADDRESS/POINTER type.
  - The callee pops its own parameters (RET imm16, which tools/asm86
    already supports).
  - The return value (BYTE or ADDRESS/POINTER) comes back in AX.

Expression codegen is a simple stack machine: every sub-expression's
value ends up in AX; binary operators push the left operand, evaluate
the right into AX, pop the left into BX, then combine.
"""
from . import ast
from .errors import PlmError

TRUE = 0xFFFF
FALSE = 0x0000


def type_size(t):
    """bytes a scalar of PL/M type t occupies: a POINTER (the COMPACT
    model's, which every utility here uses) is offset then segment"""
    return 1 if t == 'byte' else 4 if t == 'pointer' else 2


def sanitize(name):
    # PL/M identifiers are already '$'-stripped alnum/_,  but prefix them
    # so they can never collide with an ASM86 mnemonic or directive when
    # used as a bare data/code label in the emitted assembly.
    return 'pl_' + name.lower()


class Sym:
    __slots__ = ('kind', 'info')

    def __init__(self, kind, **info):
        self.kind = kind   # 'global', 'local', 'param', 'proc', 'const'
        self.info = info


class Scope:
    def __init__(self, parent=None):
        self.parent = parent
        self.vars = {}
        # True for a procedure's own local scope (params+locals), False
        # for the single global scope. Used to count static-link hops
        # between nested procedures -- see CodeGen._frame_reg_for_scope.
        self.is_proc_scope = False

    def define(self, name, sym):
        self.vars[name.lower()] = sym

    def lookup(self, name):
        s = self
        while s is not None:
            sym = s.vars.get(name.lower())
            if sym is not None:
                return sym
            s = s.parent
        return None


class CodeGen:
    def __init__(self, filename):
        self.filename = filename
        self.global_scope = Scope()
        self.cseg = []   # list of asm text lines
        self.dseg = []
        self.label_n = 0
        self.static_locals = set()  # labels of INITIAL/DATA locals already emitted
        self.proc_stack = []  # (local_scope, next_local_offset:list[int])
        self.extern_procs = set()

    def err(self, msg):
        raise PlmError(msg, self.filename)

    def new_label(self, prefix='L'):
        self.label_n += 1
        return f"{prefix}{self.label_n}"

    # -- top level ------------------------------------------------------
    MAIN_PROC_NAME = 'plmstart'

    # The base page: the first 100h bytes of a program's data segment,
    # which the loader fills in (default FCBs, command tail, ...). DRI's
    # MCD.A86 (orig/mpm862sr/07/MCD.A86, the startup module every
    # utility is linked with) declares these at fixed offsets and makes
    # them PUBLIC; a module's EXTERNAL declaration of one of these names
    # refers to that storage. Module data starts at 100h, past it.
    BASE_PAGE = {'bdisk': 0x04, 'maxb': 0x06, 'cmdrv': 0x50, 'pass0': 0x51, 'len0': 0x53,
                 'pass1': 0x54, 'len1': 0x56, 'fcb': 0x5C, 'fcba': 0x5C, 'fcb16': 0x6C,
                 'cr': 0x7C, 'rr': 0x7D, 'ro': 0x7F, 'buff': 0x80, 'tbuff': 0x80, 'buffa': 0x80}
    # MCD.A86 gives a PL/M program 256 bytes of stack, but PL/M keeps a
    # procedure's locals static, while this compiler puts them on the
    # stack: PIP's deepest call chain needs ~460 bytes (docs/bugs.md #121).
    STACK_BYTES = 1024

    def compile_module(self, module):
        # MEMORY is a PL/M-86 compiler-predeclared identifier, not
        # something the programmer ever DECLAREs: an implicit BYTE array
        # whose address is wherever this module's own static data ends,
        # i.e. the start of whatever memory is still free at runtime.
        # ATTACH.PLM never uses it, but PIP.PLM and ED.PLM both reference
        # a never-declared "MEMORY" (and PIP.PLM even uses it as an
        # AT(...) target) in exactly this way, confirming it's a real
        # compiler builtin rather than a source bug. Implemented here as
        # an ordinary global whose label is emitted as the very last
        # thing in dseg (see to_asm_text), after every other global
        # *and* every string/array constant interned while compiling
        # procedure bodies, so its address really is "the end of
        # everything this module statically allocated."
        self.global_scope.define('memory', Sym('global', label='pl_memory', size=1, fields=None,
                                                 is_array=True, elemsize=1, count=None,
                                                 external=False))
        # PL/M modules are conventionally wrapped as "modname: DO; ... END
        # modname;" -- a labeled block, which is purely structural when it
        # holds only declarations/procedures. But PL/M also allows real
        # executable statements to sit directly alongside declarations at
        # that same level (sometimes under the classic "PLMSTART:" entry
        # label, sometimes with no wrapping DO at all) -- so this walks
        # the whole top level once, in order, splitting it into hoisted
        # declarations/procedures and one ordered "main" statement list
        # (collected from everywhere, recursing through any block that
        # isn't pure declarations), which becomes one synthesized,
        # implicit, public, parameterless procedure: PL/M-86's own
        # PLMSTART entry-point convention.
        decls, main_stmts = self._flatten_toplevel(module.statements)
        # Three sub-passes so a global's INITIAL/AT(...) can forward-
        # reference another global declared later in the file (a real,
        # observed pattern -- see tools/plm/README.md): first register
        # every plain global's symbol (label/size/fields, no data yet),
        # then every AT(...)-aliased global (which needs another global's
        # label to already be registered), then emit every global's
        # actual DB/DW/RB/RW content now that all symbols exist.
        var_decls = [s for s in decls if isinstance(s, ast.VarDecl)]
        # GOTO: the main program's labels have to be known before any
        # procedure is generated, since a procedure may GOTO one (PIP's
        # error handler does "GO TO RETRY") -- see gen_Goto.
        main_decl = [st for st in decls if isinstance(st, ast.ProcDecl) and not st.external
                     and st.name.lower() == self.MAIN_PROC_NAME]
        main_body = main_decl[0].body if main_decl else main_stmts
        self.main_labels = {n: f"{sanitize(self.MAIN_PROC_NAME)}_L_{n}"
                            for n in self._collect_labels(main_body)}
        self.main_nl = set()
        self.main_done = False
        for stmt in var_decls:
            self.declare_or_skip_toplevel(stmt, phase=1)
        for stmt in var_decls:
            self.declare_or_skip_toplevel(stmt, phase=2)
        for stmt in decls:
            if isinstance(stmt, ast.ProcDecl):
                self.declare_or_skip_toplevel(stmt, phase=1)
        for stmt in var_decls:
            self.declare_or_skip_toplevel(stmt, phase=3)
        for stmt in decls:
            if isinstance(stmt, ast.ProcDecl) and not stmt.external:
                self.gen_proc(stmt)
        has_main = any(isinstance(st, ast.ProcDecl) and st.name.lower() == self.MAIN_PROC_NAME
                       and not st.external for st in decls)
        self.has_main = has_main or any(not isinstance(st, ast.LabelMarker) for st in main_stmts)
        if has_main:
            # the module defines PLMSTART itself; its top-level labels
            # (the module name) mark nothing executable
            main_stmts = [st for st in main_stmts if not isinstance(st, ast.LabelMarker)]
            if main_stmts:
                self.err("executable top-level statements alongside a PLMSTART procedure")
        if main_stmts:
            synth = ast.ProcDecl(name=self.MAIN_PROC_NAME, params=[], rettype=None,
                                  external=False, public=True, body=main_stmts)
            self.global_scope.define(self.MAIN_PROC_NAME,
                                      Sym('proc', rettype=None, nparams=0, external=False))
            self.gen_proc(synth)

    def _flatten_toplevel(self, stmts):
        decls = []
        main = []
        for stmt in stmts:
            if isinstance(stmt, (ast.VarDecl, ast.ProcDecl, ast.Empty)):
                decls.append(stmt)
            elif isinstance(stmt, ast.LabelMarker) and stmt.name.lower() == self.MAIN_PROC_NAME:
                continue
            elif isinstance(stmt, ast.Block):
                if stmt.label is not None and stmt.label.lower() != self.MAIN_PROC_NAME:
                    main.append(ast.LabelMarker(name=stmt.label))
                d2, m2 = self._flatten_toplevel(stmt.body)
                decls.extend(d2)
                main.extend(m2)
            else:
                main.append(stmt)
        return decls, main

    def declare_or_skip_toplevel(self, stmt, phase):
        if isinstance(stmt, ast.VarDecl):
            if stmt.type == 'label':
                # "DECLARE name LABEL [PUBLIC];" is just a forward-
                # reference note for a label defined later (see
                # _flatten_toplevel's PLMSTART handling) -- no storage,
                # no separate symbol, in any phase.
                return
            self.define_global_var(stmt, phase)
        elif isinstance(stmt, ast.ProcDecl):
            if phase != 1:
                return
            self.global_scope.define(stmt.name, Sym('proc', rettype=stmt.rettype,
                                                      nparams=len(stmt.params),
                                                      psizes=self._param_sizes(stmt),
                                                      external=stmt.external))
            if stmt.external:
                self.extern_procs.add(stmt.name)
        elif isinstance(stmt, ast.Empty):
            pass
        else:
            self.err(f"unsupported top-level statement: {type(stmt).__name__}")

    # -- globals ----------------------------------------------------------
    # Globals are processed in 3 phases (see compile_module) so a global's
    # INITIAL/AT(...) can forward-reference another global declared later
    # in the file -- a real pattern in this tree (STSRSP.PLM's "sts$qd"
    # initializes a field with the address of "sts$qd$buf", declared
    # right after it). Phase 1 registers plain globals (symbol only, no
    # dseg data yet). Phase 2 registers AT(...)-aliased globals (which
    # need another global's label, so must come after phase 1 completes
    # for everything). Phase 3 emits every global's actual DB/DW/RB/RW
    # content, now that every symbol -- forward or not -- is registered.
    def define_global_var(self, decl, phase):
        name = decl.name
        if decl.type == 'literally':
            if phase != 1:
                return
            try:
                from .lexer import parse_number
                val = parse_number(decl.literally)
            except ValueError:
                self.err(f"LITERALLY '{decl.literally}' for {name} is not a simple "
                         f"number -- unsupported text substitution")
            self.global_scope.define(name, Sym('const', value=val))
            return
        if decl.based_on is not None:
            if phase != 1:
                return
            if decl.type == 'structure':
                fields, total, _ = self._build_struct_fields(decl.fields)
                self.global_scope.define(name, Sym('based', based_on=decl.based_on,
                                                     type='structure', fields=fields, size=total))
            else:
                self.global_scope.define(name, Sym('based', based_on=decl.based_on, type=decl.type))
            return
        at_target = getattr(decl, 'at_target', None)
        if at_target is not None:
            if phase != 2:
                return
            # "AT(expr)" overlays this variable's storage directly onto
            # the address expr computes -- not indirection through a
            # pointer (that's BASED), a flat alias of the same bytes. No
            # new dseg storage is reserved; expr must be resolvable at
            # compile time (a plain "@var", ".var(constant-index)", or a
            # LITERALLY macro expanding to one of those).
            label, off = self._resolve_at_target(at_target)
            if decl.type == 'structure':
                fields, foff, _ = self._build_struct_fields(decl.fields)
                self.global_scope.define(name, Sym('global', label=label, size=foff, fields=fields,
                                                     is_array=decl.dims is not None, elemsize=foff,
                                                     external=False, label_offset=off))
            else:
                size = type_size(decl.type)
                self.global_scope.define(name, Sym('global', label=label, size=size, fields=None,
                                                     is_array=decl.dims is not None, elemsize=size,
                                                     count=None, external=False, label_offset=off,
                                                     type=decl.type))
            return
        if phase == 2:
            return
        if decl.external and name.lower() in self.BASE_PAGE:
            if phase == 1:
                size = 1 if decl.type == 'byte' else 2
                self.global_scope.define(name, Sym('global', label=f"pl_bp_{name.lower()}", size=size,
                                                     fields=None, is_array=decl.dims is not None,
                                                     elemsize=size, count=None, external=True))
            return
        if decl.type == 'structure':
            fields, total, field_layout = self._build_struct_fields(decl.fields)
            count = self.const_eval(decl.dims) if decl.dims not in (None, '*') else 1
            label = sanitize(name)
            if phase == 1:
                self.global_scope.define(name, Sym('global', label=label, size=total, fields=fields,
                                                     is_array=decl.dims is not None, elemsize=total,
                                                     external=decl.external))
                return
            # phase 3: emit the actual data now that every symbol exists.
            initial = getattr(decl, 'initial', None)
            if initial is not None:
                sizes = self._match_struct_initial(name, field_layout, initial, count)
                self._emit_initial_values(label, sizes, initial)
            else:
                # EXTERNAL data still gets storage reserved *in this
                # module*: there is no linker, so a real cross-module
                # EXTERNAL can't be resolved anyway (see tools/README.md)
                # -- this at least keeps the generated assembly
                # self-consistent and assemblable.
                self.dseg.append(f"{label}\trb\t{total * count}")
            return
        size = type_size(decl.type)
        label = sanitize(name)
        initial = getattr(decl, 'initial', None)
        if phase == 1:
            if decl.dims == '*':
                if initial is None:
                    self.err(f"'{name}': (*) array needs an INITIAL/DATA list to size it from")
                count = self._initial_total_bytes(initial, size) // size
            else:
                count = self.const_eval(decl.dims) if decl.dims is not None else None
            self.global_scope.define(name, Sym('global', label=label, size=size, fields=None,
                                                is_array=decl.dims is not None, elemsize=size,
                                                count=count, external=decl.external, type=decl.type))
            return
        # phase 3
        if decl.dims == '*' or initial is not None:
            self._emit_initial_values(label, [size] * len(initial), initial)
        else:
            count = self.const_eval(decl.dims) if decl.dims is not None else None
            unit = 'rb' if size == 1 else 'rw'
            self.dseg.append(f"{label}\t{unit}\t{(count or 1) * (2 if size == 4 else 1)}")

    def _emit_initial_values(self, label, sizes, initial):
        """Emit one DB/DW directive per INITIAL slot (sizes[i] bytes each,
        1 or 2), using the actual label only on the first. A string slot
        renders as a quoted DB string (asm86 expands it to its bytes
        itself); an address-of slot renders as "offset label[+n]" text,
        since that value isn't known until assembly; anything else must
        fold to a compile-time constant number."""
        first = True
        for size, expr in zip(sizes, initial):
            tag = f"{label}\t" if first else "\t"
            first = False
            if isinstance(expr, ast.Str):
                text = "'" + expr.value.replace("'", "''") + "'"
                self.dseg.append(f"{tag}db\t{text}")
            elif isinstance(expr, ast.AddrOf):
                tlabel, toff = self._resolve_at_target(expr)
                tok = f"offset {tlabel}+{toff}" if toff else f"offset {tlabel}"
                self.dseg.append(f"{tag}{'dw' if size == 2 else 'db'}\t{tok}")
            else:
                v = self.const_eval(expr) & (0xFFFF if size == 2 else 0xFF)
                self.dseg.append(f"{tag}{'dw' if size == 2 else 'db'}\t{v}")

    def _initial_total_bytes(self, initial, elem_size):
        total = 0
        for expr in initial:
            if isinstance(expr, ast.Str):
                total += len(expr.value.encode('latin-1'))
            else:
                total += elem_size
        return total

    def _match_struct_initial(self, name, field_layout, initial, outer_count):
        """Walk field_layout (repeated outer_count times, for an array of
        structures) against the actual INITIAL list, consuming the right
        number of entries per field: normally one entry per scalar
        element, but a field that's an array may instead be covered by a
        *single* string entry for the whole field at once (e.g.
        ABORT.PLM's "pname (8) byte ... initial (...,'        ')" -- one
        string, not 8 separate values -- versus ASS86.PLM's
        "op$val (2) byte initial (72, 1, ...)" -- one value per byte).
        Returns the per-consumed-entry size list _emit_initial_values
        needs (same length as `initial` if everything lines up)."""
        sizes = []
        idx = 0
        n = len(initial)
        for _ in range(outer_count):
            for fname, fsize, fcount, is_array in field_layout:
                if is_array and idx < n and isinstance(initial[idx], ast.Str):
                    sizes.append(fsize)
                    idx += 1
                else:
                    for _ in range(fcount):
                        if idx >= n:
                            break
                        sizes.append(fsize)
                        idx += 1
        if idx != n:
            self.err(f"structure '{name}': INITIAL list doesn't match its fields "
                      f"(consumed {idx} of {n} values)")
        return sizes

    def _build_struct_fields(self, field_decls):
        """field_decls: decl.fields (list of (name, dims, type)) ->
        (fields dict for Sym, total size in bytes, ordered layout list
        of (name, size, count, is_array), used by INITIAL emission)."""
        fields = {}
        off = 0
        layout = []
        for fname, fdims, ftyp in field_decls:
            fsize = type_size(ftyp)
            count = self.const_eval(fdims) if fdims is not None else 1
            fields[fname.lower()] = (off, fsize, fdims is not None)
            layout.append((fname, fsize, count, fdims is not None))
            off += fsize * count
        return fields, off, layout

    def _resolve_at_target(self, expr):
        """Resolve an AT(...) expression to (dseg_label, byte_offset)."""
        if isinstance(expr, ast.BinOp) and expr.op in ('+', '-') and isinstance(expr.left, ast.AddrOf):
            # "@var+2" / ".var-2": address arithmetic with a constant.
            label, off = self._resolve_at_target(expr.left)
            delta = self.const_eval(expr.right)
            return label, off + (delta if expr.op == '+' else -delta)
        if not isinstance(expr, ast.AddrOf):
            self.err("AT(...) target must be an address expression (e.g. '@var' or '.var')")
        inner = expr.expr
        if isinstance(inner, ast.Name):
            sym = self.resolve(inner.name)
            if sym.kind != 'global':
                self.err(f"AT(@{inner.name}): target must be a plain module-level variable")
            return sym.info['label'], sym.info.get('label_offset', 0)
        if isinstance(inner, ast.Call):
            sym = self.resolve(inner.name)
            if sym.kind != 'global':
                self.err(f"AT(.{inner.name}(...)): target must be a plain module-level array")
            if len(inner.args) != 1:
                self.err(f"AT(.{inner.name}(...)): expected exactly one subscript")
            idx = self.const_eval(inner.args[0])
            elemsize = sym.info.get('elemsize', 1)
            return sym.info['label'], sym.info.get('label_offset', 0) + idx * elemsize
        if isinstance(inner, ast.Field):
            base = inner.base
            if not isinstance(base, ast.Name):
                self.err("AT(...) field target's base must be a plain variable name")
            sym = self.resolve(base.name)
            fields = sym.info.get('fields')
            if fields is None or inner.field.lower() not in fields:
                self.err(f"AT(.{base.name}.{inner.field}): no such field")
            foff, _, _ = fields[inner.field.lower()]
            return sym.info['label'], sym.info.get('label_offset', 0) + foff
        if isinstance(inner, ast.Index):
            # "AT(.struct.field(index))": a subscript into a structure
            # field that's itself an array, e.g. PIP.PLM's
            # "AT(.DEST.FCB(33))" (DEST.FCB is a (34) BYTE field).
            base = inner.base
            if not isinstance(base, ast.Field) or not isinstance(base.base, ast.Name):
                self.err("AT(...) chained-index target must be struct.field(index)")
            sym = self.resolve(base.base.name)
            fields = sym.info.get('fields')
            if fields is None or base.field.lower() not in fields:
                self.err(f"AT(...): no such field '{base.field}'")
            foff, fsize, _ = fields[base.field.lower()]
            idx = self.const_eval(inner.index)
            return sym.info['label'], sym.info.get('label_offset', 0) + foff + idx * fsize
        self.err(f"unsupported AT(...) target expression: {type(inner).__name__}")

    def const_eval(self, expr):
        if isinstance(expr, ast.Num):
            return expr.value
        if isinstance(expr, ast.Name):
            sym = self.global_scope.lookup(expr.name)
            if sym is not None and sym.kind == 'const':
                return sym.info['value']
        if isinstance(expr, ast.UnOp):
            v = self.const_eval(expr.operand)
            if expr.op == '-':
                return -v & 0xFFFF
            if expr.op == 'not':
                return (~v) & 0xFFFF
        if isinstance(expr, ast.BinOp):
            l = self.const_eval(expr.left)
            r = self.const_eval(expr.right)
            ops = {'+': lambda a, b: a + b, '-': lambda a, b: a - b,
                   '*': lambda a, b: a * b, '/': lambda a, b: a // b,
                   'mod': lambda a, b: a % b, 'and': lambda a, b: a & b,
                   'or': lambda a, b: a | b, 'xor': lambda a, b: a ^ b}
            if expr.op in ops:
                return ops[expr.op](l, r) & 0xFFFF
        self.err(f"expected a compile-time constant expression, got {type(expr).__name__}")

    # -- procedures -------------------------------------------------------
    # Offset of a nested procedure's static link (its enclosing
    # procedure's BP, pushed as an extra hidden last argument) from its
    # own BP. Real parameters start right after it, at [bp+6], instead of
    # the usual [bp+4]. See _frame_reg_for_scope for how it's chased.
    LINK_OFFSET = 4

    def gen_proc(self, proc):
        # PL/M allows a PROCEDURE declared inside another one ("nested
        # procedures") whose body can reference the enclosing procedure's
        # own parameters/locals -- a real closure, not just a namespacing
        # convenience (see ERA.PLM's file$err/single$file). This compiler
        # has no runtime support library, so that's implemented the
        # classic way for a stack machine: a nested procedure takes one
        # extra hidden argument, a "static link" pointing at its
        # immediately enclosing procedure's stack frame, and a reference
        # to an outer local is reached by following however many links
        # are needed instead of indexing directly off BP (see
        # _frame_reg_for_scope). Nested procedures still get only one
        # flat (non-block-scoped) local scope per procedure, same as
        # everything else in this compiler.
        label = sanitize(proc.name)
        nested = getattr(proc, 'nested', False)
        parent_scope = proc.enclosing_scope if nested else self.global_scope
        local_scope = Scope(parent_scope)
        local_scope.is_proc_scope = True
        base_off = self.LINK_OFFSET + 2 if nested else 4
        # PL/M-86 pushes arguments left to right, so the last parameter
        # is nearest BP: [bp+base_off], the first deepest (DRI's MCD.A86
        # xdos stub relies on this: func at [bp+6], info at [bp+4]).
        # A POINTER parameter takes 4 bytes (offset nearest BP, then
        # segment -- the caller pushes DX then AX), every other one 2.
        sizes = self._param_sizes(proc)
        off = base_off
        for p, size in reversed(list(zip(proc.params, sizes))):
            local_scope.define(p, Sym('param', offset=off))
            off += size
        self.next_local_off = [0]
        self.cur_scope = local_scope
        self.cur_proc = proc

        # A nested PROCEDURE's own body can reference this procedure's
        # locals no matter where, textually, it's declared relative to
        # them -- so every one of this procedure's own VarDecls must be
        # registered (wherever nested in its body) *before* any nested
        # PROCEDURE within it is hoisted out and generated.
        proc.body = self._register_vardecls(proc.body)
        # GOTO targets: this procedure's own labels, collected before its
        # nested procedures are generated (they may GOTO out to one).
        # A label reached by a non-local GOTO gets an entry that resets
        # SP to this frame's base (see _emit_label); the frame size it
        # needs is only known after the body, hence the token.
        is_main = proc.name.lower() == self.MAIN_PROC_NAME
        if is_main:
            local_scope.labels, local_scope.nl_labels = self.main_labels, self.main_nl
        else:
            local_scope.labels = {n: f"{label}_L_{n}" for n in self._collect_labels(proc.body)}
            local_scope.nl_labels = set()
        local_scope.frame_token = f"@@frame_{label}@@"
        next_local_off = self.next_local_off
        proc.body = self._hoist_procs(proc.body, local_scope)
        # A nested proc's own gen_proc call (just above) overwrote these;
        # restore them to resume generating *this* procedure's own body.
        # next_local_off too: without it this procedure's frame was sized
        # by its last nested procedure's locals, so SHOW's printbcd
        # reserved none and the call to its nested pchar pushed over its
        # loop index (docs/bugs.md #120).
        self.cur_scope = local_scope
        self.cur_proc = proc
        self.next_local_off = next_local_off

        body_lines = []
        self.out = body_lines
        for stmt in proc.body:
            self.gen_stmt(stmt)
        local_bytes = -self.next_local_off[0]
        body_lines = [l.replace(local_scope.frame_token, str(local_bytes)) for l in body_lines]

        self.cseg.append(f"{label}:")
        self.cseg.append("\tpush bp")
        self.cseg.append("\tmov bp,sp")
        if local_bytes:
            self.cseg.append(f"\tsub sp,{local_bytes}")
        if is_main:
            self.main_done = True
            if self.main_nl:
                self.dseg.append("pl_main_bp\tdw\t0")
                self.cseg.append("\tmov pl_main_bp,bp")
        self.cseg.extend(body_lines)
        self.cseg.append(f"{label}_exit:")
        if local_bytes:
            self.cseg.append("\tmov sp,bp")
        self.cseg.append("\tpop bp")
        popbytes = sum(sizes)
        if nested:
            popbytes += 2  # the static link this procedure itself took
        self.cseg.append(f"\tret {popbytes}" if popbytes else "\tret")

    def _register_vardecls(self, stmts):
        """Register every VarDecl's symbol/storage in a statement list
        (and any nested block/if/while/case/for bodies within it -- but
        *not* inside a nested ProcDecl, which registers its own when its
        own gen_proc runs) into the current scope. A plain declaration
        produces no code and is removed from the list entirely -- one
        with an INITIAL/DATA value too, since that's static storage (see
        _declare_var)."""
        out = []
        for s in stmts:
            if isinstance(s, ast.VarDecl):
                self._declare_var(s)
                continue
            out.append(self._register_vardecls_in_stmt(s))
        return out

    def _register_vardecls_in_stmt(self, s):
        if isinstance(s, (ast.Block, ast.WhileBlock, ast.ForBlock, ast.CaseBlock)):
            s.body = self._register_vardecls(s.body)
        elif isinstance(s, ast.If):
            if s.then_stmt is not None:
                s.then_stmt = self._register_vardecls_in_stmt(s.then_stmt)
            if s.else_stmt is not None:
                s.else_stmt = self._register_vardecls_in_stmt(s.else_stmt)
        return s

    def _hoist_procs(self, stmts, enclosing_scope):
        """Pull every ProcDecl out of a statement list (and any nested
        block/if/while/case/for bodies within it), generating each as an
        ordinary routine (tagged as nested, taking enclosing_scope's
        frame as its static link). A nested PROCEDURE...END is purely a
        definition -- it produces no inline code at its point of
        declaration, only at a CALL elsewhere -- so it's simply removed
        from the list it was found in."""
        out = []
        for s in stmts:
            if isinstance(s, ast.ProcDecl):
                s.nested = True
                s.enclosing_scope = enclosing_scope
                self.global_scope.define(s.name, Sym('proc', rettype=s.rettype,
                                                       nparams=len(s.params), external=s.external,
                                                       psizes=self._param_sizes(s),
                                                       nested=True, enclosing_scope=enclosing_scope))
                if s.external:
                    self.extern_procs.add(s.name)
                else:
                    self.gen_proc(s)
                continue
            out.append(self._hoist_procs_in_stmt(s, enclosing_scope))
        return out

    def _hoist_procs_in_stmt(self, s, enclosing_scope):
        if isinstance(s, (ast.Block, ast.WhileBlock, ast.ForBlock, ast.CaseBlock)):
            s.body = self._hoist_procs(s.body, enclosing_scope)
        elif isinstance(s, ast.If):
            if s.then_stmt is not None:
                s.then_stmt = self._hoist_procs_in_stmt(s.then_stmt, enclosing_scope)
            if s.else_stmt is not None:
                s.else_stmt = self._hoist_procs_in_stmt(s.else_stmt, enclosing_scope)
        return s

    def _frame_reg_for_scope(self, target):
        """Returns the register ('bp', or 'bx' after emitting code to
        load it) holding the frame pointer for the procedure that owns
        `target` scope, chasing static links from the current
        procedure's own frame as needed."""
        if target is None or target is self.cur_scope:
            return 'bp'
        s = self.cur_scope
        hops = 0
        while s is not None and s is not target:
            if s.is_proc_scope:
                hops += 1
            s = s.parent
        if s is None:
            self.err("internal: cannot resolve an enclosing procedure's frame")
        self.emit(f"\tmov bx,[bp+{self.LINK_OFFSET}]")
        for _ in range(hops - 1):
            self.emit(f"\tmov bx,[bx+{self.LINK_OFFSET}]")
        return 'bx'

    def _frame_reg_for(self, sym):
        return self._frame_reg_for_scope(sym.info.get('_scope'))

    def alloc_local(self, name, words=1, **extra):
        self.next_local_off[0] -= 2 * words
        off = self.next_local_off[0]
        self.cur_scope.define(name, Sym('local', offset=off, **extra))
        return off

    # -- statements ---------------------------------------------------------
    def emit(self, line):
        self.out.append(line)

    def gen_stmt(self, s):
        if isinstance(s, (ast.Block, ast.WhileBlock, ast.CaseBlock, ast.ForBlock)) and s.label:
            if s.label.lower() in getattr(self.cur_scope, 'labels', {}):
                self._emit_label(s.label.lower())
        t = type(s).__name__
        getattr(self, f"gen_{t}", self._unsupported)(s)

    def _param_sizes(self, proc):
        """Each parameter's stack size, from the DECLAREs in the
        procedure's body that type it (4 for a POINTER, else 2). Needed
        before the body is generated, and at every call site: until
        docs/bugs.md #122 every parameter took 2 bytes and a call pushed
        only AX, so a POINTER argument lost its segment."""
        names = {p.lower() for p in proc.params}
        types = {}

        def walk(stmts):
            for st in stmts:
                if isinstance(st, ast.VarDecl) and st.name.lower() in names:
                    types[st.name.lower()] = st.type
                elif isinstance(st, ast.Block) and st.label is None:
                    walk(st.body)
        walk(proc.body or [])
        return [4 if types.get(p.lower()) == 'pointer' else 2 for p in proc.params]

    def _collect_labels(self, stmts):
        """Every label defined in a statement list, not counting those
        inside nested procedures (which have their own)."""
        out = []

        def walk(n):
            if isinstance(n, list):
                for x in n:
                    walk(x)
                return
            if n is None or isinstance(n, ast.ProcDecl):
                return
            if isinstance(n, ast.LabelMarker):
                out.append(n.name.lower())
                return
            if isinstance(n, (ast.Block, ast.WhileBlock, ast.CaseBlock, ast.ForBlock)) and n.label:
                out.append(n.label.lower())
            for attr in ('body', 'then_stmt', 'else_stmt'):
                if isinstance(getattr(n, attr, None), (list, ast.Node)):
                    walk(getattr(n, attr))
        walk(stmts)
        dups = {n for n in out if out.count(n) > 1}
        if dups:
            self.err(f"label(s) defined more than once: {', '.join(sorted(dups))}")
        return out

    def _emit_label(self, key):
        scope = self.cur_scope
        asm = scope.labels[key]
        if key in scope.nl_labels:
            # entered by a non-local GOTO with BP already this frame's:
            # drop whatever the abandoned procedures left on the stack
            self.emit(f"\tjmp {asm}")
            self.emit(f"{asm}_nl:")
            self.emit("\tmov sp,bp")
            self.emit(f"\tsub sp,{scope.frame_token}")
        self.emit(f"{asm}:")

    def gen_Goto(self, s):
        """A GOTO within this procedure is a JMP. One to a label in an
        enclosing procedure (reached through the static links) or in the
        main program (whose BP it saves in pl_main_bp) also restores that
        frame: BP here, SP at the label's _nl entry."""
        key = s.name.lower()
        if key in getattr(self.cur_scope, 'labels', {}):
            self.emit(f"\tjmp {self.cur_scope.labels[key]}")
            return
        t = self.cur_scope.parent
        while t is not None:
            if t.is_proc_scope and key in getattr(t, 'labels', {}):
                reg = self._frame_reg_for_scope(t)
                self.emit(f"\tmov bp,{reg}")
                t.nl_labels.add(key)
                self.emit(f"\tjmp {t.labels[key]}_nl")
                return
            t = t.parent
        if key in self.main_labels:
            if self.main_done:
                self.err(f"GOTO {s.name}: main program already generated")
            self.emit("\tmov bp,pl_main_bp")
            self.main_nl.add(key)
            self.emit(f"\tjmp {self.main_labels[key]}_nl")
            return
        self.err(f"GOTO {s.name}: no such label")

    def _unsupported(self, s):
        self.err(f"unsupported construct: {type(s).__name__}")

    def _declare_local_at(self, s, at_target):
        """A local "AT(@x)": the same bytes as x, no storage of its own.
        STSCOM's display$text steps a POINTER parameter through a string
        with "src$ptr structure(offset word, segment word) at (@source)";
        given fresh stack storage instead, the parameter never moved and
        STSCMD printed its first character forever (docs/bugs.md #122).
        A target in this procedure's frame is aliased by offset; anything
        else resolves like a module-level AT."""
        inner = at_target.expr if isinstance(at_target, ast.AddrOf) else None
        tsym = self.cur_scope.vars.get(inner.name.lower()) if isinstance(inner, ast.Name) else None
        if tsym is not None and tsym.kind in ('param', 'local', 'local_struct'):
            off = tsym.info['offset']
            if s.type == 'structure':
                fields, foff = {}, 0
                for fname, fdims, ftyp in s.fields:
                    fsize = type_size(ftyp)
                    fields[fname.lower()] = (foff, fsize, fdims is not None)
                    foff += fsize * (self.const_eval(fdims) if fdims is not None else 1)
                self.cur_scope.define(s.name, Sym('local_struct', offset=off, fields=fields, size=foff))
            elif s.dims is not None:
                size = type_size(s.type)
                self.cur_scope.define(s.name, Sym('local', offset=off, is_array=True, elemsize=size,
                                                    count=self.const_eval(s.dims)))
            else:
                self.cur_scope.define(s.name, Sym('local', offset=off, type=s.type))
            return
        label, off = self._resolve_at_target(at_target)
        if s.type == 'structure':
            fields, foff, _ = self._build_struct_fields(s.fields)
            self.cur_scope.define(s.name, Sym('global', label=label, size=foff, fields=fields,
                                                is_array=s.dims is not None, elemsize=foff,
                                                external=False, label_offset=off))
        else:
            size = type_size(s.type)
            self.cur_scope.define(s.name, Sym('global', label=label, size=size, fields=None,
                                                is_array=s.dims is not None, elemsize=size,
                                                count=None, external=False, label_offset=off,
                                                type=s.type))

    def gen_Empty(self, s):
        pass

    def gen_LabelMarker(self, s):
        if s.name.lower() in getattr(self.cur_scope, 'labels', {}):
            self._emit_label(s.name.lower())
        else:
            self.emit(f"{sanitize(s.name)}:")

    def _declare_var(self, s):
        """Register a local's symbol/storage -- no code emitted. Called
        early (see _register_vardecls) so a nested procedure's closure
        references resolve correctly regardless of textual order."""
        if s.type == 'literally':
            try:
                from .lexer import parse_number
                val = parse_number(s.literally)
            except ValueError:
                self.err(f"LITERALLY '{s.literally}' is not a simple number")
            self.cur_scope.define(s.name, Sym('const', value=val))
            return
        existing = self.cur_scope.vars.get(s.name.lower())
        if existing is not None and existing.kind == 'param':
            # "PROCEDURE (char); DECLARE char BYTE;" -- a parameter's
            # type declaration, not a new local
            existing.info['type'] = s.type
            return
        at_target = getattr(s, 'at_target', None)
        if at_target is not None:
            self._declare_local_at(s, at_target)
            return
        if s.based_on is not None:
            if s.type == 'structure':
                fields, total, _ = self._build_struct_fields(s.fields)
                self.cur_scope.define(s.name, Sym('based', based_on=s.based_on,
                                                    type='structure', fields=fields, size=total))
            else:
                self.cur_scope.define(s.name, Sym('based', based_on=s.based_on, type=s.type))
            return
        if s.type == 'structure':
            if getattr(s, 'initial', None) is not None:
                self.err(f"'{s.name}': INITIAL on a local STRUCTURE is not supported")
            fields = {}
            off = 0
            for fname, fdims, ftyp in s.fields:
                fsize = type_size(ftyp)
                count = self.const_eval(fdims) if fdims is not None else 1
                fields[fname.lower()] = (off, fsize, fdims is not None)
                off += fsize * count
            # local structures: allocate one word slot per 2 bytes (rounded)
            words = (off + 1) // 2
            base_off = None
            for _ in range(words):
                base_off = self.alloc_local(f"__struct_{s.name}_{_}")
            first_off = self.next_local_off[0]  # lowest address used
            self.cur_scope.define(s.name, Sym('local_struct', offset=first_off, fields=fields, size=off))
            return
        size = 1 if s.type == 'byte' else 2
        initial = getattr(s, 'initial', None)
        if initial is not None:
            # A local with INITIAL or DATA is static in PL/M: one copy,
            # initialized at load. (It used to be copied onto the stack
            # each time its declaration was reached -- PIP's ERROR has
            # ~450 bytes of DATA message tables, which ran off the end of
            # the 256-byte stack into the program's strings: docs/bugs.md
            # #121.) Storage is in dseg under a per-procedure label, so
            # ".name" in another DATA list is a constant address.
            label = f"{sanitize(self.cur_proc.name)}_s_{s.name.lower()}"
            if s.dims == '*':
                count = self._initial_total_bytes(initial, size) // size
            else:
                count = self.const_eval(s.dims) if s.dims is not None else None
            self.cur_scope.define(s.name, Sym('global', label=label, size=size, fields=None,
                                                is_array=s.dims is not None, elemsize=size,
                                                count=count, external=False, type=s.type))
            if label not in self.static_locals:
                self.static_locals.add(label)
                self._emit_initial_values(label, [size] * len(initial), initial)
            return
        if s.dims == '*':
            if initial is None:
                self.err(f"'{s.name}': (*) array needs an INITIAL/DATA list to size it from")
            count = self._initial_total_bytes(initial, size) // size
            words = (size * count + 1) // 2
            self.alloc_local(s.name, words, is_array=True, elemsize=size, count=count)
        elif s.dims is not None:
            count = self.const_eval(s.dims)
            words = (size * count + 1) // 2
            self.alloc_local(s.name, words, is_array=True, elemsize=size, count=count)
        else:
            self.alloc_local(s.name, 2 if s.type == 'pointer' else 1, type=s.type)

    def gen_Block(self, s):
        for st in s.body:
            self.gen_stmt(st)

    def gen_If(self, s):
        else_l = self.new_label('elif')
        end_l = self.new_label('endif')
        self.gen_bool_jump_false(s.cond, else_l if s.else_stmt is not None else end_l)
        self.gen_stmt(s.then_stmt)
        if s.else_stmt is not None:
            self.emit(f"\tjmp {end_l}")
            self.emit(f"{else_l}:")
            self.gen_stmt(s.else_stmt)
        self.emit(f"{end_l}:")

    def gen_WhileBlock(self, s):
        top = self.new_label('wtop')
        end = self.new_label('wend')
        self.emit(f"{top}:")
        self.gen_bool_jump_false(s.cond, end)
        for st in s.body:
            self.gen_stmt(st)
        self.emit(f"\tjmp {top}")
        self.emit(f"{end}:")

    def gen_ForBlock(self, s):
        var_sym = self.cur_scope.lookup(s.var)
        if var_sym is None:
            self.err(f"DO loop variable '{s.var}' is not declared")
        top = self.new_label('ftop')
        end = self.new_label('fend')
        self.gen_expr(s.start)
        self.store_to(var_sym)
        self.emit(f"{top}:")
        self.gen_expr(s.stop)
        self.emit("\tpush ax")
        self.load_from(var_sym)
        self.emit("\tpop bx")
        self.emit("\tcmp ax,bx")          # var > stop: done
        self.emit_jcc_far('ja', end)
        for st in s.body:
            self.gen_stmt(st)
        self.load_from(var_sym)
        if s.step is not None:
            self.emit("\tpush ax")
            self.gen_expr(s.step)
            self.emit("\tpop bx")
            self.emit("\tadd ax,bx")
        else:
            self.emit("\tadd ax,1")
        self.store_to(var_sym)
        self.emit(f"\tjmp {top}")
        self.emit(f"{end}:")

    def gen_CaseBlock(self, s):
        end = self.new_label('cend')
        arms = [self.new_label('case') for _ in s.body]
        self.gen_expr(s.selector)
        self.emit("\tmov bx,ax")
        self.emit("\tadd bx,bx")  # word-sized jump table entries
        tbl = self.new_label('ctbl')
        # the jump table lives in cseg (DW of the arm labels), so it has to
        # be read through CS: a bare "jmp tbl[bx]" reads DS:tbl, which is
        # some other program's data whenever DS <> CS (docs/bugs.md #121)
        self.emit(f"\tjmp cs:{tbl}[bx]")
        self.cseg_tables = getattr(self, 'cseg_tables', [])
        self.cseg_tables.append((tbl, arms))
        for arm_label, stmt in zip(arms, s.body):
            self.emit(f"{arm_label}:")
            self.gen_stmt(stmt)
            self.emit(f"\tjmp {end}")
        self.emit(f"{end}:")

    def gen_Assign(self, s):
        self.gen_expr(s.expr)
        for tgt in s.targets:
            self.emit("\tpush ax")
            self.store_lvalue(tgt)
            self.emit("\tpop ax")

    def gen_CallStmt(self, s):
        self.gen_call(s.name, s.args)

    def gen_Return(self, s):
        if s.expr is not None:
            self.gen_expr(s.expr)
        self.emit(f"\tjmp {sanitize(self.cur_proc.name)}_exit")

    # -- boolean short-circuit for IF/WHILE conditions ---------------------
    REL_JFALSE = {'=': 'jne', '<>': 'je', '<': 'jae', '<=': 'ja', '>': 'jbe', '>=': 'jb'}
    # Inverse of each Jcc above (used by emit_jcc_far's invert-and-JMP trick).
    JCC_INVERSE = {'je': 'jne', 'jne': 'je', 'jb': 'jae', 'jae': 'jb', 'ja': 'jbe', 'jbe': 'ja',
                   'jc': 'jnc', 'jnc': 'jc', 'js': 'jns', 'jns': 'js', 'jp': 'jnp', 'jnp': 'jp'}

    # PL/M's pre-defined flag pseudo-variables (confirmed real:
    # orig/plm80s/PLMLANG.DOC -- "CARRY, ZERO, SIGN, PARITY ... reflect
    # the current values of the cpu flags"), each mapped to the Jcc
    # mnemonic that's true exactly when that flag is set.
    FLAG_VARS = {'carry': 'jc', 'zero': 'je', 'sign': 'js', 'parity': 'jp'}

    def emit_jcc_far(self, jcc, target_label):
        """Emit a conditional jump to target_label with unlimited range.
        The 8086 Jcc opcodes only have an 8-bit displacement, but a
        procedure's body is routinely longer than that, so every
        conditional branch in this compiler's output goes through this:
        invert the condition, jump (short -- always nearby) over an
        unconditional JMP (near, full 16-bit range) to the real target."""
        skip = self.new_label('jlong')
        self.emit(f"\t{self.JCC_INVERSE[jcc]} {skip}")
        self.emit(f"\tjmp {target_label}")
        self.emit(f"{skip}:")

    def gen_bool_jump_false(self, cond, false_label):
        if isinstance(cond, ast.Name) and cond.name.lower() in self.FLAG_VARS:
            # A flag pseudo-variable (CARRY/ZERO/SIGN/PARITY) reads the
            # 8086 flag as last set by the previous arithmetic --
            # MOV/PUSH/POP (which is all this compiler emits between
            # statements) never touch flags, so this is safe to check
            # directly, with no intervening instruction to disturb it.
            jcc = self.FLAG_VARS[cond.name.lower()]
            self.emit_jcc_far(self.JCC_INVERSE[jcc], false_label)
            return
        if isinstance(cond, ast.BinOp) and cond.op in self.REL_JFALSE:
            self.gen_expr(cond.left)
            self.emit("\tpush ax")
            self.gen_expr(cond.right)
            self.emit("\tpop bx")
            self.emit("\txchg ax,bx")
            self.emit("\tcmp ax,bx")
            self.emit_jcc_far(self.REL_JFALSE[cond.op], false_label)
            return
        # general case: PL/M tests the low bit of the value (TRUE is
        # 0FFh, or any odd value; a relational result is 0FFFFh/0)
        self.gen_expr(cond)
        self.emit("\ttest al,1")
        self.emit_jcc_far('je', false_label)

    # -- expressions --------------------------------------------------------
    def gen_expr(self, e):
        t = type(e).__name__
        getattr(self, f"ex_{t}", self._ex_unsupported)(e)

    def _ex_unsupported(self, e):
        self.err(f"unsupported expression: {type(e).__name__}")

    # -- expression types ---------------------------------------------------
    # PL/M has two value types here: BYTE and ADDRESS (WORD/INTEGER/POINTER
    # are 16-bit like ADDRESS). A constant below 256 is a BYTE; an
    # operation on two BYTEs is a BYTE (8-bit result: "cnt = -1" with a
    # BYTE cnt compares against 0FFh, and 0FFh + 1 is 0); anything with
    # an ADDRESS operand is an ADDRESS (the BYTE zero-extended). Every
    # value is computed in AX; a BYTE result is truncated (AH = 0).
    def expr_type(self, e):
        if isinstance(e, ast.Num):
            return 'byte' if 0 <= e.value < 256 else 'address'
        if isinstance(e, ast.Str):
            return 'byte' if len(e.value) == 1 else 'address'
        if isinstance(e, ast.AddrOf):
            return 'pointer' if getattr(e, 'far', False) else 'address'
        if isinstance(e, ast.InlineArray):
            return 'address'
        if isinstance(e, ast.UnOp):
            return self.expr_type(e.operand)
        if isinstance(e, ast.BinOp):
            if e.op in self.REL_JFALSE:
                return 'byte'
            if e.op in ('*', '/'):
                # "Multiply and divide always produce ADDRESS results"
                # (orig/plm80s/PLMLANG.DOC): PIP's "128 * shr(exten,4)"
                # records-read count came out 0 as a BYTE product, and
                # every multi-sector copy lost its last partial group
                # (docs/bugs.md #121)
                return 'address'
            return 'byte' if self.expr_type(e.left) == self.expr_type(e.right) == 'byte' else 'address'
        if isinstance(e, ast.AssignExpr):
            return self.expr_type(e.expr)
        if isinstance(e, ast.Field):
            return {1: 'byte', 4: 'pointer'}.get(self._field_size(e), 'address')
        if isinstance(e, ast.Index):
            return 'byte' if self._index_elem_size(e) == 1 else 'address'
        if isinstance(e, ast.Name):
            low = e.name.lower()
            if low in self.FLAG_VARS:
                return 'byte'
            if low == 'stackptr':
                return 'address'
            sym = self.cur_scope.lookup(e.name)
            if sym is None:
                return 'address'
            if sym.kind == 'proc':
                return {'byte': 'byte', 'pointer': 'pointer'}.get(sym.info.get('rettype'), 'address')
            return self._sym_type(sym)
        if isinstance(e, ast.Call):
            low = e.name.lower()
            if low in self.BUILTINS and self._user_proc(e.name) is None:
                if low in ('low', 'high', 'dec'):
                    return 'byte'
                if low in ('shl', 'shr', 'rol', 'ror', 'scl', 'scr'):
                    return self.expr_type(e.args[0])
                return 'address'
            sym = self.cur_scope.lookup(e.name)
            if sym is None:
                return 'address'
            if sym.kind == 'proc':
                return {'byte': 'byte', 'pointer': 'pointer'}.get(sym.info.get('rettype'), 'address')
            if sym.kind == 'based':
                # a BASED array element, e.g. SET's fcbv(i): typed by its
                # declaration -- it has no elemsize/size, and taking it
                # as ADDRESS made rol(fcbv(i),1) a 16-bit rotate
                # (docs/bugs.md #120)
                return self._sym_type(sym)
            return 'byte' if sym.info.get('elemsize', sym.info.get('size', 2)) == 1 else 'address'
        return 'address'

    def _sym_type(self, sym):
        if sym.kind == 'const':
            return 'byte' if 0 <= sym.info['value'] < 256 else 'address'
        if sym.kind in ('param', 'local'):
            return {'byte': 'byte', 'pointer': 'pointer'}.get(sym.info.get('type'), 'address')
        if sym.kind == 'global':
            if sym.info.get('fields'):
                return 'address'
            return {1: 'byte', 4: 'pointer'}.get(sym.info.get('size'), 'address')
        if sym.kind == 'based':
            return 'byte' if sym.info.get('type') == 'byte' else 'address'
        return 'address'

    def ex_Num(self, e):
        self.emit(f"\tmov ax,{e.value}")

    def ex_Str(self, e):
        # A single-character string literal used bare (not under '.') is a
        # BYTE constant (its ASCII code) -- e.g. "fcb16(1) = ' '" -- while
        # a longer one decays to the address of its interned text, as a
        # PL/M string constant passed where an ADDRESS is expected.
        if len(e.value) == 1:
            self.emit(f"\tmov ax,{ord(e.value) & 0xFF}")
            return
        label = self.intern_string(e.value)
        self.emit(f"\tmov ax,offset {label}")

    def intern_string(self, text):
        label = self.new_label('str')
        data = ','.join(str(b) for b in text.encode('latin-1')) or '0'
        self.dseg.append(f"{label}\tdb\t{data}")
        return label

    def ex_Name(self, e):
        low = e.name.lower()
        if low in self.FLAG_VARS:
            jcc = self.FLAG_VARS[low]
            t_l = self.new_label('flagt')
            e_l = self.new_label('flage')
            self.emit(f"\t{jcc} {t_l}")
            self.emit(f"\tmov ax,{FALSE}")
            self.emit(f"\tjmp {e_l}")
            self.emit(f"{t_l}:")
            self.emit(f"\tmov ax,{TRUE}")
            self.emit(f"{e_l}:")
            return
        if low == 'stackptr':
            self.emit("\tmov ax,sp")
            return
        sym = self.resolve(e.name)
        if sym.kind == 'const':
            self.emit(f"\tmov ax,{sym.info['value']}")
            return
        if sym.kind == 'proc':
            self.gen_call(e.name, [])
            return
        self.load_from(sym)

    def ex_AddrOf(self, e):
        far = self.gen_addr(e.expr)
        if getattr(e, 'far', False) and not far:
            self.emit("\tmov dx,ds")      # @x: a POINTER, DS:x

    def ex_InlineArray(self, e):
        parts = []
        for el in e.elements:
            if isinstance(el, ast.Str):
                parts.extend(el.value.encode('latin-1'))
            elif isinstance(el, ast.Num):
                parts.append(el.value & 0xFF)
            else:
                self.err("'.(' inline array ')' elements must be constants or strings")
        label = self.new_label('arr')
        self.dseg.append(f"{label}\tdb\t{','.join(str(b) for b in parts)}")
        self.emit(f"\tmov ax,offset {label}")

    def ex_Field(self, e):
        far = self.gen_field_addr(e)
        if far:
            self.emit("\tmov es,dx")
        self.emit("\tmov bx,ax")
        self._load_at_bx(self._field_size(e), far)

    def ex_Index(self, e):
        far = self.gen_index2_addr(e)
        if far:
            self.emit("\tmov es,dx")
        self.emit("\tmov bx,ax")
        self._load_at_bx(self._index_elem_size(e), far)

    def _index_elem_size(self, e):
        return self._field_size(e.base) if isinstance(e.base, ast.Field) else 1

    def gen_index2_addr(self, e):
        """Address of base(e)[index] where base is an arbitrary addressable
        expression (not a plain named array) -- e.g. the second "(k)" in
        "dir$entries(j).file(k)"."""
        elemsize = self._index_elem_size(e)
        far = self.gen_addr(e.base)
        if far:
            self.emit("\tpush dx")
        self.emit("\tpush ax")
        self.gen_expr(e.index)
        if elemsize != 1:
            self.emit(f"\tmov cx,{elemsize}")
            self.emit("\tmul cx")
        self.emit("\tpop bx")
        self.emit("\tadd ax,bx")
        if far:
            self.emit("\tpop dx")
        return far

    def ex_Call(self, e):
        if e.name.lower() in self.BUILTINS:
            self.gen_call(e.name, e.args)
            return
        sym = self.resolve(e.name)
        if sym.kind in ('global', 'local', 'local_struct', 'param', 'based') and sym.kind != 'proc':
            self.gen_index_load(e)
            return
        self.gen_call(e.name, e.args)

    def ex_UnOp(self, e):
        self.gen_expr(e.operand)
        if e.op == 'not':
            self.emit("\tnot ax")
        elif e.op == '-':
            self.emit("\tneg ax")
        if self.expr_type(e) == 'byte':
            self.emit("\tmov ah,0")

    def ex_AssignExpr(self, e):
        self.gen_expr(e.expr)
        self.emit("\tpush ax")
        self.store_lvalue(e.target)
        self.emit("\tpop ax")

    BIN_SIMPLE = {'+': 'add', '*': None, 'and': 'and', 'or': 'or', 'xor': 'xor'}

    def ex_BinOp(self, e):
        if e.op in self.REL_JFALSE:
            true_l = self.new_label('rtrue')
            end_l = self.new_label('rend')
            jfalse = self.REL_JFALSE[e.op]
            self.gen_expr(e.left)
            self.emit("\tpush ax")
            self.gen_expr(e.right)
            self.emit("\tpop bx")
            self.emit("\txchg ax,bx")
            self.emit("\tcmp ax,bx")
            self.emit(f"\t{jfalse} {end_l}")
            self.emit(f"\tmov ax,{TRUE}")
            self.emit(f"\tjmp {true_l}")
            self.emit(f"{end_l}:")
            self.emit(f"\tmov ax,{FALSE}")
            self.emit(f"{true_l}:")
            return
        self.gen_expr(e.left)
        self.emit("\tpush ax")
        self.gen_expr(e.right)
        self.emit("\tpop bx")
        if e.op == '+':
            self.emit("\tadd ax,bx")
        elif e.op == '-':
            self.emit("\txchg ax,bx")
            self.emit("\tsub ax,bx")
        elif e.op == '*':
            self.emit("\tmul bx")
        elif e.op == '/':
            self.emit("\txchg ax,bx")
            self.emit("\tmov dx,0")
            self.emit("\tdiv bx")
        elif e.op == 'mod':
            self.emit("\txchg ax,bx")
            self.emit("\tmov dx,0")
            self.emit("\tdiv bx")
            self.emit("\tmov ax,dx")
        elif e.op == 'and':
            self.emit("\tand ax,bx")
        elif e.op == 'or':
            self.emit("\tor ax,bx")
        elif e.op == 'xor':
            self.emit("\txor ax,bx")
        else:
            self.err(f"unsupported operator '{e.op}'")
        if self.expr_type(e) == 'byte':
            self.emit("\tmov ah,0")

    # -- calls --------------------------------------------------------------
    BUILTINS = {'move', 'movw', 'movb', 'dec', 'length', 'last', 'size', 'low', 'high', 'shl',
                'shr', 'rol', 'ror', 'scl', 'scr', 'signed', 'double', 'unsign', 'time'}

    def gen_call(self, name, args):
        low = name.lower()
        # A user-declared procedure always wins over a builtin name --
        # SET.PLM declares its own parameterless "time: procedure;",
        # wholly unrelated to PL/M's standard TIME builtin (see below);
        # this check is what lets both coexist correctly.
        if low in self.BUILTINS and self._user_proc(name) is not None:
            low = None
        if low == 'move':
            return self.gen_move_builtin(args)
        if low == 'movw':
            return self.gen_movw_builtin(args)
        if low == 'movb':
            return self.gen_movb_builtin(args)
        if low == 'dec':
            # Confirmed, not a guess: Intel's own PL/M-80 language
            # reference (orig/plm80s/PLMLANG.DOC, a direct ancestor of
            # PL/M-86) documents "DEC(expr1 + expr2) and
            # DEC(expr1 PLUS expr2)" verbatim -- "does the necessary
            # decimal adjustment to produce the BCD result from the
            # addition", i.e. exactly the 8086 DAA instruction, used
            # right after a BCD addition. PIP.PLM's one use,
            # "C1 = DEC(C1+ONE)", matches this exactly (C1/C2/C3 are
            # packed-BCD digit counters -- PRINTDIG prints their two
            # nibbles separately).
            if len(args) != 1:
                self.err("DEC requires 1 argument: DEC(value)")
            self.gen_expr(args[0])
            self.emit("\tdaa")
            return
        if low == 'time':
            # Confirmed real: orig/plm80s/PLMLANG.DOC documents
            # "CALL TIME(expression)" verbatim -- "delays 100
            # microseconds times the value" on the original i8080
            # target. ED.PLM's one use is a 20-iteration loop each
            # calling TIME(250), commented "1/2 SECOND TIME OUT":
            # 250 * 100us * 20 = 500ms, an exact match. That exact
            # i8080-cycle-based timing can't be replicated on this
            # compiler's 8086 target (different CPU, and the real
            # duration depends on the host's actual clock speed
            # regardless), so this is a busy-wait loop scaled by the
            # argument -- correct in spirit (it genuinely delays
            # proportionally to the argument), not calibrated to an
            # exact wall-clock duration.
            if len(args) != 1:
                self.err("TIME requires 1 argument: CALL TIME(n)")
            self.gen_expr(args[0])
            self.emit("\tmov cx,ax")
            outer = self.new_label('timeout')
            inner = self.new_label('timein')
            done = self.new_label('timedone')
            self.emit("\tjcxz " + done)
            self.emit(f"{outer}:")
            self.emit("\tpush cx")
            self.emit("\tmov cx,100")
            self.emit(f"{inner}:")
            self.emit(f"\tloop {inner}")
            self.emit("\tpop cx")
            self.emit(f"\tloop {outer}")
            self.emit(f"{done}:")
            return
        if low in ('length', 'last', 'size'):
            return self.gen_length_builtin(low, args)
        if low in ('low', 'high'):
            self.gen_expr(args[0])
            if low == 'high':
                self.emit("\tmov al,ah")
            self.emit("\tmov ah,0")
            return
        if low in ('signed', 'double', 'unsign'):
            # All three are no-ops here. SIGNED/UNSIGN are PL/M-86's real
            # conversion functions between INTEGER (signed 16-bit) and
            # WORD/ADDRESS (unsigned) -- confirmed by STSCOM.PLM's
            # "dcl i integer; ... low(unsign(i))", converting i back to
            # unsigned before extracting its low byte. This compiler
            # never distinguishes signed from unsigned bit patterns
            # internally (every value is just a 16-bit word in AX, and
            # relational operators are always unsigned, see
            # tools/plm/README.md), so the conversion has no bits to
            # actually change either way. DOUBLE (BYTE -> ADDRESS,
            # zero-extend) is likewise already true of every loaded BYTE.
            self.gen_expr(args[0])
            return
        if low in ('shl', 'shr', 'rol', 'ror', 'scl', 'scr'):
            # SCL/SCR ("shift/rotate through carry") are confirmed real:
            # orig/plm80s/PLMLANG.DOC documents them alongside ROL/ROR as
            # "the carry flag participates in the rotate" -- exactly the
            # 8086's RCL/RCR, as opposed to ROL/ROR's plain (non-carry)
            # rotate.
            mnem = {'scl': 'rcl', 'scr': 'rcr'}.get(low, low)
            self.gen_expr(args[0])
            self.emit("\tpush ax")
            self.gen_expr(args[1])
            self.emit("\tpop bx")
            self.emit("\txchg ax,bx")       # ax = value, bx = count
            self.emit("\tmov cl,bl")
            if self.expr_type(args[0]) == 'byte':
                self.emit(f"\t{mnem} al,cl")   # an 8-bit shift/rotate
                self.emit("\tmov ah,0")
            else:
                self.emit(f"\t{mnem} ax,cl")
            return
        sym = self.resolve(name)
        if sym.kind != 'proc':
            self.err(f"'{name}' is not a procedure")
        psizes = sym.info.get('psizes') or [2] * len(args)
        for a, size in zip(args, psizes + [2] * (len(args) - len(psizes))):
            # left to right, as PL/M-86 does (see gen_proc). A POINTER
            # parameter gets DX:AX; an ADDRESS passed to one is DS-relative.
            self.gen_expr(a)
            if size == 4 and self.expr_type(a) != 'pointer':
                self.emit("\tmov dx,ds")
            self._push_value(size)
        if sym.info.get('nested'):
            # Nested procedures take one extra hidden last argument: a
            # static link to their enclosing procedure's frame (see
            # gen_proc/LINK_OFFSET). Pushed after the real arguments, so
            # it lands at [bp+4] in the callee and the real ones shift up
            # to start at [bp+6].
            reg = self._frame_reg_for_scope(sym.info['enclosing_scope'])
            self.emit(f"\tpush {reg}")
        self.emit(f"\tcall {sanitize(name)}")
        if sym.info.get('rettype') == 'byte':
            self.emit("\tmov ah,0")      # a BYTE result is AL
        elif sym.info.get('rettype') == 'pointer' and sym.info.get('external'):
            self.emit("\tmov dx,es")     # MCD's mon4: a POINTER comes back in ES:BX

    def gen_move_builtin(self, args):
        if len(args) != 3:
            self.err("MOVE requires 3 arguments: MOVE(count, source, dest)")
        count, src, dst = args
        self._gen_block_move(src, dst, count, 'movsb')

    def gen_movw_builtin(self, args):
        # MOVW isn't a declared procedure anywhere in this tree, and
        # appears exactly once -- SET.PLM's "CALL
        # MOVW(@UDA.DF$PASSWORD,@UDA$PARENT.DF$PASSWORD,4)". Its argument
        # order (source, dest, count) is the *reverse* of MOVE's
        # (count, source, dest), and its count unit is words, not bytes:
        # UDA.LIT declares DF$PASSWORD as exactly (8) BYTE, and the call
        # passes a count of 4 -- 4 words = 8 bytes, confirming the
        # hypothesis. Treated as a real (if undocumented/unconfirmed)
        # PL/M-86 builtin rather than a bug, unlike e.g. ED.PLM's `TIME`
        # (see docs/bugs.md and tools/plm/README.md for both).
        if len(args) != 3:
            self.err("MOVW requires 3 arguments: MOVW(source, dest, word_count)")
        src, dst, count = args
        self._gen_block_move(src, dst, count, 'movsw')

    def gen_movb_builtin(self, args):
        # MOVB: the byte-granularity sibling of MOVW, same (source, dest,
        # count) argument order. TOD.PLM's "CALL
        # MOVB(@EXTRNL$TOD.DATE,@LCLTOD.DATE,5)" copies the DATE/HRS/MIN/
        # SEC fields of a TOD structure (ADDRESS + 3 BYTEs = exactly 5
        # bytes) with a count of 5 -- a direct byte count, confirming
        # this one moves bytes rather than words. One of the two call
        # sites even comments "/* use pl/m-86 move */", suggesting the
        # original author considered it interchangeable with the
        # confirmed MOVE builtin, just with MOVW's reversed argument
        # convention.
        if len(args) != 3:
            self.err("MOVB requires 3 arguments: MOVB(source, dest, byte_count)")
        src, dst, count = args
        self._gen_block_move(src, dst, count, 'movsb')

    def _gen_block_move(self, src, dst, count, op):
        """rep movsb/movsw from src to dst. A POINTER operand (COMPACT
        model: `@x`, a POINTER variable, `@based.field` on one) carries
        its own segment; an ADDRESS is in DS."""
        for e in (dst, src):
            self.gen_expr(e)
            if self.expr_type(e) != 'pointer':
                self.emit("\tmov dx,ds")
            self.emit("\tpush dx")
            self.emit("\tpush ax")
        self.gen_expr(count)
        self.emit("\tmov cx,ax")
        self.emit("\tpop si")
        self.emit("\tpop bx")          # source segment
        self.emit("\tpop di")
        self.emit("\tpop es")          # destination segment
        self.emit("\tpush ds")
        self.emit("\tmov ds,bx")
        self.emit(f"\trep {op}")
        self.emit("\tpop ds")

    def gen_length_builtin(self, kind, args):
        if not args or not isinstance(args[0], ast.Name):
            self.err(f"{kind.upper()} requires a declared array name")
        sym = self.resolve(args[0].name)
        if kind == 'size':
            # Total byte size of the declaration (element size * count,
            # or just the scalar size for a non-array).
            elemsize = sym.info.get('elemsize', sym.info.get('size', 1))
            count = sym.info.get('count') or 1
            self.emit(f"\tmov ax,{elemsize * count}")
            return
        count = sym.info.get('count')
        if count is None:
            self.err(f"{kind.upper()}: '{args[0].name}' has no known array dimension")
        n = count if kind == 'length' else count - 1
        self.emit(f"\tmov ax,{n}")

    # -- lvalues / addressing ------------------------------------------------
    def _user_proc(self, name):
        """Like resolve(), but returns None instead of raising, and only
        for an actual user-declared procedure -- used to let a builtin
        name be shadowed by a real DECLARE (see gen_call)."""
        s = self.cur_scope if hasattr(self, 'cur_scope') else self.global_scope
        while s is not None:
            sym = s.vars.get(name.lower())
            if sym is not None:
                return sym if sym.kind == 'proc' else None
            s = s.parent
        return None

    def resolve(self, name):
        s = self.cur_scope if hasattr(self, 'cur_scope') else self.global_scope
        while s is not None:
            sym = s.vars.get(name.lower())
            if sym is not None:
                # Remember where this was actually found, so a reference
                # to an enclosing procedure's local/param (a closure
                # access) can tell it needs to go through a static link
                # rather than this procedure's own BP -- see
                # _frame_reg_for.
                sym.info['_scope'] = s
                return sym
            s = s.parent
        self.err(f"undefined identifier '{name}'")

    def _load_at_bx(self, size, far):
        seg = 'es:' if far else ''
        if size == 1:
            self.emit(f"\tmov al,{seg}[bx]")
            self.emit("\tmov ah,0")
        elif size == 4:
            self.emit(f"\tmov ax,{seg}[bx]")
            self.emit(f"\tmov dx,{seg}[bx+2]")
        else:
            self.emit(f"\tmov ax,{seg}[bx]")

    def _store_at_bx(self, size, far):
        seg = 'es:' if far else ''
        if size == 1:
            self.emit(f"\tmov {seg}[bx],al")
        elif size == 4:
            self.emit(f"\tmov {seg}[bx],ax")
            self.emit(f"\tmov {seg}[bx+2],dx")
        else:
            self.emit(f"\tmov {seg}[bx],ax")

    def _push_value(self, size):
        if size == 4:
            self.emit("\tpush dx")
        self.emit("\tpush ax")

    def _pop_value(self, size):
        self.emit("\tpop ax")
        if size == 4:
            self.emit("\tpop dx")

    def _frame_slot(self, reg, off):
        return f"[{reg}{off}]" if off < 0 else f"[{reg}+{off}]"

    def load_from(self, sym):
        if sym.kind in ('param', 'local') and sym.info.get('type') == 'pointer':
            reg = self._frame_reg_for(sym)
            off = sym.info['offset']
            self.emit(f"\tmov ax,{self._frame_slot(reg, off)}")
            self.emit(f"\tmov dx,{self._frame_slot(reg, off + 2)}")
            return
        if sym.kind == 'global' and sym.info.get('size') == 4 and not sym.info.get('fields'):
            self.emit(f"\tmov bx,offset {sym.info['label']}")
            if sym.info.get('label_offset', 0):
                self.emit(f"\tadd bx,{sym.info['label_offset']}")
            self._load_at_bx(4, False)
            return
        if sym.kind == 'param':
            reg = self._frame_reg_for(sym)
            self.emit(f"\tmov ax,[{reg}+{sym.info['offset']}]")
            if sym.info.get('type') == 'byte':
                self.emit("\tmov ah,0")
        elif sym.kind == 'local':
            reg = self._frame_reg_for(sym)
            off = sym.info['offset']
            self.emit(f"\tmov ax,[{reg}{off}]" if off < 0 else f"\tmov ax,[{reg}+{off}]")
            if sym.info.get('type') == 'byte':
                self.emit("\tmov ah,0")
        elif sym.kind == 'global':
            size = sym.info['size']
            label = sym.info['label']
            off = sym.info.get('label_offset', 0)
            if off:
                # A nonzero AT(...) alias offset can't be folded into a
                # bare "mov ax,label" text operand without losing its
                # memory-operand classification in asm86 (arithmetic on a
                # symbol drops its symbol-ness, see tools/plm/README.md),
                # so compute the address into BX and dereference instead.
                self.emit(f"\tmov bx,offset {label}")
                self.emit(f"\tadd bx,{off}")
                if size == 1:
                    self.emit("\tmov al,[bx]")
                    self.emit("\tmov ah,0")
                else:
                    self.emit("\tmov ax,[bx]")
            elif size == 1:
                self.emit(f"\tmov al,{label}")
                self.emit("\tmov ah,0")
            else:
                self.emit(f"\tmov ax,{label}")
        elif sym.kind == 'based':
            if sym.info['type'] == 'structure':
                self.err("cannot load a whole BASED structure as a scalar value "
                          "-- access one of its fields instead")
            far = self.load_based_addr(sym)
            if far:
                self.emit("\tmov es,dx")
            self.emit("\tmov bx,ax")
            self._load_at_bx(type_size(sym.info['type']), far)
        else:
            self.err(f"cannot read value of '{sym.kind}'")

    def store_to(self, sym):
        if sym.kind in ('param', 'local') and sym.info.get('type') == 'pointer':
            reg = self._frame_reg_for(sym)
            off = sym.info['offset']
            self.emit(f"\tmov {self._frame_slot(reg, off)},ax")
            self.emit(f"\tmov {self._frame_slot(reg, off + 2)},dx")
            return
        if sym.kind == 'global' and sym.info.get('size') == 4 and not sym.info.get('fields'):
            self.emit("\tpush ax")
            self.emit(f"\tmov bx,offset {sym.info['label']}")
            if sym.info.get('label_offset', 0):
                self.emit(f"\tadd bx,{sym.info['label_offset']}")
            self.emit("\tpop ax")
            self._store_at_bx(4, False)
            return
        if sym.kind == 'param':
            reg = self._frame_reg_for(sym)
            self.emit(f"\tmov [{reg}+{sym.info['offset']}],ax")
        elif sym.kind == 'local':
            reg = self._frame_reg_for(sym)
            off = sym.info['offset']
            self.emit(f"\tmov [{reg}{off}],ax" if off < 0 else f"\tmov [{reg}+{off}],ax")
        elif sym.kind == 'global':
            size = sym.info['size']
            label = sym.info['label']
            off = sym.info.get('label_offset', 0)
            if off:
                self.emit("\tpush ax")
                self.emit(f"\tmov bx,offset {label}")
                self.emit(f"\tadd bx,{off}")
                self.emit("\tpop ax")
                self.emit("\tmov [bx],al" if size == 1 else "\tmov [bx],ax")
            elif size == 1:
                self.emit(f"\tmov {label},al")
            else:
                self.emit(f"\tmov {label},ax")
        elif sym.kind == 'based':
            if sym.info['type'] == 'structure':
                self.err("cannot store to a whole BASED structure as a scalar value "
                          "-- assign to one of its fields instead")
            size = type_size(sym.info['type'])
            self._push_value(size)
            far = self.load_based_addr(sym)
            if far:
                self.emit("\tmov es,dx")
            self.emit("\tmov bx,ax")
            self._pop_value(size)
            self._store_at_bx(size, far)
        else:
            self.err(f"cannot assign to '{sym.kind}'")

    def load_based_addr(self, sym):
        based_on = sym.info['based_on']
        if isinstance(based_on, tuple):
            # "BASED struct.field type": the pointer value lives in that
            # structure field -- load it the same way any field read does.
            base_name, field = based_on
            fld = ast.Field(base=ast.Name(name=base_name), field=field)
            self.ex_Field(fld)
            return self._field_size(fld) == 4
        ptr_sym = self.resolve(based_on)
        self.load_from(ptr_sym)
        # BASED on a POINTER: the data is at its segment (in DX)
        return self._sym_type(ptr_sym) == 'pointer'

    def store_lvalue(self, lv):
        if isinstance(lv, ast.Name):
            if lv.name.lower() == 'stackptr':
                # Confirmed real (orig/plm80s/PLMLANG.DOC): "the variable
                # may be assigned a new value to alter the stack
                # register."
                self.emit("\tmov sp,ax")
                return
            sym = self.resolve(lv.name)
            self.store_to(sym)
        elif isinstance(lv, ast.Field):
            size = self._field_size(lv)
            self._push_value(size)
            far = self.gen_field_addr(lv)
            if far:
                self.emit("\tmov es,dx")
            self.emit("\tmov bx,ax")
            self._pop_value(size)
            self._store_at_bx(size, far)
        elif isinstance(lv, ast.Call):
            self.store_index(lv)
        elif isinstance(lv, ast.Index):
            size = self._index_elem_size(lv)
            self._push_value(size)
            far = self.gen_index2_addr(lv)
            if far:
                self.emit("\tmov es,dx")
            self.emit("\tmov bx,ax")
            self._pop_value(size)
            self._store_at_bx(size, far)
        else:
            self.err(f"unsupported assignment target: {type(lv).__name__}")

    def gen_addr(self, e):
        if isinstance(e, ast.Name):
            sym = self.resolve(e.name)
            if sym.kind == 'global':
                self.emit(f"\tmov ax,offset {sym.info['label']}")
                off = sym.info.get('label_offset', 0)
                if off:
                    self.emit(f"\tadd ax,{off}")
            elif sym.kind in ('param', 'local', 'local_struct'):
                reg = self._frame_reg_for(sym)
                off = sym.info['offset']
                self.emit(f"\tmov ax,{reg}")
                if off >= 0:
                    self.emit(f"\tadd ax,{off}")
                else:
                    self.emit(f"\tsub ax,{-off}")
            elif sym.kind == 'based':
                # The "address of" a BASED variable is just its pointer's
                # current value (used e.g. as the base of a field access,
                # "pd.cns" -> address-of(pd) + field offset) -- far, in
                # DX:AX, when that's a POINTER.
                return self.load_based_addr(sym)
            else:
                self.err(f"cannot take the address of '{e.name}'")
        elif isinstance(e, ast.Field):
            return self.gen_field_addr(e)
        elif isinstance(e, ast.Call):
            return self.gen_index_addr(e)
        elif isinstance(e, ast.Index):
            return self.gen_index2_addr(e)
        elif isinstance(e, ast.Str):
            label = self.intern_string(e.value)
            self.emit(f"\tmov ax,offset {label}")
        else:
            self.err(f"unsupported operand for '.': {type(e).__name__}")
        return False

    def gen_field_addr(self, fld):
        base = fld.base
        # base is ast.Name for a plain structure variable, or ast.Call for
        # one element of an array of structures (e.g. ERAQ.PLM's
        # "dir$entries(j).file") -- both have a plain '.name' to resolve,
        # and gen_addr (below) already knows how to compute either one's
        # address, so no further case-by-case handling is needed here.
        if not hasattr(base, 'name'):
            self.err(f"field access base must be a plain variable name or array element, "
                      f"got {type(base).__name__}")
        sym = self.resolve(base.name)
        fields = sym.info.get('fields')
        if fields is None:
            self.err(f"'{base.name}' is not a structure")
        key = fld.field.lower()
        if key not in fields:
            self.err(f"structure '{base.name}' has no field '{fld.field}'")
        off, size, _ = fields[key]
        far = self.gen_addr(base)
        if off:
            self.emit(f"\tadd ax,{off}")
        self._last_field_size = size
        return far

    def _field_size(self, fld):
        base = fld.base
        sym = self.resolve(base.name)
        off, size, _ = sym.info['fields'][fld.field.lower()]
        return size

    def gen_index_addr(self, call):
        sym = self.resolve(call.name)
        if len(call.args) != 1:
            self.err(f"'{call.name}' array indexing takes exactly one subscript")
        if sym.kind == 'based':
            # A BASED array (e.g. "fcbv BASED fcb$address (32) BYTE"): the
            # pointer's value is the base address, indexed the same way.
            if sym.info['type'] == 'structure':
                self.err(f"cannot index BASED structure '{call.name}' as an array")
            elemsize = type_size(sym.info['type'])
            far = self.load_based_addr(sym)
            if far:
                self.emit("\tpush dx")
            self.emit("\tpush ax")
            self.gen_expr(call.args[0])
            if elemsize != 1:
                self.emit(f"\tmov cx,{elemsize}")
                self.emit("\tmul cx")
            self.emit("\tpop bx")
            self.emit("\tadd ax,bx")
            if far:
                self.emit("\tpop dx")
            return far
        if sym.kind == 'local' and sym.info.get('is_array'):
            # A genuine local array ("DECLARE user(15) BYTE;"): its own
            # stack storage holds the elements directly, so the base
            # address is this local's own address (bp/static-link-
            # relative, like any other local), not a value to load.
            elemsize = sym.info.get('elemsize', 1)
            reg = self._frame_reg_for(sym)
            off = sym.info['offset']
            self.emit(f"\tmov ax,{reg}")
            self.emit(f"\tadd ax,{off}" if off >= 0 else f"\tsub ax,{-off}")
            self.emit("\tpush ax")
            self.gen_expr(call.args[0])
            if elemsize != 1:
                self.emit(f"\tmov cx,{elemsize}")
                self.emit("\tmul cx")
            self.emit("\tpop bx")
            self.emit("\tadd ax,bx")
            return False
        if sym.kind in ('param', 'local'):
            # An array parameter/local declared to hold an ADDRESS is
            # treated as a pointer when indexed (PL/M arrays are always
            # passed by reference): its value is the base address.
            self.load_from(sym)
            self.emit("\tpush ax")
            self.gen_expr(call.args[0])
            self.emit("\tpop bx")
            self.emit("\tadd ax,bx")
            return False
        if sym.kind != 'global':
            self.err(f"cannot index '{call.name}' as an array")
        elemsize = sym.info.get('elemsize', 1)
        base_off = sym.info.get('label_offset', 0)
        self.gen_expr(call.args[0])
        self.emit("\tpush ax")
        self.emit(f"\tmov ax,offset {sym.info['label']}")
        if base_off:
            self.emit(f"\tadd ax,{base_off}")
        self.emit("\tpop bx")
        if elemsize != 1:
            self.emit(f"\tmov cx,{elemsize}")
            self.emit("\txchg ax,bx")
            self.emit("\tmul cx")
            self.emit("\tadd ax,bx")
        else:
            self.emit("\tadd ax,bx")
        return False

    def _indexed_elem_size(self, sym):
        if sym.kind == 'based':
            return type_size(sym.info['type'])
        if sym.kind == 'local' and sym.info.get('is_array'):
            return sym.info.get('elemsize', 1)
        if sym.kind in ('param', 'local'):
            return 1
        return sym.info.get('elemsize', 1)

    def gen_index_load(self, call):
        sym = self.resolve(call.name)
        far = self.gen_index_addr(call)
        if far:
            self.emit("\tmov es,dx")
        self.emit("\tmov bx,ax")
        self._load_at_bx(self._indexed_elem_size(sym), far)

    def store_index(self, call):
        sym = self.resolve(call.name)
        size = self._indexed_elem_size(sym)
        self._push_value(size)
        far = self.gen_index_addr(call)
        if far:
            self.emit("\tmov es,dx")
        self.emit("\tmov bx,ax")
        self._pop_value(size)
        self._store_at_bx(size, far)

    # -- output -----------------------------------------------------------
    def to_asm_text(self):
        lines = []
        lines.append("\tcseg")
        lines.append("\torg\t0")
        if getattr(self, 'has_main', False):
            # MCD.A86's startup: the stack is in the data segment
            # (SS = DS), then PLMSTART
            lines += ["\tpushf", "\tpop ax", "\tcli", "\tmov cx,ds", "\tmov ss,cx",
                      "\tmov sp,offset pl_stack_base", "\tpush ax", "\tpopf",
                      f"\tjmp {sanitize(self.MAIN_PROC_NAME)}"]
        lines.extend(self.cseg)
        for tbl, arms in getattr(self, 'cseg_tables', []):
            lines.append(f"{tbl}:")
            for a in arms:
                lines.append(f"\tdw\toffset {a}")
        lines.append("")
        lines.append("\tdseg")
        lines.append("\torg\t0")
        seen = set()
        for name, off in sorted(self.BASE_PAGE.items(), key=lambda kv: kv[1]):
            lines.append(f"\torg\t{off:#x}")
            lines.append(f"pl_bp_{name}\trb\t0")
        lines.append("\torg\t100h")
        lines.extend(self.dseg)
        if getattr(self, 'has_main', False):
            lines.append(f"\trb\t{self.STACK_BYTES}")
            lines.append("pl_stack_base\tdb\t0")
        # MEMORY (see compile_module): its address must be past every
        # other global *and* every string/array constant interned while
        # generating procedure bodies, so this has to come after all of
        # self.dseg, not be spliced in earlier.
        lines.append("pl_memory:")
        lines.append("\tend")
        return '\n'.join(lines) + '\n'
