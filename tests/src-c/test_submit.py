#!/usr/bin/env python3
"""Differential test for src-c/kernel/submit.c (src/rsp/SUBMIT.A86: the
command-tail deblank/argument table, `$n` expansion, readbuffer/readchar
and one pass of the command loop) against the real SUBMIT running in
the real system, on real memory layout -- see tests/src-c/supref.py.

SUBMIT's code is at SUB_CS and its data segment (the real DSEG image) at
SUB_DS; its PD is supref's running PD (pdadr), its sysdatseg SYSDAT. Its
`int mpmint` goes to SysRef.user_call for f_parsefilename and the CLI
call (the real SUP -- and COMMAND.SUP over the real RTM's queues -- with
u_wrkseg = SUBMIT's DS). The calls whose results SUBMIT uses are
boundaries answered from the case's values on both sides: conin (the
ctl-C question), getdefdisk, get user code, and the SUB file's open and
read. The rest (console output, set user/disk/list device, attach) are
no-ops. A routine SUBMIT calls is run to a sentinel return address on
its own stack. Every case diffs the whole image.
"""
import ctypes
import os
import sys

from supref import SysRef, Checker, PD_RUN, SYSDAT, build_lib, c_buffer, phys, sd, w16, r16, KERNEL_C, REPO_ROOT
from asm86 import Assembler  # noqa: E402

SUBMIT_A86 = os.path.join(REPO_ROOT, 'src', 'rsp', 'SUBMIT.A86')
SUBMIT_C = [os.path.join(KERNEL_C, f) for f in
            ('submit.c', 'command.c', 'que1.c', 'que2.c', 'console.c', 'chario.c', 'findpd.c', 'parse.c',
             'sysent.c', 'proc.c', 'dsptch.c', 'load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c',
             'file1.c', 'file2.c', 'file3.c', 'file4.c')]
SUB_DS = 0x0B00
SUB_CS = 0x0C00
RET_IP = 0xFFF0
u16, u8 = ctypes.c_uint16, ctypes.c_uint8
c_u8p = ctypes.POINTER(ctypes.c_uint8)
PF_CHILDABORT, PF_CTLC = 0x0800, 0x0080


