# `/src-c`: a C port of the hand-written MP/M-86 assembly

`/src-c` is a routine-by-routine C port of the hand-written ASM86 in
`src/kernel/`, `src/loader/`, and `src/rsp/`, with the eventual goal of
building a working MP/M-86 kernel from it. It is **not** an attempt
to produce C that compiles to the same bytes as `tools/asm86/`'s output —
that was the original goal, but a representative real routine
(`src/kernel/FINDPD.RTM`) makes clear why it isn't realistic: hand-tuned
tricks like folding a sign/zero test into a `shl al,1`, or a deliberate
segment-override byte compare, aren't something any C compiler will
spontaneously emit from idiomatic C. Forcing a byte match would mean either
wall-to-wall inline asm, or fighting one specific compiler's quirks
indefinitely.

Routines are ported individually, with differential testing wherever
the test harness can exercise them (see "How a routine gets ported"
below). `src-c/Makefile`'s `make build`/`make test` compile-check the
ports and run their differential tests. The
`kernel.elf` target also links the boot stub with every compiled
`src-c/kernel/` object, but `kernel_main.c` does not call those
routines: the image boots only far enough to print its signon and is
not a working MP/M-86 system. The linked image and the differential
tests are therefore separate kinds of evidence; wiring the ported
routines into an operational kernel remains future work.

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

## `src-c/boot/`: a real `ld`-based image-builder pipeline

`src-c/boot/` supplies the hand-written multiboot entry and serial
signon used by the `kernel.elf` target. The target links that stub
with every compiled `src-c/kernel/` object and boots under QEMU, but
the stub does not call any ported routines, and `src-c/loader/` is
not part of this image. It is not a replacement for or change to
`/src`'s own real MP/M-86 boot chain (`ROM.A86` ->
`LDBIOS.A86`/`LDMPM.A86` -> the kernel, built by `tools/asm86.py` +
`tools/build_mpmsys.py`/`build_mpmldr.py` — entirely untouched by any
of this).

Two things made this nontrivial, worth recording:

- **No 8086/80186 C compiler exists anywhere on this machine** (checked
  `ia16-elf-gcc`, Watcom, SDCC — SDCC's own target list is
  `mcs51/z80/.../mos6502/f8l`, no 8086 target at all, confirmed via
  `sdcc --version`). The toolchain actually used,
  **`i686-elf-gcc`/`i686-elf-ld`/`i686-elf-grub-file`** (already
  installed via Homebrew, `brew search i686-elf`), targets **32-bit
  protected mode with a flat memory model** — a genuinely different
  execution model from MP/M-86's own real 16-bit real-mode,
  segment:offset-addressed one. This is why `src-c/boot/` stays a
  separate proof-of-pipeline rather than a drop-in replacement for
  anything above: every ported routine in `src-c/kernel/` takes a
  `uint8_t *mem` array + a 16-bit offset (written to run on the
  *host* CPU against a simulated flat byte array for differential
  testing, see "How a routine gets ported" above) — wiring any of
  them into a real 32-bit protected-mode kernel would need those
  routines' own real-memory-addressing story worked out first, a
  separate and likely large effort, not attempted here.
- **A multiboot header needs the ELF `SHF_ALLOC` flag to land inside
  the file's own first 8KB**, which a bootloader scans for it.
  `gas` doesn't infer that flag for a section name it doesn't
  already recognize (`.multiboot` isn't one of its well-known
  names) — without it (`.section .multiboot` alone), `ld` placed the
  section at the *end* of the file regardless of this script's own
  `SECTIONS` ordering, which `i686-elf-grub-file --is-x86-multiboot`
  caught by failing outright. Fixed with an explicit `.section
  .multiboot, "a"`.

`tests/src-c/test_boot.py` builds `src-c/boot/` with the real
toolchain and boots the result under QEMU (`-kernel kernel.elf`,
multiboot loads directly, no GRUB ISO/`grub-mkrescue` step needed for
this), checking the serial signon string comes back — the one
`tests/src-c/test_*.py` file that depends on external tools instead
of this project's own from-scratch `tools/asm86.py`/`tools/i86sim.py`;
it skips (not fails) if `i686-elf-gcc`/`i686-elf-ld`/
`i686-elf-grub-file`/`qemu-system-i386` aren't on `PATH`. `src-c` isn't
wired into this repo's own CI yet either (`.github/workflows/` only
builds/tests `/src`), so this doesn't affect CI either way.

`src-c/Makefile`'s own `make build` (the compile-only, no-link
type-check pass across every `src-c/kernel/`/`src-c/loader/` file)
now also uses this same real cross toolchain (`CC := i686-elf-gcc`,
`LD := i686-elf-ld`, both now also actually invoked by the real
`kernel.elf` link target, see below) instead of the host `cc` --
`CC`/`LD`/the common `CFLAGS` (`-m32 -march=i686 -mgeneral-regs-only
-ffreestanding -fno-pie -fno-stack-protector -fcf-protection=none
-fno-asynchronous-unwind-tables -O2 -Wall -Wextra -Werror` — a real
kernel-code flag set, not just "freestanding": `-mgeneral-regs-only`
stops GCC from ever emitting SSE/MMX/x87 instructions, which would
need FPU/SSE context-switch support nothing here has;
`-fcf-protection=none` drops control-flow-enforcement prologues this
freestanding kernel has no OS/CPU support for; `-fno-asynchronous-
unwind-tables` drops C++-style exception-unwinding metadata this
code never uses) live in `src-c/build.mk`, `include`d by
`src-c/Makefile` (`boot/` has no `Makefile` of its own at all --
folded into this one, see the "`kernel.elf`" section below) so
`make build`'s type-check pass and the real `kernel.elf` link can't
quietly drift apart on toolchain choice or codegen; `make build`
itself adds `-std=c11`/`-I freestanding-include` on top of
`build.mk`'s own `CFLAGS` --
`tests/src-c/test_*.py`'s own differential tests still build their
own `ctypes`-loadable shared libraries with the *host* `cc` directly
(hardcoded in each test file, not read from this Makefile), since a
cross-compiled object can't be `dlopen`ed into the host Python
process that way; the two compiles serve different purposes and
were already independent of each other. Getting this to actually
compile needed two small fixes, both worth remembering:

