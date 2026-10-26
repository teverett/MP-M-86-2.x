#!/usr/bin/env python3
"""Tests for tools/plm (the PL/M-86 compiler): small programs compiled,
assembled with tools/asm86 and run in tools/i86sim, checking PL/M
semantics the utilities depend on -- BYTE/ADDRESS typing (8-bit
arithmetic, a constant below 256 is a BYTE), shifts/rotates, IF testing
the low bit, DO loops, the left-to-right argument order and parameter
layout (DRI's MCD.A86 mon stub expects func at [bp+6]), BYTE parameters,
nested-procedure closures, COMPACT-model POINTERs (4 bytes, `@x`, data
BASED on a POINTER reached through its segment, MOVB between segments)
-- and the real src/utils/DIR.PLM and TOD.PLM, run against a scripted
BDOS behind INT 224 (the base page set up as the loader does). See
docs/bugs.md #115 and #116.
"""
import os
import struct
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402
from plm.__main__ import main as plmc_main  # noqa: E402

STUB = os.path.join(REPO, 'tools', 'plm', 'runtime_stub.A86')
CS, DS = 0x0100, 0x0600

EXPR_PLM = r'''t: do;
declare (b, c) byte, (w, v) address;
declare out(40) address;
declare k byte initial (0);
put: procedure (x);
  declare x address;
  out(k) = x; k = k + 1;
  end put;
plmstart: procedure public;
  b = 1; call put(ror(b,3));          /* 20h */
  b = 81h; call put(rol(b,1));        /* 03h */
  b = 0c0h; call put(shr(b,4));       /* 0ch */
  b = 0c0h; call put(shl(b,1));       /* 80h (byte) */
  w = 1234h; call put(shl(w,4));      /* 2340h */
  w = 1234h; call put(shr(w,4));      /* 0123h */
  w = 8001h; call put(ror(w,1));      /* 0c000h */
  b = 0ffh; c = b + 1; call put(c);   /* 0 */
  b = 0ffh; call put(b + 1);          /* 0 (byte + byte) */
  w = 0ffh; call put(w + 1);          /* 100h */
  b = 5; call put(b - 10);            /* 0fbh */
  call put(10 - 3);                   /* 7 */
  call put(20 / 6);                   /* 3 */
  call put(20 mod 6);                 /* 2 */
  b = 0ffh; call put(b = -1);         /* 0ffffh: true */
  w = 0ffffh; call put(w = -1);       /* 0: 0ffffh vs 0ffh */
  b = 3; call put(b < 4);             /* true */
  b = 3; call put(b > 4);             /* false */
  call put(not 1);                    /* 0feh */
  if not 1 then call put(1111h); else call put(2222h);   /* 2222h */
  if 3 then call put(3333h);          /* 3333h */
  do b = 1 to 3; call put(b); end;    /* 1 2 3 */
  call put(high(1234h)); call put(low(1234h));   /* 12h 34h */
  w = 300; call put(w > 255);         /* true */
  end plmstart;
end t;
'''

PROC_PLM = r'''t: do;
declare out(20) address;
declare k byte initial (0);
declare bvp address, bv based bvp (2) byte;   /* SET's fcbv */
declare bsrc (2) byte initial (80h, 0ffh);
put: procedure (x);
  declare x address;
  out(k) = x; k = k + 1;
  end put;
three: procedure (a, b, c) address;
  declare (a, c) address, b byte;
  return a * 100 + b * 10 + c;
  end three;
bytep: procedure (b) byte;
  declare b byte;
  return b + 1;
  end bytep;
outer: procedure (p, q);
  declare (p, q) address, loc address;
  inner: procedure (r);
    declare r address;
    loc = p + q + r;
    end inner;
  loc = 0;
  call inner(5);
  call put(loc);
  end outer;
looper: procedure;                 /* SHOW's printbcd: locals, and a */
  declare (i, n) byte;             /* nested procedure without any   */
  bump: procedure;
    n = n + 1;
    end bump;
  n = 0; i = 4;
  do while (i := i - 1) <> -1;
    call bump;
    end;
  call put(n);
  end looper;
plmstart: procedure public;
  call put(three(1, 2, 3));        /* 123 */
  call put(three(4, 5, 6));        /* 456 */
  call put(bytep(0ffh));           /* 0 */
  call put(bytep(1234h));          /* 35h: arg truncated to a byte */
  call outer(10, 20);              /* 35 */
  call looper;                     /* 4 */
  bvp = .bsrc;
  call put(rol(bv(0), 1));         /* 1: an 8-bit rotate */
  call put(bv(1) + 1);             /* 0: byte arithmetic */
  end plmstart;
end t;
'''


