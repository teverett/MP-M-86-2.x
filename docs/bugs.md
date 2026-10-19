# Bugs Found in the Original MP/M-86 2.x Source

While validating the assembler built in `tools/asm86/` against the files
copied into `src/`, assembly failed on several sources with genuine
defects in the original 1981 Digital Research source itself — not bugs
in the assembler. These were confirmed by reading the surrounding code,
not just by the assembler's error message.

This is consistent with the provenance already noted in the top-level
`README.md` — these are hand-labeled "work disks," not a verified golden
release, and this code likely never finished debugging on the original
hardware.

**Patch status, by section below:** #1–#3 (the RSPs) are left
**unpatched** — `src/rsp/` remains an unmodified copy of `orig/` for
those files. #4, #5, #7 (partially), #8, #9, #10, #11, #12, #13, and
#14 (the kernel/loader name typos, the `LDBDOS.A86` fixed-offset
interaction with `tools/asm86`'s raw-binary output, the `tools/asm86`
port-truncation bug, `ROM86Q.A86`'s own two real runtime bugs, and
`LDBIOS86Q.A86`'s relative-jump-to-an-absolute-address bug)
**have been patched** directly in `src/kernel/`/`src/loader/`,
`tools/asm86/`, or `tools/build_mpmldr.py` — once a fix is this
confidently evidence-backed (an exact squashed-name match to a real
declaration used incorrectly nowhere else, a byte-level confirmation of
the wrong address landing in the combined image, or — for #12-#14 —
confirmed by actually running the code in QEMU; #14's fix is confirmed
correct on its own terms but has not, on its own, gotten this actually
booting — see that section),
leaving it broken forever didn't serve the "get this booting" goal,
and each patched spot is called out explicitly below so the change is
traceable back to its evidence. #6 and #7 (the `dp_base` part) remain
unpatched: either no confident fix exists (an undeclared symbol with no
same-file candidate), or fixing it isn't a source-level change at all
(the short-jump-range tooling trade-off).

## 1. `src/rsp/CLOCK.A86` — jump to a label that doesn't exist

The clock-update loop is entered at a label spelled `clockloop:`
(line 64), but all four jumps back to the top of the loop reference
`clock_loop` (with an underscore) instead:

```
64:clockloop:		; Clock process starts here
...
78:	    jmp clock_loop
95:	    jmp clock_loop
107:	    jmp clock_loop
114:        jmp clock_loop           ; loop forever
```

`clock_loop` is never defined anywhere in the file. As written, this
RSP cannot assemble at all with any assembler, let alone run.

## 2. `src/rsp/TMP.A86` — call to a label that doesn't exist

Line 147 calls `print_string`:

```
147:	    call print_string
```

but the routine is actually defined as `printstring:` (no underscore,
line 373), and every *other* call site in the file (lines 237, 239,
266, 268, 338) correctly uses `printstring`. This one call site is the
odd one out.

## 3. `src/rsp/SUBMIT.A86` — two mismatched labels and one missing routine

Three separate defects in one file:

- Line 464 jumps to `not_builtin`, but the label actually defined
  (line 553) is `notbuiltin` (no underscore).
- Line 127 loads the address of `sopen_err`, but the label actually
  defined (line 881) is `sopenerr` (no underscore).
- Line 630, inside the `showerr:` error-printing routine, calls
  `conwrite`:

  ```
  showerr:
  ; Print Error String
  ;	input: DX = address of Error String

  	call conwrite
  	jmp submitexit
  ```

  `conwrite` is never defined anywhere in the file. Every other place
  that needs to write to the console loads `mpm_conwrite` (an EQU'd
  BDOS/MP/M function number) into `CL` and calls the shared `mpm` entry
  point instead (see lines 91, 330, 770) — `showerr:` appears to be an
  unfinished or broken implementation of the same pattern.

## Reclassified: `src/utils/ED.PLM`'s `TIME` is not a bug

An earlier version of this document flagged `ED.PLM`'s
`WAIT: PROCEDURE;` (line 1264, a half-second busy-wait delay loop
ending its loop body with `CALL TIME(250);`, line 1274) as a likely
bug: `TIME` is never declared anywhere in `ED.PLM`, and no other file
in the entire 11-disk tree calls it either, so at the time there was no
cross-reference to confirm it the way `MEMORY` had been. Once
`orig/plm80s/PLMLANG.DOC` (Intel's own PL/M-80 language reference) was
added to this tree, it settled the question: it documents
`CALL TIME(expression)` verbatim — "the expression is evaluated as a
BYTE value. The TIME procedure delays 100 microseconds times the
value." `250 * 100us * 20` (the loop's iteration count) `= 500ms`,
an exact match for the delay's own comment, "1/2 SECOND TIME OUT." A
real, documented PL/M standard-library builtin, not a dropped
declaration — see `tools/plm/README.md` for the implementation.

## Reclassified: `src/utils/SET.PLM`'s `MOVW` is not a bug

An earlier version of this document flagged `SET.PLM` line 929's
`MOVW(@UDA.DF$PASSWORD,@UDA$PARENT.DF$PASSWORD,4)` as a likely bug, by
analogy with `TIME` above: `MOVW` is never declared anywhere in the
source tree either. Further checking found evidence it's deliberate: the
field it copies, `DF$PASSWORD`, is declared in `UDA.LIT` as exactly
`(8) BYTE`, and the call passes a count of `4` — 4 *words* = 8 bytes,
exactly matching the field size if `MOVW` moves by words (with
`(source, dest, count)` argument order, the reverse of the confirmed
`MOVE(count, source, dest)` builtin). That's strong enough corroborating
evidence, even without a second call site to cross-reference against, to
treat it as a genuine (if undocumented) PL/M-86 compiler builtin rather
than a defect — see `tools/plm/README.md` for the implementation.

## 4. `src/kernel/BDOS.A86` — pervasive dropped/added underscores (45 names) — FIXED

Assembling `BDOS.A86` (after the tooling fixes in `tools/README.md` —
this is genuinely the source, not the assembler) used to stop on a long
list of undefined symbols, almost all of which were a declared name
used with an underscore added, removed, or moved. This is the same
defect class as `CLOCK.A86`/`TMP.A86`/`SUBMIT.A86` above, just far more
widespread — consistent with this disk's provenance note ("work disks,"
never finished debugging) applying to the BDOS module as a whole, not
just the RSPs.

**Patched** (the wrong spelling, as it appeared in `FILE1.BDO`–
`FILE4.BDO`/`BDOSIF.BDO`, renamed to match the one real declaration —
confirmed via a whole-tree squashed-name search, i.e. comparing with
underscores stripped, before renaming):

`PDCNT`→`PD_CNT`, `PFLAG`→`p_flag` (also in `DSPTCH.RTM`), `BTABFLAG`→
`btab_flag`, `CHECK_WRITE`→`checkwrite`, `CK_RODIR`→`ckrodir`,
`COMP_EXT`→`compext`, `CPY_DIR_LOC`→`cpydirloc`, `CUR_SELECT`→
`CURSELECT`, `DIR_TO_USER`→`dirtouser`, `DISKREAD0`→`DISK_READ0`,
`DISKWRITE2`→`DISK_WRITE2`, `DRV_LBLA`→`DRVLBLA`, `END_OF_DIR`→
`endofdir`, `FUNC_48`→`FUNC48`, `GETDMA`→`GET_DMA`, `GETRCNTA`→
`GET_RCNTA`, `GETS1`→`GET_S1`, `GET_DPTRA`→`getdptra`, `GET_MODNUM`→
`getmodnum`, `HIGHEXT`→`HIGH_EXT`, `MULTIO`→`MULT_IO`, `OPENFILE`→
`OPEN_FILE`, `OPEN_REEL`→`openreel`, `PRFX1`→`PR_FX1`, `RD_DIR`→
`rddir`, `RETURN_SEG`→`returnseg`, `ROTEST`→`RO_TEST`, `SAVESP`→
`SAVE_SP`, `SEARCH_N`→`SEARCHN`, `SEEK_COPY`→`seekcopy`, `SEL_ERROR`→
`selerror`, `SET_DATA`→`setdata`, `SET_DIR`→`setdir`, `SET_END_DIR`→
`setenddir`, `SET_RO`→`setro`, `SRCH_LOOP`→`srchloop`, `STA_RET`→
`staret`, `TMPSELECT`→`TMP_SELECT`, `copydir`→`copy_dir`,
`get_lock_add`→`GET_LOCKADD`, `getrra`→`GET_RRA`, `make_x02`→
`makex02`, `makeflag`→`MAKE_FLAG`, `makex03`→`make_x03`, `openmax`→
`open_max`, `search_nlist`→`SEARCHN_LIST`.

Separately, `clierr` (`COMMAND.SUP`, 8 call sites) → `cli_err` — the
label actually declared (`SYSDAT.FMT`/`SYSTEM.DAT`), and already used
correctly once in the same file (line 475) before this fix.

After all of the above, `BDOS.A86` assembles end-to-end cleanly (the
short-jump-range tooling trade-off that used to block it, see #6, is
now fixed too).

## 5. `src/kernel/ABORT.RTM` — five more of the same, inside one file — FIXED

Within a single file: `snipit`→`snip_it` (declared in the same file);
`fpdq_nxt_q`→`fpdq_nxtq` (an *added* underscore this time, not dropped);
`find_pdq`→`findpdq`; `findpd`→`find_pd` (declared and used correctly
everywhere else in the same file); `plink`→`p_link` (`PD.DEF`); and
`u_inint`→`u_in_int` (`UDA.FMT`). All patched. `RTM.A86` as a whole
(which includes `ABORT.RTM`) now assembles end-to-end cleanly, same as
`BDOS.A86` above.

## 6. Short-jump-range tooling limitation — FIXED (tools/asm86 now does real jump-size relaxation)

`COMMAND.SUP:139` (`SUP.A86`), `SHARE.MEM:35` (`MEM.A86`),
`DSPTCH.RTM:222` (`RTM.A86`), `FILE2.BDO:1193` (`BDOS.A86`), and one
spot in `LDBIOS.A86` used to hit `tools/asm86`'s short-jump-range
limitation rather than a source bug — the disp16-everywhere trade-off
this assembler made could push an originally-in-range conditional
jump/`JMPS` out of 8-bit range in *this* assembler's encoding, even
though DRI's own assembler (with disp8 optimization) kept it in range.
This was confirmed, at the time, by temporarily disabling the range
check and re-assembling each of the five affected files, which showed
it was the *only* remaining blocker in each — no further source defects
were hiding behind it.

