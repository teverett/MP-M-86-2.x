/*
 * programs.c -- the C programs the runtime (src-c/rt/rt.c) can run, one
 * per file in src/rsp/: the processes named here start in these C
 * mains when the dispatcher first reaches them (rt.h).
 *
 * rt_rsp_images: each RSP the image bakes in (the RSP chain from
 * SYSDAT's rspseg, as INIT.SUP walks it) that has a C version here gets
 * that version's data segment -- a C image of the same layout as its
 * .A86 DSEG, byte for byte -- in place of the assembled one, after the
 * two are compared (and any difference reported).
 */
#include <stdint.h>

#include "defs.h"
#include "rt.h"
#include "rsp.h"

extern void cache_main(uint16_t ds);
extern void clock_main(uint16_t ds);
extern void echo_main(uint16_t ds);
extern void random_main(uint16_t ds);
extern void submit_main(uint16_t ds);
extern void tmp_main(uint16_t ds);
extern const uint8_t cache_image[], clock_image[], echo_image[], tmp_image[];
/* their sizes, from each image's own type */
extern const uint16_t cache_image_len, clock_image_len, echo_image_len, tmp_image_len;

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

static const rsp_image *find_image(const uint8_t *name) {
    static rsp_image images[4];
    images[0] = (rsp_image){"CLOCK   ", clock_image, clock_image_len};
    images[1] = (rsp_image){"ECHO    ", echo_image, echo_image_len};
    images[2] = (rsp_image){"Tmp     ", tmp_image, tmp_image_len};
    images[3] = (rsp_image){"Cachinit", cache_image, cache_image_len};
    for (unsigned i = 0; i < sizeof images / sizeof images[0]; i++) {
        int same = 1;
        for (int k = 0; k < pnamsiz; k++) {
            if ((name[k] & 0x7F) != (uint8_t)images[i].name[k]) same = 0;
        }
        if (same) return &images[i];
    }
    return 0;
}

/*
 * Walk the RSP chain (SYSDAT's rspseg, each RSP's rsp_link) as INIT.SUP
 * will: install each C image over its assembled RSP, and take out of
 * the chain any RSP with no C version (MPMSTAT, which is PL/M), so
 * INIT never creates a process the runtime can't run.
 */
void rt_rsp_images(uint8_t *phys, uint16_t sysdat) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint8_t *prev_link = sd + SYSDAT_RSPSEG;
    uint16_t seg = rd16(prev_link);
    while (seg != 0) {
        uint8_t *rsp = far_ptr(phys, seg, 0);
        const uint8_t *name = rsp + rsp_pd + p_name;
        uint16_t next = rd16(rsp + rsp_link);
        const rsp_image *im = find_image(name);
        if (im) {
            rsp_install(rsp, im);
            prev_link = rsp + rsp_link;
        } else {
            char buf[pnamsiz + 1];
            for (int i = 0; i < pnamsiz; i++) buf[i] = (char)(name[i] & 0x7F);
            buf[pnamsiz] = 0;
            rt_puts("rt: no C version of RSP ");
            rt_puts(buf);
            rt_puts(", left out\r\n");
            wr16(prev_link, next);
        }
        seg = next;
    }
}
