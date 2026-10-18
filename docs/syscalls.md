# Commands the Kernel Recognizes

"Commands" here means the MP/M-86 2.x system calls: the function
numbers a program puts in `CL` before calling the kernel's public
entry point (`INT 224`/`user_entry`, the same convention `src/kernel/
MODFUNC.DEF`'s `f_xxx` constants are built from — e.g. `f_conout equ
(user*0100h)+2`). There are two separate tables in this tree, used at
two different points in the boot process:

- **The real kernel's table** (`src/kernel/`) — what every MP/M-86
  program calls once the kernel (`MPM.SYS`) is running.
- **The loader's own, much smaller table** (`src/loader/`) — a
  CP/M-1.4/2.2-style BDOS built into `LDBDOS.A86`, used only during
  boot, before the real kernel above even exists yet.

Numbers not listed, or marked "unimplemented", dispatch to a
"bad call"/no-op stub rather than crashing — this source tree carries
a lot of reserved-but-never-finished slots, consistent with this
project's documented provenance (see `README.md`).

## Kernel-level function table (`src/kernel/`)

Source of truth: the `SYSENT` dispatch table in `src/kernel/
SYSTEM.DAT` (lines 154–430, one `db module, func#, flags` entry per
`CL` value, 3 bytes each — layout in `src/kernel/ENTTAB.DEF`), walked
by `user_entry` in `src/kernel/SUPIF.SUP` (lines 9–101). `module` is
one of `src/kernel/MODFUNC.DEF`'s module constants (`sup`, `rtm`,
`mem`, `cio`, `bdos`, `xios`, `net`); each module resolves its own
`func#` through its own local table (`CIOIF.CIO`, `RTMIF.RTM`,
`MEMIF.MEM`, `SUPIF.SUP`, `BDOSIF.BDO`).

### Standard CP/M-2–compatible functions

| CL | Name | Handler |
|---|---|---|
| 0 | System Reset | rtm,0 `sysreset_entry` |
| 1 | Console Input | cio,0 `conin_entry` |
| 2 | Console Output | cio,1 `conout_entry` |
| 3 | Raw Console Input | cio,2 `rconin_entry` |
| 4 | Raw Console Output | cio,3 `rconout_entry` |
| 5 | List Output | cio,4 `listout_entry` |
| 6 | Raw Console I/O | cio,5 `dirio_entry` |
| 7 | Get I/O Byte | *unimplemented* |
| 8 | Set I/O Byte | *unimplemented* |
| 9 | Console Write (print `$`-terminated string) | cio,6 `conwrite_entry` |
| 10 | Console Read (buffered) | cio,7 `conread_entry` |
| 11 | Console Status | cio,8 `constat_entry` |
| 12 | Get Version | sup,2 `version_entry` |
| 13 | Disk Reset | bdos,0 |
| 14 | Select Disk | bdos,1 |
| 15 | Open File | bdos,2 |
| 16 | Close File | bdos,3 |
| 17 | Search First | bdos,4 |
| 18 | Search Next | bdos,5 |
| 19 | Delete File | bdos,6 |
| 20 | Read Sequential | bdos,7 |
| 21 | Write Sequential | bdos,8 |
| 22 | Make File | bdos,9 |
| 23 | Rename File | bdos,10 |
| 24 | Return Login Vector | bdos,11 |
| 25 | Return Current (Default) Disk | bdos,12 |
| 26 | Set DMA Address | bdos,13 |
| 27 | Get Allocation Vector Address | bdos,14 |
| 28 | Write-Protect Disk | bdos,15 |
| 29 | Get Read-Only Vector | bdos,16 |
| 30 | Set File Attributes | bdos,17 |
| 31 | Get Disk Parameter Block Address | bdos,18 |
| 32 | Get/Set User Code | bdos,19 |
| 33 | Read Random | bdos,20 |
| 34 | Write Random | bdos,21 |
| 35 | Compute File Size | bdos,22 |
| 36 | Set Random Record | bdos,23 |
| 37 | Reset Drive | bdos,24 |
| 38 | Access Drive | bdos,25 |
| 39 | Free Drive | bdos,26 |
| 40 | Write Random with Zero Fill | bdos,27 |

### CP/M-3 extensions

| CL | Name | Handler |
|---|---|---|
| 41 | Test and Write Record | bdos,28 |
| 42 | Lock Record | bdos,29 |
| 43 | Unlock Record | bdos,30 |
| 44 | Set Multi-Sector Count | bdos,31 |
| 45 | Set BDOS Error Mode | bdos,32 |
| 46 | Get Disk Free Space | bdos,33 |
| 47 | Chain to Program | sup,12 `chain_entry` |
| 48 | Flush Buffers | bdos,35 |
| 49 | *unimplemented* | — |

### CP/M-86 extensions

