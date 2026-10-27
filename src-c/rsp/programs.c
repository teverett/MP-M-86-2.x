/*
 * programs.c -- the C programs the runtime (src-c/rt/rt.c) can run, one
 * per file in src-assem/rsp/: the processes named here start in these C
 * mains when the dispatcher first reaches them (rt.h).
 *
 * gensys_rsps: the RSPs the image holds, in the order INIT.SUP creates
 * them (src-assem's build has MPMSTAT between ECHO and CACHE; it's PL/M,
 * with no C version, so it isn't here). src-c/gensys places each one's
 * data segment -- its C image, the same bytes as its .A86 DSEG -- after
 * a code segment of the .A86's length, and chains them.
 */
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "rsp.h"
#include "gensys.h"
#include "gensys_syms.h"

extern void cache_main(uint16_t ds);
extern void clock_main(uint16_t ds);
extern void echo_main(uint16_t ds);
extern void random_main(uint16_t ds);
extern void submit_main(uint16_t ds);
extern void tmp_main(uint16_t ds);
extern const uint8_t cache_image[], clock_image[], echo_image[], tmp_image[];
/* their sizes, from each image's own type; CACHE.RSP's code segment */
extern const uint16_t cache_image_len, clock_image_len, echo_image_len, tmp_image_len;
extern const uint16_t cache_code_len, cache_start;

/* the RSPs (by PD name), then the transients (by command name: LOAD
 * names the process after its file); SUBMIT and RANDOM install their
 * own images, see rsp.h's rsp_install_transient */
const rt_program rt_programs[] = {
    {"Cachinit", cache_main},
    {"CLOCK   ", clock_main},
    {"ECHO    ", echo_main},
    {"Tmp     ", tmp_main},
    {"RANDOM  ", random_main},
    {"SUBMIT  ", submit_main},
    {0, 0},
};

const gensys_rsp *gensys_rsps(unsigned *n) {
    static gensys_rsp rsps[4];
    rsps[0] = (gensys_rsp){"Tmp     ", tmp_image, tmp_image_len, GS_TMP_CODE_LEN, GS_TMP_START};
    rsps[1] = (gensys_rsp){"CLOCK   ", clock_image, clock_image_len, GS_CLOCK_CODE_LEN, GS_CLOCK_START};
    rsps[2] = (gensys_rsp){"ECHO    ", echo_image, echo_image_len, GS_ECHO_CODE_LEN, GS_ECHO_START};
    rsps[3] = (gensys_rsp){"Cachinit", cache_image, cache_image_len, cache_code_len, cache_start};
    *n = sizeof rsps / sizeof rsps[0];
    return rsps;
}
