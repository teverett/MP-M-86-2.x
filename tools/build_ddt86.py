#!/usr/bin/env python3
"""build_ddt86.py -- build DDT86.CMD, DRI's debugger, from
src-assem/toolchain/ the way orig/mpm862sr/06/DDT86.SUB does, with this
repo's tools in place of DRI's:

  DDT86.SUB                         here
  ---------                         ----
  plm86 ins86, dis86                tools/plmc.py
  asm86 dislnk86; link86/loc86      tools/plm/link.py, plus DISLNK86's
    (the disassembler overlay)        stubs (below)
  plm86 ass86, asmtab               tools/plmc.py
  asm86 asslnk86; link86/loc86      tools/plm/link.py, plus ASSLNK86's
    po(dis86.abs) (the assembler)     stubs; DIS86's publics as equates
  asm86 ddt86                       tools/asm86.py
  pip ddt86.h86=ddt86,dis86,ass86   the three images merged, in order
  gencmd ddt86 8080 code[m366]      an 8080-model CMD: one code group,
                                      DS = CS (written below)

DDT86 is an "8080 model" program: code and data in one segment. Its own
code runs from 100h and its data follows (`dseg org offset endcs`); the
two PL/M overlays sit at fixed origins, lasmorg (DDT86's `disem`) and
asmorg (`assem`), each starting with its link module's jump stubs: into
the overlay, and back to DDT86's vectors at 103h-10Fh (conin,
plmconout, plmgetline, asment, plmset).

The origins aren't DRI's (1400h, 2300h). tools/asm86.py picks longer
encodings than DRI's ASM86 for some instructions (no short
accumulator-direct MOV, imm8 sign-extended ALU or B8+r MOV forms), so
DDT86's own data ends at 141Ah, not 1310h -- past DRI's 1400h -- and
plmc's PL/M code is larger than Intel PL/M-86's. So the overlays are
placed after what precedes them (paragraph-aligned), and a build copy
of DDT86.A86 gets the matching lasmorg/asmorg equates; the source
itself is DRI's, unchanged.

    python3 tools/build_ddt86.py [-o src-assem/build/toolchain/DDT86.CMD]
"""
import argparse
import os
import re
import struct
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from asm86 import Assembler  # noqa: E402
from plm.link import link  # noqa: E402

TOOLCHAIN = os.path.join(ROOT, 'src-assem', 'toolchain')
DDT86_A86 = os.path.join(TOOLCHAIN, 'DDT86.A86')

# DDT86's vectors (DDT86.A86, org ddt_org): what the link modules' stubs
# jump back to
DDT_VECTORS = {'ddt_conin': 0x103, 'ddt_conout': 0x106, 'ddt_getline': 0x109, 'ddt_asment': 0x10C,
               'ddt_set': 0x10F}

# DISLNK86.ASM: the disassembler overlay's entry and DDT86 links
DIS_HEAD = ["\tjmp\tpl_disem", "pl_boot:\tret", "pl_conin:\tjmp\tddt_conin", "pl_conout:\tjmp\tddt_conout"]
# ASSLNK86.ASM: the assembler overlay's
ASS_HEAD = ["\tjmp\tpl_asm", "pl_ddtgetline:\tjmp\tddt_getline", "pl_goddt:\tjmp\tddt_asment",
            "pl_ddtset:\tjmp\tddt_set"]
# what the assembler overlay takes from the disassembler's (po(dis86.abs))
DIS_EXPORTS = ('pl_conin', 'pl_conout', 'pl_printm', 'pl_printword', 'pl_tabptrs', 'pl_nops', 'pl_opnin',
               'pl_reg16', 'pl_reg8', 'pl_segreg')


def align(n, a=16):
    return (n + a - 1) // a * a


def assemble(text, tmp, name):
    path = os.path.join(tmp, name + '.A86')
    with open(path, 'w') as f:
        f.write(text)
    asm = Assembler()
    asm.assemble(path, [TOOLCHAIN])
    return asm


def sym(asm, name):
    return next(v.value for k, v in asm.symtab.items() if k.lower() == name.lower())


def overlay(modules, org, head, tmp, name, equates):
    """link the PL/M modules at org, data right after the code ->
    (assembler, code end, data end, {module tag: its publics})"""
    eq = {**DDT_VECTORS, **equates}
    text, _ = link(modules, org, 0xF000, [TOOLCHAIN], eq, head)
    code_end = sym(assemble(text, tmp, name), 'pl_link_code_end')
    text, publics = link(modules, org, code_end, [TOOLCHAIN], eq, head)
    asm = assemble(text, tmp, name)
    return asm, code_end, sym(asm, 'pl_link_data_end'), publics


