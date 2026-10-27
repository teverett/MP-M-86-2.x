/*
 * ins86.c -- C version of src-assem/toolchain/INS86.PLM: the 8086
 * disassembler's instruction tables (DDT86's disassembler overlay, with
 * dis86.c). Two bytes an opcode: the mnemonic's index into DIS86's
 * ASCII opcode tables (OPTAB.LIT's numbering), and the instruction type
 * (how many operands of what kind, dis86.c's type cases); prefixes have
 * type 0FFh, and the ModRM-reg-selected groups have type 26 and an
 * alt$table number in place of the mnemonic.
 *
 * The tables are INS86's initialized data, which the build puts after
 * the overlay's code (toolchain_syms.h's INS86_*); ins86_image, below,
 * is those bytes, and ins86_install puts them there. dis86.c reads them
 * from memory, as DIS86.PLM's externals do.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "toolchain.h"
#include "toolchain_syms.h"

/* OPTAB.LIT: the mnemonics' indexes, by length (DIS86's ops2..ops6) */
enum { OP2_IN = 0, OP3_IN = 12, OP4_IN = 81, OP5_IN = 106, OP6_IN = 122 };
enum {
    IN_IN = OP2_IN + 0,
    JA_IN = OP2_IN + 1,
    JB_IN = OP2_IN + 2,
    JC_IN = OP2_IN + 3,
    JE_IN = OP2_IN + 4,
    JG_IN = OP2_IN + 5,
    JL_IN = OP2_IN + 6,
    JO_IN = OP2_IN + 7,
    JP_IN = OP2_IN + 8,
    JS_IN = OP2_IN + 9,
    JZ_IN = OP2_IN + 10,
    OR_IN = OP2_IN + 11,
    AAA_IN = OP3_IN + 0,
    AAD_IN = OP3_IN + 1,
    AAM_IN = OP3_IN + 2,
    AAS_IN = OP3_IN + 3,
    ADC_IN = OP3_IN + 4,
    ADD_IN = OP3_IN + 5,
    AND_IN = OP3_IN + 6,
    CBW_IN = OP3_IN + 7,
    CLC_IN = OP3_IN + 8,
    CLD_IN = OP3_IN + 9,
    CLI_IN = OP3_IN + 10,
    CMC_IN = OP3_IN + 11,
    CMP_IN = OP3_IN + 12,
    CS_IN = OP3_IN + 13,
    CWD_IN = OP3_IN + 14,
    DAA_IN = OP3_IN + 15,
    DAS_IN = OP3_IN + 16,
    DEC_IN = OP3_IN + 17,
    DIV_IN = OP3_IN + 18,
    DS_IN = OP3_IN + 19,
    ES_IN = OP3_IN + 20,
    ESC_IN = OP3_IN + 21,
    HLT_IN = OP3_IN + 22,
    INC_IN = OP3_IN + 23,
    INT_IN = OP3_IN + 24,
    JAE_IN = OP3_IN + 25,
    JBE_IN = OP3_IN + 26,
    JGE_IN = OP3_IN + 27,
    JLE_IN = OP3_IN + 28,
    JMP_IN = OP3_IN + 29,
    JNA_IN = OP3_IN + 30,
    JNB_IN = OP3_IN + 31,
    JNC_IN = OP3_IN + 32,
    JNE_IN = OP3_IN + 33,
    JNG_IN = OP3_IN + 34,
    JNL_IN = OP3_IN + 35,
    JNO_IN = OP3_IN + 36,
    JNP_IN = OP3_IN + 37,
    JNS_IN = OP3_IN + 38,
    JNZ_IN = OP3_IN + 39,
    JPE_IN = OP3_IN + 40,
    JPO_IN = OP3_IN + 41,
    LDS_IN = OP3_IN + 42,
    LEA_IN = OP3_IN + 43,
    LES_IN = OP3_IN + 44,
    MOV_IN = OP3_IN + 45,
    MUL_IN = OP3_IN + 46,
    NEG_IN = OP3_IN + 47,
    NOP_IN = OP3_IN + 48,
    NOT_IN = OP3_IN + 49,
    OUT_IN = OP3_IN + 50,
    POP_IN = OP3_IN + 51,
    RCL_IN = OP3_IN + 52,
    RCR_IN = OP3_IN + 53,
    REP_IN = OP3_IN + 54,
    RET_IN = OP3_IN + 55,
    ROL_IN = OP3_IN + 56,
    ROR_IN = OP3_IN + 57,
    SAL_IN = OP3_IN + 58,
    SAR_IN = OP3_IN + 59,
    SBB_IN = OP3_IN + 60,
    SHL_IN = OP3_IN + 61,
    SHR_IN = OP3_IN + 62,
    SS_IN = OP3_IN + 63,
    STC_IN = OP3_IN + 64,
    STD_IN = OP3_IN + 65,
    STI_IN = OP3_IN + 66,
    SUB_IN = OP3_IN + 67,
    XOR_IN = OP3_IN + 68,
    CALL_IN = OP4_IN + 0,
    IDIV_IN = OP4_IN + 1,
    IMUL_IN = OP4_IN + 2,
    INTO_IN = OP4_IN + 3,
    IRET_IN = OP4_IN + 4,
    JCXZ_IN = OP4_IN + 5,
    JMPF_IN = OP4_IN + 6,
    JMPS_IN = OP4_IN + 7,
    JNAE_IN = OP4_IN + 8,
    JNBE_IN = OP4_IN + 9,
    JNGE_IN = OP4_IN + 10,
    JNLE_IN = OP4_IN + 11,
    LAHF_IN = OP4_IN + 12,
    LOCK_IN = OP4_IN + 13,
    LOOP_IN = OP4_IN + 14,
    POPF_IN = OP4_IN + 15,
    PUSH_IN = OP4_IN + 16,
    REPE_IN = OP4_IN + 17,
    REPZ_IN = OP4_IN + 18,
    RETF_IN = OP4_IN + 19,
    SAHF_IN = OP4_IN + 20,
    TEST_IN = OP4_IN + 21,
    WAIT_IN = OP4_IN + 22,
    XCHG_IN = OP4_IN + 23,
    XLAT_IN = OP4_IN + 24,
    CALLF_IN = OP5_IN + 0,
    CMPSB_IN = OP5_IN + 1,
    CMPSW_IN = OP5_IN + 2,
    LODSB_IN = OP5_IN + 3,
    LODSW_IN = OP5_IN + 4,
    LOOPE_IN = OP5_IN + 5,
    LOOPZ_IN = OP5_IN + 6,
    MOVSB_IN = OP5_IN + 7,
    MOVSW_IN = OP5_IN + 8,
    PUSHF_IN = OP5_IN + 9,
    REPNE_IN = OP5_IN + 10,
    REPNZ_IN = OP5_IN + 11,
    SCASB_IN = OP5_IN + 12,
    SCASW_IN = OP5_IN + 13,
    STOSB_IN = OP5_IN + 14,
    STOSW_IN = OP5_IN + 15,
    LOOPNE_IN = OP6_IN + 0,
    LOOPNZ_IN = OP6_IN + 1,
};

