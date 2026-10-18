# Bugs Found in the Original MP/M-86 2.x Source

While validating the assembler built in `tools/asm86/` against the files
copied into `src/`, assembly failed on three of the four RSP sources with
genuine defects in the original 1981 Digital Research source itself —
not bugs in the assembler. These were confirmed by reading the
surrounding code, not just by the assembler's error message, and have
**not** been patched: `src/` remains an unmodified copy of `orig/`.

This is consistent with the provenance already noted in the top-level
`README.md` — these are hand-labeled "work disks," not a verified golden
release, and these RSPs likely never finished debugging on the original
hardware.

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

## 4. `src/kernel/BDOS.A86` — pervasive dropped/added underscores (42+ names)

Assembling `BDOS.A86` (after the tooling fixes in `tools/README.md` —
this is genuinely the source, not the assembler) stops on a long list of
undefined symbols, almost all of which are a declared name used with an
underscore added, removed, or moved — e.g. `GETDMA` used where `GET_DMA`
is declared, `STA_RET` used where `STARET`... no — concretely:
`CHECK_WRITE`/`checkwrite`, `SET_DATA`/`setdata`, `MULTIO`/`MULT_IO`,
`RETURN_SEG`/`returnseg`, `GETS1`/`GET_S1`, and so on for over 40 names
across `FILE1.BDO`–`FILE4.BDO` and `BDOSIF.BDO`. This is the same defect
class as `CLOCK.A86`/`TMP.A86`/`SUBMIT.A86` above, just far more
widespread — consistent with this disk's provenance note ("work disks,"
never finished debugging) applying to the BDOS module as a whole, not
just the RSPs. Three representative, individually-confirmed instances
(exactly one wrong spelling, cross-referenced against the real
declaration and, where possible, a correctly-spelled sibling use):

- `PDCNT` (`BDOSIF.BDO`, `FILE1.BDO`, `FILE3.BDO`) vs. the declared
  `PD_CNT` (`DATA.BDO`).
