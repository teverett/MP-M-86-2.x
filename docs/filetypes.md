# File Type Index

A quick lookup for every file extension used in this repository — in the
original MP/M-86 2.x distribution (`orig/mpm862sr/`), in the reference
material (`orig/plm80s/`, `orig/newplm/`, `orig/asm80/`), in the related
DRI source trees (`orig/mpm_II/`, `orig/cpn86src/`, `orig/cpnet80s/`),
and produced by this repo's own tools (`tools/asm86/`, `tools/plm/`).
See `docs/orig.md` for the full directory-by-directory breakdown and
`docs/build.md` for how these fit together in the build pipeline.

## Source languages

| Ext | Meaning |
|---|---|
| **`.A86`** | **ASM86** source — Intel 8086 assembly, the dialect DRI's `ASM86` cross-assembler compiles and this repo's `tools/asm86/` reimplements. The kernel, drivers, and loader are written in it. |
| **`.PLM`** | **PL/M-86** source — Digital Research's systems-programming language (a PL/I descendant), used for most command-line utilities and the compiler itself. `tools/plm/` compiles this dialect. |
| **`.ASM`** | Generic 8080/Z80/8086 assembly, *not* specifically the `ASM86`/`A86` dialect — small linkage stubs (e.g. `11/C86LNK.ASM`), the 8080 source form of DRI's original CP/M `ASM.COM` in `orig/asm80/` (a third-party disassembly), or the real original 8080 `ASM.COM`/`DDT` source and the rest of MP/M II's own `.ASM` modules in `orig/mpm_II/` (genuine DRI source, not a disassembly — see `docs/orig.md`). |
| **`.Z80`** | The same source hand-retargeted to Zilog Z80 opcodes (only in `orig/asm80/`, a reference tree — not part of MP/M-86). |
| **`.FOR`** | FORTRAN IV — Intel's original PL/M-80 compiler source (`orig/plm80s/PLM81.FOR`, `PLM82.FOR`, a reference tree). |

## Include / header files

| Ext | Meaning |
|---|---|
| **`.DEF`** | ASM86 "definitions" — shared constant and structure-layout includes (control-block field offsets, error codes, etc.), the assembly equivalent of a C header. |
| **`.LIT`** | PL/M "literally" includes — shared `DECLARE ... LITERALLY '...'` macro/constant definitions (see `tools/plm/README.md` for how much real macro-expansion semantics this actually requires). |
| **`.EXT`** | PL/M "external" declaration headers — `DECLARE ... EXTERNAL` forward declarations for cross-module linking, the PL/M analogue of a C header's `extern` declarations. |
| **`.LIB`** | Shared library/data-layout include files (assembly), similar in spirit to `.DEF` but for library-wide structures (e.g. `SYSDAT.LIB`, `DEBLOCK.LIB`). Same convention in `orig/mpm_II/Control_2/` (`i8085.lib`, `z80.lib`, `diskdef.lib`, etc. — see `docs/orig.md`). |
| **`.FMT`** | Binary record/structure "format" includes — memory-resident data-structure layout definitions (e.g. `UDA.FMT`, `SYSDAT.FMT`). |
| **`.X86`** | ASM86 linkage/include fragments specifically for the PL/M-86 compiler's own assembly-coded helper routines (directory `11`). |
| **`.PLB`** | PL/M "literal"/token include files used by directory `08`'s command-table code-generation tools (`GENTAB`, `GENDEF`, etc.) — a one-off naming convention, not seen elsewhere. |
| **`.PLI`** | A single file (`06/GENTAB.PLI`) with PL/I-flavored syntax, distinct from the `.PLM`-dialect PL/M-86 used everywhere else — likely an older or borrowed table-generator source. |

## Kernel module source (split by subsystem)

Each is a fragment of the single-memory-image MP/M-86 kernel, combined
by the loader into `MPM.SYS`:

| Ext | Meaning |
|---|---|
| **`.RTM`** | **R**eal-**T**ime **M**onitor source — the process scheduler/dispatcher module. |
| **`.SUP`** | **Sup**ervisor source — the command-line/syscall-dispatch module (parses console input, routes system calls). |
| **`.MEM`** | Memory-manager source — the module that tracks and allocates memory partitions. |
| **`.CIO`** | Character I/O source — the console/char-device driver module. |
| **`.BDO`** | **BDO**S source fragments — the Basic Disk Operating System (file system) module, split across several files (`BDOSIF.BDO`, `DATA.BDO`, `FILE1-4.BDO`, etc.). |