**Fixed**: `tools/asm86` now does real relaxation (see "Design choices
that trade size for simplicity" in `tools/README.md` for the
mechanism). All five files named above — `BDOS.A86`, `MEM.A86`,
`RTM.A86`, `SUP.A86`, `LDBIOS.A86` — now assemble end-to-end with no
remaining blocker at all.

## 7. `src/kernel/XIOS.A86` — one more underscore typo (FIXED), plus one genuinely unresolved symbol

Line 611, `mov word ptr .088h,offset tick_int`, used `tick_int` where
the label actually declared (line 349) is `tickint` (no underscore) —
the same bug class as everywhere else on this page. **Patched.**

Separately, line 897's `add ax,offset dp_base` references `dp_base`,
which is **not** declared anywhere in `XIOS.A86` itself, nor anywhere
else in the whole 11-disk tree under that spelling. Unlike every other
finding on this page, no squashed-name match (`dpbase`, `dp_base`, etc.)
exists in this file to point to as "the real" declaration, so this is
left as a genuinely unresolved symbol rather than guessed at — **not
patched**. Since `docs/build.md` already excludes `XIOS.A86`/`ROM.A86`
from the QEMU-bootable target (they're written for the Intel iSBC 86/12
+ SBC 204 specifically), this wasn't investigated further.

## 8. `src/loader/LDBIOS.A86` — four more dropped/added underscores, one file — FIXED

