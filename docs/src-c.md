# `/src-c`: a C port of the hand-written MP/M-86 assembly

`/src-c` is a routine-by-routine C port of the hand-written ASM86 in
`src/kernel/`, `src/loader/`, and `src/rsp/`. It is **not** an attempt to
produce C that compiles to the same bytes as `tools/asm86/`'s output —
that was the original goal, but a representative real routine
(`src/kernel/FINDPD.RTM`) makes clear why it isn't realistic: hand-tuned
tricks like folding a sign/zero test into a `shl al,1`, or a deliberate
segment-override byte compare, aren't something any C compiler will
spontaneously emit from idiomatic C. Forcing a byte match would mean either
wall-to-wall inline asm, or fighting one specific compiler's quirks
indefinitely.

Instead this follows the same approach the user's other project,
[`CP-M-C`](https://github.com/teverett/CP-M-C) (a C port of DRI's CP/M 2.2
CCP/BDOS from 8080 assembly), already settled on for the identical problem:
same routine/variable names, same control flow (`goto`s included),
translated one routine at a time, with equivalence proven by **differential
testing** against the real assembled machine code rather than a byte diff.
See the plan this effort started from for the full rationale:
`/Users/tom/.claude/plans/gleaming-seeking-cocke.md`.

## How a routine gets ported

1. Write `src-c/<dir>/<file>.c`, translating the `.RTM`/`.SUP`/`.MEM`/`.CIO`/
   `.BDO` file's logic 1:1 (same labels as `goto` targets, same variable
   names, register pairs as `uint16_t`, byte ops as `uint8_t` for correct
   wraparound). Field offsets it needs go in `src-c/<dir>/defs.h`, added to
   from the relevant `.DEF`/`.FMT` file and cross-checked against
   `python3 tools/asm86.py <file> --symbols`.
