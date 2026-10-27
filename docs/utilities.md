# The `src-assem/utils/` Utilities

The 19 `src-assem/utils/*.PLM` sources — the user-facing commands shipped
with MP/M-86 2.0, as opposed to the kernel (`src-assem/kernel/`), the boot
loader (`src-assem/loader/`), DRI's own dev tools (`src-assem/toolchain/`), or the
Resident System Processes (`src-assem/rsp/`). Each is PL/M-86 source,
compiled by `tools/plmc.py` and assembled by `tools/asm86.py` (see the
Makefile's own `PLM_UTILS` list) into `build/utils/<NAME>.cseg.bin` +
`.dseg.bin`, then combined and placed on the bootable disk image as
`<NAME>.CMD` by `tools/build_disk.py` — see `docs/diskformat.md`'s "The
utilities and `ECHO.RSP`" section for the on-disk format, and this
project's own two-segment header it's wrapped in (nothing in this tree
yet actually loads and runs a `.CMD` file — these are built and placed
on disk, not executed).

All 19 assemble cleanly with this repo's from-scratch toolchain; none
of `src-assem/utils/` needed patching (contrast `src-assem/rsp/`, where three of
the four sources have genuine unpatched 1981 bugs — see
`docs/bugs.md`).

## The commands

| Source | On disk | Purpose |
|---|---|---|
| `ABORT.PLM` | `ABORT.CMD` | Abort (terminate) a running program/process by name. |
| `ATTACH.PLM` | `ATTACH.CMD` | Attach the console to a different process, switching which process a terminal is interacting with. |
| `CONS.PLM` | `CONSOLE.CMD` | Console identification — reports which console (user number) a process is running under. |
| `DIR.PLM` | `DIR.CMD` | Directory listing — lists files matching a pattern on a given drive/user area. |
| `DSKRST.PLM` | `DSKRESET.CMD` | Disk system reset — the MP/M-86 equivalent of CP/M's warm boot disk reset, logging in disks fresh. |
| `ED.PLM` | `ED.CMD` | The standard DRI line editor — by far the largest utility (~56KB of source), a full text editor with its own command language. |
| `ERA.PLM` | `ERA.CMD` | Erase (delete) a file by name, no confirmation. |
| `ERAQ.PLM` | `ERAQ.CMD` | Erase a file, but prompts ("Y/N") for confirmation before deleting — the safer sibling of `ERA`. |
| `PIP.PLM` | `PIP.CMD` | Peripheral Interchange Program — the general-purpose file copy utility (also handles device-to-file and file-to-device transfers). The largest utility by compiled size. |
| `REN.PLM` | `REN.CMD` | Rename a file. |
| `SET.PLM` | `SET.CMD` | Set/change system or per-drive/per-device configuration parameters (e.g. read-only flags, baud rates — see the file's own command table). |
| `SHOW.PLM` | `SHOW.CMD` | Show system or disk status/configuration — the read-only counterpart to `SET`. |
| `SPOOL.PLM` | `SPOOL.CMD` | Print files on the list device in the background: loads, detaches from the console and runs as process `SPOOL` (MP/M-86 has no spooler RSP). |
| `SSPOOL.PLM` | `SSPOOL.CMD` | STOPSPLR: abort the `SPOOL` process. |
| `STSCMD.PLM` | `MPMSTAT.CMD` | The transient half of the MP/M-86 status command (`MPMSTAT`'s command-line front end) — queries and prints system/process status. See "STSCMD/STSCOM/STSRSP" below. |
| `STSCOM.PLM` | `STSCOM.CMD` | Code shared between `STSCMD` and `STSRSP` ($include'd by both, per its own header comment) — compiled standalone here since this tree has no linker, but not a command in its own right. See below. |
| `STSRSP.PLM` | `MPMSTAT.RSP` | The Resident System Process half of the status command (built with `plmc --rsp MPMSTAT`, and also baked into MPM.SYS). Typing `MPMSTAT` queues the command to it. See below. |
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
mpmstat.rsp=stsrsp.cmd`), so `tools/build_disk.py` follows those
headers: `STSCMD` is on the disk as `MPMSTAT.CMD` and `STSRSP` as
`MPMSTAT.RSP` (see below). `STSCOM` stays a plain `STSCOM.CMD`. With the
RSP resident, `MPMSTAT` reaches its queue first; `A:MPMSTAT` runs the
.CMD.

`STSRSP` also runs as DRI shipped it: `tools/build_mpmsys.py` compiles
`STSRSP.PLM` as an RSP (`plmc --rsp MPMSTAT`, RSPASM.A86's shell) and
bakes it into MPM.SYS after TMP/CLOCK/ECHO. Typing `MPMSTAT` queues
the command to it ("MPMSTAT Msg Qued"), and it prints the status on
that console. The same RSP is on the disk as `MPMSTAT.RSP` (the
Makefile's `build/rsp/MPMSTAT`, byte-identical to the baked-in copy
before build_mpmsys patches its code segment). It used to be placed as
`STSRSP.CMD`, which run as a program made its queue and then waited on
it forever, hanging the console. `STSCMD` is the same display as an
ordinary transient.

(MP/M-86 2.0 has no ABORT RSP. Every distribution in `orig/` ships
ABORT only as ABORT.CMD, and the only `abort.rsp` is MP/M-II's 8080
one. The user guide's sample listings showing `ABORT RSP` don't match
any shipped disk.)

## Where these come from

See `docs/orig.md` for which of the original 11 disks each source was
copied from (disks `07`–`09` hold the bulk of `src-assem/utils/`), and
`docs/bugs.md` for the one originally-suspected (and since cleared)
`ED.PLM` issue.
