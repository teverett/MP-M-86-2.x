# Plan: a bootable MP/M-86 floppy built from src-c

Status: done (2026-10-26). All six stages are complete; docs/bugs.md #130 summarizes.

## Goal

`make` in src-c produces a floppy image that boots by itself (QEMU
`-drive ...,if=floppy`, like `./run.sh` does for /src-assem). The image runs
the C kernel and holds only what src-c builds. Nothing from /src-assem is
needed to build it. /src-assem stays the reference, which the *tests* compare
against.

Decisions (from the user):
- **Boot:** src-c's own boot sector and loader. Not GRUB, and not
  QEMU's `-kernel`.
- **Disk:** only src-c's output: the kernel, the programs the C kernel
  can run, and sample files. No 8086 binaries from /src-assem.
- **Build:** remove the dependency on /src-assem's assembled MPM.bin too.

## Where things stand

kernel.elf is a 32-bit Multiboot kernel (linked at 1 MB, loaded with
`-kernel`). Its build depends on /src-assem in three ways:

1. **The memory image.** `rt/rt_asm.s` `.incbin`s
   `build/kernel/MPM.bin.cseg`/`.dseg`, as built by
   tools/build_mpmsys.py (the stand-in for GENSYS). From it come:
   - SYSDAT + DATA.BDO + the module data: 1AD2h bytes, about 2.5 KB of
     it non-zero, in 90 runs;
   - the module code-segment layout (each module paragraph-aligned,
     bugs.md #112) and the module table that points at it;
   - the RSP data segments and the RSPSEG chain;
   - code-segment data that the C reads: SUP's version string
     (s_mpmseg:s_version) and the modules' cs:sysdat / cs:supervisor
     cells.

   SYSDAT's segment (1A5Dh) follows from the code length. It is
   hard-coded in boot/kernel_main.c.
2. **`rt/gen_syms.py` → obj/rt/rt_syms.h:** SUP/RTM code offsets
   (user_retf, notick, idle, user_entry, fdisp, farpdisp), taken from
   assembling /src-assem/kernel.
3. **`rsp/gen_syms.py` → obj/rsp/rsp_syms.h:** the RSP/transient data
   offsets the C images static-assert against, and TMP's code-segment
   message offsets, taken from assembling /src-assem/rsp.

## Stages

### 1. Drop the generated /src-assem headers -- DONE

Done: src-c/rt/rt_syms.h and src-c/rsp/rsp_syms.h are committed (each
`#define` names its `FILE.A86: label`), rt/gen_syms.py and
rsp/gen_syms.py are deleted, and tests/src-c/test_syms.py checks all 112
constants against the assembled source. The only /src-assem input left in the
build is MPM.bin (stage 2).


- Replace rt_syms.h and rsp_syms.h with committed src-c headers. They
  carry the same values, written down like defs.h's offsets.
- A test (extend tests/src-c/test_rsp_images.py, or add a new one)
  assembles /src-assem and checks every constant against the asm symbol
  tables. A change on either side then fails the test, not the build.

### 2. src-c GENSYS: the low-memory image without /src-assem -- DONE

Done, decided as a boot-time C build (src-c/gensys: sysdat.c,
gensys.c, gensys_syms.h). rt_boot and kernel_main build the image with
it, and the MPM.bin `.incbin`s are gone. tests/src-c/test_gensys.py
matches build_mpmsys.py's image (built without MPMSTAT) byte for byte:
SYSDAT, and the code half outside the 8086 code. SYSDAT is at 183Ah.
The kernel.elf build needs nothing from /src-assem.


Build the image kernel.elf starts from (code-segment layout, SYSDAT
with DATA.BDO, RSP data segments) from src-c sources:
- **Each module's initial data** becomes initialized C data at its
  real offsets, the same way the RSP images are done (packed structs,
  static asserts), one file per module:
  - SYSDAT.A86 / SYSTEM.DAT;
  - DATA.BDO, including the BDOS messages;
  - XIOS86Q's tables: DPB/DPH, signon, buffers;
  - the CIO, RTM, MEM and SUP data;
  - the RSP images (already there).
- **GENSYS's work**, as tools/build_mpmsys.py does it now:
  - the module table and module_map;
  - the PD, QD and MD pools and their free lists;
  - the flag table; the qmau buffer;
  - the memory partitions and the lock list;
  - the RSPSEG chain;
  - xiosmod, mpmseg, endseg.

  This runs in src-c: either a host build step that writes the image,
  or C at boot that builds it in memory (decide in this stage; the
  boot-time version needs no extra tool).
- The 8086 code bytes aren't needed, because the C kernel never runs
  them. Code segments become reserved space of the same sizes (so
  segment numbers, and SYSDAT at 1A5Dh, stay the same), plus the few
  cells and strings the C reads there.
- kernel_main and rt take SYSDAT's segment from this layout, not a
  hard-coded 1A5Dh.
- **Test:** compare the src-c image with build/kernel/MPM.bin: all of
  SYSDAT byte for byte, the code-segment cells and strings, and the RSP
  segments. The only documented exclusion is the 8086 code bytes.
- Remove `MPM_IMAGE` and the `.incbin`s.

### 3. Boot sector and loader (src-c/boot/) -- DONE

Done: src-c/boot/bootsect.s (one sector, no second stage needed),
build/boot/bootsect.bin and build/boot/kernel.bin from the Makefile,
src-c/tools/bootsect.py to fill in the KRNL fields, and .bss zeroed in
_start. tests/src-c/test_bootsect.py boots a floppy to `0A>`.


- **A 16-bit boot sector** (GNU as, `.code16`):
  1. read the kernel's sectors with BIOS int 13h (CHS: 18 sectors, 2
     heads), into a buffer below 640K;
  2. enable A20;
  3. load a GDT and enter 32-bit protected mode;
  4. copy the kernel to 1 MB and zero its .bss;
  5. jump to `_start`.

  If 512 bytes isn't enough, use a second stage in the reserved tracks.