2. Add a differential test under `tests/src-c/test_<routine>.py`
   (`test_findpd.py` is the template): assemble the real routine with
   `tools/asm86.py` (via a small per-file wrapper `.A86` under
   `tests/src-c/fixtures/`, since most real files rely on equates normally
   supplied by their parent module's own `INCLUDE` chain), run the result
   through `tools/i86sim.py`, compile the C port and call it (via `ctypes`)
   against the *same* synthetic memory image, and diff the outcome.
3. `tools/i86sim.py` is a minimal 8086 interpreter built only as this test
   oracle — grown lazily, one opcode at a time, as each new routine needs
   it. An unimplemented opcode raises loudly (`I86SimError`) rather than
   silently decoding something wrong; that's the signal to extend it.

### The boundary-stub convention

Several real routines never return in the normal sense: they tail-jump
into a not-yet-ported module (the dispatcher `dsptch`/`pdisp`, the XIOS
trampoline `xiosif`, ...), or call into one mid-routine and keep going.
Rather than invent fictional behavior for an unported routine, `/src-c`
treats the call/jump itself as the boundary of what's being tested — the
same idea as CP-M-C's own `cpm_bios` struct stubbing out BIOS calls:

- A **tail JMP** into an unported routine is a `tools/i86sim.py`
  `stop_addrs` entry: the simulator halts the instant it reaches that
  address, and the test compares the register/memory state right there
  (e.g. `tests/src-c/test_idle.py`).
- A **mid-routine CALL** into an unported routine (one the caller keeps
  running after) is a `call_stub_addrs` entry: treated as an instant
  no-op return. This is accurate, not a simplification of convenience —
  from the calling process's own point of view, a dispatcher call either
  never returns to it at all (so whatever it would have left in the
  registers is moot) or resumes it exactly here once it's rescheduled.

Also not modeled: `tools/i86sim.py`'s CALL/RET use a real stack-based
return address (with a sentinel the harness pushes before the first
instruction), so an *internal* call/return within one file — e.g.
FLAG.RTM's `flag_set_entry` calling its own `flag_valid` — round-trips
normally without needing a stub at all.

### Test-harness memory addresses aren't the real hardware offsets

`src/kernel/SYSDAT.FMT` (System Data Area) and `UDA.FMT` (one process's
User Data Area) are each their own segment on real hardware, with small
offsets (e.g. SYSDAT's `rlr` at `0x68`) that only make sense relative to
their own segment base. `tools/i86sim.py` models memory as a single flat
64KB array with no real segment registers, so reusing those small offsets
directly would collide with a routine's own code. Every Tier-1+ port and
its wrapper instead place these fields at fixed, harness-chosen scratch
addresses (`SD_RLR`, `UDA_WRKSEG`, etc. in `src-c/kernel/defs.h`) —
consistently, across every routine that touches them, mirroring how they
really are "the same location" system-wide. A wrapper always declares
these as real DSEG labels at that address, never a plain EQU constant,
since EQU would wrongly assemble a read of the field (`mov bx,rlr`) as an
immediate load instead of a memory read.

## Status

| File | Routines ported | Routines deferred (and why) |
|---|---|---|
| `src/kernel/FINDPD.RTM` | `findpdname_entry` | — |
| `src/kernel/IDLE.SUP` | `idle` | — |
| `src/kernel/SYSFUNC.SUP` | all 8 entries: `not_implemented`, `bad_entry`, `version_entry`, `mpmversion_entry`, `sysdat_entry`, `tod_entry`, `serial_entry`, `callbios_entry` | — (the whole file is ported; `tod_entry`/`serial_entry` copy from real `SYSDAT.FMT`/`RTMIF.RTM` data, given their own harness-chosen scratch address/inlined literal; `callbios_entry`'s two real tail-jumps into `mpmif(f_terminate)`/`xiosif` are boundary stubs, same convention as `SYSENT.RTM`'s own `poll_entry`/etc.) |
| `src/kernel/SYSENT.RTM` | all 8 entries: `poll_entry`, `delay_entry`, `dispatch_entry`, `set_prior_entry`, `pd_entry`, `sleep_entry`, `wakeup_entry`, `creat_proc_entry` | — (the whole file is ported; `creat_proc_entry` — `call proc_creat ! jmp pdisp` — was deferred until `proc_creat`/`pdisp` existed, and now that they do it's a real internal call into both, not a boundary stub like its 7 siblings' own tail-jumps; a real quirk this surfaced: `ax`/`bx`/`cx`/`si`/`di` aren't actually preserved across the `call proc_creat` at all, see `src-c/kernel/sysent.c`'s own header comment) |
| `src/kernel/FLAG.RTM` | `flag_set_entry`, `flag_wait_entry` (+ internal `flag_valid`/`flag_exit`/`flag_bexit`) | — |
| `src/kernel/ABORT.RTM` | `sysreset_entry`, `terminate_entry` (+ internal `abt_chk`), `abort_spec_entry` (+ internal `find_pd`, `find_pdthrd`, `findpdnc`, `findpdq`, `fflgpd`, `snip_it`, `abt_putdrl`, `gccba`, and the whole `abort_tab` dispatch: `abtrun`/`abtpol`/`abtdly`/`abtswp`/`abtdq`/`abtnq`/`abtflg`/`abtcns`) | — (the whole file is ported; needed `tools/i86sim.py`'s new `CALL r/m16`/`JMP r/m16` for `call cs:abort_tab[di]` — see below) |
| `src/kernel/DSPTCH.RTM` | `fdisp`, `int_disp_exit`, `farpdisp`/`pdisp`, `dsptch` (+ internal `insert_process`, `schedule`, `switch_`, and the whole `dsp_table` dispatch: `disp_act`/`poll_act`/`delay_act`/`swap_act`/`term_act`/`sleep_act`/`flag_act`) | — (the whole file is ported, but **as a reading/debugging aid for the live scheduler-hang investigation, #83-#98, not a kernel-build component** — see the file's own header comment; a real interrupt return and a real context switch's register/stack resumption have no C equivalent, modeled as explicit output parameters instead; needed two more `tools/i86sim.py` fixes, `SS`-aware `PUSH`/`POP` and a `CS`-base fix for near indirect `CALL r/m16`/`JMP r/m16` — see #103) |
| `src/kernel/PARSE.SUP` | `parse_entry` (+ internal `setfld`/`delim`/`deblank`/`fcbinit`) | — |
| `src/kernel/RPL.SUP` | `rpl_entry` | — (the file's one routine is ported, but collapses to its single deterministic `rpl_err` outcome under the boundary-stub convention — `mpmif(f_qopen)`'s own function code, MODFUNC.DEF's `(user*0100h)+135`, is never 0, so `cmp cx,0 ! jne rpl_err` is always taken; the real success path — a hand-built fake far-return stack frame and a `retf` transferring control into a queue-delivered RSP entry point, with no C equivalent at all — is provably unreachable, same reasoning as `CONSOLE.CIO`'s/`CPMMEM.MEM`'s own unreachable branches, see `src-c/kernel/rpl.c`'s own header comment) |
| `src/rsp/ECHO.A86` | `main`/`loop` (one full pass — see below) | — |
| `src/rsp/CLOCK.A86` | `clockloop` (one full pass — see below) | — (`clock`'s own one-time setup, `mov ds,sysdat`/`mov bx,tod_offset`, is moot under the flat memory model, same as the DS/ES-switch convention elsewhere — the test starts directly at `clockloop` with `bx` preset instead) |
| `src/rsp/TMP.A86` | `nextcommand_once` (one full pass — see below) | `tmp:`'s own one-time setup — moot, every operation in it is a boundary-stub call or a register load feeding one; `goodparse` and everything downstream of it (`try_builtin`/`notuser`/`not_builtin`'s own real CLI dispatch) — provably unreachable under the boundary-stub convention, see below |
| `src/rsp/SUBMIT.A86` | `deblank`+`fill_argtable` (unlabeled code after `submit:`'s own `call openfile`), `expand_command`, `readbuffer`, `readchar`, `topofcmdloop_once` (one full pass — see below) | `submit:`'s own one-time version-check/priority/FCB-open preamble — no observable effect under the boundary-stub convention beyond what `deblank`/`fill_argtable` already cover; `clicall`'s own real CLI dispatch — provably unreachable past `notdrive`'s own `call parsefilename`, same wall `TMP.A86`'s `goodparse` hit, see below |
| `src/kernel/TICK.SUP` | `notick_once`, `tick_once` | — (both of this file's real labels are ported) |
| `src/kernel/QUE1.RTM` | all 10 routines: `makeq_entry`, `openq_entry`, `deleteq_entry`, `queverify`, `readq_entry`/`creadq_entry`/`readq`, `writeq_entry`/`cwriteq_entry`/`writeq` | — (the whole file is ported; `readq`'s/`writeq`'s own real "wait forever" retry — unconditional call, queue empty/full — is translated faithfully but not exercised by the test, same unexercisable-but-real shape as `cioattach` below/`CPMMEM.MEM`'s `cpmfree_entry`; every other path, including the conditional-and-empty/full immediate-error case, is fully portable and exercised) |
| `src/kernel/QUE2.RTM` | `rlsmx`, `getqdaddr`, `remqd`, `qspace`, `qrelease` | — (all 5 of this file's routines are ported) |
| `src/kernel/PROC.RTM` | `freepd`, `proc_creat` (+ internal `getpdadr`) | — (all 3 of this file's routines are ported, using `tools/i86sim.py`'s `segmented=True` mode plus a CSEG_BASE-shifted reference run — see below) |
| `src/kernel/CONSOLE.CIO` | all 15 routines (`lstattach_entry`, `clstattch_entry`, `cconattch_entry`, `conattach_entry`, `cioattach`, `lstdetach_entry`, `condetach_entry`, `ciodetach`, `conassign_entry`, `getdefcon_entry`, `getdeflst_entry`, `setdefcon_entry`, `setdeflst_entry`, `mimic_entry`, `unmimic_entry`) + `CHARIO.CIO`'s `getccbadr` (a small dependency, pulled in on its own) | — |
| `src/kernel/CHARIO.CIO` | all 9 entry points (`conin_entry`, `conout_entry`, `rconin_entry`, `rconout_entry`, `listout_entry`, `constat_entry`, `dirio_entry`, `conwrite_entry`, `conprint_entry`, `conread_entry`) + 20 internal routines (`rawinit`, `coninit`, `getconnum`, `attach`, `echoc`, `conbrk`, `chkctl`, `control_p`, `prcsmsg`, `prname`, `conout`, `compout`, `ctlout`, `tabout`, `pspace`, `pbacksp`, `pcr`, `crlf`, `crlfp`, `conin`) | `stdout` — its own `conoutf`/`listf` calls are both no-ops under the boundary-stub convention regardless of which branch is taken, so it collapses to nothing observable; inlined as a comment in `conout`'s own mimic-forwarding branch instead of a separate function, see that file's own header comment |
| `src/kernel/COMMAND.SUP` | `shifttail`, `fileopen`, `user_retf` | `chain_entry`/`clicmd_entry` (the file's own two real entry points) and everything only they call (`cli_cli`, `cli_copy`, `cli_parse`, `cli_fload`'s own chain-mode UDA copy, etc.) — real segment-relative addressing, same class as `PROC.RTM`/`ML.MEM`; `crlf`/`prchar`/`prtime`/`prnum`/`prdisk`/`prcusp`/`prcsm`/`prtyp`/`prnam`/`prn1`/`cprnt`/`pruser`/`prfilnam`/`mxwrite`/`mxread`/`mxw1`/`jmpm`/`pfn`/`conasn`/`fileclose` — each collapses entirely to register setup before a boundary-stub tail call with nothing downstream ever reading the result: nothing to port, same reasoning as `freeall_entry` (`src-c/kernel/cpmmem.c`) |
| `src/kernel/FILE1.BDO` | `pererror`/`roderror`/`roferror`/`selerror` (+ internal `goerr`), `move`, `selectdisk`, `home`, `PASS_ARECORD`, `rdbuff`, `wrbuff`/`wrbufflg` (+ internal `diocomp`), `seekdir`/`seek`, `dmposition`, `GET_DMA`, `getdm`, `bdos_index`, `alloct`, `atran`, `GET_ATTS`, `GET_S1`, `GET_RRA`, `GET_RCNTA`, `getexta`, `gtfcba`, `getfcb`, `setfcb`, `cmpecs`/`CMPEC0`, `CHKSUM_FCB`, `SET_CHKSUM_FCB`, `RESET_CHECKSUM_FCB`, `CHEK_FCB`, `setcdisk`, `nowrite`, `setro`, `ckrodir`/`ckrofile`/`RO_TEST`, `checkwrite`, `getdptra`, `getmodnum`, `clrmodnum`, `CLR_EXT`, `setfwf`, `compcdr`, `setcdr`, `rddirbuf`, `setdata`/`setdir`/`setdma`, `dirtouser`, `MAKE_FCB_INV`, `CHK_INV_FCB`, `TST_INV_FCB`, `endofdir`, `setenddir`, `getallocbit`, `setallocbit` | `CHECK_FCB` (+ `CHEK_FCB`'s own MPM branch calling it) — its own `PACK_SDCNT`/`SEARCH_OLIST` dependency is since ported (`src-c/kernel/file3.c`), but it also calls `CLOSE1` (`FILE2.BDO`), which cascades into `CLOSE_FCB`/`GET_DIR_EXT`/`RESTORE_RC`/`srch`/`getdptra` -- not scoped yet, so still deferred; `checksum`/`newchecksum`, `wrdir`, `rddir`/`rddir0` — each calls into `FILE2.BDO`/`FILE3.BDO`'s own real, not-yet-ported logic (`FLUSH_FILE0`/`FLUSH_FILES`, themselves blocked by a real segment-relative-addressing gap, see `FILE3.BDO`'s own status-table row); `subdh` — its only two real callers are `checksum` and `rddir`, both deferred: nothing to port |
| `src/kernel/FILE2.BDO` | `GET_NALBS`, `getblock` (+ internal `lefttst`/`righttst`/`rblok`/`rblok0`) | `SEARCH`/`SEARCHN`/`DELETE`/`RENAME`/`OPEN`/`MAKE` and the rest of the file (80+ labels) — all route through `srch`, which calls `rddir` (`FILE1.BDO`'s own deferred routine, blocked transitively by `FLUSH_FILE0`'s real segment-relative-addressing gap, `FILE3.BDO`); `GET_DIR_EXT`/`RESTORE_RC` are themselves clean, self-contained FCB arithmetic, but their only real caller `CLOSE_FCB` (part of `CLOSE1`) also calls `srch` — porting them alone wouldn't un-defer anything |
| `src/kernel/FILE3.BDO` | `PACK_SDCNT`, `COMPARE`, `SEARCH_OLIST`/`SEARCH_LLIST`/`SEARCHN_LIST` (+ internal `search_listo`), `DELETE_ITEM`, `CREATE_ITEM`, `SET_OLIST_ITEM`, `TST_TBL_LMT`, `REMOVE_LOCKS`, `FLUSH_FILES`/`FLUSH_FILE0`, `INC_PDCNT` | `TST_OLIST`/`CHK_OLIST`/`PR_TERM` — `CHK_OLIST` calls the still-unported `openx06`/`compare_pds` and relies on its own caller having pushed a word for it to `pop` partway through (an implicit calling convention, not a boundary stub); `PR_TERM` calls the still-unported `func48`; one further routine (not identified by name) uses `std` (now resolvable, `tools/i86sim.py` tracks the direction flag — not reconned this pass); the rest of the file not reconned this pass |
| `src/kernel/FILE4.BDO` | `func24`, `func25`, `func29`, `func32`, `FUNC44`, `FUNC45`, `func51`, `func52` | `func13`-`func23`, `func30`, `func33`-`func43`, `FUNC46`, `FUNC48` — all route through `srch`/`rddir`, `CURSELECT`/`initialize`, `CHECK_FCB`, or `FLUSH_FILES`, all transitively blocked by `FLUSH_FILE0`'s real segment-relative-addressing gap (`FILE3.BDO`); `GOBACK` — a 2-instruction tail jump into `BDOSIF.BDO`'s own module-dispatch epilogue (`BDOS_RETURN`), the same jump-table-dispatcher class already deferred elsewhere, not worth porting alone; the rest of the file not reconned this pass |
| `src/kernel/DATA.BDO` | — | pure data (BDOS-wide scratch fields + the `XERR_LIST` error-message table) — zero real code, nothing to port |
| `src/kernel/BDOSIF.BDO` | — | `init`/`entry`/`getdiskmx`/`CALL_BDOS`/the whole SHELL mechanism (`SHELL`/`CBDOS`/`SHELL_ERR`/`SHELL_RTN`/`INCR_RR`/`SAVE_RR`/`RESET_RR`/`TST_WRT`/`MULT_IO`/`COMPARE_RECS`/`BDOS_RETURN`) — this module's own central dispatch, needing far CALL/RET support (`callf cs:dword ptr .supervisor`/`.xiosmod`) that doesn't exist yet, the same deferred class as `MEMIF.MEM`/`CIOIF.CIO`/`RTMIF.RTM`/`SUPIF.SUP`; `XCRLF`/`XPRINT`/`staret`/`funcret`/`setlret1`/`parsave`/`parsave33`/`parsave36`/`parunsave`/`entryzero`/`mpmif`/`xiosif`/the `bootf`-family boundary stubs (`homef`/`seldskf`/.../`flush`) — each either collapses to register setup before a boundary-stub tail call (already accounted for directly inline in `file1.c` where it matters, e.g. `staret`'s own effect) or is transitively blocked by the same `entry:`/SHELL machinery (`parsave`/`parunsave`'s own `SHELL_FLAG` dependency); read in full, nothing left to port |
| `src/kernel/UTIL.MEM` | `getmd`, `freemd` | — (both of this file's routines are ported) |
| `src/kernel/ML.MEM` | `mlalloc_entry` (+ internal `mla_value`, `mla_makemau`, `mla_initmau`), `mlfree_entry` (+ internal `ml_insert`) | — (all of this file's routines are ported, using `tools/i86sim.py`'s new `segmented=True` mode — see below) |
| `src/kernel/MAU.MEM` | `maualloc_entry`, `maufree_entry` (+ internal `mau_split`, `mau_insert`, `mau_release`, `mau_collapse`) | — (all 6 of this file's routines are ported; see below for 2 real bugs found and fixed in `/src`, and 2 `tools/i86sim.py` gaps fixed along the way: a `LEA`-under-`segmented=True` bug and missing `STD`/`CLD` support) |
| `src/kernel/SHARE.MEM` | `share_entry` | — (this file's one routine is ported) |
| `src/kernel/LOAD.SUP` | `load_finish` (a named tail portion of `load:`), `diskread`, `load_group` — see below | `cload_entry`/`load_entry`/`load:`'s own remaining front half (`get_ch_info`'s own CMD-header parsing, and allocating/spreading memory across `ldtab`'s entries) — `MEMORY.MEM`'s own `malloc_entry` (`memory.c`) now makes real `mpmif(f_malloc)`-equivalent calls (see that file's own row below), so this is no longer blocked on disk I/O *or* allocation in principle; not yet attempted, since `load:`'s own front half needs its own differential-test design (CMD header parsing, `ldtab` spreading across multiple groups) separate from `diskread`/`load_group` |
| `src/kernel/MEMORY.MEM` | `malloc_entry`, `mfree_entry` (+ internal `mall_try_mau`, `mfree_it`) | — (the whole file is ported; `malloc_entry`'s own `mpmif(f_maualloc)`/`mpmif(f_mlalloc)` are now real internal calls into `mau.c`'s `maualloc_entry`/`ml.c`'s `mlalloc_entry` — both fully ported already — rather than boundary stubs; `mfree_entry`'s own `mpmif(f_maufree)` is a separate, still-deferred upgrade, see `memory.c`'s own header comment for why its own tail-jump-out-of-two-call-frames shape needs more care) |
| `src/kernel/CPMMEM.MEM` | `maxmem_entry`, `absmax_entry`, `cpmalloc_entry`, `cpmabsa_entry`, `cpmfree_entry` (+ internal `getmemory`, `free_memory`) | `freeall_entry` — one line (`mov cx,f_terminate ! sub dx,dx ! jmp mpmif`), same as `SYSENT.RTM`'s `creat_proc_entry`: nothing to port |
| `src/loader/LBDOSIO.A86` | `conbrk`, `conout`, `tabout`, `crlf`, `print`, `func9`, `staret`, `funcret`, `setlret1` | `func1`/`func3`/`func6`/`func7`/`func8`/`func10`/`func11` — each just an `EQU` alias for the bare `return:` label: nothing to port. `func2` is likewise just an alias for `tabout` itself, already ported under its own name |
| `src/loader/LBDOSF1.A86` | `selectdisk`, `home`, `rdbuff`, `wrbuff`, `seekdir`, `seek`, `dmposition`, `getdm`/`getdmd`, `bdos_index`, `alloct`, `atran`, `getexta`, `gtfcba`, `getfcb`, `setfcb`/`setfc1`, `cmpecs`/`cmpec0`, `setcdisk`, `nowrite`, `setro`, `ckrodir`/`ckrofile`, `checkwrite`, `getdptra`, `getmodnum`, `clrmodnum`, `setfwf`, `compcdr`, `setcdr`, `subdh`, `newchecksum`/`checksum`, `wrdir`, `rddirbuf`, `dirtouser`, `endofdir`, `setenddir`, `rddir`/`rddir0`, `getallocbit`, `setallocbit`, `rotr` | `selerror`/`goerr` — a pure boundary-stub tail into the still-deferred `LBDOSENT.A86`'s own error-reporting table, nothing to port; `setdata`/`setdir`/`setdma` — pure boundary-stub setup, no observable effect, not even defined as functions |
| `src/loader/LBDOSF2.A86` | the whole file: `scndm`, `initialize`, `cpydirloc`, `compext`, `srch`/`srchn`/`srchnext`, `delet`/`delet0`, `getblock`, `copyfcb`/`copydir`/`seekcopy`, `rename`, `indicators`, `open`/`opencopy`, `mergezero`, `close`, `make`, `openreel`/`openmod`/`openr0`-`openr2`/`openerr` | — (every routine in this file is ported) |
| `src/loader/LBDOSF3.A86` | `sqread`/`diskread`, `slect`/`curselect`/`reselect`, `func13`/`func15`/`func20`/`func26`, `goback`/`retmon`, plus `parsave`/`parsave33`/`parunsave` (borrowed from the otherwise-deferred `LBDOSENT.A86`, needed by `func15`/`func20`/`goback`) | over half the real file is already dead, commented-out code from an earlier rewrite (its own old `rseek`/`rdread`/`rdwrite`/`cmperr`/`getfilesize` family) — never assembled, nothing to port; `func12`/`func16`-`func19`/`func21`-`func25`/`func27`-`func37`/`func40` are each just an `EQU` alias for the bare `return:` label, nothing to port; `func14` aliases `curselect`, `func38`/`func39` alias `funcret` (`src-c/loader/lbdosio.c`), both already ported under their own names; the rest of `LBDOSENT.A86` (its own `jmp cs:functab[bx]` dispatch + `iret`, and `func50`-`func57`) remains deferred |

Each is backed by a `tests/src-c/test_<file>.py` differential test — see
each one's own header comment for exactly what it covers.

**`cioattach`'s real "must actually wait" path** (`ca_sleep`, console
busy and not conditional) is translated faithfully in `console.c`,
gotos included, but not exercised by the test: like `QUE1.RTM`'s
`readq_entry`/`writeq_entry`, it retries after a boundary-stubbed
`mpmif(f_sleep)` call that never changes the condition being waited on,
so the real `jmps cioattach` loop this implements would spin forever
under the convention. `conassign_entry`'s own retry (`as_mok`'s mismatch
case) is different and *is* fully exercised: it always re-attempts the
boundary-stubbed `mpmif(f_findpdname)` call, which always "fails" under
the convention, so that retry provably terminates after one extra pass.

**Two more translation bugs caught by the differential tests themselves
while porting `CONSOLE.CIO`** (own bugs in this session's C port, not in
`/src` — fixed before being reported done, not worth a `docs/bugs.md`
entry): `mimic_entry` and `unmimic_entry` both leave `bx` holding `rlr`
on success rather than zeroing it — the real assembly just never bothers
to clear it there, unlike every other routine in the file. And
`mimic_entry`'s own `push dx ! ... ! pop dx` around its second
`getccbadr` call restores *both* halves of `dx`, undoing the `mov
dh,dl` that set up that call's input — so the value `mimic_entry`
actually stores into `c_msource` is the original source device number
(`dh`), not the mimic device number a naive read of the instruction
sequence suggests.

**Two genuine defects found while porting `QUE1.RTM`/`QUE2.RTM`**, both
reproduced (or deliberately *not* reproduced, where that's the only sane
option) rather than fixed, per this file's own rule that `/src-c` never
"fixes" what `/src` actually does — worth proper `docs/bugs.md` entries,
not logged there yet:

- `QUE2.RTM`'s `remqd` ends with `mov qul,ax` where `ax` still holds
  `qul`'s own *old* value from two instructions earlier — a no-op
  assignment. The freed QD gets linked *to* the old `qul` head, but `qul`
  itself is never updated to point *at* the freed QD, so it's never
  actually threaded onto the free list. Almost certainly should read
  `mov qul,si`. This one has well-defined (if wrong) behavior, so
  `que2.c`'s `remqd` reproduces it exactly, and
  `tests/src-c/test_que.py` differentially confirms the reference
  assembly and the port agree on the broken behavior.
- `QUE1.RTM`'s `deleteq_entry` has a real stack-imbalance bug on its
  `qd_noq` path (deleting a QD that isn't actually on `qlr`): that path
  jumps straight to `popf ! ret` without the `pop es`/`pop ax` every
  other exit path has, leaving one extra word on the stack. `popf` pops
  that leftover `es` into FLAGS instead of the real saved flags, and
  `ret` then pops the real saved-flags word as a return address — a
  jump to a garbage location. Unlike `remqd`'s finding, there's no
  well-defined behavior here to reproduce, so `que1.c`'s `deleteq_entry`
  implements the clearly-intended behavior for that path instead
  (returning `e_q_notactive` cleanly), and the test deliberately doesn't
  exercise that one path against the real assembly — see both files'
  own header comments.

**One more genuine defect found while porting `MEMORY.MEM`**, fixed in both
trees per `AGENTS.md`'s bidirectional-sync rule (not yet logged with its own
`docs/bugs.md` entry): `mfree_entry`'s own active-PD check (`mfree_nxtpd`)
walked the thread list with `mov si,p_thread[si]` reading through `p_link[si]`
instead -- mismatching the `(offset thrdrt)-p_thread` bias set up just above
it, the exact bias `malloc_entry`'s own identical check (a few lines earlier
in the same file) gets right. Fixed in `MEMORY.MEM` by using `p_thread[si]`
like `malloc_entry` does.

**Two more defects caught porting `MEMORY.MEM` to C, own bugs in this
session's translation (not in `/src`, fixed before being reported done)**:
`memory.c`'s `malloc_entry`/`mfree_entry` both reproduced the same
`(offset thrdrt)-p_thread` bias as a plain `SD_THRDRT` start value instead of
`SD_THRDRT - p_thread`, so their very first thread-list read landed two bytes
past `thrdrt` instead of on it. And `mfree_it` had its `cmp cx,0 ! jne mfree_r`
check inverted (ran the decrease-length/`mfree_off` work when the boundary-stub
`cx` was *non-zero* instead of zero) and hardcoded its returned `bx` to 0
instead of passing the input `bx` straight through unchanged, the way the real
routine's own push/pop dance leaves it. `tests/src-c/test_memory.py` is the
differential test; `tools/i86sim.py` also gained `PUSH r/m16` (group `FF /6`)
while building it.

**`src/kernel/CPMMEM.MEM`'s own `cpmfree_entry`** has the same
real-but-unexercisable shape as `QUE1.RTM`'s `cioattach` (see that file's
own writeup above): its `free_seg` path's `jmps cpmf_next` on failure
leaves `bx` pointing at the same root it already walked from, not
advanced to the segment just (unsuccessfully) freed — fine on real
hardware, where a genuine `f_memfree` success routes back to
`cpmf_root` for a fresh walk once the list has actually changed, but
under the boundary-stub convention (`free_memory` always "fails") this
spins forever the moment the list holds a segment that isn't itself a
Load segment. `cpmmem.c` translates this faithfully (`bx` is correctly
left untouched on that path) but `tests/src-c/test_cpmmem.py`
deliberately doesn't exercise it, same reasoning as `cioattach`. Also
worth noting (not a bug, just a real quirk carried through unchanged):
`try_seg`'s `and ms_flags[si],mf_load` clobbers every other flag bit in
the Memory Descriptor in place, not merely tests one — translated
faithfully in `cpmmem.c` too.

**`src/rsp/ECHO.A86`'s `main`/`loop` never return** (`loop:`'s very last
instruction is `jmps loop`) — same shape as the Tier-1 `jmp`-tail-call
boundary case, just looping back to itself instead of into another
module. `tests/src-c/test_echo.py` stops the simulator at that one `jmps
loop` instruction's own address (not its target) rather than needing a
bounded-iteration harness, and the reference run starts at `main` so the
one-time setup (create/open queue, set priority) executes too. Every real
syscall ECHO makes goes through its own `mpm:` trampoline
(`int mpmint ! ret`), which is a `call_stub_addrs` boundary stub ECHO
never checks the result of, so there was nothing new needed for `int` —
the whole trampoline is just skipped.

**Deferred, with real infrastructure gaps** (attempted, but turned out to
need machinery that doesn't exist yet rather than being a routine-by-
routine translation — revisit once that machinery exists):

- `src/kernel/CIOIF.CIO`, `src/kernel/RTMIF.RTM`, `src/kernel/SUPIF.SUP`,
  `src/kernel/MEMIF.MEM` (each one's `init`/`entry`/`mpmif`/`xiosif`) —
  every kernel module's own central dispatcher: module init via far calls,
  then an indirect call through that module's own function table (`call
  cs:functab[si]`), none of whose targets are ported yet, plus the same
  far-call/far-ret needs as `RPL.SUP` above. `SUPIF.SUP`'s `user_entry`
  additionally needs a real `IRET` and interrupt-entry convention this
  tree has nowhere else yet.
**Tier 3 additions, same two categories as above:**

- `src/kernel/BDOSIF.BDO` (`call cs:btab_addr[si]`) — its own central
  jump-table dispatch, the same deferred class as `MEMIF.MEM`/
  `CIOIF.CIO`/`RTMIF.RTM`/`SUPIF.SUP` (now `BDOSIF.BDO`'s turn, for
  BDOS). Not yet reconned for how much of it, if any, is portable
  around that gap (same as `ABORT.RTM` turned out to be, almost
  entirely).
- `src/kernel/FILE2.BDO`, `src/kernel/FILE4.BDO` — their own
  remaining routines all route through `srch`/`rddir`, transitively
  blocked by `FLUSH_FILE0`'s real segment-relative-addressing gap in
  `FILE3.BDO` — now resolved in principle (same `mem_*_seg` helpers as
  `ml.c`/`proc.c`/`mau.c`/`share.c`/`load.c`), but `FLUSH_FILE0`
  itself isn't ported yet, so neither file has anything new to port
  until that happens. (`DATA.BDO` is confirmed pure data, see its own
  status-table row above.) `src/kernel/CHARIO.CIO` (the large, deeply
  interdependent console-input state machine this bucket used to defer
  — `coninit`/`attach`/`conbrk`/`conin`/`chkctl`/`echoc`/`tabout`/...),
  `src/kernel/COMMAND.SUP` (its own `chain_entry`/`clicmd_entry`
  hit the same real segment-relative-addressing gap `PROC.RTM`/`ML.MEM`
  did, once actually read in depth), `src/kernel/FILE1.BDO` (the
  smallest of the four BDOS filesystem files, reconned and mostly
  ported), and `src/kernel/FILE3.BDO` (only the one cluster
  `FILE1.BDO`'s own `CHECK_FCB` needed, not the whole file -- see
  below) are since reconned — see
  their own status-table rows and the Tier 3
  rollout note below. The `FILE*.BDO`
  files total ~4900 lines of BDOS file-system internals and are
  Tier-3-plus-sized on their own.

Everything else in `src/kernel/`, `src/loader/`, `src/rsp/` is **not
started**.

### Scope notes

- `src/toolchain/` and `src/utils/` (PL/M source) are out of scope for now —
  PL/M is already a C-like high-level language and a different kind of
  problem (`tools/plmc.py` already compiles it).
- Only files that currently assemble cleanly (the `*_OK` lists in the
  `Makefile`) are candidates. `XIOS.A86` (bug #7) and the three broken RSPs
  (`CLOCK`/`TMP`/`SUBMIT`, bugs #1–#3, see `docs/bugs.md`) stay out until/
  unless those are fixed independently.
- The goal is a verified reference C port for readability/documentation
  purposes, not a replacement for `/src` in the actual boot image.

### Planned rollout order

Smallest real-logic files first, to keep proving the pipeline before
committing to the ~21k total lines across `src/kernel`+`src/loader`+
`src/rsp`:

- **Tier 1 (<100 lines): done**, except where noted above —
  `SYSENT.RTM`, `SYSFUNC.SUP`, `FLAG.RTM`, `IDLE.SUP` are ported and
  tested; `RPL.SUP`/`MEMIF.MEM`/`TICK.SUP` turned out to need
  infrastructure (far calls, a function-table dispatch stub, a
  bounded-loop test harness) that doesn't exist yet and are deferred
  with that reasoning recorded above, rather than forced through.
- **Tier 2 (100–250 lines): done**, except where noted above. `PARSE.SUP`,
  `ECHO.A86`, `QUE1.RTM` and `QUE2.RTM` are ported and tested (and along
  the way `tools/i86sim.py` gained REP string instructions, a
  generalized ALU group1, shift-by-CL, signed comparisons, and `CMP
  r/m,r` in both directions — all reusable for later files, not
  one-offs). `CIOIF.CIO`/`RTMIF.RTM`/`SUPIF.SUP` are module-dispatcher
  glue (same deferred class as Tier 1's `MEMIF.MEM`); `PROC.RTM`/`ML.MEM`
  need real segment-relative addressing (a new, genuine infrastructure
  gap — see above). Porting `QUE1.RTM`/`QUE2.RTM` also surfaced two real,
  previously-unknown defects in the 1981 source itself (`remqd`'s
  no-op free-list update, `deleteq_entry`'s stack-corrupting `qd_noq`
  path) — see above.
- **Checkpoint:** after Tiers 1–2, reassess pace/approach before
  committing to the larger modules. Of the 16 files on the original
  Tier-1/2 list, 8 ported cleanly (some only partially — see each row's
  "deferred" column above), 4 are module-dispatcher glue needing far
  calls plus a function-table dispatch stub (`CIOIF.CIO`/`RTMIF.RTM`/
  `SUPIF.SUP`/`MEMIF.MEM`), 2 need a genuinely new infrastructure
  category (real segment-relative addressing: `PROC.RTM`/`ML.MEM`), 1
  needs far calls specifically for its own cross-segment return
  (`RPL.SUP`), and 1 needs a bounded-iteration test harness
  (`TICK.SUP`). The module-dispatcher shape alone accounts for a quarter
  of everything looked at so far — worth deciding, before Tier 3,
  whether to build that machinery rather than deferring it indefinitely.
- **Tier 3 (250–450 lines, plus `UTIL.MEM`/`CONSOLE.CIO` pulled forward
  from the "not detailed yet" bucket below): partially done.**
  `CONSOLE.CIO` (all 15 routines) and `UTIL.MEM` (both routines) are
  ported and tested, catching two more opcode gaps (`JCXZ`, `ADD
  r/m16,r16`) and two real bugs in this session's own C translation
  (caught by the differential tests before being reported done, not
  `/src` bugs — see above). `MAU.MEM`/`SHARE.MEM` hit the same
  segment-addressing gap as `PROC.RTM`/`ML.MEM`, plus a second one
  (reverse-direction string ops). `DSPTCH.RTM`/`ABORT.RTM`/`BDOSIF.BDO`
  are three more module-dispatcher-shaped files (6 found total now,
  across every tier). `MEMORY.MEM` (its own `mov ds,u_wrkseg`/`mov
  ds,cs:sysdat` pattern, not the blocked `mov ds,m_start[bx]` one) is now
  ported and tested too, catching one real `/src` bug (fixed in both
  trees, see above) plus two more in this session's own translation
  (also see above, and `PUSH r/m16` added to `tools/i86sim.py`).
  `CPMMEM.MEM` (the CP/M-86 memory-call compatibility shims, 175 lines)
  is ported and tested too — no new blockers, but it does surface
  another `cioattach`-shaped unexercisable path (see above). Checked
  `LOAD.SUP` against the known blocker patterns before picking
  `CPMMEM.MEM` instead: it hits the real segment-relative-addressing gap
  (`mov es,ldtab_start[si]` then fixed offsets off it — same class as
  `PROC.RTM`/`ML.MEM`), so it's deferred alongside those, not a good
  next candidate after all. `CHARIO.CIO` (599 lines, the console-input
  state machine CONSOLE.CIO's own `getccbadr` was pulled out of
  earlier) is now ported and tested too, in its own dedicated pass —
  all 9 entry points plus 20 internal routines in `src-c/kernel/
  chario.c`, calling back into `src-c/kernel/console.c`'s
  `conattach_entry`/`condetach_entry`/`mimic_entry`/`unmimic_entry` as
  ordinary cross-file calls. Two more opcode gaps surfaced (`POP r/m16`,
  `NOT r/m`). The real `xiosif` trampoline (the XIOS character-I/O
  dispatcher `constf`/`coninf`/`conoutf`/`listf` all tail into) turned
  out to need a translation choice of its own, different from every
  `mpmif` boundary stub so far: `mpmif`'s own calls leave a
  function-code constant sitting in `cx` for the caller's own `cmp
  cx,0` to key off, but `xiosif`'s call sites never pre-load any such
  sentinel into the register (`bx`) their caller eventually reads back
  — under the no-op boundary-stub convention, `bx` genuinely just
  passes through unchanged from whatever it already was, a
  caller-dependent undefined quantity (confirmed the hard way: an
  early differential-test case assumed this was deterministic and
  failed non-reproducibly on a retry-loop's second pass through it,
  since the real register chain the reference assembly runs through
  there isn't something this port's own choice of fixed value can be
  expected to match). Resolved the same way `CPMMEM.MEM`'s
  `cpmfree_entry` resolved its own undefined `bx` (see above): fixed at
  a well-defined constant (0) rather than reproduced faithfully, with
  the now-unreachable branches that choice creates (`conbrk`'s "a new
  character arrived" path, `chkctl`'s `control_s` wait loop and its
  abort dialog's "echo the answer" block) translated faithfully but not
  exercised by the test, and any case that would depend on a *second*
  pass through that choice (not just the first, deterministic one)
  left out of the test for the same reason. `COMMAND.SUP` (647 lines,
  the CLI/CCP command processor) was read in full too: its real meat
  -- `chain_entry`/`clicmd_entry`, 450+ lines handling command-line
  parsing, RSP-queue dispatch, and loading/creating the new process --
  turned out to hit the real segment-relative-addressing gap in two
  separate places once actually read closely (not caught by the
  grep-level blocker check, since neither is a `mov ds/es,...[reg]`
  pattern): `cli_fload`'s own chain-mode UDA copy (`ES` dynamically set
  to the newly loaded process's own UDA segment via `cs:sysdat` +
  `inituda`'s offset, then fixed `UDA.FMT` offsets read/written
  relative to it) and its base-page command-tail setup (`ES` set to
  `cli_bpage`, the newly loaded program's own base-page segment, then
  fixed `BASEP.FMT` offsets off it) -- both the same class as
  `PROC.RTM`/`ML.MEM`. What's left once those are deferred is a
  cluster of small, genuinely self-contained pieces: `shifttail` and
  `fileopen` (real, if modest, logic) and `user_retf` (real logic too
  -- clearing `pf_keep`/`pf_tempkeep`/`pf_ctlc`/`pf_sys` on RSP exit --
  plus a boundary tail via a software interrupt, `int mpmint`, instead
  of `mpmif`'s usual near `CALL`; modeled as a `stop_addrs` entry at
  the `INT` instruction's own address, the same idea as a tail-`JMP`
  boundary, since `tools/i86sim.py` has no `INT` opcode support at all
  and nothing after that instruction matters anyway). All 3 are ported
  and tested in `src-c/kernel/command.c`. Investigating `user_retf`
  also cleared up a scare: its own `not (pf_keep+pf_tempkeep+pf_ctlc+
  pf_sys)` read at first like it might still have the missing-parens
  `not`-precedence bug `docs/bugs.md` #80 found and fixed in this exact
  routine, but the parens shown in the source today *are* that already-
  applied fix -- confirmed directly by assembling the expression with
  `tools/asm86` and reading back the resulting immediate (`0xFD7C`,
  the correct mask, not the pre-fix `0x027E`). No bug left to
  reproduce. `DATA.BDO` was confirmed pure data on an actual read (zero
  labels at all -- not even a first skim's guess, see the status
  table). `src/kernel/FILE1.BDO` (909 lines, the smallest of the four
  BDOS filesystem files) is read in full and mostly ported: 41 real
  routines across its own error handlers and FCB/disk-I/O arithmetic,
  with 4 (+ their own internal labels) deferred because they call into
  `FILE2.BDO`/`FILE3.BDO`/`FILE4.BDO`'s own real logic -- see its own
  status-table row. Needed its own combined wrapper (`FILE1-4.BDO` +
  `BDOSIF.BDO` + `DATA.BDO`, the same bundle `src/kernel/BDOS.A86`
  itself assembles, since `FILE1.BDO` isn't self-contained: `GOBACK`
  lives in `FILE4.BDO`, `staret`/`XCRLF`/`XPRINT` in `BDOSIF.BDO`, and
  every scratch field in `DATA.BDO`), which surfaced a genuinely new
  problem none of the smaller single-file wrappers ever hit: this
  combined module's own cseg (~0x2659 bytes) runs past where
  `DATA.BDO`'s dseg starts (`org 0C00h`) when both are laid into
  `tools/i86sim.py`'s one flat memory array at their own assembled
  addresses, silently corrupting whichever one gets copied in second --
  `tests/src-c/test_file1.py`'s own `run_reference` resolves this by
  layering dseg's real field data (`0xC00` on) over cseg rather than
  trying to relocate either segment (BDOSIF.BDO's own "cseg ! org 0"
  defeats pushing cseg's own base up front-of-file). Also caught two
  real bugs in this session's own translation before reporting done
  (not `/src` bugs): `selectdisk` had its boundary-stub success/failure
  branches backwards (`bx == 0` is the real routine's own *error*
  indication, not success); `CHKSUM_FCB`'s own C translation misread
  where `CMPEC0`'s own loop leaves `bx` afterward (landing on
  `INFO+12`, not back at `INFO`), moving the following `INC BX` to the
  wrong address. `seekdir` also turned out to have no `ret` of its own
  in the real asm at all -- it falls straight through into `seek`'s own
  body (its only real caller, the deferred `rddir0`, relies on exactly
  that), translated here as `seekdir` tail-calling `seek()` rather than
  being a separate sibling function. Picked up 15 more opcode gaps in
  `tools/i86sim.py` along the way (`XOR`/`AND`/`ADD`/`ADC` in both
  register-register directions, `XCHG`, `DIV`, `LOOP`/`LOOPE`/`LOOPNE`,
  `RCL`/`ROL`/`ROR` with a variable count, `NEG`) -- by far the most of
  any file ported so far, consistent with it being a much less-trodden
  corner of the opcode space than the console/loader code exercised
  until now. Picked a small, scoped follow-up next: `FILE3.BDO`'s own
  `PACK_SDCNT`/`COMPARE`/`SEARCH_OLIST`/`SEARCH_LLIST`/`SEARCHN_LIST`
  cluster (no blockers), chosen specifically because `FILE1.BDO`'s own
  deferred `CHECK_FCB` calls `PACK_SDCNT`/`SEARCH_OLIST` -- it also
  calls `CLOSE1` (`FILE2.BDO`, cascading into `CLOSE_FCB`/
  `GET_DIR_EXT`/`RESTORE_RC`/`srch`/`getdptra`, not scoped this pass),
  so `CHECK_FCB` stays deferred regardless, but the cluster itself is
  real, useful, self-contained logic ported on its own merit. Reused
  `FILE1.BDO`'s own combined wrapper verbatim (same bundle, same
  cseg/dseg layering fix) since `FILE3.BDO` isn't self-contained
  either. All 11 differential-test cases passed on the first real run
  -- no new opcode gaps, no new translation bugs -- a useful contrast
  with how much firefighting `FILE1.BDO` itself needed. `FLUSH_FILES`/
  `FLUSH_FILE0` (also in `FILE3.BDO`) turned out to hit the real
  segment-relative-addressing gap once actually read, so porting the
  rest of `FILE3.BDO` wouldn't have fully unblocked `checksum` either
  way. A good next candidate is `FILE2.BDO`/`FILE4.BDO` (the rest of
  `FILE3.BDO` too, if revisiting `CLOSE1`'s own chain becomes
  worthwhile), none of which got more than a grep-level look yet.
- **The loader modules: first pass.** `src/loader/LBDOSIO.A86` (the BDOS
  loader's byte-I/O module, 168 lines, under its own `src-c/loader/`
  tree with its own `defs.h` — a separate module/symbol space from
  `src-c/kernel/`) is ported and tested: no blockers, and its five
  external calls (`constf`/`coninf`/`conouf`/`listf`/`wbootf`) are
  ordinary near `CALL`s into the loader's own not-yet-ported BIOS jump
  table (`src/loader/LBDOSEQU.A86`'s `BIOS+3*n` scheme), a clean fit for
  the established mid-routine-call boundary-stub convention. Caught one
  more opcode gap (`OR r8,r/m8`, `0x0A`) in `tools/i86sim.py`. Checked
  its sibling **`LBDOSENT.A86`** (211 lines, the BDOS entry module) and
  ruled it out: it dispatches via `jmp cs:functab[bx]` (the same
  module-dispatcher-jump-table class as `DSPTCH.RTM`/`ABORT.RTM`/
  `BDOSIF.BDO`) *and* ends in a real `iret` — two deferred-class
  blockers at once, now deferred alongside those. The rest of the
  loader tree (`LBDOSEQU.A86`/`LBDOSDAT.A86`, pure equates/data;
  `LBDOSNEW.A86`, a 14-line stub; `LBDOSF1.A86`–`LBDOSF3.A86`, the BDOS
  loader's own file-system internals, 476–665 lines each; `LDMPM.A86`/
  `LDBIOS.A86`/`ROM.A86`, the other three real loader modules;
  `LDBIOS86Q.A86`/`ROM86Q.A86`, the new from-scratch QEMU replacements,
  lower priority since they're not part of the original 1981 source)
  still hasn't gotten more than this pass's own grep-level check.
- **`LBDOSF1.A86`: a near-full standalone twin of `FILE1.BDO`.** This
  loader module (used at boot time, before the real BDOS is running)
  turns out to be a near-full standalone copy of the kernel's own
  `FILE1.BDO` disk-I/O/FCB-arithmetic/checksum/rddir/getallocbit logic
  -- but unlike the real `FILE1.BDO`, its own checksum/rddir/wrdir
  chain is entirely self-contained in this one file, not split across
  `FILE2.BDO`/`FILE3.BDO`/`FILE4.BDO`'s own real logic, so none of that
  cross-module blocking applies here. Ported essentially the whole
  file (35 routines) in one pass. Its only external calls
  (`seldskf`/`homef`/`readf`/`writef`/`settrkf`/`sectran`/`setsecf`/
  `setdmbf`/`setdmf`) are the same BIOS jump-table stubs
  `LBDOSIO.A86`/`src-c/loader/lbdosio.c` already treat as boundary
  stubs. Used the same minimal-standalone-wrapper approach as
  `LBDOSIO.A86`'s own `lbdosio_wrapper.A86` (not the full combined
  `LDBDOS.A86` bundle, which pulls in `LBDOSENT.A86`'s own deferred
  `jmp cs:functab[bx]` dispatch + `iret`) -- `LBDOSDAT.A86` keeps all
  of this module's own scratch data in cseg, immediately following its
  own code in the real bundle (no separate dseg), so this wrapper
  instead declares its own scratch dseg block at a harness-chosen base
  address, same convention as `SD_RLR` et al.
  Found one real, interesting discovery: `seek`/`home` each have one
  real, unconditional side effect (writing the current track number,
  or 0, through the `curtrka` pointer) that's independent of their own
  boundary-stub BIOS calls (`settrkf`/`homef`) -- unlike `FILE1.BDO`'s
  own fully-collapsed `seek`, this one needed translating for real.
  `getallocbit`'s own comment claims it computes "CX shr 3" the same
  as the kernel's own version, but read literally the real instruction
  sequence (an elaborate manual byte-level rotate/mask dance avoiding
  a 16-bit `shr bx,cl`) computes something else entirely for inputs
  with a nonzero high byte -- confirmed empirically by directly
  driving the assembled reference with a probe script (same technique
  used for `file2.c`'s own `getblock` bit-mapping investigation) rather
  than trusting the algebra by hand; translated as a literal,
  register-level transliteration instead of reusing the kernel's own
  simplified formula. `setallocbit`'s own shift-count input (`CH` at
  its own entry, used by its shared `rotr` tail) similarly isn't
  `getallocbit`'s own returned shift count -- it's whatever the
  *caller* happened to leave in `DH` before calling `setallocbit`,
  confirmed empirically to actually change the result -- and since its
  only real caller (`LBDOSF2.A86`'s own `scndm`) is itself deferred,
  porting it meaningfully would need that caller ported first to make
  sense of what it's supposed to contain; deferred instead. Caught two
  real translation bugs in the first draft via the differential test,
  both from copying field names across rather than re-deriving them:
  `seekdir` read a memory field (`LD_BLKSHF`) where the real asm uses
  the constant `dskshf` (a different, similarly-named EQU), and
  `setcdr`'s own branch condition on `compcdr`'s result was inverted
  (compcdr's own result comes back via the carry flag, not a register,
  which also caught out this test's own first draft the same way --
  fixed by reading `tools/i86sim.py`'s own `sim.cf`/`sim.zf` directly
  instead of trying to read a flag-returning routine's result out of a
  register). No new opcode gaps this pass -- `tools/i86sim.py` already
  covered everything this file needed.
- **`FILE2.BDO`: the `getblock` allocation-bitmap search.** Picked
  `GET_NALBS` + `getblock` (and its internal `lefttst`/`righttst`/
  `rblok`/`rblok0` labels) out of `FILE2.BDO`'s 80+ labels: everything
  else in the file routes through `srch` → `rddir`, one of `FILE1.BDO`'s
  own deferred routines (itself blocked transitively by `FLUSH_FILE0`'s
  real segment-relative-addressing gap in `FILE3.BDO`), so this cluster
  was chosen for being both clean and a natural extension of
  `FILE1.BDO`'s own allocation-bitmap work — it calls the already-ported
  `getallocbit` directly, as an ordinary cross-file call (`file2.c`
  declares it with a matching `extern` prototype and is compiled
  together with `file1.c`, same convention as `que1.c`/`que2.c`).
  `GET_DIR_EXT`/`RESTORE_RC` are themselves clean FCB arithmetic, but
  their only real caller `CLOSE_FCB` also calls `srch`, so porting them
  alone wouldn't un-defer anything. Reused `FILE1.BDO`'s own combined
  wrapper verbatim, but hit a *different* variant of that bundle's own
  cseg/dseg collision than `FILE1.BDO`/`FILE3.BDO` did: there, every
  ported routine's own address happened to fall below `DATA.BDO`'s own
  `dseg` range (`0xC00`–`0x1000`), so layering dseg's real field data
  over cseg was safe. Here, `getblock`'s own code address (`0xe10`)
  falls *inside* that range, so the same blanket layering stomped
  `getblock`'s own code with zero-filled dseg bytes; fixed by dropping
  the dseg layering entirely in `test_file2.py` and force-copying only
  the two fields the test actually drives (`BD_MAXALL`, `BD_ALLOCA`)
  unconditionally instead. Picked up one more opcode gap (`RCR`, group2
  `/3`) in `tools/i86sim.py`, mirroring the existing `RCL` loop. Also
  caught a real translation bug in the first `getblock` draft: `cx`
  (the real asm's left search pointer) and `dx` (the right pointer) are
  two independent, persistent cursors — `righttst` only ever borrows
  `cx` as scratch to pass the right-pointer value into `getallocbit`
  (`mov cx,dx`), then restores `cx`'s own left-pointer value from the
  stack before falling back to `lefttst`. Collapsing both into one C
  variable (as first written) silently clobbered the left pointer on
  every `righttst` attempt; caught by strengthening a test case to
  actually force a second `lefttst` iteration (the first version of
  that case passed without exercising the bug, since its byte pattern
  didn't actually force the loop) and fixed by giving the port separate
  `left`/`right` variables, matching the real two-cursor shape. Own-port
  bug, not a real `/src` bug, so fixed in `file2.c` only and not logged
  in `docs/bugs.md`, per the usual convention.
- **`FILE4.BDO`: 8 clean BDOS entry points.** `FILE4.BDO` is the BDOS
  function-dispatch table (`func13`-`func52`+, 1190 lines) -- picked
  out `func24`/`func25`/`func29`/`func32`/`FUNC44`/`FUNC45`/`func51`/
  `func52` specifically because each is a small, self-contained MPM-
  branch entry (return login vector/selected disk/r-o bit vector, set
  user code, set multi-sector count, set BDOS error mode, set/get DMA
  base) with no calls into anything deferred -- the same shape as the
  already-ported `sysent.c`/`sysfunc.c` entries. Everything else in the
  file routes through `srch`/`rddir`, `CURSELECT`/`initialize`,
  `CHECK_FCB`, or `FLUSH_FILES`, all transitively blocked by the same
  `FLUSH_FILE0` gap; `GOBACK` is a 2-instruction tail jump into
  `BDOSIF.BDO`'s own module-dispatch epilogue, the same jump-table-
  dispatcher class already deferred elsewhere. Reused the combined
  BDOS-bundle wrapper again (`file4_wrapper.A86`, identical to
  `file2_wrapper.A86`'s own), but hit a new variant of this bundle's
  own segment-collision problem: `rlr`/`dlog`/`rodsk`/`returnseg` are
  real `DATA.BDO`/`SYSDAT.FMT` fields with safe real addresses (same
  as every other `BD_*` constant), but `u_dma_ofst`/`u_dma_seg`/
  `u_error_mode`/`u_mult_cnt`/`u_retseg` are real `UDA.FMT` fields --
  `UDA.FMT` declares its own `eseg`/`org 0`, a genuinely separate
  hardware segment, so its own real addresses in this bundle (2, 4,
  0x10, 0x11, 0x30) land inside cseg's own early code once collapsed
  into the flat memory model, the same class of gap as `SYSDAT.FMT`'s
  own `rlr=0x68`. Fixed the same way `rlr` already was for `file1.c`:
  the port itself reads/writes new harness-chosen scratch constants
  (`UDA_DMA_OFST`/`UDA_DMA_SEG`/`UDA_ERROR_MODE`/`UDA_MULT_CNT`, plus
  the already-existing `SD_RLR`/`UDA_RETSEG`), while
  `test_file4.py`'s own reference run remaps each one to its real
  bundle address. All 11 differential-test cases passed on the first
  real run -- no new opcode gaps, no new translation bugs.
- **`LBDOSF2.A86`: the whole file, plus un-deferring `setallocbit`/
  `rotr`.** This loader module turned out to be entirely self-
  contained and portable now that `LBDOSF1.A86`'s own `rddir`/
  `checksum`/`setenddir`/`endofdir`/`getdptra` are ported -- it's the
  exact same search/open/close/make/rename/delete logic the kernel's
  own `FILE2.BDO` couldn't port (its own `srch` calls a `rddir` blocked
  by `FLUSH_FILE0`), un-blocked here because this module's `rddir` has
  no such cross-module split. Ported the entire file in one pass (every
  routine, no deferrals) -- the biggest single pass of this project so
  far.

  Before starting, resolved last pass's own deferred `setallocbit`/
  `rotr` (`LBDOSF1.A86`) for real, since their only real caller
  (`scndm`) lives in this file. Tracing `rotr`'s own shift-count
  register (`CH`) through the real asm's push/pop chains (confirmed
  empirically with a throwaway probe script, same technique as
  `file2.c`'s own `getblock` bit-mapping investigation) showed it's a
  genuine, faithfully-reproducible quirk: `rotr` always reads whatever
  `CH` happens to be at its *own* call site, not a value either
  `getallocbit` or `rotr` itself computes. For `setallocbit` (called
  from `scndm`), that traces back to `scndm`'s own entry-time `CH`,
  preserved unchanged through a push/pop round-trip that never touches
  the `CX` pair `scndm` uses for its own per-entry disk-map scanning --
  confirmed by direct empirical testing that `scndm`'s own *exit* `CH`
  always equals its *entry* `CH` too (an earlier, wrong first guess
  assumed `scndm` mutated it in between, briefly over-complicating
  `initialize`'s/`delet0`'s own translation with needless value-
  threading before the empirical check corrected it). For `getblock`'s
  own unrelated call to `rotr`, `CH` instead comes deterministically
  from `getallocbit`'s own internal final `CX` (exposed via a new
  `cx_out` parameter) -- computed locally, no threading needed. Along
  the way, caught a real latent bug in the first `rotr` draft: its
  shift amount wasn't reduced mod 8 before use, undefined behavior in
  C for the real, reachable `ch >= 8` values this file's own callers
  produce (the 8086's own `ror`/`rol` with a count register isn't
  masked the way 286+ is, but is periodic every 8 steps for a byte
  operand, so reducing mod 8 first is exact and avoids the UB) --
  found by reasoning about the C translation directly, not by a test
  failure (the UB happened to produce the right answer on this
  platform/compiler regardless, which the existing passing tests
  couldn't have caught).

  Two more real translation bugs caught by the differential tests
  while porting this file's own ~30 routines: `copydir` has no `RET`
  of its own in the real asm -- it falls straight through into
  `seekcopy` (`seekdir`+`wrdir`), the same "shared tail" shape as
  `LBDOSF1.A86`'s own `seekdir`/`seek` -- so every real caller
  (`copyfcb`, `rename`, `indicators`) gets a seek-and-write each time,
  not just the byte copy a first reading suggested; and the single-
  byte branch of `close`'s own disk-map merge needed its exact
  "re-read `[bx]` after a possible update, not the stale pre-update
  value" sequencing restored after an initial over-simplification
  dropped it. Reused `LBDOSF1.A86`'s own minimal-wrapper convention,
  extended to also include `LBDOSIO.A86` (for `staret`/`setlret1`,
  cross-called for real, same convention as `que1.c`'s own cross-file
  calls into `que2.c`) -- no new cseg/dseg collision this time, since
  this wrapper's own new scratch fields land well clear of both the
  other two modules' own. All 20 differential-test cases pass; no new
  opcode gaps.
- **`LBDOSF3.A86`: finishing the loader's own BDOS filesystem trio.**
  Smaller than `LBDOSF1.A86`/`LBDOSF2.A86` -- over half the real file
  is already dead, commented-out code from an earlier rewrite, never
  assembled. The live part (sequential disk read, disk (re)selection,
  the dispatcher epilogue) is fully portable now that `LBDOSF1.A86`'s/
  `LBDOSF2.A86`'s own routines are ported, plus `parsave`/`parsave33`/
  `parunsave`, borrowed from the otherwise-still-deferred `LBDOSENT.A86`
  (its own `jmp cs:functab[bx]` dispatch + `iret` stays deferred, but
  these 3 small helpers are independently portable -- the real `mov
  ds,parametersegment` is moot under this port's flat memory model,
  same reasoning as `LBDOSF1.A86`'s own `dirtouser`).

  Hit a real, instructive mistake while porting `slect`: its own `CALL
  selerror` branches on `selectdisk`'s success/failure, which boils
  down to a *flag* (`ZF`, via `JNZ`), not a register byte like the
  `diocomp`-style cases elsewhere. Mechanically replaying
  `selectdisk`'s own established "bx fixed at 0" convention (chosen
  back when `file1.c` first ported it, for a caller that never looked
  at its own `ZF`) leaves `ZF=1` here (from `selectdisk`'s own internal
  `cmp bx,0`), which would make `slect` *always* fall through to the
  still-deferred `selerror` -- the opposite of this port's first,
  wrong assumption that the success branch would be taken. Resolved by
  recognizing that "what flags an undefined-artifact convention leaves
  behind for a *different* caller to observe" is itself exactly as
  undefined as the original artifact was for its first caller, so this
  port picks `ZF=0` instead, consistent with every other undefined-
  artifact choice elsewhere in this project: the value that keeps the
  routine's own real job reachable, not a mechanical replay of a
  choice made for an unrelated caller. Caught before committing to a
  wrong translation, by checking the differential test's own result
  rather than trusting the first draft's own reasoning.

  Also hit a test-harness-only wrinkle (not a C-port bug): `seek`'s
  own real asm ends in `jmp setsecf`, a tail JMP rather than a near
  CALL, so `tools/i86sim.py`'s own `call_stub_addrs` can't intercept
  it; in this combined bundle's own byte layout, the address it
  coincidentally overlaps doesn't happen to fall through to a clean
  `RET` (unlike in smaller wrappers where this was genuinely harmless,
  per `LBDOSF1.A86`'s own wrapper comment). Worked around in
  `test_lbdosf3.py` itself (not `tools/i86sim.py`, to keep the fix
  scoped) by treating a JMP landing on any BIOS stub address the same
  way `call_stub_addrs` treats a CALL to one: pop the return address
  still on the stack from whoever really called `seek`, and resume
  there. All 16 differential-test cases pass; no new opcode gaps.
- **`src/rsp/CLOCK.A86`: the MP/M-86 clock process.** A clean BCD
  time-of-day increment loop (seconds->minutes->hours->days), same
  "whole RSP, one pass through its own never-returning main loop"
  shape as `ECHO.A86`'s own port. Its own dispatcher calls go through
  a `CALL mpm` trampoline (`mpm: int mpmint ! ret`), so -- despite
  first picking this file specifically because it looked like it
  needed real `INT`-opcode support in `tools/i86sim.py` -- it turned
  out not to: `call_stub_addrs` already intercepts the `CALL mpm`
  itself, before the `INT` instruction inside `mpm`'s own body is ever
  reached, exactly like every other dispatcher-trampoline call in this
  project (`ECHO.A86`'s own `mpm:`, `QUE1.RTM`/`QUE2.RTM`'s `mpmif`).
  Added `INT`-stub support (a new `int_stub_nums` constructor
  parameter, treated the same "instant no-op" way `call_stub_addrs`
  treats a CALL) to `tools/i86sim.py` anyway before discovering this --
  purely additive, can't break anything already passing, and kept in
  place for a future file that might call `INT` directly without a
  trampoline.

  Found a real, already-documented but previously unfixed bug:
  `docs/bugs.md` #1 records that all 4 of `CLOCK.A86`'s own loop-back
  jumps reference `clock_loop` (underscore), but the real label is
  `clockloop` (no underscore) -- the file could not assemble at all.
  `docs/bugs.md` also recorded a deliberate policy that the 3 RSP
  files (`CLOCK.A86`/`TMP.A86`/`SUBMIT.A86`) are left unpatched, unlike
  every other bug in that doc -- since porting this file needs to
  assemble the real thing as a differential-test reference, asked the
  user how to proceed rather than silently overriding that policy;
  they confirmed fixing it in `/src` like every other bug (same
  evidence standard: an exact squashed-name match, no other candidate
  label in the file), so `docs/bugs.md` #1 is now marked FIXED,
  `TMP.A86`/`SUBMIT.A86` remain untouched. Also added `CLC`/`DAA` to
  `tools/i86sim.py` (needed for the real BCD increment sequence --
  `DAA` without carried-in `AF` tracking, since nothing here needs
  it: see that opcode's own comment for why that's still exact for
  this file's own use).

  `BX`'s own real meaning (`DS:BX` -> the TOD struct in SYSDAT, via
  `mov ds,sysdat ! mov bx,tod_offset`) is moot under the flat memory
  model, same DS/ES-switch reasoning used throughout this project --
  the test starts directly at `clockloop` with `bx` preset to a new
  fixed scratch address (`SD_CLOCK_TOD_BASE`) instead. Hit one real
  test-harness mistake along the way: `tools/i86sim.py`'s own
  `stop_addrs` is checked *before* executing anything, so starting the
  reference run at the same address used to stop it (both are
  `clockloop`, since every branch loops back there) halted instantly
  with nothing executed -- fixed by forcing one real step before
  calling `run()`. All 5 differential-test cases pass.
- **`src/rsp/TMP.A86`: the MP/M-86 shell ("Terminal Message
  Processor").** Same "whole RSP, one pass through its own
  never-returning main loop" shape as `ECHO.A86`/`CLOCK.A86`, but its
  own dispatcher calls reach `mpm:` through a tail-JMP chain via one of
  its own small per-function trampolines (e.g. `prchar: mov cl,
  mpm_conout ! jmp mpm1` / `mpm1: jmp mpm`), not a direct `CALL mpm` at
  every site like those two files -- confirmed by diffing against
  `ECHO.A86`'s own call sites (`call mpm` everywhere). `call_stub_addrs`
  only intercepts a near CALL landing exactly on a listed address,
  which a JMP chain never does, so this file is the first one that
  actually needs the `int_stub_nums` support added (but not needed) for
  `CLOCK.A86`: stubbing `INT mpmint` itself (number 224) resumes at
  `mpm`'s own `ret`, which unwinds correctly back to the real CALL site
  no matter how many tail JMPs preceded it.

  Found `docs/bugs.md` #2 already fixed, by an earlier commit in this
  tree, before that doc caught up to say so (`git diff` against HEAD
  showed no changes to `src/rsp/TMP.A86`; `git log -p` found the fix
  already landed in commit `7e1cad8`) -- updated `docs/bugs.md` to
  mark it FIXED rather than asking the same policy question again,
  since no actual `/src` edit was needed this time.

  The boundary-stub convention (a no-op `call mpm` leaves every
  register exactly as it was right before the call) has two real
  consequences here, both confirmed against the file's own byte
  content:
  - `getuser`/`getdisk`/`getlist`'s own results are undefined (nothing
    pre-loads a sentinel for them, same "caller-dependent undefined
    quantity" class as `CONSOLE.CIO`'s own `xiosif` writeup above) but
    unobservable, since they only feed `prnum`/`prchar` print calls,
    themselves no-op boundary stubs with nothing to model.
  - `clicall`'s own first `call parsefilename` deterministically
    returns `cx = mpm_parse` (152), pre-loaded right before the tail
    JMP into `mpm` -- so `jcxz goodparse` is never taken (152 != 0),
    and `clierror`'s own real error-code chain never matches 152
    either (every real code is < 40), always falling through to the
    catch-all branch. This makes `goodparse` and everything downstream
    (the built-in `USER`/`PRINTER` command checks, `not_builtin`'s own
    real CLI dispatch, and every `clierror` branch but the catch-all)
    provably unreachable, not merely untested -- same class of
    boundary-stub consequence as `console.c`'s own un-exercised
    `ca_sleep` retry and `cpmmem.c`'s own `cpmfree_entry` `free_seg`
    path, but stronger (those loop forever under the convention; this
    one is never entered even once), so this port doesn't translate
    that dead code at all.

  `TMP.A86`'s own real DSEG offsets (e.g. `clicb_cmd` at `0x1e0`) land
  inside its own 0x423-byte cseg once collapsed into the flat memory
  model -- unlike `ECHO.A86`/`CLOCK.A86`, where this happened not to
  collide by luck. `tmp.c` uses new harness-chosen scratch addresses
  (`SD_TMP_CMDSENT`/`SD_TMP_READ_BLEN`/`SD_TMP_CLICB_CMD`/
  `SD_TMP_SAVEBUF` in `defs.h`) for its own 4 touched fields, same
  convention as `SD_RLR` et al. The *reference* run needs a different
  fix for the same collision, since it has to execute the real,
  unmodified machine code at its real, colliding addresses: `cseg` is
  loaded at a shifted base (`CSEG_BASE = 0x1000`, clear of every real
  DSEG address this routine touches) instead of address 0, which is
  safe because every control-flow instruction this file's own
  reachable path uses is relative (`jmp`/`call`/`je`/...), so shifting
  the whole block by a constant preserves every internal branch; the
  only same-cseg *absolute* address references this path bakes in are
  `offset prompt`/`offset catcherr`-style string addresses fed to
  `printstring`/`print_ds_string`, both no-op boundary stubs never
  dereferenced here either way. `savebuf` is pre-filled with a
  byte-ramp sentinel pattern on both sides before each case, so a case
  that never reaches `clicall` -- and so never touches `savebuf` on
  either side -- compares equal instead of comparing a cleared scratch
  region against whatever unrelated cseg bytes happen to sit at
  `savebuf`'s real, colliding address in the reference run. All 9
  differential-test cases pass.
- **`src/kernel/TICK.SUP`: the MP/M tick-flag RSP.** Walks the Delay
  List (`dlr`), decrementing the head entry's `p_wait` and moving it
  (plus any further head entries that also expire on the exact same
  tick -- a cascading `enddelay:` loop, confirmed working via a
  3-entry test case) onto the Ready List (`drl`) once it hits 0;
  stops the hardware clock (`notick:`) once the list empties out.
  Needed a new wrapper (`tests/src-c/fixtures/tick_wrapper.A86`), since
  this file has no DSEG/EQUs of its own (every field it touches —
  `rlr`/`drl`/`dlr`/`u_wrkseg`, `p_wait`/`p_link`/`p_stat`, `flag_tick`/
  `f_flagwait`, `io_stopclk`/`xiosif` — is declared by other files in
  the real build). Added a new `p_wait` offset constant to `defs.h`
  (not previously needed by any other port).

  Ported as two functions, one per real label, rather than one merged
  loop body like `CLOCK.A86`/`ECHO.A86`: `notick:` is reached only by
  falling straight through from `tick:`'s own preceding instructions,
  never by a loop-back jump, so there's no entry/exit ambiguity in
  splitting them this way -- unlike those two files' own single
  re-entrant loop head, `TICK.SUP` genuinely has two (`notick`/`tick`),
  and treating both as `stop_addrs` for a single pass starting at
  `notick` would have falsely halted the test right at the natural
  fallthrough into `tick`, before any of `tick`'s own real work ran.
  `tick_once` starts directly at `tick` instead (skipping that
  fallthrough question entirely) and stops at either `tick` (another
  tick needed) or `notick` (list now empty) -- same
  stop_addrs-equals-entry-address fix as `test_clock.py`'s own
  `clockloop` (force one real step before calling `run()`).

  `notick:`'s own `mov es,p_uda[bx] ! mov u_wrkseg,ds` (saving this
  process's own data segment into its UDA, moot addressing via `rlr`)
  collapses to a direct write of the caller-supplied `ds` parameter to
  the fixed `UDA_WRKSEG` scratch address, same pattern as `sysent.c`'s
  own `pd_entry`. `notick_once`'s own `call xiosif` (stop the hardware
  clock) and `tick_once`'s own `int mpmint` (wait for the tick flag)
  are both boundary stubs, no-ops. All 11 differential-test cases
  pass; no new opcode gaps (`cli`/`pushf`/`popf` were already
  supported).
- **`src/kernel/FILE3.BDO`, 2nd pass: `DELETE_ITEM`/`CREATE_ITEM`/
  `SET_OLIST_ITEM`.** An earlier pass's own header comment claimed
  `TST_OLIST`/`CHK_OLIST`/`PR_TERM` were also self-contained -- on
  closer reading they aren't (see this file's own status-table row
  above for exactly why), so this pass only covers the 3 that
  genuinely are: `delete_item`/`create_item` are plain olist/llist
  free-list linked-list operations; `set_olist_item` fills in an
  olist element's attrs/drive byte and copies the current packed-dcnt/
  pdaddr (via `file1.c`'s own `move`, now non-`static` for this --
  an ordinary cross-file call, same convention as `file2.c`'s own call
  into `file1.c`'s `getallocbit`).

  `delete_item`/`create_item` touch `free_root` -- not a FILE1-4.BDO
  scratch field like `BD_CURDSK` et al, but a real `SYSDAT.FMT` field
  (confirmed via `--symbols`: resolves to `0x52`, nowhere near the
  `BD_*` block's own `0xC00+` addresses) -- MP/M-86's BDOS module has
  no private DSEG of its own at all; every one of its scratch fields
  really is SYSDAT-resident on real hardware. `0x52` is safely below
  every address this combined module's test entry points
  (`delete_item`/`create_item`/`set_olist_item`) ever execute forward
  from, so it's not a new "collides with cseg" case needing a
  scratch-address substitution the way `TMP.A86`'s own fields needed
  -- `defs.h`'s new `BD_FREE_ROOT` uses this real, verified address
  directly.

  Found a real bug in `tests/src-c/test_file3.py`'s own established
  `run_reference` helper (not a `/src` or `/src-c` bug): its "overlay
  only *nonzero* `mem` bytes onto `image`" shortcut (needed because
  `dseg`'s own restore only covers addresses `>= 0xC00`) silently
  leaves a stale raw cseg code byte in place at any lower address
  whenever the test's own intended value happens to have a zero byte
  there -- `BD_CURDSK` already had an explicit override line
  working around exactly this for itself, but the pattern wasn't
  generalized. `BD_FREE_ROOT` (`0x52`, below the `0xC00` boundary)
  hit it for real the first time anything needed a *word* there: a
  `delete_item` case whose expected free-list link value happened to
  have a zero low byte came back from the reference run with that
  byte unexpectedly still holding raw code. Fixed the same way as the
  existing `BD_CURDSK` line: an explicit `image[BD_FREE_ROOT:+2] =
  mem[BD_FREE_ROOT:+2]` override after the overlay loop. Also hit a
  real translation mistake while writing the test itself (not a
  `/src-c` bug, caught before being reported done): `I86Sim`'s own
  `regs` dict only tracks full 16-bit registers (`ax`/`bx`/.../`di`,
  confirmed via `tools/i86sim.py`'s own `_R16`/`_R8_PARENT` tables) --
  passing `{'al': 0x80}` silently does nothing (it's stored as a
  dead `'al'` key never read back), so `set_olist_item`'s own `al`
  input has to be set via `'ax'` instead. All 17 differential-test
  cases pass (6 new, 11 pre-existing); no new opcode gaps.
- **`src/rsp/SUBMIT.A86`: the MP/M-86 batch-command processor.** A
  small, scoped first pass (same precedent as `file2.c`/`file3.c`'s
  own scoped passes), not a whole-RSP "one pass through the loop" port
  like `ECHO.A86`/`CLOCK.A86`/`TMP.A86`: this file's real logic isn't
  actually one unified loop-body call graph the way theirs is --
  `deblank`/`fill_argtable` (collapse whitespace in the command tail,
  then split it into `argtable`-indexed arguments) run once at
  startup, separate from `topofcmdloop`'s own per-line work, and
  `expand_command`'s own `$0`-`$9` argument substitution and
  `readbuffer`/`readchar`'s own SUB-file-line-reading are each
  self-contained units with no boundary-stub-dependent branching --
  unlike `topofcmdloop`'s own control-C check/prompt/early-exit/CLI-
  dispatch, left for a later pass.

  Needed all 3 of `docs/bugs.md` #3's bugs fixed in `/src` first, just
  to assemble at all -- same RSP-unpatched-policy question already
  asked and answered for `CLOCK.A86`/`TMP.A86`'s own bugs, this time
  for all 3 at once (`not_builtin`/`notbuiltin`, `sopen_err`/
  `sopenerr`, and a 3rd one `docs/bugs.md` had flagged as possibly
  needing a different fix shape than the other two -- on inspection
  it didn't: `con_write`, the obvious candidate, turned out to already
  be a real, correctly-used label at several other call sites in this
  same file, not an "unfinished/different pattern" the doc's own
  earlier, more cautious wording had left open).

  Confirmed `notdrive`'s own `call parsefilename` (this file's own
  copy of `TMP.A86`'s routine, same shape) hits the exact same
  provably-unreachable wall: `cx` deterministically comes back `=
  mpm_parse` (152, pre-loaded right before the tail `jmp mpm`), never
  satisfying `jcxz goodparse` and never matching any of `clierror`'s
  real error codes (all < 40) either -- so `goodparse` and everything
  downstream (built-in `USER`/`PRINTER`/`$INCLUDE` handling,
  `notbuiltin`'s own real CLI dispatch) are provably unreachable, same
  class as `TMP.A86`'s own `goodparse` -- confirming the rest of
  `topofcmdloop` isn't a quick win even for a later pass, beyond its
  own early-exit checks.

  `copyit`'s own real control-flow trick (`donecopy` pops its own
  return address off the stack and discards it -- "return is garbage"
  -- to jump straight to `echocommand` regardless of which of
  `copyit`'s own 2 call sites reached it) doesn't need replicating in
  C at all: every caller just checks `copyit`'s return value and stops
  immediately, same net effect without the stack trick.
  `fill_argtable`'s own `mov [bx+di],si` is a 16-bit store into a
  byte-declared array, but functions as a byte array in practice (`si`
  never exceeds 255; `di` only advances by 1 per entry, so each
  write's own high byte is always clobbered by the next entry's low
  byte) -- confirmed against the real, later `mov bl,argtable[bx]`
  1-byte read. This file's own real DSEG offsets collide with its own
  0x665-byte cseg exactly like `TMP.A86`'s did, same `SD_SUBMIT_*`
  scratch-address and `CSEG_BASE`-shifted-reference-run treatment.

  Caught one real test-harness mistake while writing
  `tests/src-c/test_submit.py` (not a `/src-c` bug): entering the
  reference sim directly at the real `nextchar:` label skips the few
  real instructions right before it (`mov si,offset cmd ! mov
  bp,offset clicmd ! xor di,di`) that set up its own working
  registers -- fixed by passing them as the sim's own initial register
  state instead. Also found one input this file's own `fill_argtable`
  genuinely can't handle (not a `/src` bug, just outside its real
  precondition): a truly empty command tail leaves `deblankbuf` as
  just `" \0"`, and `skiparg`'s own forward scan for a terminating
  space then runs off the end of the buffer looking for one that
  doesn't exist -- never exercised on real hardware since SUBMIT's own
  argument 0 is always the `.SUB` filename itself, so a real invocation
  never has a genuinely empty tail; not tested, same reasoning as
  `console.c`'s own un-exercised `ca_sleep` retry. All 16
  differential-test cases pass; no new opcode gaps.
- **`src/rsp/SUBMIT.A86`, 2nd pass: `topofcmdloop_once`.** Completes
  this file's own main loop to the same level `TMP.A86`'s own
  `nextcommand_once` reached: the control-C check (clear
  `pf_childabort`, copy the SUB file's own name into `subfilename` for
  the abort prompt, `charin`'s own answer), the prompt (`defdisk`/
  `defuser`/`promptutens`/`promptuones`/`promptdisk` -- real DSEG
  writes here, unlike `TMP.A86`'s own analogous code which only ever
  fed a print call with nothing to store), `expand_command` (already
  ported), and the null/`;`/disk-shortcut early exits. Confirmed
  `notdrive`'s own `call parsefilename` hits the exact same
  provably-unreachable wall `TMP.A86`'s `goodparse` did (same
  `cx = mpm_parse` mechanism), so `topofcmdloop_once` returns
  `SUBMIT_SUBMITEXIT` there, same as the real asm's own `jmp
  submitexit`.

  Caught a real translation gap while writing the differential test
  (not a `/src-c` bug, fixed before being reported done): assumed
  nothing downstream of `parsefilename`'s own deterministic failure
  had any further memory effect, by analogy with `TMP.A86`'s
  `not_builtin` -- true there (its own bad-parse tail only touches a
  separate `savebuf` copy), but not here: `notdrive`'s own bad-parse
  tail does `mov cl,126 ! mov di,offset clicmd ! mov al,' ' ! repne
  scasb ! mov byte ptr[di],'$'`, scanning `clicmd` *itself* for its
  first space and overwriting it with `$` -- a real, observable
  mutation of the just-built command buffer this port needs to
  replicate. Caught by the differential test showing a reference
  `clicmd` of `"DIR $:"` where the port (silently dropping that write)
  still had `"DIR A:"`.

  `topofcmdloop_once`'s own `charin_al`/`defdisk_al`/`defuser_al`
  parameters are exposed as explicit inputs (same "undefined artifact"
  treatment as `TMP.A86`'s own `getuser`/`getdisk`, except here they
  also get *stored*, not just printed) -- but the differential test
  itself had to respect a real mechanical constraint while doing so:
  under the no-op boundary-stub convention, a `call charin`/`call mpm`
  with no sentinel pre-loaded leaves `AL` exactly as it already was,
  so none of `charin`'s, `mpm_getdefdisk`'s, or `mpm_usercode`'s own
  "results" can be independently injected in the *reference* run (only
  the port's own parameters are independent) -- they're all
  mechanically forced to equal whatever `AL` already held, carried
  through from one no-op call to the next since nothing in this file's
  own reachable path resets it in between (except the control-C path's
  own `and al,5fh`, whose *result* is what persists afterward, not the
  raw value). All 23 differential-test cases pass (7 new, 16
  pre-existing); no new opcode gaps.
- **`tools/i86sim.py`: real (opt-in) segment-relative addressing.**
  Added a `segmented: bool = False` constructor parameter. Off (the
  default), behavior is byte-for-byte identical to before -- segment
  override prefixes are still parsed (for correct instruction length)
  but still addressing no-ops, exactly as every existing test already
  relies on (confirmed: all ~340 pre-existing cases re-ran clean after
  this change). On, every effective address becomes `(segment_value *
  16 + offset) & 0xFFFF` -- the usual 8086 rule (`SS` for a BP-based
  EA, `DS` otherwise, both overridable by a prefix; `ES` for a string
  op's own `DI`, never overridable, same as real hardware) -- still
  wrapped into the same flat 64KB array rather than a real 20-bit/1MB
  address space, so a test using it has to pick segment values that
  don't collide once wrapped (same spirit as `CSEG_BASE`-shifting a
  reference run elsewhere in this project). Kept opt-in specifically
  because `echo.c`'s own test explicitly documents relying on a
  segment override being a no-op; turning this on unconditionally
  risked silently changing what that (or some other undocumented)
  existing test actually exercises.

  This was scoped, not just implemented blind: a deeper read of
  `PROC.RTM` (one of the two files motivating this) found it needs
  more than segmentation alone -- `proc_creat` nests 3 different
  segments in one routine and loops to drain a whole chain of pending
  process creations, and `getpdadr` does real cross-segment
  *arithmetic*, not just a switch-and-read -- so it's left for its own
  dedicated pass (see its own status-table row). `ML.MEM` turned out
  to be the clean fit, ported in full below.
- **`src/kernel/ML.MEM`: the whole file.** `mlalloc_entry` scores every
  possible contiguous Memory-List window against an MPB (reached
  through `u_wrkseg`'s own dynamically-supplied segment -- the new
  `mem_r16_seg`/`mem_w16_seg`/`mem_r8_seg`/`mem_w8_seg` helpers in
  `defs.h` do the same `seg*16+offset` arithmetic explicitly on the
  port's own side), keeping the lowest-scoring (best) one and stopping
  early on a perfect (0) score; `mla_makemau` turns the winning window
  into one MAU (either the single element itself, or -- via `getmd`,
  already ported in `util_mem.c` -- a fresh "partition list" MD
  spanning several merged elements); `mla_initmau` lays out a brand
  new MAU's own SAT by writing through *its* own dynamically-supplied
  segment (`m_start[bx]`); `mlfree_entry`/`ml_insert` reverse the
  process, keeping the Memory List sorted by `m_start`. Needed a new
  wrapper (`tests/src-c/fixtures/ml_wrapper.A86`) since this file has
  no DSEG/EQUs of its own; `satlen`/`sat_start`/`sat_length`/
  `sat_nall` (normally `MAU.MEM`'s own equates, assembled ahead of
  `ML.MEM` in the real build) are redeclared directly in the wrapper
  rather than pulling in the whole of `MAU.MEM`, which would need its
  own unrelated external dependencies this port doesn't.

  Caught two real translation bugs while writing the differential test
  (not `/src` bugs, fixed before being reported done): `mla_value`'s
  own `cmp cx,dx ! jbe mlav_ab ! xchg dx,cx` swaps `dx`/`cx` only on
  the *not-taken* branch of `jbe` (i.e. when `cx > dx`) -- the first
  draft had the C `if` condition backwards (swapping on `cx <= dx`
  instead). That alone happened to not matter for a scenario with
  only one viable candidate (the wrong intermediate score still ended
  up "the best of one"), but surfaced immediately on a single-element,
  exact-fit case that depends on the score coming out to exactly 0.
  Chasing it down further also caught a second, related bug: the
  following `(dx - cx) >> 4` was computed in *signed* `int` arithmetic
  (since both operands promote to `int` and can subtract negative)
  before truncating back to `uint16_t`, instead of truncating to
  `uint16_t` *before* the shift the way the real `shr`'s own unsigned
  16-bit semantics require -- harmless once the first bug was fixed
  (a correct swap guarantees `dx >= cx` going into the subtraction),
  but fixed explicitly rather than left relying on that invariant by
  coincidence. All 5 differential-test cases pass.
- **`src/kernel/PROC.RTM`: the whole file.** `freepd` unlinks a PD from
  the Process Thread Root list (the same bias-and-walk trick as
  `src-c/kernel/memory.c`'s own `mall_try_mau`) and, if it came from the
  PD table (`pf_table`), returns it to the PD-Unused-List (`pul`, a new
  `SD_PUL` scratch constant). `getpdadr` is `que2.c`'s own `getqdaddr`
  sibling (same `cs:sysdat`/`endseg` bounds check, same "grab a free
  table entry otherwise" fallback) -- unlike that port, its fallback
  copy genuinely needs a real dynamically-supplied source segment
  (`mem_r8_seg(mem, wrkseg, ...)`), since this is the routine
  docs/src-c.md had flagged as the real gap, not a moot DS-switch.
  `proc_creat` writes through three different segments in one call: the
  newly created child's own UDA (computed fresh each call, the same
  gap `ml.c`'s own `mla_initmau` exists for -- new `u_*` UDA.FMT offset
  constants in `defs.h`, used with the existing `mem_*_seg` helpers),
  that child's own user-stack segment (for its IRET-frame setup), and
  the real, absolute interrupt-vector-table segment 0. It also drains
  a whole chain of pending creations (`jmp proc_creat`'s own tail loop)
  and calls the still-unported `mpmif(f_share)` in a loop with no
  observable effect (same convention as `que2.c`'s own `rlsmx`).

  Needed two pieces of new test infrastructure, not just
  `segmented=True`: `tests/src-c/fixtures/proc_wrapper.A86` redeclares
  UDA.FMT as a real `eseg`/`org 0` block (an `EQU` doesn't assemble --
  `cmp ds:EQUconst,dx` mis-encodes as an unsupported mem,mem combo,
  confirmed directly), and the reference run loads `cseg` at a
  CSEG_BASE shift (same technique as `tests/src-c/test_tmp.py`), since
  `proc_creat` is the first routine to actually read the real, absolute
  IVT at segment 0 as data -- every earlier file only ever touched
  scratch/SYSDAT-relative fields that could be placed anywhere. Shifting
  cseg also means `cs:sysdat`'s own segmented-mode address needs a real
  `cs` register value (`CSEG_BASE >> 4`), and any absolute `call_stub_addrs`
  target (`mpmif`) needs the same CSEG_BASE offset added, since the
  assembler resolves that displacement assuming cseg loads at 0.

  Caught one real misunderstanding while writing the differential test
  (not a `/src` bug): the IRET-frame setup's own bare `u_sp`/`u_initcs`
  references are *not* DS-relative despite DS having just been switched
  to the user's stack segment on the line above -- UDA.FMT's own `eseg`
  declaration makes every bare (non-`ds:`-prefixed) reference to one of
  its fields implicitly ES-relative, confirmed directly against the
  assembler's own encoding (an implicit ES: prefix byte on both). Every
  *other* UDA field access in this routine uses an explicit `ds:`
  override for exactly this reason; these two don't, and a first
  reading of the real asm missed why that mattered. All 6
  differential-test cases pass (including a 2-long creation chain and
  the no-op `mpmif(f_share)` loop).
- **`src/kernel/MAU.MEM`: the whole file.** `maualloc_entry`/
  `maufree_entry` (+ internal `mau_split`/`mau_insert`/`mau_release`/
  `mau_collapse`) all operate on a MAU's own suballocation table (SAT),
  reached through `m_start[bx]`'s own dynamically-supplied segment --
  the same real gap `ml.c`/`proc.c` exist for, resolved the same way
  (the existing `mem_*_seg` helpers and `sat_*`/`satlen` constants).
  The real asm's own bare `.0` (e.g. `cmp dl,.0`) is this assembler's
  "a leading dot forces a memory operand" convention applied to a
  literal 0 -- the byte at offset 0 of the ambient DS (the MAU's own
  segment), i.e. SAT0's own `ents` field; ported as `mem_r8_seg(mem,
  seg, 0)` everywhere it appears.

  **Found and fixed 2 real, 40-year-old `/src` bugs** (both present
  identically in `orig/mpm862sr/02/MAU.MEM` and `orig/lastdri/19/
  MAU.MEM` -- see docs/bugs.md #102 for the full writeup): at the exact
  same call site (`maualloc_entry`'s exact-absolute-split path),
  `mov bx,cx` clobbered `BX` right before `call mau_split` (`mau_split`
  needs `BX` to stay the target SAT entry's own address), and the call
  was also missing the `push si ! ... ! pop si` protection its sibling
  call site (`maua_splitit`) already has, letting `mau_insert`'s own
  free use of `SI` as scratch corrupt this routine's own MPB offset.
  Confirmed both by direct comparison against the file's own other,
  correct call sites to the same routine. `src-c/kernel/mau.c`'s own
  port was never affected by either (no shared "SI register" to
  clobber in a from-scratch C translation) -- it implements the
  corrected behavior directly.

  **Also found and fixed 2 real `tools/i86sim.py` bugs**, both latent
  since `segmented=True` was added for `ml.c` and both only now
  exposed because this is the first file to execute the relevant
  instruction with a real, nonzero segment register: `LEA` returned
  the ModRM decode's already-segment-adjusted address instead of the
  raw effective offset LEA is actually defined to compute (fixed via
  a new `self._last_ea_offset`, the pre-segmentation value, which LEA
  now reads instead); and `STD`/`CLD` (direction flag) weren't
  implemented at all, needed by `mau_insert`'s own real backward
  `REP MOVSB` (every earlier file only ever copied forward) -- fixed
  by tracking `self.df` and negating the string-op step when set.

  All 8 differential-test cases pass, including a direct regression
  case for the `/src` bug pair above.
- **`src/kernel/SHARE.MEM`: the whole file.** `share_entry` locates
  the owner PD's own `p_mem` entry at the requested start paragraph,
  builds a matching new entry for the requestor (via `getmd`), bumps
  the underlying MAU's own SAT share count (the same real segment gap
  as `ml.c`/`mau.c`, resolved the same way), and threads the new entry
  onto the requestor's own `p_mem` list next to any existing entry
  that already shares the same MAU.

  One asymmetry ported faithfully, not fixed: the real asm's own
  freshly-`getmd`'d MD leaks on the "owner has no MS at this start"
  failure path (`se_err`, no `call freemd`), but *is* freed on the
  "MAU's own SAT has no matching entry" path (`se_err1`). Unlike
  docs/bugs.md #102's own two findings, there's no parallel call site
  in this file to compare against to confirm this is a genuine bug
  rather than an intentional asymmetry (e.g. the first case may be
  considered "can't happen" under normal operation, the second a real
  runtime possibility worth guarding), so left as-is.

  All 5 differential-test cases pass.
- **`src/kernel/LOAD.SUP`: a named tail portion of `load:`, as
  `load_finish`.** Scoped deliberately, not a full port: `load:`'s own
  front half (`get_ch_info`'s own `diskread` calls to read the `.CMD`
  header, then `lt_more` onward spreading real `mpmif(f_malloc)`
  allocation results across `ldtab`) is fundamentally different from
  every file ported so far -- its branches depend on *real* disk-read/
  allocation results, not decorative ones a boundary-stub no-op can
  stand in for, and disk I/O (`f_freadseq`, via `load_group`/
  `diskread`) isn't ported anywhere in this tree yet. `load_finish`
  covers the real asm's own `lb_more` through `load_exit`: finding the
  Base Page's own group (Data, or Code for an 8080-model program),
  building it from every populated `ldtab` entry, and (when a PD was
  given) initializing that PD's own UDA/LDSTK from it -- all pure data
  manipulation over an already-populated `ldtab`, no `mpmif` of its
  own. Takes `ldtab`/`load_pd`/`load_nldtabents`/`load_fcb` as given
  input (fixed scratch addresses, same convention as every other
  SYSDAT-resident table this project ports), the same way other tests
  here pre-build PD/MD/SAT structures by hand rather than deriving
  them from a still-unported pipeline.

  Hits the same real segment-relative-addressing gap as `ml.c`/
  `proc.c`/`mau.c`/`share.c` three times over: the chosen group's own
  Base Page segment, the new process's own UDA segment, and that
  process's own user-stack segment are all dynamically computed.
  `ls_sp`/`ls_offset`/`ls_flags`/`ls_roffset`/`ls_rcseg` (LSTK.FMT)
  form the *other* half of a two-file relationship with `proc.c`'s own
  `proc_creat`: this routine sets `u_stack_sp = offset(ls_sp)` and
  fills in everything but `ls_cseg`; `proc_creat`'s own later, more
  generic IRET-frame step (which knows nothing LOAD.SUP-specific)
  reads that same `u_stack_sp` value back and fills in `ls_cseg`/
  `ls_flags` a second time -- confirmed by direct arithmetic (`0x56+2
  = 0x58`, `0x56+4 = 0x5A`, and the `0x0200` flags literal matches
  exactly), cross-confirming both ports.

  Needed two `tools/i86sim.py` opcodes that had never come up before:
  `SUB r/m8,r8` (`0x28`) and `SUB r/m16,r16` (`0x29`) -- every earlier
  file's own `SUB` always used a register destination (`0x2A`/`0x2B`,
  already implemented); `sub p_uda[bx],ax` here is the first to
  subtract *into* memory.

  Also caught a real test-authoring mistake while writing the
  differential test (not a `/src` or `/src-c` bug): a first pass fed
  the test a Data-group `ldtab` entry with no accompanying Code-group
  entry, expecting `bpg_cseg` to just be test-injected -- but
  `lbb_mre`'s own scan is what populates every group's own Base Page
  slot (`bpg_cseg` from the Code-group entry, `bpg_dseg` from the
  Data-group one, same base page either way), so a realistic `.CMD`
  always needs both. All 6 differential-test cases pass.
- **Re-examined `src/kernel/COMMAND.SUP` for the segment-addressing
  gap now that it's resolved (ml.c/proc.c/mau.c/share.c/load.c) --
  still correctly deferred, no new port.** Its own two gap spots read
  differently once actually reasoned through in full: the chain-mode
  UDA swap (`cli_ld1`'s own `cmp cli_chain,true` block) stashes the
  calling process's entire current UDA into the dormant real INIT
  process's own template UDA area as temporary scratch while chaining
  -- a real mechanism, but intricate enough (and chain-mode-only, so
  rarely exercised) that isolating it as its own confidently-tested
  unit wasn't a good value trade against the rest of this session's
  list. The base-page command-tail setup is entangled with `pfn`'s
  (`f_parsefilename`) own boundary-stub-dependent branching (`cmp
  bx,0ffffh`), the same "undefined branch, fix at a well-defined
  constant" situation `CONSOLE.CIO`'s own `conbrk` already needed.
  `shifttail`/`fileopen`/`user_retf` remain the whole of what's
  ported here.
- **`src/kernel/ABORT.RTM`: the whole file.** `sysreset_entry`/
  `terminate_entry` and `abort_spec_entry` (+ its own dozen internal
  helpers: list-walking, the `abort_tab` status dispatch, flag-table
  removal) are all ported. `rlsmx` (`que2.c`) and `findpdname_entry`
  (`findpd.c`) are called as ordinary internal calls, not boundary
  stubs. `jmp dsptch`/`jmp pdisp` are boundary tail-jumps into the
  still-unported dispatcher, same convention as every other already-
  ported caller of either (e.g. `SYSENT.RTM`'s `wakeup_entry`) -- the
  state set immediately beforehand is the whole observable result,
  confirmed concretely: `terminate_entry`'s own final `CX` is left
  holding `f_qread`'s own function-code value (`0x89`/137), not reset
  to 0, since nothing after the boundary-stub `mpmif` calls touches
  it before the real `jmp dsptch`.

  `call cs:abort_tab[di]` -- a real indirect near CALL through a jump
  table, the first in this tree -- needed two new `tools/i86sim.py`
  opcodes, `CALL r/m16` and `JMP r/m16` (`0xFF /2`, `/4`); every
  earlier file's own CALL/JMP targets were fixed `rel16` displacements.
  Also needed `TEST r/m8,r8`/`TEST r/m16,r16` (`0x84`/`0x85`,
  `abtdly`'s own `test si,si`) -- every earlier `TEST` used the
  immediate form (`F6`/`F7 /0`).

  `abort_spec_entry`'s own fake-return-address setup (writing
  `terminate`'s own address onto the aborted process's own stack, so
  switching back into it later lands directly there) takes
  `terminate`'s real assembled offset as an explicit parameter, same
  convention as `sysdat`/`user_retf_offset` elsewhere. All 8
  differential-test cases pass.
- **`src/kernel/FILE3.BDO`: a 2nd follow-up pass, un-blocking
  `FLUSH_FILES`/`FLUSH_FILE0` (+ new `TST_TBL_LMT`/`REMOVE_LOCKS`/
  `INC_PDCNT`).** The real segment-relative-addressing gap an earlier
  pass flagged this pair for (`FLUSH_FILE0`'s own `mov es,p_uda[bx]`)
  is resolved the same way as `ml.c`/`proc.c`/`mau.c`/`share.c`/
  `load.c` (`mem_r8_seg`/`mem_w8_seg` against the target PD's own UDA
  segment, `u_pd_cnt`'s real UDA.FMT offset already in defs.h from
  `proc.c`). `INC_PDCNT` is `CHK_OLIST`'s own internal label
  (`CHK_OLIST` itself stays deferred), ported standalone since
  `FLUSH_FILE0` shares it. Two new `BD_*` scratch constants
  (`BD_PDADDR`, `BD_FLUSHED`), confirmed via `--symbols` against the
  combined bundle same as every other one. All 6 new differential-
  test cases pass (12 pre-existing, unchanged).
- **`src/kernel/DSPTCH.RTM`: the whole file, as a reading/debugging
  aid for the live scheduler-hang investigation (#83-#98), not a
  kernel-build component** — see `src-c/kernel/dsptch.c`'s own header
  comment for the full rationale and for the three "no C equivalent"
  gaps (a real interrupt return, and a real context switch's register/
  stack save-and-resume, both modeled as explicit output parameters
  instead of attempted fidelity) plus one deliberate code-shape
  divergence in `term_act` (walks `p_mem`'s own `ms_link` chain
  directly rather than relying on the real, unported `mpmif(f_memfree)`
  boundary stub's own effect, which the boundary-stub convention would
  otherwise turn into a real infinite loop).

  Needed two more `tools/i86sim.py` fixes, both found via the new
  test's own differential failures and written up in full as #103:
  `PUSH`/`POP` ignoring `SS` entirely under `segmented=True` (latent
  since no routine before this one ever moved `SS` away from its
  default of 0 — `switch:`'s own `mov ss,u_ss ! mov sp,u_sp` is the
  first real stack switch in this tree), and near indirect `CALL
  r/m16`/`JMP r/m16` (added in #102 for `ABORT.RTM`) not adding the
  current `CS` base to the loaded target offset (latent since
  `ABORT.RTM`'s own test never needed a `CSEG_BASE` shift; `dsptch:`'s
  own `jmp cs:dsp_table[bx]`, under `test_dsptch.py`'s own shifted
  reference run, is the first to combine both). All 13 differential-
  test cases pass.
- **`src/kernel/RPL.SUP`: the whole file (`rpl_entry`).** Previously
  deferred as needing real far CALL/JMP/RET support and a
  controllable `mpmif` queue-read stub (neither of which the plain
  boundary-stub convention covers), on the assumption the routine's
  own interesting behavior was the success path. Revisited: `mpmif`'s
  own no-op-return convention leaves `cx` holding the just-loaded
  function code (`f_qopen`, MODFUNC.DEF's `(user*0100h)+135`, never
  0) going into the routine's very next `cmp cx,0 ! jne rpl_err`, so
  under the *existing* convention — no new machinery needed at all —
  the routine deterministically takes the error path on every call,
  same mechanical reason every other `mpmif`-function-code-gated
  routine in this tree already is. The real success path (the QPB
  build and the `retf` transfer into a queue-delivered RSP entry
  point — which, like `dsptch.c`'s own documented gaps, has no C
  equivalent at all) is real, well-shaped code that's simply never
  reached, same class as `CONSOLE.CIO`'s/`CPMMEM.MEM`'s own
  unreachable branches — translated faithfully in `src-c/kernel/
  rpl.c`'s own header comment, not in code. `tests/src-c/test_rpl.py`
  confirms the one deterministic outcome (`bx=1`, `cx=e_no_queue`)
  across two different CPBs. No new `tools/i86sim.py` infrastructure
  needed.
- **`src/kernel/QUE1.RTM`: the remaining 6 routines (`readq_entry`,
  `creadq_entry`, `readq`, `writeq_entry`, `cwriteq_entry`,
  `writeq`), completing the file.** Previously deferred wholesale as
  "retries in a loop after a boundary-stubbed `mpmif(f_sleep)` call."
  Revisited: that's only true of one specific combination —
  unconditional (`ax==0`) *and* the queue already empty/full — which
  does spin forever under the no-op-return convention and is
  translated faithfully (a real `for(;;)`) but never exercised, same
  class as this same file's own `cioattach` / `CPMMEM.MEM`'s
  `cpmfree_entry`. Every other combination — conditional-and-empty/
  full (an immediate, deterministic error return, no retry at all)
  and any call where a message/buffer slot is already available — is
  fully portable with nothing new needed, and is exercised by 6 new
  differential-test cases (18 pre-existing ones unchanged). `dx`
  (QPB address) is treated as already flat, the same simplification
  `openq_entry`/`deleteq_entry`/`queverify` already use in this file.
  Two new `defs.h` constants needed (`e_q_empty`, `e_q_full`,
  ERR.DEF), plus `ps_swap`/`ps_dq`/`ps_nq` (PD.DEF, `ps_swap` was
  already implicitly used elsewhere as a raw `3`, now named).
- **`src/kernel/SYSENT.RTM`: the remaining routine, `creat_proc_entry`,
  completing the file.** Previously deferred as "nothing to port
  without `proc_creat` itself"; now that `proc_creat` (`proc.c`) and
  `pdisp` (`dsptch.c`) both exist, unlike its 7 siblings in this same
  file (each still just a boundary-stub tail-jump, ported before
  either existed), this one goes all the way: a real internal call
  into both, returning `pdisp`'s own `switch_result`. Needed
  `proc_creat`'s own signature extended with an optional `uint16_t
  *dx_out` (the real asm's own final `DX` at its exit, needed to feed
  `pdisp` correctly -- `DX` is proc_creat's own loop variable, reused
  as internal scratch throughout each pass, only matching the next-
  pending-creation value right at each exit; purely additive, every
  existing caller updated to pass `NULL`). Surfaced a real, slightly
  surprising quirk along the way: `call proc_creat ! jmp pdisp` has no
  save/restore around the call, so `ax`/`bx`/`cx`/`si`/`di` are NOT
  preserved across it either -- `proc_creat` clobbers all five as
  its own scratch, same as `dx` -- so whatever `dsptch`'s own
  register-save step saves for this process is really just
  `proc_creat`'s own leftover state, observable only if the same
  process is re-dispatched into right away. The new differential-
  test case (`tests/src-c/test_sysent.py`, via its own new combined
  wrapper bundling the real `PROC.RTM`/`DSPTCH.RTM`) exercises exactly
  that (the creating process outranks its own new child and
  dispatches straight back into itself) and deliberately only checks
  `bp` of the resulting `switch_result` (the one register `proc_creat`
  never touches) -- same "real but not meaningfully specified
  register" class as `chario.c`'s own `xiosif`-left `bx`.
- **`src/kernel/SYSFUNC.SUP`: the remaining 4 entries (`sysdat_entry`,
  `tod_entry`, `serial_entry`, `callbios_entry`), completing the
  file.** `tod`/`serial` turned out to be real, fully portable data,
  not actually missing a "home": `tod` is a genuine `SYSDAT.FMT`
  field (`tod`/`tod_day`/`tod_hr`/`tod_min`/`tod_sec`, given its own
  harness-chosen `SD_TOD` scratch address and `todlen`=5, same
  convention as every other such field); `serial` is `RTMIF.RTM`'s
  own fixed, literal `db '654321'` cseg constant, inlined directly
  (not a runtime-chosen message, so `chario.c`'s own `write_cstr`
  helper wasn't needed). `sysdat_entry` is trivial, the same
  `u_retseg=ds` shape as `SYSENT.RTM`'s own `pd_entry`.
  `callbios_entry`'s own two tail-jumps (`mpmif(f_terminate)` for
  BOOT/WBOOT, `xiosif` otherwise) are ordinary boundary stubs, same
  convention as `SYSENT.RTM`'s own `poll_entry`/`delay_entry`/etc. --
  this port's job for those is the register state right up to the
  jump. One real surprise caught by the differential test itself:
  `cl`'s own "stays whatever it already was" branches (`al` in
  `{4..7}`) looked at first like a caller-register input, but the
  real asm's very first instructions load `cx` from the input block's
  own 2nd field (`mov cx,1[si]`) and those branches simply never
  overwrite it again -- not a caller input at all, see
  `src-c/kernel/sysfunc.c`'s own header comment.
- **`src/kernel/LOAD.SUP`'s own real disk-I/O blocker: resolved, with
  `diskread`/`load_group` now ported.** Unlike the deterministic-
  collapse/real-internal-call fixes applied to other files' own
  deferred routines this session, this one needed genuinely new
  infrastructure, since the whole point of these two routines
  *is* a real disk read: `tools/i86sim.py` gained `call_hooks`, a
  generalization of `call_stub_addrs` that runs a Python callback
  (with full read/write access to `sim.mem`/`sim.regs`) before
  treating the call as the usual instant no-op return. `diskread`'s
  own real `mpmif(f_freadseq)` (plus the `f_setdmab`/`f_setdma` calls
  that set up its target first) is modeled, on both the C port's
  side (`load.c`'s own new `freadseq`) and the reference-assembly
  side (`test_load.py`'s own new `call_hooks` entry, implementing the
  *identical* algorithm against `sim.mem` directly), as a
  deliberately simplified single-extent "virtual disk"
  (`SD_LOAD_DISK`/`SD_LOAD_DISK_RECS`, defs.h) indexed directly by the
  FCB's own `fcb_nxtrec` field -- no real disk-map/extent-switch
  logic, since every file this project's own tests load is small
  enough to stay within one extent (same "deliberate, documented
  simplification" spirit as `dsptch.c`'s own `term_act` divergence).
  Since both sides share the exact same simplified primitive, the
  differential test is actually exercising the real, interesting
  logic around it: `diskread`'s own ctlc-abort/hard-read-error
  handling, and `load_group`'s own buffer-reuse arithmetic (the
  "already partly in `load_dma`" transfer path, the direct-to-
  destination path for a full record, and the shared-buffer path for
  a trailing partial one). 12 new differential-test cases (6
  pre-existing `load_finish` ones unchanged). `load:`'s own remaining
  front half (CMD-header parsing, memory allocation/spreading) is a
  separate, still-open blocker -- see the status table.
- **`src/kernel/MEMORY.MEM`'s own `malloc_entry`: upgraded `mall_try_mau`/
  `mall_ml` from boundary stubs to real internal calls** into `mau.c`'s
  `maualloc_entry`/`ml.c`'s `mlalloc_entry` -- both already fully
  ported, so (same technique as `sysent.c`'s own `creat_proc_entry`,
  `que1.c`'s own `readq`/`writeq`) there was no reason left to stub
  them out. This is what directly unblocks `LOAD.SUP`'s own
  `mpmif(f_malloc)` dependency in principle (see that file's own row).
  `tests/src-c/fixtures/memory_mem_wrapper.A86`'s own `mpmif` is now a
  small real dispatcher (not a flat `equ` stub): it tail-jumps into
  the real `MAU.MEM`/`ML.MEM` (now bundled into the same wrapper) for
  exactly the two function codes that matter, `ret`-ing immediately
  for everything else, same as before. One existing test case's own
  name ("boundary stub always fails") was stale -- renamed, since it
  now passes because `mlalloc_entry` genuinely fails on an empty
  Memory Free List, not because of a stub; everything else was
  already set up to hit a real, matching failure the same way, so no
  other test needed changes. `mfree_entry`'s own `mpmif(f_maufree)` is
  NOT upgraded this pass -- its real "free an empty MAU back to the
  Memory Free List" path tail-jumps out of two call frames at once
  (`mfree_it` *and* `mfree_entry`, via `mpmif(f_mlfree)`), which needs
  more careful translation than a drop-in call; see `memory.c`'s own
  header comment.
