# MP/M-86 2.x Original Source (`orig/`)

This repository contains the historical source distribution of **MP/M-86 II**
(version 2.0), Digital Research's multi-user, multi-tasking operating system
for the Intel 8086/8088, as released circa 1981. The tree lives at
`orig/mpm862sr/` and is organized as eleven numbered directories (`01`–`11`),
matching the original distribution disk layout. There is no top-level
README; the notes below are reconstructed from file names, extensions, and
in-source comments.

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

## Notes for anyone working in this tree

- File extensions are not a fully reliable guide to text-vs-binary content
  (see `.CMD`/`.CPM` above) — inspect before assuming.
- No files have been modified from the original distribution; this is a
  reference/archival import (`git log` shows a single `orig` commit).
- macOS `.DS_Store` artifacts are not part of the original source and are
  excluded via `.gitignore`.
