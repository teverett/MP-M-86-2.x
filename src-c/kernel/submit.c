/*
 * submit.c -- C port of a self-contained subset of src/rsp/SUBMIT.A86:
 * `deblank` (not a real label -- the unlabeled code right after
 * `submit:`'s own `call openfile`), `fill_argtable` (likewise), the
 * `$`-argument-substitution logic (`nextchar`/`argmaybe`/`realarg`/
 * `onedigit`/`copyarg`/`oknum`/`copymore`/`copyit`/`donecopy`),
 * `readbuffer`/`readchar`, and `topofcmdloop_once` (one full pass
 * through `topofcmdloop:` itself -- see its own header comment).
 *
 * Not ported: `submit:`'s own version-check/priority/FCB-open
 * preamble (no observable effect under the boundary-stub convention
 * beyond what `deblank`/`fill_argtable` already cover) and
 * `clicall`'s own real CLI dispatch -- `notdrive`'s own `call
 * parsefilename` (this file's own copy, same shape as `TMP.A86`'s)
 * deterministically returns `cx = mpm_parse` (152, pre-loaded right
 * before the tail `jmp mpm`), which never satisfies `jcxz goodparse`
 * and never matches any of `clierror`'s real error codes either -- so
 * `goodparse` and everything downstream (the built-in `USER`/
 * `PRINTER`/`$INCLUDE` checks, `notbuiltin`'s own real CLI dispatch)
 * are provably unreachable, same class as `TMP.A86`'s own `goodparse`
 * writeup; `topofcmdloop_once` returns `SUBMIT_SUBMITEXIT` once it
 * reaches that wall, same as the real asm's own `jmp submitexit`.
 * Needed `docs/bugs.md` #3's all 3 fixes in `/src` first (same
 * RSP-unpatched-policy question already asked and answered for
 * `CLOCK.A86`/`TMP.A86`) just to get this far enough to assemble as a
 * differential-test reference at all.
 *
 * `cmdtail`/`deblankbuf`/`argtable`/`numargs`/`cmd`(+its own
 * `cmdbuf`/`blen` header bytes)/`clicmd`/`dma`/`buf_ptr`/`pdadr`/
 * `subfilename`/`fcb`/`cmdsent`/`defdisk`/`defuser`/`promptutens`/
 * `promptuones`/`promptdisk` are this file's own real DSEG fields,
 * moved to fixed harness-chosen scratch addresses (`SD_SUBMIT_*`, see
 * defs.h) for the same reason `TMP.A86`'s fields needed it: this
 * file's own real DSEG offsets (e.g. `cmdtail` at `0x80`) land inside
 * its own 0x665-byte cseg once collapsed into the flat memory model.
 */
#include <stdint.h>

#include "defs.h"

#define SUBMIT_MAXCMDLEN 128

/* topofcmdloop_once's own 2 real exit destinations */
#define SUBMIT_GETNXTCMD  0 /* jmp getnxtcmd -- caller should call readbuffer next */
#define SUBMIT_SUBMITEXIT 1 /* jmp submitexit -- the program terminates */

/*
 * deblank -- collapse every run of spaces/tabs in SD_SUBMIT_CMDTAIL's
 * own text (offset 1 onward -- offset 0 is its length byte, unused
 * here) down to a single space, trim leading whitespace, and always
 * end the result with " \0".
 */
void deblank(uint8_t *mem) {
    uint16_t si = (uint16_t)(SD_SUBMIT_CMDTAIL + 1);
    uint16_t di = SD_SUBMIT_DEBLANKBUF;

    while (mem[si] == ' ' || mem[si] == 9) {
        si++;
    }
    for (;;) {
        uint8_t al = mem[si++];
        if (al == ' ' || al == 9) {
            while (mem[si] == ' ' || mem[si] == 9) {
                si++;
            }
            mem[di++] = ' ';
            continue; /* deblank: re-read mem[si] fresh */
        }
        if (al == 0) {
            mem[di++] = ' ';
            mem[di] = 0;
            return; /* dedone */
        }
        mem[di++] = al;
    }
}

