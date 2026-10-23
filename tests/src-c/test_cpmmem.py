#!/usr/bin/env python3
"""Differential test for src-c/kernel/cpmmem.c against src/kernel/
CPMMEM.MEM's maxmem_entry, absmax_entry, cpmalloc_entry, cpmabsa_entry,
cpmfree_entry (+ internal getmemory, free_memory). freeall_entry is not
tested -- see cpmmem.c's own header comment for why it's deferred, not
ported."""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'cpmmem_wrapper.A86')
CPMMEM_C = os.path.join(REPO_ROOT, 'src-c', 'kernel', 'cpmmem.c')
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
SD_RLR = 0xA000
MPMIF_STUB = 0x9000

MCB_BASE, MCB_LENGTH, MCB_EXT = 0, 2, 4
MS_LINK, MS_START, MS_LENGTH, MS_FLAGS, MS_MAU = 0, 2, 4, 6, 8
P_MEM = 0x16
MF_LOAD = 0x01
F_MALLOC = 0x80


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in (
        'maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, CPMMEM_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    mem = bytearray(0x10000)
    mem_w16(mem, SD_RLR, 0x8000)  # the running process's own PD
    return mem


def run_reference(code, entry, mem, regs):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, call_stub_addrs={MPMIF_STUB})
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        for name in ('maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry'):
            fn = getattr(lib, name)
            fn.restype = BxCx
            fn.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        def call(name, mem, dx):
            buf = to_ctypes(mem)
            r = getattr(lib, name)(ctypes.cast(buf, c_u8p), ctypes.c_uint16(dx))
            return (r.bx, r.cx), bytes(buf)

        # -- every allocator entry always "fails" under the boundary-stub
        # convention (getmemory's own f_malloc code never changes) --
        for name, setup in (
            ('maxmem_entry', lambda mem, MCB: mem_w16(mem, MCB + MCB_LENGTH, 0x40)),
            ('absmax_entry', lambda mem, MCB: (mem_w16(mem, MCB + MCB_BASE, 0x1000), mem_w16(mem, MCB + MCB_LENGTH, 0x40))),
            ('cpmalloc_entry', lambda mem, MCB: mem_w16(mem, MCB + MCB_LENGTH, 0x40)),
            ('cpmabsa_entry', lambda mem, MCB: (mem_w16(mem, MCB + MCB_BASE, 0x1000), mem_w16(mem, MCB + MCB_LENGTH, 0x40))),
        ):
            mem = base_memory()
            MCB = 0x5000
            setup(mem, MCB)
            ref_regs, _ = run_reference(code, entries[name], mem, {'dx': MCB})
            port, _ = call(name, mem, MCB)
            check(f"{name}: boundary-stub allocation always fails", (ref_regs['bx'], ref_regs['cx']), port)

        # -- cpmfree_entry: mcb_ext != 0xFF -- tail-jumps straight into
        # free_memory, which always "fails" too (f_memfree, nonzero) --
        mem = base_memory()
        MCB = 0x5000
        mem_w16(mem, MCB + MCB_BASE, 0x3000)
        mem[MCB + MCB_EXT] = 0x00
        ref_regs, _ = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB, 'bx': 0})
        port, _ = call('cpmfree_entry', mem, MCB)
        check("cpmfree_entry: single-segment free, boundary stub always fails", (ref_regs['bx'], ref_regs['cx']), port)

        # -- cpmfree_entry: mcb_ext == 0xFF, empty p_mem list -- cpmf_next
        # immediately hits si==0 -> success, no segment visited --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        mem_w16(mem, RLR + P_MEM + MS_LINK, 0)
        MCB = 0x5000
        mem[MCB + MCB_EXT] = 0xFF
        ref_regs, _ = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB})
        port, _ = call('cpmfree_entry', mem, MCB)
        check("cpmfree_entry: free-all, empty p_mem list", (ref_regs['bx'], ref_regs['cx']), port)

        # -- cpmfree_entry: mcb_ext == 0xFF, one Load segment on the list --
        # try_seg's "and ms_flags[si],mf_load" clobbers ms_flags down to
        # just the mf_load bit, then skips it (mov bx,si) and moves on to
        # the next (empty) link -- success, but ms_flags visibly changed --
        mem = base_memory()
        RLR = mem_r16(mem, SD_RLR)
        MD = 0x4400
        mem_w16(mem, RLR + P_MEM + MS_LINK, MD)
        mem_w16(mem, MD + MS_FLAGS, MF_LOAD | 0x08)  # a Load segment, plus some other bit
        mem_w16(mem, MD + MS_LINK, 0)
        MCB = 0x5000
        mem[MCB + MCB_EXT] = 0xFF
        ref_regs, ref_img = run_reference(code, entries['cpmfree_entry'], mem, {'dx': MCB})
        port, port_img = call('cpmfree_entry', mem, MCB)
        ref = ((ref_regs['bx'], ref_regs['cx']), mem_r16(ref_img, MD + MS_FLAGS))
        got = (port, mem_r16(port_img, MD + MS_FLAGS))
        check("cpmfree_entry: free-all skips a Load segment, clobbering its flags", ref, got)

        # Note: mcb_ext == 0xFF with a *non*-Load segment on the list
        # isn't exercised here -- free_seg's own "jmps cpmf_next" leaves
        # bx unadvanced on failure, which under the boundary-stub
        # convention (free_memory always "fails") spins forever. Same
        # class of issue as QUE1.RTM's cioattach "must actually wait"
        # path; see cpmmem.c's own header comment on cpmfree_entry.

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
