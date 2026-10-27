/*
 * stscom.h -- C version of src-assem/utils/STSCOM.PLM, the status
 * display both MPMSTATs share: STSCMD.PLM (the transient MPMSTAT.CMD,
 * src-c/utils/stscmd.c) and STSRSP.PLM (the MPMSTAT RSP) `$include` it.
 * It walks SYSDAT's lists -- ready, delay, poll, queues and their DQ/NQ
 * lists, flags, the CCBs, the memory partitions and the free lists --
 * and prints them in hexadecimal.
 *
 * STSCOM's data is two blocks that sit at different DS offsets in the
 * two programs but are laid out the same in both: its variables
 * (stscom_vars: lparen, rparen, the eight far pointers, hex$digit,
 * link$list) and its string constants (stscom_strs: arr34..arr175, as
 * tools/plmc.py numbers them in both). Each program puts both in its
 * own data image -- initialized with STSCOM_VARS_INIT and
 * STSCOM_STRS_INIT -- asserts their offsets against its own syms header,
 * and tells stscom_init where they are. The display routines then work
 * on the program's own copy, so the memory both see is the PL/M's.
 *
 * Before the display$ routines run, the program sets the pointers'
 * segments to SYSDAT's (sysdat$pointer whole, the others' segment
 * words), as STSCMD and STSRSP's main programs do.
 */
#ifndef SRC_C_STSCOM_H
#define SRC_C_STSCOM_H

#include <stdint.h>

#include "rsp.h"

/* a PL/M pointer, as STSCOM's `structure (offset word, segment word)
 * at` overlays name its halves */
typedef struct RSP_PACKED {
    uint16_t offset, segment;
} stscom_ptr;

/* STSCOM's variables, in declaration order */
typedef struct RSP_PACKED {
    uint8_t lparen, rparen;
    stscom_ptr sysdat_ptr;
    stscom_ptr pd_ptr;
    stscom_ptr qd_ptr;
    stscom_ptr md_ptr;
    stscom_ptr ms_ptr;
    stscom_ptr sat_ptr;
    stscom_ptr flag_ptr;
    stscom_ptr ccb_ptr;
    char hex_digit[16];
    uint16_t link_list[64]; /* used by display$links & display$memory */
} stscom_vars;

/* STSCOM's string constants (plmc's arrNN, in order) */
typedef struct RSP_PACKED {
    char arr34[5], arr35[31], arr36[32], arr37[28], arr38[40], arr42[39], arr46[41], arr50[28], arr57[21],
        arr58[36], arr59[20], arr60[20], arr67[4], arr68[3], arr69[19], arr76[4], arr77[3], arr78[21],
        arr79[21], arr80[26], arr87[10], arr88[13], arr98[12], arr110[11], arr114[27], arr118[4], arr119[9],
        arr123[11], arr127[21], arr128[27], arr135[4], arr136[8], arr137[19], arr141[24], arr165[4],
        arr166[3], arr167[3], arr174[4], arr175[13];
} stscom_strs;

#define STSCOM_VARS_INIT \
    { .lparen = '[', .rparen = ']', .hex_digit = "0123456789ABCDEF" }

#define STSCOM_STRS_INIT                                          \
    {                                                             \
        .arr34 = "    $",                                         \
        .arr35 = "Number of Physical Consoles = $",               \
        .arr36 = "\r\nNumber of Virtual Consoles = $",            \
        .arr37 = "\r\nNumber of List Devices = $",                \
        .arr38 = "\r\nNumber of Free Process Descriptors = $",    \
        .arr42 = "\r\nNumber of Free Memory Descriptors = $",     \
        .arr46 = "\r\nNumber of Free Queue Control Blocks = $",   \
        .arr50 = "\r\nFree Queue Buffer Area = $",                \
        .arr57 = "\r\nNumber of Flags = $",                       \
        .arr58 = "\r\nMaximum Paragraphs Per Process = $",        \
        .arr59 = "Ready Process(es): $",                          \
        .arr60 = "Process(es) DQing: $",                          \
        .arr67 = "\r\n $",                                        \
        .arr68 = "  $",                                           \
        .arr69 = "Process(es) NQing:$",                           \
        .arr76 = "\r\n $",                                        \
        .arr77 = "  $",                                           \
        .arr78 = "Delayed Process(es):$",                         \
        .arr79 = "Polling Process(es):$",                         \
        .arr80 = "Process(es) Flag Waiting:$",                    \
        .arr87 = " -       $",                                    \
        .arr88 = "Flag(s) Set:$",                                 \
        .arr98 = "\r\nQueue(s):$",                                \
        .arr110 = "          $",                                  \
        .arr114 = "\r\nProcess(es) Attached to $",                \
        .arr118 = "\r\n $",                                       \
        .arr119 = "      - $",                                    \
        .arr123 = "Unattached$",                                  \
        .arr127 = "Control P - Console $",                        \
        .arr128 = "\r\nProcess(es) Waiting for $",                \
        .arr135 = "\r\n $",                                       \
        .arr136 = "      -$",                                     \
        .arr137 = "Memory Partitions:$",                          \
        .arr141 = "Start Length Process | $",                     \
        .arr165 = "   $",                                         \
        .arr166 = "  $",                                          \
        .arr167 = "  $",                                          \
        .arr174 = "   $",                                         \
        .arr175 = "  * FREE *  $",                                \
    }

/* SYSDAT's sd$byte(ncondev) and sd$byte(nciodev) (STSCOM's literals) */
#define stscom_ncondev 0x83
#define stscom_nlstdev 0x84
#define stscom_nciodev 0x85

/* where the program's DS, its stscom_vars and its stscom_strs (a DS
 * offset: print$buffer takes one) are; call before anything else */
void stscom_init(uint16_t ds, stscom_vars *v, uint16_t strs);

/* the BDOS calls (mon1..mon4 with DS = the program's) */
void stscom_reboot(void);
uint8_t stscom_conin(void);
void stscom_co(uint8_t c);
void stscom_print_buffer(uint16_t a);
uint16_t stscom_get_version(void);
void stscom_terminate(void);
void stscom_get_sysdat(stscom_ptr *p); /* mon4(154): ES:BX */
void stscom_mon1(uint8_t func, uint16_t info);

/* the utility functions */
void stscom_crlf(void);
void stscom_display_hex_byte(uint8_t value);
void stscom_display_hex_word(uint16_t value);
void stscom_display_text(uint8_t count, uint16_t off, uint16_t seg);

/* the displays */
void stscom_display_config(void);
void stscom_display_ready(void);
void stscom_display_dq(void);
void stscom_display_nq(void);
void stscom_display_delay(void);
void stscom_display_poll(void);
void stscom_display_flag_wait(void);
void stscom_display_flag_set(void);
void stscom_display_queues(void);
void stscom_display_ccb(uint16_t name, uint8_t first_ccb, uint8_t last_ccb);
void stscom_display_memory(void);

#endif
