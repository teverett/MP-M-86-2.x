from .lexer import preprocess, tokenize, expand_literally
from .parser import Parser
from .codegen import CodeGen


def compile_file(path, search_dirs):
    lines = preprocess(path, search_dirs)
    toks = tokenize(lines)
    toks = expand_literally(toks)
    filename = lines[0][0] if lines else path
    p = Parser(toks, filename)
    module = p.parse_module()
    cg = CodeGen(filename)
    cg.compile_module(module)
    return cg.to_asm_text()
