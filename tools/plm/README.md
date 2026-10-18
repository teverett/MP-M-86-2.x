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
  alias)/`INTEGER` (PL/M-86's real signed-16-bit type, stored the same
  as `ADDRESS` since this compiler never distinguishes signed from
  unsigned bit patterns — see `UNSIGN` below), arrays including `(*)`
  (size inferred from its `INITIAL` list),
  `STRUCTURE(...)`, `BASED` (on a plain pointer, `struct.field`, or an
  inline `STRUCTURE(...)` spec — field access works in every case),
  `AT(expr)` (storage aliased directly onto another global, a global
  array element, or a struct field — not indirection, a flat overlay),
  `EXTERNAL`, `PUBLIC`, `INITIAL(...)`/`DATA(...)` (numbers, strings,
  and address-of expressions, on scalars, arrays, and structures), and
  the grouped-name form `DECLARE (a, b, c) BYTE;` (each name may also
  carry its own `BASED ptr` override). A *local* array (declared inside
  a procedure) gets its own stack storage, indexed the same way as a
  global one; one with `INITIAL`/`DATA` (including a `(*)`-sized one)
  has its values copied onto the stack by code generated right at the
  `DECLARE` statement's own position, since a local has no "initial
  contents" the way a global's data-segment storage does — it has to be
  written there every time that point in the procedure actually runs.
  If any entry is the address of *another local* (its address is only
  known at runtime, stack-relative, so it can't be baked into a static
  table the usual way), every entry is instead stored individually with
  its own instructions rather than copied in bulk — see
  `_addrof_is_local` in `codegen.py`.
