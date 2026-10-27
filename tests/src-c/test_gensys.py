#!/usr/bin/env python3
"""src-c/gensys (gensys.c, sysdat.c) against tools/build_mpmsys.py: the
low-memory image kernel.elf starts from, built from src-c's own data,
compared with the one src-assem's build makes.

The reference is build_mpmsys.py's own image, with the RSPs src-assem's
build bakes in (TMP, CLOCK, ECHO, MPMSTAT -- STSRSP.PLM -- and CACHE),
as src-c has them. The C side is gensys() run on the host with
src-c/rsp's images.

Compared:
  - the layout: SYSDAT's segment and length;
  - SYSDAT, pools included, byte for byte;
  - the code half (from 1000:0 to SYSDAT) byte for byte, except the
    8086 code: each module's code segment and each RSP's, which gensys
    leaves zero (the C kernel never runs them), apart from INIT.SUP's
    version message in the SUP's segment, which is compared;
  - nothing else written past SYSDAT.
"""
import ctypes
import os
import struct
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_C = os.path.join(REPO_ROOT, 'src-c')
FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

CODE_SEG = 0x1000
INIT_VER = 0x37
PHYS = 0x100000


class GensysLayout(ctypes.Structure):
    _fields_ = [('sysdat', ctypes.c_uint16), ('sysdat_len', ctypes.c_uint16), ('free_start', ctypes.c_uint16)]


def reference(tmp):
    """build_mpmsys.py's image: (cseg, dseg)"""
    import build_mpmsys
    out = os.path.join(tmp, 'MPM.bin')
    argv, sys.argv = sys.argv, ['build_mpmsys.py', '-o', out]
    try:
        build_mpmsys.main()
    finally:
        sys.argv = argv
    return open(out + '.cseg', 'rb').read(), open(out + '.dseg', 'rb').read()


def c_image(tmp):
    lib_path = os.path.join(tmp, 'gensys.dylib' if sys.platform == 'darwin' else 'gensys.so')
    srcs = [os.path.join(SRC_C, 'gensys', f) for f in ('gensys.c', 'sysdat.c')]
    srcs += [os.path.join(SRC_C, d, f) for d in ('rsp', 'utils', 'toolchain')  # programs.c's table names their mains
             for f in sorted(os.listdir(os.path.join(SRC_C, d))) if f.endswith('.c') and f != 'gentab.c']
    srcs.append(os.path.join(FIXTURES, 'rt_host.c'))
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unknown-warning-option',
                    '-Wno-unterminated-string-initialization', '-shared', '-fPIC',
                    *[f'-I{os.path.join(SRC_C, d)}' for d in ('kernel', 'rt', 'rsp', 'gensys', 'utils', 'toolchain')], *srcs,
                    '-o', lib_path], check=True)
    lib = ctypes.CDLL(lib_path)
    lib.gensys_rsps.restype = ctypes.c_void_p
    lib.gensys_rsps.argtypes = [ctypes.POINTER(ctypes.c_uint)]
    lib.gensys.restype = GensysLayout
    lib.gensys.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_void_p, ctypes.c_uint]
    n = ctypes.c_uint()
    rsps = lib.gensys_rsps(ctypes.byref(n))
    mem = (ctypes.c_uint8 * PHYS)()
    layout = lib.gensys(mem, rsps, n.value)
    return bytes(mem), layout


def main():
    failures = []

    def check(label, ref, port):
        ok = ref == port
        print(f"[{'ok' if ok else 'FAIL'}] {label}: reference {ref} | port {port}")
        if not ok:
            failures.append(label)

    with tempfile.TemporaryDirectory() as tmp:
        cseg, dseg = reference(tmp)
        mem, layout = c_image(tmp)

    sysdat = CODE_SEG + (len(cseg) + 15) // 16
    check("SYSDAT's segment", hex(sysdat), hex(layout.sysdat))
    check("SYSDAT's length (pools included)", hex(len(dseg)), hex(layout.sysdat_len))

    port_sd = mem[sysdat * 16:sysdat * 16 + len(dseg)]
    diffs = [i for i in range(len(dseg)) if dseg[i] != port_sd[i]]
    check("SYSDAT byte for byte", [], [f'{i:04X}: {dseg[i]:02X}/{port_sd[i]:02X}' for i in diffs[:10]])

    # the code half: skip the 8086 code ranges
    code = []
    mod_segs = [struct.unpack_from('<H', dseg, f + 2)[0] for f in range(0, 0x30, 8)]
    seg = struct.unpack_from('<H', dseg, 0x42)[0]
    first_rsp_code = None
    rsp_code = []
    while seg:
        data = cseg[(seg - CODE_SEG) * 16:]
        uda = struct.unpack_from('<H', data, 0x20)[0] * 16
        frame = struct.unpack_from('<H', data, uda + 0x34)[0]
        cs = struct.unpack_from('<H', data, frame + 2)[0]
        rsp_code.append((cs * 16, seg * 16))
        if first_rsp_code is None:
            first_rsp_code = cs * 16
        seg = struct.unpack_from('<H', data, 0)[0]
    code.append((CODE_SEG * 16, first_rsp_code))  # the modules, back to back
    code += rsp_code
    ver_off = (mod_segs[0] - CODE_SEG) * 16 + INIT_VER  # the version message, up to its '$'
    ver = (CODE_SEG * 16 + ver_off, CODE_SEG * 16 + cseg.index(b'$', ver_off) + 1)

    def skipped(a):
        return any(s <= a < e for s, e in code) and not ver[0] <= a < ver[1]

    port_c = mem[CODE_SEG * 16:CODE_SEG * 16 + len(cseg)]
    diffs = [a for a in range(len(cseg)) if not skipped(CODE_SEG * 16 + a) and cseg[a] != port_c[a]]
    check("code half byte for byte, outside the 8086 code", [],
          [f'{CODE_SEG * 16 + a:05X}: {cseg[a]:02X}/{port_c[a]:02X}' for a in diffs[:10]])
    compared = sum(1 for a in range(len(cseg)) if not skipped(CODE_SEG * 16 + a))
    print(f"      ({compared} code-half bytes compared: the RSP data segments, padding, the version message)")
    check("INIT.SUP's version message", cseg[ver[0] - CODE_SEG * 16:ver[1] - CODE_SEG * 16],
          port_c[ver[0] - CODE_SEG * 16:ver[1] - CODE_SEG * 16])
    end = sysdat * 16 + len(dseg)
    check("nothing written past SYSDAT", True, not any(mem[end:]))
    check("nothing written below 1000:0", True, not any(mem[:CODE_SEG * 16]))

    if failures:
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
