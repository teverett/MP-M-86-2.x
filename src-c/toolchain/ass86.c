/*
 * ass86.c -- C version of src-assem/toolchain/ASS86.PLM, DDT86's line
 * assembler (its A command): asm reads a console line, assembles the
 * one 8086 instruction on it (ASM86 operand syntax: a displacement goes
 * before the brackets, 4[SI]) and stores its bytes at the far pointer
 * it is given, returning the offset past them. An empty line (or one
 * starting with '.') stores only the prefixes it has, if any, and
 * returns the offset unchanged, which ends DDT86's A command; an error
 * prints "?" (or "ambiguous operand") and goes back to DDT86 (goddt,
 * ddt_asment, which doesn't return).
 *
 * The overlay's data is ASS86.PLM's as tools/plmc.py lays it out, in
 * DDT86's segment (toolchain_syms.h): after ASMTAB's op$tab, the token
 * buffer and the scanner's state, the far pointer it assembles to, the
 * two operand descriptors (op1, op2, and op$base, the one `op` is based
 * on), the prefixes, the instruction's fields, and the DATA tables and
 * string constants; ass86_install installs them. The C works on those
 * bytes, where the PL/M has them, through ds; the register-name and
 * mnemonic tables are DIS86's (reg16, reg8, segreg, tab$ptrs, nops,
 * opn$in), as DRI linked them.
 *
 * PL/M's semantics are kept: bytes wrap, a relation is 0FFh or 0, IF
 * tests bit 0 (ASS86's own `true` is 1), AND/OR/NOT are bitwise and
 * evaluate both sides, and the procedures' own variables are C locals.
 * Some of the PL/M's quirks show through and are kept: delimiter() scans
 * num$delims = length(delimiters) + 2 bytes, running on into
 * left$bracket and right$bracket; token$word compares only a token's
 * first two characters, so AXE is AX; once a BYTE/WORD prefix has set
 * op.bw$prefix, any string token after it (a register, say) is taken
 * for another prefix, an error; MOV reg,imm doesn't range-check the
 * immediate; and OUT doesn't emit the prefixes. (IN AX,port assembled
 * IN AL,port, E4h, the W bit left out -- DRI's; fixed in ASS86.PLM too,
 * docs/bugs.md #137.)
 *
 * BYTE and WORD prefixes are recognized by token$word = 'YB' / 'OW',
 * PL/M's two-character numeric constants ('YB' = 5942h, 'OW' = 4F57h:
 * the first two bytes of the tokens BYTE and WORD, read as a word).
 * (tools/plmc.py made them the address of an interned copy until
 * docs/bugs.md #137, so the asm build never saw the prefixes.)
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "toolchain.h"
#include "toolchain_syms.h"

#define PL_TRUE 1
#define TAB 0x09
#define CR  0x0D
#define LF  0x0A
#define STRING_TYPE 0
#define NUMBER_TYPE 1
#define DELIM_TYPE  2
#define NUM_DELIMS  8 /* length (delimiters) + 2: on into the brackets */

/* the op1/op2 structure's fields (19 bytes) */
enum {
    OP_SEG_REG_NUM = 0,
    OP_REG_NUM = 1,
    OP_BASE_REG_NUM = 2,
    OP_INDEX_REG_NUM = 3,
    OP_SEG_REG_TYPE = 4,
    OP_REG16_TYPE = 5,
    OP_REG8_TYPE = 6,
    OP_NUM_TYPE = 7,
    OP_ACC_TYPE = 8,
    OP_NULL_TYPE = 9,
    OP_FAR_TYPE = 10,
    OP_MODRM_TYPE = 11,
    OP_W_BIT = 12,
    OP_BW_PREFIX = 13,
    OP_SUB_TYPE = 14,
    OP_SEG_VALUE = 15,
    OP_VALUE = 17,
    OP_SIZE = 19,
};

/* ASS86's data, from ASS86_TOKEN up to TC_ASS_END, as plmc lays it out */
typedef struct __attribute__((packed)) {
    uint8_t token[32];
    uint8_t token_len, token_type;
    uint16_t token_value;
    uint8_t next_ch;
    uint16_t assem_ptr[2];          /* assem$offset, assem$segment */
    char base_reg[4], index_reg[4];
    uint16_t op_base;
    uint8_t op1[OP_SIZE], op2[OP_SIZE];
    uint8_t prefix[3];
    uint8_t nprefix, seg_prefix_flag, lock_flag, rep_flag;
    uint8_t mod_bits, reg_bits, rm_bits, op_type, mnemonic_index, instr1, disp_len;
    uint16_t disp_word;
    uint8_t delimiters[6];
    uint8_t left_bracket, right_bracket;
    uint8_t type7_struc[2][2], type8_struc[2][4], type11_struc[2][2], cj_struc[2][2];
    char arr1[20];                  /* cr, lf, 'ambiguous operand$' */
} ass86_data;

