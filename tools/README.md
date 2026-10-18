# asm86 — an ASM86-dialect assembler for this source tree

`tools/asm86/` is a from-scratch, two-pass 8086 assembler written in
Python, covering the specific dialect and instruction/directive subset
used by the files in `src/`. It is **not** a general-purpose, fully
MASM/ASM86-compatible assembler, and it does not aim to produce
byte-identical output to the original DRI `ASM86`/`GENCMD` toolchain —
only functionally equivalent 8086 machine code. See `docs/build.md` for
why a modern build still can't boot under QEMU unmodified (the XIOS/boot
ROM target specific Intel SBC hardware); this tool only solves "there is
no runnable ASM86 assembler on a modern machine."

## Usage

```
python3 tools/asm86.py path/to/FILE.A86 [-I includedir ...] [-o outstem] [--symbols]
```

Writes `<outstem>.<segment>.bin` for each non-empty segment used (e.g.
`.cseg.bin`, `.dseg.bin`, `.aseg.bin`) — raw binary dumps of that
segment's address space, not Intel HEX. `--symbols` prints the resolved
symbol table instead of assembling output, which is useful for checking
a file without worrying about which segment layout you want downstream.

`INCLUDE filename` is resolved case-insensitively, first in the source
file's own directory, then in any `-I` directories given.

## What it supports

- Statement syntax: `;` comments, `!` as a multi-statement line
  separator, labels with or without a trailing `:`.
- Directives: `TITLE`, `NAME`, `CSEG`, `DSEG`, `ASEG`, `ORG`, `EQU`, `DB`,
  `DW`, `RB`, `RW`, `END`, `INCLUDE`, `IF`/`ENDIF` (constant/flag
  expressions only — no `ELSE`), `EJECT`, `PAGE`, `PUBLIC`/`EXTRN`/`GLOBAL`
  (parsed and ignored — no linker; see Limitations).
- Expressions: `+ - * / MOD SHL SHR AND OR XOR NOT`, unary `-`, `OFFSET`,
  `BYTE PTR`/`WORD PTR` casts, `$` (current location), decimal/`H`/`O`/`Q`/
  `B`-suffixed numbers, `'x'`/`'xy'` character literals.
- Addressing: all 8 valid 8086 16-bit memory combinations
  (`[BX]`,`[BP]`,`[SI]`,`[DI]`,`[BX+SI]`,`[BX+DI]`,`[BP+SI]`,`[BP+DI]`),
  `SYMBOL[REG(+REG)]` indexing, segment override prefixes (`cs:` etc.),
  direct addressing of a bare symbol.
- Instructions: the full data-transfer/arithmetic/logic group (`MOV`,
  `ADD`, `SUB`, `CMP`, `AND`, `OR`, `XOR`, `TEST`, `ADC`, `SBB`), `INC`/
  `DEC`/`NEG`/`NOT`/`MUL`/`IMUL`/`DIV`/`IDIV`, shifts/rotates (`SHL`/`SAL`/
  `SHR`/`SAR`/`ROL`/`ROR`/`RCL`/`RCR`, count of 1 or `CL` only — true to
  the 8086), `PUSH`/`POP`/`XCHG`/`LEA`/`IN`/`OUT`, all conditional jumps,
  `LOOP`/`LOOPE`/`LOOPNE`/`JCXZ`, `JMP`/`JMPS`/`CALL`/`RET` (near) and
  `JMPF`/`CALLF`/`RETF` (far), `INT`, the string group (`MOVS`/`LODS`/
  `STOS`/`CMPS`/`SCAS` in both DRI's `lods al`-style and the `lodsb`-style
  forms) with `REP`/`REPE`/`REPNE` prefixes, and the common no-operand
  flag/misc instructions (`CLC`, `STI`, `DAA`, `CBW`, `PUSHF`, ...).
- A DRI-specific quirk: a leading `.` on an operand (e.g. `.s_version`)
  referencing an `EQU` constant is treated as cosmetic, matching the
  source's own convention.
- CP/M `^Z` (0x1A) end-of-file padding is stripped automatically.

## Design choices that trade size for simplicity

