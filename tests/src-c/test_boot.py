#!/usr/bin/env python3
"""Proof-of-pipeline test for src-c/boot + src-c/Makefile's own
`kernel.elf` target: builds the whole src-c/kernel/ tree plus
src-c/boot/'s own stub with the real i686-elf-gcc/i686-elf-ld
toolchain and a real linker script (src-c/linker.ld), links them all
into one real image, then boots the result under QEMU and checks the
serial output. At boot, src-c/boot/kernel_main.c runs the real ported
XIOS (init_entry, xios_conout_entry) and the real ported system
initialization (sup_init) against real low memory, then reads every
field sup_init should have written back from memory and reports
"sup_init: ok" or which field was wrong. This is not /src's own real
MP/M-86 boot chain, and the rest of the ported kernel above the XIOS
still isn't called -- see src-c/boot/boot.s's own header comment for
why 32-bit protected mode is a different execution model from
MP/M-86's own real 16-bit one.

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
SIGNONS = (
    ("XIOS86Q init_entry signon", b"MP/M-86 XIOS86Q -- QEMU target (serial console, floppy disk)"),
    ("pipeline signon via xios_conout_entry", b"MP/M-86 src-c image-builder: pipeline ok"),
    ("sup_init on real memory, all fields verified", b"sup_init: ok"),
)


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

    failures = []
    for label, expected in SIGNONS:
        status = "ok" if expected in output else "FAIL"
        print(f"[{status}] {label}: expected {expected!r} in QEMU output")
        if status != "ok":
            failures.append(label)
    if failures:
        print(f"\nQEMU output was: {output!r}")
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print(f"\nall {len(SIGNONS)} case(s) match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
