#!/usr/bin/env python3
"""Differential test for src-c/kernel/load.c's load_finish against a
tail portion of src/kernel/LOAD.SUP's `load:` routine (`lb_more`
through `load_exit`). See load.c's own header comment for why only
this portion is ported -- the front half depends on real,
not-yet-ported disk I/O.

The reference run enters mid-routine at the real `lb_more` label
(same technique as src-c/kernel/clock.c's own test, entering at
`clockloop`), with `si`/`cx` pre-set to what the real asm's own
immediately-preceding instructions ("mov si,offset ldtab ! mov
cx,load_nldtabents") would have left them as, and `load_8080`
pre-zeroed the same way ("mov load_8080,0" also precedes `lb_more`).
"""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'load_wrapper.A86')
LOAD_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'load.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0

SD_RLR = 0xA000
SD_LOAD_PD, SD_LOAD_FCB, SD_LOAD_NLDTABENTS = 0xA900, 0xA902, 0xA904
SD_LOAD_BASEP, SD_LOAD_8080, SD_LOAD_UDA, SD_LOAD_LSTK = 0xA906, 0xA908, 0xA909, 0xA90B
SD_LOAD_LDTAB = 0xA910
UDA_WRKSEG = 0xA012

LDTAB_START, LDTAB_MIN, LDTAB_MAX, LDTAB_TYPE = 0, 2, 4, 14
LDTABLEN = 17

P_UDA, P_STAT, P_PRIOR, P_USER, P_DSK, P_LDSK, P_LUSER = 0x10, 4, 5, 0x13, 0x12, 0x14, 0x15
PS_RUN = 0

BPG_CSEG, BPG_8080, BPG_ESEG = 3, 5, 15
U_DMA_OFST, U_DMA_SEG, U_INITCS, U_INITDS, U_INITES, U_INITSS, U_STACK_SP = 0, 4, 0x50, 0x52, 0x54, 0x56, 0x34
LS_OFFSET, LS_FLAGS, LS_ROFFSET, LS_RCSEG = 0x56, 0x5A, 0x5C, 0x5E

ULEN = 0x100

SYSDAT = 0x3000
USER_RETF_OFFSET = 0x0002  # user_retf's own resolved address in the wrapper
CS_VAL = 0x0000            # the wrapper's own cseg loads at absolute 0

PD_ADDR = 0x5000
FCB_ADDR = 0x0030          # offset within WRKSEG's own segment
WRKSEG = 0x0040

CODE_SEG, DATA_SEG, LSTK_SEG = 0x0200, 0x0300, 0x0400


def phys(seg):
    return (seg * 16) & 0xFFFF


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    addr &= 0xFFFF
    return mem[addr] | (mem[(addr + 1) & 0xFFFF] << 8)


def put_ldtab(mem, entry_no, start, minv, maxv, type_):
    off = SD_LOAD_LDTAB + entry_no * LDTABLEN
    mem_w16(mem, off + LDTAB_START, start)
    mem_w16(mem, off + LDTAB_MIN, minv)
    mem_w16(mem, off + LDTAB_MAX, maxv)
    mem[off + LDTAB_TYPE] = type_


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    lb_more = asm.symtab['lb_more'].value
    user_retf = asm.symtab['user_retf'].value
    return code, lb_more, user_retf


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *LOAD_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory(nldtabents):
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, WRKSEG)
    mem_w16(mem, SD_LOAD_NLDTABENTS, nldtabents)
    mem[SD_LOAD_8080] = 0
    return mem


MPMIF_STUB = 0x9000


