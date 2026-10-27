/*
 * ldmpm.c -- C port of the whole of src-assem/loader/LDMPM.A86's `start:`
 * (ported here as `start_entry`), plus its internal subroutines
 * `phex`/`pnib`/`putchar` (`initlbdos`/`openfnc`/`setdma`/`setdmab`/
 * `read`/`msg`/`pcrlf` are each just a one- or two-line trampoline
 * into `sys_vec`, i.e. a particular `(cl, dx)` pair -- see below).
 *
 * `start:`'s own very first instruction is `jmp mldbios` -- a near
 * jump into src-assem/loader/LDBIOS86Q.A86's own `INIT` (concatenated
 * right after this module in the real combined MPMLDR.bin), whose
 * own real tail (`jmp ccp_offset`) jumps straight back to right
 * after this `jmp`. This is the first genuinely wired-together call
 * in this port: `start_entry` calls `init_entry` (src-c/loader/
 * ldbios86q.c) directly, an ordinary extern C call, since that's
 * exactly what really happens here once both modules are linked
 * into one image -- same spirit as `src-c/kernel/memory.c`'s own
 * real internal calls into `mau.c`/`ml.c`. tools/i86sim.py's own new
 * `jmp_hooks` mechanism models the identical thing on the reference
 * side (see that module's own docstring): the hook runs in place of
 * `mldbios`'s own real body and execution resumes right after the
 * `jmp`, same as a CALL hook resuming after the CALL.
 *
 * Every OTHER real effect in this file -- console output, opening/
 * reading MPM.SYS -- goes through `sys_vec` (`int bdos_int`), a
 * synthetic software-interrupt vector THIS routine itself sets up
 * (the real asm's own `mov bdos_offset,...`/`mov bdos_segment,...`
 * writes, not ported here: they only matter to a REAL interrupt
 * dispatch this harness doesn't have, and nothing in this file ever
 * reads them back). This is NOT the real MP/M-86 kernel's own `int
 * mpmint` (same numeric value, 224, but a completely different,
 * loader-local convention: a tiny synthetic jump table into
 * LDBDOS's/LDBIOS86Q's own functions, set up fresh by this exact
 * routine) -- modeled as two boundary-call parameters instead of
 * wiring in the real already-ported src-c/loader/lbdos*.c/
 * ldbios86q.c functions one call at a time, the same "boundary has
 * to be modeled, but not yet woven into one real call graph" scope
 * every other not-yet-linked file in this port uses (see docs/
 * src-c.md's own intro on the link/boot step being separate,
 * not-yet-started work):
 *   - `bdos_fn(cl, dx)` for every non-string-returning call
 *     (`initlbdos`/`seldsk`, `openfnc`/`openf`, `setdma`/`dmaf`,
 *     `setdmab`/`dmabf`, `putchar`/`coutf` -- `dx`'s low byte IS the
 *     character for `coutf`, matching the real asm's own `mov
 *     dl,al` before falling into `putchar`), returning `ax`.
 *   - `msg_fn(const char *)` for `pstrf` (print a `$`-terminated
 *     string) -- the literal message content is passed directly
 *     (same convention as src-c/loader/rom86q.c's own `pmsg`),
 *     since there's no `mem` array backing a real DX-pointed string
 *     here (this file needs no `mem` parameter at all -- nothing in
 *     it reads memory other than through these very hooks).
 *   - `read_fn(uint8_t *record_out_128)` for `readsf` (read one
 *     128-byte CP/M logical record of MPM.SYS) -- the real file
 *     content THIS routine's own header-validation logic genuinely
 *     needs, so (same reasoning as `src-c/loader/rom86q.c`'s own
 *     `int13_fn`) it comes back through an output parameter rather
 *     than a `mem`+address pair. Returns `al` (0 = ok, 1 = EOF,
 *     anything else = read error).
 * `openfnc`'s own FCB (`offset fcb`, a fixed, compile-time-known
 * structure this port never builds) and `read`'s own FCB parameter
 * are both just placeholders (`0`) under this scope -- same
 * "undefined artifact, fixed choice" precedent as `src-c/kernel/
 * file1.c`'s own `selectdisk`.
 *
 * `clen`/`dlen` (the CMD header's own Len fields) are PARAGRAPH
 * counts in this file's own context, not bytes (confirmed by the
 * header-consistency arithmetic itself: `cldseg+clen==dldseg` only
 * makes dimensional sense if `clen` is paragraphs, since `cldseg`/
 * `dldseg` are themselves real-mode segment values) -- a different
 * convention from `src-assem/loader/ROM86Q.A86`'s own unrelated MPMLDR.bin
 * disk-layout header, whose own `Len` field genuinely is bytes; two
 * separate header formats, not a bug.
 *
 * The one-time "break to DDT" check (`mov al,cs:.fcb1 ! cmp
 * al,'B'`) reads a fixed CS-relative byte this port has no `mem` to
 * back -- fixed at a non-`'B'` value (the always-taken, non-debug
 * path), same "undefined artifact, fixed choice" precedent as
 * `openfnc`'s own FCB above. `mpmstart` (the far-jump target's own
 * IP half) is never written anywhere in this file, always its own
 * static-data initial value (0) -- not modeled as a real output,
 * same reasoning.
 *
 * The real stack switch, the `mov ds,dldseg` MP/M-data-segment
 * switch, and the final `jmpf cs:mpm`/`int ddt_int`/`jmp stop2` (the
 * loader's own real exit, one way or another) have no C equivalent,
 * same as every other real OS entry point in this port --
 * `start_entry` returns 0 and the resolved `mpmcseg` (code segment)
 * through an output parameter on success in place of that jump, or
 * 1 (every real failure path here converges on the same `jmp stop`
 * hard halt) on failure.
 */
