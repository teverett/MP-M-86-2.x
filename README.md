# MP/M-86 2.x

Historical source code for Digital Research's **MP/M-86 2.x** operating
system. See [docs/orig.md](docs/orig.md) for a full breakdown of the source
tree.

## Directory structure

```
orig/           The original 11-disk source tree, byte-for-byte as
                downloaded (see docs/orig.md for what's on each disk).
                Never patched -- src/ is where any fixes happen.
src/            The same source, organized by what it builds:
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
build/          Build output (gitignored, created by `make`).
Makefile        Build everything with `make`; see its own header
                comment for all targets (`make disk`, `make run`, ...).
```

## Documentation

- [docs/orig.md](docs/orig.md) — what's in the original 11-disk source
  tree, disk by disk, and how it maps to `src/`.
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
  `src/utils/` commands (`PIP`, `ED`, `DIR`, `TYPE`, etc.), what each
  one does, and how they're built and placed on disk.
- [docs/syscalls.md](docs/syscalls.md) — the MP/M-86 kernel's and the
  loader's own system-call tables: every `CL` function number, what it
  does, and where it's implemented.

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