#define AOFF(f) (ASS86_TOKEN + offsetof(ass86_data, f))
_Static_assert(ASMTAB_OPTAB + ASMTAB_OP_TAB_LEN == ASS86_TOKEN, "op$tab");
_Static_assert(AOFF(token_len) == ASS86_TOKENLEN, "token$len");
_Static_assert(AOFF(token_value) == ASS86_TOKENVALUE, "token$value");
_Static_assert(AOFF(next_ch) == ASS86_NEXTCH, "next$ch");
_Static_assert(AOFF(assem_ptr) == ASS86_ASSEMPTR, "assem$ptr");
_Static_assert(AOFF(base_reg) == ASS86_BASEREG, "base$reg");
_Static_assert(AOFF(index_reg) == ASS86_INDEXREG, "index$reg");
_Static_assert(AOFF(op_base) == ASS86_OPBASE, "op$base");
_Static_assert(AOFF(op1) == ASS86_OP1, "op1");
_Static_assert(AOFF(op2) == ASS86_OP2, "op2");
_Static_assert(AOFF(prefix) == ASS86_PREFIX, "prefix");
_Static_assert(AOFF(nprefix) == ASS86_NPREFIX, "nprefix");
_Static_assert(AOFF(rep_flag) == ASS86_REPFLAG, "rep$flag");
_Static_assert(AOFF(mod_bits) == ASS86_MODBITS, "mod$bits");
_Static_assert(AOFF(op_type) == ASS86_OPTYPE, "op$type");
_Static_assert(AOFF(instr1) == ASS86_INSTR1, "instr$1");
_Static_assert(AOFF(disp_word) == ASS86_DISPWORD, "disp$word");
_Static_assert(AOFF(delimiters) == ASS86_DELIMITERS, "delimiters");
_Static_assert(AOFF(left_bracket) == ASS86_LEFTBRACKET, "left$bracket");
_Static_assert(AOFF(right_bracket) == ASS86_RIGHTBRACKET, "right$bracket");
_Static_assert(AOFF(type7_struc) == ASS86_TYPE7STRUC, "type$7$struc");
_Static_assert(AOFF(type8_struc) == ASS86_TYPE8STRUC, "type$8$struc");
_Static_assert(AOFF(type11_struc) == ASS86_TYPE11STRUC, "type11$struc");
_Static_assert(AOFF(cj_struc) == ASS86_CJSTRUC, "cjstruc");
_Static_assert(AOFF(arr1) == ASS86_ARR1, "arr1");
_Static_assert(ASS86_TOKEN + sizeof(ass86_data) == TC_ASS_END, "the overlay's end");

static const ass86_data ass86_image = {
    .base_reg = {'B', 'X', 'B', 'P'},
    .index_reg = {'S', 'I', 'D', 'I'},
    .delimiters = {',', ':', '+', CR, ' ', TAB},
    .left_bracket = '[',
    .right_bracket = ']',
    .type7_struc = {{0x48, 0x01}, {0x40, 0x00}},                     /* dec, inc */
    .type8_struc = {{0x07, 0x58, 0x8F, 0x00}, {0x06, 0x50, 0xFF, 0x06}}, /* pop, push */
    .type11_struc = {{0xE8, 2}, {0xE9, 4}},                         /* call, jmp */
    .cj_struc = {{0x9A, 3}, {0xEA, 5}},                             /* callf, jmpf */
    .arr1 = "\r\nambiguous operand$",
};

void ass86_install(uint16_t ds) {
    uint8_t *m = far_ptr(rt_phys(), ds, 0);
    for (uint16_t i = 0; i < ASMTAB_OP_TAB_LEN; i++) m[ASMTAB_OPTAB + i] = asmtab_op_tab[i];
    for (uint16_t i = 0; i < sizeof ass86_image; i++) m[ASS86_TOKEN + i] = ((const uint8_t *)&ass86_image)[i];
}

/* ---- the image ---- */

static uint16_t g_ds;
static uint8_t *m; /* ds:0 */

#define token_len       m[ASS86_TOKENLEN]
#define token_type      m[ASS86_TOKENTYPE]
#define next_ch         m[ASS86_NEXTCH]
#define nprefix         m[ASS86_NPREFIX]
#define seg_prefix_flag m[ASS86_SEGPREFIXFLAG]
#define lock_flag       m[ASS86_LOCKFLAG]
#define rep_flag        m[ASS86_REPFLAG]
#define mod_bits        m[ASS86_MODBITS]
#define reg_bits        m[ASS86_REGBITS]
#define rm_bits         m[ASS86_RMBITS]
#define op_type         m[ASS86_OPTYPE]
#define mnemonic_index  m[ASS86_MNEMONICINDEX]
#define instr1          m[ASS86_INSTR1]
#define disp_len        m[ASS86_DISPLEN]
#define TOKEN(i)        m[ASS86_TOKEN + (i)]
#define OP1(f)          m[ASS86_OP1 + (f)]
#define OP2(f)          m[ASS86_OP2 + (f)]
#define OP(f)           m[(uint16_t)(get16(ASS86_OPBASE) + (f))] /* op, based op$base */

static uint16_t get16(uint16_t off) { return rd16(m + off); }
static void set16(uint16_t off, uint16_t v) { wr16(m + off, v); }
#define assem_offset() get16(ASS86_ASSEMPTR)
#define token_word()   get16(ASS86_TOKEN)
#define OP_W(base, f)  get16((uint16_t)((base) + (f)))

