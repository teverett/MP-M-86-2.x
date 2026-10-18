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
  disp8 form), and **`JMP`/`CALL` always use the full near form**
  (`E9`/`E8`) unless the source explicitly says `JMPS` (forces the
  2-byte short `EB` form). This means an instruction's length never
  depends on a forward reference's eventual value, so the assembler
  never has to do iterative jump-size relaxation — at the cost of
  slightly larger code than the original DRI output would have produced.
  Conditional jumps/`LOOP`/`JCXZ` are still hardware-limited to an 8-bit
  displacement (true 8086 behavior); if one of those is pushed out of
  range by the size trade-off above, the assembler reports it rather
  than silently producing wrong code.
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
