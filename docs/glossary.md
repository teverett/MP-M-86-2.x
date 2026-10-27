# Glossary

Short definitions of the core Digital Research OS terms used throughout
this repository and its docs (`docs/orig.md`, `docs/build.md`,
`docs/filetypes.md`). See those files for how each piece maps onto the
actual directories and files in `orig/mpm862sr/`.

## DRI

**D**igital **R**esearch, **I**ncorporated — the company (founded by
Gary Kildall) that wrote CP/M, MP/M-86, DDT86, and every other piece of
software in `orig/mpm862sr/`. "DRI's own ..." elsewhere in these docs
just means "the original 1981 vendor's," as opposed to anything built
by this repo's own tools.

## CP/M

Digital Research's single-user, single-tasking operating system for
8080/Z80 (and later 8086, as "CP/M-86") microcomputers, first released in
1974. It's the direct ancestor of MP/M: the same basic model of a small
resident OS plus a disk-based command processor, but built for one user
and one running program at a time. MP/M-86 reuses large parts of CP/M's
design (BDOS, the console command loop) but adds multi-user,
multi-process scheduling on top.

## MP/M-86

**M**ulti-**P**rogramming **M**onitor for the 8086 — Digital Research's
multi-user, multi-tasking operating system, the subject of this entire
repository (`orig/mpm862sr/`, version 2.x). Where CP/M runs one program
for one user, MP/M-86 runs many processes (interactively from several
terminals, or as background "Resident System Processes" — see `.RSP` in
`docs/filetypes.md`) concurrently, scheduled by its Real-Time Monitor
(RTM). It's assembled from several cooperating kernel modules — RTM,
Supervisor (`.SUP`), memory manager (`.MEM`), character I/O (`.CIO`),
BDOS (`.BDO`), and the hardware-specific XIOS — combined by DRI's loader
into a single bootable image, `MPM.SYS`. See `docs/orig.md` for the
directory-by-directory breakdown and `docs/build.md` for the build
pipeline that produces it.

## RTM

**R**eal-**T**ime **M**onitor — the process scheduler/dispatcher at the
core of MP/M-86's multi-tasking (`src-assem/kernel/RTM.A86`, the `.RTM`
fragments — see `docs/filetypes.md`). It's the piece of the kernel that
makes MP/M-86 multi-*programming* rather than just multi-*user*: it
decides which ready process gets the CPU next, handles process
priorities and time-slicing, and dispatches control between processes
(including the background RSPs). Where the Supervisor (see CCP, above)
decides *what* a typed command or a system call should do, the RTM
decides *when* each process actually gets to run.

## RSP

**R**esident **S**ystem **P**rocess — an MP/M-86 background process,
loaded into memory alongside the kernel at boot and scheduled by the
RTM just like any interactive user's process, but with no console of
its own (e.g. `CLOCK.RSP`, `ECHO.RSP`, `TMP.RSP` — see the `.RSP` entry
in `docs/filetypes.md`). Source for one lives in `src-assem/rsp/` as a
`.A86` file; building it produces the loadable `.RSP` image. This is
the main thing that makes MP/M-86 "multi-programming" rather than just
"multi-user": several of these can be running continuously, unattended,
at the same time as every logged-in user's own foreground process.

## BDOS