static uint8_t rel(int c) { return c ? 0xFF : 0; }

/* ---- the DDT86 links ---- */

static void conout(uint8_t c) { ddt_conout(g_ds, c); }

static void crlf(void) {
    conout(CR);
    conout(LF);
}

static void getline(void) {
    ddt_getline(g_ds);
    next_ch = 0;
}

static void error(void) __attribute__((noreturn));
static void error(void) {
    crlf();
    conout('?');
    ddt_asment(g_ds);
}

static void ambig(void) __attribute__((noreturn));
static void ambig(void) {
    dis86_printm(g_ds, ASS86_ARR1);
    ddt_asment(g_ds);
}

static void check_ambig(void) {
    if ((OP1(OP_W_BIT) & OP2(OP_W_BIT)) == 0xFF) ambig();
    if ((OP1(OP_W_BIT) ^ OP2(OP_W_BIT)) == 1) error();
}

static uint8_t compare(uint16_t a, uint16_t b, uint8_t c) {
    while ((c = (uint8_t)(c - 1)) != 255) {
        if (m[a] != m[b]) return 0;
        a++;
        b++;
    }
    return PL_TRUE;
}

/* ---- token scanning ---- */

static uint8_t delimiter(uint8_t b) {
    for (uint8_t i = 0; i <= NUM_DELIMS - 1; i++)
        if (m[ASS86_DELIMITERS + i] == b) return PL_TRUE;
    return 0;
}

static uint8_t number(uint8_t b) { return rel((uint8_t)(b - '0') <= 9) | rel((uint8_t)(b - 'A') <= 5); }

static uint8_t hex(uint8_t b) {
    if ((uint8_t)(b - '0') <= 9) return (uint8_t)(b - '0');
    return (uint8_t)(b - 'A' + 10);
}

static void get_token(void) {
    uint8_t numeric;
    set16(ASS86_TOKENVALUE, 0);
    numeric = PL_TRUE;
    token_len = 0;
    if (next_ch == 0) next_ch = ddt_conin(g_ds);
    for (;;) {
        TOKEN(token_len) = next_ch;
        if ((token_len = (uint8_t)(token_len + 1)) >= 32) error();
        if (delimiter(next_ch) & 1) {
            if (token_len == 1) {
                token_type = DELIM_TYPE;
                next_ch = 0;
            } else {
                token_len = (uint8_t)(token_len - 1);
                token_type = (numeric & 1) ? NUMBER_TYPE : STRING_TYPE;
            }
            return;
        }
        if (numeric & 1) {
            if (number(next_ch) & 1)
                set16(ASS86_TOKENVALUE, (uint16_t)(get16(ASS86_TOKENVALUE) << 4) | hex(next_ch));
            else
                numeric = 0;
        }
        next_ch = ddt_conin(g_ds);
    }
}

static void get_real_token(void) {
    for (;;) {
        get_token();
        if ((rel(TOKEN(0) != ' ') & rel(TOKEN(0) != TAB)) & 1) return;
    }
}

/* ---- operand scanning ---- */

static uint8_t look_up_reg(uint16_t reg_tab_ptr, uint8_t tab_len, uint16_t reg_num_ptr) {
    for (uint8_t i = 0; i <= (uint8_t)(tab_len - 1); i++) {
        if (token_word() == get16(reg_tab_ptr)) {
            m[reg_num_ptr] = i;
            return PL_TRUE;
        }
        reg_tab_ptr = (uint16_t)(reg_tab_ptr + 2);
    }
    return 0;
}

static uint16_t op_field(uint8_t f) { return (uint16_t)(get16(ASS86_OPBASE) + f); }

static uint8_t check_base_reg(void) { return look_up_reg(ASS86_BASEREG, 2, op_field(OP_BASE_REG_NUM)); }

static uint8_t check_index_reg(void) { return look_up_reg(ASS86_INDEXREG, 2, op_field(OP_INDEX_REG_NUM)); }

static uint8_t check_seg_reg(void) {
    if (look_up_reg(DIS86_SEGREG, 4, op_field(OP_SEG_REG_NUM)) & 1) {
        OP(OP_SEG_REG_TYPE) = PL_TRUE;
        return PL_TRUE;
    }
    return 0;
}

static uint8_t check_reg(uint16_t reg_tab, uint8_t type_field, uint8_t sub_type, uint8_t w) {
    if (look_up_reg(reg_tab, 8, op_field(OP_REG_NUM)) & 1) {
        OP(type_field) = OP(OP_MODRM_TYPE) = PL_TRUE;
        if (OP(OP_REG_NUM) == 0) OP(OP_ACC_TYPE) = PL_TRUE;
        OP(OP_SUB_TYPE) = sub_type;
        OP(OP_W_BIT) = w;
        return PL_TRUE;
    }
    return 0;
}

static uint8_t check_reg8(void) { return check_reg(DIS86_REG8, OP_REG8_TYPE, 1, 0); }

static uint8_t check_reg16(void) { return check_reg(DIS86_REG16, OP_REG16_TYPE, 2, 1); }