- `PROCEDURE` declarations with parameters, `BYTE`/`ADDRESS`/`POINTER`
  return types, `EXTERNAL`, `PUBLIC`, and **nested procedures with real
  closure access** (a `PROCEDURE` declared inside another one, reading or
  writing the enclosing procedure's own parameters/locals — see "Nested
  procedures are real closures" below for how).
- Statements: `IF`/`THEN`/`ELSE`, `DO`/`END`, `DO WHILE`, `DO CASE`,
  `DO var = start TO stop [BY step]`, `CALL`, `RETURN`, assignment
  (`a = expr;` and multi-target `a, b = expr;`), labeled blocks, and a
  bare top-level statement sequence under PL/M-86's classic `PLMSTART`
  entry-point convention (with or without a wrapping `DO; ... END;`).
- Expressions: the full operator set (`+ - * / MOD AND OR XOR NOT`, plus
  the word-form `PLUS` as a synonym for `+` — evidence-backed but seen
  only once, see "Notable discoveries" below), relational
  `= <> < <= > >=`, array indexing (`name(i)`), chained indexing/field
  access on a computed base (`arr(i).field(k)`, `struct.field(i)`, and
  combinations of those), structure field access (`name.field`), the
  `.` *and* `@` address-of operators (interchangeable) including the
  `.( ... )` inline anonymous-array constant form, and an
  expression-embedded assignment using `:=` specifically to disambiguate
  it from `=` (equality) inside a condition — e.g.
  `if (ret := f(x)) = 20 then` — matching real PL/M-86 usage in this
  tree. `$` is an insignificant separator in numbers as well as
  identifiers (`0110$0000b`).
- Built-ins, all confirmed against Intel's real PL/M-80 language
  reference (`orig/plm80s/PLMLANG.DOC` — PL/M-86 is a direct descendant
  with the same core standard library, see "Notable discoveries"
  below): `LENGTH`/`LAST` (resolved at compile time from the array's
  declared dimension), `LOW`/`HIGH`, `DOUBLE` (a no-op here — every
  loaded `BYTE` is already zero-extended in `AX`), `SHL`/`SHR`/`ROL`/
  `ROR` and their rotate-through-carry siblings `SCL`/`SCR` (`RCL`/
  `RCR`, as opposed to `ROL`/`ROR`'s plain, non-carry rotate),
  `DEC(expr1 + expr2)` (decimal-adjust via the 8086 `DAA` instruction,
  used right after a BCD addition), and `CALL TIME(n)` ("delays 100
  microseconds times n" on the original i8080 — not something this
  8086-targeting compiler can replicate in real wall-clock terms, see
  Limitations, so it's a busy-wait loop merely scaled by `n`).
  `SIGNED`/`UNSIGN` (PL/M-86's `INTEGER`↔`ADDRESS` conversions) and
  `MOVW`/`MOVB` are not in PL/M-80's standard list (`INTEGER` and
  `ADDRESS` are themselves 8086-only extensions, and `MOVW`/`MOVB` look
  more like this codebase's own helpers) — still implemented, on their
  own evidence, see "Notable discoveries".
- The **pre-defined variables** `CARRY`, `ZERO`, `SIGN`, `PARITY`
  (each reads the matching 8086 flag as last set by the previous
  arithmetic; safe because this compiler never emits a flag-affecting
  instruction between statements other than the one that set it),
  `MEMORY` (an implicit `BYTE` array starting wherever this module's
  own static data ends — i.e. whatever memory is still free at
  runtime; `.MEMORY` gives its address, `MEMORY(i)` indexes into it as
  a buffer of unbounded size), and `STACKPTR` (reads — and can be
  assigned to directly set — the live `SP` register). See "Notable
  discoveries" below for how `MEMORY` specifically was confirmed to be a
  builtin rather than a missing declaration.

## Calling convention (defined by this compiler)

Not independently verified against a historical PL/M-86 ABI reference —
see the module docstring in `codegen.py` for the full rationale:
- Arguments are pushed right-to-left, so the first declared parameter is
  at `[bp+4]`, the second at `[bp+6]`, etc.
- Every parameter and local variable occupies a full 16-bit stack slot
  regardless of its declared type.
- The callee pops its own parameters (`RET imm16`).
- The return value (`BYTE` or `ADDRESS`/`POINTER`) comes back in `AX`.

### Nested procedures are real closures

A `PROCEDURE` declared inside another one can read and write the
enclosing procedure's own parameters and locals — PL/M-86 nested
procedures are true lexical closures, not just a namespacing
convenience, and this tree actually relies on that (see `ERA.PLM`'s
`file$err`/`single$file` above). There's no runtime support library to
lean on, so this is implemented the classic way for a stack-machine
compiler, a **static link**:

- A nested procedure takes one extra, hidden, last argument: a pointer
  to its immediately enclosing procedure's stack frame (its `BP` at the
  time of the call). It's pushed *after* the real arguments (which are
  already pushed right-to-left), so it lands at `[bp+4]` in the callee
  and the real parameters shift up to start at `[bp+6]` — see
  `LINK_OFFSET`/`gen_proc` in `codegen.py`.
- At the call site, the value pushed as the static link is whichever
  frame is currently reachable that corresponds to the callee's
  enclosing procedure: the caller's own `BP` directly, if the caller
  *is* that enclosing procedure, or — if the caller is itself a nested
  procedure some number of levels down from it — the caller's own
  static link, chased however many times is needed first. (See
  `_frame_reg_for_scope`.)
- A reference to an enclosing procedure's local/parameter is compiled
  the same way: instead of indexing directly off this procedure's own
  `BP`, it's reached by chasing that same chain of static links first,
  then indexing off wherever that lands. Each procedure's local
  variables still live in one flat (non-block-scoped) scope, same as
  everywhere else in this compiler — only the *frame* differs when the
  variable being referenced belongs to an outer procedure.
- Nested-inside-nested procedures work the same way, with more static
  links to chase; the depth is computed automatically by walking the
  lexical scope chain at compile time (`Scope.is_proc_scope`), not
  hardcoded to one level.

This is **not independently verified against a historical PL/M-86 ABI
reference** either — DRI's own compiler may have implemented nested
procedures differently (a different static-link convention, or a
"display" of frame pointers instead) — but it's internally consistent,
and it's what makes `ERA.PLM`/`REN.PLM` (both: nested procedure reading
an outer local) compile and assemble correctly end to end.

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
- **Nested procedures are a real, used feature — and are real
  closures.** `ERA.PLM`'s `file$err`, nested inside `single$file`, reads
  and writes `single$file`'s own locals `fcba`/`code` directly (not just
  calling it as a namespaced helper). `ED.PLM`'s `ZN`/`MOVEUP`/`CTRAN`/
  `SETSCR` are nested the same way. Supporting this needed a real
  implementation, not just hoisting the procedure out as if it were
  self-contained (see "Nested procedures are real closures" below).
- The PL/M-86 statement-level `=`/expression `:=` split (above) also
  applies to the `DO var = start TO stop` loop form, which several files
  write with `=`, not `:=`.
