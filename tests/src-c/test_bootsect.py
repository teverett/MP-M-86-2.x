#!/usr/bin/env python3
"""src-c's boot sector (src-c/boot/bootsect.s): booted by the BIOS from
a floppy -- no QEMU -kernel -- it loads kernel.elf's flat image
(build/boot/kernel.bin) and starts it in protected mode.

The floppy here is blank but for an empty CP/M directory and the kernel
in its last sectors (stage 4's disk builder puts it in a file); the
boot sector's fields are filled in with src-c/tools/bootsect.py.
Checks:
  - the sector: 512 bytes, the 55AAh signature, the KRNL fields filled
    in as given;
  - booting: the boot sector's message, then the kernel up to TMP's
    prompt, and ECHO running;
  - a kernel the fields point past the end of the disk: the boot
    sector's read error, not a hang or a crash.

Needs the i686-elf toolchain and qemu-system-i386 (as test_boot.py);
skips (exit 0) without them.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_C = os.path.join(REPO_ROOT, 'src-c')
sys.path.insert(0, os.path.join(SRC_C, 'tools'))
import bootsect  # noqa: E402

REQUIRED_TOOLS = ('i686-elf-gcc', 'i686-elf-ld', 'i686-elf-objcopy', 'qemu-system-i386')
DISK_SIZE = 1474560
DIR_START, DIR_LEN = 18432, 2048  # 2 reserved tracks; 64 entries


def floppy(path, boot, kernel, lba=None, nsectors=None):
    n = (len(kernel) + 511) // 512
    lba = DISK_SIZE // 512 - n if lba is None else lba
    img = bytearray(DISK_SIZE)
    img[DIR_START:DIR_START + DIR_LEN] = b'\xe5' * DIR_LEN
    at = (DISK_SIZE // 512 - n) * 512
    img[at:at + len(kernel)] = kernel
    sector = bootsect.patch(boot, lba, nsectors or n, bootsect.elf_entry(os.path.join(SRC_C, 'kernel.elf')))
    img[0:512] = sector
    open(path, 'wb').write(img)
    return sector, lba, n


def boot(path, script):
    qemu = subprocess.Popen(['qemu-system-i386', '-drive', f'file={path},format=raw,if=floppy', '-boot', 'a',
                             '-serial', 'stdio', '-display', 'none', '-no-reboot'],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    out = bytearray()

    def read_all():
        while True:
            chunk = qemu.stdout.read1(4096)
            if not chunk:
                return
            out.extend(chunk)

    reader = threading.Thread(target=read_all, daemon=True)
    reader.start()
    for wait, keys in script:
        time.sleep(wait)
        if keys:
            qemu.stdin.write(keys)
            qemu.stdin.flush()
    qemu.terminate()
    qemu.wait()
    reader.join(timeout=2)
    return bytes(out)


def main():
    missing = [t for t in REQUIRED_TOOLS if shutil.which(t) is None]
    if missing:
        print(f"skip: missing tool(s) {', '.join(missing)}")
        return 0
    subprocess.run(['make', 'kernel.elf', 'build/boot/bootsect.bin', 'build/boot/kernel.bin'], cwd=SRC_C,
                   check=True, stdout=subprocess.DEVNULL)
    boot_bin = open(os.path.join(SRC_C, 'build', 'boot', 'bootsect.bin'), 'rb').read()
    kernel = open(os.path.join(SRC_C, 'build', 'boot', 'kernel.bin'), 'rb').read()
    failures = []

    def check(label, ok, detail):
        print(f"[{'ok' if ok else 'FAIL'}] {label}: {detail}")
        if not ok:
            failures.append(label)

    with tempfile.TemporaryDirectory() as tmp:
        img = os.path.join(tmp, 'boot.img')
        sector, lba, n = floppy(img, boot_bin, kernel)
        check("boot sector is 512 bytes with the 55AAh signature", len(sector) == 512 and sector[510:] == b'\x55\xaa',
              f"{len(sector)} bytes, ends {sector[510:].hex()}")
        fields = struct.unpack_from('<4sHHI', sector, bootsect.FIELDS)
        entry = bootsect.elf_entry(os.path.join(SRC_C, 'kernel.elf'))
        check("its fields", fields == (b'KRNL', lba, n, entry),
              f"{fields[0]!r} lba {fields[1]} sectors {fields[2]} entry {fields[3]:#x}")
        try:
            bootsect.patch(boot_bin, 0, bootsect.LOAD_LIMIT + 1, entry)
            check("a kernel too big to load is refused", False, "patched anyway")
        except AssertionError:
            check("a kernel too big to load is refused", True, f"more than {bootsect.LOAD_LIMIT} sectors")

        out = boot(img, [(8, b'ECHO booted\r'), (3, b'')])
        for label, want in (("the boot sector's message", b"src-c boot: loading the kernel"),
                             ("the kernel starts (the XIOS signon)", b"MP/M-86 XIOS86Q -- QEMU target"),
                             ("TMP's prompt", b"0A>"),
                             ("ECHO runs", b"ECHO Msg Qued\r\n booted")):
            check(label, want in out, f"expected {want!r}")

        floppy(img, boot_bin, kernel, lba=DISK_SIZE // 512)  # past the end of the disk
        out = boot(img, [(4, b'')])
        check("a bad kernel sector: the read error", b"src-c boot: disk read error" in out and b"XIOS86Q" not in out,
              repr(out[-60:]))

    if failures:
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