/*
 * fill_argtable -- scan SD_SUBMIT_DEBLANKBUF's own space-separated
 * words (deblank's own output guarantees exactly one space between
 * each), recording each one's start offset into SD_SUBMIT_ARGTABLE
 * (as a single byte -- the real asm's own `mov [bx+di],si` is a
 * 16-bit store, but `si` never exceeds 255 here and `di` only
 * advances by 1 per entry, so the high byte it also writes is always
 * immediately clobbered by the next entry's own low byte, bar the
 * unused slot right after the last one -- net effect is a byte array,
 * confirmed against the real `mov bl,argtable[bx]` 1-byte read later).
 */
void fill_argtable(uint8_t *mem) {
    uint16_t bp = SD_SUBMIT_DEBLANKBUF;
    uint16_t si = 0;
    uint16_t di = 0;

    for (;;) {
        if (mem[(uint16_t)(bp + si)] == 0) {
            break; /* filldone */
        }
        mem[(uint16_t)(SD_SUBMIT_ARGTABLE + di)] = (uint8_t)si;
        di++;
        /* skiparg */
        si++;
        while (mem[(uint16_t)(bp + si)] != ' ') {
            si++;
        }
        si++;
    }
    mem_w16(mem, SD_SUBMIT_NUMARGS, di);
}

/*
 * copyit -- write al to SD_SUBMIT_CLICMD[*di], advance *di.
 *	output: returns 1 (and writes the '$' terminator) if the
 *		output buffer is now full or al was the NUL terminator
 *		-- the real asm's own `donecopy` hijacks control
 *		straight to `echocommand` at this point (discarding
 *		`copyit`'s own return address, "return is garbage"),
 *		modeled here as every caller checking this return value
 *		and stopping immediately instead.
 */
static int copyit(uint8_t *mem, uint16_t *di, uint8_t al) {
    mem[(uint16_t)(SD_SUBMIT_CLICMD + *di)] = al;
    (*di)++;
    if (*di >= SUBMIT_MAXCMDLEN || al == 0) {
        mem[(uint16_t)(SD_SUBMIT_CLICMD + *di)] = '$';
        return 1;
    }
    return 0;
}

/*
 * expand_command -- copy SD_SUBMIT_CMD into SD_SUBMIT_CLICMD,
 * substituting each `$0`-`$9` (or `$` followed by 2 digits, for
 * arguments past 9) with the matching argument's own text from
 * SD_SUBMIT_DEBLANKBUF (via SD_SUBMIT_ARGTABLE/SD_SUBMIT_NUMARGS) --
 * `echocommand`'s own tail (printing the result, skipping a leading
 * literal `$`) is a boundary stub with no memory effect, not modeled.
 */
void expand_command(uint8_t *mem) {
    uint16_t si = SD_SUBMIT_CMD;
    uint16_t di = 0;

    for (;;) {
        uint8_t al = mem[si++];
        if (al != '$') {
            if (copyit(mem, &di, al)) {
                return;
            }
            continue; /* nextchar */
        }
        /* argmaybe */
        uint8_t c = mem[si];
        if (c < '0' || c > '9') {
            /* noarg: not followed by a digit, copy the literal '$' */
            if (copyit(mem, &di, al)) {
                return;
            }
            continue;
        }
        /* realarg */
        uint8_t argnum = (uint8_t)(mem[si] - '0');
        si++;
        c = mem[si];
        if (c >= '0' && c <= '9') {
            argnum = (uint8_t)(argnum * 10);
            argnum = (uint8_t)(argnum + (mem[si] - '0'));
            si++;
        }
        /* copyarg */
        if (argnum >= mem_r16(mem, SD_SUBMIT_NUMARGS)) {
            continue; /* no such argument -- nextchar */
        }
        /* oknum */
        uint16_t pos = (uint16_t)(SD_SUBMIT_DEBLANKBUF + mem[(uint16_t)(SD_SUBMIT_ARGTABLE + argnum)]);
        for (;;) {
            uint8_t ac = mem[pos];
            if (ac == ' ') {
                break; /* nextchar: end of this argument */
            }
            if (copyit(mem, &di, ac)) {
                return;
            }
            pos++;
        }
    }
}

