/*
 * gentab.c -- C version of src-assem/toolchain/GENTAB.PLI, DRI's
 * generator for the disassembler's mnemonic tables: from OP86.DAT (one
 * mnemonic a line, ended by `$`) it writes
 *   OPTAB.DAT -- `declare opsN (*) byte initial (...)`, the mnemonics N
 *       characters long for N = 2..6, five to a line; then nops (how
 *       many of each length) and opn$in (where each length starts),
 *       DIS86.PLM's `$include(optab.dat)`;
 *   OPTAB.LIT -- the opN$in literals, then `<mnemonic>$in literally
 *       'opN$in + k'` for each mnemonic (a trailing ':' dropped:
 *       CS:, DS:, ...), which INS86.PLM and ASMTAB.PLM include,
 * and lists the counts on the console.
 *
 * A host tool, as GENTAB.CPM was (DDT86.SUB: `gentab op86.dat`), not a
 * transient: DDT86's build uses the OPTAB files in src-assem/toolchain/,
 * and tests/src-c/test_gentab.py checks that this regenerates them byte
 * for byte. The output is PL/I's: each `put skip list` line is CR, LF,
 * then the line (`^I` is a tab); the files end padded with ^Z to a
 * 128-byte CP/M record.
 *
 *     gentab [OP86.DAT [OPTAB.DAT OPTAB.LIT]]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXOPS 200
#define LINELEN 101

static char chars[MAXOPS + 1][7]; /* chars (200) char (6) varying, 1-based */
static int count;
static int opcnt[7];              /* opcnt (2:6) */

/* PL/I's put skip list (line): a new line, then the line */
static void put_skip_list(FILE *f, const char *line) {
    fputs("\r\n", f);
    fputs(line, f);
}

static void put_skip(FILE *f) { fputs("\r\n", f); }

/* deblank: the number, without char()'s leading blanks */
static const char *deblank(int i) {
    static char buf[4][12];
    static int k;
    char *b = buf[k++ & 3];
    snprintf(b, 12, "%d", i);
    return b;
}

static void cat(char *line, const char *s) {
    if (strlen(line) + strlen(s) >= LINELEN) {
        fprintf(stderr, "gentab: line too long\n");
        exit(1);
    }
    strcat(line, s);
}

/* close a CP/M file: pad with ^Z to a whole 128-byte record */
static void cpm_close(FILE *f) {
    long n = ftell(f);
    while (n++ % 128 != 0) fputc(0x1A, f);
    fclose(f);
}

int main(int argc, char **argv) {
    const char *in = argc > 1 ? argv[1] : "OP86.DAT";
    const char *dat = argc > 3 ? argv[2] : "OPTAB.DAT";
    const char *lit = argc > 3 ? argv[3] : "OPTAB.LIT";
    FILE *infile = fopen(in, "rb"), *outfile, *litfile;
    if (!infile) {
        perror(in);
        return 1;
    }
    /* read op86.dat file into chars array: list-directed input, items
     * separated by blanks or commas, up to '$' */
    char tok[16];
    for (;;) {
        int c, n = 0;
        while ((c = fgetc(infile)) != EOF && (c == ' ' || c == ',' || c == '\t' || c == '\r' || c == '\n')) {
        }
        while (c != EOF && c != ' ' && c != ',' && c != '\t' && c != '\r' && c != '\n' && c != 0x1A) {
            if (n < 15) tok[n++] = (char)c;
            c = fgetc(infile);
        }
        tok[n] = 0;
        if (n == 0 || strcmp(tok, "$") == 0) break;
        if (++count > MAXOPS || n > 6) {
            fprintf(stderr, "gentab: %s: too many opcodes, or one too long\n", in);
            return 1;
        }
        strcpy(chars[count], tok);
    }
    fclose(infile);
    outfile = fopen(dat, "wb");
    litfile = fopen(lit, "wb");
    if (!outfile || !litfile) {
        perror(!outfile ? dat : lit);
        return 1;
    }

    /* create ascii opcode tables, 1 for each character length */
    char line[LINELEN];
    for (int i = 2; i <= 6; i++) {
        snprintf(line, sizeof line, "declare ops%s (*) byte initial (", deblank(i));
        int n = 0;
        for (int j = 1; j <= count; j++) {
            if ((int)strlen(chars[j]) == i) {
                if (n > 0) cat(line, ", ");
                if (n / 5 * 5 == n) {
                    put_skip_list(outfile, line);
                    strcpy(line, "\t");
                }
                n++;
                cat(line, "'");
                cat(line, chars[j]);
                cat(line, "'");
                opcnt[i]++;
            }
        }
        cat(line, ");");
        put_skip_list(outfile, line);
        put_skip(outfile);
    }

    /* create array containing # of opcodes of each length */
    strcpy(line, "declare nops (5) byte public initial (");
    for (int i = 2; i <= 6; i++) {
        cat(line, deblank(opcnt[i]));
        if (i < 6) cat(line, ", ");
    }
    cat(line, ");");
    put_skip_list(outfile, line);
    put_skip(outfile);

    /* create array containing starting index for each opcode length */
    strcpy(line, "declare opn$in (*) byte public initial (");
    int sum = 0;
    for (int i = 2; i <= 6; i++) {
        cat(line, deblank(sum));
        cat(line, ", ");
        sum += opcnt[i];
    }
    cat(line, "255);");
    put_skip_list(outfile, line);

    /* create literals for starting indexes for each opcode length */
    sum = 0;
    put_skip_list(litfile, "declare");
    for (int i = 2; i <= 6; i++) {
        printf("\n%s\t%s-character opcodes", deblank(opcnt[i]), deblank(i));
        snprintf(line, sizeof line, "\top%s$in literally '%s'%c", deblank(i), deblank(sum), i == 6 ? ';' : ',');
        put_skip_list(litfile, line);
        sum += opcnt[i];
        opcnt[i] = 0;
    }
    printf("\n");

    /* create literals for position in opcode tables of each opcode */
    put_skip(litfile);
    put_skip_list(litfile, "declare");
    for (int j = 1; j <= count; j++) {
        int len = (int)strlen(chars[j]);
        if (strchr(chars[j], ':')) chars[j][len - 1] = 0; /* substr (chars (j), 1, len-1) */
        snprintf(line, sizeof line, "\t%s$in literally 'op%s$in + %s'%c", chars[j], deblank(len),
                 deblank(opcnt[len]), j == count ? ';' : ',');
        put_skip_list(litfile, line);
        opcnt[len]++;
    }
    cpm_close(outfile);
    cpm_close(litfile);
    return 0;
}
