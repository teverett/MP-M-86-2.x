"""bootsect.py -- fill in src-c's boot sector (src-c/boot/bootsect.s):
where the kernel's flat image is on the disk and where to start it.

The sector ends with "KRNL", then kernel_lba (word), kernel_sectors
(word) and kernel_entry (dword), at 1F0h; the 55AAh signature follows.
"""
import struct

FIELDS = 0x1F0
MAGIC = b'KRNL'
SECTOR = 512
LOAD_LIMIT = (0x9F000 - 0x20000) // SECTOR  # what fits from 2000:0 below 9F000h


def elf_entry(path):
    """kernel.elf's entry point (e_entry)"""
    with open(path, 'rb') as f:
        head = f.read(28)
    assert head[:4] == b'\x7fELF', f"{path} isn't an ELF file"
    return struct.unpack_from('<I', head, 24)[0]


def patch(bootsect, lba, nsectors, entry):
    """bootsect with its fields filled in"""
    b = bytearray(bootsect)
    assert len(b) == SECTOR and b[510:512] == b'\x55\xaa', "not a boot sector"
    assert b[FIELDS:FIELDS + 4] == MAGIC, "not src-c's boot sector (no KRNL fields)"
    assert 0 < nsectors <= LOAD_LIMIT, f"kernel too big: {nsectors} sectors (at most {LOAD_LIMIT})"
    struct.pack_into('<HHI', b, FIELDS + 4, lba, nsectors, entry)
    return bytes(b)
