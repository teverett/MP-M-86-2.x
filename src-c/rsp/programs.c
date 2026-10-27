/*
 * programs.c -- the C programs the runtime (src-c/rt/rt.c) can run, one
 * per file in src-assem/rsp/: the processes named here start in these C
 * mains when the dispatcher first reaches them (rt.h).
 *
 * gensys_rsps: the RSPs the image holds, in the order INIT.SUP creates
 * them (src-assem's build: TMP, CLOCK, ECHO, MPMSTAT, CACHE). src-c/gensys
 * places each one's data segment -- its C image, the same bytes as its
 * .A86 DSEG (or for MPMSTAT, STSRSP.PLM's) -- after a code segment of
 * the original's length, and chains them.
 */
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "rsp.h"
#include "gensys.h"
#include "gensys_syms.h"
#include "rsp_syms.h"

extern void cache_main(uint16_t ds);
extern void dir_main(uint16_t ds);
extern void type_main(uint16_t ds);
extern void era_main(uint16_t ds);
extern void ren_main(uint16_t ds);
extern void abort_main(uint16_t ds);
extern void attach_main(uint16_t ds);
extern void cons_main(uint16_t ds);
extern void dskrst_main(uint16_t ds);
extern void ed_main(uint16_t ds);
extern void eraq_main(uint16_t ds);
extern void pip_main(uint16_t ds);
extern void set_main(uint16_t ds);
extern void show_main(uint16_t ds);
extern void spool_main(uint16_t ds);
extern void sspool_main(uint16_t ds);
extern void stscmd_main(uint16_t ds);
extern void tod_main(uint16_t ds);
extern void mpmstat_main(uint16_t ds);
extern void ddt86_main(uint16_t ds); /* src-c/toolchain/ */
extern void clock_main(uint16_t ds);
extern void echo_main(uint16_t ds);
extern void random_main(uint16_t ds);
extern void submit_main(uint16_t ds);
extern void tmp_main(uint16_t ds);
extern const uint8_t cache_image[], clock_image[], echo_image[], tmp_image[], mpmstat_image[];
/* their sizes, from each image's own type; CACHE.RSP's code segment */
extern const uint16_t cache_image_len, clock_image_len, echo_image_len, tmp_image_len, mpmstat_image_len;
extern const uint16_t cache_code_len, cache_start;

/* the RSPs (by PD name), then the transients (by command name: LOAD
 * names the process after its file, so the ones the disk renames go by
 * the disk's name); SUBMIT, RANDOM and src-c/utils' PL/M utilities
 * install their own images, see rsp.h's rsp_install_transient */
const rt_program rt_programs[] = {
    {"Cachinit", cache_main, rt_any},
    {"CLOCK   ", clock_main, rt_any},
    {"ECHO    ", echo_main, rt_any},
    {"MPMSTAT ", mpmstat_main, rt_rsp_only},
    {"Tmp     ", tmp_main, rt_any},
    {"RANDOM  ", random_main, rt_any},
    {"SUBMIT  ", submit_main, rt_any},
    {"ABORT   ", abort_main, rt_any},
    {"ATTACH  ", attach_main, rt_any},
    {"CONSOLE ", cons_main, rt_any},
    {"DDT86   ", ddt86_main, rt_any},
    {"DIR     ", dir_main, rt_any},
    {"DSKRESET", dskrst_main, rt_any},
    {"ED      ", ed_main, rt_any},
    {"ERA     ", era_main, rt_any},
    {"ERAQ    ", eraq_main, rt_any},
    {"MPMSTAT ", stscmd_main, rt_transient_only},
    {"PIP     ", pip_main, rt_any},
    {"REN     ", ren_main, rt_any},
    {"SET     ", set_main, rt_any},
    {"SHOW    ", show_main, rt_any},
    {"SPOOL   ", spool_main, rt_any},
    {"SSPOOL  ", sspool_main, rt_any},
    {"TOD     ", tod_main, rt_any},
    {"TYPE    ", type_main, rt_any},
    {0, 0, rt_any},
};

const gensys_rsp *gensys_rsps(unsigned *n) {
    static gensys_rsp rsps[5];
    rsps[0] = (gensys_rsp){"Tmp     ", tmp_image, tmp_image_len, GS_TMP_CODE_LEN, GS_TMP_START};
    rsps[1] = (gensys_rsp){"CLOCK   ", clock_image, clock_image_len, GS_CLOCK_CODE_LEN, GS_CLOCK_START};
    rsps[2] = (gensys_rsp){"ECHO    ", echo_image, echo_image_len, GS_ECHO_CODE_LEN, GS_ECHO_START};
    rsps[3] = (gensys_rsp){"MPMSTAT ", mpmstat_image, mpmstat_image_len, MPMSTAT_CODE_LEN, MPMSTAT_START};
    rsps[4] = (gensys_rsp){"Cachinit", cache_image, cache_image_len, cache_code_len, cache_start};
    *n = sizeof rsps / sizeof rsps[0];
    return rsps;
}
