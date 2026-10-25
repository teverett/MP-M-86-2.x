#!/usr/bin/env python3
"""Proof-of-pipeline test for src-c/boot + src-c/Makefile's own
`kernel.elf` target: builds the whole src-c/kernel/ tree plus
src-c/boot/'s own stub with the real i686-elf-gcc/i686-elf-ld
toolchain and a real linker script (src-c/linker.ld), links them all
into one real image, then boots the result under QEMU and checks its
serial signon string comes out. See src-c/boot/boot.s's own header
comment for what this is (a real `ld`-based image-builder pipeline,
proven end to end -- including the real link, not just the stub) and
isn't (not /src's own real MP/M-86 boot chain, and not yet wired to
the real ported kernel logic actually *doing* anything at boot -- see
that same comment for why 32-bit protected mode is a different
execution model from MP/M-86's own real 16-bit one).

Needs i686-elf-gcc/i686-elf-ld/i686-elf-grub-file and
qemu-system-i386 on PATH -- not part of this project's own from-
scratch toolchain (tools/asm86.py/tools/i86sim.py), so unlike every
other tests/src-c/test_*.py this one depends on external tools; skips
with a clear message (exit 0) rather than failing if they're missing,
since src-c isn't wired into this repo's own CI yet either (see
.github/workflows/ -- it only builds/tests /src).
"""
import os
import shutil
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_C_DIR = os.path.join(REPO_ROOT, 'src-c')

REQUIRED_TOOLS = ('i686-elf-gcc', 'i686-elf-ld', 'i686-elf-grub-file', 'qemu-system-i386')
SIGNON = b"MP/M-86 src-c image-builder: pipeline ok"


def main():
    missing = [t for t in REQUIRED_TOOLS if shutil.which(t) is None]
    if missing:
        print(f"skip: missing tool(s) {', '.join(missing)} -- not part of this "
              f"project's own toolchain, see this file's own module docstring")
        return 0

    subprocess.run(['make', 'clean'], cwd=SRC_C_DIR, check=True)
    subprocess.run(['make', 'kernel.elf'], cwd=SRC_C_DIR, check=True)

    kernel_elf = os.path.join(SRC_C_DIR, 'kernel.elf')
    with tempfile.TemporaryDirectory() as tmp:
        log_path = os.path.join(tmp, 'serial.log')
        try:
            subprocess.run(
                ['qemu-system-i386', '-kernel', kernel_elf, '-serial', f'file:{log_path}',
                 '-display', 'none', '-no-reboot'],
                timeout=5, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except subprocess.TimeoutExpired:
            pass  # kernel_main halts forever by design -- expected

        with open(log_path, 'rb') as f:
            output = f.read()

    status = "ok" if SIGNON in output else "FAIL"
    print(f"[{status}] serial signon: expected {SIGNON!r} in QEMU output, got {output!r}")
    if status != "ok":
        print("\n1 case(s) failed: serial signon")
        return 1
    print("\nall 1 case(s) match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
