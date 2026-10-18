import argparse
import os
import sys

from .compiler import compile_file
from .errors import PlmError


def main(argv=None):
    ap = argparse.ArgumentParser(prog='plmc', description='PL/M-86 subset compiler -> ASM86 text')
    ap.add_argument('source', help='.PLM file to compile')
    ap.add_argument('-I', '--include-dir', action='append', default=[], dest='include_dirs',
                     help='additional directory to search for $INCLUDE files (repeatable)')
    ap.add_argument('-o', '--output', default=None, help='output .A86 path (default: input stem + .A86)')
    args = ap.parse_args(argv)

    out_path = args.output or (os.path.splitext(args.source)[0] + '.gen.A86')
    try:
        text = compile_file(args.source, args.include_dirs)
    except PlmError as e:
        print(f"plmc: error: {e}", file=sys.stderr)
        return 1

    with open(out_path, 'w', encoding='latin-1') as f:
        f.write(text)
    print(f"plmc: wrote {out_path}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
