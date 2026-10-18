# The QEMU Disk Image Format

This describes `build/disk/mpm86q.img`, the bootable floppy image
`tools/build_disk.py` produces by combining this tree's other build
outputs. There is no DRI original to match here — the real hardware
had no boot sector at all (`04/ROM.A86` ran from EPROM, not disk) — so
every convention below was designed from scratch for this project's
QEMU target, informed only by what `src/loader/ROM86Q.A86` and
`src/loader/LDMPM.A86` themselves expect to find.

## Geometry

A standard 1.44MB 3.5" floppy: 80 cylinders × 2 heads × 18 sectors/
track × 512 bytes/sector = 1,474,560 bytes total. This is the same
geometry `src/kernel/XIOS86Q.A86` and `src/loader/LDBIOS86Q.A86`
target for their own NEC765 disk I/O once MP/M-86 itself is running.

CP/M's own "logical" 128-byte-record view of this disk (used by the
Disk Parameter Block below, and by `LDBDOS.A86`'s file system) deblocks
each 512-byte physical sector into 4 logical records, and treats each
physical head-side as one CP/M "track" (so SPT = 18 × 4 = 72, and there
are 80 × 2 = 160 CP/M tracks total — see "Disk Parameter Block" below).

## Layout

| Byte range | Physical location | Contents |
|---|---|---|
| `0x00000`–`0x001FF` | cyl 0, head 0, sector 1 | `ROM86Q.bin` — the boot sector |
| `0x00200`–`0x003FF` | cyl 0, head 0, sector 2 | the loader header (9 bytes + padding) |
| `0x00400`–`0x047FF` | cyl 0, head 0, sector 3 through cyl 0, head 1, sector 18 | `MPMLDR.bin` (within the 2 reserved CP/M tracks) |
| `0x04800`–`0x067FF` | — | the CP/M directory (one 8192-byte allocation block) |
| `0x06800`– | — | `MPM.SYS`'s own body (header + code + data), then each `src/utils/` utility's own body (as `<NAME>.CMD`) and `ECHO.RSP`, back to back, each with the same header+code+data shape as `MPM.SYS` |
| everything else | — | zero-filled, unallocated |

### The boot sector (byte 0)

`ROM86Q.bin`, built by `tools/build_bootsect.py` from
`src/loader/ROM86Q.A86`. Exactly 512 bytes, ending in the mandatory
`55h AAh` boot signature. SeaBIOS (or any PC BIOS) loads this at
`0000:7C00h` and jumps to it with `DL` = the boot drive number — see
that file's own header comment for everything it does once running.

### The loader header (byte 0x200, physical sector 2)

A 9-byte structure `ROM86Q.A86` itself reads to learn where and how
much of `MPMLDR.bin` to load, reusing the exact field layout the real
`04/ROM.A86` reads from its own first sector (`Ty`/`Len`/`Abs`/`Min`/
`Max`):

