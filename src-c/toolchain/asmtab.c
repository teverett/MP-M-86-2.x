/*
 * asmtab.c -- C version of src-assem/toolchain/ASMTAB.PLM, DDT86's
 * assembler overlay's opcode table, op$tab: for each mnemonic DDT86's
 * A command knows, four bytes -- its index (OPTAB.LIT's xxx$in, the
 * position of its name in DIS86's mnemonic tables, ops2-ops6), the
 * operand type ass86.c dispatches on (1-21; 0FFh LOCK, 0FEh a REP
 * prefix), the base opcode and the reg field (or a per-type index).
 *
 * The table is initialized data in DDT86's image, at ASMTAB_OPTAB
 * (toolchain_syms.h: the overlay's data starts with it, right after
 * the code); ass86_install (ass86.c) installs it there, and ass86.c
 * reads it from the image, as ASS86.PLM does.
 */
#include <stdint.h>

#include "toolchain.h"

/* OPTAB.LIT: where each name length's mnemonics start in the index */
#define OP2_IN 0
#define OP3_IN 12
#define OP4_IN 81
#define OP5_IN 106
#define OP6_IN 122
#define AAA_IN (OP3_IN + 0)
#define AAD_IN (OP3_IN + 1)
#define AAM_IN (OP3_IN + 2)
#define AAS_IN (OP3_IN + 3)
#define ADC_IN (OP3_IN + 4)
#define ADD_IN (OP3_IN + 5)
#define AND_IN (OP3_IN + 6)
#define CALL_IN (OP4_IN + 0)
#define CALLF_IN (OP5_IN + 0)
#define CBW_IN (OP3_IN + 7)
#define CLC_IN (OP3_IN + 8)
#define CLD_IN (OP3_IN + 9)
#define CLI_IN (OP3_IN + 10)
#define CMC_IN (OP3_IN + 11)
#define CMP_IN (OP3_IN + 12)
#define CMPSB_IN (OP5_IN + 1)
#define CMPSW_IN (OP5_IN + 2)
#define CWD_IN (OP3_IN + 14)
#define DAA_IN (OP3_IN + 15)
#define DAS_IN (OP3_IN + 16)
#define DEC_IN (OP3_IN + 17)
#define DIV_IN (OP3_IN + 18)
#define ESC_IN (OP3_IN + 21)
#define HLT_IN (OP3_IN + 22)
#define IDIV_IN (OP4_IN + 1)
#define IMUL_IN (OP4_IN + 2)
#define IN_IN (OP2_IN + 0)
#define INC_IN (OP3_IN + 23)
#define INT_IN (OP3_IN + 24)
#define INTO_IN (OP4_IN + 3)
#define IRET_IN (OP4_IN + 4)
#define JA_IN (OP2_IN + 1)
#define JAE_IN (OP3_IN + 25)
#define JB_IN (OP2_IN + 2)
#define JBE_IN (OP3_IN + 26)
#define JC_IN (OP2_IN + 3)
#define JCXZ_IN (OP4_IN + 5)
#define JE_IN (OP2_IN + 4)
#define JG_IN (OP2_IN + 5)
#define JGE_IN (OP3_IN + 27)
#define JL_IN (OP2_IN + 6)
#define JLE_IN (OP3_IN + 28)
#define JMP_IN (OP3_IN + 29)
#define JMPF_IN (OP4_IN + 6)
#define JMPS_IN (OP4_IN + 7)
#define JNA_IN (OP3_IN + 30)
#define JNAE_IN (OP4_IN + 8)
#define JNB_IN (OP3_IN + 31)
#define JNBE_IN (OP4_IN + 9)
#define JNC_IN (OP3_IN + 32)
#define JNE_IN (OP3_IN + 33)
#define JNG_IN (OP3_IN + 34)
#define JNGE_IN (OP4_IN + 10)
#define JNL_IN (OP3_IN + 35)
#define JNLE_IN (OP4_IN + 11)
#define JNO_IN (OP3_IN + 36)
#define JNP_IN (OP3_IN + 37)
#define JNS_IN (OP3_IN + 38)
#define JNZ_IN (OP3_IN + 39)
#define JO_IN (OP2_IN + 7)
#define JP_IN (OP2_IN + 8)
#define JPE_IN (OP3_IN + 40)
#define JPO_IN (OP3_IN + 41)
#define JS_IN (OP2_IN + 9)
#define JZ_IN (OP2_IN + 10)
#define LAHF_IN (OP4_IN + 12)
#define LDS_IN (OP3_IN + 42)
#define LEA_IN (OP3_IN + 43)
#define LES_IN (OP3_IN + 44)
#define LOCK_IN (OP4_IN + 13)
#define LODSB_IN (OP5_IN + 3)
#define LODSW_IN (OP5_IN + 4)
#define LOOP_IN (OP4_IN + 14)
#define LOOPE_IN (OP5_IN + 5)
#define LOOPNE_IN (OP6_IN + 0)
#define LOOPNZ_IN (OP6_IN + 1)
#define LOOPZ_IN (OP5_IN + 6)
#define MOV_IN (OP3_IN + 45)
#define MOVSB_IN (OP5_IN + 7)
#define MOVSW_IN (OP5_IN + 8)
#define MUL_IN (OP3_IN + 46)
#define NEG_IN (OP3_IN + 47)
#define NOP_IN (OP3_IN + 48)
#define NOT_IN (OP3_IN + 49)
#define OR_IN (OP2_IN + 11)
#define OUT_IN (OP3_IN + 50)
#define POP_IN (OP3_IN + 51)
#define POPF_IN (OP4_IN + 15)
#define PUSH_IN (OP4_IN + 16)
#define PUSHF_IN (OP5_IN + 9)
#define RCL_IN (OP3_IN + 52)
#define RCR_IN (OP3_IN + 53)
#define REP_IN (OP3_IN + 54)
#define REPE_IN (OP4_IN + 17)
#define REPNE_IN (OP5_IN + 10)
#define REPNZ_IN (OP5_IN + 11)
#define REPZ_IN (OP4_IN + 18)
#define RET_IN (OP3_IN + 55)
#define RETF_IN (OP4_IN + 19)
#define ROL_IN (OP3_IN + 56)
#define ROR_IN (OP3_IN + 57)
#define SAHF_IN (OP4_IN + 20)
#define SAL_IN (OP3_IN + 58)
#define SAR_IN (OP3_IN + 59)
#define SBB_IN (OP3_IN + 60)
#define SCASB_IN (OP5_IN + 12)
#define SCASW_IN (OP5_IN + 13)
#define SHL_IN (OP3_IN + 61)
#define SHR_IN (OP3_IN + 62)
#define STC_IN (OP3_IN + 64)
#define STD_IN (OP3_IN + 65)
#define STI_IN (OP3_IN + 66)
#define STOSB_IN (OP5_IN + 14)
#define STOSW_IN (OP5_IN + 15)
#define SUB_IN (OP3_IN + 67)
#define TEST_IN (OP4_IN + 21)
#define WAIT_IN (OP4_IN + 22)
#define XCHG_IN (OP4_IN + 23)
#define XLAT_IN (OP4_IN + 24)
#define XOR_IN (OP3_IN + 68)