static uint8_t check_bw_prefix(void) {
    if (token_type == STRING_TYPE) {
        if (token_word() == 0x5942) /* 'YB' */
            OP(OP_BW_PREFIX) = 0;
        else if (token_word() == 0x4F57) /* 'OW' */
            OP(OP_BW_PREFIX) = 1;
        if (OP(OP_BW_PREFIX) != 0xFF) return PL_TRUE;
    }
    return 0;
}

static uint8_t check_seg_prefix(void) {
    uint8_t r = check_seg_reg();
    return r & rel(next_ch == ':');
}

static void set_prefix(uint8_t b) {
    m[ASS86_PREFIX + nprefix] = b;
    nprefix = (uint8_t)(nprefix + 1);
}

static void set_seg_prefix(void) {
    if (seg_prefix_flag & 1) error();
    set_prefix((uint8_t)(0x26 | (OP(OP_SEG_REG_NUM) << 3)));
    seg_prefix_flag = PL_TRUE;
    get_token(); /* eat ':' */
    OP(OP_SEG_REG_TYPE) = 0; /* operand is not a seg reg */
}

static void get_prefix(void) {
    for (;;) {
        get_real_token();
        if (token_type == DELIM_TYPE) {
            if (TOKEN(0) == CR) OP(OP_NULL_TYPE) = PL_TRUE;
            return;
        }
        if (check_seg_prefix() & 1)
            set_seg_prefix();
        else if (check_bw_prefix() & 1) {
            if (OP(OP_W_BIT) != 0xFF)
                error();
            else
                OP(OP_W_BIT) = OP(OP_BW_PREFIX);
        } else
            return; /* must not be a prefix */
    }
}

static void reg_indirect(uint8_t x) {
    OP(OP_MODRM_TYPE) = PL_TRUE;
    if (check_base_reg() & 1) {
        get_token();
        if (TOKEN(0) == m[ASS86_RIGHTBRACKET]) {
            OP(OP_MODRM_TYPE) = PL_TRUE;
            OP(OP_SUB_TYPE) = (uint8_t)(7 - x);
            return;
        } else if (TOKEN(0) == '+') {
            OP(OP_SUB_TYPE) = (uint8_t)(9 - x);
            get_token();
        } else
            error();
    } else
        OP(OP_SUB_TYPE) = (uint8_t)(8 - x);
    if (check_index_reg() & 1) {
        get_token();
        if (TOKEN(0) == m[ASS86_RIGHTBRACKET]) return;
    }
    error();
}

static void get_operand(void) {
    for (uint8_t f = OP_SEG_REG_TYPE; f <= OP_MODRM_TYPE; f++) OP(f) = 0;
    OP(OP_BW_PREFIX) = OP(OP_W_BIT) = 0xFF; /* indeterminate */
    get_prefix();
    if (OP(OP_NULL_TYPE) & 1) return;
    if (check_seg_reg() & 1) return;
    if (check_reg8() & 1) return;
    if (check_reg16() & 1) return;
    if (token_type == NUMBER_TYPE) {
        set16(op_field(OP_VALUE), get16(ASS86_TOKENVALUE));
        if (next_ch == m[ASS86_LEFTBRACKET]) {
            get_token(); /* eat '[' */
            get_token();
            reg_indirect(0);
        } else if (next_ch == ':') {
            set16(op_field(OP_SEG_VALUE), get16(ASS86_TOKENVALUE));
            get_token(); /* eat ':' */
            get_token();
            if (token_type != NUMBER_TYPE) error();
            set16(op_field(OP_VALUE), get16(ASS86_TOKENVALUE));
            OP(OP_FAR_TYPE) = PL_TRUE;
        } else
            OP(OP_NUM_TYPE) = PL_TRUE;
    } else {
        if (TOKEN(0) != m[ASS86_LEFTBRACKET]) error();
        get_token();
        if (token_type == NUMBER_TYPE) {
            set16(op_field(OP_VALUE), get16(ASS86_TOKENVALUE));
            get_token();
            if (TOKEN(0) == m[ASS86_RIGHTBRACKET]) {
                OP(OP_SUB_TYPE) = 3;
                OP(OP_MODRM_TYPE) = PL_TRUE;
            } else
                error();
        } else
            reg_indirect(3);
    }
}

static void get_operand1(void) {
    set16(ASS86_OPBASE, ASS86_OP1);
    get_operand();
    get_token();
    if (TOKEN(0) != ',') error();
}

static void get_operand2(void) {
    set16(ASS86_OPBASE, ASS86_OP2);
    get_operand();
    if (OP(OP_NULL_TYPE) & 1) return;
    get_token();
    if (TOKEN(0) != CR) error();
}

/* ---- opcode scanning ---- */

static void lookup_optab(void) {
    for (uint16_t i = 0; i <= ASMTAB_OP_TAB_LEN - 1; i += 4) {
        if (m[ASMTAB_OPTAB + i] == mnemonic_index) {
            op_type = m[ASMTAB_OPTAB + i + 1];
            instr1 = m[ASMTAB_OPTAB + i + 2];
            reg_bits = m[ASMTAB_OPTAB + i + 3];
            return;
        }
    }
    error(); /* should never get here */
}

