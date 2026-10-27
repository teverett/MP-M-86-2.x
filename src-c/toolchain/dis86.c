/*
 * dis86.c -- C version of src-assem/toolchain/DIS86.PLM, DDT86's 8086
 * disassembler (its overlay, with ins86.c's tables): disem prints the
 * instruction at a far pointer -- any prefixes (up to three), the
 * mnemonic padded to seven columns, the operands -- and returns the
 * offset past it; an opcode it doesn't know prints "??=    " and the
 * byte, and counts as one byte.
 *
 * Its data (OPTAB.DAT's ASCII opcode tables, the register names, the
 * PL/M variables and string constants) is DIS86's as the build lays it
 * out, after the overlay's code and INS86's tables (toolchain_syms.h's
 * DIS86_*); dis86_image, below, is its initial value, which
 * dis86_install puts there with ins86.c's. PL/M's semantics are kept:
 * bytes wrap, a relation is 0FFh or 0, IF tests bit 0 (DIS86's own
 * `true` is 1), the variables are the PL/M globals, in DDT86's segment
 * (`ds`), and the based ones read memory through their pointers:
 * table$char (DS:table$ptr), alt$table (DS:alt$table$base) and
 * disem$byte/disem$word (disem$ptr's seg:off, the offset wrapping in its
 * segment as the 8086's does).
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "toolchain.h"
#include "toolchain_syms.h"

#define T 1 /* DIS86's true */
#define F 0

typedef struct __attribute__((packed)) {
    char ops2[12 * 2], ops3[69 * 3], ops4[25 * 4], ops5[16 * 5], ops6[2 * 6]; /* OPTAB.DAT */
    uint8_t nops[5];
    uint8_t opn_in[6];
    uint16_t tab_ptrs[5];
    char left_bracket, right_bracket;
    uint16_t alt_table_base;
    uint8_t mod_bits, reg_bits, rm_bits, byte1_reg_bits, d_bit, s_bit, v_bit, w_bit, z_bit;
    uint8_t mnemonic_index, instr_type;
    uint16_t table_ptr;
    uint16_t disem_offset, disem_seg; /* disem$ptr */
    uint16_t disem_end;
    uint8_t b_or_w_flag, error_flag;
    char reg16[16], reg8[16], segreg[8];
    char arr7[8], arr11[6], arr12[6]; /* the string constants */
} dis86_data;

#define AT(f, sym) _Static_assert(offsetof(dis86_data, f) == (sym) - DIS86_OPS2, #f)
AT(ops3, DIS86_OPS3);
AT(ops4, DIS86_OPS4);
AT(ops5, DIS86_OPS5);
AT(ops6, DIS86_OPS6);
AT(nops, DIS86_NOPS);
AT(opn_in, DIS86_OPNIN);
AT(tab_ptrs, DIS86_TABPTRS);
AT(left_bracket, DIS86_LEFTBRACKET);
AT(right_bracket, DIS86_RIGHTBRACKET);
AT(alt_table_base, DIS86_ALTTABLEBASE);
AT(mod_bits, DIS86_MODBITS);
AT(byte1_reg_bits, DIS86_BYTE1REGBITS);
AT(z_bit, DIS86_ZBIT);
AT(mnemonic_index, DIS86_MNEMONICINDEX);
AT(instr_type, DIS86_INSTRTYPE);
AT(table_ptr, DIS86_TABLEPTR);
AT(disem_offset, DIS86_DISEMPTR);
AT(disem_end, DIS86_DISEMEND);
AT(b_or_w_flag, DIS86_BORWFLAG);
AT(error_flag, DIS86_ERRORFLAG);
AT(reg16, DIS86_REG16);
AT(reg8, DIS86_REG8);
AT(segreg, DIS86_SEGREG);
AT(arr7, DIS86_ARR7);
AT(arr11, DIS86_ARR11);
AT(arr12, DIS86_ARR12);
_Static_assert(sizeof(dis86_data) == TC_DIS_END - DIS86_OPS2, "length");