**B**asic **D**isk **O**perating **S**ystem — the file-system layer,
inherited in concept from CP/M. It implements the actual system calls
programs make to open/read/write/close files, manage directories, and
(in MP/M-86's multi-user extension) handle per-user file areas and
record locking. In this source tree it's split across several `.BDO`
fragments (`BDOSIF.BDO`, `DATA.BDO`, `FILE1.BDO`–`FILE4.BDO`, etc. — see
`docs/filetypes.md`) that get assembled and combined into one BDOS
module of the kernel image. Application programs reach it (and the rest
of the kernel) through a single software interrupt gate — see
`tools/plm/runtime_stub.A86` and its "Calling convention" notes in
`tools/plm/README.md` for exactly how a PL/M-86 utility performs a BDOS
call on MP/M-86.

## CCP

**C**onsole **C**ommand **P**rocessor — CP/M's resident command-line
interpreter: the part that prints the prompt, reads a typed command
line, and either runs a built-in command (like `DIR` or `ERA`) or loads
and runs a `.COM` file. MP/M-86 doesn't have a literal "CCP" module by
that name — its console/command dispatching role is absorbed into the
**Supervisor** (`.SUP`, see `docs/filetypes.md`), which parses typed
input and routes it to the right process/system call for a multi-user
system. The term is included here because it's the CP/M concept that
MP/M-86's Supervisor is the direct descendant of, and because some of
the command utilities built in this repo (`src-assem/utils/*.PLM` — `DIR`,
`ERA`, `TYPE`, etc.) are the same kind of program a CCP would launch.

## XIOS

e**X**tended **I**nput/**O**utput **S**ystem — the one kernel module
that is genuinely hardware-specific, analogous to CP/M's BIOS. It
implements the low-level device drivers (disk controller, console UART,
timer interrupt) for one particular machine. The copy in this tree
(`orig/mpm862sr/.../XIOS.A86`, `ROM.A86`) targets the Intel iSBC 86/12
single-board computer with an SBC 204 disk controller — see
`docs/build.md`'s note on why this specific XIOS can't be used to boot
MP/M-86 under QEMU without a replacement written for whatever disk/console
hardware QEMU emulates. Every other kernel module (RTM, Supervisor, BDOS,
memory manager, character I/O) is portable 8086 code that calls down
into the XIOS rather than touching hardware directly.

The term itself isn't an MP/M-86 invention: `orig/mpm_II/Control_2/
resxios.asm` ("Sample Resident XIOS for MP/M II," per its own disk's
README) shows DRI already using "XIOS" for the exact same role one CPU
generation earlier, for MP/M II's own 8080/Z80 systems — see
`docs/orig.md`'s "Related DRI source: MP/M II" section.

A new, from-scratch replacement targeting QEMU (serial console + floppy
disk) now exists at `src-assem/kernel/XIOS86Q.A86` — it assembles cleanly and
implements the real kernel's exact calling contract, but has never
actually run anywhere; see `docs/build.md` for its current status and
the file's own header comment for what it does and doesn't cover.

## DDT86

**D**ynamic **D**ebugging **T**ool for the 8086 — Digital Research's
interactive machine-code debugger/monitor for CP/M-86 and MP/M-86,
descended from CP/M-80's `DDT`. It lets a user load a program, set
breakpoints, single-step, and examine/patch memory and registers. It's
mentioned here specifically because `src-assem/toolchain/ASS86.PLM` (the
assembler) declares several `EXTERNAL` procedures — `ddtset`,
`ddtgetline`, `goddt`, `printm`, `printword`, `conin`, `conout` — that
are calls *into* a running DDT86, not into the BDOS/MP-M system-call
interface that `tools/plm/runtime_stub.A86` implements. No real DDT86
implementation has been found in this tree, so those externals remain
unresolved in `tools/plmc`-built output — see `tools/plm/runtime_stub.A86`'s
own header comment and `tools/plm/README.md`'s "Known limitations" for
the current state of that gap.

## CP/NET

Digital Research's network/workstation-sharing layer for CP/M-family
systems — lets a "slave" workstation (often diskless, or disk-light)
log in over a link to a "master" machine and use its disks, printer,
etc., a 1980s precursor to LAN-style file/printer sharing. It's a
**separate DRI product from MP/M-86**, not a part of it or a dependency
of it, just built by the same company in the same era; the two share no
code. This tree has two CP/NET source trees for reference:
`orig/cpnet80s/` (the original 8080/Z80 version, 1982, in DRI's MAC
assembler dialect — a different CPU and dialect from everything
`tools/asm86/` targets) and `orig/cpn86src/` (the 8086 port, CP/NET-86,
1983, written in real ASM86 — the one dialect match with this repo's own
tooling, though not yet validated against it). See `docs/orig.md`'s
"Related DRI source: CP/NET" section for the full file-by-file
breakdown.

## SDIR

An "extended directory" console command from **MP/M II** (Digital
Research's 8080/Z80 multi-programming monitor, the direct ancestor
MP/M-86 2.x was ported from — see `docs/orig.md`'s "Related DRI source:
MP/M II" section), with no equivalent utility anywhere in this repo's
own `src-assem/utils/` — where plain `DIR.PLM` just lists filenames, SDIR adds
sorting, searching, printing, and file time-stamp display. Found in
`orig/mpm_II/CUSPs_5/`, split one module per file (all dated 14 Sept
1981, by Danny Horovitz):

- **`dm.plm`** — `$title('Super Directory Command')`, program name
  `sdir`: the entry point/main module.
- **`sn.plm`** — the command-line scanner (parses the typed `SDIR`
  command line).
- **`da.plm`** — the arithmetic module.
- **`dse.plm`** — the search module (matching files to the command's
  arguments).
- **`dso.plm`** — the sort module.
- **`dsh.plm`** — the show/display module.
- **`dp.plm`** — the print module.
- **`dts.plm`** — the time-stamp display module — its own header notes
  the date/time-conversion code comes "From MP/M 1.1 TOD program,"
  i.e. an even earlier ancestor of this repo's own `src-assem/utils/TOD.PLM`.

`sdir.prl`/`xsdir.prl` (`.PRL` = MP/M II's Page Relocatable binary
format — see `docs/orig.md`) are two pre-built compiled forms of the
same program, not further source.