static uint8_t valid_mnemonic(void) {
    uint8_t n = (uint8_t)(token_len - 2);
    if (n > 4) error();
    uint16_t table_base = get16((uint16_t)(DIS86_TABPTRS + 2 * n));
    uint8_t last = (uint8_t)(m[DIS86_NOPS + n] - 1);
    for (uint16_t i = 0; i <= last; i++) {
        if (compare(ASS86_TOKEN, table_base, token_len) & 1) {
            mnemonic_index = (uint8_t)(i + m[DIS86_OPNIN + n]);
            return PL_TRUE;
        }
        table_base = (uint16_t)(table_base + token_len);
    }
    return 0;
}

static uint8_t get_opcode(void) {
    nprefix = 0;
    rep_flag = lock_flag = seg_prefix_flag = 0;
    for (;;) {
        get_real_token();
        if (((rel(TOKEN(0) == CR) | rel(TOKEN(0) == '.')) & 1)) return 0; /* no opcode */
        if (check_seg_prefix() & 1)
            set_seg_prefix();
        else if (valid_mnemonic() & 1) {
            lookup_optab();
            if (op_type == 0xFF) {
                if (lock_flag & 1) error();
                lock_flag = PL_TRUE;
                set_prefix(instr1);
            } else if (op_type == 0xFE) { /* repeat */
                if (rep_flag & 1) error();
                rep_flag = PL_TRUE;
                set_prefix(instr1);
            } else
                return PL_TRUE; /* opcode present */
        } else
            error();
    }
}

/* ---- forming and output of instructions ---- */

static void put_mem(uint8_t b) {
    ddt_set(g_ds, assem_offset(), b);
    set16(ASS86_ASSEMPTR, (uint16_t)(assem_offset() + 1));
}

static void put_prefix(void) {
    uint8_t i = 0;
    while ((nprefix = (uint8_t)(nprefix - 1)) != 0xFF) {
        put_mem(m[ASS86_PREFIX + i]);
        i++;
    }
}

static void put_instr(void) {
    put_prefix();
    put_mem(instr1);
}

static void set_instr(uint8_t b) {
    instr1 = b;
    put_instr();
}

static void put_word(uint16_t a) {
    put_mem((uint8_t)a);
    put_mem((uint8_t)(a >> 8));
}

static void set_w_bit(void) {
    if ((OP1(OP_W_BIT) & OP2(OP_W_BIT)) == 1) instr1 |= 1;
}

static void set_mod(void) {
    if (OP_W(get16(ASS86_OPBASE), OP_VALUE) > 0x7F)
        mod_bits = disp_len = 2;
    else
        mod_bits = disp_len = 1;
}

static void set_modrm(void) {
    disp_len = 0;
    set16(ASS86_DISPWORD, OP_W(get16(ASS86_OPBASE), OP_VALUE));
    switch ((uint8_t)(OP(OP_SUB_TYPE) - 1)) {
    case 0: /* 1: reg8 */
    case 1: /* 2: reg16 */
        mod_bits = 3;
        rm_bits = OP(OP_REG_NUM);
        break;
    case 2: /* 3: [num] */
        mod_bits = 0;
        rm_bits = 6;
        disp_len = 2;
        break;
    case 3: /* 4: [pointer reg] */
        if (OP(OP_BASE_REG_NUM) == 1) { /* BP */
            mod_bits = 1;
            rm_bits = 6;
            disp_len = 1;
            set16(ASS86_DISPWORD, 0);
        } else {
            mod_bits = 0;
            rm_bits = 7;
        }
        break;
    case 4: /* 5: [index reg] */
        mod_bits = 0;
        rm_bits = (uint8_t)(OP(OP_INDEX_REG_NUM) + 4);
        break;
    case 5: /* 6: [pointer reg + index reg] */
        mod_bits = 0;
        rm_bits = (uint8_t)(OP(OP_BASE_REG_NUM) * 2 + OP(OP_INDEX_REG_NUM));
        break;
    case 6: /* 7: num [pointer reg] */
        set_mod();
        rm_bits = (uint8_t)(7 - OP(OP_BASE_REG_NUM));
        break;
    case 7: /* 8: num [index reg] */
        set_mod();
        rm_bits = (uint8_t)(OP(OP_INDEX_REG_NUM) + 4);
        break;
    case 8: /* 9: num [pointer reg + index reg] */
        set_mod();
        rm_bits = (uint8_t)(OP(OP_BASE_REG_NUM) * 2 + OP(OP_INDEX_REG_NUM));
        break;
    default: /* (no other sub$type reaches here) */
        break;
    }
}

static void do_modrm(uint16_t op_ptr) {
    set16(ASS86_OPBASE, op_ptr);
    set_modrm();
    put_instr();
    put_mem((uint8_t)((mod_bits << 6) | (reg_bits << 3) | rm_bits));
    if (disp_len > 0) put_mem((uint8_t)get16(ASS86_DISPWORD));
    if (disp_len > 1) put_mem((uint8_t)(get16(ASS86_DISPWORD) >> 8));
}