static const dis86_data dis86_image = {
    .ops2 = "IN" "JA" "JB" "JC" "JE" "JG" "JL" "JO" "JP" "JS" "JZ" "OR",
    .ops3 = "AAA" "AAD" "AAM" "AAS" "ADC" "ADD" "AND" "CBW" "CLC" "CLD"
            "CLI" "CMC" "CMP" "CS:" "CWD" "DAA" "DAS" "DEC" "DIV" "DS:"
            "ES:" "ESC" "HLT" "INC" "INT" "JAE" "JBE" "JGE" "JLE" "JMP"
            "JNA" "JNB" "JNC" "JNE" "JNG" "JNL" "JNO" "JNP" "JNS" "JNZ"
            "JPE" "JPO" "LDS" "LEA" "LES" "MOV" "MUL" "NEG" "NOP" "NOT"
            "OUT" "POP" "RCL" "RCR" "REP" "RET" "ROL" "ROR" "SAL" "SAR"
            "SBB" "SHL" "SHR" "SS:" "STC" "STD" "STI" "SUB" "XOR",
    .ops4 = "CALL" "IDIV" "IMUL" "INTO" "IRET" "JCXZ" "JMPF" "JMPS" "JNAE" "JNBE"
            "JNGE" "JNLE" "LAHF" "LOCK" "LOOP" "POPF" "PUSH" "REPE" "REPZ" "RETF"
            "SAHF" "TEST" "WAIT" "XCHG" "XLAT",
    .ops5 = "CALLF" "CMPSB" "CMPSW" "LODSB" "LODSW" "LOOPE" "LOOPZ" "MOVSB" "MOVSW" "PUSHF"
            "REPNE" "REPNZ" "SCASB" "SCASW" "STOSB" "STOSW",
    .ops6 = "LOOPNE" "LOOPNZ",
    .nops = {12, 69, 25, 16, 2},
    .opn_in = {0, 12, 81, 106, 122, 255},
    .tab_ptrs = {DIS86_OPS2, DIS86_OPS3, DIS86_OPS4, DIS86_OPS5, DIS86_OPS6},
    .left_bracket = '[',
    .right_bracket = ']',
    .reg16 = "AX" "CX" "DX" "BX" "SP" "BP" "SI" "DI",
    .reg8 = "AL" "CL" "DL" "BL" "AH" "CH" "DH" "BH",
    .segreg = "ES" "CS" "SS" "DS",
    .arr7 = "\?\?=    $",
    .arr11 = "WORD $",
    .arr12 = "BYTE $",
};

void dis86_install(uint16_t ds) {
    ins86_install(ds);
    uint8_t *dst = far_ptr(rt_phys(), ds, DIS86_OPS2);
    for (uint16_t i = 0; i < sizeof dis86_image; i++) dst[i] = ((const uint8_t *)&dis86_image)[i];
}

/* DIS86's variables, in DDT86's segment; every routine has its `ds` */
#define G ((dis86_data *)far_ptr(rt_phys(), ds, DIS86_OPS2))

static uint8_t rel(int c) { return c ? 0xFF : 0; } /* a PL/M relation */

static uint8_t ds_byte(uint16_t ds, uint16_t a) { return *far_ptr(rt_phys(), ds, a); }
static uint16_t ds_word(uint16_t ds, uint16_t a) { return rd16(far_ptr(rt_phys(), ds, a)); }

/* INS86's externals: instr$table, alt$table$ptrs */
static uint8_t instr_table(uint16_t ds, uint16_t i) { return ds_byte(ds, (uint16_t)(INS86_INSTRTABLE + i)); }
static uint16_t alt_table_ptrs(uint16_t ds, uint8_t i) {
    return ds_word(ds, (uint16_t)(INS86_ALTTABLEPTRS + 2 * i));
}

/* alt$table (based alt$table$base), table$char (based table$ptr) */
static uint8_t alt_table(uint16_t ds, uint16_t i) { return ds_byte(ds, (uint16_t)(G->alt_table_base + i)); }
static uint8_t table_char(uint16_t ds) { return ds_byte(ds, G->table_ptr); }

