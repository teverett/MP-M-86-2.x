#!/usr/bin/env python3
"""src-c/tools/mkdisk.py: src-c's bootable floppy (build/mpm86c.img).

Checks:
  - its CP/M geometry is the C XIOS's: mkdisk.py's constants against the
    DPB src-c/gensys builds (gensys run on the host, as test_gensys.py);
  - the image: the boot sector's fields point at KERNEL.SYS, whose bytes
    are build/boot/kernel.bin (read-only, contiguous); the transients'
    CMD files (mkdisk.TRANSIENTS: SUBMIT, RANDOM, DIR, TYPE, ERA, REN),
    whose data groups are the C programs' data images and whose code
    groups have the originals' lengths; DEMO.SUB; and
    nothing else in the directory;
  - it boots by itself (no -kernel) and runs SUBMIT DEMO, DIR and TYPE
    from the disk.

Needs the i686-elf toolchain and qemu-system-i386; skips without them.
"""
import ctypes
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_C = os.path.join(REPO_ROOT, 'src-c')
FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')
sys.path.insert(0, os.path.join(SRC_C, 'tools'))
import bootsect  # noqa: E402
import mkdisk  # noqa: E402

REQUIRED_TOOLS = ('i686-elf-gcc', 'i686-elf-ld', 'i686-elf-objcopy', 'i686-elf-nm', 'qemu-system-i386')


class GensysLayout(ctypes.Structure):
    _fields_ = [('sysdat', ctypes.c_uint16), ('sysdat_len', ctypes.c_uint16), ('free_start', ctypes.c_uint16)]


def gensys_dpb(tmp):
    """the DPB in the image src-c/gensys builds"""
    lib_path = os.path.join(tmp, 'gensys.so')
    srcs = [os.path.join(SRC_C, 'gensys', f) for f in ('gensys.c', 'sysdat.c')]
    srcs += [os.path.join(SRC_C, d, f) for d in ('rsp', 'utils')  # programs.c's table names utils' mains
             for f in sorted(os.listdir(os.path.join(SRC_C, d))) if f.endswith('.c')]
    srcs.append(os.path.join(FIXTURES, 'rt_host.c'))
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unknown-warning-option',
                    '-Wno-unterminated-string-initialization', '-shared', '-fPIC',
                    *[f'-I{os.path.join(SRC_C, d)}' for d in ('kernel', 'rt', 'rsp', 'gensys', 'utils')], *srcs,
                    '-o', lib_path], check=True)
    lib = ctypes.CDLL(lib_path)
    lib.gensys_rsps.restype = ctypes.c_void_p
    lib.gensys_rsps.argtypes = [ctypes.POINTER(ctypes.c_uint)]
    lib.gensys.restype = GensysLayout
    lib.gensys.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_void_p, ctypes.c_uint]
    n = ctypes.c_uint()
    rsps = lib.gensys_rsps(ctypes.byref(n))
    mem = (ctypes.c_uint8 * 0x100000)()
    layout = lib.gensys(mem, rsps, n.value)
    syms = open(os.path.join(SRC_C, 'gensys', 'gensys_syms.h')).read()
    dpb0 = int(re.search(r'#define XQD_DPB0 (0x[0-9A-F]+)', syms).group(1), 16)
    at = layout.sysdat * 16 + dpb0
    return struct.unpack_from('<HBBBHHBBHH', bytes(mem[at:at + 15]))


def directory(img):
    """{(name, type): (attributes, [(extent, rc, blocks)])}"""
    files = {}
    for i in range(mkdisk.DIR_ENTRIES):
        e = img[mkdisk.block_offset(0) + 32 * i:mkdisk.block_offset(0) + 32 * i + 32]
        if e[0] == 0xE5:
            continue
        key = (bytes(e[1:9]), bytes(b & 0x7F for b in e[9:12]))
        attrs = bytes(b & 0x80 for b in e[9:12])
        files.setdefault(key, (attrs, []))[1].append((e[12] | e[14] << 5, e[15], [b for b in e[16:32] if b]))
    return files


def read_file(img, extents):
    out = b''
    for _, rc, blocks in sorted(extents):
        data = b''.join(img[mkdisk.block_offset(b):mkdisk.block_offset(b) + mkdisk.BLOCK] for b in blocks)
        out += data[:rc * mkdisk.RECORD]
    return out


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

    threading.Thread(target=read_all, daemon=True).start()
    for wait, keys in script:
        time.sleep(wait)
        if keys:
            qemu.stdin.write(keys)
            qemu.stdin.flush()
    qemu.terminate()
    qemu.wait()
    time.sleep(0.5)
    return bytes(out)