| CL | Name | Handler |
|---|---|---|
| 50 | Call XIOS | sup,3 `callbios_entry` |
| 51 | Set DMA Base | bdos,36 |
| 52 | Get DMA Base | bdos,37 |
| 53 | Get Max Memory | mem,0 `maxmem_entry` |
| 54 | Get Absolute Max Memory | mem,1 `absmax_entry` |
| 55 | Allocate Memory | mem,2 `cpmalloc_entry` |
| 56 | Allocate Absolute Memory | mem,3 `cpmabsa_entry` |
| 57 | Free Memory | mem,4 `cpmfree_entry` |
| 58 | Free All Memory | mem,5 `freeall_entry` |
| 59 | Load | sup,4 `load_entry` |
| 60–63 | *unimplemented* | — |

### CP/NET functions

| CL | Name | Handler |
|---|---|---|
| 64 | Network Login | net,64 |
| 65 | Network Logoff | net,65 |
| 66 | Network Send Message | net,66 |
| 67 | Network Receive Message | net,67 |
| 68 | Network Status | net,68 |
| 69 | Get Network Config Address | net,69 |
| 70–99 | *unimplemented* | — |

(No network module is built in this tree's own `tools/build_mpmsys.py` —
see that script's own "KNOWN LIMITATIONS" — so 64–69 aren't reachable
in the images this repo actually builds, even though the dispatch
table still lists them.)

### CP/M-3 extensions, continued

| CL | Name | Handler |
|---|---|---|
| 100 | Set Directory Label | bdos,38 |
| 101 | Return Directory Label Data | bdos,39 |
| 102 | Read File XFCB | bdos,40 |
| 103 | Write/Update File XFCB | bdos,41 |
| 104 | Set Current Date and Time | bdos,42 |
| 105 | Get Current Date and Time | bdos,43 |
| 106 | Set Default Password | bdos,44 |
| 107 | Return Serial Number | sup,13 `serial_entry` |
| 108–127 | *unimplemented* | — |

### MP/M functions

| CL | Name | Handler |
|---|---|---|
| 128 | Absolute Memory Request | mem,6 `malloc_entry` |
| 129 | Relative Memory Request | mem,6 `malloc_entry` (same handler as 128) |
| 130 | Memory Free | mem,7 `mfree_entry` |
| 131 | Poll Device | rtm,1 `poll_entry` |
| 132 | Flag Wait | rtm,2 `flag_wait_entry` |
| 133 | Flag Set | rtm,3 `flag_set_entry` |
| 134 | Queue Make | rtm,4 `makeq_entry` |
| 135 | Queue Open | rtm,5 `openq_entry` |
| 136 | Queue Delete | rtm,6 `deleteq_entry` |
| 137 | Queue Read | rtm,7 `readq_entry` |
| 138 | Conditional Queue Read | rtm,8 `creadq_entry` |
| 139 | Queue Write | rtm,9 `writeq_entry` |
| 140 | Conditional Queue Write | rtm,10 `cwriteq_entry` |
| 141 | Delay | rtm,11 `delay_entry` |
| 142 | Dispatch | rtm,12 `dispatch_entry` |
| 143 | Terminate | rtm,13 `terminate_entry` |
| 144 | Create Process | rtm,14 `creat_proc_entry` |
| 145 | Set Priority | rtm,15 `set_prior_entry` |
| 146 | Console Attach | cio,9 `conattach_entry` |
| 147 | Console Detach | cio,10 `condetach_entry` |
| 148 | Set Default Console | cio,11 `setdefcon_entry` |
| 149 | Console Assign | cio,12 `conassign_entry` |
| 150 | Command Line Interpreter (CLI) | sup,5 `clicmd_entry` |
| 151 | Call RPL | sup,6 `rpl_entry` |
| 152 | Parse Filename | sup,7 `parse_entry` |
| 153 | Get Default Console | cio,13 `getdefcon_entry` |
| 154 | Get System Data Address | sup,8 `sysdat_entry` |
| 155 | Get Time-of-Day Address | sup,9 `tod_entry` |
| 156 | Get Process Descriptor (PD) Address | rtm,16 `pd_entry` |
| 157 | Abort Process | rtm,17 `abort_spec_entry` |
| 158–191 | mostly *unimplemented* — MP/M-II extensions below are the exception | — |

### MP/M-II extensions

| CL | Name | Handler |
|---|---|---|
| 158 | Attach List Device | cio,15 `lstattach_entry` |
| 159 | Detach List Device | cio,16 `lstdetach_entry` |
| 160 | Set Default List Device | cio,17 `setdeflst_entry` |
| 161 | Conditional Attach List | cio,18 `clstattch_entry` |
| 162 | Conditional Attach Console | cio,19 `cconattch_entry` |
| 163 | Get MP/M Version Number | sup,11 `mpmversion_entry` |
| 164 | Get Default List Device | cio,20 `getdeflst_entry` |
| 165–191 | *unimplemented* | — |
| 192–255 | reserved MP/M-86 extensions (signal/wait-for-process, open/close/read/write/flush/mimic/make std file, etc.) — all still placeholders in this source tree | — |

### Internal-only functions (not reachable via `CL`)

A handful of `src/kernel/MODFUNC.DEF`'s `f_xxx` constants are
module-to-module calls (one kernel module calling another directly,
via `mpmif`/`callf`) and are never dispatched through the public
`SYSENT` table above:

