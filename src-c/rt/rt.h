/*
 * rt.h -- the runtime that runs the src-c kernel as a real multitasking
 * MP/M-86 in kernel.elf (rt.c's header comment), and its interface for
 * programs written in C (src-c/rsp/).
 */
#ifndef SRC_C_RT_H
#define SRC_C_RT_H

#include <stdint.h>

#include "defs.h"

/* Base of real physical memory (0 in kernel.elf: flat 32-bit, base-0
 * segments). Every 8086 address is far_ptr(rt_phys(), seg, off). */
uint8_t *rt_phys(void);
uint16_t rt_sysdat(void);

/*
 * A system call from a running process, as `int 224` makes one: CL =
 * function, DX = parameter, DS/ES = the caller's data/extra segments
 * (DS is where DX points, ES what the call returns in ES unless it
 * sets one). Returns AX (= BX), BX, CX and ES as the SUP's user_entry
 * leaves them.
 */
mpm_regs rt_int224(uint16_t ds, uint16_t es, uint8_t cl, uint16_t dx);

/* The common case: ES = DS. */
static inline mpm_regs rt_mpm(uint16_t ds, uint8_t cl, uint16_t dx) {
    return rt_int224(ds, ds, cl, dx);
}

/*
 * A program the runtime can run. When the dispatcher first switches to
 * a process, the runtime starts it in the C program whose `name`
 * matches the process name (the PD's p_name, 8 bytes, blank padded),
 * passing its DS (the UDA's u_initds, as proc_creat leaves it).
 * src-c/rsp/ lists them in rt_programs[].
 */
typedef void (*rt_main_fn)(uint16_t ds);
typedef struct {
    const char *name; /* 8 characters, blank padded */
    rt_main_fn main;
} rt_program;
extern const rt_program rt_programs[];

/* The XIOS entry, as SYSDAT's xiosmod far pointer is in the asm: CACHE
 * (src-c/rsp/cache.c) replaces it and calls through to the old one. */
typedef uint16_t (*rt_xios_fn)(uint8_t *phys, uint16_t sysdat, uint8_t al, uint16_t cx, uint16_t dx);
extern rt_xios_fn rt_xiosmod;

/* debug output straight to the console UART */
void rt_puts(const char *s);
void rt_puthex(uint16_t v);

#endif