PTR_PLM = r'''t: do;
declare out(10) address;
declare k byte initial (0);
declare p pointer;
declare pp structure (off word, seg word) at (@p);
declare far based p structure (a address, b byte, c byte);
declare local structure (a address, b byte, c byte);
put: procedure (x);
  declare x address;
  out(k) = x; k = k + 1;
  end put;
plmstart: procedure public;
  pp.off = 10h; pp.seg = 0e00h;          /* 0E00:0010 */
  call put(far.a); call put(far.b);      /* the far data: 1234h, 56h */
  far.c = 77h;                           /* far store */
  call movb(@far.a, @local.a, 4);        /* far -> near copy */
  call put(local.a); call put(local.c);  /* 1234h, 77h */
  local.b = 99h;
  call movb(@local.b, @far.b, 1);        /* near -> far */
  p = @local;                            /* @x: DS:x */
  call put(pp.seg = 0600h);              /* true: DS */
  end plmstart;
end t;
'''


def build(plm_path, workdir):
    gen = os.path.join(workdir, 'out.gen.A86')
    old = sys.argv
    sys.argv = ['plmc', plm_path, '-o', gen]
    try:
        plmc_main()
    finally:
        sys.argv = old
    src = os.path.join(workdir, 'out.A86')
    with open(gen) as f, open(src, 'w') as o:
        o.writelines(l for l in f if l.strip().lower() != 'end')
        o.write(open(STUB).read())
    a = Assembler()
    a.assemble(src, [os.path.dirname(plm_path)])
    return a


def load(a):
    mem = bytearray(0x10000)
    code, data = a.segment_bytes('cseg'), a.segment_bytes('dseg')
    mem[CS * 16:CS * 16 + len(code)] = code
    mem[DS * 16:DS * 16 + len(data)] = data
    return mem


def run_program(text, n, setup=None, check=None):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 'T.PLM')
        open(p, 'w').write(text)
        a = build(p, d)
    mem = load(a)
    if setup:
        setup(mem)
    sim = I86Sim(mem, {'cs': CS, 'ds': DS, 'es': DS, 'ss': 0x0F00, 'sp': 0x100}, CS * 16, segmented=True,
                 stop_addrs={CS * 16 + a.symtab['pl_plmstart_exit'].value})
    sim.run(max_steps=200000)
    out = DS * 16 + a.symtab['pl_out'].value
    vals = [mem[out + 2 * i] | mem[out + 2 * i + 1] << 8 for i in range(n)]
    return (vals, check(mem)) if check else vals


DIR_FILES = [(0, 'DIR     CMD', 0), (0, 'HELLO   TXT', 0), (0, 'MPM     SYS', 0x80), (1, 'OTHER   TXT', 0),
             (0, 'A       TXT', 0), (0, 'B       TXT', 0)]


SYSDAT_SEG = 0x0900


def run_tod(tail, tod):
    """TOD.PLM: version (12) is MP/M-86 2.0, get sysdat (154) returns
    SYSDAT_SEG in ES, the TOD structure is at SYSDAT:7Eh; conin (1)
    answers "Strike key". Returns the console output and SYSDAT's TOD."""
    with tempfile.TemporaryDirectory() as d:
        a = build(os.path.join(REPO, 'src', 'utils', 'TOD.PLM'), d)
    mem = load(a)
    bp = DS * 16
    t = tail.encode()
    first = (t.split() or [b''])[0][:8]
    mem[bp + 0x5C:bp + 0x68] = bytes([0]) + first.ljust(11)
    mem[bp + 0x6C:bp + 0x78] = bytes([0]) + b' ' * 11
    mem[bp + 0x80] = len(t)
    mem[bp + 0x81:bp + 0x82 + len(t)] = t + b'\0'
    tod_at = SYSDAT_SEG * 16 + 0x7E
    mem[tod_at:tod_at + 5] = struct.pack('<HBBB', *tod)
    out = bytearray()

    def bdos(sim):
        r = sim.regs
        fn, dx, ret = r['cx'] & 0xFF, r['dx'], 0
        if fn in (0, 143):
            sim.stop_addrs.add(sim.ip)
        elif fn == 2:
            out.append(dx & 0xFF)
        elif fn == 9:
            p = bp + dx
            while mem[p] != ord('$'):
                out.append(mem[p]); p += 1
        elif fn == 1:
            ret = ord('x')
        elif fn == 12:
            ret = 0x1130
        elif fn == 154:
            r['es'] = SYSDAT_SEG
        r['ax'] = r['bx'] = ret
    sim = I86Sim(mem, {'cs': CS, 'ds': DS, 'es': DS, 'ss': 0x0F00, 'sp': 0x100}, CS * 16, segmented=True,
                 int_hooks={224: bdos})
    sim.run(max_steps=3000000)
    return out.decode('latin-1').replace('\r', ''), struct.unpack_from('<HBBB', mem, tod_at)


