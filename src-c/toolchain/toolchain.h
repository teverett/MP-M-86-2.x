/*
 * toolchain.h -- the C versions of src-assem/toolchain/, DRI's DDT86
 * debugger: DDT86.A86 (ddt86.c) and its two PL/M overlays, the
 * disassembler (INS86.PLM + DIS86.PLM: ins86.c, dis86.c) and the
 * assembler (ASMTAB.PLM + ASS86.PLM: asmtab.c, ass86.c).
 *
 * DDT86 is an 8080-model program: code and data in one segment, `ds`
 * below. Every C routine works on the bytes at the offsets the real
 * build has them (toolchain_syms.h, tools/build_ddt86.py's layout):
 * DDT86's data from TC_DDT86_ENDCS, each overlay's data after its
 * code, so the memory a test compares is the asm's.
 *
 * The overlays' entries are the PL/M procedures DDT86 calls (with a far
 * pointer: segment, then offset, pushed):
 *   disem(disloc pointer) address -- disassemble the instruction at
 *       seg:off, printing it; returns the offset past it;
 *   asm(loc pointer) address -- read a line, assemble it at seg:off
 *       (storing through ddt_set); returns the offset past it.
 * and the overlays call back into DDT86 through its vectors at
 * 103h-10Fh (DISLNK86/ASSLNK86's stubs); in C those are the ddt_*
 * services below, which ddt86.c provides (a module test provides its
 * own: tests/src-c/fixtures/toolchain_host.c).
 */
#ifndef SRC_C_TOOLCHAIN_H
#define SRC_C_TOOLCHAIN_H

#include <stdint.h>

/* the disassembler overlay (DIS86.PLM's publics) */
uint16_t dis86_disem(uint16_t ds, uint16_t off, uint16_t seg);
void dis86_printm(uint16_t ds, uint16_t a);       /* print the '$'-ended string at ds:a */
void dis86_print_word(uint16_t ds, uint16_t a);   /* four hex digits */
/* install the overlay's initialized data (INS86's tables and DIS86's)
 * at their offsets in ds, as loading the overlay does: ddt86.c calls it
 * once at start-up, before any dis86_* call */
void dis86_install(uint16_t ds);
void ins86_install(uint16_t ds);                  /* INS86's part (dis86_install calls it) */

/* the assembler overlay (ASS86.PLM's) */
uint16_t ass86_asm(uint16_t ds, uint16_t off, uint16_t seg);
/* install the assembler overlay's initialized data (ASMTAB's op$tab and
 * ASS86's variables and tables, ASMTAB_OPTAB up to TC_ASS_END) at ds:
 * DDT86 calls it once at start-up */
void ass86_install(uint16_t ds);

/* ASMTAB.PLM's op$tab: 120 mnemonics, 4 bytes each (asmtab.c) */
#define ASMTAB_OP_TAB_LEN 480
extern const uint8_t asmtab_op_tab[ASMTAB_OP_TAB_LEN];

/* DDT86's services, its vectors: 103h conin, 106h plmconout, 109h
 * plmgetline, 10Ch asment (goddt: the assembler's error exit, back to
 * DDT86's A-command loop -- it doesn't return), 10Fh plmset (ddtset:
 * store and verify a byte at type1seg:off) */
uint8_t ddt_conin(uint16_t ds);
void ddt_conout(uint16_t ds, uint8_t c);
void ddt_getline(uint16_t ds);
void ddt_asment(uint16_t ds) __attribute__((noreturn));
void ddt_set(uint16_t ds, uint16_t off, uint8_t b);

/* DDT86 itself (ddt86.c): the transient's main, started with DS = its
 * one 8080-model segment, and its data image -- ddt86_image_len
 * (TC_DDT86_DATA_END) bytes from offset 0: zeros below TC_DDT86_ENDCS
 * (the base page and DDT86's code), then its data, which ddt86_main
 * installs (with the overlays', dis86_install and ass86_install) */
void ddt86_main(uint16_t ds);
extern const uint16_t ddt86_image_len;

#endif
