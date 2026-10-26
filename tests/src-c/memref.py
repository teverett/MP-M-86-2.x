"""Shared real-memory reference harness for the MEM module's differential
tests (test_util_mem.py, test_ml.py, test_mau.py, test_share.py,
test_memory.py, test_cpmmem.py, test_memif.py, test_memif_entry.py).

The reference is src/kernel/MEM.A86 itself -- the whole real module
(MEMIF/CPMMEM/MEMORY/SHARE/MAU/ML/UTIL + the real UDA.FMT/SYSDAT.FMT), so
every field offset and code path is the genuine one. It runs with
tools/i86sim.py's segmented=True, set up the way a system call reaches
the MEM module: DS = SYSDAT, ES = the running process's UDA, and SS:SP =
that UDA at `ulen` (SUPIF.SUP's user_entry: `mov ss,es ! mov sp,ulen`),
so the asm's own on-stack parameter blocks really land in the UDA's
stack area.

MEM's `mpmif` is `callf cs:dword ptr .supervisor`, into SUPIF.SUP. That
one far call is modeled with tools/i86sim.py's `far_call_hooks`: for a
MEM function the hook applies SUPIF's mpmif protocol (u_wrkseg = the
caller's DS around the call, DS = SYSDAT) and re-enters MEM's real
`entry` in a nested sim on the same memory, with cl = the function's
MEM functab index (SUP's func1 mapping for the user-level
f_freemem/f_malloc/f_memfree, the low byte for the mem-module ones).
Anything else (entry's own f_qread/f_qwrite/f_terminate, freeall's
f_terminate) is the usual no-op boundary stub.

The C side gets a 1MB buffer (physical memory) and the same SYSDAT
segment. A case's whole-image diff leaves out the code and the UDA's
stack area (62h..100h), where the asm's frames and the C port's fixed
UDA_STK_* blocks (src-c/kernel/defs.h) both live.
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

MEM_A86 = os.path.join(REPO_ROOT, 'src', 'kernel', 'MEM.A86')
KERNEL_C = os.path.join(REPO_ROOT, 'src-c', 'kernel')
MEM_C = [os.path.join(KERNEL_C, f) for f in
         ('memif.c', 'cpmmem.c', 'memory.c', 'share.c', 'mau.c', 'ml.c', 'util_mem.c')]

SIM_MEM = 0x10000
PHYS_MEM = 0x100000
SYSDAT = 0x0100     # DS; MDs/PDs/lists are SYSDAT offsets
UDA = 0x0600        # running process's UDA (ES) and system-call stack (SS)
WRKSEG = 0x0700     # its u_wrkseg; MPBs/MCBs/SPBs/MFPBs/MAFs live here
PD_RUN = 0x2000     # rlr
ULEN = 0x100
UDA_STACK = (0x62, ULEN)  # UDA offsets past its last field (u_stat_sav at 61h)

SYMS = ('init', 'entry', 'mpmif', 'sysdat', 'supervisor', 'supmod', 'rlr', 'thrdrt', 'mdul', 'mal', 'mfl',
        'mxmemowner', 'mxmemcount', 'u_wrkseg',
        'maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry',
        'freeall_entry', 'malloc_entry', 'mfree_entry', 'share_entry', 'maualloc_entry',
        'maufree_entry', 'mlalloc_entry', 'mlfree_entry', 'getmd', 'freemd',
        'f_freemem', 'f_malloc', 'f_memfree', 'f_share', 'f_maualloc', 'f_maufree',
        'f_mlalloc', 'f_mlfree',
        'p_link', 'p_thread', 'p_flag', 'p_mem', 'p_uda', 'pf_tempkeep', 'pf_ctlc',
        'm_link', 'm_start', 'm_length', 'm_plist',
        'ms_link', 'ms_start', 'ms_length', 'ms_flags', 'ms_mau',
        'mpb_start', 'mpb_min', 'mpb_max', 'mpb_pdadr', 'mpb_flags',
        'mfpb_start', 'mfpb_pd', 'maf_mau', 'maf_sat', 'maf_start',
        'spb_opd', 'spb_rpd', 'spb_start', 'sat_start', 'sat_length', 'sat_nall', 'satlen',
        'mf_load', 'mf_share', 'mf_code')


def phys(seg, off=0):
    return seg * 16 + off


def sd(off):
    return phys(SYSDAT, off)


def wk(off):
    return phys(WRKSEG, off)


def w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


class MemRef:
    def __init__(self):
        asm = Assembler()
        asm.assemble(MEM_A86, [os.path.join(REPO_ROOT, 'src', 'kernel')])
        self.code = asm.segment_bytes('cseg')
        self.s = {name: asm.symtab[name].value for name in SYMS}
        s = self.s
        assert s['sysdat'] == 6
        assert len(self.code) <= sd(0), "code must not overlap SYSDAT"
        callf = [i for i in range(s['mpmif'], s['mpmif'] + 3)
                 if self.code[i] == 0xFF and (self.code[i + 1] >> 3) & 7 == 3]
        assert len(callf) == 1, callf
        self.callf = callf[0] - 1 if self.code[callf[0] - 1] == 0x2E else callf[0]
        retf = [i for i in range(s['entry'], s['mpmif']) if self.code[i] == 0xCB]
        assert len(retf) == 1, retf
        self.entry_retf = retf[0]
        functab = ('maxmem_entry', 'absmax_entry', 'cpmalloc_entry', 'cpmabsa_entry', 'cpmfree_entry',
                   'freeall_entry', 'malloc_entry', 'mfree_entry', 'share_entry', 'maualloc_entry',
                   'maufree_entry', 'mlalloc_entry', 'mlfree_entry')
        self.funcs = {s['f_freemem']: functab.index('cpmfree_entry'),
                      s['f_malloc']: functab.index('malloc_entry'),
                      s['f_memfree']: functab.index('mfree_entry')}
        for f in ('f_share', 'f_maualloc', 'f_maufree', 'f_mlalloc', 'f_mlfree'):
            self.funcs[s[f]] = s[f] & 0xFF
        self.skip = set(range(len(self.code))) | set(range(phys(UDA, UDA_STACK[0]), phys(UDA, UDA_STACK[1])))
        self.calls = []

    def base_memory(self):
        """rlr = PD_RUN with its UDA at UDA (u_wrkseg = WRKSEG), already
        holding the MEM mutex as if called through `entry`."""
        s = self.s
        mem = bytearray(SIM_MEM)
        w16(mem, sd(s['rlr']), PD_RUN)
        w16(mem, sd(PD_RUN + s['p_uda']), UDA)
        w16(mem, phys(UDA, s['u_wrkseg']), WRKSEG)
        w16(mem, sd(s['mxmemowner']), PD_RUN)
        mem[sd(s['mxmemcount'])] = 1
        return mem

    def _supif(self, sim):
        fn = sim.regs['cx']
        self.calls.append(fn)
        idx = self.funcs.get(fn)
        if idx is None:
            return
        wrk = phys(sim.regs['es'], self.s['u_wrkseg'])
        saved = r16(sim.mem, wrk)
        w16(sim.mem, wrk, sim.regs['ds'])
        inner = I86Sim(sim.mem, {'cx': idx, 'dx': sim.regs['dx'], 'bx': sim.regs['bx'], 'ds': SYSDAT,
                                 'es': sim.regs['es'], 'ss': sim.regs['ss'], 'sp': sim.regs['sp'] - 0x10,
                                 'cs': 0},
                       self.s['entry'], segmented=True, stop_addrs={self.entry_retf},
                       far_call_hooks={self.callf: self._supif})
        inner.run(max_steps=200000)
        w16(sim.mem, wrk, saved)
        sim.regs['bx'], sim.regs['cx'] = inner.regs['bx'], inner.regs['cx']
        sim.regs['ax'] = inner.regs['bx']

    def run(self, entry, mem, regs, stop_addrs=()):
        image = bytearray(mem)
        image[0:len(self.code)] = self.code
        w16(image, self.s['sysdat'], SYSDAT)
        self.calls = []
        sim = I86Sim(image, {**regs, 'ds': SYSDAT, 'es': UDA, 'ss': UDA, 'sp': ULEN, 'cs': 0},
                     self.s[entry], segmented=True, stop_addrs=set(stop_addrs),
                     far_call_hooks={self.callf: self._supif})
        sim.run(max_steps=200000)
        return sim.regs, image


def build_lib(files=MEM_C):
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    '-I', KERNEL_C, *files, '-o', lib_path], check=True)
    return lib_path


class BxCx(ctypes.Structure):
    _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]


class Checker:
    def __init__(self, ref):
        self.ref = ref
        self.failures = []

    def check(self, label, ref, port):
        status = "ok" if ref == port else "FAIL"
        print(f"[{status}] {label}: reference {ref} | port {port}")
        if status == "FAIL":
            self.failures.append(label)

    def compare(self, label, ref_img, port_buf, ref_result, port_result, snap=lambda img: ()):
        port = bytes(port_buf)
        self.check(label, (ref_result, snap(ref_img)), (port_result, snap(port)))
        diffs = [hex(a) for a in range(SIM_MEM) if a not in self.ref.skip and ref_img[a] != port[a]]
        self.check(f"{label}: no other bytes differ", [], diffs[:8])
        self.check(f"{label}: nothing written past 64KB", True, not any(port[SIM_MEM:]))

    def finish(self):
        if self.failures:
            print(f"\n{len(self.failures)} case(s) failed: {', '.join(self.failures)}")
            return 1
        print("\nall cases match.")
        return 0


def c_buffer(mem):
    buf = (ctypes.c_uint8 * PHYS_MEM)()
    ctypes.memmove(buf, bytes(mem), len(mem))
    return buf