/* disem$byte, disem$word (based disem$ptr) */
static uint8_t disem_byte(uint16_t ds, uint16_t i) {
    return *far_ptr(rt_phys(), G->disem_seg, (uint16_t)(G->disem_offset + i));
}
static uint16_t disem_word(uint16_t ds) {
    return (uint16_t)(disem_byte(ds, 0) | (disem_byte(ds, 1) << 8));
}
static void advance(uint16_t ds, uint16_t n) { G->disem_offset = (uint16_t)(G->disem_offset + n); }

static void conout(uint16_t ds, uint8_t c) { ddt_conout(ds, c); }

static void comma(uint16_t ds) { conout(ds, ','); }

void dis86_printm(uint16_t ds, uint16_t a) {
    while (rel(ds_byte(ds, a) != '$') & 1) {
        conout(ds, ds_byte(ds, a));
        a = (uint16_t)(a + 1);
    }
}

static void print_nibble(uint16_t ds, uint8_t b) {
    if (rel(b > 9) & 1) conout(ds, (uint8_t)(b - 10 + 'A'));
    else conout(ds, (uint8_t)(b + '0'));
}

static void print_byte(uint16_t ds, uint8_t b) {
    print_nibble(ds, b >> 4);
    print_nibble(ds, b & 0x0F);
}

void dis86_print_word(uint16_t ds, uint16_t a) {
    print_byte(ds, (uint8_t)(a >> 8));
    print_byte(ds, (uint8_t)a);
}

static void error(uint16_t ds) {
    dis86_printm(ds, DIS86_ARR7); /* ??= */
    print_byte(ds, disem_byte(ds, 0));
    advance(ds, 1);
}

static void set_bits(uint16_t ds) {
    G->byte1_reg_bits = disem_byte(ds, 0) & 7;
    G->mod_bits = disem_byte(ds, 1) >> 6;
    G->reg_bits = (disem_byte(ds, 1) >> 3) & 7;
    G->rm_bits = disem_byte(ds, 1) & 7;
    G->w_bit = G->z_bit = disem_byte(ds, 0) & 1;
    G->d_bit = G->s_bit = G->v_bit = (disem_byte(ds, 0) >> 1) & 1;
}

static void print_b_or_w(uint16_t ds) {
    if (G->w_bit & 1) dis86_printm(ds, DIS86_ARR11); /* WORD */
    else dis86_printm(ds, DIS86_ARR12);              /* BYTE */
}

static void print_reg(uint16_t ds, uint16_t reg_add, uint8_t reg) {
    G->table_ptr = (uint16_t)(reg_add + (uint8_t)(reg << 1));
    conout(ds, table_char(ds));
    G->table_ptr = (uint16_t)(G->table_ptr + 1);
    conout(ds, table_char(ds));
}

static void print_reg8(uint16_t ds, uint8_t reg) { print_reg(ds, DIS86_REG8, reg); }
static void print_reg16(uint16_t ds, uint8_t reg) { print_reg(ds, DIS86_REG16, reg); }

static void print_reg_8_or_16(uint16_t ds, uint8_t reg_num) {
    if (G->w_bit & 1) print_reg16(ds, reg_num);
    else print_reg8(ds, reg_num);
}

static void print_2_reg_16(uint16_t ds, uint8_t r1, uint8_t r2) {
    print_reg16(ds, r1);
    conout(ds, '+');
    print_reg16(ds, r2);
}

static void print_a_reg(uint16_t ds) {
    if (G->w_bit & 1) print_reg16(ds, 0); /* AX */
    else print_reg8(ds, 0);               /* AL */
}

static void print_seg_reg(uint16_t ds, uint8_t reg) { print_reg(ds, DIS86_SEGREG, reg); }

static void print_data_8(uint16_t ds) {
    print_byte(ds, disem_byte(ds, 0));
    advance(ds, 1);
}

static void print_data_16(uint16_t ds) {
    dis86_print_word(ds, disem_word(ds));
    advance(ds, 2);
}

static void print_data_8_or_16(uint16_t ds) {
    if (G->w_bit & 1) print_data_16(ds);
    else print_data_8(ds);
}

/* a sign-extended byte (83h's immediate) */
static void print_data_sw(uint16_t ds) {
    uint8_t b = disem_byte(ds, 0);
    if ((uint8_t)((b << 1) | (b >> 7)) & 1) dis86_print_word(ds, (uint16_t)(0xFF00 | disem_byte(ds, 0)));
    else dis86_print_word(ds, disem_byte(ds, 0));
    advance(ds, 1);
}

