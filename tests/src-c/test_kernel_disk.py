#!/usr/bin/env python3
"""kernel.elf with a disk: the C kernel's BDOS (getdiskmx and the file
functions, src-c/kernel/bdosif.c and file1-4.c) on the real floppy image,
through the C XIOS and the C CACHE, running the C SUBMIT and RANDOM
(src-c/rsp/) as transients LOAD reads from the disk.

On a copy of build/disk/mpm86q.img (make -C src builds it) with a T.SUB
added, it types:
  - `SUBMIT T alpha beta` -- T.SUB echoes $1 and $2 and switches user
    numbers: checks each expanded command's output;
  - `RANDOM R.DAT`, then `3W` hello, `5Z` five, `3R`, `5R`, `Q` --
    random write, write with zero fill, random read, close: checks what
    it prints back, then reads R.DAT off the image (records 3 and 5).

Needs qemu-system-i386 and the i686-elf toolchain (as test_boot.py);
skips (exit 0) if they or the disk image are missing.
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
SRC_C_DIR = os.path.join(REPO_ROOT, 'src-c')
DISK = os.path.join(REPO_ROOT, 'build', 'disk', 'mpm86q.img')
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

REQUIRED_TOOLS = ('i686-elf-gcc', 'i686-elf-ld', 'qemu-system-i386')

T_SUB = b'ECHO first $1\r\nUSER 2\r\nECHO second $2\r\nUSER 0\r\n'

# (seconds to wait first, keys)
SCRIPT = [
    (6, b'SUBMIT T alpha beta\r'),
    (10, b'RANDOM R.DAT\r'),
    (3, b'3W\r'), (1, b'hello\r'),
    (2, b'5Z\r'), (1, b'five\r'),
    (2, b'3R\r'), (2, b'5R\r'),
    (2, b'Q\r'),
    (3, b''),
]

EXPECT = (
    ("SUBMIT loaded from the disk, C image identical", b"rt: SUBMIT   data segment: C image identical"),
    ("SUBMIT: $1 expanded, ECHO ran", b" first ALPHA"),
    ("SUBMIT: USER built-in", b"User Number = 2"),
    ("SUBMIT: $2 expanded, ECHO ran", b" second BETA"),
    ("RANDOM loaded from the disk, C image identical", b"rt: RANDOM   data segment: C image identical"),
    ("RANDOM: record 3 read back", b"next command? 3R\r\r\nhello"),
    ("RANDOM: record 5 (zero fill write) read back", b"next command? 5R\r\r\nfive"),
)


def cpm_entries(img):
    import build_disk as bd
    for i in range(bd.DIR_ENTRIES):
        off = bd.DIR_START + i * 32
        yield off, img[off:off + 32]


def cpm_put(img, name, ext, data):
    """Add a one-extent file to the image's directory, in free blocks."""
    import build_disk as bd
    data = data + b'\x1a' * ((-len(data)) % 128)
    used = set(range(bd.DIR_BLOCKS))
    for _, e in cpm_entries(img):
        if e[0] != 0xE5:
            used.update(b for b in e[16:32] if b)
    nblk = (len(data) + bd.BLOCK_SIZE - 1) // bd.BLOCK_SIZE
    blocks = [b for b in range(bd.TOTAL_BLOCKS) if b not in used][:nblk]
    slot = next(off for off, e in cpm_entries(img) if e[0] == 0xE5)
    img[slot:slot + 32] = struct.pack('<B8s3sBBBB16B', 0, name.ljust(8), ext.ljust(3), 0, 0, 0,
                                      len(data) // 128, *(blocks + [0] * (16 - len(blocks))))
    for k, b in enumerate(blocks):
        chunk = data[k * bd.BLOCK_SIZE:(k + 1) * bd.BLOCK_SIZE]
        img[bd.block_to_offset(b):bd.block_to_offset(b) + len(chunk)] = chunk


def cpm_get(img, name, ext):
    import build_disk as bd
    for _, e in cpm_entries(img):
        if e[0] == 0 and e[1:9] == name.ljust(8) and e[9:12] == ext.ljust(3):
            return b''.join(img[bd.block_to_offset(b):bd.block_to_offset(b) + bd.BLOCK_SIZE]
                            for b in e[16:32] if b)[:e[15] * 128]
    return None


def main():
    missing = [t for t in REQUIRED_TOOLS if shutil.which(t) is None]
    if missing:
        print(f"skip: missing tool(s) {', '.join(missing)}")
        return 0
    if not os.path.exists(DISK):
        print(f"skip: no {DISK} (make -C src builds it)")
        return 0
    subprocess.run(['make', 'kernel.elf'], cwd=SRC_C_DIR, check=True, stdout=subprocess.DEVNULL)

    with tempfile.TemporaryDirectory() as tmp:
        disk = os.path.join(tmp, 'disk.img')
        img = bytearray(open(DISK, 'rb').read())
        cpm_put(img, b'T', b'SUB', T_SUB)
        open(disk, 'wb').write(img)

        qemu = subprocess.Popen(
            ['qemu-system-i386', '-kernel', os.path.join(SRC_C_DIR, 'kernel.elf'),
             '-drive', f'file={disk},format=raw,if=floppy,cache=writethrough', '-serial', 'stdio', '-display', 'none',
             '-no-reboot'],
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
        for wait, keys in SCRIPT:
            time.sleep(wait)
            if keys:
                qemu.stdin.write(keys)
                qemu.stdin.flush()
        qemu.terminate()  # SIGTERM: QEMU writes the image out (SIGKILL could lose writes)
        qemu.wait()
        reader.join(timeout=2)
        r_dat = cpm_get(bytearray(open(disk, 'rb').read()), b'R', b'DAT')

    output = bytes(out)
    failures = []

    def check(label, ok, detail):
        print(f"[{'ok' if ok else 'FAIL'}] {label}: {detail}")
        if not ok:
            failures.append(label)

    for label, expected in EXPECT:
        check(label, expected in output, f"expected {expected!r}")
    check("no BDOS error", b"Bdos Err" not in output, "no 'Bdos Err' message")
    check("R.DAT on the disk", r_dat is not None and len(r_dat) >= 6 * 128,
          f"{len(r_dat) if r_dat else 0} bytes")
    if r_dat:
        check("R.DAT record 3", r_dat[3 * 128:3 * 128 + 6] == b'hello\0', repr(r_dat[3 * 128:3 * 128 + 8]))
        check("R.DAT record 5", r_dat[5 * 128:5 * 128 + 5] == b'five\0', repr(r_dat[5 * 128:5 * 128 + 8]))
        check("R.DAT record 4 zero filled", r_dat[4 * 128:5 * 128] == bytes(128), "128 zero bytes")
    if failures:
        print(f"\nQEMU output was: {output!r}")
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