#include <stdint.h>

#define coutf  2
#define pstrf  9
#define seldsk 14
#define openf  15
#define readsf 20
#define dmaf   26
#define dmabf  51

#define codetype 1
#define datatype 2

/* this module's own real, deterministically-resolved addresses
 * (confirmed via --symbols -- this file assembles standalone, no
 * `INCLUDE`s at all) -- `fcb`'s own content and `sec1`'s own first
 * use (as `openfnc`'s FCB parameter and `read`'s own DMA
 * destination for the CMD header) are otherwise-unmodeled
 * placeholders, same "undefined artifact, fixed choice" precedent
 * as this file's own header comment already uses for the DDT break
 * check -- what matters here is only that both sides of the
 * differential test agree on the SAME dx value for these calls. */
#define FCB_ADDR  0x0386 /* re-probed after tools/asm86/isa.py's own
    `_encode_alu` accumulator-immediate fix shifted it 8 bytes down --
    see that function's own header comment */
#define SEC1_ADDR 0x0080

typedef uint8_t (*io_in8_fn)(uint16_t port);
typedef void (*io_out8_fn)(uint16_t port, uint8_t value);
extern void init_entry(io_in8_fn io_in8, io_out8_fn io_out8);  /* src-c/loader/ldbios86q.c */

typedef uint16_t (*bdos_fn_t)(uint8_t cl, uint16_t dx);
typedef void (*msg_fn_t)(const char *s);
typedef uint8_t (*read_fn_t)(uint8_t *record_out_128);

static void putchar_(uint8_t ch, bdos_fn_t bdos) {
    (void)bdos(coutf, ch);
}

/* pnib -- print the low nibble of `al` as a hex character. */
static void pnib(uint8_t al, bdos_fn_t bdos) {
    al = (uint8_t)(al & 0x0F);
    uint8_t ch = (al > 9) ? (uint8_t)(al + ('A' - 10)) : (uint8_t)(al + '0');
    putchar_(ch, bdos);
}

/* phex -- print 4 hex characters from ax. */
static void phex(uint16_t ax, bdos_fn_t bdos) {
    for (int i = 0; i < 4; i++) {
        ax = (uint16_t)((ax << 4) | (ax >> 12));  /* rol ax,4 */
        pnib((uint8_t)ax, bdos);
    }
}

