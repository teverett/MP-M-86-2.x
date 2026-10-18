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
core of MP/M-86's multi-tasking (`src/kernel/RTM.A86`, the `.RTM`
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
in `docs/filetypes.md`). Source for one lives in `src/rsp/` as a
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
the command utilities built in this repo (`src/utils/*.PLM` — `DIR`,
`ERA`, `TYPE`, etc.) are the same kind of program a CCP would launch.

## XIOS

**X**-shaped (extended) **I**nput/**O**utput **S**ystem — the one kernel
module that is genuinely hardware-specific, analogous to CP/M's BIOS.
It implements the low-level device drivers (disk controller, console
UART, timer interrupt) for one particular machine. The copy in this tree
(`orig/mpm862sr/.../XIOS.A86`, `ROM.A86`) targets the Intel iSBC 86/12
single-board computer with an SBC 204 disk controller — see
`docs/build.md`'s note on why this specific XIOS can't be used to boot
MP/M-86 under QEMU without a replacement written for whatever disk/console
hardware QEMU emulates. Every other kernel module (RTM, Supervisor, BDOS,
memory manager, character I/O) is portable 8086 code that calls down
into the XIOS rather than touching hardware directly.

## DDT86

**D**ynamic **D**ebugging **T**ool for the 8086 — Digital Research's
interactive machine-code debugger/monitor for CP/M-86 and MP/M-86,
descended from CP/M-80's `DDT`. It lets a user load a program, set
breakpoints, single-step, and examine/patch memory and registers. It's
mentioned here specifically because `src/toolchain/ASS86.PLM` (the
assembler) declares several `EXTERNAL` procedures — `ddtset`,
`ddtgetline`, `goddt`, `printm`, `printword`, `conin`, `conout` — that
are calls *into* a running DDT86, not into the BDOS/MP-M system-call
interface that `tools/plm/runtime_stub.A86` implements. No real DDT86
implementation has been found in this tree, so those externals remain
unresolved in `tools/plmc`-built output — see `tools/plm/runtime_stub.A86`'s
own header comment and `tools/plm/README.md`'s "Known limitations" for
the current state of that gap.
