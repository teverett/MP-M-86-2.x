"""i86sim -- a minimal 8086 instruction interpreter.

This exists purely as the test oracle for /src-c's differential tests (see
docs/src-c.md): running tools/asm86.py's own assembled bytes for a routine
and comparing the result against the hand-written C port of that same
routine. It is NOT a general-purpose 8086 emulator.

Scope, deliberately narrow and grown lazily file-by-file:

- Only the ModRM forms tools/asm86/ actually emits are supported: mod=11
  (register direct) and mod=00/10 with a disp16 (tools/asm86's own README
  documents that it always uses disp16, never the disp8 ModRM form, so
  mod=01 is unreachable in practice but decoded anyway since it's free).
- Only one flat 64KB memory array is modeled. By default, segment
  override prefixes (26h/2Eh/36h/3Eh) are parsed (so instruction length
  stays correct) but have no effect on addressing. This is adequate for
  differentially testing a single routine in isolation, where the test
  harness places every operand the routine touches in that one array.
  Pass `segmented=True` to turn on real (opt-in) segment arithmetic
  instead, for a routine that dynamically switches DS/ES to a *different*
  segment and then accesses fixed offsets relative to it (e.g. PROC.RTM's
  own newly-created UDA, ML.MEM's own newly-allocated MAU) -- every
  effective address becomes `(segment_value * 16 + offset) & 0xFFFF`
  (the usual 8086 rule: SS for a BP-based EA, DS otherwise, both
  overridable by a prefix; ES for a string op's own DI, not overridable,
  same as real hardware), still wrapped into this same 64KB array rather
  than a real 20-bit/1MB address space -- the test just has to pick
  segment values that don't collide once wrapped, same spirit as
  `CSEG_BASE`-shifting a reference run elsewhere in this project. Kept
  off by default since at least one existing test (`src-c/kernel/
  echo.c`'s own `test_echo.py`) explicitly relies on a segment override
  being a no-op; turning this on unconditionally could silently change
  what an existing test actually exercises.
- Only the flags a decoded instruction actually needs are modeled (so far:
  ZF, CF, SF and OF -- enough for every unsigned and signed conditional
  jump). Add more as a future routine needs them.
- String instructions (MOVS/STOS/LODS/CMPS/SCAS, with or without a REP/
  REPE/REPNE prefix) increment SI/DI by default, decrement when the
  direction flag (DF, CLD/STD) is set -- tracked as `self.df`. Added for
  `src-c/kernel/mau.c`'s own `mau_insert`, the first routine seen doing a
  real backward shift (opening a gap in a SAT table); every earlier file
  only ever did a forward copy/scan (and explicitly CLDs first where it
  matters, e.g. SUPIF.SUP's `user_entry`).
- CALL/RET use a real stack-based return address, with a sentinel the
  harness pushes before the first instruction: a routine's own outermost
  RET pops it and halts, while an internal `call`/`ret` pair (e.g.
  FLAG.RTM's `flag_set_entry` calling its own `flag_valid`) round-trips
  normally. See `stop_addrs` / `call_stub_addrs` below for how a call into
  an *unported* routine (`dsptch`, `pdisp`, `xiosif`, ...) is handled --
  this is this module's equivalent of CP-M-C's `cpm_bios` boundary stubs.
- An opcode byte this module doesn't recognize raises I86SimError naming
  the address and byte, rather than silently decoding something wrong --
  that's the signal to extend the opcode table for the next file.
"""

from __future__ import annotations

import struct

RET_SENTINEL = 0xFFFF

_R16 = ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di']
_R8 = ['al', 'cl', 'dl', 'bl', 'ah', 'ch', 'dh', 'bh']
_SREG = ['es', 'cs', 'ss', 'ds']

# 8-bit register index (into _R8) -> (parent register in _R16, is-high-byte).
# There are no standalone 8-bit registers on the 8086: al/ah etc. are just
# the low/high byte of ax/cx/dx/bx, so self.regs only ever stores the 16-bit
# parents and these two helpers compute the alias on demand.
_R8_PARENT = [('ax', False), ('cx', False), ('dx', False), ('bx', False),
              ('ax', True), ('cx', True), ('dx', True), ('bx', True)]

# mod=00/10 ModRM rm -> (base reg name or None, index reg name or None)
_RM_MEM = {
    0b000: ('bx', 'si'),
    0b001: ('bx', 'di'),
    0b010: ('bp', 'si'),
    0b011: ('bp', 'di'),
    0b100: (None, 'si'),
    0b101: (None, 'di'),
    0b110: ('bp', None),  # mod=00,rm=110 is the direct-address exception (no bp)
    0b111: ('bx', None),
}


class I86SimError(Exception):
    pass


