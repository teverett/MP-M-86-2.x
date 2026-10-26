/*
 * parse.c -- C port of src/kernel/PARSE.SUP.
 *
 * Fully self-contained: parse_entry and its internal helpers (setfld,
 * delim, deblank, fcbinit) never call into any unported module, so unlike
 * most other Tier-2 files this one needed no boundary stubs at all.
 *
 * Not modeled: `delim`'s real assembly reaches its own `pfn_err` directly
 * on an illegal character by popping two words off the stack (discarding
 * its own return address) to jump straight past whichever of its several
 * callers invoked it -- the same category of manual stack trick as
 * FLAG.RTM's `flag_valid` (see flag.c's own header comment). This port
 * uses an ordinary tri-state return instead (see `delim` below) and lets
 * every caller propagate it with a normal early return; the observable
 * result (bx/cx and the FCB bytes written) is identical.
 *
 * Runs against real memory: the PCB, the string and the FCB are all in
 * the running process's u_wrkseg (the asm's `mov ds,u_wrkseg ! mov
 * es,u_wrkseg`), read from its UDA; the internal helpers take that
 * segment (`w`).
 */
#include <stdint.h>

#include "defs.h"

typedef struct {
    uint16_t bx, cx;
} parse_bxcx;

static parse_bxcx pfn_err(uint16_t bp) {
    parse_bxcx r = {0xFFFF, bp};
    return r;
}

static parse_bxcx pfn_endp(void) {
    parse_bxcx r = {0, 0};
    return r;
}

static parse_bxcx pfn_out(uint16_t bx) {
    parse_bxcx r = {bx, 0};
    return r;
}

/*
 * delim -- see if char is delim, if not UPPER it, err if illegal
 *	input:	si -> next char in parse string
 *	output:	*al_out = the character, converted to upper-case and with
 *		the high bit stripped if it wasn't a delimiter
 *		returns 1 if it was a delimiter, 0 if not, -1 if illegal
 *		(not a delimiter and not graphic) -- the caller must
 *		return pfn_err(e_badfname) immediately on -1, same as the
 *		real routine's own direct jump there
 */
static int delim(const uint8_t *w, uint16_t si, uint8_t *al_out) {
    static const uint8_t delims[] = {0, tab, cr, '.', ':', ';', '=', ',', '/', '[', ']', '<', '>', ' '};
    uint8_t al = w[si];

    for (unsigned i = 0; i < sizeof(delims); i++) {
        if (delims[i] == al) {
            *al_out = al;
            return 1;
        }
    }
    if (!(al > ' ')) return -1; /* no_del: not graphic either */
    if (al < 'a') {             /* del_up's "jb delret" -- unchanged, not a delimiter */
        *al_out = al;
        return 0;
    }
    if (al <= 'z') al = (uint8_t)(al & 0x5F); /* upper-case 'a'-'z' */
    al = (uint8_t)(al & 0x7F);                /* del_noup: strip the high bit (always) */
    *al_out = al;
    return al == 0 ? 1 : 0; /* del_noup's trailing AND also sets the real asm's ZF */
}

/*
 * setfld -- fill in a field until end of field or delim
 *	input/output: *bx_io (dest ptr), *ch_io (count remaining),
 *	*si_io (source ptr) -- all advance in place
 *	output: *al_out = the character setfld stopped on
 *		returns -1 on an illegal character (propagate pfn_err),
 *		0 otherwise (caller checks *ch_io: 0 means the field filled
 *		completely without hitting a delimiter)
 */
static int setfld(uint8_t *w, uint16_t *bx_io, uint8_t *ch_io, uint16_t *si_io, uint8_t *al_out) {
    for (;;) {
        uint8_t al;
        int d = delim(w, *si_io, &al);
        if (d < 0) return -1;
        if (d != 0) { /* setret */
            *al_out = al;
            return 0;
        }
        if (al == '*') {
            do {
                w[*bx_io] = '?';
                (*bx_io)++;
                (*ch_io)--;
            } while (*ch_io != 0);
            (*si_io)++;
            *al_out = al; /* AL is never overwritten by the '?' fill -- see FS_SET's comment */
            return 0;
        }
        w[*bx_io] = al;
        (*bx_io)++;
        (*ch_io)--;
        (*si_io)++;
        *al_out = al;
        if (*ch_io == 0) return 0;
    }
}

/* deblank -- strip leading blanks */
static uint16_t deblank(const uint8_t *w, uint16_t si) {
    while (w[si] == ' ' || w[si] == tab) si++;
    return si;
}