/*
 * readchar -- read one character into mem[si], refilling the SUB
 * file's own DMA buffer (a boundary-stub open+read) once SD_SUBMIT_
 * BUF_PTR reaches 128.
 *	input:	openfile_al/readfile_al -- the boundary-stub results
 *		for the refill path (`call openfile`/`call readfile`),
 *		only consulted when a refill actually happens, same
 *		"undefined artifact exposed as a parameter" treatment
 *		as the rest of this project's boundary-stub results that
 *		drive real branching (e.g. `chario.c`'s own `xiosif`)
 *	output:	returns 0 (character written to mem[si]) or 0xFFFF
 *		(error or EOF, same as the real `bx`)
 */
uint16_t readchar(uint8_t *mem, uint16_t si, uint8_t openfile_al, uint8_t readfile_al) {
    uint8_t bl = mem[SD_SUBMIT_BUF_PTR];
    uint16_t bx;
    if (bl >= 128) {
        /* mpm_usercode(command_user), openfile, readfile,
         * mpm_usercode(defuser) -- all boundary stubs except
         * openfile/readfile's own branch-driving al */
        if (openfile_al == 0xFF) {
            return 0xFFFF; /* readerr */
        }
        if (readfile_al != 0) {
            return 0xFFFF; /* readerr */
        }
        bx = 0;
    } else {
        bx = bl;
    }
    uint8_t al = mem[(uint16_t)(SD_SUBMIT_DMA + bx)];
    if (al == 0x1A) {
        return 0xFFFF; /* eof */
    }
    mem[si] = al;
    mem[SD_SUBMIT_BUF_PTR] = (uint8_t)(bx + 1);
    return 0;
}

/*
 * readbuffer -- read one line from the SUB file into the buffer at
 * dx (format: max-length byte, count byte, data...), skipping blank
 * lines.
 *	output:	returns 0 (line read into dx) or 0xFFFF (EOF/error)
 */
uint16_t readbuffer(uint8_t *mem, uint16_t dx, uint8_t openfile_al, uint8_t readfile_al) {
    uint16_t di = dx;
    mem[(uint16_t)(di + 1)] = 0;
    if (mem[di] == 0) {
        return 0xFFFF; /* zero-length buffer */
    }
    uint16_t si = (uint16_t)(di + 2);
    for (;;) {
        for (;;) {
            if (mem[(uint16_t)(di + 1)] >= mem[di]) {
                break; /* done_ok: buffer full */
            }
            uint16_t err = readchar(mem, si, openfile_al, readfile_al);
            if (err != 0) {
                return err;
            }
            if (mem[si] == 0x0A || mem[si] == 0x0D) {
                break; /* done_ok: LF or CR */
            }
            mem[(uint16_t)(di + 1)]++;
            si++;
        }
        if (mem[(uint16_t)(di + 1)] == 0) {
            continue; /* read_another: blank line, keep going */
        }
        mem[si] = 0;
        return 0;
    }
}

/*
 * topofcmdloop_once -- one pass through the SUBMIT shell's own main
 * loop, from `topofcmdloop:` through either of its own 2 real exits:
 *	- SUBMIT_GETNXTCMD ("jmp getnxtcmd"): the caller should now call
 *	  readbuffer() to read the next line, same as the real asm's own
 *	  `getnxtcmd:` (not folded into this function, since it's
 *	  already its own separately-ported/tested routine)
 *	- SUBMIT_SUBMITEXIT ("jmp submitexit"): the whole program ends
 *
 *	input:	charin_al -- the boundary-stub result of `call charin`
 *		(only consulted when a control-C abort is pending);
 *		defdisk_al/defuser_al -- the boundary-stub results of
 *		`mpm_getdefdisk`/`mpm_usercode`, undefined/caller-
 *		dependent artifacts under the convention (no sentinel
 *		is pre-loaded for either), same treatment as every other
 *		boundary-stub result that drives real branching or gets
 *		stored (e.g. `TMP.A86`'s own `getuser`/`getdisk`, except
 *		there they only fed print calls with nothing to store)
 *
 * `notdrive`'s own `call parsefilename` hits the exact same provably-
 * unreachable wall `TMP.A86`'s own `goodparse` did (see this file's
 * own header comment and that file's): `cx` deterministically comes
 * back `= mpm_parse`, so `goodparse` and everything downstream (the
 * built-in command checks, the real CLI dispatch) never run --
 * `notdrive`'s own reachable tail always ends in SUBMIT_SUBMITEXIT.
 */