def run_dir(tail):
    """DIR.PLM against a BDOS answering what it calls: console output,
    print string, console status, current disk, set DMA, user code,
    search first/next (4 entries per 128-byte record, the index in AL),
    parse filename (for an empty tail: nothing to parse), terminate."""
    with tempfile.TemporaryDirectory() as d:
        a = build(os.path.join(REPO, 'src', 'utils', 'DIR.PLM'), d)
    mem = load(a)
    bp = DS * 16
    mem[bp + 0x5C:bp + 0x68] = bytes([0]) + tail[0]
    mem[bp + 0x6C:bp + 0x78] = bytes([0]) + b' ' * 11
    mem[bp + 0x80] = 0
    out = bytearray()
    state = {'dma': 0x80, 'pos': 0, 'match': []}

    def bdos(sim):
        r = sim.regs
        fn, dx, ret = r['cx'] & 0xFF, r['dx'], 0
        if fn in (0, 143):
            sim.stop_addrs.add(sim.ip)
        elif fn == 2:
            out.append(dx & 0xFF)
        elif fn == 9:
            p = bp + dx
            while mem[p] != ord('$'):
                out.append(mem[p]); p += 1
        elif fn == 26:
            state['dma'] = dx
        elif fn in (17, 18):
            if fn == 17:
                pat = bytes(mem[bp + dx + 1:bp + dx + 12]).decode()
                state['match'] = [i for i, f in enumerate(DIR_FILES)
                                  if all(c == '?' or c == e for c, e in zip(pat, f[1]))]
            if not state['match']:
                ret = 0xFF
            else:
                i = state['match'].pop(0)
                u, name, attr = DIR_FILES[i]
                rec = bp + state['dma']
                ent = bytearray([u]) + name.encode() + bytes(20)
                ent[10] |= attr
                mem[rec + 32 * (i % 4):rec + 32 * (i % 4) + 32] = ent
                ret = i % 4
        elif fn == 152:
            ret = 0          # empty tail
        r['ax'] = r['bx'] = ret      # 11 (status), 25 (disk), 32 (user) -> 0
    sim = I86Sim(mem, {'cs': CS, 'ds': DS, 'es': DS, 'ss': 0x0F00, 'sp': 0x100}, CS * 16, segmented=True,
                 int_hooks={224: bdos})
    sim.run(max_steps=3000000)
    return out.decode('latin-1')


def main():
    failures = []

    def check(label, want, got):
        ok = want == got
        print(f"[{'ok' if ok else 'FAIL'}] {label}: expected {want!r} | got {got!r}")
        if not ok:
            failures.append(label)

    want = [0x20, 0x03, 0x0c, 0x80, 0x2340, 0x0123, 0xc000, 0, 0, 0x100, 0xfb, 7, 3, 2, 0xffff, 0, 0xffff, 0,
            0xfe, 0x2222, 0x3333, 1, 2, 3, 0x12, 0x34, 0xffff]
    got = run_program(EXPR_PLM, len(want))
    for i, (w, g) in enumerate(zip(want, got)):
        check(f"expression {i}", hex(w), hex(g))
    check("procedures: argument order, BYTE parameters, nested closures, frames around them, BASED bytes",
          [123, 456, 0, 0x35, 35, 4, 1, 0], run_program(PROC_PLM, 8))
    check("DIR: lists user 0's non-system files, 4 to a line",
          "Directory for User  0:\r\nA: DIR      CMD : HELLO    TXT : A        TXT : B        TXT"
          "\r\nSystem Files Exist", run_dir([b' ' * 11]))
    check("DIR: no match", "Directory for User  0:\r\nFile not found.", run_dir([b'NONE    X  ']))
    far = 0x0E00 * 16 + 0x10

    def setup(mem):
        mem[far:far + 4] = bytes([0x34, 0x12, 0x56, 0x00])
    vals, farbytes = run_program(PTR_PLM, 5, setup, lambda m: bytes(m[far:far + 4]))
    check("POINTER: far loads, a far store, MOVB both ways, @x is DS:x", [0x1234, 0x56, 0x1234, 0x77, 0xFFFF],
          vals)
    check("POINTER: the far data afterwards", bytes([0x34, 0x12, 0x99, 0x77]), farbytes)
    for day, text in ((1, 'Sun 01/01/78'), (60, 'Wed 03/01/78'), (790, 'Fri 02/29/80'), (3000, 'Wed 03/19/86')):
        check(f"TOD: day {day}", (f"{text} 23:59:09", (day, 0x23, 0x59, 0x09)),
              run_tod('', (day, 0x23, 0x59, 0x09)))
    check("TOD: set", ("Strike key to set time\nWed 03/19/86 12:05:30", (3000, 0x12, 0x05, 0x30)),
          run_tod('03/19/86 12:05:30', (1, 0, 0, 0)))
    check("TOD: Feb 29 of a non-leap year", ("Illegal time/date specification.", (1, 0, 0, 0)),
          run_tod('02/29/81 01:00:00', (1, 0, 0, 0)))
    if failures:
        print(f"\n{len(failures)} case(s) failed")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