def main():
    missing = [t for t in REQUIRED_TOOLS if shutil.which(t) is None]
    if missing:
        print(f"skip: missing tool(s) {', '.join(missing)}")
        return 0
    subprocess.run(['make', 'disk'], cwd=SRC_C, check=True, stdout=subprocess.DEVNULL)
    img = open(os.path.join(SRC_C, 'build', 'mpm86c.img'), 'rb').read()
    kernel = open(os.path.join(SRC_C, 'build', 'boot', 'kernel.bin'), 'rb').read()
    failures = []

    def check(label, ref, port):
        ok = ref == port
        print(f"[{'ok' if ok else 'FAIL'}] {label}: expected {ref!r} | got {port!r}")
        if not ok:
            failures.append(label)

    with tempfile.TemporaryDirectory() as tmp:
        spt, bsh, blm, exm, dsm, drm, al0, al1, cks, off = gensys_dpb(tmp)
        check("geometry = the XIOS's DPB (spt bsh blm exm dsm drm al0 al1 cks off)",
              (spt, bsh, blm, exm, dsm, drm, al0, al1, cks, off),
              (mkdisk.SPT, mkdisk.BSH, mkdisk.BLM, 0, mkdisk.DSM, mkdisk.DRM, mkdisk.AL0, 0,
               mkdisk.DIR_ENTRIES // 4, mkdisk.OFF))

        check("image size", 1474560, len(img))
        files = directory(img)
        check("the files", sorted([(b'KERNEL  ', b'SYS'), (b'DEMO    ', b'SUB')] +
                                  [(mkdisk.disk_name(n).encode().ljust(8), b'CMD') for n, _ in mkdisk.TRANSIENTS]), sorted(files))

        attrs, extents = files[(b'KERNEL  ', b'SYS')]
        check("KERNEL.SYS is read-only", b'\x80\x00\x00', attrs)
        blocks = [b for _, _, bl in sorted(extents) for b in bl]
        check("KERNEL.SYS is contiguous from block 1", list(range(1, len(blocks) + 1)), blocks)
        check("KERNEL.SYS is kernel.bin", kernel, read_file(img, extents)[:len(kernel)])
        lba, nsect, entry = struct.unpack_from('<HHI', img, bootsect.FIELDS + 4)
        check("boot sector: KERNEL.SYS's first sector", mkdisk.block_offset(blocks[0]) // 512, lba)
        check("boot sector: its length", (len(kernel) + 511) // 512, nsect)
        check("boot sector: the kernel's entry", bootsect.elf_entry(os.path.join(SRC_C, 'kernel.elf')), entry)

        for upper, where in mkdisk.TRANSIENTS:
            name, code = upper.lower(), mkdisk.code_len(upper, where)
            data = read_file(img, files[(mkdisk.disk_name(upper).encode().ljust(8), b'CMD')][1])
            image = mkdisk.c_image(os.path.join(SRC_C, 'build', where, name + '.o'), name)
            form1, cparas, _, cmin, _, form2, dparas, _, dmin, dmax = struct.unpack_from('<BHHHHBHHHH', data, 0)
            check(f"{name.upper()}.CMD: code group {code:#x} bytes", (1, (code + 15) // 16, (code + 15) // 16),
                  (form1, cparas, cmin))
            check(f"{name.upper()}.CMD: data group = the C image", (2, (len(image) + 15) // 16, image),
                  (form2, dparas, data[128 + cparas * 16:128 + cparas * 16 + len(image)]))
            check(f"{name.upper()}.CMD: data group max", mkdisk.DATA_MAX.get(upper, 0), dmax)
        demo = read_file(img, files[(b'DEMO    ', b'SUB')][1])
        check("DEMO.SUB", mkdisk.DEMO_SUB, demo.rstrip(b'\x1a'))

        path = os.path.join(tmp, 'disk.img')
        open(path, 'wb').write(img)
        out = boot(path, [(8, b'SUBMIT DEMO hello\r'), (8, b'DIR *.CMD\r'), (4, b'TYPE DEMO.SUB\r'), (4, b'')])
        for label, want in (("boots: the boot sector", b"src-c boot: loading the kernel"),
                            ("boots: TMP's prompt", b"0A>"),
                            ("SUBMIT DEMO from the disk", b" MP/M-86 on the C kernel, from a disk src-c built"),
                            ("with its argument", b" argument 1: HELLO"),
                            # DIR's first line: the first four CMD files, in mkdisk's order
                            ("DIR from the disk", b"A: " + b" : ".join(
                                mkdisk.disk_name(n).encode().ljust(9) + b'CMD' for n, _ in mkdisk.TRANSIENTS[:4])),
                            ("TYPE from the disk", b"ECHO argument 1: $1\r\nUSER 1")):
            check(label, True, want in out)

    if failures:
        print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
        return 1
    print("\nall cases match.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
