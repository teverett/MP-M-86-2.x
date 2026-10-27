"""AST node types for the PL/M-86 subset. Plain attribute-bag classes;
codegen dispatches on type(node).__name__."""


class Node:
    def __init__(self, **kw):
        self.__dict__.update(kw)

    def __repr__(self):
        return f"{type(self).__name__}({self.__dict__})"


class Module(Node):
    pass  # statements: list


class VarDecl(Node):
    pass  # name, dims(None|int), type, external, public, fields(None|list[(name,dims,type)]),
    # literally(None|Node), based_on(None|str)


class ProcDecl(Node):
    pass  # name, params(list[str]), rettype(None|'byte'|'address'|'pointer'),
    # external, public, body(list[Node])


class Block(Node):
    pass  # label(None|str), body(list[Node])


class WhileBlock(Node):
    pass  # label, cond, body


class CaseBlock(Node):
    pass  # label, selector, body(list[Node]) -- one stmt per case arm


class ForBlock(Node):
    pass  # label, var, start, stop, step(None|Node), body


class If(Node):
    pass  # cond, then_stmt, else_stmt(None|Node)


class Assign(Node):
    pass  # targets(list[Node lvalue]), expr


class CallStmt(Node):
    pass  # name, args(list[Node])


class Return(Node):
    pass  # expr(None|Node)


class Empty(Node):
    pass


class LabelMarker(Node):
    pass  # name -- a bare code label inside a statement sequence


class Interrupts(Node):
    pass  # enable -- ENABLE (True) or DISABLE (False)


class Goto(Node):
    pass  # name -- GOTO/GO TO a label


# expressions
class Num(Node):
    pass  # value


class Str(Node):
    pass  # value (python str, raw bytes of the literal)


class Name(Node):
    pass  # name


class Index(Node):
    pass  # base(Node), index(Node)


class Field(Node):
    pass  # base(Node), field(str)


class AddrOf(Node):
    pass  # expr


class InlineArray(Node):
    pass  # elements(list[Node]) -- '.(' a,b,c ')'


class Call(Node):
    pass  # name, args(list[Node])


class BinOp(Node):
    pass  # op, left, right


class UnOp(Node):
    pass  # op, operand


class AssignExpr(Node):
    pass  # target(Node lvalue), expr