def main():
    ref = SysRef(('SUP', 'RTM'))
    v = ref.v
    asm = Assembler()
    asm.assemble(SUBMIT_A86, [])
    scode, sdata, ssym = asm.segment_bytes('cseg'), asm.segment_bytes('dseg'), asm.symtab
    s = lambda name: ssym[name].value  # noqa: E731
    ds_ = lambda name, off=0: phys(SUB_DS, s(name) + off)  # noqa: E731
    ref.skip |= set(range(phys(SUB_CS), phys(SUB_CS, len(scode))))
    ref.skip |= set(range(ds_('stack'), ds_('stacktop')))    # SUBMIT's own stack
    lib_path = build_lib(SUBMIT_C)
    try:
        lib = ctypes.CDLL(lib_path)
        lib.deblank.argtypes = lib.fill_argtable.argtypes = lib.expand_command.argtypes = [c_u8p, u16]
        lib.readchar.restype = lib.readbuffer.restype = u16
        lib.readchar.argtypes = lib.readbuffer.argtypes = [c_u8p, u16, u16, u8, u8]
        lib.topofcmdloop_once.argtypes = [c_u8p, u16, u16, u8, u8, u8, u16, u16]
        ck = Checker(ref)

        def base_memory():
            mem = ref.base_memory()
            mem[phys(SUB_DS):phys(SUB_DS, len(sdata))] = sdata
            mem[phys(SUB_CS):phys(SUB_CS, len(scode))] = scode
            w16(mem, ds_('pdadr'), PD_RUN)
            w16(mem, ds_('sysdatseg'), SYSDAT)
            mem[sd(PD_RUN + v('p_name')):sd(PD_RUN + v('p_name') + 8)] = b'SUBMIT  '
            mem[ds_('command_disk')] = 3
            mem[ds_('command_user')] = 2
            mem[ds_('fcb', 1):ds_('fcb', 12)] = b'BUILD   SUB'
            mem[sd(v('dayfile'))] = 0
            w16(mem, sd(v('pul')), 0)
            qd = v('mxcliqd')
            w16(mem, sd(v('qlr')), qd)
            w16(mem, sd(qd + v('q_link')), 0)
            w16(mem, sd(qd + v('q_msgcnt')), 1)
            return mem

        def run(label, entry, mem, regs, call, result=lambda r: (), port=lambda r: (), stops=None,
                answers=(0, 0, 0, 0, 0)):
            charin, defdisk, defuser, openf, readf = answers

            def int_hook(sim):
                cl = sim.regs['cx'] & 0xFF
                al = {1: charin, 25: defdisk, 15: openf, 20: readf}.get(cl)
                if cl == 32 and sim.regs['dx'] & 0xFF == 0xFF:
                    al = defuser
                if cl in (150, 152):
                    ref.user_call(sim)
                elif al is not None:
                    sim.regs['ax'] = (sim.regs['ax'] & 0xFF00) | al
            if stops is None:      # a called routine: return to RET_IP
                w16(mem, ds_('stacktop', -2), RET_IP)
                stops, sp = {phys(SUB_CS, RET_IP)}, s('stacktop') - 2
            else:
                sp = s('stacktop')
            ref_regs, img = ref.run(s(entry), mem, regs, cs=SUB_CS, ds=SUB_DS, es=SUB_DS, ss=SUB_DS, sp=sp,
                                    int_hook=int_hook, stop_addrs=stops)
            buf = c_buffer(mem)
            got = call(buf)
            ck.compare(label, img, buf, result(ref_regs), port(got))
            return ref_regs

        # -- deblank + fill_argtable (to filldone's `call crlf`) --
        for label, tail in (("simple args", b' foo  bar baz'), ("leading tabs/spaces, embedded tab", b'\t  one\ttwo'),
                            ("single argument", b' onlyone')):
            mem = base_memory()
            mem[ds_('argtable'):ds_('argtable') + 64] = b'\xee' * 64
            mem[ds_('cmdtail')] = len(tail)
            mem[ds_('cmdtail', 1):ds_('cmdtail', 2 + len(tail))] = tail + b'\0'
            run(f"deblank+fill_argtable: {label}", 'firstwhites', mem,
                {'si': s('cmdtail') + 1, 'di': s('deblankbuf')},
                lambda b: (lib.deblank(b, SUB_DS), lib.fill_argtable(b, SUB_DS)), stops={phys(SUB_CS, s('crlf'))})

        # -- expand_command (to echocommand) --
        def expand_memory(cmd, args):
            mem = base_memory()
            mem[ds_('cmd'):ds_('cmd') + len(cmd) + 1] = cmd + b'\0'
            text = b' ' + b' '.join(args) + b' ' if args else b' '
            mem[ds_('deblankbuf'):ds_('deblankbuf') + len(text) + 1] = text + b'\0'
            w16(mem, ds_('numargs'), len(args))
            pos = 0
            for i, a in enumerate(args):
                mem[ds_('argtable', i)] = pos + 1
                pos += 1 + len(a)
            return mem
        for label, cmd, args in (("no substitution", b'DIR A:', []), ("single-digit arg", b'TYPE $1.TXT', [b'FOO']),
                                 ("two-digit arg", b'ECHO $10', [str(i).encode() for i in range(11)]),
                                 ("arg out of range", b'TYPE $5.TXT', [b'FOO']),
                                 ("literal $ not followed by a digit", b'PIP X=$Y', []),
                                 ("fills the buffer", b'X' * 140, [])):
            run(f"expand_command: {label}", 'nextchar', expand_memory(cmd, args),
                {'si': s('cmd'), 'bp': s('clicmd'), 'di': 0}, lambda b: lib.expand_command(b, SUB_DS),
                stops={phys(SUB_CS, s('echocommand'))})

        # -- readchar / readbuffer --
        SI = 0x0480
        rc = dict(result=lambda r: (r['bx'],), port=lambda r: (r,))
        for label, ptr, dma, answers in (("no refill", 5, b'\0' * 5 + b'Q', (0, 0, 0, 0, 0)),
                                         ("EOF marker", 10, b'\0' * 10 + b'\x1a', (0, 0, 0, 0, 0)),
                                         ("refill, open fails", 128, b'', (0, 0, 0, 0xFF, 0)),
                                         ("refill, read fails", 128, b'', (0, 0, 0, 0, 1)),
                                         ("refill", 128, b'Z', (0, 0, 0, 0, 0))):
            mem = base_memory()
            mem[ds_('buf_ptr')] = ptr
            mem[ds_('dma'):ds_('dma') + len(dma)] = dma
            run(f"readchar: {label}", 'readchar', mem, {'si': SI},
                lambda b, a=answers: lib.readchar(b, SUB_DS, SI, a[3], a[4]), answers=answers, **rc)
        for label, text, maxlen in (("CRLF-terminated line", b'DIR A:\r\n', 32), ("LF-only line", b'ECHO HI\n', 32),
                                    ("skips a leading blank line", b'\r\nDIR B:\r\n', 32),
                                    ("buffer fills first", b'X' * 40 + b'\r\n', 10), ("end of file", b'\x1a', 32),
                                    ("zero-length buffer", b'A\r\n', 0)):
            mem = base_memory()
            mem[ds_('buf_ptr')] = 0
            mem[ds_('cmdbuf')] = maxlen
            mem[ds_('dma'):ds_('dma') + len(text)] = text
            run(f"readbuffer: {label}", 'readbuffer', mem, {'dx': s('cmdbuf')},
                lambda b: lib.readbuffer(b, SUB_DS, s('cmdbuf'), 0, 0), **rc)

        # -- topofcmdloop_once (to getnxtcmd or submitexit) --
        exits = {phys(SUB_CS, s('getnxtcmd')): 0, phys(SUB_CS, s('submitexit')): 1}

        def loop(label, text, flags=0, charin=0, defdisk=2, defuser=5, cmdsent=0, args=(), stale=None):
            mem = expand_memory(text, list(args))
            mem[ds_('blen')] = len(text)
            mem[ds_('cmdsent')] = cmdsent
            w16(mem, sd(PD_RUN + v('p_flag')), flags)
            if stale:
                mem[ds_('parseresult'):ds_('parseresult') + 12] = stale
            answers = (charin, defdisk, defuser, 0, 0)
            ip = {}

            def where(r):
                return ()
            r = run(f"topofcmdloop_once: {label}", 'topofcmdloop', mem, {},
                    lambda b: ip.setdefault('port', lib.topofcmdloop_once(b, SYSDAT, SUB_DS, charin, defdisk, defuser,
                                                                          v('user_retf'), 0)),
                    stops=set(exits), answers=answers, result=where)
            ck.check(f"topofcmdloop_once: {label}: exit", exits.get(r['ip']), ip['port'])

        loop("ordinary command -> CLI (no free PD) -> submitexit", b'DIR A:')
        loop("empty -> getnxtcmd", b'')
        loop("';' comment -> getnxtcmd", b';ignored')
        loop("'C:' -> getnxtcmd", b'C:')
        loop("'~:' out of range -> submitexit", b'~:')
        loop("2 chars, no ':' -> builtin check, CLI", b'ls', stale=b'\0LS         ')
        loop("ctl-C pending, answer 'n' -> continues", b'', flags=PF_CHILDABORT | 1, charin=ord('n'))
        loop("ctl-C pending, answer 'Y' -> pf_ctlc, submitexit", b'DIR A:', flags=PF_CHILDABORT, charin=ord('Y'))
        loop("cmdsent cleared, user 12 prompt", b'', cmdsent=0xFF, defuser=12, defdisk=1)
        loop("USER, no argument", b'USER')
        loop("USER 5", b'USER 5')
        loop("PRINTER 12", b'PRINTER 12')
        loop("$INCLUDE file -> nested submit through the CLI", b'$INCLUDE NEXT')
        loop("$INCLUDE, no file -> CLI as is", b'$INCLUDE')
        loop("$INCLUDE, bad file -> submitexit", b'$INCLUDE A:B:C')
        loop("$1 expanded, then the CLI", b'TYPE $1', args=(b'BUILD.SUB', b'X.TXT'))
        loop("bad parse -> '$' after the first word, submitexit", b'TOOLONGNAME.BAR X')
        return ck.finish()
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