## Build artifacts (binary *or* intermediate — check contents, not just the extension)

| Ext | Meaning |
|---|---|
| **`.MPM`** | An assembled kernel module (`BDOS`/`RTM`/`SUP`/`MEM`/`CIO`/`XIOS`/`SYSDAT`), renamed from the assembler's raw output, ready to be combined into `MPM.SYS`. |
| **`.RSP`** | **R**esident **S**ystem **P**rocess — a built MP/M-86 background process image loaded alongside the kernel (e.g. `CLOCK.RSP`, `ECHO.RSP`, `TMP.RSP`). An RSP is source-equivalent to the `.A86`/`.PLM` program that built it; the extension marks the *compiled, loadable* form. |
| **`.CMD`** | A loadable MP/M-86/CP/M-86 command/program binary (roughly analogous to CP/M-80's `.COM`) — **or**, confusingly, a plain-text Submit-style build script. The extension alone doesn't tell you which; DRI reused it for both (see `docs/orig.md`'s "Toolchain and languages" section). |
| **`.CPM`** | Same ambiguity as `.CMD`: sometimes a compiled tool binary (`A86.CPM`, `GENCMD.CPM`, `XREF86.CPM`), sometimes a plain-text build/Submit script (`MPMSTAT.CPM`). Always check contents. |
| **`.H86`** | The ASM86 assembler's own intermediate output for a module — consumed by the loader/`GENCMD` in the next build stage. |
| **`.DAT`** | Raw binary data (e.g. opcode tables `OP86.DAT`/`OPTAB.DAT`, or a system data image `SYSTEM.DAT`). |
| **`.COD`** | Binary patch data (`PATCH.COD`) — small binary fixups applied post-assembly. |
| **`.SYM`** | Assembler-generated symbol table (produced alongside a `.H86`/`.CMD`; referenced by build scripts' cleanup steps, e.g. `02/CLEAN.SUB`'s `ERA *.SYM`, though none are checked into this tree — a build byproduct, not a distributed file). |
| **`.XRF`** | Cross-reference listing from the `XREF86` tool (same situation as `.SYM` — a build byproduct referenced by cleanup scripts, not present in the tree itself). |
| **`.LST`** | Assembler *listing* output (source interleaved with generated code/addresses) — likewise a build byproduct that cleanup scripts erase (`ERA *.LST`) rather than something kept in source control. |
| **`.HEX`** | Intel-hex object output from an assembler/compiler (e.g. `orig/plm80s/PLMSAMP.HEX`) — the traditional intermediate between assembling/compiling and `GENCMD`-style conversion to a loadable binary. |
| **`.PRN`** | A compiler/assembler *printer listing* (`orig/plm80s/PLMSAMP.PRN`) — similar in spirit to `.LST` but from the PL/M side of the toolchain. |
| **`.BAK`** | A generic CP/M-convention backup copy of a file, made automatically by some editors before overwriting the original (referenced by cleanup scripts; not present in this tree). |

## MP/M II (`orig/mpm_II/`) native formats — reference only, not part of MP/M-86

MP/M II is Digital Research's 8080/Z80 predecessor — the direct
ancestor MP/M-86 2.x was ported from (see `docs/orig.md`'s "Related DRI
source: MP/M II"). None of these formats are used anywhere in
`orig/mpm862sr/` or `src/`; they're kept here only because they're the
historical ancestors of several conventions already in this table.

| Ext | Meaning |
|---|---|
| **`.COM`** | MP/M II's (and plain CP/M-80's) native loadable command binary — the direct ancestor of CP/M-86/MP/M-86's own `.CMD`, already noted above. `orig/mpm_II/` still has real `.COM` binaries throughout (`ddt.com`, `gensys.com`, `mpmldr.com`, every compiled `CUSPs_3/` tool, etc.). |
| **`.PRL`** | "Page Relocatable" — MP/M II's compiled, load-time-relocatable binary format for a CUSP or system-generation tool (e.g. `xdir.prl`, `asm.prl`, `sdir.prl`), produced by `GENMOD` (`CUSPs_3/genmod.asm`/`.com`) from a linked `.HEX` intermediate. The MP/M II analogue of this repo's own build producing a loadable module from `tools/asm86`/`tools/plmc` output. |
| **`.SPR`** | A `LINK`ed (not yet page-relocated) binary for a core kernel module — `bnkbdos.spr`/`resbdos.spr` (BDOS), `bnkxdos.spr`/`xdos.spr` (XDOS/nucleus), `tmp.spr` (`nucleus/tmpsub.asm`'s RSP half) — see e.g. `orig/mpm_II/nucleus/xdos.sub`'s own `link xdos.spr=...` build step. Roughly MP/M II's equivalent of this tree's own `.H86` intermediate. |
| **`.RSP`**/**`.BRS`** | MP/M II's compiled Resident System Process, split the same resident/banked way as `.SPR` above (per `orig/mpm_II/RSPs/brspbi.asm`'s own title, `"Banked Resident System Process BDOS Interface"` — confirming what `.BRS` stands for): `.RSP` is the resident half, `.BRS` the banked half, both produced by `GENMOD` from a `.HEX` intermediate (e.g. `genmod $1brs.hex $1brs.brs` in `brsgen.sub`). `.RSP` is the direct ancestor of this repo's own `.RSP` convention above, just for a different CPU and build pipeline — though this repo's own `.RSP` is a single undivided image, matching how MP/M II's own undivided `ABORT` RSP skips the resident/banked split (and the `GENMOD` step) entirely. |

## CP/NET (`orig/cpn86src/`, `orig/cpnet80s/`) — reference only, not part of MP/M-86

CP/NET is a **separate DRI product** from MP/M-86 (see `docs/orig.md`'s
"Related DRI source: CP/NET" and `docs/glossary.md`'s "CP/NET" entry),
not a dependency of it — kept here purely for reference. Its two
directories each use exactly one, different extension, and neither
dialect is interchangeable with the other:

| Ext | Meaning |
|---|---|
| **`.MAC`** | Only in `orig/cpnet80s/` — DRI's MAC/M80-style 8080/Z80 macro-assembler source (`CPNETLDR.MAC`, `NDOS.MAC`, `SNIOS.MAC`, etc., the original 1982 CP/NET for 8080/Z80). A different CPU *and* a different assembler dialect from everything `tools/asm86/` targets — not assemblable with this repo's tools. |
| **`.A86`** | Only in `orig/cpn86src/` — real ASM86 source (`CCPN.A86`, `NDOS.A86`, `SNIOS.A86`, etc., the 1983 CP/NET-86 port), the same dialect as `src/`'s own `.A86` files and already covered under "Source languages" above. Most of these files use MASM-style `SEGMENT`/`ASSUME` (and `NDOS.A86`/`NDOSO.A86` also `MACRO`), which `tools/asm86/` doesn't implement — see `docs/orig.md` for exactly which files that affects. |

## Build scripts / documentation

| Ext | Meaning |
|---|---|
| **`.SUB`** | A **Submit** file — a CP/M/MP/M batch script (`SUBMIT.COM`/`SUBMIT.A86` interprets these), the rough equivalent of a shell script or `.BAT` file. Drives most of the build process (`MKALL.SUB`, `MKBDOS.SUB`, `BACKUP*.SUB`, etc.). |
| **`.TEX`** | Plain **text** documentation (not LaTeX) — only two exist in the whole DRI tree: `07/MPM86GEN.TEX` (the build-process writeup) and `11/BNF.TEX` (the PL/M-86 expression grammar). |
| **`.DOC`** | Plain-text documentation in the reference trees (`orig/plm80s/PLMLANG.DOC`, `PLMCOMP.DOC`; `orig/newplm/.../CAVEATS.DOC`) — same idea as `.TEX`, just a different era/source's naming convention. |
| **`.ME`** | A `READ.ME`-style plain-text readme (`orig/asm80/ASMREAD.ME`). |

## This repo's own tool output (`tools/`)

Not part of the original distribution — generated by this repo's own
`asm86`/`plmc` when you run them (see `tools/README.md` and
`tools/plm/README.md`):

| Ext | Meaning |
|---|---|
| **`.cseg.bin` / `.dseg.bin` / `.aseg.bin`** | Raw per-segment binary output from `tools/asm86.py` — the assembled code/data segment's bytes, not Intel-hex, no header. |
| **`.gen.A86`** | Default output filename pattern from `tools/plmc.py` when compiling a `.PLM` file — ASM86-dialect text, meant to be fed straight into `tools/asm86.py`. |

## Everyday / non-source files

| Ext | Meaning |
|---|---|
| **`.ZIP`** | Archives (`orig/newplm/PLM.ZIP`, `PLM2C.ZIP`) — third-party reference tools, extracted alongside the archive itself. |
| **`.DS_Store`** | macOS Finder metadata — not part of any source, excluded via `.gitignore`. |
