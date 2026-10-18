# plmc — a PL/M-86 subset compiler for this source tree

`tools/plm/` is a from-scratch PL/M-86 compiler written in Python. It
compiles the `.PLM` dialect subset used in this tree's `src/utils/` and
`src/toolchain/` to ASM86-dialect text, which `tools/asm86/` (see its own
README) then assembles to machine code. Like `asm86`, it does not aim for
historical byte-for-byte fidelity to DRI's real PL/M-86 compiler — only
functionally equivalent 8086 code — and it only handles one source file
(plus its `$INCLUDE`s) at a time: there is no linker, so a call to an
`EXTERNAL` *procedure* produces an unresolved symbol in the generated
assembly (see Limitations).

## Usage

```
python3 tools/plmc.py path/to/FILE.PLM [-I includedir ...] [-o out.A86]
python3 tools/asm86.py out.A86                 # then assemble it
```

`$INCLUDE(filename)` is resolved case-insensitively, first in the source
file's own directory, then in any `-I` directories given. Other `$`
compiler-control directives (`$TITLE`, `$COMPACT`, `$EJECT`, `$OPTIMIZE`,
`$DATE`, ...) are recognized and dropped — they only affect DRI's
compiler *listings*, never code generation.

## What it supports

- Comments (`/* ... */`, non-nesting), string/character literals
  (`'text'`, `''` as an escaped quote, allowed to span multiple physical
  lines as real PL/M-86 source does for long `LITERALLY` text).
- **Real `LITERALLY` macro expansion** (not just named constants): a
  `NAME LITERALLY 'text'` declaration is textually substituted wherever
  `NAME` is referenced afterward, and the substitution is applied to
  *every* token, not just identifiers — this tree's own `COMLIT.LIT`
  relies on exactly this to bootstrap `lit` as an abbreviation for the
  `LITERALLY` keyword itself (see Notable discoveries below). A
  `DECLARE`/`DCL` statement that turns out to consist entirely of
  `LITERALLY` definitions is dropped (it generates no code).
- `DECLARE`/`DCL` with `BYTE`/`ADDRESS`/`POINTER`/`WORD`/`ADDR` (an
  alias), arrays including `(*)` (size inferred from its `INITIAL` list),
  `STRUCTURE(...)`, `BASED` (on a plain pointer, `struct.field`, or an
  inline `STRUCTURE(...)` spec — field access works in every case),
  `AT(expr)` (storage aliased directly onto another global, a global
  array element, or a struct field — not indirection, a flat overlay),
  `EXTERNAL`, `PUBLIC`, `INITIAL(...)`/`DATA(...)` (numbers, strings,
  and address-of expressions, on scalars, arrays, and structures), and
  the grouped-name form `DECLARE (a, b, c) BYTE;` (each name may also
  carry its own `BASED ptr` override).