def ddt86(tmp, lasmorg=None, asmorg=None):
    text = open(DDT86_A86, encoding='latin-1').read()
    if lasmorg is not None:
        text = re.sub(r'(?m)^(lasmorg\s+equ\s+).*$', rf'\g<1>{lasmorg:04X}h', text)
        text = re.sub(r'(?m)^(asmorg\s+equ\s+).*$', rf'\g<1>{asmorg:04X}h', text)
    return assemble(text, tmp, 'DDT86')


def build(parts=False):
    """-> (the CMD file's bytes, layout), and with parts the three
    assembled pieces too: {'ddt86': .., 'dis': .., 'ass': ..}, each
    (Assembler, {module tag: its publics})"""
    with tempfile.TemporaryDirectory() as tmp:
        first = ddt86(tmp)
        ddt_data_end = sym(first, 'stackp')
        lasmorg = align(ddt_data_end)
        dis, dis_code_end, dis_end, dis_pub = overlay([('ins86', os.path.join(TOOLCHAIN, 'INS86.PLM')),
                                              ('dis86', os.path.join(TOOLCHAIN, 'DIS86.PLM'))],
                                             lasmorg, DIS_HEAD, tmp, 'DISOVL', {})
        exports = {n: sym(dis, n) for n in DIS_EXPORTS}
        asmorg = align(dis_end)
        ass, ass_code_end, ass_end, ass_pub = overlay([('asmtab', os.path.join(TOOLCHAIN, 'ASMTAB.PLM')),
                                              ('ass86', os.path.join(TOOLCHAIN, 'ASS86.PLM'))],
                                             asmorg, ASS_HEAD, tmp, 'ASSOVL', exports)
        ddt = ddt86(tmp, lasmorg, asmorg)
        assert sym(ddt, 'stackp') == ddt_data_end and sym(ddt, 'disem') == lasmorg and sym(ddt, 'assem') == asmorg

        endcs = sym(ddt, 'endcs')
        image = bytearray(align(ass_end))
        cs, ds = ddt.segment_bytes('cseg'), ddt.segment_bytes('dseg')
        image[0:endcs] = cs[0:endcs]                          # DDT86's code (and base page area)
        image[endcs:ddt_data_end] = ds[endcs:ddt_data_end]    # its data, after it (8080 model)
        for asm, org, code_end, end in ((dis, lasmorg, dis_code_end, dis_end), (ass, asmorg, ass_code_end, ass_end)):
            c, d = asm.segment_bytes('cseg'), asm.segment_bytes('dseg')
            image[org:code_end] = c[org:code_end]
            image[code_end:end] = d[code_end:end]
        paras = len(image) // 16
        # the 8080 model: one code group (form 1), DS = CS; relocatable
        header = struct.pack('<BHHHH', 1, paras, 0, paras, 0).ljust(128, b'\0')
        layout = {'ddt_data_end': ddt_data_end, 'endcs': endcs, 'lasmorg': lasmorg, 'dis_code_end': dis_code_end,
                  'dis_end': dis_end, 'asmorg': asmorg, 'ass_code_end': ass_code_end, 'ass_end': ass_end,
                  'paras': paras, 'image_len': paras * 16, 'exports': exports, 'asm': sym(ass, 'pl_asm'),
                  'disem': sym(dis, 'pl_disem')}
        cmd = header + bytes(image[:paras * 16])
        if parts:
            return cmd, layout, {'ddt86': (ddt, {}), 'dis': (dis, dis_pub), 'ass': (ass, ass_pub)}
        return cmd, layout


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-o', '--output', default=os.path.join(ROOT, 'src-assem', 'build', 'toolchain', 'DDT86.CMD'))
    args = ap.parse_args()
    cmd, lay = build()
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, 'wb') as f:
        f.write(cmd)
    print(f"build_ddt86: {os.path.relpath(args.output, ROOT)}: {lay['paras']:#x} paragraphs; DDT86 data to "
          f"{lay['ddt_data_end']:#06x}, disassembler {lay['lasmorg']:#06x}-{lay['dis_end']:#06x}, "
          f"assembler {lay['asmorg']:#06x}-{lay['ass_end']:#06x}")


if __name__ == '__main__':
    main()
