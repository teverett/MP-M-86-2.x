#!/usr/bin/env python3
"""Convenience wrapper: `python3 tools/asm86.py file.A86 [options]`."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from asm86.__main__ import main  # noqa: E402

if __name__ == '__main__':
    sys.exit(main())
