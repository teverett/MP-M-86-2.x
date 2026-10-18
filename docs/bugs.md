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
those files. #4, #5, #7 (partially), and #8 (the kernel/loader name
typos) **have been patched** directly in `src/kernel/` and
`src/loader/` — once a fix is this confidently evidence-backed (an
exact squashed-name match to a real declaration, used incorrectly
nowhere else), leaving it broken forever didn't serve the "get this
booting" goal, and each patched spot is called out explicitly below so
the change is traceable back to its evidence. #6, #7 (the `dp_base`
part), #9, and #10 remain unpatched: either no confident fix exists
(an undeclared symbol with no same-file candidate), or fixing it isn't
a source-level change at all (the short-jump-range tooling trade-off).

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

The RSP sources (`CLOCK.A86`, `ECHO.A86`, `TMP.A86`, `SUBMIT.A86`) and
all 22 `.PLM` utilities in `src/utils/`/`src/toolchain/` compile and
assemble cleanly end-to-end (`ECHO.A86` and every `.PLM` file — see
`tools/plm/README.md`) except where documented above. See
`tools/README.md` and `tools/plm/README.md` for how to run the
assembler/compiler yourself, and the top-level `Makefile`/
`.github/workflows/build.yml` for the automated, continuously-validated
build of everything confirmed to work.
