# Building MP/M-86 2.x and Booting Under QEMU

This document lists the source files needed to build MP/M-86 from
`orig/mpm862sr/`, and explains why it will not boot under QEMU without
porting work. See [orig.md](orig.md) for the general tree layout.

## Hardware target mismatch (read this first)

This source is hard-wired to **Intel iSBC 86/12 CPU board + Intel SBC 204
floppy controller** hardware:

- `04/ROM.A86` is the cold-boot EPROM: on reset it programs the 8251 UART
  (9600 baud), the 8259 PIC (vectors 10h-17h), and the SBC 204 FDC by
  direct port I/O (base port `0A0h`), then reads track 0/1 off an 8" SSSD
  floppy to load `MPMLDR.CMD`.
- `04/XIOS.A86` (the hardware-dependent I/O layer) is written against the
  same SBC 204 controller and iSBC 86/12 serial/interrupt hardware.

QEMU has no iSBC 86/12 machine model, and there is no BIOS-disk-services
(INT 13h style) abstraction anywhere in this tree to fall back on. **The OS
cannot boot unmodified under QEMU.** To run it under QEMU you must write a
new boot ROM/loader and a new XIOS targeting whatever QEMU machine type you
choose (e.g. a generic 8086 PC using `-drive`/INT 13h disk access, or a
custom QEMU machine that emulates the SBC 204 port interface instead). That
is a porting task, not a build task — everything below is just what's
needed to **build** the OS from source.

## 1. Toolchain

None of this exists in runnable form today; it must be sourced or
re-implemented separately before anything else can be assembled:

- `01/A86.CPM`, `03/A86.CPM` — the ASM86 cross-assembler binary
- `03/GENCMD.CPM` — converts ASM86's Intel-hex output into a loadable `.CMD`
- `06/ASS86.PLM`, `06/DIS86.PLM`, `06/GENTAB.PLM`/`.PLI`, `06/INS86.PLM` —
  assembler/disassembler internals (only needed to rebuild `DDT86`, not to
  assemble the OS itself)