/* ASMTAB.PLM: op$tab, in order of the names */
const uint8_t asmtab_op_tab[ASMTAB_OP_TAB_LEN] = {
    AAA_IN, 1, 0x37, 0,
    AAD_IN, 2, 0xD5, 10,
    AAM_IN, 2, 0xD4, 10,
    AAS_IN, 1, 0x3F, 0,
    ADC_IN, 9, 0x10, 2,
    ADD_IN, 9, 0x00, 0,
    AND_IN, 10, 0x20, 4,
    CALL_IN, 11, 0x00, 0,
    CALLF_IN, 17, 0x00, 0,
    CBW_IN, 1, 0x98, 0,
    CLC_IN, 1, 0xF8, 0,
    CLD_IN, 1, 0xFC, 0,
    CLI_IN, 1, 0xFA, 0,
    CMC_IN, 1, 0xF5, 0,
    CMP_IN, 9, 0x38, 7,
    CMPSB_IN, 1, 0xA6, 0,
    CMPSW_IN, 1, 0xA7, 0,
    CWD_IN, 1, 0x99, 0,
    DAA_IN, 1, 0x27, 0,
    DAS_IN, 1, 0x2F, 0,
    DEC_IN, 7, 0x00, 0,
    DIV_IN, 5, 0xF6, 6,
    ESC_IN, 13, 0xD8, 0,
    HLT_IN, 1, 0xF4, 0,
    IDIV_IN, 5, 0xF6, 7,
    IMUL_IN, 5, 0xF6, 5,
    IN_IN, 15, 0xE4, 0,
    INC_IN, 7, 0x01, 0,
    INT_IN, 12, 0x00, 0,
    INTO_IN, 1, 0xCE, 0,
    IRET_IN, 1, 0xCF, 0,
    JA_IN, 3, 0x77, 0,
    JAE_IN, 3, 0x73, 0,
    JB_IN, 3, 0x72, 0,
    JBE_IN, 3, 0x76, 0,
    JC_IN, 3, 0x72, 0,
    JCXZ_IN, 3, 0xE3, 0,
    JE_IN, 3, 0x74, 0,
    JG_IN, 3, 0x7F, 0,
    JGE_IN, 3, 0x7D, 0,
    JL_IN, 3, 0x7C, 0,
    JLE_IN, 3, 0x7E, 0,
    JMP_IN, 11, 0x01, 0,
    JMPF_IN, 17, 0x01, 0,
    JMPS_IN, 3, 0xEB, 0,
    JNA_IN, 3, 0x76, 0,
    JNAE_IN, 3, 0x72, 0,
    JNB_IN, 3, 0x73, 0,
    JNBE_IN, 3, 0x77, 0,
    JNC_IN, 3, 0x73, 0,
    JNE_IN, 3, 0x75, 0,
    JNG_IN, 3, 0x7E, 0,
    JNGE_IN, 3, 0x7C, 0,
    JNL_IN, 3, 0x7D, 0,
    JNLE_IN, 3, 0x7F, 0,
    JNO_IN, 3, 0x71, 0,
    JNP_IN, 3, 0x7B, 0,
    JNS_IN, 3, 0x79, 0,
    JNZ_IN, 3, 0x75, 0,
    JO_IN, 3, 0x70, 0,
    JP_IN, 3, 0x7A, 0,
    JPE_IN, 3, 0x7A, 0,
    JPO_IN, 3, 0x7B, 0,
    JS_IN, 3, 0x78, 0,
    JZ_IN, 3, 0x74, 0,
    LAHF_IN, 1, 0x9F, 0,
    LDS_IN, 6, 0xC5, 0,
    LEA_IN, 6, 0x8D, 0,
    LES_IN, 6, 0xC4, 0,
    LOCK_IN, 255, 0xF0, 0,
    LODSB_IN, 1, 0xAC, 0,
    LODSW_IN, 1, 0xAD, 0,
    LOOP_IN, 3, 0xE2, 0,
    LOOPE_IN, 3, 0xE1, 0,
    LOOPNE_IN, 3, 0xE0, 0,
    LOOPNZ_IN, 3, 0xE0, 0,
    LOOPZ_IN, 3, 0xE1, 0,
    MOV_IN, 20, 0x00, 0,
    MOVSB_IN, 1, 0xA4, 0,
    MOVSW_IN, 1, 0xA5, 0,
    MUL_IN, 5, 0xF6, 4,
    NEG_IN, 5, 0xF6, 3,
    NOP_IN, 1, 0x90, 0,
    NOT_IN, 5, 0xF6, 2,
    OR_IN, 10, 0x08, 1,
    OUT_IN, 16, 0xE6, 0,
    POP_IN, 21, 0x00, 0,
    POPF_IN, 1, 0x9D, 0,
    PUSH_IN, 8, 0x01, 0,
    PUSHF_IN, 1, 0x9C, 0,
    RCL_IN, 4, 0xD0, 2,
    RCR_IN, 4, 0xD0, 3,
    REP_IN, 254, 0xF3, 0,
    REPE_IN, 254, 0xF3, 0,
    REPNE_IN, 254, 0xF2, 0,
    REPNZ_IN, 254, 0xF2, 0,
    REPZ_IN, 254, 0xF3, 0,
    RET_IN, 14, 0xC2, 0,
    RETF_IN, 14, 0xCA, 0,
    ROL_IN, 4, 0xD0, 0,
    ROR_IN, 4, 0xD0, 1,
    SAHF_IN, 1, 0x9E, 0,
    SAL_IN, 4, 0xD0, 4,
    SAR_IN, 4, 0xD0, 7,
    SBB_IN, 9, 0x18, 3,
    SCASB_IN, 1, 0xAE, 0,
    SCASW_IN, 1, 0xAF, 0,
    SHL_IN, 4, 0xD0, 4,
    SHR_IN, 4, 0xD0, 5,
    STC_IN, 1, 0xF9, 0,
    STD_IN, 1, 0xFD, 0,
    STI_IN, 1, 0xFB, 0,
    STOSB_IN, 1, 0xAA, 0,
    STOSW_IN, 1, 0xAB, 0,
    SUB_IN, 9, 0x28, 5,
    TEST_IN, 18, 0x00, 0,
    WAIT_IN, 1, 0x9B, 0,
    XCHG_IN, 19, 0x00, 0,
    XLAT_IN, 1, 0xD7, 0,
    XOR_IN, 10, 0x30, 6,
};
