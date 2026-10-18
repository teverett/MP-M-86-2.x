"""Code generator: PL/M AST -> ASM86-dialect text (fed to tools/asm86).

Calling convention (defined by this compiler, not independently verified
against a historical PL/M-86 reference -- see tools/README.md):
  - Arguments are pushed right-to-left, so the first declared parameter
    ends up at [bp+4], the second at [bp+6], etc. (old BP at [bp+0],
    return address at [bp+2]).
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
        self.proc_stack = []  # (local_scope, next_local_offset:list[int])
        self.extern_procs = set()

    def err(self, msg):
        raise PlmError(msg, self.filename)

    def new_label(self, prefix='L'):
        self.label_n += 1
        return f"{prefix}{self.label_n}"

    # -- top level ------------------------------------------------------
    MAIN_PROC_NAME = 'plmstart'

    def compile_module(self, module):
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
                size = 1 if decl.type == 'byte' else 2
                self.global_scope.define(name, Sym('global', label=label, size=size, fields=None,
                                                     is_array=decl.dims is not None, elemsize=size,
                                                     count=None, external=False, label_offset=off))
            return
        if phase == 2:
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
                if len(field_layout) != len(initial):
                    self.err(f"structure '{name}' has {len(field_layout)} fields but "
                              f"INITIAL supplies {len(initial)} values")
                self._emit_initial_values(label, [f[1] for f in field_layout], initial)
            else:
                # EXTERNAL data still gets storage reserved *in this
                # module*: there is no linker, so a real cross-module
                # EXTERNAL can't be resolved anyway (see tools/README.md)
                # -- this at least keeps the generated assembly
                # self-consistent and assemblable.
                self.dseg.append(f"{label}\trb\t{total * count}")
            return
        size = 1 if decl.type == 'byte' else 2
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
                                                count=count, external=decl.external))
            return
        # phase 3
        if decl.dims == '*' or initial is not None:
            self._emit_initial_values(label, [size] * len(initial), initial)
        else:
            count = self.const_eval(decl.dims) if decl.dims is not None else None
            unit = 'rb' if size == 1 else 'rw'
            self.dseg.append(f"{label}\t{unit}\t{count or 1}")

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

    def _build_struct_fields(self, field_decls):
        """field_decls: decl.fields (list of (name, dims, type)) ->
        (fields dict for Sym, total size in bytes, ordered layout list
        of (name, size, count, is_array), used by INITIAL emission)."""
        fields = {}
        off = 0
        layout = []
        for fname, fdims, ftyp in field_decls:
            fsize = 1 if ftyp == 'byte' else 2
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
    def gen_proc(self, proc):
        # PL/M allows a PROCEDURE declared inside another one ("nested
        # procedures"). This compiler has no real lexical nesting --
        # a nested proc is hoisted out and generated as its own ordinary
        # routine (registered in the one global scope), *before* this
        # procedure's own body/locals are set up, since generating it
        # recursively reuses (and must finish with, before returning
        # here) the same self.cur_scope/self.out/self.next_local_off
        # instance state this procedure is about to set up for itself.
        proc.body = self._hoist_procs(proc.body)
        label = sanitize(proc.name)
        local_scope = Scope(self.global_scope)
        for idx, p in enumerate(proc.params):
            off = 4 + 2 * idx
            local_scope.define(p, Sym('param', offset=off))
        self.next_local_off = [0]
        self.cur_scope = local_scope
        self.cur_proc = proc
        body_lines = []
        self.out = body_lines
        for stmt in proc.body:
            self.gen_stmt(stmt)
        local_bytes = -self.next_local_off[0]

        self.cseg.append(f"{label}:")
        self.cseg.append("\tpush bp")
        self.cseg.append("\tmov bp,sp")
        if local_bytes:
            self.cseg.append(f"\tsub sp,{local_bytes}")
        self.cseg.extend(body_lines)
        self.cseg.append(f"{label}_exit:")
        if local_bytes:
            self.cseg.append("\tmov sp,bp")
        self.cseg.append("\tpop bp")
        popbytes = 2 * len(proc.params)
        self.cseg.append(f"\tret {popbytes}" if popbytes else "\tret")

    def _hoist_procs(self, stmts):
        """Pull every ProcDecl out of a statement list (and any nested
        block/if/while/case/for bodies within it), generating each as an
        ordinary routine. A nested PROCEDURE...END is purely a
        definition -- it produces no inline code at its point of
        declaration, only at a CALL elsewhere -- so it's simply removed
        from the list it was found in."""
        out = []
        for s in stmts:
            if isinstance(s, ast.ProcDecl):
                self.global_scope.define(s.name, Sym('proc', rettype=s.rettype,
                                                       nparams=len(s.params), external=s.external))
                if s.external:
                    self.extern_procs.add(s.name)
                else:
                    self.gen_proc(s)
                continue
            out.append(self._hoist_procs_in_stmt(s))
        return out

    def _hoist_procs_in_stmt(self, s):
        if isinstance(s, (ast.Block, ast.WhileBlock, ast.ForBlock, ast.CaseBlock)):
            s.body = self._hoist_procs(s.body)
        elif isinstance(s, ast.If):
            if s.then_stmt is not None:
                s.then_stmt = self._hoist_procs_in_stmt(s.then_stmt)
            if s.else_stmt is not None:
                s.else_stmt = self._hoist_procs_in_stmt(s.else_stmt)
        return s

    def alloc_local(self, name):
        self.next_local_off[0] -= 2
        off = self.next_local_off[0]
        self.cur_scope.define(name, Sym('local', offset=off))
        return off

    # -- statements ---------------------------------------------------------
    def emit(self, line):
        self.out.append(line)

    def gen_stmt(self, s):
        t = type(s).__name__
        getattr(self, f"gen_{t}", self._unsupported)(s)

    def _unsupported(self, s):
        self.err(f"unsupported construct: {type(s).__name__}")

    def gen_Empty(self, s):
        pass

    def gen_LabelMarker(self, s):
        self.emit(f"{sanitize(s.name)}:")

    def gen_VarDecl(self, s):
        if s.type == 'literally':
            try:
                from .lexer import parse_number
                val = parse_number(s.literally)
            except ValueError:
                self.err(f"LITERALLY '{s.literally}' is not a simple number")
            self.cur_scope.define(s.name, Sym('const', value=val))
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
            fields = {}
            off = 0
            for fname, fdims, ftyp in s.fields:
                fsize = 1 if ftyp == 'byte' else 2
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
        off = self.alloc_local(s.name)

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
        self.emit("\txchg ax,bx")
        self.emit("\tcmp ax,bx")
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
        self.emit(f"\tjmp {tbl}[bx]")
        # jump table lives in cseg as DW of arm labels (same segment as code)
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
                   'jc': 'jnc', 'jnc': 'jc'}

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
        if isinstance(cond, ast.Name) and cond.name.lower() == 'carry':
            # PL/M's CARRY pseudo-variable reads the 8086 carry flag as
            # last set by the previous arithmetic -- MOV/PUSH/POP (which
            # is all this compiler emits between statements) never touch
            # flags, so this is safe to check directly, with no
            # intervening instruction to disturb it.
            self.emit_jcc_far('jnc', false_label)
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
        # general case: evaluate to TRUE/FALSE word, then test
        self.gen_expr(cond)
        self.emit("\tcmp ax,0")
        self.emit_jcc_far('je', false_label)

    # -- expressions --------------------------------------------------------
    def gen_expr(self, e):
        t = type(e).__name__
        getattr(self, f"ex_{t}", self._ex_unsupported)(e)

    def _ex_unsupported(self, e):
        self.err(f"unsupported expression: {type(e).__name__}")

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
        if e.name.lower() == 'carry':
            t_l = self.new_label('carryt')
            e_l = self.new_label('carrye')
            self.emit(f"\tjc {t_l}")
            self.emit(f"\tmov ax,{FALSE}")
            self.emit(f"\tjmp {e_l}")
            self.emit(f"{t_l}:")
            self.emit(f"\tmov ax,{TRUE}")
            self.emit(f"{e_l}:")
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
        self.gen_addr(e.expr)

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
        self.gen_field_addr(e)
        self.emit("\tmov bx,ax")
        size = self._field_size(e)
        if size == 1:
            self.emit("\tmov al,[bx]")
            self.emit("\tmov ah,0")
        else:
            self.emit("\tmov ax,[bx]")

    def ex_Index(self, e):
        self.gen_index2_addr(e)
        self.emit("\tmov bx,ax")
        size = self._index_elem_size(e)
        if size == 1:
            self.emit("\tmov al,[bx]")
            self.emit("\tmov ah,0")
        else:
            self.emit("\tmov ax,[bx]")

    def _index_elem_size(self, e):
        return self._field_size(e.base) if isinstance(e.base, ast.Field) else 1

    def gen_index2_addr(self, e):
        """Address of base(e)[index] where base is an arbitrary addressable
        expression (not a plain named array) -- e.g. the second "(k)" in
        "dir$entries(j).file(k)"."""
        elemsize = self._index_elem_size(e)
        self.gen_addr(e.base)
        self.emit("\tpush ax")
        self.gen_expr(e.index)
        if elemsize != 1:
            self.emit(f"\tmov cx,{elemsize}")
            self.emit("\tmul cx")
        self.emit("\tpop bx")
        self.emit("\tadd ax,bx")

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

    # -- calls --------------------------------------------------------------
    BUILTINS = {'move', 'length', 'last', 'size', 'low', 'high', 'shl', 'shr', 'rol', 'ror',
                'signed', 'double'}

    def gen_call(self, name, args):
        low = name.lower()
        if low == 'move':
            return self.gen_move_builtin(args)
        if low in ('length', 'last', 'size'):
            return self.gen_length_builtin(low, args)
        if low in ('low', 'high'):
            self.gen_expr(args[0])
            if low == 'high':
                self.emit("\tmov al,ah")
            self.emit("\tmov ah,0")
            return
        if low in ('signed', 'double'):
            # Both are no-ops here: every value is already carried as a
            # full word in AX (BYTEs are zero-extended on load), which is
            # exactly what DOUBLE asks for; SIGNED's actual sign-aware
            # comparison semantics aren't modeled (this compiler's
            # relational operators are always unsigned, see
            # tools/plm/README.md), so it's treated as a pass-through.
            self.gen_expr(args[0])
            return
        if low in ('shl', 'shr', 'rol', 'ror'):
            self.gen_expr(args[0])
            self.emit("\tpush ax")
            self.gen_expr(args[1])
            self.emit("\tpop bx")
            self.emit("\txchg ax,bx")
            self.emit("\tmov cl,al")
            self.emit("\tmov ax,bx")
            self.emit(f"\t{low} ax,cl")
            return
        sym = self.resolve(name)
        if sym.kind != 'proc':
            self.err(f"'{name}' is not a procedure")
        for a in reversed(args):
            self.gen_expr(a)
            self.emit("\tpush ax")
        self.emit(f"\tcall {sanitize(name)}")

    def gen_move_builtin(self, args):
        if len(args) != 3:
            self.err("MOVE requires 3 arguments: MOVE(count, source, dest)")
        count, src, dst = args
        self.gen_expr(dst)
        self.emit("\tmov di,ax")
        self.gen_expr(src)
        self.emit("\tmov si,ax")
        self.gen_expr(count)
        self.emit("\tmov cx,ax")
        self.emit("\tpush ds")
        self.emit("\tpush ds")
        self.emit("\tpop es")
        self.emit("\trep movsb")
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
    def resolve(self, name):
        sym = self.cur_scope.lookup(name) if hasattr(self, 'cur_scope') else None
        if sym is None:
            sym = self.global_scope.lookup(name)
        if sym is None:
            self.err(f"undefined identifier '{name}'")
        return sym

    def load_from(self, sym):
        if sym.kind == 'param':
            self.emit(f"\tmov ax,[bp+{sym.info['offset']}]")
        elif sym.kind == 'local':
            off = sym.info['offset']
            self.emit(f"\tmov ax,[bp{off}]" if off < 0 else f"\tmov ax,[bp+{off}]")
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
            self.load_based_addr(sym)
            self.emit("\tmov bx,ax")
            typ = sym.info['type']
            if typ == 'byte':
                self.emit("\tmov al,[bx]")
                self.emit("\tmov ah,0")
            else:
                self.emit("\tmov ax,[bx]")
        else:
            self.err(f"cannot read value of '{sym.kind}'")

    def store_to(self, sym):
        if sym.kind == 'param':
            self.emit(f"\tmov [bp+{sym.info['offset']}],ax")
        elif sym.kind == 'local':
            off = sym.info['offset']
            self.emit(f"\tmov [bp{off}],ax" if off < 0 else f"\tmov [bp+{off}],ax")
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
            self.emit("\tpush ax")
            self.load_based_addr(sym)
            self.emit("\tmov bx,ax")
            self.emit("\tpop ax")
            typ = sym.info['type']
            self.emit("\tmov [bx],al" if typ == 'byte' else "\tmov [bx],ax")
        else:
            self.err(f"cannot assign to '{sym.kind}'")

    def load_based_addr(self, sym):
        based_on = sym.info['based_on']
        if isinstance(based_on, tuple):
            # "BASED struct.field type": the pointer value lives in that
            # structure field -- load it the same way any field read does.
            base_name, field = based_on
            self.ex_Field(ast.Field(base=ast.Name(name=base_name), field=field))
        else:
            ptr_sym = self.resolve(based_on)
            self.load_from(ptr_sym)

    def store_lvalue(self, lv):
        if isinstance(lv, ast.Name):
            sym = self.resolve(lv.name)
            self.store_to(sym)
        elif isinstance(lv, ast.Field):
            self.emit("\tpush ax")
            self.gen_field_addr(lv)
            self.emit("\tmov bx,ax")
            self.emit("\tpop ax")
            size = self._field_size(lv)
            self.emit("\tmov [bx],al" if size == 1 else "\tmov [bx],ax")
        elif isinstance(lv, ast.Call):
            self.store_index(lv)
        elif isinstance(lv, ast.Index):
            self.emit("\tpush ax")
            self.gen_index2_addr(lv)
            self.emit("\tmov bx,ax")
            self.emit("\tpop ax")
            size = self._index_elem_size(lv)
            self.emit("\tmov [bx],al" if size == 1 else "\tmov [bx],ax")
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
                off = sym.info['offset']
                self.emit(f"\tmov ax,bp")
                if off >= 0:
                    self.emit(f"\tadd ax,{off}")
                else:
                    self.emit(f"\tsub ax,{-off}")
            elif sym.kind == 'based':
                # The "address of" a BASED variable is just its pointer's
                # current value (used e.g. as the base of a field access,
                # "pd.cns" -> address-of(pd) + field offset).
                self.load_based_addr(sym)
            else:
                self.err(f"cannot take the address of '{e.name}'")
        elif isinstance(e, ast.Field):
            self.gen_field_addr(e)
        elif isinstance(e, ast.Call):
            self.gen_index_addr(e)
        elif isinstance(e, ast.Index):
            self.gen_index2_addr(e)
        elif isinstance(e, ast.Str):
            label = self.intern_string(e.value)
            self.emit(f"\tmov ax,offset {label}")
        else:
            self.err(f"unsupported operand for '.': {type(e).__name__}")

    def gen_field_addr(self, fld):
        base = fld.base
        if not isinstance(base, ast.Name):
            self.err("field access base must be a plain variable name")
        sym = self.resolve(base.name)
        fields = sym.info.get('fields')
        if fields is None:
            self.err(f"'{base.name}' is not a structure")
        key = fld.field.lower()
        if key not in fields:
            self.err(f"structure '{base.name}' has no field '{fld.field}'")
        off, size, _ = fields[key]
        self.gen_addr(base)
        if off:
            self.emit(f"\tadd ax,{off}")
        self._last_field_size = size

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
            elemsize = 1 if sym.info['type'] == 'byte' else 2
            self.load_based_addr(sym)
            self.emit("\tpush ax")
            self.gen_expr(call.args[0])
            if elemsize != 1:
                self.emit(f"\tmov cx,{elemsize}")
                self.emit("\tmul cx")
            self.emit("\tpop bx")
            self.emit("\tadd ax,bx")
            return
        if sym.kind in ('param', 'local'):
            # An array parameter/local declared to hold an ADDRESS is
            # treated as a pointer when indexed (PL/M arrays are always
            # passed by reference): its value is the base address.
            self.load_from(sym)
            self.emit("\tpush ax")
            self.gen_expr(call.args[0])
            self.emit("\tpop bx")
            self.emit("\tadd ax,bx")
            return
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

    def _indexed_elem_size(self, sym):
        if sym.kind == 'based':
            return 1 if sym.info['type'] == 'byte' else 2
        if sym.kind in ('param', 'local'):
            return 1
        return sym.info.get('elemsize', 1)

    def gen_index_load(self, call):
        sym = self.resolve(call.name)
        self.gen_index_addr(call)
        self.emit("\tmov bx,ax")
        size = self._indexed_elem_size(sym)
        if size == 1:
            self.emit("\tmov al,[bx]")
            self.emit("\tmov ah,0")
        else:
            self.emit("\tmov ax,[bx]")

    def store_index(self, call):
        self.emit("\tpush ax")
        self.gen_index_addr(call)
        self.emit("\tmov bx,ax")
        self.emit("\tpop ax")
        sym = self.resolve(call.name)
        size = self._indexed_elem_size(sym)
        self.emit("\tmov [bx],al" if size == 1 else "\tmov [bx],ax")

    # -- output -----------------------------------------------------------
    def to_asm_text(self):
        lines = []
        lines.append("\tcseg")
        lines.append("\torg\t0")
        lines.extend(self.cseg)
        for tbl, arms in getattr(self, 'cseg_tables', []):
            lines.append(f"{tbl}:")
            for a in arms:
                lines.append(f"\tdw\toffset {a}")
        lines.append("")
        lines.append("\tdseg")
        lines.append("\torg\t0")
        lines.extend(self.dseg)
        lines.append("\tend")
        return '\n'.join(lines) + '\n'