def run_reference(code, entry, mem, regs):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    image[0] = SYSDAT & 0xFF  # `sysdat dw 0` sits at cseg offset 0
    image[1] = (SYSDAT >> 8) & 0xFF
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, segmented=True,
                 call_stub_addrs={MPMIF_STUB})
    sim.run()
    return sim.regs, image


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, lb_more, user_retf = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.load_finish.restype = BxCx
        lib.load_finish.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def run_both(mem):
            ref_regs, ref_img = run_reference(
                code, lb_more, mem, {'si': SD_LOAD_LDTAB, 'cx': mem_r16(mem, SD_LOAD_NLDTABENTS)})
            buf = to_ctypes(mem)
            got = lib.load_finish(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SYSDAT),
                                   ctypes.c_uint16(user_retf), ctypes.c_uint16(CS_VAL))
            port_img = bytes(buf)
            return (ref_regs['bx'], ref_regs['cx']), ref_img, (got.bx, got.cx), port_img

        def basep_snapshot(img, seg):
            base = phys(seg)
            return (mem[base + BPG_8080] if False else img[base + BPG_8080],
                    mem_r16(img, base + BPG_CSEG), mem_r16(img, base + BPG_ESEG),
                    img[base + 6], img[base + 7], img[base + 8],  # dlen (3 bytes)
                    mem_r16(img, base + 9))  # dseg

        def uda_snapshot(img, seg):
            base = phys(seg)
            return (mem_r16(img, base + U_DMA_OFST), mem_r16(img, base + U_DMA_SEG),
                    mem_r16(img, base + U_INITCS), mem_r16(img, base + U_INITDS),
                    mem_r16(img, base + U_INITES), mem_r16(img, base + U_INITSS),
                    mem_r16(img, base + U_STACK_SP))

        def lstk_snapshot(img, seg):
            base = phys(seg)
            return (mem_r16(img, base + LS_OFFSET), mem_r16(img, base + LS_FLAGS),
                    mem_r16(img, base + LS_ROFFSET), mem_r16(img, base + LS_RCSEG))

        # -- single Data-group .CMD, with a PD to initialize --
        # (bpg_cseg isn't test-injected: lbb_mre's own scan over ldtab is
        # what has to populate it, from the separate Code-group entry --
        # that's the real field this port's own differential test caught
        # a test-setup mistake on, not a /src or /src-c bug: a Data-group
        # .CMD still needs its own Code-group entry for bpg_cseg to ever
        # be filled in at all, same base page, different ldtab slot)
        mem = base_memory(3)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)  # UDA+LSTK entry
        put_ldtab(mem, 1, CODE_SEG, 8, 8, 1)
        put_ldtab(mem, 2, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem[PD_ADDR + P_USER] = 3
        mem[PD_ADDR + P_DSK] = 2
        mem_w16(mem, phys(WRKSEG) + FCB_ADDR, 0)  # fcb_dr == 0 -> keep p_dsk's copy
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)

        ref, ref_img, port, port_img = run_both(mem)
        ref_full = ref + basep_snapshot(ref_img, DATA_SEG) + \
            (ref_img[PD_ADDR + P_UDA] | (ref_img[PD_ADDR + P_UDA + 1] << 8),
             ref_img[PD_ADDR + P_STAT], ref_img[PD_ADDR + P_PRIOR],
             ref_img[PD_ADDR + P_LUSER], ref_img[PD_ADDR + P_LDSK]) + \
            uda_snapshot(ref_img, LSTK_SEG) + lstk_snapshot(ref_img, LSTK_SEG)
        port_full = port + basep_snapshot(port_img, DATA_SEG) + \
            (port_img[PD_ADDR + P_UDA] | (port_img[PD_ADDR + P_UDA + 1] << 8),
             port_img[PD_ADDR + P_STAT], port_img[PD_ADDR + P_PRIOR],
             port_img[PD_ADDR + P_LUSER], port_img[PD_ADDR + P_LDSK]) + \
            uda_snapshot(port_img, LSTK_SEG) + lstk_snapshot(port_img, LSTK_SEG)
        check("load_finish: Data-group .CMD, PD initialized", ref_full, port_full)

        # -- FCB has an explicit disk letter -> overrides p_ldsk --
        mem = base_memory(3)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)
        put_ldtab(mem, 1, CODE_SEG, 8, 8, 1)
        put_ldtab(mem, 2, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem[PD_ADDR + P_USER] = 3
        mem[PD_ADDR + P_DSK] = 2
        mem[phys(WRKSEG) + FCB_ADDR] = 5  # explicit disk letter
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)
        ref, ref_img, port, port_img = run_both(mem)
        ref_full = ref + (ref_img[PD_ADDR + P_LDSK],)
        port_full = port + (port_img[PD_ADDR + P_LDSK],)
        check("load_finish: explicit FCB disk letter overrides p_ldsk", ref_full, port_full)

        # -- 8080-model .CMD (only a Code group, no Data group) --
        mem = base_memory(2)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)
        put_ldtab(mem, 1, CODE_SEG, 0x08, 0x08, 1)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem[PD_ADDR + P_USER] = 0
        mem[PD_ADDR + P_DSK] = 0
        mem_w16(mem, phys(WRKSEG) + FCB_ADDR, 0)
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)
        ref, ref_img, port, port_img = run_both(mem)
        ref_full = ref + basep_snapshot(ref_img, CODE_SEG) + lstk_snapshot(ref_img, LSTK_SEG)
        port_full = port + basep_snapshot(port_img, CODE_SEG) + lstk_snapshot(port_img, LSTK_SEG)
        check("load_finish: 8080-model (code-only) .CMD", ref_full, port_full)

        # -- no PD to initialize (load_pd == 0) -- just the Base Page --
        mem = base_memory(1)
        put_ldtab(mem, 0, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, 0)
        ref, ref_img, port, port_img = run_both(mem)
        check("load_finish: load_pd == 0 -> base page only", ref, port)

        # -- no CSEG anywhere -> e_no_cseg --
        mem = base_memory(1)
        put_ldtab(mem, 0, DATA_SEG, 0x10, 0x10, 3)  # neither type 1 nor 2
        mem_w16(mem, SD_LOAD_PD, 0)
        ref, ref_img, port, port_img = run_both(mem)
        check("load_finish: no code/data group -> e_no_cseg", ref, port)

        # -- Data group's own bpg_cseg is 0 -> e_no_cseg (from the PD path) --
        mem = base_memory(2)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)
        put_ldtab(mem, 1, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem_w16(mem, phys(WRKSEG) + FCB_ADDR, 0)
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)
        ref, ref_img, port, port_img = run_both(mem)
        check("load_finish: bpg_cseg == 0 -> e_no_cseg", ref, port)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
