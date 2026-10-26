"""Shared real-memory reference harness for the CIO module's differential
tests (test_console.py, test_chario.py, test_cioif.py,
test_cioif_entry.py).

The reference is src/kernel/CIO.A86 itself -- the whole real module
(CIOIF/CONSOLE/CHARIO + the real UDA.FMT/SYSDAT.FMT). It runs the way a
system call reaches CIO: DS = SYSDAT, ES = the running process's UDA,
SS:SP = that UDA at `ulen` (see tests/src-c/memref.py, which this
mirrors and shares helpers with).

CIO's two far calls are modeled with tools/i86sim.py's `far_call_hooks`:

- `mpmif` (`callf cs:dword ptr .supervisor`, into SUPIF.SUP): for
  f_findpdname (conassign_entry's name search) the hook applies SUPIF's
  protocol (u_wrkseg = the caller's DS, DS = SYSDAT) and runs the real
  src/kernel/FINDPD.RTM -- assembled separately and loaded at FINDPD_BASE
  in the same memory -- in a nested sim. f_sleep/f_wakeup/f_dispatch/
  f_terminate are the usual no-op boundary stubs.
- `xiosif` (`callf dword ptr .xiosmod`): the XIOS boundary, driven by
  an `Xios` script -- the same one chario.c's XIOS hook (cio_set_xios)
  gets on the C side. `const` reports ready while scripted input
  remains, `conin` returns the next scripted byte (0 when there's none),
  output is just recorded. Both sides log every call, as (function,
  char, device) for conout/list and (function, console) for const/conin,
  and a case compares the two logs.

The mpmif hook records the function codes it saw (`calls`).
"""
import ctypes
import os
import sys

from memref import (REPO_ROOT, KERNEL_C, SIM_MEM, PHYS_MEM, SYSDAT, UDA, WRKSEG, PD_RUN, ULEN,  # noqa: F401
                    UDA_STACK, phys, sd, wk, w16, r16, build_lib, BxCx, c_buffer)
import memref

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

CIO_A86 = os.path.join(REPO_ROOT, 'src', 'kernel', 'CIO.A86')
FINDPD_WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'findpd_wrapper.A86')
CIO_C = [os.path.join(KERNEL_C, f) for f in ('cioif.c', 'console.c', 'chario.c', 'findpd.c')]
FINDPD_BASE = 0xF000
CCB_TABLE = 0x3000      # SYSDAT offset; SYSDAT's `ccb` points here
NCNS, NCONDEV, NLSTDEV = 2, 2, 2

SYMS = ('init', 'entry', 'mpmif', 'xiosif', 'sysdat', 'supervisor', 'supmod', 'rlr', 'ccb', 'thrdrt',
        'ncns', 'ncondev', 'nlstdev', 'u_wrkseg', 'f_findpdname',
        'conin_entry', 'conout_entry', 'rconin_entry', 'rconout_entry', 'listout_entry', 'dirio_entry',
        'conwrite_entry', 'conread_entry', 'constat_entry', 'conattach_entry', 'condetach_entry',
        'setdefcon_entry', 'conassign_entry', 'getdefcon_entry', 'conprint_entry', 'lstattach_entry',
        'lstdetach_entry', 'setdeflst_entry', 'clstattch_entry', 'cconattch_entry', 'getdeflst_entry',
        'mimic_entry', 'unmimic_entry', 'getccbadr',
        'p_link', 'p_thread', 'p_stat', 'p_flag', 'p_name', 'p_uda', 'p_cns', 'p_lst', 'p_scratch',
        'pf_raw', 'pf_ctld', 'pf_ctlc',
        'c_attach', 'c_queue', 'c_flag', 'c_strtcol', 'c_column', 'c_nchar', 'c_mimic', 'c_msource', 'ccblen',
        'cf_listcp', 'cf_compc', 'acb_cns', 'acb_match', 'acb_pd', 'acb_name')


IO_CONST, IO_CONIN, IO_CONOUT, IO_LIST = 0, 1, 2, 3


class Xios:
    """A scripted XIOS: `input` is what conin hands out, in order."""
    def __init__(self, data=b''):
        self.data = list(data)
        self.log = []

    def call(self, func, cl, dl):
        if func in (IO_CONOUT, IO_LIST):
            self.log.append((func, cl, dl))
            return 0
        self.log.append((func, cl))
        if func == IO_CONST:
            return 1 if self.data else 0
        return self.data.pop(0) if self.data else 0


XIOS_FN = ctypes.CFUNCTYPE(ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8)


def c_xios(lib, data=b''):
    """install a fresh Xios(data) as the C side's XIOS; returns (it, keepalive)"""
    x = Xios(data)
    fn = XIOS_FN(lambda func, cl, dl: x.call(func, cl, dl))
    lib.cio_set_xios(fn)
    return x, fn


