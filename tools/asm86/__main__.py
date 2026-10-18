import argparse
import os
import sys

from .assembler import Assembler
from .errors import AsmError


def main(argv=None):
    ap = argparse.ArgumentParser(prog='asm86', description='ASM86-dialect assembler for MP/M-86 2.x source')
    ap.add_argument('source', help='.A86 file to assemble')
    ap.add_argument('-I', '--include-dir', action='append', default=[], dest='include_dirs',
                     help='additional directory to search for INCLUDE files (repeatable)')
    ap.add_argument('-o', '--output', default=None,
                     help='output file stem (default: input file stem)')
    ap.add_argument('--symbols', action='store_true', help='print the symbol table and exit 0')
    args = ap.parse_args(argv)

    stem = args.output or os.path.splitext(args.source)[0]
    asm = Assembler()
    try:
        asm.assemble(args.source, args.include_dirs)
    except AsmError as e:
        print(f"asm86: error: {e}", file=sys.stderr)
        return 1

    if args.symbols:
        for name in sorted(asm.symtab):
            sym = asm.symtab[name]
            print(f"{sym.name:<24} {sym.kind:<6} {sym.value:04X}h size={sym.size}")
        return 0

    any_output = False
    for seg in asm.seg_order:
        data = asm.segment_bytes(seg)
        if not data:
            continue
        out_path = f"{stem}.{seg}.bin"
        with open(out_path, 'wb') as f:
            f.write(data)
        print(f"asm86: wrote {out_path} ({len(data)} bytes)")
        any_output = True
    if not any_output:
        print("asm86: warning: no code/data emitted", file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
