#!/usr/bin/env python3
"""Differential test for src-c/kernel/command.c (COMMAND.SUP: clicmd_entry,
chain_entry and what they call) against the real system, on real memory
layout -- see tests/src-c/supref.py.

The reference is the real SUP with the real RTM, MEM, CIO and BDOS loaded
beside it, so the CLI's own `mpmif` calls run the real code: the MXCli/
MXLoad/RSP queues (RTM), the console (CIO, whose XIOS calls go to the
same scripted Xios as chario.c's hook), f_parsefilename and f_load/
f_cload (SUP, and through load the MEM and BDOS calls test_load.py
covers). f_createproc and f_terminate are dispatcher boundaries, as in
the C; f_setprior's handler does what sysent.c's set_prior_entry does
(the real one ends in `jmp pdisp`).

f_fopen/f_fclose and load's f_freadseq go through the BDOS's unported
getdiskmx path: a scripted disk (`Disk`) answers them on both sides --
the reference's handlers, and command.c's `cli_set_bdos` / load.c's
`load_set_readseq` hooks. Every case compares bx/cx and diffs the whole
image (code and the UDA's stack area aside; after a chain's cli_ld1 the
caller runs on SYSDAT's inituda block, whose stack area is skipped too).
"""
import ctypes
import os
import sys

from supref import (SysRef, Checker, MOD_NUM, PD_RUN, SYSDAT, UDA, WRKSEG, UDA_STACK, BxCx, build_lib, c_buffer,
                    phys, sd, wk, w16, r16, KERNEL_C)
from cioref import Xios, XIOS_FN

COMMAND_C = [os.path.join(KERNEL_C, f) for f in
             ('command.c', 'que1.c', 'que2.c', 'console.c', 'chario.c', 'findpd.c', 'parse.c', 'sysent.c', 'proc.c', 'dsptch.c',
              'load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c',
              'file1.c', 'file2.c', 'file3.c', 'file4.c')]
u16 = ctypes.c_uint16
c_u8p = ctypes.POINTER(ctypes.c_uint8)
BDOS_FN = ctypes.CFUNCTYPE(u16, c_u8p, ctypes.c_uint8, u16, u16)
READSEQ_FN = ctypes.CFUNCTYPE(ctypes.c_uint8, c_u8p, u16, u16, u16, u16)

CMDBUF = 0x0200              # the caller's CLICB, in u_wrkseg
DMABUF = 0x0300              # the caller's DMA buffer (chain), in u_wrkseg
CCB_TABLE = 0x1300           # SYSDAT offsets
FREE_PD = 0x1400
RSPQD = 0x1500
RSP_PD = 0x1540
MD_FREE = (0x1600, 0x1610, 0x1620, 0x1630, 0x1640, 0x1650)
MFL_MD = 0x1700
MFL_SEG = 0x0C00


def cmd_header(entries):
    rec = bytearray(128)
    for i, (form, length, base, mn, mx) in enumerate(entries):
        rec[i * 9] = form
        for j, val in enumerate((length, base, mn, mx)):
            w16(rec, i * 9 + 1 + 2 * j, val)
    return bytes(rec)


PROG = [cmd_header([(1, 8, 0, 8, 8)]), bytes([0xCC] * 128)]


class Disk:
    """files: {(disk, user, 'NAME    TYP'): (sys, records)}. Open finds
    the FCB's name on its disk (fcb_dr, else the running PD's p_dsk) and
    user (p_user); reads go through the FCB's nxtrec. The same model on
    both sides (a fresh one per side)."""
    def __init__(self, files, fail_read=False):
        self.files, self.fail_read = files, fail_read
        self.rlr_field = self.uda_state = None   # set per call: the running PD's fields, and
        # (u_error_mode, u_dma_ofst, u_dma_seg) of its UDA, which a real open uses
        self.open_file = None
        self.log = []

    def bdos(self, getb, setb, func, fcb):
        if func == 16:
            self.log.append(('close', self.uda_state()))
            return 0
        dr = getb(fcb)
        disk = dr - 1 if dr else getb(self.rlr_field('p_dsk'))
        user = getb(self.rlr_field('p_user'))
        name = bytes(getb(fcb + 1 + i) & 0x7F for i in range(11)).decode('ascii')
        self.log.append(('open', disk, user, name, self.uda_state()))
        f = self.files.get((disk, user, name))
        if f is None:
            return 0x00FF
        sys_, self.open_file = f
        if sys_:
            setb(fcb + 10, getb(fcb + 10) | 0x80)
        return 0

    def read(self, getb, setb, fcb, dma):
        if self.fail_read or self.open_file is None:
            return 0xFF
        nxt = getb(fcb + 32)
        if nxt >= len(self.open_file):
            return 1
        for i, b in enumerate(self.open_file[nxt]):
            setb(dma + i, b)
        setb(fcb + 32, nxt + 1)
        return 0


