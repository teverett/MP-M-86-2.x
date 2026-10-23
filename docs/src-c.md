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
| `src/kernel/SYSFUNC.SUP` | `not_implemented`, `bad_entry`, `version_entry`, `mpmversion_entry` | `callbios_entry`, `sysdat_entry`, `tod_entry`, `serial_entry` — call into unported `mpmif`/`xiosif` or copy from module-local data this port has no home for yet |
| `src/kernel/SYSENT.RTM` | `poll_entry`, `delay_entry`, `dispatch_entry`, `set_prior_entry`, `pd_entry`, `sleep_entry`, `wakeup_entry` | `creat_proc_entry` — one line (`call proc_creat ! jmp pdisp`), nothing to port without `proc_creat` itself |
| `src/kernel/FLAG.RTM` | `flag_set_entry`, `flag_wait_entry` (+ internal `flag_valid`/`flag_exit`/`flag_bexit`) | — |
| `src/kernel/PARSE.SUP` | `parse_entry` (+ internal `setfld`/`delim`/`deblank`/`fcbinit`) | — |
| `src/rsp/ECHO.A86` | `main`/`loop` (one full pass — see below) | — |
| `src/rsp/CLOCK.A86` | `clockloop` (one full pass — see below) | — (`clock`'s own one-time setup, `mov ds,sysdat`/`mov bx,tod_offset`, is moot under the flat memory model, same as the DS/ES-switch convention elsewhere — the test starts directly at `clockloop` with `bx` preset instead) |
| `src/rsp/TMP.A86` | `nextcommand_once` (one full pass — see below) | `tmp:`'s own one-time setup — moot, every operation in it is a boundary-stub call or a register load feeding one; `goodparse` and everything downstream of it (`try_builtin`/`notuser`/`not_builtin`'s own real CLI dispatch) — provably unreachable under the boundary-stub convention, see below |
| `src/rsp/SUBMIT.A86` | `deblank`+`fill_argtable` (unlabeled code after `submit:`'s own `call openfile`), `expand_command`, `readbuffer`, `readchar`, `topofcmdloop_once` (one full pass — see below) | `submit:`'s own one-time version-check/priority/FCB-open preamble — no observable effect under the boundary-stub convention beyond what `deblank`/`fill_argtable` already cover; `clicall`'s own real CLI dispatch — provably unreachable past `notdrive`'s own `call parsefilename`, same wall `TMP.A86`'s `goodparse` hit, see below |
| `src/kernel/TICK.SUP` | `notick_once`, `tick_once` | — (both of this file's real labels are ported) |
| `src/kernel/QUE1.RTM` | `makeq_entry`, `openq_entry`, `deleteq_entry`, `queverify` | `readq_entry`/`creadq_entry`/`readq`, `writeq_entry`/`cwriteq_entry`/`writeq` — retry in a loop after a boundary-stubbed `mpmif(f_sleep)` call, see below |
| `src/kernel/QUE2.RTM` | `rlsmx`, `getqdaddr`, `remqd`, `qspace`, `qrelease` | — (all 5 of this file's routines are ported) |
| `src/kernel/CONSOLE.CIO` | all 15 routines (`lstattach_entry`, `clstattch_entry`, `cconattch_entry`, `conattach_entry`, `cioattach`, `lstdetach_entry`, `condetach_entry`, `ciodetach`, `conassign_entry`, `getdefcon_entry`, `getdeflst_entry`, `setdefcon_entry`, `setdeflst_entry`, `mimic_entry`, `unmimic_entry`) + `CHARIO.CIO`'s `getccbadr` (a small dependency, pulled in on its own) | — |
| `src/kernel/CHARIO.CIO` | all 9 entry points (`conin_entry`, `conout_entry`, `rconin_entry`, `rconout_entry`, `listout_entry`, `constat_entry`, `dirio_entry`, `conwrite_entry`, `conprint_entry`, `conread_entry`) + 20 internal routines (`rawinit`, `coninit`, `getconnum`, `attach`, `echoc`, `conbrk`, `chkctl`, `control_p`, `prcsmsg`, `prname`, `conout`, `compout`, `ctlout`, `tabout`, `pspace`, `pbacksp`, `pcr`, `crlf`, `crlfp`, `conin`) | `stdout` — its own `conoutf`/`listf` calls are both no-ops under the boundary-stub convention regardless of which branch is taken, so it collapses to nothing observable; inlined as a comment in `conout`'s own mimic-forwarding branch instead of a separate function, see that file's own header comment |
| `src/kernel/COMMAND.SUP` | `shifttail`, `fileopen`, `user_retf` | `chain_entry`/`clicmd_entry` (the file's own two real entry points) and everything only they call (`cli_cli`, `cli_copy`, `cli_parse`, `cli_fload`'s own chain-mode UDA copy, etc.) — real segment-relative addressing, same class as `PROC.RTM`/`ML.MEM`; `crlf`/`prchar`/`prtime`/`prnum`/`prdisk`/`prcusp`/`prcsm`/`prtyp`/`prnam`/`prn1`/`cprnt`/`pruser`/`prfilnam`/`mxwrite`/`mxread`/`mxw1`/`jmpm`/`pfn`/`conasn`/`fileclose` — each collapses entirely to register setup before a boundary-stub tail call with nothing downstream ever reading the result: nothing to port, same reasoning as `freeall_entry` (`src-c/kernel/cpmmem.c`) |
| `src/kernel/FILE1.BDO` | `pererror`/`roderror`/`roferror`/`selerror` (+ internal `goerr`), `move`, `selectdisk`, `home`, `PASS_ARECORD`, `rdbuff`, `wrbuff`/`wrbufflg` (+ internal `diocomp`), `seekdir`/`seek`, `dmposition`, `GET_DMA`, `getdm`, `bdos_index`, `alloct`, `atran`, `GET_ATTS`, `GET_S1`, `GET_RRA`, `GET_RCNTA`, `getexta`, `gtfcba`, `getfcb`, `setfcb`, `cmpecs`/`CMPEC0`, `CHKSUM_FCB`, `SET_CHKSUM_FCB`, `RESET_CHECKSUM_FCB`, `CHEK_FCB`, `setcdisk`, `nowrite`, `setro`, `ckrodir`/`ckrofile`/`RO_TEST`, `checkwrite`, `getdptra`, `getmodnum`, `clrmodnum`, `CLR_EXT`, `setfwf`, `compcdr`, `setcdr`, `rddirbuf`, `setdata`/`setdir`/`setdma`, `dirtouser`, `MAKE_FCB_INV`, `CHK_INV_FCB`, `TST_INV_FCB`, `endofdir`, `setenddir`, `getallocbit`, `setallocbit` | `CHECK_FCB` (+ `CHEK_FCB`'s own MPM branch calling it) — its own `PACK_SDCNT`/`SEARCH_OLIST` dependency is since ported (`src-c/kernel/file3.c`), but it also calls `CLOSE1` (`FILE2.BDO`), which cascades into `CLOSE_FCB`/`GET_DIR_EXT`/`RESTORE_RC`/`srch`/`getdptra` -- not scoped yet, so still deferred; `checksum`/`newchecksum`, `wrdir`, `rddir`/`rddir0` — each calls into `FILE2.BDO`/`FILE3.BDO`'s own real, not-yet-ported logic (`FLUSH_FILE0`/`FLUSH_FILES`, themselves blocked by a real segment-relative-addressing gap, see `FILE3.BDO`'s own status-table row); `subdh` — its only two real callers are `checksum` and `rddir`, both deferred: nothing to port |
| `src/kernel/FILE2.BDO` | `GET_NALBS`, `getblock` (+ internal `lefttst`/`righttst`/`rblok`/`rblok0`) | `SEARCH`/`SEARCHN`/`DELETE`/`RENAME`/`OPEN`/`MAKE` and the rest of the file (80+ labels) — all route through `srch`, which calls `rddir` (`FILE1.BDO`'s own deferred routine, blocked transitively by `FLUSH_FILE0`'s real segment-relative-addressing gap, `FILE3.BDO`); `GET_DIR_EXT`/`RESTORE_RC` are themselves clean, self-contained FCB arithmetic, but their only real caller `CLOSE_FCB` (part of `CLOSE1`) also calls `srch` — porting them alone wouldn't un-defer anything |
| `src/kernel/FILE3.BDO` | `PACK_SDCNT`, `COMPARE`, `SEARCH_OLIST`/`SEARCH_LLIST`/`SEARCHN_LIST` (+ internal `search_listo`), `DELETE_ITEM`, `CREATE_ITEM`, `SET_OLIST_ITEM` | `TST_OLIST`/`CHK_OLIST`/`PR_TERM` — on closer reading (not as clean as an earlier pass of this file claimed) `CHK_OLIST` calls the still-unported `openx06`/`compare_pds` and relies on its own caller having pushed a word for it to `pop` partway through (an implicit calling convention, not a boundary stub); `PR_TERM` calls the still-unported `tst_tbl_lmt`/`remove_locks`/`func48`; the rest of the file — not reconned this pass; `FLUSH_FILES`/`FLUSH_FILE0` — `FLUSH_FILE0` dynamically switches `ES` to a PD's own UDA segment (`mov es,p_uda[bx]`) then reads a fixed offset off it, the same real segment-relative-addressing gap as `PROC.RTM`/`ML.MEM`; one further routine (not identified by name) uses `std`, the same gap as `MAU.MEM` |
| `src/kernel/FILE4.BDO` | `func24`, `func25`, `func29`, `func32`, `FUNC44`, `FUNC45`, `func51`, `func52` | `func13`-`func23`, `func30`, `func33`-`func43`, `FUNC46`, `FUNC48` — all route through `srch`/`rddir`, `CURSELECT`/`initialize`, `CHECK_FCB`, or `FLUSH_FILES`, all transitively blocked by `FLUSH_FILE0`'s real segment-relative-addressing gap (`FILE3.BDO`); `GOBACK` — a 2-instruction tail jump into `BDOSIF.BDO`'s own module-dispatch epilogue (`BDOS_RETURN`), the same jump-table-dispatcher class already deferred elsewhere, not worth porting alone; the rest of the file not reconned this pass |
| `src/kernel/UTIL.MEM` | `getmd`, `freemd` | — (both of this file's routines are ported) |
| `src/kernel/MEMORY.MEM` | `malloc_entry`, `mfree_entry` (+ internal `mall_try_mau`, `mfree_it`) | — |
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

- `src/kernel/RPL.SUP` (`rpl_entry`) — builds a custom far-return stack
  frame by hand and ends in a real `retf` to transfer control into an
  RSP's own entry point. Needs far CALL/JMP/RET support in
  `tools/i86sim.py` (not yet implemented) and a controllable stub for
  `mpmif`'s queue-read behavior, neither of which the boundary-stub
  convention above covers (this routine's whole point *is* the
  cross-segment transfer, not a call that's safe to skip over).
- `src/kernel/CIOIF.CIO`, `src/kernel/RTMIF.RTM`, `src/kernel/SUPIF.SUP`,
  `src/kernel/MEMIF.MEM` (each one's `init`/`entry`/`mpmif`/`xiosif`) —
  every kernel module's own central dispatcher: module init via far calls,
  then an indirect call through that module's own function table (`call
  cs:functab[si]`), none of whose targets are ported yet, plus the same
  far-call/far-ret needs as `RPL.SUP` above. `SUPIF.SUP`'s `user_entry`
  additionally needs a real `IRET` and interrupt-entry convention this
  tree has nowhere else yet.
- `src/kernel/PROC.RTM` (`proc_creat`, `getpdadr`, `freepd`) and
  `src/kernel/ML.MEM` (`mlalloc_entry`, `mla_makemau`/`mla_initmau`,
  `mlfree_entry`) — these genuinely need real segment-relative
  addressing, not just the boundary-stub convention. Both routines
  dynamically compute a *different* memory segment at runtime (a newly
  created process's own UDA in `proc_creat`'s `cp_uda`; a newly allocated
  MAU's own segment in `mla_initmau`) and then switch `DS`/`ES` to it so
  that an ordinary *fixed* field offset (`u_initcs`, `sat_start[si]`, ...)
  resolves to the right place in *that* segment specifically. `tools/
  i86sim.py` models one flat 64KB array with segment overrides as no-ops
  (see its own docstring) — correct for every file ported so far, where a
  fixed symbolic offset only ever means one unchanging absolute address,
  but wrong here, where the same symbolic offset must resolve to a
  *different* absolute address depending on which segment was just
  switched into. (`src/kernel/QUE2.RTM`'s `qspace`/`qrelease` looked like
  they might hit the same wall at first glance — they also juggle `DS`/`SS`
  — but don't, and are ported above: they're building a temporary
  on-stack parameter block, the same address either way, not switching to
  a genuinely different segment's own data. The ML.MEM/PROC.RTM case is a
  real, different problem.)
**Tier 3 additions, same two categories as above:**

- `src/kernel/MAU.MEM`, `src/kernel/SHARE.MEM` — the *same* real
  segment-relative-addressing gap as `PROC.RTM`/`ML.MEM`: both
  dynamically switch `DS` to a MAU's own segment (`mov ds,m_start[bx]`)
  and then access fixed suballocation-table offsets (`sat_start[bx]`,
  `sat_length[bx]`, ...) that only resolve correctly relative to *that*
  segment. `MAU.MEM`'s `mau_insert` also uses `std`/`rep movsb` (reverse
  direction), a second, independent gap -- `tools/i86sim.py`'s string
  instructions are forward-only (see its own docstring).
- `src/kernel/LOAD.SUP` — the *same* real segment-relative-addressing
  gap as `PROC.RTM`/`ML.MEM` again: `mov es,ldtab_start[si]` switches
  `ES` to a load table entry's own dynamically-chosen segment, then
  writes fixed offsets off it (`es:.5`, `es:2[bx]`, `es:3[bx]`, ...) to
  build that segment's base page.
- `src/kernel/DSPTCH.RTM` (`jmp cs:dsp_table[bx]`), `src/kernel/
  ABORT.RTM` (`call cs:abort_tab[di]`), `src/kernel/BDOSIF.BDO` (`call
  cs:btab_addr[si]`) — each one's own central jump-table dispatch, the
  same deferred class as `MEMIF.MEM`/`CIOIF.CIO`/`RTMIF.RTM`/
  `SUPIF.SUP` (now `BDOSIF.BDO`'s turn, for BDOS). `DSPTCH.RTM`
  specifically is the real process dispatcher's own internals (stack
  switching between processes, `switch:`) -- also the direct subject of
  the live, ongoing scheduler-hang investigation in this doc's #83-#98;
  porting it now would duplicate that work, not help it.
- `src/kernel/LOAD.SUP`, `src/kernel/DATA.BDO`, `src/kernel/
  FILE2.BDO`, `src/kernel/FILE4.BDO` — not reconned in depth this pass
  (only a grep-level pattern check for the known blockers above, which
  didn't trip on these). `src/kernel/CHARIO.CIO` (the large, deeply
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