/* a short jump's target */
static void print_signed_8(uint16_t ds) {
    uint16_t a = disem_byte(ds, 0);
    if (rel((uint8_t)a >= 0x80) & 1) a |= 0xFF00; /* sign extend to 16 bits */
    dis86_print_word(ds, (uint16_t)(G->disem_offset + a + 1));
    advance(ds, 1);
}

/* a near jump's or call's */
static void print_signed_16(uint16_t ds) {
    dis86_print_word(ds, (uint16_t)(G->disem_offset + disem_word(ds) + 2));
    advance(ds, 2);
}

static void print_direct_addr(uint16_t ds) {
    conout(ds, (uint8_t)G->left_bracket);
    dis86_print_word(ds, disem_word(ds));
    conout(ds, (uint8_t)G->right_bracket);
    advance(ds, 2);
}

static void print_mod_rm(uint16_t ds) {
    advance(ds, 1); /* point past mod/reg/rm byte */
    if (rel(G->mod_bits == 3) & 1) {
        print_reg_8_or_16(ds, G->rm_bits);
        return;
    }
    if (G->b_or_w_flag & 1) print_b_or_w(ds);
    if ((rel(G->rm_bits == 6) & rel(G->mod_bits == 0)) & 1) {
        print_direct_addr(ds);
        return;
    }
    if (rel(G->mod_bits == 1) & 1) {
        if ((rel(G->rm_bits != 6) | rel(disem_byte(ds, 0) != 0)) & 1) print_byte(ds, disem_byte(ds, 0));
        advance(ds, 1);
    } else if (rel(G->mod_bits == 2) & 1) {
        dis86_print_word(ds, disem_word(ds));
        advance(ds, 2);
    }
    conout(ds, (uint8_t)G->left_bracket);
    switch (G->rm_bits) {
    case 0: print_2_reg_16(ds, 3, 6); break; /* BX+SI */
    case 1: print_2_reg_16(ds, 3, 7); break; /* BX+DI */
    case 2: print_2_reg_16(ds, 5, 6); break; /* BP+SI */
    case 3: print_2_reg_16(ds, 5, 7); break; /* BP+DI */
    case 4: print_reg16(ds, 6); break;       /* SI */
    case 5: print_reg16(ds, 7); break;       /* DI */
    case 6: print_reg16(ds, 5); break;       /* BP */
    case 7: print_reg16(ds, 3); break;       /* BX */
    }
    conout(ds, (uint8_t)G->right_bracket);
}

static void print_mod_reg_rm(uint16_t ds) {
    if (G->d_bit & 1) {
        print_reg_8_or_16(ds, G->reg_bits);
        conout(ds, ',');
        print_mod_rm(ds);
    } else {
        print_mod_rm(ds);
        conout(ds, ',');
        print_reg_8_or_16(ds, G->reg_bits);
    }
}

/* the mnemonic, from the ops table for its length, padded to 7 */
static void print_mnemonic(uint16_t ds) {
    uint8_t len = 2, i;
    while (rel(G->mnemonic_index >= ds_byte(ds, (uint16_t)(DIS86_OPNIN + (uint8_t)(len - 1)))) & 1)
        len = (uint8_t)(len + 1);
    uint8_t n = (uint8_t)(G->mnemonic_index - ds_byte(ds, (uint16_t)(DIS86_OPNIN + (uint8_t)(len - 2))));
    G->table_ptr = (uint16_t)(ds_word(ds, (uint16_t)(DIS86_TABPTRS + 2 * (uint8_t)(len - 2))) + n * len);
    for (i = 1; i <= 7; i++) {
        if (rel(i <= len) & 1) {
            conout(ds, table_char(ds));
            G->table_ptr = (uint16_t)(G->table_ptr + 1);
        } else {
            conout(ds, ' ');
        }
    }
    advance(ds, 1);
}

static void type1(uint16_t ds) { print_mnemonic(ds); }