/* not an instruction (type 0: "??="), and the alt tables, in the
 * mnemonic column of a type-26 entry */
enum { QQ_IN = 0, ALT1 = 0, ALT2, ALT3, ALT4, ALT5, ALT6, ALT7, ALT8 };

/* the instruction types: INS86's type$n literals, which share cases */
enum {
    T0 = 0, T1 = 1, T2 = 2, T3 = 3, T4 = 4, T5 = 5, T6 = 6, T7 = 7, T8 = 7, T9 = 7,
    T10 = 8, T11 = 9, T12 = 10, T13 = 11, T14 = 12, T15 = 12, T16 = 12, T17 = 12,
    T18 = 13, T19 = 13, T20 = 13, T21 = 13, T22 = 14, T23 = 15, T24 = 15, T25 = 16, T26 = 16,
    T27 = 17, T28 = 17, T29 = 17, T30 = 17, T31 = 18, T32 = 18, T33 = 19, T34 = 20, T35 = 20,
    T36 = 21, T37 = 21, T38 = 22, T39 = 23, T40 = 24, T41 = 24, T42 = 25, T43 = 26, T44 = 27, T45 = 28,
    PREFIX_TYPE = 0xFF,
};

typedef struct __attribute__((packed)) {
    uint8_t mnemonic, type;
} ins86_entry;