class I86Sim:
    """One 8086 core over a flat 64KB `mem` array.

    Construct with the code bytes for a single routine (placed in `mem` at
    `code_addr`) and an initial register dict, then call `run()`.

    `stop_addrs`: addresses that, when reached by a JMP (or by falling
    through), halt the sim immediately without executing anything there --
    for a routine whose real exit is a tail-jump into an unported module
    (e.g. IDLE.SUP's `jmp xiosif`), this is where the test stops and checks
    registers, same spirit as a CP/M BIOS call boundary.

    `call_stub_addrs`: addresses that, when reached by a near CALL, are
    treated as having returned immediately with no side effects -- for a
    routine that calls into an unported routine but keeps running
    afterwards (e.g. FLAG.RTM's `flag_wait_entry` calling `dsptch`, or
    SYSENT.RTM's `wakeup_entry` calling `pdisp`). This is accurate to how
    the real OS behaves from the calling process's own point of view: a
    dispatcher call either doesn't return to this process at all (so what
    it would have left in the registers is moot), or resumes it exactly
    here when it's next scheduled.

    `int_stub_nums`: interrupt numbers that, when reached by `INT imm8`,
    are treated the same way `call_stub_addrs` treats a CALL -- an instant
    no-op, continuing with the very next instruction. Some modules invoke
    the dispatcher via a software interrupt instead of an ordinary near
    CALL (e.g. `src-assem/rsp/CLOCK.A86`'s own `int mpmint`); unlike a real
    `INT`, this never pushes flags/CS/IP or consults the interrupt vector
    table -- there's no multi-segment model here for a real IRET to
    unwind, and the same "doesn't return, or resumes exactly here" calling
    convention applies.

    `call_hooks`: like `call_stub_addrs` (an instant no-op return, no
    push, no jump), but a Python callable (`hook(sim)`) runs first and
    may freely read/write `self.mem`/`self.regs` -- for a boundary call
    whose real effect genuinely has to be modeled for the test to mean
    anything (e.g. `LOAD.SUP`'s own `mpmif(f_freadseq)`, a real
    sequential disk read: the caller's own control flow depends on
    actual data coming back, not just a fixed pass/fail sentinel, so
    the plain no-op convention above isn't enough). An address in both
    `call_hooks` and `call_stub_addrs` just runs the hook; listing it in
    `call_stub_addrs` too is unnecessary.

    `jmp_hooks`: like `call_hooks`, but keyed by a near JMP's own
    target address instead of a CALL's -- for a routine whose own
    real tail is a JMP (not a CALL) into another module that itself
    eventually jumps back to right after this JMP (e.g. `src-assem/loader/
    LDMPM.A86`'s own `jmp mldbios`, which jumps into `src-assem/loader/
    LDBIOS86Q.A86`'s own `INIT`, whose own real tail `jmp ccp_offset`
    lands right back after LDMPM's own `jmp mldbios`), the same
    "boundary has to actually be modeled" reasoning as `call_hooks`,
    just for the JMP case: the hook runs and execution simply
    continues with the very next instruction after the JMP, same as
    a CALL hook continuing after the CALL.

    `jmp_stub_addrs`: addresses that, when reached by a near JMP
    (`0xE9`/`0xEB`), are treated as an immediate RET instead -- for a
    routine whose own real tail end is a JMP (not a CALL) into an
    unported boundary that the real hardware eventually returns
    through normally, so the *caller who did a plain CALL into this
    routine* still gets a normal return (e.g. `src-assem/kernel/FILE1.BDO`'s
    own `seek`, reached via `seekdir`'s own `call seekdir` from
    `rddir0`: `seek`'s own last I/O dispatch, `jmps setsecf`, is a
    tail JMP, not a CALL, so `call_stub_addrs` alone can't catch it --
    first needed by `src-c/kernel/file2.c`'s own `searchn`, whose own
    very first directory record read, on every call, always lands on
    this exact path). Unlike `stop_addrs` (which halts right there, for a
    routine whose own real exit genuinely never returns to ITS OWN
    caller), this pops the return address the enclosing routine's own
    `call` already pushed and keeps running there, same "doesn't
    return, or resumes exactly here" reasoning as `call_stub_addrs`,
    just for the JMP case instead of the CALL case. If that popped
    address is the simulator's `RET_SENTINEL`, the sim halts just as it
    does for a RET.

    `far_call_stub_addrs`: addresses of a `CALL FAR [mem]` instruction
    itself (opcode `0xFF /3`, e.g. `callf dword ptr rtmmod[bx]`) that
    should be treated as an instant no-op -- same "doesn't return, or
    resumes exactly here" reasoning as `call_stub_addrs`, but keyed by
    the *instruction's own* address instead of its target, since the
    target here is a 4-byte far pointer loaded from memory at runtime
    (e.g. a module table entry), not resolvable at assembly time the
    way an ordinary CALL's target is. There's no multi-segment model
    here for a real far call/return to unwind (same reasoning as
    `int_stub_nums` above), so this never pushes CS:IP or touches the
    stack -- `_modrm()` has already consumed the whole instruction's
    own mod/rm/displacement bytes by the time this fires, so doing
    nothing further is exactly "continue with the very next
    instruction." First needed by `src-assem/kernel/INIT.SUP`'s own `init:`,
    which `callf`s into five other modules' own `init:` entry points
    through `SYSDAT.FMT`'s module table.

    `far_call_hooks`: like `far_call_stub_addrs` (keyed by the `CALL FAR
    [mem]` instruction's own address, no push, continue with the next
    instruction), but a Python callable (`hook(sim)`) runs first -- the
    far-call counterpart of `call_hooks`, for a far boundary call whose
    real effect has to be modeled. First needed by `src-assem/kernel/
    BDOSIF.BDO`'s own `mpmif` (`callf cs:dword ptr .supervisor`), whose
    `init`-time queue calls tests/src-c/test_bdosif_init.py runs for real.

    `far_jmp_stub_addrs`: like `far_call_stub_addrs`, but for a
    `JMP FAR [mem]` instruction (opcode `0xFF /5`) instead of a
    `CALL FAR [mem]` -- there's no caller to return to for a JMP, so
    this halts the sim outright (same as `stop_addrs`) rather than
    continuing with the next instruction. First needed by
    `src-assem/loader/ROM86Q.A86`'s own final `jmpf dword ptr leap_off`,
    the boot sector's own real exit into MPMLDR.bin.

    `int_hooks`: like `call_hooks`, but keyed by interrupt number
    instead of a call target -- for a software interrupt whose real
    effect genuinely has to be modeled for the test to mean anything
    (e.g. a BIOS `INT 13h` disk read, or a loader's own synthetic
    `INT 224` dispatch into another already-ported module's
    function table), same "boundary has to actually be modeled"
    reasoning as `call_hooks`. `hook(sim)` runs in place of the
    no-op `int_stub_nums` convention (an interrupt number listed in
    both just runs the hook); same as `call_hooks`, there's no real
    multi-segment IRET here -- the hook returns and execution simply
    continues with the very next instruction, so a hook that needs
    to report failure (e.g. `INT 13h`'s own CF-on-error convention)
    sets `sim.cf` directly rather than anything IRET would normally
    restore. First needed by `src-assem/loader/ROM86Q.A86`'s own boot-
    sector `INT 13h` floppy reads.

    `io_in_hook`/`io_out_hook`: optional callables backing the real
    8086 port-mapped I/O instructions (`IN`/`OUT`, opcodes `0xE4`-
    `0xE7`/`0xEC`-`0xEF`) -- there's no real device behind these
    otherwise, so a routine that touches hardware ports needs one of
    these supplied (the same "boundary has to actually be modeled for
    the test to mean anything" reasoning as `call_hooks`, not a plain
    no-op stub). `io_in_hook(sim, port, width)` returns the value read
    (`width` is 8 or 16); `io_out_hook(sim, port, width, value)` is
    called for the write, return value ignored. Hitting `IN`/`OUT`
    with no hook supplied raises loudly, same as any other
    unimplemented opcode -- there's deliberately no silent default
    device (e.g. always-0) that could paper over a real driver's own
    polling loop never seeing the response it's waiting for. First
    needed by `src-assem/loader/LDBIOS86Q.A86`'s own UART driver (console
    I/O at port 0x3F8) and NEC765 floppy controller driver (port
    0x3F0) -- real port-mapped hardware, not a boundary stub this
    port can skip the way `xiosif`/`mpmif` elsewhere are.
    """

    def __init__(self, mem: bytearray, regs: dict, code_addr: int,
                 stop_addrs=frozenset(), call_stub_addrs=frozenset(),
                 int_stub_nums=frozenset(), segmented: bool = False,
                 call_hooks=None, jmp_hooks=None, jmp_stub_addrs=frozenset(),
                 far_call_stub_addrs=frozenset(), far_jmp_stub_addrs=frozenset(),
                 io_in_hook=None, io_out_hook=None, int_hooks=None,
                 far_call_hooks=None):
        if len(mem) != 0x10000:
            raise I86SimError(f"mem must be exactly 64KB, got {len(mem)} bytes")
        self.mem = mem
        self.regs = {r: 0 for r in _R16}
        self.regs.update({s: 0 for s in _SREG})
        self.regs.update(regs)
        self.stop_addrs = set(stop_addrs)
        self.call_stub_addrs = set(call_stub_addrs)
        self.jmp_stub_addrs = set(jmp_stub_addrs)
        self.far_call_stub_addrs = set(far_call_stub_addrs)
        self.far_jmp_stub_addrs = set(far_jmp_stub_addrs)
        self.io_in_hook = io_in_hook
        self.io_out_hook = io_out_hook
        self.int_stub_nums = set(int_stub_nums)
        self.call_hooks = dict(call_hooks) if call_hooks else {}
        self.jmp_hooks = dict(jmp_hooks) if jmp_hooks else {}
        self.int_hooks = dict(int_hooks) if int_hooks else {}
        self.far_call_hooks = dict(far_call_hooks) if far_call_hooks else {}
        self.segmented = segmented
        self.seg_override = None  # set per-instruction by a 26h/2Eh/36h/3Eh prefix
        self._last_ea_offset = 0  # raw (pre-segmentation) offset from the most
                                  # recent _modrm() mem operand -- LEA's own
                                  # value, see its own opcode handler
        self._push16_into(self.regs, RET_SENTINEL)
        self.ip = code_addr
        self.zf = 0
        self.cf = 0
        self.sf = 0
        self.of = 0
        self.df = 0  # direction flag -- CLD/STD; see string-op handling below
        self.halted = False

    # -- memory helpers -----------------------------------------------

    def _r8(self, addr: int) -> int:
        return self.mem[addr & 0xFFFF]

    def _w8(self, addr: int, val: int) -> None:
        self.mem[addr & 0xFFFF] = val & 0xFF

    def _r16(self, addr: int) -> int:
        addr &= 0xFFFF
        lo = self.mem[addr]
        hi = self.mem[(addr + 1) & 0xFFFF]
        return lo | (hi << 8)

    def _w16(self, addr: int, val: int) -> None:
        addr &= 0xFFFF
        self.mem[addr] = val & 0xFF
        self.mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF

    # -- fetch helpers ---------------------------------------------------

    def _fetch8(self) -> int:
        b = self.mem[self.ip]
        self.ip = (self.ip + 1) & 0xFFFF
        return b

    def _fetch16(self) -> int:
        lo = self._fetch8()
        hi = self._fetch8()
        return lo | (hi << 8)

    def _fetch_simm8(self) -> int:
        b = self._fetch8()
        return b - 0x100 if b & 0x80 else b

    def _fetch_simm16(self) -> int:
        return struct.unpack('<h', struct.pack('<H', self._fetch16()))[0]

    # -- segmentation (opt-in, see module docstring) ----------------------

    def _seg_addr(self, offset: int, default_seg: str, overridable: bool = True) -> int:
        """Resolve an effective (segment:offset) address. A no-op --
        returns `offset` unchanged -- unless self.segmented."""
        if not self.segmented:
            return offset & 0xFFFF
        seg_name = self.seg_override if (overridable and self.seg_override) else default_seg
        return (self.regs[seg_name] * 16 + offset) & 0xFFFF

    # -- ModRM decode -----------------------------------------------------
    # Returns (mod, reg_field, operand) where operand is either
    # ('reg', regname) for mod==11, or ('mem', address) otherwise.

    def _modrm(self):
        byte = self._fetch8()
        mod = byte >> 6
        reg = (byte >> 3) & 0b111
        rm = byte & 0b111
        if mod == 0b11:
            return mod, reg, ('reg', rm)
        if mod == 0b00 and rm == 0b110:
            addr = self._fetch16()  # direct address, no base register
            self._last_ea_offset = addr & 0xFFFF
            return mod, reg, ('mem', self._seg_addr(addr, 'ds'))
        base, index = _RM_MEM[rm]
        addr = 0
        if base is not None:
            addr += self.regs[base]
        if index is not None:
            addr += self.regs[index]
        if mod == 0b01:
            addr += self._fetch_simm8()
        elif mod == 0b10:
            addr += self._fetch16()
        default_seg = 'ss' if base == 'bp' else 'ds'
        self._last_ea_offset = addr & 0xFFFF
        return mod, reg, ('mem', self._seg_addr(addr, default_seg))

    def _get_r8(self, idx: int) -> int:
        parent, high = _R8_PARENT[idx]
        return (self.regs[parent] >> 8) & 0xFF if high else self.regs[parent] & 0xFF

    def _set_r8(self, idx: int, value: int) -> None:
        parent, high = _R8_PARENT[idx]
        value &= 0xFF
        if high:
            self.regs[parent] = (self.regs[parent] & 0x00FF) | (value << 8)
        else:
            self.regs[parent] = (self.regs[parent] & 0xFF00) | value

    def _load(self, operand, width: int) -> int:
        kind, val = operand
        if kind == 'reg':
            return self._get_r8(val) if width == 8 else self.regs[_R16[val]]
        return self._r8(val) if width == 8 else self._r16(val)

    def _store(self, operand, width: int, value: int) -> None:
        kind, val = operand
        if kind == 'reg':
            if width == 8:
                self._set_r8(val, value)
            else:
                self.regs[_R16[val]] = value & 0xFFFF
        elif width == 8:
            self._w8(val, value)
        else:
            self._w16(val, value)

    # -- run ---------------------------------------------------------

    def run(self, max_steps: int = 100000) -> None:
        for _ in range(max_steps):
            if self.halted:
                return
            if self.ip in self.stop_addrs:
                self.halted = True
                return
            self.step()
        raise I86SimError(f"did not halt within {max_steps} steps -- runaway loop?")

    def step(self) -> None:
        start_ip = self.ip
        self.seg_override = None
        op = self._fetch8()

        # Segment override prefixes: parsed for correct instruction length
        # always; only affect addressing when self.segmented -- see module
        # docstring and _seg_addr.
        if op in (0x26, 0x2E, 0x36, 0x3E):
            self.seg_override = _SREG[{0x26: 0, 0x2E: 1, 0x36: 2, 0x3E: 3}[op]]
            op = self._fetch8()

        if op == 0x9C:  # PUSHF
            self._push16(self._flags_word())
        elif op == 0x9D:  # POPF
            self._set_flags_word(self._pop16())
        elif op == 0x9E:  # SAHF -- flags (low byte) <- AH. First needed by
            # src-assem/kernel/FILE2.BDO's own SEARCHN (`LAHF`...`SAHF`, saving
            # `compext`'s own ZF result across two intervening MOVs that
            # don't touch it), tests/src-c/test_file2.py. Reuses
            # `_flags_word`'s own existing ZF/CF-only packing -- the same
            # minimal-flag convention every other flags-word consumer in
            # this module (PUSHF/POPF) already uses, sufficient here
            # since this specific caller only actually cares about ZF.
            ah = (self.regs['ax'] >> 8) & 0xFF
            self._set_flags_word(ah)
        elif op == 0x9F:  # LAHF -- AH <- flags (low byte), same packing as SAHF above
            self.regs['ax'] = (self.regs['ax'] & 0x00FF) | ((self._flags_word() & 0xFF) << 8)
        elif op == 0xF8:  # CLC
            self.cf = 0
        elif op == 0xF9:  # STC -- the C ROM86Q port's (since removed) read_sector
                          # is the first to need it (every earlier CF set
                          # came from an arithmetic/shift op, not STC
                          # directly)
            self.cf = 1
        elif op == 0x27:  # DAA -- decimal-adjust AL after addition. AF
            # (the half-carry flag) isn't tracked anywhere else in this
            # module (see its own docstring), so this only follows the
            # low/high-nibble->9 checks, not a carried-in AF -- exact for
            # every real caller seen so far (CLOCK.A86's own BCD counter
            # increment: CLC, INC AL, DAA on a value already known to be
            # valid BCD, so AF's only other trigger -- nibble wraparound
            # from a prior ADD -- never arises here).
            al = self.regs['ax'] & 0xFF
            if (al & 0x0F) > 9:
                al = (al + 6) & 0xFF
            if al > 0x9F:
                al = (al + 0x60) & 0xFF
                self.cf = 1
            self.regs['ax'] = (self.regs['ax'] & 0xFF00) | al
        elif op == 0xC2:  # RET imm16 (near, then drop imm16 bytes of arguments)
            drop = self._fetch16()
            target = self._pop16()
            self.regs['sp'] = (self.regs['sp'] + drop) & 0xFFFF
            if target == RET_SENTINEL:
                self.halted = True
            else:
                self.ip = target
        elif op == 0xC3:  # RET (near)
            target = self._pop16()
            if target == RET_SENTINEL:
                self.halted = True
            else:
                self.ip = target
        elif op == 0xE8:  # CALL near, rel16
            disp = self._fetch_simm16()
            ret_addr = self.ip
            target = (self.ip + disp) & 0xFFFF
            if target in self.call_hooks:
                self.call_hooks[target](self)
            elif target not in self.call_stub_addrs:
                self._push16(ret_addr)
                self.ip = target
            # else: stubbed -- treated as an instant no-op return, see class docstring
        elif op == 0xCD:  # INT imm8
            num = self._fetch8()
            if num in self.int_hooks:
                self.int_hooks[num](self)
            elif num not in self.int_stub_nums:
                raise I86SimError(f"INT 0x{num:02X} not stubbed (not in int_stub_nums) at {start_ip:#06x}")
            # else: stubbed -- treated as an instant no-op, see class docstring
        elif op in (0xE4, 0xE5):  # IN AL/AX, imm8 -- see io_in_hook's own docstring
            port = self._fetch8()
            width = 8 if op == 0xE4 else 16
            if self.io_in_hook is None:
                raise I86SimError(f"IN (port 0x{port:02X}) with no io_in_hook at {start_ip:#06x}")
            val = self.io_in_hook(self, port, width) & (0xFF if width == 8 else 0xFFFF)
            if width == 8:
                self.regs['ax'] = (self.regs['ax'] & 0xFF00) | val
            else:
                self.regs['ax'] = val
        elif op in (0xEC, 0xED):  # IN AL/AX, DX
            port = self.regs['dx']
            width = 8 if op == 0xEC else 16
            if self.io_in_hook is None:
                raise I86SimError(f"IN (port 0x{port:02X}) with no io_in_hook at {start_ip:#06x}")
            val = self.io_in_hook(self, port, width) & (0xFF if width == 8 else 0xFFFF)
            if width == 8:
                self.regs['ax'] = (self.regs['ax'] & 0xFF00) | val
            else:
                self.regs['ax'] = val
        elif op in (0xE6, 0xE7):  # OUT imm8, AL/AX
            port = self._fetch8()
            width = 8 if op == 0xE6 else 16
            val = self.regs['ax'] & (0xFF if width == 8 else 0xFFFF)
            if self.io_out_hook is None:
                raise I86SimError(f"OUT (port 0x{port:02X}) with no io_out_hook at {start_ip:#06x}")
            self.io_out_hook(self, port, width, val)
        elif op in (0xEE, 0xEF):  # OUT DX, AL/AX
            port = self.regs['dx']
            width = 8 if op == 0xEE else 16
            val = self.regs['ax'] & (0xFF if width == 8 else 0xFFFF)
            if self.io_out_hook is None:
                raise I86SimError(f"OUT (port 0x{port:02X}) with no io_out_hook at {start_ip:#06x}")
            self.io_out_hook(self, port, width, val)
        elif 0x50 <= op <= 0x57:  # PUSH r16
            self._push16(self.regs[_R16[op - 0x50]])
        elif 0x58 <= op <= 0x5F:  # POP r16
            self.regs[_R16[op - 0x58]] = self._pop16()
        elif op in (0x06, 0x0E, 0x16, 0x1E):  # PUSH segreg
            self._push16(self.regs[_SREG[(op >> 3) & 0b11]])
        elif op in (0x07, 0x17, 0x1F):  # POP segreg (0x0F is a 2-byte escape on later CPUs, not POP CS)
            self.regs[_SREG[(op >> 3) & 0b11]] = self._pop16()
        elif op == 0x8F:  # POP r/m16 (always /0)
            _, _, rm = self._modrm()
            self._store(rm, 16, self._pop16())
        elif op == 0x86:  # XCHG r/m8, r8
            _, reg, rm = self._modrm()
            a, b = self._load(rm, 8), self._get_r8(reg)
            self._store(rm, 8, b)
            self._set_r8(reg, a)
        elif op == 0x87:  # XCHG r/m16, r16
            _, reg, rm = self._modrm()
            a, b = self._load(rm, 16), self.regs[_R16[reg]]
            self._store(rm, 16, b)
            self.regs[_R16[reg]] = a
        elif op == 0x8E:  # MOV segreg, r/m16
            _, reg, rm = self._modrm()
            self.regs[_SREG[reg & 0b11]] = self._load(rm, 16)
        elif op == 0x8C:  # MOV r/m16, segreg
            _, reg, rm = self._modrm()
            self._store(rm, 16, self.regs[_SREG[reg & 0b11]])
        elif op == 0x8B:  # MOV r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._load(rm, 16)
        elif op == 0xA0:  # MOV AL, [imm16] -- accumulator short form of the
                           # same direct ("mod=00,rm=110") addressing mode
                           # as _modrm's own `mem` case, just without a
                           # ModRM byte (one of the assembler's own size
                           # optimizations for AL/AX, first seen in
                           # src-c/kernel/command.c's own differential test)
            addr = self._fetch16()
            self._set_r8(0, self._r8(self._seg_addr(addr, 'ds')))
        elif op == 0xA1:  # MOV AX, [imm16]
            addr = self._fetch16()
            self.regs['ax'] = self._r16(self._seg_addr(addr, 'ds'))
        elif op == 0xA2:  # MOV [imm16], AL
            addr = self._fetch16()
            self._w8(self._seg_addr(addr, 'ds'), self._get_r8(0))
        elif op == 0xA3:  # MOV [imm16], AX
            addr = self._fetch16()
            self._w16(self._seg_addr(addr, 'ds'), self.regs['ax'])
        elif op == 0x89:  # MOV r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self.regs[_R16[reg]])
        elif op == 0x8A:  # MOV r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._load(rm, 8))
        elif op == 0x88:  # MOV r/m8, r8
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._get_r8(reg))
        elif op == 0x8D:  # LEA r16, r/m16 (rm must be 'mem'; the *raw offset* is
                          # the value -- never segment-adjusted, even under
                          # segmented=True: LEA computes an effective address,
                          # not a physical one. rm[1] (from _modrm) is the
                          # already-segmented address used by ordinary loads/
                          # stores, so this must use self._last_ea_offset
                          # (the same ModRM decode's pre-segmentation value)
                          # instead -- confirmed as a real bug via src-c/
                          # kernel/mau.c's own differential test, the first
                          # file to exercise LEA with a non-zero DS under
                          # segmented=True (every earlier one happened to
                          # have DS=0 at its own LEA, masking this).
            _, reg, rm = self._modrm()
            if rm[0] != 'mem':
                raise I86SimError(f"LEA with a register operand at {start_ip:#06x}")
            self.regs[_R16[reg]] = self._last_ea_offset
        elif 0xB0 <= op <= 0xB7:  # MOV r8, imm8 (short form) --
                                  # the C LBDOSENT port's (since removed)
                                  # differential test is the first to
                                  # need this (every earlier immediate-
                                  # to-register MOV used 0xC6/0xC7's
                                  # register-direct ModRM form instead)
            self._set_r8(op - 0xB0, self._fetch8())
        elif 0xB8 <= op <= 0xBF:  # MOV r16, imm16 (short form)
            self.regs[_R16[op - 0xB8]] = self._fetch16()
        elif op == 0xC6:  # MOV r/m8, imm8
            _, _reg, rm = self._modrm()
            self._store(rm, 8, self._fetch8())
        elif op == 0xC7:  # MOV r/m16, imm16
            _, _reg, rm = self._modrm()
            self._store(rm, 16, self._fetch16())
        elif op == 0x28:  # SUB r/m8, r8 -- src-c/kernel/load.c's own
                          # differential test is the first to need this
                          # direction (every earlier SUB seen used a
                          # register destination, 0x2A/0x2B below)
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._sub(self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x29:  # SUB r/m16, r16 -- same as 0x28, word form
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._sub(self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op == 0x2A:  # SUB r8, r/m8  (al = al - [di], etc.)
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._sub(self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x2B:  # SUB r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._sub(self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x10:  # ADC r/m8, r8
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._adc(self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x12:  # ADC r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._adc(self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x11:  # ADC r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._adc(self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op == 0x13:  # ADC r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._adc(self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x18:  # SBB r/m8, r8 (plmc's MINUS, docs/bugs.md #131)
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._sbb(self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x1A:  # SBB r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._sbb(self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x19:  # SBB r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._sbb(self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op == 0x1B:  # SBB r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._sbb(self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x00:  # ADD r/m8, r8
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._add(self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x02:  # ADD r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._add(self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x01:  # ADD r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._add(self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op == 0x03:  # ADD r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._add(self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x38:  # CMP r/m8, r8
            _, reg, rm = self._modrm()
            self._sub(self._load(rm, 8), self._get_r8(reg), 8)  # discards the result, keeps the flags
        elif op == 0x39:  # CMP r/m16, r16
            _, reg, rm = self._modrm()
            self._sub(self._load(rm, 16), self.regs[_R16[reg]], 16)  # discards the result, keeps the flags
        elif op == 0x3A:  # CMP r8, r/m8
            _, reg, rm = self._modrm()
            self._sub(self._get_r8(reg), self._load(rm, 8), 8)  # discards the result, keeps the flags
        elif op == 0x3B:  # CMP r16, r/m16
            _, reg, rm = self._modrm()
            self._sub(self.regs[_R16[reg]], self._load(rm, 16), 16)  # discards the result, keeps the flags
        elif op == 0x20:  # AND r/m8, r8
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._bitwise('and', self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x22:  # AND r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._bitwise('and', self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x21:  # AND r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._bitwise('and', self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op == 0x23:  # AND r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._bitwise('and', self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x30:  # XOR r/m8, r8
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._bitwise('xor', self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x32:  # XOR r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._bitwise('xor', self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x31:  # XOR r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._bitwise('xor', self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op == 0x33:  # XOR r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._bitwise('xor', self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x0A:  # OR r8, r/m8
            _, reg, rm = self._modrm()
            self._set_r8(reg, self._bitwise('or', self._get_r8(reg), self._load(rm, 8), 8))
        elif op == 0x0B:  # OR r16, r/m16
            _, reg, rm = self._modrm()
            self.regs[_R16[reg]] = self._bitwise('or', self.regs[_R16[reg]], self._load(rm, 16), 16)
        elif op == 0x08:  # OR r/m8, r8 -- the C LBDOSENT port's (since removed)
                          # differential test is the first to need this
                          # direction (every earlier OR used 0x0A/0x0B)
            _, reg, rm = self._modrm()
            self._store(rm, 8, self._bitwise('or', self._load(rm, 8), self._get_r8(reg), 8))
        elif op == 0x09:  # OR r/m16, r16
            _, reg, rm = self._modrm()
            self._store(rm, 16, self._bitwise('or', self._load(rm, 16), self.regs[_R16[reg]], 16))
        elif op in (0x81, 0x80):  # group1 word/byte, imm16/imm8 (ADD/OR/AND/SUB/XOR/CMP)
            width = 16 if op == 0x81 else 8
            _, reg, rm = self._modrm()
            imm = self._fetch16() if width == 16 else self._fetch8()
            self._group1(reg, rm, imm, width)
        elif op in (0x04, 0x05, 0x0C, 0x0D, 0x14, 0x15, 0x1C, 0x1D,
                    0x24, 0x25, 0x2C, 0x2D, 0x34, 0x35, 0x3C, 0x3D):
            # ADD/OR/ADC/SBB/AND/SUB/XOR/CMP AL/AX,imm -- the dedicated,
            # shorter accumulator-immediate form (idx*8+4/+5) tools/
            # asm86.py's own `_encode_alu` now emits for an AL/AX
            # destination instead of always the longer group1 r/m,imm
            # form (0x80/0x81 + modrm) -- see that function's own
            # header comment for why (a real DRI source's own `$+N`
            # literal byte-offset jump, calibrated against the *real*
            # assembler's own shorter encoding, silently landing
            # mid-instruction otherwise). First needed by `search`'s
            # own SEARCHN cluster, tests/src-c/test_file2.py.
            width = 8 if op % 2 == 0 else 16
            reg = op >> 3
            imm = self._fetch8() if width == 8 else self._fetch16()
            self._group1(reg, ('reg', 0), imm, width)
        elif op in (0xD0, 0xD1, 0xD2, 0xD3):  # group2 shift/rotate (only SHL/4 and SHR/5 implemented)
            width = 8 if op in (0xD0, 0xD2) else 16
            by_cl = op in (0xD2, 0xD3)
            _, reg, rm = self._modrm()
            count = self._get_r8(1) if by_cl else 1  # _R8_PARENT[1] is 'cl'
            mask = 0xFF if width == 8 else 0xFFFF
            val = self._load(rm, width)
            if reg == 0b000:  # ROL
                bits = width
                c = count % bits
                result = ((val << c) | (val >> (bits - c))) & mask if c else val
                self.cf = result & 1
                self._store(rm, width, result)
                return
            if reg == 0b001:  # ROR
                bits = width
                c = count % bits
                result = ((val >> c) | (val << (bits - c))) & mask if c else val
                self.cf = (result >> (width - 1)) & 1
                self._store(rm, width, result)
                return
            if reg == 0b010:  # RCL -- rotate left through CF
                result = val
                for _ in range(count):
                    new_cf = (result >> (width - 1)) & 1
                    result = ((result << 1) | self.cf) & mask
                    self.cf = new_cf
                self._store(rm, width, result)
                return
            if reg == 0b011:  # RCR -- rotate right through CF
                result = val
                for _ in range(count):
                    new_cf = result & 1
                    result = ((result >> 1) | (self.cf << (width - 1))) & mask
                    self.cf = new_cf
                self._store(rm, width, result)
                return
            if reg not in (0b100, 0b101):
                raise I86SimError(f"group2 /{reg} (0x{op:02X}) not implemented at {start_ip:#06x}")
            result = ((val << count) & mask) if reg == 0b100 else (val >> count) & mask
            self._store(rm, width, result)
            self.zf = 1 if result == 0 else 0
        elif op in (0xFE, 0xFF):  # INC/DEC r/m8 or r/m16 (/0 INC, /1 DEC); FF also /6 PUSH r/m16
            width = 8 if op == 0xFE else 16
            _, reg, rm = self._modrm()
            if op == 0xFF and reg == 0b110:  # PUSH r/m16
                self._push16(self._load(rm, 16))
            elif op == 0xFF and reg == 0b010:  # CALL r/m16 (near indirect) --
                                                # src-c/kernel/abort.c's own
                                                # `call cs:abort_tab[di]` is
                                                # the first routine to need
                                                # this (every earlier near
                                                # CALL target was a fixed
                                                # rel16, opcode 0xE8)
                target = self._load(rm, 16)
                self._push16(self.ip)
                self.ip = (self.regs['cs'] * 16 + target) & 0xFFFF
            elif op == 0xFF and reg == 0b100:  # JMP r/m16 (near indirect)
                target = self._load(rm, 16)
                self.ip = (self.regs['cs'] * 16 + target) & 0xFFFF
            elif op == 0xFF and reg == 0b011:  # CALL FAR [mem] (indirect far
                                                # call through a 4-byte far
                                                # pointer in memory) -- see
                                                # `far_call_stub_addrs`'s own
                                                # docstring above for why
                                                # this is keyed by the
                                                # instruction's own address
                if start_ip in self.far_call_hooks:
                    self.far_call_hooks[start_ip](self)
                elif start_ip in self.far_call_stub_addrs:
                    pass  # no-op -- ip is already past the whole instruction
                else:
                    raise I86SimError(f"CALL FAR r/m not implemented (no stub) at {start_ip:#06x}")
            elif op == 0xFF and reg == 0b101:  # JMP FAR [mem] (indirect far
                                                # jump through a 4-byte far
                                                # pointer in memory) -- see
                                                # `far_jmp_stub_addrs`'s own
                                                # docstring above; unlike
                                                # `far_call_stub_addrs`,
                                                # there's no caller to
                                                # return to, so this halts
                                                # outright, same as
                                                # `stop_addrs`
                if start_ip in self.far_jmp_stub_addrs:
                    self.halted = True
                else:
                    raise I86SimError(f"JMP FAR r/m not implemented (no stub) at {start_ip:#06x}")
            elif reg in (0b000, 0b001):
                mask = 0xFF if width == 8 else 0xFFFF
                delta = 1 if reg == 0b000 else -1
                val = (self._load(rm, width) + delta) & mask
                self._store(rm, width, val)
                self.zf = 1 if val == 0 else 0
            else:
                raise I86SimError(f"group (0x{op:02X}) /{reg} not implemented at {start_ip:#06x}")
        elif op in (0x84, 0x85):  # TEST r/m8,r8 / TEST r/m16,r16 -- src-c/
                                  # kernel/abort.c's own differential test
                                  # is the first to need a register-vs-
                                  # register/memory TEST (every earlier one
                                  # used the F6/F7 immediate form below)
            width = 8 if op == 0x84 else 16
            _, reg, rm = self._modrm()
            if width == 8:
                self._bitwise('and', self._load(rm, 8), self._get_r8(reg), 8)
            else:
                self._bitwise('and', self._load(rm, 16), self.regs[_R16[reg]], 16)
        elif op in (0xA8, 0xA9):  # TEST AL,imm8 / TEST AX,imm16 -- src-c/
                                  # loader/ldbios86q.c's own fdc_wait_seek
                                  # is the first to need the accumulator-
                                  # immediate TEST form (every earlier one
                                  # used the F6/F7 or 0x84/0x85 forms)
            if op == 0xA8:
                imm = self._fetch8()
                self._bitwise('and', self._get_r8(0), imm, 8)  # _R8_PARENT[0] is 'al'
            else:
                imm = self._fetch16()
                self._bitwise('and', self.regs['ax'], imm, 16)
        elif op in (0xF6, 0xF7):  # group F6/F7 byte/word (only /0 TEST, /4 MUL implemented)
            width = 8 if op == 0xF6 else 16
            _, reg, rm = self._modrm()
            if reg == 0b000:  # TEST r/m, imm
                imm = self._fetch8() if width == 8 else self._fetch16()
                self._bitwise('and', self._load(rm, width), imm, width)  # discards the result, keeps the flags
            elif reg == 0b010:  # NOT r/m -- flags unaffected
                mask = 0xFF if width == 8 else 0xFFFF
                self._store(rm, width, (~self._load(rm, width)) & mask)
            elif reg == 0b011:  # NEG r/m
                self._store(rm, width, self._sub(0, self._load(rm, width), width))
            elif reg == 0b100:  # MUL r/m16 -> dx:ax; MUL r/m8 -> ax
                if width == 8:
                    product = self._get_r8(0) * self._load(rm, 8)  # _R8_PARENT[0] is 'al'
                    self.regs['ax'] = product & 0xFFFF
                else:
                    product = self.regs['ax'] * self._load(rm, 16)
                    self.regs['ax'] = product & 0xFFFF
                    self.regs['dx'] = (product >> 16) & 0xFFFF
            elif reg == 0b110:  # DIV r/m16: dx:ax / r/m16 -> ax=quot, dx=rem;
                                 # DIV r/m8: ax / r/m8 -> al=quot, ah=rem
                divisor = self._load(rm, width)
                if width == 8:
                    dividend = self.regs['ax']
                    quot, rem = divmod(dividend, divisor)
                    self.regs['ax'] = ((rem & 0xFF) << 8) | (quot & 0xFF)
                else:
                    dividend = (self.regs['dx'] << 16) | self.regs['ax']
                    quot, rem = divmod(dividend, divisor)
                    self.regs['ax'] = quot & 0xFFFF
                    self.regs['dx'] = rem & 0xFFFF
            else:
                raise I86SimError(f"group (0x{op:02X}) /{reg} not implemented at {start_ip:#06x}")
        elif op in (0xA4, 0xA5, 0xAA, 0xAB, 0xAC, 0xAD, 0xA6, 0xA7, 0xAE, 0xAF):  # string ops, no REP
            self._string_op_once(op)
        elif op in (0xF3, 0xF2):  # REP / REPE/REPZ / REPNE/REPNZ prefix
            str_op = self._fetch8()
            self._string_op_rep(op, str_op)
        elif op == 0x76:  # JBE/JNA rel8 (CF set or ZF set)
            disp = self._fetch_simm8()
            if self.cf or self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x77:  # JA/JNBE rel8 (CF clear and ZF clear)
            disp = self._fetch_simm8()
            if not self.cf and not self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x72:  # JB/JC rel8 (CF set)
            disp = self._fetch_simm8()
            if self.cf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x73:  # JAE/JNB rel8 (CF clear)
            disp = self._fetch_simm8()
            if not self.cf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x74:  # JZ rel8
            disp = self._fetch_simm8()
            if self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x75:  # JNZ rel8
            disp = self._fetch_simm8()
            if not self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x7C:  # JL/JNGE rel8 (signed less-than: SF^OF)
            disp = self._fetch_simm8()
            if self.sf != self.of:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x7D:  # JGE/JNL rel8 (signed: !(SF^OF))
            disp = self._fetch_simm8()
            if self.sf == self.of:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x7E:  # JLE/JNG rel8 (signed: (SF^OF) or ZF)
            disp = self._fetch_simm8()
            if (self.sf != self.of) or self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0x7F:  # JG/JNLE rel8 (signed: !(SF^OF) and !ZF)
            disp = self._fetch_simm8()
            if (self.sf == self.of) and not self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0xE2:  # LOOP rel8
            disp = self._fetch_simm8()
            self.regs['cx'] = (self.regs['cx'] - 1) & 0xFFFF
            if self.regs['cx'] != 0:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0xE1:  # LOOPE/LOOPZ rel8
            disp = self._fetch_simm8()
            self.regs['cx'] = (self.regs['cx'] - 1) & 0xFFFF
            if self.regs['cx'] != 0 and self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0xE0:  # LOOPNE/LOOPNZ rel8
            disp = self._fetch_simm8()
            self.regs['cx'] = (self.regs['cx'] - 1) & 0xFFFF
            if self.regs['cx'] != 0 and not self.zf:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0xE3:  # JCXZ rel8
            disp = self._fetch_simm8()
            if self.regs['cx'] == 0:
                self.ip = (self.ip + disp) & 0xFFFF
        elif op == 0xEB:  # JMP rel8 (JMPS)
            disp = self._fetch_simm8()
            target = (self.ip + disp) & 0xFFFF
            if target in self.jmp_hooks:
                self.jmp_hooks[target](self)  # ip stays as-is -- continue past this JMP
            elif target in self.jmp_stub_addrs:
                ret_addr = self._pop16()
                if ret_addr == RET_SENTINEL:
                    self.halted = True
                else:
                    self.ip = ret_addr
            else:
                self.ip = target
        elif op == 0xE9:  # JMP rel16
            disp = self._fetch_simm16()
            target = (self.ip + disp) & 0xFFFF
            if target in self.jmp_hooks:
                self.jmp_hooks[target](self)  # ip stays as-is -- continue past this JMP
            elif target in self.jmp_stub_addrs:
                ret_addr = self._pop16()
                if ret_addr == RET_SENTINEL:
                    self.halted = True
                else:
                    self.ip = ret_addr
            else:
                self.ip = target
        elif op == 0xFA:  # CLI -- no interrupt model here; parsed and ignored
            pass
        elif op == 0xFB:  # STI -- ditto
            pass
        elif op == 0xFC:  # CLD
            self.df = 0
        elif op == 0xFD:  # STD -- src-c/kernel/mau.c's own mau_insert is the
                          # first routine seen needing a real backward
                          # string copy (shifting a SAT table up to make
                          # room) -- see the string-instructions section's
                          # own updated docstring note below
            self.df = 1
        else:
            raise I86SimError(f"unimplemented opcode 0x{op:02X} at {start_ip:#06x}")

    def _group1(self, reg, rm, imm, width):
        # group1 (ADD/OR/ADC/SBB/AND/SUB/XOR/CMP) /digit matches ALU's own
        # 0-7 index order (tools/asm86/isa.py's ALU table) -- not a
        # coincidence, it's the real 8086 encoding.
        val = self._load(rm, width)
        if reg == 0b000:  # ADD
            self._store(rm, width, self._add(val, imm, width))
        elif reg == 0b010:  # ADC
            self._store(rm, width, self._adc(val, imm, width))
        elif reg == 0b011:  # SBB
            self._store(rm, width, self._sbb(val, imm, width))
        elif reg == 0b001:  # OR
            self._store(rm, width, self._bitwise('or', val, imm, width))
        elif reg == 0b100:  # AND
            self._store(rm, width, self._bitwise('and', val, imm, width))
        elif reg == 0b101:  # SUB
            self._store(rm, width, self._sub(val, imm, width))
        elif reg == 0b110:  # XOR
            self._store(rm, width, self._bitwise('xor', val, imm, width))
        elif reg == 0b111:  # CMP
            self._sub(val, imm, width)  # discards the result, keeps the flags

    def _bitwise(self, op: str, a: int, b: int, width: int) -> int:
        mask = 0xFF if width == 8 else 0xFFFF
        result = {'and': a & b, 'or': a | b, 'xor': a ^ b}[op] & mask
        self.zf = 1 if result == 0 else 0
        self.cf = 0  # AND/OR/XOR always clear CF on the 8086
        return result

    # -- string instructions ---------------------------------------------
    # DF (direction, CLD/STD) is tracked (self.df) -- SI/DI advance forward
    # by default, backward when DF=1 (src-c/kernel/mau.c's own mau_insert
    # needs a real backward shift to open a gap in a SAT table without
    # self-clobbering the overlapping region; every earlier file only
    # ever did a forward copy/scan).

    def _string_op_once(self, op: int) -> None:
        width = 16 if op & 1 else 8  # byte/word forms are adjacent opcodes
        step = (2 if width == 16 else 1) * (-1 if self.df else 1)
        # SI's own segment is DS by default, overridable; DI's is always
        # ES, never overridable -- same as real hardware (only matters
        # when self.segmented; a no-op plain offset otherwise).
        si_addr = self._seg_addr(self.regs['si'], 'ds')
        di_addr = self._seg_addr(self.regs['di'], 'es', overridable=False)
        if op in (0xA4, 0xA5):  # MOVS
            self._store(('mem', di_addr), width, self._load(('mem', si_addr), width))
            self.regs['si'] = (self.regs['si'] + step) & 0xFFFF
            self.regs['di'] = (self.regs['di'] + step) & 0xFFFF
        elif op in (0xAA, 0xAB):  # STOS
            src = self._get_r8(0) if width == 8 else self.regs['ax']  # _R8_PARENT[0] is 'al'
            self._store(('mem', di_addr), width, src)
            self.regs['di'] = (self.regs['di'] + step) & 0xFFFF
        elif op in (0xAC, 0xAD):  # LODS
            val = self._load(('mem', si_addr), width)
            if width == 8:
                self._set_r8(0, val)
            else:
                self.regs['ax'] = val
            self.regs['si'] = (self.regs['si'] + step) & 0xFFFF
        elif op in (0xA6, 0xA7):  # CMPS
            self._sub(self._load(('mem', si_addr), width), self._load(('mem', di_addr), width), width)
            self.regs['si'] = (self.regs['si'] + step) & 0xFFFF
            self.regs['di'] = (self.regs['di'] + step) & 0xFFFF
        elif op in (0xAE, 0xAF):  # SCAS
            a = self._get_r8(0) if width == 8 else self.regs['ax']
            self._sub(a, self._load(('mem', di_addr), width), width)
            self.regs['di'] = (self.regs['di'] + step) & 0xFFFF
        else:
            raise I86SimError(f"unimplemented string opcode 0x{op:02X}")

    def _string_op_rep(self, prefix: int, str_op: int) -> None:
        unconditional = str_op in (0xA4, 0xA5, 0xAA, 0xAB, 0xAC, 0xAD)  # MOVS/STOS/LODS: plain REP
        want_zf = 1 if prefix == 0xF3 else 0  # REPE/REPZ=0xF3 vs REPNE/REPNZ=0xF2, for CMPS/SCAS only
        while self.regs['cx'] != 0:
            self.regs['cx'] = (self.regs['cx'] - 1) & 0xFFFF
            self._string_op_once(str_op)
            if not unconditional and self.zf != want_zf:
                break

    def _sub(self, a: int, b: int, width: int) -> int:
        mask = 0xFF if width == 8 else 0xFFFF
        sign_bit = 0x80 if width == 8 else 0x8000
        result = (a - b) & mask
        self.zf = 1 if result == 0 else 0
        self.cf = 1 if a < b else 0  # unsigned borrow
        self.sf = 1 if result & sign_bit else 0
        self.of = 1 if ((a ^ b) & (a ^ result) & sign_bit) else 0
        return result

    def _add(self, a: int, b: int, width: int) -> int:
        mask = 0xFF if width == 8 else 0xFFFF
        sign_bit = 0x80 if width == 8 else 0x8000
        result = (a + b) & mask
        self.zf = 1 if result == 0 else 0
        self.cf = 1 if (a + b) > mask else 0  # unsigned carry
        self.sf = 1 if result & sign_bit else 0
        self.of = 1 if (~(a ^ b) & (a ^ result) & sign_bit) else 0
        return result

    def _adc(self, a: int, b: int, width: int) -> int:
        return self._add(a, b + self.cf, width)

    def _sbb(self, a: int, b: int, width: int) -> int:
        return self._sub(a, b + self.cf, width)

    # -- flags / stack -------------------------------------------------
    # Only ZF and CF round-trip through PUSHF/POPF; every other flag bit
    # reads back as 0 and is discarded on POPF. Good enough for the
    # pushf/cli/.../popf critical-section idiom these routines use, where
    # nothing the test checks depends on the other flag bits surviving.

    def _flags_word(self) -> int:
        return (0x0040 if self.zf else 0) | (0x0001 if self.cf else 0)

    def _set_flags_word(self, word: int) -> None:
        self.zf = 1 if (word & 0x0040) else 0
        self.cf = 1 if (word & 0x0001) else 0

    def _push16(self, value: int) -> None:
        self._push16_into(self.regs, value)

    def _push16_into(self, regs: dict, value: int) -> None:
        regs['sp'] = (regs['sp'] - 2) & 0xFFFF
        self._w16(self._stack_addr(regs), value)

    def _pop16(self) -> int:
        value = self._r16(self._stack_addr(self.regs))
        self.regs['sp'] = (self.regs['sp'] + 2) & 0xFFFF
        return value

    def _stack_addr(self, regs: dict) -> int:
        # SS:SP, exactly like any other segment:offset pair -- see the
        # module docstring's `segmented=True` rule. Every routine before
        # src-c/kernel/dsptch.c's own differential test only ever ran
        # with SS=0 (so plain SP already was the flat address and this
        # was never exercised); DSPTCH.RTM's own `switch:` is the first
        # to really switch SS (`mov ss,u_ss`) before its own final RET,
        # so PUSH/POP need to honor it too.
        if not self.segmented:
            return regs['sp'] & 0xFFFF
        return (regs['ss'] * 16 + regs['sp']) & 0xFFFF