- **`MEMORY` is a real compiler builtin, not a missing declaration.**
  `ED.PLM` references `MEMORY` dozens of times (`.MEMORY`, `MEMORY(i)`,
  both read and assigned) without ever declaring it — looked, at first,
  exactly like the undeclared-procedure bugs in `docs/bugs.md`. But
  `PIP.PLM` (`DBUFF(1024) BYTE AT (.MEMORY)`, `SBASE = .MEMORY;`) and
  `MAINP.PLM` (`(FREEPT-.MEMORY) / ...`) use the identical undeclared
  `MEMORY` the same way, in otherwise-unrelated files — confirming it's
  a real PL/M-86 compiler-predeclared identifier for "the rest of free
  memory," not a shared bug. (At the time this was written, `TIME` in
  `ED.PLM` looked like the opposite case — a genuine bug, appearing
  exactly once with no cross-reference anywhere to confirm it — but it
  too turned out to be a real, documented builtin; see below.)
  Implemented as an ordinary global whose label is emitted as the very
  last thing in the generated `dseg`, after every other global and every
  string constant interned while compiling procedure bodies (see
  `to_asm_text`), so its address is genuinely "the end of everything
  this module statically allocated."
- **`SET.PLM`'s `MOVW` is the same kind of thing, on weaker evidence.**
  `MOVW(@UDA.DF$PASSWORD,@UDA$PARENT.DF$PASSWORD,4)` is the only call to
  `MOVW` anywhere in the tree — no second file to cross-reference against
  the way `MEMORY` had. But `DF$PASSWORD` is declared in `UDA.LIT` as
  exactly `(8) BYTE`, and the call passes a count of `4`: 4 *words* is
  8 bytes, exactly the field size, which only lines up if `MOVW` moves by
  words with `(source, dest, count)` argument order — the reverse of
  `MOVE`'s confirmed `(count, source, dest)`. That's enough to implement
  it as a builtin (`REP MOVSW`, see `gen_movw_builtin`) rather than leave
  it flagged as a bug in `docs/bugs.md`, though it's fair to call this
  one evidence-backed rather than fully confirmed.
- **`TOD.PLM`'s `MOVB` confirms the `MOVW` pattern.** It's `MOVW`'s
  byte-granularity sibling: `MOVB(@EXTRNL$TOD.DATE,@LCLTOD.DATE,5)`
  copies a TOD structure's `DATE` (`ADDRESS`) plus `HRS`/`MIN`/`SEC`
  (three `BYTE`s) — 2+1+1+1 = exactly 5 bytes — with a byte count of 5,
  confirming byte (not word) granularity, same `(source, dest, count)`
  order as `MOVW`. One of its two call sites even comments
  `/* use pl/m-86 move */`, suggesting whoever wrote this considered it
  interchangeable with the confirmed `MOVE` builtin, just following
  their own `MOVW`/`MOVB` naming and argument-order convention instead.
  (`orig/plm80s/PLMLANG.DOC`, added to this tree later, doesn't document
  `MOVW`/`MOVB` at all — they're not PL/M-80 standard, so they stay
  evidence-backed rather than independently confirmed, unlike `TIME`/
  `DEC`/`PLUS`/`MEMORY`/`CARRY` below.)
