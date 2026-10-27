/*
 * clock.c -- C version of src/rsp/CLOCK.A86, the CLOCK resident system
 * process: each time it returns from waiting for the Second flag (2) it
 * adds a second to SYSDAT's time of day, setting the Minute flag (3)
 * as each minute ends.
 *
 * Its data segment, below, is CLOCK.A86's DSEG byte for byte (rsp.h).
 * The kernel's view of CLOCK -- its PD, UDA, initial frame -- is that
 * image; clock_main is `clock:`/`clockloop:`.
 */
#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "rsp.h"
#include "rsp_syms.h"

#define mpm_flagw 132
#define mpm_flags 133
#define tod_offset 0x07E
#define sec_flag 2
#define min_flag 3
#define tod_day  0 /* word */
#define tod_hour 2 /* byte */
#define tod_min  3
#define tod_sec  4

typedef struct RSP_PACKED {
    uint16_t sysdat;    /* rsp_link: INIT.SUP gives the RSP SYSDAT here */
    uint16_t hdr[7];
    rsp_pd_t pd;
    rsp_uda_t uda;
    uint8_t dma[128];
    uint16_t stack[15];
    rsp_frame_t stack_tos;
    uint16_t end;       /* "dw 0 ; END OF DATA" */
} clock_data;

_Static_assert(offsetof(clock_data, sysdat) == CLOCK_SYSDAT, "sysdat");
_Static_assert(offsetof(clock_data, pd) == rsp_pd, "pd");
_Static_assert(offsetof(clock_data, uda) == CLOCK_UDA, "uda");
_Static_assert(offsetof(clock_data, dma) == CLOCK_DMA, "dma");
_Static_assert(offsetof(clock_data, stack) == CLOCK_STACK, "stack");
_Static_assert(offsetof(clock_data, stack_tos) == CLOCK_STACK_TOS, "stack_tos");

const uint16_t clock_image_len = sizeof(clock_data);
const clock_data clock_image = {
    .pd = RSP_PD(190, pf_keep, "CLOCK   ", 0),
    .uda = RSP_UDA(CLOCK_DMA, CLOCK_STACK_TOS),
    .stack = {0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC,
              0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC},
};

/* clc ! inc al ! daa */
static uint8_t bcd_inc(uint8_t al) {
    uint8_t lo_carry = (uint8_t)((al & 0x0F) == 0x0F); /* AF out of the inc */
    al = (uint8_t)(al + 1);
    if ((al & 0x0F) > 9 || lo_carry) al = (uint8_t)(al + 6);
    if (al > 0x99) al = (uint8_t)(al + 0x60);
    return al;
}

void clock_main(uint16_t ds) {
    clock_data *d = RSP_DATA(clock_data, ds);
    uint16_t sysdat = d->sysdat;                    /* mov ds,sysdat */
    uint8_t *tod = far_ptr(rt_phys(), sysdat, tod_offset); /* mov bx,tod_offset */
    for (;;) {
        /* clockloop: wait for the Seconds flag */
        (void)rt_mpm(sysdat, mpm_flagw, sec_flag);
        uint8_t al = bcd_inc(tod[tod_sec]);
        tod[tod_sec] = al;
        if (al < 0x60) continue;

        /* update_min: set the minute flag */
        tod[tod_sec] = 0;
        (void)rt_mpm(sysdat, mpm_flags, min_flag);
        al = bcd_inc(tod[tod_min]);
        tod[tod_min] = al;
        if (al < 0x60) continue;

        /* update_hour */
        tod[tod_min] = 0;
        al = bcd_inc(tod[tod_hour]);
        tod[tod_hour] = al;
        if (al < 0x24) continue;

        /* update_day */
        tod[tod_hour] = 0;
        wr16(tod + tod_day, (uint16_t)(rd16(tod + tod_day) + 1));
    }
}
