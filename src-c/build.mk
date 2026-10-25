# build.mk -- the real i686-elf cross toolchain settings used by
# src-c/Makefile for both the compile-only type-check pass and the
# kernel.elf linker/image-builder target (which includes boot/'s
# hand-written entry stub). Keeping CC/LD/common CFLAGS here prevents
# the build and link settings from drifting apart.
#
# i686-elf, not a host compiler and not an 8086 one: no 8086/80186 C
# compiler exists on this machine (ia16-elf-gcc/Watcom/SDCC all
# checked -- SDCC's own target list has no 8086 at all), and the
# *host* `cc` is reserved for a different job entirely (tests/src-c/
# test_*.py's own differential tests hardcode it directly, not via
# this file, to build `ctypes`-loadable shared libraries a cross
# compiler's own output can't be). i686-elf-gcc targets 32-bit
# protected mode with a flat memory model -- a different execution
# model from MP/M-86's own real 16-bit real-mode, segment:offset one;
# see docs/src-c.md's own "`src-c/boot/`" section for the full
# reasoning and what that does and doesn't mean for this tree.
#
# `-ffreestanding` is required here, not optional, even for a file
# that only uses `<stdint.h>`: without it, i686-elf-gcc's own bundled
# `stdint.h` assumes a *hosted* environment and does `#include_next
# <stdint.h>`, expecting a further, real libc one somewhere down the
# search path -- which doesn't exist on this bare, libc-less cross
# toolchain, so it fails outright. With `-ffreestanding`, the same
# header instead defines everything itself, per the C standard's own
# "freestanding implementations must provide stdint.h standalone"
# requirement.
#
# Each Makefile that includes this one adds its own extra CFLAGS
# (`-std=c11` vs `-std=gnu11`, `-I` paths) rather than this file
# picking one -- the two builds have genuinely different needs (a
# syntax/type-check pass vs. an actual linked binary), not just a
# style difference.
#
# The rest of CFLAGS below is a real kernel-code flag set, not just
# "freestanding": `-m32 -march=i686` pins the actual target (this
# cross toolchain defaults to i686 already, but pinning it keeps that
# explicit rather than implicit); `-mgeneral-regs-only` stops GCC
# from ever emitting SSE/MMX/x87 instructions, which would need real
# FPU/SSE context-switch support this kernel doesn't have (and isn't
# going to, matching MP/M-86's own real CPU target having no FPU
# state to save either); `-fcf-protection=none` drops the `endbr32`
# control-flow-enforcement prologues this host toolchain's own
# defaults would otherwise emit -- meaningless (and possibly
# unsupported) without the matching CPU/OS support a freestanding
# kernel doesn't have; `-fno-asynchronous-unwind-tables` drops `.eh_frame`
# generation, which exists for C++-style exception unwinding this
# code never does and would just be dead weight in the final image.
CC := i686-elf-gcc
LD := i686-elf-ld
CFLAGS := -m32 -march=i686 -mgeneral-regs-only -ffreestanding -fno-pie \
          -fno-stack-protector -fcf-protection=none \
          -fno-asynchronous-unwind-tables -O2 -Wall -Wextra -Werror

# Windows' python.org/GitHub Actions installs often expose only
# "python", not "python3" -- same reasoning as the top-level Makefile.
PYTHON := $(shell command -v python3 2>/dev/null || command -v python)
QEMU    := qemu-system-i386