- **`ED.PLM`'s `TIME` was a real builtin all along — `orig/plm80s/
  PLMLANG.DOC` settled it.** Once that doc turned up, it documents
  `CALL TIME(expression)` verbatim: "the expression is evaluated as a
  BYTE value. The TIME procedure delays 100 microseconds times the
  value." ED.PLM's one use is a 20-iteration loop each calling
  `TIME(250)`, commented "1/2 SECOND TIME OUT": `250 * 100us * 20 =
  500ms`, an exact match confirming it beyond reasonable doubt. The
  same doc also confirmed `ZERO`/`SIGN`/`PARITY` (the same
  flag-pseudo-variable pattern as the already-implemented `CARRY`),
  `STACKPTR` (reads, and can be assigned to directly set, the live `SP`
  register), and `SCL`/`SCR` (rotate-through-carry, `RCL`/`RCR`, as
  opposed to `ROL`/`ROR`'s plain rotate) — none of these had been seen
  in any file yet, but since they're real and the implementation is a
  direct extension of the `CARRY`/`ROL`/`ROR` patterns already in place,
  all five were added alongside `TIME`. Doing this exposed one real
  bug: the blanket `if low == 'time': ...` check in `gen_call` shadowed
  `SET.PLM`'s own, wholly unrelated, user-declared `time: procedure;`
  (a parameterless helper, nothing to do with PL/M's delay builtin) —
  fixed by checking for a real user declaration first and preferring it
  over any builtin name (`_user_proc` in `codegen.py`), which generalizes
  to every builtin, not just `TIME`.
- **Local arrays were never actually implemented — only global ones.**
  `SHOW.PLM`'s `userstatus` procedure declares `user(15) BYTE` and
  `ufcb(*) BYTE DATA (...)` as plain locals and indexes/`LAST`s them
  normally. The original local-variable codegen allocated exactly one
  stack slot per `DECLARE`, silently ignoring any array dimension —
  `user(15)` would compile to the same single 2-byte slot as a bare
  `user BYTE`, with every `user(i)` access reading/writing adjacent
  stack memory instead of a real array. Fixed by making `alloc_local`
  reserve as many words as the declared size needs, and by generating,
  for a local with `INITIAL`/`DATA`, an inline `REP MOVSB` copy from an
  interned data-segment constant onto the stack *at the `DECLARE`
  statement's own position* every time it's reached (a local has no
  "initial contents" any other way — see `gen_VarDecl`/`_declare_var`).
- **`LITERALLY` is scoped like any other declaration — not file-global
  forever.** `PIP.PLM` declares `M LITERALLY '20'` inside one nested
  `DO` block, then reuses the plain name `M` as an ordinary `BYTE` local
  in a completely different, unrelated procedure later in the file. The
  original `expand_literally` treated every macro definition as
  permanently global for the rest of the file once seen, so that later,
  unrelated `M` got silently (and wrongly) substituted with the number
  `20` before the parser ever saw it as a declaration name. Fixed by
  tracking a stack of macro scopes, pushed on every `DO`/`PROCEDURE` and
  popped on its matching `END`, matching real PL/M block scoping.
- **`PIP.PLM`'s `C2 PLUS 0` and `DEC(x)` turned out to be fully
  documented PL/M-80 standard syntax, not guesses after all.** At the
  time these were implemented, nothing in this tree declared `PLUS`
  anywhere, and PIP.PLM has no `$INCLUDE`s at all, so the reasoning was
  "word operator, same idea as `AND`/`OR`/`MOD`, and harmless if wrong."
  Once `orig/plm80s/PLMLANG.DOC` (Intel's own PL/M-80 reference, added
  to this tree later) turned up, it documents, verbatim:
  `DEC(expr1 + expr2) and DEC(expr1 PLUS expr2)` — "does the necessary
  decimal adjustment to produce the BCD result from the addition,"
  exactly the 8086 `DAA` idiom this compiler already implemented, and
  `PLUS` was never a loose guess at all — it's `DEC`'s own documented
  companion syntax, not a general word-form `+`. Both match PIP.PLM's
  usage (`C1 = DEC(C1+ONE); C2 = DEC(C2 PLUS 0)`, `C1`/`C2`/`C3` being
  packed-BCD digit counters that `PRINTDIG` prints as two nibbles) with
  no changes needed to either implementation.
- **Field access on an array element was never actually implemented.**
  `ERAQ.PLM`'s `dir$entries(j).file(k)` requires `gen_field_addr` to
  resolve a field on `dir$entries(j)` (an `ast.Call`, one element of an
  array of structures) rather than a plain variable — the code only ever
  checked for `ast.Name`, despite this file's own earlier "Notable
  discoveries" entry claiming the construct worked (it had only tested
  the outer index, not this inner field access). Fixed since both
  `ast.Name` and `ast.Call` expose the same `.name` to resolve, and
  `gen_addr` already knew how to compute either one's address.
- **`STSCOM.PLM`'s `UNSIGN` is a real PL/M-86 standard function, and
  confirms `INTEGER` is a real type.** `dcl i integer; ... do i = 0 to
  signed(sd.nflags - 1); ... low(unsign(i))` only makes sense if
  `INTEGER` is PL/M-86's signed 16-bit type (as opposed to `WORD`/
  `ADDRESS`, unsigned) and `SIGNED`/`UNSIGN` are the conversion
  functions between them — unlike `MOVW`/`MOVB`/`PLUS`/`DEC`, this one
  isn't really a guess, it's just a part of the standard library this
  tree hadn't happened to exercise yet. Added `INTEGER` as a declared
  type (stored the same as `ADDRESS`, 2 bytes) and `UNSIGN` alongside
  the already-implemented `SIGNED`/`DOUBLE` as a third no-op — this
  compiler never distinguishes signed from unsigned bit patterns
  internally, so none of the three conversions have any bits to change.
- **A structure's `INITIAL` list needed real per-field matching, not a
  flat count.** `ASS86.PLM` declares `type$7$struc (2) structure
  (op$val (2) byte) initial (72,1,64,0)` — an *array of 2 structures*,
  each with one field that's *itself* a 2-byte array, needing
  `2 (elements) x 2 (bytes/element) = 4` values; comparing against the
  raw field count (1) rejected it outright. Fixing that by just
  multiplying field-count by array-size broke three already-passing
  files in turn: `ABORT.PLM`'s `pname (8) byte` field is initialized by
  a single `'        '` *string* covering the whole 8-byte array at
  once, not 8 separate values — a real, and more common, PL/M idiom.
  `_match_struct_initial` now walks the field list against the actual
  `INITIAL` entries one at a time, consuming exactly one entry for an
  array field when that entry happens to be a string, or one entry per
  element otherwise — handling both idioms, and combinations of them,
  correctly.

## Known limitations

- `GOTO` and `LABEL`-typed declarations are parsed and dropped — no
  control-flow support for them (`ABORT.PLM`'s own `PLMSTART` label
  works regardless, since that specific pattern is recognized
  structurally — see the "bare top-level statement sequence" item
  above).
- `DO CASE` generates a jump table but hasn't been exercised against a
  real file yet.
- `SIGNED`/`DOUBLE`/`UNSIGN` are accepted but are no-ops (every value is
  already a full word in `AX`, and this compiler's relational operators
  are always unsigned) — code that depends on them actually changing
  signedness/width will compile but may not behave identically.
- `CALL TIME(n)` is a busy-wait loop merely *scaled* by `n`, not
  calibrated to the documented "100 microseconds times n" — that exact
  i8080-cycle-based timing has no meaningful equivalent on this
  compiler's 8086 target (different CPU, and the real duration would
  depend on the host's actual clock speed regardless). Code that
  depends on `TIME`'s delay being any particular real-world duration
  won't get it; code that just needs "pause for roughly proportionally
  longer with a bigger argument" will.
- A call to an `EXTERNAL` *procedure* compiles to a `CALL` on an
  unresolved symbol — there is no linker, so this is as far as a single
  translation unit can go. `EXTERNAL` *data* is, by contrast, still
  given storage in this module (so the output stays assemblable), which
  means it won't actually alias the real external module's copy.
- No `INTERRUPT`/`REENTRANT` semantics.
- A structure field declared as an array (e.g. `pname(8) byte`) isn't
  validated against the length of a string given for it in `INITIAL` —
  a mismatch just lays out whatever length the string actually is,
  silently shifting every later field's address. (The code that used to
  catch this was removed when `INITIAL` rendering was reworked to defer
  address-of values to assembly time — see the commit history in this
  file's "Notable discoveries" for why.)

## Validated so far

**All 22 of the `.PLM` files currently in `src/utils/` and
`src/toolchain/` compile *and* their output assembles through
`tools/asm86` end-to-end**, with zero errors other than the expected
unresolved external monitor/debugger calls every file depends on but
none can supply a body for (`pl_mon1`–`pl_mon4`, and in `ASS86.PLM`'s
case a handful of `DDT86` entry points — see Limitations above).

A few needed real compiler work along the way, not just missing
include files: `ERA.PLM`/`REN.PLM` needed nested procedures to get real
closure support (each has a nested procedure reading its enclosing
procedure's own locals); `SET.PLM` needed the `MOVW` builtin (and,
once `TIME` was added for `ED.PLM`, exposed the builtin/
user-declaration shadowing bug fixed alongside it); `SHOW.PLM` needed
real local-array support; `TOD.PLM` needed the `MOVB` builtin;
`ERAQ.PLM` needed real field-access-on-an-array-element support;
`PIP.PLM` needed the `LITERALLY` scoping fix and `AT(...)` support for
a chained field-then-index target and for an address-of-a-local entry
in another local's `INITIAL` list; `STSCMD.PLM`/`STSCOM.PLM`/
`STSRSP.PLM` (all three built from the same shared `STSCOM.PLM` body)
needed `INTEGER` and `UNSIGN`; `ED.PLM` needed the `TIME` builtin;
`ASS86.PLM` needed real per-field `INITIAL` matching for a structure
field that's itself an array. `DIS86.PLM`/`INS86.PLM`, by contrast,
needed nothing more than copying in their missing `$INCLUDE` targets
(`OPTAB.DAT`/`OPTAB.LIT` — both plain PL/M text despite the
misleading-looking extensions, see `docs/filetypes.md`) — the `(*)`
array mixing multi-character strings and plain numbers that OPTAB.DAT
turned out to contain was already handled correctly by the local-array
and `INITIAL`-rendering work done for earlier files, with no further
changes needed.