- A working PL/M-86 compiler — its own source is in dirs **10**/**11**, but
  bootstrapping it requires an existing PL/M-86 compiler (historically this
  ran on a VAX; see `07/MPM86GEN.TEX`)

## 2. Kernel modules → combined into `MPM.SYS`

| Module | Source files |
|---|---|
| BDOS | `01/BDOS.A86`, `01/BDOSIF.BDO`, `01/DATA.BDO`, `01/EQU.BDO`, `01/FILE1.BDO`–`01/FILE4.BDO`, `01/*.DEF` |
| RTM | `02/RTM.A86` + `02/*.RTM` (`DSPTCH`, `PROC`, `QUE1`, `QUE2`, `FLAG`, `FINDPD`, `ABORT`, `SYSENT`, `RTMIF`) |
| SUP | `02/SUP.A86` + `02/*.SUP` (`COMMAND`, `INIT`, `LOAD`, `PARSE`, `RPL`, `SUPIF`, `SYSFUNC`, `TICK`) |
| MEM | `02/MEM.A86` + `02/*.MEM` (`MEMORY`, `MAU`, `CPMMEM`, `MEMIF`, `ML`, `SHARE`, `UTIL`) |
| CIO | `02/CIO.A86`, `02/CIOIF.CIO`, `03/CHARIO.CIO`, `03/CONSOLE.CIO` |
| XIOS | `04/XIOS.A86` (needs rewriting for the QEMU target — see caveat above) |
| SYSDAT | `02/SYSDAT.A86`, `02/SYSDAT.LIB`, `02/SYSTEM.DAT` |

Shared includes pulled in by the modules above:

- `.DEF` files: `CCB`, `ACB`, `APB`, `PD`, `QD`, `FCB`, `MEM`, `MODTAB`,
  `MODFUNC`, `XIOSCB`, `CPYRIGHT`, `ERR`, `SYSTEM`, `LDTAB`, `CLICB`, `CHAR`
- `.FMT` layout files: `SYSDAT.FMT`, `UDA.FMT`, `BASEP.FMT`, `LSTK.FMT`

## 3. Loader → `MPMLDR.CMD`

- `04/MPMLDR.H86`
- `04/LDMPM.A86` / `04/LDMPM.H86`
- `04/LDBIOS.A86` / `04/LDBIOS.H86`
- `04/LDBDOS.A86` / `04/LDBDOS.H86`
- `04/LBDOSDAT.A86`, `04/LBDOSENT.A86`, `04/LBDOSEQU.A86`,
  `04/LBDOSF1.A86`, `04/LBDOSF2.A86`, `04/LBDOSF3.A86`,
  `04/LBDOSIO.A86`, `04/LBDOSNEW.A86` — BDOS-loader support
- `04/DEBLOCK.LIB`, `04/SINGLES.LIB`, `04/SINGLES.DEF`, `04/SYSDAT.LIB`,
  `04/SYSTEM.LIB`
- `04/ROM.A86` — the cold-boot EPROM (the piece that needs porting to QEMU)
- Build script: `04/MKALL.SUB`

## 4. Resident System Processes (needed for a usable console)

- `05/TMP.A86` → `TMP.RSP`
- `05/ECHO.A86` → `ECHO.RSP`
- `05/CLOCK.A86` → `CLOCK.RSP`
- `05/SUBMIT.A86` → `SUBMIT.CMD` (required to run any `.SUB` batch file)

## 5. Minimum useful command set (optional, but needed to do anything once booted)

- `08/PIP.PLM` → `PIP.CMD` (file copy)
- `08/ED.PLM` → `ED.CMD` (editor)
- `07/DIR.PLM`, `07/TYPE.PLM`, `07/REN.PLM`, `07/ERA.PLM`, `07/ERAQ.PLM`,
  `07/SET.PLM`, `07/SHOW.PLM`
- `09/STSCMD.PLM`, `09/STSCOM.PLM`, `09/STSRSP.PLM` → `MPMSTAT.CMD` /
  `MPMSTAT.RSP` (system status; the names in STSCMD.PLM's and
  STSRSP.PLM's own headers. STAT is a different MP/M-86 command, file
  and disk status.)
- `07/ATTACH.PLM`, `07/TOD.PLM`, `07/SPOOL.PLM`, `07/SSPOOL.PLM`,
  `07/DSKRST.PLM`, `07/ABORT.PLM`, `07/CONS.PLM`

## Build sequence (per `07/MPM86GEN.TEX`)

1. Assemble each kernel module (`BDOS`, `RTM`, `SUP`, `MEM`, `CIO`, `XIOS`,
   `SYSDAT`) with ASM86.
2. Run `GENCMD` on each to convert Intel-hex output to a loadable `.CMD`,
   then rename to `.MPM` (kernel modules) or `.RSP`/`.CMD` (RSPs/utilities).
3. Combine the renamed modules into `MPM.SYS` and `MPMLDR.CMD` using the
   loader sources in `04/`.
4. Utility programs (PL/M sources in `07/`–`09/`) are compiled separately —
   historically on a VAX via the `03/VAX.CPM` cross-development link, then
   uploaded and run through `GENCMD` on the micro.

## What's actually needed to boot under QEMU

1. A working ASM86 assembler + `GENCMD` (or equivalents) to produce `.CMD`
   files from this source — **this part now exists**: `tools/asm86/`
   assembles this dialect (see `tools/README.md`), and all of
   `BDOS.A86`/`MEM.A86`/`RTM.A86`/`SUP.A86`/`CIO.A86`/`SYSDAT.A86` and
   all four loader modules now assemble cleanly end-to-end (see
   `docs/bugs.md`). The loader piece specifically is also now
   **combined**: `tools/build_mpmldr.py` re-does DRI's own
   `orig/mpm862sr/04/MKALL.SUB` build step (`pip
   mpmldr.h86=ldmpm.h86,ldbdos.h86,ldbios.h86` then `gencmd mpmldr 8080
   code[a0400]`) without a real GENCMD, laying `LDMPM.A86`+
   `LDBDOS.A86`+`LDBIOS86Q.A86`'s assembled code back-to-back into one
   `MPMLDR.bin` (`make` builds it, under `build/loader/`). It had to
   lay them out *sequentially* rather than at DRI's own fixed offsets
   (`LDBDOS` at `0406h`, `LDBIOS` at `1200h`, hard-coded in `LDMPM.A86`
   to patch an interrupt-table entry and reach `LDBIOS`'s own init
   code): `tools/asm86`'s larger code (always disp16, always near
   `JMP`/`CALL` — see `tools/README.md`) makes `LDBDOS.A86` alone
   bigger than DRI's entire 3578-byte budget for it. The script keeps
   `LDMPM.A86`'s own `ldbdos_offset`/`mldbios_offset` constants in sync
   with wherever the pieces actually land, re-patching and
   re-assembling `LDMPM.A86` automatically if a future `tools/asm86`
   change moves them.

   The real `04/LDBIOS.A86` (iSBC 86/12 + iSBC 204 FDC + 8251 UART) has
   no QEMU equivalent, so `MPMLDR.bin` is built from a new
   `src/loader/LDBIOS86Q.A86` instead -- same classic CP/M BIOS
   jump-vector calling convention as the real file (so `LDBDOS.A86` is
   unchanged), but reusing `src/kernel/XIOS86Q.A86`'s own NEC765/8250
   driver code for the actual hardware access, and a from-scratch Disk
   Parameter Block computed for the 80-cylinder/2-head/18-sector(512B),
   4:1-deblocked floppy geometry both files now target (`SPT=72`,
   8192-byte blocks, `DSM=176`, 64 directory entries -- `XIOS86Q.A86`'s
   own DPB was updated to match exactly, since both read the same
   on-disk filesystem). See that file's own header comment for the
   full rationale and limitations (it inherits `XIOS86Q.A86`'s own
   "never actually run" caveat).

   The **kernel** side of the same job is also now done:
   `tools/build_mpmsys.py` combines `SUP.A86`/`RTM.A86`/`MEM.A86`/
   `CIO.A86`/`BDOS.A86`/`XIOS86Q.A86` (the real `XIOS.A86` is swapped
   for the new one described in item 2 below, since the real one still
   can't assemble at all — see `docs/bugs.md` #7) into one combined
   code blob and one combined data blob (`make` builds both under
   `build/kernel/`), patching `SYSDAT.A86`'s own module table (the
   `supmod`/`rtmmod`/`memmod`/`ciomod`/`bdosmod`/`xiosmod` far pointers
   every module's own `xiosif`/`mpmif`/`supif`-style trampoline calls
   through) with each module's real, individually-queried `init`/
   `entry` label addresses — there's no `GENSYS-86` source anywhere in
   this distribution to copy a real recipe from (see `docs/orig.md`'s
   "Related DRI source: MP/M II" for why), so this is a from-scratch
   replacement informed by `SYSDAT.FMT`'s own module-table layout and
   `LDMPM.A86`'s documented `MPM.SYS` file-format expectations. See the
   script's own docstring for exactly what it does and doesn't handle
   (a few `SYSDAT` "answers to GENSYS questions" fields, the
   `dispatcher` far pointer's exact target, and the real on-disk
   `MPM.SYS` header format are all flagged there as open items).

   Investigating this surfaced a real, previously-undiscovered
   correctness bug in `tools/asm86` along the way: `BDOS.A86`,
   `MEM.A86`, and `CIO.A86` — all three already in the Makefile's
   known-good list — were silently having their own `CSEG` header
   overwritten with zero bytes by `UDA.FMT`'s `ESEG`/`ORG 0` (an
   unrecognized `ESEG` directive fell through to being parsed as a
   harmless label). Fixed; see `tools/README.md`.
2. A **new XIOS** replacing `04/XIOS.A86` — **a first version now
   exists**: `src/kernel/XIOS86Q.A86`, targeting a standard PC UART
   (8250, polled, COM1) for the console and the standard PC floppy
   controller (NEC 765/8272, PIO, CP/M-style 128-byte sectors deblocked
   4:1 over 512-byte physical sectors) for disk, plus the 8253/8254 PIT
   + 8259 PIC for the scheduler's timer tick. It implements the real
   kernel's exact XIOS calling contract (confirmed against
   `CIOIF.CIO`/`RTMIF.RTM`/`SUPIF.SUP`/`MEMIF.MEM`'s own call sites and
   `XIOSCB.DEF`/`SYSDAT.FMT`/`MODFUNC.DEF`/`PD.DEF`) and assembles
   cleanly with `tools/asm86`, but **has never actually run** — nothing
   in this repo can execute 8086 code or drive QEMU, so the NEC765/8250
   programming sequences follow the chips' own datasheets, not anything
   verified against a real or emulated machine. See the file's own
   header comment for the full list of simplifications (one console,
   one floppy drive). Its disk parameter block, initially a flagged
   placeholder, has since been computed properly for the target
   geometry and kept in sync with `LDBIOS86Q.A86`'s own (see item 1;
   also noted in `src/Makefile`'s own `KERNEL_NEW := XIOS86Q`
   comment).
3. A **new boot sector** replacing `04/ROM.A86` — **done and verified
   working in QEMU**: `src/loader/ROM86Q.A86`, a 512-byte PC boot
   sector (built by `tools/build_bootsect.py` into
   `build/loader/ROM86Q.bin`), since QEMU's SeaBIOS plays the role the
   real iSBC 86/12's EPROM played and hands off to exactly this kind of
   boot sector. Unlike item 2's XIOS replacement, this one *does* lean
   on a BIOS abstraction (BIOS INT 13h) for disk I/O rather than a
   from-scratch NEC765 driver — a deliberate choice (confirmed via
   `AskUserQuestion`): at this exact point in the boot process
   SeaBIOS's own floppy service is present and already working, so
   there's no reason to pay for, or room in 512 bytes for, a second
   copy of the driver `XIOS86Q.A86`/`LDBIOS86Q.A86` already carry.
   Console output, however, is the same direct 8250 UART poll those
   two files use, not BIOS INT 10h — switched to that after first
   testing this file for real turned out to make observing the boot
   process far simpler (see the file's own header comment). It reads a
   new 9-byte header (reusing `ROM.A86`'s own `Ty`/`Len`/`Abs`/`Min`/
   `Max` field layout) from the disk's second physical sector to learn
   where and how much of `MPMLDR.bin` to load, then reads the body in
   and jumps to it. That actual QEMU testing — the first in this whole
   project — caught two real bugs, now fixed: see `docs/bugs.md` #12
   (a `tools/asm86` port-truncation bug affecting every port >= 256 in
   this tree) and #13 (a 512-byte disk read overflowing a 9-byte
   buffer). See the file's own header comment for the exact disk
   layout it expects and its remaining limitations.
4. The kernel modules (section 2) and loader glue (section 3) rebuilt
   against the new XIOS/loader, then combined into an actual bootable
   disk image — **done**: `make disk` (a separate target from plain
   `make`, since it re-invokes the three scripts below itself) runs
   `tools/build_disk.py`, which combines `ROM86Q.bin`, `MPMLDR.bin`,
   and `MPM.bin.{cseg,dseg}` into one 1.44MB floppy image
   (`build/disk/mpm86q.img`): boot sector in physical sector 1, the
   loader header `ROM86Q.A86` reads in sector 2, `MPMLDR.bin` starting
   at sector 3, then a real on-disk `MPM.SYS` file — built by hand to
   the exact header format `LDMPM.A86`'s own code checks for, in a
   real CP/M directory this script also builds by hand from the DPB
   both `XIOS86Q.A86` and `LDBIOS86Q.A86` already carry. See that
   script's own docstring for the full layout and the segments it
   loads things at. **Actually tested in QEMU** (`make run`), unlike
   everything else in this list except item 3's own boot sector: boots
   through SeaBIOS, the boot sector prints its own startup message,
   reads the loader header and all of `MPMLDR.bin`'s body sectors
   (confirmed by its own per-sector progress dots), and jumps into
   `MPMLDR`. Three real bugs were found and fixed this way so far —
   see `docs/bugs.md` #12, #13, and #14 — but **this still does not
   boot**: nothing is observed from `LDMPM.A86` itself after that jump
   (no signon message, nothing from a debug probe placed directly in
   its own code right after it resumes), consistent with a crash
   rather than a benign hang. Pinning this down further needs a real
   debugger attached to QEMU (its GDB stub, `-s -S`) rather than more
   printf-style probing — not yet done.
5. At minimum `SUBMIT.CMD` and a console RSP (section 4) to have a usable
   command line once booted; the utilities in section 5 to do real work.