/* fcbinit -- Initialize FCB */
static void fcbinit(uint8_t *w, uint16_t bx) {
    uint16_t di = bx;
    w[di++] = 0;                                   /* 0    = 0 */
    for (int i = 0; i < 11; i++) w[di++] = ' ';     /* 1-11 = ' ' */
    for (int i = 0; i < 2; i++) { seg_w16(w, di, 0); di += 2; } /* 12-15 = 0 */
    for (int i = 0; i < 8; i++) w[di++] = ' ';      /* 16-23 = ' ' */
    for (int i = 0; i < 3; i++) w[di++] = 0;         /* 24-26 = 0 */
}

/*
 * parse_entry -- Parse filename into FCB
 *	input:	dx -> PCB in u_wrkseg
 *	output:	bx = 0xFFFF on error
 *		   = 0      on last name in string
 *		   = offset of delimiter following name
 *		cx = error code
 */
parse_bxcx parse_entry(uint8_t *phys, uint16_t sysdat, uint16_t dx) {
    uint8_t *w = far_ptr(phys, seg_r16(far_ptr(phys, rlr_uda(far_ptr(phys, sysdat, 0)), 0), u_wrkseg), 0);
    uint16_t pcb = dx;
    uint16_t si = seg_r16(w, (uint16_t)(pcb + pcb_flnmptr));
    uint16_t fcb = seg_r16(w, (uint16_t)(pcb + pcb_fcbptr));
    uint16_t bx = fcb;

    fcbinit(w, bx);
    si = deblank(w, si);

    uint16_t bp = e_badfname;
    uint8_t al;
    int d = delim(w, si, &al);
    if (d < 0) return pfn_err(bp);
    if (d != 0) return pfn_endp(); /* the very first char is itself a delimiter */

    /* parse1 */
    uint8_t ch_saved = al;
    si++;
    al = w[si];
    if (al == ':') {
        bp = e_illdisk;
        if (ch_saved < 'A') return pfn_err(bp); /* sub al,'A' ! jc */
        uint8_t drv = (uint8_t)(ch_saved - 'A');
        if (drv >= 16) return pfn_err(bp); /* cmp al,16 ! jge */
        w[(uint16_t)(fcb + fcb_dr)] = (uint8_t)(drv + 1);
        bp = e_badfname;
        si++;
        d = delim(w, si, &al);
        if (d < 0) return pfn_err(bp);
        if (d != 0 && (al == '.' || al == ':' || al == ';')) return pfn_err(bp);
        /* else: parse3, whether or not a (different) delimiter was found */
    } else {
        si--; /* parse2: use default disk */
    }

    /* parse3: name field */
    uint8_t ch = 8;
    bx = (uint16_t)(fcb + fcb_name);
    if (setfld(w, &bx, &ch, &si, &al) < 0) return pfn_err(bp);
    if (ch == 0) {
        d = delim(w, si, &al);
        if (d < 0) return pfn_err(bp);
        if (d == 0) return pfn_err(bp); /* more than 8 chars */
    }

    /* parse4: file type, if present */
    if (al == '.') {
        bp = e_badftype;
        ch = 3;
        bx = (uint16_t)(fcb + fcb_type);
        si++;
        if (setfld(w, &bx, &ch, &si, &al) < 0) return pfn_err(bp);
        if (ch == 0) {
            d = delim(w, si, &al);
            if (d < 0) return pfn_err(bp);
            if (d == 0) return pfn_err(bp);
        }
    }

    /* parse5: password, if present */
    if (al == ';') {
        bp = e_ill_passwd;
        ch = 8;
        bx = (uint16_t)(fcb + fcb_passwd);
        si++;
        if (setfld(w, &bx, &ch, &si, &al) < 0) return pfn_err(bp);
        w[(uint16_t)(fcb + fcb_plen)] = (uint8_t)(8 - ch);
        if (ch == 0) {
            d = delim(w, si, &al);
            if (d < 0) return pfn_err(bp);
            if (d == 0) return pfn_err(bp);
        }
    }

    /* parse8: see if more to parse */
    bx = si; /* the offset pfn_out returns, if this is the last name in the string */
    si = deblank(w, si);
    d = delim(w, si, &al);
    if (d < 0) return pfn_err(bp);
    if (d == 0) return pfn_out(bx);
    bx = si;
    if (al == 0) return pfn_endp();
    if (al != cr) return pfn_out(bx);
    return pfn_endp();
}