- `PROCEDURE` declarations with parameters, `BYTE`/`ADDRESS`/`POINTER`
  return types, `EXTERNAL`, `PUBLIC`, and **nested procedures** (a
  `PROCEDURE` declared inside another one — hoisted out and generated as
  an ordinary routine; see Limitations for what that doesn't cover).
- Statements: `IF`/`THEN`/`ELSE`, `DO`/`END`, `DO WHILE`, `DO CASE`,
  `DO var = start TO stop [BY step]`, `CALL`, `RETURN`, assignment
  (`a = expr;` and multi-target `a, b = expr;`), labeled blocks, and a
  bare top-level statement sequence under PL/M-86's classic `PLMSTART`
  entry-point convention (with or without a wrapping `DO; ... END;`).
- Expressions: the full operator set (`+ - * / MOD AND OR XOR NOT`,
  relational `= <> < <= > >=`), array indexing (`name(i)`), chained
  indexing/field access on a computed base (`arr(i).field(k)`), structure
  field access (`name.field`), the `.` *and* `@` address-of operators
  (interchangeable) including the `.( ... )` inline anonymous-array
  constant form, and an expression-embedded assignment using `:=`
  specifically to disambiguate it from `=` (equality) inside a condition
  — e.g. `if (ret := f(x)) = 20 then` — matching real PL/M-86 usage in
  this tree. `$` is an insignificant separator in numbers as well as
  identifiers (`0110$0000b`).
- Built-ins: `MOVE(count,src,dst)` (expands inline to a `REP MOVSB` loop,
  no runtime library needed), `LENGTH`/`LAST`/`SIZE` (resolved at compile
  time from the array's declared dimension/element size), `LOW`/`HIGH`,
  `SHL`/`SHR`/`ROL`/`ROR`, `SIGNED`/`DOUBLE` (both no-ops here — see
  Limitations), and the `CARRY` pseudo-variable (reads the 8086 carry
  flag as last set by the previous arithmetic; safe because this
  compiler never emits a flag-affecting instruction between statements
  other than the one that set it).

## Calling convention (defined by this compiler)

Not independently verified against a historical PL/M-86 ABI reference —
see the module docstring in `codegen.py` for the full rationale:
- Arguments are pushed right-to-left, so the first declared parameter is
  at `[bp+4]`, the second at `[bp+6]`, etc.
- Every parameter and local variable occupies a full 16-bit stack slot
  regardless of its declared type.
- The callee pops its own parameters (`RET imm16`).
- The return value (`BYTE` or `ADDRESS`/`POINTER`) comes back in `AX`.

## Notable discoveries made while building this

- **`COMLIT.LIT` bootstraps its own syntax sugar.** It defines `lit` as
  `LITERALLY` for `'literally'`, then immediately uses that abbreviation
  (`dcl lit 'declare'`, `true lit '0ffh'`, `forever lit 'while true'`,
  ...) later in the very same file. Handling this correctly required
  implementing macro expansion as a proper token-stream rewrite (see
  `expand_literally()` in `lexer.py`), not a parser-level constant table.
- **Statement assignment is `=`; expression-embedded assignment is
  `:=`.** Real PL/M-86 syntax uses plain `=` for an assignment
  *statement* (`a = b + c;`) but requires `:=` when an assignment is
  embedded inside a larger expression, since bare `=` there would be
  read as equality (`if (ret := f(x)) = 20 then`, seen verbatim in
  `ATTACH.PLM`).
- **`BASED` can be typed with an inline, macro-expanded
  `STRUCTURE(...)`.** `PROCES.LIT` defines a structure template split
  across two `LITERALLY` strings (`pd$hdr` then `pd$structure`, the
  second one referencing the first) specifically so it reassembles into
  one `STRUCTURE(...)` spec at the point of use — here, directly inside
  a `BASED` declaration. Field access on such a variable works.
- Fixing the above surfaced a real gap in `tools/asm86`: `OFFSET SYMBOL`
  as a plain instruction operand (not inside a `DW`) was being
  misclassified as a memory dereference instead of an immediate address,
  and `[bp-2]`-style frame-relative addressing (displacement *inside*
  the brackets) wasn't supported at all — only this tree's own
  `SYMBOL[register]` convention was. Both are now fixed in `asm86` itself
  (see its README's changelog-style notes), since this compiler's
  generated code depends on both.
- **`AT(...)` takes any address expression, not just `@plain_name`.**
  `SHOW.PLM` overlays a field onto a byte offset *inside* another array
  (`rrec address at(rreca)`, where `rreca` is a macro expanding to
  `.fcb(7dh-5ch)`), and `ASS86.PLM` uses address arithmetic directly
  (`at(.assem$ptr+2)`). Resolving this needed a general compile-time
  constant-folding evaluator (`const_eval` now handles `BinOp`/`UnOp`,
  not just bare numbers), and `AT`-aliased globals carry an explicit
  byte offset into the target's storage (`label_offset` on the `Sym`) —
  loads/stores through a nonzero offset compute the address into a
  register rather than trying to fold it into the bare-symbol text
  operand asm86 expects (which would silently misclassify it; see the
  comment on `load_from`/`store_to` in `codegen.py`).
- **A declaration's `INITIAL`/`AT(...)` can forward-reference another
  global declared later in the same file.** `STSRSP.PLM`'s `sts$qd`
  initializes a field with the address of `sts$qd$buf`, declared right
  after it. Globals are now processed in three passes (register plain
  globals → register `AT`-aliased globals → emit every global's actual
  data), mirroring the two-pass forward-reference handling already built
  for `tools/asm86`.
- **Nested procedures are a real, used feature** (a `PROCEDURE`
  declared inside another one, e.g. `ED.PLM`'s `ZN`/`MOVEUP`/`CTRAN`/
  `SETSCR`, `ERA.PLM`'s `file$err` inside `single$file`). They're hoisted
  out of the enclosing body and generated as ordinary top-level routines
  — which works as long as the nested procedure is self-contained; see
  Limitations for the real gap this doesn't cover.
- The PL/M-86 statement-level `=`/expression `:=` split (above) also
  applies to the `DO var = start TO stop` loop form, which several files
  write with `=`, not `:=`.

## Known limitations

- **Nested procedures can't access the enclosing procedure's locals**
  (no real lexical closure/display-pointer support — each hoisted
  procedure gets its own empty local scope under the single global
  scope). `ERA.PLM`'s `file$err` (nested in `single$file`) reading
  `single$file`'s own `fcba`/`code` locals is exactly this gap, and is
  the reason `ERA.PLM`/`REN.PLM` don't compile yet. A nested procedure
  that only uses its own parameters/locals works fine.
- `GOTO` and `LABEL`-typed declarations are parsed and dropped — no
  control-flow support for them (`ABORT.PLM`'s own `PLMSTART` label
  works regardless, since that specific pattern is recognized
  structurally — see the "bare top-level statement sequence" item
  above).
- `DO CASE` generates a jump table but hasn't been exercised against a
  real file yet.
- `SIGNED` and `DOUBLE` are accepted but are no-ops (every value is
  already a full word in `AX`, and this compiler's relational operators
  are always unsigned) — code that depends on them actually changing
  signedness/width will compile but may not behave identically.
- A call to an `EXTERNAL` *procedure* compiles to a `CALL` on an
  unresolved symbol — there is no linker, so this is as far as a single
  translation unit can go. `EXTERNAL` *data* is, by contrast, still
  given storage in this module (so the output stays assemblable), which
  means it won't actually alias the real external module's copy.
- No `INTERRUPT`/`REENTRANT` semantics, no local `INITIAL`.
- A structure field declared as an array (e.g. `pname(8) byte`) isn't
  validated against the length of a string given for it in `INITIAL` —
  a mismatch just lays out whatever length the string actually is,
  silently shifting every later field's address. (The code that used to
  catch this was removed when `INITIAL` rendering was reworked to defer
  address-of values to assembly time — see the commit history in this
  file's "Notable discoveries" for why.)

## Validated so far

Passing end-to-end (compiles *and* the output assembles through
`tools/asm86` with zero errors other than the expected unresolved
external monitor calls — `pl_mon1`/`pl_mon2`/`pl_mon3` — see
Limitations above): `ATTACH.PLM`, `ABORT.PLM`, `CONS.PLM`, `DIR.PLM`,
`DSKRST.PLM`, `SPOOL.PLM`, `SSPOOL.PLM`, `TYPE.PLM` — 8 of the 20 `.PLM`
files currently in `src/utils/` and `src/toolchain/`.

Still failing, with the specific gap identified for each (none of these
have been patched, since the point is to know what's missing, not to
paper over it):

| File | Blocked on |
|---|---|
| `ERA.PLM`, `REN.PLM` | nested-procedure closure access to the enclosing proc's locals (see Limitations) |
| `ED.PLM` | an undefined identifier `MEMORY` used as if it were a declared array — not yet root-caused (may be a genuine source bug like the ones in `docs/bugs.md`, or an include this tree is still missing) |
| `ERAQ.PLM` | a field-access base that isn't a plain variable name (`dir$entries(j).file(k)` works; something adjacent to it doesn't) |
| `PIP.PLM` | a `PLUS` token where `)` was expected — not yet root-caused |
| `SET.PLM` | calls an undeclared procedure `MOVW` — likely a genuine source bug, not yet confirmed |
| `SHOW.PLM`, `TOD.PLM` | undefined identifiers (`zerosup`, `movb`) — not yet root-caused |
| `STSCMD.PLM`, `STSCOM.PLM`, `STSRSP.PLM` | undefined identifier `unsign` (presumably a builtin, not yet implemented) |
| `ASS86.PLM` | a structure's `INITIAL` list supplying more values than it has fields (one field is itself an array needing several INITIAL values — the per-field-array-length case `_build_struct_fields`/`_emit_initial_values` doesn't handle) |
| `DIS86.PLM`, `INS86.PLM` | `$INCLUDE` targets (`OPTAB.DAT`/`OPTAB.LIT`) not yet copied into `src/`, and (once copied) a `(*)`-sized array mixing multi-character string and plain numeric `INITIAL` entries that hasn't been tried yet |