- **The kernel** is a flat binary (objcopy of kernel.elf), at the
  sector and with the length the disk builder patches into the boot
  sector.
- Keep the Multiboot header, so `-kernel kernel.elf` still works for
  debugging. kernel_main must not depend on Multiboot state.
- **Watch:** kernel size against the load buffer, the BIOS/PIC state
  after the switch (rt already remaps the PIC and installs its own
  IDT), and the floppy motor/FDC state handed to the C XIOS.

### 4. The disk image (src-c builds it) -- DONE

Done: src-c/tools/mkdisk.py, `make disk` → build/mpm86c.img (KERNEL.SYS
at LBA 52, SUBMIT.CMD, RANDOM.CMD, DEMO.SUB), and
tests/src-c/test_mkdisk.py. It boots by itself and runs SUBMIT DEMO.


- The same CP/M geometry as the XIOS's DPB (2 reserved tracks, 8K
  blocks, 64 directory entries), taken from src-c's own XIOS data
  (stage 2), not from tools/build_disk.py.
- **Files on it:**
  - KERNEL.SYS: the kernel binary, in contiguous blocks, so the boot
    sector reads contiguous sectors. It also shows in DIR, like /src-assem's
    MPM.SYS.
  - SUBMIT.CMD and RANDOM.CMD: CMD files whose data group *is* the C
    program's data image, taken from the compiled object as
    test_rsp_images.py does. The code group is the size LOAD needs;
    the C kernel runs the C main by name.
  - Sample files: a .SUB, and so on.
- Builder: a src-c script (src-c/tools/), sharing nothing with
  tools/build_disk.py.

### 5. Make targets and tests -- DONE

Done: `make run` boots build/mpm86c.img from the floppy; `make run-elf`
boots it with -kernel kernel.elf, for debugging.
tests/src-c/test_kernel_disk.py runs SUBMIT and RANDOM on the src-c
image, booted by itself. The floppy boot test is test_mkdisk.py (and
test_bootsect.py). A clean `make disk`, and the boot and disk tests,
work with src-assem/build/ removed.


- `make disk` → obj/mpm86c.img. `make run` boots it with `-drive`, with
  no `-kernel` and nothing from /src-assem.
- tests/src-c/test_kernel_disk.py boots the src-c image, not the /src-assem
  one.
- **New boot test:** the floppy boots on its own to `0A>`.
- Check that a clean src-c build works with /src-assem's build directory
  removed. The tests may still assemble /src-assem (they're differential).

### 6. Docs -- DONE

Done: docs/src-c.md's new "Building and booting src-c" section (and its
introduction), the README (directory structure, "Building and
booting"), the src-c/Makefile header, boot.s's header, and docs/bugs.md
#129 and #130.


- docs/src-c.md "Running the port": how to build and boot.
- docs/bugs.md: anything found along the way.
