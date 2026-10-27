# MP/M-86 2.x Original Source (`orig/`)

This repository contains the historical source distribution of **MP/M-86 II**
(version 2.0), Digital Research's multi-user, multi-tasking operating system
for the Intel 8086/8088, as released circa 1981. The tree lives at
`orig/mpm862sr/` and is organized as eleven numbered directories (`01`–`11`),
matching the original distribution disk layout. There is no top-level
README; the notes below are reconstructed from file names, extensions, and
in-source comments.

## Contents

- [Provenance](#provenance)
- [Toolchain and languages](#toolchain-and-languages)
- [Directory map](#directory-map)
- [Build process (per `07/MPM86GEN.TEX`)](#build-process-per-07mpm86gentex)
- [Reference material (`orig/newplm/`, `orig/plm80s/`, `orig/asm80/`)](#reference-material-orignewplm-origplm80s-origasm80)
- [Related DRI source: MP/M II (`orig/mpm_II/`)](#related-dri-source-mpm-ii-origmpm_ii)
- [Related DRI source: CP/NET (`orig/cpn86src/`, `orig/cpnet80s/`)](#related-dri-source-cpnet-origcpn86src-origcpnet80s)
- [Related DRI source: factory-labeled disk images (`orig/driorig/`)](#related-dri-source-factory-labeled-disk-images-origdriorig)
- [Related DRI source: in-house disk images (`orig/lastdri/`)](#related-dri-source-in-house-disk-images-origlastdri)
- [Related third-party source: CP/M TCP/IP (`orig/cpm-tcp/`)](#related-third-party-source-cpm-tcpip-origcpm-tcp)
- [Reference manuals (`orig/manuals/`)](#reference-manuals-origmanuals)
- [Notes for anyone working in this tree](#notes-for-anyone-working-in-this-tree)

## Provenance

- Copyright strings found in-source: *"M P / M - 8 6 I I, Copyright (c) 1981,
  Digital Research, P.O. Box 579, Pacific Grove, California 93950"*
  (`01/CPYRIGHT.DEF`, `02/CPYRIGHT.DEF`).
- `08/PIP.PLM` and `08/ED.PLM` carry revision histories crediting
  **Thomas Rolander** (MP/M 1.1, 1980) and **Ray Pedrizetti** (MP/M-86 2.0,
  Oct 1981).
- `04/MPMLDR.CMD` embeds the string *"MP/M-86 Loader V2.0 (9/21/81)"*.
- `11/GLOBAL.EXT` has a compiler-development log by **R. Silberstein**
  (modifications dated 3/28/81 through 9/2/81).
- `09/VERS.LIT` defines the version/function-call constants the BDOS uses to
  report itself (`bdos20`, `bdos30`, `mpm`, `mpm86`).

Only two plain-text documentation files exist in the whole tree:
`07/MPM86GEN.TEX` (describes the build process) and `11/BNF.TEX` (a BNF
grammar for PL/M-86 expressions). `.TEX` here just means "text," not LaTeX.

## Toolchain and languages

The source is written in two languages of the era:

- **ASM86/A86** — Intel 8086 assembly (`.A86` source, `.H86` headers,
  `.X86` linkage includes). Used for the kernel, drivers, and loader.
- **PL/M-86** — Digital Research's systems language (`.PLM` source, `.LIT`
  literal/constant includes, `.EXT` external-declaration headers). Used for
  the compiler itself and most command-line utilities.

Build artifacts use extensions that encode pipeline stage rather than
content type alone:
- `.DEF` / `.LIB` / `.FMT` — shared constant, structure, and binary-layout
  definitions, `#include`-style.
- `.MPM` — an assembled kernel module renamed for linking into `MPM.SYS`
  (per the stage convention documented in `07/MPM86GEN.TEX`).
- `.SUB` — Submit (CP/M/MP/M batch) build scripts.
- `.CMD` — either a built, loadable command binary **or** a plain-text
  Submit-style build script; the extension alone does not distinguish these
  — check file contents (e.g. `04/MPMLDR.CMD` is a binary loader, while
  `09/MPMSTAT.CPM` is a text build script despite the different-looking
  `.CPM` extension).
- `.CPM` — similarly mixed: some are compiled tool binaries
  (`01/A86.CPM`, `03/GENCMD.CPM`, `03/VAX.CPM`, `03/XREF86.CPM`,
  `06/GENTAB.CPM`), others are plain-text build scripts.

DRI's convention of giving each module/utility directory its own private
copy of shared includes (rather than one central include directory) means
the same filename (e.g. `CPYRIGHT.DEF`, `COPYRT.LIT`, `COMLIT.LIT`,
`DATA.BDO`, `PD.DEF`, `QD.DEF`) recurs across multiple directories with
identical or near-identical contents.

## Directory map

| Dir | Contents |
|-----|----------|
| **01** | **BDOS** (Basic Disk Operating System / file system) core: `BDOS.A86`, the split `BDOSIF.BDO`/`DATA.BDO`/`EQU.BDO`/`FILE1-4.BDO` source fragments, shared `.DEF` includes, `MKBDOS.SUB` build script. Also bundles a copy of the `A86.CPM` assembler binary for local reassembly. |
| **02** | Main kernel build directory: **RTM** (Real-Time Monitor/scheduler: `DSPTCH.RTM`, `PROC.RTM`, `QUE1/2.RTM`, `FLAG.RTM`, `SYSENT.RTM`, …), **SUP** (supervisor / CLI / syscall dispatch: `COMMAND.SUP`, `PARSE.SUP`, `SYSFUNC.SUP`, …), **MEM** (memory manager: `MEMORY.MEM`, `MAU.MEM`, `CPMMEM.MEM`, …), and **CIO** (character I/O: `CIOIF.CIO`). Each module has an `.A86` linkage stub and an assembled `.MPM` output. Largest, most heterogeneous directory — the central kernel assembly point. |
| **03** | Mixed toolchain/glue: the `A86.CPM` cross-assembler, `XREF86.CPM` cross-reference utility, `VAX.CPM` (a VAX RJE/login/file-transfer utility for cross-development, *not* part of MP/M-86 itself), console/char I/O drivers (`CHARIO.CIO`, `CONSOLE.CIO`), and `IDLE.SUP` (the idle process). |
| **04** | **XIOS** (Extended BIOS — hardware-dependent I/O/boot layer: `XIOS.A86`) and the **MPMLDR** system loader that combines the separately-assembled kernel modules into `MPM.SYS`. Includes the `LBDOS*`/`LDBIOS`/`LDMPM`/`LDBDOS` link-support modules and disk deblocking routines (`DEBLOCK.LIB`). `MPMLDR.CMD` is the built, binary boot loader (v2.0, 9/21/81). |
| **05** | Resident System Processes (RSPs) and sample programs: `CLOCK`, `ECHO`, `RANDOM`, `SUBMIT` (the MP/M-86 batch processor), `TMP`. Each has `.A86` source plus a built `.RSP` or `.CMD`. |
| **06** | 8086 dev-tool sources: the line assembler (`ASS86.PLM`), disassembler (`DIS86.PLM`), **DDT86** debugger (`DDT86.A86`/`.CMD`), and `GENTAB` opcode-table generator feeding both. |
| **07** | CP/M-86-style transient command utilities as PL/M source: `PIP`, `SHOW`, `SET`, `TYPE`, `ERA`/`ERAQ`, `REN`, `DIR`, `SPOOL`/`SSPOOL`, `ATTACH`, `TOD`, `CONS`, `ABORT`, `DSKRST`. Plus `MPM86GEN.TEX`, the one build-process writeup in the tree, and numerous `BACKUP*`/`COM*`/`GEN*` Submit scripts. |
| **08** | More utilities: the **ED** line editor (with a revision history back to 1979 CP/M ED), **PIP**, **STAT86**, and the `GENCMD`/`GENDEF`/`GEMIT`/`GSCAN`/`GTOKEN` code-generation tools used to build command tables. |
| **09** | The **STAT** command family (system/queue/device status), plus `SEARCH`, `SORT`, `DISP`, `TIMEST`, and `RSPASM.A86` (an assembler that produces `.RSP` response files). Includes `VERS.LIT`, the version-constant definitions. |
| **10** | **PL/M-86 compiler** front end ("CM"): expression parsing (`EXPR`, `BREXPR`), macro expansion (`CMAC1-5`), mnemonic tables (`MNEM1-4`), instruction/line generation (`INSTR`, `DLINE`, `OUTP`), pseudo-op handling (`PSEUD1/2`). Purely `.PLM` — the only single-extension directory in the tree. |
| **11** | Companion to **10**: `.EXT`/`.LIT`/`.X86` support includes for the same compiler modules, plus the compiler's remaining `.PLM` modules (`SCAN`, `SUBR1/2`, `SYMB`, `TEXT`), a linkage stub (`C86LNK.ASM`), and `BNF.TEX` (the PL/M-86 expression grammar). |

Directories **10** and **11** together form one tool (the PL/M-86 compiler),
deliberately split in two, apparently for size.

## Build process (per `07/MPM86GEN.TEX`)

1. Assemble each kernel module (`BDOS`, `RTM`, `SUP`, `MEM`, `CIO`, `XIOS`,
   `SYSDAT`) with the `ASM86` cross-assembler.
2. Convert Intel-hex assembler output to a loadable `.CMD` with `GENCMD`.
3. Rename the resulting modules to `.MPM`/`.CMD` for combination.
4. Combine the renamed modules into `MPM.SYS` and `MPMLDR.CMD` via the
   loader (directory 04).
5. Utility programs (PL/M sources in 07–09) are compiled separately, in
   some cases on a VAX host via the `VAX.CPM` cross-development link
   (directory 03).

## Reference material (`orig/newplm/`, `orig/plm80s/`, `orig/asm80/`)

Three further trees live under `orig/`, added later and **not part of
the DRI MP/M-86 2.x distribution** — third-party tooling/documentation
used as independent references while building `tools/plm/`'s PL/M
compiler and `tools/asm86/`'s assembler, specifically to check guesses
about undocumented-looking builtins against a real specification
instead of guessing blind (see each entry below for how much a given
tree actually delivers on that).

- **`orig/plm80s/`** — the FORTRAN IV source of Intel's original
  **PL/M-80** cross-compiler (`PLM81.FOR`, `PLM82.FOR`; per
  `README.PLM`, released by Intel sometime in the late 1970s, provenance
  "a bit murky" but believed genuinely Intel's), plus `PLMLANG.DOC` (a
  PL/M-80 language summary) and `PLMCOMP.DOC` (compiler-control
  documentation), and a sample program (`PLMSAMP.PLM`/`.HEX`/`.PRN`).
  `PLMLANG.DOC` is what confirmed several MP/M-86 utility sources were
  calling real standard-library builtins (`TIME`, `DEC`/`PLUS`,
  `MEMORY`, `CARRY`, `ZERO`/`SIGN`/`PARITY`, `STACKPTR`, `SCL`/`SCR`)
  rather than referencing undeclared/missing procedures — see
  `tools/plm/README.md`'s "Notable discoveries" for the specifics.
  PL/M-86 is a direct descendant of PL/M-80 (same core builtins, plus
  `ADDRESS`/`WORD`/`INTEGER` extensions for the 8086), so this doc's
  builtin-function and pre-defined-variable sections transfer across
  almost unchanged.
- **`orig/newplm/`** — two independent, modern (1990s) third-party PL/M
  tools, unrelated to each other and to Intel/DRI: `PLM.ZIP` contains a
  basic lex/yacc PL/M-86-ish grammar/parser (`plm/plm-lex.l`,
  `plm/plm-parse.y` — explicitly "does nothing useful" per its own
  README, offered only as a published grammar reference) plus an
  unrelated bundled AVL-tree library (`avl-subs/`); `PLM2C.ZIP` is a
  PL/M-286-to-C source converter (`plm2c/`, originally written to
  convert "several meg" of a former employer's PL/M source, maintained
  only informally since).

A third reference tree, **`orig/asm80/`**, is a 1997 third-party
disassembly (by Larry A. Greene) of Digital Research's **original 1978
8080 CP/M `ASM.COM`** — given in both 8080 form (`ASM.ASM`) and a
hand-retargeted Z80 form (`ASM.Z80`), per `ASMREAD.ME`. This is a
disassembly of the assembler's own 8080 machine code (labels are just
addresses, e.g. `H0200`), not example assembly source or a syntax
reference, and it predates the 8086 entirely — it's a different CPU
from everything `tools/asm86/` targets, so despite the superficially
similar name, **it cannot validate `tools/asm86.py`'s 8086 instruction
encoding**, and nothing in it has been found to confirm or contradict
anything in this project's own `asm86` implementation. Kept here only
as a curiosity about DRI's assembler lineage (e.g. its one-letter
internal error codes for "comma missing," "bad register syntax," etc.).

## Related DRI source: MP/M II (`orig/mpm_II/`)

A much larger, directly relevant tree added later: the complete source
of **MP/M II V2.0** (per `orig/mpm_II/nucleus/mpm.asm`'s title line),
Digital Research's 8080/Z80 multi-programming monitor — the **direct
ancestor MP/M-86 2.x was ported from**, by the same company, with the
same architecture (BDOS, a scheduler/dispatcher nucleus, RSPs, CUSPs)
and even many of the same module/file names, just for an earlier CPU.
Copyright notices throughout date it to 1976–1981, with some files
explicitly revised "14 Sept 81 by Thomas Rolander" — i.e. written and
maintained right up to around when the MP/M-86 2.x port (this repo's
main subject) was done. 239 files across 15 top-level subdirectories
(`ASM/` has one further level, `DDT/`, inside it):

- **`nucleus/`** — the scheduler/dispatcher core, built the same way
  real MP/M-86 kernel modules are built but with the actual steps
  preserved in `xdos.sub` (unlike anything found so far in
  `orig/mpm862sr/`): each module is individually assembled with `RMAC`
  into a `.REL` relocatable object, the objects are grouped into three
  `LIB.COM` libraries, and the libraries are `LINK`ed together into the
  final `xdos.spr` — concrete, DRI-sanctioned precedent for real
  multi-object linking, directly relevant to `docs/build.md`'s "do we
  need a linker?" question (see also `mpmldr/gensys/` below). By title
  line (all "MP/M II V2.0 ..."):
  - `xdos1.lib` = `xdos.asm` (the XDOS dispatch entry itself — XDOS is
    MP/M II's name for what MP/M-86 splits into the Supervisor),
    `dsptch.asm` ("Dispatcher"), `queue.asm` ("Queue Management"),
    `flag.asm` ("Flag Management"), `memmgr.asm` ("Memory Management" —
    together with `dsptch`/`queue`/`flag`, the direct analogues of
    MP/M-86's RTM/MEM modules), `th.asm` ("Terminal Handler," the CIO
    analogue), `lst.asm` ("List Handler," the printer/list-device
    analogue — MP/M-86 has no separate module for this, it's folded
    into CIO).
  - `xdos2.lib` = `cli.asm` ("Command Line Interpreter," MP/M II's
    Supervisor-equivalent command parser), `tick.asm` ("Tick Process"),
    `clock.asm` ("Clock Process" — the direct ancestor of
    `src-assem/rsp/CLOCK.A86`; this and `tmpsub.asm` below are themselves RSP
    source, not nucleus code proper, despite living in this directory),
    `attach.asm` ("Attach Process" — the ancestor of `src-assem/utils/
    ATTACH.PLM`, here apparently a background process rather than a
    console utility).
  - `xdos3.lib` = `datapg.asm` ("Data Page" — shared data storage),
    `mpm.asm` ("Main Program," the entry point), `rlsmx.asm` ("Release
    MX queues Procedure"), `rlsdev.asm` ("Release Console & List
    Devices").
  - Linked directly into `xdos.spr` alongside the three libraries:
    `ver.asm` ("Version & Revision Date"), `clbdos.asm` ("CLBDOS
    Procedures" — the nucleus's own gateway *into* BDOS, the opposite
    direction from this repo's own `tools/plm/runtime_stub.A86`),
    `xdosif.asm` ("Common/Banked XDOS I/F," the dispatch entry
    application programs actually call into — MP/M II's analogue of
    MP/M-86's single `INT 224` gate), and `patch.asm` ("Patch Area," a
    reserved post-build patch space — the same role as this tree's own
    `PATCH.COD`/`.CPM` convention in `docs/filetypes.md`).
  - Built and linked *separately*, not part of `xdos.spr` itself:
    `tmpsub.asm` ("Terminal Message Processor & Submit" — MP/M II
    combines what MP/M-86 later splits into two separate RSPs,
    `TMP.A86` and `SUBMIT.A86`, into this one module; its linked output
    is renamed `tmp.spr`) and `bnkxdos.asm` ("Banked XDOS," the
    banked-memory counterpart to `xdos.asm` — same relationship as
    `bdos/`'s resident/non-resident split below, just for the nucleus).
- **`bdos/`** — the BDOS file-system module for a **banked**
  (multi-memory-bank) system, split into a resident part (lives in
  every bank) and a non-resident part (swapped into whichever bank is
  current) — confirmed by each file's own title line, not just file
  naming — built by literally concatenating two `.asm` files with `PIP`
  before assembling the result as one file, per `bdos.sub`:
  - **`resbdos.spr`** = `resbdos1.asm` ("Resident Portion of Banked
    BDOS") + `conbdos.asm` ("Console Portion" of BDOS — console-related
    BDOS calls), concatenated into one `resbdos.asm` and assembled.
  - **`bnkbdos.spr`** = `bnkbdos1.asm` ("Non-Resident Portion of Banked
    BDOS") + `bdos30.asm` (the actual, ~100KB core file-system logic —
    no `$title` line of its own since it's meant to be concatenated in,
    but `dvers equ 30h` inside it confirms "BDOS 3.0"), concatenated
    into one `bnkbdos.asm` and assembled. This is MP/M-86's own `.BDO`
    fragment-concatenation convention (`BDOSIF.BDO`, `FILE1-4.BDO`,
    etc. — `docs/filetypes.md`) done the exact same way one CPU
    generation earlier, down to the same "glue source files together,
    then assemble the result as one unit" mechanism — not a new idea
    this repo's own `tools/asm86`/`tools/plmc` stub-concatenation
    invented either (see `mpmldr/gensys/` below for a third instance of
    the same pattern, at the hex-binary level instead of source level).
  Banked vs. non-banked memory was an 8080/Z80-specific concern MP/M-86's
  flat 8086 memory model doesn't have, so neither variant has a direct
  MP/M-86 counterpart as such — `src-assem/kernel/BDOS.A86` corresponds to
  *both* of these together conceptually, not to either one alone.
- **`RSPs/`** — background Resident System Processes, all with no direct
  MP/M-86 equivalent in this tree. Three of the four follow the exact
  same resident/banked split already seen in `bdos/` and `nucleus/`
  above — confirmed directly by `brspbi.asm`'s own title, `"Banked
  Resident System Process BDOS Interface"`, which settles what `.BRS`
  actually stands for (not a guess from the extension alone):
  - **Scheduler** (`scrsp.plm`, "Scheduler Process - Resident Portion"
    → `sched.rsp`; `scbrs.plm`, "Scheduler Process - Banked Portion" →
    `sched.brs`).
  - **Spool** (`sprsp.plm`/`spbrs.plm`, same Resident/Banked split →
    `spool.rsp`/`spool.brs` — a print-spooler process).
  - **Status** (`msrsp.plm`/`msbrs.plm`, "Status Process," same split →
    `mpmstat.rsp`/`mpmstat.brs` — the RSP behind the `MPMSTAT`/`STAT`
    family of commands; `CUSPs_2/msts.plm` below is the user-facing
    command that talks to it, the same command-talks-to-a-status-RSP
    shape MP/M-86's own `STSCMD`/`STSRSP` split has, just for a
    different RSP).

  Each banked ("BRS") half is linked together with `brspbi.asm` itself
  (a small shared BDOS-call interface for the banked side) and built
  twice — located at both `0000H` and `0100H`, converted to Intel-hex,
  then diffed and run through `GENMOD` (per `brsgen.sub`/`brsgent.sub`/
  `cbrsgen.sub`, generic templates for this exact
  two-location-then-diff-then-GENMOD build, parameterized on the RSP
  name) — the same "assemble a module twice at two addresses and let
  `GENMOD` diff the two hex outputs to find the address-dependent
  bytes" relocation technique documented in full under `ASM/DDT/` below
  (and, independently, in every `CUSPs_1`–`CUSPs_5` build script) — not
  the simpler source-concatenation technique `bdos/` above uses, a
  genuinely different mechanism despite both being "glue pieces
  together" in spirit. `mscmn.plm`
  ("Common Literals") is a shared, title-less PL/M fragment meant to be
  pulled into each of these — `TRUE`/`FALSE`/`BOOLEAN` constants plus
  `mon1`/`mon2`/... `EXTERNAL` procedure declarations for the BDOS/XDOS
  call interface, the exact same shape (even the same names) as this
  repo's own `tools/plm/README.md` calling-convention and
  `tools/plm/runtime_stub.A86`'s `pl_mon1`–`pl_mon4`.

  **Abort** (`abort.asm`, "Abort Resident System Process" → `abort.rsp`)
  is the odd one out: a single, undivided RSP with no resident/banked
  split at all, built directly by `abort.sub` (`RMAC` → `XREF` → `LINK`,
  no `GENMOD`/hex-concatenation step) — a real architectural difference
  from `ABORT.PLM`, which is an ordinary console utility under MP/M-86,
  not a background process.

`CUSPs_1` through `CUSPs_5` are "Console Utility Programs," MP/M II's
name for the category `src-assem/utils/` holds under MP/M-86 — split across
five directories (by distribution disk, going by `Control_1`/`Control_2`
below holding the equivalent pre-built binaries in the same groupings)
rather than one, unlike `src-assem/utils/`. Every one of them is built the
same way, confirmed directly in each directory's own build script
(`CUSPs_1/prla1-3.sub`, `CUSPs_2/prlb1-4.sub`, `CUSPs_4/pip.sub`,
`CUSPs_5/dall.sub`): compile with `PLM80`, then `LINK` with a small
shared external-stub object (`x0100`, the same file as `mpmldr/
gensys/x0100.asm`'s "Externals" — reused across every `CUSPs_*`
directory, not local to the loader) and `LOCATE` at `0100H`; separately
link+locate the *same* object again at `0200H`; `OBJHEX` both, `PIP`
them together, then `GENMOD` the combined hex into the final `.PRL` —
the same two-address-then-diff relocation technique already confirmed
for `RSPs/` above (and, below, for `ASM/DDT/` too), now shown to be how
literally every CUSP in
this tree is built, not an exception. The final `GENMOD` command spells
out its own output filename by hand with a leading `x` (e.g. `genmod
dir.hex xdir.prl`) — see the correction under `CUSPs_3/` below for where
that `x`-prefix convention actually comes from.

- **`CUSPs_1/`** — direct file-for-file ancestors of utilities already
  in `src-assem/utils/`, same PL/M source just for the 8080: `dir.plm`,
  `era.plm`, `eraq.plm`, `ren.plm`, `set.plm`, `show.plm`, `type.plm`,
  and `stat.plm` (the undivided ancestor of MP/M-86's `STSCMD.PLM`/
  `STSCOM.PLM`/`STSRSP.PLM` split — one PL/M-80 program where MP/M-86
  has three). The `x*.prl`/`prla*.sub` files are this disk's pre-built
  compiled forms and build (Submit) scripts for the same set.
- **`CUSPs_2/`** — more direct ancestors (`abort.plm`, `cns.plm`
  ≈ `CONS.PLM`, `drst.plm` ≈ `DSKRST.PLM`, and `tod.plm` — the direct
  ancestor of `src-assem/utils/TOD.PLM`, title "MP/M II V2.0 Date and Time"),
  plus several command-side utilities with **no equivalent in
  `src-assem/utils/`**: `mschd.plm` ("Scheduler Transient Program," controls
  the `sched` RSP in `RSPs/`), `mspl.plm` ("Spool Program," controls the
  `spool` RSP), `stpsp.plm` ("Stop Spooler Program"), `msts.plm`
  ("Status Program," controls the `mpmstat` RSP — the same
  command-talks-to-a-status-RSP shape as MP/M-86's `STSCMD`/`STSRSP`
  split, just for a different RSP), `print.plm` ("List Number
  Assign/Display," a LIST-device management command), `user.plm` ("User
  Number Assign/Display"), `sub.plm` (the console-side `SUBMIT` command,
  pairing with `nucleus/tmpsub.asm`'s RSP half), and `prlcm.plm` ("PRL
  to COM File," a format-conversion utility). `mscmn.plm` is a shared
  `DECLARE`d-literals/common-code module for the `ms*` family, not a
  command on its own. `dump.asm`/`extrn.asm` are a sample `DUMP` utility
  and its separately-assembled external-reference stub, per `dump.asm`'s
  own header comment on how to build it — demonstration code, not a
  real product utility. The `x*.prl`/`prlb*.sub` files are this disk's
  compiled forms/build scripts.
- **`CUSPs_3/`** — the `GENHEX`/`GENMOD`/`LOAD` system-generation
  toolchain (MP/M II's equivalent of what `orig/mpm862sr/`'s numbered
  disks hold for MP/M-86's own build process): `genhex.asm`/`.com` and
  `genmod.asm`/`.com` (source plus pre-built binary for each — `GENMOD`
  is "MP/M Relocatable Module Generator," per its own title line) and
  `load.plm`/`.com` (`LOAD`, the "CP/M Command File Loader" — a generic
  Intel-hex-to-`.COM` converter, the final step of building *any*
  loadable binary, not an RSP-specific tool; see `mpmldr/gensys/`'s own
  build scripts below for it in actual use). `comgen.sub` is a simpler,
  *different* generic template (a single `link`+`locate` with no
  double-address/`GENMOD` relocation step at all) — **correcting an
  earlier version of this document**, which guessed the `x`-prefix on
  every compiled CUSP (`xdir.prl`, `xabort.prl`, `xpip.prl`, etc.) came
  from this file's own `LINK ... TO x$1.mod` output naming. It doesn't:
  the real per-utility build scripts (`CUSPs_1/prla1-3.sub` etc., see
  above) show the `x`-prefix is just typed directly into each `GENMOD`
  command's own output filename argument (`genmod dir.hex xdir.prl`) —
  a plain naming convention for the final compiled form, with no
  acronym behind it, most likely chosen just to avoid colliding with
  the same-named `.PLM` source file in the same directory during the
  build: `Control_1/` (the actual distribution disk) has the *same*
  compiled programs under their clean, unprefixed names (`dir.prl`,
  `era.prl`, `sdir.prl`, etc., with no source `.PLM` alongside them to
  collide with there), so the `x`-prefix is dropped again somewhere
  between building a CUSP here and shipping it on that disk.
- **`CUSPs_4/`** — two more direct ancestors of `src-assem/utils/` utilities,
  confirmed via `pip.sub` to use the exact same double-address/`GENMOD`
  build as everything else in `CUSPs_1`–`CUSPs_5`: `ed.plm`/`.prl` and
  `pip.plm`/`.prl`/`.sub` (`ED.PLM` and `PIP.PLM` already exist under
  those exact names in `src-assem/utils/`).
- **`CUSPs_5/`** — a single program with no MP/M-86 equivalent in this
  tree at all — **SDIR**, an "extended directory" command built from one
  module per file (`dm.plm`, "Super Directory Command," is the entry
  point; `da`=arithmetic, `dp`=print, `dse`=search, `dsh`=show/display,
  `dso`=sort, `dts`=time-stamp display, `sn`=the command-line scanner,
  each file's own `$title` confirms its role) — see `docs/glossary.md`'s
  "SDIR" entry. `xsdir.prl` is `dall.sub`'s own `GENMOD` output;
  `sdir.prl` (no `x`) is the same program under its clean, shipping
  name — see the `x`-prefix note under `CUSPs_3/` above.
- **`LITs/`** — `DECLARE ... LITERALLY` include files, the direct
  ancestor of this tree's own `.LIT` convention (see
  `docs/filetypes.md`). File-by-file, against what's actually in
  `src-assem/` (not just name-matching — each pair was `diff`ed):
  - **`copyrt.lit`** → `src-assem/utils/COPYRT.LIT`, nearly verbatim (just the
    copyright year range shortened from "1979,1980,1981" to "1981").
  - **`fcb.lit`** (`fcb$descriptor` structure) → `src-assem/utils/`'s own FCB
    handling, same concept, not a verbatim carryover.
  - **`flag.lit`** → `src-assem/utils/FLAG.LIT`: both define a process
    flag/status data layout, but the actual fields differ (MP/M II's
    is just `nmbflags literally '16'`; MP/M-86's is a full
    `flag$structure` with `pd`/`ignore` fields) — same role, reworked
    content, not a copy.
  - **`proces.lit`** → `src-assem/utils/PROCES.LIT` — and this is the one
    with the smoking-gun evidence for the whole tree's lineage:
    MP/M-86's own copy's banner comment literally reads `"Proces
    Literals MP/M-8086 II"`, an intermediate name directly naming both
    ancestor and descendant in one string.
  - **`queue.lit`** (`queueheader` structure) → renamed to
    `src-assem/utils/QD.LIT` under MP/M-86 ("QD" = Queue Descriptor), not a
    name match despite being the same concept.
  - **`sysdat.lit`** (byte-offset layout comments for system data) →
    renamed `src-assem/utils/SD.LIT` ("System Data Page") under MP/M-86, same
    concept (a based structure describing the system data page), not a
    name match either.
  - **`memmgr.lit`** (one combined `memory$descriptor` structure) →
    split into three named structures, `src-assem/utils/MDSAT.LIT`'s
    `md$structure`/`ms$structure`/`sat$structure` — same role, evolved
    layout, renamed file.
  - **`common.lit`** (`TRUE`/`FALSE`/`BOOLEAN`/`FOREVER` plus the
    `LITERALLY`→`LIT`/`DECLARE`→`DCL` abbreviation bootstrap) → renamed
    `src-assem/utils/COMLIT.LIT` under MP/M-86 — the exact file
    `tools/plm/README.md`'s "Notable discoveries" already singles out
    for "bootstrapping its own syntax sugar."
  - **`dpgos.lit`**, **`xdos.lit`** — BDOS/XDOS function-code constants
    with no identified direct `.LIT` counterpart in `src-assem/`; the same
    *information* (function numbers) lives on in `src-assem/`'s own `.DEF`
    files instead, under the 8086 port's own naming.

  `src-assem/utils/UDA.LIT`'s own banner comment is a second instance of the
  same "MP/M-86 II" intermediate naming found in `PROCES.LIT` above —
  `"MP/M-86 II  User Data Area format - August 8, 1981"` — further
  confirming the whole `.LIT` set's lineage isn't just a resemblance,
  it's a literal, dated, in-place evolution of these same files.
- **`ASM/DDT/`** — the actual **original source** of DRI's 8080 `ASM.COM`
  cross-assembler and of `DDT` (the 8080 ancestor of the `DDT86` this
  tree's own `docs/glossary.md` already describes), each module
  identified by its own title line, not guessed from the filename:
  - **`ASM.COM`**: `as0com.asm` ("Asm Common Data Area"), `as1io.asm`
    ("Asm IO Module"), `as2scan.asm` ("Asm Scanner Module"), `as3sym.asm`
    ("Asm Symbol Table Module"), `as4sear.asm` ("Asm Table Search
    Module"), `as5oper.asm` ("Asm Operand Scan Module"), `as6main.asm`
    ("Asm Main Module").
  - **`DDT.COM`**: `ddt2mon.asm` ("MP/M Debugger (DEMON) 9/81" — the
    main debugger/monitor command loop; "DEMON" appears to be DRI's own
    internal nickname, **DE**bugger-**MON**itor), `ddt1asm.asm` ("MP/M
    Debugger (ASMOD) 9/81," commented "MP/M Debugger Disassembler/
    Assembler Module" — DDT's own built-in mini-assembler/disassembler,
    letting a user patch memory by typing mnemonics directly), and
    `ddt0mov.asm` ("DDT Relocator Program," which the file's own
    comment says exists "to perform the move from 200H to the
    destination address" — a small relocation stub run once DDT is
    loaded).

  `asm.sub`/`ddt.sub` (this directory's own build scripts) reveal a
  build technique used throughout MP/M II wherever a `.PRL` is the
  final target, not just here: each module is assembled **twice**, with
  a `$$pzsz+r`-style option changing the second pass, and the two hex
  outputs are diffed by `GENMOD` — any byte that differs between the
  two assemblies is address-dependent and gets flagged for load-time
  relocation, which is mechanically *how* "Page Relocatable" binaries
  work, not just what the format is called. The same two-pass-then-diff
  shape already showed up in `RSPs/`'s `locate ... code(0000H)` /
  `code(0100H)` pair. `DDT.COM`'s own build additionally runs through
  `PRLCOM` (`CUSPs_2/prlcm.plm`, "PRL to COM File," already documented
  above) as its very last step, converting the GENMOD'd `rdt.prl`
  (DDT's own fully-relocated image, also present pre-built in
  `Control_1/`) into the final loadable `.COM`.

  Unlike `orig/asm80/` (a third-party *disassembly* of the compiled
  `ASM.COM` binary — see the "Reference material" section above), this
  is DRI's own original source, but it's still 8080 code, a different
  CPU from everything `tools/asm86/` targets — so, like `orig/asm80/`,
  it cannot directly validate this repo's 8086 instruction encoding,
  only the surrounding directive/macro conventions.
- **`Control_1/`** — a pre-built "golden master" distribution disk, not
  source: compiled forms of essentially everything described above, in
  MP/M II's native binary formats (`.PRL` = Page Relocatable — every
  CUSP and `CUSPs_3`'s system-gen tools, plus `ASM/DDT/`'s own output —
  `asm.prl` (`ASM.COM`'s final, shipped form is itself a `.PRL`, unlike
  `DDT.COM`) and `rdt.prl` (an intermediate artifact from `DDT.COM`'s
  build, kept here alongside the final `ddt.com`); `.SPR` =
  the banked/resident kernel-module format, for `bnkbdos`/`bnkxdos`/
  `resbdos`/`tmp`/`xdos`; `.BRS`/`.RSP` for the `abort`/`sched`/`spool`/
  `mpmstat` RSPs; `.COM` for `gensys`/`load`/`mpmldr`/`ddt`). See
  `docs/filetypes.md` for how this repo's own `.MPM`/`.RSP`/`.CMD`
  conventions descend from these same formats.
- **`Control_2/`** — DRI's **development toolchain** disk, also mostly
  pre-built binaries rather than source. `disk.doc` (this disk's own
  README) explicitly categorizes every file on it, resolving any
  ambiguity about what's a real tool, what's a macro library, and what
  is (and isn't) sample/demo code:
  - **Tools**: `RMAC.COM` ("Relocating Macro Assembler," the actual
    assembler used to build everything in `nucleus/`/`bdos/`/`ASM/DDT/`
    — a more capable successor to plain `ASM.COM`), **`LINK.COM`**
    ("Linkage Editor" — a real linker; see `docs/build.md`'s "do we
    need a linker?" discussion, MP/M II's own build process evidently
    used one throughout, unlike what's been found so far for MP/M-86),
    `LIB.COM` ("Library Manager"), `XREF.COM` ("Cross Reference
    Utility").
  - **Macro libraries** (source, not binaries): `i8085.lib` (8085
    RIM/SIM instruction macros), `z80.lib` (Z80 opcode macro library,
    with its own `z80.doc`), `simpio.lib`/`seqio.lib` (simple/sequential
    BDOS I/O), `stack.lib`/`dstack.lib` (stack-machine libraries),
    `compare.lib`/`ncompare.lib` (8-bit comparison macros), and
    `when.lib`/`dowhile.lib`/`select.lib` (structured-programming
    control-flow macros — `WHEN`/`DOWHILE`/`SELECT` constructs grafted
    onto assembly language). `inter.lib`/`treadles.lib`/`buttons.lib`
    are `disk.doc`'s odd one out: macro libraries for a traffic-light
    intersection control demo ("see manual"), unrelated to MP/M II
    itself.
  - **`disk.doc`'s own "Sample ... for MP/M II" group** — six files,
    all genuinely samples by DRI's own categorization, not just the
    hardware-specific ones: `ldrbios.asm` ("Sample Loader BIOS for
    MP/M II MPMLDR" — direct precedent for the "write a new XIOS for
    QEMU" problem `docs/build.md` already describes for MP/M-86),
    `boot.asm` ("Sample Boot"), `deblock.asm` ("Sample Deblocking
    algorithm"), `todcnv.asm` ("Sample Time & Date Conversion"),
    `resxios.asm` ("Sample Resident XIOS" — confirming "XIOS" is an
    MP/M II term MP/M-86 inherited, not something coined for the 8086
    port; titled internally for a specific OEM machine, "DSC-2 Basic &
    Extended I/O Systems"), and `dump.asm` ("Sample PRL" — a
    demonstration of building a `.PRL`-format program, not a real
    diagnostic tool). `extrn.asm` ("Sample External module for
    DUMP.ASM") is `dump.asm`'s companion stub. Both `dump.asm` and
    `extrn.asm` are byte-for-byte identical to the copies already
    described under `CUSPs_2/` above — the same two files kept in both
    places, not a coincidence of naming.
- **`mpmldr/gensys/`** — MP/M II's own loader and system-generator, the
  direct ancestor of `orig/mpm862sr/`'s `MPMLDR` (`docs/glossary.md`)
  and of this repo's own `src-assem/loader/`. Two separately-built programs,
  each with its own PL/M main module plus supporting `.ASM` pieces —
  and `mpmldr.sub`/`gensys.sub` (this directory's own build scripts, not
  just a generic `comgen.sub`-style template) spell out exactly how:
  - **`MPMLDR.COM`** (`mpmldr.plm`, "MP/M II V2.0 Loader" — reads the
    system image off disk and starts MP/M running) is built from three
    *separately* assembled/compiled pieces, combined two different ways:
    `mpmldr.plm` and `ldmonx.asm` ("Loader BDOS Interface," a small
    stub) are compiled/assembled then **`LINK`ed together** into one
    module; `ldrbdos.asm` ("Loader BDOS," a cut-down standalone BDOS
    usable before the real kernel is running) and `ldrbios.asm`
    ("Skeleton Ldrbios" — explicitly a *skeleton*, i.e. the
    OEM-adaptation point for this machine's own disk/console driver,
    assembled with `MAC`, not `ASM80`) are instead each turned into
    their own Intel-hex output and **concatenated at the hex level**
    with the linked loader module (`pip mpmldr.hex=impmldr.hex[I],
    ldrbdos.hex[I],ldrbios.hex[H]`), then run through `LOAD` (from
    `CUSPs_3/`) to produce the final `.COM`. The hardware-specific
    `ldrbios.asm` is deliberately kept *outside* the `LINK` step and
    only glued on by simple concatenation — the same shape of problem
    `docs/build.md` already describes for MP/M-86's own XIOS, and
    concrete precedent that "assemble the OEM-specific piece separately
    and concatenate" is a real, DRI-sanctioned pattern here, not
    something invented for this repo's own `tools/plmc`+`tools/asm86`
    stub-concatenation (`tools/plm/README.md`'s "pl_mon1–pl_mon4").
  - **`GENSYS.COM`** (`gensys.plm`, "MP/M II V2.0 System Generation" —
    the interactive program that actually builds a configured system
    image, prompting for things like "Enter memory segment table" per
    its own embedded strings) links together with `ldrlwr.asm` ("Load,
    Relocate, and Write File" — the routine that reads each compiled
    kernel/CUSP module and relocates+writes it into the final image;
    this is the closest thing in either MP/M tree to "real loader/
    linker logic," and the most concrete lead yet for `docs/build.md`'s
    "do we need a linker?" question) and `x0100.asm` ("Externals," a
    shared external-declaration stub for the other two — the same role
    as this repo's own `tools/plm/runtime_stub.A86`).
  - `mpmldr.com`/`gensys.com` are this directory's own pre-built
    binaries of the above.
- **`OEM/`** — `mpm2srl.asm`/`.com`, the "Serialization Program for
  MP/M II diskettes" (per its own title line) — *not* a hardware/serial
  **I/O** driver despite the name (an earlier version of this document
  mistook it for one): it's a disk-duplication utility that copies the
  distribution diskettes while stamping each copy with an incrementing
  serial number (`nsrlsec`, `bserial`/`aserial`, `incserial` — real
  identifiers inside it), the OEM-licensing/anti-piracy tracking
  mechanism `TEX_Files/mpm2oem.tex` below documents how to use.
- **`TEX_Files/`** — plain-text documentation (same `.TEX`-as-plain-text
  convention already noted in `docs/filetypes.md`), each more specific
  than its filename alone suggests:
  - **`mpm2ltr.tex`** — a cover letter ("Dear MP/M II user:") shipped
    with the V2.0 release, including DRI's own estimate of porting
    effort: "less than a week" for a simple polled system on a machine
    with working CP/M 2.2, "several weeks" for a fully interrupt-driven,
    bank-switched configuration.
  - **`mpm2oem.tex`** — the "OEM REDISTRIBUTION GUIDE," which turns out
    to be the actual manual for the `OEM/` serialization program above,
    not general hardware-porting guidance: it explains "the diskette
    serialization process which allows construction and verification of
    MP/M diskettes which are distributed by the OEM," and explicitly
    names "Control 1/, 2/, and Serialization" as the three distribution
    diskettes it covers — confirming that this tree's own `Control_1/`/
    `Control_2/` directory names are taken directly from DRI's own
    diskette-labeling scheme, not something assigned later.
  - **`mpm2stat.tex`** — an internal DRI **project-status report**, not
    end-user documentation for the `MPMSTAT` command (an earlier version
    of this document assumed it was): a dated schedule table ("Status
    reporting date: 08-17-81") tracking XDOS/BDOS/Utilities/
    Serialization/Production-Release milestones by staff initial
    ("Tom," "Dave" — almost certainly Thomas Rolander, credited in the
    "Revised: 14 Sept 81" header comment on dozens of files throughout
    this entire tree) against scheduled vs. completed dates.

Nothing in `orig/mpm_II/` has been assembled or otherwise validated
against this repo's own tooling — `tools/asm86/`/`tools/plmc.py` target
the 8086, and this entire tree is 8080/Z80 source. It's kept here as
architectural reference (the clearest available picture of what
MP/M-86 2.x was actually ported *from*) and, via `Control_2/`, as the
most concrete lead yet on what a real MP/M-86 linker and a real
hardware-adaptation (XIOS-equivalent) layer should look like.

## Related DRI source: CP/NET (`orig/cpn86src/`, `orig/cpnet80s/`)

Two more trees, added later — genuine Digital Research product source,
like the MP/M-86 tree itself, but a **different product**: CP/NET,
DRI's network/workstation-sharing layer for CP/M-family systems (lets a
diskless or disk-light "slave" workstation log in over a serial/network
link and use a "master" machine's disks, printer, etc. — a 1980s
precursor to LAN file-and-printer sharing, not related to MP/M-86's own
multi-user/multi-process model beyond being built by the same company in
the same era). Neither tree is needed to build or boot MP/M-86 itself;
they're kept here as further genuine DRI source to potentially validate
`tools/asm86/` against, since the newer of the two (`orig/cpn86src/`)
happens to be written in the exact ASM86 dialect this repo's own
assembler targets.

- **`orig/cpnet80s/`** — **CP/NET** (the original, 8080/Z80 version,
  "VER 1.0", dated 1982 in file headers), 9 `.MAC` files (DRI's
  MAC/M80-style 8080/Z80 macro-assembler dialect — see `docs/filetypes.md`
  for `.ASM`/`.Z80` vs this repo's own `.A86`; `tools/asm86/` does **not**
  target this dialect or CPU at all, so none of this is assemblable with
  this repo's tools): `CPNETLDR.MAC` (loader), `NBOOT.MAC` (network boot
  loader for diskless slaves), `NBDOS.MAC` ("no disk operation" BDOS
  variant for diskless slaves), `NDOS.MAC`/`NDOSS.MAC` (network DOS —
  the slave-side redirector; `NDOSS.MAC` adds dated "NOS" modifications),
  `SNIOS.MAC`/`NNIOS.MAC` (Slave Network I/O System — `NNIOS.MAC` is the
  "NOS mode" variant), `SLVSP.MAC` (Slave Support Process), and
  `GENNET.MAC` (the CP/NET system generator, "FOR NOS"). "NOS" here
  appears to label a later sub-variant/mode of CP/NET within this same
  source generation, not a separate DRI product.
- **`orig/cpn86src/`** — **CP/NET-86**, the 8086 port (file headers dated
  Aug–Sep 1983, a year after the `cpnet80s` files and two years after
  this tree's own MP/M-86 2.x), 9 `.A86` files in real ASM86. Same
  architecture as the 8080 version, renamed/reorganized: `CPNETLDR.A86`
  (loader), `NDOS.A86`/`NDOSO.A86` (network DOS slave — identical source
  apart from one `BASE EQU` relocation constant, `0` vs `100H`: two
  link-address variants of the same module, not two different programs),
  `SNIOS.A86`/`SNIOSO.A86` (Slave Network I/O System, same `0`-vs-`100H`
  relocation-variant relationship, confirmed by diff), `CCPN.A86`
  (Console Command Processor for CP/NET-86 — a CP/NET-aware command
  shell, distinct from MP/M-86's own Supervisor/`COMMAND.SUP`),
  `CPNETSTS.A86` (network status display utility), and `LOGIN.A86`/
  `LOGOFF.A86` (the user-facing transient commands to join/leave the
  network).

  None of these 9 files `INCLUDE` anything else — each is fully
  self-contained — but most of them (`CCPN.A86`, `CPNETLDR.A86`,
  `LOGIN.A86`, `LOGOFF.A86`, `NDOS.A86`, `NDOSO.A86`, `SNIOS.A86`,
  `SNIOSO.A86`) use MASM-style `SEGMENT`/`ASSUME` (and `NDOS.A86`/
  `NDOSO.A86` also `MACRO`), which `tools/asm86/` explicitly doesn't
  implement (see its README's "Known limitations") — the same gap
  `tools/plm/runtime_stub.A86`'s own header notes for `MCD.A86`/
  `SCD.A86`'s `SEGMENT`/`GROUP` usage. Only `CPNETSTS.A86` uses neither, but it
  doesn't assemble cleanly with the current tooling either (a real,
  not-yet-investigated gap — `trailing tokens in expression: '['` on its
  line 22). None of `orig/cpn86src/` has been validated against
  `tools/asm86/` yet; this is just the inventory.

## Related DRI source: factory-labeled disk images (`orig/driorig/`)

The same collector's companion set to `orig/lastdri/` below, imaged the
same way (`22disk` transfer, hence the `.CPM`-renamed CP/M `.COM`
binaries; Teledisk v2.15 `.TD0` images; a `.JPG` label scan per disk) —
but, per `orig/driorig/README.TXT`, "the last of my DRI 'original'
disks **with factory labels**," i.e. genuine shrink-wrapped DRI product
releases rather than `orig/lastdri/`'s in-house/development disks. The
README specifically calls out two as OEM **master** disks: **Pascal
MT+** and **CB86**. None of this is MP/M-86 source or needed to build
it — it's further genuine DRI product material, same era, kept as
reference the way `orig/lastdri/` and `orig/mpm_II/` are:

- **`MTP1OF3/`, `MTP2OF3/`, `MTP3OF3/`** — the three-disk **Pascal MT+**
  compiler distribution (confirmed by file naming — `MTPLUS.CPM`,
  `LINKMT.CPM`, `FASTCOMP.CPM` — "MT" = Pascal MT+, DRI's 8080/Z80
  Pascal product, a different language/product from PL/M, C, or
  CBASIC):
  - **`MTP1OF3`** — the compiler/linker/runtime itself: `MTPLUS.CPM`
    (compiler), `FASTCOMP.CPM`, `LINKMT.CPM`, `MTPLUS.000`–`.006`
    (numbered overlay segments), `PASLIB.ERL`/`ROVLMGR.ERL` (runtime
    library and overlay-manager relocatables), and demo/sample source
    (`DEMO.SRC`, `PROG.SRC`/`MOD1.SRC`/`MOD2.SRC` — an overlay demo
    program with `EXTERNAL [n] PROCEDURE` overlay calls, `SPEED.DEM`).
  - **`MTP2OF3`** — development tools built *with* Pascal MT+ itself:
    `INDEXER.PAS`/`.DOC` (a Pascal source-indexer utility, with its own
    usage doc), `PRETTY.PAS` (a pretty-printer), plus the low-level
    runtime support those and the compiler need: `XREF.SRC`/`.CPM`/
    `.DOC` (cross-reference utility), `DIS8080.CPM` (an 8080
    disassembler), `DEBUGGER.ERL`/`DBUGHELP.TXT`, floating-point/BCD
    runtime (`FPREALS.ERL`, `BCDREALS.ERL`, `REALIO.ERL`,
    `TRANCEND.ERL` — transcendental functions), **AMD 9511** arithmetic
    processing unit (APU) hardware floating-point support
    (`AMD9511.CMD`/`AMD9511X.CMD`, `APUSUB.MAC`, `TRAN9511.SRC`/`.ERL`
    — an optional hardware FPU co-processor chip, not emulated/software
    floating point), and assorted `.MAC` glue (`CHN.MAC` chaining,
    `OVLMGR.MAC`, `CWT.MAC`, `RST.MAC`, `DIVMOD.MAC`).
  - **`MTP3OF3`** — a second, larger Pascal application built with the
    same toolchain: **`SPP`** (`SPPMAIN.CMD`, `SPP.CPM`, `SPP.001`–
    `.00A` as further numbered overlay segments, `LINKSPP.SUB`) — a
    screen/structured editor judging by its module names
    (`EDITWRIT.ERL`, `EDGLBLS.SRC`/`EDTYPES.SRC`, `SCANNER.ERL`,
    `SYNCHECK.ERL`, `VARCHECK.ERL`, `TRAVERSE.ERL`, `DIROVL.ERL`,
    `SBLOG.ERL`) alongside a second overlay set `SB001.CMD`–`SB00A.CMD`
    with its own `SBINIT.ERL`/`SBIFDEF.LIB` — likely "SB" = a
    syntax-directed/structured **editor** companion to "SPP," though
    the exact SPP/SB relationship (two programs sharing the `MTP2OF3`/
    `MTP3OF3` runtime vs. one program in two overlay families) hasn't
    been traced further. `E2.CMD`/`E3.CMD`, `NSB.SRC`, `BACKUP.SRC`,
    and `SMALLERR.TXT` round out the set.
- **`PLI80/`** — a second, more complete copy of the **PL/I-80**
  toolchain already seen in `orig/lastdri/09/` (`PLI.CPM`,
  `PLI0-2.OVL`, `PLILIB.IRL`, `XREF.CPM`, `RMAC.CPM`, `LINK.CPM`,
  `LIB.CPM`), here with the pieces specific to calling MP/M directly
  from PL/I that `orig/lastdri/09/` didn't have: `MPMCALLS.PLI`/
  `MPMCALLC.PLI` ("Direct MP/M Call Test Program[s]," per
  `MPMCALLS.PLI`'s own header comment — demonstrating PL/I-to-MP/M
  syscalls), `MPMDIO.ASM`/`.DCL` (the matching assembly-level direct-IO
  stub and its PL/I `DECLARE` binding), and `PLIDIO.ASM` (a more
  general direct-I/O interface). Direct, reusable precedent for how
  DRI itself bound a third-party-language runtime into MP/M-86's own
  syscall gate — relevant to this repo's own `tools/plm/runtime_stub.A86`.
- **`CBASIC86-719/`, `CBASIC86-720/`** — two separate **CBASIC-86**
  distribution-disk images (`CBAS86.CMD` compiler, `CRUN86.CMD`
  runtime, `XREF86.CMD` cross-reference tool) — CBASIC being DRI's
  BASIC-family product, a different language again from PL/M, C,
  Pascal MT+, and PL/I. The two disks' file sets are otherwise
  identical; the distinction between them (two pressings, two
  revisions, or a compiler/runtime split) hasn't been investigated.
- **`CB86-Master_Control/`** — per the README, one of the two **OEM
  master disks** in this set, and per its own `READ.ME` ("CBASIC
  Compiler (CB86) ... Version 2.0," June 1983) the master build/release
  disk for the 8080/Z80 **CBASIC** compiler (not CBASIC-86 above —
  despite the shared "CBASIC" name, this is the original 8-bit product,
  confirmed by the overlay naming): `CB86.CMD` (compiler) built from
  `CB86.L86` plus three overlay files `CB86.OR1`–`.OR3`, `LINK86.CMD`/
  `LIB86.CMD` (the linker/librarian used to produce it — master-disk
  build tooling, not end-user tools), and five CBASIC demo programs
  (`TSTCIR.BAS`, `GRAPHR.BAS`, `GRAPHCOM.BAS`, `CIRCOM.BAS`,
  `DEMOGRAF.BAS` — circuit/graphics demos).
- **`CPNET80/`** — a fuller **CP/NET** disk than either
  `orig/cpnet80s/` or `orig/cpn86src/` above: still the 8080/Z80 side
  (per `CPNIOS.ASM`'s own title, "Requester Network I/O System for
  CP/NET **1.2**" — a later version than `orig/cpnet80s/`'s "VER 1.0"),
  but here as a mix of source *and* DRI's own built binaries:
  `CCP.SPR` (the CP/NET-side console processor, built `.SPR` just like
  `orig/mpm_II/`'s banked kernel modules), `NDOS.SPR`, `CPBDOS.REL`/
  `CPNDOS.REL`/`CPNOS.REL` (relocatable BDOS/network-DOS objects),
  `SERVER.RSP` (a CP/NET server-side RSP), plus source for the pieces
  `orig/cpnet80s/` already has under different/renamed files
  (`CPNIOS.ASM`, `CPBIOS.ASM`, `SNIOS.ASM`, `XIOSNET.ASM`,
  `NETWRKIF.ASM`) and the user commands as both source and built
  `.CPM` (`LOGIN.ASM`/`.CPM`, `LOGOFF.ASM`/`.CPM`, `CPNETSTS.CPM`,
  `DSKRESET.CPM`, `MAIL.CPM`, `PIP.CPM`, `XSUB.CPM`). It also carries
  **OEM sample slave drivers** for two specific third-party network
  transports — `OMNINIOS.ASM`/`OMNINWIF.ASM` ("Sample Slave Network
  I/O System for CORVUS OMNINET," dated 20 Oct 82) and
  `ULCNIOS.ASM`/`ULCIF.ASM` (a second transport, "ULCN" — not yet
  identified further) — concrete, dated precedent for exactly the kind
  of "OEM supplies the hardware-specific driver, DRI supplies
  everything else" split this repo's own XIOS problem already has
  (see `orig/mpm_II/Control_2/`'s `ldrbios.asm` and `docs/build.md`).
- **`ZSID/`** — **ZSID**, DRI's Z80-targeted symbolic debugger (the
  Z80 counterpart to the 8080 `SID`/MP/M-86's own `DDT86` family):
  `ZSID.CPM` plus `TRACE.UTL`/`HIST.UTL` (trace/history companion
  utilities, same role as `orig/mpm862sr/06`'s `DDT86`-adjacent tools).

Nothing in `orig/driorig/` has been assembled, compiled, or
cross-checked against this repo's own tooling — it's a different set
of DRI *products* (Pascal MT+, PL/I-80, CBASIC/CBASIC-86, CP/NET 1.2,
ZSID), not MP/M-86 source, kept purely as further genuine, dated DRI
reference material from the same era.

## Related DRI source: in-house disk images (`orig/lastdri/`)

A later addition: 25 numbered directories (`01`–`25`), each one disk
image from the collector's own set of DRI in-house/development
diskettes (not factory-labeled releases), per `orig/lastdri/README.TXT`
— the collector's own notes on provenance, transcribed with `22disk`
(which renames CP/M `.COM` files to `.CPM`, explaining the many
`.CPM`-extension binaries throughout) and imaged whole with Teledisk
v2.15. Each directory accordingly holds a `NN.TD0` (the raw Teledisk
disk image) and an `NN.JPG` (a scan of the disk's handwritten label)
alongside whatever files `22disk` could extract — three disks (`17`,
`18`, `21`) extracted almost nothing beyond the image and label scan,
and one disk ("Odd unreadable DRI disk.JPG") was unreadable entirely
and is represented only by its label scan. Per the README, four disks
use VAX-style 128/256-byte mixed sectors (DRI cross-compiled PL/M-86 on
a VAX) and were not transferred file-by-file; disk `14` is serialized
`M86-000-00001`, an actual MP/M-86 V2.0 master disk DRI would have
copied and sent to an OEM.

Unlike `orig/mpm862sr/`'s clean 11-disk distribution set, this is a
grab-bag of unrelated in-house material spanning several DRI products
and projects, in every stage of completion — nothing here has been
diffed against `src-assem/` or validated against this repo's tooling; it's
inventoried, not analyzed:

- **`01`, `02`** — label scan plus disk image only; no files extracted.
- **`03`** — **Digital Research C** for CP/M-86 (`DRC860-862.CMD` —
  three compiler-pass binaries, `SAMPLE.C`, `STARTUP.A86`, the
  `SYSLIB*.L86` runtime libraries, standard headers `CTYPE.H`/`ERRNO.H`/
  `SETJMP.H`/`PORTAB.H`), plus `READ.ME`, a June 1983 release-notes
  file for "Digital Research C ... for the CP/M-86 Family," version
  1.0 — a different DRI product from both MP/M-86 and PL/M-86.
- **`04`** — more of the same C toolchain: `CC1-3.CMD`/`CL.CMD`/
  `CVTH86.CMD`/`LIBR.CMD`/`ARCH.CMD`, `C80LIB.A86`, `CLIB.CL`/`.ARC`,
  `STD.H` — looks like an earlier or parallel build of `03`'s compiler.
- **`05`** — `LFUNC1.C`/`.OBJ`, `LOGO.H`/`LOGOPT.H`, `DRC861.CMD` — more
  Digital Research C material, including what looks like a LOGO
  language runtime piece.
- **`06`** — **WordStar** (`WS.CPM`/`ZWS.CPM`, `WSOVLY1.OVR`,
  `WSMSGS.OVR`) plus assorted unrelated CP/M utilities (`SID.CPM`
  debugger, `DDT.CPM`, `PIP.CPM`, `STAT.CPM`, `FORMAT.CPM`,
  `SYSGEN.CPM`, `COMPARE.CPM`, `ATTACH.CPM`) and a set of `.LIB`
  macro/text libraries (`PLATE.LIB`, `MEMO.LIB`, `SCREEN.LIB`,
  `TITLEPAG.LIB`, `LEV1-3.LIB`, `LITERAL.LIB`, `TOFCCOMS.LIB`,
  `COPYRPAG.LIB`, `VERSIONS.LIB`, `GENFORM.LIB`) that look like a
  document-production/publishing toolset (plates, memos, title pages,
  table-of-contents, cover pages), not development tools — likely
  support material for producing DRI's own printed documentation.
- **`07`, `08`, `13`** — working **MP/M-86** installations: built
  `.MPM` kernel modules (`BDOS`/`RTM`/`SUP`/`MEM`/`CIO`/`XIOS`/
  `SYSDAT`), `MPM.SYS`/`MPMLDR.CMD`, the full `src-assem/utils/`-equivalent
  command set (`DIR`/`TYPE`/`PIP`/`ED`/`STAT`/`SDIR`/`ASM86`/`DDT86`
  etc. as built `.CMD`s), and RSPs (`CLOCK`, `TMP`, `MPMSTAT`,
  `ECHO`) — i.e. the same system this repo's `src-assem/` builds from
  source, here as DRI's own working binaries, plus live operational
  artifacts not present anywhere in `orig/mpm862sr/`: a `MAIL.CMD`/
  `MAIL.NDX`/`.HLP` internal mail system, `HELPNDX.CMD`/`MPM.HLP`
  online help, a `TIMELOG.CMD` family with `.DBF`/`.SUB` data/batch
  files (apparent time-tracking), `WTMP`/`WTMP.DBF` (a login/logoff
  log, same name as the Unix convention), `PASSWD.SAM`/`MPMINIT.SAM`/
  `USERINIT.SAM` (sample config files), and a stray `#DRI#07` file (a
  CP/M "ambiguous/temp file" marker left over from an interrupted
  operation, not meaningful content). `08` additionally has `CPM.SYS`
  and `XIOSN.MPM`, suggesting a dual-boot CP/M-86 configuration, plus
  `DDT86P.CMD`/`RUN.CMD` not seen on `07`/`13`. `13` is a
  utilities-only subset (no kernel modules), evidently a second copy
  of just the command set.
- **`09`** — a **PL/I** toolchain, a third DRI/third-party language
  entirely, distinct from both PL/M-86 and C: `PLI.CPM`/`PLI0-2.OVL`
  (the compiler and its overlays), `PLILIB.IRL`, `LINK.CPM`,
  `RMAC.CPM`, `SID.CPM`, plus PL/I *source* — `SCAN.PLI`, `XLATE.PLI`,
  `MEXP.PLI`/`OLDMEXP.PLI`, `SC.PLI`, `X.PLI`, and, unexpectedly,
  `LISP.PLI`: a Lisp interpreter written in PL/I, with `XLATE.PLI`
  translating M-expressions (Lisp's original 1958 surface syntax) to
  S-expressions — course-project or research-style code, not a DRI
  product.
- **`10`** — a grab-bag tied to the same C/overlay toolchain:
  `WS.CPM`/`WSA.CPM`/overlays again, `NED.CPM` (a different editor),
  `SOFTCOM.CPM`/`PSOFTCOM.CPM`/`SOFTLIBC.REL`, memory-model-specific
  C runtime startups (`CRT0.MED`/`.SML`, `CRT1.A86`/`.B86`,
  `MACHDEP.MED`/`.SML`, `OSCRT.MED`/`.SML` — "medium"/"small" model C
  startup code), `SORT.PLI` (more PL/I), `DRCGSX.A86`/`.OBJ` (Digital
  Research C "GSX" graphics-library linkage), and copies of the same
  `MAIL`/`TIMELOG`/`WTMP` operational files seen on `07`.
- **`11`** — three small standalone `.A86` utilities with no
  surrounding system: `HIST.A86`/`.CMD` ("history"?), `TIMER.A86`,
  `PARSE.A86` — no further context recovered.
- **`12`, `14`, `20`** — three near-identical copies of a from-scratch
  **loader/XIOS development** disk: `LDBIOS.A86`/`.H86`, `ROM.A86`,
  `LDMPM.H86`, `LDBDOS.H86`, `DEBLOCK.LIB`, `SINGLES.LIB`/`.DEF`,
  `SYSTEM.LIB`, alongside a full built MP/M-86 kernel/RSP set — i.e.
  exactly the loader-plus-from-scratch-XIOS problem this repo's own
  `src-assem/loader/LDBIOS86Q.A86`/`ROM86Q.A86` solve for QEMU, except these
  are DRI's *own* original equivalents for whatever real hardware this
  disk set targeted (not yet identified) — likely worth a closer diff
  against `src-assem/loader/` if the loader work stalls.
- **`15`** — `MPM86SRL.A86`/`.ASM`/`.CMD`/`.CPM`: a serialization
  utility for MP/M-86 distribution disks, the direct 8086-port
  counterpart to `orig/mpm_II/OEM/mpm2srl.asm` documented above.
- **`16`** — a from-source MP/M-86 kernel build tied to a specific
  OEM/test configuration: `SUP.A86`/`COMMAND.SUP`, `RTM`-family modules
  (`ABORT.RTM`, `FINDPD.RTM`, `QUE1.RTM`), `SYSDAT.A86`, RSP sources
  (`CLOCK.A86`, `RANDOM.A86`, `SUBMIT.A86`), a `RPLTEST.RSP` (test RSP,
  no equivalent elsewhere in this project), and `TOVAX.SUB`/`ALL.SUB`
  build scripts — i.e. source-level evidence for exactly the kind of
  module-by-module kernel build `orig/mpm862sr/02`–`04` already
  document, on a different (apparently in-progress/test) configuration.
- **`17`, `18`, `21`** — effectively empty: `18` has only a stray
  `TMP.A86`; `17`/`21` extracted nothing but the disk image and label
  scan.
- **`19`** — the fullest from-source MP/M-86 kernel snapshot in this
  entire disk set: nearly every `.A86`/`.DEF`/`.FMT`/`.LIT`-equivalent
  module named in `orig/mpm862sr/02`'s directory-map entry above,
  together in one place with their `.MPM` build outputs and `.SUB`
  build scripts (`ONE.SUB`, `CLEAN.SUB`, `ALL.SUB`) — the closest thing
  in this repository to a single, coherent, buildable snapshot of the
  full MP/M-86 kernel source tree outside of `orig/mpm862sr/` itself;
  worth cross-referencing against `src-assem/kernel/` if any kernel module
  there is ambiguous or incomplete.
- **`22`** — mixed: a `CPIP.CMD`/`SID86.CMD`/`VAX86.CMD`-flavored build
  environment plus stray debug artifacts (`JUNK.*`, `RESUME.PRN`), a
  `CACHE.RSP` (disk cache RSP, no equivalent elsewhere in this repo;
  now baked into the build as `src-assem/rsp/CACHE.RSP` -- docs/bugs.md #124),
  and three `MPMSEM{1,2,3}.TEX` files — internal **"MP/M-86 Seminar
  Notes"** (marked "All Information Herein is Proprietary to Digital
  Research, Inc."), a training/overview document walking through the
  kernel module structure (SUP, RTM, memory layout, SYSDAT) at a
  narrative level — the single best piece of *prose* documentation for
  MP/M-86's own architecture found anywhere in `orig/`, worth a closer
  read against `docs/glossary.md` and `docs/syscalls.md`.
- **`23`** — `FDBIOS.A86`/`.H86`/`.SYM`/`.LST`: a floppy-disk BIOS
  variant build, plus `CPM.SYS` and `VAX86.CMD`.
- **`24`** — a cluster of loader variants for what looks like a
  specific memory/boot configuration: `2MBIOS`/`2MCPM`/`2MLOADER`/
  `2MLDBIOS` ("2M" likely a memory-size or model designator) alongside
  plain `LOADER.CMD`/`MLOADER.CMD`/`MLDBIOS.H86`, `MROM.ODD`/`MROM.EVN`
  (odd/even ROM half images — common for byte-interleaved ROM pairs),
  and `TEST.H86`/`.CMD`.
- **`25`** — unrelated to MP/M-86 entirely: Z80 tools (`Z80.LIB`,
  `STRASM.CMD`, `M5.CMD`), a `TURTLE.M80` LOGO-turtle-graphics module,
  and `Z19LOGO.SYM`/`.CPM` — a LOGO environment targeting the Heath/
  Zenith **Z-19** terminal (its graphics/cursor primitives — `BEEP`,
  `SCROLL`, `SETXY`, `GPLOT`, `GRAFON` — are named in `Z19LOGO.SYM`'s
  symbol table), consistent with `05`'s `LOGO.H`/`LOGOPT.H` and `09`'s
  general interest in alternative-language experiments rather than
  MP/M-86 development proper.

## Related third-party source: CP/M TCP/IP (`orig/cpm-tcp/`)

A final tree, unrelated to Digital Research entirely: a CP/M port of
**Phil Karn's (KA9Q) amateur-radio TCP/IP stack** — the same NET/NOS
code family that became the basis for most early amateur packet-radio
Internet stacks — kept here purely as further period-appropriate 8-bit
systems-programming reference material, not as DRI source and not
needed to build or boot MP/M-86. Per `orig/cpm-tcp/README` (dated
13 Nov 1991 and 05 Nov 1994), the collector no longer remembers exactly
where this came from, notes it may be an inconsistent merge of "two
separate versions," and quotes Karn himself as having "just about
disowned this code" by the time of writing — so provenance here is
explicitly uncertain, by the collector's own admission, unlike every
other tree in `orig/`.

- **Protocol core** — an essentially complete, modular RFC-793/791/768
  implementation in portable (`int16`/`int32`-typed) C, documented in
  its own `tcpguide` ("Amateur Radio Internet Programmer's Guide"):
  `ip.c`/`ip.h`/`iproute.c` (IP plus routing), `icmp.c`/`icmp.h`,
  `tcp.h`/`tcpinput.c`/`tcpoutpu.c`/`tcpsubr.c`/`tcptimer.c`/
  `tcpuser.c` (TCP, split by direction/role), `udp.c`/`udp.h`,
  `mbuf.c`/`mbuf.h` (BSD-style packet buffer chains), `timer.c`/
  `timer.h`, `netuser.c`/`netuser.h` (the session-layer API
  applications call), and `slip.c` (SLIP framing for serial links).
- **Applications** atop that core: `ftp.c`/`sftp.c` (FTP server/simple
  client), `smtp.c`, `telnet.c`/`telnetm.c`/`telnet.h`, `server.c`, and
  `ttydriv.c`.
- **Hardware-specific glue**, explicitly for the **Xerox 820**
  (a Z80 CP/M machine) per `820.h`'s own header comment (copyright
  1986, Philip Karn): `820.c`/`820.h`/`820ints.asm` (Z80-SIO-chip
  serial driver and interrupt handlers), `machdep.c`/`machdep.h`
  (the machine-dependent shim the portable core calls into),
  `csum.asm`/`inout.asm` (hand-written Z80 assembly for the checksum
  inner loop and port I/O — the only non-C source outside `AX25/`),
  and `debug.c`/`fragtest.c`/`tcptest.c` (test harnesses). `make.sub`/
  `make1.sub` (Z80-targeted `cz`/`as` Submit build scripts) and
  `objects.tst` list the resulting object set.
- **`AX25/`** — a separate, self-contained program: per its own
  `readme.doc` ("the second 'friendly user' release of my TNC code for
  the Xerox 820"), this is Karn's **AX.25 packet-radio TNC**
  (terminal-node-controller) firmware, not part of the TCP/IP stack
  above — a different protocol (AX.25, not IP) built with the Aztec C
  compiler, supporting either the Xerox 820's onboard Z80-SIO
  (`820.c`/`.h`) or an add-on "FADCA 8530 daughter board"
  (`fad.c`/`fad.h`) by swapping which driver is compiled in; also
  includes its own `line.c`/`term.c`/`command.c`/`cmdtab.c`/`cmdrout.c`
  (a command-line/terminal interface), `link.c`/`control.c`/`addr.c`/
  `hash.c`/`queue.c` (AX.25 link-layer state), and `820ints.asm`/
  `inout.asm` (its own copies of the Z80 interrupt/IO assembly, not
  shared with the parent directory's copies). `objects.sio`/
  `objects.fad` are its two build variants' object lists;
  `descript.doc`/`usage.doc` are further prose documentation.

Nothing in `orig/cpm-tcp/` has been built, assembled, or cross-checked
against anything else in this repository — it targets a different CPU
generation (Z80, not 8086) and a different problem domain (network
protocols, not an operating-system kernel) from everything else here.

## Reference manuals (`orig/manuals/`)

Scanned PDF copies of DRI's own published manuals, not source — kept
as the authoritative reference for behavior/usage questions the source
alone doesn't answer (command syntax, option meanings, intended
workflow):

- **`MPM86_userGuide_Sep81.pdf`** — the **MP/M-86 User's Guide** (Sep
  81), the direct end-user documentation counterpart to this
  repository's own main subject and `docs/utilities.md`.
- **`MPM86_pgmr_Sep81.pdf`** — the companion **MP/M-86 Programmer's
  Guide** (Sep 81, 364 pages) — this is the file formerly listed here
  as the unidentified `Unconfirmed 414507.crdownload` leftover; it's
  since been identified and renamed.
- **`CPM-86_Users_Guide.pdf`**, **`CPM-86_System_Guide.pdf`**,
  **`CPM-86_Programmers_Guide.pdf`**, **`CPM-86_Command_Summary.pdf`**
  — the CP/M-86 manual set (CP/M-86 being MP/M-86's single-user sibling
  OS, sharing much of the same command/utility surface — see the
  `08`/`22` `CPM.SYS` dual-boot configurations noted under
  `orig/lastdri/` above).
- **`Intro_to_CPM_Feat_and_Facilities.pdf`** — a general CP/M-family
  features/facilities overview, broader than the CP/M-86-specific
  guides above.
- **`ASM.pdf`**, **`MAC_CPM_Macro_Assembler.pdf`** — manuals for DRI's
  8080 `ASM` and Z80 `MAC` assemblers, the direct documentation for the
  tools `orig/mpm_II/Control_2/RMAC.COM` and `orig/asm80/`'s
  disassembled `ASM.COM` implement.
- **`DDT.pdf`**, **`SID_ZSID.pdf`** — manuals for DRI's debuggers:
  `DDT` (ancestor of `DDT86`/`DDT86.CMD` built throughout
  `orig/mpm862sr/06` and `orig/lastdri/`) and `SID`/`ZSID` (the 8080
  and Z80 symbolic debuggers — `ZSID` itself is in `orig/driorig/ZSID/`
  above).
- **`ED.pdf`** — the `ED` line editor manual (`src-assem/utils/ED.PLM`).
- **`Despool_Print_Utility_UG.pdf`** — manual for `DESPOOL`, a
  print-spool utility with no source copy currently in this
  repository's `orig/` trees.
- **`CBASIC_1982.pdf`** — the CBASIC language manual, covering both the
  8080 `CB86` compiler (`orig/driorig/CB86-Master_Control/`) and
  CBASIC-86 (`orig/driorig/CBASIC86-719/`, `-720/`).

Every PDF in `orig/manuals/` now has a sibling `.md` of the same name,
to make these faster for an LLM to search/read than paging through a
scanned PDF — but they're not all extracted the same way, because only
one of these PDFs actually has an embedded text layer:

- **`CBASIC_1982.md`** — `CBASIC_1982.pdf` is the one manual here with
  a genuine embedded text layer (confirmed via `pdffonts`/
  `pdftotext`), so this is a lossless `pdftotext -layout` extraction,
  further restructured into real Markdown headings (one per
  Section/Appendix, numbered subsection, and — the bulk of it — one
  per statement/function reference entry), with stray page numbers and
  the original dot-leader Table of Contents stripped.
- **Every other `.md` here is raw OCR**, produced with `pdftoppm`
  (300dpi grayscale page rendering) piped through `tesseract`, since
  `pdftotext`/`pdffonts` return nothing for any of these — confirmed
  scanned images with no text layer, even on `MPM86_pgmr_Sep81.pdf`'s
  364 pages. Each one is only lightly cleaned (stray page-number lines
  dropped, literal `*`/leading `#` escaped so Markdown doesn't
  misinterpret them) and kept as a flat, per-`_Page N_`-marker dump —
  **no heading structure has been reconstructed**, unlike
  `CBASIC_1982.md`, because OCR'ing 1980s dot-matrix/typewriter scans
  already risks misread characters, garbled logos/graphics, and lost
  table layout; inventing headings on top of that text without
  checking it first would just compound the risk of confidently wrong
  structure. Treat these as a grep-able first pass, not a verified
  transcription — cross-check anything that matters against the
  source PDF.

## Notes for anyone working in this tree

- File extensions are not a fully reliable guide to text-vs-binary content
  (see `.CMD`/`.CPM` above) — inspect before assuming.
- No files have been modified from the original distribution; this is a
  reference/archival import (`git log` shows a single `orig` commit).
- macOS `.DS_Store` artifacts are not part of the original source and are
  excluded via `.gitignore`.
