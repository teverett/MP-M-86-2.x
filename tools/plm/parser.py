"""Recursive-descent parser for the PL/M-86 subset -> ast.py nodes."""
from . import ast
from .errors import PlmError
from .lexer import parse_number

TYPE_WORDS = {'byte', 'address', 'pointer', 'word', 'label'}


class Parser:
    def __init__(self, toks, filename):
        self.toks = toks
        self.i = 0
        self.filename = filename

    def peek(self, off=0):
        return self.toks[self.i + off]

    def at(self, kind):
        return self.peek().kind == kind

    def at_kw(self, *kinds):
        return self.peek().kind in kinds

    def next(self):
        t = self.toks[self.i]
        self.i += 1
        return t

    def expect(self, kind):
        t = self.next()
        if t.kind != kind:
            self.err(f"expected {kind}, got {t.kind} ({t.val!r})", t)
        return t

    def err(self, msg, tok=None):
        tok = tok or self.peek()
        raise PlmError(msg, self.filename, tok.lineno)

    # ---- top level -------------------------------------------------
    def parse_module(self):
        stmts = []
        while not self.at('EOF'):
            stmts.extend(self.parse_statement_items())
        return ast.Module(statements=stmts)

    def parse_statement_items(self):
        """Like parse_statement, but a multi-variable DCL splices all of
        its VarDecls into the caller's list (same scope), instead of
        nesting them in a Block (which would wrongly scope-limit them)."""
        if self.at_kw('DECLARE', 'DCL'):
            return self.parse_decl_stmt()
        return [self.parse_statement()]

    # ---- statements --------------------------------------------------
    def parse_statement(self):
        if self.at(';'):
            self.next()
            return ast.Empty()

        # label ':' statement   OR   name ':' PROCEDURE ...
        if self.at('IDENT') and self.peek(1).kind == ':':
            label = self.next().val
            self.next()  # ':'
            if self.at_kw('PROCEDURE'):
                return self.parse_proc(label)
            stmt = self.parse_statement()
            if isinstance(stmt, (ast.Block, ast.WhileBlock, ast.CaseBlock, ast.ForBlock)):
                stmt.label = label
            return stmt

        if self.at_kw('DECLARE', 'DCL'):
            decls = self.parse_decl_stmt()
            return decls[0] if len(decls) == 1 else ast.Block(label=None, body=decls)
        if self.at_kw('DO'):
            return self.parse_do()
        if self.at_kw('IF'):
            return self.parse_if()
        if self.at_kw('CALL'):
            return self.parse_call_stmt()
        if self.at_kw('RETURN'):
            self.next()
            if self.at(';'):
                self.next()
                return ast.Return(expr=None)
            e = self.parse_expr()
            self.expect(';')
            return ast.Return(expr=e)
        if self.at_kw('GOTO', 'GO'):
            self.next()
            if self.peek(-1).kind == 'GO':
                if self.at_kw('TO'):
                    self.next()
            name = self.expect('IDENT').val
            self.expect(';')
            return ast.Empty()  # GOTO not supported in codegen; parsed & dropped
        if self.at_kw('DISABLE', 'ENABLE'):
            self.next()
            self.expect(';')
            return ast.Empty()

        # assignment: lvalue (',' lvalue)* ('=' | ':=') expr ';'
        # PL/M-86 statement-level assignment is plain '=' ("a = b + c;");
        # ':=' is reserved for an assignment embedded inside an expression
        # (e.g. "if (ret := f(x)) = 20 then"), where bare '=' would
        # otherwise be read as equality. Both are accepted here.
        targets = [self.parse_lvalue()]
        while self.at(','):
            self.next()
            targets.append(self.parse_lvalue())
        if self.at('='):
            self.next()
        else:
            self.expect(':=')
        e = self.parse_expr()
        self.expect(';')
        return ast.Assign(targets=targets, expr=e)

    def parse_lvalue(self):
        e = self.parse_postfix_primary()
        return e

    def parse_decl_stmt(self):
        self.next()  # DECLARE/DCL
        decls = list(self.parse_one_decl())
        while self.at(','):
            self.next()
            decls.extend(self.parse_one_decl())
        self.expect(';')
        return decls

    def parse_at_target(self):
        """AT(expr): the address this declaration's storage is aliased
        onto (instead of getting its own allocation). expr is any
        address-valued expression -- "@name", ".name(index)", a LITERALLY
        macro expanding to one of those, etc. -- resolved in codegen."""
        self.expect('(')
        expr = self.parse_expr()
        self.expect(')')
        return expr

    def parse_one_decl(self):
        """Returns a list of VarDecl -- almost always one, except for the
        grouped-name form "DECLARE (a, b, c) BYTE;", which declares all of
        a/b/c with the same type/attributes in one go."""
        if self.at('('):
            self.next()
            # Each grouped name may carry its own "BASED ptr" override
            # (e.g. "(op1, op2, op BASED op$base) STRUCTURE(...)") while
            # still sharing the group's common type/attributes.
            names = [self._parse_grouped_name()]
            while self.at(','):
                self.next()
                names.append(self._parse_grouped_name())
            self.expect(')')
            n0, based0 = names[0]
            template = self._parse_decl_rest(n0)
            if based0 is not None:
                template.based_on = based0
            out = [template]
            for n, based in names[1:]:
                clone = ast.VarDecl(**template.__dict__)
                clone.name = n
                clone.based_on = based
                out.append(clone)
            return out
        name = self.expect('IDENT').val
        return [self._parse_decl_rest(name)]

    def _parse_grouped_name(self):
        name = self.expect('IDENT').val
        based_on = None
        if self.at_kw('BASED'):
            self.next()
            based_on = self.expect('IDENT').val
            if self.at('.'):
                self.next()
                based_on = (based_on, self.expect('IDENT').val)
        return name, based_on

    def _parse_decl_rest(self, name):
        dims = None
        if self.at('('):
            self.next()
            if self.at('*'):
                # "(*)" -- size inferred from the INITIAL/DATA list given
                # for this declaration, resolved in codegen.
                self.next()
                dims = '*'
            else:
                dims = self.parse_expr()
            self.expect(')')

        if self.at_kw('LITERALLY'):
            self.next()
            val = self.next()
            if val.kind != 'STR':
                self.err("LITERALLY requires a quoted string", val)
            return ast.VarDecl(name=name, dims=None, type='literally', literally=val.val,
                                external=False, public=False, fields=None, based_on=None)

        if self.at_kw('STRUCTURE'):
            self.next()
            self.expect('(')
            fields = [self.parse_struct_field()]
            while self.at(','):
                self.next()
                fields.append(self.parse_struct_field())
            self.expect(')')
            initial = None
            at_target = None
            while self.at_kw('INITIAL', 'DATA', 'AT'):
                if self.at_kw('AT'):
                    self.next()
                    at_target = self.parse_at_target()
                    continue
                self.next()
                self.expect('(')
                initial = [self.parse_expr()]
                while self.at(','):
                    self.next()
                    initial.append(self.parse_expr())
                self.expect(')')
            return ast.VarDecl(name=name, dims=dims, type='structure', fields=fields,
                                external=False, public=False, literally=None, based_on=None,
                                initial=initial, at_target=at_target)

        if self.at_kw('BASED'):
            self.next()
            based_on = self.expect('IDENT').val
            if self.at('.'):
                self.next()
                based_on = (based_on, self.expect('IDENT').val)
            if self.at('('):
                # "BASED ptr (n) type" -- an array of n elements living at
                # the based pointer's address. The dimension itself isn't
                # tracked (BASED arrays aren't indexed through 'based' Syms
                # yet, see tools/plm/README.md), but still parse correctly.
                self.next()
                self.parse_expr()
                self.expect(')')
            if self.at_kw('STRUCTURE'):
                # Unlike parse_type_name()'s generic STRUCTURE(...) handling
                # (which discards the fields -- fine for a BASED type that's
                # never field-accessed), a BASED variable routinely *is*
                # field-accessed ("pd.cns"), so keep the real field list.
                self.next()
                self.expect('(')
                fields = [self.parse_struct_field()]
                while self.at(','):
                    self.next()
                    fields.append(self.parse_struct_field())
                self.expect(')')
                return ast.VarDecl(name=name, dims=dims, type='structure', based_on=based_on,
                                    external=False, public=False, fields=fields, literally=None)
            typ = self.parse_type_name()
            return ast.VarDecl(name=name, dims=dims, type=typ, based_on=based_on,
                                external=False, public=False, fields=None, literally=None)

        typ = self.parse_type_name()
        external = False
        public = False
        initial = None
        at_target = None
        while self.at_kw('EXTERNAL', 'PUBLIC', 'INITIAL', 'DATA', 'AT'):
            k = self.next().kind
            if k == 'EXTERNAL':
                external = True
            elif k == 'PUBLIC':
                public = True
            elif k == 'AT':
                at_target = self.parse_at_target()
            else:  # INITIAL or DATA
                self.expect('(')
                initial = [self.parse_expr()]
                while self.at(','):
                    self.next()
                    initial.append(self.parse_expr())
                self.expect(')')
        return ast.VarDecl(name=name, dims=dims, type=typ, external=external, public=public,
                            fields=None, based_on=None, literally=None, initial=initial,
                            at_target=at_target)

    def parse_struct_field(self):
        name = self.expect('IDENT').val
        dims = None
        if self.at('('):
            self.next()
            dims = self.parse_expr()
            self.expect(')')
        typ = self.parse_type_name()
        return (name, dims, typ)

    def parse_type_name(self):
        if self.at_kw(*[w.upper() for w in TYPE_WORDS]):
            return self.next().val.lower()
        if self.at('IDENT') and self.peek().val.lower() == 'addr':
            self.next()
            return 'address'
        if self.at('IDENT') and self.peek().val.lower() == 'integer':
            # PL/M-86's signed 16-bit type (as opposed to WORD/ADDRESS,
            # unsigned). This compiler doesn't distinguish signed from
            # unsigned bit patterns internally -- SIGNED/UNSIGN below are
            # both no-ops -- so INTEGER is just treated as a 2-byte type,
            # same storage as ADDRESS.
            self.next()
            return 'address'
        if self.at_kw('STRUCTURE'):
            # An inline STRUCTURE(...) spec as a BASED type (this happens
            # after LITERALLY macro expansion, e.g. PROCES.LIT's
            # pd$structure). Its field layout is discarded here -- field
            # access on such a BASED variable isn't supported, see
            # tools/README.md -- but the syntax still parses correctly.
            self.next()
            self.expect('(')
            depth = 1
            while depth > 0:
                t = self.next()
                if t.kind == '(':
                    depth += 1
                elif t.kind == ')':
                    depth -= 1
                elif t.kind == 'EOF':
                    self.err("unterminated STRUCTURE(...) spec")
            return 'pointer'
        if self.at('IDENT'):
            # a named structure type used as a BASED type
            return self.next().val
        self.err("expected a type (BYTE/ADDRESS/POINTER/WORD or a structure name)")

    def parse_proc(self, name):
        self.expect('PROCEDURE')
        params = []
        if self.at('('):
            self.next()
            if not self.at(')'):
                params.append(self.expect('IDENT').val)
                while self.at(','):
                    self.next()
                    params.append(self.expect('IDENT').val)
            self.expect(')')
        rettype = None
        if self.at_kw(*[w.upper() for w in TYPE_WORDS]):
            rettype = self.next().val.lower()
        external = False
        public = False
        while self.at_kw('EXTERNAL', 'PUBLIC', 'REENTRANT', 'INTERRUPT'):
            k = self.next().kind
            if k == 'EXTERNAL':
                external = True
            elif k == 'PUBLIC':
                public = True
        self.expect(';')
        body = []
        if not external:
            while not (self.at_kw('END')):
                body.extend(self.parse_statement_items())
            self.expect('END')
            if self.at('IDENT'):
                self.next()
            self.expect(';')
        else:
            # external stub: may have a DECLARE of its own params (ignored
            # for codegen, just consumed) then "END name;"
            while not self.at_kw('END'):
                self.parse_statement()
            self.expect('END')
            if self.at('IDENT'):
                self.next()
            self.expect(';')
        return ast.ProcDecl(name=name, params=params, rettype=rettype, external=external,
                             public=public, body=body)

    def parse_do(self):
        self.expect('DO')
        if self.at_kw('WHILE'):
            self.next()
            cond = self.parse_expr()
            self.expect(';')
            body = self.parse_body_until_end()
            return ast.WhileBlock(label=None, cond=cond, body=body)
        if self.at_kw('CASE'):
            self.next()
            sel = self.parse_expr()
            self.expect(';')
            body = self.parse_body_until_end()
            return ast.CaseBlock(label=None, selector=sel, body=body)
        if self.at('IDENT') and self.peek(1).kind in ('=', ':='):
            var = self.next().val
            self.next()  # '=' or ':='
            start = self.parse_expr()
            self.expect('TO')
            stop = self.parse_expr()
            step = None
            if self.at_kw('BY'):
                self.next()
                step = self.parse_expr()
            self.expect(';')
            body = self.parse_body_until_end()
            return ast.ForBlock(label=None, var=var, start=start, stop=stop, step=step, body=body)
        self.expect(';')
        body = self.parse_body_until_end()
        return ast.Block(label=None, body=body)

    def parse_body_until_end(self):
        body = []
        while not self.at_kw('END'):
            body.extend(self.parse_statement_items())
        self.expect('END')
        if self.at('IDENT'):
            self.next()
        self.expect(';')
        return body

    def parse_if(self):
        self.expect('IF')
        cond = self.parse_expr()
        self.expect('THEN')
        then_stmt = self.parse_statement()
        else_stmt = None
        if self.at_kw('ELSE'):
            self.next()
            else_stmt = self.parse_statement()
        return ast.If(cond=cond, then_stmt=then_stmt, else_stmt=else_stmt)

    def parse_call_stmt(self):
        self.expect('CALL')
        name = self.expect('IDENT').val
        args = []
        if self.at('('):
            self.next()
            if not self.at(')'):
                args.append(self.parse_expr())
                while self.at(','):
                    self.next()
                    args.append(self.parse_expr())
            self.expect(')')
        self.expect(';')
        return ast.CallStmt(name=name, args=args)

    # ---- expressions (low to high precedence) --------------------------
    def parse_expr(self):
        # An embedded assignment ("(ret := f(x)) = 20"), whose target can
        # be a plain name OR something like "fcb16(i)" (array element) or
        # "s.field" -- try parsing a full lvalue and back out if it isn't
        # actually followed by ':='.
        if self.at('IDENT'):
            save = self.i
            target = self.parse_postfix_primary()
            if self.at(':='):
                self.next()
                e = self.parse_expr()
                return ast.AssignExpr(target=target, expr=e)
            self.i = save
        return self.parse_or()

    def parse_or(self):
        v = self.parse_xor()
        while self.at_kw('OR'):
            self.next()
            v = ast.BinOp(op='or', left=v, right=self.parse_xor())
        return v

    def parse_xor(self):
        v = self.parse_and()
        while self.at_kw('XOR'):
            self.next()
            v = ast.BinOp(op='xor', left=v, right=self.parse_and())
        return v

    def parse_and(self):
        v = self.parse_rel()
        while self.at_kw('AND'):
            self.next()
            v = ast.BinOp(op='and', left=v, right=self.parse_rel())
        return v

    REL_OPS = {'=', '<>', '<', '>', '<=', '>='}

    def parse_rel(self):
        v = self.parse_add()
        while self.peek().kind in self.REL_OPS:
            op = self.next().kind
            v = ast.BinOp(op=op, left=v, right=self.parse_add())
        return v

    def parse_add(self):
        v = self.parse_mul()
        while True:
            t = self.peek()
            if t.kind in ('+', '-'):
                op = self.next().kind
            elif t.kind == 'IDENT' and t.val.lower() == 'plus':
                # Seen exactly once in this tree (PIP.PLM: "C2 PLUS 0"),
                # never declared as anything -- a word-form synonym for
                # '+', the same idea as PL/M's own word operators
                # (AND/OR/XOR/MOD). Unconfirmed by a second usage, but
                # the only reading that parses at all, and harmless if
                # wrong (see tools/plm/README.md).
                self.next()
                op = '+'
            else:
                break
            v = ast.BinOp(op=op, left=v, right=self.parse_mul())
        return v

    def parse_mul(self):
        v = self.parse_unary()
        while True:
            if self.at('*'):
                self.next()
                v = ast.BinOp(op='*', left=v, right=self.parse_unary())
            elif self.at('/'):
                self.next()
                v = ast.BinOp(op='/', left=v, right=self.parse_unary())
            elif self.at_kw('MOD'):
                self.next()
                v = ast.BinOp(op='mod', left=v, right=self.parse_unary())
            else:
                break
        return v

    def parse_unary(self):
        if self.at_kw('NOT'):
            self.next()
            return ast.UnOp(op='not', operand=self.parse_unary())
        if self.at('-'):
            self.next()
            return ast.UnOp(op='-', operand=self.parse_unary())
        if self.at('.') or self.at('@'):
            self.next()
            if self.at('('):
                self.next()
                elems = [self.parse_expr()]
                while self.at(','):
                    self.next()
                    elems.append(self.parse_expr())
                self.expect(')')
                return ast.InlineArray(elements=elems)
            return ast.AddrOf(expr=self.parse_unary())
        return self.parse_postfix_primary()

    def parse_postfix_primary(self):
        e = self.parse_primary()
        while True:
            if self.at('.') and self.peek(1).kind == 'IDENT':
                self.next()
                fld = self.next().val
                e = ast.Field(base=e, field=fld)
            elif self.at('(') and not isinstance(e, ast.Name):
                # A trailing "(...)" on anything other than a bare name is
                # indexing into that result (never a procedure call, which
                # only ever applies directly to a bare name) -- e.g. the
                # second "(k)" in "dir$entries(j).file(k)".
                self.next()
                idx = self.parse_expr()
                self.expect(')')
                e = ast.Index(base=e, index=idx)
            else:
                break
        return e

    def parse_primary(self):
        t = self.peek()
        if t.kind == 'NUM':
            self.next()
            return ast.Num(value=parse_number(t.val))
        if t.kind == 'STR':
            self.next()
            return ast.Str(value=t.val)
        if t.kind == '(':
            self.next()
            e = self.parse_expr()
            self.expect(')')
            return e
        if t.kind == 'IDENT':
            self.next()
            node = ast.Name(name=t.val)
            if self.at('('):
                self.next()
                args = []
                if not self.at(')'):
                    args.append(self.parse_expr())
                    while self.at(','):
                        self.next()
                        args.append(self.parse_expr())
                self.expect(')')
                node = ast.Call(name=t.val, args=args)
            return node
        self.err(f"unexpected token {t.kind} ({t.val!r}) in expression")