static void modrm_w(uint8_t ins, uint8_t reg, uint16_t op_ptr) {
    set16(ASS86_OPBASE, op_ptr);
    if (OP(OP_MODRM_TYPE) & 1) {
        if (OP(OP_W_BIT) == 0xFF) ambig();
        instr1 = ins | OP(OP_W_BIT);
        reg_bits = reg;
        do_modrm(op_ptr);
    } else
        error();
}

static void modrm_16(uint8_t ins, uint8_t reg) {
    if ((OP2(OP_MODRM_TYPE) & (uint8_t)~OP2(OP_REG8_TYPE)) & 1) {
        instr1 = ins;
        reg_bits = reg;
        do_modrm(ASS86_OP2);
    } else
        error();
}

static void modregrm(void) {
    check_ambig();
    if (OP1(OP_SUB_TYPE) <= 2) { /* first op is register */
        reg_bits = OP1(OP_REG_NUM);
        do_modrm(ASS86_OP2);
    } else {
        instr1 |= OP2(OP_W_BIT);
        reg_bits = OP2(OP_REG_NUM);
        do_modrm(ASS86_OP1);
    }
}

static void modregrm_w(void) {
    set_w_bit();
    modregrm();
}

static void modregrm_dw(void) {
    if ((OP1(OP_MODRM_TYPE) & rel(OP1(OP_SUB_TYPE) <= 2)) & 1) instr1 |= 2; /* set d bit */
    modregrm_w();
}

static uint16_t op2_value(void) { return OP_W(ASS86_OP2, OP_VALUE); }

static void put_immed(void) {
    put_mem((uint8_t)op2_value());
    if (OP1(OP_W_BIT) & 1) put_mem((uint8_t)(op2_value() >> 8));
}

static void put_immed_s(void) {
    put_mem((uint8_t)op2_value());
    if ((instr1 & 3) == 1) put_mem((uint8_t)(op2_value() >> 8));
}

static void check_w_and_val(void) {
    if ((rel(OP1(OP_W_BIT) == 0) & rel(op2_value() > 255)) & 1) error();
    if (OP1(OP_W_BIT) == 0xFF) {
        if (op2_value() >= 256)
            OP1(OP_W_BIT) = 1;
        else
            ambig();
    }
}

static void acc_immed(void) {
    check_w_and_val();
    set_w_bit();
    put_instr();
    put_immed();
}

static void modrm_immed(void) {
    check_w_and_val();
    set_w_bit();
    do_modrm(ASS86_OP1);
    put_immed();
}

static void set_instr_w(uint8_t b) {
    if (OP2(OP_SUB_TYPE) <= 2)
        instr1 = b | OP2(OP_W_BIT);
    else
        instr1 = b | OP1(OP_W_BIT);
}

static uint8_t check_acc_immed(void) { return OP1(OP_ACC_TYPE) & OP2(OP_NUM_TYPE); }

static uint8_t check_modrm_immed(void) { return OP1(OP_MODRM_TYPE) & OP2(OP_NUM_TYPE); }

static uint8_t check_modrm_modrm(void) {
    return OP1(OP_MODRM_TYPE) & OP2(OP_MODRM_TYPE) &
           (uint8_t)~(rel(OP1(OP_SUB_TYPE) > 2) & rel(OP2(OP_SUB_TYPE) > 2));
}

/* ---- mnemonic processing ---- */

/* simple one-byte instructions: no operands */
static void type1(void) {
    get_operand2();
    if (OP2(OP_NULL_TYPE) & 1)
        put_instr();
    else
        error();
}

/* aad aam: no operands; xxxxxxxx 00001010 */
static void type2(void) {
    get_operand2();
    if (OP2(OP_NULL_TYPE) & 1) {
        put_instr();
        put_mem(0x0A);
    } else
        error();
}

/* conditional jumps, loopxx, jmps: a number; xxxxxxxx IP-INC8 */
static void type3(void) {
    get_operand2();
    if ((OP2(OP_NUM_TYPE) & rel((uint16_t)(op2_value() - (uint16_t)(assem_offset() - 0x7E)) <= 255)) & 1) {
        put_instr();
        put_mem((uint8_t)(op2_value() - (uint16_t)(assem_offset() + 1)));
    } else
        error();
}

/* shifts and rotates: 2 operands; xxxxxxvw mmxxxr/m [disp] */
static void type4(void) {
    get_operand1();
    get_operand2();
    if (OP1(OP_MODRM_TYPE) & 1) {
        if ((OP2(OP_REG8_TYPE) & rel(OP2(OP_REG_NUM) == 1)) & 1)
            instr1 |= 2; /* set v bit */
        else if ((uint8_t)~(OP2(OP_NUM_TYPE) & rel(op2_value() == 1)) & 1)
            error();
        modrm_w(instr1, reg_bits, ASS86_OP1);
    } else
        error();
}

/* div idiv mul imul not neg: 1 operand; xxxxxxxw mmxxxr/m [disp] */
static void type5(void) {
    get_operand2();
    modrm_w(instr1, reg_bits, ASS86_OP2);
}

/* les lds lea: reg16, memory; xxxxxxxx mmregr/m [disp] */
static void type6(void) {
    get_operand1();
    get_operand2();
    if ((OP1(OP_REG16_TYPE) & OP2(OP_MODRM_TYPE) & rel(OP2(OP_SUB_TYPE) > 2)) & 1)
        modregrm();
    else
        error();
}

