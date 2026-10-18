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

## Not yet checked

Only the four RSP sources (`CLOCK.A86`, `ECHO.A86`, `TMP.A86`,
`SUBMIT.A86`) have been run through the assembler so far; `ECHO.A86`
assembled cleanly. The kernel, loader, and utility sources in `src/`
have not yet been assembled and may contain further defects of this
kind — see `tools/README.md` for how to run the assembler yourself.