def main():
    ref = SysRef(('SUP', 'RTM', 'MEM', 'CIO', 'BDOS'))
    v = ref.v
    cio = ref.mods['CIO']
    user_retf = v('user_retf')
    base_skip = set(ref.skip)
    inituda_stack = set(range(sd(v('inituda') + UDA_STACK[0]), sd(v('inituda') + UDA_STACK[1])))
    lib_path = build_lib(COMMAND_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.cli_set_bdos.argtypes = [BDOS_FN]
        lib.load_set_readseq.argtypes = [READSEQ_FN]
        lib.cio_set_xios.argtypes = [XIOS_FN]
        lib.shifttail.argtypes = [c_u8p, u16, u16]
        lib.fileopen.restype = u16
        lib.fileopen.argtypes = [c_u8p, u16]
        lib.user_retf.argtypes = [c_u8p, u16]
        lib.clicmd_entry.restype = BxCx
        lib.clicmd_entry.argtypes = [c_u8p, u16, u16, u16, u16]
        lib.chain_entry.restype = BxCx
        lib.chain_entry.argtypes = [c_u8p, u16, u16, u16]
        ck = Checker(ref)
        f = lambda addr, name: sd(addr + v(name))  # noqa: E731
        cli = lambda name: sd(v(name))  # noqa: E731

        def rlr_field(name):
            return lambda mem: sd(r16(mem, sd(v('rlr'))) + v(name))

        def uda_state(mem):
            uda = r16(mem, sd(r16(mem, sd(v('rlr'))) + v('p_uda')))
            return (mem[phys(uda, v('u_error_mode'))], r16(mem, phys(uda, v('u_dma_ofst'))),
                    r16(mem, phys(uda, v('u_dma_seg'))))

        def base_memory(dayfile=0):
            mem = ref.base_memory()
            mem[cli('dayfile')] = dayfile
            mem[cli('srchdisk')] = 0
            mem[cli('ncondev')] = 1
            mem[cli('nlstdev')] = 1
            mem[cli('ncns')] = 1
            w16(mem, cli('pul'), 0)
            mem[f(PD_RUN, 'p_name'):f(PD_RUN, 'p_name') + 8] = b'TMP0    '
            mem[f(PD_RUN, 'p_dsk')] = 2
            mem[f(PD_RUN, 'p_user')] = 3
            mem[f(PD_RUN, 'p_lst')] = 1
            w16(mem, phys(UDA, v('u_dma_ofst')), DMABUF)
            w16(mem, phys(UDA, v('u_dma_seg')), WRKSEG)
            mem[phys(UDA, v('u_error_mode'))] = 0x11
            # consoles: one CCB, unattached; one list device
            w16(mem, cli('ccb'), CCB_TABLE)
            for n in range(2):
                mem[sd(CCB_TABLE + n * cio.v('ccblen') + cio.v('c_mimic'))] = 0xFF
                mem[sd(CCB_TABLE + n * cio.v('ccblen') + cio.v('c_msource'))] = 0xFF
            mem[cli('tod') + 2:cli('tod') + 5] = b'\x12\x34\x56'
            # MXCli and MXLoad: one token each, on qlr
            qcli, qload = v('mxcliqd'), v('mxloadqd')
            w16(mem, cli('qlr'), qcli)
            w16(mem, sd(qcli + v('q_link')), qload)
            w16(mem, sd(qcli + v('q_msgcnt')), 1)
            w16(mem, sd(qload + v('q_link')), 0)
            w16(mem, sd(qload + v('q_msgcnt')), 1)
            # MEM: its mutex already ours; free MDs; one free region
            w16(mem, cli('mxmemowner'), PD_RUN)
            mem[cli('mxmemcount')] = 1
            w16(mem, cli('mdul'), MD_FREE[0])
            for i, md in enumerate(MD_FREE):
                w16(mem, f(md, 'm_link'), MD_FREE[i + 1] if i + 1 < len(MD_FREE) else 0)
            w16(mem, cli('mfl'), MFL_MD)
            w16(mem, f(MFL_MD, 'm_start'), MFL_SEG)
            w16(mem, f(MFL_MD, 'm_length'), 0x0300)
            w16(mem, cli('thrdrt'), PD_RUN)
            return mem

        def free_pd(mem):
            w16(mem, cli('pul'), FREE_PD)
            w16(mem, f(FREE_PD, 'p_link'), 0)

        def rsp(mem, name=b'FOOBAR  '):
            # an RSP queue on qlr, and its process on the thread list
            w16(mem, sd(v('mxloadqd') + v('q_link')), RSPQD)
            w16(mem, f(RSPQD, 'q_link'), 0)
            w16(mem, f(RSPQD, 'q_flags'), v('qf_rsp'))
            mem[f(RSPQD, 'q_name'):f(RSPQD, 'q_name') + 8] = name
            w16(mem, f(RSPQD, 'q_msglen'), 4)
            w16(mem, f(RSPQD, 'q_nmsgs'), 1)
            w16(mem, f(RSPQD, 'q_buf'), 0x1580)
            w16(mem, f(PD_RUN, 'p_thread'), RSP_PD)
            mem[f(RSP_PD, 'p_name'):f(RSP_PD, 'p_name') + 8] = name

        def command(mem, text):
            mem[wk(CMDBUF)] = 0
            mem[wk(CMDBUF + 1):wk(CMDBUF + 1) + len(text) + 1] = text + b'\0'

        def run(label, entry, mem, regs, call, files=None, result=lambda r: (r['bx'], r['cx']),
                port=lambda r: (r.bx, r.cx), snap=lambda i: (), stop=(), chain=False, fail_read=False):
            files = files or {}
            ref.skip = base_skip | (inituda_stack if chain else set())
            rdisk, rx = Disk(files, fail_read), Xios()

            def disk_handler(func):
                def h(sim):
                    rdisk.rlr_field = lambda name: rlr_field(name)(sim.mem)
                    rdisk.uda_state = lambda: uda_state(sim.mem)
                    uda = r16(sim.mem, sd(r16(sim.mem, sd(v('rlr'))) + v('p_uda')))
                    fcb = phys(r16(sim.mem, phys(uda, v('u_wrkseg'))), sim.regs['dx'])
                    getb, setb = (lambda a: sim.mem[a]), (lambda a, b: sim.mem.__setitem__(a, b))
                    if func == 7:
                        dma = phys(r16(sim.mem, phys(uda, v('u_dma_seg'))), r16(sim.mem, phys(uda, v('u_dma_ofst'))))
                        res = rdisk.read(getb, setb, fcb, dma)
                    else:
                        res = rdisk.bdos(getb, setb, 15 if func == 2 else 16, fcb)
                    sim.regs['ax'] = sim.regs['bx'] = res
                return h

            def setprior(sim):
                sim.mem[sd(r16(sim.mem, sd(v('rlr'))) + v('p_prior'))] = sim.regs['dx'] & 0xFF
            ref.handlers = {(MOD_NUM['BDOS'], 2): disk_handler(2), (MOD_NUM['BDOS'], 3): disk_handler(3),
                            (MOD_NUM['BDOS'], 7): disk_handler(7), (MOD_NUM['RTM'], 15): setprior}

            def ref_xios(sim):
                sim.regs['ax'] = sim.regs['bx'] = rx.call(sim.regs['ax'] & 0xFF, sim.regs['cx'] & 0xFF,
                                                          sim.regs['dx'] & 0xFF)
            ref.xios = ref_xios
            ref_regs, img = ref.run(entry, mem, regs, stop_addrs=stop)
            ref_calls = list(ref.calls)
            buf = c_buffer(mem)
            cdisk, cx = Disk(files, fail_read), Xios()

            def c_bdos(p, func, seg, off):
                cdisk.rlr_field = lambda name: rlr_field(name)(p)
                cdisk.uda_state = lambda: uda_state(p)
                return cdisk.bdos(lambda a: p[a], lambda a, b: p.__setitem__(a, b), func, phys(seg, off))

            def c_read(p, fseg, foff, dseg, doff):
                return cdisk.read(lambda a: p[a], lambda a, b: p.__setitem__(a, b), phys(fseg, foff), phys(dseg, doff))
            hooks = (BDOS_FN(c_bdos), READSEQ_FN(c_read), XIOS_FN(lambda fn, cl, dl: cx.call(fn, cl, dl)))
            lib.cli_set_bdos(hooks[0])
            lib.load_set_readseq(hooks[1])
            lib.cio_set_xios(hooks[2])
            got = call(buf)
            ck.compare(label, img, buf, result(ref_regs), port(got), snap)
            ck.check(f"{label}: same disk calls", rdisk.log, cdisk.log)
            ck.check(f"{label}: same console output", rx.log, cx.log)
            return ref_calls

        clicmd = lambda b: lib.clicmd_entry(b, SYSDAT, CMDBUF, user_retf, 0)  # noqa: E731
        chain = lambda b: lib.chain_entry(b, SYSDAT, user_retf, 0)  # noqa: E731
        nores = dict(result=lambda r: (), port=lambda r: ())
        tail = cli('cli_cmdtail')

        # -- shifttail --
        mem = base_memory()
        mem[tail] = 0xFF
        run("shifttail: no tail -> cli_cmdtail cleared", 'shifttail', mem, {'ax': 0},
            lambda b: lib.shifttail(b, SYSDAT, 0), **nores)
        mem = base_memory()
        mem[tail + 10:tail + 16] = b'HELLO\0'
        run("shifttail: tail shifted to the buffer start", 'shifttail', mem, {'ax': v('cli_cmdtail') + 10},
            lambda b: lib.shifttail(b, SYSDAT, v('cli_cmdtail') + 10), **nores)

        # -- fileopen (stops at its ret: the sim has no caller) --
        fo_ret = [i for i in range(v('fileopen'), v('fileopen') + 0x20) if ref.sup.code[i] == 0xC3][0]
        for label, files in (("fileopen: found, sets f6' (read-only)", {(2, 3, 'FOO     CMD'): (False, PROG)}),
                             ("fileopen: not found -> 00FFh", {})):
            mem = base_memory()
            mem[cli('cli_fcb'):cli('cli_fcb') + 12] = b'\0FOO     CMD'
            run(label, 'fileopen', mem, {}, lambda b: lib.fileopen(b, SYSDAT), files,
                result=lambda r: (r['bx'],), port=lambda r: (r,), stop={fo_ret})

        # -- user_retf: clears keep/tempkeep/ctlc/sys only --
        mem = base_memory()
        w16(mem, f(PD_RUN, 'p_flag'), 0x06C3 | v('pf_tempkeep'))
        ur_int = [i for i in range(user_retf, user_retf + 0x20) if ref.sup.code[i] == 0xCD][0]
        run("user_retf: clears keep/tempkeep/ctlc/sys", 'user_retf', mem, {},
            lambda b: lib.user_retf(b, SYSDAT), stop={ur_int + 2}, **nores)

        # -- clicmd_entry: an RSP --
        q = lambda i: (r16(i, sd(RSPQD + v('q_msgcnt'))), bytes(i[sd(0x1580):sd(0x1584)]))  # noqa: E731
        mem = base_memory()
        rsp(mem)
        command(mem, b'foobar some args')
        run("clicmd_entry: RSP -> command queued, console assigned", 'clicmd_entry', mem, {'dx': CMDBUF},
            clicmd, snap=q)
        mem = base_memory(dayfile=0xFF)
        rsp(mem)
        command(mem, b'FOOBAR')
        run("clicmd_entry: RSP, dayfile -> time and 'Msg Qued' printed", 'clicmd_entry', mem, {'dx': CMDBUF},
            clicmd, snap=q)
        mem = base_memory()
        rsp(mem)
        w16(mem, f(RSPQD, 'q_msgcnt'), 1)
        command(mem, b'FOOBAR')
        run("clicmd_entry: RSP queue full -> e_q_full", 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd, snap=q)

        # -- clicmd_entry: errors before a load --
        mem = base_memory()
        command(mem, b'A:B:C')
        run("clicmd_entry: bad command line -> parse error", 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd)
        mem = base_memory()
        command(mem, b'NOPE')
        run("clicmd_entry: no free PD -> e_no_pd", 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd)
        for label, files, srch in (
                ("clicmd_entry: not found anywhere -> e_bad_open, PD released", {}, 1),
                ("clicmd_entry: search disk is the default -> one open only", {}, 2),
                ("clicmd_entry: non-SYS file on the search disk, user 0 has none -> e_bad_open",
                 {(1, 3, 'NOPE    CMD'): (False, PROG)}, 1),
                ("clicmd_entry: non-SYS file on the search disk and user 0 -> e_bad_open",
                 {(1, 3, 'NOPE    CMD'): (False, PROG), (1, 0, 'NOPE    CMD'): (False, PROG)}, 1)):
            mem = base_memory()
            free_pd(mem)
            mem[cli('srchdisk')] = srch
            command(mem, b'NOPE')
            run(label, 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd, files)

        # -- clicmd_entry: a .CMD file, loaded --
        bp = lambda i: bytes(i[phys(MFL_SEG):phys(MFL_SEG, 0x100)])  # noqa: E731
        for label, text, files, dayfile, srch in (
                ("clicmd_entry: found on the default disk, loaded", b'prog', {(2, 3, 'PROG    CMD'): (False, PROG)},
                 0, 1),
                ("clicmd_entry: SYS file on the search disk, two arguments, dayfile",
                 b'prog a:one.txt;pw b:two', {(1, 3, 'PROG    CMD'): (True, PROG)}, 0xFF, 1),
                ("clicmd_entry: SYS file under user 0 on the search disk", b'prog x',
                 {(1, 0, 'PROG    CMD'): (True, PROG), (1, 3, 'PROG    CMD'): (False, PROG)}, 0, 1)):
            mem = base_memory(dayfile)
            free_pd(mem)
            mem[cli('srchdisk')] = srch
            command(mem, text)
            calls = run(label, 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd, files, snap=bp)
            ck.check(f"{label}: f_createproc reached", True, (MOD_NUM['RTM'], 14) in calls)
        mem = base_memory()
        free_pd(mem)
        command(mem, b'prog')
        run("clicmd_entry: load fails -> e_bad_read, PD released", 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd,
            {(2, 3, 'PROG    CMD'): (False, PROG)}, fail_read=True)
        mem = base_memory()
        free_pd(mem)
        w16(mem, f(PD_RUN, 'p_flag'), v('pf_ctlc'))
        command(mem, b'prog')
        run("clicmd_entry: ctl-C pending -> e_abort, nothing loaded", 'clicmd_entry', mem, {'dx': CMDBUF}, clicmd,
            {(2, 3, 'PROG    CMD'): (False, PROG)})

        # -- chain_entry: the command in the caller's DMA buffer; a chain
        # that reaches an RSP or loads terminates (bx/cx not compared) --
        def chain_memory(text):
            mem = base_memory()
            mem[wk(DMABUF):wk(DMABUF) + len(text) + 1] = text + b'\0'
            return mem
        mem = chain_memory(b'NOPE')
        free_pd(mem)
        run("chain_entry: not found -> e_bad_open", 'chain_entry', mem, {}, chain)
        mem = chain_memory(b'FOOBAR')
        rsp(mem)
        w16(mem, f(PD_RUN, 'p_flag'), 0x0283)
        run("chain_entry: RSP -> queued, terminates (MXCli kept)", 'chain_entry', mem, {}, chain, snap=q, **nores)
        mem = chain_memory(b'prog arg')
        free_pd(mem)
        run("chain_entry: loaded -> UDA handed to the new PD, terminates", 'chain_entry', mem, {}, chain,
            {(2, 3, 'PROG    CMD'): (False, PROG)}, chain=True, snap=bp, **nores)
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