- `PFLAG`/`pflag` (`FILE1.BDO`, `DSPTCH.RTM`) vs. the declared `p_flag`
  (`PD.DEF`'s structure-offset chain).
- `clierr` (`COMMAND.SUP`, 8 call sites) vs. the declared `cli_err`
  (`SYSDAT.FMT`/`SYSTEM.DAT`) — `COMMAND.SUP` itself uses the correct
  `cli_err` spelling once (line 475), so both spellings coexist in the
  same file.

Not patched; `src/` remains an unmodified copy of `orig/`. Fixing
`tools/asm86` to tolerate `INCLUDE`'d fragments ending in their own
`END` (see `tools/README.md`) was necessary just to get *this far* into
the file — real tooling gaps and real source bugs were interleaved.

## 5. `src/kernel/ABORT.RTM` — four more of the same, inside one file

Within a single file: `snipit` used where `snip_it` is declared (same
file); `fpdq_nxt_q` used where the declared label is `fpdq_nxtq` (an
*added* underscore this time, not dropped); `find_pdq` used where the
declared label is `findpdq`; `findpd` used (once) where `find_pd` is
declared and used correctly everywhere else in the same file; and
`u_inint` used where `u_in_int` is declared (`UDA.FMT`). Not patched.

## 6. `src/kernel/COMMAND.SUP` — out-of-range short jump after fixing `clierr`

Past the `clierr`/`cli_err` bug above (see #4), `COMMAND.SUP:139` and
`src/kernel/SHARE.MEM:35` and `src/kernel/DSPTCH.RTM:222` all hit
`tools/asm86`'s own documented short-jump-range limitation (see "Design
choices that trade size for simplicity" in `tools/README.md`) rather
than a further source bug — the disp16-everywhere trade-off this
assembler makes can push an originally-in-range conditional jump/`JMPS`
out of 8-bit range in *this* assembler's encoding, even though DRI's own
assembler (with disp8 optimization) kept it in range. Noted here for
completeness, not fixed (fixing it means implementing real jump-size
relaxation, a substantially bigger undertaking, not a one-line bug fix).

## 7. `src/kernel/XIOS.A86` — one more underscore typo, plus one genuinely unresolved symbol

Line 611, `mov word ptr .088h,offset tick_int`, uses `tick_int` where the
label actually declared (line 349) is `tickint` (no underscore) — the
same bug class as everywhere else on this page. Not patched.

Separately, line 897's `add ax,offset dp_base` references `dp_base`,
which is **not** declared anywhere in `XIOS.A86` itself, nor anywhere
else in the whole 11-disk tree under that spelling. Unlike every other
finding on this page, no squashed-name match (`dpbase`, `dp_base`, etc.)
exists in this file to point to as "the real" declaration, so this is
left as a genuinely unresolved symbol rather than guessed at. Since
`docs/build.md` already excludes `XIOS.A86`/`ROM.A86` from the
QEMU-bootable target (they're written for the Intel iSBC 86/12 + SBC 204
specifically), this wasn't investigated further.

## 8. `src/loader/LDBIOS.A86` — four more dropped/added underscores, one file

`ccpoffset`/`ccp_offset`, `dp_base`/`dpbase`, `seg_table`/`segtable`, and
`z_ret`/`zret` — the same bug class, all four within this one file, each
used exactly once. Not patched. After accounting for all four, the only
remaining blocker is the short-jump-range tooling trade-off (see #6),
confirming the file has no further defects of this kind.

## 9. `src/loader/LDMPM.A86` — one genuinely unresolved symbol

Line 57, `abs_zero rw 2*bdosint`, references `bdosint`, which is only
ever declared in `DDT86.A86` (a completely different module, the
debugger) — not in `LDMPM.A86` or anything it has access to. No
same-file squashed-name candidate exists, so (as with XIOS.A86's
`dp_base` in #7) this is left unresolved rather than guessed at. Not
patched.

## 10. `src/loader/ROM.A86` — `fdc_stat` referenced but never declared here

Line 264, `in al,fdc_stat`, is identical to four call sites in
`src/kernel/XIOS.A86` and five in `src/loader/LDBIOS.A86` — but
`fdc_stat` is only ever `EQU`'d in those *other* two files, never in
`ROM.A86` itself, and `ROM.A86` has no `INCLUDE`s at all to pull it in
from them. Consistent with `docs/build.md`'s existing note that
`ROM.A86`/`XIOS.A86` target the Intel iSBC 86/12 + SBC 204 specifically
and are out of scope for the QEMU-bootable build — not investigated
further.

## Status of the kernel/loader sources

`src/kernel/CIO.A86`, `src/kernel/SYSDAT.A86`, and `src/loader/LDBDOS.A86`
compile and assemble cleanly end-to-end as-is. `src/kernel/BDOS.A86`,
`MEM.A86`, `RTM.A86`, `SUP.A86`, and `src/loader/LDBIOS.A86` are each
blocked only by a combination of #4–#6/#8 above — real, undisturbed 1981
source defects plus the one accepted tooling trade-off, not missing
includes or remaining assembler gaps. `src/kernel/XIOS.A86`,
`src/loader/LDMPM.A86`, and `src/loader/ROM.A86` are each additionally
blocked on at least one genuinely unresolved symbol (#7, #9, #10) with
no confident fix available from evidence in this tree.

The RSP sources (`CLOCK.A86`, `ECHO.A86`, `TMP.A86`, `SUBMIT.A86`) and
all 22 `.PLM` utilities in `src/utils/`/`src/toolchain/` compile and
assemble cleanly end-to-end (`ECHO.A86` and every `.PLM` file — see
`tools/plm/README.md`) except where documented above. See
`tools/README.md` and `tools/plm/README.md` for how to run the
assembler/compiler yourself, and the top-level `Makefile`/
`.github/workflows/build.yml` for the automated, continuously-validated
build of everything confirmed to work.