#define TYPE7(i, j)  m[(uint16_t)(ASS86_TYPE7STRUC + (i) * 2 + (j))]
#define TYPE8(i, j)  m[(uint16_t)(ASS86_TYPE8STRUC + (i) * 4 + (j))]
#define TYPE11(i, j) m[(uint16_t)(ASS86_TYPE11STRUC + (i) * 2 + (j))]
#define CJ(i, j)     m[(uint16_t)(ASS86_CJSTRUC + (i) * 2 + (j))]

/* inc dec: 1 operand; xxxxxreg, or xxxxxxxw mmxxxr/m [disp] */
static void type7(void) {
    get_operand2();
    if (OP2(OP_REG16_TYPE) & 1)
        set_instr(TYPE7(instr1, 0) | OP2(OP_REG_NUM));
    else
        modrm_w(0xFE, TYPE7(instr1, 1), ASS86_OP2);
}

static void type8or8a(void) {
    if (OP2(OP_SEG_REG_TYPE) & 1)
        set_instr((uint8_t)(TYPE8(instr1, 0) | (OP2(OP_SEG_REG_NUM) << 3)));
    else if (OP2(OP_REG16_TYPE) & 1)
        set_instr(TYPE8(instr1, 1) | OP2(OP_REG_NUM));
    else
        modrm_16(TYPE8(instr1, 2), TYPE8(instr1, 3));
}

/* push: xxxSRxxx, xxxxxreg, or xxxxxxxx mmxxxr/m [disp] */
static void type8(void) {
    get_operand2();
    type8or8a();
}

/* pop: the same, but not CS */
static void type8a(void) {
    get_operand2();
    if ((OP2(OP_SEG_REG_TYPE) & rel(OP2(OP_SEG_REG_NUM) == 1)) & 1)
        error();
    else
        type8or8a();
}

/* add adc cmp sub sbb (9), and or xor (10): 2 operands */
static void type9or10(uint8_t type) {
    get_operand1();
    get_operand2();
    if (check_acc_immed() & 1) {
        instr1 |= 4;
        acc_immed();
    } else if (check_modrm_immed() & 1) {
        instr1 = 0x80;
        if (type == 9) {
            if ((rel(op2_value() < 0x80) | rel(op2_value() >= 0xFF80)) & 1) instr1 |= 2;
            check_w_and_val();
            set_w_bit();
            do_modrm(ASS86_OP1);
            put_immed_s();
        } else
            modrm_immed();
    } else if (check_modrm_modrm() & 1)
        modregrm_dw();
    else
        error();
}

/* call jmp: 1 operand; xxxxxxxx IP-INC16, or xxxxxxxx mmxxxr/m [disp] */
static void type11(void) {
    get_operand2();
    if (OP2(OP_NUM_TYPE) & 1) {
        set_instr(TYPE11(instr1, 0));
        put_word((uint16_t)(op2_value() - (uint16_t)(assem_offset() + 2)));
    } else
        modrm_16(0xFF, TYPE11(instr1, 1));
}

static void type_int(void) {
    get_operand2();
    if ((OP2(OP_NUM_TYPE) & rel(op2_value() <= 255)) & 1) {
        if (op2_value() == 3)
            set_instr(0xCC);
        else {
            set_instr(0xCD);
            put_mem((uint8_t)op2_value());
        }
    } else
        error();
}

static void type_esc(void) {
    get_operand1();
    get_operand2();
    uint16_t v = OP_W(ASS86_OP1, OP_VALUE);
    if ((OP1(OP_NUM_TYPE) & rel(v < 64) & OP2(OP_MODRM_TYPE)) & 1) {
        instr1 |= (uint8_t)((uint8_t)v >> 3);
        reg_bits = (uint8_t)v & 7;
        do_modrm(ASS86_OP2);
    } else
        error();
}

static void type_ret_retf(void) {
    set16(ASS86_OPBASE, ASS86_OP2);
    get_operand();
    if (OP2(OP_NULL_TYPE) & 1)
        set_instr(instr1 | 1);
    else if (OP2(OP_NUM_TYPE) & 1) {
        put_instr();
        put_word(op2_value());
    } else
        error();
}

static void type_in(void) {
    get_operand1();
    if ((uint8_t)~OP1(OP_ACC_TYPE) & 1) error();
    get_operand2();
    if ((OP2(OP_REG16_TYPE) & rel(OP2(OP_REG_NUM) == 2)) & 1) /* DX */
        set_instr(instr1 | 8 | OP1(OP_W_BIT));
    else if ((OP2(OP_NUM_TYPE) & rel(op2_value() <= 255)) & 1) {
        set_instr(instr1 | OP1(OP_W_BIT)); /* IN AX,port: E5 (it was put_instr: E4, IN AL; bugs.md #137) */
        put_mem((uint8_t)op2_value());
    } else
        error();
}