| Offset | Field | Size | Meaning |
|---|---|---|---|
| 0 | `Ty` | 1 byte | type (cosmetic only — `ROM86Q.A86` doesn't check it) |
| 1 | `Len` | word | `MPMLDR.bin`'s size in bytes |
| 3 | `Abs` | word | the real-mode segment to load `MPMLDR.bin` at |
| 5 | `Min` | word | unused by `ROM86Q.A86`'s own logic |
| 7 | `Max` | word | unused by `ROM86Q.A86`'s own logic |

The remaining 503 bytes of this 512-byte sector are zero padding —
irrelevant to `ROM86Q.A86`, which only reads the first 9 bytes, but see
"A hard-won lesson" below for why that distinction matters at the
*destination* end of the read, not just here.

`tools/build_disk.py` currently always sets `Abs` to `0800h` (physical
address `8000h` — comfortably clear of the boot sector itself at
`7C00h`-`7E00h` and the stack below it) and `Min`/`Max` to `MPMLDR.bin`'s
own length, with `Ty = 1`.

### `MPMLDR.bin` (byte 0x400, physical sector 3 onward)

The combined `LDMPM.A86` + `LDBDOS.A86` + `LDBIOS86Q.A86` image built
by `tools/build_mpmldr.py` (see that script's own docstring for how
the three modules are laid out within it). Placed entirely within the
first 2 physical tracks of the disk — the same "reserved tracks"
region CP/M's own `OFF` DPB field (below) reserves for exactly this
purpose, so the loader and the CP/M filesystem below never collide.

### The CP/M directory (byte 0x4800)

One 8192-byte allocation block (block 0), holding up to 64 32-byte
directory entries. Unused entries are filled with `E5h` (CP/M's
standard "available" marker), not zero. `MPM.SYS` needs 2 entries here
(see "Why some files need more than one directory entry" below); each
`src/utils/` utility and `ECHO.RSP` gets its own entry (or more, by the
same rule) after it — 23 entries in total.

### `MPM.SYS`'s body (byte 0x6800, allocation block 1 onward)

Built by `tools/build_disk.py` itself (`build_mpm_sys_body`), combining
`MPM.bin.cseg`/`MPM.bin.dseg` (from `tools/build_mpmsys.py`) with a
128-byte header `src/loader/LDMPM.A86`'s own code reads and validates
(`cmp ctype,codetype ! jnz badhdr`, etc. — see that file's `perr:`/
`hdrok:` logic around line 140):

| Offset | Field | Size | Meaning |
|---|---|---|---|
| 0 | `ctype` | 1 byte | must be exactly `1` (`codetype`) |
| 1 | `clen` | word | the combined kernel code size |
| 3 | `cldseg` | word | the real-mode segment the code loads at |
| 5 | `cmin` | word | unused by `LDMPM.A86`'s own logic |
| 7 | `cmax` | word | unused by `LDMPM.A86`'s own logic |
| 9 | `dtype` | 1 byte | must be exactly `2` (`datatype`) |
| 10 | `dlen` | word | the combined kernel data size |
| 12 | `dldseg` | word | the real-mode segment the data loads at |
| 14 | `dmin` | word | unused by `LDMPM.A86`'s own logic |
| 16 | `dmax` | word | unused by `LDMPM.A86`'s own logic |

padded with zeros to 128 bytes, followed immediately by `clen` bytes
of code then `dlen` bytes of data (one continuous sequential read, as
far as `LDBDOS.A86`'s file I/O is concerned — there's no gap between
the header record and the code, or between the code and the data).

`tools/build_disk.py` currently sets `cldseg = 1000h` and
`dldseg = cldseg + ceil(clen/16)` (the next free paragraph after the
code), with the `min`/`max` fields set to 0 (unused).

Unlike the loader header above, `MPM.SYS` is a real file in the CP/M
directory — `LDMPM.A86` finds it by name (`FCB` for `"MPM     SYS"`),
not by a fixed disk address, so it needs the directory entries and
Disk Parameter Block described below to exist at all.

## Disk Parameter Block

The DPB both `src/kernel/XIOS86Q.A86` and `src/loader/LDBIOS86Q.A86`
carry (and must keep identical, since both read the same on-disk
filesystem) — `tools/build_disk.py` builds the directory/allocation
layout to match it exactly:

| Field | Value | Meaning |
|---|---|---|
| `SPT` | 72 | 128-byte logical records per CP/M track (18 physical sectors × 4) |
| `BSH` | 6 | block shift — 8192-byte (2^13) allocation blocks |
| `BLM` | 63 | block mask (64 records/block − 1) |
| `EXM` | 0 | extent mask — one directory entry covers at most 128 records (16KB), regardless of block size |
| `DSM` | 176 | total blocks − 1 (177 blocks total) |
| `DRM` | 63 | directory entries − 1 (64 entries total) |
| `AL0` | `80h` | allocation bitmask — block 0 (only) reserved for the directory |
| `AL1` | `0` | (no further directory blocks) |
| `CKS` | 16 | checksum vector size (nonzero: removable/floppy media) |
| `OFF` | 2 | reserved CP/M tracks before the directory/data area starts |

Block 0 (8192 bytes) holds the entire directory (64 × 32 = 2048 bytes
of real entries; the remaining 6144 bytes of that block are allocated
to the directory but otherwise unused, a normal consequence of
allocating the directory in whole blocks). Blocks 1 through 176 are
available for file data — `MPM.SYS`, the utilities, and `ECHO.RSP`
below share them, allocated back to back in the order
`tools/build_disk.py` processes them.

### The utilities and `ECHO.RSP` (after `MPM.SYS`'s own blocks)

Each of the 19 `src/utils/` `.PLM` utilities the Makefile's own
`PLM_UTILS` list builds, plus `src/rsp/ECHO.A86` (the only RSP that
assembles cleanly — see `RSP_OK` in the Makefile), gets written as its
own file: `<NAME>.CMD` for the utilities, `ECHO.RSP` for `ECHO` — plain
8.3 names matching the Makefile's own basenames (see
`tools/build_disk.py`'s own docstring for why that's not the same as
`docs/build.md` section 5's renamed `STSCMD`/`STSRSP` →
`STAT.CMD`/`MPMSTAT.RSP` mapping).