| Name | Module,func | What it is |
|---|---|---|
| `f_inflagset` | rtm,3 | internal flag-set, used by `entry()`'s special case for the public Flag Set call |
| `f_sleep` | rtm,18 | put the calling process on the system sleep list |
| `f_wakeup` | rtm,19 | wake the top process on the system sleep list |
| `f_findpdname` | rtm,20 | find a Process Descriptor by name |
| `f_share`, `f_maualloc`, `f_maufree`, `f_mlalloc`, `f_mlfree` | mem,8–12 | internal memory-allocator helpers |
| `f_load` | sup,10 | internal "load" routine behind the public Load call |
| `f_cload` | sup,14 | internal chain-load routine behind Chain to Program |
| `f_conprint` | cio,14 | internal "print string" entry, used by `BDOSIF.BDO`'s own error-printing helpers |
| `f_mimic`, `f_unmimic` | cio,21–22 | console mimic/un-mimic |
| `f_bdosterm` | bdos,46 | Terminate Process, BDOS's own internal function-table entry 46 |

## Loader-level function table (`src/loader/`)

This is the separate, much smaller CP/M-1.4/2.2-style BDOS built into
`LDBDOS.A86` — used only while `MPMLDR` is loading the real kernel, not
reachable once `MPM.SYS` is running. Dispatch table: `functab` in
`src/loader/LBDOSENT.A86` (lines 94–111). `CL` 0–40 are the "8080"
functions; `CL` 50–57 (offset by `sfunc86 equ 50`) are the "8086"
extensions appended after them — `CL` 41–49 is a gap, not dispatched.
Most slots are unimplemented stubs (`ret`/`return`) left over from the
real CP/M-86 LDBIOS/LDBDOS this was trimmed down from; only a handful
actually do anything, since the loader only ever needs to read one
file (`MPM.SYS`) off the boot disk.

| CL | Name | Behavior |
|---|---|---|
| 0 | System Reset | jumps to the BIOS warm-boot entry |
| 1 | Console Input | stub |
| 2 | Console Output | expands tabs, writes to console |
| 3 | Raw Console Input | stub |
| 4 | Punch Output | direct BIOS punch-device call |
| 5 | List Output | direct BIOS list-device call |
| 6 | Raw Console Output | stub |
| 7 | Get I/O Byte | stub |
| 8 | Set I/O Byte | stub |
| 9 | Print String | writes a `$`-terminated string (pointed to by `DX`) to the console |
| 10 | Console Read | stub |
| 11 | Console Status | stub |
| 12 | *(unused slot)* | stub |
| 13 | Disk Reset | resets the read-only/login/current-disk state, selects disk 0 |
| 14 | Select Disk | selects the disk number in the low byte of `DX` |
| 15 | Open File | copies the 33-byte FCB in from the caller, re-selects the disk, opens the file |
| 16 | Search First | stub |
| 17 | Search Next | stub |
| 18 | Delete File | stub |
| 19 | *(unused slot)* | stub |
| 20 | Read Sequential | copies the FCB in, re-selects the disk, performs a sequential read |
| 21 | Write Sequential | stub |
| 22 | Make File | stub |
| 23 | Rename File | stub |
| 24 | Return Login Vector | stub |
| 25 | Return Current Disk | stub |
| 26 | Set DMA Address | sets the DMA address from `DX` |
| 27 | Get Allocation Vector | stub |
| 28 | Write-Protect Disk | stub |
| 29 | Get Read-Only Vector | stub |
| 30 | Set File Attributes | stub |
| 31 | Get Disk Parameter Block | stub |
| 32 | Get/Set User Code | stub |
| 33 | Read Random | stub |
| 34 | Write Random | stub |
| 35 | Compute File Size | stub |
| 36 | Set Random Record | stub |
| 37 | Reset Drive | stub |
| 38 | Access Drive | no-op |
| 39 | Free Drive | no-op |
| 40 | Write Random with Zero Fill | stub |
| 50 | *(falls through to 51's code)* | no-op |
| 51 | Set Segment Base for Disk I/O | sets the DMA segment base from `DX` |
| 52–57 | *(8086 extension stubs)* | no-ops |

## Sources

- `src/kernel/SYSTEM.DAT` lines 154–430 — the master `SYSENT` dispatch
  table, with an inline comment naming every `CL` slot.
- `src/kernel/SUPIF.SUP` lines 9–153 — the `user_entry` dispatcher.
- `src/kernel/CIOIF.CIO`, `RTMIF.RTM`, `MEMIF.MEM`, `BDOSIF.BDO` — each
  module's own local function table that `SYSENT` resolves into.
- `src/kernel/MODFUNC.DEF` — the symbolic `f_xxx` EQU mirror of the
  same numbering, used by caller code throughout the kernel.
- `src/kernel/ENTTAB.DEF` — the byte layout of one `SYSENT` entry.
- `src/loader/LBDOSENT.A86` lines 79–111 — the loader-level dispatch
  table, and `LBDOSIO.A86`/`LBDOSF3.A86`/`LBDOSNEW.A86` for its
  functions' actual implementations.