typedef struct __attribute__((packed)) {
    uint16_t alt_table_ptrs[8];   /* .alt$1$tab .. .alt$8$tab */
    ins86_entry alt_tab[8][8];    /* alt$1$tab .. alt$8$tab: by ModRM reg */
    ins86_entry instr_table[256]; /* by the first opcode byte */
} ins86_data;

_Static_assert(offsetof(ins86_data, alt_tab[0]) == INS86_ALT1TAB - INS86_ALTTABLEPTRS, "alt1tab");
_Static_assert(offsetof(ins86_data, alt_tab[1]) == INS86_ALT2TAB - INS86_ALTTABLEPTRS, "alt2tab");
_Static_assert(offsetof(ins86_data, alt_tab[2]) == INS86_ALT3TAB - INS86_ALTTABLEPTRS, "alt3tab");
_Static_assert(offsetof(ins86_data, alt_tab[3]) == INS86_ALT4TAB - INS86_ALTTABLEPTRS, "alt4tab");
_Static_assert(offsetof(ins86_data, alt_tab[4]) == INS86_ALT5TAB - INS86_ALTTABLEPTRS, "alt5tab");
_Static_assert(offsetof(ins86_data, alt_tab[5]) == INS86_ALT6TAB - INS86_ALTTABLEPTRS, "alt6tab");
_Static_assert(offsetof(ins86_data, alt_tab[6]) == INS86_ALT7TAB - INS86_ALTTABLEPTRS, "alt7tab");
_Static_assert(offsetof(ins86_data, alt_tab[7]) == INS86_ALT8TAB - INS86_ALTTABLEPTRS, "alt8tab");
_Static_assert(offsetof(ins86_data, instr_table) == INS86_INSTRTABLE - INS86_ALTTABLEPTRS, "instrtable");
_Static_assert(sizeof(ins86_data) == DIS86_OPS2 - INS86_ALTTABLEPTRS, "length");