static void type2(uint16_t ds) { /* AAM, AAD: 0Ah follows */
    if (rel(disem_byte(ds, 1) == 0x0A) & 1) {
        print_mnemonic(ds);
        advance(ds, 1);
    } else {
        G->error_flag = T;
    }
}

static void type3(uint16_t ds) {
    print_mnemonic(ds);
    print_reg16(ds, G->byte1_reg_bits);
}

static void type4(uint16_t ds) {
    uint8_t temp = (disem_byte(ds, 0) >> 3) & 3;
    print_mnemonic(ds);
    print_seg_reg(ds, temp);
}

static void type5(uint16_t ds) {
    print_mnemonic(ds);
    print_signed_8(ds);
}

static void type6(uint16_t ds) {
    print_mnemonic(ds);
    print_signed_16(ds);
}

static void type8(uint16_t ds) { /* 7, 9 */
    print_mnemonic(ds);
    print_mod_rm(ds);
}

static void type10(uint16_t ds) {
    print_mnemonic(ds);
    print_data_8(ds);
}

static void type11(uint16_t ds) {
    print_mnemonic(ds);
    print_data_16(ds);
}

static void type12(uint16_t ds) {
    print_mnemonic(ds);
    conout(ds, '3');
}

static void type13(uint16_t ds) { /* seg:off */
    uint16_t temp;
    print_mnemonic(ds);
    temp = disem_word(ds);
    advance(ds, 2);
    print_data_16(ds);
    conout(ds, ':');
    dis86_print_word(ds, temp);
}

static void type14(uint16_t ds) { /* 15, 16, 17 */
    print_mnemonic(ds);
    print_mod_reg_rm(ds);
}

static void type18(uint16_t ds) { /* 19, 20, 21 */
    print_mnemonic(ds);
    if (G->d_bit & 1) {
        print_direct_addr(ds);
        comma(ds);
        print_a_reg(ds);
    } else {
        print_a_reg(ds);
        comma(ds);
        print_direct_addr(ds);
    }
}

static void type22(uint16_t ds) {
    print_mnemonic(ds);
    if (G->d_bit & 1) {
        print_data_8(ds);
        comma(ds);
        print_a_reg(ds);
    } else {
        print_a_reg(ds);
        comma(ds);
        print_data_8(ds);
    }
}

static void type23(uint16_t ds) { /* 24 */
    print_mnemonic(ds);
    print_a_reg(ds);
    comma(ds);
    print_data_8_or_16(ds);
}

static void type25(uint16_t ds) { /* 26 */
    print_mnemonic(ds);
    if (G->d_bit & 1) {
        print_reg16(ds, 2); /* DX */
        comma(ds);
        print_a_reg(ds);
    } else {
        print_a_reg(ds);
        comma(ds);
        print_reg16(ds, 2);
    }
}

static void type27(uint16_t ds) { /* 28, 29, 30 */
    print_mnemonic(ds);
    G->b_or_w_flag = T;
    print_mod_rm(ds);
    comma(ds);
    if (G->v_bit & 1) print_reg8(ds, 1); /* CL */
    else conout(ds, '1');
}

static void type31(uint16_t ds) { /* 32 */
    set_bits(ds);
    G->reg_bits = G->byte1_reg_bits;
    G->w_bit = (disem_byte(ds, 0) >> 3) & 1;
    print_mnemonic(ds);
    print_reg_8_or_16(ds, G->reg_bits);
    comma(ds);
    print_data_8_or_16(ds);
}

static void type33(uint16_t ds) {
    print_mnemonic(ds);
    print_reg16(ds, 0); /* AX */
    comma(ds);
    print_reg16(ds, G->byte1_reg_bits);
}

static void type34(uint16_t ds) { /* 35 */
    print_mnemonic(ds);
    G->b_or_w_flag = T;
    print_mod_rm(ds);
    comma(ds);
    print_data_8_or_16(ds);
}

static void type36(uint16_t ds) { /* 37 */
    G->w_bit = T; /* force 16 bit reg, mem */
    if (rel(G->reg_bits > 3) & 1) {
        G->error_flag = T;
        return;
    }
    print_mnemonic(ds);
    if (G->d_bit & 1) {
        print_seg_reg(ds, G->reg_bits);
        comma(ds);
        print_mod_rm(ds);
    } else {
        print_mod_rm(ds);
        comma(ds);
        print_seg_reg(ds, G->reg_bits);
    }
}