`ccpoffset`→`ccp_offset`, `dp_base`→`dpbase`, `seg_table`→`segtable`,
and `z_ret`→`zret` — the same bug class, all four within this one file,
each used exactly once. All patched. `LDBIOS.A86` now assembles
end-to-end cleanly (the short-jump-range trade-off that used to block
it afterward, see #6, is now fixed too).

Separately (not a bug in this file, noted here only for
cross-reference): `LDBIOS.A86`'s iSBC 86/12/iSBC 204/8251-specific
disk and console drivers, like `XIOS.A86`'s own (#7), have no QEMU
equivalent, so `tools/build_mpmldr.py` builds `MPMLDR.bin` from a new
`src/loader/LDBIOS86Q.A86` instead — not a fix to this file, a parallel
replacement targeting different hardware. See `docs/build.md`'s "What's
actually needed to boot under QEMU" item 1 and that new file's own
header comment.

## 9. `src/loader/LDMPM.A86` — underscore typo — FIXED

Line 57, `abs_zero rw 2*bdosint`, used `bdosint` where `bdos_int` is
declared earlier in the very same file (line 48, `bdos_int equ 224`,
the MP/M BDOS interrupt number) — this was missed on the first pass
because the whole-tree squashed-name search that caught every other
instance of this bug class only turned up an unrelated same-spelled
label in a completely different module (`DDT86.A86`'s debugger), not
this file's own, underscore-only variant. The fix also makes sense
dimensionally: `2 * 224` words reserves exactly enough space for an
8086 interrupt-vector table (2 words per vector) covering vectors `0`
through `224`. **Patched.**

## 10. `src/loader/ROM.A86` — three more underscore typos — FIXED

Three separate instances of the same bug class, all within this one
file: line 264's `in al,fdc_stat` used `fdc_stat` where `fdcstat` (no
underscore) is declared at line 58 and used correctly at three other
call sites in the same file; `setup_dma` (two call sites) used where
`setupdma` is the label actually declared; `last_com` used where
`lastcom` is the label actually declared (and used correctly at two
other sites). All three **patched**.

Separately, line 410's `er0 rb length cer0` (and nine more like it)
surfaced a real `tools/asm86` gap, not a source bug: `LENGTH name` is a
genuine DRI ASM86 operator — evaluating to the total byte length of a
`DB`/`DW`/etc. declaration (confirmed here: `Cer0 db cr,lf,'Null Error
??',0` is 16 bytes, matching this line's own `;16` comment exactly) —
that this assembler didn't implement at all. Fixed in `tools/asm86`
(see `tools/README.md`).

## 11. `src/loader/LDBDOS.A86` — its own fixed-offset EQUs silently broke at runtime under `tools/asm86` — FIXED

Not a dropped/added underscore like the rest of this page, and not
caught by assembly failing (it assembles cleanly either way) — found
by manually inspecting the combined `MPMLDR.bin`'s actual bytes while
reconciling `LDBDOS.A86`'s own `bdosoffset`/`biosoffset`/`bdoscodesize`
EQUs (DRI's original fixed values, `400h`/`1200h`/`0B00h`) against
`LDMPM.A86`'s already-patched `ldbdos_offset`/`mldbios_offset` (see #9
and `tools/build_mpmldr.py`'s own docstring).

DRI's own build placed every loader module via Intel-hex `pip`, which
merges sparse, absolute-address records — a module's `org` to some
fixed address costs nothing, since the hex format simply has no
records at all for addresses below it. `tools/asm86` emits raw binary
instead, where the same `org bdosoffset` (used at the very top of
`LBDOSENT.A86`, right where `LDBDOS.A86`'s real entry point is) means
literal zero-byte padding in `LDBDOS.cseg.bin` from file offset 0 up to
`400h`. `tools/build_mpmldr.py` then placed that *whole* blob — padding
included — starting at `LDMPM.A86`'s own patched `ldbdos_offset`
(`3efh`), so the interrupt vector `LDMPM.A86` patches to reach `LDBDOS`
actually pointed at zero padding, not the real entry code, 0x400 bytes
short of where it needed to land. Confirmed directly: byte `0x3ef` of
the combined image was `00`, not the `JMP bdose` instruction
`LBDOSENT.A86` places right at its own `org bdosoffset`. The same
problem applied to `biosoffset` (`1200h`), used as the absolute base
for every `CALL`/`JMP` `LDBDOS.A86` makes into the loader BIOS's jump
table (`constf`/`coninf`/`conouf`/`readf`/`writef`/`wbootf` in
`LBDOSEQU.A86`/`LBDOSENT.A86`/`LBDOSIO.A86`/`LBDOSF1.A86`) — it never
matched wherever `LDBIOS86Q.A86` actually ended up (`e8fh` once the
fix below is applied, nowhere near DRI's fixed `1200h`), so those calls
would have jumped into whatever code happened to be sitting at the
wrong address instead of the real BIOS functions.

**Fixed**, in two parts:
- `src/loader/LDBDOS.A86`'s own `bdosoffset` is now permanently `0` —
  this module no longer self-positions via its own `org`; its place in
  the final image is purely `tools/build_mpmldr.py`'s sequential
  concatenation, the same as `LDMPM.A86`/`LDBIOS86Q.A86`.
- `tools/build_mpmldr.py` now also self-corrects `LDBDOS.A86`'s
  `biosoffset` (set to the real `mldbios_offset`) and `bdoscodesize`
  (set to `LDBDOS.A86`'s own real assembled CSEG size, so its DSEG data
  lands right after its real code instead of DRI's original, now again
  too-small, `0B00h` budget) — the same self-correcting-patch mechanism
  already used for `LDMPM.A86`'s constants, extended to this file.

Verified by re-inspecting the rebuilt `MPMLDR.bin` directly: the byte
at the patched `ldbdos_offset` is now the real `JMP bdose` opcode, and
the bytes at the patched `mldbios_offset` are `LDBIOS86Q.A86`'s real
jump-vector table, with `biosoffset`'s derived function addresses
(`constf`, `coninf`, etc.) landing on the correct individual jump-table
entries within it.

## 12. `tools/asm86` — immediate IN/OUT silently truncated ports >= 256 to 8 bits — FIXED

Found by actually running `src/loader/ROM86Q.A86` in QEMU for the first
time (see docs/build.md's "What's actually needed to boot under QEMU"
item 3) — the first and only code in this whole project to have
actually executed, rather than just assembled and been reviewed by
hand. The boot sector's own UART/FDC port access (`out uart_lcr,al`
etc., ports in the `03F8h`-`03FDh`/`03F0h`-`03F7h` range) silently did
nothing useful: `tools/asm86`'s `IN`/`OUT` encoder packed the port
number into the immediate 8-bit field the `0xE4`-`0xE7` opcodes
actually have, with no range check, so `out 3FBh,al` quietly became
`out 0FBh,al` — a real port, just the wrong one, so nothing failed to
assemble and nothing crashed, it just silently talked to garbage
hardware instead of the UART. Confirmed by manually decoding the assembled bytes: `out uart_lcr,al`
(port `03FBh`) produced `E6 FB` — the 2-byte immediate-port `OUT`
opcode with its port truncated to `0FBh`, not the 3-byte `mov dx,03FBh
! out dx,al` sequence the port actually needed.

**Fixed** in two parts:
- `tools/asm86/isa.py`'s `_encode_in`/`_encode_out` now reject any
  immediate port `>= 0x100` at assembly time (the 8086's only way to
  address such a port is through `DX`), instead of silently truncating
  it.
- Every real occurrence this caught — `src/kernel/XIOS86Q.A86`,
  `src/loader/LDBIOS86Q.A86`, `src/loader/ROM86Q.A86` (all three new,
  QEMU-target files using the 8250 UART and/or NEC765 FDC, the only
  ports in this tree above 0xFF) — was changed to `mov dx,port ! in/out
  al,dx`, preserving `DX` across `fdc_out_byte`/`fdc_in_byte` with an
  explicit `push dx`/`pop dx` where their own header comments already
  promised not to disturb any register but `AL`/flags.

## 13. `src/loader/ROM86Q.A86` — a 512-byte sector read into a 9-byte buffer — FIXED

Also found by the same first real QEMU boot. `INT 13h` always
transfers one full 512-byte physical sector regardless of how much of
it the caller actually cares about; this file's header read used to
target a 9-byte buffer (`hdrbuf rb 9`) declared inside its own data
area, so the other ~500 bytes DMA'd in overwrote every variable and
string declared after it in memory — `msg_boot`, `msg_fail`, and the
jump-progress message, replaced with zeros (the unused tail of that
disk sector) the instant the header was read. Confirmed directly: a
test build that read `msg_boot`'s own first byte back out right after
the header read got `0x00`, not the `0x0D` (CR) it's declared with.

**Fixed** by pointing the header read at `hdr_scratch` (`0500h`), a
fixed low-memory address outside this file's own 512 bytes entirely —
not a label in this module's own data, which is exactly what made the
original buffer vulnerable. That surfaced a second, smaller bug while
fixing the first: `word ptr hdr_scratch+1`-style addressing silently
resolved to the *immediate* value `0x501`, not a memory dereference —
`hdr_scratch` is a bare `EQU` constant, not a label backed by reserved
storage, so there's no way for the assembler to tell "the number 0x500
used as an address" apart from "the number 0x500 used as a value" from
that syntax alone (unlike `src/kernel/XIOS86Q.A86`'s own `read_parms+2`
style, which works because `read_parms` *is* a real label). **Fixed**
by using register-indexed addressing instead (`word ptr 1[bx]`, with
`BX` already holding `hdr_scratch` from the disk read) — the same
displacement-before-bracket idiom the real `04/ROM.A86` already uses
(`mov AL,1[BX]`), confirmed working by testing it for real.

Verified end to end in QEMU after both fixes: the boot sector prints
its own boot message, reads the 9-byte loader header correctly, prints
one progress dot per body sector read (9 of them, matching `MPMLDR.bin`'s
real size), prints its "about to jump" message, and actually jumps
into `MPMLDR` — the first piece of this entire project confirmed
running correctly on real (emulated) hardware, not just assembled and
reviewed by hand.

## 14. `src/loader/LDBIOS86Q.A86` — a relative jump used to reach an absolute cross-module address — FIXED (but booting is still blocked past this point)

Found continuing the same QEMU boot testing as #12/#13, after those two
fixes got the boot sector and `MPMLDR.bin`'s own loading fully working
(confirmed: boot message, all 9 body-sector progress dots, "about to
jump" message, then the actual jump into `MPMLDR`) but nothing from
`LDMPM.A86` itself ever appeared afterward.

The real `LDBIOS.A86` ends its `INIT` routine with `jmp ccp`, where
`ccp_offset equ 0103h ;base of MPMLOADER` and `ccp:` is declared via
`org ccp_offset` *within LDBIOS.A86's own single-pass assembly* —
since the jump and the label it targets are both resolved by the same
assembler run, a plain relative jump between them is computed
correctly no matter how far apart they land in the final image.
`LDBIOS86Q.A86` is assembled as a separate, independent module and
only concatenated into `MPMLDR.bin` afterwards
(`tools/build_mpmldr.py`) — a relative `jmp ccp_offset` encoded
assuming this module starts at address 0 is wrong once it's relocated
to wherever it actually lands (`mldbios_offset`): it lands at
`mldbios_offset + 0x103`, not the intended absolute `0x103`. Confirmed
by tracing the exact encoded bytes and the arithmetic by hand, and by
a debug probe placed directly inside `LDMPM.A86` (temporarily, not
kept) confirming its own code at `0x103` was never reached.

**First fix attempt**: an indirect jump through a register (`mov
ax,ccp_offset ! jmp ax`) instead — absolute within `CS` regardless of
where the instruction itself physically sits, immune to this module's
own relocation. `WBOOT` (the real `LDBIOS.A86`'s own, different
warm-boot target, `jmp ccp+6`) was simplified to the same target here,
since it isn't on this loader's cold-boot path.

That got the jump landing on the right bytes, confirmed by inspection,
but booting still didn't get past it — and testing in QEMU showed
*why* not: a register-indirect `jmp` reliably failed to transfer
control at all on this specific QEMU target, for a reason never root-
caused. See #15 below for the real, structural fix this led to (which
made the indirect jump unnecessary in the first place) and #16-#19 for
everything else this same investigation turned up before the loader
finally booted past `LDMPM.A86`'s own signon message.

## 15. `src/loader/LDBIOS86Q.A86` — *every* local `offset`/direct
reference was wrong the same way as #14, not just the one jump — FIXED

The #14 jump wasn't a one-off: *every* plain `offset <label>` or
direct-memory reference in this file (`stkbase`, `secbuf`,
`specify_parms`, the FDC command tables, the DPB/DPE...) baked in an
address relative to this module's own `org 0`, not its real position
(`mldbios_offset`) in the concatenated `MPMLDR.bin` — the identical
problem #14's jump had, just for every other reference instead of one
jump target. Confirmed by testing in QEMU: boot got past the loader's
own "jump into MPMLDR" message and ran `INIT`'s hardware setup (I/O
port writes don't care about addressing), but then `mov sp,offset
stkbase` pointed `SP` ~0xe97 bytes too low — into `LDMPM.A86`'s own
code — so `INIT`'s own `PUSH`es corrupted it before the #14 jump back
there ever ran. That's also why this file used to declare its data in
a separate `dseg`: `tools/build_mpmldr.py` only ever copied each
module's `.cseg.bin`, so that whole `dseg` — the signon string, DPB,
FDC command tables, `secbuf`, the boot stack — was silently dropped
from `MPMLDR.bin` entirely, on top of the addressing bug.

**Fixed** structurally instead of one reference at a time: this
module's `cseg` now starts with `org mldbios_offset` (a local copy of
`LDMPM.A86`'s own constant, kept in sync by `tools/build_mpmldr.py`)
instead of `org 0`, with the `dseg` merged into `cseg` right after it,
so every `offset <label>` the assembler computes is already the real,
absolute, `CS`-relative address — no per-reference fixup needed. This
also makes a plain direct `jmp ccp_offset` compute the correct
displacement on its own, so #14's register-indirect jump (which
turned out to be unreliable on this QEMU target anyway) was replaced
with that. `tools/build_mpmldr.py` strips the resulting leading
`mldbios_offset` zero-padding bytes (`tools/asm86` pads a raw binary
from 0 up to any `org` target) before concatenating this module's real
content.

## 16. `src/loader/LDBIOS86Q.A86` `INIT` — interrupts left enabled the
whole way in, so raw FDC port I/O collided with SeaBIOS's own IRQ6
handler — FIXED

`src/loader/ROM86Q.A86`'s own boot sector re-enables interrupts (`sti`)
right after setting up `SS:SP`, and nothing in the loader chain ever
disabled them again — so by the time `INIT` started doing raw FDC
register writes, IRQ6 (floppy) was still unmasked and still vectored
through SeaBIOS's own `INT 0Eh` handler behind this code's back.
Confirmed via GDB: the hang here wasn't this file's own polling loop
spinning forever — `CS:IP` was sitting inside SeaBIOS itself
(`CS=F000h`), stuck handling an IRQ6 it received while BIOS's own idea
of the controller's state no longer matched reality.

**Fixed** with a `cli` as the very first instruction of `INIT` — this
driver is purely polled and never needs interrupts delivered at all,
so there's no reason to re-enable them (`sti`) afterward either.

A related, now-unnecessary "fix" this investigation also reverted: an
earlier version of `INIT` added a synthetic DOR reset pulse (toggling
`NDRESET` 0 then 1) to try to force the FDC into a known state, plus a
loop draining 4 `SENSE INTERRUPT STATUS` responses before `SPECIFY`.
That synthetic reset left QEMU's emulated 8272 with no genuine pending
interrupt to report, so the SIS drain hung forever polling for a
result byte QEMU's model never produces for a SIS it doesn't recognize
as answering a real interrupt (it just silently drops back to idle
instead of entering the result phase). Removed: SeaBIOS's own `INT
13h` floppy driver already reset/initialized this controller for real
during the boot sector's own reads (`ROM86Q.A86`'s `read_sector`), so
`INIT` just re-asserts the already-working motor/DMA-enable/drive-
select DOR state (no edge, no synthetic reset, no SIS needed) and goes
straight to `SPECIFY`.

## 17. `tools/asm86` — `JMPF`/`CALLF` with a bare memory-pointer
operand silently became a *direct* far jump to the wrong place — FIXED

Found continuing from #15/#16: with those fixed, `INIT`'s own code
and jump all checked out by hand, yet a register read back from memory
right after a `jmpf dword ptr <mem>`-style jump kept coming back
`0` where it should have been the loader's own code segment.
Reproduced in total isolation, outside this project's multi-module
build entirely: a minimal boot sector that does `jmpf dword ptr
leap_off` (the exact same idiom `ROM86Q.A86` uses to enter `MPMLDR`,
and the exact idiom several `callf dword ptr .xiosmod`-style indirect
calls in `src/kernel/INIT.SUP`/`SUPIF.SUP` use) landed with `CS=0`
instead of the real target segment, confirmed by reading back the
landed `CS` from memory.

The cause: `tools/asm86`'s operand parser (`expr.py`'s
`parse_operand`) always treated a bare label/memory operand to any
mnemonic in `BRANCH_MNEMONICS` — which included `jmpf`/`callf` — as an
immediate *relative branch target*, the correct reading for `jmp`/
`call`/`jcc`/`loop`/etc, but not for `jmpf`/`callf`: their bare-label
operand means "the far pointer *stored at* this address", a memory
dereference, not a branch-target immediate. `isa.py`'s `_encode_jmpf`/
`_encode_callf` then saw an `'imm'`-kind operand and emitted a
*direct* far jump/call (opcode `0xEA`/`0x9A`) with that label's own
*address* baked in as the literal target and the segment hardcoded to
`0`, instead of the intended *indirect* far jump/call through memory
(`0xFF /5` or `/3`) that would actually read the real target
segment:offset from that address at runtime.

**Fixed** in `tools/asm86/assembler.py`'s `_parse_operands`: `jmpf`/
`callf` are excluded from the branch-target-immediate shortcut, so
their operands fall through to the normal mem-vs-imm resolution (same
logic `mov`/`call [reg]`/etc already use) instead.

## 18. `tools/asm86` — a forward-referenced `label equ offset $`-style
constant prescanned with the wrong size, destabilizing jump-size
relaxation — FIXED

Fixing #17 also corrected several previously-broken `callf dword ptr
.xiosmod`/`.netmod`-style indirect calls in `src/kernel/SUPIF.SUP`/
`INIT.SUP` that were silently wrong the same way — but fixing those
changed code sizes enough to expose this second, independent bug:
`src/kernel/SUP.A86` started failing to assemble with `LOAD.SUP:78:
conditional/loop branch out of range (rel=129) after relaxation` —
`tools/asm86`'s own short-jump relaxation pass (#6) is supposed to
catch exactly this and grow the jump to its long form, so a relaxed
jump still landing out of range pointed at a relaxation bug, not
just a source-level distance problem.

Tracing it (via targeted instrumentation of `Assembler._relax_jumps`/
`_do_instruction`, not kept) found the same `SHORT_JUMP_MNEMONICS`
occurrence computed at a different address in the final encoding pass
than in the last (converged, stable) tentative relaxation pass — a
genuine instability the relaxation loop can't see, since it only
re-checks jump *ranges*, not whether every instruction's *size* is
actually stable pass to pass. The culprit: a `callf dword ptr
.netmod`-style operand encoded 5 bytes (a direct far call, `0x9A`, the
#17 bug's own fallback) during early/tentative passes — because
`.netmod` was still a forward reference there, and `_prescan_sizes`'s
heuristic for an `EQU`'s eventual size only recognized an explicit
`byte ptr`/`word ptr` cast, not `netmod equ (offset $)`'s implicit
word-sized value — but 4 bytes (the real, correct indirect `0xFF /3`
encoding, #17's fix) once `.netmod` was fully resolved in the final
pass. That one-byte difference shifted every address after it for the
rest of the file.

**Fixed** in `_prescan_sizes`: an `EQU` whose expression uses the
`offset` keyword anywhere, or is exactly `$`, now prescans as a
2-byte value (matching what `offset <anything>`/`$` always actually
evaluate to: a 16-bit address or address difference within one 64KB
segment) even without an explicit cast — the same size `word ptr`
would have declared, just inferred from the expression shape instead.

## 19. `tools/asm86` — a segment-override prefix (`mov cs:foo,al`)
landed *after* the opcode instead of before it, corrupting the
instruction stream — FIXED

Found continuing from #16/#17 once `INIT` reliably reached `SPECIFY`:
booting now looped forever printing the same few debug characters,
and a register read back right after a `call`/`ret` through
`fdc_send_cmd` showed `SP` jumping by ~3000 bytes across one `call`,
despite every instruction in that call chain checking out byte-for-
byte correct by hand. Found by disassembling `fdc_out_byte`'s own
assembled bytes directly: `mov cs:outbyte_tmp,al` (this file's own
NEC765 driver, shared with `src/kernel/XIOS86Q.A86`) encoded as
`88 2E 06 ...` — opcode first, *then* the `CS` prefix byte (`0x2E`) —
instead of `2E 88 06 ...`. A segment-override prefix has to be the
very first byte of an instruction; with it stuck in the middle, the
CPU decoded the prefix byte as part of the `ModRM`/displacement
instead, corrupting that instruction and everything decoded after it
(here: scribbling over the stack via a garbage effective address).

The cause: every mnemonic-specific encoder in `isa.py` built its
result as `bytes([opcode]) + encode_rm(mem_operand, ...)`, and
`encode_rm` included the segment-prefix byte as part of *its own*
return value — landing it after the opcode that already got emitted,
for every mnemonic with a memory operand that carries a segment
override (not just `MOV`).

**Fixed** centrally in `isa.py`: `encode_rm` no longer emits the
prefix at all; the public `encode()` entry point now computes it once
from the operand list and prepends it to whatever the mnemonic-
specific encoder (now `_encode_body`) returns, for every mnemonic
uniformly.

## 20. `src/loader/LDBDOS.A86` — the exact same `org 0` addressing bug
as #15, the opposite way around — FIXED

With #16-#19 fixed, `INIT`/`LDMPM.A86`'s own handoff worked and the
real "MP/M-86 V2.0 Loader" signon message finally printed — but the
very next thing `LDMPM.A86` does, printing it via `LDBDOS.A86`'s own
`BDOS` entry point (`int 224`), hung. An earlier fix (see `tools/
build_mpmldr.py`'s own docstring/history) zeroed this file's
`bdosoffset` (its own `org` self-positioning target) specifically to
stop `tools/asm86`'s raw-binary `org`-padding from landing this
module's real entry code at the wrong offset — but zeroing it just
traded that bug for the #15 one instead: `LBDOSENT.A86`'s `lea
sp,endstack` (and every other local `offset`/direct reference in this
module) then computed an address relative to module-local `0`, not
this module's real position (`ldbdos_offset`) in the concatenated
`MPMLDR.bin`, pointing `SP` ~0x3ef bytes too low — into `LDMPM.A86`'s
own code, the identical failure mode #15 fixed for `LDBIOS86Q.A86`.

**Fixed** the same way as #15: `bdosoffset` is now this module's real,
self-corrected absolute position (kept in sync by `tools/
build_mpmldr.py`, which strips the resulting leading zero-padding
before concatenating); `biosoffset` (an absolute `CALL`/`JMP` target
into `LDBIOS86Q.A86`'s own jump table) is self-corrected the same way
it already was. `LBDOSDAT.A86`'s own data, previously declared in a
separate `dseg` that `tools/build_mpmldr.py` silently dropped (same
issue as #15), was merged into `cseg` too.

## 21. `tools/asm86` — `IRET` was never implemented at all, and
silently assembled to *zero bytes* instead of erroring — FIXED

Found continuing from #20, once `LDMPM.A86`'s own handoff into
`LDBDOS.A86`'s `BDOS` entry point (`int 224`) finally ran: the real
"MP/M-86 V2.0 Loader" signon message printed (further than boot had
ever gotten), but control never returned to `LDMPM.A86` afterward.
Narrowed with debug probes (not kept) bracketing `LBDOSENT.A86`'s own
`bdose:` exit path: every step up to and including `iret` itself ran
(confirmed reached), yet whatever came *after* `iret` never did. A
register read back from memory right there showed `SP` short by
exactly 6 bytes — precisely the three words (`FLAGS`/`CS`/`IP`) a real
`IRET` pops that this one apparently didn't.

The cause: `tools/asm86/isa.py`'s `NO_OPERAND` table (single-byte
opcodes with no operand — `ret`, `retf`, `nop`, `cli`, ...) never had
an entry for `iret` at all, and `ALL_MNEMONICS` (built from that same
table) didn't recognize it as a mnemonic either — so `iret` in source
wasn't treated as an unknown instruction (which would have raised a
clear error); `parse_statement` instead read it as a bare, harmless
label definition with nothing after it, and the assembler silently
emitted *no bytes* for it at all. Every file using `iret`
(`LBDOSENT.A86`, `src/kernel/SUPIF.SUP`, `src/kernel/DSPTCH.RTM`, and
two `src/toolchain/` files) was affected the same way, each one byte
short exactly where its own `IRET` should have been.

**Fixed** by adding `'iret': 0xCF` to `NO_OPERAND` in `tools/asm86/
isa.py`.

## 22. QEMU's emulated floppy controller stalls partway through a PIO
sector read — FIXED (switched to real 8237 DMA transfers)

Found continuing from #21: with `IRET` fixed, booting progressed much
further — through `LDBDOS.A86`'s disk-select (`SELDSK`) and the real
`LDBIOS86Q.A86` `HOME` (recalibrate/seek) call, into `LDBDOS.A86`'s
own directory-initialization scan (`LBDOSF2.A86`'s `initialize`),
which issues the first real sector *data* read (as opposed to a seek)
of this whole boot sequence. That hung: `LDBIOS86Q.A86`'s
`fdc_pio_in` (polling `fdc_in_byte` 512 times for one sector) read
the same, small, *constant* number of bytes (7, confirmed via a
per-byte progress probe, not kept) every time before `fdc_in_byte`'s
own `MSR` poll (`fib_wait`) never saw `RQM`+`DIO` go ready again —
reproduced identically with an added delay between each byte read
(also not kept), ruling out a timing race: a genuine QEMU NEC765/82077
emulation behavior this driver's pure-PIO, no-DMA approach (`SPECIFY`'s
`ND` bit set) doesn't satisfy past an initial FIFO-sized burst.

**Fixed** by implementing real 8237 DMA-driven transfers instead of
polled PIO: a new `dma_setup`/`dma_setup_read`/`dma_setup_write` in
`LDBIOS86Q.A86` programs DMA channel 2 (mask, clear flip-flop, mode,
address split into a 16-bit address + an 8-bit page register computed
from `secbuf`'s real physical address `CS*16 + offset(secbuf)`, count)
before each `READ DATA`/`WRITE DATA` command; `specify_parms`'s `ND`
bit is cleared (DMA mode) to match; `fdc_pio_in`/`fdc_pio_out` collapse
into one routine that only drains the 7-byte PIO *result* phase every
`READ`/`WRITE DATA` command still has regardless of DMA vs PIO data
transfer.

That alone led to a second, related hang in the exact same
call sites: `READ`/`WRITE`'s own trailing `SENSE INTERRUPT STATUS`
call (after the command's own result phase was already drained) hung
the same way #16 already diagnosed for a different call site — no
genuine pending interrupt for QEMU's model to report. Removed (the
command's own result phase already *is* its status; a further SIS
isn't needed or expected here).

With both fixed, booting gets past the entire floppy I/O layer —
seek, DMA read, directory scan, `MPM.SYS` file lookup — and reaches
real file-content validation, reporting `Bad Header Record in MPM.SYS`
(a data/filesystem-format mismatch, not a hang; see the status section
below).

## 23. `tools/build_disk.py` — `MPM.SYS`'s own header recorded its code
and data segment lengths in bytes, where `LDMPM.A86` expects
paragraphs — FIXED

Found continuing from #22: with the floppy I/O layer finally working
end to end, booting reached `LDMPM.A86`'s own header-sanity check on
the real `MPM.SYS` it just read -- and failed it every time, printing
`Bad Header Record in MPM.SYS`. Confirmed by reading back the header
fields `LDMPM.A86` actually parsed (debug probes, not kept): `ctype`/
`dtype` were correct, but `cldseg + clen` (code segment plus code
length -- `LDMPM.A86`'s own check that this equals `dldseg`, i.e. "the
data segment starts immediately after the code segment") came out
nowhere close to `dldseg`.

The cause: `tools/build_disk.py`'s `build_two_segment_body` wrote
`len(code)`/`len(data)` -- raw byte counts -- into the header's
`clen`/`dlen` fields. `LDMPM.A86`'s own arithmetic only makes sense if
those are paragraph counts: `cldseg`/`dldseg` are segment (paragraph)
values, and `LDMPM.A86`'s later length check (`lenok:`) independently
confirms the convention by shifting the real bytes-read count right 4
(converting it to paragraphs) before comparing it against
`clen + dlen`.

**Fixed**: `clen`/`dlen` are now paragraph counts
(`ceil(len/16)`), and the code/data bytes written to disk are padded
out to a whole paragraph each (not left at their raw lengths) --
needed because `LDMPM.A86` loads the body as one continuous sequential
read, so data only actually lands at the memory address `dldseg`
promises if the code portion on disk really is `clen` paragraphs long,
padding included; skipping the pad would also make the `lenok:` check
fail falsely whenever a segment isn't already a whole number of
paragraphs.

## 24. `src/loader/LDBIOS86Q.A86`/`src/kernel/XIOS86Q.A86` --
`rw_physical` decremented an already-0-based sector number, silently
reading from one 128-byte logical record too early every time — FIXED

Found immediately after #23: the header check now passed, but the
*data* `LDMPM.A86` loaded for `MPM.SYS` was still wrong partway through
diagnosing #23 -- before #23's own fix, the very first sector of real
`MPM.SYS` *data* read back as all `E5`/`00` bytes (the CP/M "available
directory entry" filler, confirmed via debug probes printing the
actual bytes), i.e. empty disk space rather than file content, even
though `LDBDOS.A86`'s own file-open had just correctly found the file
and filled in real, sane block pointers (also confirmed by printing
them). Tracing the exact CHS this computed for that read (another
probe, not kept) against hand-computed arithmetic for exactly where
`tools/build_disk.py` physically placed `MPM.SYS`'s first data block
found it one 512-byte physical sector short -- landing in the *tail*
of the reserved directory block instead (mostly unused entries, hence
the `E5` bytes) each time, by exactly one 128-byte logical record.

The cause: `rw_physical`'s very first instruction, `mov al,sect ! dec
al`, assumed `sect` arrives 1-based and needs converting to 0-based --
documented as such in `XIOS86Q.A86`'s own header comment for this
routine. But `src/loader/LBDOSF1.A86`'s `seek` (real, unmodified DRI
source -- the only caller, via `SETSEC`, that ever sets `sect`) passes
the raw, already-0-based `DX` remainder from `div sectpt` straight
through with no `+1` anywhere -- so the extra `dec al` silently
wrapped `sect=0` to `0xFF` and quietly shifted every other value down
by one logical record, confirmed by testing in QEMU: directory reads
still mostly worked (the whole directory block is reserved space, so a
one-record-off read often still landed on *some* real directory data),
but `MPM.SYS`'s own first data sector did not.

**Fixed** by removing the erroneous `dec al` in both files' identical
`rw_physical` (`sect` really is 0-based, matching every real,
unmodified caller) and correcting the header comment in
`XIOS86Q.A86` that had documented the wrong convention.

With #23 and #24 both fixed, `LDMPM.A86` loads the real `MPM.SYS` off
disk completely correctly (confirmed: the real "Code Paragraph
Address"/"Data Paragraph Address"/"Last Paragraph" memory-map lines it
only prints after a fully successful load) and jumps into the actual
MP/M-86 kernel for the first time this project has ever gotten that
far -- which is also where it currently stops (silently, no further
output): `src/kernel/SUP.A86`'s own kernel initialization has never
been exercised in any form before this, a new frontier past everything
#1-#24 covered.

## 25. `src/loader/LDMPM.A86` — `jmpf mpm` read the far pointer to jump
to from the wrong segment — FIXED

Found continuing from #24: with `MPM.SYS` loading completely correctly,
`LDMPM.A86` reached its own final `gompm: jmpf mpm` -- and execution
never reached `MPM.SYS`'s own `init:` (its very first instruction,
`src/kernel/INIT.SUP`) at all, confirmed by a debug probe placed at
that label never printing.

The cause: `mov ds,dldseg` (a few lines earlier, switching `DS` to
MP/M's own data segment so the file's data can be validated/printed)
runs *before* `jmpf mpm`, but `mpm`/`mpmstart`/`mpmcseg` are themselves
data in `LDMPM.A86`'s own (now former) `DS` segment. An unprefixed
`jmpf mpm` therefore read the far pointer to jump to out of the *MP/M*
data segment instead of `LDMPM.A86`'s own -- garbage, not
`{mpmstart,mpmcseg}`.

**Fixed** by changing it to `jmpf cs:mpm`, the same `cs:` override
already needed a few lines above it for `cs:.fcb1` for the exact same
reason.

## 26. `tools/build_mpmsys.py` — the module table's far-pointer
segments were never patched at all, despite a stale comment claiming
they were — FIXED

Found continuing from #25: with the kernel entry point finally reached
(debug probes placed at `INIT.SUP`'s `init:` now printed), execution
still never reached `RTMIF.RTM`'s own `init:` via `INIT.SUP`'s `callf
dword ptr rtmmod[bx]`. Reading back the module table's actual far
pointers directly from the built `MPM.bin.dseg` (`struct.unpack`)
showed every single one with segment `0000`.

The cause: `tools/build_mpmsys.py` wrote `0` as the segment half of
every module table far pointer, next to a comment claiming "Segment is
patched in below, once" -- no such patch existed anywhere in this
script or in `tools/build_disk.py` (checked). Every intermodule call
through the table (every "xiosif"/"mpmif"/"supif"-style trampoline,
down to `INIT.SUP`'s own first module-init call) therefore jumped to
physical address `0x00000 + offset` -- straight into the IVT/BIOS data
area.

**Fixed** by adding a `MPM_CODE_SEG = 0x1000` constant (matching
`tools/build_disk.py`'s own segment choice for where the combined code
actually loads) and writing it as the segment half of every entry/init
far pointer and the `dispatcher` field, instead of `0`.

## 27. `tools/asm86` — a bare `BYTE`/`WORD`/`DWORD` size-literal used in
field-offset arithmetic leaked its own byte-width onto the resulting
EQU's size, misclassifying later bare references to it as memory
operands — FIXED

Found continuing from #26: with the module table's segments correctly
patched (confirmed via `struct.unpack` showing `1000` everywhere), the
hang at `RTMIF.RTM`'s `init:` persisted. A debug probe placed directly
on `BX` right before `INIT.SUP`'s `callf dword ptr rtmmod[bx]` showed
`BX` holding garbage instead of `4` (`mod_init`'s real value).

The cause: `MODTAB.DEF`'s `mod_init equ mod_entry + dword` is pure
offset-arithmetic (intentionally dimensionless -- it's meant to be
used as a plain index, e.g. `mov bx,mod_init`), but the bare `dword`
term's *value* (4, its own byte-width) was also being returned as that
Value's `size` attribute in `tools/asm86/expr.py`'s expression parser.
`size` doubles as a "this came from a sized memory reference" flag
elsewhere in the same file, and `parse_add`'s `v.size or r.size`
propagation carried that spurious size onto `mod_init`'s own stored
EQU symbol -- with no `byte`/`word ptr` cast and no `offset`/`$`
anywhere in its definition (the only two cases `tools/asm86/
assembler.py`'s own prescan heuristic, added for #18, treats as
"really is a memory-sized constant"). A later bare `mov bx,mod_init`
then classified as a memory operand instead of an immediate, assembling
as `mov bx,[0004]` (a direct read of whatever word happens to live at
absolute offset 4) instead of `mov bx,4`.

**Fixed** by having the bare `byte`/`word`/`dword` literal return only
its numeric value, not also stamp that value as the Value's `size` --
matching every one of this convention's other users in the tree (every
`XXXlen equ ... + byte/word/dword` struct-length constant), none of
which are meant to describe a sized memory reference either.

## 28. `src/loader/LBDOSF1.A86`'s `seek` — read `SECTRAN`'s return
value from the wrong register, corrupting every physical sector number
whenever disk sector translation is disabled — FIXED

Found continuing from #27: with the module table and `mod_init` both
confirmed correct, `RTMIF.RTM`'s `init:` was finally reached (and ran
successfully) -- but `MPM.SYS`'s own *data* segment, read back from
memory at runtime, still didn't match what was verified correct on
disk: the whole data segment (everything from the code/data boundary
onward) was wrong, while the entire (much larger) code portion,
spanning many tracks, loaded perfectly.

Extensive debug probing (temporary, not kept) of `LDBIOS86Q.A86`'s
`READ`'s destination addressing (`dma_seg`/`dma_adr`, printed for
every one of the ~235 sector reads this load needs) showed it
advancing correctly and contiguously the entire time -- ruling out the
destination side entirely. Printing the *physical* sector number
(`cur_psec`) computed for each read instead showed it climbing as high
as `64`, impossible for this 18-sectors/track floppy; printing `seek`'s
own `arecord`/`sectpt`/`DIV` remainder showed that intermediate value
correctly bounded to `0..71` the entire time.

The cause: `seek` computes the real 0-based sector in `DX` (verified
correct), passes it to `SECTRAN` for translation, then reads
`SECTRAN`'s result from the wrong register -- `mov cx,bx` instead of
`mov cx,ax`. `SECTRAN`'s documented, real return value is always in
`AX`; this disk's own DPB explicitly configures *no* sector
translation (`xlt0 equ 0`), and `SECTRAN`'s "no translation" branch
(`LDBIOS86Q.A86`) only ever sets `AX` (`mov ax,cx ! ret`), leaving `BX`
stale from whatever last used it. Every `seek` therefore fed `SETSEC` a
garbage sector number drawn from leftover `BX`, not the real one --
confirmed by testing: `cur_psec` values well outside the real
1-18 range, yet never erroring, because the stale `BX` values
"happened" to still decode to *some* FDC-acceptable sector, just the
wrong one, silently returning whatever was really at that wrong
location instead. (`SECTRAN`'s *translation-table* branch happens to
leave the correct value in `BX` too, built directly in `BL` by the
table lookup -- which is presumably why this went unnoticed in the real
DRI source for as long as it did: a DPB with real sector translation
configured never exercises the branch that drops it.) No file in this
tree had been big enough, read through this exact path, to notice
before `MPM.SYS`.

**Fixed** by changing it to `mov cx,ax`, matching `SECTRAN`'s actual,
documented return convention in both of its branches.

## 29. `tools/build_mpmsys.py` — RTM's "unused list" heads (`pul`,
`mdul`, `qlr`) inherit stale, actively-wrong values from the real
system's `DATA.BDO` template, and `qul` has no real pool to point to
at all — FIXED (partially; see below)

Found continuing from #28: with `MPM.SYS` now loading byte-for-byte
correctly, booting reached `RTM`'s own `init:` (confirmed: all three
debug probes bracketing its `proc_creat` call -- before, during, and
after -- agreed), but the module table's `xiosmod` entry (`sysdat`
offset `0x28`) still ended up corrupted by the time `INIT.SUP` tried
to use it, moments later.

A QEMU hardware watchpoint on that exact address (`watch
*(unsigned short*)<physical sysdat+0x28>`, via `-s -S`/gdb) caught the
corrupting write directly: a `rep movsw` inside RTM's `getpdadr`
(`PROC.RTM`), copying 48 bytes (`pdlen`) from `idlepd` to absolute
`sysdat` offset `0x20` -- squarely inside the module table (`0x20` is
`bdosmod`, with `xiosmod` right after it at `0x28`). `getpdadr`'s
`gpd_bad`/`gpd_have` fallback trusts `pul` (the free-PD-slot list
head) as a real destination address whenever it's non-zero; dumping
`SYSDAT.A86`'s own raw assembled `dseg.bin` found `pul` baked in as
`0x0020` -- a stale leftover from whatever the real system's `DATA.BDO`
template (real, unmodified DRI source) captured it as, not the empty
list ("0") a fresh boot actually has. Checking every other "unused
list"/"xxx root" field in `SYSDAT.FMT` the same way found two more:
`mdul` (Memory Descriptor Unused List) and `qlr` (Queue List Root)
are *also* non-zero when they should start empty.

**Fixed** `pul`/`mdul`/`qlr` by resetting them to 0 at build time.
`qul` (QCB Unused List) needed more: unlike `pul` (whose only real
caller here, creating RTM's own `tick`/`idle` processes, never
actually hits the fallback -- their PDs are already sysdat-resident),
`getqdaddr`'s own `gqd_bad` fallback (`QUE2.RTM`) genuinely fires the
first time `BDOSIF.BDO`'s own init creates its "mxdiskqd" queue (that
QD lives in BDOS's own code segment via `DATA.BDO`, not in `sysdat`,
so `getqdaddr` always treats it as "foreign" and tries to copy it into
a real system-table slot). This tree never had such a slot -- there's
no GENSYS equivalent anywhere here to have built one. **Fixed** by
writing a small real 4-slot QD pool directly into existing slack space
in `SYSDAT`'s 4096-byte `dseg` (starting right after `SYSDAT.FMT`'s own
last declared field, `mxmemcount`, computed via `asm86 --symbols`
rather than hardcoded) and linking it as a proper free list through
`qul`.

This is only a **partial** fix: `BDOSIF.BDO`'s "create disk mx queue"
(`qmake`) now reaches its following debug marker either way (clean
`e_no_qd`/success both return control the same way, since the caller
never checks), but the *next* call, "open mx disk queue" (`qopen`, via
`openq_entry` in `QUE1.RTM`), now hangs with no further output at all
-- a new, different, not-yet-diagnosed hang, confirmed to be a true
hang (no output) rather than a repeat/reset (ruled out directly: a
probe printing the *incoming* `SS:SP` at `INIT.SUP`'s own `init:` only
ever fires once per boot). `openq_entry`'s own queue-list walk
(`qo_nqd: mov si,q_link[si] ...`) looks structurally incapable of
looping forever with only one queue linked, so the cause is still
open -- possibly another address-translation bug in the same family
as #28/this one, triggered somewhere between `qmake` successfully
linking the queue and `qopen` trying to find it again.

## 30. Six genuinely broken module-dispatch mechanisms, all cousins of
#28/#29's "assembled standalone, never relocated" bug family -- three
source typos plus `tools/build_mpmsys.py` never relocating the
dispatch *instructions* that locate each module's own internal
function table, only the table's own contents -- CONFIRMED, PARTIALLY
FIXED

Found continuing from #29, chasing the `qopen` hang with a QEMU
hardware watchpoint on `QUE1.RTM`'s `openq_entry` (the technique that
found #28): the watchpoint never fired at all. A watchpoint on
`makeq_entry` (`QUE1.RTM`'s *other* queue entry point, called just
before `openq_entry` by the same `BDOSIF.BDO` init sequence) didn't
fire either -- meaning `RTM`'s own `entry:` dispatcher (every
intermodule call into `RTM`, including `qmake` itself) was never
really working, the apparent `qmake`/`qopen` success in #29 having
been a lucky wild jump into harmless bytes rather than real code.

Single-stepping `RTM`'s `entry:` dispatcher (`RTMIF.RTM`) with GDB
found the first bug directly: `mov si,supervisor` (used to cache
`SUP`'s far pointer locally, and -- in a different spot -- to fetch a
process's initial CS) has no `offset` keyword, so `tools/asm86.py`
assembles it as `mov si,[supervisor]` (a memory *read* of whatever is
at that small offset) instead of `mov si,8` (the intended constant
load). `RTMIF.RTM`'s *first* use of this pattern already has `offset`
and works; its own other two uses, plus the *only* use in each of
`MEMIF.MEM`, `CIOIF.CIO`, `BDOSIF.BDO`, and `XIOS.A86`, are all missing
it. Since `supervisor`'s own offset (8) happens to fall inside
`SUP`'s own module-table "supervisor" slot once `SUP`'s module_base=0
is applied, the bogus memory read ends up loading *that* slot's
already-correct value anyway -- except when the module in question is
dispatching an *inter*module call (`RTM`'s own `mov si,supervisor` at
its PD-setup for `tickpd`/`idlepd`, used to read back a process's
initial CS), where it instead silently loads garbage from SYSDAT
offset 8 (`rtmmod`'s own far pointer) as if it were a segment value.
**Fixed** by adding the missing `offset` everywhere (`RTMIF.RTM`
×2 more, `MEMIF.MEM`, `CIOIF.CIO`, `BDOSIF.BDO`, `XIOS.A86` ×1 each).

The same bug, same assembler quirk, found a second source bug:
`BDOSIF.BDO`'s `entry:` does `mov ax,btablen` (meant to load the
3-byte-per-entry function-table stride as a constant) with no
`offset`, so it loads whatever's at SYSDAT offset 3 instead of `3`,
corrupting the `mul cx` right after it (and so every BDOS dispatch's
table index). **Fixed** the same way (added `offset`).

A third, different bug in the same dispatcher family: `XIOS86Q.A86`'s
`entry:` does `call functab[bx]` with **no segment override at all**,
so (`[bx]` defaulting to DS, not CS) it reads the call target from
whatever DS happens to be -- `sysdat`, at the point XIOS's `entry:` is
actually reached -- instead of from XIOS's own code segment. **Fixed**
by adding the `cs:` override this needed (`call cs:functab[bx]`),
matching every other module's own dispatcher.

With those three source bugs fixed, GDB still found `RTM`'s `entry:`
dispatcher jumping into garbage instead of `makeq_entry` on the very
next call. Tracing it with `set architecture i8086` and single-stepping
found why: `call word ptr cs:[si+0107h]` (`entry:`'s own "call through
my function table" instruction -- `0107h` is `function`'s *standalone*
`RTM.A86` offset) is a *second*, distinct kind of unrelocated-address
bug from #29's `FUNCTION_TABLES` fix. That fix already correctly adds
`module_base` to every *value stored in* the table -- but the
dispatch instruction that *locates the table itself* still points at
bare offset `0107h`, not `module_base+0107h`. Since `CS` is the one
`MPM_CODE_SEG` shared by every module, `cs:[0107h]` always resolves
inside **`SUP`'s** own code (module_base 0) regardless of which
module's dispatcher is running -- i.e. every non-`SUP` module's
internal function-table dispatch was reading 21-47 words of
essentially random `SUP` bytes as if they were its own relocated
table, the entire time, `FUNCTION_TABLES`' own fix notwithstanding.
`RTM`/`MEM`/`CIO` encode this as `2E FF 94 <table_off>` (`call word
ptr cs:[si+table_off]`); `XIOS` (after the `cs:` fix above) as `2E FF
97 <table_off>` (`cs:[bx+table_off]`); `BDOS` as the immediate-load
`C7 C6 <table_off>` (`mov si,offset functab`). **Fixed** in
`tools/build_mpmsys.py`: for each `FUNCTION_TABLES` module, after
relocating the table's own contents, find that exact byte pattern
(unique-match-checked, same discipline as every other patch in that
script) and add `module_base` to its own embedded `table_off`
immediate too.

**Confirmed fixed by this**: `RTM`'s own `entry:` dispatch now
genuinely reaches `makeq_entry` (`getqdaddr`'s own probes -- `I`
through `N` -- all fire in the right order), and `qmake` completes for
real, not by lucky accident.

**Resolved in #31** (see below): the `module_map` misread wasn't an
in-line write or an async interrupt at all -- it was `tools/asm86`
silently defaulting every bare `ESEG`-declared symbol reference (not
just `module_map`, every UDA field) to `DS` instead of `ES`, the same
"never implemented" gap as `ESEG` itself once was (`tools/README.md`).

## 31. The real `ESEG`-default fix, three more dispatch tables/instructions
in the same unrelocated-address family, and five independent, unrelated
bugs found chasing the boot sequence from "first `MEM` dispatch" to
"the last instruction `INIT.SUP` executes" -- MOSTLY FIXED, one new
bug still open

This is one long continuous debugging session picking up exactly where
#30 left off (the `module_map` mystery), so its findings are grouped
here as one entry rather than split by exact chronology.

**The real `module_map` fix**: `tools/asm86` never implements an
`ASSUME`-style default segment for memory operands -- confirmed by
reading `tools/asm86/expr.py`'s `parse_operand`, which only ever
applies an explicit `es:`/`cs:`/etc. prefix already written in the
source; the real DRI `ASM86`'s `ESEG`, by definition, means "wherever
ES currently points," so a bare reference to an `ESEG`-declared symbol
is supposed to default to `ES`, with no explicit prefix needed anywhere
-- and nothing in this entire source tree ever writes one. Confirmed
directly: `u_func`/`u_wrkseg` (`UDA.FMT`, declared under `ESEG`) read
via `cs:[6]`-style disassembly showed plain `[6]` with no override at
all, meaning every bare UDA-field reference anywhere in the tree was
silently `DS`-relative instead of `ES`-relative the whole time -- which
is also, in hindsight, why `MEMIF.MEM`'s own `pf_tempkeep` save/restore
(`push p_flag[si]` et al -- *not* a UDA field, so unaffected by this
specific fix, but neighboring code that first made this visible)
looked consistent in isolation while the *real* bug was a module away.
**Fixed** in the assembler itself (not a source workaround): `Symbol`
now carries which segment (`cseg`/`dseg`/`eseg`/`aseg`) it was declared
under (`Assembler.define_symbol`), and a new prescan (`Assembler.
_prescan_sizes`'s own `preseg`, parallel to its existing `presize`/
`prekind`, needed for exactly the same forward-reference-stability
reason already documented there) lets `parse_operand` default a bare
reference to `es:` whenever the symbol's segment is `eseg`, in both the
bracketed (`label[reg]`) and plain forms. Confirmed fixed: `module_map`
reads `0x3F` correctly on both the first (`RTM`) and second (`MEM`)
dispatch now, and `MEM`'s own dispatch succeeds for the first time.

**`DSPTCH.RTM`'s own `dsp_table`** (the process-status-code -> handler
jump table `dsptch` uses, reached for real for the first time once
`RTM`'s own `entry:` dispatch started working) is a *second*, separate
internal jump table in `RTM.A86`, not covered by #30's `FUNCTION_TABLES`/
`DISPATCH_PATCHES` (which only knew about `RTM`'s *external* `function`
table) -- same unrelocated-table-contents-and-locating-instruction bug,
same fix, in `tools/build_mpmsys.py`.

**Five further, independent, confirmed bugs**, found in order while
tracing the boot sequence past `MEM`'s now-working dispatch, each
blocking the next step until fixed:

1. `MAU.MEM`'s `qmau` (`SYSDAT.FMT`'s MAU for queue buffers) has the
   same never-initialized-real-backing-memory problem as #29's
   `pul`/`mdul`/`qlr`/`qul`: `m_start`=0 (absolute segment 0, the real
   interrupt vector table) instead of a real buffer. **Fixed** in
   `tools/build_mpmsys.py`: carves `QMAU_BUF_SIZE` bytes out of
   `SYSDAT`'s own slack space (paragraph-aligned, right after #29's QD
   pool) with a from-scratch Suballocation Table -- `QMAU_SAT_ENTRIES`
   reserves real slack beyond the one real entry, needed because
   `MAU.MEM`'s own `mau_insert` (used whenever an allocation has to
   *split* a free entry) only succeeds if the table has an all-zero
   slot beyond the declared entries to grow into, confirmed by reading
   it directly -- a 1-entry table fails every allocation that doesn't
   consume an entry's *entire* length.
2. `XIOS86Q.A86`'s `pmsg` (prints `signon` at the end of `init:`) uses
   `bx` as its own message-pointer loop variable across a `call conout`
   whose own contract (`bh`/`bl` = `c0outdev`) doesn't preserve it --
   confirmed by probe: the signon message printed one garbage byte and
   stopped. **Fixed** by saving/restoring `bx` around that one call.
3. The same `init:` ends with a bare `pop ds` with no matching `push`
   anywhere in the routine (the *other* push/pop ds pair, for a brief
   segment-0 IVT-hook access, is already self-balanced) -- confirmed by
   probe: nothing past it ever executed; it was popping the far return
   address's own IP word into DS, leaving the following `retf` to pop
   the wrong two words as its own CS:IP. **Fixed** by deleting it.
4. `VEC.FMT` (the real, absolute interrupt-vector-table overlay, used
   by "set DS to 0 and use the following variables") has no `org 0` of
   its own, so its fields land wherever the DSEG location counter
   already is when it's included -- in `SUP.A86`, 60h bytes into
   `BASEP.FMT`/`LSTK.FMT`'s own content, so `i_mpm_ip` (`INT mpmint`'s
   real IVT entry) resolved to 3E0h instead of the real 380h
   (`mpmint`*4) that `INT 0E0h` actually reads: the vector `INIT.SUP`
   needs to terminate the init process was never actually installed.
   **Fixed** by adding the missing `org 0`.
5. `QUE2.RTM`'s `qspace` and `SUPIF.SUP`'s own `mpmif` (used by *every*
   intermodule call) both do a bare `mov ds,sysdat` to restore/bootstrap
   DS -- discovered while tracing why DS read back as pure garbage (not
   0x0000 by coincidence, an actual uninitialized stack word) the one
   time `qspace` calls through `mpmif` with DS deliberately pointed at
   SS instead of SYSDAT (for an on-stack MPB). `sysdat` is declared
   identically in every module's own CSEG, but is **not** a per-module
   cache the way `supervisor` is: every module's own init-time
   `mov sysdat,ax` is *also* a bare write, so at cold boot (where DS
   genuinely is SYSDAT) it writes into SYSDAT's own self-referential
   offset 6 -- not into that module's CSEG cell -- and every later bare
   read, as long as DS is *already* SYSDAT-ish, finds that same
   (coincidentally self-consistent) value there. It only breaks when
   something calls through with DS deliberately not SYSDAT-ish, as
   `qspace` does. **Fixed**: `qspace` now saves/restores the real DS
   around its own DS=SS window via the stack (timed so it doesn't
   disturb the on-stack MPB's own addressing); `SUPIF.SUP`'s `mpmif`
   now reads/writes its own cold-boot value via `cs:sysdat` instead of
   bare -- safe for `SUP` specifically only because its `module_base`
   is 0, so no `tools/build_mpmsys.py` relocation patch is needed for
   this one (unlike every other CS-relative fix this tree has needed).

**Net result this session**: boot now gets all the way through
`INIT.SUP`'s own sequence -- every module's `init:` (`RTM`'s `MEM`'s,
`CIO`'s, `BDOS`'s including its own `qmake`/`qopen`/`qwrite` sequence,
and `XIOS86Q`'s, which now actually arms the UART/PIT/floppy controller
and prints its real signon banner end to end) -- through every
`INIT.SUP` marker (`'1'`-`'D'`), to `INIT.SUP`'s own final instruction,
`int mpmint` asking to terminate the init process. `RTM`'s own
`terminate_entry` (`ABORT.RTM`) is reached correctly (confirmed by
probe) and correctly calls `abt_chk` to check whether the init process
may actually terminate now.

**Still open**: `abt_chk` rejects it (`cx`=0FFFFh, "tempkeep") because
the init process's own `p_flag` already has `pf_tempkeep` (0200h) set
by this point -- confirmed stuck as early as `INIT.SUP`'s own marker
`'6'` (immediately after `BDOSIF.BDO`'s `init:` returns), i.e. set
somewhere during `BDOSIF.BDO`'s own `qmake`/`qopen`/`qwrite` sequence.
Narrowed by probe: `MEMIF.MEM`'s own `pf_tempkeep` set/clear (around
its `f_maualloc` work, inside `qmake`) is confirmed clean -- `p_flag`
reads back `0x0000` right after its own cleanup, and again after its
own `f_qwrite` lock-release call -- so the bit gets set *after* `qmake`
returns, somewhere in `qopen`/`qwrite` or back in `BDOSIF.BDO` itself.
`QUE1.RTM`/`QUE2.RTM` (openq_entry/writeq_entry, and everything they
call) never reference `p_flag`/`pf_tempkeep` at all by direct grep, so
the write isn't in-line there either -- not yet resolved. The
`qspace`/`mpmif` DS-bootstrap fix above (found and fixed while chasing
*this* bug) was necessary but not sufficient; it's possible there's
one more occurrence of the exact same "bare `mov ds,sysdat` when DS
isn't already reliable" bug still lurking somewhere in this remaining
span, not yet found. Debug probes left in place throughout (`INIT.SUP`'s
`'!'`, `BDOSIF.BDO`'s `'?'`/`'['`, `ABORT.RTM`'s `'$'`/`'%'`/`'^'`/`'&'`/
`'*'`/`'+'`/`'='`, `DSPTCH.RTM`'s `'~'`, `MEMIF.MEM`'s `'n'`/`'o'`
`p_flag` dumps) are ready for picking this up again.

## 32. `#31`'s "`pf_tempkeep` stuck" symptom was a debugging-instrumentation
artifact, not a real kernel bug -- real root cause found and fixed; a
second, independent, real `module_table` corruption bug found, not yet
fixed

**Context**: picking up exactly where #31 left off (the `pf_tempkeep`
"stuck set" mystery, `abt_chk` rejecting `RTM`'s `terminate_entry`).

**The `pf_tempkeep` mystery was never real.** All of #31's "confirmed
clean" / "confirmed stuck" readings came from the free-running serial
trace (`qemu-system-i386 ... -serial file:...` with no `-S`/single-
stepping). Verified this session, with a GDB hardware breakpoint/
watchpoint session against the live QEMU gdbstub (`-s -S`, `target
remote localhost:1234`), that **QEMU's UART emulation silently drops
bytes written back-to-back at full CPU speed** -- the same `push ax !
mov al,'#' ! mov dx,03f8h ! out dx,al ! pop ax`-style single-character
probes that print correctly throughout *most* of the trace sometimes
vanish entirely when several fire in close succession (confirmed with
`SUPIF.SUP`'s own `'#'`/`'Y'`/`'Q'`/`'Z'` probes: all four are silently
missing from a full-speed run's serial log immediately after `'@'`,
even though single-stepping the *identical* code path through GDB
prints all of them, in the right order, with the right values, every
time). This means: a probe's *presence* in a free-run trace is reliable
evidence it fired; a probe's *absence* is not reliable evidence it
didn't -- exactly backwards from what #29–#31 assumed when ruling
things out ("probe X never fires, so Y never happens"). Confirmed this
is a real QEMU/host-timing effect, not a project bug: verified with the
same binary that single-instruction-stepping (via repeated `stepi`)
never drops a byte, no matter how many probes fire back-to-back.

Re-doing the actual check reliably (GDB hardware watchpoint directly on
the live physical address of the init process's own `p_flag` word, not
a serial probe) showed `pf_tempkeep` is set exactly once, by
`MEMIF.MEM`'s own `entry:` (`or p_flag[si],pf_tempkeep`, its normal
first-acquire-of-the-mxmem-lock path) -- and then **never cleared**,
because the `f_maualloc` call it makes (`QUE2.RTM`'s `qspace`, via
`SUPIF.SUP`'s reliable `cs:sysdat`-based `mpmif`) was failing every
single time, confirmed via a breakpoint on `MAU.MEM`'s `maualloc_entry`:
`DX`, its own live MPB-pointer argument (set by `qspace` just before the
call), read back as `03F8h` -- the serial port I/O address, not a valid
MPB offset. Root cause: **`MEMIF.MEM`'s own `'a'`/`'b'` debug probes,
between the critical section's `havemx:` label and the `call
cs:functab[si]` that invokes `maualloc_entry`, used the single-register
save style (`push ax ! mov al,'a' ! mov dx,03f8h ! out dx,al ! pop
ax`)** -- correct for *most* call sites in this file, where `DX` isn't
live, but `maualloc_entry` is reached here specifically via `qspace`'s
on-stack-MPB trick, which *does* keep its MPB pointer live in `DX` all
the way through this exact dispatch chain. The probe's own `mov
dx,03f8h` permanently clobbered it, with no code anywhere to restore
it, so `maualloc_entry` read garbage and failed every time, leaving
`MEMIF.MEM`'s `entry:` stuck in the "lock held" state (`pf_tempkeep`
still set) for the rest of the boot. This is debugging instrumentation
breaking the very thing it was added to observe -- not a bug in
`BDOSIF.BDO`'s `qopen`/`qwrite` or anywhere `#31` was looking.

**Fix**: `MEMIF.MEM`'s `'a'` and `'b'` probes now also save/restore
`DX` (`push ax ! push dx ! mov al,'a' ! ... ! pop dx ! pop ax`), matching
every *other* probe in this same critical section (`havemx:`'s own
2-hex-nibble dump, `'c'`/`'j'`/`'k'`/`'l'`/`'m'`/`'n'`, etc., all of
which already correctly save/restore every register they touch -- `'a'`
and `'b'` were the only two that didn't). Confirmed fixed: rebuilt and
re-ran under GDB with the same reliable hardware-watchpoint-on-`p_flag`
check -- `maualloc_entry` now receives the real MPB pointer, succeeds,
and `MEMIF.MEM`'s critical section cleanly clears `pf_tempkeep` back to
`0x0000` on exit (confirmed via the existing `'n'`/`'o'` dumps, which
now read `0000` both times in the free-run trace too, for whatever
that's worth given the byte-drop caveat above).

**Net result**: boot now gets measurably further -- past `abt_chk`'s
rejection entirely, into `RTM`'s real `term_act` cleanup logic (freeing
the init process's memory blocks, detaching console/list devices) --
confirmed by new serial-trace content (`MEMIF.MEM`'s own `'h'`/`'i'`
`mpmif` probes and further `SUPIF.SUP` dispatch probes) that was never
reached in any prior session.

**New blocker found, not yet fixed**: boot now hangs a bit further in,
inside this same termination cleanup. Confirmed with a GDB hardware
watchpoint directly on `module_table`'s own live `RTM` segment-field
word (`rtmmod+2` in `SYSDAT`'s dseg) that it is silently overwritten
from the correct `1000h` to `0F00h` partway through boot, from code
inside `SUP.A86`'s `LOAD.SUP` (in the neighborhood of its "chain-load
free old memory" / CMD-header-to-`ldtab` logic, which this session
confirmed shares its field layout correctly with `SYSDAT.FMT`'s own
canonical `ldtab`/`load_pd`/etc. declarations -- so, unlike several
earlier bugs this project, this is *not* another instance of a
module's private scratch colliding with `SYSDAT`'s structure by
accident). The exact faulting instruction was not pinned down with
confidence this session -- two different breakpoints placed near the
GDB-reported faulting address each landed on a *plausible*-looking but
mutually-inconsistent write (one zeroing `ldtab_pd` for a fixed, small
`SI`, matching neither the corrupted address nor the `0F00h` value),
and this GDB/QEMU combination has already shown several cosmetic
reporting quirks elsewhere this session (see `#`-prefixed note in the
project's own debugging notes on `continue` after a watchpoint hit), so
the reported fault PC may not be exact. Once `module_table`'s `RTM`
entry is corrupted, the next far dispatch through it lands at `0F00:
17C5` instead of `1000:17C5` -- not an immediate crash (the segment
still happens to contain *something*, since boot doesn't visibly fault,
just drifts into nonsense dispatches: `ES`/mod/`DS`/`module_map` all
read back as garbage on the next dispatch attempt, and boot goes quiet
after an `e_not_implemented` reject). This is the new, confirmed
blocking point for the next continuation. Next step: re-run the same
GDB hardware-watchpoint session on the live `module_table` RTM-segment
address (`sysdat_segment*16 + 0x0A`, i.e. `rtmmod+2`) and single-step
(not `continue`, to avoid the GDB/QEMU batch-mode "Cannot execute this
command while the target is running" issue already documented
elsewhere in this file) from well before the hit to nail the exact
instruction and its real intent.

## Status of the kernel/loader sources

`src/kernel/CIO.A86`, `SYSDAT.A86`, `BDOS.A86`, `MEM.A86`, `RTM.A86`,
`SUP.A86`, and all four of `src/loader/LDBDOS.A86`/`LDBIOS.A86`/
`LDMPM.A86`/`ROM.A86` now compile and assemble cleanly end-to-end. Every
source-level defect found in `BDOS.A86`, `RTM.A86`, `LDBIOS.A86`,
`LDMPM.A86`, and `ROM.A86` has been fixed (#4, #5, #8, #9, #10) — what
first looked like two genuinely unresolved symbols in #9/#10 turned out,
on closer inspection, to be the same underscore-typo bug class as
everywhere else — and the short-jump-range tooling trade-off that used
to additionally block several of these is also fixed now that
`tools/asm86` does real jump-size relaxation (#6). Only `src/kernel/
XIOS.A86` remains blocked, on one genuinely unresolved symbol
(`dp_base`, #7) with no confident fix available from evidence in this
tree even after a second, closer look — its one fixable typo
(`tick_int`) was corrected.

Booting under QEMU (`make run`) now gets past the boot sector, all of
`MPMLDR.bin` loading, its internal `LDBIOS86Q.A86`→`LDMPM.A86` handoff,
the "MP/M-86 V2.0 Loader" signon message, the entire floppy I/O layer
in `LDBDOS.A86` (disk select, home/recalibrate, DMA sector reads,
directory scan, `MPM.SYS` file lookup), and a complete, verified-
correct load of the real on-disk `MPM.SYS` into memory (#15-#24, #28
above) — no longer hanging or failing anywhere in the loader. It then
jumps into the actual MP/M-86 kernel (`src/kernel/INIT.SUP`'s own
`init:`, #25) and, for the first time this project has ever gotten
this far, successfully initializes every kernel module in turn --
`RTM` (#26, #27), `MEM`, `BDOS`, `CIO`, and `XIOS` -- including `RTM`'s
own process creation (`proc_creat`, called twice: once for the tick
process, once for idle).

With #29's `pul`/`mdul`/`qlr`/`qul` fixes, `RTM`'s own process creation
no longer corrupts the module table (`RTMIF.RTM`'s pre/post-`proc_creat`
far-pointer dump now reads back identical, correct values all three
times), and `INIT.SUP` successfully makes its first *post-init* call
back into `XIOS` (`io_maxconsole` via `xiosif`) to query character-
device counts.

With #31's fixes -- chiefly `tools/asm86` finally defaulting bare
`ESEG` references to `ES` instead of silently reading `DS` -- boot now
runs the *entire* `INIT.SUP` sequence end to end: every module's own
`init:` (including `BDOSIF.BDO`'s full `qmake`/`qopen`/`qwrite`
sequence and `XIOS86Q`'s real UART/PIT/floppy-controller setup and
signon banner), every `INIT.SUP` marker `'1'`-`'D'`, down to
`INIT.SUP`'s own final instruction: `int mpmint` asking to terminate
the init process. `RTM`'s `terminate_entry` is reached correctly and
calls `abt_chk` to check whether that's allowed right now.

#32 found the "`pf_tempkeep` stuck" symptom reported above was itself a
debugging-instrumentation artifact (`MEMIF.MEM`'s own `'a'`/`'b'`
probes were clobbering a live `DX` argument, making every
`f_maualloc` call inside `qmake` fail) -- now fixed. With that fix,
`abt_chk`'s rejection is gone entirely: `RTM`'s `terminate_entry` now
proceeds into real `term_act` cleanup (freeing the init process's
memory, detaching its console/list devices). Boot now hangs a bit
further in, during that same cleanup, because of a second, unrelated,
*confirmed* (via a GDB hardware watchpoint directly on the live memory
address) corruption of `module_table`'s own `RTM` segment field (goes
from the correct `1000h` to `0F00h`, written from somewhere in
`SUP.A86`'s `LOAD.SUP`) -- see #32's own writeup for exactly what's
confirmed and what isn't (the exact faulting instruction wasn't pinned
down with full confidence this session). This is the current, precise
blocking point. Debug probes left in place throughout the now-cleared
path (`INIT.SUP`'s numbered serial markers `'1'`-`'D'` plus `'!'`,
`SUPIF.SUP`'s `'X'`/`'Q'`/`'R'`/`'S'`/`'T'`/`'U'`/`'Y'`/`'Z'`/`'@'`/
`'#'`, `XIOS86Q.A86`'s `'E'`/`'F'`/`'0'`/`'p'`-`'w'`, `BDOSIF.BDO`'s
`'q'`/`'r'`/`'s'`/`'t'`/`'?'`/`'['`, `RTMIF.RTM`'s `'V'`/`'W'`,
`QUE1.RTM`'s `'G'`/`'H'`, `QUE2.RTM`'s `'I'`-`'P'`, `ABORT.RTM`'s
`'$'`/`'%'`/`'^'`/`'&'`/`'*'`/`'+'`/`'='`, `DSPTCH.RTM`'s `'~'`) are
harmless to leave as-is, but given #32's finding, any *new* single-
register-save probe added anywhere a call's arguments are live in `DX`
(or `BX`/`CX`) needs to save/restore that register too -- don't trust
a probe's absence from a free-run serial trace; verify with GDB
single-stepping (not `continue`, which has its own batch-mode
reporting quirks against this QEMU gdbstub) instead.



The RSP sources (`CLOCK.A86`, `ECHO.A86`, `TMP.A86`, `SUBMIT.A86`) and
all 22 `.PLM` utilities in `src/utils/`/`src/toolchain/` compile and
assemble cleanly end-to-end (`ECHO.A86` and every `.PLM` file — see
`tools/plm/README.md`) except where documented above. See
`tools/README.md` and `tools/plm/README.md` for how to run the
assembler/compiler yourself, and the top-level `Makefile`/
`.github/workflows/build.yml` for the automated, continuously-validated
build of everything confirmed to work.