static void type_out(void) {
    uint8_t temp = 0;
    get_operand1();
    uint16_t v = OP_W(ASS86_OP1, OP_VALUE);
    if ((OP1(OP_REG16_TYPE) & rel(OP1(OP_REG_NUM) == 2)) & 1) /* DX */
        temp = 1;
    else if ((OP1(OP_NUM_TYPE) & rel(v <= 255)) & 1)
        temp = 0;
    else
        error();
    get_operand2();
    if ((uint8_t)~OP2(OP_ACC_TYPE) & 1) error();
    put_mem((uint8_t)(instr1 | (temp << 3) | OP2(OP_W_BIT)));
    if (temp == 0) put_mem((uint8_t)OP_W(ASS86_OP1, OP_VALUE));
}

static void type_callf_jmpf(void) {
    get_operand2();
    if (OP2(OP_FAR_TYPE) & 1) {
        set_instr(CJ(instr1, 0));
        put_word(op2_value());
        put_word(OP_W(ASS86_OP2, OP_SEG_VALUE));
    } else
        modrm_16(0xFF, CJ(instr1, 1));
}

static void type_test(void) {
    get_operand1();
    get_operand2();
    if (check_acc_immed() & 1) {
        instr1 = 0xA8;
        acc_immed();
    } else if (check_modrm_immed() & 1) {
        instr1 = 0xF6;
        modrm_immed();
    } else if (check_modrm_modrm() & 1) {
        set_instr_w(0x84);
        modregrm();
    } else
        error();
}

static void type_xchg(void) {
    get_operand1();
    get_operand2();
    if ((OP1(OP_ACC_TYPE) & OP1(OP_W_BIT) & OP2(OP_REG16_TYPE)) & 1)
        set_instr((uint8_t)(0x90 + OP2(OP_REG_NUM)));
    else if (check_modrm_modrm() & 1) {
        set_instr_w(0x86);
        modregrm();
    } else
        error();
}

static void type_mov(void) {
    get_operand1();
    get_operand2();
    if ((OP1(OP_SEG_REG_TYPE) & OP2(OP_MODRM_TYPE) & (uint8_t)~OP2(OP_REG8_TYPE)) & 1) {
        instr1 = 0x8E;
        reg_bits = OP1(OP_SEG_REG_NUM);
        do_modrm(ASS86_OP2);
    } else if ((OP1(OP_MODRM_TYPE) & (uint8_t)~rel(OP1(OP_SUB_TYPE) == 1) & OP2(OP_SEG_REG_TYPE)) & 1) {
        instr1 = 0x8C;
        reg_bits = OP2(OP_SEG_REG_NUM);
        do_modrm(ASS86_OP1);
    } else if ((OP1(OP_ACC_TYPE) & OP2(OP_MODRM_TYPE) & rel(OP2(OP_SUB_TYPE) == 3)) & 1) {
        instr1 = 0xA0 | OP1(OP_W_BIT);
        put_instr();
        put_word(op2_value());
    } else if ((OP2(OP_ACC_TYPE) & OP1(OP_MODRM_TYPE) & rel(OP1(OP_SUB_TYPE) == 3)) & 1) {
        instr1 = 0xA2 | OP2(OP_W_BIT);
        put_instr();
        put_word(OP_W(ASS86_OP1, OP_VALUE));
    } else if (check_modrm_immed() & 1) {
        if (OP1(OP_SUB_TYPE) <= 2) {
            set_instr((uint8_t)(0xB0 | (OP1(OP_W_BIT) << 3) | OP1(OP_REG_NUM)));
            put_immed();
        } else {
            instr1 = 0xC6;
            modrm_immed();
        }
    } else if (check_modrm_modrm() & 1) {
        instr1 = 0x88;
        modregrm_dw();
    } else
        error();
}

/* asm: procedure (loc) address public -- assemble a line at seg:off */
uint16_t ass86_asm(uint16_t ds, uint16_t off, uint16_t seg) {
    g_ds = ds;
    m = far_ptr(rt_phys(), ds, 0);
    set16(ASS86_ASSEMPTR, off);     /* assem$ptr = loc */
    set16(ASS86_ASSEMPTR + 2, seg);
    crlf();
    dis86_print_word(ds, get16(ASS86_ASSEMPTR + 2));
    conout(':');
    dis86_print_word(ds, assem_offset());
    conout(' ');
    getline();
    set16(ASS86_OPBASE, ASS86_OP1); /* must be set for prefix scan */
    if (get_opcode() & 1) {
        if (op_type > 21) error();
        switch (op_type) {
        case 0: error();
        case 1: type1(); break;
        case 2: type2(); break;
        case 3: type3(); break;
        case 4: type4(); break;
        case 5: type5(); break;
        case 6: type6(); break;
        case 7: type7(); break;
        case 8: type8(); break;
        case 9: type9or10(9); break;
        case 10: type9or10(10); break;
        case 11: type11(); break;
        case 12: type_int(); break;
        case 13: type_esc(); break;
        case 14: type_ret_retf(); break;
        case 15: type_in(); break;
        case 16: type_out(); break;
        case 17: type_callf_jmpf(); break;
        case 18: type_test(); break;
        case 19: type_xchg(); break;
        case 20: type_mov(); break;
        case 21: type8a(); break;
        }
    } else
        put_prefix(); /* may be prefix without opcode */
    return assem_offset();
}