int topofcmdloop_once(uint8_t *mem, uint8_t charin_al, uint8_t defdisk_al, uint8_t defuser_al) {
    uint16_t bx = mem_r16(mem, SD_SUBMIT_PDADR);
    uint16_t flags = mem_r16(mem, (uint16_t)(bx + p_flag));
    if (flags & pf_childabort) {
        mem_w16(mem, (uint16_t)(bx + p_flag), (uint16_t)(flags & ~pf_childabort));
        for (int i = 0; i < 11; i++) {
            mem[SD_SUBMIT_SUBFILENAME + i] = mem[(uint16_t)(SD_SUBMIT_FCB + 1 + i)];
        }
        /* mov dx,offset askabortmsg ! call con_write -- boundary stub, no-op */
        /* call charin -- boundary stub, result supplied by caller */
        uint8_t al = (uint8_t)(charin_al & 0x5F);
        if (al == 'Y') {
            flags = mem_r16(mem, (uint16_t)(bx + p_flag));
            mem_w16(mem, (uint16_t)(bx + p_flag), (uint16_t)(flags | pf_ctlc));
            return SUBMIT_SUBMITEXIT;
        }
        /* else: gonext */
    }

    /* gonext */
    if (mem[SD_SUBMIT_CMDSENT] != 0) {
        mem[SD_SUBMIT_CMDSENT] = 0;
        /* call crlf -- boundary stub, no-op */
    }

    /* mov cl,mpm_getdefdisk ! call mpm -- boundary stub, result supplied */
    mem[SD_SUBMIT_DEFDISK] = defdisk_al;
    /* mov cl,mpm_usercode ! mov dl,0ffh ! call mpm -- boundary stub, result supplied */
    mem[SD_SUBMIT_DEFUSER] = defuser_al;

    mem[SD_SUBMIT_PROMPTUTENS] = 13;
    mem[SD_SUBMIT_PROMPTUONES] = '0';
    uint8_t al = defuser_al;
    if (al >= 10) {
        mem[SD_SUBMIT_PROMPTUTENS] = '1';
        al = (uint8_t)(al - 10);
    }
    mem[SD_SUBMIT_PROMPTUONES] = (uint8_t)(mem[SD_SUBMIT_PROMPTUONES] + al);
    mem[SD_SUBMIT_PROMPTDISK] = (uint8_t)('A' + defdisk_al);
    /* mov dx,offset prompt/promptutens ! call mpm -- boundary stub, no-op */

    expand_command(mem);
    /* echocommand/crlf -- boundary stubs, no-op */

    uint8_t blen = mem[SD_SUBMIT_BLEN];
    uint8_t *cmd = &mem[SD_SUBMIT_CMD];
    if (blen == 0 || cmd[0] == ';') {
        return SUBMIT_GETNXTCMD; /* gonextcmd */
    }
    if (blen == 2 && cmd[1] == ':') {
        uint8_t dl = (uint8_t)((cmd[0] & 0x5F) - 'A');
        if (dl > 15) {
            return SUBMIT_SUBMITEXIT; /* subex */
        }
        /* okdrive: call mpm (mpm_diskselect) -- boundary stub, no-op */
        return SUBMIT_GETNXTCMD; /* gonextcmd */
    }

    /* notdrive: call parsefilename -- deterministically "fails" (see
     * header comment), taking the bad-parse tail: `mov cl,126 ! mov
     * di,offset clicmd ! mov al,' ' ! repne scasb ! mov byte ptr
     * [di],'$'` -- unlike TMP.A86's own equivalent (which writes its
     * '$' terminator into a separate savebuf copy), this one scans
     * clicmd *itself* for its first space and overwrites there, a
     * real memory effect this port needs to replicate even though
     * `goodparse` itself stays unreachable. REPNE SCASB always
     * advances DI once per byte examined, including the one that
     * matches, so DI ends up one past the found space (or at
     * clicmd+126 if none of the first 126 bytes is one). */
    {
        uint16_t di = SD_SUBMIT_CLICMD;
        uint16_t count = 126;
        while (count > 0 && mem[di] != ' ') {
            di++;
            count--;
        }
        if (count > 0) {
            di++; /* the matching byte itself still advances di */
        }
        mem[di] = '$';
    }
    return SUBMIT_SUBMITEXIT;
}
