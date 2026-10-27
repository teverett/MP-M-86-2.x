"""Shared real-memory reference harness for the BDOS tests that run with
SYSDAT at a nonzero segment (test_file4.py, test_bdosif.py,
test_bdos_real.py). test_file1-3.py keep SYSDAT at segment 0; this
harness is what catches anything that still assumes DS = 0.

The reference is src-assem/kernel/BDOS.A86 itself -- the whole real module
(BDOSIF + FILE1-4 + DATA.BDO + the real UDA.FMT/SYSDAT.FMT). The combined
SYSDAT+DATA.BDO data segment is the BDOS's DS (SUPIF enters the BDOS with
DS = sysdat), loaded from the module's real assembled dseg bytes at
SYSDAT, so every DATA.BDO field starts at its real initial value. ES is
DS inside a BDOS function (getdiskmx does `push ds ! pop es`) but the
running UDA for the functions BDOSIF dispatches directly (`nomx`), which
read u_* fields through ES; `run` takes which one. SS:SP is the running
UDA at `ulen`.

BDOSIF's two far calls are boundary stubs via tools/i86sim.py's
`far_call_hooks`: `mpmif` (`callf cs:dword ptr .supervisor` -- console
messages, the disk mutex, flags) returns at once, and `xiosif` (`callf
dword ptr .xiosmod` -- every disk call) returns AX = BX = 0, the values
src-c/kernel/file1.c's header fixes for them (seldsk "fails", read/write
succeed).
"""
import ctypes
import os

from memref import (REPO_ROOT, KERNEL_C, SIM_MEM, PHYS_MEM, ULEN, UDA_STACK, phys, w16, r16,  # noqa: F401
                    build_lib, Checker, c_buffer)

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

BDOS_A86 = os.path.join(REPO_ROOT, 'src-assem', 'kernel', 'BDOS.A86')
BDOS_C = [os.path.join(KERNEL_C, f) for f in ('bdosif.c', 'file1.c', 'file2.c', 'file3.c', 'file4.c', 'que1.c', 'que2.c')]
SYSDAT = 0x0300     # the BDOS's DS: SYSDAT+DATA.BDO
UDA = 0x0800        # running process's UDA (and system-call stack)
USER = 0x0900       # the caller's segment: parameter segment / DMA
PD_RUN = 0x1100     # rlr, a DS offset past the 1000h-byte data segment


def ds(off):
    return phys(SYSDAT, off)


def us(off):
    return phys(USER, off)


class BdosRef:
    def __init__(self, extra_syms=()):
        asm = Assembler()
        asm.assemble(BDOS_A86, [os.path.join(REPO_ROOT, 'src-assem', 'kernel')])
        self.code = asm.segment_bytes('cseg')
        self.data = asm.segment_bytes('dseg')
        self.sym = asm.symtab
        assert len(self.code) <= ds(0), "code must not overlap SYSDAT"
        assert len(self.data) <= PD_RUN

        def callf_in(label, n):
            start = self.sym[label].value
            hits = [i for i in range(start, start + n)
                    if self.code[i] == 0xFF and (self.code[i + 1] >> 3) & 7 == 3]
            assert len(hits) == 1, (label, hits)
            return hits[0] - 1 if self.code[hits[0] - 1] == 0x2E else hits[0]

        self.mpmif_callf = callf_in('mpmif', 16)
        self.xiosif_callf = callf_in('xiosif', 16)
        self.skip = (set(range(len(self.code))) |
                     set(range(phys(UDA, UDA_STACK[0]), phys(UDA, UDA_STACK[1]))))
        self.calls = []

    def v(self, name):
        return self.sym[name].value

    def base_memory(self):
        """DATA.BDO's real initial values at SYSDAT; rlr = PD_RUN with its
        UDA at UDA."""
        mem = bytearray(SIM_MEM)
        mem[ds(0):ds(len(self.data))] = self.data
        w16(mem, ds(self.v('rlr')), PD_RUN)
        w16(mem, ds(PD_RUN + self.v('p_uda')), UDA)
        return mem

    def _mpmif(self, sim):
        self.calls.append(('mpm', sim.regs['cx']))

    def _xiosif(self, sim):
        self.calls.append(('xios', sim.regs['ax'] & 0xFF))
        sim.regs['ax'] = sim.regs['bx'] = 0

    def run(self, entry, mem, regs, es_uda=False, stop_addrs=(), stubs=()):
        image = bytearray(mem)
        image[0:len(self.code)] = self.code
        w16(image, self.v('sysdat'), SYSDAT)
        self.calls = []
        start = entry if isinstance(entry, int) else self.v(entry)
        sim = I86Sim(image, {**regs, 'ds': SYSDAT, 'es': UDA if es_uda else SYSDAT, 'ss': UDA, 'sp': ULEN,
                             'cs': 0},
                     start, segmented=True, stop_addrs=set(stop_addrs), call_stub_addrs=set(stubs),
                     far_call_hooks={self.mpmif_callf: self._mpmif, self.xiosif_callf: self._xiosif})
        sim.run(max_steps=500000)
        return sim.regs, image


u16 = ctypes.c_uint16
u8 = ctypes.c_uint8
c_u8p = ctypes.POINTER(ctypes.c_uint8)
c_u16p = ctypes.POINTER(ctypes.c_uint16)
