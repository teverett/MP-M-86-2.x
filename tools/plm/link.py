"""tools/plm/link.py -- link several plmc-compiled PL/M modules into one
assembly program, the job LINK86/LOC86 do for DRI's builds.

plmc compiles one module at a time and has no linker: a module's
EXTERNAL data gets storage of its own (so it assembles alone), and
every module has its own labels (pl_plmstart, arrNN, jlongN, ...).
link() takes the modules plmc compiled and makes one program of them:
  - each module's own labels -- everything it defines but its PUBLIC
    names -- get a per-module prefix, so two modules' labels don't
    clash;
  - the storage plmc reserved for a module's EXTERNAL data is dropped,
    so references bind to the PUBLIC copy in another module (or to an
    absolute address the caller gives in `equates`: a symbol of a
    program linked separately, LOC86's "po(...)");
  - the base page labels and plmc's "org 100h" data start are dropped:
    the code and data go at `code_org` and `data_org`.
The publics come from the parsed source (VarDecl/ProcDecl's public
flag), the same parse plmc compiles.

Used by tools/build_ddt86.py: DDT86's disassembler (INS86 + DIS86) and
assembler (ASMTAB + ASS86) overlays (orig/mpm862sr/06/DDT86.SUB).
"""
import re

from .ast import ProcDecl, VarDecl
from .codegen import CodeGen, sanitize
from .lexer import preprocess, tokenize, expand_literally
from .parser import Parser

_LABEL_DEF = re.compile(r'^([A-Za-z_][\w]*)(?=:|\s|$)')
_IDENT = re.compile(r'[A-Za-z_][\w]*')


def _declarations(path, search_dirs):
    """(public names, external data names) of the module at path, as
    plmc's labels (sanitize)"""
    lines = preprocess(path, search_dirs)
    module = Parser(expand_literally(tokenize(lines)), lines[0][0] if lines else path).parse_module()
    publics, ext_data = set(), set()

    def walk(stmts):
        for st in stmts:
            if isinstance(st, (VarDecl, ProcDecl)):
                if getattr(st, 'public', False):
                    publics.add(sanitize(st.name))
                if isinstance(st, VarDecl) and getattr(st, 'external', False):
                    ext_data.add(sanitize(st.name))
            body = getattr(st, 'body', None)
            if isinstance(body, list) and not isinstance(st, ProcDecl):
                walk(body)

    walk(module.statements)
    return publics, ext_data


def _compile(path, search_dirs):
    lines = preprocess(path, search_dirs)
    module = Parser(expand_literally(tokenize(lines)), lines[0][0] if lines else path).parse_module()
    cg = CodeGen(lines[0][0] if lines else path)
    cg.compile_module(module)
    return cg.to_asm_text()


def _split(text):
    """plmc's output -> (code lines, data lines), without the segment
    directives, the base page block, plmc's data org and `end`"""
    code, data, part = [], [], None
    for ln in text.split('\n'):
        s = ln.strip()
        if s == 'cseg':
            part = code
            continue
        if s == 'dseg':
            part = data
            continue
        if s == 'end' or s == 'pl_memory:' or part is None:
            continue
        if re.fullmatch(r'org\s+\S+', s):
            continue
        if part is data and re.match(r'pl_bp_\w+\s+rb\s+0$', s):
            continue
        part.append(ln)
    return code, data


def link(modules, code_org, data_org, search_dirs=(), equates=None, head=()):
    """modules: [(tag, path)]; returns (assembly text, {tag: its
    publics}) -- `head` is assembly placed first at code_org (jump
    stubs). A module's own labels become <tag>_<label>."""
    equates = dict(equates or {})
    codes, datas, publics_by = [], [], {}
    for tag, path in modules:
        publics, ext_data = _declarations(path, list(search_dirs))
        publics_by[tag] = publics
        code, data = _split(_compile(path, list(search_dirs)))
        # the EXTERNAL data's own storage: one line each (plmc's rb/rw/db)
        data = [ln for ln in data if not (_LABEL_DEF.match(ln) and _LABEL_DEF.match(ln).group(1) in ext_data)]
        defined = {m.group(1) for ln in code + data for m in [_LABEL_DEF.match(ln)] if m}
        local = defined - publics

        def rename(ln, tag=tag, local=local):
            if ln.lstrip().startswith(';'):
                return ln
            return _IDENT.sub(lambda m: f"{tag}_{m.group(0)}" if m.group(0) in local else m.group(0), ln)
        codes.append([rename(ln) for ln in code])
        datas.append([rename(ln) for ln in data])
    publics_all = set().union(*publics_by.values())
    out = [f"{name}\tequ\t{value:#06x}" for name, value in sorted(equates.items())
           if name not in publics_all]
    out += ["\tcseg", f"\torg\t{code_org:#06x}"] + list(head)
    for c in codes:
        out += c
    out.append("pl_link_code_end:")
    out += ["\tdseg", f"\torg\t{data_org:#06x}"]
    for d in datas:
        out += d
    out += ["pl_link_data_end:", "\tend"]
    return '\n'.join(out) + '\n', publics_by