static void type38(uint16_t ds) {
    print_mnemonic(ds);
    print_mod_rm(ds);
    comma(ds);
    print_data_8(ds);
}

static void type39(uint16_t ds) {
    if (rel(G->mod_bits == 3) & 1) {
        G->error_flag = T;
        return;
    }
    print_mnemonic(ds);
    print_reg16(ds, G->reg_bits);
    comma(ds);
    print_mod_rm(ds);
}

static void type40(uint16_t ds) { /* 41 */
    if (rel(G->mod_bits == 3) & 1) {
        G->error_flag = T;
        return;
    }
    print_mnemonic(ds);
    G->b_or_w_flag = T;
    print_mod_rm(ds);
    comma(ds);
    print_data_8_or_16(ds);
}

static void type42(uint16_t ds) { /* ESC */
    print_mnemonic(ds);
    print_byte(ds, (uint8_t)((uint8_t)(G->byte1_reg_bits << 3) | G->reg_bits));
    comma(ds);
    print_mod_rm(ds);
}

static void type44(uint16_t ds) {
    print_mnemonic(ds);
    G->b_or_w_flag = T;
    print_mod_rm(ds);
    comma(ds);
    if ((rel(G->s_bit == 1) & rel(G->w_bit == 1)) & 1) print_data_sw(ds);
    else print_data_8_or_16(ds);
}

static void type45(uint16_t ds) {
    G->b_or_w_flag = T;
    type8(ds);
}

static void dis(uint16_t ds) {
    G->error_flag = G->b_or_w_flag = F;
    set_bits(ds);
    if (rel(G->instr_type == 26) & 1) { /* a group: the ModRM reg picks it */
        G->alt_table_base = alt_table_ptrs(ds, G->mnemonic_index);
        G->mnemonic_index = alt_table(ds, (uint16_t)(G->reg_bits * 2));
        G->instr_type = alt_table(ds, (uint16_t)(G->reg_bits * 2 + 1));
    }
    if (rel(G->instr_type > 28) & 1) {
        G->error_flag = T;
    } else {
        switch (G->instr_type) {
        case 0: G->error_flag = T; break;
        case 1: type1(ds); break;
        case 2: type2(ds); break;
        case 3: type3(ds); break;
        case 4: type4(ds); break;
        case 5: type5(ds); break;
        case 6: type6(ds); break;
        case 7: type8(ds); break;
        case 8: type10(ds); break;
        case 9: type11(ds); break;
        case 10: type12(ds); break;
        case 11: type13(ds); break;
        case 12: type14(ds); break;
        case 13: type18(ds); break;
        case 14: type22(ds); break;
        case 15: type23(ds); break;
        case 16: type25(ds); break;
        case 17: type27(ds); break;
        case 18: type31(ds); break;
        case 19: type33(ds); break;
        case 20: type34(ds); break;
        case 21: type36(ds); break;
        case 22: type38(ds); break;
        case 23: type39(ds); break;
        case 24: type40(ds); break;
        case 25: type42(ds); break;
        case 26: break;
        case 27: type44(ds); break;
        case 28: type45(ds); break;
        }
    }
    if (G->error_flag & 1) error(ds);
}

uint16_t dis86_disem(uint16_t ds, uint16_t off, uint16_t seg) {
    uint8_t nprefix;
    G->disem_offset = off; /* disem$ptr = disloc */
    G->disem_seg = seg;
    nprefix = 0;
    for (;;) {
        G->mnemonic_index = instr_table(ds, (uint16_t)(disem_byte(ds, 0) * 2));
        G->instr_type = instr_table(ds, (uint16_t)(disem_byte(ds, 0) * 2 + 1));
        if ((rel(G->instr_type == 0xFF) & rel(nprefix < 3)) & 1) { /* a prefix */
            print_mnemonic(ds);
            nprefix = (uint8_t)(nprefix + 1);
        } else {
            if (rel(G->instr_type == 0xFF) & 1) G->instr_type = 1;
            dis(ds);
            return G->disem_offset;
        }
    }
}