- **Displacements are always disp16** when present (never the ModRM
  disp8 form), and plain `JMP`/`CALL` (no explicit `JMPS`) always use
  the full near form (`E9`/`E8`). This means a *memory operand*'s length
  never depends on a forward reference's eventual value, so there's
  nothing to relax there — at the cost of slightly larger code than the
  original DRI output would have produced.
- **`JMPS`/every `Jcc`/`LOOP*`/`JCXZ` do get real jump-size relaxation.**
  These are hardware-limited to an 8-bit displacement in their normal
  2-byte form, and the disp16-everywhere trade-off above can genuinely
  push an in-range branch out of range in this assembler's (larger)
  encoding even when DRI's own assembler kept it in range — this
  actually happened in practice (see `docs/bugs.md` bug #6: `BDOS.A86`,
  `MEM.A86`, `RTM.A86`, `SUP.A86`, `LDBIOS.A86`). `Assembler._relax_jumps`
  (`tools/asm86/assembler.py`) handles it: it re-runs pass 1 in a loop,
  assigning each `JMPS`/`Jcc`/`LOOP*`/`JCXZ` occurrence a stable
  sequential index and checking its real (now fully resolved) target
  against the 2-byte form's range after each run; any site found out of
  range gets permanently marked "long" (`EncodeCtx.jump_long`) and the
  loop reruns pass 1 again, since growing one site can push others out
  of range too. This is guaranteed to terminate: growing a site only
  ever *increases* distances for everyone else, never decreases them, so
  `jump_long` only grows monotonically. `JMPS`'s long form is simply the
  real 3-byte near `JMP` (`E9`) — it's already unconditional, so there's
  nothing clever needed. The conditional forms can't do that (there's no
  16-bit-displacement encoding for a `Jcc`/`LOOP`/`JCXZ` on the 8086 at
  all), so they grow to an 8-byte trampoline instead — see the comment on
  `_encode_jcc` in `tools/asm86/isa.py` for the exact byte layout — which
  works identically for every mnemonic in the set without needing a
  table of "inverse condition" opcodes.
- **No sign-extended-immediate optimization**: `ADD r/m,imm` always uses
  the full-width immediate opcode matching the destination's size.
- **No real segmentation**: `JMPF`/`CALLF` to a direct label always write
  segment `0000`. This tree only uses far jumps within a single module's
  own code segment (XIOS/loader boot-time jumps), where the real segment
  is only known once the loader places the module in memory, so this
  was never going to be resolvable at single-file assembly time anyway.
- Two passes: pass 1 walks the file once, tolerating a forward reference
  to an as-yet-undefined label (the one genuine two-pass problem this
  dialect has, e.g. `mov ds,sysdat` before `sysdat`'s own `DW` line) by
  pre-scanning the whole file first for every label/EQU's *kind and
  size* (not its address), so pass 1 can still classify and size such an
  operand correctly. Pass 1 leaves the symbol table fully resolved; pass
  2 reuses it unchanged and just re-emits bytes.
- `PUBLIC`/`EXTRN`/`GLOBAL` are accepted but produce no cross-module
  linking — this assembler only handles one `.A86` file (plus its
  `INCLUDE`s) at a time, matching how the files in `src/` are mostly
  self-contained per RSP/module.

## Bugs found and fixed in this assembler

- `ret`/`retf` with an immediate operand (`ret 4`, used by callee-cleans-up
  calling conventions) were silently assembled as plain `ret`/`retf`
  (opcode `C3`/`CB`, no operand) instead of `C2 imm16`/`CA imm16` — `ret`
  and `retf` were listed in `isa.py`'s no-operand opcode table, which was
  checked before the operand-aware encoding further down in the same
  `encode()` function, so the operand was silently dropped. Found while
  assembling `tools/plm/runtime_stub.A86`'s `ret 4` (the real MP/M-86
  `mon1`–`mon4` dispatch stub's own stack cleanup) — see
  `tools/plm/README.md`'s "`pl_mon1`–`pl_mon4` now have a real
  implementation" section.

The following were all found validating the kernel sources
(`src/kernel/`) — see `docs/bugs.md` for which kernel modules now
assemble end-to-end and which remain blocked by genuine 1981 source
defects rather than any of this:

- `INCLUDE` combined with `!` on the same line (DRI's own
  `eject ! include foo.def` convention, used throughout the kernel) was
  never recognized as an include at all — `flatten()` only checked
  whether a *whole raw line* started with `include`, before any `!`
  splitting happened. Fixed by splitting each line into its `!`-joined
  statements first and checking each one.
- `byte/word ptr` casts didn't recognize `dword ptr` (used for far
  pointers, e.g. `callf cs:dword ptr .supervisor`), raising a spurious
  "trailing tokens in expression: 'ptr'".
