# The `src/utils/` Utilities

The 19 `src/utils/*.PLM` sources — the user-facing commands shipped
with MP/M-86 2.0, as opposed to the kernel (`src/kernel/`), the boot
loader (`src/loader/`), DRI's own dev tools (`src/toolchain/`), or the
Resident System Processes (`src/rsp/`). Each is PL/M-86 source,
compiled by `tools/plmc.py` and assembled by `tools/asm86.py` (see the
Makefile's own `PLM_UTILS` list) into `build/utils/<NAME>.cseg.bin` +
`.dseg.bin`, then combined and placed on the bootable disk image as
`<NAME>.CMD` by `tools/build_disk.py` — see `docs/diskformat.md`'s "The
utilities and `ECHO.RSP`" section for the on-disk format, and this
project's own two-segment header it's wrapped in (nothing in this tree
yet actually loads and runs a `.CMD` file — these are built and placed
on disk, not executed).

All 19 assemble cleanly with this repo's from-scratch toolchain; none
of `src/utils/` needed patching (contrast `src/rsp/`, where three of
the four sources have genuine unpatched 1981 bugs — see
`docs/bugs.md`).

## The commands

| Source | On disk | Purpose |
|---|---|---|
| `ABORT.PLM` | `ABORT.CMD` | Abort (terminate) a running program/process by name. |
| `ATTACH.PLM` | `ATTACH.CMD` | Attach the console to a different process, switching which process a terminal is interacting with. |
| `CONS.PLM` | `CONS.CMD` | Console identification — reports which console (user number) a process is running under. |
| `DIR.PLM` | `DIR.CMD` | Directory listing — lists files matching a pattern on a given drive/user area. |
| `DSKRST.PLM` | `DSKRST.CMD` | Disk system reset — the MP/M-86 equivalent of CP/M's warm boot disk reset, logging in disks fresh. |
| `ED.PLM` | `ED.CMD` | The standard DRI line editor — by far the largest utility (~56KB of source), a full text editor with its own command language. |
| `ERA.PLM` | `ERA.CMD` | Erase (delete) a file by name, no confirmation. |
| `ERAQ.PLM` | `ERAQ.CMD` | Erase a file, but prompts ("Y/N") for confirmation before deleting — the safer sibling of `ERA`. |
| `PIP.PLM` | `PIP.CMD` | Peripheral Interchange Program — the general-purpose file copy utility (also handles device-to-file and file-to-device transfers). The largest utility by compiled size. |
| `REN.PLM` | `REN.CMD` | Rename a file. |
| `SET.PLM` | `SET.CMD` | Set/change system or per-drive/per-device configuration parameters (e.g. read-only flags, baud rates — see the file's own command table). |
| `SHOW.PLM` | `SHOW.CMD` | Show system or disk status/configuration — the read-only counterpart to `SET`. |
| `SPOOL.PLM` | `SPOOL.CMD` | Start spooling console/file output to the print spooler RSP. |
| `SSPOOL.PLM` | `SSPOOL.CMD` | Stop spooling started by `SPOOL`. |
| `STSCMD.PLM` | `STSCMD.CMD` | The transient half of the MP/M-86 status command (`MPMSTAT`'s command-line front end) — queries and prints system/process status. See "STSCMD/STSCOM/STSRSP" below. |
| `STSCOM.PLM` | `STSCOM.CMD` | Code shared between `STSCMD` and `STSRSP` ($include'd by both, per its own header comment) — compiled standalone here since this tree has no linker, but not a command in its own right. See below. |
| `STSRSP.PLM` | `STSRSP.CMD` | The Resident System Process half of the status command, cooperating with `STSCMD` to gather status MP/M keeps per-process rather than globally. See below. |
| `TOD.PLM` | `TOD.CMD` | Time of day — gets/sets the system clock. |
| `TYPE.PLM` | `TYPE.CMD` | Type (display) a text file's contents to the console. |

## STSCMD / STSCOM / STSRSP

These three together implement one logical command, `MPMSTAT` (system
status). `STSCOM.PLM`'s own header comment calls it "Common Include
Module for RSP and Transient MPMSTAT" — it's meant to be `$include`'d
into both `STSCMD.PLM` (the transient/command side) and `STSRSP.PLM`
(the resident-process side) by DRI's own build, not assembled as an
independent program. This tree's toolchain has no linker (see
`tools/README.md`), so `STSCOM.PLM` is compiled and placed on disk as
its own `.CMD` here rather than folded into the other two — a
deviation from the original build, noted so `STSCOM.CMD`'s presence on
disk isn't mistaken for a fourth independent command.

`docs/build.md`'s own "minimum useful command set" section renames
these three to `STAT.CMD`/`MPMSTAT.RSP`, following DRI's own documented
`GENCMD`+`REN` build step — but that section's mapping actually
disagrees with `STSCMD.PLM`'s and `STSRSP.PLM`'s own header comments on
which file becomes which name (`ren mpmstat.cmd=stscmd.cmd`, `ren
mpmstat.rsp=stsrsp.cmd`), so `tools/build_disk.py` keeps all three as
plain `<NAME>.CMD` on disk rather than guess at a rename.

## Where these come from

See `docs/orig.md` for which of the original 11 disks each source was
copied from (disks `07`–`09` hold the bulk of `src/utils/`), and
`docs/bugs.md` for the one originally-suspected (and since cleared)
`ED.PLM` issue.