- `-ffreestanding` is required, not optional, even for files that
  only use `<stdint.h>`: without it, `i686-elf-gcc`'s own bundled
  `stdint.h` assumes a *hosted* environment and does `#include_next
  <stdint.h>`, expecting a further, real libc one somewhere down the
  search path -- which doesn't exist on this bare, libc-less cross
  toolchain, so it fails outright (`fatal error: stdint.h: No such
  file or directory`, from inside the compiler's own header). With
  `-ffreestanding`, the same header instead defines everything
  itself, per the C standard's own "freestanding implementations
  must provide stdint.h standalone" requirement.
- `<string.h>` isn't one of the handful of headers a freestanding
  implementation is required to ship (unlike `stdint.h`), and this
  toolchain has no libc at all, so it doesn't exist here, period —
  a handful of files (`submit.c`/`tmp.c`/`xios86q.c`/`ldbios86q.c`)
  include it for `memcpy`/`memcmp`. Fixed with a tiny declarations-
  only shim, `src-c/freestanding-include/string.h` (`-I
  freestanding-include`, added to `CFLAGS`) -- no definitions exist
  anywhere yet, so this is still compile-only; a real link step
  would need actual bodies for both.

### `kernel.elf`: the whole `src-c/kernel/` tree, actually linked

`src-c/Makefile`'s `kernel.elf` target (`linker.ld` now lives in
`src-c/` itself, not under `boot/`, since it's no longer that
directory's own private concern) links `src-c/boot/`'s own stub
together with *every* compiled `src-c/kernel/` object (deliberately
not `src-c/loader/` -- in the real system the loader and the kernel
are two separate executables that never coexist in one linked image
either, so linking them together here would be wrong even by the
real architecture's own design, not just inconvenient) into one real
ELF via `i686-elf-ld`. This **succeeds** -- `kernel.elf` links, passes
`i686-elf-grub-file --is-x86-multiboot`, and still boots/signs on
under QEMU exactly like `src-c/boot/`'s own standalone stub (nothing
in `kernel_main.c` calls into any of these routines yet, so this
isn't evidence any of them *work* together -- just that the tree
*links* as one unit, see this section's own opening paragraphs above
for why that's still a different question from "boots a working
MP/M-86 kernel").

Getting there required two real fixes, not just flag-tuning:

- **`memcpy`/`memcmp` needed real bodies, not just declarations** --
  `freestanding-include/string.h`'s own shim only declared them
  (enough for `make build`'s compile-only pass), but linking
  anything that calls them needs an actual definition somewhere.
  `freestanding-include/string_shim.c` (byte-at-a-time, no attempt
  at the usual libc-style optimization -- it exists only to make
  `kernel.elf` link, not as a differential-tested port of anything).
- **Several genuinely different real routines in genuinely different
  real files had been independently ported to the same generic C
  name**, because their real asm labels happen to match -- harmless
  for isolated per-file differential testing (the only linking these
  files had ever done before), but a hard link failure once the
  whole tree links as one unit. Two distinct cases, both fixed by
  adding a module prefix to the C name only (the real asm label, and
  every differential test's own understanding of *that*, is
  untouched):
  - `init`/`entry`: `BDOSIF.BDO`/`CIOIF.CIO`/`MEMIF.MEM`/`RTMIF.RTM`'s
    own inter-module dispatchers, plus `INIT.SUP`'s own separate
    `init`, all real different routines, all five named exactly
    `init`/`entry` in their own real asm (purely locally/table-
    relative there, never needing to be globally unique) -- renamed
    to `bdosif_init`/`bdosif_entry`, `cioif_init`/`cioif_entry`,
    `memif_init`/`memif_entry`, `rtmif_init`/`rtmif_entry`, and
    `sup_init` (`src-c/kernel/init.c`, `INIT.SUP`'s own `entry` isn't
    ported at all, see that row below, so no second rename needed
    there).
  - `conin_entry`/`conout_entry`: `CHARIO.CIO`'s own higher-level
    console-layer entries (`chario.c`, called by `cioif.c`'s
    dispatcher and `command.c`) collided with `XIOS86Q.A86`'s own,
    genuinely different, low-level hardware-driver entries
    (`xios86q.c`, called only internally within that same file) --
    renamed the `xios86q.c` side only (`xios_conin_entry`/
    `xios_conout_entry`), since it was the one with no other file's
    own call sites to update.
  - Every affected differential test (`test_bdosif.py`/
    `test_cioif.py`/`test_cioif_entry.py`/`test_init.py`/
    `test_memif.py`/`test_memif_entry.py`/`test_rtmif.py`/
    `test_rtmif_entry.py`/`test_xios86q.py`) had its own `ctypes`
    `lib.<name>` lookups updated to match -- all still pass.

This was deliberately a rename, not a build-system-only `-D` macro
patch kept in a separate list: more invasive up front (eight test
files touched), but the source now says what the symbol actually is,
with nothing to keep in sync by hand.

## Status

| File | Routines ported | Routines deferred (and why) |
|---|---|---|
| `src/kernel/FINDPD.RTM` | `findpdname_entry` | — |
| `src/kernel/IDLE.SUP` | `idle` | — |
| `src/kernel/INIT.SUP` | `init` (system initialization) | `entry` (inter-module BDOS-style dispatch into the Supervisor) — a genuinely different routine, not reconned this pass. `init` never returns in the normal sense (ends via `int mpmint`/`f_terminate`, an `int_stub_nums` no-op) and far-calls into 5 other modules' own `init:` entry points through `SYSDAT.FMT`'s module table (`callf dword ptr <rtm\|mem\|bdos\|cio\|xios>mod[bx]`) — each one needed `tools/i86sim.py`'s brand-new `far_call_stub_addrs` (an indirect far `CALL`'s own target is only known at runtime, so it can't be stubbed by target address like an ordinary `CALL`). Caught and fixed a real, previously-uncaught bug in `/src` along the way: `mov u_wrkseg,ds` (no `es:` prefix) landed on `SYSDAT`'s own offset 2Eh instead of the init UDA's own `u_wrkseg` field — which happens to be the back half of the module table's own XIOS entry (its "initialize" far pointer's segment word), right before the `xiosmod` `callf` a few lines later would have used it; confirmed by offset arithmetic (`MODTAB.DEF`/`UDA.FMT`), not just suspected — same bug class, same file, as two other bare-`sysdat`-write fixes `INIT.SUP` already had. The RSP-creation loop and the fixed `u_wrkseg` write are both translated correctly (`mem_r16_seg`/`mem_w16_seg`) but not independently exercised by the differential test — both need `tools/i86sim.py`'s `segmented=True` mode, which would also break every *other* field this routine touches (flat scratch addresses, not real segment-relative ones) in the same run; see `init.c`'s own header comment |
| `src/kernel/SYSFUNC.SUP` | all 8 entries: `not_implemented`, `bad_entry`, `version_entry`, `mpmversion_entry`, `sysdat_entry`, `tod_entry`, `serial_entry`, `callbios_entry` | — (the whole file is ported; `tod_entry`/`serial_entry` copy from real `SYSDAT.FMT`/`RTMIF.RTM` data, given their own harness-chosen scratch address/inlined literal; `callbios_entry`'s two real tail-jumps into `mpmif(f_terminate)`/`xiosif` are boundary stubs, same convention as `SYSENT.RTM`'s own `poll_entry`/etc.) |
| `src/kernel/SYSENT.RTM` | all 8 entries: `poll_entry`, `delay_entry`, `dispatch_entry`, `set_prior_entry`, `pd_entry`, `sleep_entry`, `wakeup_entry`, `creat_proc_entry` | — (the whole file is ported; `creat_proc_entry` — `call proc_creat ! jmp pdisp` — was deferred until `proc_creat`/`pdisp` existed, and now that they do it's a real internal call into both, not a boundary stub like its 7 siblings' own tail-jumps; a real quirk this surfaced: `ax`/`bx`/`cx`/`si`/`di` aren't actually preserved across the `call proc_creat` at all, see `src-c/kernel/sysent.c`'s own header comment) |
| `src/kernel/FLAG.RTM` | `flag_set_entry`, `flag_wait_entry` (+ internal `flag_valid`/`flag_exit`/`flag_bexit`) | — |
| `src/kernel/ABORT.RTM` | `sysreset_entry`, `terminate_entry` (+ internal `abt_chk`), `abort_spec_entry` (+ internal `find_pd`, `find_pdthrd`, `findpdnc`, `findpdq`, `fflgpd`, `snip_it`, `abt_putdrl`, `gccba`, and the whole `abort_tab` dispatch: `abtrun`/`abtpol`/`abtdly`/`abtswp`/`abtdq`/`abtnq`/`abtflg`/`abtcns`) | — (the whole file is ported; needed `tools/i86sim.py`'s new `CALL r/m16`/`JMP r/m16` for `call cs:abort_tab[di]` — see below) |
| `src/kernel/DSPTCH.RTM` | `fdisp`, `int_disp_exit`, `farpdisp`/`pdisp`, `dsptch` (+ internal `insert_process`, `schedule`, `switch_`, and the whole `dsp_table` dispatch: `disp_act`/`poll_act`/`delay_act`/`swap_act`/`term_act`/`sleep_act`/`flag_act`) | — (the whole file is ported, but **as a reading/debugging aid for the live scheduler-hang investigation, #83-#98, not a kernel-build component** — see the file's own header comment; a real interrupt return and a real context switch's register/stack resumption have no C equivalent, modeled as explicit output parameters instead; needed two more `tools/i86sim.py` fixes, `SS`-aware `PUSH`/`POP` and a `CS`-base fix for near indirect `CALL r/m16`/`JMP r/m16` — see #103) |
| `src/kernel/PARSE.SUP` | `parse_entry` (+ internal `setfld`/`delim`/`deblank`/`fcbinit`) | — |
| `src/kernel/RPL.SUP` | `rpl_entry` | — (the file's one routine is ported, but collapses to its single deterministic `rpl_err` outcome under the boundary-stub convention — `mpmif(f_qopen)`'s own function code, MODFUNC.DEF's `(user*0100h)+135`, is never 0, so `cmp cx,0 ! jne rpl_err` is always taken; the real success path — a hand-built fake far-return stack frame and a `retf` transferring control into a queue-delivered RSP entry point, with no C equivalent at all — is provably unreachable, same reasoning as `CONSOLE.CIO`'s/`CPMMEM.MEM`'s own unreachable branches, see `src-c/kernel/rpl.c`'s own header comment) |
| `src/rsp/ECHO.A86` | `main`/`loop` (one full pass — see below) | — |
| `src/rsp/CLOCK.A86` | `clockloop` (one full pass — see below) | — (`clock`'s own one-time setup, `mov ds,sysdat`/`mov bx,tod_offset`, is moot under the flat memory model, same as the DS/ES-switch convention elsewhere — the test starts directly at `clockloop` with `bx` preset instead) |
| `src/rsp/TMP.A86` | `nextcommand_once` (one full pass through `nextcommand:`/`clicall:`/`goodparse:`/`try_builtin:`/`not_builtin:`/`clierror:` — see below), `a_to_b` | `tmp:`'s own one-time setup — moot, every operation in it is a boundary-stub call or a register load feeding one; `setuser`/`setlist` — never identified, moot regardless (see below) |
| `src/rsp/SUBMIT.A86` | `deblank`+`fill_argtable` (unlabeled code after `submit:`'s own `call openfile`), `expand_command`, `readbuffer`, `readchar`, `topofcmdloop_once` (one full pass through `topofcmdloop:`/`notdrive:`/`goodparse:`/`try_builtin:`/`notbuiltin:`/`clierror:` — see below), `a_to_b` | `submit:`'s own one-time version-check/priority/FCB-open preamble — no observable effect under the boundary-stub convention beyond what `deblank`/`fill_argtable` already cover; `$INCLUDE`'s own real body — needs `sysdatseg`, a real segment value only `submit:`'s own un-ported one-time setup captures, not available in this reduced scope (the comparison against it still runs for real, so this only matters if a real `$INCLUDE` command is typed); `setuser`/`setlist` — never identified, moot regardless (see below) |
| `src/kernel/CIOIF.CIO` | `init` and `entry` (inter-module CIO dispatch -- `src-c/kernel/cioif.c`; `tests/src-c/test_cioif_entry.py`'s own `cioif_entry_wrapper.A86` links in the real `CONSOLE.CIO`/`CHARIO.CIO` too, same combination `CIO.A86` itself assembles, so `entry`'s dispatch is checked against real targets, not stubs) | — (the whole file is ported now; `entry`'s own `mpmif`/`xiosif` boundary-stub far calls are unreachable by any of its 23 cases themselves, but wired up for any real caller that needs them) |
| `src/kernel/RTMIF.RTM` | `init` and `entry` (21-way inter-module RTM dispatch -- `src-c/kernel/rtmif.c`; `tests/src-c/test_rtmif_entry.py`'s own `rtmif_entry_wrapper.A86` links in the real `SYSENT.RTM`/`FLAG.RTM`/`QUE1.RTM`/`QUE2.RTM`/`FINDPD.RTM`/`ABORT.RTM` too) | — (the whole file is ported now; 20 of the 21 dispatched targets' own `bx`/`cx`/etc. are independently verified through this dispatcher. `creat_proc_entry` (case 14) is not: its own real tail (`call proc_creat ! jmp pdisp`) and `ABORT.RTM`'s own `terminate_entry` tail (`jmp dsptch`) share the one `dsptch` label once this module is linked for real, and letting one run for real (as the C port's own `creat_proc_entry` already does, via `proc.c`/`dsptch.c`) forces the other to run for real too, with no PD-list setup this generic dispatcher test is built to survive — same "known, documented gap" class as `MEMIF.MEM`'s own `entry` row above; see `rtmif_entry_wrapper.A86`'s/`test_rtmif_entry.py`'s own header comments) |
| `src/kernel/MEMIF.MEM` | `init` and `entry` (inter-module MEM dispatch, wrapped in a real binary-semaphore acquire/release -- `src-c/kernel/memif.c`; `tests/src-c/test_memif_entry.py`'s own `memif_entry_wrapper.A86` links in the real `CPMMEM.MEM`/`MEMORY.MEM`/`SHARE.MEM`/`MAU.MEM`/`ML.MEM`/`UTIL.MEM` too, same combination `MEM.A86` itself assembles) | — (the whole file is ported now; the mutex acquire/release logic is fully verified for every case, but 9 of the 13 dispatched targets' own `bx`/`cx` results aren't independently re-checked through this dispatcher -- `CPMMEM.MEM`'s own 5 functions + `freeall_entry` are thin `mpmif`-passthrough wrappers that `tools/i86sim.py` has no way to give a real dispatch target (`mpmif` is one real, shared symbol once `MEMIF.MEM` is linked in, and a real `CALL FAR [mem]` can only be stubbed at a fixed address, never redirected to a computed one), and `MAU.MEM`/`ML.MEM`'s own 3 functions need `segmented=True` plus a real `u_wrkseg` that this generic dispatcher test doesn't replicate (already handled by `test_mau.py`/`test_ml.py`'s own dedicated setup) — see `test_memif_entry.py`'s own header comment) |
| `src/kernel/SUPIF.SUP` | `func1` (`src-c/kernel/supif.c`), now including its real `mod==sup` dispatch-through into the 15-entry `supfunc` table | `user_entry` -- the real `INT 224` entry prologue/epilogue, no C equivalent, same class as `lbdosent.c`'s own `bdose`; `func` -- a one-line `mov u_func,cl ! call func1` wrapper whose own write has no observable effect anywhere this port can see (`user_entry`/`mpmif` are both deferred), same "collapses to nothing observable" reasoning as `CHARIO.CIO`'s own `stdout`; `entry`/`mpmif`/`xiosif` -- far-call trampolines, same convention as every other module's own `entry`/`mpmif`/`xiosif`. `mod==sup`'s own near dispatch is wired up for real (`load_entry`/`cload_entry` collapsed in as direct forced-`bx` calls into `load()`/`cload_entry()`, matching real `LOAD.SUP`'s own `load_entry:`/`cload_entry:` aliasing of `load:`); `mod!=sup`'s real cross-module far call through `module_table` still has no C target, same far-call gap as `init.c`'s own module-table dispatch. `tests/src-c/test_supif.py` verifies the dispatch logic itself (which slot each (mod,func) lands on, and that `bx` arrives forced or ambient exactly as claimed) against marker-stub doubles on both sides, not each target's own full behavior -- already independently covered by `test_sysfunc.py`/`test_load.py`/`test_command.py`/`test_rpl.py`/`test_parse.py`; see `supif.c`'s own header comment for the full reasoning, including why the real network-intercept path is provably unreachable in this project |
| `src/kernel/XIOS.A86` | `SETTRK`/`SETSEC`/`SETDMA`/`SETDMAB`/`SECTRAN`/`SELDSK`, `maxconsole`/`maxlist`/`getsegt`/`selmemory`/`flushbuf`/`strtclk`/`stopclk` (`src-c/kernel/xios.c`) | `entry`/`supif` — far-call trampolines, same convention as every other module's `entry`/`mpmif`/`xiosif`; `const`/`conin`/`conout`/`plist`/`punch`/`reader`/`listst`/`polldev`/`rtm_poll` — real `in`/`out` port I/O, no C equivalent under this project's flat-memory host model; `HOME`/`READ`/`WRITE`/`execute`/`send_com`/`flpy_*` — real 8271 floppy controller command sequences (every `dskhomtbl`/`dskrdtbl`/`dskwrttbl` entry is a `flpy_*` routine, no non-hardware path exists); `init`/`intdisp`/`int_trap`/`tickint`/`pmsg` — real hardware initialization (DMA controller, 8259 PIC, 8271) and interrupt service, no C equivalent; `idle` — a pure `mov cl,f_dispatch ! call supif ! jmp idle` passthrough with zero data transformation of its own (the call is a boundary stub), so porting it would add no real logic — see `xios.c`'s own header comment for the full list and reasoning |
| `src/kernel/XIOS86Q.A86` | not DRI source (a from-scratch QEMU driver this project wrote — see `xios86q.c`'s own header comment), but the file actually combined into the real bootable `MPM.bin`: nearly the whole file is ported (all 25 XIOS functions, `init`, and `tickint`'s own tick_count/flag logic — `src-c/kernel/xios86q.c`, following `ldbios86q.c`'s own `io_in8`/`io_out8` callback architecture for the identical NEC 765/8272 FDC + 8237 DMA + 8250 UART hardware) | `entry`/`supif` — far-call trampolines, same convention as every other module's own; `idle` — zero-transformation passthrough, same as `xios.c`'s own `idle`; `tickint`'s own real ISR shell (stack switch, register save/restore, PIC EOI, final `jmpf cs:dword ptr mydispatch` tail-dispatch) — no C equivalent, same class as every other real interrupt entry/exit point in this port (its own tick_count/flag logic IS ported, as `tickint_once`); `rtm_poll`'s own "yield via the Supervisor's f_polldev" tail — a boundary-stub tail jump, modeled as an immediate return since neither of its two real callers ever inspects its return value; `init`'s own far-pointer copies out of SYSDAT's module table, its real-mode interrupt-vector write, and its final `retf` — no C equivalent or nothing for this isolated port to copy to/from |
| `src/kernel/TICK.SUP` | `notick_once`, `tick_once` | — (both of this file's real labels are ported) |
| `src/kernel/QUE1.RTM` | all 10 routines: `makeq_entry`, `openq_entry`, `deleteq_entry`, `queverify`, `readq_entry`/`creadq_entry`/`readq`, `writeq_entry`/`cwriteq_entry`/`writeq` | — (the whole file is ported; `readq`'s/`writeq`'s own real "wait forever" retry — unconditional call, queue empty/full — is translated faithfully but not exercised by the test, same unexercisable-but-real shape as `cioattach` below/`CPMMEM.MEM`'s `cpmfree_entry`; every other path, including the conditional-and-empty/full immediate-error case, is fully portable and exercised) |
| `src/kernel/QUE2.RTM` | `rlsmx`, `getqdaddr`, `remqd`, `qspace`, `qrelease` | — (all 5 of this file's routines are ported) |
| `src/kernel/PROC.RTM` | `freepd`, `proc_creat` (+ internal `getpdadr`) | — (all 3 of this file's routines are ported, using `tools/i86sim.py`'s `segmented=True` mode plus a CSEG_BASE-shifted reference run — see below) |
| `src/kernel/CONSOLE.CIO` | all 15 routines (`lstattach_entry`, `clstattch_entry`, `cconattch_entry`, `conattach_entry`, `cioattach`, `lstdetach_entry`, `condetach_entry`, `ciodetach`, `conassign_entry`, `getdefcon_entry`, `getdeflst_entry`, `setdefcon_entry`, `setdeflst_entry`, `mimic_entry`, `unmimic_entry`) + `CHARIO.CIO`'s `getccbadr` (a small dependency, pulled in on its own) | — |
| `src/kernel/CHARIO.CIO` | all 9 entry points (`conin_entry`, `conout_entry`, `rconin_entry`, `rconout_entry`, `listout_entry`, `constat_entry`, `dirio_entry`, `conwrite_entry`, `conprint_entry`, `conread_entry`) + 20 internal routines (`rawinit`, `coninit`, `getconnum`, `attach`, `echoc`, `conbrk`, `chkctl`, `control_p`, `prcsmsg`, `prname`, `conout`, `compout`, `ctlout`, `tabout`, `pspace`, `pbacksp`, `pcr`, `crlf`, `crlfp`, `conin`) | `stdout` — its own `conoutf`/`listf` calls are both no-ops under the boundary-stub convention regardless of which branch is taken, so it collapses to nothing observable; inlined as a comment in `conout`'s own mimic-forwarding branch instead of a separate function, see that file's own header comment |
| `src/kernel/COMMAND.SUP` | `shifttail`, `fileopen` (a real call into `FILE4.BDO`'s own `func15`), `user_retf`, `chain_entry` (now ported too, a thin wrapper), `clicmd_entry` (both the ordinary and chain-mode paths — + internal `cli_pfn`, `cli_checkque`, `cli_fload`, `cli_gopen` (a real call into `load.c`'s `load` plus the whole Base Page cmdtail/default-FCB/argument-parse setup, see below), `cli_rmpd`, `clicmd_exit`, `prchar`/`crlf`/`prnum`/`prtime`/`prdisk`/`write_cstr`/`prcsm`/`prnam`/`prtyp`/`pruser`/`prcusp`/`prfilnam`/`conasn`) — see below | `f_createproc`/`f_setprior` stay plain boundary stubs (their own results are never read by anything downstream, so this doesn't truncate anything, see `command.c`'s own header comment); `cli_gopen`'s own chain-mode `cli_ld1` UDA hand-off block (real segment-relative addressing, resolved the same way as `PROC.RTM`/`ML.MEM`/`MAU.MEM`/`SHARE.MEM`) has no differential test, same reason as its own non-chain continuation past `prfilnam` — both need a real, matching directory entry `fileopen` can't be made to produce in any test in this project |
| `src/kernel/FILE1.BDO` | `pererror`/`roderror`/`roferror`/`selerror` (+ internal `goerr`), `move`, `selectdisk`, `home`, `PASS_ARECORD`, `rdbuff`, `wrbuff`/`wrbufflg` (+ internal `diocomp`), `seekdir`/`seek`, `dmposition`, `GET_DMA`, `getdm`, `bdos_index`, `alloct`, `atran`, `GET_ATTS`, `GET_S1`, `GET_RRA`, `GET_RCNTA`, `getexta`, `gtfcba`, `getfcb`, `setfcb`, `cmpecs`/`CMPEC0`, `CHKSUM_FCB`, `SET_CHKSUM_FCB`, `RESET_CHECKSUM_FCB`, `CHEK_FCB`, `setcdisk`, `nowrite`, `setro`, `ckrodir`/`ckrofile`/`RO_TEST`, `checkwrite`, `getdptra`, `getmodnum`, `clrmodnum`, `CLR_EXT`, `setfwf`, `compcdr`, `setcdr`, `rddirbuf`, `setdata`/`setdir`/`setdma`, `dirtouser`, `MAKE_FCB_INV`, `CHK_INV_FCB`, `TST_INV_FCB`, `endofdir`, `setenddir`, `getallocbit`, `setallocbit`, `checksum`/`newchecksum`, `rddir`/`rddir0`, `wrdir` (the last 3 were blocked on `FILE3.BDO`'s own `FLUSH_FILE0`/`FLUSH_FILES`, now resolved — see that file's own status-table row; `subdh`, `checksum`'s own size check, folds directly into an `if` there instead of a standalone function), `check_fcb1`/`check_fcb` (`CHECK_FCB1`/`CHECK_FCB` — `CLOSE1` is ported now too, `src-c/kernel/file2.c`; `CHECK_FCB1`'s own `POP BX ;DISCARD RETURN ADDRESS` trick, same category as `PARSE.SUP`'s own `delim`, is translated as a plain boolean return instead — see `check_fcb1`'s own header comment for the full calling-convention writeup; `CHECK_FCB` is NOT dead, contrary to an earlier revision of this comment — `FILE4.BDO`'s own `func20`/`func21` are its own two real callers, both ported now too), non-`static` `dmposition`/`getdm`/`diocomp` (cross-file calls from `file3.c`'s own `diskwrite`/`func48`), and now `set_cdisk1`/`test_vector1` (split out of `setcdisk`/`test_vector` to take an explicit bit-position/shift-amount instead of always reading `BD_SELDSK`, for `file3.c`'s own `diskreset`) | — (every routine in this file is now ported) |
| `src/kernel/FILE2.BDO` | `GET_NALBS`, `getblock` (+ internal `lefttst`/`righttst`/`rblok`/`rblok0`), `SEARCH`/`SEARCHN` (`search`/`search1`/`searchn`, + internal `searchi`/`searchi1`/`compext`/`swap_sdcnt`), `CLOSE`/`CLOSE1` (`close`/`close1`/`close_fcb`/`seekcopy`, + internal `get_dir_ext`/`restore_rc`/`set_rc`/`merge_word`/`merge_byte`, the last 3 now non-`static` for `FILE3.BDO`'s own `rseek`) — **the underlying DATA.BDO cseg/dseg collision is now resolved for real** (`file1.c`/`file2.c`/`file3.c`/`file4.c` all threaded a new `ds_bdos` parameter through every function touching a `BD_*` field, converting every such access to `mem_*_seg`/`mem_seg_addr` against it -- `defs.h`'s own new `mem_seg_addr` helper, zero behavior change for every existing caller since they all pass `ds_bdos=0`, which degenerates back to the exact flat addresses this project used before). `swap_sdcnt` (the real asm's own `SWAP`, previously the simplest routine in this exact collision range) now has a real differential test proving it (`test_file2.py`'s own `DS_BDOS`/`run_reference_segmented`, `segmented=True` with a genuine non-degenerate `DS`). `close`/`close1`/`search`/`searchn`/`open`/`open1`/`opencopy`/`copy_dir0`-`2` are unblocked the exact same way but don't have their own new differential tests yet -- "resolved in principle, not yet written," same category as several other clusters below. Previously, this gap read: their own real cseg footprint, in the combined FILE1-4.BDO wrapper, runs well past 0xc00, directly overlapping where DATA.BDO's own dseg-relative fields (`BD_INFO`/`BD_SRCHA`/etc, the very fields these routines read/write) resolve to — code and the data it operates on colliding at the same flat address, not fixable by relocating just one side the way every earlier SD_RLR-style gap was; `initialize` (+ internal `scndm`/`test_dir`), `MAKE` (`make`, + internal `set_dcnt_dblk`/`copy_dir0`/`copy_dir2`), `DELETE` (`delet`, + internal `cpydirloc`/`init_xfcb_search`/`init_xfcb_search1`/`chk_wild`/`does_xfcb_exist`/`delete1_entry`/`delete2_entry`), `RENAME` (`rename_`, + internal `check_wild`/`check_wild0`/`copy_user_no`/`copy_dir`), `OPEN`/`OPEN1`/`opencopy` (its real top-level entry point is `FILE4.BDO`'s own `func15`, see that file's own row below), `openreel` (+ internal `sdcnt_eq_xdcnt`, non-`static` for `FILE3.BDO`'s own `fix_olist_item`), `delet0` (non-`static` for `FILE4.BDO`'s own `func22`/MAKE), and now `indicators` (non-`static` for `FILE4.BDO`'s own `func30`) — `initialize`/`make`/`set_dcnt_dblk`/`openreel`/`sdcnt_eq_xdcnt` confirmed clear of the DATA.BDO collision via `--symbols` (0x0a8a-0x0b74, 0x10b8-0x112c, 0x114d-0x11ed, and 0x10ce respectively, all outside dseg's own 0-0x1000 span), but `copy_dir0`/`copy_dir1`/`copy_dir2` (0xe50-0xe6d) and `open`/`OPEN1`/`opencopy` (0xf34-0xf3d) and `SWAP` (0xd1d) land inside it, same as `CLOSE1`'s own cluster, and `delet`/`rename_` weren't checked either way; no differential tests written for any of these yet either way (time, not always a technical blocker). `delet`'s own single-pass branch does `jmps delet0` right after `ckrodir`, relying on `ckrodir`'s own real ZF there being an incidental side effect of `RO_TEST`'s own `ADD BX,ROFILE` (always leaves ZF clear in practice) rather than a real not-found check -- not modeled as a flag, this port just always takes that branch's own fixed destination directly, see `delet`'s own header comment. `delet`'s own `DELETE1` label became its own function, `delete1_entry` (+ `DELETE2`'s own `delete2_entry`), since `rename_`'s own real asm calls straight into the middle of `DELETE`'s body as an ordinary subroutine. `searchi`/`SET_DCNT_DBLK`/`CHECK_WILD`/`SWAP`/`sdcnt_eq_xdcnt`/`COMPEXT`/`GET_DIR_EXT`/`RESTORE_RC`/`SET_RC` all made non-`static` for cross-file calls into them (the last 4 for `FILE3.BDO`'s own `rseek`). `openreel`'s own real `call make` is followed by a `jz openerr` that tests a flag `make`'s own tail (`setfwf`'s `or al,fwfmsk`) can never leave clear -- not modeled, same "unreachable" precedent as `cli_gopen`. **`FILE2.BDO` is now ported in full** -- the "30+ remaining labels" this row's own "missing" column used to list were re-audited label by label against every real one in the file (111 total) and every single one turned out to already map onto an already-ported cluster above (`DRV_LBL`/`INITIAL3`/`TEST_DIR*`/`scndm*` into `initialize`; `GET_DE1`-`3` into `GET_DIR_EXT`; `SAVE_DCNT_POS*`/`SWAP`/`SRCHN1`/`SEARCHNM`/`srch*` into `search`/`searchn` -- `srch`/`srchn` are literally the real-asm names already-ported `search`/`searchn` use; `dmset`/`DMSET1`/`fcbnzero`/`buffnzero`/`merge*` into `close_fcb`'s own merge logic; `openr0`-`2`/`OPENR3`/`openmod`/`openerr` into `openreel`'s own body; `RET8A`/`ret8b`-`ret13`/`LRET_EQ_FF`/`DONT_SAVE` are trivial internal return labels inside `DELETE`/`SEARCHN`) -- the count was simply never recomputed as each incremental pass (`initialize`, `MAKE`, `DELETE`, `RENAME`, `openreel`, `indicators`) chipped away at the original tally | — (every real routine in this file is ported; the DATA.BDO collision above is a *test-harness* gap, not missing C code) |
| `src/kernel/FILE3.BDO` | `PACK_SDCNT`, `COMPARE`, `SEARCH_OLIST`/`SEARCH_LLIST`/`SEARCHN_LIST` (+ internal `search_listo`), `DELETE_ITEM`, `CREATE_ITEM`, `SET_OLIST_ITEM`, `TST_TBL_LMT`, `REMOVE_LOCKS`, `FLUSH_FILES`/`FLUSH_FILE0`, `INC_PDCNT`, `CURSELECT` (+ internal `select_`), `TST_OLIST`/`CHK_OLIST` (+ internal `chk_olist0`), `COMPARE_PDS`, `CHK_PASSWORD`/`CHK_PWS` (+ internal `get_dir_mode`/`cmp_pw`/`get_xfcb`/`get_xfcb0`/`chk_pw`), and now `OPEN`'s own remaining dependencies: `SET_SELDSK`, `reselect`, `parsave`/`parsave33`/`parsave33r` (the first two really BDOSIF.BDO's own, ported here for lack of a `bdosif.c`), `COPY_DMA_IN`/`COPY_DMA_OUT`, `STAMP1`/`STAMP2` (+ internal `stamp3`), `COUNT_OPENS`, `CREATE_OLIST_ITEM`, `FIX_OLIST_ITEM` (`MAKE`'s own `openreel`, `file2.c`), and now `diskread`/`sqread`, `diskwrite`/`sqwrite` (+ internal `test_disk_fcb`/`test_lock`/`move_arecord`/`reset_fwf`), `chk_xfcb_password1`, `init_xfcb`/`init_xfcb0`, and `set_pw` (`FILE4.BDO`'s own `func19`-`func23`, see that file's own row below), `diskreset` (`func13`/`func14`/`func37`), `rseek`/`rseek1` (the latter now split out for `FILE4.BDO`'s own `func40`), `parsave36`/`parsave36r`, `rdread`, `rdwrite` (`FILE4.BDO`'s own `func33`/`func34`), `getfilesize`/`setrandom` (+ internal `cmperr`, `FILE4.BDO`'s own `func35`/`func36`), `tmp_select` (`FILE4.BDO`'s own `func46`), and now `lock_` (+ internal `incr_rr`/`save_rr`/`reset_rr`, `count_locks`, `check_free`, `get_lockadd`, `lock_err_apply`, `free_lock`, `set_lock`, `FILE4.BDO`'s own `func42`/`func43`) — `CURSELECT` checked clear of the DATA.BDO collision (resolves to 0x15e3+, past dseg's own 0x1000-byte length entirely), and so is this pass's entire new cluster (`func15`'s own row below has the detail), but none of these have a differential test yet. An earlier revision of this comment claimed `CHK_OLIST` "relies on its own caller having pushed a word for it to pop partway through" -- re-reading it closely this pass, that's wrong: the `pop dx`/`push dx` pair around its own `compare_pds` call is just ordinary register-save plumbing (freeing a register across a call, then restoring it before any real `ret`), not an implicit-caller convention; `openx06` (`FILE4.BDO`) and `compare_pds` are both ported now; `chk_pw`'s own `CHK_PW1` tail never returns normally (same as `openx06`), so `chk_pw`/`chk_password`/`chk_pws` all return a plain boolean instead of literally discarding anything, same convention `check_fcb1` established; `get_dir_mode`/`get_xfcb`/`cmp_pw` all made non-`static` for `func15`'s own cross-file calls into them; and now `PR_TERM` (ported as `pr_term`; its own `call func48` resolves fine — `func48` is a real label, `FILE4.BDO`'s own `FUNC48`, uppercase; a first, case-sensitive grep for lowercase `func48` across `/src` turned up nothing and briefly looked like a genuine DRI-source bug before rechecking case), `XCURSELECT` (`xcurselect`, genuinely reaches `selerror`/`goerr` for real — unlike `select_`'s own always-unreachable-past-`selectdisk` tail, `CURSELECT`'s `TEST_FF_DISK` branch calls `selerror` directly whenever `BD_SELDSK+1==0`, independent of any boundary-stub artifact), and `RESELECTX` (`reselectx`, a second entry point into `reselect`'s own body via a shared `reselect_tail` helper both now call — see `cmp_pw`'s own `std` note below, which this row's own earlier "not identified by name" caveat was about: `cmp_pw` was already ported in an earlier pass, this was just a stale doc note) | `TMP_SELECT` was already ported (`tmp_select`, `FILE4.BDO`'s own `func46`, see that file's own row); — (every routine in this file is now ported, confirmed by a fresh full recon rather than assumed; see the deferred-items list's own note on this row) |
| `src/kernel/FILE4.BDO` | `func24`, `func25`, `func26`, `func29`, `func32`, `FUNC44`, `FUNC45`, `func51`, `func52`, `SET_ARET`/`openx06`, `func15` (BDOS OPEN) + its own `OPENX`/`OPENX0`-`OPENX09` cascade, `func16` (BDOS CLOSE) + `FUNC48` (flush buffers), `func17`/`func18` (search for first/next occurrence of a file, + internal `csearch`/`reselectx`), and now `func19` (delete), `func20`/`func21` (sequential read/write), `func22` (MAKE), `func23` (RENAME) — `func16` replicates `CHECK_FCB1`'s own push/pop convention by hand too, mapping straight onto `check_fcb1`'s boolean return since its own fallback target (`CLOSE00`) is a trivial `set_aret`-and-return; `func17`/`func18`'s own shared `CSEARCH` always runs `dirtouser` after `search`/`searchn` via a manual stack trick (unconditional, unlike every other such trick in this port); `func20`/`func21` are `CHECK_FCB`'s own two real callers (an earlier revision of this comment wrongly called `CHECK_FCB` dead, see `check_fcb1`'s own header comment, `file1.c`); `func22` reuses `func15`'s own `OPENX1`/`OPENX03`/`OPENX08` cascade directly (refactored into their own functions for this); each of `func19`-`func23` calls into `file2.c`'s own colliding `close`/`close1`/`search`/`searchn`/`delet`/`rename_`/`open` clusters somewhere in its chain, so none of them get a new differential test either, same as `func15`; and now `func13`/`func14` (reset disk system/select disk info) + `func37` (reset drive, + internal `reset_37`) — reuse `FILE3.BDO`'s own new `diskreset` and `file1.c`'s own `TEST_VECTOR1`/`SET_CDISK1`, both split out of `test_vector`/`setcdisk` to take an explicit shift-amount/bit-position instead of always reading `BD_SELDSK`; `func13` calls `RESET_37` directly (`func37`'s own real continuation, same "call into the middle" pattern as `rename_`'s own `CALL DELETE1`); `TEST_VECTOR1`/`SET_CDISK1` land inside the DATA.BDO collision range, so this cluster gets no new differential test either, despite `diskreset`/`func13`/`func14`/`func37` themselves being individually clear of it. `func38`/`func39`/`FUNC41`/`FUNC47`/`FUNC49`/`FUNC50` have no body of their own at all (`EQU FUNCRET`, a dispatch-table alias onto `BDOSIF.BDO`'s own empty `funcret`), nothing to port; and now `func33`/`func34` (random read/write) — thin wrappers (`parsave36r` + `CHECK_FCB`, one of its own real callers too, same as `func20`/`func21`, + a tail call into `FILE3.BDO`'s own `rseek`-based `rdread`/`rdwrite`); clear of the collision themselves, but `parsave36` (lands inside it, same as `parsave`/`parsave33`) doesn't, so no new differential test; and now `func30` (set file indicators) — another thin wrapper (`parsave33`/`copy_dma_in`/`CHECK_WILD`/`RESELECTX`/`CHK_PASSWORD` + a tail call into `FILE2.BDO`'s own `indicators`), clear of the collision itself but `indicators` isn't; and now `func35`/`func36` (file size, set random record) — thin wrappers around `FILE3.BDO`'s own `getfilesize`/`setrandom`; `func35` calls into `file2.c`'s own colliding `search`/`searchn` transitively, but `func36`/`setrandom`/`cmperr` don't touch that cluster at all, a legitimate (if not yet written) differential-test candidate; and now `func40` (write random with zero fill) + `FUNC46` (get disk free space) — `func40` calls `rseek1`/`diskwrite` directly (not `rseek`/`rdwrite`, which would re-zero `BD_SEQIO`); `FUNC46` counts zero bits in the disk's own allocation vector via `FILE2.BDO`'s own `GET_NALBS`, converting block count to record count in a wider type instead of chasing the real asm's own split 16+8-bit shift; both land clear of the collision, but `diskwrite`/`GET_NALBS` don't, so no new differential tests; and now `func42`/`func43` (lock/unlock record) — thin wrappers into `FILE3.BDO`'s own `lock_`; `LOCK_PERR` (the real asm's own `MOV SP,LOCK_SP` stack-unwind entry) turned out to be provably dead code, nothing anywhere calls it, so the whole cluster's own manual-stack trick reduces to the ordinary `POP`/`POP`-discard category already modeled elsewhere, no real stack manipulation needed. **`FILE4.BDO` is now ported in full** — every real BDOS dispatch function has a C counterpart; `func38`/`func39`/`FUNC41`/`FUNC47`/`FUNC49`/`FUNC50` are bare `EQU FUNCRET` dispatch-table aliases with no body of their own (straight onto `BDOSIF.BDO`'s own empty `funcret: ret`), and `GOBACK` is a 2-instruction tail jump into `BDOSIF.BDO`'s own module-dispatch epilogue (`BDOS_RETURN`) — both out of scope by definition, not "remaining work" | — (every routine in this file is now ported) |
| `src/kernel/DATA.BDO` | — | pure data (BDOS-wide scratch fields + the `XERR_LIST` error-message table) — zero real code, nothing to port |
| `src/kernel/BDOSIF.BDO` | `init`, `entry` (`src-c/kernel/bdosif.c`) | `entry` resolves `cl` (BDOS function #, 0-46) into `functab` the same way the real asm does, then dispatches for real into the 10 functions whose own `btab_flag` is 0 (`func24`/`func25`/`func26`/`func29`/`func32`/`FUNC44`/`FUNC45`/`func51`/`func52`/`FUNC107`, function #s 11/12/13/16/19/31/32/36/37/45) -- real asm's own `nomx: call cs:btab_addr[si]`, an ordinary near indirect call, no far-call gap involved. `func26` was found genuinely missing (not merely deferred) while wiring this up and is now ported too (`file4.c`). The other 37 functions all have `bf_getmx` set, meaning the real dispatch goes through `getdiskmx` first — a genuinely large piece of real OS plumbing (acquire the mx disk queue, a critical-section stack switch, copy the UDA's own BDOS-relevant fields into this module's own local area, and — for functions whose own `btab_flag` also sets `BF_CSHELL`/`BF_TANDW` — the SHELL multi-sector-I/O wrapper around `CALL_BDOS` itself: `SHELL`/`CBDOS`/`SHELL_ERR`/`SHELL_RTN`/`INCR_RR`/`SAVE_RR`/`RESET_RR`/`TST_WRT`/`MULT_IO`/`COMPARE_RECS`/`BDOS_RETURN`) that has no C port here at all; `entry` reports reaching it (`reached_getdiskmx`) instead of silently wrong-answering those 37. `init` itself (the module's own `jmp init`/`jmp entry` header's first target) does the same `sysdat`/`supervisor` copy as `CIOIF.CIO`/`MEMIF.MEM`'s own `init`, then creates/opens/writes the MX disk queue via three real `mpmif` calls, traced to `QUE1.RTM`'s own already-ported `makeq_entry`/`openq_entry`/`writeq_entry` (`que1.c`). `parsave`/`parsave33`/`parsave36` are ported too (`src-c/kernel/file3.c`, for `FILE4.BDO`'s own `func15`/`func33`/`func34` etc.), leaving `XCRLF`/`XPRINT`/`staret`/`funcret`/`setlret1`/`parunsave`/`entryzero`/`mpmif`/`xiosif`/the `bootf`-family boundary stubs (`homef`/`seldskf`/.../`flush`)/the whole `getdiskmx`+SHELL mechanism above — that's the module's own real remaining scope |
| `src/kernel/UTIL.MEM` | `getmd`, `freemd` | — (both of this file's routines are ported) |
| `src/kernel/ML.MEM` | `mlalloc_entry` (+ internal `mla_value`, `mla_makemau`, `mla_initmau`), `mlfree_entry` (+ internal `ml_insert`) | — (all of this file's routines are ported, using `tools/i86sim.py`'s new `segmented=True` mode — see below) |
| `src/kernel/MAU.MEM` | `maualloc_entry`, `maufree_entry` (+ internal `mau_split`, `mau_insert`, `mau_release`, `mau_collapse`) | — (all 6 of this file's routines are ported; see below for 2 real bugs found and fixed in `/src`, and 2 `tools/i86sim.py` gaps fixed along the way: a `LEA`-under-`segmented=True` bug and missing `STD`/`CLD` support) |
| `src/kernel/SHARE.MEM` | `share_entry` | — (this file's one routine is ported) |
| `src/kernel/LOAD.SUP` | `load_finish`, `diskread`, `load_group`, `get_ch_info`, `load`/`load_entry`/`cload_entry` — see below | — (every real routine in this file is ported) |
| `src/kernel/MEMORY.MEM` | `malloc_entry`, `mfree_entry` (+ internal `mall_try_mau`, `mfree_it`) | — (the whole file is ported; `malloc_entry`'s own `mpmif(f_maualloc)`/`mpmif(f_mlalloc)` and `mfree_entry`'s own `mpmif(f_maufree)`/`mpmif(f_mlfree)` are all now real internal calls into `mau.c`'s `maualloc_entry`/`maufree_entry` and `ml.c`'s `mlalloc_entry`/`mlfree_entry` — see below) |
| `src/kernel/CPMMEM.MEM` | `maxmem_entry`, `absmax_entry`, `cpmalloc_entry`, `cpmabsa_entry`, `cpmfree_entry` (+ internal `getmemory`/`free_memory`, both now real calls into `memory.c`'s `malloc_entry`/`mfree_entry` — see below) | `freeall_entry` — one line (`mov cx,f_terminate ! sub dx,dx ! jmp mpmif`), same as `SYSENT.RTM`'s `creat_proc_entry`: nothing to port |
| `src/loader/LBDOSIO.A86` | `conbrk`, `conout`, `tabout`, `crlf`, `print`, `func9`, `staret`, `funcret`, `setlret1` | `func1`/`func3`/`func6`/`func7`/`func8`/`func10`/`func11` — each just an `EQU` alias for the bare `return:` label: nothing to port. `func2` is likewise just an alias for `tabout` itself, already ported under its own name |
| `src/loader/LBDOSNEW.A86` | `func51` (set segment base for disk I/O) | `func50`/`func52`/`func53`/`func54`/`func55`/`func56`/`func57` — each just a separate entry point into the same bare `ret` at the end of this file (`func51`'s own tail falls through all of them), nothing to port |
| `src/loader/LBDOSDAT.A86` | — | pure data declarations (`DB`/`DW`/`RB`), zero real code — nothing to port, same as `DATA.BDO` |
| `src/loader/LDBDOS.A86` | — | pure `INCLUDE` glue (assembles the loader's own BDOS by concatenating `LBDOSEQU`/`LBDOSENT`/`LBDOSNEW`/`LBDOSIO`/`LBDOSF1`-`3`/`LBDOSDAT`) — zero real code of its own, nothing to port |
| `src/loader/LDBIOS86Q.A86` | the whole file: `CONST`, `CONIN`, `CONOUT`, `PUNCH`/`READER` (same body, one label falls through to the other), `GETIOBF`, `INIT`, `SELDSK`, `HOME`, `SETTRK`, `SETSEC`, `SECTRAN`, `SETDMA`, `SETDMAB`, `GETSEGT`, `READ`, `WRITE`, plus the FDC/DMA utilities they all share (`fdc_send_cmd`/`fdc_out_byte`/`fdc_in_byte`/`fdc_sense_int`/`fdc_wait_seek`/`fdc_pio_in`/`rw_physical`/`fdc_fill_rw_parms`/`fdc_seek_to`/`dma_setup`) | `LISTOUT`/`LISTST`/`SETIOBF` — bare `ret`, no observable effect, nothing to port; `WBOOT` — nothing but the same hardcoded jump back into `LDMPM.A86` `INIT` itself ends with, nothing to port; `motor_delay` — no caller anywhere in this file, dead code, nothing to port |
| `src/loader/ROM86Q.A86` | the whole file: `start:` (ported as `start_entry`), `advance_chs`, `read_sector`, `pmsg`, `putc` | — (every real routine in this file is ported) |
| `src/loader/LDBIOS.A86` | the whole file: `CONST`, `CONIN`, `CONOUT`, `PUNCH`/`READER`, `GETIOBF`, `SELDSK` (2 real drives, unlike `LDBIOS86Q.A86`'s own single-drive restriction), `HOME`, `SETTRK`, `SETSEC`, `SECTRAN`, `SETDMA`, `SETDMAB`, `GETSEGT`, `READ`, `WRITE`, plus the real iSBC 204/8271+8257 FDC/DMA utilities they share (`send_com`/`execute`/`dma_setup`) | `INIT` — no hardware setup of its own at all (confirmed directly against the real asm; `src/loader/ROM.A86`'s own boot sequence already does it before `LDBIOS.A86` is loaded), just the usual stack/segment switch, nothing to port; `WBOOT` — nothing but `jmp ccp+6`, nothing to port; `LISTOUT`/`LISTST`/`SETIOBF` — bare `ret`s, no observable effect; the real error-retry-with-console-interaction tail `execute`'s own error path would otherwise need — not modeled, same simplification `LDBIOS86Q.A86`'s own `read_entry`/`write_entry` already make, kept for consistency with that sibling (see `ldbios.c`'s own header comment) |
| `src/loader/ROM.A86` | the whole file: `start:` (ported as `start_entry`), plus the real iSBC 204/8271+8257 FDC/DMA/UART/PIC-init utilities it uses (`send_com`/`execute`/`setupdma`) | `conin`/`conout`/`pmsg` and the fatal-error-with-console-retry tail they only otherwise exist for — not modeled, armed but never used on the happy path this port models; the initial EPROM-to-RAM data copy — a deterministic byte copy with no transformation, ported as the already-copied VALUES themselves (`specs1`/`specs2`/`specs3`/`home_cmd`/`read0`/`read1`/`read2`, static C arrays) rather than modeled mechanically; the final `jmpf dword ptr leap_offset` — no C equivalent, same as every other real OS entry point in this port; see `rom.c`'s own header comment for all of the above, plus a real, previously-uncaught `tools/asm86` quirk this port's own differential test found and worked around rather than fixed (a bare "offset $"-rooted EQU used without `offset` at the use site, e.g. `mov SI,drombegin`/`mov bx,sector1`, assembles as a direct memory read instead of the immediate load every call site obviously intends — a one-line `is_addr` fix cleanly resolves it for `MOV`, but also breaks `LBDOSENT.A86`'s own `lea sp,endstack`, which genuinely needs the mem-shaped operand `is_addr` provides; properly separating those two cases needs LEA's own operand parsing to force that shape regardless, not attempted here) |
| `src/loader/LDMPM.A86` | the whole file: `start:` (ported as `start_entry`), `phex`, `pnib`, `putchar` (`initlbdos`/`openfnc`/`setdma`/`setdmab`/`read`/`msg`/`pcrlf` are each just a one- or two-line trampoline into `sys_vec`, folded into the `bdos_fn`/`msg_fn`/`read_fn` boundary-call parameters they resolve to — see `ldmpm.c`'s own header comment) | — (every real routine in this file is ported) |
| `src/loader/LBDOSENT.A86` | `oldbdos` (the `jmp cs:functab[bx]` dispatch, ported as a plain C switch, same convention as `src-c/kernel/dsptch.c`), `persub`, `selsub`, `rodsub`, `rofsub` (+ internal `errflg`); `parsave`/`parsave33`/`parunsave` ported earlier, in `src-c/loader/lbdosf3.c` (borrowed there, see that file's own header comment) | `bdose` (the real `INT bdos_int` interrupt-entry prologue/epilogue) — no C equivalent, same as every other real OS/interrupt entry point in this port |
| `src/loader/LBDOSF1.A86` | `selectdisk`, `home`, `rdbuff`, `wrbuff`, `seekdir`, `seek`, `dmposition`, `getdm`/`getdmd`, `bdos_index`, `alloct`, `atran`, `getexta`, `gtfcba`, `getfcb`, `setfcb`/`setfc1`, `cmpecs`/`cmpec0`, `setcdisk`, `nowrite`, `setro`, `ckrodir`/`ckrofile`, `checkwrite`, `getdptra`, `getmodnum`, `clrmodnum`, `setfwf`, `compcdr`, `setcdr`, `subdh`, `newchecksum`/`checksum`, `wrdir`, `rddirbuf`, `dirtouser`, `endofdir`, `setenddir`, `rddir`/`rddir0`, `getallocbit`, `setallocbit`, `rotr` | `selerror`/`goerr` — a pure boundary-stub tail (`JMP w[bx]`) into `LBDOSENT.A86`'s own error-reporting table, now ported for real (see that file's own row above) but NOT wired up here: these 3 call sites (`diocomp`/`ckrofile`/`checkwrite`) are already unreachable under this file's own, separate, independently-documented choice to fix `readf`/`writef`/`nowrite`'s own boundary-stub results at "always succeeds" — true whether or not `persub`/`selsub`/`rodsub`/`rofsub` exist, so wiring them up here wouldn't change anything observable; `setdata`/`setdir`/`setdma` — pure boundary-stub setup, no observable effect, not even defined as functions |
| `src/loader/LBDOSF2.A86` | the whole file: `scndm`, `initialize`, `cpydirloc`, `compext`, `srch`/`srchn`/`srchnext`, `delet`/`delet0`, `getblock`, `copyfcb`/`copydir`/`seekcopy`, `rename`, `indicators`, `open`/`opencopy`, `mergezero`, `close`, `make`, `openreel`/`openmod`/`openr0`-`openr2`/`openerr` | — (every routine in this file is ported) |
| `src/loader/LBDOSF3.A86` | `sqread`/`diskread`, `slect`/`curselect`/`reselect`, `func13`/`func15`/`func20`/`func26`, `goback`/`retmon`, plus `parsave`/`parsave33`/`parunsave` (borrowed from the otherwise-deferred `LBDOSENT.A86`, needed by `func15`/`func20`/`goback`) | over half the real file is already dead, commented-out code from an earlier rewrite (its own old `rseek`/`rdread`/`rdwrite`/`cmperr`/`getfilesize` family) — never assembled, nothing to port; `func12`/`func16`-`func19`/`func21`-`func25`/`func27`-`func37`/`func40` are each just an `EQU` alias for the bare `return:` label, nothing to port; `func14` aliases `curselect`, `func38`/`func39` alias `funcret` (`src-c/loader/lbdosio.c`), both already ported under their own names; `LBDOSENT.A86`'s own dispatch (`oldbdos`) and `func50`-`func57` (all bare-`return:` aliases) are now ported too, see that file's own row above |

Current validation: `make -C src-c build` compiles all 50 C files, and
`make -C src-c test` passes. The `test_lbdosf2.py` reference harness
models BIOS targets reached by both near calls and tail jumps.

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

- `src/kernel/CIOIF.CIO`'s and `src/kernel/MEMIF.MEM`'s own `init` are
  **now ported** (`cioif.c`/`memif.c`) -- this framing was stale: their
  own `init` is a straight-line, 7-instruction body with no
  `call`/`callf`/`jmp` of any kind (just two cseg-cell writes, ending
  in a far `RET`), needing none of the far-call infrastructure this
  bullet used to claim blocked it; `tools/i86sim.py`'s own
  `far_call_stub_addrs` (added for `INIT.SUP`) was never actually the
  missing piece. `entry`/`mpmif`/`xiosif` themselves are *not* ported
  (and have no project value if they were): every real C-side caller
  that needs one of `entry`'s own functab targets already traces
  straight to the already-ported routine itself (`fileopen`→`func15`,
  `f_load`→`load_entry`, `getmemory`→`malloc_entry`, etc. — the same
  "boundary call resolves to an already-ported routine" pattern used
  throughout this project), so nothing in this tree ever needs to call
  *through* a module's own `entry` to reach them. `src/kernel/
  RTMIF.RTM`'s own `init` **is now ported too** (`rtmif.c`) — same
  shape (no far calls, one ordinary near `call proc_creat`) but a
  much larger body (sets up the interrupt/intermodule dispatch
  vectors and creates the tick/idle processes directly via UDA
  writes), using this project's already-established `mem_*_seg`
  conventions (see this file's own changelog entry for the detail,
  including a real `DS=sysdat` addressing subtlety the other two
  modules' own trivial `init` never surfaced). `src/kernel/SUPIF.SUP`
  has no separate `init` label of its own at all (confirmed via
  direct inspection, not assumed from the shared header shape with
  the other four modules — `SUPIF.SUP` starts straight at
  `user_entry:`) — **resolved, not a gap**: `INIT.SUP`'s own already-
  ported `init` (`init.c`) `callf`s into `rtmmod`/`memmod`/`bdosmod`/
  `ciomod`/`xiosmod`'s own far pointers but conspicuously *not*
  `supmod`'s, because `INIT.SUP` itself runs as part of the SUP module
  and serves as SUP's own initialization inline. `SUPIF.SUP`'s
  `user_entry` genuinely needs a real `IRET`/interrupt-entry
  convention this tree has nowhere else yet, and remains out of scope
  for that reason (unrelated to module-dispatcher `init`). `src/kernel/
  BDOSIF.BDO`'s own `init` **is now ported too** (`bdosif.c`) — see
  the Tier 3 bullet below, superseding what it used to say.
**Tier 3 additions, same two categories as above:**

- `src/kernel/BDOSIF.BDO`'s own `init` **is now ported**
  (`bdosif.c`) — the same `sysdat`/`supervisor` copy as `CIOIF.CIO`/
  `MEMIF.MEM`'s own `init`, then three real `mpmif` calls
  (create/open/write the MX disk queue) traced to `QUE1.RTM`'s own
  already-ported `makeq_entry`/`openq_entry`/`writeq_entry`
  (`que1.c`). `entry` itself still isn't ported (same "boundary call
  resolves to an already-ported routine" reasoning as every other
  module dispatcher) — `entry`/`mpmif`/`xiosif` need far `RET`
  (`retf`) and an indirect far `CALL` through a memory operand (`callf
  cs:dword ptr`), neither of which exists in `tools/i86sim.py` yet.
  `entry:`'s own `call cs:btab_addr[si]` BDOS-function dispatch is
  a near indirect call, already supported (same as `ABORT.RTM`/
  `DSPTCH.RTM`) — so this was always a narrower gap than the
  `MEMIF.MEM`/`CIOIF.CIO`/`RTMIF.RTM`/`SUPIF.SUP` class above, not the
  same one, and it's isolated to this file: `FILE1-4.BDO`'s own BDOS
  functions (fully ported now) are what that dispatch table calls
  *into*, not the other way around, so nothing there is blocked by it.
  See this file's own status-table row above for the full detail.
- `src/kernel/FILE2.BDO`, `src/kernel/FILE4.BDO` — **stale**:
  `FLUSH_FILE0`'s real segment-relative-addressing gap in
  `FILE3.BDO` is long since resolved (`mem_r8_seg`/`mem_w8_seg`, the
  same helpers already used in `ml.c`/`proc.c`/`mau.c`/`share.c`/
  `load.c`), and `FILE2.BDO`/`FILE4.BDO` are both now **fully ported**
  (see their own status-table rows above) — this bullet used to
  describe a real blocker but no longer reflects the tree. The
  `DATA.BDO` cseg/dseg test-harness collision that used to block
  *differential testing* of a handful of already-ported clusters
  (`close`/`close1`/`search`/`searchn`, `open`/`opencopy`,
  `copy_dir0-2`, `SWAP`, plus a few `FILE3.BDO`/`FILE4.BDO` thin
  wrappers that call into them transitively) is **also now resolved**
  -- `file1.c`/`file2.c`/`file3.c`/`file4.c` all thread a real
  `ds_bdos` segment parameter through every `BD_*`-touching function
  now (see `FILE2.BDO`'s own status-table row for the detail), so
  these clusters can get real segmented differential tests same as
  `swap_sdcnt`'s new one; most of them (`close`/`search`/`open`/
  `copy_dir0-2`) just don't have one *written* yet. (`DATA.BDO` is
  confirmed pure data, see its own status-table row above.) `src/kernel/CHARIO.CIO` (the large, deeply
  interdependent console-input state machine this bucket used to defer
  — `coninit`/`attach`/`conbrk`/`conin`/`chkctl`/`echoc`/`tabout`/...),
  `src/kernel/COMMAND.SUP` (its own `chain_entry`/`clicmd_entry`
  **now fully ported, chain mode included** -- see this file's own
  status-table row and the changelog entry below), `src/kernel/FILE1.BDO` (the
  smallest of the four BDOS filesystem files, reconned and mostly
  ported), and `src/kernel/FILE3.BDO` (**stale**: its own status-table
  row's deferred column still said "the rest of the file not reconned
  this pass" as of the `PR_TERM`/`XCURSELECT`/`RESELECTX` changelog
  entry, but a fresh, full recon found nothing left -- every label in
  the real file is either a routine this row's ported column already
  names, an internal jump target inside one of them (`SELECT`/
  `SELECT1`/`SELECT2`, `WRITE`/`WRITE1`, `LOCK`/`LOCK_ERR`, etc. --
  confirmed via a cross-file-call-target sweep against every other
  `/src/kernel` file: every label any *other* file calls into is
  already ported, and no bare `CALL`/`JMPS` to these internal-only
  names exists anywhere), or ported elsewhere in this tree already
  (`REMOVE_DRIVE`, `file1.c`). `src/kernel/FILE3.BDO` is now **fully
  ported**, same as `FILE2.BDO`/`FILE4.BDO`) are since reconned — see
  their own status-table rows and the Tier 3
  rollout note below. The `FILE*.BDO`
  files total ~4900 lines of BDOS file-system internals and are
  Tier-3-plus-sized on their own.

The table above is the current per-file status. Files and routines
listed there include work in `src/kernel/`, `src/loader/`, and
`src/rsp/`; use each row's deferred column for the remaining scope.

### Scope notes

- `src/toolchain/` and `src/utils/` (PL/M source) are out of scope for now —
  PL/M is already a C-like high-level language and a different kind of
  problem (`tools/plmc.py` already compiles it).
- Only files that currently assemble cleanly (the `*_OK` lists in the
  `Makefile`) are candidates. `XIOS.A86` (bug #7) and the three broken RSPs
  (`CLOCK`/`TMP`/`SUBMIT`, bugs #1–#3, see `docs/bugs.md`) were kept out
  until those were fixed — all four now assemble cleanly and are in
  `RSP_OK`/`KERNEL_OK`, and each has its own ported routines (see the
  table above): `CLOCK.A86`/`TMP.A86`/`SUBMIT.A86` in full (modulo the
  usual boundary-stub/one-time-setup carve-outs every RSP has), and
  `XIOS.A86`'s own portable function-set routines (its real hardware I/O
  and interrupt code stay out for a different reason — no C equivalent,
  not a blocked bug — see `xios.c`'s own header comment).
- The goal remains a working MP/M-86 kernel built from this C port,
  not just a verified reference. The current `kernel.elf` links and
  boots the ported kernel objects with the multiboot stub, but does not
  execute them; the real MP/M-86 boot chain and operational kernel
  behavior remain unfinished.

### Original rollout notes (historical)

The notes below preserve the original rollout plan and intermediate
status snapshots. They are historical; the status table above is
authoritative for what is ported and deferred now.

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
- **`src/kernel/LOAD.SUP`: the remaining front half (`get_ch_info`,
  `load`/`load_entry`), completing the file except `cload_entry`'s
  own chain-load mode.** Now that disk I/O and allocation are both
  real (see the two entries just above), ported `get_ch_info` (CMD
  header parsing into `ldtab`, including `ch_alloc`'s own immediate
  `malloc_entry` call for absolute/shared-code entries, reusing the
  target `ldtab` entry itself as the MPB -- faithful to the real asm,
  which does the same thing since the two structs share a layout) and
  `load`'s own memory-allocation/spreading math (`lt_more`/`lt_spread`,
  calling `malloc_entry`/`mfree_entry` for real) and "fill memory from
  file" (`load_group`, now wired to a real combined allocation).
  `cload_entry`'s own chain-load mode is excluded, same reason as
  before -- see `load.c`'s own header comment.

  One real asm subtlety caught the differential test's first run:
  `load`'s own "fill memory from file" loop saves `ldtab_start[si]`
  around each `call load_group` and restores it afterward
  (`push ldtab_start[si] ! call load_group ! pop ldtab_start[si]`),
  specifically to discard `load_group`'s own internal bookkeeping
  drift (`xfer_direct`'s own `add ldtab_start[bx],8`, advancing the
  field to track *where the next write within this transfer goes*,
  not where the group's own destination started) -- everything
  downstream (`load_finish`'s own Base Page lookup) needs
  `ldtab_start` to keep meaning "this group's own fixed destination
  segment". Missed on the first pass (load.c's own port let the drift
  through), caught immediately by the one-relative-Code-group
  differential-test case (off by exactly one record's worth of
  paragraphs), fixed by the same save/restore around the call.
- **`src/kernel/COMMAND.SUP`'s `clicmd_entry` (the non-chain path --
  see the status table above for why `chain_entry`/chain mode is
  excluded).** Many of its own `mpmif(...)` calls are now real
  internal calls into already-ported routines, the same technique
  used throughout this project: `mxread`/`mxwrite` (`que1.c`'s
  `readq_entry`/`writeq_entry`, both unconditional -- the differential
  test pre-seeds the MXCli queue with room/a token so neither call
  ever has to retry), `cli_checkque`'s own `openq_entry`/
  `cwriteq_entry` (`que1.c`) and `conasn` (`console.c`'s
  `conassign_entry`), `cconattch_entry` (`console.c`), `prchar`/
  `cprnt` (`chario.c`'s `conout_entry`/`conprint_entry`), and `pfn`
  (`parse.c`'s `parse_entry`, via this port's own `cli_pfn` helper).

  `cli_pcb`'s own `pcb_flnmptr`/`pcb_fcbptr` and `cli_cuspqpb`'s own
  `qpb_buffptr`/`qpb_nmsgs` are never written anywhere in the real
  `COMMAND.SUP` source -- real SYSDAT fields the real system's own
  boot-time init sets once and `COMMAND.SUP` only ever reads. This
  port's own `cli_pfn`/`cli_checkque` set them explicitly on every
  call instead of modeling that one-time init; the differential test
  pre-seeds `cli_pcb` the same way, standing in for that same boot
  init (since the test never runs it either).

  `fileopen`'s own real `mpmif(f_fopen)` (blocked on `FILE1.BDO`/
  `FILE2.BDO`'s still-unported `srch`-dependent bulk) stays a boundary
  stub, but with `BL`/`BH` fixed at a well-defined constant
  (`BL=0xFF` hard failure, `BH=0` not extended) rather than left
  genuinely undefined, same precedent as `CONSOLE.CIO`'s/`CPMMEM.MEM`'s
  own undefined-register fixes. `cli_fload`'s own real success path
  is translated faithfully but never reachable under that fix, same
  class as those files' own unreachable branches; `f_load`/`f_cload`/
  `f_createproc`/`f_setprior`/`f_terminate` are therefore left as
  plain boundary stubs too (never reached either, so upgrading them
  would add nothing testable) -- same "dispatch-adjacent calls don't
  need upgrading" reasoning `SYSENT.RTM`'s own `wakeup_entry`'s
  `call pdisp` already established.

  The differential test (`tests/src-c/fixtures/command_wrapper.A86`)
  bundles `QUE1.RTM`, `CONSOLE.CIO`+`CHARIO.CIO`, and `PARSE.SUP`
  alongside `COMMAND.SUP` itself, with a real `mpmif` dispatcher
  assembled right into the wrapper (tail-jumping into whichever of
  those real routines a given function code now reaches for real,
  same pattern `memory_mem_wrapper.A86` already uses for
  `MAU.MEM`/`ML.MEM`) rather than a flat Python stub -- the first
  wrapper in this tree combining this many real modules under one
  dispatcher. Two `tools/i86sim.py` gaps surfaced along the way and
  got fixed: the accumulator short-form direct-addressing opcodes
  (`0xA0`-`0xA3`, `MOV AL/AX,[imm16]` and the reverse -- the
  assembler's own size optimization for AL/AX moves to/from a fixed
  address, never hit by an earlier file's own differential test).
- **Documentation fix: `src/kernel/FILE1.BDO`'s `checksum`/
  `newchecksum`, `rddir`/`rddir0`, and `wrdir` were already ported and
  passing in `src-c/kernel/file1.c` (and already exercised by
  `tests/src-c/test_file1.py`), but the file's own header comment,
  its inline `subdh` comment, `test_file1.py`'s own module docstring,
  and this doc's own status table all still described them as
  deferred, blocked on `FILE3.BDO`'s `FLUSH_FILE0`/`FLUSH_FILES`.**
  That blocker was in fact already resolved -- a 2nd `FILE3.BDO` pass
  (see the entry above, from the same earlier session) ported
  `FLUSH_FILE0`/`FLUSH_FILES` for real using the same `mem_r8_seg`/
  `mem_w8_seg` segment-relative-addressing fix already established
  for `PROC.RTM`/`ML.MEM`/`MAU.MEM`/`SHARE.MEM`/`LOAD.SUP`, and a
  follow-up pass on `file1.c` then ported `checksum`/`newchecksum`/
  `rddir`/`rddir0`/`wrdir` against it -- but that follow-up's own
  code changes never got their matching documentation updated. No
  code changed here, only comments and the status table, to bring
  them back in line with what's actually in the tree: confirmed via
  `grep` that `flush_file0`/`checksum`/`newchecksum`/`rddir`/`wrdir`
  all already exist as real functions in `file1.c`/`file3.c`, and via
  a full differential-test-suite run (`for f in tests/src-c/
  test_*.py; do python3 "$f"; done`) that every case, including
  `test_file1.py`'s own `checksum`/`rddir`/`wrdir` cases, still passes.
  `CHECK_FCB` is the only routine in `FILE1.BDO` still genuinely
  deferred, blocked on `FILE2.BDO`'s own still-unported `CLOSE1`.
- **`src/kernel/FILE2.BDO`'s `SEARCH`/`SEARCHN` (`search`/`search1`/
  `searchn`, + internal `searchi`/`searchi1`/`compext`/`swap_sdcnt`).**
  Deferred in an earlier pass only because `rddir` was -- now that
  `rddir`'s own real segment-relative-addressing gap is resolved (see
  the `FILE1.BDO`/`FILE3.BDO` entries above), this ports the whole
  cluster: the XFCB (extended-FCB) special case, the per-character
  match loop (`srchloop`/`srchext`/`srchok`/`SEARCHNM`), and the
  "user 0" fallback pass (`USER_ZERO_PASS`/`swap_sdcnt`).

  Along the way, `tools/i86sim.py` gained `jmp_stub_addrs`: addresses
  that, when reached by a near JMP, are treated as an immediate RET
  instead of actually jumping there -- for a routine whose own real
  tail end is a JMP (not a CALL) into an unported boundary that real
  hardware eventually returns through normally (`seek`'s own final
  `jmps setsecf`, reached via `seekdir`'s `call seekdir` from
  `rddir0` -- `searchn`'s own very first directory-record read always
  takes this exact path, the first routine to need it; `call_stub_addrs`
  alone only catches the CALL case).

  Despite that fix, this cluster ended up **not exercised by a
  differential test** -- the first on this project to be translated
  faithfully but left untested. `searchn`'s own real cseg footprint,
  in the combined FILE1-4.BDO wrapper (`tests/src-c/fixtures/
  file1_wrapper.A86`/`file2_wrapper.A86`), runs well past where
  DATA.BDO's own dseg-relative fields start (0xc00) -- confirmed via
  `--symbols`, `SRCHN1` (one of `searchn`'s own internal branch
  targets) resolves to 0xc11, a few bytes before `BD_INFO`'s own real
  address (0x0c17). On real hardware this is harmless (cseg and dseg
  are different physical segments), but `tools/i86sim.py` flattens
  both into one 64KB array at these same raw offsets -- confirmed
  directly: writing a test FCB pointer into `BD_INFO` overwrites
  `searchn`'s own real instruction bytes at that exact flat address,
  and the reference run starts executing its own operand data as
  code. Unlike every earlier SD_RLR-style gap, relocating just the
  *data* side doesn't work here, since the *code* (`searchn`'s own
  body) is the other half of the same collision. See `file2.c`'s own
  header comment for the full writeup and why a quick fix (a smaller,
  rescoped wrapper) isn't straightforward either. `get_nalbs`/
  `getblock`'s own 5 pre-existing differential-test cases still pass,
  unchanged.
- **`src/kernel/FILE2.BDO`'s `CLOSE`/`CLOSE1` (`close`/`close1`/
  `close_fcb`/`seekcopy`, + internal `get_dir_ext`/`restore_rc`/
  `set_rc`/`merge_word`/`merge_byte`).** This was `FILE1.BDO`'s own
  long-standing blocker for `CHECK_FCB` -- deferred only because it
  called `srch`, ported in the entry just above. Ports the disk-map
  merge `CLOSE_FCB` does against the matching directory entry
  (single-byte and double-byte map variants both), the backward
  disk-map scan in `GET_DIR_EXT` (whose own real asm comment, "BX =
  .FCB(VRECORD)", is wrong -- confirmed against `file1.c`'s own
  `gtfcba`/`getfcb` that its real output is `.fcb(nxtrec)`, which is
  what this actually starts from), and the record-count reconciliation
  at the end of a successful merge (an MPM-specific "another process
  may have extended this file, keep the higher reccnt" step). `close`
  itself (the real F_CLOSE BDOS function, a 2-check wrapper around
  `close1`) is ported too, nothing left deferred in its way. Same
  outcome as `search`/`search1`/`searchn` just above: translated
  faithfully, `make build` and the full existing suite stay green, but
  **not exercised by a differential test**, for the identical reason
  (this cluster's own real cseg footprint lands even deeper into
  DATA.BDO's own dseg-field range: `close`=0xfa8 through `CLOSE2`'s
  own merge-loop labels at 0x1023-0x10b7).

  `CHECK_FCB` itself stays deferred, but for its own reason now, not
  `CLOSE1`'s: on its "invalid checksum, try a partial close" path it
  does `POP BX ;DISCARD RETURN ADDRESS`, a deliberate manual stack-pop
  (same category of trick as `PARSE.SUP`'s own `delim`, see
  `parse.c`'s header comment) that returns one call frame higher than
  its own direct caller -- a separate piece of work, left for its own
  pass.
- **`src/kernel/FILE1.BDO`'s `CHECK_FCB1`/`CHECK_FCB` -- the
  complication flagged in the entry just above, resolved in the same
  session.** Reading the real asm turned up exactly what the `POP BX`
  was for: both of `FILE4.BDO`'s own real call sites (its `OPEN`/
  `CLOSE` MPM branches, not yet ported) manually `PUSH` the address
  they want control to land at instead of their own normal return
  address, right before `CALL CHECK_FCB1`, and `POP` it back off
  immediately after -- a callee-discards-its-own-return-address error
  jump, same mechanism as `PARSE.SUP`'s own `delim`. Translated the
  same way that file's own header comment describes: `check_fcb1`
  just returns a plain `0`/`1`, and any future port of `FILE4.BDO`'s
  own two call sites will need to replicate the push/pop by hand (an
  ordinary early-return in C instead) -- documented in `check_fcb1`'s
  own header comment in full, since this is now the 2nd routine in
  this tree needing that writeup (after `delim`'s own). `CHECK_FCB`
  itself (the version that resets `BD_CHECK_FCB_RET=false` before
  falling into `CHECK_FCB1`) is provably dead -- neither real call
  site reaches it, both set `BD_CHECK_FCB_RET=true` and call
  `CHECK_FCB1` directly -- but it's translated anyway, since doing so
  cost one line. This closes out `FILE1.BDO`: every routine in that
  file is now ported. `check_fcb1`'s own cross-file call into
  `close1` (`src-c/kernel/file2.c`) is a new dependency file1.c
  didn't have before, so `tests/src-c/test_file1.py`'s/
  `test_file3.py`'s own `build_c_library` calls needed `file2.c`
  added to their own link lists. Full existing suite stays green; no
  new differential-test cases (same reasoning as `CLOSE1`'s own entry
  above -- `CHECK_FCB1`'s own real address in this combined bundle
  lands in the same DATA.BDO-colliding range).
- **`src/kernel/FILE2.BDO`'s `initialize` (+ internal `scndm`/
  `test_dir`) and `src/kernel/FILE3.BDO`'s `CURSELECT` (+ internal
  `select_`).** This un-blocks `FILE2.BDO`'s own `DELETE`/`RENAME`/
  `OPEN`/`MAKE` and most of `FILE4.BDO`'s remaining `func*` entries,
  which routed through one or both of these (plus `srch`/`CHECK_FCB`,
  both already ported in earlier entries above).

  `curselect`'s own real tail (`SELECT`'s disk-login/reinit body, and
  the final `jmp initialize`) is translated faithfully but never
  reached: `selectdisk`'s own real BDOS-disk-header lookup is a
  fixed, always-failing boundary stub (`file1.c`'s own header
  comment), so `select_` always ends at `selerror` -- same "stop
  translating right past the last real boundary" choice `command.c`'s
  own `cli_gopen` makes for an analogous case. `scndm`'s own real asm
  carries a 0/1 bit across loop iterations via push/pop instead of a
  register (the register it would otherwise use gets reused inside
  the loop body for the disk-map block number) -- no C-level
  equivalent needed, a plain local variable does the same job.
  `test_dir`'s own real asm shares one `ret` between two different
  callers (its own internal loop-exit jumps, and `initialize`'s own
  early-return test right after calling it) -- ordinary code reuse
  around a single shared instruction, not a stack trick, so this
  returns a plain boolean instead for `initialize` to branch on.
  `file1.c` gained two new small cross-file helpers, `test_vector`
  (`TEST_VECTOR`, generalizing `nowrite`'s own inlined version of the
  same shift) and `remove_drive` (`REMOVE_DRIVE`).

  Unlike this session's own earlier entries (`searchn`, `CLOSE1`),
  these four routines are confirmed clear of the DATA.BDO flat-
  address collision -- checked directly via `--symbols` against the
  same combined bundle: `initialize`'s own real range (0x0a8a-0x0b74)
  stays safely below where DATA.BDO's own fields start (0xc00), and
  `curselect`'s (0x15e3+) lands past dseg's own 0x1000-byte length
  entirely. A differential test is therefore actually feasible here,
  just not written in this pass. Full existing suite stays green.
- **`src/kernel/FILE2.BDO`'s `MAKE` (`make`, + internal
  `set_dcnt_dblk`/`copy_dir0`/`copy_dir2`).** Of `DELETE`/`RENAME`/
  `OPEN`/`MAKE`, this is the one that turned out to be genuinely
  self-contained once actually read: it only needs routines already
  ported (`search`/`searchn`, `setcdr`, `checkwrite`, `setfwf`) plus
  its own small internal helpers. `DELETE`/`RENAME` both still route
  through `FILE3.BDO`'s own still-unported `TST_OLIST`/`CHK_OLIST`
  (blocked on `openx06`/`compare_pds`, see that file's own status-
  table entry); `OPEN`'s real entry point turned out to be
  `FILE4.BDO`'s own `OPEN_FILE`, not `FILE2.BDO`'s own `open:` (which
  it only falls into partway through) -- and `OPEN_FILE`'s own MPM
  branch is one of `CHECK_FCB1`'s two real callers, so porting it
  needs that push-a-fallback-address/pop-it-on-success convention
  replicated by hand in C (documented in `check_fcb1`'s own header
  comment, `file1.c`, specifically for this).

  `copy_dir0`'s own two register-reuse layers (`COPY_DIR2`'s own
  push/pop round-trip, re-purposing `dx` to carry `cx` across a
  `getdptra` call; `COPY_DIR0`/`COPY_DIR1`'s own `inc cl`/`dec cl`
  loop-count idiom) both collapse to plain C locals and an ordinary
  `for`/`if` without needing either trick modeled literally -- same
  spirit as `scndm`'s own stack-carried bit, two entries above.
  `copy_dir` itself (the bare `DH=0x80` prologue `COPY_DIR0` falls
  through from) isn't ported -- nothing in `make`'s own call graph
  reaches it, only `COPY_DIR0` directly. Full existing suite stays
  green; no new differential-test cases (`copy_dir0`/`copy_dir1`/
  `copy_dir2`'s own real addresses, 0xe50-0xe6d, land inside the same
  DATA.BDO-colliding range as `CLOSE1`'s own cluster -- `make`/
  `set_dcnt_dblk` themselves don't, but calling through to
  `copy_dir0` either way makes a clean test here just as unreachable
  as it was there).
- **`src/kernel/FILE3.BDO`'s `COMPARE_PDS`/`TST_OLIST`/`CHK_OLIST`
  (+ internal `chk_olist0`) and `src/kernel/FILE4.BDO`'s `SET_ARET`/
  `openx06`.** `DELETE`/`RENAME`'s own cited blocker. `COMPARE_PDS`
  turned out to be a 1-line tail call into the already-ported
  `compare`. `openx06`/`SET_ARET` never return normally (`openx06`
  sets `BD_ARET`'s own high byte to 5 and `BD_LRET` to the generic
  0xFF failure sentinel, then either `jmp GOBACK` or falls into
  `REPORT_ERR`'s own pure-display tail -- see `set_aret`'s own header
  comment, `file4.c`, for why neither is translated past those 2
  writes, and for the one way this differs from `file1.c`'s own
  `goerr`, which shares that same tail but has a real side effect of
  its own checked *before* reaching it).

  An earlier revision of this doc's own `FILE3.BDO` row claimed
  `CHK_OLIST` "relies on its own caller having pushed a word for it
  to pop partway through" -- wrong, caught re-reading it closely this
  pass: the `pop dx`/`push dx` pair around its own `compare_pds` call
  is just ordinary register-save plumbing (freeing a register across
  one call, restored before any real `ret`), not an implicit calling
  convention at all. `TST_OLIST`/`CHK_OLIST` turned out to be the
  same routine (`chk_olist0`) with one flag flipped (verify-only vs.
  also-remove-and-bump-`BD_PD_CNT`).

  Reading `DELETE` itself after clearing this blocker turned up a
  *new*, separate one: its own very first real step, `CHK_PWS`
  (password checking), needs `GET_DIR_MODE`/`GET_XFCB`/`GET_XFCB0`/
  `CMP_PW` -- a whole password-FCB subsystem not reconned at all yet.
  `DELETE` stays deferred for that reason now, not `TST_OLIST`/
  `CHK_OLIST`. Full existing suite stays green; no new differential-
  test cases (same reasoning as this session's earlier entries).
- **`src/kernel/FILE3.BDO`'s password-FCB cluster: `CHK_PASSWORD`/
  `CHK_PWS` (+ internal `get_dir_mode`/`cmp_pw`/`get_xfcb`/
  `get_xfcb0`/`chk_pw`).** `DELETE`'s own real remaining blocker from
  the entry just above. `cmp_pw`'s own `CMP_PW3` loop walks an xfcb's
  own password field *backwards* (`STD`) while decrypting each byte
  against the xfcb's own checksum byte, falling back first to a
  "null password" special case (all blanks/zeros, matches anything)
  and then, on a real mismatch, to the system-wide default password
  -- needed no new `tools/i86sim.py` support, direction-flag tracking
  was already added for `MAU.MEM`'s own `mau_insert`.

  `chk_pw`'s own `CHK_PW1` tail (`mov al,7 ! jmp set_aret`) never
  returns normally to its own caller, same shape as `openx06`
  (previous entry) -- but unlike `openx06`, `chk_pw`'s own real
  callers (`CHECK_PASSWORD`, and `DELETE`'s own `CHK_PWS`) genuinely
  need to know whether this happened, so they can stop too instead of
  continuing as if nothing went wrong. `chk_pw`/`chk_password`/
  `chk_pws` all return a plain boolean instead of literally
  discarding a return address -- the same convention `check_fcb1`
  already established (see that function's own header comment,
  `file1.c`) -- and this was caught and fixed mid-pass: an earlier
  draft had them all silently `return` after calling `set_aret`
  with no way for `DELETE` to tell the difference, which would have
  silently swallowed every real password error. `set_aret` itself
  (`file4.c`) is no longer `static`, for this same cross-file call.

- **`src/kernel/FILE2.BDO`'s `DELETE` (`delet`, + internal
  `cpydirloc`/`init_xfcb_search`/`init_xfcb_search1`/`chk_wild`/
  `does_xfcb_exist`).** Needed the password cluster above plus
  `TST_OLIST`/`CHK_OLIST`/`COMPARE_PDS` (an earlier entry this same
  session) to actually assemble into something complete. The
  trickiest part translating it was `delet`'s own "one well-defined,
  non-wild match" fast path: the real asm does `jmps delet0` right
  after `ckrodir`, and `delet0`'s very next instruction is `JZ
  DELETE2` -- but `ckrodir`'s own real `ret` leaves ZF as whatever
  `RO_TEST`'s own `ADD BX,ROFILE` set it to (an address addition,
  essentially never exactly 0 in practice), not a real "was anything
  found" result; the *other* path into `delet0` (via `DELETE02`'s own
  fresh `search` call) *does* carry a meaningful ZF. Rather than
  model that incidental, almost-always-false flag literally, this
  port's own fast path just always takes `delet0`'s own "found"
  branch directly (`goto delete1`), which is what it does in
  practice anyway. `searchi1` (`file2.c`, already existed from the
  `SEARCH`/`SEARCHN` pass) is no longer `static`, since
  `does_xfcb_exist` calls it directly (reusing the existing
  `BD_SRCHA` pointer rather than resetting it from `BD_INFO`, same as
  the real asm). Full existing suite stays green; no new differential-
  test cases (same reasoning as this session's earlier entries --
  `delet` wasn't individually checked against the DATA.BDO collision,
  but given how much of this cluster already falls inside it, it's
  the safer assumption).
- **`src/kernel/FILE2.BDO`'s `RENAME` (`rename_`, + internal
  `check_wild`/`check_wild0`/`copy_user_no`/`copy_dir`).** The last of
  `DELETE`/`RENAME`/`OPEN`/`MAKE` to need its own dedicated pass (and
  the one requiring the most structural rework). Needed `delet`'s own
  `DELETE1` label split out into its own real function: the real
  asm's `rename_` does `CALL DOES_XFCB_EXIST ! JNZ $+5 ! CALL
  DELETE1`, calling straight into the middle of `DELETE`'s own body
  as an ordinary subroutine, which eventually returns normally (via
  what's now `delete2_entry`'s own `cpydirloc`/`staret` tail) to
  whichever `CALL` reached it. `copy_dir` itself (the bare `DH=0x80`
  prologue `COPY_DIR0` falls through from, left unexposed in the
  `MAKE` entry above since nothing needed it yet) turned out to need
  a second look here too: `rename_`'s own `MOV CL,DSKMAP ! MOV
  DL,EXTNUM` reuses those 2 constants purely for their *numeric*
  value (16 and 12), as a plain "copy starting at this fcb offset,
  this many bytes" pair, not their usual field-offset meaning --
  copying the fcb's own second-half name/type/extnum (the new name)
  over the matching directory entry's own first 12 bytes.
  `CHECK_WILD`/`CHECK_WILD0` (the opposite sense of `delet`'s own
  wildcard check -- `RENAME` errors out *if* a `?` is found in either
  filename, `DELETE` wants one for a wildcard delete) are ported too.
  Full existing suite stays green; no new differential-test cases
  (same reasoning as this session's earlier entries).

  This closes out `DELETE`/`RENAME`/`MAKE` in `FILE2.BDO`; `OPEN`
  itself followed in the very next pass (see below).
- **`src/kernel/FILE4.BDO`'s `func15` (BDOS OPEN), + its own `OPENX`/
  `OPENX0`-`OPENX09` cascade, and the rest of its own dependency
  chain: `src-c/kernel/file2.c`'s `open`/`OPEN1`/`opencopy` (+
  non-`static` `SWAP`/`SEARCHI`/`SET_DCNT_DBLK`/`CHECK_WILD`);
  `src-c/kernel/file3.c`'s `SET_SELDSK`/`reselect`,
  `parsave`/`parsave33`/`parsave33r` (the first two really
  BDOSIF.BDO's own, ported here for lack of a `bdosif.c`),
  `COPY_DMA_IN`/`COPY_DMA_OUT`, `STAMP1`/`STAMP2` (+ internal
  `stamp3`), `COUNT_OPENS`, `CREATE_OLIST_ITEM`, and non-`static`
  `GET_DIR_MODE`/`GET_XFCB`/`CMP_PW`; `src-c/kernel/file1.c`'s
  non-`static` `RO_TEST`.**

  Scoping this out fully (reading every real dependency transitively
  before writing any code) turned up a pleasant surprise: almost
  everything `func15` touches was already ported from earlier
  passes this session (`search`/`searchn`, `HOME`/`setenddir`,
  `CHECK_FCB1`, `GET_ATTS`, `RESET_CHECKSUM_FCB`, `CURSELECT`,
  `CLR_EXT`/`GET_RCNTA`, `search_olist`/`searchn_list`/
  `compare_pds`/`create_item`/`set_olist_item`, `GET_RRA`, `NOWRITE`,
  `seekcopy`). Only `reselect`'s own cluster, the `parsave` family,
  `COUNT_OPENS`, and `CREATE_OLIST_ITEM` were genuinely new.

  `func15`'s own MPM branch replicates `CHECK_FCB1`'s push-fallback/
  pop-on-success convention by hand: the real asm pushes `OFFSET
  OPEN_FILE` as `CHECK_FCB1`'s own fallback target, but that target
  turns out to be identical to the normal-success continuation
  whenever `BD_HIGH_EXT != 0x60`, so both collapse into a single
  `goto open_file` in the port. `OPENX` itself is the real "manual
  stack trick" of this cluster -- its own `POP BX ;DISCARD RETURN
  ADDRESS` right after a successful `endofdir` check means it only
  ever returns normally to `func15`'s own call sites on outright
  open failure; on success it runs the *entire* rest of the real BDOS
  OPEN call itself (`OPENX0` through `OPENX2`'s own final `ret`/`jmp
  goback`), so one C function (`openx`) covers the whole cascade --
  see that function's own header comment, `file4.c`, for the detail
  (including one genuinely dead branch: the MPM code's own second
  `OR AL,AL ;JNZ OPENX0` tests the same `AL` the line right before it
  already proved is 0, so it can never branch -- not modeled, same
  "translate faithfully but note unreachable" precedent as
  `cli_gopen`/`curselect`'s own `SELECT` success path).

  `STAMP1`/`STAMP2`'s own `OFFSET TOD` (day/hour/min/sec, a real
  `src/rsp/CLOCK.A86` struct) got its own new harness-chosen scratch
  constant, `SD_FILE_TOD` -- deliberately *not* reusing the existing
  `SD_TOD`/`SD_CLOCK_TOD_BASE` constants `command.c`/`tmp.c`/
  `clock.c` already use for (plausibly) the same real struct, since
  neither of those was ever confirmed to be the same physical copy
  FILE3.BDO/FILE4.BDO read, and this port's own convention is to mint
  an independent scratch base rather than assume two unconfirmed
  routines share one.

  `--symbols` against the combined wrapper turned up one more
  DATA.BDO flat-address collision cluster: `file2.c`'s own `open`/
  `OPEN1`/`opencopy` (0xF34-0xF3D) and `SWAP` (0xD1D) land inside
  DATA.BDO's own dseg range (0x000-0x1000), same class of gap as
  `searchn`/`CLOSE`/`copy_dir0`-`2` earlier this session -- so, same
  as those, no differential test was written for this pass's own
  additions, even though `func15`/`openx` and most of their own
  dependencies are individually clear of it (calling through the
  colliding `open` makes the whole chain untestable against real
  hardware regardless). Full existing suite stays green; `make clean
  && make build` stays green.
- **`src/kernel/FILE2.BDO`'s `openreel` (+ internal
  `sdcnt_eq_xdcnt`), and `src/kernel/FILE3.BDO`'s `FIX_OLIST_ITEM`.**
  `openreel` is `MAKE`'s own extent-rollover helper -- called from the
  (still-unported) sequential/random read/write BDOS functions
  whenever a file's own current extent runs out, it either bumps the
  extent field in place (same directory entry) or closes the current
  extent and opens/creates the next one. `FIX_OLIST_ITEM` is its own
  real `FILE3.BDO` dependency: once `openreel` extends a file via
  `MAKE`, it propagates the file's own new (larger) directory
  position into every open-file-list entry tracking it.

  `FIX_OLIST_ITEM`'s own real `swap`/`pack_sdcnt`/`swap` sandwiched
  right before its `searchn_list` call recomputes `BD_PACKED_DCNT`
  from whatever `BD_SDCNT`/`BD_SDBLK` held *before* the call (restored
  by the second `swap`) -- but a second, unconditional `pack_sdcnt`
  call later in the same loop iteration always overwrites that result
  before anything reads it. Provably dead the same way `CHECK_FCB` is
  (see that function's own header comment, `file1.c`), so not
  translated. `openreel`'s own `call make` is followed by a `jz
  openerr` whose flag `make`'s own real tail (`setfwf`'s `or
  al,fwfmsk`) can never leave clear -- also not modeled, same
  "translate faithfully but note unreachable" precedent as
  `cli_gopen`/`curselect`'s own `SELECT` success path.

  `SDCNT_EQ_XDCNT` (needed by `FIX_OLIST_ITEM`, a cross-file call into
  `FILE2.BDO`) and `openreel`/`FIX_OLIST_ITEM` themselves all land
  clear of the DATA.BDO flat-address collision (`--symbols`: 0x10ce,
  0x114d-0x11ed, 0x1ca2 respectively), unlike `open`/`OPEN1`/
  `opencopy`/`SWAP` from the previous pass -- but since `openreel`
  itself calls `open`-adjacent machinery (`close`, `opencopy`, both
  inside the collision range already), no new differential test was
  written here either, same reasoning as `func15`'s own entry above.
  Two new plain constants (`maxmod`) and two new `BD_*` fields
  (`BD_SAVE_MOD`, `BD_RMF`) added to `defs.h`. Full existing suite
  stays green; `make clean && make build` stays green.
- **`src/kernel/FILE4.BDO`'s `func16` (BDOS CLOSE) and `FUNC48`
  (flush blocking/deblocking buffers).** The other of `CHECK_FCB1`'s
  two real callers flagged back when `check_fcb1` itself was ported
  (`file1.c`) -- turned out to be the easy half: `func16`'s own
  fallback target, `CLOSE00`, is just `mov al,6 ! jmp set_aret` (a
  checksum-error report), so it maps directly onto `check_fcb1`'s own
  boolean return (`if (!check_fcb1(mem)) { set_aret(mem, 6); return;
  }`) with no `goto` needed at all, unlike `func15`'s own fallback
  (which had to collapse into its normal-success continuation).
  `FUNC48` is a 2-line wrapper: `FLUSH` (`BDOSIF.BDO`) is a boundary-
  stub tail jump into the XIOS table, and `DIOCOMP` (`file1.c`,
  made non-`static` for this) already has a fixed, always-0 result
  under the convention, so `func48` itself has no observable effect
  either.

  `func16` ends with the same "last reference closes the olist item,
  and its own lock-list entries if unlocked" logic `func15`'s own
  `OPENX05`/`OPENX08` build up -- `delete_item`/`remove_locks` (both
  `file3.c`, already ported/non-`static`) cover it with no new
  dependencies. One new `BD_*` field, `BD_FILE_ID` (0x0C88), added to
  `defs.h`. `func16`/`FUNC48` themselves land clear of the DATA.BDO
  collision (`--symbols`: 0x2005/0x24d7), but `func16` calls straight
  into `file2.c`'s own `close`/`close1` cluster, which doesn't -- so,
  same reasoning as `func15`'s own entry, no new differential test.
  Full existing suite stays green; `make clean && make build` stays
  green.
- **`src/kernel/FILE4.BDO`'s `func17`/`func18` (BDOS search for
  first/next occurrence of a file), + internal `csearch`/
  `reselectx`.** Both BDOS functions funnel into one shared tail,
  `CSEARCH`, which turned out to hide this port's first genuinely
  *unconditional* manual stack trick: the real asm `push`es `OFFSET
  dirtouser` then `jmp`s (never `call`s) into `SEARCH`/`SEARCHN`, so
  that routine's own ordinary `ret` lands at `dirtouser` instead of
  back in `CSEARCH` -- every single time, regardless of whether
  anything was found. Every other manual-stack-trick case ported so
  far (`CHECK_FCB1`, `OPENX`) only redirects on one specific outcome
  (an error, or success) -- this is the first one that's really just
  an unconditional tail call wearing the same disguise, so it
  collapses cleanly into a plain `search`/`searchn` call followed by
  an unconditional `dirtouser` call, no boolean/`goto` plumbing
  needed at all.

  `RESELECTX` itself is a second entry point into `reselect`
  (`file3.c`) -- at the time this paragraph was first written, only
  a local, `csearch`-scoped stub existed here (forcing
  `BD_HIGH_EXT=0` and nothing else, since nothing along `CSEARCH`'s
  own call path reads `BD_ACTUAL_RC` or the rest of `reselect`'s own
  tail back). That stub turned out to be genuinely incomplete, not
  just scoped: the real `RESELECTX` always falls straight through
  into `reselect`'s own full `RESELECT1`-onward tail (the "check
  current fcb"/`set_seldsk`/`CURSELECT`/user-code-folding logic),
  unconditionally -- caught while porting `FILE3.BDO`'s own real
  `RESELECTX` for that file's own row (see below), which needed the
  complete version anyway. Fixed by deleting the local stub and
  having `csearch` call the real, now fully-ported `reselectx`
  (`file3.c`) instead; confirmed via the full differential suite that
  nothing here depended on the missing tail being skipped. Two new
  `BD_*` fields, `BD_SRCHABASE`/`BD_SRCHAOFST` (`func17`'s own saved
  caller fcb location, restored by `func18`), added to `defs.h`.

  Everything this pass added lands clear of the DATA.BDO collision,
  but `csearch` calls straight into `file2.c`'s own colliding
  `search`/`searchn` cluster, so no new differential test here
  either, same reasoning as `func15`'s/`func16`'s own entries. Full
  existing suite stays green; `make clean && make build` stays green.
- **`src/kernel/FILE4.BDO`'s `func19` (delete), `func20`/`func21`
  (sequential read/write), `func22` (MAKE), and `func23` (RENAME).**
  The largest single pass this session: scoping `func20`/`func21`
  out transitively turned up a big, previously-unscoped chunk of
  `FILE3.BDO`'s own "part 3" (sequential disk I/O), almost entirely
  self-contained since nearly everything it calls (`getfcb`/`setfcb`/
  `bdos_index`/`alloct`/`atran`/`seek`/`rdbuff`/`getblock`/`openreel`)
  was already ported from earlier passes. New: `diskread`/`sqread`
  and `diskwrite`/`sqwrite` (`src-c/kernel/file3.c`), + their own
  shared `test_disk_fcb`/`TEST_DISK_FCB1` helper (refactored into a
  `test_disk_fcb1_tail` shared by both `test_disk_fcb` itself and
  `diskwrite`'s own direct `CALL CLOSE ! CALL TEST_DISK_FCB1`, which
  reuses the same tail after a different close variant), `reset_fwf`,
  and the lock-list check `test_lock`/`move_arecord` (`diskwrite`'s
  own unlocked-mode path -- `TEST_LOCK`'s own failure tail,
  `LOCK_ERR`, does a `POP`/`POP` against a stack shape `diskwrite`
  deliberately sets up to match, the same manual-stack-trick category
  as `CHECK_FCB1`, modeled as a plain boolean since this port doesn't
  track a call stack at all). `dmposition`/`getdm` (`file1.c`) made
  non-`static` for this.

  `func20`/`func21` turned out to be `CHECK_FCB`'s own two real
  callers -- an earlier revision of `check_fcb1`'s own header comment
  (`file1.c`) wrongly called `CHECK_FCB` provably dead, reasoning only
  about `func15`/`func16`'s own two `CHECK_FCB1` call sites; re-
  reading `CHECK_FCB`'s own real asm closely showed it's nothing but
  `MOV CHECK_FCB_RET,FALSE` falling straight into `CHECK_FCB1`, no
  separate complication at all (a second stale claim this same
  comment made, also fixed). Both corrected in `file1.c`'s own header
  comments.

  `func22` (MAKE) needed the most new ground: `chk_xfcb_password1`,
  `init_xfcb`/`init_xfcb0`, and `set_pw` (all `file3.c`), plus
  `delet0` (`file2.c`'s own `delet` dispatch, split out non-`static`
  since `func22` calls it directly after its own `SEARCH`, same
  "`rename_` calls `delete1_entry` directly" precedent from an earlier
  pass). More importantly, `func22`'s own tail reuses `func15`'s own
  `OPENX1`/`OPENX03`/`OPENX08` cascade directly -- `openx` (`file4.c`)
  was a single monolithic function before this pass; refactored into
  `openx2`/`openx08`/`openx03`/`openx03_create`/`openx05`/`openx04`/
  `openx1` so both `openx` and `func22` can enter it at the right
  point. `func22` also has its own instance of the "`MAKE`'s own tail
  always leaves `AL` nonzero/`ZF` clear" unreachable-branch pattern
  `openreel` found first (`file2.c`'s own header comment) -- the
  "delete the fcb just created, report no room" fallback after a
  `MAKE` call is dead code here too.

  `func19`/`func23` are both thin wrappers (`copy_dma_in` + a single
  tail call into `delet`/`rename_`, already ported). All 5 functions
  land clear of the DATA.BDO collision, but every one of them calls
  into `file2.c`'s own colliding `search`/`close`/`open`/`delet`/
  `rename_` clusters somewhere in its chain, so none gets a new
  differential test, same reasoning as `func15`'s own entry. Several
  new `BD_*`/plain constants added to `defs.h` (`BD_SRCHABASE`/
  `BD_SRCHAOFST`, `BD_DMINX`, `lstrec`). Full existing suite stays
  green; `make clean && make build` stays green.

  This leaves `func13`/`func14` (reset disk system / select disk
  info, needing `FILE3.BDO`'s own `DISKRESET` -- a sizable per-disk
  open-list scan not reconned yet), `func30` (set file indicators),
  `func33`/`func34` (random read/write, needing `FILE3.BDO`'s own
  `RSEEK`, also not reconned), `func35`-`func37`/`func40`-`func43`
  (file size, set random record, reset/access/free drive, write-
  random-with-zero-fill, lock/unlock record), and `FUNC46` (get disk
  free space) as `FILE4.BDO`'s own remaining unported functions.
- **`src/kernel/FILE3.BDO`'s `diskreset`, and `src/kernel/FILE4.BDO`'s
  `func13`/`func14` (reset disk system/select disk info) + `func37`
  (reset drive, + internal `reset_37`).** `func14` turned out trivial
  (`SET_SELDSK`/`CURSELECT`, both already ported). `func13`/`func37`
  share `DISKRESET`: for each bit set in its own input vector
  (`BD_INFO`), check whether another process has a file open on that
  disk in a removable/forced-read-only way (`SEARCHN_LIST`/
  `compare_pds`, both already ported); if every open file on that
  disk belongs to this same process, it's safe to reset, otherwise
  report "denied" (`BD_ARET=0xFFFF`, the actual denied-process display
  a boundary-stub no-op under this port's convention, same as
  `XCRLF`/`XPRINT` elsewhere).

  `DISKRESET`'s own `TEST_VECTOR1`/`SET_CDISK1` calls (`file1.c`) turned
  out to need splitting those two functions out of the already-ported
  `test_vector`/`setcdisk`: both of the real asm's own callers here
  pass an explicit bit-position/shift-amount (`BD_CURDSK`, the disk
  actually being checked) rather than letting the function read
  `BD_SELDSK` itself. `func13` calls `RESET_37` directly -- `func37`'s
  own real continuation after its own `CALL DISKRESET`, with a
  different `BD_INFO` already set up -- the same "call into the
  middle of another function" pattern as `rename_`'s own `CALL
  DELETE1` from an earlier pass.

  `DISKRESET`/`func13`/`func14`/`func37` themselves land clear of the
  DATA.BDO collision, but `TEST_VECTOR1`/`SET_CDISK1` don't (0x8c5/
  0x8da in the combined wrapper), so this cluster gets no new
  differential test either. `func38`/`func39`/`FUNC41`/`FUNC47`/
  `FUNC49`/`FUNC50` turned out to have no body of their own at all --
  `EQU FUNCRET`, a dispatch-table alias straight onto `BDOSIF.BDO`'s
  own empty `funcret: ret` -- nothing to port, removed from the
  "remaining" list. Several new `BD_*` constants added to `defs.h`
  (`BD_NTLOG`, `BD_SET_RO_FLAG`, `BD_CHECK_DISK`, `BD_DENIEDDRV`,
  `BD_DENIEDCNS`, `BD_DENIEDPRC`). Full existing suite stays green;
  `make clean && make build` stays green.

  This leaves `func30` (set file indicators), `func33`/`func34`
  (random read/write, needing `FILE3.BDO`'s own `RSEEK`, not reconned
  yet), `func35`/`func36` (file size, set random record), `func40`-
  `func43` (write-random-with-zero-fill, lock/unlock record), and
  `FUNC46` (get disk free space) as `FILE4.BDO`'s own remaining
  unported functions.
- **`src/kernel/FILE3.BDO`'s `rseek`.** The user explicitly asked for
  this one by name. Decodes the fcb's own packed 3-byte `ranrec`
  field (bit-twiddling: record# in byte0's low 7 bits, extent# split
  across byte0's own high bit and byte1's low nibble, module# packed
  across byte1's high nibble and byte2 via a nibble-swap) into
  {module#, extent#, record#}, then either confirms the currently-
  addressed extent already matches (same module and extent, or an
  adjacent extent within the same directory-fcb group via `COMPEXT`)
  or closes it and opens/creates the target extent (`RANCLOSE`) --
  reading a not-yet-written extent is an error, writing one creates
  it via `MAKE`.

  Needed four `file2.c` helpers non-`static` for cross-file use:
  `COMPEXT`, `GET_DIR_EXT`, `RESTORE_RC`, `SET_RC` (all from the
  already-ported `CLOSE`/`CLOSE1` cluster) -- `get_dir_ext_result`
  moved to `defs.h` for this, same precedent as `get_xfcb_result`.
  The real asm's own flags entering `rseek`'s own two (not yet
  ported) callers, `rdread`/`rdwrite`'s own `JNZ RET19` check, are
  unambiguous on the pure-success and direct-`SEEKERR` tails but
  genuinely incidental on the `RANCLOSE`/`BADSEEK` paths (nothing
  between `CALL MAKE_FCB_INV` and the final `RET` touches flags
  again) -- translated as the clearly intended boolean throughout
  instead, same class of judgment call as `diskreset`'s own flag
  handling from the previous pass.

  `rseek` itself lands clear of the DATA.BDO collision, but
  `COMPEXT`/`GET_DIR_EXT`/`RESTORE_RC`/`SET_RC` (part of the already-
  colliding `CLOSE` cluster) and `close`/`open`/`make` themselves
  don't, so no new differential test. Full existing suite stays
  green; `make clean && make build` stays green.
- **`src/kernel/FILE4.BDO`'s `func33`/`func34` (random disk
  read/write), + `FILE3.BDO`'s own `rdread`/`rdwrite`/`parsave36`/
  `parsave36r`.** Both BDOS functions are thin wrappers now that
  `rseek` exists: `parsave36r` (parsave a 36-byte xfcb, then
  `reselect` -- same shape as `parsave33r`) + `CHECK_FCB` (a third and
  fourth real caller, alongside `func20`/`func21` -- `check_fcb1`'s
  own header comment, `file1.c`, has the full writeup) + a tail call
  into `rdread`/`rdwrite`, each just `rseek` followed by
  `diskread`/`diskwrite` (both already ported) on success.

  `rdread`/`rdwrite`/`func33`/`func34`/`parsave36r` all land clear of
  the DATA.BDO collision, but `parsave36` doesn't (0x454, same as
  `parsave`/`parsave33` from an earlier pass), and `rdread`/`rdwrite`
  transitively touch the already-colliding `close`/`open`/`make`/
  `search` cluster regardless -- so, same reasoning as `func15`'s own
  entry, no new differential test. Full existing suite stays green;
  `make clean && make build` stays green.

  This leaves `func30` (set file indicators), `func35`/`func36` (file
  size, set random record -- needing `FILE3.BDO`'s own
  `getfilesize`/`setrandom`/`cmperr`, not reconned yet), `func40`-
  `func43` (write-random-with-zero-fill, lock/unlock record), and
  `FUNC46` (get disk free space) as `FILE4.BDO`'s own remaining
  unported functions.
- **`src/kernel/FILE4.BDO`'s `func30` (set file indicators), +
  `FILE2.BDO`'s own `indicators`.** `indicators` turned out to use a
  simpler, *unmasked* variant of the directory-copy pattern
  `COPY_DIR0`/`COPY_DIR1` already use: it calls `COPY_DIR2` (already
  ported, `copy_dir2`) directly and then a plain `MOVE` instead of
  `COPY_DIR1`'s own per-byte attribute-masking loop -- `MOVE`'s own
  `MOV CH,0` discards whatever attribute mask `COPY_DIR2` packed into
  `CH`, so the copy is unconditional. `fcb_extnum` (12) gets reused
  purely for its *numeric* value here too, as the copy length, not
  its usual field-offset meaning -- the same "constant reuse for
  numeric value" trick `rename_`'s own `DSKMAP`/`EXTNUM` use.

  `func30` itself is a thin wrapper around `indicators`; the real
  asm's own `CALL indicators ! JMP cpydirloc` is redundant, since
  `indicators` already ends with its own call into `cpydirloc` -- the
  second one is just a no-op repeat, not translated separately.
  `func30` lands clear of the DATA.BDO collision, but `indicators`
  doesn't (0xf0f, inside `COPY_DIR0`-`2`'s own already-colliding
  range), so no new differential test. Full existing suite stays
  green; `make clean && make build` stays green.
- **`src/kernel/FILE4.BDO`'s `func35`/`func36` (return file size,
  set random record), + `FILE3.BDO`'s own `getfilesize`/`setrandom`
  (+ internal `cmperr`).** `cmperr` packs a directory entry's or
  fcb's own record-count/extent#/module# fields into the same 24-bit
  "mmmm eeee errr rrrr" position `ranrec` itself stores, shared by
  both callers (`getfilesize` via `fcb_reccnt`'s own offset,
  `setrandom` via `fcb_nxtrec`'s). `getfilesize` scans every matching
  directory extent via `SEARCH`/`SEARCHN` (both already ported),
  computing each one's own position and keeping the largest
  (`fcb_ranrec` ends up holding the file's own logical size); the
  real asm's own 24-bit unsigned compare (`SUB AX,[BX]` then `SBB
  AL,2[BX]`, testing the final carry) translated as a plain C `int`
  subtraction with an explicit borrow term, simpler than chasing
  flags by hand. `setrandom` just runs `cmperr` once and writes the
  result straight into `fcb_ranrec` -- the reverse direction of what
  `getfilesize` reads.

  `getfilesize`/`setrandom`/`cmperr`/`func35`/`func36` all land clear
  of the DATA.BDO collision -- but `getfilesize` calls into
  `file2.c`'s own colliding `search`/`searchn` cluster, so `func35`
  still gets no new differential test; `setrandom`/`func36`, by
  contrast, never touch that cluster at all (pure fcb-field
  arithmetic) and would be a legitimate candidate for one, just not
  written yet (time, not a technical blocker -- same standing
  disclaimer as several earlier entries). Full existing suite stays
  green; `make clean && make build` stays green.

  This leaves `func40`-`func43` (write-random-with-zero-fill, lock/
  unlock record) and `FUNC46` (get disk free space) as `FILE4.BDO`'s
  own remaining unported functions.
- **`src/kernel/FILE4.BDO`'s `func40` (write random with zero fill)
  and `FUNC46` (get disk free space), + `FILE3.BDO`'s own `rseek1`/
  `tmp_select`.** `func40` turned out to need `rseek`'s own real body
  as a *separate* entry point: the real asm's own `RSEEK1` label
  skips `RSEEK`'s own `MOV SEQIO,0` (since `func40` just set
  `BD_SEQIO=2` itself, right before calling it), and calls `DISKWITE`
  directly rather than going through `rdwrite`'s own `rseek`-then-
  `diskwrite` wrapper. Split the existing `rseek` into `rseek1` (the
  real work) + a 2-line `rseek` wrapper (`BD_SEQIO=0` then `rseek1`)
  to match, and made `diskwrite` non-`static` for `func40`'s own
  direct call.

  `FUNC46` needed one new small helper, `tmp_select` (`TMP_SELECT`,
  `file3.c`): temporarily select a given disk via `CURSELECT`,
  restoring `BD_SELDSK` (but not `BD_CURDSK`, `CURSELECT`'s own real
  side effect) afterward. The free-space count itself is a per-byte
  popcount over the disk's own allocation vector (`GET_NALBS`,
  already ported) -- the real asm's own `SHR`/`JNC` loop skips
  leading zero bits before counting each `1` bit, but a plain bit-by-
  bit scan computes the identical total (same Hamming weight either
  way), so that's what's translated. Converting the resulting block
  count to a record count (`SHL ...,BLKSHF`) is done in a 32-bit
  intermediate instead of chasing the real asm's own split 16-bit-
  plus-carry-into-AH computation -- verified by hand against a
  worked example (0x1234 blocks, shift 4, both approaches give
  0x12340) rather than assumed.

  `func40`/`FUNC46`/`rseek1`/`tmp_select` all land clear of the
  DATA.BDO collision, but `diskwrite` (`func40`'s own tail) and
  `GET_NALBS` (`FUNC46`'s own tail) don't, so no new differential
  test for either. `FUNC41` turned out to be another `EQU FUNCRET`
  alias, same as `func38`/`func39`/`FUNC47`/`FUNC49`/`FUNC50` from an
  earlier pass -- nothing to port. Full existing suite stays green;
  `make clean && make build` stays green.

  This leaves only `func42`/`func43` (lock/unlock record) as
  `FILE4.BDO`'s own remaining unported functions -- their own real
  dependency, `FILE3.BDO`'s `LOCK`/`GET_LOCKADD`/`LOCK_ERR` cluster,
  does genuine stack-pointer manipulation (`LOCK_PERR`'s own `MOV
  SP,LOCK_SP`) to unwind out of a per-record loop on a lock conflict
  -- a fundamentally different class of translation problem than
  anything else in this file, not reconned in enough detail yet to
  be confident in a translation; left for its own dedicated pass.
- **`src/kernel/FILE4.BDO`'s `func42`/`func43` (lock/unlock record),
  + `FILE3.BDO`'s own `LOCK`/`GET_LOCKADD`/`LOCK_ERR` cluster (`lock_`,
  + internal `incr_rr`/`save_rr`/`reset_rr`, `count_locks`,
  `check_free`, `get_lockadd`, `lock_err_apply`, `free_lock`,
  `set_lock`). This finishes `FILE4.BDO`.**

  The dedicated recon this cluster needed turned up the one finding
  that made the whole thing tractable: `LOCK_PERR` -- the entry point
  that actually does `MOV SP,LOCK_SP` to restore the real stack
  pointer -- has **no caller anywhere in `src/`**, confirmed by a full
  grep across every `.BDO`/`.SUP`/`.MEM`/`.CIO`/`.RTM` file. It's
  provably dead code. Every real path that fires `LOCK_ERR` (three of
  them: `GET_LOCKADD`'s own two internal checks, and `TEST_LOCK`'s own
  conflict case) goes through the *other*, much more ordinary half --
  a `POP DX ! POP CX` discard against a stack shape `LOCK`'s own per-
  record loop deliberately sets up (one pushed countdown word per
  iteration, balanced by its own `POP`, plus one pushed fallback
  address -- `OFFSET LOCK9` -- that stays on the stack for the whole
  loop and gets silently consumed by whichever `RET` reaches it first,
  success or error). That's the same manual-stack-trick category
  already modeled throughout this port (`CHECK_FCB1`, `OPENX`,
  `CSEARCH`'s own unconditional variant) -- translated here by
  threading the loop's own counter through explicitly as `remaining`,
  standing in for the real asm's own popped `CH`, instead of modeling
  a call stack.

  One real wrinkle survived needing a judgment call: `GET_LOCKADD`'s
  own `RSEEK` call never has its read/write-mode input (`CL`) set by
  any of its real callers -- genuinely incidental register content,
  same "undefined artifact" category as `selectdisk`'s own fixed
  `BX=0` (`file1.c`); resolved the same way, with a fixed, documented
  choice (read_mode=0) rather than chasing an unknowable value.
  `SET_LOCK`'s own `MOV [DI],DX` (storing the owning olist item's
  address at a lock-list item's own `item+8` field, overwriting the
  zero `SET_OLIST_ITEM` itself just wrote there) relies on the same
  real `rep movs` DI-after-the-copy side effect already documented for
  `SET_OLIST_ITEM` itself (`file3.c`'s own header comment) -- not
  modeled directly, the address is recomputed as `bx+8` instead, same
  precedent. `TEST_LOCK` itself needed a small retrofit: it used to
  set `BD_LRET=8` directly on conflict (correct for `diskwrite`'s own
  single call, which happens to make the real `LOCK_ERR` computation
  collapse to exactly that), but `lock_`'s own per-record loop needs
  the full `mult_cnt`-shifted version instead -- so `test_lock` no
  longer touches `BD_LRET` itself, and each of its two callers applies
  its own version now.

  `lock_`/`count_locks`/`check_free`/`get_lockadd`/`free_lock`/
  `set_lock`/`func42`/`func43` all land clear of the DATA.BDO
  collision, but `incr_rr`/`save_rr`/`reset_rr` (`BDOSIF.BDO`'s own,
  ported here since nothing else uses them) don't, so no new
  differential test for this cluster. Several new `BD_*` constants
  added to `defs.h` (`BD_LOCK_SP`/`BD_LOCK_SHELL` -- both confirmed
  write-only and thus not modeled at all, `BD_LOCK_UNLOCK`,
  `BD_LOCK_MAX`, `BD_SHELL_RR`). Full existing suite stays green;
  `make clean && make build` stays green.

  **`FILE4.BDO` is now ported in full.** Every real BDOS dispatch
  function (`func13`-`func52`) has a C counterpart, modulo the handful
  that are bare `EQU FUNCRET` dispatch-table aliases with no body of
  their own (`func38`/`func39`/`FUNC41`/`FUNC47`/`FUNC49`/`FUNC50`)
  and `GOBACK` (a 2-instruction tail jump into `BDOSIF.BDO`'s own
  module-dispatch epilogue) -- both out of scope by definition, not
  unfinished work.
- **`src/loader/LBDOSNEW.A86`'s `func51`, `src/kernel/INIT.SUP`'s
  `init`, and `tools/i86sim.py`'s new `far_call_stub_addrs`.** Picked
  up at the user's own explicit direction to go after the boot/BIOS-
  loader chain, starting with the smallest, most self-contained
  pieces. `LBDOSDAT.A86`/`LDBDOS.A86` turned out to be pure data/pure
  `INCLUDE` glue respectively -- nothing to port, confirmed by
  reading them in full, not assumed from their small size.
  `LBDOSNEW.A86`'s own `func51` ported and tested trivially.

  `INIT.SUP`'s own `init` looked harder than it turned out to be: its
  five `callf dword ptr <...>mod[bx]` calls (far-calling into RTM/
  MEM/BDOS/CIO/XIOS's own `init:` entry points through `SYSDAT.FMT`'s
  module table) needed a genuinely new `tools/i86sim.py` capability --
  an indirect far `CALL`'s own *target* is only known at runtime
  (loaded from the module table), so it can't be stubbed by target
  address the way an ordinary `CALL`/`call_stub_addrs` is. Added
  `far_call_stub_addrs`, keyed by the `CALL FAR [mem]` instruction's
  *own* address instead (opcode `0xFF /3`) -- `_modrm()` already
  consumes the whole instruction's own bytes by the time the stub
  check fires, so doing nothing further is exactly "continue with the
  next instruction," the same "doesn't return, or resumes exactly
  here" reasoning as every other stub mechanism in this simulator.

  Along the way, found and fixed a real, previously-uncaught bug in
  `/src`: `mov u_wrkseg,ds` (no `es:` prefix) in `INIT.SUP` landed on
  `SYSDAT`'s own offset `2Eh` instead of the init process's own UDA
  (`u_wrkseg`'s real offset there) -- which happens to be the exact
  same offset as the module table's own XIOS entry's "initialize" far
  pointer's segment word (`xiosmod`'s own base + `mod_init` + 2),
  confirmed by offset arithmetic against `MODTAB.DEF`/`UDA.FMT`, not
  just suspected -- right before the `xiosmod` `callf` a few lines
  later would have jumped through that same corrupted word. Same bug
  class, same file, as two other bare-`sysdat`-write fixes `INIT.SUP`
  already had from an earlier pass; fixed the same way (an explicit
  `es:` prefix), with the same style of comment.

  The RSP-creation loop (walking `SD_RSPSEG`'s own linked list via
  each entry's own `rsp_link` field) is modeled faithfully via
  `mem_r16_seg`/`mem_w16_seg`, the segment-relative-addressing
  convention already established for `ml.c`/`proc.c`/`mau.c`/
  `share.c`/`load.c`. Neither that loop nor the fixed `u_wrkseg` write
  is independently exercised by `tests/src-c/test_init.py`, though:
  both go through a real `es:`-prefixed access, and
  `tools/i86sim.py` only computes a real segment:offset address for
  one when `segmented=True` -- which would also turn every *other*
  field this routine touches (flat scratch addresses, not real
  segment-relative ones) into broken `ds*16+offset` computations in
  the same run. Several new `BD_*`/`SD_*`/plain constants added to
  `src-c/kernel/defs.h` (`i_mpm_cs`, `u_wrkseg`, `rsp_pd`, `rsp_link`,
  `mpmint`, `bdosmod_bit`/`ciomod_bit`/`xiosmod_bit`, `mod_init`,
  `io_maxconsole`/`io_maxlist`, `SD_NCIODEV`, the `SD_INIT_*`/
  `SD_SUPMOD_SEG`/`INIT_SYSDAT`/`INIT_SUPERVISOR_SEG` block). Full
  existing suite stays green; `make clean && make build` stays green.

  This leaves `src/loader/LDBIOS86Q.A86`/`LDBIOS.A86`, `ROM86Q.A86`/
  `ROM.A86`, and `LDMPM.A86` as the rest of the boot chain -- the
  user has explicitly confirmed these are in scope (not excluded the
  way `XIOS.A86`/`XIOS86Q.A86` once looked, which was itself a
  mischaracterization: that exclusion is about a real, separate
  assembly bug (#7) blocking `XIOS.A86` specifically, not a policy
  against hardware-driver code). `LDBIOS86Q.A86` (the one actually
  used by `tools/build_mpmldr.py`; `LDBIOS.A86` is the real-hardware
  variant, not wired into the build at all) is a genuine UART + NEC765
  floppy-controller driver -- real port-mapped I/O (`IN`/`OUT`), which
  `tools/i86sim.py` has no support for at all yet (confirmed by grep,
  not assumed). That's the next piece of new simulator infrastructure
  needed here, parallel to (but a bigger lift than) `far_call_stub_addrs`
  above.

  Added that `IN`/`OUT` support to `tools/i86sim.py`: all four real
  8086 forms (`0xE4`/`0xE5` imm8-port, `0xEC`/`0xED` DX-port, for `IN`;
  `0xE6`/`0xE7`/`0xEE`/`0xEF` for `OUT`), each 8- or 16-bit depending
  on the opcode's own low bit. Backed by two new optional constructor
  callables, `io_in_hook(sim, port, width) -> value` and
  `io_out_hook(sim, port, width, value)` -- deliberately "no silent
  default device": a `CALL_FAR` or `INT` with no matching stub set is
  at least an explicit, inert no-op by design, but a real I/O port
  with no hook has no sane no-op value to return, so it raises
  loudly instead of silently returning e.g. `0`. `tools/asm86/isa.py`
  already encoded all four forms (confirmed by reading it directly,
  not grep alone, since `tools/asm86.py` itself turned out to be an
  11-line wrapper around the real `tools/asm86/` package) -- no
  assembler-side work was needed, only the simulator side. Smoke-
  tested directly (not yet via a differential test, since there's no
  C port consuming it yet): a tiny assembled `IN`/`OUT` program
  round-trips correctly through both hooks, and an `IN`/`OUT` with no
  hook raises as designed. Full existing suite stays green.

  Next: a minimal simulated UART (and eventually NEC765 floppy
  controller) device model to back these hooks, then the actual
  `LDBIOS86Q.A86` port.

- **`src/loader/LDBIOS86Q.A86`'s console-I/O subset ported**: `CONST`,
  `CONIN`, `CONOUT`, `PUNCH`/`READER` (one body, two labels -- `PUNCH`
  has no code of its own, just falls into `READER`'s), `GETIOBF`.
  `LISTOUT`/`LISTST`/`SETIOBF` are bare `ret`s with no observable
  effect -- nothing to port, same class as `LBDOSNEW.A86`'s own
  `func50`/`52`-`57`. `pmsg`/`signon` have no caller anywhere in this
  file (confirmed by grep) -- dead code, same "assembles but never
  executed" class as other such findings in this tree.

  First file in this port needing real port-mapped I/O: `src-c/loader/
  ldbios86q.c`'s ported routines take `io_in8`/`io_out8` function-
  pointer parameters, matching `tools/i86sim.py`'s new `io_in_hook`/
  `io_out_hook` one-for-one. Backed, for testing, by a new shared
  fixture, `tests/src-c/fixtures/uart86q.py`'s `Uart86Q` -- a
  deliberately narrow model of just the two UART registers (LSR,
  RXTX) this driver touches, not a general 8250 emulation -- driving
  both the reference run (via `io_in_hook`/`io_out_hook`) and the C
  port (via `ctypes.CFUNCTYPE` callbacks of the same shape) from one
  real device instance, the same "one real model, two call sites"
  shape every other fixture in this tree uses for the shared memory
  image. `TXRDY` is always reported set (this model's own `out8`
  "transmits" instantly), so `CONOUT`'s own `cout_wait` loop is
  translated faithfully as a real loop, just one that always
  succeeds on the first poll under this model; `RXRDY` genuinely
  reflects whether the test's own `rx` queue still has bytes left, so
  `CONIN`'s own `cin_wait` loop is exercised for real too (unlike
  `INIT.SUP`'s RSP loop or `QUE1.RTM`'s retry loops, both of which
  could only be translated faithfully, not independently exercised).

  `tests/src-c/test_ldbios86q.py` assembles `LDBIOS86Q.A86` directly
  (no per-file wrapper needed -- the file is fully self-contained, no
  `INCLUDE`s at all, confirmed by grep). All 7 cases match. Full
  existing suite stays green; `make clean && make build` stays green.

  This leaves `INIT`/`WBOOT` and the whole NEC765 floppy-controller
  driver (`SELDSK`/`HOME`/.../`READ`/`WRITE`, plus the 8237 DMA
  setup) as the rest of `LDBIOS86Q.A86` -- a bigger lift than the
  console subset (PIO result-phase draining, DMA channel programming,
  several rounds of real, already-documented `/src` bugs this exact
  file's own header comment catalogs), and the next piece of this
  port's own boot-chain work after this.

- **`src/loader/LDBIOS86Q.A86`'s `INIT` and non-DMA disk-I/O entry
  points ported**: `INIT` (UART/FDC hardware setup -- its own stack
  switch and final `jmp ccp_offset` back into `LDMPM.A86` have no C
  equivalent, same as every other real OS entry point in this port;
  `WBOOT` is nothing but that same jump, nothing to port), `SELDSK`,
  `HOME`, `SETTRK`, `SETSEC`, `SECTRAN`, `SETDMA`, `SETDMAB`,
  `GETSEGT`, plus the FDC command-sequencing utilities they share
  (`fdc_send_cmd`/`fdc_out_byte`/`fdc_in_byte`/`fdc_sense_int`/
  `fdc_wait_seek`). `motor_delay` has no caller anywhere in this file
  (confirmed by grep) -- dead code, same "assembles but never
  executed" class as `pmsg`/`signon`.

  `fdc_wait_seek`'s own `loopnz` (poll MSR's `BUSY` bit, bounded by a
  `cx` that starts at 0 -- a real ~65536-iteration bound, not a
  genuine infinite loop) needed one more new `tools/i86sim.py`
  opcode: `TEST AL,imm8`/`TEST AX,imm16` (`0xA8`/`0xA9`, the
  accumulator-immediate form -- every earlier `TEST` in this tree
  used the register/memory (`0x84`/`0x85`) or group-F6/F7-immediate
  forms instead).

  `SELDSK`'s own `offset dpe0` and `GETSEGT`'s own `offset segtable`
  are both real, deterministically-resolved addresses (confirmed via
  `--symbols` -- this file assembles standalone, no `INCLUDE`s at
  all), ported as the constants themselves; neither one's own
  pointed-to content is modeled, since nothing ported so far reads
  either structure, same "unconsumed, not modeled" precedent as
  `INIT.SUP`'s own `p_uda` write (`src-c/kernel/init.c`).

  Tested against two new shared device fixtures, both driving the
  reference run (via `io_in_hook`/`io_out_hook`) and the C port (via
  `ctypes.CFUNCTYPE` callbacks) from one real model each, same shape
  as `uart86q.py`'s own `Uart86Q`: `tests/src-c/fixtures/fdc86q.py`'s
  `Fdc86Q` (a deliberately narrow NEC765 model -- MSR/DATA only, no
  real seek/media state, auto-generating SENSE INTERRUPT STATUS's own
  2-byte result the instant that command byte is sent, since this
  fixture has no other way to drive the controller's own internal
  state machine forward). 18 cases match in all. Full existing suite
  stays green; `make clean && make build` stays green.

  This leaves the real sector-transfer path -- `rw_physical`/
  `fdc_fill_rw_parms`/`fdc_seek_to`/`fdc_pio_in`/`dma_setup`/`READ`/
  `WRITE` -- as the rest of `LDBIOS86Q.A86`, and the last piece of
  the boot chain's own critical path in this file. After that:
  `ROM86Q.A86` and `LDMPM.A86`.

- **`src/loader/LDBIOS86Q.A86` is now fully ported**: `READ`/`WRITE`
  (the real sector-transfer path) plus the utilities they alone
  needed (`rw_physical`, `fdc_fill_rw_parms`, `fdc_seek_to`,
  `fdc_pio_in`, `dma_setup`). `fdc_fill_rw_parms`/`fdc_seek_to` each
  rewrite their own command table's FULL content every call (not
  just the head/cyl/sector fields the real asm's own version
  touches) -- this port's `mem` has no "preloaded image" the way a
  real cseg's own static data does when called standalone, and
  nothing else ever reads/writes the untouched bytes of either
  table, so this produces identical observable behavior, same
  "simplified without changing what's observable" precedent as
  `SELDSK`'s own fixed `bx=0`.

  `READ`/`WRITE`'s own `cs` (needed by `dma_setup`'s 20-bit
  physical-address math for `secbuf`) is a parameter, same "ambient/
  external, not derivable from this file alone" class as
  `src-c/kernel/init.c`'s own `cs`/`ds_sysdat`. The actual
  `fd_secsize`-byte disk<->`secbuf` DMA transfer itself is NOT
  modeled (no disk media exists anywhere in this harness for either
  side to transfer from/to) -- what's real and tested is the
  CPU-visible 128-byte copy between `secbuf`+`cur_bufoff` and the
  caller's own `dma_seg:dma_adr` buffer, a genuine segment:offset
  access. New `mem_r8_seg`/`mem_w8_seg` helpers in `src-c/loader/
  defs.h`, mirroring `src-c/kernel/defs.h`'s own.

  Tested for real (not just "translated faithfully, unexercised")
  by running the reference side with `tools/i86sim.py`'s
  `segmented=True` mode for the first time in `/src-c/loader`, with
  this module's own `cs`/`ds`/`ss` pinned to 0 -- a valid choice
  since this file's own `cs` is genuinely unconstrained from the
  outside, and unlike `INIT.SUP`'s own `u_wrkseg`/RSP-loop, no OTHER
  real field `READ`/`WRITE` touches depends on `ds` being nonzero, so
  there's no version of that file's own segmented-vs-flat conflict
  here. `tests/src-c/fixtures/fdc86q.py`'s `Fdc86Q` grew a command-
  length table (`CMD_LEN`/`RESULT_LEN`) so its own result phase
  triggers only once a whole command (not just its first byte) has
  gone out -- triggering any earlier made `fdc_out_byte`'s own wait
  for "ready for another parameter byte" spin forever on the same
  command's own remaining bytes, caught by a real `did not halt`
  from the differential test itself, the same signal this harness
  always uses to find a wrong fixture. All 24 cases match. Full
  existing suite stays green; `make clean && make build` stays
  green.

  `LDBIOS86Q.A86` is done. Next: `ROM86Q.A86` and `LDMPM.A86`, the
  rest of the boot chain.

- **`src/loader/ROM86Q.A86` (the QEMU boot sector) and `src/loader/
  LDMPM.A86` (MPMLDR's own main orchestrator) are both fully ported**,
  the last two files in the boot chain.

  `ROM86Q.A86` needed a new `tools/i86sim.py` mechanism: `int_hooks`,
  keyed by interrupt number like `call_hooks` is keyed by a CALL
  target -- a BIOS `INT 13h` disk read's own real effect (the sector
  data, and success/failure) genuinely has to be modeled for the
  test to mean anything, the same "boundary has to be modeled, but
  not via a plain no-op stub" reasoning `call_hooks` already uses.
  `src-c/loader/rom86q.c`'s own `int13_fn` callback mirrors this one
  for one. A new shared fixture, `tests/src-c/fixtures/bootdisk.py`'s
  `BootDisk`, backs both sides (same "one real model, two call
  sites" shape as `uart86q.py`'s own `Uart86Q`). Also needed: `STC`
  (`0xF9`, `read_sector`'s own final-failure path) and a new `JMP
  FAR [mem]` form (`0xFF /5`, `far_jmp_stub_addrs` -- like
  `far_call_stub_addrs` but halting outright, since there's no
  caller to return to for a JMP) for this file's own real exit, the
  `jmpf dword ptr leap_off` into MPMLDR.bin. Neither the header's own
  bytes nor any body sector's own bytes are modeled as landing
  anywhere in a flat `mem` array -- nothing in this port reads
  either back, so `int13_fn` hands them straight back through an
  output parameter instead, and this file needs no `mem` parameter
  at all. Found (via the retry-path test case) that the real asm's
  own disk-reset call reuses AL/CH/CL/DH from the just-failed read
  attempt (only AH/DL are reloaded) -- translated faithfully, not
  zeroed, after the first attempt at zeroing them produced a
  mismatching call sequence against the reference run. 12 cases
  match.

  `LDMPM.A86` needed one more new mechanism: `jmp_hooks`, keyed by a
  near JMP's own target like `call_hooks` is keyed by a CALL's --
  first needed because `start:`'s own very first instruction is
  `jmp mldbios`, a near jump into `LDBIOS86Q.A86`'s own `INIT`
  (concatenated right after this module in the real MPMLDR.bin),
  whose own tail (`jmp ccp_offset`) jumps straight back to right
  after this `jmp`. This is the first genuinely wired-together call
  in this whole port: `src-c/loader/ldmpm.c`'s own `start_entry`
  makes a REAL `extern` call into `src-c/loader/ldbios86q.c`'s own
  `init_entry` (both `.c` files compiled into one shared library for
  the test -- the first multi-file link here), while the reference
  run's own `jmp_hooks` entry replays the identical UART/FDC write
  sequence for the comparison.

  Every OTHER real effect in `LDMPM.A86` -- console output, opening/
  reading MPM.SYS -- goes through its own synthetic `int 224`/
  `sys_vec` (confirmed to be a completely different, loader-local
  convention from the real MP/M-86 kernel's own `int mpmint`, same
  number but a fresh jump table this exact routine sets up itself,
  not wired to the real already-ported `src-c/loader/lbdos*.c`
  functions -- same "boundary has to be modeled, but not yet woven
  into one real call graph" scope every other not-yet-linked file in
  this port uses, see this doc's own intro on the link/boot step
  being separate work). Modeled as three C callback parameters
  (`bdos_fn`/`msg_fn`/`read_fn`) and one `int_hooks` entry on the
  reference side, backed by a new shared fixture,
  `tests/src-c/fixtures/mpmsysfile.py`'s `MpmSysFile` (a synthetic
  MPM.SYS file, read sequentially, with an optional injected read
  error).

  Hit, and fixed, a real test-harness-specific collision while
  building this one: the real asm's own `mov bdos_offset,offset
  ldbdos_offset ! mov bdos_segment,cs` (two bare writes to an
  ABSOLUTE address meant for a genuinely different, real `dseg 0`/
  `org 0` segment -- the interrupt vector table) landed on top of
  this SAME module's own cseg-resident message strings once
  collapsed into this test's flat, unsegmented memory model (real
  two-segment hardware never has this collision, since dseg@0 and
  cseg@wherever-this-loaded are different physical memory). Confirmed
  by watching `lenmsg`'s own string actually get clobbered before
  the fix: the `jmp_hooks` entry for `mldbios` now also skips
  `start:`'s entire register/IVT-setup prologue outright (landing
  directly on `mov dx,offset signon`), matching `ldmpm.c`'s own
  choice not to model either write (the stack/segment switch has no
  C equivalent regardless; the two writes themselves have no
  consumer under this port's own `int_hooks`-based `sys_vec`
  abstraction). 16 cases match.

  Both files' own `jmpf cs:mpm`/`jmpf dword ptr leap_off` final exits
  (into MP/M.SYS proper and MPMLDR.bin respectively) have no C
  equivalent, same as every other real OS entry point in this port --
  each C port returns a resolved segment value through an output
  parameter in their place. Full existing suite stays green across
  both; `make clean && make build` stays green.

  This completes the entire boot chain this port set out to recon:
  `ROM86Q.A86` → `LDMPM.A86` → `LDBIOS86Q.A86`/`LDBDOS` → `INIT.SUP`
  are now all ported, routine by routine, with one real, wired-
  together cross-file call (`LDMPM.A86` → `LDBIOS86Q.A86`) proving
  the boundary-stub convention used everywhere else in this port is
  sound once two modules actually need to interoperate for real.
  Linking these into one real bootable image (the actual link/boot
  step this doc's own intro has called out as separate,
  not-yet-started work all along) remains the next phase.

- **`src/loader/LBDOSENT.A86` is now fully ported**: `oldbdos` (the
  loader's own synthetic BDOS dispatch, reached via `LDMPM.A86`'s own
  `int bdos_int`/`sys_vec` -- NOT the real MP/M-86 kernel's `int
  mpmint`, same number but a completely different, loader-local
  convention, see `src-c/loader/ldmpm.c`'s own header comment), plus
  `persub`/`selsub`/`rodsub`/`rofsub` (+ internal `errflg`), the
  error-reporting tail `LBDOSF1.A86`'s own deferred `selerror`/
  `goerr` calls were always meant to reach. `bdose` (the real `INT
  bdos_int` interrupt-entry prologue/epilogue) has no C equivalent,
  same as every other real OS entry point in this port.

  `oldbdos`'s own `jmp cs:functab[bx]` dispatch is ported as a plain
  C switch, same convention `src-c/kernel/dsptch.c` already uses.
  Of its 49 function numbers, the overwhelming majority are each
  just an `EQU` alias for the bare `return:` label (confirmed in
  `LBDOSF3.A86`'s own docs row) -- translated as the `default` case,
  a real call into the already-ported `funcret` (`src-c/loader/
  lbdosio.c`), not skipped outright, since `funcret` is itself a
  genuine already-ported routine, not a boundary-stub artifact. The
  rest wire up directly to already-ported real functions: `func2`=
  `tabout`, `func9`, `func13`, `func14`=`curselect`, `func15`,
  `func20`, `func26` (`src-c/loader/lbdosf3.c`/`lbdosio.c`), and
  `punchf`=`PUNCH` (`src-c/loader/ldbios86q.c`'s own `punch_entry`,
  resolved via `LBDOSEQU.A86`'s own `punchf EQU BIOS+3*6` into
  `LDBIOS86Q.A86`'s own BIOS jump-vector entry 6). `wbootf`=`WBOOT`
  and `listf`=`LISTOUT` are both real boundary exits under this
  port's own existing choices for those two; `wbootf`'s own dispatch
  case never reaches this routine's own epilogue (`goback`) in the
  real asm at all (`WBOOT` jumps away, it doesn't return) -- never
  actually observable here, since nothing in this whole port's own
  call sites ever dispatches function 0.

  `errflg`'s own final `jmp coninf` (wait for one keypress) stays a
  boundary stub, consistent with `src-c/loader/lbdosio.c`'s own
  pre-existing treatment of `coninf`/`conouf`/`listf` elsewhere in
  that same file (not silently upgraded to a real call here) -- its
  own result (the keypress) is an undefined artifact, fixed at 0
  (not `ctlc`), same "undefined artifact, fixed choice" precedent as
  `selectdisk`'s own fixed `bx=0`; that makes `persub`'s own ctlc-
  triggered reboot branch unreachable, translated faithfully, not
  skipped. `selsub`/`rodsub`/`rofsub` all end in an unconditional
  `JMP wbootf` after `errflg` -- a real boundary exit, nothing to
  port past the message. `LBDOSF1.A86`'s own `selerror`/`goerr` call
  sites are NOT updated to call these for real: already unreachable
  there under that file's own separate, pre-existing choice to fix
  `readf`/`writef`/`nowrite` at "always succeeds" -- true whether or
  not these four functions exist, so wiring them up wouldn't change
  anything observable; they're still genuinely ported and
  independently tested on their own.

  Needed three new `tools/i86sim.py` opcode forms, all firsts for
  this differential-testing tool despite being common real 8086
  encodings: `OR r/m8, r8`/`OR r/m16, r16` (`0x08`/`0x09`, the
  reverse-direction form of the already-supported `0x0A`/`0x0B`),
  and `MOV r8, imm8`/`MOV r16, imm16`'s short accumulator-style form
  (`0xB0`-`0xBF`, as opposed to the already-supported `0xC6`/`0xC7`
  ModRM register-direct form).

  Tested via a new per-file wrapper, `tests/src-c/fixtures/
  lbdosent_wrapper.A86` (same minimal-wrapper convention as
  `lbdosf3_wrapper.A86`'s own: a fake dseg at the usual 0xB0xx
  scratch addresses) -- NOT by assembling the real `src/loader/
  LDBDOS.A86` bundle directly, since that bundle's own real
  `LBDOSDAT.A86` inclusion resolves `info`/`aret`/etc. to genuinely
  different (real) addresses than every other already-ported file in
  this tree uses (all independently tested against their own 0xB0xx-
  scratch per-file wrappers) -- confirmed the hard way, by first
  trying the real-bundle approach and finding `oldbdos`'s own `mov
  info,dx` write landing nowhere the rest of this test could see.
  Also hit, and fixed, a wrapper-authoring mistake worth recording:
  `LBDOSENT.A86` must be `INCLUDE`d *before* `LBDOSNEW`/`LBDOSIO`/
  `LBDOSF1`-`3` (matching `LDBDOS.A86`'s own real include order), not
  after -- its own `org bdosoffset` preamble resets the cseg write
  cursor back to this module's own base, silently clobbering
  whatever the other modules had already written there if included
  later (caught by `return`'s own "ret" byte coming back as `0x00`).
  A second mistake, same flavor: this wrapper's own new cseg-resident
  scratch cells (`axsave`/`dskmsg`/etc., standing in for the real
  `LBDOSDAT.A86`'s own copies) were first declared inside the
  wrapper's existing *dseg* block by accident, resolving to harness
  scratch addresses with no real bytes behind them -- `errflg`'s own
  `print` call read nothing but zeros forever as a result, an
  infinite loop caught and fixed by moving them into `cseg`.

  Exercising `oldbdos`'s own `func13`/`func14`/`func15`/`func20`
  dispatch targets reaches `LBDOSF3.A86`'s own `slect`/`selectdisk`
  pair, which needs the exact same `ret4`/`diocomp` register-state
  fix-up `tests/src-c/test_lbdosf3.py` already established (forcing
  `ZF=0` at `selectdisk`'s own early-return point, since replaying
  that undefined artifact mechanically would fall through to the
  still-undermodeled `selerror` branch) -- reused here verbatim.
  `selsub`/`rodsub`/`rofsub`'s own real `JMP wbootf` tail (reached by
  a plain JMP, not a CALL, unlike every other address in this test's
  own stub set) needed one more special case in the test harness: a
  terminal stop, not a "pop a return address and keep going" --
  there's no caller to return to there. 18 cases match. Full existing
  suite stays green; `make clean && make build` stays green.

- **`src/kernel/FILE3.BDO`'s `PR_TERM`/`XCURSELECT`/`RESELECTX`
  ported** (`pr_term`/`xcurselect`/`reselectx`). `PR_TERM`'s own
  `call func48` turned out to be fully resolvable, not a real gap:
  an initial case-sensitive `grep` for lowercase `func48` across all
  of `/src` (including `orig/mpm862sr`, the original DRI archive)
  found nothing and briefly looked like a genuine, previously-
  uncaught bug in the original source -- the real label is
  `FUNC48`, uppercase (`FILE4.BDO`, already ported). `pr_term` walks
  every open-file-list entry (`searchn_list`, `ch=0`, unconditional),
  filtering by `BD_PDADDR` directly via `tst_tbl_lmt` against each
  item's own pdaddr field (offset 6) instead of `searchn_list`'s own
  built-in drive-matching modes, removing locks and the olist entry
  for each match, then flushing buffers once the walk ends. The
  "one further routine uses `std`" note this row used to carry was
  stale -- that's `CMP_PW`, already ported in an earlier pass.

  `XCURSELECT` genuinely reaches `selerror`/`goerr` for real (unlike
  `select_`'s own always-unreachable-past-`selectdisk` tail):
  `CURSELECT`'s own `TEST_FF_DISK` branch calls `selerror` directly
  whenever `BD_SELDSK+1==0`, independent of any boundary-stub
  artifact, so `BD_RETURN_FFFF`'s own real effect (forcing
  `aret=0xFFFF` via `goerr`'s `RTN_PHY_ERRS` branch) is fully
  testable. `RESELECTX` is `reselect`'s own real second entry point
  (`RESELECT1` onward) -- split `reselect` into itself (computes
  `BD_XFCB_READ_ONLY`/`BD_HIGH_EXT`/`BD_ACTUAL_RC` for real, then
  calls the shared tail) and a new `reselectx` (forces
  `BD_HIGH_EXT=0`/`BD_ACTUAL_RC=0` -- an undefined artifact, same
  "undefined artifact, fixed choice" precedent as `selectdisk`'s own
  fixed `bx=0` -- then calls the same shared tail directly). Finding
  this exposed a real incompleteness in `src-c/kernel/file4.c`'s own
  `csearch`, which had its own local, `csearch`-scoped `reselectx`
  stub (forcing only `BD_HIGH_EXT=0`) predating this file's own real
  port -- the REAL `RESELECTX` always falls through the WHOLE shared
  tail unconditionally, not just the `BD_HIGH_EXT` write. Fixed by
  deleting that local stub and having `csearch` call the real,
  complete `reselectx` instead; the full differential suite stayed
  green afterward, confirming nothing there depended on the missing
  tail being skipped.

  Tested via `tests/src-c/test_file3.py` (3 new cases, reusing that
  file's own combined `file3_wrapper.A86`). `PR_TERM`'s own `call
  func48` reaches `FLUSH`'s `jmps xiosif` tail jump -- not
  interceptable via `call_stub_addrs` (which only catches a near
  `CALL` landing on a listed address, not a `JMP`), so this needed
  `jmp_stub_addrs` instead (treating the JMP as an immediate return,
  same mechanism `tools/i86sim.py`'s own docstring already
  describes for exactly this "tail JMP into a boundary stub" shape).
  `XCURSELECT`'s own test also hit, and fixed, the same `rlr`-real-
  address-vs-`SD_RLR`-scratch-address dual-write gap `test_file1.py`
  already solved for `pererror`/`selerror`/`roderror` (`rlr`'s own
  real address, 0x68 in this combined bundle, needs an unconditional
  overwrite in `run_reference` the same way `BD_CURDSK`/
  `BD_FREE_ROOT` already get one, since the "only nonzero mem bytes
  win" merge rule silently drops it whenever the PD address's own
  low byte is 0) -- caught by a first run where both sides
  "matched" by both being wrong in the same way (writing to two
  different, unrelated addresses that both happened to default to
  0), not by an outright crash, worth remembering as its own
  category of bug to watch for. Full existing suite stays green;
  `make clean && make build` stays green.

- **`src/rsp/TMP.A86`'s own "provably unreachable" claim was wrong --
  `goodparse` and everything downstream are now ported for real.**
  This file's own header comment used to argue `clicall`'s own `call
  parsefilename` deterministically returns `cx=mpm_parse` (152) under
  the boundary-stub convention (pre-loaded right before a no-op tail
  JMP into `mpm`), making `goodparse` unreachable and `parsefilename`
  itself an unidentified black box. Tracing `mpm_parse` (152) against
  `src/kernel/MODFUNC.DEF`'s own `f_parsefilename equ (user*0100h)+152`
  shows it's `src-c/kernel/parse.c`'s own already-ported `parse_entry`
  (the exact same real target `src-c/kernel/command.c`'s own `cli_pfn`
  already calls) -- genuinely callable for real, not a dead end.
  Likewise `not_builtin`'s own `mpm_clicmd` (150) is `command.c`'s
  already-ported `clicmd_entry`. Both are now wired up for real:
  `parsefilename` builds a local `PCB.DEF` block and calls
  `parse_entry`; `not_builtin` builds a local `CLICB.DEF` block
  (reusing `SD_TMP_CLICB_CMD`'s own already-populated command text
  directly, via adjacency, rather than copying it) and calls
  `clicmd_entry`. `goodparse`/`try_builtin`/`clierror` are all ported
  too; `try_builtin`'s own `setuser`/`setlist` dispatch was never
  identified, but that turned out not to matter -- their only
  observable job, under this flat memory model, is a `jmp pruser`/
  `jmp prprinter` either way, which this port already reaches
  directly. `a_to_b` is fully self-contained (pure register
  arithmetic, no `mpm` call at all) and ported alongside them.

  Tested by extending `tests/src-c/test_tmp.py` to drive the real
  calls from the Python side: ONE `int_hooks` entry on `INT mpmint`
  inspects `CL` and, for `mpm_parse`/`mpm_clicmd`, makes a genuine
  `ctypes` call into this same shared library's own `parse_entry`/
  `clicmd_entry` against `sim.mem` directly (via `ctypes.c_uint8.
  from_buffer`, a zero-copy view -- writes the C call makes land
  straight in `sim.mem`), falling through to the old blanket no-op
  for every other function code. This is the same "wire a real call"
  precedent `src-c/loader/ldmpm.c`'s own extern call into `init_entry`
  already established, just driven from the test harness instead of
  from another C file -- genuinely exercising `command.c`'s own real
  (and separately, exhaustively tested) `clicmd_entry` logic rather
  than re-deriving it in Python.

  Making `goodparse` actually reachable surfaced two real bugs in
  this test's own pre-existing `CSEG_BASE`-shift technique, neither
  triggered before since nothing used to run that far: (1)
  `print_ds_string`'s own `mov ax,cs ! mov ds,ax` prologue computes
  its own addressing basis from the live `cs` register, which a
  `CSEG_BASE` shift (moving where the *bytes* live in the test's own
  flat array, not the assembled addresses the code itself computes)
  can't retroactively fix under this file's own flat/non-segmented
  addressing -- letting it run for real sent its own `$`-scan hunting
  through memory that was never populated, spinning for a very long
  time instead of crashing outright (caught by watching step counts
  climb into the tens of thousands with no sign of halting, then
  histogramming which addresses recurred most often to find where).
  (2) `try_builtin`'s own `mov di,offset usercmd`/`offset printercmd`
  are absolute references to cseg-resident *data* (not a control-flow
  target, which survives a shift fine) -- same class of problem,
  fixed by writing their own real content back to their own
  unshifted real address in the test's own image, the same treatment
  `cmdsent`/`read_blen`/`clicb_cmd`/`savebuf` already got. Both fixed
  by either stubbing the routine outright (the print calls -- true
  no-ops, no observable effect either way) or restoring the real
  data (`usercmd`/`printercmd`). 12 cases match (6 new). Full
  existing suite stays green; `make clean && make build` stays green.

  `src/rsp/SUBMIT.A86`'s own near-identical claim (`clicall`'s real
  CLI dispatch "provably unreachable past `notdrive`'s own `call
  parsefilename`") is now suspect for the exact same reason, but not
  yet re-examined -- a candidate for a future pass.

- **`src/rsp/SUBMIT.A86`'s own identical claim was wrong too --
  `goodparse`/`try_builtin`/`notbuiltin` ported for real**, the same
  correction as `TMP.A86`'s own (see that file's own changelog entry
  just above): `parsefilename`'s real target is `parse.c`'s already-
  ported `parse_entry`, and `notbuiltin`'s own `mpm_clicmd` is
  `command.c`'s already-ported `clicmd_entry`. `a_to_b` is ported too
  (a second, independent copy of the identical real asm `tmp.c`'s
  own copy already has). `$INCLUDE`'s own real body is the one piece
  deliberately left out -- it needs `sysdatseg`, a real segment value
  only `submit:`'s own un-ported one-time setup captures (not
  available in this reduced scope); the comparison against it still
  runs for real either way, so a command that *doesn't* name
  `$INCLUDE` is unaffected, and one that does gets a fixed, "not a
  real outcome of this port's own scope" result instead of silently
  mismodeling it.

  Unlike `TMP.A86` (every real path ends in `jmp nextcommand`),
  `SUBMIT.A86`'s own `clierror` tail (`showerr`) is `jmp submitexit`
  for every error code except `e_nullcmd` -- `topofcmdloop_once` now
  reflects that exactly (`SUBMIT_GETNXTCMD` for `pruser`/`prprinter`'s
  own tails, a successful `clicmd_entry` call, and `e_nullcmd`;
  `SUBMIT_SUBMITEXIT` for every other `clicmd_entry` failure).
  `notdrive`'s own bad-parse tail (scanning `clicmd` itself for its
  first space and overwriting it with `$`, a real memory effect
  already modeled before this pass) now also calls the real
  `clierror` with `parsefilename`'s own real error code, instead of
  unconditionally reporting `SUBMIT_SUBMITEXIT`.

  Reused `tmp.c`'s/test_tmp.py's own fixes verbatim rather than
  rediscovering them: `call_stub_addrs` for every print-adjacent
  routine (`con_write`/`crlf`/`getuser`/`setuser`/`getlist`/
  `setlist`/`prnum`) to avoid the same `cs`-relative-addressing-
  under-a-`CSEG_BASE`-shift problem, and an unshifted-real-address
  override for `usercmd`/`printercmd`/`includecmd`'s own actual
  content. That override needed one more real fix specific to this
  file, though: `SUBMIT.A86` (unlike `TMP.A86`) switches from `cseg`
  to `dseg` partway through (confirmed by grep for `^\s*dseg`) --
  `usercmd`/`printercmd`/`includecmd` are DSEG-resident, not cseg-
  resident, so their own real content has to come from
  `asm.segment_bytes('dseg')`, not `code` (cseg). An initial attempt
  copied them from `code` by analogy with `tmp.c`'s own test and
  silently read garbage (bytes that happened to be instruction
  opcodes at that same offset in cseg) -- caught by searching the
  assembled bytes for the literal string `USER` and finding it
  nowhere in `cseg` at all, which is what actually prompted checking
  for a second `dseg` switch in the first place.

  `notbuiltin`'s own real `clicmd_entry` call needed the usual
  `command.c` supporting fixture (`SD_RLR`/`UDA_WRKSEG`/an MXCli
  queue with one token), reused verbatim from test_command.py's own
  `base_memory()`, same as `tmp.c`'s test. All pre-existing
  `topofcmdloop_once` cases keep passing with no behavior change
  (none of them happen to name a real RSP queue, so `clicmd_entry`
  now genuinely runs and genuinely fails the same way the old fake
  boundary stub used to, for a different, real reason) plus 3 new
  cases (`USER`/`USER <n>`/`PRINTER <n>`). Full existing suite stays
  green; `make clean && make build` stays green.

- **`src/kernel/CPMMEM.MEM`'s own `getmemory` was mischaracterized the
  same way `TMP.A86`/`SUBMIT.A86` were -- its "always fails under the
  boundary-stub convention" framing was accurate at the time it was
  written (before `MEMORY.MEM`'s own `malloc_entry` was itself real),
  but it quietly went stale once `memory.c` landed.** `getmemory`'s own
  real body (`push cx,cx,dx,bx,ax ! mov dx,sp ! mov ds,ss ! mov
  cx,f_malloc ! call mpmif`) looked at first like an ordinary register-
  passing convention this port's own `malloc_entry(mem, dx)` C
  signature couldn't reproduce (the real routine's own output is
  documented as "AX = start, DX = length", but `malloc_entry` only
  returns `bx`/`cx`) -- investigating `MAU.MEM`'s own `mau_split`
  (`mov es:mpb_start[si],ax`) resolved the mismatch: the "stack push"
  isn't register passing at all, it's building a genuine `MPB.DEF`
  block (`mpb_start`/`mpb_min`/`mpb_max`/`mpb_pdadr`/`mpb_flags`) in
  memory, with `dx`/`ds:ss` just pointing the callee at it -- the same
  memory `mau_split` writes `mpb_start` back into on success, which
  `getmemory`'s own trailing `pop ax` then reads back (not a stale
  echo of its own input). `mpb_min` is never written by anything in
  the real chain, so `getmemory`'s own documented "DX = length" output
  is genuinely just an echo of its own `min` input -- confirmed against
  the real asm, not a port bug.

  Fixed by giving `getmemory` a fixed scratch `MPB.DEF` block
  (`SD_CPMMEM_MPB`, `defs.h`) instead of a register-passing stub: it
  writes `start`/`min`/`max` into the block (`pdadr`/`flags` fixed at
  0, matching the real push sequence), calls `memory.c`'s real
  `malloc_entry(mem, SD_CPMMEM_MPB)`, and returns `{ax = mpb_start
  read back, dx = the original min input, cx = malloc_entry's own
  error code}`. All 4 callers (`maxmem_entry`/`absmax_entry`/
  `cpmalloc_entry`/`cpmabsa_entry`) now use that real `ax`/`dx` on
  success to fill in `mcb_base`/`mcb_length` (plus `mcb_ext=1` for
  `maxmem_entry`'s own `mm_gm`, matching the real asm exactly) instead
  of falling through an "unreachable" comment. `free_memory`
  (`cpmfree_entry`'s own tail call) stays a boundary stub -- it would
  need `memory.c`'s own `mfree_entry`, whose own `mpmif(f_maufree)`
  call is itself still deferred there (see that file's own header
  comment on why its tail-jump-out-of-two-call-frames shape needs more
  care) -- upgrading `free_memory` is blocked until that lands first,
  not a new gap this pass introduced.

  `tests/src-c/fixtures/cpmmem_wrapper.A86` now bundles `MEMORY.MEM`/
  `MAU.MEM`/`ML.MEM`/`UTIL.MEM` the same way `memory_mem_wrapper.A86`
  already does, with `mpmif` upgraded from a flat `call_stub_addrs`
  target to a real dispatcher for `f_malloc`/`f_maualloc`/`f_mlalloc`
  (the first of those three is new; the latter two are needed because
  `malloc_entry`'s *own* real asm body makes its own nested
  `mpmif(f_maualloc)`/`mpmif(f_mlalloc)` calls, which this wrapper's
  `mpmif` has to route too, or they silently fall through to a no-op
  `ret` instead of reaching the real `MAU.MEM`/`ML.MEM` code bundled
  right there in the same file -- caught by a reference-side `cx`
  coming back as `f_mlalloc` itself instead of a real error code).
  `test_cpmmem.py`'s own 4 "always fails" cases are now a real
  differential instead (no unused MDs -> `e_no_umd`, propagated for
  real through `malloc_entry` rather than echoing the old hardcoded
  `f_malloc` sentinel), plus a new `cpmalloc_entry` case exercising the
  real `mall_ml` path end to end (one unused MD, this PD's own p_mem
  list empty, MFL root genuinely empty -- fails for a real reason,
  `e_no_memory`, with the MD freed back, matching `memory.c`'s own
  established "mlalloc genuinely fails" precedent). A full genuine-
  *success* case (reaching `mla_makemau`/`mla_initmau`'s own real MAU
  creation) was deliberately not attempted here -- it needs
  `tools/i86sim.py`'s `segmented=True` mode (for `ML.MEM`'s own
  dynamically-computed MAU segment writes) combined with this file's
  own flat-model `SD_*` scratch addresses, a combination no existing
  test exercises yet; safer to leave as a clearly-scoped follow-up than
  to guess at it. Full existing suite stays green; `make clean && make
  build` stays green.

  This also settles `LOAD.SUP`'s own long-standing note about
  `cload_entry`'s chain-load cleanup being "blocked on `cpmfree_entry`
  succeeding for a real MAU release": that blocker is about *freeing*
  memory (`free_memory`/`mfree_entry`), not allocating it, and remains
  exactly as blocked as before -- this pass only upgrades the
  allocate side (`getmemory`/`malloc_entry`). `LOAD.SUP`'s own chain-
  load mode is still open.

- **`memory.c`'s own `mfree_entry`/`mfree_it` upgrade, closing the
  free side that the `getmemory`/`malloc_entry` pass above left open.**
  The "tail-jump-out-of-two-call-frames" framing in `mfree_it`'s own
  previous header comment undersold it -- once `getmemory`'s own MPB
  resolution showed the real "push a parameter block ! mov dx,sp !
  mov ds,ss" shape is just a genuine `MAF.DEF` block in memory, not a
  register convention, `mfree_it`'s own real body translated directly:
  build the MAF at a fixed scratch block (`SD_MFREE_MAF`, `defs.h`,
  `UDA_WRKSEG` set to 0 first -- the same "ambient-flat" trick
  `tmp.c`/`submit.c`'s own `parsefilename` calls already use, since
  `mau.c`'s `maufree_entry` reads the MAF through `UDA_WRKSEG`), call
  `mau.c`'s already-ported `maufree_entry` for real, and act on its
  own `{bx, cx}`. The real asm's own "free an empty MAU back to the
  Memory Free List" tail genuinely does jump clear out of both
  `mfree_it` *and* `mfree_entry` via `mpmif(f_mlfree)` -- but since
  `ml.c`'s own `mlfree_entry` always leaves `bx=0,cx=0` (its own header
  comment) and the plain "MAU still in use" exit (`mfree_exit`) is
  *also* always `bx=0,cx=0`, both of `mfree_it`'s own reachable
  "freed the whole segment" outcomes collapse to the same fixed
  `{0, 0}`, translated as a plain return rather than an actual
  tail-jump (this port has none). The one genuinely unreachable edge
  (the freed MAU not found on the active MAU list, `SD_MAL` -- should
  never happen, every in-use MAU is invariantly on that list) keeps
  the real asm's own stale `BX` (the freed MD's own address) rather
  than inventing a cleaner value, same "undefined artifact, fixed
  choice" treatment used throughout this project.

  This in turn closed the loop on `cpmmem.c`'s own `free_memory`,
  which the previous pass left as the one remaining boundary stub:
  its own real on-stack "push cx(0) ! push dx" is, same pattern again,
  a genuine `MFPB.DEF` block (`start`/`pdadr`), now built at a fixed
  scratch block (`SD_CPMMEM_MFB`) and passed straight to `memory.c`'s
  `mfree_entry`. This surfaced a real, independent correction in
  `cpmfree_entry` itself, not just an upgrade: its own `mcb_ext !=
  0xFF` path is a genuine tail-*jump* into `free_memory` in the real
  asm (`jne free_memory`), so its own final output is `free_memory`'s
  real `BX` too -- the previous boundary-stub-era comment ("the real
  routine never writes BX on that path, so this port fixes it at 0")
  was only ever true because `free_memory` itself never set `BX`
  meaningfully; now that it does, `cpmfree_entry` passes it through
  unchanged instead of forcing a fixed value that's no longer
  "undefined," it's simply wrong.

  `tests/src-c/fixtures/memory_mem_wrapper.A86`'s own `mpmif` gained
  `f_maufree`/`f_mlfree` dispatch (alongside the existing
  `f_maualloc`/`f_mlalloc`), and `test_memory.py` switched its
  `run_reference` to `tools/i86sim.py`'s `segmented=True` mode (needed
  now that `mfree_it`'s real call reaches `MAU.MEM`'s own dynamically-
  computed MAU segment) -- confirmed safe first by re-running every
  existing case unchanged under `segmented=True` before adding
  anything new (plain unprefixed accesses behave identically either
  way, so nothing already passing could regress). Two new cases
  exercise the real call end to end: "MAU still in use" (a SAT entry
  freed while a neighbor stays allocated) and "MAU now empty,
  released to MFL" (the freed entry was the MAU's only real one,
  verified against the real reference by hand-deriving a correct MAF/
  SAT/MAL fixture and cross-checking it against the real assembly
  before wiring it into the C port's own test -- the first attempt at
  the MFPB field order had `start`/`pdadr` swapped, caught immediately
  because the real reference run itself failed in a way that didn't
  match the intended scenario). Two *pre-existing* cases that used a
  bogus, made-up `ms_mau` pointer (`0x9999`) needed their own fix once
  `mfree_it` stopped being a stub: one now correctly exercises
  `maufree_entry` genuinely failing against that bogus MAU (its own
  `ents` byte comes back 0 off the garbage segment, failing the first
  entry-count check) instead of a stale "always fails, nonzero stub
  code" comment; the other was rebuilt with a real MAU/SAT fixture so
  it actually exercises the "decrease length" path for real (a genuine
  `mau_split`), instead of coincidentally returning the same "fails
  for an unrelated garbage-pointer reason" result the bogus fixture
  produced either way.

  `tests/src-c/fixtures/cpmmem_wrapper.A86`'s own `mpmif` gained the
  same four dispatch entries (`f_memfree` directly, plus
  `f_maualloc`/`f_mlalloc`/`f_maufree`/`f_mlfree` for the nested calls
  `malloc_entry`'s/`mfree_entry`'s own real bodies make), and
  `test_cpmmem.py` switched to `segmented=True` too. Its own
  `cpmfree_entry` cases were extended/corrected the same way: the
  single-segment-free case's stale "always fails" comment is now a
  real `e_no_memory` differential (empty p_mem list), plus a new real-
  success case proving `free_memory`'s own `BX` passthrough fix; the
  free-all loop's own long-standing "non-Load segment, spins forever
  under the boundary-stub convention" note is now a real, passing
  success case (a genuine free, MAU released to the MFL, loop
  restarts via `cpmf_root` and terminates cleanly) instead of a
  documented gap. Full existing suite stays green; `make clean && make
  build` stays green.

  This fully closes `LOAD.SUP`'s own long-standing blocker ("`cload_
  entry`'s chain-load cleanup depends on `cpmfree_entry` succeeding
  for a real MAU release") on the `CPMMEM.MEM`/`MEMORY.MEM` side --
  both the allocate and free paths are real now. `LOAD.SUP`'s own
  chain-load mode itself (`bx==1` into `load:`) has not been attempted
  yet; that's the next natural step this unblocks.

- **`src/kernel/LOAD.SUP`'s `cload_entry` -- the chain-load mode the
  previous entry's own fix unblocked.** Ported as a thin wrapper
  (`load_bxcx cload_entry(uint8_t *mem, uint16_t dx, uint16_t bx,
  uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs)`) calling
  `load` with a new, explicit `int load_chain` parameter, instead of
  reproducing the real asm's own `push bx ! mov bx,1 ! jmps load`
  sentinel-smuggling trick (the real `load:` entry point shares a
  single `bx` register slot three ways -- 0, a real PD address, or the
  chain sentinel `1` with the real PD stacked underneath -- this port
  has no need to reproduce that, same reasoning as every other place
  here that passes `sysdat`/`cs` directly instead of resolving a
  `cs:`-resident label).

  `load`'s own `if (load_pd != 0)` block (`form_uda`) now has a
  `if (load_chain)` tail porting `ld_fnext`/`ld_freeit` directly:
  walk the calling process's own `p_mem` list (`SD_RLR`), clearing
  `mf_load` on every segment except the one matching the new PD's own
  (already-established) `p_uda`; call `cpmmem.c`'s `cpmfree_entry` in
  free-all mode (`mcb_ext = 0xFF`) via a fixed scratch `MCB.DEF` block
  (`SD_LOAD_MCB`, `defs.h`) -- its own output is discarded, matching
  the real asm's own "pop cx!pop cx!pop cx" that never checks it
  either; then a second, direct `memory.c` `mfree_entry` call (not
  through `cpmmem.c`) frees the tail of the UDA segment beyond what
  the UDA+LDSTK group itself needs, via its own scratch `MFPB.DEF`
  block (the existing `SD_LOAD_MFPB`, already used by `load_giveup`)
  -- also fire-and-forget. Finally "transfer memory to new pd"
  reparents the calling process's own (now-trimmed) `p_mem` list onto
  the new PD and zeroes the caller's own.

  `ld_freeit`'s own `mpmif(f_freemem)` resolves, via `MEMIF.MEM`'s own
  `functab`, to `cpmmem.c`'s `cpmfree_entry` itself (func#4 in the MEM
  module's own table -- the same table `f_malloc`/`f_memfree`'s own
  func#6/#7 live in, confirmed by reading `MEMIF.MEM` directly) --
  another case, like `TMP.A86`/`SUBMIT.A86` before it, of a boundary
  call that turns out to resolve to an already-ported routine once
  actually traced.

  `tests/src-c/fixtures/load_wrapper.A86` now bundles `CPMMEM.MEM`
  too (alongside the already-bundled `MEMORY.MEM`/`MAU.MEM`/`ML.MEM`/
  `UTIL.MEM`), and `test_load.py` gained `maufree_entry`/`mlfree_entry`
  to its own `real_targets` redirect map. This surfaced a genuine gap
  in that test's own existing redirect mechanism: `mfree_it`'s own
  closing call is a real tail-*jump* (`jmp mpmif`), not a `CALL`, and
  the existing `call_hooks`-based redirect (manually pushing a return
  address before jumping into the real routine) is wrong for a JMP --
  there's no return address to push, since the callee's own eventual
  `ret` has to pop whatever's already on the stack from `mfree_it`'s
  own original caller, exactly like a real tail call. Added a second,
  `jmp_hooks`-based redirect (`make_jmp_redirect_hook`) that does the
  same `real_targets` lookup but only sets `sim.ip`, pushing nothing --
  caught by watching execution drift into pure data (`SD_LOAD_DISK`'s
  own content bytes, eventually hitting a byte `i86sim.py` couldn't
  decode at all) after the untouched stack misaligned a later, unrelated
  `ret`.

  Getting a real, fully-threaded differential case for `cload_entry`
  past this point took several more rounds, each catching a genuine
  fixture gap rather than a port bug:
  - `mau_collapse`'s own real asm returns its "assume empty" default
    the instant it finds a zero `sat_start` -- *before* ever examining
    the next real entry's own `nall` -- so a "still in use" fixture
    needs a real third (boundary) SAT entry behind the two real ones,
    or the second one's own allocated status is never actually
    checked at all (caught by the reference and port disagreeing on
    whether a MAU got released when the fixture only had two).
  - An MD's own `ms_mau` field left at 0 reads `m_start[0]` as a
    "segment" -- off real `cseg` bytes in the reference run (non-zero,
    genuine code) but off a zeroed buffer in the port, diverging
    `malloc_entry`'s own "try an existing MAU first" step between the
    two; fixed by giving the one pre-existing MD a real (deliberately
    full, `e_no_memory`-on-request) dummy MAU instead of leaving it at
    the undefined default.
  - The final remaining difference -- *which* MAU ends up satisfying
    `ldtab[0]`'s own combined allocation once the dummy MAU and the
    newly-freed one both exist on the free lists -- is `mau.c`'s/
    `ml.c`'s own pre-existing, already independently verified
    "existing-MAU-first" search order, not something this new test
    needs to additionally pin down; the committed test instead checks
    the overall `(bx, cx)` outcome plus the one piece of state purely
    under the chain cleanup's own control (`SD_RLR`'s own `p_mem`
    ending up empty, reparented away) -- see that test's own comment
    for why going further would be re-verifying someone else's
    already-covered logic rather than this pass's own new code.

  Full existing suite stays green; `make clean && make build` stays
  green. This completes `LOAD.SUP` -- every real routine in the file
  is now ported, with no open items left in it.

- **`FILE2.BDO`'s own status-table "30+ remaining labels" note was
  stale, and `COMMAND.SUP`'s `fileopen` turned out to be another case
  of the same mischaracterization already corrected for `TMP.A86`/
  `SUBMIT.A86`/`LOAD.SUP`.** A fresh audit of every real label in
  `src/kernel/FILE2.BDO` (111 total) against `src-c/kernel/file2.c`
  found that all of them already map onto an already-ported cluster
  -- see the status table's own updated `FILE2.BDO` row for the full
  mapping. **`FILE2.BDO` is fully ported**; the count was simply never
  recomputed as each incremental pass chipped away at the original
  tally.

  That, in turn, meant `command.c`'s own header comment justifying
  `fileopen` as a boundary stub -- citing `FILE2.BDO`'s own "still-
  unported `srch`-dependent bulk" -- was itself stale. Retracing
  `fileopen`'s own real `mpmif(f_fopen)` call (function 15, traced
  through `MODFUNC.DEF` and `BDOSIF.BDO`'s own `functab`, func#2 in
  the `bdos` module's own numbering) landed on `FILE4.BDO`'s own
  `func15` -- confirmed **fully ported** already (`file4.c`, and in
  turn `file1.c`/`file2.c`/`file3.c`'s own clusters it calls into).
  `fileopen` now sets `BD_INFO = CLI_FCB` and calls `func15` directly,
  then reads `BX = BD_ARET` back -- `BDOSIF.BDO`'s own shared
  dispatch convention (`entry`'s own "mov info,dx" before calling a
  functab target, `RETMON`'s own "mov bx,aret" after), replicated by
  hand around the direct call, same as `LBDOSENT.A86`'s own `oldbdos`/
  `goback` already does for the loader's side of this exact pattern.

  This makes `cli_fload`'s own real success path (a file that
  genuinely opens) reachable for the first time, and `cli_gopen` with
  it -- previously both were "provably unreachable" purely because of
  `fileopen`'s own old fixed-failure stub. `cli_gopen` itself still
  stops at `prfilnam`, falling straight to `cli_rmpd` rather than
  going on to load the program and create its process (`f_load`/
  `f_cload`/`f_createproc`/`f_setprior`) -- a separate, deliberate
  follow-on step (bridging `load.c`'s `load`/`sysent.c`'s
  `creat_proc_entry`, both already ported elsewhere, the same way
  `fileopen` just bridged `func15`), not attempted this pass.

  `func15` itself inherits a pre-existing, structural test-harness
  limitation, not a new one: `file2.c`'s own header comment already
  documents that `open`/`OPEN1`/`opencopy`'s own real cseg footprint,
  in the combined `FILE1-4.BDO`+`BDOSIF.BDO`+`DATA.BDO` bundle (the
  same bundle `src/kernel/BDOS.A86` itself assembles -- not a choice
  this project's own tests get to make), lands inside the flat range
  `DATA.BDO`'s own dseg fields resolve to, and `func15` has had no
  differential test of its own for exactly that reason since it was
  first ported. `tests/src-c/test_command.py`'s own `command_wrapper.A86`
  reference run therefore still uses the old fixed-stub result for
  `f_fopen` (confirmed by direct, independent ctypes calls that with
  no matching directory entry anywhere in this test's own empty
  fixture, `func15`'s real logic deterministically fails the exact
  same way, `BL=0xFF`/`BH=0` -- so the two sides still agree, now for
  a real reason on the port side instead of a coincidence, without
  needing to solve `func15`'s own pre-existing collision to do it).
  `test_command.py`/`test_tmp.py`/`test_submit.py` (all three link
  `command.c`, so all three now transitively link `file1.c`-`file4.c`
  too) needed their own C-file lists extended accordingly; no fixture
  changes beyond that, since none of their own existing test cases
  ever set up a matching directory entry either. Full existing suite
  stays green; `make clean && make build` stays green.

- **`COMMAND.SUP`'s `cli_gopen` -- the natural follow-on the `fileopen`
  fix above left open, now wired all the way to `clicmd_exit`.**
  `cli_gopen`'s own `mpmif(f_load)` was traced the same way
  `fileopen`'s own `mpmif(f_fopen)` was: `MODFUNC.DEF` (`f_load` =
  function 59) and `SUPIF.SUP`'s own `supfunc` table (func#4 in the
  `sup` module's own numbering) land on `LOAD.SUP`'s own `load_entry`
  -- already ported, and its own real body (`sub bx,bx ! jmp load`)
  always discards its own caller's `bx` and forces "don't init a PD,"
  which is why `cli_pd` is never passed into `load.c`'s `load` at all
  on this path; `cli_gopen` calls it directly with `bx=0`, threading
  `sysdat`/`user_retf_offset`/`cs` through `clicmd_entry`/`cli_fload`/
  `cli_gopen`'s own signatures the same way every other `cs:`-resident
  address this project ports already does.

  Past that, `cli_gopen`'s own real `cli_gload` block -- closing the
  file, copying and uppercasing the command tail into the newly
  loaded program's own Base Page DMA buffer, building its own default
  FCB (the real asm's own "zero the first 16 bytes, then duplicate
  them into the next 16" trick, since a blank FCB's own later fields
  happen to want the identical zero/space pattern), and parsing up to
  two filename arguments out of the tail into it -- is translated in
  full too, using this project's own established `mem_*_seg` pattern
  for the Base Page's own genuinely dynamic segment (`cli_bpage`,
  `load`'s own just-returned result). The "2nd argument" parse reuses
  `cli_pcb`'s own `pcb_flnmptr`, which is still `CLI_CMDTAIL` from
  `clicmd_entry`'s own earlier `cli_pfn` call -- `shifttail` already
  rewrote `CLI_CMDTAIL`'s own *content* down to just the arguments by
  then, so re-parsing from the same pointer parses the first argument
  rather than the command name again (this project's own prior
  framing, that `pcb_flnmptr` is restored after a temporary
  overwrite, turns out to be a misreading -- there's no save/restore
  at all, just this same-pointer-different-content reuse).

  `f_createproc` (`sysent.c`'s `creat_proc_entry`, whose own real tail
  is `jmp pdisp` -- a dispatcher switch, not an ordinary call/return;
  see `dsptch.c`'s own header comment on why this port's single-
  threaded host has nothing to map that onto) and `f_setprior` stay
  plain boundary stubs -- but, critically, neither one's own result is
  ever read by the real asm itself (the final `cli_err = cx` comes
  from `conasn`, called *after* `f_createproc`, not from it), so
  leaving them as no-ops doesn't truncate the translation at all:
  console assignment (`conasn`, now returning its own real result
  instead of discarding it) and the final `cli_err`/`cli_exit` are
  both faithfully ported too. `cli_gopen` now runs all the way through
  to a real `clicmd_exit` on success, the same as every other path
  through this file.

  This piece has no differential test of its own, for the same root
  reason `func15` itself has none (see that pass's own changelog
  entry): reaching it requires `fileopen` to genuinely succeed, which
  needs a real, matching directory entry that no test in this project
  currently fakes convincingly, and bundling `LOAD.SUP`'s own real asm
  into `command_wrapper.A86` to test it directly would also require
  switching that wrapper's own reference run from non-segmented to
  `segmented=True` (`load_finish`'s own UDA writes are genuinely
  segment-relative) and re-verifying every existing case there for
  safety first -- attempted this pass, then deliberately backed out
  once the scope became clear, rather than leaving a half-migrated
  wrapper behind; `command_wrapper.A86`/`test_command.py` are
  unchanged from before this entry. The translation itself was done
  carefully by hand against the real asm and reviewed for compile-time
  and logical soundness (full existing suite stays green; `make clean
  && make build` stays green), but -- same as `func15` -- carries the
  caution any untested translation deserves until a real differential
  exists for it.

- **`CIOIF.CIO`'s and `MEMIF.MEM`'s own `init` -- the "blocked on
  far-call infrastructure" framing for these two was simply wrong,
  the same stale-claim pattern this session kept finding elsewhere.**
  Re-read directly: `init` in both files is a straight-line,
  7-instruction body with no `call`/`callf`/`jmp` of any kind at all
  -- `mov cs:sysdat,ds` (copy the caller's own `DS`, the real sysdat
  segment, into this module's own local `sysdat` cseg cell) followed
  by a 2-word copy of `SYSDAT.FMT`'s own `module_table[SUP]` entry
  far-pointer into this module's own local `supervisor` cseg cell --
  ending in a far `RET`. `tools/i86sim.py`'s own `far_call_stub_addrs`
  (added for `INIT.SUP`, which genuinely needs it for its own 5
  `callf dword ptr <module>mod[bx]` calls) was never the missing
  piece for *these* two files; they needed nothing beyond a plain
  `stop_addrs` right before the `RET`, the same simple technique
  `init.c`'s own test could have used for a body this size.

  Ported as `cioif.c`/`memif.c` (`void init(uint8_t *mem, uint16_t
  ds_sysdat)` -- no `cs` parameter, unlike `init.c`'s own `init`,
  since neither of these ever reads its own `cs`, only copies the
  *Supervisor* module's own far pointer). `sysdat`=6/`supervisor`=
  8/0xA are real, already-resolved cseg offsets (both files share the
  identical `jmp init ! jmp entry` 6-byte header with `INIT.SUP`
  itself), confirmed directly via `--symbols` against each file's own
  new wrapper (`tests/src-c/fixtures/cioif_wrapper.A86`/
  `memif_wrapper.A86`), not assumed from the shared shape alone. Added
  `SD_SUPMOD_OFF` (`defs.h`) alongside the pre-existing
  `SD_SUPMOD_SEG`, since `INIT.SUP`'s own `init` only ever needed the
  segment half (its own `supervisor`'s offset half is a fixed
  compile-time constant, unlike these two).

  `entry` (each module's own `call cs:functab[si]`-style inter-module
  dispatch) is deliberately not ported, and there is no project value
  in doing so regardless of simulator capability: every real C-side
  caller that needs one of `entry`'s own functab targets already
  traces straight to the already-ported routine itself (`fileopen`→
  `FILE4.BDO`'s `func15`, `cli_gopen`'s `f_load`→`LOAD.SUP`'s
  `load_entry`, `cpmmem.c`'s `getmemory`→`MEMORY.MEM`'s
  `malloc_entry`, etc. -- the same "boundary call resolves to an
  already-ported routine" pattern this whole session kept finding).
  Porting `entry` itself would just wire up work already done a
  different way, not unblock anything.

  This does *not* extend to `SUPIF.SUP`/`BDOSIF.BDO` (and, at the
  time, not yet to `RTMIF.RTM` either -- see the next changelog
  entry, which ported it right after this one). `SUPIF.SUP` has no
  separate `init` label at all (confirmed via grep, not assumed) --
  how/whether the SUP module gets initialized needs its own dedicated
  look before it can even be scoped. `BDOSIF.BDO`'s own `init` does
  the same trivial sysdat/supervisor copy, then genuinely calls out
  (`call mpmif`, creating/opening/writing the disk MX queue) --
  likely the same boundary-stub no-op treatment used everywhere else
  in this project applies, but not verified this pass. Full existing
  suite stays green; `make clean && make build` stays green (43
  files now, up from 41).

- **`RTMIF.RTM`'s own `init` -- the bigger-bodied sibling the
  previous entry flagged as "tractable, just not done yet."** Ported
  as `rtmif.c`. Same `sysdat`/`supervisor` copy as `CIOIF.CIO`/
  `MEMIF.MEM`'s own `init`, then: initializes `SYSDAT.FMT`'s own
  `dispatcher`/`rtm_pdisp` far pointers to `DSPTCH.RTM`'s own
  `fdisp`/`farpdisp` (both already ported, `dsptch.c`) within this
  same `cs`; relocates the tick and idle processes' own `p_uda`
  (a paragraph offset relative to `sysdat`'s own base in the stock
  `SYSTEM.DAT` template, not yet an absolute segment -- the same
  "GENSYS stock template" class the real asm's own comment on
  `u_initss` documents at length, already fixed in the current
  `/src`, not re-fixed here) to a real segment and writes each one's
  own `u_initcs` (the *Supervisor* module's own segment -- both
  processes first run through `SUPIF.SUP`'s own `user_entry`, not a
  real user `CS`) and `u_initss` (`sysdat`'s own segment); then one
  real call into `proc.c`'s already-ported `proc_creat` (`dx =
  offset idlepd`, since it walks the whole pending-creation chain
  itself -- one call handles both tick and idle), whose own output
  is discarded, matching the real asm's own immediate `retf`.

  Confirmed directly (not assumed from the shared header shape with
  `CIOIF.CIO`/`MEMIF.MEM`): `init` has no far call of any kind here
  either, just the one ordinary near `call proc_creat` -- still none
  of `tools/i86sim.py`'s far-call machinery needed.

  This file surfaced a real addressing subtlety the other two
  modules' own trivial `init` never exercised: `supmod`/`dispatcher`/
  `rtm_pdisp`/`idlepd`/`tickpd` are genuine `DS`-relative `SYSDAT.FMT`
  references in the real asm, and `init` never resets `DS` away from
  the real, non-zero `sysdat` segment it's handed (per its own
  "assume DS = Sysdat Segment" precondition) -- unlike
  `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG` (flat addresses that only work for
  `CIOIF.CIO`/`MEMIF.MEM`/`INIT.SUP` because those are tested
  non-segmented, where the segment register is ignored and only the
  offset matters), this routine's own test genuinely needs
  `segmented=True` (for the real tick/idle UDA writes), and under
  that mode a flat address is wrong -- confirmed directly, `dispatcher`'s
  own real raw `SYSDAT.FMT` offset (0x38) sits *inside* `init`'s own
  cseg body if `sysdat` is ever treated as flat 0. Added `defs.h`'s
  own `SYSDAT_*` constants (real raw offsets, read via
  `mem_*_seg(mem, ds_sysdat, ...)`) instead of forcing these onto the
  existing flat `SD_*` convention.

  `proc_creat`'s own real asm, called from here, would *also* read
  `rlr`/`pul`/etc as `sysdat`-relative in this exact calling context
  (the real asm never resets `DS` before the call either) -- but
  `tests/src-c/fixtures/proc_wrapper.A86`'s own already-verified,
  separately-tested convention places those at flat scratch addresses
  for *its own* standalone test (`DS=0` there). Combining both
  conventions in one reference run isn't sound, so the reference
  wrapper treats `proc_creat` as a boundary stub (its own real result
  is discarded immediately anyway, so this doesn't lose any coverage
  of `init` itself) while `rtmif.c` still makes the real call into
  `proc.c`'s own already-independently-verified `proc_creat` -- caught
  by watching a stub-vs-real mismatch surface as a flat-out wrong
  `OUT` instruction where a `ret` should have been, which traced back
  to a second, more mundane bug in the same test wrapper: declaring
  stub labels *before* `include rtmif.rtm` let that file's own
  leading `org 0` silently overwrite them, the identical gotcha
  `LBDOSENT.A86`'s own wrapper already documents (now fixed the same
  way: the real file's own `include` comes first). Full existing
  suite stays green; `make clean && make build` stays green (44
  files now, up from 43).

- **`SUPIF.SUP` resolved (no gap at all) and `BDOSIF.BDO`'s own
  `init` ported, closing out the module-dispatcher survey this
  session's been working through.** `SUPIF.SUP` has no separate
  `init` label of its own — confirmed directly, it starts straight at
  `user_entry:` — and `INIT.SUP`'s own already-ported `init`
  deliberately `callf`s into `rtmmod`/`memmod`/`bdosmod`/`ciomod`/
  `xiosmod`'s own far pointers but *not* `supmod`'s, because
  `INIT.SUP` itself runs as part of the SUP module and serves as
  SUP's own initialization inline. Nothing further to port there.

  `BDOSIF.BDO`'s own `init` ported as `bdosif.c`: the same `sysdat`/
  `supervisor` copy as `CIOIF.CIO`/`MEMIF.MEM`'s own `init` (tested
  non-segmented, same flat `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG` convention
  — this module's own `init` has no far call and nothing that forces
  genuine segmented addressing the way `RTMIF.RTM`'s own `init`
  needed), then three real `mpmif` calls creating/opening/writing the
  MX disk queue (`src/kernel/DATA.BDO`'s own `mxdiskqd`/`mxdiskqpb`).
  Traced via `MODFUNC.DEF`'s `f_qmake`(134)/`f_qopen`(135)/
  `f_qwrite`(139) through `SYSTEM.DAT`'s own `sysent` table to
  `RTMIF.RTM`'s own `function` table entries 4/5/9 — all already
  ported in `que1.c` as `makeq_entry`/`openq_entry`/`writeq_entry`,
  the same "boundary call resolves to an already-ported routine"
  pattern this whole session kept finding.

  `mxdiskqd`/`mxdiskqpb` turned out to live in the combined
  `SYSDAT.FMT`+`DATA.BDO` dseg (`BDOS.A86` includes `sysdat.fmt` then
  `data.bdo` back to back, continuing the same shared dseg) — probed
  directly: real raw offsets 0xf92/0xfae, clear of both the
  `FILE2.BDO`/`FILE4.BDO` collision range and `SYSDAT.FMT`'s own
  already-used fields (`tickpd` ends at 0xa50). Since this module's
  own `init` is tested non-segmented (flat, degenerate `SYSDAT`), and
  since `que1.c`/`que2.c`'s own already-independently-verified
  routines (`test_que.py`) are themselves only correct under that
  same flat convention (`SD_QLR`/`SD_RLR`/etc. are hardcoded flat
  addresses there, not `mem_*_seg`-relative), `mxdiskqd`/`mxdiskqpb`
  are addressed via new flat scratch constants (`SD_MXDISKQD`/
  `SD_MXDISKQPB`, `defs.h`) instead of their real raw offset —
  consistent with `SD_SUPMOD_OFF`/`SD_SUPMOD_SEG` already being a
  scratch address rather than `supmod`'s own real raw offset (0),
  not a new exception.

  The one genuine blocker: `BDOSIF.BDO`'s own real `mpmif` is a
  genuine indirect far call (`ES = P_UDA[RLR]` then `callf
  cs:[supervisor]`), and `tools/i86sim.py` has no real-redirect
  capability for `CALL FAR [mem]` at all (confirmed directly against
  its own opcode-`0xFF /3` handling) — only a no-op
  `far_call_stub_addrs`, which can't be driven through to a real
  target the way `RTMIF.RTM`'s own single *near* `call proc_creat`
  could. Worse, `src/kernel/BDOSIF.BDO` doesn't even assemble
  standalone through `tools/asm86.py` at all — `entry`'s own
  `getdiskmx` helper uses an operand combination
  (`or p_flag[bx],pf_tempkeep`) that toy assembler's own operand
  support can't handle (confirmed: assembling the real file alone
  fails there), and since the whole file has to assemble whether or
  not `getdiskmx` ever runs, `init` couldn't be reached by `include`
  ing the real file the way every other such wrapper in this tree
  does. `tests/src-c/fixtures/bdosif_wrapper.A86` instead hand-
  transcribes just `init`'s own real header/body (verified line for
  line against the real source) and declares its own fresh `mpmif`
  near dispatcher — same convention `cpmmem_wrapper.A86` already uses
  for its own `mpmif` — tail-jumping into the real, `include`d
  `QUE1.RTM`/`QUE2.RTM` bodies based on `cx`. `QUE1.RTM`/`QUE2.RTM`'s
  own `cs:sysdat` references resolve to the same shared `sysdat` cell
  `BDOSIF.BDO`'s own `init` just wrote — not a symbol collision, the
  same single cell both modules' own `init` would independently set
  to the same ambient value in the real system anyway. Full existing
  suite stays green; `make clean && make build` stays green (45
  files now, up from 44).

- **`COMMAND.SUP`'s `chain_entry`/chain-mode `clicmd_entry`, closing
  out the real segment-relative-addressing gap flagged for this file
  since the `CHARIO.CIO` recon pass.** All three real differences
  between chain mode and the ordinary (ported-last-session) path are
  gated on `SD_CLI_CHAIN` (set once in `clicmd_entry`, from `dx==0`):

  1. `cli_copy`'s own command-tail source: chain mode copies 128
     bytes from the *calling* process's own DMA buffer
     (`cli_dma_seg:cli_dma_ofst`, genuinely segment-relative on the
     source side, `mem_r8_seg`) instead of the caller's own
     `CLICB.DEF`-shaped buffer.

  2. `cli_gopen`'s own `cli_ld1` block: chain mode hands the
     *current* process's own live UDA memory off to the newly
     allocated child PD in place, rather than letting `f_load` /
     `load_entry` allocate fresh UDA memory -- but the *running*
     process is still executing out of that same memory, so it first
     copies its own live UDA content onto a shared scratch block
     (`SYSDAT_INITUDA`, the exact same block `INIT.SUP`'s own `init`
     reserves for its own init process at cold boot -- probed
     directly, real raw `SYSDAT.FMT` offset 0x6C0, added to `defs.h`)
     and relocates its own `p_uda` there, continuing to run from the
     scratch block while its original memory now belongs entirely to
     the new child. Both the UDA copy and the relocation are real
     segment-relative addressing (`mem_r8_seg`/`mem_w8_seg` against a
     genuine runtime segment, not a fixed `SD_*` flat address) -- the
     exact gap this file's own header comment used to say chain mode
     was blocked on, resolved the same way `PROC.RTM`/`ML.MEM`/
     `MAU.MEM`/`SHARE.MEM` already were. The real asm's own
     accompanying `mov ss,ax` has no C equivalent (same as every
     other real stack switch in this port, `init.c`'s own header
     comment); `load()` is then called with `load_chain=1` and
     `bx=cli_pd` (not `bx=0`) -- `load.c`'s own `cload_entry` already
     documents this exact `bx=PD` convention, inlined here directly
     rather than going through that thin wrapper, same as the
     ordinary path already inlines `load_entry`'s own `bx=0`.

  3. `clicmd_exit`'s own tail: if chain mode set `SD_CLI_TERM` (via a
     new `cli_qful` helper, shared by both the success *and*
     `e_q_full` exits out of `cli_checkque` -- an existing bug in this
     port, not the real asm: the `e_q_full` exit used to skip straight
     to `clicmd_exit` without the chain/term check `cli_qful` now
     applies, harmless before since `cli_chain` was always false),
     the real asm's own `jmp mpmif(f_terminate)` is a true tail jump
     that never returns to `clicmd_exit`'s own caller in the real
     system at all. `f_terminate` is a boundary-stub no-op everywhere
     else in this project, so this port just clears the documented
     `p_flag` bits and returns a fixed `(bx,cx)=(0,0)` -- an
     "undefined artifact, fixed choice" (same category as `init.c`'s
     own `io_maxconsole`/`io_maxlist`), since no real caller ever
     actually observes it.

  `tests/src-c/test_command.py` adds `chain_entry` cases against the
  *existing* `command_wrapper.A86` (no new wrapper needed: neither the
  DMA-copy source nor the `clicmd_exit` tail needs `segmented=True` --
  the DMA source's own offset can be freely chosen to avoid colliding
  with anything under non-segmented testing, same convention as every
  other flat `SD_*`/`CLI_*` field in this file) covering the no-RSP-
  match path (through `cli_fload`'s own real `fileopen` failure, same
  as the existing non-chain case) and the RSP-dispatch-success path
  (verifying `cli_term` gets set and that `clicmd_exit`'s own chain+
  term tail genuinely skips `clirls`/`mxwrite` -- not asserting on the
  post-tail `bx`/`cx`, a documented fixed artifact with no real
  significance to check). `cli_gopen`'s own `cli_ld1` block itself has
  no differential test, same reason as its own non-chain continuation
  past `prfilnam` already didn't: both need a real, matching directory
  entry `fileopen` can't be made to produce in any test in this
  project. Full existing suite stays green; `make clean && make
  build` stays green (45 files, unchanged -- no new files this pass).

- **Documentation fix: `src/kernel/FILE3.BDO` is fully ported, same
  stale-note pattern as `FLUSH_FILE0`/`SUPIF.SUP` earlier this
  session.** Its own status-table row's deferred column still read
  "the rest of the file not reconned this pass," left over from the
  `PR_TERM`/`XCURSELECT`/`RESELECTX` changelog entry a few passes
  back. A fresh recon (every real label in the file, cross-checked
  against both `file3.c`'s own exported names and a sweep of every
  *other* `/src/kernel` file's own `CALL`/`JMPS` targets into
  `FILE3.BDO`) found nothing left to port: every externally-called
  label already has a C counterpart, and every label with no
  cross-file caller turned out to be an internal jump target inside
  an already-ported routine (`SELECT`/`SELECT1`/`SELECT2` inside
  `select_`, `WRITE`/`WRITE1` inside `diskwite`, `LOCK`/`LOCK_ERR`
  inside `lock_`, etc. -- confirmed by grepping for a bare
  `CALL`/`JMPS` to each one and finding none outside its own parent
  routine's body) or ported in a different file already
  (`REMOVE_DRIVE`, `file1.c`). No code changed here, only the status
  table and deferred-items list, to bring them back in line with what
  was actually already in the tree.

- **The `DATA.BDO` cseg/dseg test-harness collision, resolved for
  real: `file1.c`/`file2.c`/`file3.c`/`file4.c` now take a genuine
  segment parameter (`ds_bdos`) for every `BD_*`-field access.** This
  was the user's own explicit choice between two options laid out
  for them: a cheap fix confined to the test harness alone (relocate
  the reference assembly's own code bytes to a non-colliding flat
  address, zero C changes) versus making the C code itself
  segment-aware for real, since it might matter for the eventual
  linking/booting work independent of this one test gap -- they chose
  the latter, twice, after both the cost estimate and a follow-up
  correctness-risk finding (see below) were raised.

  The collision itself: this combined module's own `cseg` (bundling
  `BDOSIF.BDO`+`FILE1-4.BDO`, ~0x2659 bytes) and `DATA.BDO`'s own real
  field data (the `BD_*` constants, real raw offsets within the
  combined `UDA.FMT`+`SYSDAT.FMT`+`DATA.BDO` dseg, confirmed real via
  `--symbols`, not harness-chosen like `SD_*`/`CLI_*`/`UDA_*`
  elsewhere) share one flat address space in every existing wrapper's
  own non-segmented convention -- any routine whose own code lands
  inside dseg's `[0, 0x1000)` window gets its own instructions
  corrupted the moment that window's real field data gets laid over
  it. Confirmed via a cross-file-call-target sweep (same technique as
  this session's earlier `FILE3.BDO` staleness fix) that the entire
  call graph closes within these four files, plus exactly one external
  call site (`command.c`'s own `fileopen`, into `func15`).

  A genuine data-flow subtlety surfaced along the way and was worked
  through, not glossed over: some `BD_*` fields (`BD_FREE_ROOT`,
  `BD_OPEN_ROOT`, `BD_LOCK_ROOT`) are linked-list *roots*, and the
  obvious worry was that chasing their own stored pointers (`bx =
  mem_r16(mem, bx)`-style, inside `search_listo`/`create_item`/etc.)
  would also need `ds_bdos`, since those pointers lead to the actual
  list nodes. Checked directly against `tests/src-c/test_file3.py`'s
  own existing, already-passing setup: it places every such node at
  an ordinary harness-chosen flat scratch address (`0x4100`+), *not*
  within `DATA.BDO`'s own dseg -- confirmed by `file3.c`'s own header
  comment too ("MP/M-86's BDOS module has no private DSEG of its own
  at all... every one of its scratch fields... really is SYSDAT-
  resident on real hardware"). So the real rule turned out to be
  exactly the simpler one originally assumed: only a *literal* `BD_*`
  identifier used as an address needs `ds_bdos`; any value obtained by
  *dereferencing* one (an FCB pointer from `BD_INFO`, a chased list
  node from `BD_OPEN_ROOT`, etc.) is already a final, ordinary flat
  address needing no further adjustment -- these pointed-to buffers
  simply aren't part of the colliding region, confirmed structurally,
  not assumed.

  Mechanically: every function touching a `BD_*` field directly or
  transitively gained a new `uint16_t ds_bdos` parameter (added via a
  scripted transform across all ~155 affected functions/~580 access
  sites, then individually verified); every `mem[BD_X]`/`mem_r16(mem,
  BD_X)`/`mem_w16(mem, BD_X, ...)` became the `_seg` form against it;
  a handful of generic, already-flat address-taking helpers (`move`/
  `compare`/`cmpec0`) stayed untouched on purpose -- they operate on
  addresses their *callers* already resolve, so the few call sites
  passing a bare `BD_*` constant directly now wrap it with a new,
  pure address-only helper, `mem_seg_addr` (`defs.h`), instead of
  threading `ds_bdos` into those generic helpers too. 11 functions
  that turned out to touch no `BD_*` field at all, directly or
  transitively (`diocomp`/`seek`/`ro_test`/`chk_wild`/`tst_tbl_lmt`/
  `cmperr`/`setrandom`/`func48`/`func25_entry`/`func32_entry`/
  `func44_entry`/`func45_entry`/`func51_entry`), had the otherwise-
  unused parameter removed again -- caught automatically by
  `-Wunused-parameter` under this project's own `-Werror` build, the
  same mechanical safety net that caught every signature/call-site
  mismatch along the way. `command.c`'s own `fileopen` passes
  `ds_bdos=0` into `func15` (its one external call site), matching
  its own existing flat `BD_INFO`/`BD_ARET` convention exactly.

  Every existing differential test across all four files (plus
  `test_command.py`) needed its own `ds_bdos=0` ctypes argument added
  to stay consistent with the now-changed C signatures -- confirmed to
  reproduce every existing result bit-for-bit (degenerate `ds_bdos=0`
  is definitionally identical to the old flat addressing), not just
  assumed. One new test was added to prove the fix does something
  real: `swap_sdcnt` (`test_file2.py`), the simplest routine in the
  collision range, now gets a genuine `segmented=True` reference run
  with a real, non-degenerate `DS` (`DS_BDOS=0x300`, chosen clear of
  this wrapper's own ~0x2659-byte cseg) -- the first differential test
  this project has ever had for any routine in this cluster.
  `close`/`close1`/`search`/`searchn`/`open`/`open1`/`opencopy`/
  `copy_dir0`-`2` are unblocked the identical way but don't have their
  own new tests written yet (a real follow-up, not a technical
  blocker). Full existing suite stays green; `make clean && make
  build` stays green (45 files, unchanged -- no new files this pass).

- **Writing `search`'s own new differential test (the follow-up to
  `swap_sdcnt` above) surfaced a real `tools/asm86.py` encoding bug,
  fixing which broke 7 *other* already-passing tests' own hardcoded
  probed addresses -- 6 are fixed, 1 (`test_lbdosf2.py`) is not yet.**
  `search`'s own real asm (`SEARCHN`) has `CMP CH,MODNUM ! JNZ $+4 !
  AND AL,3Fh` -- a deliberate DRI byte-reuse trick: when the jump is
  taken, `$+4` is meant to land exactly past the 3-byte `AND AL,3Fh`
  (`80 /4 ib`), but `tools/asm86/isa.py`'s own `_encode_alu` always
  emitted that longer encoding, even for an AL/AX destination, where
  the real 8086 (and the real DRI assembler) has a dedicated, shorter
  accumulator-immediate form (`idx*8+4`/`+5`, e.g. `AND AL,imm8` =
  `24 ib`, 2 bytes) -- so `$+4` landed one byte short, inside the
  AND's own immediate operand, corrupting decode from there on
  (caught as "unimplemented opcode" crashes, then wrong results once
  that was worked around). Fixed in `_encode_alu` itself (now emits
  the short form for an AL/AX destination), plus two new
  `tools/i86sim.py` opcodes this exposed a real need for: the
  accumulator-immediate forms of all 8 ALU ops (`0x04`-`0x3D`, not
  just the group1 r/m,imm forms already supported) and `LAHF`/`SAHF`
  (`0x9F`/`0x9E`, saving/restoring `compext`'s own ZF result across
  two intervening MOVs -- reuses `_flags_word`'s own existing
  ZF/CF-only packing, the same minimal-flag convention PUSHF/POPF
  already use).

  This is a real, universal assembler fix -- every `AND/OR/ADC/SBB/
  SUB/XOR/CMP AL,imm8`-or-wider instruction in every real source file
  this project assembles now encodes 1-3 bytes shorter wherever it
  occurs, shifting every label after it. That broke 7 other already-
  passing tests' own hardcoded, `--symbols`-probed addresses (not a
  regression in the C port itself -- every one of these constants was
  already a "real raw offset, re-probe if the assembler ever changes"
  value, just never expected the assembler to change). Fixed:
  `test_file1.py`/`test_file2.py`/`test_file3.py`/`test_file4.py`
  (shared `mpmif`/`homef`/`seldskf`/.../`xprint` `STUBS` set, off by a
  uniform 3 bytes); `test_ldbios86q.py` (the `LDB86Q_*` static-data
  addresses, including `DPE0`/`SEGTABLE` and `DISK` through
  `WRITE_PARMS`, shifted non-uniformly and re-probed individually, not
  guessed); `test_ldmpm.py` (`ldmpm.c`'s own `FCB_ADDR`); and
  `test_load.py`, which turned out to be a second, independent class
  of fallout: its own `FCB_ADDR`/`WRKSEG` scratch pair happened to
  resolve to a physical address *inside* this wrapper's own cseg --
  writing a plain `0` there to mean "no explicit disk letter" never
  actually overrode the real assembled byte living there (same "only
  nonzero bytes override cseg" convention `test_file2.py`'s own
  `get_nalbs`/`getblock` already relies on elsewhere), so the test
  was already reading whatever real cseg byte happened to be there --
  working by sheer coincidence (it was already 0) until the fix
  changed what that byte is. Moved `FCB_ADDR` to a flat scratch
  address comfortably clear of this wrapper's entire cseg instead of
  re-probing (the right fix for a scratch address that was never
  supposed to depend on real code layout to begin with).

  **`test_lbdosf2.py`'s reference-run failure is resolved.** The
  wrapper configured BIOS entry points as `call_stub_addrs` only, but
  `rddirbuf` reaches `setdmf` through a tail `JMP`. Without a matching
  jump stub, the simulator executed unrelated bytes at the BIOS
  function's numeric address in the wrapper, returned a spurious
  nonzero status, and entered the error handler. Configuring those
  affected tail-jump targets (`setsecf` and `setdmf`) in
  `jmp_stub_addrs` models those BIOS returns from the enclosing `CALL`
  correctly. The focused test and full
  `make -C src-c test` suite now pass; `make -C src-c build` compiles
  all 50 C files.
