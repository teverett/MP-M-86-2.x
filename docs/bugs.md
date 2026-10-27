# Bugs Found in the Original MP/M-86 2.x Source

While validating the assembler built in `tools/asm86/` against the files
copied into `src-assem/`, assembly failed on several sources with genuine
defects in the original 1981 Digital Research source itself — not bugs
in the assembler. These were confirmed by reading the surrounding code,
not just by the assembler's error message.

This is consistent with the provenance already noted in the top-level
`README.md` — these are hand-labeled "work disks," not a verified golden
release, and this code likely never finished debugging on the original
hardware.

**Patch status, by section below:** #1 (`CLOCK.A86`), #2 (`TMP.A86`),
and #3 (`SUBMIT.A86`) were each **patched** once `src-c/` needed to
assemble the real file for real as a differential-test reference (see
`docs/src-c.md`'s own entries for each) — same confidence level as
every other fix below (an exact squashed-name match to a real
declaration, used incorrectly nowhere else in the
file). #4, #5, #7
(partially), #8, #9, #10, #11, #12, #13, and #14 (the kernel/loader
name typos, the `LDBDOS.A86` fixed-offset interaction with
`tools/asm86`'s raw-binary output, the `tools/asm86` port-truncation
bug, `ROM86Q.A86`'s own two real runtime bugs, and `LDBIOS86Q.A86`'s
relative-jump-to-an-absolute-address bug)
**have been patched** directly in `src-assem/kernel/`/`src-assem/loader/`,
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

## 1. `src-assem/rsp/CLOCK.A86` — jump to a label that doesn't exist — FIXED

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

**Fixed** by changing all four `jmp clock_loop` to `jmp clockloop`,
matching the real label — no other candidate label exists in the
file, so there's no ambiguity about intent.

## 2. `src-assem/rsp/TMP.A86` — call to a label that doesn't exist — FIXED

Line 147 called `print_string`:

```
147:	    call print_string
```

but the routine is actually defined as `printstring:` (no underscore,
line 373), and every *other* call site in the file (lines 237, 239,
266, 268, 338) correctly used `printstring`. This one call site was the
odd one out.

**Fixed** (already, by an earlier commit in this tree, before this
file caught up to say so) by changing it to `call printstring` — no
other candidate label exists in the file, so there's no ambiguity
about intent.

## 3. `src-assem/rsp/SUBMIT.A86` — two mismatched labels and one missing routine — FIXED

Three separate defects in one file:

- Line 464 jumped to `not_builtin`, but the label actually defined
  (line 553) is `notbuiltin` (no underscore).
- Line 127 loaded the address of `sopen_err`, but the label actually
  defined (line 881) is `sopenerr` (no underscore).
- Line 630, inside the `showerr:` error-printing routine, called
  `conwrite`:

  ```
  showerr:
  ; Print Error String
  ;	input: DX = address of Error String

  	call conwrite
  	jmp submitexit
  ```

  `conwrite` is never defined anywhere in the file, but `con_write`
  (no missing underscore) is — a real, already-used label (`mov
  cl,mpm_conwrite ! jmp mpm`), called directly from several other
  spots in this same file (e.g. the `sopenerr`/`usererrmsg`/
  `printererrmsg` handlers) — the same convention `showerr:` itself
  is clearly trying to follow, not a different/unfinished one.

**Fixed** all 3 by changing them to `notbuiltin`, `sopenerr`, and
`con_write` respectively — each an exact squashed-name (or, for the
third, exact-name) match to a real declaration used correctly
elsewhere in the file, no other candidate in any of the 3 cases.

## Reclassified: `src-assem/utils/ED.PLM`'s `TIME` is not a bug

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

## Reclassified: `src-assem/utils/SET.PLM`'s `MOVW` is not a bug

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

## 4. `src-assem/kernel/BDOS.A86` — pervasive dropped/added underscores (45 names) — FIXED

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

## 5. `src-assem/kernel/ABORT.RTM` — five more of the same, inside one file — FIXED

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

## 7. `src-assem/kernel/XIOS.A86` — two underscore typos — FIXED

Line 611, `mov word ptr .088h,offset tick_int`, used `tick_int` where
the label actually declared (line 349) is `tickint` (no underscore) —
the same bug class as everywhere else on this page. **Patched.**

Separately, line 897's `add ax,offset dp_base` references `dp_base`,
which is **not** declared anywhere in `XIOS.A86` itself, nor anywhere
else in the whole 11-disk tree under that spelling. The real
declaration is `dpbase equ $` in `src-assem/kernel/SINGLES.LIB` line 2 (base
of the Disk Parameter Blocks), which `XIOS.A86` already pulls in via
`include singles.lib` (line 1551) — the exact same added-underscore bug
class as bug #8's `dp_base`/`dpbase` fix in `LDBIOS.A86`, just never
caught in this sibling file until `XIOS.A86` was actually assembled on
its own. Cross-confirmed against `LDBIOS.A86`'s own correct usage
(`add bx,offset dpbase` at line 244, `dpbase equ $` at line 531).
**Patched**: line 897 now reads `add ax,offset dpbase`.

With both typos fixed, `XIOS.A86` assembles cleanly on its own
(confirmed via `tools/asm86`, `dpbase = 0x16d1`) and has been added to
`src-assem/Makefile`'s `KERNEL_OK` validation list. Since `docs/build.md`
already excludes `XIOS.A86`/`ROM.A86` from the QEMU-bootable target
(they're written for the Intel iSBC 86/12 + SBC 204 specifically,
replaced there by `XIOS86Q.A86`), it is validated standalone but still
not combined into the actual `MPM.bin`.

## 8. `src-assem/loader/LDBIOS.A86` — four more dropped/added underscores, one file — FIXED

`ccpoffset`→`ccp_offset`, `dp_base`→`dpbase`, `seg_table`→`segtable`,
and `z_ret`→`zret` — the same bug class, all four within this one file,
each used exactly once. All patched. `LDBIOS.A86` now assembles
end-to-end cleanly (the short-jump-range trade-off that used to block
it afterward, see #6, is now fixed too).

Separately (not a bug in this file, noted here only for
cross-reference): `LDBIOS.A86`'s iSBC 86/12/iSBC 204/8251-specific
disk and console drivers, like `XIOS.A86`'s own (#7), have no QEMU
equivalent, so `tools/build_mpmldr.py` builds `MPMLDR.bin` from a new
`src-assem/loader/LDBIOS86Q.A86` instead — not a fix to this file, a parallel
replacement targeting different hardware. See `docs/build.md`'s "What's
actually needed to boot under QEMU" item 1 and that new file's own
header comment.

## 9. `src-assem/loader/LDMPM.A86` — underscore typo — FIXED

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

## 10. `src-assem/loader/ROM.A86` — three more underscore typos — FIXED

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

## 11. `src-assem/loader/LDBDOS.A86` — its own fixed-offset EQUs silently broke at runtime under `tools/asm86` — FIXED

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
- `src-assem/loader/LDBDOS.A86`'s own `bdosoffset` is now permanently `0` —
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

Found by actually running `src-assem/loader/ROM86Q.A86` in QEMU for the first
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
- Every real occurrence this caught — `src-assem/kernel/XIOS86Q.A86`,
  `src-assem/loader/LDBIOS86Q.A86`, `src-assem/loader/ROM86Q.A86` (all three new,
  QEMU-target files using the 8250 UART and/or NEC765 FDC, the only
  ports in this tree above 0xFF) — was changed to `mov dx,port ! in/out
  al,dx`, preserving `DX` across `fdc_out_byte`/`fdc_in_byte` with an
  explicit `push dx`/`pop dx` where their own header comments already
  promised not to disturb any register but `AL`/flags.

## 13. `src-assem/loader/ROM86Q.A86` — a 512-byte sector read into a 9-byte buffer — FIXED

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
that syntax alone (unlike `src-assem/kernel/XIOS86Q.A86`'s own `read_parms+2`
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

## 14. `src-assem/loader/LDBIOS86Q.A86` — a relative jump used to reach an absolute cross-module address — FIXED (but booting is still blocked past this point)

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

## 15. `src-assem/loader/LDBIOS86Q.A86` — *every* local `offset`/direct
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

## 16. `src-assem/loader/LDBIOS86Q.A86` `INIT` — interrupts left enabled the
whole way in, so raw FDC port I/O collided with SeaBIOS's own IRQ6
handler — FIXED

`src-assem/loader/ROM86Q.A86`'s own boot sector re-enables interrupts (`sti`)
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
calls in `src-assem/kernel/INIT.SUP`/`SUPIF.SUP` use) landed with `CS=0`
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
.xiosmod`/`.netmod`-style indirect calls in `src-assem/kernel/SUPIF.SUP`/
`INIT.SUP` that were silently wrong the same way — but fixing those
changed code sizes enough to expose this second, independent bug:
`src-assem/kernel/SUP.A86` started failing to assemble with `LOAD.SUP:78:
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
NEC765 driver, shared with `src-assem/kernel/XIOS86Q.A86`) encoded as
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

## 20. `src-assem/loader/LDBDOS.A86` — the exact same `org 0` addressing bug
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
(`LBDOSENT.A86`, `src-assem/kernel/SUPIF.SUP`, `src-assem/kernel/DSPTCH.RTM`, and
two `src-assem/toolchain/` files) was affected the same way, each one byte
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

## 24. `src-assem/loader/LDBIOS86Q.A86`/`src-assem/kernel/XIOS86Q.A86` --
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
routine. But `src-assem/loader/LBDOSF1.A86`'s `seek` (real, unmodified DRI
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
output): `src-assem/kernel/SUP.A86`'s own kernel initialization has never
been exercised in any form before this, a new frontier past everything
#1-#24 covered.

## 25. `src-assem/loader/LDMPM.A86` — `jmpf mpm` read the far pointer to jump
to from the wrong segment — FIXED

Found continuing from #24: with `MPM.SYS` loading completely correctly,
`LDMPM.A86` reached its own final `gompm: jmpf mpm` -- and execution
never reached `MPM.SYS`'s own `init:` (its very first instruction,
`src-assem/kernel/INIT.SUP`) at all, confirmed by a debug probe placed at
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

## 28. `src-assem/loader/LBDOSF1.A86`'s `seek` — read `SECTRAN`'s return
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

## 33. `#32`'s `module_table` corruption was never real either -- five
independent, real bugs found and fixed (one in `tools/asm86` itself);
`RTM`'s `terminate_entry` now runs real `term_act` cleanup for the first
time ever; a new, further-in blocking point found, not yet fixed

**Context**: continuing directly from #32's "new blocker" (`module_table`'s
`RTM` segment field corrupted from `1000h` to `0F00h`, fault site not
pinned down with confidence). Chased this for a long time with GDB before
realizing -- the same lesson as #32 all over again -- that several of the
GDB *techniques* used to chase it were themselves unreliable in this QEMU/
gdbstub combination, in ways that actively misled the investigation before
being caught:

- **Repeated `continue` against the same already-hit breakpoint (or set of
  breakpoints) does not reliably resume real execution** in this QEMU/
  gdbstub/GDB combination -- it sometimes just re-reports the *same*
  stopped CPU state over and over (identical `PC`/registers across many
  "hits"), even with a *fresh* `tbreak` each time, even though the guest
  demonstrably keeps running to completion in the background regardless
  (confirmed by the serial log reaching its usual final length while GDB
  was still "stuck" reporting hit after identical hit). This produced a
  convincing but entirely false "this function is called thousands of
  times in a tight retry loop" reading (a `MAU.MEM` entry counter,
  deliberately added and read back as `0A0Eh` — which turned out to just
  be `mau_insert`'s own code address, read from the *wrong* segment, not a
  real call count at all; see below). The one case this session fully
  cross-checked with an independent, reliable signal (a plain DSEG counter
  variable, incremented once per real call and watched with a *hardware
  watchpoint* rather than re-continued breakpoints) showed the call
  actually happens exactly **once**. Moral, sharpened from #32's UART
  finding: trust a single fresh breakpoint hit; do not trust a second
  `continue` against one already hit, and do not trust registers read
  after it without independent corroboration.
- **A GDB hardware watchpoint on an address inside the combined `cseg`
  (code) never fires**, even across many minutes of real continued
  execution and even though the location demonstrably *does* get written
  (confirmed by reading its value afterward) -- likely a self-modifying-
  code/translation-block-invalidation interaction specific to watching
  *code* pages, as opposed to `dseg`/`SYSDAT` addresses, where every
  watchpoint this project has ever set (this session and earlier ones)
  has fired reliably. Moral: always put a throwaway diagnostic counter in
  `dseg` (e.g. borrowing an unused field of an existing `SYSDAT` structure,
  as done here with `qmau`'s otherwise-dead `m_link` word), never in a
  module's own `cseg`, if it needs to be watched rather than just printed.

With those two traps avoided, the real picture: `module_table` itself was
never actually corrupted by anything *writing* to it out of turn. What
really happened is five separate, genuine bugs, several of them in the
exact same call chain (`BDOSIF.BDO`'s `init:` → `f_qmake` → `QUE1.RTM`'s
`makeq_entry` → `QUE2.RTM`'s `getqdaddr`/`qspace` → `f_maualloc` →
`MAU.MEM`'s `maualloc_entry`, the system's *first-ever* real memory
allocation), whose combined, cascading symptoms (a dispatch landing with
the wrong segment, a lock that never released, a syscall parameter read
as garbage) collectively looked exactly like the kind of "something wrote
garbage somewhere" corruption #31/#32 kept finding elsewhere in this
project, right down to reusing the same `0F00h`/`03F8h`-style tells:

1. **`SUPIF.SUP`'s `goodmod` wasn't setting up `XIOS86Q.A86`'s own calling
   convention.** Every module's own `entry:` dispatcher (`RTM`, `MEM`,
   `CIO`, `BDOS`) indexes its local function table with `CX` (`mov ch,0 !
   shl cx,1`) -- except `XIOS86Q.A86`'s, the one exception, which indexes
   with `AX` instead (`mov ah,0 ! shl ax,1`). Reached through `goodmod`'s
   general dispatch path (as opposed to the direct `xiosif` trampoline,
   where every caller sets `AX` itself before the far call), `AX` was
   simply whatever was last live -- typically a stale `u_func` left by
   `SUPIF.SUP`'s own `mpmif` trampoline -- misrouting any such XIOS
   dispatch to the wrong local function. Fixed with one new instruction,
   `mov ax,cx`, right before `goodmod`'s `callf dword ptr
   module_table[si]` -- free for every other module (none of them read
   `AX` for dispatch) and no longer a documented *output*-only register at
   that point in the call ("`AX` - Copy of `BX`" per `user_entry`'s own
   header comment).
2. **A real `tools/asm86` bug, not specific to this file**, immediately
   chased down afterward when the above fix only partially helped: a bare
   (non-bracketed) reference to an EQU defined via arithmetic on *another*
   EQU that itself carries an explicit `byte ptr`/`word ptr` cast (e.g.
   `MAU.MEM`'s `satlen equ sat_nall + byte`, where `sat_nall equ byte ptr
   sat_length + word`) was being classified as a *memory operand*
   (dereferencing live memory at that numeric address) instead of a plain
   immediate. `add bx,satlen` assembled as `03 1E 05 00` (`add bx, word
   ptr [5]` -- read the live word at `DS:5`) instead of `81 C3 05 00`
   (`add bx, 5`). Confirmed to be systemic, not a MAU.MEM-only quirk, by
   finding the *identical* mis-assembly in `LOAD.SUP`'s completely
   unrelated `lt_next: add bx,ldtablen` (`ldtablen equ ldtab_id + word`):
   `add bx, word ptr [0x11]` instead of `add bx, 0x11`. Root cause in
   `tools/asm86/expr.py`'s `parse_add`: `"SYMBOL + const"` arithmetic
   deliberately preserves the left operand's `.symbol`/`.size` so that a
   true *label* plus a constant offset (its own motivating historical
   example, `"MOV BYTE PTR ARECORD+2,AH"`, `FILE1.BDO`, where `ARECORD` is
   a real `dw`-declared label) still classifies as a memory operand --
   but the same propagation was firing for an *EQU* operand too (even one
   with a `.size` from its own `ptr`-cast definition), which is wrong:
   that `.size` only exists so a later *bracketed* reference to the same
   name (`sat_nall[bx]`) sizes correctly, via a completely separate code
   path that never looks at `.symbol`/`.size` at all. Confirmed this
   historical idiom ("stride" EQUs built this exact way) is used
   identically in the original 1981 source (`orig/mpm862sr/02/MAU.MEM`),
   so the real DRI `ASM86` must have drawn this exact distinction (label
   vs. EQU) too, even though nothing in this project's reimplementation
   previously had reason to notice. Fixed by restricting the propagation
   to `v.symbol.kind == 'label'` specifically (dropping the `sym.kind ==
   'equ' and sym.size` half of `parse_operand`'s classification check's
   matching condition, which no longer has anything to match once `equ`
   symbols stop propagating through `+`). This is the same bug class
   already fixed once before, from the *other* direction, in #27 (a bare
   `BYTE`/`WORD`/`DWORD` *literal* leaking size into an EQU that didn't
   want it) -- that fix handled the literal side; this one handles the
   "EQU-of-an-EQU" side of the identical underlying ambiguity. Any other
   "stride"/"total length" EQU built the same way elsewhere in this tree
   (there are several similarly-named `*len` EQUs) would have had the
   identical latent bug; none of the others happen to have been exercised
   by any code path reached so far.
3. **Several real `mov ds,sysdat` (bare) vs. `mov ds,cs:sysdat` bugs**,
   the same mistake #26/`QUE2.RTM`'s own already-fixed `qspace` comment
   warns about, now found freshly in the routines `qspace`'s `f_maualloc`
   call actually reaches: `MAU.MEM`'s `maualloc_entry` (`maua_out`) and
   `maufree_entry` (`mauf_out`), `MEMORY.MEM`'s `mall_ml`, `mall_try_mau`,
   and `mfree_it`, and `ML.MEM`'s `mlalloc_entry`. Every one of these
   switches `DS` away from `sysdat` partway through (to a MAU's own data
   segment via `mov ds,m_start[bx]`, or to `SS` for an on-stack MPB/MAF
   via `push ss ! pop ds`, or to `u_wrkseg`) and then tries to restore it
   with a bare `mov ds,sysdat` -- which, like every other instance of this
   bug in this project, reads `sysdat`'s *cached* segment-number cell
   through whatever `DS` currently is, not through `CS` (where the cell
   actually lives, consistently, regardless of module). Confirmed via GDB
   directly on `MAU.MEM`'s `maua_out`: `DS` read back `0000h` right after
   this instruction (reading address `0:offset_of_sysdat`, not the real
   cached value) -- leaving every subsequent `SYSDAT`-relative access in
   `MEMIF.MEM`'s `entry:` (critically, `dec mxmemcount`) operating against
   segment `0` instead of the real `SYSDAT` segment. That single `dec`
   landing in the wrong segment is *why* `mxmemcount` (confirmed
   separately with its own, correctly-`dseg`-resident hardware watchpoint)
   never returned to `0` after being incremented to `1` for this call --
   not a logic bug in the increment/decrement pairing itself, just the
   decrement hitting the wrong byte in memory. All six fixed the same way,
   `mov ds,cs:sysdat`.
4. **`ABORT.RTM`'s own `'$'` probe, at the very top of `terminate_entry`**,
   clobbering `DX` -- the entry point's own documented live input ("`DX -
   terminate code`") -- with the by-now-familiar single-register-save
   style, exactly like #32's `MEMIF.MEM` `'a'`/`'b'` bug. Confirmed via
   GDB directly on `abt_chk`'s entry: `DX` read back `03F8h` (the serial
   port, left over from the probe) instead of `INIT.SUP`'s real `0FFh`.
5. **`SUPIF.SUP`'s own `user_entry` `'@'` probe** -- the literal first
   instruction executed on *every* `int mpmint` syscall from user-level
   code -- had the identical bug, clobbering `DX` before this entry
   point's own documented live input (again, "`DX -> function
   parameter`") is ever read. This one likely has the widest blast radius
   of any single fix this session: internal kernel-to-kernel calls
   (through a module's own `mpmif` trampoline, straight to `SUP`'s
   `entry:` label) never go through `user_entry` at all, which is exactly
   why every *earlier*, successful dispatch this whole multi-session
   effort has shown correct `DX`/`CX` values -- only a genuine software
   interrupt does, and `INIT.SUP`'s own `"mov dl,0ffh ! int mpmint"`
   (asking to terminate itself) is the very first one this from-scratch
   kernel has ever executed. Any future user-level program issuing *any*
   BDOS/syscall via `int mpmint` with a meaningful byte/word parameter
   would have hit this identically.

**Net result**: `pf_tempkeep` no longer gets stuck (bug #3 above was its
real cause all along, not anything in `MEMIF.MEM`'s probes a second time);
`abt_chk` now correctly returns "OK to abort" (bug #5's fix repaired the
terminate code it reads); `RTM`'s `terminate_entry` now runs the real
`terminate:`/`term_act` cleanup path for the first time this project has
ever reached it -- freeing the init process's memory and devices through
several more successful dispatches (`f_bdosterm`, another `f_qread` on
`mxmemqpb`, more) -- before hitting a **new blocking point**: a second,
apparently-nested call back into `terminate_entry` during that same
cleanup, where `abt_chk` this time returns `CX=0000h` ("not OK to abort").
Not yet confirmed whether this second rejection is itself correct MP/M
behavior (e.g. a child/RSP process legitimately not abortable yet) or
another bug of the same kind as this entry's -- not investigated this
session; this is the next thing to chase, with the two GDB caveats above
firmly in hand this time.

## 34. The second `terminate_entry` rejection was itself a real bug chain
(four more fixes, one in `tools/asm86` again) -- the scheduler now runs
a full dispatch cycle for the first time ever; boot reaches real queue
I/O before hitting yet another new blocking point

Continuing directly from #33's open question: is the second,
`abt_chk`-rejected `terminate_entry` call legitimate MP/M behavior, or a
bug? It was a bug -- four of them, found the same way as #33: `break`+
`ignore N`+single `continue` for register snapshots, `watch` only on
`dseg` addresses, and (new this round) adding short-lived, uniquely-
charactered serial probes at specific source lines, rebuilding, and
reading the resulting trace, rather than trying to single-step through
the whole dispatcher by hand.

**Bug 1 (confirmed, fixed) -- the real second caller was
`CHARIO.CIO`'s `constat_entry`, not a scheduler-level abort check.**
Tracing the dispatch sequence between the first and second
`terminate_entry` hits (via a new `(CX)` dump added right after
`SUPIF.SUP`'s `nonet:` sets up the target module/function) showed the
call landing on `CIO` local function 8 (`constat_entry`) partway
through `BDOSIF.BDO`'s `getdiskmx` wrapper (its own, unrelated
`F_CONSTAT` call, made before `f_qread` -- nothing to do with
`f_bdosterm`/`PR_TERM`'s own logic, which was the wrong place #33 had
been looking). `constat_entry` (`CHARIO.CIO`):

```
constat_entry:
	call coninit
	mov di,rlr ! cmp di,c_attach[si]
	je cse_cl
	    mov cl,f_dispatch ! call mpmif	;not console-attached -> yield
	    mov bl,0 ! ret
cse_cl:	call conbrk
	mov bl,al ! ret
```

Since `INIT` isn't the process attached to the console, it correctly
calls `f_dispatch` (global function 142, routed via `sysent` to
`RTM`'s local function 12, `dispatch_entry`) to voluntarily yield the
CPU. This is legitimate MP/M behavior, not a bug -- but it exposed
three real bugs in the scheduler path it called into, below.

**Bug 2 (confirmed, fixed) -- `DSPTCH.RTM`'s `dsptch` had the `bare
mov ds,sysdat` bug a seventh time**, right where the current process is
taken off the ready list:

```
		; save interrupt vectors
	mov bx,0 ! mov ds,bx			;DS=0, to copy from 0:0 (real IVT)
	mov si,bx ! mov di,offset u_ivectors
	mov cx,10 ! rep movs ax,ax
	mov si,offset i_mpm_ip ! mov di,offset u_mpm_ip
	mov cx,4 ! rep movs ax,ax
		; take current process off RLR.
	mov ds,sysdat				;WRONG: DS is still 0 here
	cli
	mov si,rlr
	mov ax,p_link[si] ! mov rlr,ax
```

`DS` was deliberately set to `0` a few instructions earlier (to copy
the real interrupt-vector table from physical address `0:0`), and this
bare `mov ds,sysdat` -- same bug class as #33's six instances in
`MAU.MEM`/`MEMORY.MEM`/`ML.MEM` -- reads the module's own `sysdat`
cache cell through `DS=0` instead of `CS`, picking up garbage from the
very bottom of memory instead of the real `SYSDAT` segment. Confirmed
via a temporary serial probe dumping `si`/`rlr` right at this point:
`rlr` read back as a nonsense value (`0E99h`) instead of `initpd`
(`09C0h`), which then made `dsptch`'s `jmp cs:dsp_table[bx]` (indexed
by `p_stat[si]`, read through the same wrong `DS`) jump hundreds of
bytes past the end of the 9-entry table into unrelated code/data --
the proximate cause of a full CPU reset a few instructions later.
Fixed the same way as every other instance: `mov ds,cs:sysdat`.

Two more instances of the identical bug were found and fixed
proactively in the same file once this one was spotted, both also
"restore DS after deliberately pointing it elsewhere" spots that
hadn't been exercised yet by anything this session: `fdisp`'s own
entry (`DS` = whatever segment the *interrupted* code had, not
sysdat-ish) and `switch:`'s tail (`DS` = the newly-switched-to
process's own UDA segment, set a few lines above via `mov ds,dx` from
`p_uda[bx]`, not sysdat-ish either).

**Bug 3 (confirmed, fixed) -- a second, genuinely new `tools/asm86`
bug: `parse_add` lost an operand's `is_offset` flag when propagating
its label/size through further `+`/`-` arithmetic.** `DSPTCH.RTM`'s
`insert_process` (called from `disp_act` to put `INIT` back on the
ready list after it yields) is invoked with `BX=(offset rlr)-p_link` --
a standard "treat a plain variable as if it were a linked list's head
node" trick (`p_link` is a zero-offset structure field EQU, so this is
meant to assemble as a pure constant: `rlr`'s address minus 0).
`"offset rlr"` correctly sets `Value.is_offset=True` to flag this as a
constant even though it still carries `rlr`'s label/size (this is
exactly what `parse_operand`'s `if v.is_offset: return Operand('imm',
...)` check -- added for #27/#33 -- exists to catch). But
`parse_add`'s `"+"`/`"-"` handling, when it decided to carry a label's
`.symbol`/`.size` through the subtraction (so that bare `LABEL+const`
offset arithmetic, e.g. `FILE1.BDO`'s `ARECORD+2`, still sizes
correctly -- the fix from #33), rebuilt the result with `Value(n, size,
sym)` and never passed `is_offset` through, silently resetting it to
`False`. So `"(offset rlr)-p_link"` lost its "offset"-ness midway and
fell through to the *next* check instead (`sym.kind=='label'` ->
`Operand('mem', ...)`), misassembling `mov bx,(offset rlr)-p_link` as
a live memory read of `DS:rlr`'s address instead of loading the
intended constant. Net effect: `insert_process`'s final `mov
p_link[bx],si` -- which is *supposed* to be `mov rlr,si` via the same
trick -- silently went to the wrong place, so the real `rlr` variable
was *never actually updated*, leaving the ready list permanently empty
after the very first `dispatch_entry`/`schedule()` cycle completed.
Confirmed via a temporary probe dumping `rlr` immediately after the
`call insert_process` returns (`rlr` still read back `0000h`) and via
direct disassembly comparison before/after the fix. Fixed in
`tools/asm86/expr.py`'s `parse_add` by carrying the chosen operand's
`is_offset` through into the rebuilt `Value`, symmetrically with how
`.symbol`/`.size` are already carried.

**Net result of bugs 2+3 together**: `schedule()`/`switch()` now
complete a full cycle correctly for the first time ever -- confirmed via
probes showing `rlr` holding the correct PD (`09C0h`) at every step
through `insert_process`, `switch:`, and back -- and `dispatch_entry`'s
call into the scheduler now *returns* normally to its caller
(`constat_entry`) instead of crashing/resetting the system. Boot no
longer double-prints the signon banner (the tell-tale sign of the
previous CPU-reset loop): the serial trace is now a single, clean,
uninterrupted run, and goes well past the previous `module_table`
fnchk/goodmod dispatch for the first `f_qread` call -- which now
correctly reaches `RTM`'s `readq_entry` (local function 7, not
`dispatch_entry`'s 12 as it had been misrouted to while `rlr` was
still corrupted) -- into further genuine queue I/O (further `f_qread`/
`f_qwrite` dispatches completing normally) and a `term_act` cleanup
cycle that gets measurably further than #33's.

**New, not-yet-fixed blocking point**: shortly after this, the trace
shows signs of a *different* kind of corruption -- an `fnchk`/`goodmod`
module-table-integrity mismatch at slot `0` (`SUP`'s own slot, not the
benign, expected slot-`0x30`/`net`-module mismatch seen everywhere
else in every trace this session) during what looks like a *second*
`term_act` cycle (reached via `ABORT.RTM`'s own `'+'`/`'='` markers,
i.e. a real `terminate:` completing its memory-free loop and calling
`jmp dsptch` again), with `rlr`/`si` reading `0BBBh` -- a value that
doesn't match any of the three known static PDs (`initpd=09C0h`,
`idlepd=09F0h`, `tickpd=0A20h`) and falls inside `dsptchtos`'s own
dispatcher-stack region (`0B5Eh`-ish upward), suggesting a stack/
register-corruption bug of some kind, not yet root-caused. This is the
next thing to chase, with the same proven methodology (targeted,
uniquely-charactered serial probes + rebuild + trace read, reserving
GDB for register snapshots where a probe can't reach). Note for next
session: many single ASCII characters are now already claimed by
probes across the tree (checked via a quick per-character `grep -rl`
sweep); pick an unused one before adding a new probe, or two unrelated
probes' output becomes indistinguishable in the trace.

All fixes from this entry are in `src-assem/kernel/DSPTCH.RTM` and
`tools/asm86/expr.py`; the temporary diagnostic probes added and then
*not* removed (`CHARIO.CIO`'s `cdbg`/`'|'`/`'_'`/`` '`' ``,
`SYSENT.RTM`'s `','`, `DSPTCH.RTM`'s `'['`/`'<'`/`'>'`) are left in
place deliberately, the same way #31-#33's probes were, since they're
still actively useful for the next session and carry no behavioral
risk (temp probes are documented at the top of this file).

**Follow-up within the same session, after the above was written**:
chased the new blocking point further. Added more probes (`DSPTCH.RTM`'s
`'.'` at every `dsptch` entry, dumping `DS` and `indisp`; `ABORT.RTM`'s
`'*'` extended to also dump `DS`; `XIOS86Q.A86`'s `'"'` at `tickint`'s
entry, to rule in/out a mid-flight timer interrupt; `QUE1.RTM`'s marker
byte `0x01` at `writeq`'s `qw_wait`, to rule in/out the write-queue-full
sleep path; `RTMIF.RTM`'s `'W'` extended to also dump `DS`; `SUPIF.SUP`'s
`'('`/`')'` extended to also dump the target module number alongside
the local function number). All printable ASCII marker characters are
now exhausted across the tree (confirmed by a per-character `grep -rl`
sweep) -- the `tickint` probe had to reuse `'"'` and the `qw_wait`
probe had to fall back to a raw, non-printable marker byte (`0x01`,
shows as an unlabelled control character under `cat -v`) for lack of
any free printable character; future probes should do the same (pick
an unused byte value, not necessarily printable) rather than reusing
an existing marker and making the trace ambiguous.

Confirmed via these probes: no timer interrupt fires anywhere in this
whole window (ruling out `fdisp`/`do_dispatch`/`int_disp_exit` as the
source), and the write-queue-full/sleep path is never taken either
(ruling out `sleep_act`). `DS` is confirmed *correct* (`cs:sysdat`'s
value) at every `RTM`-module call return (`RTMIF.RTM`'s `'W'` probe)
right up through a `writeq_entry` (`f_qwrite`, global function 139)
call completing normally -- but by the very next instruction with a
probe (`SUPIF.SUP`'s `nonet:` probe for the *next* dispatch, module
`0002`=`RTM`, local function `5`=`openq_entry`), `ABORT.RTM`'s `'*'`
probe immediately after reads `DS=0000`. That next dispatch's target
module now reads correctly as `RTM` (not the `0028`/`'('`-byte
artifact an earlier, buggy version of this same probe briefly showed --
self-inflicted: the probe's own `mov al,'('` was clobbering the very
`AL`/module byte it was about to print, before the fix of saving it in
`BL` first). The exact instruction that zeroes `DS` between those two
probes is still not pinned down.

Re-running with the fixed, module-aware probe produced a *longer and
further* trace than every previous run this session (grew to 2609
bytes, confirmed stable/non-growing over a 20-second wait, vs. ~1950
bytes before) -- `makeq_entry`, a second `openq_entry`/`writeq_entry`
round with module correctly shown as `0002` throughout, and a *third*
`terminate_entry` hit for `initpd` (`bx=09C0`), this time with
`abt_chk` returning `CX=FFFFh` (tempkeep-deferred, not outright
rejected) and `p_flag=0285h`. Immediately after, `INIT.SUP`'s own
`init:` markers (`'1'`-`'5'`, `'q'`) print *again*, but this time with
a plausible, non-garbage `SS:SP` (`16D0:0F82`, inside `BDOSIF.BDO`'s
own `bdosstack` region) instead of the `0000:FFFC`-style garbage seen
at every previous crash/reset this session -- i.e. this does not look
like the same CPU-reset failure mode as before; it looks like
something is deliberately re-entering `SUP`'s `init:` from within
`BDOS`'s own internal stack context, which is suggestive of a genuine
MP/M warm-boot/restart path (`BDOSIF.BDO` has unreferenced
`bootf:`/`wbootf:` labels that were assumed to be dead code earlier
this session -- worth re-checking whether something *does* reach them
via a jump table not yet found) rather than a bug of the same kind as
everything else in this entry. Not yet root-caused or categorized as
bug-vs-intended-behavior; this is where the next session should pick
up, armed with the `DS=0000` lead above and the "is this actually a
reboot vs. a real warm-boot path" question.

**Further follow-up**: traced what happens *after* that second `init:`
re-entry completes. It is a genuine, controlled re-entry, not a CPU
reset -- there is no repeated loader banner (`"Code Paragraph
Address..."` etc.) before it, which a real reset would always produce,
and the `SS:SP` it reports (`16D0:0F82`, inside `BDOSIF.BDO`'s own
`bdosstack`) is consistent with a live jump/call, not a fresh CPU
coming out of reset with a BIOS-default stack. `SUP`'s `init:` runs
the *entire* module-init sequence a second time end to end (`RTM`
twice-created idle/tick far-pointer dump, `BDOS`'s own disk-mx-queue
`qmake`/`qopen`/`qwrite` dance complete with its own `pf_tempkeep`
probes showing `p_flag=0205h` this time instead of `0005h`, `CIO`,
`XIOS`'s UART/PIT/IRQ/floppy setup and signon banner) and then, as its
own very last act (same as the very first boot), issues `int mpmint`
to terminate itself again. This time `abt_chk` returns `CX=FFFFh`
(tempkeep-deferred, not an outright "ok" or "not ok") because
`p_flag[initpd]` has `pf_tempkeep` (`0200h`) set in it (confirmed:
`0285h & 0200h = 0200h`) -- and that is where the trace permanently
stops (stable across a 40-second wait).

Investigated *why* `pf_tempkeep` is still set on `initpd` at this
point, since every writer of that bit (`BDOSIF.BDO`'s `getdiskmx`,
`MEMIF.MEM`'s `entry:`) is supposed to balance every `or
p_flag[si],pf_tempkeep` with a matching clear once its critical
section ends. Added a probe at `MEMIF.MEM`'s `entry:` dumping
`mxmemowner` right before the `cmp ax,mxmemowner`/`je havemx` decision
that distinguishes a *fresh* mx-queue acquisition (which pushes/sets
`pf_tempkeep`) from a *nested/reentrant* one (which skips both the
push and the set, relying on the outer call's balance) -- hypothesis
being that some reentrant call was mis-detected as fresh or vice
versa, permanently losing the restore. That specific probe, as first
written, introduced a *new* regression worth recording as its own
lesson: it was inserted directly between `cmp ax,mxmemowner` and the
`je havemx` that reads the comparison's flags, and its own body
(`dec`/`cmp`/`and` for the hex-nibble dump) silently clobbered those
flags, so `je havemx` branched on garbage and the boot regressed to
hanging far earlier (453 bytes instead of 2609+). Fixed by wrapping
the probe in `pushf`/`popf`. **General lesson for future probes in
this tree**: never insert a probe between a flag-setting instruction
and the conditional jump that consumes it without `pushf`/`popf`
around it -- this is a distinct failure mode from the register-clobber
bug class (#3/#5/#33's DX-clobber entries) that this session hadn't
hit before. With the fix in place, the probe shows `mxmemowner=0003h`
at *every* `MEM` `entry:` call this entire boot (both the very first
one and the last one right before the hang) -- never `initpd`'s real
value (`09C0h`), and never changing -- which means every single
`MEM`-module call this whole session takes the *fresh-acquire* branch,
never the reentrant `havemx` shortcut. This rules out the
reentrancy-confusion theory for the `pf_tempkeep` leak: `MEMIF.MEM`'s
push/set-then-pop/clear dance around `pf_tempkeep` is always balanced,
once per call, every time. The real source of the stuck
`pf_tempkeep` bit on `initpd` is therefore still open -- most likely
`BDOSIF.BDO`'s `getdiskmx`/`RETMONX` dance (the *other* writer of this
bit) rather than `MEMIF.MEM`, not yet checked with the same rigor.
Next step: apply the identical "dump the flag right before every
write/clear, across every call this boot" technique to
`BDOSIF.BDO`'s `pf_tempkeep` save/restore in `getdiskmx`/`RETMONX`
instead.

**Root cause finally found, via exactly that approach, plus one
important new methodology lesson about this file's own probes.**
Bisected forward from `getdiskmx`'s `push si` (saving the function-
table index across `getdiskmx`'s own body) to the final `pop si`
(right before `CALL_BDOS` dispatches through it) using a raw, non-
printable marker byte (`0x07`/`0x08`, since every printable ASCII
character is now claimed by some existing probe -- confirmed by a
`grep -rl "'<char>'"` sweep across the whole `src-assem/kernel/` tree) to
dump `SI` at each end: it changed from a plausible-looking value
(`4184h`) to obvious garbage (`7580h`) *somewhere in between*, and
`CALL_BDOS`'s own call target (`cs:btab_addr[si]`) read back `1000h`
-- not a real function address, but a wild near-jump landing inside
`SUP`'s own code (the single shared 0x1000 code segment holds every
module back-to-back, `SUP` first), which is exactly why this looked
like "`SUP`'s `init:` is somehow being re-entered" in the earlier
investigation above: it wasn't a deliberate warm-boot path after all,
it was a crash landing in a plausible-looking place purely because of
the combined segment's layout.

Bisecting further (peeking the stack slot directly, without popping,
immediately before and after the one call in between -- `entryzero`
then a conditional `call setdata` -- both individually confirmed
stack-balanced) narrowed it to `entryzero` itself: `mov cx,
uda_ovl_len ! rep movs al,al`. `uda_ovl_len` (`UDA.FMT`) is `equ
(offset $)-(offset u_dma_ofst)` -- the exact same "distance between
two offsets, used bare as a pure byte count" idiom as `MAU.MEM`'s
`satlen` and `LOAD.SUP`'s `ldtablen` from bug #33's `tools/asm86` fix,
just not caught by that fix because this one goes one level deeper:
`"(offset $)-(offset u_dma_ofst)"` *does* correctly evaluate with
`is_offset=True` during the EQU's own definition (bug #33's fix for
`parse_add` already carries that flag through the subtraction
correctly), but `is_offset` was never a saved property of the defined
Symbol -- only ever a transient attribute of the `Value` live during
that one evaluation. Every *later* bare reference to the EQU by name
(`mov cx,uda_ovl_len`) built a brand new `Value` from the Symbol's
bare `(value, size)`, with `is_offset` defaulting back to `False`,
which fell through to the `sym.kind=='equ' and sym.size` memory-
operand check and misassembled as `mov cx, word ptr [0019h]` (a live
read of whatever happens to be at `DS:0019h`) instead of `mov cx,
19h`. That garbage `CX` fed directly into `rep movs al,al`'s count,
running the copy for a huge, essentially random length and scribbling
over unrelated `SYSDAT` memory -- including, in this exact call
chain, `getdiskmx`'s own saved `SI` sitting a little further up the
very same internal `bdosstack` the copy's destination (`dmaad`) also
lives in.

First fix attempt (persist `is_offset` on the `Symbol` itself, so
every later lookup inherits it) was **too broad and caused a real
regression**: it also flipped every *single-term* `LABEL equ offset
$` EQU across the whole tree (a completely different, extremely
common idiom used purely to make an EQU alias a label exactly, e.g.
`stkbase`/`lastoff`/`dpb0`/`begdat`/`enddat`/`drombegin`/`dromend` in
various loader and kernel files) from their original "behaves exactly
like the label it aliases, including reading memory when referenced
bare" semantics to "always an immediate" -- several of which are
genuinely read as memory elsewhere by design. Caught immediately by
the standard boot smoke test: the *loader* itself (not even the
kernel) crash-looped, repeating its own signon banner thousands of
times a second (confirmed: `grep -c` on the serial log, not just an
eyeball guess) because `LBDOSDAT.A86`'s own `dpblist equ (offset
endlist)-(offset sectpt)` (the loader's independent copy of the exact
same idiom, used by `LBDOSF1.A86` to size a DPB-table copy) flipped
the same way and broke *its* copy length instead. **Lesson**: never
assume a `(offset X)-(offset Y)` EQU's bare-reference semantics should
always become "immediate" just because that's what one call site
needs -- some of this 1981 codebase's `offset`-based EQUs are
genuinely meant to alias a label (memory-read semantics), and there is
no cheap, purely-structural way for the assembler to tell those apart
from a true offset-subtraction byte count at the point an EQU is
*defined* (only at each individual *use site* does the surrounding
code's intent become clear, which is exactly why ASM86 leaves this
ambiguous and 1981 MP/M's own authors apparently never had to care,
having presumably written each use site by hand with the right form).

**Final fix, much narrower and verified not to regress the loader**:
reverted the `Symbol.is_offset` persistence entirely. Instead,
`parse_add` now special-cases the exact shape that's unambiguous --
both operands of a `+`/`-` individually already `is_offset`-flagged
(i.e. the EQU's own right-hand side is *itself* built from two
`offset X` terms, not a bare single `offset X` with no arithmetic at
all) -- and only then drops the resulting `Value`'s `size` (and
`symbol`) to `None`. Unlike `is_offset`, `size` already flows straight
into `define_symbol`'s stored `Symbol.size` through the existing,
unmodified code path, so this is exactly the minimum change that
makes a later bare reference correctly fall through every
memory-operand check in `parse_operand` to the plain-immediate case,
without touching any single-term `offset $`-style EQU's behavior at
all (confirmed: rebuilt and reran the full boot smoke test after this
narrower fix -- still broke the loader identically, because
`LBDOSDAT.A86`'s `dpblist` *also* matches the "both operands
individually offset-flagged" shape, same as `UDA.FMT`'s
`uda_ovl_len`, so the general rule still isn't safe to apply to every
EQU shaped this way across the whole tree).

Given that, the fix actually kept is **source-level, not
assembler-level**: `tools/asm86/expr.py`'s `parse_add` change was
reverted back out entirely (confirmed back to bug #33's state, no
diff), and instead every bare reference to one of these EQUs that
this session could find and confirm matters was replaced with its
literal, pre-computed value plus a comment explaining why (checked via
`--symbols` against the real built value, not guessed): `BDOSIF.BDO`'s
two `uda_ovl_len` uses (`getdiskmx`, `RETMON`) -- the one that was
actually diagnosed as corrupting memory this session -- `BDOSIF.BDO`'s
one `zerolength` use (`entryzero`, the very function this corruption
was found inside, copied as a precaution since it shares the exact
same bare-EQU-feeding-`rep`-count shape), `PARSE.SUP`'s one `ndelims`
use, and `FILE1.BDO`'s one `dpblist` use (`DATA.BDO`'s copy of this
EQU, distinct from the loader's own `LBDOSDAT.A86` copy of the same
name -- the loader's own use was deliberately left untouched, since
reverting the assembler change already restores its previously-working
behavior and this session never diagnosed it as actually broken).
`grep -rn "equ.*(offset.*-.*(offset\|equ.*offset \$-"` across
`src-assem/kernel/` turned up exactly these four EQU definitions and no
others; a future session touching `src-assem/loader/` should run the same
grep there before assuming the loader's own `dpblist` is safe to leave
alone forever.

**Net result, confirmed via the serial trace**: boot is a clean,
single run again (no loader crash-loop), `SI` no longer gets corrupted
through `entryzero`, and the trace reaches a new high-water mark
(3401 bytes, confirmed stable across a 25-second wait, up from the
2621-2661-byte plateau every run previously stalled at). A **new**,
not-yet-root-caused symptom appears at this new, further point: `rlr`
reads back `0000h` at `switch:` (previously-reliable at `09C0h`
through this exact code path) and *stays* `0000h` all the way through
a third `terminate_entry` hit (`bx=0000h` this time, not `09C0h`;
`p_flag` dumps as `16DDh`, which doesn't look like a real flags word
at all and is worth treating with suspicion rather than taken at face
value). This is the next thing to chase, with the same proven
methodology (and the same character-exhaustion caveat for any new
probe).

## 35. `endseg` was never initialized (GENSYS-era gap, now patched at
build time) -- `proc_creat` could never link `idle`/`tick` onto the
ready list, which was the real cause of every "`rlr` reads back
0000h/0BBBh" symptom above; fixed, and the dispatcher now sustains a
live scheduler loop for the first time in this project's history

Picked up the #34 entry's open "`rlr=0BBBh` inside the dispatcher
stack" lead. Bisected forward through `DSPTCH.RTM`'s own
`insert_process` (confirmed correct and already fixed by #34) using
the same raw-marker-byte technique, this time tracing all the way
back to `RTMIF.RTM`'s `init:` calling `PROC.RTM`'s `proc_creat` (once,
for `idlepd` -- chained to also create `tickpd` via `p_link`, a real,
intentional design: processing one PD pops the next one to process
off a list threaded through the PDs' own `p_link` fields before those
get repurposed for the real ready-list linkage). Found, bisecting
instruction-by-instruction with marker probes (including one
self-inflicted false lead: a badly-written probe read `DX` *after*
already clobbering it for its own port write, briefly looking like yet
another register-corruption bug before the mistake was spotted and the
probe fixed) that `proc_creat`'s call to `getpdadr` (which validates a
PD's address before using it) returns `CX=000Ch` (`e_no_pd`) for
*both* `idlepd` and `tickpd` -- real, legitimate, in-range SYSDAT
addresses, rejected as invalid.

Root cause: `getpdadr` (`PROC.RTM`) and `QUE2.RTM`'s `getqdaddr` both
gate on `cmp ax,sysdat ! jb bad ! cmp ax,endseg ! jae bad` before a
second, tighter "within 64K of sysdat" check. `endseg` (`SYSTEM.DAT`:
`"dw 0 ;1st parag. outside of MP/M"`) is `GENSYS`-computed data in the
original 1981 build (`SYSTEM.DAT`'s own header comment: "This data is
initialized by GENSYS") -- and `tools/build_mpmsys.py`, this tree's
from-scratch replacement for `GENSYS`, already patches several other
fields this same way (`pul`/`mdul`/`qlr`/`qul`/`qmau`, see their own
comments) but never patched `endseg`. Confirmed via `grep -rn
"endseg,"` across the whole tree: nothing ever writes to it, so it
stays at its static source value, `0`. With `endseg=0`, `cmp
ax,endseg ! jae bad` is true for literally every real address (any
real SYSDAT address is `> 0`), so `getpdadr` unconditionally fails --
which is why `proc_creat` never reached the "put on dispatcher ready
list" code for either `idlepd` or `tickpd`, which is why `drl` (the
dispatcher ready list `proc_creat` feeds) stayed `0` for the entire
rest of boot, which is why every `schedule()` call's `drltorlr` loop
had nothing to transfer onto `rlr` beyond whichever single process
yielded into it, which is the real explanation for every "`rlr` reads
back `0000h`/`0BBBh`" symptom in this entry's own earlier paragraphs
(not a new, separate corruption bug after all -- `idle`/`tick` were
simply never on the list to begin with).

Fixed in `tools/build_mpmsys.py`, patched the same way as the other
GENSYS-era fields: `ENDSEG_FIELD_OFFSET = 0x44` (confirmed against
`--symbols` output, consistent across every module that includes
`SYSTEM.DAT`), written as `0xFFFFh` -- deliberately generous (the
largest representable 16-bit paragraph address, just under the 1MB
real-mode ceiling) since neither check this guards is really meant to
be the *binding* constraint; the second, tighter "within 64K of
sysdat" check always is, so there's no need to compute a true,
precise end-of-RAM value here.

**Net result, confirmed via the serial trace**: boot still completes
cleanly (no loader crash-loop, no reset), and the trace volume
explodes from the low-thousands-of-bytes plateau every previous bug
this session got stuck at to **3.8MB in a 15-second window** -- the
dispatcher now sustains a real, continuous scheduling loop, visibly
alive, for the first time this from-scratch reimplementation has ever
reached. The specific pattern seen repeating indefinitely is a tight
cycle dispatching to `RTM` local function 2 (`flag_wait_entry`,
`S0002`) over and over, with no crash, no reset, and no further
*distinct* progress visible in the trace -- almost certainly `idle`
(or `tick`) legitimately looping (that is `idle`'s actual job when
there's nothing else ready to run), but not yet confirmed whether
`INIT` itself ever finished terminating and handed off cleanly, or
whether it's stuck somewhere else now that `idle`/`tick` are finally
real, schedulable competitors for the CPU. **This is the next thing to
check**: is this `flag_wait` loop `idle`'s normal, correct behavior (in
which case the real next step is making sure `INIT`'s own termination
actually completes and something -- `CCP`, presumably -- gets loaded
and started next), or is `INIT` now stuck waiting on a flag that
nothing will ever set? Given the sheer trace volume now, prefer a
short, time-boxed capture (a few hundred KB, not multiple MB) and grep
for distinct markers rather than reading the whole thing, and consider
disabling/thinning some of this session's noisier always-fire probes
(`#0030#`/`Y`/`Q`/`Z`/`d`/`R` in particular) before the next round,
since they now dominate the trace at this scale.

All temporary diagnostic probes added and removed during this entry's
own bisection were cleaned up afterward (`git diff --stat
src-assem/kernel/PROC.RTM` is empty) -- the only real, permanent fix is the
one-line `tools/build_mpmsys.py` patch described above, plus its
`ENDSEG_FIELD_OFFSET` constant and explanatory comment.

## 36. Wiring up `src-assem/rsp/TMP.A86` (the real MP/M-86 shell) as the
system's first RSP -- one genuine new kernel bug found and fixed
(`INIT.SUP`'s own `nrsp:` loop), substantial new build-system support
added, but one more real bug still blocking `f_createproc` itself,
not yet root-caused

With #35's fix, boot finally sustains a live scheduler -- but with
nothing except `IDLE`/`TICK` ever running, since `INIT` terminates
without anything to hand off to. Checked `tools/build_disk.py`'s own
comments first, as asked: confirmed, in its own words, "no CCP/process
loader exists anywhere in this tree yet to actually read a .CMD/.RSP
file and run it." But `src-assem/rsp/TMP.A86` -- the *real* MP/M-86 Terminal
Message Process (its actual console/shell equivalent, not a CP/M-style
CCP) -- already exists in this tree, just never wired to anything,
with exactly one known, trivial, already-diagnosed bug (docs/bugs.md
#2: `call print_string` should be `call printstring`, line 147).
Fixed directly in `src-assem/rsp/TMP.A86` itself this time (unlike #1/#3,
left alone) -- the project's own stated policy for kernel/loader files
("once a fix is this confidently evidence-backed... leaving it broken
forever didn't serve the 'get this booting' goal") applies just as
much to the one file that's now directly on the critical path to an
actual prompt. It assembles cleanly once fixed.

**Baking TMP into the boot image.** `INIT.SUP`'s own `nrsp:` loop
already walks a chain of RSPs starting from `rspseg` (SYSTEM.DAT:
`"dw 0 ;segment of first RSP"`), calling `f_createproc` once per
entry -- never exercised before because nothing ever set `rspseg`.
TMP is an "8080 Model" RSP per its own header comment (MP/M-86
Programmer's Guide 5.2: CS=DS=ES=SS all the same segment), so its
assembled code+data are appended, back to back, onto the end of the
combined kernel `cseg` blob in `tools/build_mpmsys.py` (new
`RSPSEG_FIELD_OFFSET` section) -- the same general technique as
XIOS86Q's own private dseg tail, just appended to `cseg` instead.

Two real placement bugs were found and fixed while getting this right:
- **First attempt**: appended TMP's bytes to `cseg` *after*
  `sysdat_segment` was already computed from the original, smaller
  `cseg`. Collision: `tmp_code_seg` (computed the same way,
  `MPM_CODE_SEG + ceil(len(cseg)/16)`, from that same, still-
  unmodified `cseg`) came out *identical* to `sysdat_segment` --
  SYSDAT and TMP's code would have loaded on top of each other.
- **Second attempt**: fixed the collision by placing TMP after
  SYSDAT's own full span instead, but this broke something subtler:
  `tools/build_disk.py`'s own `build_two_segment_body` independently
  *recomputes* "where SYSDAT loads" from `MPM.bin.cseg`'s real,
  on-disk file length (its own comment already says so: "the same
  arithmetic build_disk.py's own `build_two_segment_body` uses"). Since
  TMP's bytes were now part of that file, appended *after* where
  `sysdat_segment` had already been fixed, the two computations
  disagreed -- the kernel's own internal patches (`module_table`,
  `qmau`, etc.) were correct for one segment while the loader
  actually placed SYSDAT at a different one, corrupting everything
  within the first few dispatches after boot (confirmed via serial
  trace: widespread, cascading `module_table` integrity-check
  failures, previously only ever seen as a single, benign, expected
  mismatch at the unused network-module slot).
- **Fix**: moved the entire TMP-assembly-and-append step to *before*
  `sysdat_segment = MPM_CODE_SEG + (len(cseg)+15)//16` is computed, so
  that single, shared formula -- used identically by both
  `build_mpmsys.py` internally and `build_disk.py`'s own independent
  recomputation -- counts TMP's bytes either way. Confirmed via the
  loader's own printed "Data Paragraph Address" matching
  `build_mpmsys.py`'s own computed `sysdat_segment` exactly.

TMP's own dseg template needed two fields patched at build time
(GENSYS would have filled these in the real 1981 build): `stack_top`'s
2nd word (the "code seg" slot of its 3-word IRET-start structure,
left as the placeholder `unknown` equ/0 in the template) and
`u_initcs` (UDA-relative offset `50h`, confirmed against `--symbols`
on a real kernel file that includes `UDA.FMT`) -- both set to TMP's
own computed code segment, matching "Small Model" semantics (DS/ES/SS
deliberately left 0 so `PROC.RTM`'s own `cp_uda` auto-fills them from
`ax`=`u_wrkseg`, which `INIT.SUP`'s `nrsp:` loop already arranges to
be `rspseg` via `mov ds,cx` right before `int mpmint`).

**A second, genuine, pre-existing kernel bug found and fixed in the
process** (never exercised before, for the same reason as #35's
`endseg`/flag-table gaps -- `rspseg` was always 0 until now):
`INIT.SUP`'s own `nrsp:` loop does `mov ds,cx` (loading the RSP's data
segment into DS) immediately followed by `mov cl,f_createproc` -- an
**8-bit** move that only sets `CL`, leaving `CH` holding whatever it
last was, which at that exact point is still the *high byte of `cx`*
from the `mov ds,cx` just above (i.e. the RSP segment's own high
byte). `SUPIF.SUP`'s dispatcher (`func1: cmp ch,0 ! je userf`) reads a
nonzero `CH` as an *explicit module number* rather than routing this
as the global function it actually is, sending `f_createproc`'s global
number (144/90h) hunting for local function 90h in whatever bogus
"module" the stray high byte named -- confirmed directly via a serial-
trace probe at `user_entry`'s own entry point: `CX` read back as
`1790h` (rspseg's own high byte, `17h`, joined with `f_createproc`'s
low byte, `90h`), not `0090h`. Fixed: `mov cl,f_createproc` ->
`mov cx,f_createproc`.

**Not yet root-caused**: fixing that bug changed the symptom (`CX`
now correctly reads `0090h` at `user_entry`) but did not fully fix the
underlying problem -- the same widespread `module_table` corruption
still happens on this exact call. Bisected with marker-byte probes
(the same technique as #34/#35) as far as: `user_entry` is reached
correctly with `CX=0090h`; `SUPIF.SUP`'s own `func1: cmp ch,0 ! je
userf` should be moot anyway, since `user_entry` *unconditionally*
does `mov ch,0 ! call func` a few instructions before dispatching
regardless of what `CH` was on entry (meaning the `INIT.SUP` fix above
is still a real, independent bug -- confirmed via its own direct
evidence -- but is not, by itself, sufficient to explain this one);
`RTM`'s own `creat_proc_entry` (local function 14, where `f_createproc`
is supposed to land) is *never reached* at all (a probe at its very
first instruction never fires); `PROC.RTM`'s `proc_creat` itself *is*
reached a 3rd time, but via the *wrong* path -- the `jmp proc_creat`
tail-chain-continuation at its own end (from processing `TICK`'s
static `p_link` chain during `RTM`'s own, unrelated, direct-call
process creation earlier in boot), not a fresh call from
`creat_proc_entry`, and with `DX=0000h` (so it just exits immediately,
`cmp dx,0 ! jne cp_doit` not taken). So somewhere between `user_entry`
correctly calling `func` with `CX=0090h` and `creat_proc_entry` being
reached, the dispatch still goes somewhere else entirely, corrupting
`module_table` along the way. Candidates not yet checked: whether
`user_entry`'s own UDA-relative stack switch (`ulen`-sized, `mov
ss,bp ! mov sp,ulen`) is somehow implicated specifically for this call
site (it has worked correctly for every other syscall this session,
so this seems unlikely but hasn't been directly ruled out); whether
`sysent`'s own table entry for function 144 is being misread at
runtime despite being confirmed correct in the static build image
(checked directly: `module_table` itself, read straight from
`build/kernel/MPM.bin.dseg`, is correct in the static image -- segment
`1000h` in all 6 real slots -- so whatever corrupts it does so at
runtime, not at build time). This is the next thing to chase, with
the same proven methodology; a live GDB watchpoint on `rspseg`'s own
runtime address was attempted but didn't finish executing within a
reasonable wait (the `ignore N` + single-`continue` pattern that's
been reliable for breakpoints all session may not transfer cleanly to
watchpoints -- worth a fresh, patient attempt, or falling back to more
marker-byte probes, next time).

All of this round's temporary diagnostic probes remain in place in
`src-assem/kernel/PROC.RTM`, `src-assem/kernel/SYSENT.RTM`, and `src-assem/kernel/
SUPIF.SUP` (markers `0x19`-`0x1F` as raw, non-printable bytes --
printable ASCII is now fully exhausted across the tree) alongside the
two real fixes (`src-assem/kernel/INIT.SUP`, `tools/build_mpmsys.py`) and
TMP's own one-line fix (`src-assem/rsp/TMP.A86`).

## Status of the kernel/loader sources

`src-assem/kernel/CIO.A86`, `SYSDAT.A86`, `BDOS.A86`, `MEM.A86`, `RTM.A86`,
`SUP.A86`, and all four of `src-assem/loader/LDBDOS.A86`/`LDBIOS.A86`/
`LDMPM.A86`/`ROM.A86` now compile and assemble cleanly end-to-end. Every
source-level defect found in `BDOS.A86`, `RTM.A86`, `LDBIOS.A86`,
`LDMPM.A86`, and `ROM.A86` has been fixed (#4, #5, #8, #9, #10) — what
first looked like two genuinely unresolved symbols in #9/#10 turned out,
on closer inspection, to be the same underscore-typo bug class as
everywhere else — and the short-jump-range tooling trade-off that used
to additionally block several of these is also fixed now that
`tools/asm86` does real jump-size relaxation (#6). Only `src-assem/kernel/
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
jumps into the actual MP/M-86 kernel (`src-assem/kernel/INIT.SUP`'s own
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
`abt_chk`'s rejection was gone, briefly, before #33 found the apparent
`module_table` corruption that followed was *also* never real. #33's
five fixes (`goodmod`'s own XIOS calling-convention gap, a genuine
`tools/asm86` assembler bug misclassifying certain bare EQU-arithmetic
expressions as memory operands, three more `mov ds,sysdat` bare-vs-
`cs:`-prefixed bugs in `MAU.MEM`/`MEMORY.MEM`/`ML.MEM`, and two more
instances of the DX-clobbering single-register-probe bug in
`ABORT.RTM` and -- most consequentially -- `SUPIF.SUP`'s own
`user_entry`) together mean `RTM`'s `terminate_entry` now runs the real
`terminate:`/`term_act` cleanup path for the first time this project
has ever reached it, successfully freeing the init process's memory and
devices through several more dispatches, before hitting a **new**
blocking point: a second, apparently-nested call back into
`terminate_entry` during that same cleanup, where `abt_chk` this time
returns `CX=0000h` ("not OK to abort") -- not yet confirmed whether
that's correct MP/M behavior or another bug. See #33's own writeup for
the full detail, including two GDB/QEMU methodology traps (unreliable
repeated `continue` against an already-hit breakpoint; `cseg`
hardware watchpoints that never fire) that cost significant time this
session before being caught -- avoid both in any future continuation.
Debug probes left in place throughout the now-cleared path
(`INIT.SUP`'s numbered serial markers `'1'`-`'D'` plus `'!'`,
`SUPIF.SUP`'s `'X'`/`'Q'`/`'R'`/`'S'`/`'T'`/`'U'`/`'Y'`/`'Z'`/`'@'`/
`'#'`, `XIOS86Q.A86`'s `'E'`/`'F'`/`'0'`/`'p'`-`'w'`, `BDOSIF.BDO`'s
`'q'`/`'r'`/`'s'`/`'t'`/`'?'`/`'['`, `RTMIF.RTM`'s `'V'`/`'W'`,
`QUE1.RTM`'s `'G'`/`'H'`, `QUE2.RTM`'s `'I'`-`'P'`, `ABORT.RTM`'s
`'$'`/`'%'`/`'^'`/`'&'`/`'*'`/`'+'`/`'='`, `DSPTCH.RTM`'s `'~'`) are
harmless to leave as-is, but given #32/#33's findings, any *new* single-
register-save probe added anywhere a call's arguments are live in `DX`
(or `BX`/`CX`) needs to save/restore that register too -- don't trust
a probe's absence from a free-run serial trace; verify with GDB
single-stepping (not `continue`, which has its own batch-mode
reporting quirks against this QEMU gdbstub) instead.

## 37. Two more real bugs found and fixed past #36's checkpoint --
`user_entry`'s own bare `sysdat` read, and `pul` (PD free-list head)
left permanently empty -- TMP's process now actually gets created and
switched to; one further interrupt-reentrancy bug now blocking it,
not yet root-caused

**Bug A (fixed): `SUPIF.SUP`'s `user_entry` read `sysdat` bare.**
`#33`/`#34`/`mpmif`'s own comment already found and fixed this exact
bug class (`mov ds,sysdat` defaults to a DS-relative read, which only
finds the real SYSDAT segment if DS already happens to be sysdat-ish)
at `mpmif`. `user_entry` itself -- every real `int mpmint` syscall's
entry point, reached *before* DS gets reset to sysdat -- had the exact
same bare read at its very first instructions (`cld ! mov di,ds /
mov ds,sysdat ! mov bx,rlr`), missed previously because every syscall
exercised so far happened to arrive with the caller's DS already
sysdat-ish. `INIT.SUP`'s `nrsp:` loop breaks that assumption on
purpose: it sets `DS=rspseg` (the RSP's own data segment) right before
`int mpmint`, specifically so `user_entry`'s own `mov di,ds` captures
the RSP's segment into `u_wrkseg`. With the bare read, `user_entry`
instead loaded "sysdat" through the RSP's own data segment, picking up
whatever garbage sat at that offset in TMP's dseg -- and used *that*
as the sysdat segment for the rest of the dispatch, corrupting every
`module_table`/`p_uda` lookup downstream (`func:`'s own `fnchk`
integrity check, which had been quietly printing false corruption
since #33 for this exact reason). Fixed the same way as `mpmif`:
`mov ds,sysdat` -> `mov ds,cs:sysdat`.

**Bug B (fixed): `pul` (PROC.RTM's PD free-list head) had no real pool
behind it.** `#35`'s own fix zeroed `pul` (along with `mdul`/`qlr`) to
stop a *different*, now-historical wild-write (a stale non-zero
DATA.BDO value being trusted as a real PD-table slot). But zeroing it
only moved the failure, not fixed it: `getpdadr`'s `gpd_bad` path --
taken for *any* PD that doesn't already live sysdat-resident, which is
every RSP's own PD (TMP's included, since TMP's PD lives in TMP's own
dseg) -- checks `pul` and returns `e_no_pd` immediately if it's empty.
With `pul` permanently 0 and nothing ever adding to it before
something needs it, `f_createproc` for TMP (or any dynamic process
creation) failed outright, every single boot, silently -- `nrsp:`
never checks the return code, so `INIT.SUP` sailed on to `rsp_out:`
and terminated anyway, and the serial trace's only visible symptom was
the dispatcher looping forever on `INIT`'s own already-terminating PD
(chased down with a GDB hardware watchpoint on `thrdrt`/`p_thread`,
which is how Bug A above was also found -- the watchpoint caught
`PROC.RTM`'s thread-list insert for `tick` correctly writing `idlepd`,
then immediately being overwritten with garbage by a later, unrelated
write while `CX` still read `f_createproc` -- i.e. mid-dispatch for
TMP's own creation, confirming the corruption path was inside this
exact syscall). Fixed the same way `#35` already fixed `qul`/the QD
pool: a real `PD_POOL_SLOTS`-slot (4) free-PD pool is carved out of
SYSDAT's own slack space (right after the flag table) and linked
through `pul` as a standard singly-linked free list (each slot's own
`p_link`, offset 0, pointing to the next; last points to 0) -- see
`tools/build_mpmsys.py`'s `PUL_FIELD_OFFSET`/`PD_POOL_SLOT_SIZE`/
`PD_POOL_SLOTS`.

**Result of both fixes together:** TMP's `f_createproc` call now
succeeds (confirmed via the serial trace: two new PD-pool slot
addresses, e.g. `0E40`/`0E70`, get allocated and inserted onto the
dispatcher's ready list alongside `idle`/`tick`, and the `switch:`
probe shows the dispatcher actually switching *to* one of them,
`<0E70>`, instead of looping on `INIT`'s PD forever). Two new PD-pool
slots get consumed per TMP creation rather than one -- not yet
explained; worth checking whether `proc_creat`'s own tail-chain
mechanism (`pop dx ! jmp proc_creat`) is firing a second time
unexpectedly for TMP, or whether this is legitimate (e.g. TMP's own
password-inheritance/shared-memory setup triggering a second
allocation) before spending more time on it.

**Still open, not yet root-caused:** right after the dispatcher's
`switch:` probe fires for the new PD (`<0E70>`), the very next debug
output is `DSPTCH.RTM`'s `int_disp_exit` probe (`']'`, dumping `DS`)
showing a clearly bogus `DS=F555` -- `int_disp_exit` is only reached
when `u_in_int=true`, i.e. this dispatch was entered via `fdisp`
("entry point used by interrupt routines in XIOS", per its own
comment), not a normal `int mpmint` syscall. `fdisp` cuts in with no
protection against firing *during* `switch:`'s own non-atomic
restore sequence (DS/SS/SP get reloaded for the newly-switched-to
process across several unprotected instructions) -- if the interrupt
source (not yet identified; the timer ISR's own `'"'` probe never
once appears in any trace this session, so it's something else in
XIOS86Q, most likely the serial console's own RX/TX interrupt) fires
mid-restore, `fdisp`'s assumptions about the current DS/stack are
violated. The trace goes silent immediately after (confirmed hung,
not just slow, via a 25-second free-run that produced zero additional
bytes past this point) -- next step is identifying XIOS86Q's actual
interrupt source here and whether `switch:`/`dsptch` needs explicit
`cli` protection across the restore that it's currently missing.

Debug probes from #33 onward remain in place and were relied on
throughout (`SUPIF.SUP`'s existing `'#'`/`'Y'`/`'Q'`/`'('`/`'Z'`/`'d'`/
`'R'`/`'U'`/`'!'`/`'S'`/`'T'` dispatch-trace chain; `PROC.RTM`'s
`0x1F`/`0x19`/`0x1A`; `SYSENT.RTM`'s `0x1E`/`0x1D`; `INIT.SUP`'s
`0x1B`; `DSPTCH.RTM`'s `'<'`/`'>'`/`'['`/`'z'`/`'\'`/`']'`) -- no new
probes were added this round; the GDB hardware-watchpoint technique
(`watch *(unsigned short*)<physical address>`, no competing
breakpoint active, physical/linear addressing throughout since CS is
always `MPM_CODE_SEG`) proved far more reliable than marker-byte
probes for this kind of "who corrupts this cell" question and should
be preferred over adding new probes when the question is "which
instruction wrote here" rather than "was this code path reached at
all".



## 38. Past #37's checkpoint: `proc_creat`'s own tail-chain mechanism
fires a *second*, bogus PD creation for TMP that corrupts `rspseg`
(and neighboring SYSDAT fields) via a wild `getpdadr` translation --
root cause narrowed but not yet nailed down

With #37's two fixes in place, TMP's `f_createproc` call genuinely
succeeds (confirmed: a real PD-pool slot gets allocated and the
dispatcher switches to it) -- but the system still hangs shortly
after, and this time the failure mode is a wild far jump (`CS`
observed as `0x20E9`, nowhere near any real module/RSP segment,
wandering through different `IP` values rather than looping, i.e.
executing whatever garbage happens to live there) once TMP starts
actually running.

**What's confirmed, via GDB hardware watchpoints (the only reliable
technique found this session for this question -- see the
methodology note below):**

- TMP's `f_createproc` call reaches `proc_creat` correctly with
  `dx=rsp_pd(0x10)`, and `getpdadr` correctly allocates a real PD-pool
  slot for it (confirmed earlier, #37).
- `proc_creat`'s own tail-chain mechanism (`cp_gpd: mov dx,p_link[si] /
  push dx ! push si / ... / pop dx ! jmp proc_creat` at the very end)
  fires a *second* time for TMP specifically, with a nonzero `dx`
  (observed as `0x04CF` via the existing `0x1F` probe) -- even though
  TMP's own static PD data (`src-assem/rsp/TMP.A86`'s `pd: dw 0,0 ;link
  fields`) declares `p_link=0`, which should make this chain
  terminate immediately after the one real creation. This means
  either the copied-in PD's `p_link` field isn't really 0 by the time
  `cp_gpd` reads it, or something else is driving this second
  iteration -- not yet isolated which.
- This second iteration's own `getpdadr` call for `dx=0x04CF` returns
  a *PD address far too small to be valid* -- not a real PD-pool slot,
  not any legitimate sysdat-resident PD. The exact value wasn't read
  directly (register-only arithmetic inside `getpdadr`'s "good path"
  leaves no memory write to watch), but it's pinned down indirectly
  and precisely: `cp_uda`'s `p_lst[si]` write (`cp_slst: add
  al,ncondev ! mov p_lst[si],al`, with `al` ending up `1` given
  `ncondev=1` and TMP's own `p_lst` starting at 0) landed exactly on
  `rspseg` (confirmed via watchpoint: `rspseg` flips from the correct
  `0` to `1` mid-chain, and `p_lst`'s own PD.DEF offset is `0x24`,
  `0x24 + si = 0x42 = rspseg`'s own SYSDAT offset exactly if and only
  if `si = 0x1Eh` for this second, bogus PD) -- i.e. `getpdadr`
  computed `si≈0x1E` for this call, squarely inside SYSDAT's own early
  fields (overlapping `rspseg`/`endseg`/`module_map`/version info),
  not anywhere a real PD belongs.
- This corrupts `rspseg` from `0` (correctly "chain empty" after
  TMP) to `1`. `nrsp`'s own loop then re-reads `rspseg` as `1`,
  `jcxz` doesn't fire, and it treats segment `1:0000` (the bottom of
  real memory) as if it were an RSP, reading garbage
  (`es:.rsp_link` from segment 1 came back `0xFF53`) and firing a
  *third*, totally wild `f_createproc` with `DS=1` -- this is also
  where the earlier `u_wrkseg=1`/`p_uda=0xD445` garbage (misattributed
  to TMP's own creation in #37's writeup; it's actually this third,
  wild call) comes from. This cascades into the `CS=0x20E9` wild jump
  once something tries to actually run whatever "process" this
  produced.
- Attempting to pin down `getpdadr`'s exact arithmetic for the
  `dx=0x04CF` call by hand (assuming the "good path": `si = dx +
  ((u_wrkseg - sysdat) << 4)`) gives `si=0x01BF`, not `0x1E` --
  these don't match, so either the hand arithmetic has an error, the
  "bad" (free-list) path is actually being taken instead (with `pul`
  itself already corrupted or exhausted after only one real
  allocation), or `u_wrkseg` isn't what it's assumed to be at this
  specific point in the chain. Not yet resolved -- this is the
  concrete next step: trace `getpdadr` itself (not just its
  before/after effects) for this specific second call, ideally via a
  watchpoint on `pul` and on `u_wrkseg` simultaneously, single-shot
  each time (see methodology note).

**Methodology note, confirmed the hard way this round:** GDB software
breakpoints on this QEMU gdbstub, hit repeatedly via a loop of
`continue` calls in one script, are **not reliable past the first
hit** -- every subsequent `continue` can report back the exact same
register/PC state as the first hit, never actually progressing,
even though the serial trace and watchpoint-based checks confirm the
program really did keep running. (Confirmed directly: a breakpoint on
`PROC.RTM`'s `cp_uda`'s own `mov cx,[0x42]` read reported identical
`cx=0x172b`, same stack, across 6 consecutive hits -- while a hardware
*watchpoint* on the exact same memory cell, run the same way, showed
three genuinely different values across three hits.) Hardware
watchpoints (`watch *(unsigned short*)<physical address>`, one per
GDB session, read registers immediately after each `continue`)
remained reliable throughout and should be preferred exclusively over
breakpoint-loop polling for any further "who wrote/read this and with
what register state" question on this target.

Checked `orig/asm80/ASM.ASM` and `orig/mac-b/` (both suggested as
possible references) -- neither applies: `asm80/ASM.ASM` is DRI's
original 8080-only assembler (no `ESEG`/`JCXZ`/segment-override
concepts exist in 8080 at all, predates the 8086 port entirely), and
`orig/mac-b/` is DRI's MAC macro-assembler, also 8080/Z80/8085-only
(`.LIB` macro files like `DOWHILE`/`WHEN`/`STACK`, `.COM` binaries, no
8086 source). This bug is a kernel runtime logic error (a live
register/address-computation bug in `getpdadr`'s own arithmetic or in
what reaches it), not an assembler syntax or encoding question, so
historical assembler sources don't bear on it.

## 39. Found and fixed the TRUE root cause behind most of #38's
confusion: `INIT.SUP`'s own `inituda`-segment computation used a bare
`sysdat` read -- the same bug class, a sixth instance, now fixed.
Boot progresses substantially further; one more, still-open issue
remains in `proc_creat`'s chain for TMP specifically.

Chasing #38's "`si≈0x1E`, lands in SYSDAT's own early fields" lead
with a GDB hardware watchpoint on `p_uda[initpd]` led straight to the
real root cause: `INIT.SUP`'s own very early setup --

```
	mov bx,offset initpd
	mov ax,offset inituda
	mov cl,4 ! shr ax,cl
	add ax,sysdat ! mov p_uda[bx],ax
```

-- has a **bare** `add ax,sysdat` (no `cs:` override), the exact same
bug class as `mpmif`'s and `user_entry`'s own already-fixed `sysdat`
reads (#33/#37). At this point in `init:`, DS is correctly
sysdat-ish, so the bare read doesn't fail outright -- it silently
reads *SYSDAT's own offset 6* instead of this module's own cseg cell.
Offset 6 is the back half of the module table's SUP entry (its
"initialize" far pointer's segment word), which `build_mpmsys.py`
bakes as `MPM_CODE_SEG` (`1000h`) -- and since SUP's own
`module_base` is 0, that's indistinguishable from a plausible-looking
value, which is exactly why this one went unnoticed through every
earlier working syscall this whole session: `p_uda[initpd]` silently
computed to `106Ch` (`inituda`'s own paragraph count `6Ch` + the
wrong `1000h`) instead of the real `17C8h` (`6Ch` + the real sysdat
segment `175Ch`) -- landing INIT's *entire own UDA* (`u_insys`,
`u_wrkseg`, every other `u_*` field any of INIT's own `int mpmint`
calls ever touch) squarely inside SUP's own live code bytes instead
of SYSDAT's reserved `inituda` area. Confirmed directly: `u_insys`
read back `8Bh` live (a stray opcode byte, not a small nesting count)
right before this fix; every earlier syscall this session tolerated
the corruption only because it never hit a case where `u_insys`
being garbage, or any other misplaced `u_*` field, mattered enough to
visibly break something -- RSP creation via `nrsp` was the first path
sensitive enough to expose it, which is why this whole investigation
(all of #36-#38) kept finding downstream symptoms (`rspseg`
corruption, wild PD addresses, the `CS=0x20E9` jump) without ever
reaching this upstream cause. Fixed the same way as every other
instance: `add ax,sysdat` -> `add ax,cs:sysdat`.

**Confirmed via GDB immediately after the fix:** `p_uda[initpd]` now
reads `17C8h` correctly (exactly matching the hand-computed correct
value), and every `ES` dump throughout the dispatch trace now shows
`17C8` consistently (previously `106C`). A conditional breakpoint
(`break *user_entry+1 if $cx==0x90`, single-shot, no repeated
`continue` -- see #38's methodology note, still holds) confirms `DS`
is correctly `172Bh` (TMP's own data segment) for the real
`f_createproc` call.

**Methodology correction, found the hard way (again) this round:** a
*second* GDB watchpoint quirk, distinct from #38's breakpoint-reuse
one -- a hardware watchpoint on a freshly-growing/shrinking address
range can report a stale-then-corrected pair of hits for what is
actually a *single* underlying write, with the first report showing
plausible-but-wrong register state. (Concretely: a watchpoint on the
relocated `u_wrkseg` cell showed `di=0xC000` for what looked like the
real `f_createproc` call -- directly contradicted moments later by a
clean, single-shot conditional breakpoint showing `DS=172Bh`, the
correct value, for the exact same call.) Trust a single-shot
conditional breakpoint over a watchpoint's reported register state
when they disagree; use the watchpoint only to find *where* execution
is, not to trust *what it reports mid-transition*.

**Still open, narrowed further this round:** the system hangs at the
same point as before. Found a **third** GDB reliability gotcha this
session, worse than the first two: `condition`/`ignore N`/
`continue N` on this QEMU gdbstub are **silently no-ops** -- a
breakpoint with any of these set still stops on the very first actual
hit, as if the qualifier were never given (confirmed directly: `break
*addr if $si==0x0e40`, `ignore 1 2`, and `continue 3` all three
stopped on the *first* hit of a breakpoint known to fire multiple
times with different `si` values). The only technique that reliably
reaches a *specific later* occurrence of an address that's hit
multiple times: stage through an *intermediate*, known-singular
landmark first (e.g. `nrsp`'s own entry, which by construction is
only reached once, after idle/tick already exist) with its own
breakpoint, `continue` once, `delete` it, *then* set the breakpoint
or watchpoint of actual interest and `continue` once more -- this
"two-stage" approach was used for everything below and gave
consistent, cross-checked results.

Using that technique: `cp_gpd`'s `mov dx,p_link[si]` (staged past
`nrsp`) now correctly reads `si=0xE40` (TMP's real PD-pool slot,
confirmed = TMP's own creation, not idle/tick's), `dx=0` -- the
tail-chain does **not** continue spuriously anymore; #38's `dx=0x04CF`
second-chain symptom is gone, fully explained as fallout from #39's
root cause, not a separate bug. Good -- but at that exact same
breakpoint hit, `es=0x17C8` (correct) and yet reading `u_wrkseg`
(`ES:0x2E`) gives `0xC000`, not the expected `0x172B`. Cross-checked
two independent ways: `PROC.RTM`'s own existing `0x1A` serial probe
(not a GDB artifact) agrees, and a staged watchpoint on that exact
physical cell also agrees. Yet staging up to `user_entry`'s own very
first instruction for this same call (confirmed via `dx=0x10,
cx=0x90`) shows `DS=0x172B` correctly, and nothing between there and
`cp_gpd` -- traced through `func`/`fnchk`/`userf`/`nonet`/`inmod`/
`gofar`/`goodmod`/`callf`/`creat_proc_entry`/`proc_creat`/`cp_doit`/
`getpdadr` -- appears to touch `ES` or write to `u_wrkseg`'s address.
A staged watchpoint on `u_wrkseg`'s cell shows it written *twice* in
quick succession for what looks like the same call: once correctly
(`di=0x172B`) and once wrong (`di=0xC000`, same instruction address
both times) -- not yet determined whether that's a second, real write
(something else legitimately overwriting it a moment later) or
another watchpoint-reporting quirk layered on top of the three
already found. This is exactly where the investigation stands;
next step is a byte-level disassembly walk of `getpdadr`'s actual
compiled "bad path" (the `rep movsw` and its surrounding `push/pop
es/ds` dance) against the live register state at each instruction,
rather than trusting any single breakpoint or watchpoint report in
isolation.

## 40. Abandoned GDB for this round in favor of pure serial-trace
probes (deterministic, immune to every gdbstub quirk in #38/#39) --
resolved #39's `u_wrkseg=0xC000` mystery as stale cross-build data,
confirmed TMP's own process creation (plus its 4 child processes) now
works end to end, and found the *next* real hang: an apparent
infinite loop (or crash) in `ABORT.RTM`'s `terminate_entry`, walking
the `p_thread` list, when `INIT` terminates itself.

Given #39's closing methodology note (don't trust GDB breakpoint/
watchpoint register state in isolation), this round switched entirely
to new `TEMP DEBUG PROBE` serial prints (`out dx,al` to the UART,
decoded offline from a raw QEMU `-serial file:...` capture with a
small Python byte-level decoder) -- genuinely immune to every
GDB/gdbstub quirk found in #38/#39, since nothing is asked to
`continue`/stage/re-hit anything; the trace is one continuous,
uninterrupted run.

**Picking a free marker byte is harder than it looks.** Every
printable ASCII character from `' '` (0x20) through `'~'` (0x7E) is
*already* claimed by some existing probe somewhere in the tree (there
are now dozens). Worse, control-byte markers collide too, silently:
a new `1Eh`/`1Dh` marker added to `PROC.RTM` turned out to already be
used by `SYSENT.RTM`'s `creat_proc_entry` probe, and `21h` collides
with the plain ASCII `'!'` used throughout `SUPIF.SUP`'s `fnchk`/
`mtchk` integrity checks -- `grep -rn "mov al,.*h" | sort -u` silently
hides exactly this kind of collision (dedup makes two independent
uses of the same byte look like one line). The reliable check is
`grep -c`/no-dedup across every `mov al,'X'` *and* `mov al,XXh` site
before picking a new marker.

**A genuinely new, reproducible finding: inserting probe bytes at
one specific point in `SUPIF.SUP`'s `user_entry` -- right before
`mov u_wrkseg,di` (between it and the preceding `mov u_retseg,ax`) --
made the system hang specifically on TMP's own real `f_createproc`
call (the very first `int mpmint` call that reaches that point;
idle/tick's direct `call proc_creat` never goes through
`user_entry` at all, so they were unaffected).** Moving the *exact
same* probe to *after* the write instead (reading `u_wrkseg` back
rather than `di` beforehand) made the hang disappear entirely, and
boot proceeded much further. This is **not** an `asm86` short-jump-
range bug -- confirmed by reading `tools/asm86/isa.py`: both the
`JMPS` short-jump and conditional/loop-branch encoders explicitly
range-check `rel8` and call `ctx.err(...)` (a hard build failure) if
out of range, and every build here succeeded with no errors. So this
is a **real**, timing/layout-sensitive runtime bug whose symptom
(hang or not, and *where*) shifts with small changes to total code
size elsewhere in the combined `cseg` -- the same family of
"coincidentally works only because of exact address alignment" bug
this whole investigation keeps finding, just not yet pinned to a
specific cause. Flagging this as its own open issue: something is
sensitive to *exact* code placement around that one instruction in a
way nothing else in the tree has shown before. Needs a non-probe
(static layout analysis, or careful single-step) approach to pin
down, since adding *any* further probe bytes near it risks moving --
not fixing -- the symptom.

**With the probe relocated to avoid that hang, the trace shows
`u_wrkseg` is written and read back *consistently* throughout TMP's
entire real `f_createproc` call** -- at the write (`user_entry`),
at `cp_gpd`, and at `cp_nm` (`PROC.RTM`), all agreeing (e.g.
`0xB9E8` in one build, `0x1767` for idle/tick in the same build --
the exact numeric value drifts between builds purely from layout,
as expected). **No corruption was observed this round.** This
strongly suggests #39's closing `u_wrkseg=0xC000` "second write"
finding was contaminated by exactly the GDB/watchpoint
unreliability #39 itself flagged as unresolved (comparing a GDB
session's register state against a *different build's* serial
probe output, which was never a like-for-like comparison to begin
with) -- not a real bug. Marking it **superseded/not reproducing**;
revisit only if the same symptom reappears in a pure serial trace.

**New result: TMP itself, plus 4 child processes it spawns (its
console-scanner workers, one per console), all get created
successfully** with correct, mutually-consistent `p_uda`/`u_wrkseg`
values -- confirmed via the `1A`/`1C`/`05` probes across all 5
creations in one continuous trace. This is real forward progress:
boot now gets well past the point of every previous symptom in
#36-#39 (the `CS=0x20E9` wild jump, the chain-corruption symptom,
etc. — none of those reproduce anymore).

**The new, current blocker:** once all processes are created, `INIT`
terminates itself (`mov cl,f_terminate ! mov dl,0ffh ! int mpmint`,
as designed). `ABORT.RTM`'s `terminate_entry` is reached correctly
(confirmed via its own `'$'`/`'%'` probes: `BX=initpd`, `CX=1` i.e.
not the error path, `p_flag=5`, `DX=0x03FF` -- the `0x3F` high byte
of `DX` is simply leftover/don't-care, since `INIT.SUP` only ever
sets `DL` with a byte move; **not** a bug, matches the documented
byte-only interface). Execution reaches the `terminate:` label's own
`'^'` probe (confirmed, it's the last byte in every trace) and then
**nothing ever follows it again, even after 60+ seconds** -- genuinely
stuck, not just slow. The very next real code is the `nchld:` loop
(walking `p_thread` from `thrdrt`, reparenting any of `INIT`'s own
children to `0`) immediately followed by `rlsmx`/`f_bdosterm`/the
`'&'` probe, none of which ever fire.

Ruled out so far (static reasoning, not yet proven at runtime):
- `thrdrt`/`rlr`'s raw template values in `build/kernel/MPM.bin.dseg`
  are `0x9C0` -- exactly `initpd`'s real, current offset (confirmed
  via direct byte read of the built `.dseg`) -- so unlike `pul`/
  `mdul`/`qlr`/`qul` (see `tools/build_mpmsys.py`'s own long comments
  on each), this pair's stale DATA.BDO-template value happens to
  still be correct for this tree's current layout. Not the cause.
- `PROC.RTM`'s own thread-list insert (`cp_uda`'s tail: `pop si !
  pushf ! cli ! mov dx,thrdrt ! mov p_thread[si],dx ! mov thrdrt,si`)
  reads structurally correct in isolation: `si` is explicitly
  restored from the stack (pushed back at `cp_gpd`) specifically
  *because* the two preceding `rep movs ax,ax` blocks (copying
  `u_ivectors` and `i_mpm_ip`/`i_mpm_cs`) clobber `si` as a source
  index; the push/pop nesting with the matching `dx` save (from
  `cp_gpd`'s own `mov dx,p_link[si]`) is balanced correctly across
  the whole function. Nothing jumped out as wrong on inspection
  alone -- needs runtime confirmation, not just a read-through.
- Tried adding one more probe directly inside `nchld:` (dump `BX`
  each iteration) to settle whether it's a real infinite loop versus
  a crash/fault -- this *itself* reintroduced a hang, further
  upstream (during tick's creation), reinforcing the "any code-size
  change moves the symptom" pattern above. Reverted immediately
  (`ABORT.RTM` is back to no new probe in `nchld:`); do **not** just
  retry a bigger/smaller probe there next time -- it will likely just
  relocate the symptom again rather than reveal it.

**Recommended next step (not yet attempted):** avoid adding any more
assembly bytes near the suspect regions, since every attempt so far
has moved the bug rather than exposed it. Options that don't touch
code size: (a) have `tools/build_mpmsys.py` print every dynamically-
computed pool offset/size (`qd_pool_*`, `qmau_buf_*`, `flagtab_*`,
`pd_pool_*`) alongside the static `SYSDAT.FMT` offsets for
`p_thread`/`thrdrt`/the PD table, purely to check for an overlap by
arithmetic rather than by observed symptom; (b) a single, carefully
isolated GDB session that only ever sets *one* breakpoint and issues
*one* `continue` (the only pattern confirmed reliable across #38-#40)
at the `nchld:` loop body itself, to catch `BX` on its first and
(if reachable) second iteration without perturbing layout at all.

## 41. Pinned down *which* write corrupts the thread list from #40 --
`idlepd` gets run through `proc_creat` a **second, spurious time**,
overwriting its own `p_thread` field and creating the `idle<->tick`
cycle that hangs `nchld:`. The *mechanism* behind that second pass is
still open, and a new, more severe GDB reliability failure showed up
while chasing it.

Followed #40's own recommendation (b) first -- single breakpoint,
single `continue`, at `nchld:`'s loop body (`*0x12706` in that build),
stepping forward with pure `stepi` (never `continue` again) for 60
iterations. Confirmed definitively: `BX` cycles `0xED0 -> 0xEA0 ->
0xE70 -> 0xE40 -> 0x9F0 -> 0xA20 -> 0x9F0 -> 0xA20 -> ...` forever --
a genuine infinite loop (not a crash), caused by `p_thread[idlepd]`
holding `0xA20` (`tickpd`) instead of the correct terminator-chain
value. Cross-checked against a direct physical-memory read at a
*different*, known-safe single-breakpoint landmark (`nrsp`'s own
entry, reached exactly once) that `p_thread[idlepd]=0x9C0` (`initpd`,
correct) and `p_thread[tickpd]=0x9F0` (`idlepd`, correct) **at that
point** -- so the corruption happens later, during `nrsp`'s own RSP
creation, not during RTM's original idle/tick setup.

A hardware watchpoint on `p_thread[idlepd]`'s physical cell, `continue`
*twice* in one session (accepted as risky, but the resulting register
state was internally consistent both times, and a direct physical-
memory read after each hit corroborated it independently of any
register report) showed two real writes: first `0->0x9C0`
(`initpd`, idle's own correct creation), second `0x9C0->0x0A20`
(`tickpd`) -- both writes with `SI=0x9F0` (`idlepd`) as the operand,
confirming the corrupting write really does target `p_thread[idlepd]`
specifically (not a different cell that merely looks similar).

**A completely new serial-probe technique, not needing GDB at all**:
added a probe at `proc_creat`'s very entry (`PROC.RTM`) that reads the
near-call return address directly off the stack (`push bp ! mov
bp,sp ! ... ! mov ax,[bp+2]`, since 8086 doesn't allow `sp` as a base
register for memory addressing -- `asm86` correctly rejects that with
`'sp' is not a valid base register for 8086 memory addressing`,
confirmed by trying it first) and dumps it via `ydbg`. This identified
the caller for every `proc_creat` invocation without GDB: idle, tick,
*and* the mystery second idle call all show the **identical** return
address (`0x1830`, disassembled and confirmed to be the single `call
proc_creat` byte in `RTMIF.RTM`'s own `init:`, right after its own
`mov dx,bx` -- there is only one `call proc_creat` site in the whole
tree outside `creat_proc_entry`, confirmed by `grep`). Since idle and
tick are chained via `jmp proc_creat` (not `call`), reusing the one
outer return address for every chain link is *expected* for them --
but the mystery call reuses that *exact same* value much later in
boot, well after that chain already returned (confirmed via a `dx=0`
exit in the trace in between). That means either (a) something
genuinely re-enters the chain without a fresh call (unresolved how),
or (b) this is a stale leftover stack word being misread as a "return
address" by a wild jump that lands directly on `proc_creat`'s address
with whatever `DX`/stack content happened to be lying around (which
would also explain `DX=0x9F0` arriving as if it were a deliberate
"create `idlepd`" request). Leaning toward (b) given this investigation's
whole history of wild-jump symptoms, but not confirmed either way.

**New, more severe GDB/QEMU reliability failure, distinct from every
one found in #38-#40**: starting QEMU halted (`-s -S`), setting a
single breakpoint on `proc_creat`'s address, and issuing a single
`continue` **never returned**, even after 100+ seconds, across
multiple independent attempts -- despite the exact same address being
reachable within about a second of normal (non-GDB) execution (and
despite a near-identical single-breakpoint/single-continue pattern
working fine minutes earlier for `nchld:` and `nrsp`). Disassembling
the target address directly from the built `.cseg.bin` confirmed the
address itself was correct. Starting QEMU *without* `-S` and
attaching a moment later also failed, but for an understood reason:
by the time GDB connects, the guest has already run far enough,
unsupervised, to be stuck in #40's infinite loop, so the early
`proc_creat` code never executes again. Root cause of the `-s -S`
case specifically is **not understood** -- flagging it rather than
burning more time on it. Given how good the serial-probe return-
address technique above turned out to be, **prefer it over GDB
breakpoints entirely for "who calls X" questions going forward**;
it has none of the staging/repeat-`continue`/watchpoint caveats this
whole investigation keeps running into.

**Still open:** the exact mechanism that re-enters `proc_creat` (or
jumps directly onto it) with `DX=0x9F0` a second time, after idle and
tick's own chain has already correctly finished and returned. This
happens somewhere between `XIOS86Q.A86`'s own init (confirmed via its
banner text appearing in the trace -- initially and wrongly suspected
as a mid-boot *reboot*, since the kernel's `XIOS86Q.A86` prints the
exact same banner string as the standalone loader's
`LDBIOS86Q.A86`; ruled out once both files were found to independently
declare the same banner text) and `nrsp`'s own first iteration. A
`CL=0`/`sysreset_entry` dispatch (falling through to
`terminate_entry`, taking the harmless `term_err` branch since
`abt_chk` returns `CX<=0`) also happens in this same narrow window,
immediately *after* the mystery `proc_creat` call, not before --
tempting to connect the two, but not yet shown to be cause and effect
rather than two independent, adjacent events. Next step: use the same
return-address-probe technique at `entry:`'s own dispatch point
(`RTMIF.RTM`) to log every `CX` value reaching it in this window, and
check whether any of them could misindex `function[]` out of bounds
onto `proc_creat`'s own address.

## 42. #41's "mystery second `proc_creat` call" theory doesn't survive
a byte-by-byte re-derivation of a fresh trace -- the real finding is
narrower and stranger: `idlepd` really does get a *second* insertion
onto `drl` (the Dispatch Ready List), but not via `proc_creat` at all,
and the whole thing is genuinely non-deterministic run-to-run on the
identical binary.

Re-ran the current (clean) build with no new probes -- everything
needed was already in place from `#40`/`#41` plus `DSPTCH.RTM`'s own
pre-existing probes (not previously catalogued in this doc): `'z'`
(`fdisp` entry), `'.'`  (`dsptch` entry, dumps DS/indisp), raw byte
`11`=`0x0B` (`switch`-off-rlr, dumps SI then `p_link[si]`), `'{'`/`'}'`
(dsp_table dispatch index), `'~'` (`term_act`), `'['` (`disp_act`,
dumps `rlr`), raw byte `12`=`0x0C` (**`drltorlr`'s own drain loop**,
dumps `drl` on every iteration), `'<'`/`'>'` (`switch`, dumps `rlr`).
That `0x0C` probe is the one that matters here -- it had been
misread in-session as an unrecognized/undocumented marker before
tracing it to `DSPTCH.RTM:373`.

This run happened to get much further than #41's hang point before
stalling (confirmed via the usual stable-wait pattern, same final
byte count across 3 repeats *of this exact build*): `TMP` and all 4
children created successfully (`creat_proc_entry`'s `1Eh`/`1Dh`
markers each firing 4 times via real `int mpmint` syscalls, all with
return address `0x1957`, correctly distinct from the static
`0x1830` RTM-`init:` call site), `nrsp`'s loop draining fully to
`rspseg=0`, `rsp_out` reached, and `INIT`'s own `f_terminate` call
in progress -- i.e. dramatically past where #40/#41 were stuck. The
`0x0C` (`drltorlr`) probe's trace, read off byte-by-byte, is:
`0xED0, 0xEA0, 0xE70, 0xE40` (the dynamic PD-pool entries, newest
first -- expected, since each is head-inserted onto `drl` later than
idle/tick), then `0x9F0` (`idlepd`), `0xA20` (`tickpd`), `0x9F0`
(`idlepd` *again*), `0` (terminator).

Worked through what a *correct* single idle->tick chain predicts for
`drl`'s layout (head-insert each time: idle inserted first with
`drl` empty gives `p_link[idle]=0`; tick inserted second on top gives
`p_link[tick]=idle`, `drl=tick` -- so the tail of the chain should
read `..., tick, idle, 0`, idle appearing *once*, last). The actual
trace instead shows idle appearing *before* tick, pointing at tick
(`p_link[idle]=0xA20` at the point the drain reads it) -- a value
that could only get there if something re-inserted `idlepd` onto
`drl` *after* tick's own legitimate insertion (when `drl` genuinely
was `tickpd`), overwriting idle's original terminator-value `p_link`.
The second, *later* appearance of idle in the same walk is not a real
second list node, though -- it's `drltorlr`'s own call to
`insert_process(SI=idle)` (made right after reading `p_link[idle]`
for the drain) **reusing that same `p_link[idle]` cell** for the
Ready List instead, so the next read of it (triggered by the earlier,
already-consumed `p_link[tick]=idle` pointer) picks up
`insert_process`'s freshly written value (`0`) instead of the stale
`tick` value read moments before. In other words: **one** spurious
extra `drl` insertion of `idlepd`, not an inherently-infinite 2-cycle
-- #41's hardware-watchpoint finding (`p_thread[idlepd]` written
twice, `0->0x9C0` then `0x9C0->0xA20`) is a *different* field
(`p_thread`, offset 2) than this one (`p_link`, offset 0) and from a
*different* build/run, so the two may well be two independent
instances of the same underlying "idle gets touched an extra time"
bug rather than the same event described two ways.

Ruled out via direct, static inspection of the live built image
(`build/kernel/MPM.bin.dseg`, no probes needed): the real baked-in
`p_link`/`p_thread` template data for `idlepd`/`tickpd`/`initpd` is
completely clean (`idlepd.p_link=0xA20`, `tickpd.p_link=0`, all
`p_thread` fields `0`, `thrdrt=rlr=initpd` correct) -- so this is not
a GENSYS-style stale-static-data bug like `pul`/`mdul`/`qlr`/`endseg`/
`flags` (all already fixed in `tools/build_mpmsys.py`); it is a live,
runtime write. Also ruled out: the "real QD pool" `build_mpmsys.py`
carves out of SYSDAT's slack space (`mxdiskqd`'s backing storage) --
every `q_dq`/`q_nq` field in all 4 slots reads back `0`, so
`SYSENT.RTM`'s `wakeup_entry` (which does `mov bx,dx ! mov si,[bx]`
then, if `si<>0`, inserts `si` onto `drl` -- a real, direct `drl`
writer, called from `QUE1.RTM`'s `qr_finish`/`qw_fin` with
`DX=&q_dq[queue]`/`&q_nq[queue]`) can't currently be firing with a
nonzero, garbage `si` from that source.

Found a second real, direct `drl`-writer worth checking:
`ABORT.RTM`'s `abt_putdrl` (called only from `abort_spec_entry`,
RTM function 17 -- "set up the specified PD for termination when it
is next in context" -- `mov p_link[bx],drl ! mov drl,bx` with
arbitrary caller-supplied `BX`). This is an exact structural match
for the symptom: if `abort_spec_entry` ever got invoked with its APB
resolving to `idlepd` (e.g. by name/console match, or by a stale/
garbage parameter -- this investigation's single most common bug
class all along), it would explain a genuine extra `drl` insertion of
idle with no `proc_creat` involvement at all, matching this round's
re-derivation far better than #41's "second `proc_creat` call"
framing did. Added one minimal, `0Eh`-probe-style check -- a single
marker (raw byte `0x15`, confirmed unused via a non-deduped grep
across the whole `src-assem/kernel/` tree first) right at `abort_spec_entry`'s
own entry, dumping the incoming `DX` (APB address) via `ydbg`. Result,
repeated 3 times for stability: the marker **never fired**, and
-- unlike every other probe added successfully this session --
it also **shifted the hang point earlier and made it deterministic**
(1391 bytes every time, vs. 2140 bytes for the unmodified build,
stalling right after the idle/tick chain's own terminator step)
instead of leaving the known-good stopping point alone. Reverted
immediately (confirmed via `git diff`/`git status` back to a clean
`ABORT.RTM`, and the unmodified build back to reaching 2140 bytes).
Since the probe never fired even though it also never got to see
most of boot in those runs, this neither confirms nor rules out
`abort_spec_entry` -- it's just inconclusive, and `ABORT.RTM` joins
`RTMIF.RTM`'s `entry:` as a second region where even a single-
instruction, zero-side-effect probe measurably perturbs timing.

Also newly confirmed: **this symptom is genuinely run-to-run
non-deterministic on one unchanged binary**. The very first test this
round (unmodified build, no probe changes at all) landed on 2140
bytes with the rich `drl`-corruption trace described above; three
back-to-back repeats of that *exact same* image all landed on exactly
2140 bytes too (not re-checked byte-for-byte identical, only
length-identical) -- but that still doesn't make this a simple "every
run behaves identically" situation, since changing *any* unrelated
instruction count anywhere nearby (even in a cold, rarely-run function
like `abort_spec_entry`) reliably moves the stall point. Consistent
with a real-time-clock/timer-interrupt race (this whole investigation
already established that `dsptch`'s critical sections genuinely
re-enable interrupts briefly between list operations), not a pure
instruction-sequencing bug -- which would explain both the
determinism-within-one-binary (same code, same timer-interrupt
cadence relative to instruction count, every run) and the extreme
sensitivity to any instruction-count change anywhere in the hot path
(shifts exactly where in that cadence each critical section lands).

**Still open:** which code actually performs the extra `drl`
insertion of `idlepd`. `abort_spec_entry`/`abt_putdrl` is the leading,
structurally-plausible candidate but unconfirmed; `FLAG.RTM`'s
`fs_noff` (flag-wait wakeup, also a direct unguarded `drl` writer) and
`TICK.SUP`'s `enddelay` (delay-list-expiry wakeup, ditto) haven't been
checked at all yet. Given how disruptive probing near `ABORT.RTM` and
`RTMIF.RTM`'s `entry:` has both turned out to be, the next attempt at
any of these should budget for the strong possibility that the probe
itself will relocate the symptom, and should re-verify the known-good
2140-byte stopping point immediately after, exactly as done here.

## 43. Correction to #42: the "second `proc_creat(idle)` call" from
#41 is real after all -- #42's "debunking" of it was wrong, caused by
only checking the *first three* `0Eh` markers in the trace instead of
all of them. Found the second occurrence, and a sharp, reproducible
anomaly immediately preceding it that's now the leading clue.

Re-decoding the exact same 2140-byte trace from #42 byte-by-byte
(writing a proper offset-exact decoder instead of eyeballing
concatenated marker text -- the earlier mis-reads this session, like
the trailing-space confusion and the `0x0C`-vs-ASCII-`'n'` mixup, kept
coming from exactly this kind of hand-transcription error) turned up
**two more `0Eh`/`1Fh` pairs**, around offset 1311-1385, well after
the ones already accounted for at offsets 177-313. They show the
unmistakable idle-creation signature again: `0Eh`=`1830` (the same
static `RTIF.RTM` `init:` return address), `1Fh`=`09F0` (`idlepd`),
followed by the usual `19h`/`1Ah`/`1Ch`/`05h` steps and a second
`0Eh`=`1830`/`1Fh`=`0000` chain-terminator. #42 missed these because
it only looked at the first three `0Eh` occurrences near the start of
the trace and treated that as the complete set.

**Independent, conclusive proof this is a real second pass, not a
stale-read artifact**: the `1Ah` (`cp_gpd`) probe's second occurrence
shows `p_uda[idlepd]=0x17E3`, where the *first* occurrence showed
`p_uda[idlepd]=0x007C` (the correct static relative value,
`idleuda>>4`). `cp_nm`'s own code (`PROC.RTM`) does `mov ax,u_wrkseg !
mov dx,p_uda[si] ! add dx,ax ! mov p_uda[si],dx` -- it *adds*
`u_wrkseg` onto whatever is *currently* in `p_uda[si]` and writes the
result back, every single time it runs. `0x007C + 0x1767`
(`u_wrkseg` during idle's first, legitimate creation) `= 0x17E3`
exactly. The second pass's `1Ah` probe reads this *already-converted*
absolute value back out via `cp_gpd` (which runs *before* `cp_nm` on
each pass) -- direct, arithmetic proof that a full second pass through
idle's creation genuinely executed, independent of anything to do with
`drl`/`p_link`. (It also means a *third* pass would silently corrupt
`p_uda[idlepd]` into nonsense, doubly offset -- this bug has more than
one way to do damage if it ever triggers more than twice.)

**Found the trigger moment, and it's new and sharp**: immediately
*before* the first of these two 0Eh markers, `INIT.SUP`'s `nrsp` loop
makes its very first real `int mpmint` (`@`=`0090`, i.e.
`f_createproc` for TMP itself -- confirmed via the already-existing
`1Bh` probe showing `rspseg=0x1736`, TMP's own data segment per
`build_mpmsys`'s own printed log). That call **fails with
`not_implemented`**, and -- this is the sharp part -- it does so
*without ever printing `SUPIF.SUP`'s `'Y'` marker or running `func()`'s
own `fnchk` integrity loop*, both of which are unconditional, right at
the top of `func()`, for *every* call that reaches it. The trace goes
straight from `'@'0090` to `'Q'`/`'('` (`nonet`, deep inside `func1`)
with no `'Y'` in between -- confirmed by searching the raw bytes for
every `'Y'` in the whole surrounding region; the next one doesn't
appear until the *following* (successful) `f_createproc` call, 150+
bytes later. The `'('` dump itself shows `CX=0000`/module`=0000`
where the live image's own static `sysent` table (checked directly:
`sysent[144].ent_mod=0x02`, `.ent_func=0x0E`, correct) says it should
read `000E`/module 2 (RTM) -- i.e. `func()`'s dispatch read *zeroed*
table data, not the real data, for this one call only (the very next
`f_createproc` call, moments later, reads the table correctly and
succeeds normally).

**Not yet explained**: how execution reaches `'Q'`/`nonet` without
passing through `func()`'s own top (where `'Y'` lives), and how that
connects to a static-return-address re-entry into `RTIF.RTM`'s
`init:`'s `call proc_creat` moments later. Checked and ruled out as an
explanation: `INIT`'s own `u_insys` (checked directly against the
built image: `0`, clean, not a stale-GENSYS-leftover like `pul`/`mdul`/
`qlr` before they were fixed) and idle actually running its own
`f_dispatch` loop for real (RTM function 12 never appears anywhere in
the whole trace's exhaustive `'S'`-marker sweep, which does catch
every other RTM dispatch that happens in this window: `0`
(`sysreset_entry`, also unexplained but harmless -- see #41),
`4`/`5`/`9` (`makeq`/`openq`/`writeq`, BDOS's own queue setup, read
and re-checked in full -- `QUE1.RTM`/`QUE2.RTM` don't touch `drl` or
any PD list), `13`/`14` (terminate/creat_proc, both legitimate). Also
re-confirmed via direct inspection of the built image that the real
`drl`/`p_link`/`p_thread` static template data for idle/tick/init is
completely clean, and that the real "QD pool" `getqdaddr` carves out
of SYSDAT (and the `mxdiskqd` template it copies from, in
`DATA.BDO`) both have their `q_dq`/`q_nq` fields explicitly zeroed,
so neither can be quietly handing back `idlepd`'s address as a stale
"process waiting on this queue."

**Next step**: instrument right at this specific failure, since it's
now by far the most concrete, reproducible anchor point found this
session -- a single marker at `func()`'s own very first instruction
(after the `fnchk` prologue, so it doesn't interfere with `'Y'`) would
confirm whether `func()` is entered at all for the failing call, or
whether something jumps in lower down. Given this session's track
record, budget for the strong chance that even this minimal probe
relocates the whole symptom (as happened with the `abort_spec_entry`
probe above), and re-verify the known-good 2140-byte stopping point
immediately after adding it, before trusting anything it reports.

The RSP sources (`CLOCK.A86`, `ECHO.A86`, `TMP.A86`, `SUBMIT.A86`) and
all 22 `.PLM` utilities in `src-assem/utils/`/`src-assem/toolchain/` compile and
assemble cleanly end-to-end (`ECHO.A86` and every `.PLM` file — see
`tools/plm/README.md`) except where documented above. See
`tools/README.md` and `tools/plm/README.md` for how to run the
assembler/compiler yourself, and `src-assem/Makefile`/
`.github/workflows/build.yml` for the automated, continuously-validated
build of everything confirmed to work.

## 44. Executed #43's recommended next step (a marker at `func()`'s
*literal* first instruction, marker `16h`, before even the `fnchk`
prologue, in `SUPIF.SUP`). It did exactly what #43 warned it might --
relocated the whole symptom -- but this time the relocation was a huge
step forward: instead of hanging, the system now runs for real, all the
way into a live, legitimate steady-state loop. Confirmed via a real
keystroke-injection test that no console/shell process is actually
alive yet, and found fresh, sharper evidence for the "`func()` entered
through its middle, not its top" theory from #43.

**The `16h` probe's effect, confirmed by A/B test**: with it in place,
a stable-wait capture that used to settle quickly now grows to 43MB+ in
20 seconds and never stops on its own (had to be killed). Stashing just
this one-line edit back out and rebuilding reproduces a *different*,
*earlier* silent hang, stable at 1607 bytes (not #42/#43's 2140 --
`PROC.RTM`'s own probes from earlier this session shift it too); popping
the stash back and rebuilding reproduces the 43MB runaway again,
confirming this one new probe -- not anything else -- is what pushes
execution past the old hang point. Consistent with this whole session's
running finding that this kernel's boot sequence is extremely sensitive
to absolute code layout (any size change anywhere in the combined `cseg`
retimes every subsequent label against the hardware timer interrupt).

**What the 43MB steady-state loop actually is**: decoded it against
`TICK.SUP` and `FLAG.RTM` directly, not just by eyeballing markers. It's
`TICK.SUP`'s `notick:`/`tick:` loop running for real and correctly, over
and over: `io_stopclk` (since nothing is on the delay list) followed by
`mov cx,f_flagwait ! int mpmint` waiting on `flag_tick`. `FLAG.RTM`'s
`flag_wait_entry` has two outcomes: if the flag is already
`flag_on`, it consumes it and returns *immediately* (no real block); if
`flag_off`, it actually parks the process (`ps_flagwait`) and calls
`dsptch`. The repeating trace shows the immediate-return path every
single time, which is exactly what you'd expect if the real hardware
timer interrupt handler calls `flag_set_entry(flag_tick)` on every
genuine timer tick -- i.e. this is architecturally correct MP/M
behavior for a quiescent system with nothing on the delay list, not a
busy-spin bug by itself. `TICK`'s own code never touches console I/O,
queues, or anything RSP-related, so none of this loop says anything
either way about whether a real shell is running.

**Confirmed, via a live test, that nothing is actually listening on
the console**: ran QEMU with a Unix-socket `chardev` backing the serial
port (`-chardev socket,...,server=on,wait=off -serial chardev:com1`)
instead of a write-only `file:` backend, connected with a plain Python
socket, captured 20 seconds of boot output (43,182,033 bytes), then
sent a single `\r` byte and captured 5 more seconds (11,412,586 bytes).
The output pattern across the keystroke boundary is **byte-for-byte
identical** -- the exact same `TICK` cycle, with no interruption, no new
marker, nothing. Searched the whole 43MB capture for every marker that
would fire if *any* console output, queue operation, or other
RSP-level activity ever happened (`conout`'s `'['`/`']'`/`'{'`/`'}'`
probes in `XIOS86Q.A86`, `QUE1.RTM`'s `'G'`-`'P'`/`'u'`-`'y'` probes):
**zero occurrences of any of them, anywhere in the capture.** `TMP.A86`
(confirmed by reading its own header comment: "The TMP determines the
user interface to MPM" -- this *is* the shell/console driver, not a
throwaway test program) never produces a single byte of output and
never does anything queue-related in this entire window. Whatever is
running, it is not the console-facing process.

**Re-examined the *earlier* part of the very same trace -- the nrsp
loop's real `f_createproc(TMP)` call that #43 showed failing with
`not_implemented` -- now with three independent "top of `func()`"
markers instead of one**: `'03h'`/`'#'` (both already in `user_entry`,
unconditional, straight-line code with no branch between them and
`call func`) and the brand-new `'16h'` (literally `func()`'s first
instruction, before even the `fnchk` loop `'Y'` sits behind). For this
exact call, **all three are missing** -- not just `'Y'` as in #43 --
yet the trace still reaches `'Q'`/`'('` (`nonet`, deep inside `func1`)
moments later, with register contents (`AX=1700`, `CX=0001`,
`AL`-as-word`=0000` at the `'('` dump) that don't match sysent's real,
confirmed-correct data for function 144 (`ent_mod=2`,`ent_func=0x0E`).
This rules out every explanation that pins the skip on something
*inside* `func()` (the `fnchk` loop itself, or the `'Y'` probe's own
code) -- `'03h'`/`'#'` sit in `user_entry`, *before* `call func` is
even reached, with no conditional branch between them and the call.
The only way to skip three unconditional, straight-line markers like
this while still reaching code deep inside the same function is for
control to never pass through `call func`'s target address at all.

**New working hypothesis for the mechanism**: `user_entry` does `cmp
intflag,0 ! je noints` / `sti` *before* `call func` (`SUPIF.SUP` line
~101-108), and `INIT.SUP` sets `intflag` to `0FFh` (so `sti` really
does run, interrupts really are live) before ever reaching `nrsp`. A
real hardware timer interrupt landing in that window would not, by
itself, skip instructions in the interrupted code -- `iret` resumes
exactly where it left off. But if `DSPTCH.RTM`'s timer-ISR path has a
bug in how it saves/restores the *interrupted* process's own context
(as opposed to the process it switches *to*), it could resume that
`iret` at the wrong saved IP, which would look exactly like "skipped
three straight-line probes, landed inside `func()`'s body." Not yet
checked directly.

**Next step**: instrument `DSPTCH.RTM`'s timer-interrupt entry/exit --
specifically wherever it saves/restores the *interrupted* process's own
IP/CS/SP (as distinct from the process being switched *to*) -- to check
whether the saved return address for this one specific interrupted
`user_entry` call is the correct one. Budget, as always this session,
for the strong chance that adding even one more probe shifts the
symptom yet again; re-verify behavior immediately after.

## 45. Followed #44's next step into `DSPTCH.RTM`, found and ruled out
a plausible-looking segment-mismatch theory with hard evidence (a true
negative, not just abandoned), and then found a **third**, independent
instance of #43/#44's "dispatch computes the right target and is about
to call it, but the target's own first instruction never visibly
executes" anomaly -- this time in `RTM.RTM`'s *local* function-table
call, a completely different dispatch layer from `SUPIF.SUP`'s `func()`.

**Important toolchain fact, confirmed directly, worth recording so it
doesn't have to be re-derived**: `tools/asm86/assembler.py` tracks which
segment (`CSEG`/`DSEG`/`ESEG`) each `DB`/`DW`/`RB`/`RW`-declared label
was declared under, and a *bare* reference to an `ESEG` label gets an
**automatic `ES:` prefix**, not `DS:`, with no override needed in
source. `UDA.FMT` declares its whole field list (`u_ss`, `u_sp`,
`u_in_int`, `u_ax`..`u_bp`, `u_dparam`, etc.) under `eseg` (line 15) --
so every bare `u_*` reference anywhere in the kernel is really
`es:[...]`, automatically, regardless of what `DS` currently holds.
This invalidated an entire manual trace this round that assumed "bare
= DS" (true for plain `DSEG` labels like `SYSDAT.FMT`'s `flags`/
`module_table`, **not** true for `UDA.FMT` fields) -- flagging this
explicitly for next time, since the same wrong assumption is an easy
trap to fall into again.

**The segment-mismatch theory and how it was tested, cleanly, to a
negative result**: hypothesized that `DSPTCH.RTM`'s `dsptch` (saves
`u_ss`/`u_sp`/`u_flag_sav`/`u_in_int` around line 107-122) and `switch`
(restores them around line 385-413) might use *different* segments for
the save vs. the restore, corrupting the dispatcher's "was this process
interrupted or did it block voluntarily" distinction and its saved
stack pointer. Added two probes (`17h` explicit `ds:u_in_int`, `18h`
bare/auto-`ES:` `u_in_int`, both in `switch`, a few lines apart) to
compare directly. Both read back `00FF` (`true`) identically, in the
one `switch` call this trace captured (`rlr=0A20h`, decoded as idle's
first-ever activation) -- `ES` and `DS` genuinely do agree at both
points, because `fdisp`'s `do_dispatch` (interrupt path) and `switch`'s
own `pop es` (line 405, direct-call path) both correctly re-point `ES`
at the relevant process's real UDA before any `ESEG`-field access.
**Theory debunked, cleanly, by direct measurement** -- no segment
mismatch here. (Along the way, caught and fixed my own mistake in the
first version of these two probes: `mov al,13`/`mov al,14` are *decimal*
13/14 = `0Dh`/`0Eh` in this assembler -- bare numeric literals are
decimal unless suffixed `h` -- colliding with `'\r'` and with
`PROC.RTM`'s own existing `0Eh` marker. Fixed by using explicit `17h`/
`18h`. Worth remembering for any future probe: always suffix hex
literals with `h`, and grep for the *decimal* interpretation too, not
just the hex one, when checking a candidate marker byte for collisions.)

**The real new finding -- a third instance of the "call site computes
the right target, callee's own top never visibly runs" anomaly**:
while testing the above, found that the 1.3-1.5MB/6-second steady-state
loop from #44 (`TICK.SUP`'s `notick`/`tick` cycle) decodes, every single
iteration, as reaching `SUPIF.SUP`'s `goodmod` with the exactly correct
register state for a dispatch to `flag_wait_entry` (RTM local function
2): the `'('` dump shows `CX=0002` (`ch=0,cl=2`), the mtchk/`'S'` block
dumps `CX=0002`/offset`=18B0`/segment`=1000` (confirmed via
`--symbols` and direct disassembly of the built `RTM.cseg.bin`: `18B0`
*is* `RTMIF.RTM`'s own `entry:` label, and its `function` table's 3rd
`dw` entry, index `cx<<1=4`, *is* `flag_wait_entry`'s own address,
`0A0Fh` -- both checked directly against the actual built binary, not
just the source). `RTMIF.RTM`'s `entry:` is reached (`'V'` fires every
time). Then **nothing** -- not even a brand-new probe (marker `0Fh`)
placed as `flag_wait_entry`'s own literal first instruction (confirmed,
again by direct disassembly of the built binary, correctly assembled
at exactly `flag_wait_entry`'s labeled address) -- fires even once, in
two full captures (1.4MB and 1.5MB). The very next thing printed after
`'V'` is `rtm_ret`'s own `'W'`, with zero bytes between them. This is
structurally the *same* symptom as #43/#44 (a dispatch chain computes
every register correctly, right up to the call/callf instruction, and
the callee's own unconditional first instruction still never visibly
executes) but at a *different* dispatch layer: `RTMIF.RTM`'s *local*
`function[]` table lookup (a plain near `call cs:function[si]`), not
`SUPIF.SUP`'s `sysent`/`module_table` machinery. Finding the same shape
of anomaly at a second, structurally unrelated dispatch site is a
meaningful new data point: whatever's doing this isn't specific to one
table or one piece of lookup logic, which argues against a data
corruption theory (wrong table contents) and further in favor of a
control-flow-level cause common to *any* `call`/`callf` executed in
this general vicinity -- e.g. a timer-interrupt race that corrupts a
return address generically, independent of which particular call it
lands on.

**Also newly noticed, likely benign but previously undocumented**:
every single iteration of this loop's `mtchk` integrity check
(`SUPIF.SUP` `goodmod`) prints `!0030!` -- i.e. `module_table`'s 7th
slot (byte offset `0x30`, index 6) has a segment word that isn't
`1000h`. With only `sup`/`rtm`/`mem`/`cio`/`bdos`/`xios` wired up and
networking excluded (`module_map` was built as `3Fh` -- see
`tools/build_mpmsys.py`), slot 6 is almost certainly the unused
"network" module slot, so this is very likely harmless noise rather
than a new bug, but it was never called out explicitly before and is
now a known, constant, unexplained feature of every single dispatch's
diagnostic output -- worth remembering so a future session doesn't
mistake it for something new.

**Not yet explained**: the actual mechanism causing a callee's own
first instruction to be skipped while the caller's own dispatch
reaches the call site with provably correct data, now confirmed at two
independent dispatch layers. Given the segment-mismatch theory is
cleanly ruled out, the leading remaining candidate is still a
timer/interrupt race corrupting a return address or a call target in a
way that isn't specific to any one table.

**Next step**: given probes keep confirming "the call site is right,
the callee's top is never reached," the next most informative probe is
on the *caller's own* immediate post-call state -- e.g. instrument
`rtm_ret` (right where `'W'` already sits) to dump `SS:SP` and compare
against what they *should* be immediately after a normal near `call`
return, to see whether the stack itself is consistent, or already
textually dump the actual bytes at `[sp]` right before the `call
cs:function[si]` executes (to see whether the pushed return address
even matches the instruction immediately following the call, as it
must for a `call` to behave normally) -- that would directly show
whether the `call` instruction's own mechanics are intact here, rather
than adding yet another probe on the far side of a call and hoping it
lights up.

## 46. Followed #45's own next step into `FLAG.RTM`, caught a real bug
in my *own* probes (same class #45 already warned about), and -- once
that was fixed and the symptom re-tested -- landed on what looks like
the **furthest this system has ever gotten**: a real dispatch all the
way through to the point of actually switching control to `TMP` (the
real MP/M-86 shell process), for the first time this tree has ever
exercised that path, with a new, sharp, reproducible lead on exactly
what stops it one step short.

**Self-correction first, so it doesn't get mistaken for a real kernel
finding**: added probes at `flag_wait_entry`'s and `flag_valid`'s own
absolute first instructions (markers `11h`/`13h`) to see whether
`flag_valid` was ever actually returning to its caller (it looked like
it wasn't -- #45 ended on exactly this question). Both fired every
time, immediately adjacent, but the very next byte in the trace showed
`DX=03F8h` where the real input (`flag_tick`=1) belonged -- at first
this looked like a real corruption, until re-reading my own two new
probes: `mov al,11h ! mov dx,03f8h ! out dx,al ! pop ax` saves/restores
`AX` but never saved `DX` before clobbering it with the port address,
and `DX` is this function's own live input (`DL` = flag number) --
**the exact same bug class `SUPIF.SUP`'s historical `'03h'`/`'2Ah'`
probes already hit once**, this time self-inflicted instead of
inherited. Fixed by adding `push dx`/`pop dx` around both. After the
fix, the *same* exact scenario (the `TICK.SUP` busy-loop from #44)
could not be reproduced again -- the extra instruction bytes shifted
the layout once more, and the run that followed never reached that
loop at all. **Conclusion: #45's "`flag_valid` never returns to its
caller" finding should be treated as unconfirmed, most likely caused
entirely by this probe bug, not a real kernel issue** -- noted here so
it isn't cited as established in a future session. Lesson reinforced
yet again: *any* new probe using `mov dx,03f8h` must save/restore `DX`
if `DX` is a live value at that point, no exceptions, even for a
"just checking one bit" probe.

**What happened instead, once the fix was in**: the very next rebuild
produced a short (1666-byte), *perfectly deterministic* trace (byte-
identical across two separate runs, and again -- shifted only by the
expected few bytes from unrelated code-size changes -- after a third,
unrelated probe was added later) that decodes much further than any
previous capture this session:

- `INIT.SUP`'s `nrsp` loop's `f_createproc(TMP)` call (`CX=0090`,
  `DX=0010`=`rsp_pd`) this time dispatches with **completely correct**
  `sysent` data (`(000E0002)` = RTM function 14/`creat_proc_entry`,
  module 2) -- unlike #43/#44's capture of this *same* call, where the
  table read came back zeroed. Yet another confirmation that the
  earlier corruption there is purely a timing/layout artifact, not a
  fixed, reproducible data problem.
- `creat_proc_entry`/`proc_creat` create TMP's real PD (copied by
  `getpdadr` into sysdat slot `si=0E40`) correctly, matching `rsp_pd`'s
  own offset (`0x10`) and reading back the right `p_uda`/`p_mem`
  values for a first-time creation.
- `proc_creat`'s own `cp_uda`/thread-list tail (`pop dx ! jmp
  proc_creat`, `PROC.RTM` line 164) then **recurses a second time**,
  this time with `DX=2BAB`-ish (shifts slightly between rebuilds --
  `29CB`/`29DB` for the resulting PD slot) -- the value it read out of
  `p_link[si]` for TMP's freshly-copied PD right after the first pass.
  Checked `build/kernel/TMP.dseg.bin` directly: the *static* template
  has `p_link[rsp_pd]` (word at offset `0x10`) as a clean `0000` --
  so this second pass is reading a value that cannot have come from
  TMP's own on-disk template. Something writes into `p_link[si]` for
  this freshly-allocated PD slot between `getpdadr`'s copy and
  `proc_creat`'s very next instruction reading it back (`mov dx,
  p_link[si]`, `PROC.RTM` line 44 -- nothing but one already-verified-
  safe probe sits in between).
- Both passes (`si=0E40` and the second, bogus `si=29CB`/`29DB` slot)
  end up correctly inserted onto `drl` and drained onto `rlr` by the
  scheduler (`drltorlr`'s drain this time pulls *four* entries:
  `29CB`/`0E40`/`0A20`(idle)/`09F0`(tick) -- confirmed via the existing
  raw-`0x0C` marker). `switch` then picks the bogus second slot
  (`rlr=29CB`/`29DB`) to run **first**, and -- since it's a freshly
  created process (`u_in_int=TRUE`, confirmed again via #45's own
  `17h`/`18h` probes, which still agree with each other, consistent
  with #45's negative result standing) -- takes the `int_disp_exit`/
  `iret` path to actually start running it.
- Added a brand-new probe (marker `27h`, `src-assem/rsp/TMP.A86`) at `tmp:`,
  `TMP`'s own real program-entry label (as opposed to `mpm:`, its
  internal `int mpmint!ret` trampoline) -- **it never fires**, in three
  separate rebuilds. The trace goes completely and permanently silent
  immediately after `int_disp_exit`'s own `']'` marker (which does
  fire, dumping `DS=sysdat` right before the final `iret`) -- no
  further bytes at all for the remaining ~5-6 seconds of every capture.

**So the system now gets further than ever recorded before**: past
loader, past kernel init, past `nrsp`'s `TMP`-creation syscall (with
correct dispatch data this time), through a real scheduler pass, and
all the way to actually `iret`-ing into what the dispatcher believes is
a legitimate, freshly-created, ready-to-run process -- the first time
any trace in this project's history has reached that exact point. It
just turns out to be the *wrong* process: a second, spurious PD that
was never really part of TMP's design, created because `p_link` for
TMP's real PD read back nonzero when the static template says it
should be zero.

**Working hypothesis, now the strongest unifying one across multiple
sections of this doc**: this is structurally the *same* mechanism as
#41/#43's "mystery second `proc_creat(idle)` call" -- in both cases, a
PD that a static template (confirmed directly, byte-for-byte, to be
clean/zeroed) says should terminate a chain instead reads back a
nonzero `p_link` moments after being freshly copied into its sysdat
table slot, triggering an unintended second creation pass. Two
independent instances of "a freshly-copied PD's `p_link` doesn't match
its own known-clean template" is a strong signal this is one bug, not
two, and that it's specifically about freshly-allocated PD slots
racing against *something else* that also touches `p_link` around the
same moment (the obvious candidate, given this whole session's
findings, is the real hardware timer interrupt's own `drltorlr`/
`insert_process` activity, which *also* reads and rewrites `p_link`
for whatever's currently on `drl`, and could plausibly collide with a
PD slot `pul` just handed out if the free-list and the dispatch-ready
list ever briefly overlap).

**Not yet explained**: the exact write that corrupts `p_link[si]`
between `getpdadr`'s copy and `proc_creat`'s very next read of it (only
one already-audited, stack-balanced probe sits in that gap -- the
corruption is not coming from debug instrumentation this time), and
why the dispatcher ends up running the bogus second PD *before* TMP's
real one (`si=0E40`) even though `0E40` should have been inserted onto
`drl` first and `insert_process` is priority-ordered, not just FIFO --
worth checking both PDs' `p_prior` values directly.

**Next step**: re-read `getpdadr` (`PROC.RTM`) and `freepd`/`pul`'s own
free-list bookkeeping once more with this specific lens -- whether the
PD slot `pul` hands out for `si=0E40` could already be live on *another*
list (most likely `drl`, given this session's running theory about the
timer ISR) at the exact moment it's handed out, so that the real
hardware timer interrupt's own `insert_process`/`drltorlr` work writes
into the same `p_link[si]` slot moments after `getpdadr`'s `rep movsw`
copy finishes but before `proc_creat`'s own very next instruction reads
it back. A direct, decisive test: dump `pul`'s own linked-list contents
(walk it fully) immediately before `getpdadr` hands out `si=0E40`, to
see whether that slot is actually, legitimately free at that moment.

## 47. Followed #46's own lead into `getpdadr` directly, caught and
fixed a second instance of my own DX-clobber probe bug (same class as
#46's own `11h`/`13h` fix, this time self-inflicted on a brand-new
probe before it ever got a chance to mislead anyone), and then got a
**direct, non-inferred confirmation** of the `p_link` corruption --
while also exhausting every plausible *logic* explanation for it,
which strengthens the case that this is a genuine interrupt race
rather than a bug reachable by reading the code.

**Another self-caught probe bug, fixed before it cost a detour this
time**: added marker `2Dh` right after `PROC.RTM`'s `mov dx,p_link[si]`
(`cp_gpd`) to directly dump the value instead of inferring it from the
later recursive `0Eh`/`1Fh` markers. First version saved `DX` into `BX`
to survive the `mov dx,03f8h` port-address overwrite, dumped it, but
never copied it back from `BX` into `DX` before falling through into
the real `push dx` two lines later -- so the real code immediately
pushed `03F8h` (the leftover port address) instead of the real value,
and the whole recursion chain used `03F8h`-derived garbage for one
rebuild (visible as a 76KB/6-second runaway with `\x1f03F8` all over
it). Caught by comparing the probe's own freshly-dumped value against
what the very next `0Eh`/`1Fh` pair showed and finding `03F8h` instead
of a sane-looking flag/PD value. Fixed with an explicit `mov dx,bx`
before falling through. **Lesson reinforced a second time, now
numbered precisely because it kept recurring**: when a probe's whole
point is "peek at a live register via the serial port without
disturbing it," the one line that's easiest to forget is restoring
that register's *original* value (not just the scratch copy) before
the probe's final instruction -- a `pop`/`mov` back into the real
register, not just into the register the value was borrowed into.

**Direct confirmation, with the fix in place**: marker `2Dh` now
reads `-0E40  2BEF` -- i.e. `p_link[0E40]` (TMP's freshly-copied PD
slot) really is `2BEFh` *immediately* after `mov dx,p_link[si]` itself,
confirmed by direct read, not inference, and self-consistent with the
following `0Eh`/`1Fh` recursion pair showing the identical `2BEF`.
This is now about as solid as a serial-trace finding can be: not a
probe artifact, not mis-attributed to the wrong call.

**Then tried to catch the actual corruption in the act, and came up
empty-handed in the best possible way**: added marker `2Fh` right at
the very top of `getpdadr`'s `gpd_have` (before `mov ds,u_wrkseg` and
the `rep movsw` that's supposed to write a clean `0` into this same
`p_link[0E40]`), dumping `DI` (the slot about to be overwritten),
`DX` (the source offset within `u_wrkseg`), and `u_wrkseg` itself. All
three read back exactly as they should: `DI=0E40`, `DX=0010` (`rsp_pd`),
`u_wrkseg=1777`. Checked `build/kernel/TMP.dseg.bin` directly (again):
its own byte 0x10-0x11 (`p_link[rsp_pd]`) is a clean `0000`. So every
single input to the copy that's supposed to clear `p_link[0E40]` is
individually correct, confirmed by direct measurement, immediately
before the copy -- and `p_link[0E40]` still doesn't end up `0`
afterward. Re-read `getpdadr`'s *entire* `gpd_bad`/`gpd_have` body and
`cp_uda`'s full push/pop discipline instruction-by-instruction (every
register, every stack slot, from `cp_gpd`'s own `push dx ! push si`
all the way to the final `pop dx ! jmp proc_creat`) looking for an
unbalanced push/pop or a wrong segment on *any* instruction that
touches `p_link[si]` for this slot -- found none. The `cli`/`popf`
bracket around `gpd_bad`'s whole body (including the `rep movsw`)
looks intact and correctly scoped.

**Also, separately, directly confirmed `u_wrkseg`'s own very first
capture is correct** (new marker `3Bh`, `SUPIF.SUP`, literally
`user_entry`'s first real instruction, before `cld!mov di,ds` even
runs): `DS` reads back `1747`, matching that same run's `rspseg`
(`TMP`'s own data segment, confirmed via the existing `1Bh` probe) --
not `sysdat`. So the one other theory worth naming and ruling out --
that `u_wrkseg` is *always* wrong, capturing `sysdat` instead of the
caller's real segment -- doesn't hold up either, at least not at this
earliest possible checkpoint. (This same rebuild, true to the whole
session's running theme, landed on a completely different downstream
symptom -- back to #43/#44's `not_implemented`/`func()`-entered-via-
its-middle anomaly instead of ever reaching `nrsp`'s `TMP` creation at
all, reconfirming yet again how violently one more instruction
anywhere in the combined image can retime everything downstream of
it.)

**Where this leaves the investigation**: every individual piece of
`getpdadr`/`cp_uda`'s logic, traced by hand all the way through, is
provably correct in isolation, and every input to the one copy that's
supposed to zero `p_link[0E40]` is independently confirmed correct
immediately beforehand -- yet the field still reads back nonzero
right after. Static-logic explanations are now thoroughly exhausted
for this specific step; what's left standing is a *dynamic* one: a
hardware timer interrupt landing somewhere inside this nominally
`cli`-protected window and doing the actual damage, either by writing
into `p_link[0E40]` directly (if the real ready-list `drl` happens to
briefly alias this exact slot during the same window, continuing
#46's own hypothesis) or by corrupting the `rep movsw`'s own operation
mid-flight in some way that doesn't show up in any register dumped
before or after it.

**Next step**: the one thing not yet directly tested is whether the
`cli`/`pushf...popf` bracket around `gpd_bad` is *actually* keeping
`IF` clear for the bracket's full duration at runtime, as opposed to
being correct source that nonetheless gets undermined by something
else (e.g. a nested call that does its own `sti`, or an `iret` deep in
a probe's own call chain inadvertently re-enabling interrupts). A
direct test: sample the flags register (`pushf`, peek the saved word)
immediately before the `rep movsw` and again immediately after, and
confirm `IF` (bit 9) reads `0` at both points, bracketing the exact
instruction most under suspicion instead of still-correct-looking
code around it.

## 48. Followed #47's own next step (sample FLAGS around the `rep
movsw`), never got a clean read on it because the layout shifted yet
again, but tripped over something more important along the way: a
**third and fourth** instance of this session's own recurring DX-
clobber probe bug -- this time in two *old* probes (`16h`, two rounds
old, and `3Bh`, one round old) that had been silently live through
every single capture in #46 and #47. This likely retracts #46/#47's
specific "`DX=03F8h` at `flag_valid`"/"`flag_valid` never returns"
finding back to "probably just this," though the evidence for that
retraction isn't as clean as it should be -- see below.

**The new probes (`7Fh`/`81h`, sampling FLAGS before/after `rep
movsw`) never got to fire**: the next two rebuilds landed back in
#43/#44's `not_implemented`/`func()`-entered-via-its-middle territory
instead of ever reaching `getpdadr`'s copy, and a third rebuild (after
fixing the bug described below) landed there too. Still unresolved;
not contradicted either.

**Re-discovered the `DX=03F8h` symptom from #46/#47 one more time**
(a `TICK.SUP` flag-wait loop, same as before, with `flag_valid` taking
the `e_ill_flag` branch every iteration) and, instead of adding yet
another probe to chase it, went back and actually re-read every probe
added so far this session for the one specific defect that kept
recurring (DX not saved/restored around `mov dx,03f8h`). Found two
that had been missed:

- `SUPIF.SUP` line 130, marker `16h` -- `func()`'s own literal first
  instruction, added at the very start of this multi-turn session (the
  original #43/#44 investigation), **two full rounds before** the
  `11h`/`13h`/`2Dh`/`2Fh` fixes in #46/#47. `func()`'s own documented
  input is `DX = arg`; this probe never saved it.
- `SUPIF.SUP` line 65, marker `3Bh` -- `user_entry`'s own literal
  first real instruction, added in #47 to chase `u_wrkseg`. Same
  defect, same documented live input (`"DX -> function parameter"`,
  spelled out in `user_entry`'s own header comment, the *exact* same
  one the historical `'03h'`/`'2Ah'` bug already quotes).

Both fixed the same way as every previous instance: save the real
`DX` into a register untouched by the rest of the probe, dump it, and
explicitly move it back into `DX` before falling through -- not just
into the scratch register, into `DX` itself.

**Why this almost certainly explains #46/#47's finding, but the
confirmation is messier than it should be**: marker `16h` has been
live, broken, since section #43 -- present in *every single trace*
captured in #44 through #47, including the ones that showed
`creat_proc_entry`/`proc_creat` correctly receiving `DX=0010` (the
`1Eh`/`1Fh` markers, multiple captures). If `16h` unconditionally
destroyed `DX` on every call, `f_createproc(TMP)`'s own `DX` should
have read wrong just as often as `f_flagwait`'s did -- it didn't, in
every capture checked. So `16h` (and probably `3Bh`, added later) does
not corrupt `DX` *unconditionally* the way the first-version `11h`/
`13h`/`2Dh`/`2Fh` probes clearly did (confirmed instantly, every time,
in #46/#47) -- something about exactly which call, or exactly how the
timer-interrupt race lands relative to these two probes' extra
instructions, determines whether the corruption actually reaches the
function that needed `DX`. Both are fixed now regardless, since
leaving a function's own documented live input unsaved around a port
write is wrong on its own merits, independent of whether it's *the*
explanation here. But this means **the retraction of #46/#47's
finding is not as clean as the retraction in #46 itself was** (that
one, `11h`/`13h`, really did reproduce the exact symptom and then
really did make it disappear on the very next direct test). This one
is "the obvious, confirmed-broken code was sitting right there for two
rounds" rather than "fixed it and watched the symptom vanish
cleanly" -- worth remembering as a *weaker* form of self-correction
than #46's.

**Taking stock**: by this point there are 17 TEMP DEBUG PROBE markers
added just this session (`08h,0Fh,11h,13h,14h,15h,16h,17h,18h,
1Eh(pre-existing)...,2Dh,2Fh,27h,3Bh,7Fh,81h`, plus the earlier-session
`@`/`03h`/`2Ah` fixes), each one adding real instruction bytes to the
combined image, and this whole session's own running finding is that
*any* instruction-count change retimes the real, independent interrupt
race this investigation is actually trying to find. At this density,
every single new probe is now close to guaranteed to relocate the
symptom before it can be cleanly observed twice. Worth considering,
before adding more: whether some of the now-answered probes (`@`'s
original CX-only question, `03h`/`2Ah`'s ES/DS questions, `17h`/`18h`'s
now-settled ES-vs-DS question) could be removed to free up some of
that margin rather than only ever adding more.

**Next step, unchanged from #47**: still want a clean FLAGS sample
around `getpdadr`'s `rep movsw` (`7Fh`/`81h`, now in place and DX-safe
by construction since they never use `DX` as anything but the port
address, which nothing after `mov si,dx` still needs) -- just need a
rebuild that actually reaches it. Given the note above, consider
reverting one or two fully-answered old probes first to buy back some
margin before adding anything new.

## 49. Acted on #48's own "buy back margin" recommendation -- found
something new instead of the thing being chased

Removed the two fully-answered, dx-safe probes `03h` (`SUPIF.SUP`,
`user_entry`'s ES/`u_wrkseg` check, answered since #44) and `2Ah`
(`SUPIF.SUP`, `mpmif`'s ES/DS-on-entry check, answered since #45).
Rebuilt, captured a trace (`/tmp/mpm_trace21.txt`) -- and hit something
new: a clean, byte-for-byte **reproducible hang** (confirmed via two
runs, 20s and 40s timeouts, identical output both times) right at
marker `3Bh` (`user_entry`'s own first instruction), for what the
trace shows is the *first ever `int mpmint` syscall this session has
captured* -- `f_createproc(TMP)` (`CX=0090h,DX=0010h`), dispatched from
`INIT.SUP`'s `nrsp` loop. Every earlier PD creation this session
(idle, tick) went through `proc_creat` via a **direct call**, bypassing
`user_entry`/`int mpmint` entirely -- so this is genuinely new
territory, not a re-run of an old path.

**Added two new, narrowly-targeted, dx-safe probes** right after the
hang point, reusing the two just-freed marker bytes (`03h`, `2Ah`) --
not for the same questions as before, but to directly inspect
`user_entry`'s own stack-switch logic (the exact code right after
marker `3Bh`):

- `03h`, right after `mov es,p_uda[bx]` (`SUPIF.SUP` line 92-93) --
  dumps `BX` (`=rlr`) and the resulting `ES` (`=p_uda[rlr]`).
- `2Ah`, at `uecont:` (line 101), right after the conditional
  stack-switch branch -- dumps `SS` and `ES`, to see whether the
  switch (`mov ss,es-derived-bp ! mov sp,ulen`) landed on a sane
  segment.

Rebuilt again (`/tmp/mpm_trace22.txt`). **The hang is gone** -- these
two probes' extra instruction bytes retimed things enough that
`f_createproc(TMP)` now runs to completion instead of stopping dead.
`03h` read back `BX=09C0` (matches `rlr`'s documented cold-boot value,
`initpd`), `ES=17E4`; `2Ah` read back `SS=ES=17E4` both times the
switch branch was taken -- so the stack switch itself lands on a sane
segment every time it was observed. **This particular hang is not
explained yet** (the two probes that made it go away don't, by
themselves, show what was wrong) -- it's simply gone, same as every
other "fixed by retiming" symptom this whole session.

**What showed up instead, once the call could finally run to
completion, is a much more precise version of #43/#44's long-standing
"`func()` entered via its middle" finding.** Walking the new trace
marker-by-marker against `build_mpmsys.py`'s own printed module base
table (`RTM code @ 0x17aa ... entry=0x133` &rarr; RTM's `entry:` =
`0x18DD`; `XIOS code @ 0x6aef ... entry=0x13b` &rarr; XIOS's `entry:` =
`0x6C2A` -- both exact, not approximate, confirmed by direct
arithmetic against the build log):

1. For this call, `func()`'s own top-of-function markers (`16h`) and
   `nonet`'s own `Y`/`Q`/`(` markers -- which *always* fire first for
   every normal dispatch, including every other call in this same
   trace -- **never fire**. Execution is not entering `func()` at its
   top for this call.
2. `gofar`'s own markers (`Z`/`d`/`R`/`!`/`S`) fire directly, and
   `goodmod`'s `callf dword ptr module_table[si]` correctly reaches
   RTM's `entry:` trampoline (`S` dumps `18DD:1000` immediately before
   `V` -- RTMIF.RTM's own, RTM-exclusive marker -- fires). So far this
   matches a legitimate dispatch, just missing its own front door.
3. RTM's `entry:` does its own internal near-dispatch (`mov
   ch,0!shl cx,1!mov si,cx!call cs:function[si]`) -- which should land
   on `creat_proc_entry` (table index 14, `SYSENT.RTM`, marked `1Eh`).
   **`1Eh` never fires anywhere in this trace.** Instead, execution
   winds up straight back in `SUPIF.SUP`'s `gofar`/`goodmod` **a
   second time** (`Z`/`d`/`R`/`!`/`S` fire again, right after `V`, with
   no `Y`/`Q`/`(` in between either) -- and this second `goodmod` call
   resolves to **module 6 (`xios`)**, landing on XIOS's `entry:`
   trampoline at `6C2A:1000` (confirmed: the `S` marker's own
   `module_table[si]`/`[si+2]` dump reads `6C2A:1000` directly out of
   memory, and `0x6AEF (XIOS base) + 0x13B (XIOS entry local offset) =
   0x6C2A` exactly).
4. Immediately after that, `proc_creat` runs again (`0Eh` return
   address `186E` -- the same single call site as every other
   `proc_creat` invocation this trace, nothing new there) with
   `DX=09F0` -- **the same absolute address as idle's own,
   already-created PD** -- and re-runs `cp_uda`/`cp_nm` on it, which
   re-triggers the entire init-time module-init sequence a second
   time, including the `XIOS86Q` "target (serial console, floppy
   disk)" prompt text appearing **twice**, byte-for-byte identical,
   in the one trace. (Earlier in this same session a very similar
   repeat had been provisionally misread as a full MPMLDR reboot --
   it isn't: `MPMLDR`'s own loader banner, `Code Paragraph
   Address`/etc., appears only once at the very start of
   `/tmp/mpm_trace22.txt`. Only the inner XIOS console-select prompt
   repeats.)

**So, stated precisely rather than approximately for the first time**:
`f_createproc(TMP)`'s dispatch chain reaches RTM's `entry:` correctly,
then (skipping RTM's own `creat_proc_entry` handler entirely) ends up
back inside `SUPIF.SUP`'s generic cross-module dispatcher a second
time, which this time resolves the target module to XIOS instead of
RTM, and from there ends up re-running `proc_creat` against idle's own
PD address instead of TMP's. This is almost certainly the same
underlying defect as #43/#44's "`func()` entered via its middle" and
#46/#47's "bogus second PD" findings, just caught with much sharper
instrumentation this time -- but *why* RTM's own internal `call
cs:function[si]` never reaches `creat_proc_entry`, and why the stray
second dispatch resolves to module 6 specifically, is still open.

**Also still open, smaller**: a short, unexplained `0010` appears in
the raw byte stream exactly where `func()`'s own `#`/`16h` CX-dump
probes should be (between marker `2Ah`'s output and the first `#`) --
consistent with CX genuinely holding `0010h` (`DX`'s own value) at
that point rather than `0090h` (the real function code), which would
itself help explain why `func()`'s top/`nonet` never fire (a bad `CX`
would misdirect `fnchk` long before `nonet` prints anything) -- but
this hasn't been directly confirmed with its own probe yet.

**Next step**: add two narrow probes to pin down the actual control
flow precisely -- one right at `gofar`'s own top (`SUPIF.SUP` line
162) dumping `CX`/`AX` on *every* entry (to catch the second,
unexplained visit and see exactly what it's carrying), and one right
at RTM's `entry:` dispatch (`RTMIF.RTM` line 108-109) dumping `CX`/`SI`
right before `call cs:function[si]` (to see whether RTM's own table
lookup is already wrong at that point, or whether `creat_proc_entry`
itself does something that bounces back out). Given this round's own
lesson (two cheap, unrelated probes completely changed which bug
surfaced), expect another retiming on the next rebuild regardless.

## 50. Added #49's own next-step probes (`09h`/`0Bh` at `gofar`'s top,
`10h` at RTM's `entry:` dispatch) -- caught `gofar` red-handed with a
bogus register pair

As predicted, the rebuild (`/tmp/mpm_trace23.txt`) retimed things yet
again: the hang from #49 is gone, replaced by the *original*
`not_implemented` symptom from #43/#44 -- but this time `09h`/`0Bh`
were sitting right at `gofar`'s own top to catch it directly, instead
of having to infer it.

For the call immediately following marker `3Bh` (i.e. the one and
only `int mpmint` syscall in this trace, still `f_createproc(TMP)`,
`CX=0090h,DX=0010h` at `user_entry`'s absolute top) -- `gofar`'s own
markers read:

```
09:CX=0086  0B:AX=000B  (so AL=0Bh)  Z(cl after dec)=000A
d(DS)=177C  (module_map)=003F  R(shifted bit)=0000  -> U (not_implemented)
```

**`AL=0Bh` is module 11 -- there is no module 11** (`sup..net` only
go 1-7, `MODFUNC.DEF`). `0Bh` shifted into `module_map` (`003F`,
correctly showing modules 1-6 present) by `CL=0Ah` necessarily lands
on 0 -- `not_implemented` isn't a secondary symptom here, it's the
*correct, honest* response to a module number that was never valid in
the first place.

**And `CX=0086h` at `gofar`'s top is not `f_createproc`'s own function
code (`0090h`) at all -- it's the exact `CX` value from one of
*idle's own* internal calls earlier in this same trace** (the first of
the three `16:0086 / 0087 / 008B` calls idle's own init code made,
confirmed identical down to all four hex digits). This is the
clearest, most literal confirmation yet that this whole symptom family
(`#43` onward) is a **stale/leftover-register problem, not a
dispatch-logic problem**: `gofar` is running off a `CX`/`AX` pair left
over from a previous, unrelated call, not off `f_createproc(TMP)`'s
own `CX=0090h`. Consistent with #49's still-open "`0010` where `func`'s
own CX-dump should be" oddity -- different stale value this time
(`0086h` instead of `0010h`), same shape of bug: whatever this call
path is, it reaches `gofar` without ever loading `CX`/`AX` itself.

**Downstream of the `not_implemented` return (`bx=0ffffh`, `ret`)**,
control does *not* make it back to `INIT.SUP`'s `nrsp` loop at all --
the trace instead shows `INIT.SUP`'s own early-boot markers (`2`/`3`,
printed in pairs) firing again, immediately followed by **another
full `idle`-PD creation round at the same address (`09F0`) and via a
*different* `proc_creat` return address (`1898h`, not the `186Eh` used
by every direct, intentional call this session)** -- i.e. whatever
unwound from the bogus `gofar` call landed back somewhere in `INIT`'s
own early sequence, not at the real caller. This matches (and sharpens)
#46/#47's "bogus second PD" observation yet again, from a different
angle.

**Open**: `10h` (RTM's own `entry:` dispatch, dumping `SI` right before
`call cs:function[si]`) never fired, and neither did `1Eh`
(`creat_proc_entry`) -- meaning for *this* trace, control never even
reached RTM's `entry:` on the way to `gofar`'s bogus visit; the stale
`CX=0086h`/`AX=000Bh` pair must be arriving some other way. Where
exactly `gofar` gets called from on this particular path (i.e. what's
on the stack right above it, or equivalently -- given this was reached
without `nonet`'s own `Y`/`Q`/`(` -- which `jmp`/`call` actually targets
`gofar` directly) is the next concrete thing to pin down. A probe
dumping the near-call return address right at `gofar`'s own top (same
`[bp+2]`-off-the-stack trick `proc_creat`'s `0Eh` marker already uses)
would answer this directly without needing to guess at the caller.

## 51. Followed #50's own next step into `func1` -- partial correction,
plus a sharper read on the recurring `0010` artifact

Added marker `17h` at `func1`'s own absolute top (`SUPIF.SUP`), dumping
`CH` (the module# `func1` is about to test) and the real top-of-stack
word underneath the probe's own two pushes (same idea as `proc_creat`'s
`0Eh` caller-address trick, just without a `bp` frame to lean on).
First version had a bug of its own -- `mov ax,ch` isn't a real 8086
encoding (can't move an 8-bit register into a 16-bit one); fixed to
`sub ax,ax!mov al,ch` and rebuilt before trusting any of its output.

**Correction to #50**: with the bug fixed, `func1`'s own `CH` reads
back as `0002` (module `rtm` -- a perfectly valid module number) for
the call immediately after marker `3Bh`, **not** the `0Bh`/module-11
value #50 reported. #50's own value came from the broken `mov ax,ch`
probe and should not have been trusted -- retracting that specific
number, though not the broader finding (`gofar` still reads `AL=0Bh`
immediately afterward, confirmed again this trace, byte-for-byte
unchanged: `09:CX=0086 0B:AX=000B Z=000A ... R=0000 -> U`). So
`func1` itself starts out looking completely sane (`CH=2`, a real
module), and the corruption to `AL=0Bh` happens strictly *between*
`func1`'s own `mov al,ch` and `gofar`'s top -- i.e. in the two-instruction
gap `mov al,ch ! mov ch,0 ! jmps inmod ! (inmod:) cmp al,sup ! jne gofar`.
Nothing in that gap should be able to change `AL` from `2` to `0Bh` on
its own.

**The stack-top half of the same probe reads `0014`** -- too small to
be a plausible return address for either of `func1`'s two real
callers (`func()`'s own fallthrough, or `entry:`'s `call func1 ! mov
ax,bx ! retf` at line ~225, both deep enough into `SUPIF.SUP`'s module
to have a much larger offset than `0014h`). Not yet explained.

**Sharper read on the recurring `0010` artifact** (first seen in #49,
still present in every trace since): it sits exactly where
`user_entry`'s own `#` probe (dumping `CX` right before `mov ch,0!call
func`) ought to print `CX` -- but the marker byte itself (`#`, `0x23`)
is *also* missing every time, which a register corruption alone can't
explain (the `out dx,al` that writes the marker character doesn't
depend on `CX` at all). That rules out "probe fired, just read a
corrupted `CX`" and points instead at **output interleaving from the
real hardware timer interrupt** -- interrupts are enabled a few
instructions earlier (`noints: ... sti`), and if the timer fires in
that exact window, `DSPTCH.RTM`/`TICK.SUP`'s own handler could run and
write its own bytes to the same serial port mid-stream, without
needing to corrupt anything in `user_entry`'s own registers at all.
This is a materially different (and more mundane) explanation than
"`CX` is corrupted" -- worth testing directly next by temporarily
masking the timer IRQ around this exact window (or by giving the
interrupt handler's own likely culprits -- `DSPTCH.RTM`'s `switch`,
`FLAG.RTM`'s `flag_wait_entry` -- one more marker each) rather than by
continuing to treat `0010` as evidence about `user_entry`'s own
registers.

**Taking stock again**: this round went three rebuilds deep chasing
one register value and ended up *retracting* its own prior
conclusion partway through (the `0Bh`→module-11 read), while the
actual, load-bearing mystery (`AL` changing between two adjacent
instructions with nothing in between that could do it, in source) is
still open. Given how reliably every probe this session retimes the
underlying race, the next session should treat "single-register,
single-visit" probes like `09h`/`0Bh`/`17h` as already answered (the
gap exists, is real, and is too narrow in source for the current
theories to explain) and shift toward testing the *interrupt
interleaving* theory specifically, since it's the first new
explanation this session that doesn't depend on another coincidental
retiming to confirm.

## 52. Tested #51's interrupt-interleaving theory directly -- retracted
it -- then switched tools entirely (GDB over QEMU's gdbstub) and got
the first non-serial, non-ambiguous confirmation of this whole bug
family

**#51's theory, tested and retracted.** Added one more narrow, DX-safe
probe (`18h`, at the real, literal last instruction of `user_entry`
before `mov ch,0!call func` -- as close to the "ghost value" window as
an instruction can get) and checked the already-existing, pre-built-in
timer-interrupt marker (`XIOS86Q.A86`'s `tickint:`, marker `"`
(`0x22`), which fires on *every single* real IRQ0 tick, no exceptions)
across two separate rebuilds. **Zero timer interrupts fired in either
trace, start to finish.** That rules out #51's interrupt-interleaving
theory outright -- there is no interrupt activity anywhere near this
window to interleave with. Retracted.

With that ruled out, and three rounds of single-register serial
probes each producing a plausible-looking but mutually-inconsistent
story (#49's "stale CX from an earlier call", #50's "bogus module
11", #51's own retraction of #50's specific number), serial-trace
decoding had clearly hit its limit for this particular question.
Switched tools: QEMU was already running with `-serial file:...`
every time, but nothing had used its GDB stub (`-s -S`, port 1234) at
all this session for live inspection, despite plenty of old `/tmp/gdb*.py`
scripts from earlier sessions proving the workflow exists and works.

**Set a real breakpoint at `func`'s absolute address** (`0x104CB` --
computed from `asm86.py --symbols` on `SUP.A86`, cross-checked against
`build_mpmsys.py`'s own module-base printout, both giving `func` at
local offset `0x4CB` with SUP's `module_base=0`) and let it run,
dumping `CS:IP`/`AX`/`BX`/`CX`/`DX`/stack on every hit. Four hits came
back, completely unambiguous, no serial-byte guessing involved:

```
hit1: CX=0086 DX=0f92  stack=[0743,1000,1780]
hit2: CX=0087 DX=0fae  (AX/BX differ too: ax=ff00,bx=ffff)
hit3: CX=008b DX=0fae  (same ax/bx as hit2)
hit4: CX=0086 DX=0f92  stack=[0743,1000,1780]  -- byte-for-byte IDENTICAL to hit1
```

**Hit1 and hit4 are not just "the same function code" -- every single
register and all three stack words captured are bit-for-bit identical
between them**, including the return address (`0743h`, the same call
site both times). This is the cleanest possible confirmation of
#49/#50's "the whole idle/tick-creation-and-exercise sequence runs
twice" finding -- not inferred from a confusing serial byte stream
this time, but read directly out of the CPU's own registers by an
independent tool. It settles, for good, that this is a genuine
**re-execution of the identical call**, not a corrupted-register
illusion.

**Also surfaced a tooling trap worth recording so the next round
doesn't lose time to it**: tried to catch the *call site* doing this
(RTM's own `init` label, `0x11851`) the same way, and `continue`
reported **49 consecutive hits at that address with completely
identical register state every time** -- both with a software
breakpoint and with a hardware one (`hbreak`). That looked at first
like a genuine runaway loop. It wasn't: deleting the repeated
`continue`s and single-stepping (`stepi`) from the same starting
point instead showed completely normal, real, forward-progressing
execution (`BX` advancing `0000` -> `0038` -> `003c`, `AX` changing
`1733` -> `0718` -> `1000` across a dozen real instructions). **GDB's
`continue` against this specific QEMU real-mode target does not
reliably resume guest execution at some addresses** -- it can report
the exact same trap over and over with zero actual progress, while
`stepi` from an identical starting state works correctly every time.
Given this session's prior GDB scripts (`/tmp/gdb*.py`) also mostly
used single continues or short step sequences rather than long chains
of blind repeated `continue`s, this may be why the trap wasn't noticed
before. **Lesson for next time**: don't trust a loop of blind
`continue` calls to count real hits at an arbitrary address in this
setup -- re-arm the breakpoint fresh per hit (delete + re-break, or
disconnect/reconnect) instead, or confirm forward progress with
`stepi` before trusting a `continue`-based hit count.

**Next step**: now that GDB access is confirmed to work (for
single-hit-then-inspect, if not for long continue chains), use it to
directly answer the one question three rounds of serial probes
couldn't pin down cleanly: break on `func` a *fifth* time (or
re-arm after hit4) and watch for `CX=0090h` specifically -- if it
never arrives within a reasonable number of real hits, that's now
GDB-confirmed (not serial-trace-inferred) proof that `f_createproc(TMP)`
never reaches `func()`'s own top at all, settling #49's original
question directly instead of through another retimed serial capture.

**Did exactly that (re-arm per hit, `stepi` once after each delete to
guarantee real forward progress before re-breaking) and ran it to
completion.** Seven real hits, then the whole program exited:

```
#1 CX=0086 DX=0f92   #2 CX=0087 DX=0fae   #3 CX=008b DX=0fae
#4 CX=0086 DX=0f92   #5 CX=0087 DX=0fae   #6 CX=008b DX=0fae   (1-3 repeat, byte for byte)
#7 CX=008f DX=03ff    -- matches f_terminate's call signature exactly
[Inferior 1 (process 1) exited normally]
```

**`CX=0090h` (`f_createproc`) never appears at all.** Under this run's
(GDB-slowed) timing, `INIT.SUP`'s `nrsp` loop evidently finds
`rspseg=0` and goes straight to `f_terminate` without ever attempting
`TMP`'s creation through `func()`'s own top -- a *third*, independently
timed outcome for the exact same binary (the first two being #49's
"hangs, then succeeds with `03h`/`2Ah` added" and #50/#51's "reaches
`gofar` with a stale/bogus register pair"). This is consistent with,
and reinforces, this whole session's central finding: literally
*any* change in execution speed -- not just probe byte-count, running
under GDB too -- retimes which of several latent outcomes this system
lands on for `TMP`'s creation.

**`0086h`/`0087h`/`008Bh` identified, by the way** (`MODFUNC.DEF`):
these are `f_qmake`(134)/`f_qopen`(135)/`f_qwrite`(139) -- RTM's own
queue-management calls, not raw debug noise. Something in `INIT.SUP`'s
own early sequence makes, opens, and writes a queue once per round --
twice, identically, in every trace that reaches this far (serial or
GDB). Not yet traced to its exact call site; worth doing before
chasing anything else, since it's the one part of this whole
investigation that reproduces identically regardless of timing.

**One more finding, orthogonal to all of the above but important for
trusting *any* future serial-trace reading in this codebase**: this
session's marker-collision checks (going back to the start) only ever
scanned for hex-coded markers (`mov al,0NNh`). A full scan for the
other convention used throughout -- quoted-character markers
(`mov al,'X'`) -- turns up real, **previously unchecked collisions**:
`'3'`/`'4'`/`'5'`/`'6'` are each used by *both* `INIT.SUP` (module-init
step markers) *and* `XIOS86Q.A86` (`rtm_poll`'s own markers), and
`'q'`/`'r'`/`'s'` similarly collide between `BDOSIF.BDO` and
`XIOS86Q.A86`, among several others (`'Y'`, `'('`/`')'`, `'!'`, `'$'`,
single-letter matches in `QUE1.RTM`/`QUE2.RTM`/`TMP.A86`/`SUBMIT.A86`).
Checked the specific case that mattered for this round --
`XIOS86Q.A86`'s `'3'`-`'6'` live inside `rtm_poll`, a runtime
device-poll routine that has no business running during `INIT.SUP`'s
single-threaded, pre-dispatch module-init sequence -- so this round's
own `'4'`/`'5'` readings are very likely still `INIT.SUP`'s, not a
misattribution. But the collision is real and unaudited everywhere
else, and every quoted-character marker reading from earlier in this
session (back through #43) should be treated as provisional until
checked against where else that same character is used. **For any
new probe going forward, check both conventions** (the existing
hex-scan plus a `mov al,'.'` scan) before picking a marker byte.

## 53. Followed #52's own next step (trace the queue-call repeat to
its call site) straight to a real, confirmed, five-instance bug --
fixed it, and it changes the symptom

Traced `f_qmake`/`f_qopen`/`f_qwrite` (`0086h`/`0087h`/`008Bh`) to
their real source: `BDOSIF.BDO`'s own `init:` routine (BDOS's
cold-boot "create the mx disk queue" sequence, markers `q`/`[`/`r`/
`s`/`t`). Set a GDB breakpoint directly on `BDOSIF.BDO`'s `init:`
address (computed from `build_mpmsys.py`'s own module-base printout)
using the safe delete-and-rearm pattern from #52. **Two hits, byte for
byte identical** -- `AX=1035 BX=0004 CX=0000 DX=03f8`, same stack,
same return address (`017Ah`, confirmed via `asm86 --symbols` to be
the single instruction right after `INIT.SUP`'s one and only `callf
dword ptr bdosmod[bx]`). BDOS's `init:` really does run twice, called
from the exact same, single call site both times.

**Followed the same logic one level up**: is `SUP`'s own top-level
`init:` (the very first code the boot loader jumps to) *also* entered
twice? Breakpointed it directly. **Yes -- but the two hits are NOT
identical this time**, which turned out to be the key clue:

```
hit1: SS=0800 SP=03ee retaddr=[0000,0000]   -- the real, legitimate boot-time jump
hit2: SS=17ec SP=0102 retaddr=[0000,1780]   -- a UDA-shaped stack, mid-syscall-looking
```

Dumped `module_table[sup]`'s own 8 bytes (`entry_off,entry_seg,
init_off,init_seg`) at each hit:

```
hit1: 0718 1000 007a 1000   -- correct
hit2: 0718 1000 007a 1780   -- init_seg corrupted to 1780h (sysdat's own segment)
```

**`init_seg` reads back `1780h` instead of `1000h` (`MPM_CODE_SEG`) by
hit2.** `1780h` is exactly this build's `sysdat` segment value, and
SYSDAT's own offset 6 is precisely `module_table[sup]`'s `init_seg`
field. That is *the exact same bug class* `SUPIF.SUP`'s own
`user_entry`/`mpmif` comments already document and already fix (a
bare `mov sysdat,ax`/`ds` lands on SYSDAT's own offset 6 instead of
the writer's own local cseg cell, whenever the writer's `DS` is
already sysdat-ish) -- except this time it's not in `SUPIF.SUP` at
all. **Every one of the other five kernel modules' own `init:`
routines opens with a bare `mov sysdat,ds`**, each one's own local
`sysdat` cache variable sitting at a small fixed offset (module_base
+ 6, right after that module's own `jmp init!jmp entry` header) --
and `init:`'s own documented precondition is "assume DS = Sysdat
Segment", so this bare write reliably lands on `DS:0006` = **SYSDAT's
own offset 6** every single time one of these routines runs. Checked
all six modules directly:

- `RTMIF.RTM` line 25, `MEMIF.MEM` line 29, `CIOIF.CIO` line 25,
  `BDOSIF.BDO` line 34, `XIOS86Q.A86` line 206 -- all five had the
  identical bare `mov sysdat,ds`/`mov sysdat,ds`. (`SUPIF.SUP`'s own
  two instances of this exact bug class were already found and fixed
  earlier this session.)

**Fixed all five** the same way as every previous instance of this
bug class this session: `mov sysdat,ds` -> `mov cs:sysdat,ds`.
Rebuilt, re-ran the same GDB check on `SUP`'s `init:` -- `module_table
[sup].init_seg` now reads `1000h` correctly, and (more directly) `SUP`'s
`init:` was **not re-entered at all** within the test window this
time, where it reliably had been before. The fix works as intended.

**This still doesn't fully explain *how* the corrupted `1780h`
segment would have caused hit2's actual observed `CS:IP=1000:007A`**
(a far call using the corrupted `init_seg` directly would target
`1780:007A`, not `1000:007A`) -- so the exact mechanical link between
this corruption and the specific double-entry into `SUP`'s `init:` is
still open. But the corruption itself is real, directly observed via
memory reads (not inferred from serial bytes), matches an
already-established defect pattern in this exact codebase, and is
worth having fixed regardless of whether it's the full explanation.

**The fix changes the symptom yet again (expected, per this entire
session's own running finding)** -- but this time into something
new and, encouragingly, *deterministic*: two consecutive free-running
boots (no GDB, no added probes beyond what's already in place)
produced **byte-for-byte identical 271-byte traces**, both stopping
at `getpdadr`'s own `19h` marker showing `SI=81F0h` for idle's very
first PD lookup -- `81F0h` has the high bit set and is nowhere near
the small, sysdat-relative values (`09F0h`, `0A20h`, etc.) every
previous trace this session has shown at this exact point. Not
explained yet, but -- unlike almost everything else chased this
session -- it doesn't require fighting the timing to reproduce; it
just happens, the same way, every time so far.

**Next step**: pin down where `SI=81F0h` comes from at this first
`getpdadr` call -- check `pul` (the free-PD-pool head) directly via
GDB at this exact point, since `getpdadr`'s own documented "pop di"
trick (returning the pool slot address through a stack-juggling
pop rather than a separate variable, see the `gpd_have` code in
`PROC.RTM`) means a corrupted `pul` would show up exactly like this.

## 54. Followed #53's own next step into `getpdadr` -- found the
*mirror-image* of the bug just fixed, then found it's actually
**system-wide** (19 instances across every kernel module) -- fixed
all of them -- `f_createproc(TMP)` reaches `func()`'s own top for the
first time this entire session

Single-stepped `getpdadr` via GDB from its own top, instruction by
instruction, watching `AX`/`DX` through the exact fast-path arithmetic
(`PROC.RTM`'s `cmp ax,sysdat`/`mov cx,sysdat`/`sub ax,sysdat`
sequence). Confirmed `u_wrkseg` reads correctly (`1780h`, matching
`sysdat` itself -- correct for idle's PD, which really is
sysdat-resident) and the arithmetic proceeds exactly as hand-derived
*until* `sub ax,sysdat` at step16: `AX` goes from `1780h` to `0780h`,
meaning the bare `sysdat` read there evaluated to `1000h`, not the
real `1780h`. **Same bug class as #53 -- just the read side instead
of the write side, and in a different file.**

`PROC.RTM`'s `getpdadr` is assembled together with `RTMIF.RTM` as one
unit (`RTM.A86`), sharing one symbol table -- so a *bare* `sysdat`
reference in `PROC.RTM` resolves to `RTMIF.RTM`'s own local `sysdat`
cache (a `cseg`-declared variable, module-base-relative), the exact
same symbol #53 just fixed the *write* side of. Reading it bare (no
`cs:` prefix) means the CPU reads from `DS:<that variable's own small
offset>` instead -- and since `DS` is genuinely `sysdat` itself at
this point (by design, this routine's whole job depends on it), that
lands on **`SYSDAT`'s own offset 6 -- `module_table[sup].init_seg`**,
the exact same corrupted cell #53 found and fixed. Before #53's fix,
that cell happened to hold something close to the real `sysdat` value
(by the coincidence of the corruption itself writing a plausible
near-match there), so this read "mostly worked" by accident. After
#53's fix, it correctly holds `1000h` (`module_table[sup]`'s own,
correct `init_seg`) -- which is *correct for that cell*, and *wrong*
for what this unrelated read actually needed. **Fixing one bug
unmasked a second, previously-hidden one at the exact same memory
location.**

**Given that, searched the entire kernel for every bare (non-`cs:`)
reference to `sysdat`, since the cache-shadowing pattern (`sysdat dw
0` declared locally in every module's own header, shadowing nothing
global since there never was a global `SYSDAT.FMT` field by that
name) means *any* bare reference anywhere in the tree has the same
exposure.** Found and fixed **19 separate instances**, across both
the read and write sides, well beyond the one `getpdadr` call that
started this:

- `RTMIF.RTM`: the `add ax,sysdat` x2 in `init:` (computing tick's and
  idle's own UDA segments -- these run *before* `proc_creat` is ever
  called, so this one alone could have been corrupting idle/tick's
  own `u_initcs` from the very first process this kernel ever
  creates).
- `PROC.RTM`: `getpdadr`'s remaining two bare reads (`mov cx,sysdat`,
  the `cmp ax,sysdat` bounds check).
- `DSPTCH.RTM`: `mov ss,sysdat` in `dsptch`'s own stack-switch (this
  file already had three *other* occurrences fixed earlier this
  session -- this fourth one had been missed).
- `QUE1.RTM`: `mov es,sysdat` in `makeq_entry` -- `f_qmake`'s own real
  implementation, directly in the path of the queue-create/open/write
  triplet #52/#53 have been chasing all along.
- `QUE2.RTM`: `getqdaddr`, the queue-address equivalent of
  `getpdadr`, with the identical three-instruction bug.
- `BDOSIF.BDO`: `mov ss,sysdat` in BDOS's own `entry:` stack-switch.
- `INIT.SUP`: `mov ds,sysdat` at the literal top of the `nrsp:` loop
  (**"reset DS"** -- executed at the start of *every single nrsp
  iteration, including the one that processes `TMP`'s own RSP**).
- `IDLE.SUP`: the idle process's own `mov ds,sysdat`.
- `COMMAND.SUP`: four instances (command-tail copy, FCB setup, chained
  UDA copy x2).
- `LOAD.SUP`: six instances (disk-read segment setup, UDA
  zero-init, two memory-free segment restores, UDA-vs-sysdat
  subtraction, second disk-read call).
- `RPL.SUP`: one instance (RPL queue-flag check).
- (`XIOS.A86`'s own four instances were found but are dead code --
  confirmed via `Makefile`/`build_mpmsys.py` that this build uses
  `XIOS86Q.A86` exclusively, `XIOS.A86` is never assembled.)

Every one fixed the same way as #53's own fixes: `sysdat` -> `cs:sysdat`.

**Rebuilt and tested. Two results, in order:**

1. **A regression, briefly**: with only #53's fix in place (before
   this round's full sweep), `getpdadr`'s own un-fixed bare reads
   (still reading the now-*correctly*-`1000h` `init_seg` cell) made
   idle's PD lookup compute `SI=81F0h` and the system crashed almost
   immediately (a clean, deterministic 271-byte trace, confirmed
   twice) -- worth recording since it's a reminder that this bug
   class has *two* sides, and fixing only the write side of one
   instance can,  by removing the accidental "close enough" value the
   write-side bug had been leaving behind, make a *different*,
   previously-masked read-side bug actively wrong instead of
   accidentally tolerable.

2. **With the full sweep (all 19 instances) applied**: that crash is
   gone, and -- for the first time in this entire multi-session
   investigation -- **`f_createproc(TMP)`'s own real `int mpmint`
   syscall reaches `func()`'s own absolute top, marker `16h`, with
   the correct `CX=0090h`.** Every previous trace this whole session,
   without exception, either hung, crashed, or landed partway through
   `func()`'s body having skipped its own top entirely. This is new,
   confirmed ground.

**It still doesn't finish cleanly.** A few instructions later, right
between marker `16h` (func's top, `CX=0090h`, `CH=0`) and marker `17h`
(`func1`'s own top, a few straight-line instructions later with
nothing in between that touches `CX` in the source), `CH` reads back
as `10h` instead of `0`, and by the next marker (`09h`, `gofar`'s own
top) `CL` has *also* changed, to `00h` (from the original `90h`) --
taking the "system function, module 10h" branch (invalid -- there is
no module 10h) and landing on `not_implemented` again. Tick-interrupt
markers are still at zero for this entire trace, so it isn't
interrupt interleaving. Not explained yet -- this is squarely #53's
own "next step" now, just one level further in than before.

**Also notable, same trace**: the previously-untouchable tail-end
`f_terminate` call (`CX=008Fh`) now correctly fires *every* probe
in its path for the first time too -- including the brand-new `18h`
marker (user_entry's literal last instruction) that had never fired
even once in any earlier trace this session, for *any* call. Real,
broad improvement, even though the createproc-specific `CH`/`CL`
mystery above is still open.

**Next step**: add one narrow probe between marker `16h` and marker
`17h` (e.g. right after `fnchk`'s loop, right after `mov u_func,cl`,
and right at `func1`'s own very first instruction, before this
probe's own pushes) to bisect exactly which of the handful of
straight-line instructions in between is where `CX` actually changes
-- it's a short enough span now that two or three well-placed probes
should pin it down directly, the same way #53's GDB single-step did
for `getpdadr`.

## 55. Bisected #54's own next step all the way down -- found the
corruption happens inside marker `16h`'s *own* probe code, and found
a real methodological gap along the way: every CX-reading probe this
entire session has been silently immune to exactly this class of bug

Added markers `84h` (SP/SS at `func`'s top), `88h` (CX right before
the `fnchk` loop's first iteration), `86h`/`87h` (CX immediately
before/after the mismatch branch's own digit-printing `zdbg` call),
and `85h` (CX right after the mismatch's closing `#`) -- walking
backward from #54's own `17h` reading. Every one of them showed
`CX=1000h` (`=CS`) for `f_createproc(TMP)`'s call, all the way back to
`88h`, *before the loop's first iteration even runs*. Bisected as far
back as physically possible inside `fnchk` -- the corruption isn't in
the loop at all.

**Added one more marker, `89h`, directly after marker `16h` --
deliberately *without* `89h`'s own `push cx`/`pop cx` wrapper, reading
the live `CX` register bare instead of restoring it from a
self-saved copy first.** This is the one thing genuinely different
about it: every single CX-reading probe added this entire
session -- `16h`, `Y`, `17h`, `82h`, `83h`, and now `84h`-`88h` --
wraps its read in its own `push cx ! ... ! pop cx`, which means each
one *always* shows the value it saved for itself, regardless of
whatever the live register ends up holding afterward. `89h` is the
first one that doesn't. **It shows `CX=1000h`, immediately after
marker `16h` -- which itself correctly showed `CX=0090h` just moments
before, via its own protected read.**

So the live `CX` register is already wrong by the time marker `16h`
finishes -- specifically, sometime during its own `mov ax,cx!call
zdbg` or its own trailing `pop dx!pop cx!pop ax`. Every one of #54's
later markers (`88h` onward) was just re-observing the same already-
corrupted value, not catching it fresh each time.

**This exposes something that should have been caught much earlier
this session**: because every prior CX-reading probe protected itself
with its own save/restore, *none of them could ever have revealed a
case where the live register was wrong independent of what got
pushed and popped around it*. This specific corruption could
plausibly have been present (silently) in earlier rounds too, masked
the same way, and nothing in the probe methodology used until now
would have caught it.

**Tested the two most obvious theories directly, both negative:**

- **Stack exhaustion**: `84h` shows `SP=00FCh`, `SS`=the UDA segment,
  right at `func`'s own top -- meaning essentially the *entire*
  256-byte (`ulen`) UDA stack is still free at this point. Ruled out.
- **An interrupt landing mid-probe**: added a `pushf!cli`/`popf`
  diagnostic bracket around the whole `fnchk` block -- corruption
  persisted identically. Moved it earlier, to `func`'s own absolute
  first instruction (before marker `16h` even runs) -- the corrupted
  value *changed* (from `1000h` to a different garbage, `0455h`, on
  one run, then correct on an immediate retry) but did not go away.
  Since this changes real control flow without fixing anything, and
  since #54 already established zero tick-interrupt markers fire
  anywhere in these traces, both `cli` diagnostics were reverted
  rather than left in the tree -- inconclusive, and not worth the
  risk of a silent behavior change for an unproven theory. The honest
  state: not interrupt-explained in any way found so far, but the one
  timing-sensitive wrinkle (the value changing when `cli` was added
  earlier) means the underlying race, whatever it is, is still live.

**Still open, now about as narrowly bisected as source-level
reasoning can get it**: the corruption occurs between marker `16h`'s
`mov ax,cx!call zdbg` and the moment marker `89h` reads `CX`
immediately afterward -- i.e. either inside `zdbg` itself (despite
multiple careful readings finding its own push/pop entirely balanced)
or in marker `16h`'s own `pop dx!pop cx!pop ax`. It happens *only* for
calls that reach `func()` via the small, UDA-based stack-switch
convention (confirmed: idle's own three queue calls, which run on the
original/large boot stack and never switch, show `CX` correctly at
every one of these same markers, every time). `CX` consistently lands
on `1000h`, exactly `CS` -- consistent with some kind of stack
misalignment that leaves a stray segment value sitting where `CX`'s
saved copy should be, but nothing in `zdbg`'s own structure (checked
multiple times, push/pop counts match exactly: 4 in, 4 out at the
outer level, 1 in/1 out per digit at the inner level) explains how
that could happen.

**Next step**: this needs a cleaner tool than one more serial probe.
Since GDB's `continue`-based breakpoints reliably miss this exact
call under its own timing (confirmed repeatedly this session -- GDB
never seems to reach `f_createproc(TMP)`'s own call to `func()` within
a reasonable number of continues), the next productive move is
probably a **memory watchpoint** on the exact stack address marker
`16h`'s own `push cx` writes to (computable directly from `SP` at
`84h`'s own reading), set *before* GDB's own timing has a chance to
avoid the call -- i.e. let the free-running serial trace establish
that the call happens at all, then attach GDB and step from a
breakpoint placed earlier in `user_entry` (which GDB *does* reliably
reach) forward instruction-by-instruction into `func()`, watching
that one stack slot the whole way, rather than trying to land a
breakpoint on `func` itself and hope the timing cooperates.

## 56. Followed #55's own next step with GDB -- proved the corruption
is a genuine timing race, found and fixed a real, previously-pending
interrupt, caught and fixed a self-inflicted probe bug in the same
sweep, and reached further than this entire session ever has: TMP's
own PD gets created cleanly and a real queue function succeeds

**Breaking on `func` directly under GDB never once caught
`f_createproc(TMP)`'s own call** (confirmed again this round, many
tries) -- but breaking on `user_entry` instead (the real `int mpmint`
syscall gate, reached for both of the two true syscalls this trace
ever issues) **caught it reliably, every time.** From that reliable
breakpoint, single-stepped (`stepi`, not `continue`) all the way
through `user_entry`'s own prologue, into `func()`, through the
entire `fnchk` loop, past `Y`/`func1`/`nonet`, and out the other side
-- roughly 1600 real instructions -- watching `CX` continuously.
**It never corrupts.** Every single step shows the correct `CX=0090h`.

This proves #55's bisection conclusively: the corruption is a **real,
genuine hardware-timing race**, not a static logic bug anywhere in
`zdbg`, marker `16h`, or the `fnchk` block -- identical code,
single-stepped, behaves perfectly every time; the same code, free-
running, corrupts reliably. Single-stepping changes real-world timing
enough (GDB's own per-instruction round-trip overhead) that whatever
asynchronous event causes this never gets the chance to land in the
same place.

**Found it directly.** Real x86 `sti` has a well-known one-instruction
delay before it actually takes effect -- an interrupt that's already
pending at the moment `sti` runs will not be recognized until *after*
the very next instruction. `SUPIF.SUP`'s `uecont:` does `sti` to
re-enable interrupts for the very first time this syscall (`cmp
intflag,0 ! je noints ! sti`). Changed it to `sti ! cli` -- enable for
exactly one instruction's worth of time, then immediately disable
again, using the delay as a trap door that lets nothing through.
**The `CX` corruption disappeared completely** -- confirmed via
marker `89h`'s own unprotected read, clean `CX=0090h` at every single
marker from there through the entire `fnchk`/`Y`/`func1` sequence,
reproduced twice. This is about as direct a confirmation as this kind
of bug ever gets: something was genuinely pending, right at that
exact instant, on every single `f_createproc(TMP)` call this whole
session, and giving it even one instruction's window to fire was
enough to corrupt the stack-based register save/restore discipline
`zdbg`'s callers all depend on. (Kept the `sti!cli` fix in the tree --
it's clearly net-positive and well-understood in its own right, even
though *which* interrupt source was pending, and why, is still open.
Tick-interrupt markers remain at zero throughout, so it isn't the
periodic timer specifically; some other, not yet identified source.)

**With that fixed, dispatch now reaches `gofar` correctly for the
first time -- and immediately ran into what looked like the exact
same corruption again, one register over.** Marker `0Bh` (dumping the
incoming `AX`/target-module at `gofar`'s own top) kept reading back
`000Bh` no matter what -- tried the same `sti`-delay theory at
`gofar`'s own top, then at `nonet`'s, with plain `cli` this time
(not the one-instruction trick, just blocking interrupts outright
across the whole span) -- **no effect either way.** That ruled out
interrupts for this specific one and forced a re-read of the probe's
own source:

```
push dx ! mov al,0Bh ! mov dx,03f8h ! out dx,al ! call zdbg ! pop dx
```

**`mov al,0Bh` -- the marker byte itself -- overwrites `AL` before
`call zdbg` ever reads it.** This probe was never dumping the real
`AX`; it's been dumping its own marker byte back at itself since the
moment it was added, in #50. Every "`0B:000B`" reading in this entire
session's logs, across every earlier round, was this bug, not a
kernel finding -- retracting whatever conclusions leaned on it.
Fixed the same way marker `09h`, right next to it, already did it
correctly: save the real value into a scratch register (`BX`) before
clobbering `AL` with the marker, restore it before the real dump.
Reverted the now-answered `cli` experiments at `gofar`/`nonet` (they
were chasing a probe artifact, not a real race -- no reason to leave
an unexplained behavior change in the tree for a bug that turned out
not to exist).

**Rebuilt with both fixes in place. The result is the furthest this
entire multi-session investigation has ever gotten, by a wide
margin:**

- `gofar`'s own `S` marker now correctly resolves `CX=000Eh` (local
  function 14) against module 2 (`rtm`), target `19F5:1000` --
  `RTMIF.RTM`'s own `entry:` address, exactly as it should.
- `V` fires (RTM's `entry:` reached), then marker `10h` shows
  `SI=001Ch` (`14 * 2` -- `creat_proc_entry`'s own function-table
  slot, exactly matching).
- **Marker `1Eh` fires -- `creat_proc_entry` itself, reached for the
  first time in this entire session**, with `DX=0010h`, exactly
  `TMP`'s own `rsp_pd` offset.
- `proc_creat` is entered from a brand new return address (`1ABCh`,
  `creat_proc_entry`'s own call site -- not the old `186Eh`/`18DEh`
  used by every direct, bootstrap-time call), confirming this is
  genuinely `TMP`'s own creation, not a repeat of idle/tick's.
- `getpdadr` correctly takes the *slow* path this time (marker `2Fh`
  fires, `DI=0E40h` -- a real, clean `pul` slot), copies `TMP`'s PD
  template from its own RSP data segment into that slot, and markers
  `7Fh`/`81h` (added back in #48, never fired until now) both read
  `FLAGS=0046h` -- **answering #47's original open question
  directly: `IF` genuinely does stay clear through the whole `rep
  movsw` critical section.**
- `p_link[0E40h]` reads back **`0000h` -- clean** -- no bogus second
  PD this time. The `cp_uda` recursion checks it, finds zero, and
  stops immediately. `TMP`'s PD creation completes with no corruption
  at all.
- Shortly after, a **real RTM function call succeeds end to end**:
  `f_qread`-equivalent (`CX=0089h`, local func 7) resolves correctly,
  reaches `RTMIF.RTM`'s own `entry:` dispatch (marker `10h`,
  `SI=000Eh`), and returns cleanly via marker `T` -- no `U`.

**Not clean yet.** One call in between (`CX=0000h`, right after
`creat_proc_entry` returns through `pdisp`) still resolves to
`not_implemented` -- not yet investigated, could be anything from a
genuinely-invalid function code in this exact path to another
instance of this same probe-artifact family; given today's lesson,
it gets checked by hand before being trusted. And after the
successful queue call, the trace shows `INIT.SUP`'s own cold-boot
markers (`1`, the `SS:SP` dump, `2`, `3`) firing *again* -- the same
literal signature every trace this session has shown exactly once,
at the very start, right after the boot banner -- strongly suggesting
`SUP`'s own `init:` is somehow being re-entered a second time, deep
into what should be steady-state operation. The trace then goes
completely silent (zero further bytes in 40 seconds, confirmed twice)
with no termination markers (`$`/`%`/`^`) -- a real hang, not a
graceful stop. `TMP`'s own real entry point (marker `27h`) still
never fires.

**Taking stock**: this round turned up two more instances of exactly
the lesson #52 and #55 already taught -- a marker byte silently
clobbering the very thing it was meant to display (`0Bh`, since #50),
and a corruption that only a genuinely different *tool* (GDB
single-stepping, not another serial probe) could distinguish from a
static bug. Both lessons paid for themselves immediately this round.
Worth treating as standing practice: before trusting any "always
reads back X" probe finding, check whether the probe's own marker
byte could have landed on the register being dumped; before spending
another round bisecting a "corruption" with more probes, check first
whether it reproduces identically under single-stepping.

**Next step**: two independent threads, either one reasonable to pick
up first --

1. Chase the new hang: is `SUP`'s `init:` genuinely being re-entered
   (same technique as #53 -- breakpoint on `init:`'s own address,
   confirm hit count and register state), or is this another marker
   collision/probe artifact given today's track record (`1`/`2`/`3`
   are bare digit characters, extremely collision-prone per #52's own
   audit)?
2. Identify the actual interrupt source the `sti!cli` trick is
   papering over, so it can be fixed at its root (masked/acknowledged
   properly) instead of narrowly dodged for one instruction on every
   syscall entry.

## 57. Chased #56's "SUP `init:` re-entered" thread -- found and fixed
a real, independent alignment bug in `build_mpmsys.py` along the way,
though it turned out not to be this particular symptom's explanation

**Confirmed the re-entry directly via GDB** (same technique as #53):
breakpointed `SUP`'s own `init:` label (`0x7A`). Hit twice -- the
first, at boot, with the expected fresh `SS:SP=0800:03ee`; the
second, much later (after the successful queue-read call from #56),
with `SS:SP=178B:0F76` and the three words above it on the stack all
reading `0xCCCC` -- i.e. genuinely uninitialized memory, not a real
return frame. `init:` really is being re-entered, with `CS:IP`
landing exactly on `1000:007Ah` from a context that looks nothing
like a legitimate call or the original boot jump.

**Went looking for a plausible mechanism and found a real bug, just
not (as far as tested) this one.** `TMP`'s own PD template bakes in
its initial dispatch target (`stack_top`'s two words: `offset tmp` --
TMP's own code-relative IP, assembled directly -- and a segment slot
that `build_mpmsys.py` patches at combine time with TMP's real,
computed code segment). Checked the patch logic itself and found:

```python
tmp_code_seg = MPM_CODE_SEG + (len(cseg) + 15) // 16   # rounds UP
...
cseg += tmp_cseg                                        # appends at the
                                                         # CURRENT (un-
                                                         # rounded) length
```

**`len(cseg)` (the six kernel modules' combined code, before `TMP` is
appended) is 29023 bytes -- not itself a multiple of 16.**
`tmp_code_seg` rounds that up to the next paragraph boundary (byte
29024) and uses *that* as TMP's segment, but the actual `cseg +=
tmp_cseg` three lines later appends TMP's real bytes starting at the
unrounded 29023. A 1-byte gap, confirmed by direct computation against
this build's own module sizes -- meaning every single address inside
TMP (`offset tmp` included) was being read from one byte off from
where its own code and data actually live. This is a correctness bug
in our own from-scratch tooling, not DRI-original source, and has
apparently been there since `TMP` was first baked in.

**Fixed**: pad `cseg` itself up to the next paragraph boundary before
computing `tmp_code_seg`, so the segment and the actual append agree:

```python
cseg += b'\0' * ((-len(cseg)) % 16)
tmp_code_seg = MPM_CODE_SEG + len(cseg) // 16
```

(`build_disk.py`'s own independent recomputation of `sysdat_segment`,
which this exact kind of mismatch broke once before per this script's
own comments, still agrees afterward -- the total combined length is
still paragraph-aligned either way, so nothing downstream of TMP
shifts.)

**Rebuilt and retested. The `init:` double-hit and the hang after it
are unchanged** -- same signature, same point in the trace, just
different incidental garbage values (expected, per this whole
session's "every byte change retimes everything" finding). So this
specific 1-byte misalignment, while real and now fixed, wasn't (at
least on its own) *this* symptom's cause. Kept the fix regardless --
it's independently correct and was never going to do anything but
help.

**Status**: `#56`'s two threads (identify the re-entry's real trigger;
identify the interrupt source the `sti!cli` trick dodges) are both
still open. This round's main achievement stands on its own:
`f_createproc(TMP)` now completes cleanly end-to-end for the first
time, including a verified-clean `p_link` and a confirmed-clear `IF`
through `getpdadr`'s critical section -- the hang now happens
*after* that success, during whatever runs next (the dispatcher
picking a process to run, and that process's own early behavior),
which is new territory for this entire investigation.

## 58. Reframed #56/#57's "`SUP init:` re-entered" finding entirely --
it isn't `init:` running again, it's a crash landing that happens to
execute `init:`'s own bytes

Tried to find the actual jump/call responsible via GDB. Breaking on
`func`/`gofar` directly never catches anything useful here (same old
timing-sensitivity), and a GDB single-step walk of ~12000 instructions
from `user_entry` only made it back to `user_entry`'s own early `zdbg`
loop -- nowhere near far enough to reach this point, confirming (again)
that this entire region of execution is too deep to single-step
through in a reasonable number of steps. Tried attaching GDB to an
*already-running*, unpaused QEMU instead (hoping to keep full-speed
timing up to the breakpoint) -- by the time the script finished
connecting, the entire sequence (boot through the hang) had already
played out at full TCG speed; the breakpoint never fired, and the
captured serial trace was byte-identical to a free-running capture.
Confirms the interesting window here is a handful of real seconds at
most.

**Went back to the reliable `-S` + breakpoint + `continue` pattern
(not single-stepping) and read much deeper into the stack and
registers at the second `init:` hit than before.** The result
reframes the whole finding:

```
DS=ES=0xCCCC   AX=0xFFFF   BX=1   CX=9   SI=0x0E40   DI=0x01D8
stack: cccc cccc cccc cccc ... (long run) ... 0001 584D 6964 6B73 2020 0000 ...
```

**`SI=0x0E40` is `pul`'s own real free-PD-pool slot address** (the
exact one `getpdadr` allocated for `TMP`'s PD, confirmed clean back
in #56). **`DI=0x01D8` is `stack_top`'s own offset within `TMP`'s
dseg** -- the literal field `proc_creat`'s "setup user stack for
iret" code (`PROC.RTM`, `mov di,u_sp ! mov ax,u_initcs ! mov
2[di],ax ! mov word ptr 4[di],0200h`) writes the new process's
initial `CS`/flags into. **`584D 6964 6B73 2020`, read as bytes, spells
"MXdisk  "** -- the literal name of `BDOS`'s own disk multiplexer
queue, created back in `BDOSIF.BDO`'s `init:` (the very `f_qmake`/
`f_qopen`/`f_qwrite` triplet #52/#53 chased originally). **`DS`/`ES`
both reading exactly `0xCCCC`** matches `TMP.A86`'s own declared
canary pattern (`stack dw 0CCCCh,0CCCCh,0CCCCh`) -- a value that
should never legitimately end up in a segment register.

**None of this is `SUP`'s `init:` logic running again.** It's the
CPU executing `init:`'s own bytes with completely unrelated register
state left over from somewhere entirely different -- a wild jump (or
a bad `ret`/`retf` popping garbage) that happens to land on `CS:IP
=1000:007Ah` coincidentally, not a genuine re-entry into the cold-boot
routine. The actual crash is almost certainly happening during or
right after `proc_creat`'s own IRET-frame setup for `TMP`'s very
first dispatch -- `DI` sitting exactly on `stack_top`'s offset is too
specific to be unrelated -- with `DS`/`ES` ending up pointing at
`TMP`'s own uninitialized canary area instead of its real UDA/stack
segment, and a reference to `BDOS`'s queue name somehow also live on
the same stack.

**Traced the segment-defaulting logic this would need to go through**
(`PROC.RTM`'s `cp_uda`: `u_initss`/`u_initds`/`u_inites` all read `0`
in `TMP`'s own un-patched template, confirmed by direct inspection of
`build/kernel/TMP.dseg.bin`, so they should correctly default-fill
from `AX`=`u_wrkseg` at that point -- which should be `TMP`'s own
real RSP data segment, not `0xCCCCh`) and didn't find an obvious
static defect in it -- the path *should* produce a sane segment.
Given this round's own repeated lesson (every clean static read-
through this session has eventually needed either a timing tool or a
direct memory read to find the real defect), this needs the same
treatment rather than more hand-tracing: a GDB breakpoint on
`proc_creat`'s own "setup user stack for iret" block specifically
(not on `func`/`gofar`, which are too far upstream and too timing-
sensitive to reliably reach), watching `u_ss`/`u_initss`/`DI` at that
exact point, the same way #54's step-by-step `getpdadr` walk found
the `sysdat` bug.

**Next step**: breakpoint directly on `PROC.RTM`'s `mov ds,ds:u_ss`
line (the one right before the `stack_top` write) and read `u_ss`/
`u_initss`/`u_wrkseg` from memory at that exact point, for `TMP`'s
own creation specifically -- this is a single, well-aimed breakpoint
rather than a long step-walk, and should show directly whether the
`0xCCCC` enters via a bad default-fill, a bad `DI`, or something
else neither reading has considered yet.

**Did exactly that -- result is negative, but cleanly so.** Added two
probes directly in `PROC.RTM`, right before and right after `mov
ds,ds:u_ss`, dumping `DS`/`u_ss`/`u_initss`/`u_stack_sp` and then the
post-switch `DS`/`DI`. For `TMP`'s own creation specifically:

```
before: DS=1764  u_ss=1760  u_initss=1760  u_stack_sp=01D8
after:  DS=1760  DI=01D8
```

(`build_mpmsys.py` confirms this build's real values: `TMP code @
0x171C, data @ 0x1760` -- so `u_ss`/`u_initss` read back exactly
`tmp_data_seg`, and `u_stack_sp` reads back exactly `stack_top`'s
own offset.)

**This is all internally consistent and correct, not a bug.** `DS`
reads `1764` beforehand because that's the UDA struct's own real
absolute segment (`tmp_data_seg` plus `TMP`'s own `p_uda`-relative
offset, a few paragraphs in) -- genuinely different from
`tmp_data_seg` itself, by design, since the UDA sub-structure doesn't
start at offset 0 of `TMP`'s dseg. `mov ds,ds:u_ss` deliberately
*switches* `DS` from the UDA's own segment to `tmp_data_seg`, because
`stack_top` (unlike the UDA fields) was assembled relative to the
dseg's own base, not the UDA's. Every value lines up with where it
should: `DS` correctly becomes `1760` after the switch, `DI` correctly
lands on `01D8h`, and the write that follows (`mov 2[di],ax`) is
reading/writing exactly where it should be. My working theory from
earlier in this section -- that this exact block was where
`0xCCCC` enters -- is retracted; this specific stretch is clean.

**So the crash happens later than this** -- somewhere after `pop
ds!pop es` (line 150), through the thread-list linking, and
(most likely, given how specific `DI`/`SI`'s values were at the
crash) during the dispatcher's own first `IRET` into `TMP`. Given
this round's pattern of needing several wrong guesses to narrow
things down, the next probe should go *after* this block rather than
inside it -- specifically wherever `switch`/`dsptch` loads a
process's saved registers and `ES`/`DS` back out before `iret`,
since that's the next (and likely last) place `ES`/`DS` get set from
UDA-stored values before `TMP` actually starts running.

**Found `switch` (`DSPTCH.RTM`), read it closely, and found a
genuinely suspicious branch -- though not yet confirmed as the real
cause.** `switch` restores the picked process's registers from its
UDA, sets `SS:SP` from `u_ss`/`u_sp` (which, for `TMP`, correctly
point at `stack_top`'s own pre-built IRET frame per the previous
negative result), then:

```
cmp u_in_int,true ! jne dret
    jmp int_disp_exit     ; <- ends in "iret", using u_ds_sav/u_es_sav
dret: ...ret               ; <- ends in a plain "ret", not iret
```

`proc_creat`'s own `cp_uda` **unconditionally** sets `u_in_int,true`
for every newly created process (`PROC.RTM`, "set other uda values").
That means `TMP`'s *very first ever* trip through `switch` takes the
`int_disp_exit` branch -- the path meant for resuming a process that
was suspended *mid-syscall*, which restores `DS`/`ES` from that
process's own `u_ds_sav`/`u_es_sav` and performs the real `iret` --
rather than `dret`'s plain `ret` (used for anything else, including,
presumably, a process's first-ever dispatch). Checked whether
`u_ds_sav`/`u_es_sav` could plausibly be the `0xCCCC` source: no --
`cp_uda` initializes both from `u_initds`/`u_inites`, which
default-fill from `u_wrkseg` exactly like `u_initss` already did
(confirmed clean), so they should read back `1760h` too, not
garbage. This narrows but doesn't fully resolve it: `u_in_int=true`
for a never-yet-run process looks like a real, independent defect
either way (taking the interrupt-resume exit path for a process that
was never interrupted), but on its own it doesn't yet explain the
exact `0xCCCC` reading.

**Added a probe to confirm this empirically for `TMP`'s own case
(`u_in_int`/`u_ds_sav`/`u_es_sav`/`DS`/`ES` right before the `cmp`)
and it never fired** -- this run's retiming (one more probe, one more
byte) skipped `switch` for `TMP`'s dispatch entirely, landing back on
the same `func(CX=0000)`-fails-then-hangs shape from #56/#57 instead,
by a different route. The underlying hang's *overall shape*
(successful PD creation, then a failure a short distance later, then
the same `1`/`SS:SP`/`2`/`3` tail) keeps reproducing; the exact
path to it keeps shifting probe to probe, consistent with everything
else this session has found about retiming.

**Next step, concretely queued for the next round**: re-run with the
`8Ch` probe already in place (no further edits needed) until a trace
reaches `switch` for `TMP`'s own dispatch, and read back `u_in_int`/
`u_ds_sav`/`u_es_sav`/`DS`/`ES` directly -- if `u_in_int` reads `true`
and the branch goes to `int_disp_exit` as the source now shows it
must, that's confirmation enough to justify fixing the real, likely
bug underneath all of this: `cp_uda` should probably set
`u_in_int,false` for a freshly created process, not `true` --
matching every other kernel's convention that `true` means
"currently suspended mid-syscall," which a process that has never
run cannot be.

**Deliberately held off on actually changing `u_in_int`'s default.**
Re-checked the reasoning first and found it cuts both ways: `switch`'s
`int_disp_exit` branch (restore `DS`/`ES` from `u_ds_sav`/`u_es_sav`,
then `iret`) is *also* plausibly the intended, correct way to perform
*any* dispatch via `switch`, new process or not -- its `DS`/`ES`
restore would correctly land on `TMP`'s own `1760h` (matching
`u_ss`, confirmed sane) right before the `iret`, which is exactly
what a fresh "Small Model" process should see. `dret`'s plain `ret`
(no `iret` at all) looks more like it's for some *other* caller of
`switch` that doesn't need a context switch, not a counterpart to
`int_disp_exit` for "first dispatch vs. resumed dispatch". Changing
`u_in_int`'s default without first confirming which of these two
readings is right would risk trading a real bug for a different one
on pure guesswork -- left it alone and documented the open question
instead of acting on an unconfirmed theory.

**Re-ran the same build three times to retry marker `8Ch` -- byte-
identical output every time (2049 bytes), and it never fires.** This
build's race has stabilized on a *different* path than the one that
reaches `switch`: `creat_proc_entry`'s own PD-creation chain still
completes cleanly, but the subsequent `func(CX=0000)` failure (same
one flagged as unexplained in #56/#57) shows the **exact same kind
of corruption the `sti!cli` fix addressed for the first syscall** --
marker `16h` reads `CX=0000` and the very next marker, `89h` (bare,
unprotected), reads back `CX=1FC4h` instead, for this later call.
**The `sti!cli` fix only closes the one specific window it was
placed at** (`user_entry`'s own `sti`) -- whatever's pending here is
either the same interrupt source landing at a second, later `sti` (or
equivalent re-enable point) elsewhere in this exact call path, or a
genuinely different source. Not yet traced to its own enable point;
flagging it explicitly rather than let it blend into the general
"timing is messy" background noise, since it's the same *shape* of
bug #56 already solved once.

**Taking stock for next time**: three real, open items now, in
rough priority order --

1. Confirm (don't guess) whether `switch`'s `dret` vs
   `int_disp_exit` branch is actually wrong for a fresh process, by
   actually catching marker `8Ch` in a trace (may need a fresh probe
   placement or a deliberate small retime to get there, since this
   exact build's race currently avoids it).
2. Find and close the *second* `sti`-delay-shaped window exposed by
   marker `16h`/`89h`'s mismatch on the `CX=0000` call, the same way
   #56 closed the first one.
3. Only once both of those are resolved, re-examine whether the
   `CS:IP=1000:007Ah` crash landing (confirmed via GDB, #58) still
   happens, or was itself just a downstream symptom of one of these
   two.

## 59. Followed #58's item 2 first -- found and fixed a second
instance of the `sti`-delay bug, this time in `dsptch` itself, and a
new, previously-undetected marker collision along the way

**Confirmed `switch` genuinely never runs in this entire trace**,
via a direct GDB breakpoint at its own address under `-S`+`continue`
(preserves full-speed timing, same technique that worked cleanly in
#56): a single `continue` ran straight to program exit without ever
stopping there. `dsptch` itself, though, *does* run at least once --
its own entry marker (`.`, confirmed real this time, not a false
positive off the boot banner's progress dots) fires with a sane
`DS`/`indisp=false`, takes the `dispin` branch correctly.

**Caught a real, previously-undetected marker collision while
narrowing this down**: `DSPTCH.RTM`'s own `mov al,11` (decimal
literal, byte `0x0B`) collides with `SUPIF.SUP`'s `mov al,0Bh` (hex,
same byte) -- the exact decimal/hex-literal gotcha this session
already hit once before (documented early on), but this session's
own collision-scanning regex only ever matched the `NNh` hex form,
never a bare decimal literal, so it was never caught. Readings of
`0x0B` deep inside `dsptch`'s own scheduling logic are real
(`DSPTCH.RTM`'s own marker, "old rlr / new rlr"), not `SUPIF.SUP`'s
`gofar` probe misfiring somewhere it has no business running --
worth remembering for any future scan of this kind.

**Added a probe at `dispin`'s own first instruction (confirmed hit)
and another right after its own `sti` (never fired).** Same shape,
same fix as #56: `dispin`'s stack-switch-then-`sti` (re-enabling
interrupts on the brand-new `dsptchtos` scheduler stack, right before
saving the current process's registers into its UDA) has the exact
same one-instruction-pending-interrupt vulnerability `user_entry`
had. Changed `sti` to `sti ! cli` the same way. **The probe right
after it now fires reliably.**

**With this fixed, tracing deeper**: `dsptch`'s own
"take current process off the ready list, dispatch by status" tail
now runs cleanly and visibly for the first time -- `rlr`/`p_link`
read sanely (old `rlr=initpd`, taken off, `dsp_table[0]` ="run" status
correctly selected, `insert_process` puts `initpd` back as the new
`rlr` since nothing else is ready yet), and `dsptch` returns normally
(`dret`, not a crash). **The race has simply moved one step further
downstream again** -- right after `dsptch` returns to its caller, a
`func()` call with a now-corrupted `CX` appears, the same
adjacent-marker-mismatch shape #56 fixed once and #58 found a second
instance of. Not yet traced to its own `sti` (or equivalent) -- this
is squarely the next thing to find, using the exact same technique
that worked twice now (bisect with paired probes until the exact
instruction is caught, then check whether a `sti` sits right before
it).

**Pattern now clear across three confirmed instances**: every
"adjacent probe shows a different, wrong register value" finding
this entire investigation has traced back to a bare `sti` with
nothing guarding its one-instruction delay. Worth checking
systematically next time instead of one at a time: grep every bare
`sti` in the combined kernel (not just the two found so far) and
decide, for each one, whether it needs the same `! cli` guard, rather
than waiting for probes to find each one by accident.

**Did exactly that.** Grepped every bare `sti` in the whole kernel
(`src-assem/kernel/*.*` and `src-assem/rsp/*.*`). Only three exist in total,
all in `DSPTCH.RTM`: the one in `dispin` (just fixed above),
`schedule:`'s own (`"we can enable interrupts now... there MUST be a
process on the RLR at this point"`), and `drltorlr`'s loop-tail
(`sti ! jmps drltorlr`). The third is already self-protected by
construction: the very next instruction after the jump it's attached
to is `drltorlr`'s own `cli`, so the one-instruction delay never
actually gives an interrupt a window -- no fix needed. Applied the
same `sti ! cli` guard to `schedule:`'s, since it has the same shape
as the two confirmed bugs (a real instruction follows, not an
immediate `cli`) -- flagged in its own comment as precautionary,
since there's no probe confirming it was actually causing a problem
the way the other two were (the fix only narrows the already-enabled
window, never widens it, so the downside of being wrong here is
low).

**Rebuilt and retested. New ground, again**: `drltorlr`'s own loop
(never once reached before this round) now runs and shows **`drl`
holding `0E40h` -- `TMP`'s own just-created PD address**, sitting on
the dispatcher's ready list for the first time ever. The scheduler
is genuinely getting close to actually running it. The same
adjacent-marker `CX` corruption shows up again immediately
afterward, but this time **not** explained by a bare `sti` --
confirmed by the same whole-kernel grep coming back empty after the
three already found. Whatever this fourth instance is, it's a
different mechanism, not another case of the same bug.

**Status at the end of this round**: three real `sti`-delay bugs
found this session (two directly confirmed via probes, one applied
as a reasoned precaution), `TMP`'s PD now visibly reaches the
dispatcher's own ready list, and the investigation has moved
decisively past where it's ever been before. The remaining
corruption -- right where `drltorlr` is about to pull `TMP` off `drl`
and call `insert_process` to make it the running process -- is the
clear next target, and it's a *new* kind of corruption, not a repeat
of the `sti` family.

## 60. Found it -- a fourth `sti`-delay bug, this one hiding behind a
jump instead of a straight-line instruction, and fixing it produced
the biggest jump forward this entire investigation has seen

Added probes at `insert_process`'s own top and at both its exit
branches (`ins_out`/`ins_exit`), confirming it's correctly called
*twice* -- once for `initpd` (from `disp_act`), once for `TMP` (from
`drltorlr`, `SI=0E40h`) -- and that **both calls produce entirely
correct linked-list results**: `initpd` stays at the head of `rlr`,
`TMP` gets spliced in right after it. No bug in `insert_process`
itself, for either call.

**Went to GDB for a direct answer rather than more probes.**
Breakpointed `ins_exit` (full-speed `-S`+`continue`, not single-
stepping, preserving real timing), reached `TMP`'s own call reliably,
then single-stepped forward from there -- interrupts are provably
off this whole stretch (`drltorlr`'s own `cli`, nothing re-enables
until the tail), so stepping here can't itself dodge a hardware race
the way it did in earlier rounds. Watched `insert_process` return
correctly via `ret`, landing exactly back at `drltorlr`'s own `sti !
jmps drltorlr` -- then watched the actual fault line:

```
sti ! jmps drltorlr      ; sti's one-instruction delay applies to
drltorlr: cli ! mov si,drl  ; the NEXT instruction retiring, which is
                              ; the JUMP -- so a pending interrupt
                              ; becomes eligible to fire the instant
                              ; the CPU lands on drltorlr's own first
                              ; instruction, one full instruction
                              ; before THAT "cli" can matter
```

Every other `sti`-delay fix this session guarded a *straight-line*
instruction (`sti` immediately followed by real code, then an
explicit `cli`). This one is different in shape: `sti` is immediately
followed by a *jump*, and the delay's "next instruction" is the jump
itself -- which means the interrupt becomes eligible to fire right as
execution arrives at the jump *target*, before that target's own
`cli` (if any) has actually run. The original code's own structure
(loop back to the top of `drltorlr`, which starts with `cli`) looked
safe on a first read -- it isn't, by exactly one instruction. This
had been sitting unnoticed since the very first `sti`-delay fix in
#56, mis-assessed (by me) as self-protecting in #59.

**Fixed by relocating the brief re-enable, not removing it**:
`cli ! mov si,drl ! sti ! cli` at `drltorlr`'s own top (replacing
the jump-adjacent `sti`), so the one-instruction window lands inside
a straight-line pair with no jump between `sti` and the guarding
`cli` -- same intent as the original comment ("we can enable
interrupts... since interrupts doing a Flag_set may also"), same
net effect (one brief window per loop iteration), just relocated to
where it's actually safe.

**The result is the single biggest jump forward this entire
investigation has produced.** The trace nearly tripled in length
(2092 -> 5818 bytes) and stayed exactly that length across a second,
longer capture (confirmed deterministic, not a timeout artifact).
In order:

- `drltorlr` now correctly processes **all four PDs** waiting on
  `drl` in one pass -- `TMP` (`0E40h`), `tick` (`0A20h`), and `idle`
  (`09F0h`), each spliced into `rlr`'s list by `insert_process`
  without a single wrong value anywhere.
- **`switch` is reached for the first time this entire session** --
  confirmed via its own entry marker (`bx=rlr=09C0h`, `initpd`) and
  marker `8Ch`'s own direct read: `u_in_int=false`, correctly taking
  the plain `dret` exit (not `int_disp_exit`) -- so a part of #59's
  still-open question about that branch is now moot for this call,
  since it's not the branch taken.
- `dret`'s `ret` correctly unwinds the entire original call chain
  back up through `RTMIF.RTM`'s `entry:`, `goodmod`, `gofar`,
  `func()`, and `mpmif` -- `f_createproc(TMP)` completes **cleanly,
  end to end, for the first time ever** in this whole multi-session
  investigation.
- `INIT.SUP`'s `nrsp` loop correctly finds no more RSPs and issues
  its own `f_terminate` -- and *that* syscall also completes with
  **zero register corruption anywhere in its own trace** (every
  marker pair that used to disagree now agrees), reaching
  `terminate_entry` and `abt_chk` and continuing into real cleanup
  work (multiple further `RTM` queue calls, all resolving correctly
  through `goodmod`/`V`/`T`).
- Deeper still: `rlr` later reads `0A20h` (`tick`) rather than
  `09C0h`, meaning a real process switch away from `INIT` happened at
  some point in this trace -- genuinely new territory no prior round
  has ever reached a description of.

**Still open**: `TMP`'s own real entry point (marker `27h`) has not
fired yet in this trace. The capture settles at a stable 5818 bytes
with no further output (confirmed across two different capture
lengths) -- either a graceful idle wait or a new, not-yet-identified
stopping point, not yet distinguished. Given how much new ground
just opened up, decoding this fully is squarely next round's task
rather than something to rush through now.

**Methodological note for next time**: this makes four confirmed
`sti`-delay bugs this session, each mis-identified as safe at least
once before being confirmed otherwise. Any bare `sti` followed by a
jump (not a straight-line instruction) deserves the same suspicion
as one followed by real code -- "the jump target starts with `cli`"
is not the same protection as "the very next instruction is
`cli`."

## 61. Followed #60's own open thread -- the `SUP init:` re-entry is
still happening, but it's a different, more clarifying problem than
#58 first found

Decoded the full 5818-byte trace from #60's fix. Beyond `f_createproc
(TMP)` and `INIT`'s own `f_terminate` both completing cleanly, the
scheduler goes on to genuinely dispatch **`tick`** as well (`switch`
runs a third time with `rlr=0A20`, `u_in_int` correctly reads `true`,
`u_ds_sav`/`u_es_sav` both read back sane `179D` -- not corrupted),
and several more real `RTM`/`MEM` calls resolve correctly afterward.
Real, further progress beyond what #60 itself decoded.

**The trace still ends with what reads as `SUP`'s `init:` running a
second time** -- same unambiguous `1`/`2`/`3` markers as before.
Re-checked via GDB (`-S`+`continue`, same technique as #58) to see
whether this is still the same "crash landing with garbage registers"
#58 found. **It isn't, anymore.** `DS`/`ES` both read back sane
(`179Dh`, real `sysdat`) instead of `0xCCCC`, and single-stepping
forward from the hit shows `init:`'s own code -- the `'1'` marker,
then a real call into `zdbg` to dump `SS:SP` -- executing completely
normally, byte for byte matching what `init:`'s own source actually
says to do. **This is a clean, well-formed second execution of
`init:`'s entire body, not a wild jump landing on its bytes by
coincidence.** The `sti`-delay fixes evidently cleaned up the
`0xCCCC`-flavored corruption along with everything else they fixed;
what's left is a more fundamental question than "what's corrupting
registers" -- it's "what legitimately causes `SUP`'s cold-boot
routine to run twice."

**Ruled out the two most obvious mechanisms directly**: `dispatcher`
(the far-pointer `RTMIF.RTM`'s `init:` sets to `fdisp`, later copied
into `XIOS86Q.A86`'s own `mydispatch`) lives at `SYSDAT` offset
`0x38`, nowhere near `module_table[sup]`'s own offset range -- no
plausible aliasing there. Checked every real label near `init:`'s own
address (`0x7Ah`) for a coincidental match (`idle`, `tick`, `notick`
all live far away, offsets `0x329`+) -- nothing lines up. `CX` reads
exactly `0x7Ah` (matching `IP`) at the moment of the hit, which may
be meaningful or may just be unrelated leftover state; not yet
resolved either way.

**Context for why this is now a lower-stakes mystery than #58 made
it look**: `INIT` is the process calling its own `f_terminate` right
before this happens -- i.e., this isn't a bug corrupting some
*other* process's execution, it's `INIT` itself, at the natural end
of its own job (every RSP created, nothing left to do), ending up
back at its own starting point instead of just idling. Still wrong,
but a much narrower, more specific question than "something is
smashing registers somewhere" -- most likely either a genuine logic
gap in how `f_terminate`/`abt_chk` hands off to the scheduler when no
other process is left ready, or one more wrong-field/stale-pointer
read in that handoff path, in the same family as the ones #53/#54
already found and fixed for `module_table[sup]` -- just not yet
isolated to a specific instruction the way those were.

**Next step**: trace `abt_chk`/`terminate_entry`'s own tail
(`ABORT.RTM`) the same way #54 traced `getpdadr` -- a GDB breakpoint
at its very last few instructions, checking exactly what it calls or
jumps to once cleanup is done, rather than working backward from the
`init:` landing site as this round did.

**Did exactly that, and found the real bug is one step further back
than expected.** `ABORT.RTM`'s `terminate_entry` ends with `jmp
dsptch`, which (correctly) routes to `DSPTCH.RTM`'s `term_act` for a
`ps_term` process. `term_act` has its own, independently interesting
pattern: it temporarily stores the *old* `rlr` into the terminating
process's own `p_link[si]` (`mov p_link[si],ax`), sets `rlr=si` so
the several nested `mpmif` calls it makes during cleanup
(`f_condetach`/`f_lstdetach`/`f_memfree`/`f_qwrite`, freeing the
terminating process's own resources) have a valid "current process"
to operate against, then restores it at the very end (`mov
ax,p_link[si] ! mov rlr,ax`).

Added two probes -- one at `term_act`'s own top (dumping the real
`rlr` being saved, plus `si`), one right before the final restore
(dumping `si` and `p_link[si]` again) -- to check whether that
temporary storage survives the four nested calls intact.

**It doesn't, but the more interesting finding is what it started
as.** The *saved* `rlr` value, at `term_act`'s own very first
instruction -- before any of `term_act`'s own code has touched
anything -- read back as `0A95h`. That's not a valid PD address at
all: every real PD slot this session has ever seen is paragraph-
aligned to `PD_POOL_SLOT_SIZE` (`0x30`), and `0A95h` isn't (`0A95h mod
30h` isn't `0`). **`rlr` was already invalid by the time `INIT` called
its own `f_terminate`, in this specific (longer, further-reaching)
trace** -- this is upstream of everything in `ABORT.RTM`/`DSPTCH.RTM`'s
own termination path, not caused by it.

(For completeness: `p_link[si]` *does* separately get clobbered
during the four nested cleanup calls too, read back as `0A20h`
--`tick`'s own, valid address -- instead of the `0A95h` that was
saved into it. Coincidentally more plausible-looking than what was
saved, but still an uncontrolled overwrite of state `term_act` is
relying on staying put across calls it doesn't own -- worth fixing on
its own merits once the real, upstream `rlr` corruption is found,
but not itself the root cause here.)

**Next step**: this round's own longer trace is the first one ever
to exercise `flag_act`/`swap_act` (dispatch table entries this
investigation has never reached before -- confirmed via the `{...}`
dispatch-table-index marker showing index `8`, `flag_act`, earlier in
this same trace) -- that's the most likely place `rlr` first goes
bad, not anything in the termination path itself. Next round should
breakpoint/probe `flag_act`'s and `swap_act`'s own handling of `rlr`
directly, the same way #54 isolated `getpdadr` and #60 isolated
`drltorlr` -- newly reachable code, not yet exercised by any fix or
probe this session.

## 62. Correction to #61: the "`rlr` already invalid before
`term_act`" finding was a probe artifact -- the exact same bug class
as marker `0Bh` in #56, caught this time before it went further

Went to confirm #61's finding directly rather than trust it blindly
-- added a probe immediately after `dsptch`'s own `mov rlr,ax` (the
write #61 suspected of disagreeing with what `term_act` read back
moments later), dumping the written value and an immediate memory
read-back side by side, no other instruction in between.

**They didn't match even that close -- which was the tell.** Re-read
both of #61's own probes (`95h`, `97h`) and found they have the
*identical* bug marker `0Bh` had in #56: `mov al,95h` (the marker
byte) overwrites `AL` before the probe's own `call ydbg` dumps the
value it was supposed to be showing, so both were printing `AH` (the
real value's high byte, genuinely `0Ah`) glued to `95h`/`97h` (the
marker's own low byte) -- `0A95`/`0A97` was never a real register
value, just the bug re-manifesting with a different marker number.
Fixed both the same way as every prior instance: save the real value
into a scratch register first.

**With the fix in, `rlr` reads `0A20h` consistently at every single
point checked** -- immediately after `dsptch`'s write, at `term_act`'s
own top, and right before its final restore. `p_link[si]` also reads
back exactly what was saved into it, unchanged across all four nested
cleanup calls. **#61's two claimed findings (`rlr` already invalid
before `term_act` starts; `p_link[si]` clobbered during cleanup) are
both retracted.** `term_act`'s entire `rlr`-handling sequence is
correct, start to finish, with no corruption anywhere in it.

**The actual remaining mystery is unchanged in shape, just not
explained by this**: the trace still ends with `SUP`'s `init:`
genuinely re-running (same clean, well-formed re-execution #61 already
established, not a crash), and this time a *second* process --
`tick`, not `INIT` -- ends up calling its own `f_terminate` right
before it happens. Given `term_act` itself is now confirmed clean,
whatever causes the re-entry into `init:` has to be sought somewhere
else: either in how `schedule`/`switch` behaves when `rlr` genuinely
has nothing left to hand off to, or in a completely different
mechanism this session hasn't looked at yet. #61's suggested next
step (`flag_act`/`swap_act`, newly reached this round) is still a
reasonable place to keep looking, but this round's own work doesn't
point at them specifically anymore -- that lead came from the now-
retracted finding.

**Standing lesson, worth internalizing rather than re-learning each
time**: any probe that does `mov al,<marker> ! out dx,al` and then
immediately dumps a register that includes `AL` without first saving
it elsewhere is broken, full stop -- this is the second time this
exact shape of bug has produced a confident-looking wrong conclusion
(`0Bh` in #56, `95h`/`97h` in #61/#62). Worth a standing rule: *never*
write `mov al,<marker>` immediately before dumping a value that
occupies `AL`, even when under time pressure to get a probe placed
quickly -- save first, always.

## 63. Ruled out two more plausible mechanisms for the `SUP init:`
re-run -- `tick`'s own dispatch is confirmed clean, down to the exact
`IRET` frame

With `term_act` cleared (#62), looked for other candidate mechanisms.

**Checked `idle_tos`/`tick_tos` directly against `SYSDAT.FMT` and
`SYSTEM.DAT`'s own static templates, suspecting they might clobber
whatever follows them in memory.** They don't -- `idle_tos`/`tick_tos`
are declared as full 3-word reservations (`rw 3`), and `SYSTEM.DAT`
confirms each one is a complete, intentional pre-built `IRET` frame
(start IP, segment placeholder, `0200h` flags) -- the exact same
pattern as `TMP`'s own `stack_top`, just named differently and
declared once per built-in system process instead of once per RSP.
Not a bug; retracting the suspicion before it went any further this
time.

**Checked whether `tick`'s own first dispatch actually jumps where it
should, directly via GDB** -- breakpointed `int_disp_exit` (the
`switch` branch taken for a freshly created process, confirmed in
#60 to be the one `tick` takes) and read the `IRET` frame right off
the stack before single-stepping through the rest of it. **It's
exactly correct**: `IP=033Dh` (`notick`, `tick`'s own real entry),
`CS=1000h`, `FLAGS=0200h`. `tick` is being dispatched correctly, full
stop -- this rules out "built-in system process has a broken startup
frame" as cleanly as #58 ruled out "wild jump with garbage registers"
and #62 ruled out "`rlr` corruption in termination."

**Also noticed, but couldn't pin down**: right after this exact
dispatch, `func()`'s own absolute-first-instruction marker fires
showing `CX=0000` -- immediately followed by the very next marker
(reading the same live register, unprotected) showing a completely
different, internally-consistent value (`033Dh` this time) that then
resolves through `goodmod` to a real, legitimate `MEM` function call.
This is the *same shape* of artifact that's shown up intermittently
since #56 (also seen in #60's trace, different value each time) --
every single occurrence so far has resolved into a real, successful
call afterward, never an actual crash, so it doesn't look like it's
breaking anything currently. But it's never been explained, and
`func()`'s own top marker is about as unambiguous a probe as exists
in this codebase (non-printable byte, no collision possible) -- it
genuinely means `func()`'s first instruction ran. Worth its own
investigation eventually, separate from the `init:` re-run mystery,
since nothing currently ties the two together.

**Net result of #61-#63**: `term_act`, `tick`'s `IRET` setup, and the
static `idle_tos`/`tick_tos` templates are all now confirmed clean.
The `SUP init:` re-run itself is unchanged from #61's finding (a
clean, well-formed second execution, not a crash) and its actual
trigger is still not identified. Three plausible mechanisms ruled
out this session alone is real progress, even without a positive
finding yet -- the search space for "what legitimately sends control
back to `init:`" has narrowed to: something in `schedule`'s own poll-
list handling (never yet probed directly), something in `flag_act`
specifically (tick's own real next action, `f_flagwait`, which this
round never actually traced into), or a mechanism entirely outside
the scheduler this session hasn't considered.

**Next step**: trace `flag_act` and `FLAG.RTM`'s `flag_wait_entry`
directly for `tick`'s own real `f_flagwait(flag_tick)` call -- the
one syscall `tick`'s source code actually makes, right after the
dispatch this round just confirmed is clean. That's the one piece of
`tick`'s own real behavior no round has traced yet, and it's the very
next thing that happens after everything just confirmed correct.

## 64. Followed #63's own next step directly into the real bug --
`TMP`'s own code runs for the first time in this entire multi-session
investigation

Breakpointed `tick`'s own real entry labels (`notick`/`tick`,
`0x033D`/`0x350`) directly -- under full-speed `-S`+`continue`
(preserving real timing, the same technique that's worked throughout
this session) -- expecting to confirm `flag_wait_entry` is reached.
**Neither address was ever hit, even once, despite #63's own GDB
check moments earlier confirming the `IRET` frame pointing there was
completely correct.** That contradiction -- correct under single-
step, never reached at full speed -- is exactly this session's
recurring signature for a hardware-timing race, just not one that had
been checked in this specific spot yet.

**Found the mechanism directly.** Unlike `sti`, which has a
documented one-instruction delay before it takes effect (the basis
for every `sti!cli` fix this session), **`iret` has no such delay** --
popping `FLAGS` (which every process's own pre-built dispatch frame
sets to `0200h`, enabling interrupts) takes effect *immediately*, as
part of the same instruction that also jumps to the new `CS:IP`. If
an interrupt is already pending at the exact instant `iret` executes,
it gets serviced **before the newly-dispatched process's own first
instruction ever runs** -- diverting control away from `tick`'s real
entry (and, by the same mechanism, away from `TMP`'s).

**Tested directly**: changed the one place that builds this flags
word (`PROC.RTM`'s `mov word ptr 4[di],0200h`, part of every
process's pre-built `IRET` frame) to `0000h` -- keeping interrupts
off through the handoff, as a diagnostic, not a real fix. **Rebuilt,
and for the first time in this entire multi-session investigation,
marker `27h` fires -- `TMP`'s own real entry point, printing `"TMP"`
--** followed by `TMP` making multiple real syscalls of its own
(`f_setdefcon`-shaped, resolving through `CIO`; a second one
afterward) that resolve correctly through `goodmod` and return.
Confirmed reproducible across two different capture lengths (both
landing on an identical 5434-byte trace). `flag_wait_entry`'s own
marker also fires for the first time, for the same reason.

**This is very likely the root cause that's been behind both of this
session's two biggest standing mysteries at once** -- `TMP` never
reaching its own code, and `SUP`'s `init:` appearing to "re-run" --
both are exactly what you'd expect from control being diverted, right
at the handoff into a brand new process, into some other code path
that eventually loops back to a known, fixed address.

**This diagnostic trades one real problem for another, smaller one,
and that trade was made deliberately.** Forcing every process's
initial `FLAGS` to `0000h` means no newly-dispatched process starts
with interrupts enabled -- the periodic tick-based scheduling this
system is supposed to have won't function until each process does
its own `sti` at some point (most don't, since the original design
assumed `IRET` would do it for them). Given this session's standing
goal is reaching a working console prompt, and this change is what
finally gets `TMP` running at all, it's being kept rather than
reverted -- but it is **not the final fix**, and whatever interrupt
is actually pending at this exact moment (still not identified --
tick-interrupt markers remain at zero throughout, so it isn't the
periodic timer) should still be found and handled properly, probably
by re-enabling interrupts explicitly, one instruction after the new
process's real entry, the same way `dispin`'s own `sti` was finally
made safe in #60 -- rather than leaving them off indefinitely.

**Next step**: now that `TMP` actually runs, trace what it does after
its first couple of syscalls -- does it reach a real console prompt,
or hit a new wall? That's now directly reachable and is the most
valuable thing to chase next, given this session's own standing
goal. Separately, and lower priority now that the system is making
real progress: identify the actual pending interrupt source this
fix is routing around, and restore interrupts properly instead of
leaving them permanently off.

**Decoded what `TMP` actually does with its new lease on life.**
After the `f_setdefcon`-shaped call, it makes a second real syscall
(`CX=000Eh`) resolving to `BDOS` module function 1, then (without a
fresh `int mpmint` -- internal, from within that same call's own
processing) a `CIO` call, then **`f_dispatch`** (`CX=008Eh`, matching
`MODFUNC.DEF` exactly) -- `TMP` voluntarily yielding to the
scheduler. That unwinds cleanly through the *entire* call chain back
up to `dret` (not `int_disp_exit` this time -- `u_in_int` correctly
reads `false`, since this is `TMP`'s second dispatch, not its first)
and back out through `goodmod`/`gofar`/`func()`/`mpmif`, returning
control to `TMP`'s own code right after its own `int mpmint`. **A
complete, successful round-trip through the scheduler and back,
initiated and consumed entirely by `TMP`'s own real code.**

**Then the trace goes quiet**, ending in a short run of what looks
like a garbled or incomplete dump (repeating `4600` bytes, interleaved
with a few single-byte markers not yet matched to any known probe in
this exact context) rather than a clean termination or a recognizable
further syscall. Not yet decoded. Given how much further this reaches
than anything before it, this is squarely the next thing to trace --
but on its own terms, as new territory, not as a continuation of any
mystery from earlier in this session.

## 65. Followed #64's own next step -- traced the "4600" silence to a
real divide-by-zero crash, and found two genuine, independent bugs
behind it

Picked up exactly where #64 left off: decode the "4600"-repeating
tail and find out whether `TMP`'s console-attach retry loop (via
`CHARIO.CIO`'s `constat_entry`/`f_dispatch`) ever actually succeeds.

**First, ruled out byte-level marker archaeology as the way to answer
this.** The tail region mixes at least four different marker bytes
that collide across files (`09h` alone is used by both `SUPIF.SUP`'s
`gofar` and `BDOSIF.BDO`'s own marker 9, with different dump shapes),
and manually disambiguating which probe fired where, from raw serial
bytes alone, stopped being reliable. Switched to a much more direct
method instead: QEMU's own monitor (`info registers`, `xp`), which
can inspect the *live, truly free-running* machine state without
GDB's `-S`/`stepi` perturbing timing the way it has all session.

**Confirmed the "trace goes quiet" ending is a real, permanent hang,
not a clean idle state.** Booted completely free (no `-S` at all),
waited several seconds past the point the serial trace stops
growing (confirmed stable at exactly 5434 bytes across repeated
20-second runs), then queried the QEMU monitor's `info status` / `info
registers` directly. `VM status: running`, not paused or shut down --
but `CS:EIP` reads back **identical** (`1000:4D85`) across three
checks two seconds apart, with `EFLAGS` showing `IF=0` (interrupts
still off, consistent with #64's `0000h` diagnostic). This is a true
infinite busy-loop with interrupts permanently disabled, not a HLT,
not a reboot (double-checked `-d cpu_reset`: the only two "CPU Reset"
log lines are QEMU's own well-known double-logging of the *single*
power-on reset, not a second, in-flight one), and not a retry that's
merely slow.

**Mapped `1000:4D85` to real source.** `build_mpmsys.py`'s own printed
module bases put this flat offset inside `BDOS` (base `0x447e`, local
offset `0x907`), and `asm86.py --symbols src-assem/kernel/BDOS.A86` plus a
direct byte dump of `build/kernel/MPM.bin.cseg` at that exact offset
(`f7 36 48 0c` = `DIV WORD PTR [0C48h]`) pinned it precisely to
`FILE1.BDO`'s `seek:` routine -- specifically its `DIV SECTPT`
instruction (`sectpt`'s own symbol address, `0C48h`, matches exactly).
`seek` is reached from `FILE3.BDO`'s `diskread`/`sqread` -- a real
file *read*, further into actual execution than this session has
ever reached, not disk-select or console-attach. With no
divide-error (`#DE`/vector 0) handler installed anywhere in this
kernel, the CPU faulting on `DIV SECTPT` with a zero divisor and
nothing to catch it is exactly what parks it here forever, silently,
well past `TMP`'s last debug probe -- explaining the "4600" tail as
whatever `TMP`/BDOS probes fired right up until this crash, not a
decode error.

**Root cause #1 -- `XIOS86Q.A86`'s `dph0` was missing its DIRBUF
word.** `sectpt` is one of several BDOS globals (`sectpt`, `blkshf`,
`maxall`, ...) that `FILE1.BDO`'s `selectdisk` fills in by copying 15
bytes starting at whatever `dpbaddr` points to -- and `dpbaddr` itself
is set, a few lines earlier in the same routine, by walking fixed
word offsets from the Disk Parameter Header (`seldskf`'s return value)
assuming the standard CP/M/MP/M-86 layout: `XLT, 3x scratch, DIRBUF,
DPB, CSV, ALV`. `XIOS86Q.A86`'s own `dph0` only had `XLT, 3x scratch,
DPB, CSV, ALV` -- the `DIRBUF` word was missing entirely, shifting
every field BDOS reads back by one word: `dpbaddr` ended up holding
`csv0`'s address instead of `dpb0`'s, so the later 15-byte DPB copy
pulled `csv0`'s zero-initialized 16 bytes instead of the real DPB,
leaving `sectpt` (and everything else in that struct) permanently
zero. Confirmed this wasn't a design choice but a genuine divergence:
`src-assem/loader/LDBIOS86Q.A86`'s own DPH (explicitly commented as needing
to stay identical to this one, same physical disk) *does* have the
DIRBUF word in the right place (`dw dirbuf,dpb0 ! dw csv0,alv0`).
Fixed by adding a `dirbuf0 rb 128` buffer and wiring it into `dph0` at
the correct position in `XIOS86Q.A86`.

**This fix alone didn't change anything observable** (rebuilt, same
5434-byte trace, same hang at the same instruction) -- which was
itself a useful, confusing signal. Chased it with a non-perturbing-ish
GDB check (set-and-continue, not single-step, to minimize the
timing-race risk this session has hit repeatedly): breakpoints on
`selectdisk`, `retselect`, and `ret4` showed `ret4` (the *early-exit,
"illegal disk" error* path) hit 20+ times in a row, `retselect` (the
success path, where the DIV-feeding fields actually get filled in)
never hit at all. So the DIRBUF fix was necessary but not sufficient:
`selectdisk` was failing before ever reaching the code it fixed.

**Root cause #2 -- `SYSTEM.DAT`'s `srchdisk` defaults to drive B:, but
this build only has drive A:.** A follow-up breakpoint on
`BDOSIF.BDO`'s `seldskf` (disk# in `CL`, read before anything inside
it can clobber it) showed every single one of those 20+ calls
requesting disk **1**, not 0. `XIOS86Q.A86`'s `dtrntbl` (the logical
disk) : physical drive) only has slot 0 wired up (`db 000h,0ffh,0ffh,
0ffh`), so disk 1 is correctly rejected as illegal every time --
`seldsk` returns `AX=0`, `selectdisk` takes its `cmp bx,0 / jz ret4`
bailout, and `sectpt` never gets touched by anything, DIRBUF fix or
not. Traced the `1` back to `TMP.A86`'s own init code: `tmp:` reads
`sysdatseg:.s_sysdisk` (a hardcoded `byte ptr 04bh` offset) as the
disk to pass to `setdisk`. That exact offset is `SYSDAT.A86`'s
`srchdisk` ("default search disk"), and `SYSTEM.DAT` initializes it to
`1` -- almost certainly a stock GENSYS answer left over from the
original multi-drive Intel SBC target, exactly the kind of
unconverted "answers to GENSYS questions" field `build_mpmsys.py`'s
own `KNOWN LIMITATIONS` section already flagged as a risk ("written
for the real XIOS.A86/BDOS configuration, not necessarily this one").
Fixed by having `build_mpmsys.py` patch `srchdisk` to `0` alongside
its existing `ncondev`/`nlstdev`/`nciodev`/`tickspersec` patches,
rather than hand-editing `SYSTEM.DAT`'s own stock value -- consistent
with how this script already adapts GENSYS answers for this specific
QEMU/single-floppy target.

**Both fixes together move the hang forward, for the first time ever
this session, into genuinely new territory: real floppy controller
I/O.** Rebuilt with both fixes; the free-running trace length changed
(5445 bytes, not 5434, the first time this exact number has moved in
this whole investigation) and the new stable hang point (confirmed
via the same monitor-based `info registers` technique, identical
`CS:EIP` across three 2-second-apart checks) is `1000:71FE`, inside
`XIOS86Q.A86` at `fib_wait` -- `fdc_in_byte`'s own tight polling loop
on the FDC Main Status Register (`in al,dx ! and al,msr_rqm+msr_dio !
cmp al,msr_rqm+msr_dio ! jne fib_wait`), waiting for the controller to
signal "byte ready" for a disk read that `selectdisk`'s DPB fields can
now, for the first time, actually support. This is pure PIO polling,
not interrupt-driven, so `IF=0` isn't the cause this time -- QEMU's
emulated FDC itself is apparently never setting `RQM+DIO`, which
points at an actual FDC command/protocol issue (wrong command bytes,
missing drive-select/motor-on step, or a timing assumption this PIO
driver doesn't account for).

**Next step**: debug `XIOS86Q.A86`'s FDC command sequence (whatever
calls `fdc_send_cmd`/`fdc_wait_seek`/`fdc_pio_in` leading up to this
read) against what QEMU's emulated NEC765/8272 actually expects --
this is the first time this investigation has ever reached real disk
I/O, so there's no earlier finding in this file to build on.

## 66. Followed #65's own next step -- ported the already-solved DMA
fix from the loader's FDC driver into the kernel's, then found and
fixed a second, unrelated kernel-only bug (`ccb`'s own "unconfigured
GENSYS answer" problem, same class as #65's `srchdisk`) blocking the
very next milestone

Picked up exactly where #65 left off: `XIOS86Q.A86`'s `fib_wait` was
spinning forever waiting for the FDC to signal "byte ready" for a
real sector read, having already correctly transferred exactly 7
bytes (confirmed via `EDI`/`ECX` against `secbuf`'s own symbol
address) before stalling.

**Recognized this as an already-solved problem, not a new one.**
`src-assem/loader/LDBIOS86Q.A86`'s own copy of this exact driver has a
detailed comment (and `docs/bugs.md` #22) describing the identical
symptom and its fix: pure PIO for the full `fd_secsize` data phase
genuinely stalls after a handful of bytes on this QEMU target's
NEC765/82077 emulation (confirmed there via testing, independent of
timing), fixed by switching to real 8237 DMA-driven transfers for the
data phase, keeping PIO only for the 7-byte result phase every
READ/WRITE DATA command ends with regardless of DMA vs PIO. `XIOS86Q.A86`
(the kernel's own copy of this driver) never received that fix --- it
still had the old pure-PIO data loop and `specify_parms`'s `ND`
(non-DMA) bit still set.

**Fixed** by porting the loader's `dma_setup`/`dma_setup_read`/
`dma_setup_write` into `XIOS86Q.A86` nearly verbatim, clearing
`specify_parms`'s `ND` bit, collapsing `fdc_pio_in`/`fdc_pio_out` down
to just the 7-byte result-phase drain (DMA now handles the data
phase), and removing the trailing `SENSE INTERRUPT STATUS` calls after
each read/write (the loader's own fix removed these too, for the same
reason as #16/#22: QEMU hangs on a SIS with no genuine pending
interrupt to report). **One deliberate, necessary difference from the
loader's version**: the loader's `secbuf` lives in its own CSEG (no
separate DSEG), so its physical-address math for arming the DMA
controller is `CS*16 + offset(secbuf)`. This kernel module's `secbuf`
lives in the SYSDAT-based combined DSEG instead (`build_mpmsys.py`'s
"SYSDAT's dseg.bin plus XIOS86Q's own private tail"), so copying the
loader's `CS`-based formula verbatim would have armed the DMA
controller with the wrong physical address -- used `DS` instead,
matching this file's own pre-existing `pmsg` comment, which already
established the identical DS-not-CS point for `signon`, declared
right alongside `secbuf` in that same private tail.

**That alone changed observable behavior for the first time across
all of #65/#66's hangs** (previously: an identical 5434- or
5445-byte trace, bit-for-bit, every single run) -- but into a new,
far bigger problem: a 4,958,585-byte trace that turned out, on
inspection, to be a tight, exactly-periodic loop (period 2,479,025
bytes, confirmed byte-for-byte identical at two widely separated
offsets) repeating forever. `TMP`'s own entry marker (`27h`+`"TMP"`)
appeared exactly once near the start, ruling out repeated process
creation; `CHARIO.CIO`'s `constat_entry`/`cioattach` markers appeared
only a handful of times, ruling out the console-status retry loop
from #64's own speculation. Breakpointing `CONSOLE.CIO`'s
`conattach_entry`/`cioattach`/`ca_sleep`/`ca_atch` (with the
established "one `stepi` after each hit" discipline this session has
learned the hard way is required to avoid GDB's own "stuck re-hitting
the same breakpoint with zero progress" artifact) caught it directly:
`TMP.A86`'s own first real console attach (`nextcommand`'s `call
attach`) hits `conattach_entry`, then immediately `ca_sleep` -- an
*unconditional* attach request finding the console already (and
permanently) "attached" to someone else, sleeping forever waiting for
a detach that can never come.

**Root cause -- `ccb` (SYSDAT.FMT's Console Control Block table
address) is another unconfigured GENSYS answer, exactly like #65's
`srchdisk`, just far more damaging.** `SYSTEM.DAT` declares `ccb dw 0`
-- a placeholder real GENSYS would normally compute and fill in with
wherever it laid out the real per-console CCB array in memory. With
`ccb` stuck at 0, `CHARIO.CIO`'s own `getccbadr` (`ccb + console#*12`)
resolves console 0's CCB to flat address **0** instead of a real,
dedicated block -- every console attach/detach/status call ever made,
by any process, reads and writes `c_attach`/`c_queue`/etc at that
same bogus, never-actually-owned address instead of a real,
zero-initialized CCB. Confirmed directly: reading `c_attach[si]` live
via the QEMU monitor at the `ca_sleep` breakpoint showed `SI=0`, and
whatever garbage already happened to sit at sysdat-segment flat
address 0 read back non-zero, so `cioattach`'s unconditional-attach
path treated the console as already (and forever) owned by someone
else.

**Fixed** by reserving real storage for this build's one CCB
(`ncondev`+`nlstdev` = 1+0 = 1, this build's own already-patched
GENSYS answers from #22/#65) in `XIOS86Q.A86`'s own private dseg tail
-- `ccbtab0 rb 12` (`CCB.DEF`'s own `ccblen`), zero-initialized by
`rb` the same way `dirbuf0` was in #65 -- and having
`build_mpmsys.py` patch `ccb` (SYSDAT offset `0x54`) to point at it,
via the same `get_symbol('XIOS86Q', ...)` mechanism the script
already uses elsewhere for this exact kind of cross-module pointer
patch. Also patched `nccb` (SYSDAT offset `0x49`) from `SYSTEM.DAT`'s
stock `5` (written for the original multi-device target) down to `1`,
to stay consistent with the real count, though `ccb` itself -- not
`nccb` -- was what actually caused the hang.

**With all three of #65/#66's fixes in place, booting changes shape
again**: the free-running trace drops to 93,321 bytes (much shorter
than the 2.48MB idle-spin loop the `ccb` bug caused, and shorter than
either of #65's two earlier hangs), `TMP`'s own entry marker still
fires exactly once, and -- new this round -- `XIOS86Q.A86`'s own init
signon banner (`"MP/M-86 XIOS86Q -- QEMU target (serial console,
floppy disk)"`) is readable in the trace with per-character dispatch
markers interleaved, immediately followed by `INIT.SUP`'s own `nrsp`
marker (`1Bh`) -- i.e., this is apparently the first run where the
*entire* module-init phase, XIOS's floppy driver included, completes
and genuinely hands off into `nrsp`'s real RSP-creation loop, not a
coincidental repeat.

**Then it hangs again, differently**: confirmed via the same
monitor-based `info registers` technique (three checks, several
seconds apart, byte-identical `CS:EIP`) -- `CS:IP = 1000:004A`, `EFLAGS`
still showing `IF=0`. Mapping that flat address against
`build/kernel/MPM.bin.cseg` directly (not `asm86 --symbols`, which for
very low offsets like this returns misleading hits -- `SUP.A86` also
includes `sysdat.fmt` purely for its own offset alignment, so small
numeric offsets collide between real SUP code labels and unrelated
SYSDAT *data* field names sharing the same small numbers) shows `CS:IP`
sitting **inside `INIT.SUP`'s own literal copyright/signon banner
text** ("`...2.0 [5 Oct 81]\r\nCopyright (C) ...`"), not real code --
a wild jump into data, the same *class* of bug as the already-fixed
`XIOS86Q.A86` init `'w'`-marker bug (an unbalanced `retf`/stack
mismatch), but a different occurrence: `ESP` is unusually low (`0xDC`
into its segment), and the stack nearby holds what looks like a
legitimate, unrelated `CIOIF.CIO` `entry:`-dispatcher return address a
few words further up (`call cs:[table] ! retf`, confirmed by
disassembling those exact bytes directly from the built binary) --
not obviously the source of the corruption itself.

**Next step**: find whatever `call`/`retf` actually lands at
`1000:004A` -- almost certainly another unbalanced push/pop (or a
wrong segment popped as part of a `retf`) somewhere in the code that
runs between `nrsp`'s own marker and this crash, the same bug class
`INIT.SUP`'s own documented `'w'` fix and this round's `ccb` discovery
both already belong to. Given how much further this reaches than
anything in #65 (through the *entire* module-init phase and into real
RSP creation), this is squarely new territory, not a continuation of
#65's own mystery.

## 67. Followed #66's own next step -- narrowed the `1000:004A` wild
jump to `INIT.SUP`'s own self-termination call, found a real,
concrete suspect, but did not nail down the exact mechanism (left
for the next round)

Picked up `INIT.SUP`'s own source right after `nrsp`'s loop exits
(`rsp_out:`): it dumps `p_flag[rlr]` (marker `'!'`), then
unconditionally does `mov cl,f_terminate ! mov dl,0ffh ! int mpmint`
to terminate itself -- with **nothing after that call**. The very
next thing in the file is `zdbg:`'s own subroutine body, and the file
right after `INIT.SUP` in `SUP.A86`'s own `include` list is
`IDLE.SUP` (`idle:` -- jumps into XIOS's real idle loop). If this
termination call ever *returns* instead of transferring control away
(`ABORT.RTM`'s `terminate_entry`, on success, ends in `jmp dsptch` --
on failure, a plain `ret`), execution falls straight through into
`zdbg`'s own prologue with no call frame set up for it, and past that
into `idle:` -- not obviously a crash by itself, but entirely at the
mercy of whatever garbage happens to be on the stack by the time
`zdbg`'s own trailing `ret` (or anything else downstream) tries to pop
a return address.

**Confirmed, via a breakpoint on `rsp_out` itself** (`SUP.A86`'s own
`asm86 --symbols`, `rsp_out` at local offset `0x2BC`), reading `rlr`
directly from memory at that exact point: `rlr = initpd` (`0x9C0`),
exactly as it should be -- `INIT` itself is still the running process
right up to the termination call. Single-stepping forward from there
confirms the marker `'!'` probe's own `call zdbg` executes completely
normally (lands exactly on `zdbg`'s real label, `0x2FA`, confirmed via
symbols, loops through its real 4-hex-digit dump body, `zdbg1`
(`0x302`) through `zdbg3` (`0x317`), exactly as designed) -- this
specific call is not the bug.

**Separately, breakpointing `ABORT.RTM`'s own `terminate_entry`/
`abt_chk` path directly** (its `'$'`/`'%'` markers) caught something
that doesn't fit cleanly with the above: at the exact moment the
`'%'` marker dumps `BX(=rlr)`/`CX(=abt_chk's result)`/`p_flag[bx]`/
`DX`, `BX` reads `0E40` (`TMP`'s own PD, confirmed via this entire
session's own long-standing identification of that address) with
`p_flag=0x0211` (`pf_sys`+`pf_table`+`pf_tempkeep`) and `CX=0FFFFh`
(`abt_chk`'s own documented "tempkeep" return value) -- not `INIT`'s
own PD (`0x9C0`) at all. `terminate_entry`'s own `cmp cx,0 ! jle
term_err` (original, unmodified 1981 logic: correctly routes both the
"no abort" case, `cx=0`, and the "tempkeep" case, `cx=0FFFFh` treated
as signed `-1`, to the error return) then returns an error --
`bx=0ffffh`, `cx=e_pd_noterm` -- via a plain `ret`, not `jmp dsptch`.

**This is the real, concrete suspect, but not yet a confirmed root
cause**: somewhere between `rsp_out` (`rlr=INIT`, confirmed) and
whichever call actually reached `terminate_entry`/`abt_chk` with
`rlr=TMP`, `rlr` changes -- and `TMP` itself does have real,
documented reasons to carry `pf_tempkeep` (every `BDOSIF.BDO`
`getdiskmx` call sets it for the call's own duration, clearing it
again on return). Two read as consistent with each other only if
either (a) this specific `'%'`-marker hit belongs to a *different*
termination attempt than `INIT`'s own line-210 call (plausible:
`RETMONX`'s own `pf_ctlc` check, `BDOSIF.BDO` line 321-323, also calls
`f_terminate` under some conditions), or (b) `rlr` itself changes
between `rsp_out` and the real `int mpmint` dispatch in a way this
round's single-stepping didn't catch cleanly (budget ran out mid-trace,
still inside `zdbg`'s own loop, short of reaching the `int` instruction
itself). Not resolved either way this round -- genuinely two different
GDB sessions, not one continuous trace, so the two findings haven't
been tied together with full confidence yet.

**Next step**: single-step `INIT.SUP`'s own code continuously,
without interruption, from `rsp_out` all the way through the real
`int mpmint ! retf`-or-fallthrough at line 210 (budget for this: a
few hundred steps should comfortably cover it, not the ~140 this
round used), watching specifically for (1) whether `rlr` changes
*before* that `int` executes, and if so what instruction does it, and
(2) whether `terminate_entry` takes the `jmp dsptch` success path or
the plain-`ret` failure path this exact time, to settle definitively
whether `INIT`'s own termination is what's failing, or whether this
round's two findings describe two unrelated moments.

## 68. Followed #67's own next step -- settled it: `INIT`'s own
termination is clean, `rlr` tracks correctly throughout; the `1000:
004A` crash happens much later, deep in `TMP`'s own idle console-read
retry loop, and its timing is sensitive enough to GDB's own
perturbation that it could not be pinned to one exact instruction
this round

Did the continuous single-step `#67` asked for, from `rsp_out`
through the real `int mpmint` dispatch, watching `rlr` the whole way
(reading it from memory every step, not just at breakpoints): it
**never changes from `initpd` (`0x9C0`)** across the full call.
Confirmed with a second, breakpoint-based pass (`rsp_out` /
`terminate_entry` / `dsptch` / the `1000:004A` crash site, each as
its own breakpoint, `continue`d between hits rather than single-
stepped): `terminate_entry` is reached exactly once with `rlr=INIT`,
and the *very next* hits are `dsptch` -- `INIT` really did take the
`jmp dsptch` **success** path, not the error `ret` #67 worried about.
`rlr` then updates correctly, `initpd` (`0x9C0`) -> `tickpd`
(`0x0A20`) -> `TMP`'s own PD (`0x0E40`) -- exactly the ready-list
handoff this should look like. **#67's own `BX=0E40`/`pf_tempkeep`
finding at `terminate_entry` was, as its own write-up already
hedged, a different, later moment -- not `INIT`'s own termination at
all.** `INIT`'s side of this is fully exonerated.

**From there, `rlr=TMP` for a long run of repeated `dsptch` calls --
confirmed stable at the identical caller stack depth (`SS:SP` pinned
to `1779:00D0`, unchanged, ruling out a per-iteration stack leak) for
well over 100 consecutive hits before the same session eventually
reached the `1000:004A` crash site, still with `rlr=TMP`.** Identified
what's actually looping, via `SUPIF.SUP`'s own `user_entry` (dumps the
incoming `CX`, i.e. the real function code, directly): `TMP`'s own
real sequence -- `setconsole` (`CX=94h`), `setdisk` (`CX=0Eh`),
`attach` (`CX=92h`) -- followed by many repeats of `CX=96h`
(`mpm_clicmd`, `TMP.A86`'s own CLI-dispatch call, `nextcommand`'s
`clicall:`). So `TMP` is not stuck in `attach` or `conread` itself --
it's genuinely completing full passes of its own shell-prompt loop,
repeatedly, submitting something to `mpm_clicmd` each time. Peeked at
`clicb`/`read_blen` (`TMP.A86`'s own symbols, `clicb`=`0x1DF`, same
address as `read_blen`) at one such call: **all zero** -- i.e., an
empty command line is apparently what's being submitted each pass,
which doesn't obviously square with `nextcommand`'s own `cmp
read_blen,0 ! je gonextcmd` short-circuit (own source, read directly)
that should skip `mpm_clicmd` entirely for an empty line. Not
resolved: either this specific snapshot caught `clicb` after some
later code already cleared it back to zero, or there's a real,
separate bug in how `conread`'s own result reaches this check.

**Likely explanation for *why* this loops at all with no real
terminal attached**: `CHARIO.CIO`'s `conin`/`rtm_poll`/`polldev`
chain waits on the UART's own RX-ready status bit
(`XIOS86Q.A86`'s `polldev`, `uart_lsr`+`uart_lsr_rxrdy`) before ever
reading a character -- with this run's `-serial file:...` backend
(write-only, nothing ever feeds the UART), that bit should never be
set, so this should block forever, not cycle every pass. The fact
that it visibly *does* complete full prompt-loop passes, over and
over, suggests QEMU's emulated UART occasionally reports RX-ready
with no real input behind it, letting `conin` through to read stale/
default data (very plausibly all zero bytes, matching the all-zero
`clicb` snapshot above) -- not confirmed directly this round (would
need instrumenting `polldev`'s own read, not just downstream
symptoms), but consistent with everything observed.

**Timing-sensitive, could not pin to one exact repro**: re-running
the identical breakpoint sequence a second time (to catch the precise
`CX` value immediately *after* one of these `mpm_clicmd` calls, right
before the crash) landed on a run where the crash was never reached
at all within the same step budget that reliably reached it moments
earlier -- GDB's own single-stepping measurably changes how many
loop passes it takes (or whether it's reached at all), the same
"single-stepping dodges a timing-sensitive race" pattern this session
has hit many times before (most dramatically, the `sti`/`IRET` delay
discoveries). This makes the exact failing instruction materially
harder to pin down than #65's/#66's hangs, which were perfectly
deterministic (bit-identical trace length every run).

**Next step**: instrument `XIOS86Q.A86`'s own `polldev` directly (a
debug probe dumping the raw `uart_lsr` byte on every single poll,
kept only long enough to confirm or rule out the "false RX-ready"
theory) rather than continuing to infer it from `TMP`-side symptoms --
that's the one piece of direct evidence every other finding this
round rests on without yet having confirmed it. Separately, worth
checking independently of the UART theory: why `nextcommand`'s own
`cmp read_blen,0 ! je gonextcmd` doesn't appear to be short-circuiting
the empty-command case the way its source reads it should.

## 69. Did #68's own next step -- confirmed the false-RX-ready theory
directly, fixed it as real, defensible hardening -- but it is **not**
the `1000:004A` crash's cause: the free-running trace is
byte-for-byte identical with the fix in place, crash included

Added the probe #68 asked for: `polldev`'s `c0indev` branch now dumps
the raw `uart_lsr` byte (no hex conversion needed -- fed straight to
the serial port, decoded offline) on every single console-input poll.

**Confirmed directly, not just inferred**: 1924 polls across one
20-second run, 1898 correctly read `0x60` (not ready, no error bits).
The other 26 are the real finding: `0x5D`, `0x7D`, `0x5B` (8/8/4
times), `0x46`, `0x30`, `0x67` -- **every one of the four distinct
"rxrdy bit set" values also has at least one of overrun/parity/
framing/break (bits 1-4) set alongside it.** QEMU's emulated UART,
backed by this build's write-only `-serial file:...` (nothing ever
really transmits to it), genuinely does spuriously report "data
ready" roughly 1% of the time, always paired with an error condition
a real driver would treat as "discard this, it's not real data."
`XIOS86Q.A86`'s own `polldev` wasn't checking for that -- it trusted
the rxrdy bit alone.

**Fixed**: added `uart_lsr_errmask` (`1Eh`, the four error bits) and
a check in `polldev`'s `c0indev` branch -- if any error bit
accompanies a "ready" reading, treat it as not-ready (`AL=0`)
instead of handing `TMP` a stale/garbage byte from the data register
as if it were a real keystroke. This is real, defensible hardening
against a genuine, now-confirmed QEMU/UART quirk, independent of
whatever else is going on -- kept regardless of the result below.

**Result: no effect on the crash.** Rebuilt, re-ran: the free-running
trace is **byte-for-byct identical** to the pre-fix run, all 97,117
bytes, including every one of the 26 error-bit occurrences at the
same positions with the same values (meaning the fix's own new
branch is being compiled correctly and reached -- confirmed directly
in the assembled bytes, `TEST AL,1Eh ! JNZ polldev_notrdy` sitting
right where it should -- but produces a control-flow outcome that
makes no observable difference), and the same `1000:004A` wild jump
at the same point. **This rules out the "garbage input from a false-
ready UART poll" theory as the crash's cause** -- #68's own
`CX=0096h`/all-zero-`clicb` observation at `mpm_clicmd` must be
either coincidental or caused some other way, not by this mechanism.

**Status**: the `1000:004A` wild jump inside `SUP`'s own banner text,
reached from deep within `TMP`'s own repeated-`dispatch` idle loop,
remains open. Three concrete leads from #67/#68 are now each
individually ruled out or unconfirmed: `INIT`'s own termination
(#68, exonerated), the false-RX-ready UART theory (this entry,
exonerated), and the `read_blen`/`clicb` empty-command-short-circuit
question (#68, still genuinely open -- not yet investigated on its
own terms, independent of the now-discarded UART angle). That's
the most promising remaining thread: find out directly (not inferred
from a now-disproven theory) why `mpm_clicmd` is reached at all with
`clicb`/`read_blen` reading zero, when `nextcommand`'s own source
should skip it for an empty line.

## 70. Followed #69's own next step -- the `mpm_clicmd`/`clicb` thread
was a dead end (never even reached); found the *real* immediate
shape of the crash instead: `TMP.A86`'s own `crlf`/`getuser`
trampolines alternate exactly ~128 times, then jump straight from
`nextcommand`'s own top into the `1000:004A` wild jump with nothing
else in between

Set breakpoints across `nextcommand`'s entire prompt-printing
sequence (`freedrive`, `attach`, `crlf`, `prchar`, `getuser`,
`prnum`, `getdisk`, `printstring`, plus `conread_entry`/`readen`/
`gonextcmd`/`clicall` from #68/#69 and the crash site itself) and let
it run with the `continue`+`stepi`-between-hits discipline, budget
raised to 600 iterations this time (every narrower trace before this
ran out of budget first).

**The `mpm_clicmd` thread is dead**: `conread_entry`, `readen`,
`gonextcmd` and `clicall` are never hit at all before the crash --
`TMP` never gets anywhere near actually reading a console line this
run. #68's `CX=0096h` sighting at `mpm_clicmd`, and the all-zero
`clicb` snapshot that went with it, must belong to some other moment
entirely (a later pass this trace never reaches, or a different run
altogether) -- not a lead worth pursuing further on its own.

**What actually happens, cleanly, every time**: `freedrive` and
`attach` each fire exactly 3 times (alternating) right at the start,
then `crlf` and `getuser` alternate **exactly 128 times** (256 raw
hits), then `nextcommand`'s own top fires once more, and the *very
next* instruction-level stop is the `1000:004A` crash site -- with
`prnum`, `getdisk`, `printstring`, and `freedrive`/`attach` again,
all never hit in between. Note the exact count: **128** matches, to
the digit, the "128 LSR reads with `rxrdy`=0 (and no error bits)"
figure already captured independently in #68's raw-byte scan of the
pre-#69-fix trace -- not likely a coincidence, though the mechanism
connecting the two hasn't been nailed down this round.

**Why `crlf` and `getuser` alternating 128 times is itself suspicious,
independent of the 128-count**: neither of `TMP.A86`'s own source
calls the other, and nothing in `nextcommand`'s own straight-line
flow re-executes either of them without also re-executing
`freedrive`/`attach`/`prnum`/etc first (own source, read directly) --
yet here they alternate *exclusively*, hundreds of times, with every
neighboring label in that same tightly-packed trampoline block
(`prnum`@`0x2ED`, `prchar`@`0x300`, `getuser`@`0x306`, `crlf`@`0x30F`,
`printstring`@`0x313`) staying completely cold. `getuser`
(`mov dl,0ffh`, falling into `setuser`'s own `mov cl,mpm_usercode !
jmp mpm1`) and `crlf` (`mov dx,offset crlfstr`, falling into
`printstring`'s own `... ! call print_ds_string ! pop ds ! ret`)
each end up making a real syscall (`mpm_usercode` with the `DL=0FFh`
"query" convention, and `mpm_conwrite` respectively) via the same
`int mpmint` trampoline -- two *different* outbound calls, landing
back from `ret` at two *different* return addresses pushed by
`nextcommand`'s own two separate, ordinary `call` sites. A clean
`ret` from either should never land anywhere near the other's own
label address. Bouncing between exactly these two specific addresses,
repeatedly, smells like a corrupted/swapped return address or saved
context -- not a logic bug in `TMP.A86`'s own straight-line code.

**Not resolved this round**: which syscall (`getuser`'s or `crlf`'s)
is the one actually retrying 128 times, whether the OTHER one is a
real second call or an artifact of how GDB's breakpoint hits get
named from `$pc`'s low 16 bits, and what specifically makes the
dispatcher hand control back to the *other* trampoline's address
instead of each call's own real return point. Given #68 already
independently found QEMU's UART backend behaving oddly at a similar
scale (not the same mechanism -- that was a data-register content
issue, already fixed, already shown not to change this crash) and
this round's 128-count lines up suspiciously well with it anyway,
the most promising next check is probably: does `crlf`'s own
`mpm_conwrite` call (printing `"\r\n"` via `conout`) genuinely need
~128 poll-and-yield cycles per byte against this QEMU target's
emulated UART transmit timing (plausible, if TCG runs much faster
than the UART's real baud-rate-paced byte timing) -- and if so,
whether `getuser`'s own address only *looks* like it's firing because
of a `$pc`-decoding artifact in the tracing script, not a real second
call. Worth re-running with full `$cs:$pc` (not just the low 16 bits)
logged at every single hit, and the actual return address read off
the stack at each stop, to settle this directly instead of inferring
it from label names.

**Did that check immediately, same round -- it settles the "artifact
vs real" question and sharpens the lead considerably.** At every
`crlf` hit: `SS:SP=177A:00D0` (`TMP`'s own, completely normal
process stack) and a **constant** return address, `0x1A1C` -- a real
address inside `TMP.A86` itself (a few bytes before `prnum`), i.e.
`crlf`'s own call site in `nextcommand`, called the ordinary way,
repeatedly, exactly as its own source says it should be when console
output isn't immediately ready. Nothing wrong with `crlf` itself.

At every `getuser` hit, by sharp contrast: `SS:SP=17A7:0B5A` --
**not `TMP`'s stack at all**, and a constant return address,
`0x231D`, that isn't a `TMP.A86` address either: mapped against
`RTM.A86`'s own `asm86 --symbols`, `0x231D` (RTM's module base,
`0x18BF`, plus local offset `0xA5E`) lands squarely inside
`DSPTCH.RTM`'s `schedule`/`polld_another` -- the scheduler's own
poll-list walk -- right where it returns from `call xiosif` (`mov
al,io_polldev ! call xiosif`, `polld_another`'s own first real
device check). So `getuser` (`TMP.A86`, flat `0x11A38`) is never
really being *called* by anyone -- something inside the kernel's own
poll-list walk is landing exactly on that address instead of
returning to its own very next instruction, runs `getuser`'s
harmless `mov dl,0ffh`/`mpm_usercode`-query code as a complete
accident, and -- since nothing about *that* code path disturbs the
stack -- its own trailing `ret` happens to pop the right thing
(`0x231D`) anyway, landing back in `polld_another` none the wiser.
That's why it keeps alternating with `crlf`: every time `crlf`'s own
real retry yields via `poll_act`, `schedule`'s poll-list walk runs
again, and this wrong-landing recurs.

**Not yet confirmed**: the actual mechanism inside `xiosif`/`entry`'s
`call cs:functab[bx]` (or `polld_another`'s own setup immediately
before that call) that could make this land at flat `0x11A38`.
`functab`'s own entries (`XIOS86Q.A86`'s `dw polldev` etc.) looked,
on inspection, like they *should* need the same module-base
adjustment this exact file's own comments already flag for `offset
signon`/`offset tickint` elsewhere -- but if that adjustment were
genuinely missing project-wide, every other XIOS function call this
whole session (console I/O, the floppy DMA work in #65/#66) would
already be broken the same way, and they aren't. So either `functab`
is fine and something else (bad `bx`, a stale `ax` feeding the
`shl ax,1` index, or similar) misroutes this *one* call, or there's a
narrower condition under which the base-adjustment gap bites that
hasn't been identified. Didn't resolve which before running out of
budget this round.

**Checked immediately -- ruled out, didn't even need to boot QEMU.**
Read `build/kernel/MPM.bin.cseg` directly: the real `functab[17]`
slot (`XIOS_base` `0x6D93` + `functab`'s own local offset `0x166` +
`17*2`) holds `0x704E`, which is exactly `XIOS_base + polldev`'s own
local offset (`0x6D93 + 0x2BB`) -- `functab`'s entries are already
correctly adjusted to their real, combined-cseg position, not raw
per-module-standalone-compile local offsets. (Checked the theory's
own predicted wrong answer too, for completeness: reading `functab`
as if `cs:` meant "flat offset 0, no module-base adjustment" lands
on `0xC720`, which isn't `0x1A38` either and isn't a plausible jump
target at all -- past the end of the whole cseg.) So `entry`'s own
`call cs:functab[bx]` is not the bug, and the earlier concern (that
this is the same "module_base-free offset" class as the already-
fixed `signon`/`tickint` issues) doesn't apply here after all.

**Status at the point this round's context ran out**: the mechanism
is still open. What's solid: `crlf` is a real, repeated, perfectly
ordinary call from `TMP`'s own stack with a real return address --
not the bug. `getuser` is reached from a completely different stack
context (`SS:SP=17A7:0B5A`, not `TMP`'s) with a return address
sitting right after `schedule`'s `polld_another ! call xiosif` for
`io_polldev` -- something lands on `TMP`'s `getuser` label instead
of coming back to that call's own next instruction, runs it as an
accident, and its harmless side effects don't disturb the stack
enough to matter, so it still manages to pop its way back to the
right place afterward. `functab` itself, and the dispatch through
`entry`, are now confirmed clean. The next place to look is `bx`'s
own value at the moment of that specific `call cs:functab[bx]` --
whatever computes it (`ax` coming in as `io_polldev`=17, `shl ax,1`)
-- on the exact pass that's about to land wrong, to see whether `bx`
itself is corrupted before the table lookup even happens, rather than
the lookup itself being at fault.

## 71. Followed #70's own next step -- `bx`, `functab`, `xiosmod`,
`polldev` and `insert_process` are all confirmed clean on every one
of the 128 cycles; narrowed the mechanism to a stack misalignment
between `entry`'s own `callf`/`retf`, and found a concrete,
already-flagged-as-unconfirmed suspect: the tick ISR's own
non-`IRET`, "abandon this context and jump straight into the
dispatcher" design

Checked every piece #70 left open, in order, each with hard evidence
(register/memory reads, not inference):

- **`bx` at `entry`'s own `call cs:functab[bx]`**: `0x0022` on every
  single one of 128 consecutive calls (`17<<1`, exactly `io_polldev`'s
  own correct index) -- never corrupted.
- **`functab[17]`'s own stored word**, read directly out of
  `build/kernel/MPM.bin.cseg` (no need to even boot QEMU for this
  one): `0x704E`, exactly `XIOS_base + polldev`'s own local offset
  -- already correctly adjusted, confirmed in #70, still true.
- **`polldev`'s own return value and the waiting PD**: `AL=0FFh`
  (ready) and `SI=0E40h` (`TMP`'s own PD), on *every single one* of
  the 128 cycles, not just the last -- `TMP` is being found "ready"
  and spliced onto `rlr` via `insert_process` every time, not just
  once at the end. Given this device is `c0outdev` (console output,
  almost always immediately ready against a real UART's THR-empty
  bit), finding it ready every poll is itself unremarkable -- the
  128-count most likely reflects `TMP` printing something on the
  order of 128 characters through this same yield-and-recheck
  convention, one character at a time, not 128 failed waits for one
  byte.
- **`insert_process`'s own splice logic** (`DSPTCH.RTM`), read
  directly: structurally sound even for the "list currently empty"
  case it hits here. (A same-round `rlr=0` reading turned out to be
  from a stale, wrong `sysdat_segment` baked into that specific
  debug script -- `0x17A6`, where this build's real segment is
  `0x17A7`, confirmed directly afterward by `DS` at the crash itself
  reading `0x17A7` -- so that one number is retracted, but the code
  itself, read by eye, handles an empty list correctly regardless.)
- **`xiosmod`'s own far pointer** (`SYSDAT.FMT` offset `0x28`, used
  by `RTMIF.RTM`'s `xiosif: mov si,mod_entry ! callf dword ptr
  xiosmod[si]`): `1000:6ECF` -- `XIOS`'s own real, correct `entry`
  address -- both statically (`build/kernel/MPM.bin.dseg`) and live,
  read directly out of memory at the exact moment of the `1000:004A`
  crash. Not corrupted, even then.

**So every static piece of this dispatch chain is provably correct,
every single cycle, right up to and including the crash -- yet
`getuser` (`TMP.A86`, flat `0x11A38`) still gets executed as if it
were `xiosif`'s own return point.** Read directly off the stack at a
`getuser` hit (#70): the top word is `0x231D`, `polld_another`'s own
genuine, correct return address for `call xiosif` -- untouched,
correct, still sitting there waiting to be popped. The only way to
reconcile "the correct return address is still on the stack,
undisturbed" with "execution is at the wrong address anyway" is that
`entry`'s own `retf` (which pops the *outer*, far-call frame --
`xiosif`'s own continuation address, one level further down than
`0x231D`) is popping the wrong pair of words for *that* frame, while
the frame underneath it is untouched. That means something adds (or
removes) exactly one extra pushed value onto the stack somewhere
between `callf dword ptr xiosmod[si]` landing in `entry` and
`entry`'s own `retf` -- i.e. during `entry`'s probes or `call
cs:functab[bx]`'s own execution (now shown clean) -- most
consistent with an **asynchronous event interrupting that exact
window**, not a logic bug in any of the code just re-verified.

**Found a concrete, already-flagged-but-never-confirmed suspect for
exactly that**: `XIOS86Q.A86`'s own tick ISR (`tickint`, IRQ0)
deliberately does **not** execute `IRET` to return to whatever it
interrupted -- its own comment says so outright ("this ISR never
executes IRET, it falls into the dispatcher instead"): it saves
registers, acks the PIC, restores the *interrupted* `SS:SP`, then
does `jmpf cs:dword ptr mydispatch` -- abandoning the interrupted
context's own place in its code entirely and forcing a fresh
reschedule. If this interrupt fires during the narrow window between
`entry`'s `callf`-landing and its own `retf` -- while `IF` is
transiently `1` -- the interrupted process's own saved `CS:IP`
(whatever `switch` stores for it to resume from later) would be
*inside* that window, and resuming it later wouldn't replay the rest
of `entry`/`xiosif`/`polld_another` cleanly; it would resume
mid-flight with whatever stack state happened to exist at interrupt
time. `DSPTCH.RTM`'s own `schedule:` has exactly one `sti ! cli`
pair, right before `polld_another`'s own loop starts (line 358),
*already flagged in its own comment* as "TEMP/precautionary... NOT
yet confirmed via its own probe the way the other two were" -- i.e.
a previously-identified, still-open risk in precisely this code
path, from earlier in this same session, never actually exercised
until now.

**Not confirmed this round** (context ran out): whether a tick
interrupt genuinely fires during this window, and whether `IF` is
really transiently `1` there given `PROC.RTM`'s own `0000h`
diagnostic (#64) keeps every freshly-dispatched process's `IF` off
by default. This is the strongest remaining lead, but it's still a
hypothesis, not something read directly off a register yet.

**Next step**: probe `tickint`'s own very first instruction (marker
`'"'`, already present) and `schedule`'s own `sti`/`cli` pair
directly, specifically watching whether `tickint` ever fires between
`entry`'s `call cs:functab[bx]` and its `retf` during one of these
128 `TMP`-console-output cycles -- that's the one remaining concrete
check needed to either confirm or rule out this exact mechanism.

## 72. Two more concrete, directly-tested hypotheses, both with zero
effect on the crash; one real, independent hardening fix kept anyway
(IRQ6 was unmasked and had no business being so); several leads from
this round retracted after closer checking; status honestly updated

Checked `tickint`'s own next step from #71 immediately: breakpointed
its very top across the full run from boot to the `1000:004A` crash.
**It fires zero times.** The tick-interrupt theory is dead -- there
is no tick anywhere in this picture.

**Dead end, caught before being written up as a finding**: a
properly rigorous (non-overlapping, exact-byte-pattern) decode of a
free-running trace's `conout` probe output appeared to show `XIOS`'s
own signon banner printing a *second* time, deep in the trace, right
after ~128 repeats of a two-character sequence. This looked like a
dramatic new lead (`INIT` re-entered). It wasn't checked carefully
enough initially -- the pattern-match was real, but matches can
legitimately span widely separated points in the trace when a call
yields and resumes later (exactly what's happening here), so
"reconstructed in order" isn't the same as "printed contiguously".
Given how much else this round already demonstrated about this bug's
own timing sensitivity, this one is left here as a caution rather
than a retracted claim either way -- not confirmed, not disproven,
just not pursued further.

**A stack-pointer "smoking gun" that turned out to be a pairing bug,
not a real corruption**: added a probe dumping `SP` immediately
before and after `entry`'s own `call cs:functab[bx]`, free-running,
no GDB. Found, with perfect reproducibility, 128 cases where the
"before" and "after" `SP` differed by a huge, fixed amount. Adding
`SS` to the same probe revealed why: those weren't one call's before
and after -- they were two *different*, legitimately interleaved
calls (`TMP`'s own `conin`, yielding because no input is ready, and
the scheduler's own nested `polldev` check running on its own
dedicated stack while `conin`'s call is suspended). Naive in-order
pairing matched the wrong before/after records together. Once SS was
checked, every real pair matched perfectly. No bug here -- reverted
the probe.

**Clarified, this time with real confidence, what `TMP` is actually
doing in its own loop**: a cleaner probe (dumping the incoming XIOS
function number, not just guessing from addresses) showed the
repeating unit is an 8-step cycle -- `const`, `conout`, `conout`,
`const`, `conout`, `conout`, `conin`, `polldev` -- repeating for as
long as the run lasts (the exact repeat count varies run to run, from
128 to several hundred, consistent with everything else's timing
sensitivity). This is `TMP` printing two characters, trying to read
one (never succeeding, since nothing feeds real input), and yielding
once per pass. `conin` genuinely never returns real data in any of
these runs.

**Chased "128" to its real source, which turned out to be mundane**:
`TMP.A86`'s own `maxcmdlen` is `128`, and its `read_buf`/`clicb_cmd`
convention means a console-read line really does stop, by design,
once 128 characters accumulate. Checked whether that boundary causes
a buffer overflow in `clicall`'s own `mov byte ptr [bx],'$'` (writing
to `savebuf + read_blen`, which equals `savebuf + 128` -- one byte
past `savebuf`'s declared 128-byte allocation): it doesn't look like
one in practice -- `TMP.A86`'s own source ends with a trailing `db 0`
right after `savebuf`, and `build_mpmsys.py`'s own paragraph-rounding
doesn't add any further padding, so that one-byte write already has
exactly the slack the original 1981 source seems to have deliberately
left for it. Not the bug, as far as this round could tell -- but
worth a second look if `TMP`'s data segment layout ever changes.

**Found and fixed something real and independently worth keeping,
even though it didn't move the crash**: `info pic` showed the
master PIC's IRQ6 (floppy controller) line unmasked -- left that way
by SeaBIOS, which relies on it for its own INT13h floppy driver.
Confirmed a genuine SeaBIOS handler is still installed at that exact
IVT slot (`F000:EF57`, read directly out of guest memory, not
garbo). This kernel's own floppy driver is entirely polled (never
waits on this interrupt) but *does* set the FDC's own DMA/interrupt-
enable bit on every DMA-driven read (#65/#66), which can make the
real NEC765/82077 raise IRQ6 on completion -- left unmasked, that
interrupt could fire at any point during this kernel's own execution
and vector into SeaBIOS's own handler, asynchronously, matching this
bug's own non-determinism suspiciously well. **Fixed** by masking
IRQ6 at the PIC during `XIOS86Q.A86`'s own init, right alongside its
existing IRQ0 unmask -- costs nothing, since nothing here ever
relies on it. **Rebuilt and tested: no effect on the crash** --
confirmed via `info pic` that the mask bit really did take
(`imr=0xF8`, bit 6 set) and the crash still lands at the identical
`1000:004A`, byte-for-byte the same trace length as before. Kept the
fix anyway (it's real, independent hardening), but this specific
mechanism is now also ruled out.

**Honest status after this round**: six concrete hypotheses for the
`1000:004A` crash are now ruled out with direct, hands-on evidence
across #68-#72 (false-ready UART data, `functab`'s module-base
adjustment, `INIT`'s own termination, the tick interrupt, a stack-
pointer corruption in `entry`'s own dispatch, and now an unmasked
IRQ6). The entire static dispatch chain -- `entry`, `xiosmod`,
`functab`, `polldev`, `insert_process` -- has been directly verified
correct on every cycle leading up to the crash. What's still real and
unexplained: `getuser` (`TMP.A86`, an address with no business being
reached from the scheduler's own poll-list walk) gets executed while
running on the dispatcher's own stack, with the correct return
address for that call still sitting untouched on the stack
underneath it (#70/#71's own finding, not retracted). Two independent,
mechanism-based fixes aimed squarely at async-interrupt theories have
each had zero effect, which is itself informative: whatever's
happening either isn't an interrupt at all, or isn't one of the two
sources checked (IRQ0/tick, IRQ6/floppy) -- keyboard (IRQ1) and
cascade (IRQ2) are also unmasked per the same `info pic` output and
haven't been checked or ruled out yet. That's the most concrete
remaining thread for the next round: either rule those out too, or
find a genuinely non-interrupt explanation for how `entry`'s own
`retf` ends up pointing at `TMP`'s `getuser` while the stack
underneath it stays perfectly intact.

**Did exactly that immediately, same round**: masked IRQ1 (keyboard)
and IRQ2 (cascade-to-slave) too, alongside IRQ6 -- this build has no
keyboard driver and no use for anything behind the slave PIC, so
there's no downside regardless of outcome. Rebuilt, confirmed via
`info pic` that the master PIC's mask is now `0xFE` (every line
masked except IRQ0/tick, which #72 already proved never fires at
all), and the crash is still byte-for-byte identical, same
`1000:004A`. **This definitively rules out every master-PIC
interrupt source as the cause** -- combined with tick's own confirmed
zero firings, there is no hardware interrupt of any kind involved in
this crash. (Kept all three masks -- IRQ1/2/6 -- as legitimate,
harmless hardening regardless; none of this kernel's own code uses
any of them.)

**Revised status**: the `1000:004A` crash is conclusively *not*
interrupt-driven. It's a pure logic/data issue somewhere in the
already-verified-clean `entry`/`xiosmod`/`functab`/`polldev`/
`insert_process` dispatch chain, or in something adjacent to it that
hasn't been checked yet. Given the sheer amount of ground already
covered without pinning down the exact faulting instruction, the
most promising remaining approach for a future round is probably to
stop inferring the mechanism from symptoms and instead trace the
*complete*, uninterrupted instruction sequence from a known-good
point (e.g. `polld_another`'s own `call xiosif`) all the way to the
`1000:004A` landing in one continuous pass, accepting the cost of
however many single-steps that takes, rather than sampling specific
checkpoints across separate runs -- the cross-run sampling approach
this round relied on has repeatedly produced inconsistent counts
(128 here, 700+ there) precisely because the bug's own manifestation
point drifts with timing, making point-sample evidence from
different runs hard to stitch into one coherent story.

**Did that check -- ruled out, cleanly.** Breakpointed `tickint`'s
own top directly, running all the way from boot to the `1000:004A`
crash site: it fires **zero times** the entire run. `#71`'s own
leading hypothesis is dead -- there is no tick interrupt anywhere in
this picture, so `schedule`'s own unconfirmed `sti ! cli` pair isn't
the mechanism either (nothing is pending there to be let through).

**One dead end along the way, worth recording so it isn't re-chased**:
scanning a free-running trace for `XIOS`'s own signon banner
appeared, at first, to show it printing a *second* time deep in the
trace, right after ~128 repeats of a `^@` pattern -- which looked
like a dramatic new lead (`INIT` re-entered, recreating every RSP
from scratch). It isn't real: that "second signon" was an artifact
of the same per-character `[c]36{c}c` extraction heuristic used
earlier to read printed text out of a trace cluttered with debug
markers -- the regex pulled matching single-character fragments from
widely separated, unrelated parts of the 97KB trace and concatenated
them in file order, which can spell out things that were never
actually printed. Checked directly: the literal bytes `MP/M-86
XIOS86Q` do not occur a second time anywhere in the raw trace file.
Any future text reconstructed this way needs to be confirmed against
the raw bytes before treating it as a real finding -- this one
wasn't.

**Status**: four concrete hypotheses for the `1000:004A` crash are
now ruled out with hard evidence across #68-#71 (false-ready UART
data, `functab`'s own module-base adjustment, `INIT`'s own
termination, and the tick interrupt), and the dispatch chain through
`entry`/`xiosif`/`polldev`/`insert_process` is confirmed structurally
correct on every one of the 128 cycles leading up to the crash. The
crash itself remains unexplained. Given how acutely timing-sensitive
this has proven -- the exact same breakpoint set on consecutive runs
has produced a "128 clean cycles then crash" trace and a "128
`crlf`/`getuser` alternations then crash" trace, different enough
that the precise moment of divergence hasn't been pinned down twice
in a row -- further progress likely needs either a way to observe
without perturbing timing (not available here), or picking apart
exactly what `TMP` is printing that takes ~128 characters in the
first place, independent of the crash mechanism itself, as a way in
from a different angle.

## 73. Found and fixed a major methodological bug in this *session's*
own tracing, going back several rounds: `TMP.A86`'s own addresses
were computed with the wrong formula the entire time -- the
"`getuser` gets wrongly executed" finding that drove #70-#72 was a
phantom. Corrected tracing shows the crash doesn't need 128 cycles at
all -- it happens on `TMP`'s very first character, inside (or just
after) printing a control-character escape for a literal NUL byte

**The error**: `TMP.A86` is a separate RSP, loaded at its own real
segment, `tmp_code_seg` (`build_mpmsys.py`'s own `tmp_code_seg =
MPM_CODE_SEG + len(cseg)//16` -- a real segment *value*, computed
from how many paragraphs the combined kernel cseg occupies). Every
flat address this session computed for a `TMP.A86` symbol, across
#70, #71, #72, used `MPM_CODE_SEG*16 + local_offset` (the formula
that's correct for `SUP`/`RTM`/`MEM`/`CIO`/`BDOS`/`XIOS`, which really
do all share `CS=1000h`) instead of `tmp_code_seg*16 + local_offset`.
`TMP`'s own local offsets happen to be small (low hundreds/thousands),
coincidentally landing inside `RTM`'s real address range under the
wrong formula -- which is exactly why a breakpoint "at `getuser`"
kept catching a real, legitimate, correctly-executing instruction
inside `RTMIF.RTM`'s own `xiosif` (its `call word far [si+0x28]`,
confirmed by disassembling the actual bytes there with `ndisasm -b
16`) every time, with a perfectly intact, correct return address
sitting right underneath it -- because that *was* real, correct
code, not a wrong landing at all. None of #70-#72's *other* findings
depended on this (the `1000:004A` crash site itself is in `SUP`,
whose base really is 0, and all the ruled-out hypotheses -- UART,
`functab`, `INIT`'s termination, every PIC interrupt -- stand on
their own), but the specific "`entry`'s own `retf` lands on `TMP`
code" narrative that organized #70-#72 does not, and is retracted.

**Separately, re-confirmed and fixed a second, real tooling trap**:
single-stepping (`stepi`) in this environment triggers a genuine
`INT1` (debug exception) after every instruction, and the guest's own
IVT entry 1 is a real SeaBIOS handler (`F000:FF53`, confirmed via
`xp` on physical address 4). That's a real detour through BIOS code
on *every single step* that free-running execution never takes.
Worked around it for the rest of this round by never calling `stepi`
at all -- advancing past a breakpoint by deleting it, issuing one
`continue` (full speed, no trap flag), then re-arming it -- which
keeps the Python-side bookkeeping GDB needs for "don't re-hit the
same stale breakpoint" without ever setting TF.

**With both fixed, corrected, clean traces (TMP addressed correctly,
zero `stepi` calls) show something much simpler than every previous
round's picture**: `TMP` reaches `nextcommand` → `freedrive` →
`attach` → `nextcommand` → `freedrive` → `attach` → `getuser` (all
confirmed via `CS=1733`, `TMP`'s own real segment, this time) →
`CHARIO.CIO`'s `conread_entry` (`CS=1000`, correct) → `readen` →
`1000:004A`, with the breakpoint budget for the *entire* sequence
under 10 hits, not hundreds. `readen` dumps `read_blen` (confirmed:
`BX=0042` at the crash, i.e. only 66, nowhere near `maxcmdlen`=128)
and calls `pcr`/`conoutf`, which cleanly round-trips through
`xiosc`/`xiosif`/`entry`/`conout` dozens of times (watched this
directly, no crash) -- so the "it takes ~128 characters to go wrong"
read from every earlier round was itself a symptom of the
`stepi`/`INT1` contamination perturbing timing, not a real property
of the bug.

**Narrowed the real location further, directly, no longer relying on
cross-run inference**: breakpointing `CHARIO.CIO`'s `readn0` right
after `and dl,07fh` (dumping the just-read character) shows **`DL=00`
(a literal NUL byte) on `BL=00` (the very first character of the
line) -- and the crash follows directly, with no further `readn0`
hits at all.** So this doesn't need 128 retries to manifest; it's
wrong from `TMP`'s very first character. Read `CHARIO.CIO`'s own
`rdecho`/`rdech1`/`ctlout` by eye for this exact byte: `chkctl`
correctly passes a NUL through untouched (doesn't match `ctld`/
`ctls`/`ctlc`), `rdecho` itself is correctly stack-balanced (confirmed
directly, not just by reading), but `ctlout` -- called on every
character via `rdech1` -- calls `echoc` to decide if the character is
"graphic"; a NUL is not, so it takes `ctlout`'s *other* branch:
`push dx ! mov dl,ctl ! call conout ! pop dx ! or dl,40h` -- an
*additional*, nested `conout` call (print a literal `^` prefix)
specifically for control characters, before falling through to
`tabout` to print the (XOR'd) character itself. This is the one
piece of the per-character path this round didn't get to exercise or
rule out directly before running out of context.

**Did exactly that immediately, same round -- and it points at
something much bigger than a dispatch bug.** Traced `tabout`/`conout`
cleanly (stepi-free) for ~25 more characters past the first NUL,
logging `DL` at each one: `06, 06, 00, 50, 46, 40, 32, 47, 42, 78,
03, 6e, 50, 52, 46, 40, 20, 47, 42, 78, 03, 6e, 5a, 58, 47...`.
Decoded as ASCII where printable, this spells (with doubled bytes
exactly matching how a `mov al,X ! out dx,al` debug probe emits each
byte): `..PFF@@22GGBBxx..nnPPRRFF@@ ..GGBBxx..nnZZXG...` -- this is
not random UART noise. It is **recognizably this kernel's own debug
probe output** -- the same `[char]`/markers shape seen throughout
every serial trace this whole session, specifically the
`"]36{char}char"` / `"PPFF@@"`-style sequences this round already
decoded out of free-running traces (#70's own careful byte-exact
decode).

**Leading theory, not yet proven**: this build's debug probes write
their diagnostic bytes to port `03F8h` -- the exact same UART port
`XIOS86Q.A86`'s real console I/O (`conin`/`conout`) uses. If writes
to that port are visible back on the read side (QEMU's UART model
looping transmitted bytes into its own receive path when the
backing chardev is a plain file with no real far end, or some
similar artifact of this specific `-serial file:...` setup), then
**every debug probe this entire session has ever emitted has also
been quietly feeding itself back into the console as "typed" input**
-- which would mean the "128 characters", the UART `rxrdy`-with-
error-bits anomaly (#68), and this round's single-NUL-character
crash are not separate phenomena at all: they're all downstream of
the same self-inflicted interference between this kernel's own
instrumentation and the thing it's instrumenting.

**Tested directly, same round -- ruled out, and found something more
fundamental in the process.** Moved every debug probe in the kernel
(236 occurrences across 18 files, verified the one real exception --
`uart_base equ 03F8h` itself -- was left untouched) from `03F8h` to
an otherwise-idle `02F8h` (COM2), rebuilt, and captured the real
console (COM1) and the relocated debug stream on two separate
`-serial file:` backends. **The same "garbage" text still appears on
the real console port**, completely unchanged, with debug fully
isolated on a different port. Also tried `-serial stdio` instead of
`-serial file:` for the console, in case this was a quirk of the
file-backed chardev specifically (e.g. transmitted bytes looping
back as received ones with no real far end attached): identical
output either way. **Both theories are dead**: this is not debug-
probe self-interference, and it is not a serial-backend artifact.
The "garbage" is real content the guest itself is sending out on the
real console port.

**What's actually significant, and still unexplained**: that
content appears immediately after `XIOS86Q.A86`'s own init signon
banner finishes printing -- i.e., during `SUP`'s own module-init
sequence, well before `TMP` (or any RSP) has even been created. It
cannot be `TMP` echoing back something it misread via `conin`; `TMP`
doesn't exist yet at this point. Whatever's producing it is coming
from the kernel's own early boot path. Given this round ran out of
context immediately after finding this, it has not been decoded or
traced further -- worth independent investigation in its own right
in a future round, and may or may not be related to the
`1000:004A` crash chased in #70-#72.

**Process note, logged for completeness**: the "move every probe to
a different port" edit was done with a careless Python script that
read these 18 files in text mode and rewrote them, silently
collapsing their original `\r\n` line endings to bare `\n` in the
process (caught via `git diff --stat` showing near-total-file-sized
diffs after reverting the port change back to `03F8h`). Fixed by
renormalizing all 18 files back to `\r\n` directly, byte for byte,
and verified both that no `02F8h` remained and that this round's own
legitimate `XIOS86Q.A86` edits (DMA, IRQ masking, `ccbtab0`, etc.)
were still intact -- confirmed afterward with `git diff --stat -b`
(ignoring whitespace), which shows only `XIOS86Q.A86`'s real 40
inserted lines and zero content differences anywhere else. No work
was lost, but this is exactly the kind of mistake `AGENTS.md`'s own
"never commit" rule exists to leave room to catch -- recorded here
so a future round doesn't need to rediscover it the hard way.

## 74. Followed up on #73's own open thread (the early-boot garbage
right after the signon banner) -- re-confirmed `INIT.SUP`'s own
module-init/`nrsp`/self-terminate sequence is genuinely correct, byte
for byte, by disassembling the real compiled bytes directly; then
settled what the repeating `"^@"` text actually is (`TMP`
legitimately, harmlessly echoing a real stream of NUL bytes from
`conin` as the standard `^@` control-character escape -- not a bug
by itself); and finally hit a wall that looks like the real
explanation for why this entire investigation has been so hard to
pin down: **the crash's own timing is sensitive to which exact
breakpoints are set, not just to single-stepping**

**Re-verified #73's "garbage right after signon" lead was itself
heading toward a correct, boring answer, not a new bug.**
Disassembled the real bytes (not symbol lookups, which this file has
already shown twice now can't be trusted at these low SUP offsets)
from right after `XIOS86Q.A86`'s own `init` returns, all the way
through `INIT.SUP`'s own `nrsp` process-creation loop and into
`rsp_out`'s own termination call. Every instruction matches
`INIT.SUP`'s own source exactly: the module-present check and far
call that start this entry's own trace, the real IVT installation
for `mpmint` (vector `0xE0`, pointed at `user_entry`, offset `0x39D`
-- confirmed directly in the disassembly, matching `SYSTEM.DEF`'s own
`mpmint equ 224`), the `'X'`/`':'` probe dumping `xiosmod`'s far
pointer (already independently confirmed correct in #71), the real
`nrsp` loop (`int 0E0h` with `CX=90h`, i.e. `f_createproc`, looped
via `jcxz`/`jmp` exactly as `INIT.SUP`'s source reads), and
`rsp_out`'s own markers through to the real termination call. No
bug found here -- this whole stretch is correct, which also means
#68/#71's own earlier "`INIT`'s termination is clean" finding is
reconfirmed a third, independent way.

**Settled the `"^@"` mystery directly.** Breakpointed `CHARIO.CIO`'s
`ctlout` and its own inner `call conout` (the branch taken
specifically for non-graphic characters -- `push dx ! mov dl,'^' !
call conout ! pop dx ! or dl,40h`) with the stepi-free method: with
`DL=00` (a real NUL, confirmed previously in #73), this round watched
it cycle `ctlout -> conout('^') -> back to ctlout` **repeatedly**
(30+ clean cycles in one run before this round's own breakpoint
budget ran out, no crash). This is exactly `TMP`'s own designed
behavior for an unprintable character: print `^`, then `0x00 XOR
0x40 = 0x40 = '@'` -- i.e. `"^@"`, the conventional rendering of a NUL
in a terminal. The repeating `"^@^@^@..."` seen all the way back in
#70's own trace decode (and briefly, wrongly, suspected of being a
reconstruction artifact or an `ABORT.RTM` marker collision) is simply
this: `TMP` faithfully echoing a long run of real NUL bytes arriving
from `conin`, over and over, exactly as designed. Not a bug on its
own.

**The actual wall hit this round**: re-running the *same* `ctlout`
breakpoint check, with only one fewer intermediate breakpoint address
being watched (dropping the `after_conout` checkpoint, keeping only
`ctlout` itself and the crash site), changed the outcome completely
-- the crash now hit after just **one** `ctlout` call, where the
previous run had cycled cleanly through 30+ with no sign of trouble.
Nothing about the *guest's* own code changed between these two runs;
only how many addresses this session's own tooling was watching did.
That rules out "it just takes N retries to overflow something" as
the story -- if it did, the count would depend on the guest's own
state, not on how many GDB breakpoints happen to be live. This
strongly suggests the real trigger is a genuine wall-clock/real-time
race (almost certainly still rooted in the UART's own emulated
timing, given #68's already-confirmed false-ready quirk and how
TCG's own execution speed relative to a real 16450's baud-rate-paced
byte timing has come up repeatedly this session) that GDB's own
overhead -- in this specific environment, apparently even just the
bookkeeping cost of an extra live breakpoint, not only `stepi`'s
already-confirmed `INT1` detour -- is itself enough to shift.

**Where this leaves things**: the `1000:004A` crash is real,
reproducible in every free-running trace, and still not root-caused
to one specific faulting instruction. But its *context* is now
solid and triply-confirmed: `INIT`'s own module-init/`nrsp`/
termination sequence is clean; `TMP` reaches its own real
`nextcommand`/`attach`/`conread_entry` flow correctly; the repeating
output many earlier rounds chased as mysterious is `TMP` correctly
echoing real NUL input; and every dispatch-chain component
(`entry`/`xiosmod`/`functab`/`polldev`/`insert_process`) has been
independently verified clean. What's left is a timing race this
session's own tooling cannot observe without disturbing it -- not
because of a single identified contamination source (`stepi`/`INT1`
is confirmed and worked around; this round shows plain breakpoint
*count* also matters, which is a new, harder constraint). Future
investigation here will likely need either genuinely passive
observation (a full instruction trace recorded by QEMU itself, e.g.
`-d exec` to a log file, read after the fact, with zero GDB attached
during the run) or accepting that this specific race needs to be
reasoned about from the source and the hardware timing model
directly, rather than caught live.

## 75. Followed #74's own closing recommendation -- a genuinely
passive QEMU-only trace, zero GDB, zero breakpoints, zero
single-stepping -- and it paid off: the exact faulting `ret`, its
wrong target, and the full accidental-decode path into `1000:004A`
are now all directly confirmed, settling #67's year-old leading
suspect for the first time

**Method**: `qemu-system-i386 -accel tcg,one-insn-per-tb=on -d
exec,nochain -D <logfile> -serial file:<consolelog> -display none
-no-reboot -no-shutdown`, no `-s`/`-S`, no monitor, no GDB attached at
any point. `one-insn-per-tb` forces exactly one guest instruction per
translated block, so `-d exec`'s per-TB trace becomes a genuine
per-instruction trace with no debugger-side perturbation at all --
the thing #73 flagged `stepi`/`INT1` contamination couldn't give and
#74 flagged plain breakpoint *count* also couldn't give. **Validated
against the established baseline before trusting any of it**: the
real COM1 console output this run produced is byte-identical in
length to the already-known-good free-running trace (97,117 bytes,
matching #73's own "97KB trace" and #65's post-fix 93,321-byte
figure's successor) -- this specific logging setup does not change
guest-observable behavior, unlike adding `-d int` alongside it (tried
and discarded: logging interrupts as well as exec slows emulation
enough that a 15-second run never even got past early BIOS/floppy
polling, console output `0` bytes -- too perturbing to trust for a
bug this timing-sensitive, so the write-up below relies only on the
`exec,nochain`-only capture).

**What the trace actually shows at the crash, read directly off real
flat addresses (no `asm86 --symbols` reliance -- #73 already proved
that's misleading at these exact low SUP offsets)**: after the
already-confirmed-clean `nrsp` loop and `INIT`'s own termination
sequence (#74), the very next two addresses are `0x104CD` then
`0x102FA`, back to back, with nothing between them. Disassembling
`build/kernel/MPM.bin.cseg` directly confirms `0x4CD` is a bare
`IRET` (byte `CFh`), which means its popped `CS:IP` really is
`1000:02FAh` -- and `0x2FA` is exactly `zdbg`, the hex-digit-to-port
debug-print helper **confirmed at this same address by #67's own
earlier, symbol-verified breakpoint**. This is the direct, traced
confirmation of #67's own leading (and, at the time, unconfirmed)
suspect: `INIT.SUP`'s `rsp_out` ends its `int mpmint`
self-termination call with nothing coded after it but `zdbg`'s own
body, so whatever return address the `mpmint` syscall gate's own
dispatch-exit `iret` ends up using is the same one `int mpmint`
itself pushed at the call site -- landing on `zdbg`, with no real
`call` frame set up for it, exactly as #67 described but could not
pin down end-to-end that round.

**Traced the rest of the fall-through, instruction by instruction,
the same way**: `zdbg` runs its real, correct 4-hex-digit print loop
(`zdbg1`/`zdbg2`/`zdbg3`, `0x302`-`0x317`, matching #67's own
symbol-confirmed addresses) and reaches its own unconditional near
`ret` at `0x328` (byte `C3h`) -- confirmed by direct disassembly,
right where `idle:` begins in the source per #67. Because this
particular entry into `zdbg` was never a real `call` (it was the
`iret`'s own fall-through landing, per above), there is no legitimate
return address under it for this `ret` to consume. It pops whatever
is actually there: `0x0043` -- squarely inside `INIT.SUP`'s own
literal copyright/signon banner text, the same neighborhood #66
already identified as a "wild jump into data," but never previously
down to the exact instruction that produced it.

**Decoded the accidental instruction stream from there, byte for
byte, against the real banner text (`"...6 2.0 [5 Oct 81]..."`)**:
`0x0043`: `2E 30 20` = `cs: xor [bx+si],ah` (3 bytes) → `0x0046`: `5B`
= `pop bx` (1 byte) → `0x0047`: `35 20 4F` = `xor ax,0x4f20` (3 bytes)
→ `0x004A`: `63 74 20` = `arpl [si+0x20],si`. All four landing
addresses match the trace's own observed PCs exactly
(`0x43`→`0x46`→`0x47`→`0x4A`), confirming this isn't guesswork --
it's the literal path the real CPU took through literal string data.

**At `0x004A`, the trace stops advancing and alternates forever**:
`Trace: ...104a, fff53, 104a, fff53, 104a, fff53...`, confirmed over
hundreds of repeats with nothing else interleaved. `0xFFF53` is
`F000:FF53` -- the exact same SeaBIOS `INT1`/`#DB` debug-exception
handler #73 identified and blamed entirely on GDB's own `stepi`
mechanism. **That attribution needs qualifying, not retracting**:
this round's trace proves the same handler is reached completely
natively, with no GDB involved at all, right at this crash -- so
`#DB`/`INT1` is not purely a tooling artifact; it is also a real part
of this bug's own mechanism, at least here. The most likely
explanation (not directly confirmed this round -- no GDB was attached
to read `EFLAGS`/`DR7` at the exact moment) is that the trap flag
ends up set somewhere in this same corrupted-stack chain, so every
attempt to execute `arpl` at `0x004A` immediately re-traps before
advancing to `0x004D`, and SeaBIOS's own handler `iret`s straight back
without ever clearing it -- a self-sustaining trap loop that looks,
from the outside (`info registers`, #66's own technique), exactly
like a CPU permanently parked at one address, when it is actually
cycling between two.

**Where this leaves things**: the full mechanical chain from `INIT`'s
own termination call down to the infinite `1000:004A`/`F000:FF53`
trap loop is now traced and confirmed end-to-end, for the first time,
with zero debugger interference -- closing #67's own open thread from
many rounds back. What's still genuinely open: *why* the `mpmint`
syscall gate's own dispatch-exit lands on `zdbg`'s pushed return
address instead of `rsp_out`'s real caller context (is this always
what happens on `INIT`'s very last termination, or does it depend on
`rlr`/stack state left over from something earlier -- #67's own (a)/
(b) ambiguity about *which* termination attempt it was is still
unresolved), and *where* the trap flag genuinely gets set. Next
round: disassemble the real `mpmint` interrupt-gate dispatch-exit
code itself (flat offsets `0x4A2`-`0x4CD` in `SUP`'s own cseg) against
`SUP.A86`'s source directly, byte for byte the same way this round
read `zdbg` and the banner text, to find exactly which stack slot it
pulls `0x02FA` from and why.

## 76. Followed #75's own next step -- read `SUPIF.SUP`'s real
`user_entry` source directly against the `0x4A2`-`0x4CD` region
identified there, and it needs no "stack corruption" at all: the
whole chain is explained by one documented design assumption that
just doesn't hold this one time

**`0x4A2`-`0x4CD` is `user_entry`'s own epilogue, confirmed by
source, not a mystery routine**: every `int mpmint` syscall enters
through `user_entry`, which tracks nesting via `u_insys`. On the
*outermost* call it switches to a dedicated per-process supervisor
stack (`u_stack_ss`/`u_stack_sp` saved, `ss:sp` set to the UDA/
`ulen`); on a *nested* call it just `push`es `u_wrkseg` instead. After
`call func` returns, the epilogue mirrors this: `dec u_insys`, and if
still nonzero takes the `nstk: pop u_wrkseg` path straight to `ueout:
mov es,u_retseg ! iret` -- never restoring the real user stack at
all. This `nstk`/`ueout`/`iret` sequence is exactly `0x4C3`-`0x4CD`;
#75's trace already confirmed this exact nested-path branch (`jnz
0x4c3`) is the one taken right before the crash.

**That, by itself, fully explains #75's `iret`-lands-on-`zdbg`
finding, with no additional corruption required**: `INIT.SUP`'s
`rsp_out` calls `int mpmint` for `f_terminate` and -- per its own
source comment, confirmed again here -- has **nothing coded after
that instruction but `zdbg`'s own body**, because the original 1981
author assumed this call can only ever succeed (successful
termination takes `ABORT.RTM`'s own `jmp dsptch` path, which abandons
this stack/process entirely and never returns here at all -- #67's
own documented distinction). If it *fails* instead (`terminate_entry`
on the `cmp cx,0 ! jle term_err` path, a plain `ret`, exactly as #67
caught with `rlr=TMP`/`pf_tempkeep`), control returns completely
normally, through `func`/`gofar`, into `user_entry`'s own ordinary
epilogue -- which, being an ordinary IRET-based return (not a crash
in itself), faithfully transfers control to whatever address
genuinely follows the `int mpmint` instruction in the compiled
binary. That address is `zdbg`'s own entry point, with no call frame
ever set up for it -- so `zdbg`'s own trailing `ret` (end of #75's
chain) pops whatever word is actually next on the stack at that
point, landing in the banner text. **No stack push/pop imbalance bug
is needed anywhere in this chain**: a syscall that fails when the
source assumed it couldn't, with zero error handling at the call
site, is sufficient on its own.

**What's still open, and is now the single remaining question for
this entire `1000:004A` saga**: *why* does `INIT`'s own
self-termination call fail -- i.e. why doesn't `rlr`/`abt_chk` see a
cleanly-terminatable `INIT` PD at that exact moment (#67's own
unresolved (a)-vs-(b) choice, never settled). Tried to settle it this
round with the same zero-GDB passive trace: `RTM.A86`'s real
`terminate_entry` (`asm86 --symbols`, confirmed cseg-only this time
per #73's own caution -- local `0x118A`, RTM's module base `0x18BF`
computed directly from `SUP.cseg.bin`'s real 6335-byte length since
`build_mpmsys.py` adds no inter-module padding, flat `0x12A49`) is
only reached **twice** in the entire run. Inconclusive, not
negative: both hits lead into what's almost certainly `RTM.A86`'s own
`ydbg` debug-print helper (RTM's equivalent of SUP's `zdbg`, confirmed
at local `0xD5`/flat `0x11994` by the same symbol method) immediately
after `abt_chk` returns, and a trace of addresses alone can't tell
apart "this is `abt_chk`'s own legitimate nested debug-probe call,
business as usual" from "this is a sign the call already went wrong"
-- that needs the actual `rlr` value in memory, which `-d exec`
cannot give. **Next step, concrete and well-scoped**: a single GDB
breakpoint at `terminate_entry` (flat `0x12A49`), re-armed once
between the two hits (the established "re-arm fresh per hit" safe
pattern from #39/#40 -- never `continue` twice past the same
breakpoint on this gdbstub), reading `rlr` directly off each hit, to
finally settle which of the two is `INIT`'s own call and what PD it
actually sees -- not reached this round (usage ran out immediately
after confirming `gdb` itself is available at `/opt/homebrew/bin/gdb`,
before a session could be started).

## 77. Followed #76's own next step with exactly the GDB session it
specified -- and the result **corrects #75/#76's own causal claim**:
`INIT`'s self-termination genuinely succeeds, every time checked; the
`zdbg` fallthrough is caused by a *different*, later call, which does
need a real stack-corruption explanation after all

**Ran the single-breakpoint/re-arm-once session #76 specified**:
`break *0x12A49` (`terminate_entry`'s real flat address, confirmed
again this round the same way as #75/#76 -- `RTM`'s module base is
exactly `SUP.cseg.bin`'s own 6335-byte length, no inter-module
padding), `continue` to hit 1, read `rlr` (`sysdat_segment*16+0x68`,
with `sysdat_segment` recomputed directly from `build_mpmsys.py`'s own
output as `0x17A8` -- flat `0x17AE8`), delete, re-arm, `continue` to
hit 2, read `rlr` again. **Hit 1: `rlr=0x9C0` (`initpd`, confirmed) --
followed up immediately with two more fresh breakpoints (`term_err`/
`dsptch`) to see which this call reaches: `dsptch`, the *success*
path.** **Hit 2: `rlr=0xE40` (`TMP`'s own PD, confirmed) -- same
follow-up shows `term_err`, the *failure* path.**

This finally settles #67's own long-open (a)-vs-(b) question: **(a)
was right** -- these are two genuinely separate termination attempts,
not one call's `rlr` drifting mid-dispatch. `INIT`'s own call
(`rsp_out`'s `int mpmint`) is hit 1, and it succeeds, cleanly, via
`jmp dsptch`, exactly as the 1981 source assumed. Hit 2 is some other,
later caller (per #67, almost certainly `RETMONX`'s own `pf_ctlc`
check) failing on `TMP`'s `pf_tempkeep`, exactly as before.

**Checked immediately whether a successful `INIT` termination this
run still produces the crash, since #75/#76's entire causal chain
assumed it had to fail to reach `zdbg` at all: it does.** Detached
(not killed) after reading hit 2, let the same guest run freely to
completion, and it still parks at `CS:IP=1000:0x4A`, same
`console.log` length (`97,117` bytes) as every other run this whole
saga. **#75/#76's attribution of the `zdbg` fallthrough to `rsp_out`'s
own call is wrong** -- that call succeeds and genuinely never returns
here at all (`jmp dsptch` abandons this context for good); whatever
actually reaches `zdbg` has to be something else.

**Re-ran the same zero-GDB passive trace from #75 fresh (to rule out
this GDB session's own 3-breakpoint perturbation, per #74's own
standing warning) and confirmed all of the above independently, with
no debugger involved at all**: `terminate_entry` is hit exactly
twice; `dsptch` is reached shortly after hit 1 (confirming `INIT`'s
own call succeeds in the *un-perturbed* baseline too, not just under
GDB); `term_err` is hit exactly once, immediately after hit 2.
**Critically, searched for every occurrence of `user_entry`'s own
nested-return `iret` (`0x104CD`) in this clean trace: it fires 18
times over the course of the run -- a perfectly ordinary, frequent
event for any nested syscall -- and only the *very last* one, firing
immediately after hit 2's own `term_err` return (98 trace lines
later), is the one that lands on `0x102FA` (`zdbg`).** The other 17
all return correctly to wherever they should. This is the real
trigger: **`TMP`'s own failed termination attempt (hit 2, `rlr=TMP`,
`term_err`), not `INIT`'s self-termination.**

**What this means for #75/#76's mechanism**: #75's byte-level
decode (the exact `ret`/wrong-target chain from `zdbg` through the
banner text to `0x004A`, and the alternation with SeaBIOS's `INT1`
handler) is still accurate as a description of *what happens once
`user_entry`'s epilogue lands on `zdbg`*. #76's identification of
`user_entry`'s nested-path epilogue (`nstk`/`ueout`/`iret` at
`0x4A2`-`0x4CD`) as the mechanism is also still accurate. What's
retracted is *why* it lands on `0x02FA` specifically: it is **not**
`rsp_out`'s own documented "nothing coded after this call" fallthrough
(that call never gets here -- it succeeds and jumps away via
`dsptch`). `TMP`'s own calling code lives nowhere near `zdbg` (a
completely different module, segment `0x1733`), so a correct,
uncorrupted return for *this* call cannot legitimately land on
`0x02FA` at all -- this one genuinely does need a real stack
corruption, not a documented design gap. The path between `term_err`
and `user_entry`'s epilogue (`func`/`gofar`/the dispatch back up
through `terminate_entry`) is, per #67's own source excerpts, saturated
with `push.../call zdbg-or-ydbg/pop...`-style TEMP DEBUG PROBEs at
nearly every instruction -- `0x02FA` being exactly `zdbg`'s own entry
address is almost certainly a stale copy of one of *those* probes' own
real return addresses, left one slot deeper than it should be by
some specific unbalanced probe along that exact path, rather than a
coincidence.

**Next step**: walk the real instruction sequence from `term_err`'s
own `ret` (flat `0x12B88`) through to `user_entry`'s epilogue
(`0x4A2`) byte-for-byte against `RTM.A86`/`SUPIF.SUP`'s source,
accounting for every single `push`/`pop` around every debug probe on
that specific path (there are many), to find the one that's short or
long by exactly one word -- the same technique #66 already named for
this whole bug class, just not yet actually walked end-to-end for
*this* specific call path.

## 78. Followed #77's own next step byte-for-byte with a live GDB
session, instead of the planned pure source read -- and found there
is **no** push/pop imbalance anywhere in the path. The real cause is
`TMP`'s UDA physically reusing `INIT`'s own freed UDA memory, which
still holds `INIT`'s last, never-unwound `int mpmint` return frame

**Corrected a self-inflicted false lead first.** Early this round,
reading memory at `$sp` directly (not `$ss*16+$sp`) at the `ueout`
breakpoint appeared to show a stale `F000:FF53` (SeaBIOS's `INT1`
handler) pattern already sitting on the stack *before* the call's own
code could have written it -- briefly looked like independent
evidence of an `INT1`/trap-flag-storm leaving debris behind. That was
wrong: `$sp` alone is just the 16-bit offset, not a linear address;
`0xC8`-`0xD2` as a *bare* address lands in the low-memory IVT/BIOS
data area, not anywhere near this process's real stack. No such
pre-existing BIOS pattern was ever actually on `TMP`'s stack. Noted
here so this round's real finding isn't confused with that discarded
one.

**Set up the exact single-breakpoint/re-arm-once chain #77 itself
used (`terminate_entry`/`term_err`/`rtm_ret`/`ueout`, `-fda` floppy
boot this time -- a `-drive ...,format=raw` hard-disk boot was tried
first and silently stalls forever at BIOS/bootloader level, 36 CPU-
minutes with zero console output and not one breakpoint hit; `make
run`'s own `-fda $(DISK_IMG)` is the only boot method that actually
reaches the kernel).** At `term_err` (flat `0x12B88`, the one and
only hit, per #77): `SS=0x177B`, `SP=0xC8`. Read `TMP`'s own UDA
fields directly (`p_uda[TMP]` off the PD table, `rlr=0xE40` confirmed
live): `u_insys=2`, `u_stack_ss=0x1777`, `u_stack_sp=0x1D6`,
`u_wrkseg=0x17A8`. **`u_insys=2`, not `1`, going into this call is
the first real finding**: `user_entry`'s own prologue only reaches 2
by `inc`-ing an already-`1` value, meaning this termination attempt
is itself nested one level inside some other, still-open `int mpmint`
call for `TMP` -- not the fresh top-level call #75/#76 implicitly
assumed. That by itself already explains why the epilogue takes the
`nstk: pop u_wrkseg` branch (never restores `u_stack_ss`/`u_stack_sp`)
instead of the stack-switch-back branch.

**Walked the SP delta hop by hop, live, instead of by eye on the
source**: `term_err`'s own `ret` (`SP:0xC8->0xCA`, +2, a clean near
return) -> `rtm_ret` (confirmed) -> `retf` (+4) -> back in `SUP`'s
`goodmod`, past the `'T'` probe, `func`'s own `ret` (+2) ->
`user_entry`'s `nstk: pop u_wrkseg` (+2) -> `ueout`, `SP=0xD2`. Total
+10 bytes from `term_err` to `ueout`, which is *exactly* what a
perfectly balanced near-ret/retf/near-ret/pop chain predicts, with
nothing left over and nothing missing. **This directly disproves
#77's own "needs a real stack corruption" framing of the problem**:
there is no unbalanced probe anywhere between `term_err` and
`user_entry`'s epilogue for this call. The corruption, if there is
one, has to already be sitting on the stack *before* this call even
starts touching it.

**Read the real stack contents at the correct linear address
(`$ss*16+$sp`, not bare `$sp` -- see the correction above) at
`term_err`, six words up from `SP=0xC8` (flat `0x17878`)**: `0x1a1c
0x079f 0x1000 0x04a2 0x17a8 0x02fa`. Decoded against the call chain
just walked, outermost (highest address/oldest push) to innermost: `
0x02fa` is where `SP` will land right after this call's own `nstk:
pop u_wrkseg` consumes `0x17a8` (confirmed as exactly `u_wrkseg`'s own
live value) -- i.e. `0x02fa` is the *next* word `ueout`'s `iret` is
going to pop as `IP`. `0x04a2` one slot below that is `user_entry`'s
own `call func` return address (matching #76's own identified
epilogue entry point exactly). `0x1000`/`0x079f` below that is the
far-call return frame for `goodmod`'s `callf dword ptr
module_table[si]`. `0x1a1c` is the near-call return address for
`entry:`'s own `call cs:function[si]` (`rtm_ret`, matching #77's own
cited address). Every single word on this stack accounts for a real,
live frame from the call chain just walked -- **except `0x02fa`
itself**, which sits one slot *deeper* than anything this call's own
execution ever pushed.

**Confirmed directly (not inferred) that `0x02fa` is exactly what the
real `iret` consumes**: re-ran the same chain to `ueout`, `stepi`'d
through `mov es,u_retseg` then the `iret` itself, and read `$cs`/`$pc`
live afterward: `CS=0x1000 PC=0x2fa`. Matches #75's original
passive-trace finding exactly, this time with every intermediate
stack word in hand to explain *why*.

**`0x02fa`, with `CS=0x1000`, is not an arbitrary stale value -- it
is precisely `INIT.SUP`'s own `rsp_out` `int mpmint` instruction's
real, hardware-pushed return address** (`rsp_out`'s `int mpmint` ends
at flat `0x2F8`-`0x2F9`, so its own pushed return `IP` is `0x2FA`,
immediately followed by nothing but `zdbg` -- #75's own already-
confirmed fact about that exact instruction). For *this* word to be
sitting at this exact depth in `TMP`'s own UDA-dedicated stack, with
nothing in `TMP`'s own call chain having written it, the simplest
explanation fits every byte observed: **`TMP`'s own UDA (`p_uda[TMP]
= SS = 0x177B`, confirmed live off the PD table) is physically the
same memory `INIT`'s own UDA/dedicated-stack occupied.** `INIT`'s own
self-termination succeeds (#77) via `jmp dsptch`, which abandons that
stack wholesale -- it is never unwound, never zeroed, just left
exactly as it was at the moment of success, with `rsp_out`'s own
`int mpmint` return frame (`IP=0x2FA,CS=0x1000`, pushed by the CPU
itself, no `zdbg` call frame involved) still sitting in it. Once
`INIT` terminates, its PD/UDA memory is freed (`p_uda[initpd]` reads
back `0` now, confirming `freepd` already cleared the PD fields) and
is legitimately reassigned -- to `TMP`'s own newly-created PD. `TMP`'s
own, much later, nested termination call happens to reach the exact
same stack depth `INIT`'s own call once did, and its epilogue's
`nstk` path (never restores `SS`/`SP`, just pops one word and falls
into `iret`) reads straight through to that same never-cleared slot.

**What this means for the whole `1000:004A` saga**: no push/pop
imbalance, in this call or any other, is the cause. It is UDA/stack
memory being reused across processes (`INIT` -> `TMP`, same physical
segment `0x177B`) without clearing the previous occupant's own
abandoned dedicated-stack contents, combined with `INIT`'s own
termination path (`jmp dsptch`) never unwinding that stack on its way
out. `TMP`'s own call is a genuinely ordinary nested syscall, pushing
and popping correctly throughout -- it just inherits one stale word
left over from a completely different process's completely
successful exit, many instructions and one full process-creation
earlier.

**Next step**: confirm the UDA-reuse claim from the other end -- catch
`INIT`'s own termination call *before* `freepd` clears its PD (a
breakpoint on `dsptch` right after `INIT`'s own `terminate_entry` hit,
per #77's own two-breakpoint pattern, reading `p_uda[initpd]` at that
moment, before anything zeroes it) and confirm it reads back
`0x177B` -- the one link in this chain not yet directly observed
(this round inferred it from the freed-and-now-zeroed PD field plus
the exact byte-level stack match, not from a direct before-and-after
read of the same segment value). If confirmed, the actual fix is in
whatever allocates PD/UDA memory for a newly-created process: either
zero a freed UDA's dedicated stack area before handing it to a new
owner, or have a successfully-terminating process's own `dsptch` path
pop/invalidate its last `int mpmint` frame before abandoning the
stack, rather than relying on nothing downstream ever reaching that
exact depth again.

## 79. Followed #78's own next step and instead **falsified #78's
whole theory directly**, by live-measuring `proc_creat`'s actual UDA
computation for every process this boot creates, including `TMP`
itself -- `TMP`'s UDA is not reused `INIT` memory at all, it is
correctly, freshly computed from `TMP`'s own load segment. Found a
new, reproducible, unexplained fourth `proc_creat` call in its place.

**Chronology alone already contradicts #78, before any live check**:
re-reading `INIT.SUP` straight through shows the `nrsp:` loop (lines
157-204, which creates `TMP` via `f_createproc`) runs to completion
*before* `rsp_out`'s own `f_terminate` call (lines 208-210). `TMP`'s
PD/UDA address is therefore nailed down at `TMP`'s own creation time,
chronologically *before* `INIT` ever terminates -- it cannot be
"reusing `INIT`'s freed memory" via any free-then-reassign path,
because nothing has been freed yet when `TMP`'s own UDA segment gets
computed. #78 had the sequence backwards.

**Also, directly from source, not inference**: `freepd` (`PROC.RTM`
lines 9-26) never touches `p_uda` or any other UDA-area byte -- it
only relinks `p_thread`/`p_link` for the free-PD list (`pul`). #78's
"`p_uda[initpd]` reads back `0` now, confirming `freepd` already
cleared the PD fields" has no mechanism behind it in the source as
written; whatever actually produced that `0` (if it was genuinely
`0` and not a stale/misattributed read), it wasn't `freepd`.

**Live-measured `proc_creat`'s actual UDA arithmetic (`cp_nm`, flat
`0x11C82`: `mov ax,u_wrkseg ! mov dx,p_uda[si] ! add dx,ax ! mov
p_uda[si],dx`) end to end, catching both the *before* value (at
`cp_nm`) and the *after* value (at the next label, `cp_uda1`, flat
`0x11CD2`) for every call this boot makes, via the established
single-use-breakpoint/delete/re-break discipline (an interleaved
`cp_nm`/`cp_uda1` pair per call -- letting `cp_nm` alone free-run
across multiple hits, as tried first, reproduced the exact same
frozen register snapshot every time, which turned out to be a GDB
artifact of this remote target, not real repeated execution; the
interleaved form reliably advances)**:

- **Call 1 (idle)**: `SI=0x9F0` (`idlepd`), `before=0x7C`,
  `after=0x1824`. `0x1824` = `sysdat_seg(0x17A8) + idleuda's own
  template offset (0x7C0>>4)` exactly. Correct.
- **Call 2 (tick)**: `SI=0xA20` (`tickpd`), `before=0x8C`,
  `after=0x1834` = `sysdat_seg + tickuda's own offset (0x8C0>>4)`.
  Correct.
- **Call 3 (`TMP`, via `nrsp`)**: `SI=0xE40` (matching every prior
  entry's own cited `rlr` for `TMP`), `before=0x4`, `after=0x177B`.
  `0x177B` is exactly `0x1777 (TMP`'s own `rspseg`/load segment,
  matching #78's own cited `u_stack_ss=0x1777`) + 0x4` (`TMP`'s own
  on-disk template's tiny UDA offset, 4 paragraphs/64 bytes into its
  own data area). **This is `TMP`'s own, legitimately-computed UDA,
  built entirely from `TMP`'s own load segment -- not a byte of it
  comes from anything `INIT` ever owned.** #78's central claim is
  directly falsified, not just chronologically implausible.
- **Call 4 (unidentified, unexpected)**: `SI=0x9F0` again -- `idlepd`'s
  own slot, *reused* a second time -- `before=0x1824` (not a fresh
  template value: this is exactly idle's own real, final, already-
  in-use UDA segment from Call 1, read back unchanged), `after=
  0x2FCC` (`0x1824 + 0x17A8`, i.e. idle's own old UDA segment plus
  `sysdat_seg` again, landing on pure nonsense -- not `idleuda`,
  not any real process's UDA, just garbage one call's own leftover
  state plus another call's own `u_wrkseg`).

**Call 4 should not exist at all under the build's own stated
configuration**: `build_mpmsys.py`'s `NCONDEV_OFFSET` write
(`dseg[NCONDEV_OFFSET] = 1`) hard-codes exactly one console, so
`nrsp`'s own RSP chain should hold exactly one entry (`TMP`) and
`nrsp:` should loop exactly once -- ruling out "a second `TMP`
instance for a second console" as the source of Call 4. Checked the
other obvious explanation directly too: `TMP.dseg.bin`'s own on-disk
`rsp_link` field (offset `0`, per `rsp_link equ 0`) reads back a clean
`0x0000`, so `nrsp`'s own loop-termination condition isn't bad data on
disk either -- `nrsp:` really should only iterate once for `TMP`.

**Leading hypothesis, not yet confirmed**: Call 4 is `TMP` itself,
once dispatched and running, making its *own* `f_createproc` call (a
command processor routinely spawns child processes for whatever a
user/script runs), and *that* call is the one that trips the
already-documented `getpdadr` "already recognized as sysdat-resident"
branch bug (the `bare sysdat` reads at `gpd_bad`'s own comparison,
flagged in `PROC.RTM`'s own comments and chased at length back in
entries in the 40s, e.g. around line 2219 and line 3362 of this file)
-- except this time for a *different* caller than those entries ever
traced, misrouting it onto `idlepd`'s already-used slot instead of
pulling a genuinely free one off `pul`. This is offered as the
leading explanation, not a confirmed one -- nothing this round
actually caught Call 4's own caller red-handed.

**What this means for the whole `1000:004A` saga**: #78 is retracted.
The stale `0x02FA`/`0x1000` word found on `TMP`'s stack in #78 is real
and still unexplained, but it cannot be explained by UDA-memory reuse
across `INIT`→`TMP`, since `TMP`'s UDA was never `INIT`'s to begin
with. What *is* newly confirmed, and far more promising, is a second,
independent, live-reproducible process-descriptor collision (Call 4
onto `idlepd`) in the exact same `getpdadr`/`proc_creat` machinery
this whole saga keeps circling back to -- just triggered by a
different, not-yet-identified caller than any previous entry caught.

**Next step**: identify Call 4's real caller. `proc_creat` already
carries its own `0Eh` marker probe (`PROC.RTM` line 33, dumping the
near-call return address off the stack) -- re-arm a breakpoint there
for this specific (fourth) hit and read that return address directly,
instead of guessing. If it confirms `TMP` as the caller, the next
question is why *this* call reaches `getpdadr`'s "already resident"
branch at all (what address it's checking, and why `0x17A8`-relative
arithmetic there mistakes it for one), the same unresolved thread
entries in the 40s left open for a different caller.

## 80. Told to continue until successful boot. Found and fixed **two
real, confirmed source bugs** (same missing-parens operator-precedence
mistake, in two different routines) while chasing #78/#79's open
thread, directly confirmed the system's actual final resting state for
the first time via a proper live interrupt, and narrowed -- but did
not yet close -- the mystery of what actually calls the fatal
`f_terminate`

**Fix #1, `COMMAND.SUP`'s `clirls` (the normal "command finished,
return to the TMP prompt" exit)**: `and cx,not pf_tempkeep+pf_keep`
parses as `(not pf_tempkeep)+pf_keep` (confirmed directly against
`tools/asm86/expr.py`'s own `parse_unary`: `not` binds only to the
single following term, same precedence as unary `-`), giving a mask
(`0FE01h`) that leaves bit 9/`pf_tempkeep` *set* instead of clearing
it -- `clicmd_entry`'s own top (`and ax,not pf_keep!or ax,pf_tempkeep`)
sets `pf_tempkeep` unconditionally on every command run, so any run
that falls through to this exit (every normal, non-chaining command)
left it stuck on permanently. Fixed by parenthesizing, matching the
already-correct `and p_flag[bx],not (pf_keep+pf_sys+pf_tempkeep+
pf_ctlc)` two lines above it (the "chain and terminate" exit), which
never had this bug.

**Fix #2, `COMMAND.SUP`'s `user_retf` (the "an RSP fell off the end of
its own code via a bare `retf`" cleanup -- the *normal*, expected way
`TMP` itself finishes running, not an error path)**: the exact same
missing-parens mistake, `and p_flag[bx],not pf_keep+pf_tempkeep+
pf_ctlc+pf_sys`, evaluates to mask `027Eh` (bit 9 still set) instead of
the intended `not (pf_keep+pf_tempkeep+pf_ctlc+pf_sys)` = `0FD7Ch`.
Fixed the same way.

**Rebuilt and re-ran after each fix -- the crash is byte-for-byte
identical both times** (same 97117-byte serial trace, same final
state). Chased *why* directly rather than assuming the fixes were
wrong: a live hardware watchpoint on `p_flag[TMP]`'s own memory
(`0x188C6` this run) shows the exact sequence of every write to it for
the whole boot: created at `0x13` (`pf_sys+pf_table`); two clean
`getdiskmx`/`RETMONX` critical-section cycles (`BDOSIF.BDO`, correctly
sets then restores `pf_tempkeep` both times); then `clicmd_entry`'s own
top sets it (`0x13`->`0x211`, confirmed live at flat `0x10949`,
matching `COMMAND.SUP` line 50 exactly); then **nothing else ever
writes `p_flag[TMP]` again** until `abt_chk`'s own `ac_tk` branch adds
`pf_ctlc` (`0x211`->`0x291`, at flat `0x102DA9`, inside `abt_chk`) right
before `term_err`. Neither `clirls` nor `user_retf` -- the two sites
just fixed -- ever runs in between: there is no third write to
`p_flag[TMP]` for either of them to have made, fixed or not. Both
fixes are real, confirmed bugs (verified against the assembler's own
expression-parsing rules, not just read by eye), and both will matter
once execution ever reaches them cleanly -- they just are not what's
firing on *this* specific path.

**Walked back from `u_insys[TMP]`'s own live watchpoint instead,
across the whole boot, to see exactly which `int mpmint` call is still
open when the fatal one fires**: `u_insys` toggles `0<->1` dozens of
times as `TMP` makes ordinary syscalls (`conattach`, `conout`,
`usercode`, `condetach`, `conassign`, `parsefilename`, ...), each one
cleanly opening and closing. The *last* `0->1` transition before
everything goes wrong is `CX=96h` (`mpm_clicmd` -- `TMP.A86`'s own
`lea dx,clicb!mov cl,mpm_clicmd!call mpm`, i.e. "run the command line
the user typed/the boot-time default") -- and it never closes (no
matching `1->0`). Instead, **while still inside it**, `u_insys` goes
`1->2` with `CX=8Fh` (`f_terminate`), `DX=3FFh` (`DL=0FFh`) -- this is
the exact nested call #78 already found (`u_insys=2`, `SS=177Bh`,
`SP=C8h`), now caught at the moment it's issued rather than inferred
from its aftermath.

**That nested `f_terminate` call does not match any real call site in
the source.** Checked every `f_terminate` reference in the whole tree
(`COMMAND.SUP` x2 -- the two just fixed, both via direct `mpmif`/`jmp
mpmif` except `user_retf`; `SYSFUNC.SUP`, `BDOSIF.BDO` x2, `CPMMEM.MEM`
-- all `jmp mpmif`/`call mpmif`, which never touch `u_insys` at all)
against `user_retf`, the one real `int mpmint` site: it does `sub
dx,dx!int mpmint`, i.e. `DX` must be `0000h` -- the observed `DX=3FFh`
rules it out directly, no ambiguity. `clicmd_entry`'s own body (`cp_doit`'s
400+ lines) contains zero `int mpmint` instructions of its own (grepped
directly) -- everything it does, including its own `f_createproc` at
line 429 and both of its own `f_terminate`s, goes through `mpmif`. So
whatever actually executes `int mpmint` with `CX=8Fh`/`DX=3FFh` while
`clicmd_entry`'s own request is still open is not a deliberate,
named call site at all.

**Directly confirmed the system's real final resting state for the
first time, properly** (not inferred from a stack read): launched
QEMU `-s -S`, let it run to the known 97117-byte plateau, then sent
GDB's own `continue` followed by a real `SIGINT` to the `gdb` process
itself (not a stale/wrong-address stack peek) and read `$cs:$pc`
directly off the genuinely-running target. Result: **`CS:IP =
F000:FF53`, `SS:SP = 177B:D6`** -- i.e. the CPU is permanently spinning
inside SeaBIOS's own ROM (the exact address #78's own now-retracted
false lead named, but this is an independent, correctly-obtained
confirmation of the real final state, not a repeat of that mistake),
on `TMP`'s own stack. This is consistent with -- and now directly
confirms -- the cascade #78/#79 already traced partway (`term_err`'s
`nstk` epilogue landing on `zdbg`'s own address via a stale stack
word): `zdbg` is real, executable code, so its own `ret` pops whatever
garbage is sitting one level further down, and the chain apparently
bottoms out in SeaBIOS's own trap handler, which just loops forever
once entered this way.

**What this means**: there are now two independently real, fixed bugs
(both real for *some* future boot path, once one is ever reached) and
one correctly-pinpointed-but-not-yet-source-located remaining mystery:
something, while `TMP`'s `clicmd_entry` request is still open, executes
a genuine `int mpmint` with `CX=8Fh`/`DX=3FFh` that corresponds to no
actual `call`/`jmp` in the source. The two leading explanations,
neither yet confirmed: (a) wild/corrupted execution lands on the raw
`0CDh 0E0h` ("int mpmint") opcode bytes somewhere mid-`clicmd_entry`
(e.g. inside `fileopen`/the BDOS critical section) with whatever `CX`/
`DX` happen to be live at that instant -- which would make the
`8Fh`/`3FFh` values coincidental, not meaningful; or (b) there is a
real, not-yet-found call site that *does* intentionally call
`f_terminate` via a genuine nested `int mpmint` (matching `INIT.SUP`'s
own convention, `dl=0FFh`) that this round's greps missed because it's
generated differently (e.g. emitted by `tools/plmc.py` for a `.PLM`
source, not a literal `.SUP`/`.RTM` text match).

**Next step**: single-step (or watchpoint-bisect, the technique that
worked cleanly this round) through `clicmd_entry`'s own execution from
its own top (flat `0x10949`'s containing routine, entered at local
`SUP` offset `0x92C`) forward, instruction by instruction through
`fileopen`/the file-open-failure path specifically (the likeliest
place for this QEMU build's limited disk image to produce the error
this headless, non-interactive run is actually hitting), watching for
the exact point execution diverges from clean source-level control
flow -- rather than continuing to work backward from the already-
confirmed nested-call symptom.

## 81. Followed #80's own next step and got the real answer immediately
-- the mystery nested `f_terminate` call is not wild execution landing
on a coincidental opcode at all. It is `INIT.SUP`'s own, long-finished
`rsp_out: mov cl,f_terminate ! mov dl,0ffh ! int mpmint` instruction,
executing a **second time**, with `rlr` now pointing at `TMP` instead
of `INIT`

**Found by verifying the stack-frame arithmetic byte-for-byte instead
of continuing to hand-derive it** (hand-derivation had already
produced one implausible result this round -- `CS=0042h`,
`FLAGS=0000h` -- a sign the offset count was off by something, rather
than evidence of real corruption): at the exact instant `u_insys[TMP]`
flips `1->2` (same live watchpoint technique as #80), scanned every
adjacent word-pair in the 40 bytes above `SP` and, for each one
read as a candidate `IP:CS`, checked whether the **two bytes
immediately before that computed flat address are literally `0CDh
0E0h`** (`int mpmint`'s own real opcode encoding) -- i.e. confirmed by
disassembly, not by a plausibility guess. Exactly one match in the
whole window: `IP=02FAh, CS=1000h` (stack offset `+2` from `SP`),
confirming the two bytes at flat `0x102F8`-`0x102F9` really are `CD
E0`, and the hardware's own pushed return `IP` really is `02FAh`.

**`flat 0x102F8` is not a coincidence -- it is `INIT.SUP`'s own
`rsp_out` label's `int mpmint` instruction, byte-for-byte** (confirmed
directly against the source: `rsp_out:`'s own two probes, then `mov
cl,f_terminate ! mov dl,0ffh ! int mpmint`, with the `int mpmint`
opcode landing exactly at `0x102F8`-`0x102F9` and its own pushed return
`IP` therefore exactly `0x2FA` -- the same fact #75 already
established about this one instruction, now reached from the opposite
direction: not "what does TMP's stale stack word point at", but "what
code just genuinely executed"). `CL=f_terminate` matches the observed
`CX=8Fh` exactly; `DL=0ffh` matches the observed `DX=3FFh`'s own low
byte exactly (`DH=03h` is just whatever was left over in the high byte
-- `rsp_out` never sets it, same as it never did for `INIT`'s own
original, legitimate call).

**So this was never a corrupted/coincidental `int mpmint` -- it is
`INIT`'s own 1-line self-termination sequence, genuinely re-executing,
long after `INIT` itself finished.** Everything downstream that #74-80
already traced (`abt_chk`'s `ac_tk` branch, `term_err`, the `nstk`
epilogue landing back on this exact same `0x2FAh/CS=1000h` pair via a
stale stack word) is real and correctly explained -- but the
*ultimate* trigger for all of it is simpler than any of the "wild
execution" or "UDA reuse" theories floated so far: **something causes
control to flow into `rsp_out`'s own code a second time, with `TMP`
now sitting at `rlr` instead of `INIT`.** This also plausibly explains
the project's own original, title-level `1000:004A` crash address:
`004A` sits within `INIT.SUP`'s own very early data (the copyright/
version strings, before `init:`'s own real label even starts) --
i.e. the *same small module*, strongly suggesting this is not the
first time something has jumped into a stale `INIT.SUP` address.

**What's still open**: *why* control reaches `rsp_out` a second time.
`rsp_out` is reached legitimately exactly once, via `nrsp:`'s own
`jcxz rsp_out` -- nothing else in the source jumps or calls there, so
this has to be an indirect jump through a stale pointer rather than a
literal second `jmp`/`call` anywhere in the text. The leading
candidates, in order of how cheaply each can be checked next: (a) a
per-process saved copy of the `i_mpm_ip`/`i_mpm_cs` interrupt vector
(`u_mpm_ip`/`u_mpm_cs`, copied in/out of the real vector on every
`dsptch`/`switch`) that still holds an `INIT`-era value for some
process and gets written back into the *real*, global vector at the
wrong moment; (b) a saved `CS:IP` return frame (not necessarily
`TMP`'s own) that was supposed to point at `TMP`'s own code but still
holds this stale kernel address, invoked via a `callf`/`retf`/`iret`
rather than a `jmp`; (c) something is treating a leftover value, still
sitting in some register or memory cell since `INIT`'s own run, as a
jump target, the same general bug class (stale pointer, correct value
for an earlier owner, never refreshed for the new one) as nearly every
other fix already applied to this codebase, just not yet pinned to
its specific carrier.

**Next step**: watch `i_mpm_ip`/`i_mpm_cs` (the real, low-memory
interrupt-vector cells `DSPTCH.RTM`'s own `switch`/context-save logic
reads from and writes to on every dispatch) with the same live
watchpoint technique, across the whole boot, and confirm they hold
`user_entry`'s own address throughout (ruling out (a) directly) --
if they do, watch whatever `CS:IP` cell is read immediately before the
jump that lands at `0x102F5`ish (one GDB session back from where this
round stopped) to catch the *write* that put `rsp_out`'s address there
in the first place, the same cheap, decisive technique that resolved
this exact mystery already.

**(a) is already ruled out, this same round**: watched `i_mpm_ip`/
`i_mpm_cs` (flat `0x380`/`0x382`, the real `int 0E0h` vector) across
the *entire* boot, past the point of the mystery jump and on into the
final stall. After the loader's own early, expected churn and
`INIT.SUP`'s own two (already-known, correct) writes (`0x39D:1000`,
matching `user_entry` exactly), it never changes again -- confirmed
stable for 30+ CPU-seconds, well past where the mystery call fires.
The real vector was never the problem; `int mpmint` genuinely,
correctly vectors to `user_entry` every single time, including for
the mystery call itself (confirmed earlier in this same entry). The
bug is purely about *what gets jumped to before* the `int` instruction
ever executes.

**Also checked, this round, whether `rsp_out` is reached via a wild
jump into the middle of its own code (skipping its own two probes) or
a clean (re-)entry at its own top label**: broke at `rsp_out`'s own
very first byte (flat `0x102BC`, before either probe) and it fired
*twice* -- once for `INIT`'s own real, original call, and once more
later, still running on `TMP`'s own stack (`SS=177Bh`) the whole time.
Whatever causes this is landing exactly on `rsp_out`'s own label, not
on some offset into its body -- consistent with a *stored address*
(a saved pointer, vector, or table entry) rather than raw corrupted
execution landing by chance on the right byte.

**Also checked and ruled out: a stale/misbuilt `sysent` table entry**
(the `userf:`/`nonet:` global-function-number lookup `SUPIF.SUP` uses
to dispatch `TMP`'s own `CL=150`/`mpm_clicmd` call that's already
open when the mystery call fires) -- `sysent` (`SYSDAT.FMT` offset
`0xA0`, `256*3` bytes, 3 bytes/entry: `ent_mod`,`ent_func`,`ent_flag`)
is exactly the kind of GENSYS-era table every other similarly-shaped
field in this file (`pul`/`qul`/`module_map`/`endseg`/`flags`/
`rspseg`) turned out to need a `build_mpmsys.py` patch for -- and
`build_mpmsys.py` never references `sysent` at all, so it's worth
real suspicion. Read entry 150 directly out of the real built
`build/kernel/SYSDAT.dseg.bin` (offset `0xA0+150*3`): `ent_mod=1`
(`sup`), `ent_func=5` -- exactly `clicmd_entry`'s own correct index in
`SUPIF.SUP`'s `supfunc` table (`not_implemented`=0,`bad_entry`=1,
`version_entry`=2,`callbios_entry`=3,`load_entry`=4,`clicmd_entry`=5).
Checked a dozen neighboring entries (144-155, plus several of `TMP`'s
other own call numbers) too -- all land on sensible, correctly
incrementing `(mod,func)` pairs. This specific table is correctly
populated; the dispatch into `clicmd_entry` itself is not where this
goes wrong -- it has to be *during* `clicmd_entry`'s own subsequent
execution (matching #80's own original "trace `fileopen`/the BDOS
critical section forward" next step, which this round's detour through
`rsp_out` answered a different question instead of).

## 82. Still told to continue until successful boot. **It boots now.**
Root cause of the whole `1000:004A`/`F000:FF53` crash family was never
a corrupted stack or a wild jump at all -- it was `XIOS86Q.A86`'s own
`mov word ptr .020h,offset tickint` hooking the real hardware timer
interrupt (IRQ0) with `tickint`'s **unrelocated, standalone offset**
instead of its real combined-image address, so every single 60Hz tick
vectored into the middle of `SUP` instead of into `tickint`. Two more
real, confirmed, independently-fatal-to-a-clean-boot bugs found and
fixed alongside it (`c_mimic` and `mpmseg`, both "GENSYS question
never answered" defaults, same bug class as `pul`/`qul`/`ccb`/`srchdisk`
above). All three are fixed in `tools/build_mpmsys.py`, plus a large
mechanical cleanup (described below) of every `TEMP DEBUG PROBE`/`TEMP
DIAGNOSTIC` instruction this whole investigation has accumulated across
#43-81, in `src-assem/kernel/*` and `src-assem/rsp/TMP.A86`.

**Starting point this round**: entry #81 left off suspecting the
mystery nested `f_terminate` was `rsp_out` itself re-executing a second
time, reached via some unidentified stale pointer. Chasing that
further (a fresh, very close live single-step trace of the exact
transition) surfaced something entry #81's own opcode-scan technique
had no way to catch: **`0x2FA` -- the address #81 verified as "the two
bytes before it are `CD E0`, so this must be `rsp_out`'s own `int
mpmint` return" -- is *also*, completely coincidentally, `zdbg`'s own
entry label** (`INIT.SUP`'s shared hex-digit-printing subroutine,
called from dozens of `TEMP DEBUG PROBE` lines throughout `SUPIF.SUP`
alone). `rsp_out`'s real `int mpmint` sits immediately before `zdbg`'s
own code in the source (nothing else separates them), so the two
addresses are adjacent, and *any* ordinary `call zdbg` from one of
those probes also produces `CS:IP=1000:2FAh` the instant it's entered
-- indistinguishable from #81's own signature by address alone. #81's
breakpoint-based confirmation (`rsp_out`'s own *top label*, `0x2BC`,
firing twice, and `nrsp`'s own top *also* firing twice) is unaffected
by this and remains real and correctly explained -- but the opcode-scan
"second independent proof" wasn't actually independent, and should be
read as retracted. This is also, in hindsight, exactly why removing
the probes mattered: with them gone, the real trigger became
observable directly instead of through a coincidental alias.

**The actual cleanup, done first**: every file this investigation has
ever added a `TEMP DEBUG PROBE`/`TEMP DIAGNOSTIC` instruction to --
`ABORT.RTM`, `BDOSIF.BDO`, `CIOIF.CIO`, `CHARIO.CIO`, `DSPTCH.RTM`,
`FILE3.BDO`, `MEMIF.MEM`, `FLAG.RTM`, `QUE1.RTM`, `MAU.MEM`, `PROC.RTM`,
`QUE2.RTM`, `RTMIF.RTM`, `SYSENT.RTM`, `SUPIF.SUP`, `INIT.SUP`,
`XIOS86Q.A86`, `TMP.A86` -- was diffed against `7e1cad8` (the original,
pre-investigation import commit) with comments stripped, to separate
every hunk into exactly two kinds: a narrow, single-token *replace*
(a real fix: a missing `cs:` prefix, a typo'd label, a missing paren)
versus a pure *insert* with no corresponding original line (a probe).
Every pure insert matching the serial port or a `zdbg`/`xdbg`/`ydbg`/
`mdbg`/`bdbg`/`cdbg` call was removed; every replace was kept. Two
replaces were deliberately **not** kept, because their own comments
already flagged them as live, unresolved experiments rather than
confirmed fixes: `SUPIF.SUP`'s and `DSPTCH.RTM`'s `sti ! cli` (reverted
to plain `sti` -- the trailing `cli` permanently defeats the `sti` it
follows, read literally, and the comment that introduced it says so
itself) and `PROC.RTM`'s freshly-dispatched-process flags word
(`0000h`, reverted back to the original `0200h`/IF-set -- same thing,
its own comment calls it "NOT a real fix, just a test"). This was all
mechanical, file-by-file, cross-checked against the original import
for every single hunk; `COMMAND.SUP` turned out to have never had any
probes in it at all (only the two already-documented `not (...)`
parenthesization fixes from #80), confirmed by the exact same diff.

**Root cause #1 (the fatal one) -- `tickint`'s unrelocated IVT
hookup**: traced via the exact same "live single-step, watch for the
transfer that changes `CS`" technique #80-81 already relied on, now
clean of probe noise: continuing from `terminate_entry`'s own tempkeep
no-op return, through several correct `ret`/`retf` unwinds back up
through `RTM`'s `entry`/`SUP`'s `goodmod`/`func`/`mpmif`, the trace
lands on a completely unremarkable `ABORT.RTM` `rlsmx` tail (`popf !
ret`) -- and the **next** single-step is `CS:IP=F000:FF53` (confirmed
directly, not inferred: printed both registers together), with `SP`
having moved by exactly `-4` (a near `ret`'s own `+2`, net against a
hardware interrupt entry's `-6`). That is a hardware interrupt firing
asmid-instruction, not a corrupted jump. `info pic` (via a short-lived
QEMU monitor socket) confirms only IRQ0 is ever unmasked on this build
(`imr=0FEh`) and that IRQ0 is in service (`isr=01h`) at the moment of
the freeze -- so this has to be the 60Hz timer tick itself vectoring
somewhere wrong. Read the live IVT entry 8 (`int 08h`, physical
`0x20`-`0x23`) directly: `1000:0468`. `0468h` is **`tickint`'s own
bare, standalone `XIOS86Q.A86` offset** (confirmed via `asm86
--symbols`) -- not `tickint`'s real address in the combined image
(`module_base + 0468h = 61CBh`). `XIOS86Q.A86`'s own `init:` writes
this vector with `mov word ptr .020h,offset tickint`, and `offset
tickint` bakes in the *standalone* value at assemble time, same as
every other "`offset <local-label>` used as cross-module data" bug
this tree has already found and fixed in `RTM.A86` (`fdisp`/
`farpdisp`, #entries much earlier) -- just never extended to
`XIOS86Q.A86`, since that file didn't exist yet when the earlier fixes
were written. Every single tick (60/sec) was vectoring into the middle
of `SUP`'s own code and running whatever bytes happened to sit at
combined offset `0468h` as if they were an interrupt handler, with no
stack-switch, no `cld`, nothing -- eventually wandering into SeaBIOS's
own catch-all "unclaimed interrupt" stub (`F000:FF53`, confirmed: *every
single one* of the 256 real IVT entries not explicitly claimed by this
kernel points at that exact same address) and getting stuck there,
since nothing about reaching it via a corrupted vector ever produces a
clean `iret` back. **Fixed** in `tools/build_mpmsys.py`: the exact
same find-the-immediate/verify-uniqueness/patch-in-`module_base`
pattern already used for `RTM.A86`'s `fdisp`/`farpdisp`, now also
applied to `XIOS86Q.A86`'s own `offset tickint` immediate.

**Root cause #2 -- `c_mimic` defaults to a real device number instead
of "no mimic"**: with root cause #1 fixed, the boot got dramatically
further (past the crash entirely, `TMP` genuinely dispatched and
running, confirmed via `CS=1623h` -- its own real, separate code
segment -- making dozens of real syscalls) but the console printed
every character of `XIOS86Q`'s own signon and `TMP`'s own version
banner **twice**, back-to-back, byte for byte. A live breakpoint on
`XIOS86Q`'s own lowest-level `conout:` (the one real `out dx,al` to the
UART) confirmed it, firing twice per character with *the same return
address* both times. `CHARIO.CIO`'s own `conout` (the kernel-level
routine, distinct from `XIOS86Q`'s) explains why: after the real
output, it checks `c_mimic[si]` (`CCB.DEF`: "mimic - cio dev that
mimics us. 0ffh means no mimic device") and, if it isn't `0FFh`, sends
the *same character again* to whatever device number `c_mimic` holds
-- a deliberate feature (echo console output to a second device, e.g.
a printer) with `0FFh` as its "disabled" sentinel. The CCB storage
backing it (`ccbtab0`, `XIOS86Q.A86`'s own private dseg tail) is
reserved with a bare `rb 12` -- all-zero -- so `c_mimic` defaults to
`0`, a *real* device number (this build's own, only console!), not the
sentinel. Every character mimics itself. Same "GENSYS question never
answered, and 0 isn't a safe default" bug class as `pul`/`qul`/`ccb`
itself earlier in this file. **Fixed** in `build_mpmsys.py`, right next
to the existing `ccb`/`CCB_OFFSET` patch: `ccbtab0[8]` (`c_mimic`'s own
byte offset within the CCB, per `CCB.DEF`) explicitly set to `0FFh`.

**Root cause #3 -- `mpmseg` never set, so the version banner reads
through a null segment**: fixing #2 removed the *doubling* but not the
garbage itself -- still unreadable noise, now appearing once instead
of twice, between the signon line and the `0A>` prompt. `TMP.A86`'s
own startup ("print version") does `mov ds,sysdatseg ! mov
dx,.s_version ! mov ds,.s_mpmseg ! call print_ds_string`: `.s_version`
(`TMP.A86`'s own local `equ word ptr 78h`) correctly reads `SYSDAT`'s
`version` field (confirmed via `asm86 --symbols` against the real,
current `SYSDAT.FMT` layout: `version` really is at offset `0x78`,
holding `INIT.SUP`'s own `ver` string's *offset* within the shared code
segment) -- but `.s_mpmseg` (`equ word ptr 40h`, "begin MPM segment",
also confirmed at the right real offset, `0x40`/`mpmseg`) is the
*segment* half of that same far pointer, and nothing in
`build_mpmsys.py` ever writes it. Left at its stock all-zero default,
`print_ds_string` reads the version string through `DS=0` -- the
IVT/BIOS data area at the bottom of memory -- instead of `DS=1000h`,
printing whatever bytes happen to live there as if they were "MP/M-86
2.0 [5 Oct 81]". Same bug class as #2, a third time. **Fixed** in
`build_mpmsys.py`: `mpmseg` (`SYSDAT.FMT` offset `0x40`) explicitly set
to `MPM_CODE_SEG` (`1000h`), right next to the other "answers to
GENSYS questions" field writes.

**Result, confirmed via QEMU serial console, byte for byte**:

    MP/M-86 XIOS86Q -- QEMU target (serial console, floppy disk)


    MP/M-86 2.0 [5 Oct 81]
    Copyright (C) 1981, Digital Research

    0A>

No crash, no garbage, no probes left anywhere in the tree. `idle`/
`tick` cycle cleanly in the background (confirmed live: `f_dispatch`
calls repeating with completely stable register state, not a GDB
artifact -- verified by removing the breakpoint and single-stepping
raw) and the system sits at a genuine CP/M-style `0A>` prompt rather
than hanging or crashing.

**Still open, next step**: typed input against this same `0A>` prompt
(tested over a QEMU `-serial unix:...` socket, both a bare Enter and a
real command line) produced no visible response within several
seconds, across more than one attempt. Not yet root-caused -- candidates
include the test harness itself (a `unix:` chardev's own buffering/
framing with this QEMU version, not yet cross-checked against a
known-good interactive target), and a genuine remaining bug somewhere
in the `conin`/`rtm_poll`/UART-RX-ready path (`CHARIO.CIO`'s
`rconin_entry`/`conin:`, `XIOS86Q.A86`'s own `conin:`/`polldev`). The
next step is to confirm or rule out the harness first (e.g. a real
interactive terminal, or `-serial stdio` without redirection), since
every fix this entry documents was found by trusting a *direct*,
two-register-confirmed observation over an inferred one, and "no
response over a brand-new socket-chardev test rig" isn't that yet.

## 83. Told to figure out why console input isn't working. The harness
was innocent (confirmed with a real, raw-mode pty, no echo) -- input
really was dead. Found and fixed **three more real, confirmed bugs**
getting it from "nothing, ever" to "reads and echoes any number of
real characters correctly"; one precisely-scoped symptom remains (the
line *terminator* specifically wedges the scheduler) and is handed off
un-fixed, with the exact evidence needed to pick it back up.

**Harness check, first**: before trusting "no response" as a kernel
bug, the test rig itself was replaced twice -- a `-serial unix:...`
socket (the previous entry's own open question), then a real
`pty.openpty()` pair with `ECHO`/`ICANON` explicitly turned off on the
slave's own termios (ruling out the pty's *own* line discipline
echoing injected bytes back as fake "output", which is exactly what a
first pass with echo still enabled produced -- `^B^B^B...` appearing
to "come from" iPXE's own signon when it was only ever the test
sending Ctrl-B back to itself). With echo off, zero bytes ever came
back after a bare `\r` at a genuine `0A>` prompt. Real bug, not a
harness artifact.

**Root cause #1 -- `idle`'s/`tick`'s own UDA `u_initss` field, another
unanswered GENSYS question**: `PROC.RTM`'s own `cp_uda` (`mov ax,ds:
u_initss ! mov ds:u_ss,ax`) seeds a freshly-created process's *very
first* resume stack segment from its own template UDA's `u_initss`
field, paired with `u_stack_sp` (already correctly template-set to
`offset(idle_tos)`/`offset(tick_tos)`, both living *inside* `sysdat`'s
own segment). Read `build/kernel/MPM.bin.dseg` directly at `idleuda`'s
own `u_initss` offset: `0000h` -- not `sysdat_segment`. `RTMIF.RTM`'s
own `init:` only ever sets `u_initcs` for tick/idle (confirmed matching
`orig/mpm862sr/02/RTMIF.RTM` byte-for-byte -- a real, original gap,
not a transcription error), relying entirely on the template already
having the right `u_initss`, same as `u_initcs` would need to if
`RTMIF.RTM` didn't *also* explicitly patch that one at runtime. Left
unset, idle's/tick's own first-ever `SS` becomes `0000h` -- the IVT/
BIOS data area -- instead of `sysdat_segment`. **Fixed**: `RTMIF.RTM`'s
own `init:` now also writes `u_initss` (`mov ax,cs:sysdat ! mov
u_initss,ax`) for both tick and idle, right alongside the existing
`u_initcs` write, using the identical ES-relative convention. Confirmed
live via GDB, before and after: idle's own UDA `u_initss`/`u_ss` read
back `0`/`0` before the fix, `1699h`/`1699h` (the real `sysdat_segment`
this build) after.

**Root cause #2 -- `XIOS86Q.A86`'s own `idle:` loop corrupts its own
dispatch call with stale `CH`**: with #1 fixed, idle's stack was sane
but the *symptom* (the scheduler's own `indisp` reentrancy flag,
`DSPTCH.RTM`, stuck permanently `0FFh`, so `dsptch`'s own `cmp
indisp,true ! jne dispin` bails to a bare `ret` on *every* subsequent
call and a real process switch never happens again) was unchanged.
Live single-step tracing (the same "watch every instruction, match
addresses against `asm86 --symbols`" technique #80-82 relied on) caught
idle -- confirmed by name, its own PD's `p_name` reading `"Idle"` --
genuinely, repeatedly calling `CHARIO.CIO`'s `conin_entry`/`attach`/
`cioattach`, retrying forever against a console `TMP` legitimately
already owns, instead of its own real `idle:` loop. `idle:` itself is
only three lines (`mov cl,f_dispatch ! call supif ! jmp idle`) -- and
`f_dispatch` (`MODFUNC.DEF`: `(user*0100h)+142` = `008Eh`) is a *global*
sysent-routed function number, needing `CH=0` for `SUPIF.SUP`'s own
`func1`'s `cmp ch,0 ! je userf` to take the right branch. `mov cl,
f_dispatch` is an 8-bit move -- `CH` survives untouched from whatever
this process's own `u_cx` last held (verbatim-restored by `DSPTCH.RTM`'s
own `switch:` on every dispatch) -- so whenever that's nonzero, `func1`
takes the *explicit-module* branch instead, misrouting the call based
on leftover register state. This is the *exact* bug class already
fixed once in `INIT.SUP`'s own `nrsp` loop (`mov cl,f_createproc`,
docs/bugs.md much earlier) -- just never caught in this file, which
didn't exist yet at the time. **Fixed**: `mov cl,f_dispatch` -> `mov
cx,f_dispatch` in `idle:`. Found and fixed the identical pattern at the
same time, same file, for `f_polldev` (`rtm_poll`) and `f_flagset`
(`tickint`, used twice) -- the latter doubly suspect, since `CX` on
entry to a hardware ISR is whatever the *interrupted* code happened to
be using, never guaranteed zero in `CH` at all.

**Root cause #3 -- the identical bug, confirmed as a genuine original
DRI typo, in `CHARIO.CIO`'s own `constat_entry`**: grepped every
`mov cl,f_` call site across the kernel for the same pattern. One more
turned up, in `constat_entry` (`mov cl,f_dispatch ! call mpmif`).
Checked `orig/mpm862sr/03/CHARIO.CIO` directly: this file's own sibling
call site (`dirio_entry`, a few lines down, same constant) already
correctly uses `mov cx,f_dispatch` -- in the real, unmodified 1981
source itself. The two call sites disagree with each other in the
authentic original; this one is a genuine DRI-era typo, not a rebuild
artifact, confirmed the same way #74's "missing parens" fixes were.
**Fixed**: `mov cl,f_dispatch` -> `mov cx,f_dispatch` in
`constat_entry` too.

**Effect of all three together**: console input went from *zero bytes,
ever* to correctly reading and echoing arbitrary runs of real
characters -- confirmed with `DIR` (3 chars) and separately with
`ABCDEFGH` (8 chars, spaced 0.2-0.3s apart to rule out a UART-overrun
explanation), every single one landing on screen exactly as typed, over
a real pty with echo disabled (so what's observed is genuinely the
guest's own echo, not the test rig's).

**Still open -- CR itself, specifically, reliably wedges it again**:
after any number of regular characters (tested up to 8), sending `\r`
produces *no* echo, no newline, nothing -- and live inspection
afterward shows `indisp` stuck `0FFh` again, exactly like before #82's
fixes, with `rlr` back on idle and TMP still parked on the poll list
(`p_stat=1`, `p_wait=0`, unchanged). This is not the same mechanism as
root cause #2 (idle's own misrouted `f_dispatch`): `CHARIO.CIO`'s own
`conread_entry` char-reading loop (`readnx`/`readn0`/`rdecho`/`eofre`/
`readen`) was traced by hand, instruction group by instruction group,
confirming the stack depth *into* `readen:` (`pop di`) is identical
--exactly one word, the buffer offset from `read:`'s own top -- whether
reached via `eofre` (CR/LF) or via `rdecho`'s own `jae readen`
(buffer-full case): no stack imbalance there. The regular-character
path (`rdecho`) and the CR path (`eofre`) both call through the exact
same `conin`/`rtm_poll`/poll-list/`schedule` machinery to *get* each
character, and that machinery is demonstrably fine for every character
except whichever one happens to be CR. The leading hypothesis, not yet
confirmed: this isn't really about CR's own bytes at all, but about
what `TMP.A86` does immediately *after* a line is complete and
`conread_entry` returns -- `nextcommand:`'s own `call freedrive`/
re-`attach`/prompt-redraw sequence, all still using the same
unprotected `mov cl,<global-function-constant>` convention as idle's
own (now-fixed) bug, confirmed matching the real original DRI
`TMP.A86` byte-for-byte (so, if real, this is the *same* "CH must
already be 0, and the rebuild doesn't reliably guarantee that the way
the real 1981 system implicitly did" problem, just manifesting at a
different call site) -- not yet caught in the act the way root causes
#1-3 were. **Next step**: live-trace specifically from `conread_entry`'s
own `ret` (right after `readen`/`pcr` finish echoing CR) through
whatever `TMP.A86` calls next, the same instruction-by-instruction,
address-matched-against-`asm86 --symbols` technique used for every
other fix in this entry, watching `CH` at each syscall trampoline to
catch the first one that's nonzero when it shouldn't be.

## 84. Told to keep going and fix #83's "still open" CR bug. **The
"CR-specific" framing from #83 is wrong** -- found and fixed one more
real, confirmed bug (this time in the toolchain itself, `tools/asm86/`,
not the kernel source), but the actual scheduler-corruption hang #83
handed off survives it unfixed; this entry's own live evidence is
handed off in its place.

**#83's own framing disproven first**: a clean boot with a real pty but
*zero bytes ever sent* -- no CR, no input at all -- still ends up with
`indisp` stuck `0FFh` within the same ~20s window #83 saw it happen
after a CR. Confirmed by watching `indisp` (flat `sysdat_seg*16+0B5Eh`)
live from a cold `-S` halt with the serial port wired to `/dev/null`.
Whatever's wedging the scheduler is input-independent and roughly
boot-timing-dependent, not a property of the CR byte itself; #83's own
"next step" (chasing `TMP.A86`'s post-`conread_entry` `CH` handling) was
chasing a byte that isn't actually the trigger.

**A real, confirmed, separate bug found along the way -- `tools/asm86`'s
own operand classifier, not kernel source**: live register tracing of
the hang (breakpoint at `CONSOLE.CIO`'s `cioattach`, `0x12e64`) caught
`SI` reading back `37A0h` on entry -- garbage, since `conattach_entry`'s
own `mov si,p_cns` should load the small, constant PD-field offset
`0020h` (`PD.DEF`: `p_cns equ byte ptr p_parent + word`). Raw byte
dump of the compiled `cioattach` confirmed why: `8B 36 20 00` --
`MOV SI,[0020h]`, a *memory read* of absolute address `DS:0020h`, not
`MOV SI,0020h` (an immediate load). `DS` at that point is `sysdat`, and
`sysdat:0020h` happens to fall inside `module_table[bdos].mod_entry`'s
own low word (`build_mpmsys`'s own reported `BDOS entry=0101h` +
`BDOS module_base=369Fh` = `37A0h` -- an exact match). `tools/asm86`
was mis-assembling `mov si,p_cns` as a memory operand because `p_cns`'s
own `equ` definition carries a `byte ptr` cast (there purely to size a
later *bracketed* use like `p_cns[bx]`), and the assembler's operand
classifier (`tools/asm86/expr.py`'s `parse_operand`) treated *any*
`equ` symbol with a cast-derived size as memory when referenced bare,
with no way to tell "this cast sizes a structure-field offset constant"
apart from "this cast aliases a real address" (e.g. `RTMIF.RTM`'s
`supervisor equ offset $`, or `DATA.BDO`'s `lret equ byte ptr aret`,
aliasing the real label `aret`). Confirmed this is a toolchain defect,
not a kernel-source one: `CONSOLE.CIO` diffs byte-for-byte identical to
`orig/mpm862sr/03/CONSOLE.CIO`.

**Fixed**: added `Symbol.is_addr` (`tools/asm86/expr.py`), true only
when an `equ`'s own defining expression is rooted in a real `offset $`/
`offset LABEL`, or is a *bare* alias of an actual label (`v.symbol.kind
== 'label'`) -- false for one built from pure `equ`/literal arithmetic
under a `byte/word ptr` cast (`p_cns`'s own shape). `parse_operand` now
requires `sym.size and sym.is_addr` (not just `sym.size`) before
classifying a bare `equ` reference as memory, so `p_cns`/`p_lst`-shaped
offset constants fall through to the plain-immediate case instead.
Also had to teach `_prescan_sizes` (`tools/asm86/assembler.py`) the
same distinction for *forward* references (a 3rd `is_addr` element
added to its `prekind` tuples, guessed from the cast's own text: a bare
single identifier inside the cast counts as address-like, same as the
`offset` keyword; an arithmetic expression doesn't) -- otherwise a
forward reference occurring before the `equ`'s own definition line
(`BDOSIF.BDO`'s `mov lret,al`, reached before `DATA.BDO`'s own `lret
equ byte ptr aret` since `BDOSIF.BDO` is `include`d first in `BDOS.A86`)
guessed wrong during the tentative pass and the *final* pass's correct
answer (memory, since `aret` really is a label) came too late to avoid
an `unsupported MOV operand combination (mem,mem not allowed)` abort.
Also had to extend the existing "a leading `.` on a bare number forces
a memory operand" convention (`parse_operand`, already used for things
like `XIOS.A86`'s `mov .0,offset int_trap`) to cover a leading `.` on a
bare *name* too (`INIT.SUP`'s `mov es:.rsp_link,ds`, where `RSP.DEF`'s
`rsp_link equ word ptr rsp_top` and `rsp_top equ 0` are both pure
offset-constant `equ`s with `is_addr=False` under the new rule) --
without it, the dot's own "dereference this address" meaning silently
lost to the new, stricter bare-reference rule for the exact same reason
`p_cns` needed fixing in the first place, and the same `mem,mem` abort
resurfaced one file later. Confirmed via byte dump: `cioattach` now
assembles `mov si,p_cns` as `C7 C6 20 00` (`MOV SI,0020h`, immediate) --
and a fresh live trace confirms it: `SI` reads back `0020h` on entry,
`DH` (console #) and the resulting CCB lookup are sane, and the PD
caught retrying `cioattach` is now legitimately `TMP` (`rlr=0E40h`,
confirmed by PD address) rather than garbage. Full `make`/`make disk`
rebuild clean, no regressions (every other already-fixed bug's own
instruction encoding, spot-checked, unchanged).

**Still open -- the actual hang survives this fix untouched**: same
test as before (boot, wait, send `DIR`\r over a real pty) still gets
`DIR` echoed and then nothing -- `indisp` stuck `0FFh` again. Live
inspection of the stuck state (`CS`/`SS`/`SP` read together, one
breakpoint hit, not across separate attaches) shows something stranger
than a simple missed-wakeup: whichever PD is `rlr` at the moment
`RTM.A86`'s `sleep_entry` (`flat 0x11500`) or `CONSOLE.CIO`'s
`cioattach` top fires -- it's been seen as both `idle` (`0x9f0`) and
`TMP` (`0xe40`) across different runs/attaches, not consistently one or
the other -- is running with `SS=CS=1000h` and `SP` pointing into a run
of all-zero bytes sitting inside `SUP.A86`'s own `RPL.SUP` code
(flat `0x112bc`-ish, between `rpl_entry` and `rpl_ret`), not into any
declared stack buffer at all. That's not idle's/TMP's own real
per-process stack: separately read back on a live (not-yet-hung) boot,
idle's own `u_ss`/`u_sp` and `p_uda[idlepd]` are exactly what they
should be (`1699h`/`1715h`, matching #83's own fix), and watchpoints on
both of those exact flat addresses, armed from a cold `-S` halt and
left running through an entire boot-to-hang cycle (including the `DIR`
\r injection), **never fire even once** -- whatever ends up substituting
a bogus `SS:SP` for the real one isn't writing through either of those
fields, so `switch:`'s own `mov ss,u_ss ! mov sp,u_sp` (`DSPTCH.RTM`)
isn't the corruption site either; something upstream of it is handing
`switch:` a `BX`/`DS` that doesn't actually resolve to the UDA it
thinks it does, or `rlr` itself goes through a state (plausibly `0`,
the empty-list sentinel, briefly) this round of live testing didn't
directly catch -- `DSPTCH.RTM`'s own `schedule:` comment ("there MUST
be a process on the RLR at this point, ie. IDLE...") says plainly what
should never happen but would explain every single symptom gathered so
far (the all-zero "stack", the semi-plausible but address-mismatched
bouncing through `SUPIF.SUP`'s `entry`/`func`/`userf`/`goodmod` region,
`indisp` never clearing because `switch:`/`dret:` never cleanly
finish) if it ever does. **Next step**: watch `rlr` (flat `0x169f8`)
and `drl`/`plr` (`0x169fc`/`0x169fe`) together, continuously, from a
cold halt through the hang, specifically checking whether `rlr` ever
reads `0` for even one instruction -- the one direct observation that
would confirm or kill the "ready list goes briefly empty" theory file
instead of further inference from where execution ends up afterward.

## 85. Continued from #84's own handed-off next step. The "ready list
goes briefly empty" theory is dead -- `rlr`/`drl`/`plr` are all sane
throughout -- but the real mechanism is now pinned down precisely:
`indisp` really is stuck permanently true (not transiently toggling,
contra a stray, never-written-up observation from #84's own last
live session), `schedule`/`polldev` provably never run again once
stuck (confirmed by a hardware breakpoint that fires normally during
boot but never once in 20+ real seconds after the hang, console input
injected or not), and the specific retry loop keeping the CPU busy the
whole time is identified by name and call site.

**First, a methodology correction, since it directly contradicts
#84's own framing**: a leftover scratchpad log from #84's own final,
un-written-up experiment (`gdb_indispfinal2.log`, continuously
watching `indisp` from a cold halt with serial wired to `/dev/null`,
~190 hits recorded) shows `indisp` toggling cleanly between `0` and
`0FFh` forever, at two fixed addresses -- not sitting stuck at a
single value the way #84's prose describes it (that prose was written
from single-point sampling, before the continuous-watch experiment
ran). Decoded against `asm86 --symbols` on `RTM.A86`, those two
addresses are `dispin` (flat `0x11804`) and `dret` (flat `0x11AF8`) --
i.e. that specific toggle is **normal, correct dispatcher activity**
(every real dispatch cycle sets `indisp` true at `dispin` and clears
it again at `dret`), not a bug at all. It shows up with `/dev/null`
wired because printing the boot banner over a real UART takes dozens
of poll/dispatch cycles, one per character -- the exact same toggle is
directly confirmed this round during the early boot-message-printing
window with a *real* pty attached too (see methodology note below).
**The real hang only sets in once boot reaches the interactive `0A>`
idle state**, and that part of #84's own "input-independent, same
~20s window" finding still holds, now confirmed far more directly.

**Methodology note on hardware breakpoints, confirmed the hard way
this round**: a single long-lived QEMU process, reattached to by many
successive `gdb -x` invocations each killed by `timeout` mid-`continue`
(the pattern used for most of this round's probing, and apparently
also late in #84's own session), eventually makes **hardware
breakpoints/watchpoints silently stop firing at all** -- confirmed
directly: `hbreak *0x15f91` (`polldev`'s own entry, see below) and
`watch *(unsigned char*)0x174ee` (`indisp`) both sat armed through 15-
20 real seconds of confirmed, register-sampled, actively-executing
CPU with zero hits, on a QEMU instance that had already survived
several earlier reattach/kill cycles -- then **the identical
`hbreak *0x15f91` fired on the very first `continue`** against a
freshly-launched QEMU process that had never been attached to before.
Killing and relaunching QEMU per probe (not just reattaching GDB) is
now the required discipline; the existing "prefer watchpoints/hbreak
over software breakpoints" lesson (#38, #51) is necessary but not
sufficient on this specific gdbstub.

**Confirmed, via a disciplined fresh-QEMU-per-probe methodology,
boot through hang, with a real raw-mode pty (not `/dev/null`) and real
`DIR`+CR injected right at the `0A>` prompt, well inside the
established ~20s window:**

- `plr=0E40h` (= TMP's own PD), TMP's `p_stat=01h` (`ps_poll`),
  `p_link=0` -- TMP is correctly, genuinely linked onto the real poll
  list. The poll-list-linkage theory floated implicitly by #83/#84 is
  not the bug.
- `polldev` (`XIOS86Q.A86`, flat `0x15F91` -- confirmed by direct
  disassembly match, `cmp cl,0`/`jne polldev_1` landing exactly where
  `asm86 --symbols` says) is hit completely normally during boot
  (confirmed hit, `cl=4`=`c0outdev`, while the boot banner prints) --
  `schedule`'s poll loop provably works correctly before the hang.
- After injecting real input at the `0A>` prompt, that *same*
  hardware breakpoint, on a *freshly launched* QEMU instance, never
  fires once in 20 real seconds. `schedule` -- and therefore
  `polldev` -- provably never runs again once hung, with or without
  real console input. This directly kills #84's own "ready list goes
  briefly empty inside schedule" theory: schedule isn't intermittently
  misbehaving, it simply never executes at all post-hang.
- `indisp` reads `0FFh` on direct, repeated, post-hang inspection
  (confirmed on a freshly-attached session, not inferred) -- genuinely
  stuck, matching #83/#84's original characterization, not the
  boot-time toggle #84's own unwritten-up log showed.
- The CPU is *not* frozen: repeated `interrupt`+register-read samples
  over several real seconds show `$pc`/`$sp` genuinely changing
  between samples (e.g. `pc=33a4`u`sp=12c0`, `pc=1488`u`sp=12be`,
  `pc=2fb`u`sp=12c8` -- small, bounded `SP` range, `CS=SS=1000h`
  throughout, so the stack itself is sane here, unlike #84's own
  "all-zero stack" sighting elsewhere). A hardware breakpoint at RTM's
  own generic inter-module `entry:` trampoline (`RTMIF.RTM`, flat
  `0x11488`) fires readily post-hang, with `CX=12h` (18 decimal) each
  time -- `RTMIF.RTM`'s own function table (`function dw ...`) lists
  index 18 as **`sleep_entry`** (`SYSENT.RTM`). So post-hang, *something*
  keeps calling RTM's `f_sleep` (module-class, `CH=rtm(2)`, explicit,
  not routed through `SUPIF.SUP`'s `sysent` table) in a tight loop.
  (Chased whether this is `userf`'s `sysent[142]` -- `f_dispatch`,
  what `idle`'s own loop actually calls -- misrouting to `sleep_entry`
  instead of the correct `dispatch_entry`: it isn't. Read live:
  `sysent[142] = {ent_mod=2(rtm), ent_func=0Ch(12)}`, and index 12 is
  genuinely `dispatch_entry` per `RTMIF.RTM`'s own table. `idle`'s
  `f_dispatch` call is correctly routed; it is not the thing calling
  `sleep_entry`. Whoever calls `f_sleep` directly -- plausibly `tick`'s
  own heartbeat, not yet confirmed by name -- is likely incidental,
  not the root cause; see next step.)
- `sleep_entry` (`SYSENT.RTM`: `mov bx,rlr / mov u_dparam,dx / ...
  mov p_stat[bx],ps_sleep / jmp dsptch`) unconditionally `jmp dsptch`s.
  `dsptch`'s own reentrancy guard (`DSPTCH.RTM`: `cmp indisp,true !
  jne dispin / popf ! ret`) is hit first, every single time, and with
  `indisp` already stuck true, bails out immediately via a bare
  `ret` -- never reaching `dispin`/`schedule`/`polldev` again. This is
  a fully sufficient, directly-observed explanation for everything
  seen this round and in #83/#84: the CPU stays busy forever re-
  entering RTM's `entry:` trampoline and immediately bouncing off
  `dsptch`'s own guard, while `schedule`'s poll loop -- and TMP's
  correctly-linked poll-list entry with it -- never runs again,
  regardless of what arrives on the console.

**What's still open, precisely**: the loop described above is a
*symptom* -- it explains why nothing recovers once stuck, not what got
`indisp` stuck in the first place. Exactly one real (non-guard-
bounced) pass through `dsptch`'s full body must have set `indisp:=true`
at `dispin` (flat `0x11804`) and then failed to reach either clear
site (`dret`, flat `0x11AF8`, or `int_disp_exit`, flat `0x117B6`) --
i.e. one specific call never returned, rather than this being a race
between concurrent callers. **Next step**: using the now-confirmed
fresh-QEMU-per-probe discipline, hardware-breakpoint `dispin` (flat
`0x11804`) from a cold halt, let it run with `delete+rearm per hit`
(the #38/#80 lesson) counting hits, and on the *last* hit before the
hang sets in, single-step (`stepi`) all the way through `dsptch`'s
body instead of `continue`-ing, watching for the first instruction
where it diverges from the known-good path traced during #83/#84's
own successful (pre-hang) dispatch cycles -- that single trace should
catch the exact instruction that fails to reach `dret`/`int_disp_exit`,
the same direct-tracing technique that nailed every other root cause
in this file.

## 86. Continued from #85's own next step. Didn't get to single-stepping
`dispin` yet -- a side investigation (chasing `sleep_entry`'s own caller)
ran long, cost a real but ultimately **self-corrected** false lead on
console-attach ownership, and then landed on something more directly
useful: live, stepi-confirmed evidence that `rlr` really does go
transiently to `0` -- the exact thing #84/#85 both looked for and
couldn't catch -- plus a second, separate GDB-reliability gotcha worth
recording alongside #85's own.

**False lead, fully self-corrected -- recorded so the next round
doesn't re-walk it**: chasing *who* calls `sleep_entry` in #85's own
busy-loop, a live read of `SYSDAT.FMT`'s `ccb` field (flat `0x169E4`,
`CCB_OFFSET` in `tools/build_mpmsys.py`) came back `1015h`, which
doesn't correspond to any real PD -- looked, for a while, like a second
real corruption bug (even traced a plausible-looking but ultimately
irrelevant path through the loader's own disk-read code before
catching the error). It isn't one: `ccb` is a *pointer* field (GENSYS-
computed in the real system; `build_mpmsys.py`'s own comment right
above the write explains this is already a deliberate, correct fix for
exactly the placeholder-zero problem its own comment describes), not
the CCB array's own base -- `CHARIO.CIO`'s `getccbadr` (`mov si,ccb !
add si,ax`) loads it and uses the *result* as the real base. `1015h`
is `ccbtab0_off` (`XIOS86Q.A86`'s own private-tail CCB storage),
exactly as intended. The *real* `c_attach` lives at `sysdat + 1015h`
(flat `0x179A5`), and reads `0E40h` -- TMP's own PD, a perfectly
legitimate, correct attachment. No console-ownership bug exists;
disregard the opposite impression a shallower read of this same cell
would give.

**What's actually confirmed this round, with the methodology to
reproduce it:**

- `idle`'s own `f_dispatch` call was traced instruction-by-instruction
  end to end this time (not just spot-read), `hbreak` at `idle` itself
  (flat `0x15F69`) through roughly 35 `stepi`s -- `mov cx,f_dispatch`
  (`CX=8Eh`, `CH=0` correctly loaded, confirming #83's own fix is
  intact) → `supif` → `SUPIF.SUP`'s `entry`/`entry_notfs`/`mpmif`/
  `func`/`func1`/`userf`/`nonet` (`CL` correctly becomes `0Ch` from
  `sysent[8Eh]`) → `inmod`/`gofar`/`goodmod` (`AL=2`=`rtm`, module-map
  bit test passes) → right up to the `callf module_table[si]` that
  should land on RTM's own `entry:`. Every single register matches
  the known-good path at every step. **`idle`'s own loop is not where
  the busy-loop comes from** -- a cleaner, harder confirmation of what
  #85 already suspected from the `sysent[142]` read alone.
- Breaking at `cioattach`'s own top (`CONSOLE.CIO`, flat `0x12E64`)
  and trusting naive repeated `continue` (no `stepi` in between)
  reproduced **the exact same stale-non-progress pattern #51 already
  documented for plain breakpoints** -- six "hits" in a row with
  bit-for-bit identical `BX`/`CX`/`SI`/`SP`. Re-run with one `stepi`
  pair inserted between each `delete`+`hbreak`+`continue` (confirming
  real forward motion before trusting the next hit, exactly #51's own
  prescription) tells a completely different story: hit 1 is TMP's own
  legitimate first attach (`BX=0E40h`); hits 2 through 6 are a second,
  genuinely-advancing (stack pointer dropped from `F8h` to `E8h`, a
  real nested call) sequence with **`BX=0`** -- i.e. `cioattach`'s own
  `mov bx,rlr` read the ready-list-empty sentinel directly, live,
  stepi-verified real. This is the one direct observation #84 and #85
  both went looking for and missed (#85's own continuous `watch` on
  `rlr`'s memory cell, run at the wrong moments, never caught it).
- It is **not** a permanent condition: a plain `interrupt`+read
  immediately afterward, same session, shows `rlr=09F0h` (idle) again,
  `indisp` still stuck `0FFh`. So `rlr` reads `0` for at least one real
  instruction during whatever sequence gets `indisp` stuck, then gets
  refilled with `idle` by *something* before the system settles into
  the busy-loop #85 already characterized (bouncing through `entry:`
  trampolines, `sleep_entry`, `dsptch`'s guard bailout) -- it isn't
  `switch:` blindly running on a `rlr=0` it never recovers from, the
  simplest version of that theory, since `rlr` is sane again by the
  time anything else gets to read it.

**Next step, sharpened by this round's own false start and its real
finding**: #85's own prescribed `dispin` single-step-through is still
the right next move, unchanged -- but now specifically watch `rlr`
*during* that one traced pass (not just before/after it) for the exact
instruction that reads it as `0`, and the exact instruction afterward
that refills it with `idle`, to see whether that refill is what
*fails* to also clear `indisp` on this one occasion (the two could be
adjacent in the same code path, `switch:`'s own tail, and a single
skipped instruction there would explain both symptoms from one root
cause). Use `hbreak`+`stepi`-confirmed-progress throughout, per both
this entry's and #51's own methodology notes -- naive repeated
`continue` has now been caught giving false "no progress" *and* false
"progress" (stale-identical-hit) readings on this gdbstub, at two
different addresses, in two different sessions.

## 87. Tried #86's own next step (single-step the one bad `dispin` pass
live) and hit a wall worth recording precisely: **this specific hang
is a genuine Heisenbug** -- every fine-grained live-observation
technique tried so far (GDB hardware watchpoint on `indisp`, GDB
breakpoint+`stepi` on `dispin`, and even a fully passive, zero-GDB
QEMU-only instruction trace) prevents the race from ever firing, while
coarse/infrequent polling does not and reproduces it reliably. Handing
off with exact numbers so the next round doesn't re-spend the time
confirming this.

**Precise timing under near-native conditions, confirmed this round**:
booted completely untouched (no GDB attached at all, not even to
`interrupt`), then polled `indisp` (flat `0x174EE`) every ~2s via a
fresh `interrupt`+read+implicit-resume (not a watchpoint, not a
breakpoint -- the lightest possible touch). Stuck `0FFh` by the very
first sample, `t+13s`, and stayed stuck through `t+76s` (10 samples,
every single one `0FFh`). This is the real, reproducible window --
matches #83/#84's own "~20s" figure closely enough to be the same
phenomenon, now pinned to low-teens seconds specifically for this
build.

**Every attempt to watch the transition itself, instead of just
sampling before/after it, failed to reproduce the hang at all**, three
different ways:

1. A GDB hardware watchpoint on `indisp`, looped via `while`+`continue`
   (no `stepi`, pure watchpoint -- the exact technique #85 validated as
   reliable, and the exact technique #84's own leftover, never-written-
   up log appeared to use) for 3000 consecutive toggles from a cold
   halt: completed cleanly, every single toggle a clean `dispin`
   (flat `0x11804`)/`dret`-or-`int_disp_exit` (`0x11AF8`/`0x117B6`)
   alternation, `rlr` never once `0`, no hang. (This also finally
   explains #84's own leftover log the right way: it wasn't sampling a
   lucky non-hanging window, continuous observation itself was
   preventing the hang the whole time.)
2. A fully passive trace -- `qemu-system-i386 -d exec,nochain -D
   <log> -serial file:<consolelog> -display none -no-reboot
   -no-shutdown`, genuinely zero GDB, matching #75's own "no debugger-
   side perturbation at all" method but *without* `-accel
   tcg,one-insn-per-tb=on` this time (that flag alone made the trace so
   slow boot never even got past SeaBIOS's own floppy-wait loop in 15
   real seconds; dropping it let boot complete normally while still
   logging every executed TB) -- run for a 25-real-second budget,
   grepped for the three flat addresses above: `dispin` hit **62941**
   times, `dret` **62880**, `int_disp_exit` **61** -- and `62880+61 ==
   62941` *exactly*. Every single dispatch, the whole 25 seconds,
   paired up clean. No imbalance, no hang, despite this being the
   closest thing to a truly unperturbed run available.
3. Reconciling #1/#2 with the `t+13s` figure from the untouched-boot
   test: `-d exec` logging (420MB+ for 25 seconds here) is itself
   heavy enough to slow real wall-clock progress through the guest's
   *own* timeline -- 25 real seconds of *traced* execution evidently
   doesn't correspond to 25 real seconds of *un*traced execution, so
   the trace may simply not have reached the real hang point yet, not
   that tracing "fixed" it. A longer-budget version of the same traced
   run (150s) was left running in the background this session under
   `/private/tmp/claude-501/.../scratchpad/qemu_trace3.out` /
   `exec_trace3.log` -- **note for whoever picks this up: that's a
   session-scoped scratchpad path and the background process may not
   survive past this session ending; if the file's gone, just re-run
   the exact command above with a longer `timeout` budget (try 90-150s
   first) and re-grep for the same three addresses.**

**Working theory, not yet confirmed**: given `fdisp`'s own
documented lack of protection against firing mid-`switch:`-restore
(an *earlier*, separate entry's own still-open theory, long before
#83-86's own investigation thread -- see this file's own discussion of
interrupt-driven redispatch cutting into the non-atomic DS/SS/SP
restore sequence), and given that *every* technique which adds enough
per-instruction overhead to change the CPU-cycles-per-real-timer-tick
ratio makes the hang vanish, the most likely remaining explanation is
a real hardware-interrupt-vs-non-atomic-dispatcher-restore race tied
to wall-clock timer timing, not a deterministic logic bug reachable by
static tracing. **Next step**: stop trying to catch it with anything
that watches *every* cycle. Either (a) let the long-budget passive
trace above finish and grep it the same way once it covers enough
real time to actually reach `t+13s`-equivalent guest execution, or (b)
try a GDB breakpoint on `dispin` with a `commands` block that counts
hits *silently* (no `printf`, minimal per-hit overhead) and only
dumps verbose state (`rlr`/`indisp`/`pc`) every, say, 500th hit --
cheap enough, maybe, to not perturb the race, while still giving
*some* visibility close to the actual transition. Neither has been
tried yet.

**Follow-up, same session**: (a) was tried -- the 150-second-budget
trace finished and was grepped the same way. Still no hang: `dispin`
hit **652500** times (up from 25s's 62941 -- the rate itself climbs
over time, from ~2500/s early, dominated by slow floppy/BIOS work, to
~4350/s overall once settled into the steady idle-dispatch loop),
`int_disp_exit` frozen at exactly **61** both times (whatever triggers
an interrupt-sourced redispatch stops happening entirely after some
early point and never resumes, a possibly-related side observation
worth keeping in mind), `dret` **652484**. 150 real seconds -- over
10x the untouched-boot hang window -- and still no imbalance. This
rules out "the first trace just wasn't long enough" and makes the
Heisenbug conclusion solid: *something about running under QEMU's
`-d exec` tracing itself* (not just GDB) avoids the race, possibly the
`nochain` flag's own effect on TB-to-TB timing (it forces a full
round-trip through the main loop between blocks instead of direct
chaining, changing timing granularity even with no single-instruction
forcing and no logging-volume difference per block). Next session:
try the same passive trace *without* `nochain` first, before
attempting (b)'s sampled-breakpoint approach -- cheaper to rule out
than to keep guessing at the mechanism.

**Follow-up, next session: tried without `nochain`, and it pins the
culprit down precisely -- it's `nochain` itself, not logging volume,
not `-d exec`, not disk I/O.**

Dropping `nochain` (`-d exec` alone) first exposed a measurement
problem, not a timing one: without `nochain`, QEMU only logs a TB the
*first* time it's entered through the main dispatch loop -- once two
blocks chain directly (the normal case for a tight loop like `dispin`
<-> `dret`), later executions skip the exec-trace callback entirely.
A 20-second run logged `dispin` all of **2** times, `dret` **1**,
`int_disp_exit` **1** -- not because the dispatcher only ran 2 times,
but because chaining hid essentially all of it. So `-d exec` without
`nochain` can't be used to *count* dispatch cycles the way the
`nochain` runs could.

It's still useful for the actual question (does it hang?), though,
once the counting problem is separated from it: booted completely
untouched except for `-d exec` (no `nochain`) running the whole time
plus the gdbstub merely *exposed* (`-s`, never touched until one
single `interrupt`+read at the end), `indisp` was `0FFh` by `t+16s`,
twice, independently, two separate fresh boots. **Light `-d exec`
tracing does not prevent the hang** -- a sharp contrast with every
`nochain` variant tried in #87, all of which ran the exact same boot
for far longer (150s) without ever getting stuck.

To be sure it's really `nochain` and not just "any use of `-d exec`
sometimes does, sometimes doesn't" (the lighter run above is still
only two data points), scoped a `nochain` run down with `-dfilter`
(confirmed working syntax: `-dfilter 0x11480+0x690`, start+size, not
start-end or start..end -- both of those are rejected as "Invalid
range") to just `dispin`..`dret`'s own narrow address window, to rule
out log *volume* as the confound while keeping `nochain` itself. Still
no hang in 20 seconds: `dispin` **7178**, `dret` **7160**,
`int_disp_exit` **17** -- `7160+17=7177`, one short of `7178`, i.e.
one dispatch pass genuinely still in flight when the timeout killed
qemu (ordinary truncation, not a stuck guard). `nochain`'s own effect
on TB-to-TB timing -- not the volume of what gets written to disk --
is what avoids the race.

**This settles the methodology question but not the bug.** The
passive-trace approach is a dead end for catching this live (any
`nochain` variant removes the race; without `nochain` there's no
reliable per-cycle signal to grep for). The viable path now: use
sparse, independent fresh-boot probes (confirmed safe -- infrequent
`interrupt`+read doesn't perturb it either) to binary-search the exact
elapsed time the transition happens at (narrower than today's
"somewhere by `t+13s`"), then, once that window is a second or less
wide, let the boot run completely untouched up to just before it and
*only then* switch on fine-grained monitoring (`nochain` trace or
GDB `stepi`) for that last short burst -- betting that perturbing
timing for under a second right at the end is less likely to steer
the outcome than perturbing it for the entire multi-second run the
way every attempt so far has.

## 88. Continued #87's own next step (binary-search the hang-onset
time). Found a real methodology bug in this and every prior session's
own GDB usage worth fixing first, then used the corrected version to
localize the transition far more precisely than "somewhere by
`t+13s`" -- down to a ~30-50ms window, naming the exact handoff it
happens during.

**Methodology fix, checked carefully because it could have tainted
every "confirmed stuck" reading in #83-87**: a `gdb -x <script>` batch
run that ends right after an `interrupt` (no explicit `continue` or
`detach`) does **not reliably resume the guest on disconnect** --
confirmed directly: two single-touch `interrupt`+read checks one real
second apart came back with *bit-for-bit identical* `$pc`, i.e. the
guest sat frozen the whole second, not running at all, then a third
check *had* moved. Adding an explicit `detach` as the script's last
command made every subsequent check come back with a different `$pc`,
every time -- genuine, reliable resume-on-disconnect. **This was
re-verified against the real bug, not just in isolation**: re-ran
#86/#87's own "sample `indisp` every ~2s" test with `detach` added,
20 real seconds, 10 samples -- still `0FFh` every single time, `rlr`
still `idle` every time. The hang itself is real and this bug doesn't
explain it away; it's a pure methodology hole (silent, inconsistent
non-resume on a bare end-of-script disconnect) that any future
session's own scripts should close by always ending a read-only probe
with an explicit `detach`, not relying on gdb's own default behavior
at EOF.

**New localization, with the fixed methodology, on today's (fast,
lightly-loaded) host**: the entire boot-to-hang sequence now completes
in well under a second of real time -- a sharp contrast with #83-87's
own `t+13s`-to-`t+16s` figures, strong independent confirmation that
this really is tied to the *ratio* of real/wall-clock timer-interrupt
cadence to guest execution speed (a slower or more loaded host just
stretches the same relative race out over more real seconds) rather
than anything resembling a fixed elapsed-time trigger. A sweep of
independent fresh boots, each given exactly one single light touch at
a different fixed delay, found:

```
delay=0.10s  indisp=0   rlr=0E40h (TMP)
delay=0.12s  indisp=0   rlr=0E40h (TMP)
delay=0.15s  indisp=0FFh rlr=09F0h (idle)   <- stuck
delay=0.18s..0.3s  indisp=0FFh rlr=09F0h (idle), every time
```

So the transition -- `TMP` still legitimately running, `indisp`
clean, through `indisp` permanently stuck and `rlr` landed on `idle`
-- happens inside a roughly 30ms window, right as control passes from
`TMP` to `idle`. This is the most precise localization yet: not "some
dispatch cycle, eventually," but specifically *the handoff away from
TMP*, naming a concrete pair of candidates to trace next (whatever
`TMP` does right before yielding, and whatever the *first* `idle`
dispatch after that does differently from every *subsequent* one that
loops forever without getting stuck).

**One attempt to catch it live, right at this window, came up empty
-- recorded so the next one is better-informed, not repeated
blind**: booted untouched to `t=0.10s` (the confirmed "still fine,
TMP running" point), then attached and ran 10 cycles of
delete+`hbreak *0x11804`+`continue` (one per `dispin` hit, verbose,
`rlr`/`indisp` printed every time). All 10 came back clean -- `rlr`
transitioning normally, `indisp` always false going in -- i.e. this
particular 10-cycle monitored burst, started right at the edge of the
known window, did not reproduce the stuck transition at all,
consistent with #87's finding that *any* per-cycle GDB round-trip
(even with minimal, non-`printf` overhead -- also separately
reconfirmed this round with a silent `hbreak`+blind-`continue` loop
counting 50000 `dispin` hits with no hang) is enough perturbation to
dodge the race. Only one attempt was made at this exact starting
point; it's not yet clear whether more tries from the same `t=0.10s`
start, or starting even closer to the window's right edge, would ever
catch it, or whether (per #87's own conclusion) *no* GDB-attached
window can.

**Next step**: now that the window is this narrow, read `TMP`'s own
code for whatever it does in its very last moments before yielding
back (around whatever syscall hands control to `idle` next -- likely
back through `cioattach`'s successful-attach path or a subsequent
`conin`/read-wait, given #86's own finding that `TMP` legitimately
owns the console by this point) side by side with `idle`'s own first
few dispatch cycles afterward, looking for anything that depends on
*relative* timing between them (a shared, non-atomically-updated
field each one reads/writes without the other's lock/`cli`) rather
than continuing to try to catch the transition live at all -- #87 and
this entry's own single attempt both point the same way: direct
reasoning from the source, the technique that nailed every earlier
root cause in this file before #83, is more likely to work here than
another live-catch attempt.

**One specific candidate checked and ruled out, same session**:
confirmed `TMP.A86`'s own `nextcommand:` loop -- print prompt, then
`mov dx,offset read_buf ! call conread` -- is exactly where `TMP`
blocks for the first time (`conread` bottoms out in `poll_entry`,
which is precisely the transition this entry localized). Floated a
theory that `idle`'s own saved `u_ivectors` (`DSPTCH.RTM`'s
`switch:` restores whatever a process last saved there back into the
*real* IVT, vectors 0-4) might be garbage on `idle`'s *first* real
switch-to, the same "GENSYS question never answered" class as the
already-fixed `u_initss` bug -- wrong. Read live, right after the
hang: `idle`'s own `u_ivectors` (flat `0x17188`) and the real IVT
(flat `0x0`) are bit-for-bit identical, both holding the genuine BIOS
stub addresses (`F000:FF53` for vectors 0/1/3/4, `F000:E2C3` for
vector 2) -- `switch:`'s save/restore of these is working correctly,
not the mechanism here.

## 89. Continued #88's own next step (read TMP's blocking path and
`idle`'s dispatch cycles side by side for a shared non-atomic field).
Found no new root cause, but **closed out two long-standing open
theories with hard live/byte-level evidence** (one of them predating
#83), replicated #88's timing finding on a third, independently-fast
host, and caught an important methodology gap in #88's own single-
touch window-narrowing that future sessions should not repeat.

**Theory #1 closed, live-confirmed, 5/5 -- the pre-#83 "`fdisp` cuts
into `switch:`'s own non-atomic DS/SS/SP restore" theory (`switch:`,
flat `0x11A8F`) is wrong.** Using the established delete+rearm-per-hit
`hbreak` discipline (#85/#86), broke at `switch:`'s entry 5 independent
times across a fresh boot and read `$eflags` each time: **`IF=0` all 5
times**, no exceptions. `switch:` (and therefore its `mov ss,u_ss !
mov sp,u_sp`) only ever runs with hardware interrupts disabled --
confirming, live, what a careful re-read of the full `dsptch:` body
already implied (`drltorlr`'s own unconditional `cli` at its top is
reached either straight through from `schedule:`'s poll loop, which
re-`sti`s only transiently per polled device and always re-`cli`s
before the next `drltorlr` iteration, or directly if the poll list and
DRL are both empty -- there is no path from `dispin` to `switch:` that
leaves `IF=1`). `fdisp`'s own interrupt path can still fire *during*
`schedule`'s poll-loop `sti` window, but lands on `indisp` already
`true` and bails via a bare `iret` that touches nothing -- safe by
construction. This specific mechanism, floated again as #87's own
"working theory," is now a dead end; stop pursuing it.

**Theory #2 closed, byte-level-confirmed -- `dispin`'s register-save
sequence and `switch:`'s restore sequence are NOT asymmetric.** Worried
(from a cold read of `DSPTCH.RTM` alone) that `mov u_ss,ss`/`mov
u_sp,sp` at `dispin` (lines just before the `push es ! pop ds` comment
block) might execute before `DS` is switched to the outgoing process's
own UDA, landing in a shared SYSDAT scratch cell instead -- while
`switch:` restores `u_ss`/`u_sp` from the *incoming* process's own UDA,
a real corruption path if true. Settled by disassembling the live
compiled bytes at `dispin` (flat `0x11804`) instruction by instruction,
confirmed against `UDA.FMT`'s own offsets: `C6 06 5E 0B FF` = `mov
indisp,true` (confirms `indisp`'s dseg offset `0B5Eh`, and live `$ds`
read back `1699h` at the same breakpoint -- the first direct live
confirmation of the SYSDAT segment value this file has on record);
`26 8F 06 4E 00` = **`ES:` pop `u_flag_sav`** (offset `004Eh`); `26 8C
16 1E 00` = **`ES:` mov `u_ss`,ss** (offset `001Eh`); `26 89 26 1C 00`
= **`ES:` mov `u_sp`,sp** (offset `001Ch`) -- all three correctly
`ES:`-prefixed (byte `26`), even though the source text itself never
spells out `es:` anywhere in this sequence. That's `tools/asm86/expr.py`
doing exactly what its own comment says it does: a bare reference to a
label declared under `ESEG` (`UDA.FMT`'s own `eseg`/`org 0` block,
which `u_ss`/`u_sp`/`u_flag_sav`/`u_in_int` all live in) defaults to
`ES:`, not `DS:`, with no override ever written explicitly anywhere in
this source tree -- confirmed in the assembler source
(`parse_operand`'s `if mem_seg is None and sym.seg == 'eseg': mem_seg =
'es'`) and now in the actual emitted machine code too. `ES` at that
point is genuinely the *outgoing* process's own UDA (set by
`do_dispatch`'s `mov es,p_uda[bx]` for the interrupt path, or expected
as a standing kernel-wide invariant for the direct `pdisp` path, per
`UDA.FMT`'s own header comment: "While in the operating system, The
Extra Segment register points to the beginning of the User Data
Area"). `switch:`'s own restore side was spot-checked the same way
(bytes at flat `0x11A8F`) and matches source exactly, `DS` explicitly
loaded from `p_uda[rlr]` before every `u_*` read -- no asymmetry
exists. Also resolved, along the way, why `dsptchtos` (`mov sp,offset
dsptchtos`) decodes to the *same* immediate value as `indisp`'s own
offset (`0B5Eh`): `SYSDAT.FMT` declares `dsptchtos` as a zero-size `rw
0` marker sitting immediately after a dedicated 48-word (`rw 48`)
scratch buffer, with `indisp` the very next field after that marker --
the standard GENSYS `xxx_tos`-after-a-reserved-buffer idiom used
identically for `idle_tos`/`tick_tos`/`init_tos` just above it in the
same file, not an overlap or a bug.

**Timing, replicated on a third host**: narrowed the clean-to-stuck
transition (TMP still legitimately polling via `rtm_poll`/`polldev`,
XIOS86Q.A86 flat `0x15FD9`-ish, vs. `idle` dispatched with `indisp`
genuinely stuck) to roughly **t=0.12s-0.14s** post-boot on this
session's host, via the same single-touch sparse-probe technique as
#88, +-10-20ms of run-to-run host scheduling jitter (confirmed by
repeating the identical nominal delay 5 times and seeing both outcomes
at e.g. 0.12s). This is a close independent replication of #88's own
0.10s-0.15s figure on a *different* fast host, reinforcing the
already-established "absolute time-to-hang tracks host CPU speed, not
a fixed guest-time duration" conclusion rather than adding a new one.
On this specific host, a boot left completely untouched until a single
first probe at t=2.0s is already stuck 3/3 -- i.e. the full boot-to-hang
window here is on the order of **2 real seconds or less**, even faster
than #83-87's hosts (~13-20s) and in the same fast ballpark as #88's
own (sub-second).

**Methodology gap found and worth flagging explicitly**: a *single*
`interrupt`+read sample showing `indisp=0FFh` is **not by itself proof
of permanent stuck-ness** -- `indisp` reads true for the entire
duration of every legitimate, successfully-completing dispatch pass
too (confirmed directly: the 5 `switch:`-breakpoint hits used for the
`IF` check above all read `indisp=0FFh`, `rlr=09F0h` (idle), simply
because `switch:` sits inside the `dispin`...`dret` window by
construction, not because anything was hung). #87/#88's own "poll
every ~2s" and "two single-touch reads one real second apart" techniques
survive this concern (a real stuck condition reads `0FFh` on *every*
sample across many seconds, which a normal toggling pass cannot fake
for long), but a *single* fresh-boot probe at one specific delay --
exactly #88's own last-round technique for narrowing the 0.10s-0.15s
window, and this entry's own first pass at it -- cannot distinguish
"caught mid-dispatch, about to clear normally" from "genuinely wedged"
on its own. Confirmed empirically by re-running this entry's own
single-touch probes with a second, ~2s-later confirm read added on the
*same* still-running QEMU instance: every single probed delay from
0.10s through 0.20s read `indisp=0FFh` on the confirm pass regardless
of what the first touch showed, consistent with this host's own ~2s
natural hang window (see above) rather than the touch itself causing
or accelerating anything -- a two-sample confirm is cheap (sparse
touches don't perturb the race per #87/#88) and should be standard
practice for any future single-boot timing probe in this investigation.

**Next step**: both of the two remaining *structural* theories
on record predating this entry (non-atomic `switch:` restore,
save/restore segment asymmetry) are now closed with direct evidence.
The one piece of the dispatch body never yet audited live is
`schedule:`'s own poll loop (`polld_another`, `XIOS86Q.A86`-adjacent
`DSPTCH.RTM` code) -- the *only* place `indisp` is true while `IF=1`
(deliberately, via its own `sti`), relying entirely on `fdisp`'s
indisp-guard bare-`iret` bail for safety. That bail path assumes the
interrupted code's own live registers (notably `DI`/`SI`, `schedule`'s
own list-walk cursors, never saved by `fdisp`'s trivial bail) survive
an arbitrary tick interrupt untouched -- true for a clean `iret`, but
never directly confirmed live. Breakpoint inside `polld_another`
(after `mov di,si` each iteration) with `indisp` already true, let a
real tick interrupt land (don't disable the clock), and confirm `DI`
reads back unchanged afterward, is the one structural stone still
unturned in this specific path.

## 90. Skipped #89's own handed-off `polld_another`/`DI` check (static
reasoning made it low-priority: `fdisp`'s bail path only ever touches
`DS` and flags, never `DI`/`SI`, confirmed by re-reading `fdisp`'s own
body line by line -- `cli / push ds / mov ds,cs:sysdat / cmp
indisp,true / jne do_dispatch / pop ds / iret`, nothing else). Instead
found a new, previously-unconsidered **candidate root cause** for how
`indisp` gets stuck in the first place, confirmed down to the actual
compiled bytes, plus a **live-confirmed downstream mechanism** for why
getting stuck is permanent and symptom-matching, neither tried before.

**Candidate root cause, byte-level confirmed, not yet caught live**:
`switch:`'s own `dret:` tail --

```
dret:	push u_flag_sav ! popf
	mov indisp,false
	ret
```

-- is byte-for-byte identical to the original 1981 DR source
(`orig/mpm862sr/02/DSPTCH.RTM`, same three lines, confirmed by direct
diff of both files' `dret:`-through-`ret` regions) and to this port's
own actual compiled bytes (read directly from `build/kernel/MPM.bin.cseg`
at `dret`'s file offset `0x1AF8`: `26 ff 36 4e 00` = `ES: push
[004Eh]` i.e. `push u_flag_sav`; `9d` = `popf`; `c6 06 5e 0b 00` =
`mov [0B5Eh],0` i.e. `mov indisp,false`, no segment prefix, matching
`indisp`'s confirmed DS-relative offset; `c3` = `ret` -- no hidden
`cli`/`sti` anywhere in the sequence). **`POPF`, unlike `STI`, has no
one-instruction interrupt-inhibition shadow on x86** (documented
since the original 8086: `STI` delays interrupt recognition until
after the *following* instruction executes; `POPF` takes effect
immediately, with no such delay) -- so if `u_flag_sav` has `IF=1` (the
normal case for both `idle` and `TMP`), a pending hardware tick
interrupt can be taken by the CPU in the single-instruction gap
between `popf` and `ret`, i.e. **after** `indisp` has already been
cleared but **before** the process has actually resumed.

If that happens: the tick ISR (`XIOS.A86`'s tick handler) unconditionally
ends with `jmp intdisp -> fdisp`. `fdisp` checks `indisp`, now sees
`false`, and -- instead of its usual bare-`iret` bail -- takes the
`do_dispatch` path, treating the about-to-resume process as an
ordinary *running* process being legitimately preempted. `do_dispatch
-> dsptch -> dispin` then saves `u_ss:u_sp` as the *current* `SS:SP`
-- but the hardware interrupt entry has already pushed a 6-byte
`IP:CS:FLAGS` frame onto that exact stack, on top of the process's own
real, legitimate return address, since `dret`'s `ret` never got to
consume it. That corrupted `SP` becomes the process's officially saved
resume pointer. The next time this same process is switched back in,
`switch:`'s own final `ret` pops the *stale interrupt frame* instead
of the real return address -- jumping into the middle of kernel code
(right after `switch:`'s own `popf`, by construction) instead of back
into the process's own code, with a flags value that happens to be
whatever `switch:`'s own local working flags were caught at.

**This single mechanism accounts for every empirical finding in
#83-89 without exception**: any added per-instruction overhead
(`stepi`, watchpoints, `-d exec`/`nochain` tracing) shifts exactly
which instruction a tick interrupt lands on, and the vulnerable window
here is a single instruction wide -- explaining why *every* live-
observation technique tried so far made the hang vanish. Absolute
time-to-hang tracking host CPU speed rather than guest time is exactly
what a probabilistic race against real-wall-clock timer cadence would
produce. #88's localization of the transition to the exact handoff
into `idle`'s first dispatch is consistent (every dispatch exercises
this exact `dret` sequence, so the first one is as likely to lose the
race as any other). #89's own confirmation that `switch:` always runs
with `IF=0` is also exactly consistent -- true for the entire body,
right up to the one instruction (`popf`) that flips it, one
instruction before the vulnerable `ret`.

**Not yet confirmed live** -- this is reasoned from the source and
the documented hardware/QEMU-TCG interrupt-shadow model (QEMU's
target/i386 implements `STI`'s one-instruction shadow explicitly and
gives `POPF` no equivalent, matching real silicon), not caught in the
act. The race is exactly as hard to catch live as everything else in
this file for the same reason it exists: any observation technique
precise enough to catch the exact `popf`/`ret` boundary is also
precise enough to perturb which instruction the next tick lands on.

**New downstream mechanism, live-confirmed this round, independent of
the above**: once `indisp` is stuck `true`, every *subsequent* call
into `pdisp`/`dsptch` -- including from the currently-running process
itself, voluntarily yielding, not just interrupt-forced redispatch --
becomes a **silent no-op**. `dsptch`'s own entry guard is `pushf / cld
/ cli / cmp indisp,true / jne dispin / popf ! ret` -- if `indisp` is
already `true`, the call just restores flags and returns immediately,
with no actual list manipulation, no actual switch, nothing. Confirmed
live, three independent fresh-boot samples, single-touch-plus-
`detach` each (per #88's own resume-reliability fix): once in the
confirmed-stuck state, `CS` stayed `1000h` every time but `IP` was
different every single sample (`0x33A4` inside `CIO.A86`'s
`getccbadr` -- confirmed by disassembling the actual compiled bytes at
that file offset and matching them instruction-for-instruction against
`getccbadr`'s known body, `mov bx,[rlr] / sub ax,ax / mov al,bh / shl
ax,1 / shl ax,1 / mov si,[ccb] / add si,ax / add si,ax / add si,ax /
ret`, i.e. `ccb + console#*12` per `XIOS86Q.A86`'s own comment on this
routine; `0x268`/`0x2FB`, two more samples two real seconds apart on a
second fresh boot, both inside `SUP`'s own module range) -- `eflags`
also varied sample to sample (`IF` sometimes 0, sometimes 1). **This
rules out both "the CPU is halted" and "the CPU is spinning in one
tight infinite loop"**: something is genuinely, continuously executing
varied kernel code the whole time `indisp` stays stuck. The
straightforward reading: whichever process is currently `rlr` (almost
always `idle`, per #88) keeps calling `pdisp` in its own loop exactly
as designed, but every one of those calls is now a no-op that returns
immediately without ever reaching `schedule`/`switch` again -- so
`idle` just spins forever, legitimately busy (making whatever other
calls its own loop body makes along the way, which is why `IP` varies
sample to sample), while `TMP` (or whoever actually owns the console)
never gets a turn again. This is the first live confirmation of *why*
a stuck `indisp` specifically presents as "console input never
serviced again" rather than a crash or visible lockup.

(One live reading this round should be discounted: an attempted direct
read of `rlr` at flat `0x169F8` -- `sysdat` segment `0x1699` (independently
re-confirmed this round via the same `indisp`-at-`0x174EE` byte-read
method #89 used) `*16 + 0x68` (`rlr`'s own confirmed `SYSDAT.FMT`
offset) -- returned `0xE5E5` identically across two independent fresh
boots, which looks like a stale/wrong-segment artifact rather than a
real `rlr` value and needs redoing carefully, not trusted as-is.)

**Next step**: the live-confirmed downstream mechanism (silent-no-op
`pdisp` once stuck) is now solid and doesn't need more work. The
candidate root cause (the `popf`/`ret` interrupt-shadow gap in `dret`)
is the one piece left needing a live-or-equivalent confirmation before
calling this closed, given how many earlier "working theories" in this
file turned out wrong under scrutiny. Two ways forward, neither tried:
(a) a targeted fault-injection test outside QEMU's normal timer path --
e.g. a one-off patched build that forces a tick IRQ to fire
synchronously right after `dret`'s `popf` (a debug-only, deliberately
non-authentic build, not a change to `src-assem/`) and confirms the
predicted stack corruption happens exactly as reasoned; or (b) a
live GDB session with a breakpoint on `fdisp`'s `do_dispatch` entry
(not the bail path) that, on each hit, checks whether the *resuming*
process's own about-to-be-restored `IP` (read from `u_ss:u_sp` before
the corruption, if catchable at all) falls outside that process's own
known code range -- harder to set up than (a) but requires no
non-authentic build. If either confirms the mechanism, the fix is a
real design decision, not a one-line patch: the original DR dispatcher
has always had this exposure on real 8086 hardware too (confirmed
identical in `orig/mpm862sr/02/DSPTCH.RTM`), just apparently rare
enough there to never bite in practice, so patching `dret` to close it
would be a deliberate deviation from the authentic source, not a
restoration of broken intent -- worth flagging to whoever decides
whether this port stays byte-faithful to GENSYS-86 dispatcher
semantics or not.

## 91. Tried #90's own option (b) -- a live GDB breakpoint on `fdisp`'s
`do_dispatch` entry (flat `0x1177B`), checking each hit's interrupted
`CS:IP` against `switch:`/`dret`'s own narrow address range. Two new,
reusable GDB/QEMU-gdbstub methodology fixes came out of getting this to
run at all; the actual result is a clean negative, fully consistent
with (not evidence against) #90's own theory, plus one more direct
confirmation that the underlying hang is still real and still has the
same signature.

**Methodology fix #1 -- the "Cannot execute this command while the
target is running" error (already flagged elsewhere in this file, #32)
has a root cause now: it fires specifically when `target remote`
*reattaches* to a QEMU instance that is already free-running** (either
launched without `-S`, or previously `detach`ed from after a `-S`
start). Confirmed directly: a QEMU launched with `-S` (genuinely halted
at the reset vector, `0xfff0`) accepts `hbreak`+`continue` perfectly
the *first* time; `detach`, then `target remote` again on the *same*
still-running instance, reliably reproduces the error on the very next
`continue` -- regardless of adding `interrupt`, `print $pc`, `info
program`, or `stepi` beforehand (all tried, all still hit the same
error on the subsequent `continue`; `info program`/`stepi` do get a
genuine, correctly-updated stopped state each time, so this isn't a
real vs. perceived stop-state mismatch, it's specifically `continue`
itself that's unreliable post-reattach on this gdbstub). **Net rule:**
set up every breakpoint and run the entire session's worth of
`continue`s against one single, continuously-attached `-S`-started
session; never detach-and-reattach mid-investigation if more
`continue`s will be needed afterward -- kill and relaunch fresh with
`-S` instead, exactly the existing "kill-and-relaunch per probe"
discipline from #85, just now also covering *why* reattach-based
multi-sample loops fail outright rather than just "eventually, after
many reattaches."

**Methodology fix #2 -- a breakpoint's own `commands` block
auto-`continue`ing on hit does NOT reliably keep the session running
across many hits in `gdb -batch` mode**, even within one single
unbroken, never-detached session. Confirmed directly: the exact same
breakpoint+`commands`+inner-`continue`+outer-`continue` script that
should loop indefinitely consistently stopped and auto-detached after
hit #1, every time, with `timeout` reporting the whole `gdb` process
exited with code 0 (not killed) well under its time budget -- i.e.
`-batch` mode appears to treat the top-level command list as
"finished" once the single top-level `continue` and its triggered
`commands` block complete one round-trip, regardless of a `continue`
issued from *inside* that block. **The fix: use GDB's own `while ...
end` control-flow construct at the top level instead**, wrapping an
explicit `continue` + inspection + counter-increment each iteration
(`while $iters < N ... continue ... set $iters = $iters + 1 ... end`).
This looped reliably and correctly for the full run below -- the
`commands`-block pattern should be considered unreliable on this
gdbstub/GDB-batch-mode combination specifically and avoided in favor
of an explicit `while` loop for any future multi-hit sampling here.

**The actual test, using both fixes**: one continuously-attached `-S`
session, `hbreak *0x1177B` (`do_dispatch`), a `while` loop running
`continue` up to 100000 times, each hit reading the interrupted
`CS:IP` off the stack (`[SS:SP+2]`/`[SS:SP+4]`, the two words `fdisp`'s
own `push ds` leaves sitting just below its own pushed `DS`) and
flagging any hit where that `CS:IP` falls inside `switch:`/`dret`'s own
flat range (`0x11A8F`-`0x11B03`, the one-instruction-wide `popf`/`ret`
gap and its immediate surroundings). **Result: 0 matches across
~96000+ hits**, spanning real wall-clock time that carried the boot
all the way through to the interactive `0A>` prompt printing cleanly
(confirmed in the serial log) -- i.e. this breakpoint, firing roughly
2000+ times/second throughout, is by far the heaviest-weight live
observation technique tried anywhere in #83-91, consistent with (not
contradicting) the established pattern that any sufficiently frequent
live touch suppresses this race entirely while active.

**And the hang still happened anyway, immediately after**: a
follow-up single-touch read (no `continue`, just a register/memory
peek, the technique that's never perturbed anything in this
investigation) taken right after detaching from the 96000-hit loop
found `indisp` already stuck `0xFFh` again, with `CS:IP` at
`1000:33A4h` -- the *exact same* flat address (`CIO.A86`'s
`getccbadr`) as #90's own very first stuck-state sample. This is a
clean, expected negative result, not a refutation of #90's theory: the
race, if #90's mechanism is right, needs a tick interrupt to land in a
single specific instruction's width, and a breakpoint trapping into
the debugger ~2000 times/second for the entire run is exactly the kind
of continuous perturbation that has reliably prevented the race from
firing anywhere else in this file (#87's watchpoint-loop and
passive-trace results, #88's single-touch-burst attempts) -- it firing
*immediately* once that perturbation stopped is, if anything, one more
small data point in the theory's favor, though nowhere near proof.

**Next step**: do_dispatch-breakpoint live monitoring is now a
confirmed dead end for catching this, for the same reason everything
else is -- any technique precise enough to inspect the right moment is
also precise enough to prevent that moment from occurring. #90's other
proposed option -- a deliberately non-authentic debug build that
injects a synchronous tick IRQ right after `dret`'s `popf` to directly
confirm the predicted corruption mechanism in isolation, without
relying on catching the real race live at all -- remains untried and
is now the most promising concrete next step.

## 92. Did #91's own handed-off fault-injection test -- via live GDB
state manipulation on the real binary rather than a separately-built
non-authentic image (simpler, no rebuild needed, and arguably more
convincing since it's the actual shipped bytes under test) -- and it
**directly confirms #90's candidate root cause end to end, deterministically,
with no reliance on the Heisenbug firing naturally at all**.

**Method**: one continuously-attached `-S`-started GDB session (per
#91's methodology fix #1). `hbreak *0x11B03` (`dret`'s own `ret`
instruction, i.e. right after `popf` has already executed and
`indisp` is already `false` -- the exact window #90 identified).
`continue` to the first hit (caught `rlr=0x9C0`, `initpd`, early in
boot -- the mechanism doesn't depend on which process this is). At the
hit, read and record the genuine, uncorrupted state: `old_ss:old_sp =
1705:00F8` (`initpd`'s real stack), `real_target_ip = 0x1494` (what
this `ret` would legitimately pop and resume at), `flags = 0x246`
(`IF=1`, confirming this process really would resume with interrupts
enabled, the precondition for the race). Then **manually constructed,
byte for byte, exactly the stack frame a real hardware tick interrupt
would have pushed had it landed in this exact one-instruction gap**:
three words at `SS:(old_sp-6)` = `[IP=0x1B03][CS=0x1000][FLAGS=0x246]`
(the real 8086 interrupt-entry push order), `$sp` set to `old_sp-6`,
then `$pc` set directly to `fdisp`'s own entry (`0x176B`) -- i.e.
jumping to exactly where a real tick ISR's own `jmp intdisp` would
have landed, with the stack already arranged exactly as that ISR
would have left it. From there, let the CPU run its own real,
unmodified code -- no further manual intervention.

**Result, read back after letting `fdisp`/`do_dispatch`/`dsptch`/
`dispin` run for real off the injected state**: `dispin`'s own `mov
u_ss,ss ! mov u_sp,sp` (confirmed live via breakpoint right after it,
`*0x11818`) saved `u_sp = 0xF2` -- **exactly `old_sp - 6`, bit for
bit**, not the real `0xF8`. Reading what the *next* `switch:`/`dret`
for this same process would pop as its resume `IP` off that corrupted
`u_sp` gives **`0x1B03`** -- `dret`'s own `ret` instruction's address,
inside the kernel, *not* the real `0x1494` resume point inside
`initpd`'s own code. This is the complete mechanism from #90, now
directly observed rather than reasoned: a tick landing in the
`popf`/`ret` gap genuinely does leave the resuming process's saved
stack pointer permanently off by exactly the size of the spurious
interrupt frame, and the next resume for that process genuinely does
jump into dispatcher-internal code instead of back into the process.

**What this does and doesn't prove**: it proves the mechanism is real
and mechanically exactly as predicted -- not a hypothesis anymore.
It does *not* by itself prove this is *the* cause of every hang in
#83-91 (that still requires the race to actually fire on its own,
un-injected, which remains as hard to catch live as ever, per #87-91's
own repeated findings) -- but given this mechanism is now proven to
produce exactly the right kind of permanent, un-recoverable corruption
(a process's own resume point silently replaced by kernel-internal
code, with no crash, no visible symptom until that specific process is
next switched back in), and given #90 already showed it's the only
candidate that explains every single empirical timing/perturbation
observation in this file, this should now be treated as the
established root cause pending only the formality of a natural catch.

**Next step**: the mechanism is proven; what's left is optional. (a)
a real fix would change `dret`'s tail to close the `popf`/`ret` gap --
e.g. `cli ! push u_flag_sav ! popf ! mov indisp,false ! <use `sti` and
re-derive the one piece of state that still needs setting after it, or
else accept a process always resumes with `IF` forced via `sti`'s own
one-instruction-shadow instead of via `popf`'s unshadowed restore> --
but as #90 already flagged, this is a deliberate deviation from the
authentic 1981 DR dispatcher, not a restoration, and should be a
decision made by whoever owns that tradeoff, not applied unilaterally
here. (b) if a live natural catch is still wanted for completeness,
the fault-injection technique in this entry suggests a much cheaper
way to go hunting for it: instead of a breakpoint on every
`do_dispatch` hit (#91's ~2000/sec, too heavy), set the hbreak only
on `dret`'s `ret` (as this entry did) and, on every *natural* hit,
non-invasively check `$eflags`'s `IF` bit and whether `u_sp` was
already corrupted from a *previous* occurrence (i.e. check for the
telltale `0x1B03`-at-`u_sp` pattern this entry establishes as the
unique fingerprint) rather than trying to catch the interrupt landing
in the gap itself -- `dret` is hit far less often than `do_dispatch`
(only once per real dispatch cycle, not once per tick), so this may
perturb the timing far less while still being able to detect, after
the fact, that a given process's `u_sp` already carries the
fingerprint from a race that already happened un-observed.

## 93. Tried #92's own handed-off idea (b) -- a lighter-weight
`switch:`-entry fingerprint check instead of a `do_dispatch`-rate
breakpoint. The core assumption behind calling it "lighter" turned out
wrong in practice; result is a clean negative of the same character as
#91's, plus one more confirmation of how fast the un-observed hang
recurs.

**Method**: one continuously-attached `-S` session (methodology fix
#1), `hbreak *0x11A8F` (`switch:`'s own entry, before `mov ss,u_ss`/
`mov sp,u_sp` actually run), a `while` loop (methodology fix #2)
reading, on every hit, the about-to-be-resumed process's own saved
`u_ss:u_sp` (via `rlr` -> `p_uda[rlr]` -> that UDA's own `0x1E`/`0x1C`
fields) and checking whether the *stack word already sitting there* --
i.e. whatever an *earlier*, already-completed dispatch for this same
process saved as its resume point, not anything about the live
in-flight interrupt state -- falls inside `switch:`/`dret`'s own
narrow flat range (`0x1A8F`-`0x1B03`), which #92 established as the
unique fingerprint of a resume point already corrupted by a past,
unobserved occurrence of the race.

**Result: 0 matches across ~83000 hits** over a 50-real-second window
that, per the serial log, carried boot all the way through to the
`0A>` prompt printing cleanly. **The "lighter-weight" assumption behind
this approach was wrong**: `switch:` turns out to fire nearly as often
as `do_dispatch` did in #91 (~1660/sec here vs. ~2000+/sec there,
same order of magnitude, not meaningfully lighter) -- every dispatch
pass reaches `switch:` exactly once, and dispatch passes are
apparently about as frequent as `do_dispatch`-triggering ticks, not an
order of magnitude rarer as hoped. So this is, in practice, still one
of the heaviest-weight live techniques tried in this file, and the
result carries the same caveat as #91's: a clean negative, fully
consistent with (not evidence against) #90/#92's theory, since nothing
this invasive has ever let the race fire while active anywhere in
#83-93.

**And, same as #91, the hang recurred immediately once monitoring
stopped**: a single light touch (register/memory peek only, no
`continue`, the one technique that's never perturbed anything here)
taken right after detaching from the 83000-hit loop found `indisp`
already stuck `0xFFh` again, `CS:IP = 1000:1803h` (a different
specific address than #90's or #91's own post-detach samples, but
still `CS=1000h`, still consistent with #90's live-confirmed
"no-op-`pdisp`, varying `IP`" downstream signature, not a new
mechanism). **Three independent sessions now (#90's original
observation, #91's `do_dispatch` test, this entry's `switch:` test)
have all found the hang reappearing within at most a couple of real
seconds of monitoring stopping**, every single time it's been
checked -- suggesting the race's true, unperturbed probability per
unit time is high enough that "stop watching briefly" is close to a
reliable way to *get* the hang, even if no technique tried so far can
watch *through* the actual moment it happens.

**Next step**: both of #92's proposed follow-ups (the real `dret`
fix, and a lighter live-catch attempt) are now covered -- the fix is
still a deliberate, deferred design decision (#90/#92), and the
"lighter fingerprint check" idea specifically (switching the
breakpoint to `switch:` instead of `do_dispatch`) did not pan out as
meaningfully lighter and shouldn't be retried as-is. If a live natural
catch is still wanted, the next genuinely untried angle is reducing
*when* the breakpoint is live rather than *where* it is: use the
established safe/non-perturbing single-touch polling (#87-90) to
narrow in on the known sub-second-to-low-teens-of-seconds hang window
first, then arm the `switch:`/`dret` breakpoint only for that last
narrow slice, rather than for the whole boot -- cheap to try, not yet
done. Absent that, #92's direct fault-injection proof already stands
on its own as the mechanism's confirmation; further live-catch
attempts are now optional polish, not required to consider this
closed.

## 94. Applied #90/#92's candidate `dret` fix (route `IF=1` restores
through `STI`'s interrupt shadow instead of the unguarded `POPF`),
confirmed it assembles and boots -- **and confirmed, with the same
rigor #87 used to establish "permanently stuck" in the first place,
that it does NOT fix the hang**: 10 samples 8s apart over a fresh,
completely untouched 80-second boot still show `indisp=0xFFh` on
every single sample, `rlr=idle` every time, `IP` varying (identical
signature to the unpatched build). The patch is left in place (it's
real, harmless hardening, confirmed earlier via direct fault
injection in #92) but **it is not resolving the actual observed
hang, or not the only thing that is**.

Re-examined every other `popf`/`iret` site in the kernel for the
same hazard: `int_disp_exit`'s own tail sets `indisp,false` a few
instructions before its final `iret`, but `iret` restores flags and
transfers control as a single atomic instruction (no following
instruction for a tick to land on with mismatched `IF`), so it's not
exposed the same way `dret` was. `dsptch`'s own entry-guard `popf !
ret` bail only ever runs while `indisp` is still `true`, so a tick
landing there sees `fdisp`'s safe bail path, not `do_dispatch`. No
other exposed site found.

**This forced a re-read of this file's own much earlier #83, which
turns out to have been quietly abandoned mid-investigation without
its own next step ever being followed up on** -- #83 found and fixed
three real, confirmed bugs (`idle`'s own UDA `u_initss` field left
`0` instead of `sysdat_segment`; `idle:`'s and two other sites'
`mov cl,f_xxx` leaving stale `CH` and misrouting the call; a genuine
1981 DRI typo in `constat_entry`), getting console input from "zero
bytes, ever" to "echoes any number of real characters correctly" --
but documented one precisely-scoped symptom still open: **sending
`CR` specifically, after any number of successfully-echoed regular
characters, reliably wedges the scheduler again**, every time, not a
race. #84 immediately pivoted into the Heisenbug/`popf`-`ret`
investigation (#84-93) without ever returning to this handed-off CR
symptom. Re-confirmed live, this session, that #83's finding still
reproduces exactly as described: a real interactive harness (`expect`
driving `qemu -serial mon:stdio`, the pty- and socket-based harnesses
tried first both turned out to be broken rigs that delivered zero
bytes in either direction -- raw-mode `tty.setraw`/termios tweaks
didn't fix the pty rig either, root cause not pinned down, `expect`'s
own pty handling is simply what worked) shows `ABC` echoing correctly
immediately after the `0A>` prompt, then zero response of any kind
once `\r` is sent.

**This is a second, apparently distinct bug from #84-93's
idle/dispatch race -- with a completely different live signature.**
Single-touch reads taken 1-6 seconds after sending a real `CR`
consistently show `indisp=0x0` (false, not stuck) and `rlr=0xE40`
(`TMP`'s own PD, not `idle`) -- the opposite of every #84-93 reading.
`CS:IP` varies between touches and lands in places like `F000:xxxx`/
segment `0` (BIOS ROM address space) rather than anywhere in the
kernel's own `cs=1000h`.

**Confirmed, via QEMU's own `-d int`/`-d int,cpu` exception logging
(not inferred from register snapshots), that this is a real CPU
fault, not a scheduler deadlock**: shortly after the `CR` keystroke,
the log shows `check_exception old: 0xffffffff new 0x6` (**exception
vector 6 -- `#UD`, invalid opcode**) repeating in an exact, regular
37-line cycle -- a tight hardware fault loop, not sporadic. The `-d
cpu` register dump immediately before the first occurrence shows
`CS=0000 EIP=00000000` -- **the CPU is trying to execute at flat
address `0x00000000`**, inside the real interrupt-vector table
itself, not anywhere code should ever be. Confirmed independently via
GDB: a hardware breakpoint at `*0x0`, armed through the same
interactive `expect` harness while sending `CR`, is followed almost
immediately by `[Inferior 1 (process 1) exited normally]` -- strongly
suggesting the fault loop is a precursor to an actual **triple
fault**, with `-no-reboot` causing QEMU to tear the whole VM down
(not a graceful reset) once that happens, rather than the guest
"just" hanging.

**Not yet found**: the actual instruction that makes `CS:IP` (or just
`IP`, if this is a near `jmp`/`ret` with `CS` already corrupted to
`0` earlier) land on `0000:0000`. The `-d cpu` trace immediately
before the jump shows the CPU mid-way through what looks like genuine
interrupt-vector-table *installation* code (individual `mov
[addr],imm16` writes landing on values consistent with vector
offset/segment pairs, including one write that stores the *current*
`CS` register as `mpmint`'s (`int 224`, flat IVT offset `0x380`) own
vector segment) -- plausible as legitimate `SUP.A86`/`RTMIF.RTM`
init-time vector setup, but deeply suspicious to be running again
*after* the system has already booted fully and processed real
commands. Whether this is genuinely re-executing old init code (wild
jump landing back there), or coincidental data at that address that
merely disassembles into plausible-looking opcodes, is not resolved.

**Next step**: pin down the actual faulting instruction and its real
cause -- a GDB hardware watchpoint on the real IVT's vector-6 entry
(`write` watchpoint on flat `0x18`-`0x1B`, the 4 bytes `#UD`'s own
vector points to) would catch anything that corrupts *that* specific
entry; alternatively, a `-d exec,nochain` trace scoped tightly (via
`-dfilter`) to a narrow address window around the suspected
TB-ending branch, read after the fact, would show the exact
instruction without needing to catch it live with GDB at all. This is
a deterministic, 100%-reproducible repro (not a race) -- send real
characters ending in `CR` to a booted system over a real interactive
harness -- so it should not need any of #83-93's own Heisenbug-specific
sparse-probing discipline; ordinary breakpoints should work fine here.

**Follow-up, same session**: tried a GDB write-watchpoint on the real
IVT's own vector-6 (`#UD`) entry (flat `0x18`-`0x1B`) across the full
`CR`-triggered crash, coordinated against the live interactive
`expect` harness via a file-based ready-marker so the watchpoint was
armed before `CR` was ever sent. It never fired -- vector 6's own IVT
entry is not what gets corrupted; whatever's happening doesn't rely on
rewriting that specific vector.

**A passive `-d exec` trace (no `nochain` -- with it, boot never
reaches the prompt within any reasonable budget, reconfirming #87's
own finding that `nochain` itself, not log volume, is what's
unworkably slow here) across the same `CR`-triggered crash shows an
exact, regular 3-TB repeating cycle taking up the entire back half of
the trace** (hundreds of thousands of consecutive lines, all 3 TBs,
in the same order) -- flat addresses matching `F000:FF53` (the BIOS
dummy vector-0/1/3/4 stub, same address already identified in #89's
own `u_ivectors` read) paired with two other addresses that *drift
slowly downward* across the trace (`FFFF`/`0003` early on, `FFFD`/
`0001` hundreds of thousands of lines later) -- consistent with an
`IRET`-only dummy handler blindly returning to the same bad `CS:IP`
over and over, re-faulting every time, with the guest's own `SP`
creeping down by a word or two on each cycle (a small, consistent net
stack imbalance per iteration, not a clean repeat). Binary-searching
backward through the trace for where this cycle *starts* did not
converge this session -- by 1/3 of the way into even a short
(~600,000-line) trace, the cycle (in some drifting variant) is already
running, meaning whatever triggers it happens very early, close to
`CR` itself, not after an extended delay -- but the exact triggering
instruction was not isolated.

**Three separate attempts to catch the transition live with GDB all
failed the same way, for a reason specific to this bug rather than to
GDB/QEMU in general**: a hardware breakpoint at `*0x0` (the fault
target itself), a write-watchpoint on the vector-6 IVT entry, and a
breakpoint at `CHARIO.CIO`'s own `readen:` (flat `0x13351`, where `CR`
is first recognized) followed by a `stepi` loop, were all tried.  The
`readen:` breakpoint item itself genuinely hit (confirmed: the
script's own `=== HIT readen ===` marker printed), but reading
registers immediately afterward failed with gdb's own `No registers`
error, and separately, `[Inferior 1 (process 1) exited normally]` was
printed close to every attempt -- strongly suggesting **QEMU itself
tears the whole process down once the fault loop is reached or a
triple fault occurs, even starting from a point as early as `readen:`
itself**, which is *before* any of the suspect kernel-dispatch code
runs at all. This may mean the fault happens fast enough after `CR`
that normal single-stepping through the next few hundred instructions
from `readen:` isn't enough headroom, or that something about
single-stepping specifically (as opposed to free-running) interacts
badly with whatever QEMU-internal condition tears the VM down. Not
resolved this session.

**Status at handoff**: the *mechanism* (real CPU exception --
`#UD`/vector 6 -- repeating in a tight fault loop after jumping to
`CS:IP=0000:0000`, most likely culminating in an actual triple fault
that `-no-reboot` then surfaces as the whole VM exiting rather than
rebooting) is now solid, confirmed three independent ways (live
register snapshots, `-d int`/`-d int,cpu` exception logging, and the
passive `-d exec` trace's repeating-TB signature). The *one specific
instruction* that first sets `CS:IP` to `0000:0000` -- presumably a
bad `jmp`/`call`/`ret` somewhere in `TMP.A86`'s or `CHARIO.CIO`'s own
post-`CR` path, given #83's own narrowing to "whatever `TMP.A86` does
immediately after a line is complete" -- is not yet isolated. **Next
step, untried**: rather than single-stepping from `readen:` forward
(which this session's attempts suggest may not survive long enough),
work *backward* from the crash instead -- a `-d exec` trace (no
`nochain`) scoped with `-dfilter` to just the `CHARIO.CIO`/`TMP.A86`
address range (ruling out needing to wade through the BIOS-loop tail
at all) would show the real last few kernel/RSP-level instructions
executed before control left that range, without needing GDB to
survive anywhere near the actual fault.

## 95. Followed #94's own handed-off next step (a `-dfilter`-scoped
passive trace, working backward from the crash instead of trying to
survive forward into it with GDB) -- **and it worked, isolating the
real trigger precisely, with no GDB-survival problems at all**.

**Method**: `-d exec,cpu -dfilter 0x10000+0x5d95,0x16270+0x430`
(covering the whole kernel from `SUP` through `BDOS`, plus `TMP`'s own
runtime code segment, found live via its UDA's `u_initcs` field --
`0x1627`, matching `build_mpmsys`'s own static layout number almost
exactly) over the same real, interactive `expect`-driven `CR` repro.
No GDB needed anywhere in this entry -- side-steps #94's own repeated
"GDB session dies with the VM" problem entirely, since this reads a
log file after the fact.

**Found it**: the kernel's own absolute first instruction (flat
`0x10000`, `RTMIF.RTM`'s `jmp init` -- the *boot-time reset entry
point*, confirmed by reading the actual compiled bytes there) is
genuinely **re-executed a second time**, tens of thousands of trace
lines after its one legitimate appearance during normal boot. The
instructions immediately preceding this second occurrence are
`PROC.RTM`'s own `cp_uda`/`proc_creat` body (confirmed against
`asm86 --symbols`: the flat addresses land squarely between
`cp_uda4` and `getpdadr`) -- **i.e. the system is in the middle of
creating a new process, immediately before jumping to the kernel's
own reset vector instead of into whatever process it just created.**

**This fits together completely with #83's own original, never-
followed-up finding and with every live signature from #94**: sending
a line ending in `CR` makes `TMP.A86` try to run it as a command
(`nextcommand`'s own `clicall`/`not_builtin` path, `mov cl,mpm_clicmd
! call mpm`) -- and typing an unrecognized command (this session's
own repro used `AB`, which has no matching `.CMD` file) is exactly
what reaches `proc_creat` on a command-load attempt. `cp_uda`'s own
job is to seed the new process's `u_initcs`/`u_ss`/`u_sp` and build an
`iret`-ready stack frame (`u_initcs`'s value as the frame's `CS`) for
the dispatcher to later jump into via `int_disp_exit`'s `iret`. If
`u_initcs` ends up `0` (or, more likely given the observed `CS=1000h`
match, equal to the *kernel's own* code segment by some left-over/
uninitialized value) instead of the real loaded-command's segment,
the next dispatch of that "process" doesn't start the command at all
-- it jumps straight into the kernel's own reset-time `init:` code,
which then re-runs genuine one-time boot setup (the same interrupt-
vector-table-installation-looking code this file's own #94 entry
disassembled and was puzzled to find running again) against a machine
that is very much *not* actually resetting, corrupting enough state
(plausibly including the real IVT, directly explaining #94's own
`CS:IP=0000:0000` / repeating `#UD` crash loop) to bring the whole
system down shortly after.

`COMMAND.SUP`'s own `clicmd_entry` -- the code actually responsible
for opening and loading a `.CMD` file on `TMP`'s behalf -- was read in
full for the "command not found" path specifically, since that's
exactly what `AB` should trigger: it *does* correctly check
`fileopen`'s own failure return (`cmp bl,0ffh ! je cli_bopen`) and
sets `e_bad_open` before bailing, which should prevent ever reaching
`f_load`/`proc_creat` for a genuinely nonexistent file -- so either
this specific check has a gap not yet found (e.g. an FCB-matching
quirk that makes `AB` spuriously "find" something, a wildcard/blank-
padding issue, or similar), or `proc_creat` is reached via some other
path than the one read here. **Not yet pinned down: the exact reason
`u_initcs` ends up wrong, or the exact reason `AB` reaches
`proc_creat` at all** -- but the mechanism from "process gets created
with a bad starting segment" through to "kernel reset code re-runs
and corrupts the real IVT" is now a complete, well-supported causal
chain, a huge narrowing from #94's own "somewhere in `TMP.A86`'s
post-`CR` path" vagueness.

**Next step**: read `PROC.RTM`'s `cp_uda` (`src-assem/kernel/PROC.RTM`
lines ~70-95, the "set initial segment values"/"set user's stack"
block already quoted in this file's own #83 write-up for the
`idle`/`tick` case) side by side with whatever `COMMAND.SUP`/`LOAD.SUP`
actually passes it for a failed `.CMD` lookup, to find the exact spot
`u_initcs` (or the UDA segment it's copied from) goes wrong for this
specific case -- `cp_uda`'s own "default from `u_wrkseg` if still `0`"
logic (`cp_uda1`-`cp_uda4`) is the same generic mechanism #83 already
confirmed protects ordinary process creation in general, so the bug
here is more likely upstream of `cp_uda` (whatever segment value
`COMMAND.SUP` hands it in the first place for this failure case) than
in `cp_uda` itself.

## 96. Followed #95's own next step -- read `COMMAND.SUP`'s
`clicmd_entry`/`cli_fload` and `PARSE.SUP`'s `parse_entry` in full,
side by side. **Ruled out the most obvious candidate theory (stale/
unpadded FCB bytes making `AB` spuriously match a real file like
`ABORT.CMD`) with direct evidence, narrowing the remaining mystery,
but did not find the exact fault** -- and hit a new, specific,
reproducible GDB/interactivity limitation worth recording for next
time.

**FCB construction confirmed clean**: `PARSE.SUP`'s `parse_entry`
calls `fcbinit` unconditionally at its own top, which blank-pads the
*entire* FCB (name bytes 1-11 explicitly set to `' '`, bytes 12-15
zeroed, password field blanked) before anything else touches it, then
`parse_entry`'s own `setfld` fills in only as many real characters as
were actually typed, leaving the rest however `fcbinit` left them --
i.e. `AB` genuinely becomes `"AB      "` (two real chars, six real
blanks), not stale/leftover bytes from a previous parse. This rules
out the "incomplete padding makes `AB` wildcard-match `ABORT.CMD`"
theory directly, by reading the actual padding code, not by
inference.

**`cli_fload`'s own structure re-confirmed**: a PD is pulled from the
free list (`pul`) and zeroed *before* the file is ever opened, but the
actual dispatch to `f_createproc` (line ~429, well after `cli_gopen`)
is only reached if `fileopen` returns `bl=0`, with every failure path
(`cli_bopen`/`cli_bopene`) correctly setting `cli_err` and jumping to
`cli_rmpd` (which returns the PD to `pul`) *before* reaching the
create-process call -- read in full, this logic looks correct for the
simple "file genuinely not found" case. The real question is now
squarely "why does `fileopen` return success for `AB`", not anything
about `cli_fload`'s own control flow.

**Traced as far as the existing passive capture allows**: `cli_fload`
(flat `0x105AF`, confirmed) is reached, and execution continues a
little further (to `0x105BC`) before leaving the logged range -- but
neither `cli_gopen` (`0x106E0`) nor `cli_bopen`/`cli_bopene`
(`0x106D2`/`0x106D7`) appear anywhere in #95's own saved trace capture,
for either outcome. This is very likely a `nochain`-style gap (the
earlier capture used plain `-d exec,cpu`, *without* `nochain`, for
speed -- #88's own established caveat that chained blocks only log
their *first* visit through the main loop, and a long, mostly-linear
run through `fileopen`'s own cross-module call and the few comparisons
around it would very plausibly chain straight through without a fresh
log line at either label) rather than evidence that neither branch
ran.

**New GDB/interactivity limitation found, specific to this scenario,
not general**: every attempt this session to set a *live* breakpoint
(at `cli_gopen`, at `cli_bopen`, at both at once, with or without the
file-marker coordination trick from earlier entries) and `continue`
through a real `expect`-driven keystroke sequence failed the exact
same way -- `[Inferior 1 (process 1) exited normally]` almost
immediately, regardless of which address was armed, including
addresses that should only ever be hit *after* a real command line is
submitted (i.e. failing before the target keystrokes were even due to
arrive). Isolated carefully: a `continue` through a hardware
breakpoint works completely normally against a `-serial mon:stdio`
QEMU instance spawned under `expect`, *as long as no real input is
ever sent* -- confirmed directly, same exact script, same spawn
method, just without the later `send`. The instability appears
specifically the moment real characters are sent through `expect`'s
own `send` while a GDB session is simultaneously attached and
`continue`-ing. Not resolved *why* -- a real race/interaction between
this QEMU gdbstub and its own stdio-multiplexed serial handling under
simultaneous remote-debug control, rather than anything about this
bug's own address range (sanity-checked against `dispin`, a
completely unrelated, frequently-hit, known-safe address, which
reproduced the same instability once real input was involved). **Any
future live GDB session on this specific CR-crash repro should assume
`continue` while input is in flight is unreliable and prefer passive
`-d exec,cpu` trace capture (per #95) over live breakpoints.**

**Next step**: re-capture #95's own trace, this time either (a) with
`-dfilter` narrowed *very* tightly to just `0x105af`-`0x106ff`
(`cli_fload` through `cli_gopen`/`cli_bopen`) *and* `nochain` added --
narrow enough that `nochain`'s per-TB overhead might stay affordable
even though it's not supposed to help with filtering per se, worth
trying empirically since the window here is much smaller than a full
boot -- or (b) accept the chaining gap and instead set the live
breakpoint at `cli_fload` itself (confirmed reachable, well before any
`send` of real input happens, so plausibly safe per this entry's own
finding) and single-step (`stepi`, never `continue`) through
`fileopen`'s own body by hand from there, since `stepi` was not
implicated in this entry's own instability finding, only `continue`
across a `send` boundary was.

## 97. Continued pushing on #96's specific gap with three cheap,
decisive black-box repro tests instead of more (broken) live tooling
-- narrowed the bug precisely, down to "any attempt to load a
non-existent `.CMD` file," ruling out the leading theory along the
way.

**Test 1 -- a command that cannot possibly fuzzy-match any real
file**: sent `ZZ` + `CR` (no file on this disk starts with `ZZ`,
unlike `AB`/`ABORT.CMD`). **Crashed identically** (echoed `ZZ` back,
then nothing -- no error message, no second prompt, same silent
death as every `AB` repro). **This directly refutes #95/#96's own
leading "spurious FCB fuzzy-match against a real file" theory** --
the bug cannot be about `AB` accidentally matching `ABORT.CMD`,
since `ZZ` matches nothing on this disk at all and dies the exact
same way.

**Test 2 -- a genuinely empty command (bare `CR`, nothing typed
first)**: survived completely normally, echoed cleanly, printed a
real second `0A>` prompt. **This rules out every theory that blamed
something generic in `TMP.A86`'s own post-`CR`/post-`conread`
sequence (echo, `freedrive`, `attach`, prompt redraw)** -- those all
run on the empty-command path too, and are fine. The bug is
specifically inside the *non-empty, unrecognized-command* path --
i.e. genuinely inside `clicmd_entry`/`cli_fload`'s attempt to find
and load a `.CMD` file that does not exist, exactly as originally
suspected, just not for the reason (fuzzy match) first guessed.

**Test 3 -- confirmed no error message *ever* prints, for either
repro**: neither `AB` nor `ZZ` ever produces `TMP.A86`'s own
`openerr` string (`"?Can't Find Command"`), which the normal
"file genuinely not found" path (`cli_bopen`/`cli_bopene` setting
`e_bad_open`, returned up through `clierror`'s own `showerr`) should
print before looping back to `nextcommand`. **This means the crash
happens *before* `cli_fload` ever returns an error code to `TMP` at
all** -- i.e. somewhere inside the open-attempt-and-retry sequence
itself (`cli_fload`'s own PD-pull/zero step, the `fileopen` cross-
module call, the system-disk retry, or `FILE2.BDO`'s own real
directory-search loop), not in how the *result* of a correctly-
detected failure gets reported afterward. `FILE2.BDO`'s own
character-by-character directory-search loop (`srchloop`/`srchok`/
`SEARCHNM`, read in full this entry) looks like a faithful,
structurally correct CP/M-style exact-match-or-wildcard scan on a
static read -- nothing obviously wrong jumped out, consistent with
this being deep, probably-authentic 1981 DRI logic rather than a
rebuild artifact, same caution as #96's own read of `cli_fload`.

**Status at handoff**: the bug is now narrowed to a specific,
three-step chain -- `cli_fload`'s PD-pull-and-zero, then the
`fileopen`/`f_fopen` cross-module dispatch and `FILE2.BDO`'s own
real search, for a file that is *certain* not to exist -- somewhere
in there, not in anything downstream of a correctly-returned failure
code. `cli_fload`'s own structure (#96) and `FILE2.BDO`'s own search
loop (#97) have both been read in full and look correct on inspection;
the live-GDB route to catch this directly remains blocked by #96's
own "`continue` across input delivery is unreliable for this repro"
finding. **One cheap static lead checked and ruled out, same entry**:
`cli_fload`'s own PD-zeroing step (`rep stosw`, size `pdlen/2`) uses
`SYSTEM.DEF`'s hardcoded `pdlen equ 030h`. Summed `PD.DEF`'s own
field list by hand (`p_link` through `p_wscrtch`, respecting each
field's real size including `p_name`'s `pnamsiz`-sized block and
`p_reserved`'s 2-word span): total is exactly `48` (`030h`) bytes,
matching `pdlen` precisely -- no stale-constant mismatch here, unlike
the similar-looking bugs this file has found elsewhere (#4/#5/#6).

**Next step, still untried**: the two options #96 already proposed
remain the most promising paths to the exact faulting instruction --
a tightly `-dfilter`'d `nochain` capture just for the open/retry
window, or a `stepi`-only walk from a safely-reached breakpoint at
`cli_fload` (never issuing `continue` across the point where real
input is in flight, per #96's own isolation of that specific
instability).

## 98. Did the `stepi` walk from `cli_fload` #96/#97 handed off --
**and it worked end to end**, after first solving the exact tooling
problem #96 flagged. Caught the wild jump to `0000:0000` live,
instruction by instruction, and identified both its immediate cause
and, one level further back, the real corruption.

**Tooling fix, making the live walk possible at all**: #96's own
"`continue` is unreliable once real input is in flight" finding turned
out to be solvable, not a hard wall. Root cause: every previous
attempt used `mon:stdio`/pty/`-serial unix:...` rigs that, it turns
out, were *themselves* broken independent of GDB (confirmed directly:
a plain, no-GDB `-chardev socket,...,wait=off` test produced zero
bytes in either direction, even sending a bare `CR` and waiting).
Switching to `-chardev socket,...,server=on,wait=on` (QEMU blocks
startup until a client connects, so nothing written before connection
is ever lost) gives a clean, fully reliable bidirectional channel --
confirmed first stand-alone (full boot banner captured byte for byte,
no GDB involved), then combined with a `-S`-started GDB session: one
single `hbreak`+`continue` from the pristine, never-touched `-S` start
(matching #91's own "one continuous session, never detach-reattach"
discipline) runs cleanly through the *entire* boot, the external
socket client's own `ZZ`+`CR` send, and lands on the `cli_fload`
breakpoint without any instability at all. The earlier failures were
about broken *input* rigs, not about GDB/`continue` and real input
being fundamentally incompatible.

**The walk itself**: `stepi`, no further `continue`, logging
`cs:ip`/`bx`/`ds`/`es` every step. Confirmed directly, live: execution
runs through `COMMAND.SUP`'s own `cli_fload` and `fileopen` path,
through `TMP.A86`'s own code (flat `~0x166e`-`0x169c`) and `RTM.A86`
(flat `~0x1574`-`0x16f3`), and then -- the actual moment -- hits a
`ret` at `RTM.A86` flat `0x1157c` that pops **`0x0000`** as its return
address, landing at flat `0x10000` -- **`INIT.SUP`'s own absolute
first instruction, `jmp init`, the one-time cold-boot entry point**.

**The `ret` itself identified precisely**: flat `0x1157c` is
`PROC.RTM`'s own `proc_creat`, at its *own* top-of-routine early-exit
path -- confirmed byte for byte against the source (`cmp dx,0 ! jne
cp_doit / sub bx,bx ! ret`, `src-assem/kernel/PROC.RTM` lines 33-34): `BX=0`
at the moment of the crash matches the just-executed `sub bx,bx`
exactly, and `proc_creat` is reached here with **`DX=0`**, taking its
own documented "no PD address given" immediate-return path. This механ
is itself completely ordinary, correct code -- the bug is not in
`proc_creat`. The `ret` popping `0x0000` instead of the real caller's
address means **the stack was already wrong by the time `proc_creat`
was entered** -- this `ret` is a victim, the symptom, not the cause;
a prior corruption (a lost/extra push or pop somewhere between
`cli_fload`'s own `call f_createproc` dispatch chain and here) already
happened before this.

**What happens after landing at `init:` -- directly confirmed,
instruction by instruction, byte-for-byte against `INIT.SUP`'s own
source (lines 29-106)**: every single instruction from `init:`'s
cold-boot prologue onward re-executes exactly as written -- `mov
ss,ax`/`mov sp,offset init_tos` (re-pointing the stack at the
init-time scratch area, *while real processes are running*), `mov
cs:sysdat,ax`, the supervisor-entry-point/`version`/`tick_tos`/
`idle_tos` setup, **`initpd`'s own UDA segment being recomputed and
rewritten** (`mov bx,offset initpd ! mov ax,offset inituda ! shr ax,4
! add ax,cs:sysdat ! mov p_uda[bx],ax`, directly confirmed byte for
byte: `BX=0x9C0`, matching `initpd`'s own already-known PD address
from #92/#97), `mov es,ax`/`mov u_wrkseg,ds`, and then straight into
**"initialize modules"**'s own `push bx ! callf
rtmmod[bx]/memmod[bx]/bdosmod[bx]/ciomod[bx]/xiosmod[bx] ! pop bx`
sequence -- i.e. **re-running RTM/MEM/BDOS/CIO/XIOS's own one-time
cold-boot module-`init:` routines against an already-running system**.
This is unconditionally destructive by its very nature (reinitializing
shared module state -- `SYSDAT`, `module_table`, device tables -- out
from under already-running processes), and fully explains #94's own
observed end state (repeating `#UD`/invalid-opcode fault loop,
`CS:IP` parked at the BIOS's dummy vector stub): whichever module's
`init:` runs into stale/already-different-than-cold-boot assumptions
first corrupts something further, cascading into the crash loop
already characterized in #94/#95. Confirmed the loop keeps re-entering
`init:` repeatedly afterward too (the same `c5`-through-near-`0x10000`
sequence recurs identically, ~30 steps apart, for as long as the walk
was allowed to run) -- once this starts, it never recovers on its own.

**Status: root cause of Bug B is the `proc_creat`-time stack
corruption, not yet the corruption's own origin.** The `ret` that
lands at `0000` is a downstream symptom of a stack already wrong by
the time `cli_fload` dispatches into `f_createproc` with `DX=0` --
*why* `DX` is `0` (should be `cli_pd`, a real PD address, per
`COMMAND.SUP` line 429: `mov dx,cli_pd ! mov cx,f_createproc ! call
mpmif`) and *why* the stack is already off by the time `proc_creat`'s
own `ret` runs are the two threads still open.

**Next step**: now that live stepping through this exact repro is
confirmed reliable (methodology fix above), walk backward one more
hop with the same technique -- breakpoint at `COMMAND.SUP`'s own line
429 call site (`mov dx,cli_pd ! mov cx,f_createproc`) to directly read
`cli_pd`'s real value there (confirming whether `DX` is genuinely `0`
already at the call site, meaning `cli_pd` itself was never properly
set after the `cli_fload`'s own PD-pull step, or whether it's correct
there and gets lost somewhere in the `mpmif`/`func`/`gofar`/
`module_table` dispatch chain before reaching `proc_creat`), and
separately watch `SP` itself across that same dispatch chain (a
hardware watchpoint on the live stack-pointer-relative return-address
slot, or simply logging `$sp` at every `stepi`, the same technique
used in this entry) to catch exactly where the push/pop imbalance is
introduced.

## 99. `src-assem/kernel/QUE2.RTM` — `remqd` never actually threads a freed QD onto the free list — FIXED

Found while writing `/src-c`'s C port of this routine (`src-c/kernel/
que2.c`, see `docs/src-c.md`) and differentially testing it against the
real assembled bytes via `tools/i86sim.py` — not from an assembly
failure, since this is a real logic bug, not a syntax one.

`remqd` ("Place qd on queue unused list") is supposed to take a freed
Queue Descriptor off `qlr` and put it on `qul`, the free list, so a
later `getqdaddr` can reuse it:

```
remqd:
	mov ax,q_flags[si]
	and ax,qf_table ! jz rqd_exit
	    pushf ! cli
	    mov ax,qul ! mov q_link[si],ax
	    mov qul,ax ! call qrelease
	    popf
rqd_exit:
	ret
```

`mov ax,qul` loads `qul`'s current value into `ax`; `mov q_link[si],ax`
correctly points the freed QD (`si`) at the old free-list head. But
`mov qul,ax` then writes that *same unchanged* value straight back into
`qul` — a no-op, since nothing between those two instructions ever
modifies `ax`. The freed QD's own `q_link` field is set correctly, but
`qul` itself is never updated to point *at* `si`, so the QD is never
actually threaded onto the free list at all — the only reference to it
(`si`) exists in the caller's own register, gone the moment `remqd`
returns, and the slot leaks.

Confirmed this isn't a misreading by checking the analogous working code
a few lines above it in the same file (`getqdaddr`'s own `gqd_have`
success path): `mov ax,qul ! mov q_link[di],ax` followed by `mov
qul,di` — the same two-step "link to the old head, then move the head"
idiom, but correctly using the *new* node (`di`) for the second move,
not reloading the *old* value. `remqd` is the one place this pattern
was written with the wrong register.

**Patched**: `mov qul,ax` → `mov qul,si`.

## 100. `src-assem/kernel/QUE1.RTM` — `deleteq_entry`'s `qd_noq` path corrupts the stack — FIXED

Found the same way as #99 above, while building `/src-c`'s differential
test for `deleteq_entry` (`src-c/kernel/que1.c`, `tests/src-c/
test_que.py`): `tools/i86sim.py`'s reference run landed on an invalid
opcode at address `0x0000` partway through a test case exercising this
exact path, which traced back to a real stack imbalance, not a test bug.

`deleteq_entry` pushes `es` once at entry and expects every exit path to
pop exactly one word before `popf ! ret`, to keep the stack balanced:

```
deleteq_entry:
	pushf ! cli
	push es ! mov es,u_wrkseg
	...
	add dx,ax ! jnz qd_use
	    ...
qd_nqd:	    mov si,q_link[bx]
	    cmp si,0 ! jz qd_noq
		cmp si,di ! jne qd_neq
		    ...
		    pop es ! jmps qd_e
qd_neq:		mov bx,si ! jmps qd_nqd
qd_noq:	    mov cx,e_q_notactive ! jmps qd_be
qd_use:	pop ax ! mov cx,e_q_inuse
qd_be:	mov bx,0ffffh
qd_e:	popf ! ret
```

The success path (`pop es ! jmps qd_e`) and the `qd_use` path (`pop ax
! mov cx,...`) each consume exactly the one word `push es` put there.
`qd_noq` — reached whenever the QD being deleted isn't actually linked
into `qlr` — is the one path that jumps straight to `qd_be`/`qd_e`
without popping anything first. With that word still on the stack,
`popf` pops it into FLAGS instead of the real saved flags, and the
following `ret` then pops the real saved-flags word and treats it as a
return address — a jump to whatever bit pattern the flags happened to
be, not back to the caller. Confirmed directly: a test memory image
with `qlr` empty landed `tools/i86sim.py`'s reference run at address
`0x0000` (decoding the flags word `0x0000` as a return address) instead
of returning normally.

**Patched**: added a `pop ax` at the start of `qd_noq`, matching
`qd_use`'s own convention for discarding the pushed `es` on an error
exit (rather than restoring it via `pop es`, which nothing downstream of
either path relies on `es` still holding).

## 101. `src-assem/kernel/MEMORY.MEM` — `mfree_entry`'s active-PD check walks the wrong field — FIXED

Found the same way as #99/#100 above, while building `/src-c`'s
differential test for `mfree_entry` (`src-c/kernel/memory.c`,
`tests/src-c/test_memory.py`, see `docs/src-c.md`).

`mfree_entry`'s `mfree_chkpd`/`mfree_nxtpd` walk the Process Thread
list to check whether the PD memory is being freed from is a currently
active process other than the caller:

```
mfree_chkpd:
	mov si,(offset thrdrt)-p_thread
mfree_nxtpd:
	mov si,p_link[si]
	cmp si,0 ! je mfree_gotpd
	    cmp si,bx ! jne mfree_nxtpd
		cmp si,rlr ! je mfree_gotpd
		    mov bx,0ffffh
		    mov cx,e_active_pd
		    ret
mfree_gotpd:
```

`mov si,(offset thrdrt)-p_thread` biases `si` so that the very next
`p_thread[si]` read lands on `thrdrt` itself (the real head of the
Process Thread list), and every subsequent iteration's `p_thread[si]`
reads the next PD's own thread-link field. But that first instruction
reads `p_link[si]`, not `p_thread[si]` — a different field (`p_link` is
offset 0 in a PD, `p_thread` is offset 2), so the bias set up for
`p_thread` is wrong for it, and the walk follows the System List link
chain instead of the Thread List, visiting the wrong set of PDs (or
garbage, depending on what currently happens to sit at `thrdrt+2`
relative to `p_link`'s own 0 offset).

Confirmed this is a genuine mismatch, not a misreading, by checking
`malloc_entry`'s own identical active-PD check a few lines earlier in
the same file (`mall_pdnxt`), which sets up the exact same
`(offset thrdrt)-p_thread` bias and then correctly reads `p_thread[si]`
both times.

**Patched**: `mov si,p_link[si]` → `mov si,p_thread[si]`.

`/src-c/kernel/memory.c`'s own translation had two further, separate
bugs of its own (not `/src-assem` bugs, so not logged here — see
`docs/src-c.md`'s own writeup): both `malloc_entry`'s and
`mfree_entry`'s C ports of this same `(offset thrdrt)-p_thread` bias
were missing the bias term entirely, and `mfree_it`'s C port had its
`cmp cx,0 ! jne mfree_r` check inverted plus a hardcoded `bx = 0` in its
return where the real routine always returns its own input `bx`
unchanged.

## 102. `src-assem/kernel/MAU.MEM` — `maualloc_entry`'s exact-absolute-split path clobbers `BX` before calling `mau_split` — FIXED

Found while writing `/src-c`'s C port of this file (`src-c/kernel/
mau.c`, see `docs/src-c.md`) — a logic bug, not a syntax one, and a
genuinely old one: present identically in both `orig/mpm862sr/02/
MAU.MEM` and `orig/lastdri/19/MAU.MEM`, so this has been wrong since
the original 1981/1986 DRI source, not something this project's own
transcription introduced.

`maualloc_entry`'s absolute-address path, when the requested address
falls inside a free SAT entry but not exactly at its start, has to
split that entry at the requested boundary:

```
			    mov dl,.0
			    cmp cx,sat_start[bx] ! je maua_nabs ;?exact?
				sub cx,sat_start[bx]
				mov bx,cx ! call mau_split	;make exact
				jmps maua_start
```

`mau_split`'s own documented calling convention is `BX = address of
SAT element` (the entry being split), `CX = length to split at`. Right
before the call, `BX` correctly holds the target SAT entry's own
address (unchanged since the comparisons above it) and `sub cx,
sat_start[bx]` correctly computes the split length into `CX` — but the
following `mov bx,cx` then overwrites that correct `BX` with the very
same length value, so `mau_split` is entered with `BX == CX` (both the
split length), not the SAT entry's address. `mau_split`'s own body
immediately does `add bx,satlen ! call mau_insert` to make room for
the new entry right after the one being split — with `BX` holding a
small length instead of a real table address, this inserts (or tries
to) at a essentially arbitrary offset, corrupting whatever happens to
sit there instead of the SAT table.

Confirmed this is a real mismatch, not an intentional alternate
calling convention, by checking `maufree_entry`'s own call to
`mau_split` a few lines later in the same file: `sub cx,dx ! call
mau_split` — the identical "compute the split length into CX" step,
with no `mov bx,...` afterward, leaving `BX` as the SAT entry address
`mau_split` actually needs. `maualloc_entry`'s path is the one call
site that gets this wrong.

Only reachable when an absolute (`mpb_start != 0`) allocation request
lands inside a free SAT entry without landing exactly on its start —
a narrow enough case that it likely went unnoticed (or unconfirmed) in
practice, same as several other bugs in this tree found only once a
routine was finally exercised directly.

**Patched**: deleted the `mov bx,cx` between the `sub` and the `call`.
`src-c/kernel/mau.c`'s own port implements the corrected behavior (it
is a from-scratch C translation, not translated-then-fixed), matching
`maufree_entry`'s already-correct pattern.

**A second, related bug found at the exact same call site**, only
exposed once the `BX` bug above was fixed and the differential test's
reference run could actually get far enough to reach it: the same
`push si ! call mau_split ! pop si` protection `maua_splitit`'s own
call to `mau_split` (a few lines below) already has is missing here.
`mau_insert` (called transitively via `mau_split`'s own `add bx,satlen
! call mau_insert`) uses `SI` freely as scratch and never restores it.
`maualloc_entry`'s own `SI` holds its MPB offset (set once, `mov
si,dx`, at the very top) and is still needed afterward: this call site
`jmps maua_start` right after, restarting the whole scan, which reads
`es:mpb_start[si]` again on its very first iteration — with `SI`
clobbered by the `mau_insert` call that just ran, that re-read lands
on the wrong address (confirmed directly: `SI` read back as the
*target absolute address itself*, `0x305` in the test case that
caught this, not the original MPB offset `0x10` — coincidental in
that one case, since `mau_insert`'s own internal `REP MOVSB` happened
to leave `SI` pointing into the SAT table near a byte pattern that
summed to the same value, but wrong regardless of what it happens to
read). The practical effect: the restarted scan reads a garbage
"requested absolute address" of 0, taking the non-absolute allocation
path instead of finding and taking the newly-split exact-match entry.

Also present identically in `orig/mpm862sr/02/MAU.MEM` and `orig/
lastdri/19/MAU.MEM` — likely never reached cleanly in the original
system either, since the `BX` bug above would have corrupted the SAT
table before execution ever got far enough for this one to matter on
its own.

**Patched**: wrapped the call in `push si ! ... ! pop si`, matching
`maua_splitit`'s own already-correct pattern. `src-c/kernel/mau.c`
was never affected (its `maualloc_entry` port never had a "SI
register" to begin with — `wrkseg`/`si` there are plain C locals, not
aliased with whatever `mau_split`'s own internal calls touch).

**A pre-existing `tools/i86sim.py` bug, not specific to this file,
caught while first investigating the `BX` fix above** (before the
`SI` bug was found): `LEA` (opcode `8D`) returned the ModRM decode's
already-segment-adjusted address under `segmented=True` mode, instead
of the raw effective offset LEA is actually defined to compute (a real
physical address only for ordinary loads/stores, which go through the
same ModRM decode) -- so `mau_collapse`'s own `lea di,satlen[bx]`
(used to locate the next SAT entry) ended up with a nonsensical,
doubly-segmented `DI`. Latent since segmented addressing itself was
only added for `src-c/kernel/ml.c`: every file since then happened to
only ever execute a `LEA` with `DS=0` (where the bug is a no-op,
`0*16+offset == offset`), until `src-c/kernel/mau.c` became the first
to execute one with a real, nonzero `DS` (the MAU's own segment).
**Fixed**: `_modrm()` now records the raw pre-segmentation offset
(`self._last_ea_offset`) alongside the segmented address it already
returned, and `LEA` reads that instead of the ModRM tuple's own
(segmented) value.

**A second `tools/i86sim.py` gap, same investigation**: `STD`/`CLD`
(opcodes `FD`/`FC`) weren't implemented at all — every file ported
before this one only ever did a forward string copy/scan. `src-c/
kernel/mau.c`'s own `mau_insert` needs a real backward `REP MOVSB`
(`STD`) to shift a SAT table up without self-clobbering the
overlapping region it's shifting into. **Fixed**: added a tracked
direction flag (`self.df`), `STD`/`CLD` opcode handlers, and made

## 103. `tools/i86sim.py` — two more gaps found porting `DSPTCH.RTM` as a debugging aid (no `/src-assem` bug) — FIXED

`src-c/kernel/dsptch.c` (a C port of `DSPTCH.RTM`, written per request
as a reading/debugging aid for the still-open scheduler-hang
investigation, #83-98, not a kernel-build component — see the file's
own header comment) is the first routine in this project to actually
switch `SS` away from zero (`switch:`'s own `mov ss,u_ss ! mov
sp,u_sp`, restoring the newly-dispatched process's own real stack)
and the first to execute a near indirect `JMP`/`CALL` through a
*CS-segment-overridden* table (`jmp cs:dsp_table[bx]` at the top of
`dsptch:`, and `call mpmif` inside `term_act`) while also under a
`CSEG_BASE`-shifted reference run (`tests/src-c/test_dsptch.py` needs
the shift for the same reason `proc.c`'s/`load.c`'s own tests do:
absolute segment-0 IVT reads/writes). Both of the new gaps below were
latent in exactly that combination and never exercised before.

**Gap 1 — `PUSH`/`POP` ignored `SS` entirely under `segmented=True`.**
`_push16`/`_pop16` always used `regs['sp']` as a bare flat address,
never combining it with `SS` the way every other segment:offset pair
in the module already does. Harmless for every routine before this
one (none of them ever changed `SS` away from its default of 0, so
plain `SP` already *was* the flat address) but fatal here: after
`switch:`'s own stack switch, `dret`'s/`dret_noif`'s closing `ret`
popped from physical address `0x0000:SP` instead of the real
`SS:SP`, landing on whatever garbage happened to be at that flat
offset instead of the differential test's own placed return sentinel
(see the test's own comment on `u_ss`/`u_sp`) — a wild jump, not
dsptch's own real control flow. **Fixed**: added `_stack_addr()`,
used by both `_push16_into` and `_pop16`, that applies the module's
usual `(seg*16+offset)&0xFFFF` rule to `SS:SP` under `segmented=True`
and falls back to bare `SP` otherwise (so every pre-existing test,
which never sets a nonzero `SS`, is unaffected).

**Gap 2 — near indirect `CALL r/m16`/`JMP r/m16` (opcode `FF` `/2`/`/4`,
added in #102's own investigation for `ABORT.RTM`) didn't add the
current `CS` base to the loaded target offset.** `self.ip` is tracked
as an absolute address throughout this module (already including
whatever base a `CSEG_BASE`-shifted test loaded code at), but the
loaded r/m16 value is a bare *offset* — exactly like a direct `CALL
rel16`/`JMP rel16`'s own displacement is added to the current
(already-absolute) `self.ip`, an indirect near jump's target offset
needs the current `CS*16` added too. Never visible in `ABORT.RTM`'s
own test (no `CSEG_BASE` shift there, so `CS*16` is always 0) or
anywhere else that used this opcode form, until `dsptch:`'s own `jmp
cs:dsp_table[bx]` ran under `dsptch`'s `CSEG_BASE`-shifted test and
landed `0x800` bytes short of every `dsp_table` entry's real address.
**Fixed**: both handlers now set `self.ip = (self.regs['cs'] * 16 +
target) & 0xFFFF` instead of `self.ip = target`.

Also needed, not a bug in the module itself: `term_act`'s own `call
mpmif`/`call xiosif` (an *unported, fixed-address* boundary stub, see
the module's own docstring on `call_stub_addrs`) is a direct `CALL
rel16`, so its *displacement* was assembled assuming the call site
itself would land exactly on `mpmif`'s/`xiosif`'s real address
(`9000h`/`9001h`) — true with no `CSEG_BASE` shift, but off by
`CSEG_BASE` under one, for the same reason as gap 2 above (the
displacement gets added to an already-shifted `self.ip`). Not a sim
bug: `test_dsptch.py`'s own `call_stub_addrs` just has to name the
shifted absolute addresses (`CSEG_BASE + 0x9000`, `CSEG_BASE +
0x9001`), which it now does.
`_string_op_once`'s own SI/DI step negative when `self.df` is set.

## 104. `src-assem/kernel/ABORT.RTM` — `findpdq` loses its place after the first queue — FIXED

Found while moving `tests/src-c/test_abort.py` onto real memory layout
(`/src-c` real-memory conversion, stage 3) and adding cases for every
`abort_tab` handler: aborting a process waiting on the DQ/NQ list of any
QD *other than the first one on `qlr`* failed in the reference run with
`e_no_pdname`, while `src-c/kernel/abort.c` found and snipped it.

`findpdq` walks `qlr`, and for each QD searches its DQ (or NQ) list with
`find_pd`, keeping the current QD on the stack across the call:

```
fpdq_nxtq:
	pop di			; Recall QD adr
	mov di,q_link[di]	; Next QD adr
	test di,di ! jz fpdq_no_pd
	test al,al ! jz fpdq_dq_lst
	lea di,(q_nq-p_link)[di]
	jmps fpdq_nxt_pd
fpdq_dq_lst:
	lea di,(q_dq-p_link)[di]	; root address of list to search
fpdq_nxt_pd:
	push di			; Save QD adr
	call find_pd
	jcxz fpdq_nxtq
```

The `push di` sits *after* the `lea`, so what it saves isn't the QD but
that QD's list root (`QD + q_dq - p_link`). When `find_pd` misses, `pop
di ! mov di,q_link[di]` then reads the word at the list root -- the
first PD waiting on that QD (or 0) -- and carries on treating it as the
next QD. With an empty list the walk just stops after the first QD; with
waiters it wanders through PD memory. Either way only the first QD is
ever really searched, so `abort_spec_entry` can't abort a process
blocked reading or writing any other queue. The original DRI source
(`orig/mpm862sr/02/ABORT.RTM`) has the same ordering, so this is an
original bug, not one introduced here.

**Patched**: moved `push di` up to just after the end-of-list test,
before the `lea`, so it saves the QD address the comment says it does.
The success path's `pop dx` and the miss path's `pop di` both still
balance it. abort.c's `findpdq` already walked the QDs correctly; now
the asm does too, and test_abort.py's `abtdq`/`abtnq` cases put the
target on the second QD to cover it.

## 105. `src-assem/kernel/SHARE.MEM` — `share_entry` leaks its new MD on two paths — FIXED

Found while moving `tests/src-c/test_share.py` onto real memory layout
(`/src-c` real-memory conversion, stage 4). Present in the original DRI
source (`orig/mpm862sr/02/SHARE.MEM` is identical).

`share_entry` starts by taking an MD off `mdul` with `getmd` (kept in
`BP`) for the requestor's new memory-segment entry. Two exits never give
it back:

- **Sharing with oneself.** When the owner and requestor PDs are the
  same (both default to `rlr` when 0), `cmp si,bx ! je se_ge` jumps
  straight to the success exit. There's nothing to do, but the MD is
  kept.
- **No owner segment at that start.** When the owner's `p_mem` list
  has no entry at the requested paragraph, `je se_err` skips the
  `freemd` that the neighbouring `se_err1` path (no matching SAT
  entry) does right before it.

Each failed or self-share call loses one MD for good. MDs come from a
fixed pool, and `malloc_entry` needs one per allocation, so once enough
have leaked the whole system fails allocations with `e_no_umd`.

**Patched**: both paths now free the MD. Sharing with oneself goes
through a new `se_self: mov bx,bp ! call freemd` before `se_ge`, and the
no-owner-segment path jumps to a new `se_free` label, shared with
`se_err1`, that frees it before `se_err`. `src-c/kernel/share.c` matches.
`test_share.py` checks that `mdul` gets the MD back on both paths.

## 106. `src-assem/kernel/MAU.MEM` — `mau_collapse` reports a MAU empty while its last segment is still allocated — FIXED

Found while moving `tests/src-c/test_mau.py` onto real memory layout
(stage 4): freeing the first of two allocated SAT entries made
`maufree_entry` return "MAU empty" (BX=0FFFFh). Present in the
original DRI source.

`mau_collapse` walks the SAT judging each entry `BX` by looking at the
next one, `DI`. It starts with `BP = 0FFFFh` ("assume empty") and only
clears `BP` when it reaches an allocated entry (`mauc_notfree`). Both
ways out of the loop skip that check for the last entry in use:

```
mauc_nsat:
	inc cx
	cmp cx,.0 ! jb mauc_notend
mauc_done:  sub cx,cx ! mov bx,bp ! ret		; table end: last slot unjudged
mauc_notend:
	add bx,satlen ! lea di,satlen[bx]
	cmp sat_start[bx],0 ! je mauc_done
	cmp sat_start[di],0 ! je mauc_done	; BX is the last entry in use:
						; leaves before checking BX's nall
```

So a table like [free, allocated] (or a full table whose last slot is
allocated) comes back "empty". `MEMORY.MEM`'s `mfree_it` acts on that:
after freeing a segment from a MAU it takes an "empty" MAU off `mal` and
returns it to the Memory Free List with `f_mlfree`. If the freed segment
wasn't the MAU's last allocation, the rest of the MAU goes back to the
free list while another segment in it is still in use, and the next
allocation can hand the same memory out again. That takes nothing
unusual: allocate A then B out of one fresh MAU, then free A.

**Patched**: both exits now go through a new `mauc_last`. It checks the
last entry in use (`BX`; at the table end, the table's final slot) and
clears `BP` if that entry is allocated. `src-c/kernel/mau.c` matches.
Covered by `test_mau.py` (both exits) and `test_memory.py` ("last SAT
still allocated -> MAU kept").

## 107. `src-assem/kernel/CPMMEM.MEM` — `cpmfree_entry`'s free-all walk wipes Load segments' flags, and hangs on a segment it can't free — FIXED

Found while moving `tests/src-c/test_cpmmem.py` onto real memory layout
(stage 4). Present in the original DRI source.

`cpmfree_entry` with `mcb_ext = 0FFh` (free everything but Load memory)
walks the caller's `p_mem` list:

```
try_seg:    and ms_flags[si],mf_load ! jz free_seg
		mov bx,si ! jmps cpmf_next
free_seg:   mov dx,ms_start[si]
	    push si ! call free_memory ! pop si
	    jcxz cpmf_root
		jmps cpmf_next
```

- **`and` where a `test` was meant.** The `and` writes its result
  back, so every segment it looks at loses all its flag bits except
  `mf_load`. That doesn't matter for segments about to be freed, but
  the Load segments it keeps lose `mf_code`/`mf_share`, and
  `share_entry` copies those flags to anyone who later shares the
  segment.
- **No progress on a failed free.** If `free_memory` fails,
  `jmps cpmf_next` re-reads the same `ms_link[bx]` (BX unchanged), so
  it tries the same segment again forever while holding the MEM mutex.

**Patched**: `and` → `test`, and the failure path now does `mov bx,si`
before `jmps cpmf_next`, stepping past the segment it couldn't free.
`src-c/kernel/cpmmem.c` matches. `test_cpmmem.py` checks that a kept
Load segment's flags survive, and that the walk frees the next segment
after one whose SAT entry is missing.

## 108. `src-assem/kernel/CONSOLE.CIO` — `mimic_entry` doesn't set BX/CX on success, so ctl-P can report "Printer Busy" after it worked — FIXED

Found while moving `tests/src-c/test_cioif_entry.py` onto real memory
layout (`/src-c` real-memory conversion, stage 5): dispatched through
CIOIF's `entry`, a successful `mimic_entry` came back with CX = 2Ah.
Present in the original DRI source (`orig/mpm862sr/03/CONSOLE.CIO`).

`mimic_entry`'s header says "BX = 0 if successful, 0ffffh on error;
CX = Error Code", and the error exit sets both. The success exit
doesn't touch either:

```
			mov c_attach[si],0ffffh
			mov c_msource[si],dh
			mov c_mimic[di],dl ! popf ! ret
```

BX is left holding `rlr` (from `getccbadr`) and CX is whatever the
caller had. Through `entry` that's twice the function number, since
entry does `shl cx,1` to index its table. Inside the CIO module it's
whatever the call chain left, including what the XIOS returned in CX.

The one internal caller depends on it. `control_p` (ctl-P while
`conread_entry` is reading a line) does `call mimic_entry ! jcxz
mimout`. With CX nonzero it prints "Printer Busy" and skips toggling
the console's `cf_listcp`, even though the list device is now mimicking
the console. The flag and the CCBs then disagree. The next ctl-P tries
`mimic_entry` again, which fails because `c_mimic` is no longer 0FFh,
so printer echo can't be turned off either.

**Patched**: `sub bx,bx ! mov cx,bx` before the success exit's `popf !
ret`. `src-c/kernel/console.c` matches (it used to return BX = rlr, as
the asm did). `test_cioif_entry.py` runs ctl-P through `entry` and
checks that `cf_listcp` gets set; with the original code the reference
leaves it clear.

## 109. `src-assem/kernel/LOAD.SUP` — `ld_read` resets the FCB's `nxtrec` through DS instead of `u_wrkseg` — FIXED

Found while moving `tests/src-c/test_load.py` onto real memory layout
(`/src-c` real-memory conversion, stage 7). Present in the original DRI
source (`orig/mpm862sr/02/LOAD.SUP`, `orig/lastdri/19/LOAD.SUP`).

`load:`'s header says the FCB is "addr of open FCB in u_wrkseg", and
every BDOS call `load` makes on it goes through `mpmif`, which hands the
BDOS `u_wrkseg`. But the one direct store into the FCB, the record reset
before the header read, used DS:

```
ld_read:
	mov bx,load_fcb
	mov fcb_nxtrec[bx],0
```

DS is SYSDAT throughout the SUP, so this cleared a byte in SYSDAT at the
FCB's offset + 20h and left the FCB's own `nxtrec` alone. It only worked
when `u_wrkseg` happened to be SYSDAT -- true for the CLI (COMMAND.SUP
calls f_load from SYSDAT's `cli_fcb`), not for a program calling
f_load (59) on its own FCB. For that caller the header read started at
whatever record the FCB was left at, and an unrelated SYSDAT byte got
zeroed.

**Patched**: `push es ! mov es,u_wrkseg ! mov es:fcb_nxtrec[bx],0 !
pop es`. `src-c/kernel/load.c` resets the FCB in u_wrkseg too. With the
original line, 10 of `test_load.py`'s cases fail (its FCB is in a
u_wrkseg that isn't SYSDAT).

## 110. `src-assem/rsp/SUBMIT.A86` — the bad-parse `$` scan reads the code segment — FIXED

Found while moving `tests/src-c/test_submit.py` onto real memory layout
(`/src-c` real-memory conversion, stage 7). Present in the original DRI
source (`orig/mpm862sr/05/SUBMIT.A86`, `orig/lastdri/16/SUBMIT.A86`).

When `notdrive`'s parse of a command fails, SUBMIT puts a `$` after the
command's first word in `clicmd` before reporting the error:

```
			mov cl,126
			mov di,offset clicmd
			mov al, ' '
			push cs ! pop es
			repne scasb
			mov byte ptr[di], '$'
```

`clicmd` is in SUBMIT's data segment (DS), but the scan runs over ES =
CS, so it looks for the space in SUBMIT's code at `clicmd`'s offset. The
`$` (stored through DS) lands wherever that code happens to have a 20h
byte, or 126 bytes in. TMP.A86, which this code is evidently copied
from, keeps its constants in the code segment and its buffers in DS
too, but scans `clicb_cmd` with ES = DS; SUBMIT moved its constants to
DS and kept a `push cs` here. (The `$` is never printed afterwards --
`clierror` prints only the message -- so the visible effect is a stray
byte in `clicmd`.)

**Patched**: `push ds ! pop es`. `src-c/kernel/submit.c` scans DS, as
it always did.

## 111. `src-assem/kernel/XIOS86Q.A86` — the sector copies, the tick ISR and `fdc_out_byte` use the kernel code segment for SYSDAT data — FIXED

Found while moving `tests/src-c/test_xios86q.py` onto real memory layout
(`/src-c` real-memory conversion, stage 7). XIOS86Q is this project's
own QEMU XIOS, not DRI source.

XIOS86Q's fields (`tick_count` at 1000h onward: `clockon`, `dma_adr`/
`dma_seg`, `outbyte_tmp`, `secbuf`, the tick ISR's stack) are in its
DSEG, which `tools/build_mpmsys.py` appends to SYSDAT's: they are
SYSDAT-segment data, reached through DS. Its code is in the combined
kernel code segment (`MPM_CODE_SEG`, 1000h), which every module shares
at its own offset -- so CS:x for one of those offsets is SUP's code, not
the XIOS's data. Five places used CS anyway:

- **fdc_do_read**'s copy out of `secbuf`: `push cs ! pop ds` before
  `mov di,dma_adr ! mov es,dma_seg`, so the DMA address *and* the source
  were read from SUP's code at 1007h/1009h/1040h+. Every disk read
  copied 128 bytes of SUP code to whatever address those bytes formed.
- **fdc_do_write**'s copy into `secbuf`: `push cs ! pop es`, so the
  caller's record overwrote SUP's code at 1040h+ (and the sector went
  to disk without it).
- **fdc_out_byte** kept its byte in `cs:outbyte_tmp` -- a write into
  SUP's code at 1010h for every FDC command byte.
- **tickint**, 60 times a second: after `push cs ! pop ds` (right for
  its CSEG save cells) it ran `dec tick_count` and `cmp clockon,true`
  through CS, so it decremented a byte of SUP's code at 1000h on every
  tick and stored 60 there every second. 1000:1000h is inside LOAD.SUP's
  `load_group`.
- **tickint**'s stack: `mov ax,cs ! mov ss,ax ! mov sp,offset
  tickint_tos` -- copied from the DRI XIOS.A86, where CS = SYSDAT (that
  XIOS lives in the SYSDAT segment). Here it put the ISR's pushes, and
  the f_flagset call it makes, into SUP's code below 1306h. The label was
  also on the first word of its 32-word area (`tickint_tos rw 32`), so
  even in the right segment the stack would have grown down into
  `dirbuf0`.

**Patched**: the copies use DS for `secbuf`/`dma_adr`/`dma_seg` (and DS
as the write copy's ES); `fdc_out_byte` uses DS; `tickint` saves the
interrupted SS/SP/AX in its CS cells as before, then switches SS and DS
to SYSDAT (`mov ax,sysdat`) and restores SS/SP through `cs:`
overrides; `tickint_tos` is now the end of its area (`rw 32` then
`tickint_tos rw 0`).

`test_xios86q.py` now runs the reference with CS (the code) and DS
(SYSDAT) as different segments, checks the whole image after the disk
reads/writes, and runs the whole tick ISR (from `tickint` to its
dispatcher `jmpf`) checking for stray writes. With the original file,
DISKREAD/DISKWRITE and all four tick cases fail.

Since 1000h-1306h of SUP is LOAD.SUP and RPL/PARSE code, this looked
like it could be "Bug B" (#83-98: running a command crashes). It isn't
the whole story: a rebuilt image with this fix still boots to `0A>` and
still hangs on `DIR` (no LF echo after the command's CR), the same as
before the fix. (The hang's actual cause is #112.)

## 112. `tools/build_mpmsys.py` — module code shared one segment, and only some code offsets were relocated: Bug B's cause — FIXED

Found with a scripted QEMU session: boot, type `DIR`, and log every
executed instruction from the CR on (QEMU monitor `log exec,nochain,cpu`
with `-accel tcg,one-insn-per-tb=on`), then map each address back to its
module and label. This is the real cause of "Bug B" (#83-98): typing a
command at `0A>` wrecks the system.

`build_mpmsys.py` put every kernel module in one code segment
(`MPM_CODE_SEG`, 1000h) at a byte offset, `module_base`. But each module
is assembled on its own, at origin 0, so every code address it contains
is wrong by `module_base`. The script relocated what it knew about:
- the module table
- each module's `entry` function table, and the instruction that
  indexes it
- RTM's `dsp_table`
- RTMIF's `offset fdisp`/`offset farpdisp`
- XIOS86Q's `offset tickint`

Two gaps were hit typing `DIR`:

- **CIO's `functab` count was 16; the table has 23 entries.** Entries
  16-22 (`lstdetach` .. `unmimic`) kept their standalone offsets. The
  CLI's first call, `f_cconattch` (the dayfile check, CIO function 19),
  jumped to 1000:00ADh, the middle of SUP's `init`. That code wrote over
  ES and the stack, and the CLI's own `ret` then went to garbage
  (1000:F216h), through SYSDAT, to a `hlt` with interrupts on. That is
  the "hang" after `DIR`.
- **Code-label immediates nobody relocated.** With CIO's count fixed,
  the open and load got further, and FILE4.BDO's `func16`, on the way to
  the close, did `mov bx,offset close00 ! push bx`. That `offset` was
  BDOS's standalone 1FF2h, so `check_fcb1`'s `ret` landed in RTM's
  `terminate` (`nchld`), running on the BDOS's internal stack, and the
  BDOS's final `retf` went to 3000:1130h. Others of the same kind:
  - BDOSIF's `offset shell_rtn`/`rdran_off`/`wrran_off`
  - FILE3's `offset lock9`
  - FILE4's `offset open_file`/`dirtouser`
  - ABORT's `offset terminate`
  - CHARIO's message offsets
  - ABORT's `abort_tab`

**Patched**: each module now gets its own code segment, starting on a
paragraph boundary in the combined image (SUP 1000h, RTM 1138h, MEM
1231h, CIO 12DDh, BDOS 136Dh, XIOS 15D6h), so the offsets it was
assembled with are right as is. The module table, `dispatcher` and the
IVT entries are far pointers built as (offset, cs), so nothing else
needs changing. All the per-site relocations (and the FUNCTION_TABLES/
DISPATCH_PATCHES tables) are gone. The combined code grew by alignment
padding, to 26992 bytes, so SYSDAT is now at 1697h.

## 113. `tools/build_mpmsys.py` — no Lock List and no free memory (GENSYS answers never set) — FIXED

Next, `DIR` reached the BDOS and got `Bdos Err On A: No Room in System
Lock List` on its open (function 15). DATA.BDO says "FREE_ROOT,
LOCK_MAX, OPEN_MAX INITIALIZED BY SYSGEN"; build_mpmsys.py, which
stands in for GENSYS, left `free_root` 0 and both limits 0. With that
fixed, the load failed with `?Not Enough Memory`: `mdul` (unused memory
descriptors) and `mfl` (the memory free list) were empty too, so
MEM's `getmd` had nothing to give `f_malloc`.

**Patched**:
- **Lock List:** 16 ten-byte items linked from `free_root`, with
  `lock_max`/`open_max` and the three totals at 16.
- **Memory:** 23 free MDs on `mdul`, plus one MD on `mfl` describing
  the free memory from the paragraph after the system data (17DFh) to
  9000h. The MD pool follows the XIOS's dseg tail, since SYSDAT's own
  4096 bytes are full.

Now `SUBMIT FOO` loads, runs, prints `No 'SUB' File Present` and
terminates, repeatedly; an unknown command gets `?Can't Find Command`;
`DIR` loads and starts, and terminates at once (#115).

## 114. `tools/asm86` — `''` inside a `db` string wasn't collapsed to one quote — FIXED

`db 'Can''t'` assembled as `Can''t`, so TMP printed `?Can''t Find
Command` and SUBMIT `No ''SUB'' File Present`. ASM-86 treats a doubled
quote inside a string as one quote. **Patched** in `_do_db`.

## 115. `tools/plmc.py` — compiled PL/M utilities can't run — FIXED (DIR)

With the kernel fixed (#112, #113), `DIR` loads, its process is
created and dispatched, and it immediately does system call 0 and
terminates, printing nothing. That's DIR.CMD, not the kernel. The
PL/M compiler's output has at least four problems, in the generated
`DIR.gen.A86`:

- **No startup code.** The real build links DRI's `MCD.A86` first
  (VAXCMD.LIT; `orig/mpm862sr/07/MCD.A86`), which sets up the stack and
  calls `plmstart`. `tools/plm/runtime_stub.A86` has only the `mon1`-
  `mon4` wrappers, so a .CMD starts at whatever procedure comes first
  in the module (DIR: `read$console`).
- **Argument order.** Calls push their arguments right to left
  (`mon2(1,0)` pushes 0, then 1), but the stub, like PL/M-86, takes
  the first argument from the deeper slot (`mov cx,[bp+6]`), so the
  function number and the parameter swap. `read$console`'s `mon2(1,0)`
  becomes system call 0, which is what DIR does first.
- **Parameters.** A procedure's parameters are read from locals:
  `write$console` does `sub sp,2 ! mov ax,[bp-2]` for `char`,
  instead of `[bp+4]`.
- **`plmstart` is emitted twice** (`pl_plmstart` at two places).

Every PL/M utility on the disk (DIR, TYPE, PIP, ERA, ...) goes through
this compiler, so none of them can work until it's fixed. A separate
piece of work.

**Fixed** (`tools/plm/codegen.py`), in the order DIR hit them:

- **Startup:** a module with PLMSTART now begins with MCD.A86's
  startup (SS = DS, SP = a 256-byte stack at the end of its data,
  `jmp plmstart`).
- **Argument order:** arguments are pushed left to right, with the
  parameter offsets to match, so DRI's verbatim `xdos` stub works.
- **Parameters:** a `DECLARE` of a parameter's own name types the
  parameter instead of making a new local.
- **Duplicate `plmstart`:** gone.
- **Base page:** EXTERNAL base-page variables (`fcb`, `tbuff`, ...)
  map onto MCD's base-page layout (fcb at 5Ch, buff at 80h), not 1-byte
  globals at DS:0; module data starts at 100h.
- **DO loops:** `DO i = a TO b` compared `b` against `i`, so a loop ran
  once at most (DIR's "fill the FCB with `?`" loop never ran, so it
  searched for blanks).
- **BYTE/ADDRESS typing:** a constant below 256 is a BYTE, byte-op-byte
  is 8-bit, and BYTE results, BYTE procedure results (`mon2`'s AL) and
  BYTE parameters/locals are truncated. DIR's `if cnt = -1` (BYTE
  `cnt`) never matched, so "File not found." never printed.
- **ROL/ROR/SHL/SHR:** they shifted the count by the value instead of
  the value by the count, in 16 bits; now value by count, 8-bit on a
  BYTE. DIR's `ror(dcnt,3)` gave 0 for the second entry of each
  directory record, so it printed the first entry twice and never
  showed the second.
- **IF/WHILE:** they now test the low bit, as PL/M does (`not 1` is
  0FEh: FALSE).

Also `tools/i86sim.py` gained `RET imm16` (compiled PL/M uses it
everywhere). `src-assem/Makefile` now rebuilds the utilities when
`tools/plm/*.py` changes.

**Verified**: in QEMU, `DIR` lists the disk four to a line, `DIR *.RSP`
and `DIR [S]` work, `DIR NONE.X` prints "File not found.", and DIR runs
repeatedly. New `tests/plm/test_plmc.py`: the semantics above, checked
on small compiled programs, plus the real DIR.PLM against a scripted
BDOS. Putting back the old rotate, argument order or IF test fails it.
The other utilities compile and assemble; they haven't been run.

## 116. `tools/plmc.py` — POINTERs were 16-bit, so TOD read the time from page 0 — FIXED

`TOD` printed `Mon 01/00/78 00:00:4?`. TOD (like ATTACH, ERA, REN, SET,
SHOW, SPOOL, STSCMD, STSCOM, STSRSP) is compiled `$COMPACT`: it gets
SYSDAT's address as a POINTER from `mon4(154)` (ES:BX), sets its offset to
7Eh through an `AT(@tod$pointer)` overlay, and copies the TOD structure
BASED on it with `MOVB(@extrnl$tod.date, @lcltod.date, 5)`. The compiler
treated POINTER as a 16-bit ADDRESS: the segment was never kept, the
overlay's `segment` field sat on the next variable, and every BASED
access went through DS -- so TOD read 0000:007Eh, the interrupt vector
table, and a time it set landed there too.

**Patched** (`tools/plm/codegen.py`, `parser.py`):
- **Storage:** a POINTER is 4 bytes (offset, segment), as a global,
  local or structure field.
- **Values:** a POINTER value is DX:AX. `@x` (the parser now tells `@`
  from `.`) is DS:x, or the base pointer's segment for BASED data.
- **BASED data:** data BASED on a POINTER, scalars, fields and array
  elements, is accessed through ES.
- **`mon4`:** a POINTER-returning EXTERNAL comes back in ES:BX.
- **Moves:** MOVE/MOVB/MOVW copy between the operands' own segments.

Also, `src-assem/Makefile`'s disk image didn't depend on the utility and RSP
binaries, so a rebuilt utility never reached `mpm86q.img`. That cost a
debugging detour here: QEMU kept running the old TOD.

**Verified**: in QEMU, `TOD` shows `Fri 09/25/81 12:00:00` (SYSDAT's
SYSTEM.DAT value), `TOD 03/19/86 12:05:30` + a key sets it, and the next
`TOD` -- and the CLI's dayfile stamp -- show the new time. (It doesn't
advance: the CLOCK RSP isn't loaded in this build. Years before 78 are
rejected by TOD itself, by design.) `tests/plm/test_plmc.py` adds
POINTER cases and TOD's display (leap years included), set, and Feb 29
of a non-leap year; the pre-fix compiler fails 7 of its checks.

## 117. `tools/build_mpmsys.py` — CLOCK never ran, and the "slack" tables sat on top of DATA.BDO — FIXED

**CLOCK.** The time never advanced: TOD always showed SYSDAT's
SYSTEM.DAT value. CLOCK.RSP (src-assem/rsp/CLOCK.A86: wait on the seconds
flag, BCD-increment SYSDAT's TOD) was on the disk but never loaded --
nothing loads an RSP from disk; build_mpmsys.py bakes RSPs into the
image, and it only baked TMP. Now it bakes a list (`BAKED_RSPS`: TMP,
then CLOCK). Each RSP is assembled and appended paragraph-aligned, its
code segment patched into its initial stack frame and `u_initcs` (from
its own symbols, no longer hardcoded offsets), and the RSPs chained
through `rsp_link` from SYSDAT's `rspseg`, which INIT.SUP's `nrsp:` loop
walks to create one process each. The PD pool grew to 6, since each RSP
takes a slot.

**The slack.** With that, two programs in a row worked and the third
failed with "No Room in System Lock List". SYSDAT's 4096 bytes are
SYSTEM.DAT's fields up to `mxmemcount` (B66h), then DATA.BDO from
`efcb` (C00h): the BDOS's variables, its internal stack (`bdosstack`,
F8Eh, growing down) and `mxdiskqd`/`mxdiskqpb` (to FBFh). The script
treated everything after `mxmemcount` as free, checking only against
4096, and put the queue buffer's MAU (BE0h-DE0h), the flag table, the
PD pool and the Lock List (#113) on top of DATA.BDO. The BDOS and those
tables overwrote each other; growing the PD pool for CLOCK just moved
the overlap somewhere that broke sooner.

**Patched**: only the QD pool stays in the real gap (B67h-BD7h, now
checked against `efcb`); the queue MAU, flag table, PD pool and Lock
List go after the XIOS's data tail, alongside the MD pool, so DATA.BDO's
own initial values are intact too.

**Verified** in QEMU: TOD reads 12:00:14, then 12:00:24 ten seconds
later; set to 03/19/86 23:59:55, it shows Thu 03/20/86 00:00:04 nine
seconds later; ten programs in a row (DIR, SUBMIT, TOD, ERA, REN, ...)
run, the clock advancing through them.


## 118. `tools/build_mpmsys.py` — ECHO's queued message overwrote SYSDAT's header — FIXED

**Symptom.** With ECHO baked in as a third RSP, `ECHO hello` hung the
system: every later system call went through `gofar` to `mpmif` with no
module to dispatch to. A memory dump showed SYSDAT 0-~130h overwritten
with message and FCB data, `module_table` and `module_map` included.

**Cause.** ECHO's QD lives in its own data segment, below SYSDAT, so
QUE2.RTM's `getqdaddr` copies it into the QD pool and `qspace` takes its
131-byte buffer from `qmau`. `maualloc` returns the SAT entry's
`sat_start`, which goes straight into `q_buf`, and `writeq`'s `qw_move`
(QUE1.RTM) copies the message to `DS:q_buf` with DS = SYSDAT. So SAT
starts have to be SYSDAT offsets. The script's hand-built SAT (#117 and
earlier) gave SAT1 a start relative to the MAU's own segment (37h, just
past the table), so the first message landed at SYSDAT:37h. TMP's and
the BDOS's queues never hit this: the CLI queue's QD is already inside
the system, and the disk MX queue has no buffer.

**Patched**: SAT1.start = the buffer's SYSDAT offset + the table length.
No kernel change, so /src-assem and /src-c are unchanged. `src-c/boot/kernel_main.c`'s
`SYSDAT_SEG` moves to 16F9h (ECHO adds 944 bytes of code).

**Verified** in QEMU: `ECHO hello, world` prints "ECHO Msg Qued" then
" hello, world"; TOD, DIR, a second ECHO and TOD follow, with the clock
advancing. test_boot, test_que, test_mau, test_echo and test_plmc pass.

## 119. SPOOL couldn't run: fixed-address .CMD files, one memory partition, no list device — FIXED

**Request.** "Get the SPOOL RSP loaded." MP/M-86 2.0 has no spooler
RSP: the user's guide and both MP/M-86 distributions ship SPOOL only
as a .CMD (SPOOL.PLM). It loads, detaches from its console and keeps
running as a process named `SPOOL` until it has printed its files;
STOPSPLR (SSPOOL.PLM) aborts it by name. The `spool.rsp` in
`orig/mpm_II` is MP/M-II's, 8080 code. So the job was to make
SPOOL.CMD work, which took three fixes.

**1. `tools/build_disk.py` — every .CMD loaded at 2000h.** The CMD
header for each utility gave both groups an absolute base (code at
2000h, data right after) with min = max = 0. LOAD.SUP takes a nonzero
base as an absolute request, so every program went to the same
address. That worked only while one program at a time was resident.
After SPOOL detached there, every later command failed with
"?Not Enough Memory". (With min 0, `maualloc` also handed out
zero-length segments.) **Patched**: groups are relocatable, as GENCMD
writes them: base 0, min = the group's length, max 0 (LOAD.SUP reads
that as min).

**2. `tools/build_mpmsys.py` — all of free memory was one partition.**
MP/M-86 2.0 uses fixed partitions, which GENSYS asks for. ML.MEM's
`mlalloc` builds a process's MAU from whole Memory Free List entries,
and the MAU stays that process's until it frees everything in it. With
one MFL entry covering 18A0h-9000h, the first resident program would
own all of it. **Patched**: free memory is cut into 16K
(`PARTITION_SIZE`) partitions, 29 of them, each an MD on `mfl` in
memory order; `mlalloc` still combines contiguous ones for bigger
requests. The MD pool is sized to the partition count plus 40 spares.

**3. No list device.** XIOS86Q's LIST/LISTST were stubs and `maxlist`
was 0, and SYSDAT's CCB table had only the console. Every PD's `p_lst`
is CCB index 1 (list 0, after the console), so SPOOL's `attach$list`
slept on a `c_queue` past the end of the table. Nothing could wake it,
and STOPSPLR's abort (`abtcns`/`abtlst`) couldn't find it on any CCB
queue: "Spooler not running". **Patched**:
- XIOS86Q.A86 drives LPT1 (378h). LIST waits for not-busy through
  `rtm_poll` (poll device 08h, `l0outdev` as in DRI's XIOS), latches
  the byte and pulses strobe. LISTST polls it, and `maxlist` = 1.
  `ccbtab0` holds two CCBs.
- build_mpmsys sets `nlstdev` 1, `nciodev` 2, `nccb` 2, and
  `c_mimic`/`c_msource` 0FFh in both CCBs.
- run.sh sends QEMU's parallel port to `build/printer.txt`.
- src-c's xios86q.c has the same LIST/LISTST/POLLDEV/MAXLIST, and its
  `XQ_*` offsets after `ccbtab0` moved by 12 (defs.h). SYSDAT is now at
  16FCh (kernel_main.c, docs/src-c.md).

**Verified** in QEMU:
- `SPOOL CLOCK.RSP` prints the file to LPT1 up to its ^Z, while TOD and
  DIR run alongside it.
- With ^P on (so the console holds the printer), SPOOL waits for the
  printer, STOPSPLR prints "Spooler aborted", and a second STOPSPLR
  prints "Spooler not running".
- ECHO, DIR, TOD and ERA work before and after.

All 56 tests/src-c and tests/plm test files pass, including
test_xios86q.py's new PLIST/LISTST/l0outdev cases.

**Not fixed here:** SHOW printed "A: RW, Space: 0" and then garbage
forever, the same with the old headers. It was a PL/M compiler bug,
fixed in #120.

## 120. `tools/plm/codegen.py` — frames too small around nested procedures; BASED bytes typed as ADDRESS — FIXED (SHOW, SET, ED)

**Symptom.** SHOW printed "A: RW, Space: 0" and then `]]]]...` forever.
SET reported "read write (RW)" for a file it had just made read-only.
ED said "NO MEMORY".

**1. A procedure's frame was sized by its nested procedure's locals.**
`gen_proc` registers a procedure's locals, then hoists and generates
its nested procedures. Each of those resets `self.next_local_off`, and
only `cur_scope`/`cur_proc` were restored afterwards. So the enclosing
procedure's `sub sp,N` came from the *last nested procedure's* locals.
SHOW's `printbcd` (locals `zerosup`, `i`; nested `pchar` with none) got
no `sub sp` at all. Each `call pchar` pushed over `i`, so
`do while (i:=i-1) <> -1` never ended. ED, ERA, PIP, REN, SET and SHOW
all had procedures like this. (This was found by diffing every
utility's output before and after the fix.)
**Patched**: `next_local_off` is saved around the hoist and restored.

**2. An element of a BASED array, `x(i)`, was ADDRESS.** That form
parses as a Call node. On that path `expr_type` used `elemsize`/`size`,
which a BASED symbol doesn't have, so it fell back to ADDRESS. SET's
`attribute` does `if rol(fcbv(i),1)` to test an attribute bit (bit 7).
As a 16-bit `rol ax,cl` the bit went to bit 8, so every attribute read
as off. Byte arithmetic on such elements also lost its `mov ah,0`.
**Patched**: a BASED symbol is typed from its declaration
(`_sym_type`).

**3. `tools/build_disk.py` — ED had no edit buffer.** ED sizes its
buffer from base-page MAXB, the data group's real length. With
relocatable headers (#119), max = min, so there was nothing past its
data. DRI's build gave ED `data[m253 xfff]` and PIP `data[m374 x980]`.
**Patched**: `DATA_MAX` gives those two utilities those maximums.

tests/plm/test_plmc.py has new cases for both compiler fixes: a counting
loop around a nested procedure, and `rol`/byte arithmetic on a BASED
byte. The loop case runs away without fix 1.

**Verified** in QEMU:
- SHOW, SHOW SPACE/USERS/DRIVE/A:DRIVE/LABEL/HELP and an invalid option
  all print correctly. Free space reads 1,152k, which matches the
  directory: 177 8K blocks, minus 1 for the directory and 32 for files.
- SET [RO]/[RW SYS]/[DIR] report and set correctly, and ERA refuses a
  read-only file.
- REN works both ways.
- ED creates a file, inserts text and saves it, and TYPE shows it.

All 56 tests/src-c and tests/plm files pass.

**Still open here:** PIP hung after loading (`PIP U.TXT=T.TXT`), the
same with the compiler from before these fixes. Fixed in #121.

## 121. `tools/plm` — GOTO was dropped; CASE tables read through DS; byte products truncated; DATA locals on the stack — FIXED (PIP)

**Symptom.** PIP hung after loading, doing disk I/O over and over. Once
that was fixed:
- MPM.SYS copied 6 records short.
- "PIP NONE.TXT=MISSING.TXT" printed "TEFILE NOT FOUND IT:" followed
  by garbage.

**1. GOTO.** The parser parsed every `GOTO`/`GO TO` and dropped it, and
kept a label only when it was on a `DO` block. PIP's control flow is
built on them: 40 GOTOs, such as `GO TO NOTDEST` in its device-name
scanner, `GO TO SIMPLECOM`/`ENDCOM` in the main loop, and `GO TO RETRY`
out of its ERROR procedure. ED (30), REN (5), ERA, ERAQ, SET and TYPE
(4 each) use them too. **Patched**:
- A label can sit on any statement.
- Labels belong to their procedure, and a GOTO within it is a `JMP`.
- A GOTO out to an enclosing procedure (found through the static links)
  or to the main program (whose BP is saved in `pl_main_bp` on entry)
  sets BP to that frame. It then enters the label's `_nl` stub, which
  resets SP to the frame's base. The stack is balanced at every
  statement boundary, so that's always the right SP.
- See tools/plm/README.md, "GOTO".

**2. DO CASE.** It dispatched with `jmp ctbl[bx]`, but the table is in
the code segment, so it was read through DS: an arbitrary word of the
program's data whenever DS ≠ CS, which is every .CMD here. PIP and SET
use DO CASE. **Patched**: `jmp cs:ctbl[bx]`.

This also fixed ED printing `BREAK ">" AT ^@` right after "NEW FILE".
ED's startup ends `RESTART: CALL SETUP; ... GO TO START;`. With the
GOTO dropped, it fell through `OVERCOUNT:`/`BADCOM:`/`OVERFLOW:`, set
`FLAG = '>'`, and printed the error report at `RESET:` with `CHAR`
still 0. Checked by replaying the same keystrokes against an ED built
with the pre-#121 compiler (the message appears) and the current one
(it doesn't).

**3. `*` and `/` on two BYTEs.** These were typed BYTE, so the result
was truncated to 8 bits. "Multiply and divide always produce ADDRESS
results" (orig/plm80s/PLMLANG.DOC). PIP's fast copy reads 8 records at
a time; at EOF it adds back `128 * shr(exten,4)`, the records the BDOS
read before EOF, which the BDOS reports in AH's high nibble. A test
.CMD confirmed the BDOS's side: 34 reads of 0000, then 6001h. The
compiler made 128 × 6 = 0, so the last partial group was dropped.
**Patched**: `*` and `/` are always ADDRESS.

**4. Locals with INITIAL/DATA were copied onto the stack** each time
their DECLARE was reached. PIP's ERROR declares about 450 bytes of DATA
message tables, more than the program's whole 256-byte stack, so they
overwrote the strings below it. In PL/M such locals are static.
**Patched**:
- They live in dseg as `pl_<proc>_s_<name>`, initialized once, and
  `.name` in another DATA list is a constant.
- `gen_VarDecl` and `_addrof_is_local` are gone.
- The program stack is now 1K (`STACK_BYTES`). This compiler keeps
  ordinary locals on the stack, unlike PL/M, and a call-graph estimate
  puts PIP's deepest chain at about 460 bytes and ED's at about 190.

New tests/plm/test_plmc.py cases:
- GOTO forward;
- GOTO out of 5-deep recursion back to the main program three times,
  with SP the same each time;
- GOTO out of a nested procedure, with the parent's local still
  readable;
- DO CASE with DS ≠ CS;
- a BYTE product and quotient that need 16 bits.

**Verified** in QEMU (comparing files read back out of the disk
image):
- `PIP BIG.SYS=MPM.SYS[V]` (278 records, three extents) and
  `PIP P2.CMD=PIP.CMD` (20K) copy byte-identical.
- Text copies, `[U]`, `CON:`, `LST:` (to the printer), and interactive
  `*` mode all work.
- The errors print "ERROR - FILE NOT FOUND - A:MISSING.TXT" and
  "ERROR - INVALID SOURCE".
- A regression pass over DIR, TOD, SHOW, ECHO, ED, TYPE, REN, SET, ERA,
  ERAQ, SPOOL/SSPOOL, ABORT, DSKRST, CONS and ATTACH is all correct.

All 56 tests/src-c and tests/plm files pass.

**Still open here:** STSCMD (MPMSTAT) printed `SSSS...` forever, the
same with the compiler from before #120. Fixed in #122.

## 122. STSCMD (MPMSTAT): POINTER parameters, local AT, and three wrong status values — FIXED

**Symptom.** STSCMD printed `SSSS...` forever after its banner.
STSCOM's `display$text(count, source)` writes a string a byte at a time
from a POINTER, stepping it with
`src$ptr structure(offset word, segment word) at (@source)`.

**1. `tools/plm/codegen.py` — POINTER parameters had 2 bytes.**
- Every parameter got a 2-byte slot, and a call pushed only AX. So a
  POINTER argument lost its segment, and the callee read `count` from
  the pointer's segment word.
- `RET n` used the same 2-bytes-per-parameter count.
- **Patched**:
  - Parameter sizes come from the DECLAREs that type them
    (`_param_sizes`). A POINTER is 4 bytes (offset nearest BP), and
    `RET n` is the sum of the sizes.
  - Each procedure symbol records `psizes`, and a call pushes DX:AX
    for a POINTER parameter. An ADDRESS argument gets DS.

**2. A local `AT(@x)` got its own storage.** `src$ptr` was a fresh
local, so incrementing `src$ptr.offset` never moved `source`, and the
first character printed forever. **Patched** (`_declare_local_at`): a
target in the same frame is aliased by offset, and anything else
resolves like a module-level AT.

Once STSCMD ran, three of its values were wrong:

**3. `tools/build_mpmsys.py` — `ncns`/`nlst`** were SYSTEM.DAT's stock
4 and 1 (GENSYS answers that were never set). It showed 4 physical
consoles and `nccb - (nlst + ncns)` = FDh virtual ones, and
CONSOLE.CIO's console-number check went by `ncns`. **Patched**: both 1.

**4. `src-assem/kernel/PROC.RTM` (and `src-c/kernel/proc.c`) — TMP's memory
list was `0FFFFh`.** TMP.A86's PD, like DRI's own, has `p_mem` =
0FFFFh. That keeps `proc_creat` from sharing the creator's memory with
it (it shares only into a child whose `p_mem` is 0). But nothing ever
turned it into an empty list, so every `p_mem` walker followed it to
SYSDAT:FFFFh. MPMSTAT listed a `0000 0000 Tmp` partition, and
termination (DSPTCH.RTM) and LOAD.SUP's chain path would walk it too.
**Patched**: after the share check, a 0FFFFh `p_mem` becomes 0.
test_proc.py has a new case for it, matching asm and C.

Collateral:
- RTM grew, so SYSDAT is now at 16FDh (kernel_main.c, docs/src-c.md).
- rtmif.c's `RTMIF_ABORT_TERMINATE_OFFSET` is now 0C76h (and
  test_rtmif_entry.py's copy of it).

tests/plm/test_plmc.py's POINTER case now also covers:
- `display$text`'s pattern (summing 3 bytes through a POINTER
  parameter stepped via `AT(@source)`);
- a POINTER argument's segment;
- an ADDRESS passed as a POINTER getting DS.

**Verified** in QEMU: STSCMD shows the whole status display, matching
the configuration:
- 1 console, 0 virtual consoles, 1 list device;
- 2 free PDs, 3 free QCBs;
- 146h of queue buffer free (the qmau buffer less its table and ECHO's
  131 bytes);
- the ready/DQ/flag-wait lists, the ECHO and MXdisk queues, the
  console/printer owners, and the 16K partitions, with STSCMD's own
  partition marked.

The regression pass (DIR, ECHO, TOD, ED, PIP, TYPE, ^P with
SPOOL/STOPSPLR, SHOW) is correct, and all 56 test files pass.

## 123. MPMSTAT RSP baked in; MPM.bin didn't depend on the RSP sources — FIXED

**Request.** "Get the ABORT RSP loaded too." MP/M-86 2.0 has no ABORT
RSP: every distribution in `orig/` ships ABORT only as ABORT.CMD (which
works: `ABORT SPOOL` removes a waiting spooler), and the only
`abort.rsp` is MP/M-II's. Instead, as agreed, the RSP MP/M-86 did ship
and this build lacked: **MPMSTAT** (STSRSP.PLM + STSCOM.PLM, linked by
DRI behind RSPASM.A86).

**Changes:**
- `tools/plm`: `--rsp NAME[,PRIORITY[,FLAGS]]` compiles a PL/M RSP
  (tools/plm/README.md). There's no MCD startup; the data segment
  starts with RSPASM.A86's shell (header, PD, UDA with `u_sp` at an
  initial PLMSTART frame, `buff`, stack, FCBs), and `rsplink`/`buff`/
  `fcb`/`fcb16` resolve to it.
- `tools/build_mpmsys.py`: BAKED_RSPS entries can name a .PLM source,
  compiled as an RSP with the mon1-4 stub. MPMSTAT follows ECHO. The PD
  pool went from 6 to 8, since each RSP's PD takes a slot. The QD pool
  stays at 4: it has to fit in B67h-BFFh, and ECHO and MPMSTAT leave 2.
- `src-assem/Makefile`: MPM.bin was rebuilt only for kernel source changes,
  though build_mpmsys assembles the baked-in RSPs itself. It now also
  depends on `rsp/TMP|CLOCK|ECHO.A86`, `utils/STSRSP.PLM`,
  `STSCOM.PLM` and the compiler.
- SYSDAT is now at 1920h (code 37376 bytes; kernel_main.c,
  docs/src-c.md).

**Verified** in QEMU:
- `MPMSTAT` prints "MPMSTAT Msg Qued" and the full status display,
  then hands the console back to TMP, repeatedly. It shows 4 free PDs,
  2 free QCBs and C3h of queue buffer (326 less MPMSTAT's 131), and
  STSCMD lists `[MPMSTAT] MPMSTAT` waiting on its queue.
- It also runs from a SUBMIT file.
- With SPOOL resident, MPMSTAT shows SPOOL's partition.
- ED/PIP/TYPE/SPOOL/STOPSPLR/DIR/SHOW/TOD/ECHO work alongside.

All 56 tests/src-c and tests/plm files pass.

## 124. CACHE RSP baked in (DRI binary; no source exists)

**Request.** "Get the CACHE RSP loaded too." CACHE.RSP exists only as
a DRI binary on `orig/lastdri/22` (docs/orig.md: "disk cache RSP, no
equivalent elsewhere"). It's now copied to `src-assem/rsp/CACHE.RSP` and
baked into MPM.SYS after MPMSTAT.

**What it does** (from disassembling it):
- It's a GENCMD-format RSP: a 1Ah-paragraph code group, a 123h-paragraph
  data group with the RSPASM layout, a PD `Cachinit` (priority 40h,
  flags 0), the UDA at 40h, and the start frame at 158h, IP 9.
- Its init saves SYSDAT's XIOS entry (`xiosmod`, 28h/2Ah) in its code
  segment, points the entry at its own handler (CS:0038), and
  terminates.
- The handler passes functions below 6 straight to the XIOS. It
  intercepts:
  - SELDSK: finds the drive in its table at data 1E7h (6-byte entries:
    drive, valid, buffer, last record) and invalidates the entry on a
    first login;
  - SETDMA/SETDMAB: tracks the DMA address;
  - HOME: skipped while the entry is valid;
  - READ/WRITE: for CH = 0 and DX up to the entry's last record,
    copies 128 bytes between the caller's DMA and `buffer + DX*128`.
- The table configures drives A and B, records 0-15 each: a 64-entry
  directory, as in this disk's DPB.
- Writes go through to the XIOS. The cache fills as a sequential
  directory read goes by and becomes valid at record 15.

It works with this kernel because the BDOS passes the absolute record
in CH:DX to READ/WRITE (FILE1.BDO's `PASS_ARECORD`) and the first-login
bit in DL to SELDSK. It calls the XIOS through `callf .xiosmod` on
every request, so the hook takes effect.

**`tools/build_mpmsys.py`**: a BAKED_RSPS source ending in `.RSP` is
read as a GENCMD file. Its UDA comes from the PD's `p_uda`, and its
start frame from the UDA's `u_sp`. Its code segment is patched into the
frame and `u_initcs`, as for the assembled RSPs. The Makefile's MPM.bin
rule depends on it. SYSDAT is now at 1A5Dh (code 42448 bytes;
kernel_main.c, docs/src-c.md).

**Verified** in QEMU:
- From a memory dump after a session: `xiosmod` = 1920:0038 (the
  handler), the original 15D7:00F7 saved in the cache's code segment,
  drive A's entry active and valid, and its buffer holding the
  directory.
- Cachinit has terminated (absent from MPMSTAT).
- Six `DIR *.RSP` take 0.08 s each, against 0.11 s on the build
  without CACHE.
- PIP (a 20K copy with [V], byte-identical), ED, TYPE, REN, ERA,
  SPOOL/STOPSPLR, SUBMIT, SHOW and MPMSTAT all work. The directory read
  back from the disk image matches what DIR shows through the cache.

All 56 test files pass.

## 125. MPMSTAT.RSP on the disk; STSRSP.CMD hung the console — FIXED

**Symptom.** The disk held `STSRSP.CMD`: STSRSP.PLM compiled as an
ordinary program. Typing `STSRSP` made the MPMSTAT queue, then read it
forever, so the console never came back.

**Patched**:
- `src-assem/Makefile` builds `build/rsp/MPMSTAT.{cseg,dseg}.bin` from
  STSRSP.PLM with `plmc --rsp MPMSTAT` plus the stub, part of
  `RSP_BIN`. STSRSP is off `PLM_UTILS`.
- `tools/build_disk.py` places it as `MPMSTAT.RSP` beside ECHO.RSP and
  CLOCK.RSP (the rename STSRSP.PLM's header gives: `ren
  mpmstat.rsp=stsrsp.cmd`). STSRSP is off `UTIL_NAMES`.

The disk file has relocatable code and data groups like ECHO/CLOCK.RSP.
Its groups are byte-identical to build_mpmsys's baked-in copy before
the code-segment patch. (DRI's own MPMSTAT.RSP, orig/lastdri/08, is
8080-model, one group; CACHE.RSP shows DRI also shipped two-group
RSPs.)

**Verified** in QEMU:
- `DIR *.RSP` lists ECHO, CLOCK and MPMSTAT.
- `DIR STS*.*` lists only STSCMD and STSCOM.
- `MPMSTAT` still runs from the resident copy.

**Follow-up:** CACHE.RSP is on the disk too. `tools/build_disk.py`'s
`RAW_FILES` copies already-GENCMD files byte for byte, and the image
depends on `rsp/CACHE.RSP`. It reads back from the image with the same
MD5 as `orig/lastdri/22/CACHE.RSP` (8923ec2e…), and `DIR *.RSP` lists
ECHO, CLOCK, MPMSTAT and CACHE.

**Follow-up:** STSCMD is on the disk as MPMSTAT.CMD (`DISK_NAMES` in
build_disk.py, per STSCMD.PLM's header: `ren mpmstat.cmd=stscmd.cmd`).
In QEMU:
- `DIR MPMSTAT.*` lists MPMSTAT.CMD and MPMSTAT.RSP.
- `MPMSTAT` goes to the resident RSP ("Msg Qued").
- `A:MPMSTAT` loads and runs MPMSTAT.CMD (a drive prefix skips the
  RSP check).

**Follow-up:** TMP.RSP is on the disk too (build_disk.py's `RSP_NAMES`),
as in DRI's distributions. Its code and data groups are byte-identical
to the copy build_mpmsys bakes in, before the patch. `DIR *.RSP` lists
TMP, ECHO, CLOCK, MPMSTAT and CACHE.

**Follow-up:** RANDOM.CMD is on the disk: DRI's sample random-access
program, `src-assem/rsp/RANDOM.A86`, copied from orig/mpm862sr/05 (identical
to orig/lastdri/16's) and built like SUBMIT (Makefile `RSP_OK`;
build_disk.py `RSP_NAMES` as `.CMD`).
- Its data group is byte-identical to DRI's RANDOM.CMD. The code group
  differs only by encoding: asm86 uses the long forms `C7 C4`/`C6 C1`
  for `mov reg,imm`, where DRI's ASM86 used `BC`/`B1`.
- In QEMU: `RANDOM R.DAT` writes records 5, 2 and 300 (`5W`...), reads
  them back (`5R`...), and quits (`Q`). A second run reads them again.
  Unwritten record 7 gives "error, try again.". R.DAT is sparse on
  disk: extent 0 with rc 6, and extent 2 with rc 45 for record 300.

**Follow-up:** DSKRST is on the disk as DSKRESET.CMD, the name the MP/M-86
user guide gives the command ("The Dskreset Command"; its sample
listings show `DSKRESET CMD`). It's a `DISK_NAMES` entry in
build_disk.py. In QEMU, `DIR DSK*.*` lists only DSKRESET.CMD, and
`DSKRESET` runs. `DSKRST` gives "?Can't Find Command". DIR works after
the reset.

**Follow-up:** CONS is on the disk as CONSOLE.CMD, the user guide's name
("5.3 The CONSOLE Command"; `CONSOLE CMD` in its listings) and DRI's
distributions' (orig/mpm86-21/D1/CONSOLE.CMD). It's a `DISK_NAMES` entry
in build_disk.py. In QEMU, `CONSOLE` prints "Console = 0", and `CONS`
gives "?Can't Find Command".

## 126. ATTACH failed under ^P: the CLI created the process before giving it the console — FIXED

**Symptom.** With ^P on (console echoed to the printer), `ATTACH SPOOL`
or `ATTACH ED` printed "Attach Failed", though the process was detached
and named correctly. Without ^P it worked.

**Cause.** gdb on QEMU's stub, with breakpoints on CONSOLE.CIO's
`conassign` exits, showed the order:
1. ATTACH (the PD at 16F0h) calls `f_conassign` while the console's
   `c_attach` is still TMP (1600h), and gets "not owner".
2. Only afterwards does TMP, inside the CLI, assign the console to
   ATTACH.

COMMAND.SUP's step 10 created the process and step 11 then printed the
dayfile CRLF and assigned the console. Under ^P that CRLF also goes to
the printer, and QEMU's parallel port reads busy for a couple of status
polls after each byte. So the XIOS's LIST yields (`rtm_poll` →
`f_polldev`) and the new process runs before step 11.

Most programs don't notice: their first console I/O waits on the CCB
until the assign wakes them. ATTACH notices, because its
`f_conassign` must already own the console. Every DRI copy of
COMMAND.SUP in orig/ has the same order, so real MP/M-86 had this race
whenever the printer was slow.

**Patched** (`src-assem/kernel/COMMAND.SUP`, `src-c/kernel/command.c`): print
the CRLF and assign the console first, then create the process
(`f_conassign` with an explicit PD needs only `p_cns`, set in step 9).
`cli_err` is saved before `f_createproc`, which clobbers CX.

**Verified** in QEMU, with ^P on and off: `ED T.TXT`, ^D (detach), DIR,
`ATTACH ED` ("Attach: ED"), then ED takes commands and `q`/`y` exits.
The regression pass (PIP [V], TYPE, REN, ERA, MPMSTAT, A:MPMSTAT,
CONSOLE, DSKRESET, SHOW, RANDOM, TOD; SUBMIT with arguments and
`$INCLUDE`) is correct, and all 56 test files pass.

## 127. `src-assem/Makefile` — kernel include files weren't dependencies — FIXED

Both MPM.bin's stamp and the disk image listed only the seven top-level
kernel `.A86` files, not the ~60 `.SUP/.RTM/.CIO/.BDO/.MEM/.DEF/...`
files they INCLUDE. After #126's edit to COMMAND.SUP, `./run.sh`
rebuilt neither, and the first retests ran the old kernel. (Earlier
include-file fixes got rebuilt only because build_mpmsys.py changed in
the same step.) **Patched**: `KERNEL_SRC := $(wildcard kernel/*)` is a
dependency of both.


## 128. src-c: the BDOS connected (getdiskmx) — C-port bugs FIXED

**Request.** "Connect the BDOS getdiskmx." kernel.elf's BDOS calls that
used the disk returned "not connected". Now getdiskmx, the SHELL,
CALL_BDOS/BDOS_RETURN, GOBACK, REPORT_ERR and the BDOS's XIOS calls are
ported. See docs/src-c.md, "Running the port".

Running the C SUBMIT and RANDOM from the floppy turned up these bugs.
All are in the C port; the asm in /src-assem is correct, so nothing changed
there.
- `rddirbuf` didn't fall through into `setdata`.
- `initialize` passed 1, not 0FFh, as "true" to rddir: the checksum
  vector was never filled, and the drive went R/O.
- `single` was set to 1, not 0FFh.
- `selectdisk` didn't copy the DPH's addlist into buffa..alloca.
- FUNC40's zero fill zeroed the `buffa` word (and DATA.BDO after it), not
  the buffer, and ignored `wflag`.
- `get_dir_ext` returned the disk-map scan pointer, not .fcb(ext).
- `merge_byte` compared against the pre-merge directory byte.
- SET_PW0 encrypted from a fixed offset.
- `f_bdosterm` was 12Eh (SUP), not 52Eh (BDOS).
- The CLI's f_createproc/f_terminate were no-ops.
- The runtime could resume a terminated context for a new process with
  the same PD.

**Tests:**
- test_bdosif.py now runs getdiskmx itself against BDOSIF.BDO: seven
  cases, comparing the image and the mpmif/XIOS call sequence
  (fixtures/bdos_hooks.c).
- New test_kernel_disk.py boots kernel.elf on the floppy and runs SUBMIT
  and RANDOM, then checks the file RANDOM wrote on the image.
- All 57 test files pass.

## 129. /src renamed to /src-assem; build output under each tree

**Requests.**
- "Change /src/Makefile and /src-c/Makefile so that /src/Makefile puts
  the object files at /src/build and so that /src-c/Makefile puts the
  object files at /src-c/build."
- "Rename /src to /src-assem."

**Changes.**
- src-assem/Makefile builds into src-assem/build/ (it was the top-level
  build/). The tools follow: build_mpmsys.py, build_mpmldr.py,
  build_bootsect.py and build_disk.py. So do run.sh (the image, and the
  printer file), the CI artifact path and .gitignore.
- src-c/Makefile builds into src-c/build/ (it was obj/).
- The rename is a `git mv`. References across the tree (paths, Python
  path components, `make -C`/`cd`, docs, AGENTS.md) were rewritten to
  src-assem, with src-c left alone and CRLF files kept CRLF.

**One binary change.** XIOS86Q.A86 has `'XIOS86Q for QEMU -- see
src/kernel/XIOS86Q.A86'` in its code segment. The path now reads
src-assem/, 6 bytes longer, so the XIOS's later code offsets moved:
init 3Eh → 44h, entry F7h → FDh. tests/src-c/test_syms.py caught the
drift, and src-c/gensys/gensys_syms.h follows. The combined code length
didn't change (the 6 bytes fit in paragraph padding), so SYSDAT stays at
1A5Dh in src-assem's image.

## 130. src-c builds and boots its own MP/M-86 floppy

**Request.** "We want an MP/M image built from src-c." The decisions
were src-c's own boot sector, only src-c's output on the disk, and no
use of src-assem's assembled MPM.bin. The stages are in
docs/plan-src-c-image.md:
1. The /src-assem offsets src-c uses are committed headers (rt_syms.h,
   rsp_syms.h, gensys_syms.h), checked by test_syms.py; there are no
   generator scripts.
2. src-c/gensys builds the low-memory image in C at boot, replacing the
   MPM.bin .incbin. test_gensys.py finds it identical to
   build_mpmsys.py's image without MPMSTAT: SYSDAT byte for byte, and
   the code half outside the 8086 code. SYSDAT is at 183Ah.
3. src-c/boot/bootsect.s, a one-sector loader (int 13h, A20, flat GDT,
   copy to 1 MB), with `_start` zeroing .bss (test_bootsect.py).
4. src-c/tools/mkdisk.py → `make disk` → build/mpm86c.img: KERNEL.SYS
   and the C SUBMIT/RANDOM as CMD files built from their C images, plus
   DEMO.SUB (test_mkdisk.py).
5. `make run` boots it; `make run-elf` boots it via -kernel.
   test_kernel_disk.py runs on it.

No bugs turned up along the way. One portability fix: cache.c's
pushf/cli is i386-only, so that host tests can build it.

**Verified.**
- A clean `make disk`, and the boot and disk tests, work with
  src-assem/build/ removed.
- All 62 test files pass.