- Constant-expression evaluation (`EQU`, etc.) didn't understand a
  leading `byte/word/dword ptr` cast at all (`eval_text`, a separate
  code path from instruction-operand parsing) — needed both to just
  evaluate the result (`SYSDAT.FMT`'s `module_table equ dword ptr
  (offset $)`) and, now, to carry the cast's size onto the resulting
  value so a later `EQU`'d name still carries its declared size forward
  (`BDOSIF.BDO`'s `btab_flag equ byte ptr (btab_addr + word)`, later used
  as a sized `TEST cs:btab_flag[si],...`).
- Bare `BYTE`/`WORD`/`DWORD` (no `PTR`) are a real DRI idiom for literal
  size constants 1/2/4 inside field-offset arithmetic — e.g. `p_thread
  equ word ptr p_link + word` (`MDB.FMT`) or `mod_init equ mod_entry +
  dword` (`MODTAB.DEF`, sizing a far-pointer table entry). Previously
  treated as undefined-symbol references.
- A leading `.` is cosmetic on a bare *number* too, not just a symbol
  name — `mov .0,offset int_trap` / `mov .2,cs` (`XIOS.A86`, setting up
  interrupt vector 0 at absolute addresses 0/2) and the same idiom in
  `MAU.MEM`/`ML.MEM`/`SHARE.MEM`. Implemented as direct (absolute)
  memory addressing — the only reading consistent with `mul byte ptr
  .0` (`MAU.MEM`), since `MUL` has no immediate operand form on the
  8086 at all.
- The dot-is-cosmetic fallback itself had a bug: if the dotted name
  wasn't already a fully-defined symbol, the *tentative forward-
  reference* placeholder was still registered under the dotted key
  (`.xiosmod`) instead of the stripped one (`xiosmod`), so it could
  never resolve once the real, undotted definition was reached later in
  the same file.
- `SYMBOL + constant` (no brackets, e.g. `MOV BYTE PTR ARECORD+2,AH`,
  `FILE1.BDO`) lost track of which symbol it came from after the
  addition — `parse_add` built a fresh `Value` without carrying the
  `.symbol` field forward, so `parse_operand` could no longer tell the
  expression was label-derived and misclassified it as a plain
  immediate (illegal as a `MOV` destination) instead of a memory
  operand.
- `RS` (DRI's third "reserve" directive, alongside `RB`/`RW` — `DATA.BDO`'s
  `endlist rs 0` zero-size marker, `header rs 128`) wasn't recognized at
  all. Implemented identically to `RB` (reserves N raw bytes).
- `END` terminated the *entire* assembly the moment it was seen,
  including one reached via `INCLUDE` — but DRI's own fragment files
  routinely end with their own `END` (`FILE1.BDO`'s last line is `;...end
  bdos filesystem part 1` / `END`) while the including file
  (`BDOS.A86`) still has more `INCLUDE`s queued up after it. `END` is now
  scoped to the file it appears in: it stops that file's own
  contribution and returns to whichever file (or the top level) included
  it, rather than aborting the whole assembly.
- `DD` (define doubleword, alongside `DB`/`DW`) wasn't recognized at
  all — found validating `src/loader/LDMPM.A86`'s `mpm dd abs_zero`
  (a placeholder immediately overlaid by two `DW`s via `org offset
  mpm`). Implemented like `JMPF`/`CALLF`'s direct-label form elsewhere
  in this assembler: offset word from the operand's value, then a
  segment word always written as `0000` (see the existing "No real
  segmentation" design choice above). While adding it, found `RS`
  (added earlier for the kernel) was missing from the *real* label-size
  classification used when a label is actually defined, not just from
  the forward-reference prescan — fixed alongside it.
- `LENGTH name`, a real DRI ASM86 operator evaluating to the total byte
  length of a `DB`/`DW`/`DD`/`RB`/`RW`/`RS` declaration, wasn't
  implemented at all — found validating `src/loader/ROM.A86`'s `er0 rb
  length cer0` (and nine more like it), where `cer0` is a `DB` string
  constant declared elsewhere in the same file; confirmed against the
  file's own `;16`-style length comments next to each use. Implemented
  by tracking each labeled `DB`/`DW`/`DD`/`RB`/`RW`/`RS` statement's
  total emitted byte length (`Assembler.lengths`, keyed by label) and
  adding a `LENGTH` unary operator to the expression parser, parallel
  to the existing `OFFSET` handling.

## Known limitations

- Not every ASM86 directive is implemented (no `MACRO`, `STRUC`, `DUP`,
  `PROC`/`ENDP`, `ASSUME`, `EVEN` — none of these are used by the files
  currently in `src/`; if a file needs one, the assembler will raise a
  clear "unsupported" error rather than mis-assembling it).
- No listing file, no cross-reference output, no Intel-HEX output (only
  raw per-segment binaries).
- Output is not byte-for-byte identical to the original DRI toolchain.

## Validated so far

`python3 tools/asm86.py src/rsp/ECHO.A86` assembles cleanly end-to-end.
Testing the other RSPs surfaced **real bugs in the original 1981 source**
(not assembler bugs — confirmed by reading the surrounding source):

- `src/rsp/CLOCK.A86`: every `jmp clock_loop` (4 places) references a
  label that doesn't exist; the label actually defined is `clockloop:`.
- `src/rsp/TMP.A86`: `call print_string` (line 147) references a label
  that doesn't exist; the label actually defined is `printstring:`.
- `src/rsp/SUBMIT.A86`: `jmp not_builtin` references `notbuiltin:`, and
  `mov dx,offset sopen_err` references `sopenerr`, both similarly
  mismatched. `call conwrite` (in the `showerr:` routine) calls a
  routine that is never defined anywhere in the file at all.

These are consistent with this tree's own provenance note in the top
level `README.md` ("hand written labels... probably work disks") —
these RSPs likely never finished debugging on the original hardware
either. They have **not** been patched here; `src/` remains an unmodified
copy of `orig/`.

### Kernel sources (`src/kernel/`)

`src/kernel/CIO.A86`, `SYSDAT.A86`, `BDOS.A86`, `MEM.A86`, `RTM.A86`,
and `SUP.A86` all compile and assemble cleanly end-to-end now. Every
source-level name-mismatch bug in `BDOS.A86`/`RTM.A86` (45 + 6 names —
see `docs/bugs.md` bugs #4/#5) was **patched directly in `src/`** (at
the user's explicit request — this is the one place in this tree where
`src/` is no longer an unmodified copy of `orig/`); `MEM.A86` and
`SUP.A86` had no name-mismatch bugs of their own to begin with, only the
short-jump-range issue below. The short-jump-range trade-off that used
to additionally block all four of `BDOS.A86`/`MEM.A86`/`RTM.A86`/
`SUP.A86` (bug #6) is now handled by real jump-size relaxation (see
"Design choices that trade size for simplicity" above) — no source
patch needed for that part. `XIOS.A86` had its one fixable typo patched
too, but still has one genuinely unresolved symbol (`dp_base`) with no
fix available from evidence in this tree (bug #7).

### Loader sources (`src/loader/`)

All four of `src/loader/LDBDOS.A86`/`LDBIOS.A86`/`LDMPM.A86`/`ROM.A86`
compile and assemble cleanly end-to-end now. `LDBIOS.A86`'s four
name-mismatch bugs and `LDMPM.A86`/`ROM.A86`'s four more (bugs #8, #9,
#10 — what first looked like genuinely unresolved symbols in the latter
two turned out, on a closer second look, to be the same underscore-typo
bug class) were all patched; the short-jump-range issue that used to
additionally block some of these is fixed the same way as the kernel
files above (real relaxation, no source patch needed). `ROM.A86` also
needed the new `LENGTH` operator support described above.
