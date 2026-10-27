"""Shared real-memory reference harness for the Supervisor (SUP) tests --
and, through it, a small model of the whole system.

SYSDAT is the real src-assem/kernel/SYSDAT.A86 image (SYSTEM.DAT + DATA.BDO).
The reference is src-assem/kernel/SUP.A86 itself (INIT/IDLE/TICK/SUPIF/SYSFUNC/
COMMAND/LOAD/PARSE/RPL + the real UDA.FMT/SYSDAT.FMT), run the way a
system call reaches it: DS = SYSDAT (SUPIF's mpmif does `mov
ds,cs:sysdat`), ES = the running process's UDA, SS:SP = that UDA at `ulen`.
Optionally the other kernel modules -- the real RTM.A86, MEM.A86, CIO.A86,
BDOS.A86 -- are loaded too, each at its own code segment, all sharing the
one SYSDAT (as in the real system: every module's DS is SYSDAT). The
module table and each module's own `cs:sysdat`/`cs:supervisor` cells are
filled in as the modules' real `init`s would.

Far calls are modeled with tools/i86sim.py's `far_call_hooks`, following
the real routing:

- SUPIF `func1`'s `callf dword ptr module_table[si]` (a function in
  another module) runs that module's real `entry` in a nested sim --
  unless (module, function) is in `boundary`, the dispatcher-level calls
  the C port treats as no-op boundary stubs (sleep/dispatch/terminate...),
  or the module isn't loaded, in which case it's a no-op stub too. A
  test can also route a function to a Python handler (`handlers`), for a
  boundary with modeled effects (e.g. a scripted disk read).
- each loaded module's own `mpmif` (`callf cs:dword ptr .supervisor`)
  runs SUP's real `entry` the same way.
- every `xiosif` callf goes to `xios(sim)` (default: AX = BX = 0).

i86sim has no RETF, so each nested run stops at its entry routine's own
RETF. `calls` records every cross-module call as (module, function).
"""
import os

from memref import (REPO_ROOT, KERNEL_C, SIM_MEM, PHYS_MEM, ULEN, UDA_STACK, phys, w16, r16,  # noqa: F401
                    build_lib, Checker, BxCx, c_buffer)

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

KDIR = os.path.join(REPO_ROOT, 'src-assem', 'kernel')
CODE_SEG = {'SUP': 0x0000, 'RTM': 0x0140, 'MEM': 0x0240, 'CIO': 0x02F0, 'BDOS': 0x0380}
MOD_NUM = {'SUP': 1, 'RTM': 2, 'MEM': 3, 'CIO': 4, 'BDOS': 5}
SYSDAT = 0x0600     # DS of every module
UDA = 0x0900        # running process's UDA (ES, and the system-call stack)
WRKSEG = 0x0A00     # its u_wrkseg
PD_RUN = 0x1100     # rlr (a SYSDAT offset past the 1000h-byte data segment)
USER_RET = 0xFFF0   # user_call's return address in the SUP: where its nested run stops


def sd(off):
    return phys(SYSDAT, off)


def wk(off):
    return phys(WRKSEG, off)


def _callfs(code, start, n):
    hits = [i for i in range(start, start + n) if code[i] == 0xFF and (code[i + 1] >> 3) & 7 == 3]
    return [h - 1 if code[h - 1] in (0x2E, 0x26, 0x3E) else h for h in hits]


def _retfs(code, start, end):
    return [i for i in range(start, end) if code[i] == 0xCB]


class Module:
    def __init__(self, name):
        asm = Assembler()
        asm.assemble(os.path.join(KDIR, f'{name}.A86'), [KDIR])
        self.name = name
        self.code = asm.segment_bytes('cseg')
        self.data = asm.segment_bytes('dseg')
        self.sym = asm.symtab
        self.seg = CODE_SEG[name]
        self.base = phys(self.seg)

    def v(self, name):
        return self.sym[name].value