class CioRef:
    def __init__(self):
        asm = Assembler()
        asm.assemble(CIO_A86, [os.path.join(REPO_ROOT, 'src', 'kernel')])
        self.code = asm.segment_bytes('cseg')
        self.s = {name: asm.symtab[name].value for name in SYMS}
        s = self.s
        assert s['sysdat'] == 6
        assert len(self.code) <= sd(0), "code must not overlap SYSDAT"

        def callf_at(label, n):
            hits = [i for i in range(s[label], s[label] + n)
                    if self.code[i] == 0xFF and (self.code[i + 1] >> 3) & 7 == 3]
            assert len(hits) == 1, (label, hits)
            return hits[0] - 1 if self.code[hits[0] - 1] == 0x2E else hits[0]

        self.mpmif_callf = callf_at('mpmif', 3)
        self.xiosif_callf = callf_at('xiosif', 3)
        retf = [i for i in range(s['entry'], s['mpmif']) if self.code[i] == 0xCB]
        assert len(retf) == 1, retf
        self.entry_retf = retf[0]

        fasm = Assembler()
        fasm.assemble(FINDPD_WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
        assert fasm.symtab['findpdname_entry'].value == 0
        self.findpd = fasm.segment_bytes('cseg')
        assert FINDPD_BASE + len(self.findpd) <= SIM_MEM

        self.skip = (set(range(len(self.code))) | set(range(FINDPD_BASE, FINDPD_BASE + len(self.findpd))) |
                     set(range(phys(UDA, UDA_STACK[0]), phys(UDA, UDA_STACK[1]))))
        self.calls = []
        self.xios = Xios()

    def ccb(self, n, field=None):
        """physical address of CCB n (or one of its fields)"""
        return sd(CCB_TABLE + n * self.s['ccblen'] + (self.s[field] if field else 0))

    def pd(self, addr, field):
        return sd(addr + self.s[field])

    def base_memory(self):
        """rlr = PD_RUN (console 0, list device NCONDEV = list 0) with its
        UDA at UDA (u_wrkseg = WRKSEG); every CCB unattached, nothing
        mimicked, nothing buffered."""
        s = self.s
        mem = bytearray(SIM_MEM)
        w16(mem, sd(s['rlr']), PD_RUN)
        w16(mem, self.pd(PD_RUN, 'p_uda'), UDA)
        w16(mem, phys(UDA, s['u_wrkseg']), WRKSEG)
        mem[self.pd(PD_RUN, 'p_cns')] = 0
        mem[self.pd(PD_RUN, 'p_lst')] = NCONDEV
        mem[self.pd(PD_RUN, 'p_name'):self.pd(PD_RUN, 'p_name') + 8] = b'RUNNER  '
        w16(mem, sd(s['ccb']), CCB_TABLE)
        mem[sd(s['ncns'])] = NCNS
        mem[sd(s['ncondev'])] = NCONDEV
        mem[sd(s['nlstdev'])] = NLSTDEV
        for n in range(NCONDEV + NLSTDEV):
            mem[self.ccb(n, 'c_mimic')] = 0xFF
            mem[self.ccb(n, 'c_msource')] = 0xFF
        return mem

    def _mpmif(self, sim):
        fn = sim.regs['cx']
        self.calls.append(fn)
        if fn != self.s['f_findpdname']:
            return
        wrk = phys(sim.regs['es'], self.s['u_wrkseg'])
        saved = r16(sim.mem, wrk)
        w16(sim.mem, wrk, sim.regs['ds'])
        inner = I86Sim(sim.mem, {'bx': sim.regs['bx'], 'dx': sim.regs['dx'], 'ds': SYSDAT,
                                 'es': sim.regs['es'], 'ss': sim.regs['ss'], 'sp': sim.regs['sp'] - 0x10,
                                 'cs': 0}, FINDPD_BASE, segmented=True)
        inner.run(max_steps=100000)
        w16(sim.mem, wrk, saved)
        sim.regs['bx'], sim.regs['cx'] = inner.regs['bx'], inner.regs['cx']
        sim.regs['ax'] = inner.regs['bx']

    def _xiosif(self, sim):
        r = self.xios.call(sim.regs['ax'] & 0xFF, sim.regs['cx'] & 0xFF, sim.regs['dx'] & 0xFF)
        sim.regs['ax'] = sim.regs['bx'] = r

    def run(self, entry, mem, regs, stop_addrs=(), xios_input=b''):
        image = bytearray(mem)
        image[0:len(self.code)] = self.code
        image[FINDPD_BASE:FINDPD_BASE + len(self.findpd)] = self.findpd
        w16(image, self.s['sysdat'], SYSDAT)
        self.calls = []
        self.xios = Xios(xios_input)
        sim = I86Sim(image, {**regs, 'ds': SYSDAT, 'es': UDA, 'ss': UDA, 'sp': ULEN, 'cs': 0},
                     self.s[entry], segmented=True, stop_addrs=set(stop_addrs),
                     far_call_hooks={self.mpmif_callf: self._mpmif, self.xiosif_callf: self._xiosif})
        sim.run(max_steps=200000)
        return sim.regs, image


class Checker(memref.Checker):
    pass


def lib_with(files=CIO_C):
    return build_lib(files)


u16 = ctypes.c_uint16
u8 = ctypes.c_uint8
c_u8p = ctypes.POINTER(ctypes.c_uint8)
