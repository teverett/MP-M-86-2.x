# MP/M-86 2.x

Historical source code for Digital Research's **MP/M-86 2.x** operating
system. See [docs/orig.md](docs/orig.md) for a full breakdown of the source
tree.

## Directory structure

```
orig/           The original 11-disk source tree, byte-for-byte as
                downloaded (see docs/orig.md for what's on each disk).
                Never patched -- src-assem/ is where any fixes happen.
src-assem/      The same source, organized by what it builds:
  kernel/         MP/M-86 kernel modules (BDOS, RTM, SUP, MEM, CIO,
                  XIOS, SYSDAT) plus XIOS86Q.A86, a from-scratch
                  replacement XIOS for QEMU.
  loader/         The boot loader (LDMPM/LDBDOS/LDBIOS/ROM) plus
                  LDBIOS86Q.A86/ROM86Q.A86, from-scratch QEMU
                  replacements for LDBIOS/ROM.
  rsp/            Resident System Processes (CLOCK, ECHO, TMP, SUBMIT).
  toolchain/      PL/M sources for DRI's own dev tools (ASS86, DIS86,
                  INS86).
  utils/          PL/M sources for user-facing utilities (PIP, ED,
                  DIR, TYPE, etc.).
  Makefile        Build everything with `make -C src-assem` (or `cd
                  src-assem && make`); see its own header comment for
                  all targets (`make disk`, `make run`, ...).
src-c/          A C port of src-assem/ (see docs/src-c.md) that builds
                and boots its own MP/M-86 floppy, with nothing from
                src-assem/:
  kernel/         The kernel modules, routine by routine.
  loader/         The loader.
  rsp/            The RSPs (TMP, CLOCK, ECHO, MPMSTAT, CACHE) and
                  transients (SUBMIT, RANDOM), as C programs.
  utils/          All of src-assem/utils/'s PL/M utilities (PIP, ED,
                  SET, SHOW, DIR, ...), as C programs.
  rt/             The runtime that runs the kernel as MP/M in 32-bit
                  protected mode.
  gensys/         System generation: the low-memory image.
  boot/           The boot sector and the kernel's entry.
  tools/          The disk builder (mkdisk.py, bootsect.py).
  Makefile        `make -C src-c run` builds the floppy and boots it;
                  `make -C src-c test` runs tests/src-c/.
tests/          The differential and boot tests (tests/src-c/) and the
                PL/M compiler's (tests/plm/).
tools/          This project's own from-scratch build tooling (none of
                it is DRI source):
  asm86/          An ASM86-dialect assembler, since none survived.
  asm86.py        CLI entry point for the assembler.
  plmc.py         A small PL/M-86 compiler.
  plm/            Supporting PL/M runtime stub/docs.
  build_mpmldr.py   Combines LDMPM+LDBDOS+LDBIOS86Q into MPMLDR.bin.
  build_mpmsys.py   Combines the kernel modules into MPM.bin.
  build_bootsect.py Builds the ROM86Q.A86 boot sector.
  build_disk.py     Combines everything above into one bootable
                    QEMU floppy image.
docs/           Documentation -- see "Documentation" below.
src-assem/build/  src-assem's build output (gitignored, created by
                `make -C src-assem`).
src-c/build/    src-c's build output, its floppy (mpm86c.img)
                included (gitignored, created by `make -C src-c`).
```

## Building and booting

Two builds, each with its own bootable QEMU floppy:

- **src-assem** (the original assembly, built with tools/):
  `./run.sh`, or `make -C src-assem disk run`. The floppy is
  `src-assem/build/disk/mpm86q.img`.
- **src-c** (the C port, 32-bit, built with i686-elf-gcc):
  `make -C src-c run`. The floppy is `src-c/build/mpm86c.img`.

Both put MP/M's console on the terminal (QEMU's serial port; Ctrl-A X
quits). They need `qemu-system-i386`; src-c also needs the i686-elf
cross toolchain (`i686-elf-gcc`, binutils). See docs/src-c.md, "Building
and booting src-c".

## Documentation

- [docs/orig.md](docs/orig.md) — what's in the original 11-disk source
  tree, disk by disk, and how it maps to `src-assem/`.
- [docs/filetypes.md](docs/filetypes.md) — the file-extension
  conventions this source uses (`.A86`, `.PLM`, `.DEF`, `.FMT`, etc.)
  and what each one means.
- [docs/glossary.md](docs/glossary.md) — terms, module names, and
  abbreviations used throughout the source and these docs.
- [docs/bugs.md](docs/bugs.md) — genuine defects found in the original
  1981 source (and a couple of real bugs found in this project's own
  tooling/new code), what's been patched and what hasn't, with
  evidence for each.
- [docs/build.md](docs/build.md) — what it takes to actually build and
  boot this source, piece by piece, and the current status of each
  piece (what's done, what's new/from-scratch, what's still missing).
- [docs/diskformat.md](docs/diskformat.md) — the exact byte-for-byte
  layout of the bootable QEMU floppy image `make disk` produces.
- [docs/utilities.md](docs/utilities.md) — the 19 user-facing
  `src-assem/utils/` commands (`PIP`, `ED`, `DIR`, `TYPE`, etc.), what each
  one does, and how they're built and placed on disk.
- [docs/syscalls.md](docs/syscalls.md) — the MP/M-86 kernel's and the
  loader's own system-call tables: every `CL` function number, what it
  does, and where it's implemented.
- [docs/src-c.md](docs/src-c.md) — `/src-c`, a routine-by-routine C port of
  the hand-written kernel/loader/RSP assembly, verified by differential
  testing rather than byte-for-byte matching; what's ported so far and
  what's next.

## Source

This source was downloaded from
[www.cpm.z80.de/source.html](http://www.cpm.z80.de/source.html), from the
archive:

[http://www.cpm.z80.de/download/mpm862sr.zip](http://www.cpm.z80.de/download/mpm862sr.zip)

Described there as:

> 60K It contains the contents of 11 8" SSSD disks labeled 01 through 11.
> These are from Digital Research original disks. The first 6 disks are
> labeled as being able to compile on micro and the last 5 are to be
> compiled on "the VAX". All diskettes, though originally from DRI, have
> hand written labels, i.e. they are probably work disks. If you're
> curious, here's a scan of a disk label.

`orig/driorig/` and `orig/lastdri/` were downloaded from
[cpm.retropc.se/cpm/randyfiles/DRI/index.html](https://cpm.retropc.se/cpm/randyfiles/DRI/index.html).

The scanned DRI manuals in `orig/manuals/` have also been extracted to
Markdown (`.md` alongside each `.pdf`) to make them faster and easier
for LLMs to use as reference material.

[cpm.retropc.se/cpm/develop.html](https://cpm.retropc.se/cpm/develop.html)
lists more CP/M-era applications/source that could be candidates for a
similar porting effort.