/* start_entry -- start: see this file's own header comment.
 *	input:	cs = this module's own real code segment (ambient,
 *		see src-c/kernel/init.c's own header comment for the
 *		same class of value)
 *	output:	returns 0 and sets *mpmcseg_out on success (ready for
 *		the real jmpf), 1 (boot failed) otherwise
 */
int start_entry(uint16_t cs, io_in8_fn io_in8, io_out8_fn io_out8,
                 bdos_fn_t bdos, msg_fn_t msg, read_fn_t readf,
                 uint16_t *mpmcseg_out) {
    init_entry(io_in8, io_out8);  /* jmp mldbios */

    msg("\r\n\nMP/M-86 V2.0 Loader\r\n");

    (void)bdos(seldsk, 0);                   /* initlbdos */
    uint16_t ax = bdos(openf, FCB_ADDR);      /* openfnc */
    if ((uint8_t)ax == 255) {
        msg("\r\nThe File MPM.SYS Not Found On This Disk");
        return 1;
    }

    /* perr: */
    (void)bdos(dmabf, cs);
    (void)bdos(dmaf, SEC1_ADDR);
    uint8_t sec1[128] = {0};
    (void)readf(sec1);  /* real asm never checks al for this one read */

    uint8_t ctype = sec1[0];
    uint16_t clen = (uint16_t)(sec1[1] | (sec1[2] << 8));
    uint16_t cldseg = (uint16_t)(sec1[3] | (sec1[4] << 8));
    uint8_t dtype = sec1[9];
    uint16_t dlen = (uint16_t)(sec1[10] | (sec1[11] << 8));
    uint16_t dldseg = (uint16_t)(sec1[12] | (sec1[13] << 8));

    int badhdr = 0;
    if (ctype != codetype || dtype != datatype) {
        badhdr = 1;
    } else {
        uint16_t sum1 = (uint16_t)(cldseg + clen);
        if (sum1 != dldseg) {
            badhdr = 1;
        } else {
            uint16_t sum2 = (uint16_t)(sum1 + dlen);
            if (!(sum2 > cldseg)) {  /* ja hdrok (unsigned) */
                badhdr = 1;
            }
        }
    }
    if (badhdr) {
        msg("\r\nBad Header Record in MPM.SYS");
        return 1;
    }

    msg("\r\nCode Paragraph Address = ");
    phex(cldseg, bdos);
    msg("\r\nData Paragraph Address = ");
    phex(dldseg, bdos);

    uint16_t dmab = cldseg;
    uint16_t dx = 0;
    uint8_t al;
    for (;;) {
        (void)bdos(dmabf, dmab);
        dx = 0;
        for (;;) {
            (void)bdos(dmaf, dx);
            uint8_t rec[128];
            al = readf(rec);
            if (al == 1) {
                goto done;
            }
            if (al != 0) {
                msg("\r\nError In Reading MPM.SYS");
                return 1;
            }
            dx = (uint16_t)(dx + 0x80);
            if (dx != 0) {
                continue;  /* readit2 */
            }
            dmab = (uint16_t)(dmab + 0x1000);
            break;  /* readit1 */
        }
    }
done:
    {
        uint16_t paragraphs_read = (uint16_t)(dx >> 4);
        uint16_t total_paragraphs = (uint16_t)((uint16_t)(paragraphs_read + dmab) - cldseg);
        uint16_t needed = (uint16_t)(clen + dlen);
        if (!(total_paragraphs >= needed)) {  /* jae lenok */
            msg("\r\nMPM.SYS Too Short ");
            return 1;
        }
        msg("\r\nLast Paragragh         = ");
        phex((uint16_t)(needed + cldseg - 1), bdos);
        msg("\r\n");
    }

    /* mov ds,dldseg -- no C equivalent; DDT 'B' break check -- fixed
     * non-'B', see this file's own header comment */
    *mpmcseg_out = cldseg;
    return 0;
}