static const ins86_data ins86_image = {
    .alt_table_ptrs = {INS86_ALT1TAB, INS86_ALT2TAB, INS86_ALT3TAB, INS86_ALT4TAB, INS86_ALT5TAB, INS86_ALT6TAB,
                       INS86_ALT7TAB, INS86_ALT8TAB},
    .alt_tab = {
        /* alt1 */ {{ADD_IN, T44}, {OR_IN, T34}, {ADC_IN, T44}, {SBB_IN, T44}, {AND_IN, T34}, {SUB_IN, T44}, {XOR_IN, T34}, {CMP_IN, T44}},
        /* alt2 */ {{ADD_IN, T44}, {QQ_IN, T0}, {ADC_IN, T44}, {SBB_IN, T44}, {QQ_IN, T0}, {SUB_IN, T44}, {QQ_IN, T0}, {CMP_IN, T44}},
        /* alt3 */ {{ADD_IN, T38}, {QQ_IN, T0}, {ADC_IN, T38}, {SBB_IN, T38}, {QQ_IN, T0}, {SUB_IN, T38}, {QQ_IN, T0}, {CMP_IN, T38}},
        /* alt4 */ {{ROL_IN, T29}, {ROR_IN, T29}, {RCL_IN, T29}, {RCR_IN, T29}, {SHL_IN, T29}, {SHR_IN, T29}, {QQ_IN, T0}, {SAR_IN, T29}},
        /* alt5 */ {{ROL_IN, T27}, {ROR_IN, T27}, {RCL_IN, T27}, {RCR_IN, T27}, {SHL_IN, T27}, {SHR_IN, T27}, {QQ_IN, T0}, {SAR_IN, T27}},
        /* alt6 */ {{TEST_IN, T34}, {QQ_IN, T0}, {NOT_IN, T45}, {NEG_IN, T45}, {MUL_IN, T45}, {IMUL_IN, T45}, {DIV_IN, T45}, {IDIV_IN, T45}},
        /* alt7 */ {{INC_IN, T45}, {DEC_IN, T45}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}},
        /* alt8 */ {{INC_IN, T45}, {DEC_IN, T45}, {CALL_IN, T9}, {CALLF_IN, T7}, {JMP_IN, T9}, {JMPF_IN, T7}, {PUSH_IN, T7}, {QQ_IN, T0}},
    },
    .instr_table = {
    /* 00 */ {ADD_IN, T14}, {ADD_IN, T15}, {ADD_IN, T16}, {ADD_IN, T17}, {ADD_IN, T23}, {ADD_IN, T24}, {PUSH_IN, T4}, {POP_IN, T4},
    /* 08 */ {OR_IN, T14}, {OR_IN, T15}, {OR_IN, T16}, {OR_IN, T17}, {OR_IN, T23}, {OR_IN, T24}, {PUSH_IN, T4}, {QQ_IN, T0},
    /* 10 */ {ADC_IN, T14}, {ADC_IN, T15}, {ADC_IN, T16}, {ADC_IN, T17}, {ADC_IN, T23}, {ADC_IN, T24}, {PUSH_IN, T4}, {POP_IN, T4},
    /* 18 */ {SBB_IN, T14}, {SBB_IN, T15}, {SBB_IN, T16}, {SBB_IN, T17}, {SBB_IN, T23}, {SBB_IN, T24}, {PUSH_IN, T4}, {POP_IN, T4},
    /* 20 */ {AND_IN, T14}, {AND_IN, T15}, {AND_IN, T16}, {AND_IN, T17}, {AND_IN, T23}, {AND_IN, T24}, {ES_IN, PREFIX_TYPE}, {DAA_IN, T1},
    /* 28 */ {SUB_IN, T14}, {SUB_IN, T15}, {SUB_IN, T16}, {SUB_IN, T17}, {SUB_IN, T23}, {SUB_IN, T24}, {CS_IN, PREFIX_TYPE}, {DAS_IN, T1},
    /* 30 */ {XOR_IN, T14}, {XOR_IN, T15}, {XOR_IN, T16}, {XOR_IN, T17}, {XOR_IN, T23}, {XOR_IN, T24}, {SS_IN, PREFIX_TYPE}, {AAA_IN, T1},
    /* 38 */ {CMP_IN, T14}, {CMP_IN, T15}, {CMP_IN, T16}, {CMP_IN, T17}, {CMP_IN, T23}, {CMP_IN, T24}, {DS_IN, PREFIX_TYPE}, {AAS_IN, T1},
    /* 40 */ {INC_IN, T3}, {INC_IN, T3}, {INC_IN, T3}, {INC_IN, T3}, {INC_IN, T3}, {INC_IN, T3}, {INC_IN, T3}, {INC_IN, T3},
    /* 48 */ {DEC_IN, T3}, {DEC_IN, T3}, {DEC_IN, T3}, {DEC_IN, T3}, {DEC_IN, T3}, {DEC_IN, T3}, {DEC_IN, T3}, {DEC_IN, T3},
    /* 50 */ {PUSH_IN, T3}, {PUSH_IN, T3}, {PUSH_IN, T3}, {PUSH_IN, T3}, {PUSH_IN, T3}, {PUSH_IN, T3}, {PUSH_IN, T3}, {PUSH_IN, T3},
    /* 58 */ {POP_IN, T3}, {POP_IN, T3}, {POP_IN, T3}, {POP_IN, T3}, {POP_IN, T3}, {POP_IN, T3}, {POP_IN, T3}, {POP_IN, T3},
    /* 60 */ {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0},
    /* 68 */ {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0}, {QQ_IN, T0},
    /* 70 */ {JO_IN, T5}, {JNO_IN, T5}, {JB_IN, T5}, {JNB_IN, T5}, {JZ_IN, T5}, {JNZ_IN, T5}, {JBE_IN, T5}, {JA_IN, T5},
    /* 78 */ {JS_IN, T5}, {JNS_IN, T5}, {JP_IN, T5}, {JNP_IN, T5}, {JL_IN, T5}, {JNL_IN, T5}, {JLE_IN, T5}, {JG_IN, T5},
    /* 80 */ {ALT1, T43}, {ALT1, T43}, {ALT2, T43}, {ALT2, T43}, {TEST_IN, T14}, {TEST_IN, T15}, {XCHG_IN, T16}, {XCHG_IN, T17},
    /* 88 */ {MOV_IN, T14}, {MOV_IN, T15}, {MOV_IN, T16}, {MOV_IN, T17}, {MOV_IN, T36}, {LEA_IN, T39}, {MOV_IN, T37}, {POP_IN, T9},
    /* 90 */ {NOP_IN, T1}, {XCHG_IN, T33}, {XCHG_IN, T33}, {XCHG_IN, T33}, {XCHG_IN, T33}, {XCHG_IN, T33}, {XCHG_IN, T33}, {XCHG_IN, T33},
    /* 98 */ {CBW_IN, T1}, {CWD_IN, T1}, {CALLF_IN, T13}, {WAIT_IN, T1}, {PUSHF_IN, T1}, {POPF_IN, T1}, {SAHF_IN, T1}, {LAHF_IN, T1},
    /* A0 */ {MOV_IN, T18}, {MOV_IN, T19}, {MOV_IN, T20}, {MOV_IN, T21}, {MOVSB_IN, T1}, {MOVSW_IN, T1}, {CMPSB_IN, T1}, {CMPSW_IN, T1},
    /* A8 */ {TEST_IN, T23}, {TEST_IN, T24}, {STOSB_IN, T1}, {STOSW_IN, T1}, {LODSB_IN, T1}, {LODSW_IN, T1}, {SCASB_IN, T1}, {SCASW_IN, T1},
    /* B0 */ {MOV_IN, T31}, {MOV_IN, T31}, {MOV_IN, T31}, {MOV_IN, T31}, {MOV_IN, T31}, {MOV_IN, T31}, {MOV_IN, T31}, {MOV_IN, T31},
    /* B8 */ {MOV_IN, T32}, {MOV_IN, T32}, {MOV_IN, T32}, {MOV_IN, T32}, {MOV_IN, T32}, {MOV_IN, T32}, {MOV_IN, T32}, {MOV_IN, T32},
    /* C0 */ {QQ_IN, T0}, {QQ_IN, T0}, {RET_IN, T11}, {RET_IN, T1}, {LES_IN, T39}, {LDS_IN, T39}, {MOV_IN, T40}, {MOV_IN, T41},
    /* C8 */ {QQ_IN, T0}, {QQ_IN, T0}, {RETF_IN, T11}, {RETF_IN, T1}, {INT_IN, T12}, {INT_IN, T10}, {INTO_IN, T1}, {IRET_IN, T1},
    /* D0 */ {ALT4, T43}, {ALT4, T43}, {ALT5, T43}, {ALT5, T43}, {AAM_IN, T2}, {AAD_IN, T2}, {QQ_IN, T0}, {XLAT_IN, T1},
    /* D8 */ {ESC_IN, T42}, {ESC_IN, T42}, {ESC_IN, T42}, {ESC_IN, T42}, {ESC_IN, T42}, {ESC_IN, T42}, {ESC_IN, T42}, {ESC_IN, T42},
    /* E0 */ {LOOPNE_IN, T5}, {LOOPE_IN, T5}, {LOOP_IN, T5}, {JCXZ_IN, T5}, {IN_IN, T22}, {IN_IN, T22}, {OUT_IN, T22}, {OUT_IN, T22},
    /* E8 */ {CALL_IN, T6}, {JMP_IN, T6}, {JMPF_IN, T13}, {JMPS_IN, T5}, {IN_IN, T25}, {IN_IN, T26}, {OUT_IN, T25}, {OUT_IN, T26},
    /* F0 */ {LOCK_IN, PREFIX_TYPE}, {QQ_IN, T0}, {REPNE_IN, PREFIX_TYPE}, {REP_IN, PREFIX_TYPE}, {HLT_IN, T1}, {CMC_IN, T1}, {ALT6, T43}, {ALT6, T43},
    /* F8 */ {CLC_IN, T1}, {STC_IN, T1}, {CLI_IN, T1}, {STI_IN, T1}, {CLD_IN, T1}, {STD_IN, T1}, {ALT7, T43}, {ALT8, T43},
    },
};

void ins86_install(uint16_t ds) {
    uint8_t *dst = far_ptr(rt_phys(), ds, INS86_ALTTABLEPTRS);
    for (uint16_t i = 0; i < sizeof ins86_image; i++) dst[i] = ((const uint8_t *)&ins86_image)[i];
}