class SysRef:
    def __init__(self, modules=('SUP',)):
        self.mods = {n: Module(n) for n in modules}
        self.sup = self.mods['SUP']
        s = self.sup
        order = sorted(self.mods.values(), key=lambda m: m.base)
        for a, b in zip(order, order[1:]):
            assert a.base + len(a.code) <= b.base, (a.name, b.name)
        assert order[-1].base + len(order[-1].code) <= sd(0)
        # SYSDAT image: the real src-assem/kernel/SYSDAT.A86 (SYSTEM.DAT's
        # GENSYS-initialized tables -- sysent, flags, the stock PDs -- plus
        # DATA.BDO), the same bytes the /src-assem build loads
        asm = Assembler()
        asm.assemble(os.path.join(KDIR, 'SYSDAT.A86'), [KDIR])
        self.data = asm.segment_bytes('dseg')
        self.sysdat_sym = asm.symtab
        assert len(self.data) <= PD_RUN

        # far-call sites
        self.hooks = {}
        # func1 has two callfs: `.netmod` (never taken: no network module)
        # and the module-table one, last before `entry`
        mt = _callfs(s.code, s.v('func1'), s.v('entry') - s.v('func1'))
        assert len(mt) == 2, mt
        self.hooks[s.base + mt[-1]] = self._module_call
        for m in self.mods.values():
            if m.name != 'SUP' and 'mpmif' in m.sym:
                for h in _callfs(m.code, m.v('mpmif'), 0x10)[:1]:
                    self.hooks[m.base + h] = self._sup_call
            if 'xiosif' in m.sym:
                for h in _callfs(m.code, m.v('xiosif'), 0x14)[:1]:
                    self.hooks[m.base + h] = self._xios_call
        # each module entry's own RETF(s): between `entry` and `mpmif`
        self.entry_retfs = {}
        for m in self.mods.values():
            r = _retfs(m.code, m.v('entry'), m.v('mpmif'))
            assert r, m.name
            self.entry_retfs[m.name] = {m.base + x for x in r}
        # {(module number, function)} -> no-op stub. By default the RTM's
        # scheduler-level functions (poll, flag wait, delay, dispatch,
        # terminate, create process, abort, sleep, wakeup): every C module
        # treats them as boundary stubs
        self.boundary = {(MOD_NUM['RTM'], f) for f in (1, 2, 11, 12, 13, 14, 17, 18, 19)}
        self.handlers = {}        # {(module number, function): fn(sim)}
        self.xios = lambda sim: sim.regs.update(ax=0, bx=0)
        self.calls = []
        self.skip = set()
        for m in self.mods.values():
            self.skip |= set(range(m.base, m.base + len(m.code)))
        self.skip |= set(range(phys(UDA, UDA_STACK[0]), phys(UDA, UDA_STACK[1])))

    def v(self, name):
        return self.sup.v(name)

    def base_memory(self):
        s = self.sup
        mem = bytearray(SIM_MEM)
        mem[sd(0):sd(len(self.data))] = self.data
        w16(mem, sd(s.v('rlr')), PD_RUN)
        w16(mem, sd(PD_RUN + s.v('p_uda')), UDA)
        w16(mem, phys(UDA, s.v('u_wrkseg')), WRKSEG)
        # module table + each module's cs:sysdat / cs:supervisor
        for m in self.mods.values():
            slot = sd(s.v('module_table') + (MOD_NUM[m.name] - 1) * 8)
            w16(mem, slot, m.v('entry'))
            w16(mem, slot + 2, m.seg)
        # every module present, as in the real system; one that isn't
        # loaded here is a stub (see _module_call)
        mem[sd(s.v('module_map'))] = 0x1F
        return mem

    def _load_code(self, image):
        for m in self.mods.values():
            image[m.base:m.base + len(m.code)] = m.code
            w16(image, m.base + m.v('sysdat'), SYSDAT)
            if m.name != 'SUP' and 'supervisor' in m.sym:
                w16(image, m.base + m.v('supervisor'), self.sup.v('entry'))
                w16(image, m.base + m.v('supervisor') + 2, self.sup.seg)

    def _nested(self, sim, mod, ip):
        regs = dict(sim.regs)
        regs.update(cs=mod.seg, sp=(sim.regs['sp'] - 4) & 0xFFFF)   # the far return address
        inner = I86Sim(sim.mem, regs, mod.base + ip, segmented=True,
                       stop_addrs=self.entry_retfs[mod.name], far_call_hooks=self.hooks,
                       int_stub_nums={224})
        inner.run(max_steps=1000000)
        for r in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'ds', 'es'):
            sim.regs[r] = inner.regs[r]

    def _module_call(self, sim):
        s = self.sup
        slot = sd(s.v('module_table') + sim.regs['si'])
        off, seg = r16(sim.mem, slot), r16(sim.mem, slot + 2)
        mod = next((m for m in self.mods.values() if m.seg == seg and m.name != 'SUP'), None)
        num = sim.regs['si'] // 8 + 1
        fn = sim.regs['cx'] & 0xFF
        self.calls.append((num, fn))
        if (num, fn) in self.handlers:
            self.handlers[(num, fn)](sim)
        elif mod is None or (num, fn) in self.boundary:
            return
        else:
            self._nested(sim, mod, off)

    def _sup_call(self, sim):
        self._nested(sim, self.sup, self.sup.v('entry'))

    def user_call(self, sim):
        """an `int mpmint` from a process (an RSP): SUPIF's user_entry
        as user_enter (defs.h) models it -- u_wrkseg = the caller's DS --
        then the real SUP `func1` (CH = 0) with DS = SYSDAT, ES = SS =
        the running UDA. Back in the caller: AX = BX, the function's
        registers, DS = u_wrkseg, ES unchanged."""
        uda = r16(sim.mem, sd(r16(sim.mem, sd(self.v('rlr'))) + self.v('p_uda')))
        wrk = phys(uda, self.v('u_wrkseg'))
        w16(sim.mem, wrk, sim.regs['ds'])
        w16(sim.mem, phys(uda, ULEN - 2), USER_RET)
        regs = {**sim.regs, 'cx': sim.regs['cx'] & 0xFF, 'bx': r16(sim.mem, sd(self.v('rlr'))), 'ds': SYSDAT, 'es': uda, 'ss': uda,
                'sp': ULEN - 2, 'cs': self.sup.seg}
        self.calls.append(('user', sim.regs['cx'] & 0xFF))
        inner = I86Sim(sim.mem, regs, self.sup.base + self.v('func1'), segmented=True,
                       stop_addrs={self.sup.base + USER_RET}, far_call_hooks=self.hooks, int_stub_nums={224})
        inner.run(max_steps=1000000)
        for r in ('bx', 'cx', 'dx', 'si', 'di', 'bp'):
            sim.regs[r] = inner.regs[r]
        sim.regs['ax'] = inner.regs['bx']
        sim.regs['ds'] = r16(sim.mem, wrk)

    def _xios_call(self, sim):
        self.xios(sim)

    def run(self, entry, mem, regs, stop_addrs=(), es=None, ss=None, ds=None, sp=ULEN, cs=0, int_hook=None):
        image = bytearray(mem)
        self._load_code(image)
        self.calls = []
        start = phys(cs, entry if isinstance(entry, int) else self.v(entry))
        sim = I86Sim(image, {**regs, 'ds': SYSDAT if ds is None else ds, 'es': UDA if es is None else es,
                             'ss': UDA if ss is None else ss, 'sp': sp, 'cs': cs},
                     start, segmented=True, stop_addrs=set(stop_addrs), far_call_hooks=self.hooks,
                     int_stub_nums=set() if int_hook else {224}, int_hooks={224: int_hook} if int_hook else None)
        sim.run(max_steps=2000000)
        return {**sim.regs, 'ip': sim.ip}, image     # ip: where it stopped (a physical address)
