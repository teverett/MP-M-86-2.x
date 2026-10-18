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
- `09/STSCMD.PLM`, `09/STSCOM.PLM`, `09/STSRSP.PLM` → `STAT.CMD` /
  `MPMSTAT.RSP` (system status)
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
   files from this source — none exist in runnable form today.
2. A **new XIOS** replacing `04/XIOS.A86`, targeting whichever QEMU machine
   type and disk interface you pick (e.g. a generic 8086 PC using BIOS/INT
   13h-style disk access instead of the SBC 204 port protocol).
3. A **new boot loader** replacing `04/ROM.A86`, compatible with however
   QEMU presents the boot device (e.g. a floppy/IDE image via `-drive`),
   since there is no BIOS abstraction to lean on in this source tree.
4. The kernel modules (section 2) and loader glue (section 3) rebuilt
   against the new XIOS/loader, then combined into `MPM.SYS` /
   `MPMLDR.CMD` as above.
5. At minimum `SUBMIT.CMD` and a console RSP (section 4) to have a usable
   command line once booted; the utilities in section 5 to do real work.