Each of these files assembles to *two* pieces — a `.cseg.bin` and a
`.dseg.bin` (real initialized data: error-message strings, working
storage, etc.) — so each one gets the same header+code+data shape as
`MPM.SYS` above (`build_two_segment_body`, the same function, reused),
at a shared placeholder code segment (`2000h`): there's no real
loader anywhere in this tree yet that reads a `.CMD`/`.RSP` and runs
it, so this is modeled on `MPM.SYS`'s own confirmed format rather than
any confirmed `.CMD`/`.RSP` convention.

Not yet present: `SUBMIT.CMD` or the other RSPs (`CLOCK`/`TMP`) — their
sources have genuine, deliberately unpatched 1981 bugs and don't
assemble (`docs/bugs.md` #1–#3) — see `tools/build_disk.py`'s own
"KNOWN LIMITATIONS".

## Why some files need more than one directory entry

With `EXM = 0`, one directory entry ("extent") can describe at most
128 128-byte records (16KB), no matter how many blocks its own
allocation array could otherwise address. A file bigger than that
needs `ceil(body_len / 128)` records split across as many directory
entries as it takes — `tools/build_disk.py`'s
`directory_entries_for_file` helper handles this for every file it
places (extent 0 covering records 0–127, extent 1 the next 128, and so
on), each with its own slice of the file's allocation blocks.
`MPM.SYS`'s own body (currently around 29.8KB: a 128-byte header plus
the combined kernel code and data) needs exactly two entries this way;
most of the utilities fit in one.

## A hard-won lesson: don't let a disk read overflow its own buffer

`src/loader/ROM86Q.A86`'s own header read used to target a 9-byte
buffer declared inside its own 512-byte boot sector — but `INT 13h`
(and any real floppy controller) always transfers a full 512-byte
sector regardless of how much of it the caller actually wants, so the
other ~500 bytes silently overwrote every other variable and string
the boot sector declared after that buffer. This was a real, confirmed
bug (see `docs/bugs.md` #13), found only by actually booting the image
in QEMU — nothing about the source or the assembled bytes looked wrong
beforehand. The fix was to point that one read at a fixed low-memory
scratch address (`0500h`) entirely outside the boot sector's own 512
bytes, rather than at a label inside it. This is why the "loader
header" section above gets read into a completely different part of
memory than where it sits on disk, while `MPMLDR.bin`'s own body read
was never at risk the same way (it lands in its own dedicated segment,
`Abs:0000`, with nothing else living there).

## What this doesn't cover yet

No `SUBMIT.CMD` and no other RSPs (`CLOCK`/`TMP`) are placed on this
disk — their sources have genuine, deliberately unpatched 1981 bugs
and don't assemble — see `tools/build_disk.py`'s own "KNOWN
LIMITATIONS". Nothing in this tree yet actually loads a `.CMD`/`.RSP`
file from disk and runs it, either.

More importantly: **this still does not boot**. The boot sector and
`MPMLDR.bin`'s own loading are confirmed working in QEMU, but nothing
has been observed from `LDMPM.A86` itself after it jumps into
`MPMLDR` — see `docs/bugs.md` #14 and `docs/build.md`'s "What's
actually needed to boot under QEMU" item 4 for the current status.

## Building and inspecting it

```
make disk        # build build/disk/mpm86q.img
make run          # boot it in QEMU (builds it first if needed)
```

See `tools/build_disk.py`'s own docstring and source for the exact
mechanics (it's a single, fairly short script — reading it directly is
often faster than cross-referencing this document against it).
