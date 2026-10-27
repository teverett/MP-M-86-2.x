"""rsp_syms.py -- the constants in src-c/rsp/rsp_syms.h and
src-c/utils/utils_syms.h, for the tests"""
import os
import re

_SRC_C = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'src-c')
_VALUES = {}
for _h in (os.path.join(_SRC_C, 'rsp', 'rsp_syms.h'), os.path.join(_SRC_C, 'utils', 'utils_syms.h')):
    _VALUES.update({m.group(1): int(m.group(2), 16)
                    for m in re.finditer(r'^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+)', open(_h).read(), re.MULTILINE)})


def value(name):
    return _VALUES[name]
