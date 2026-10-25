/*
 * supif_test_stubs.c -- test-only doubles for supif.c's func1's own 15
 * extern supfunc targets, mirroring the marker stubs in
 * tests/src-c/fixtures/supif_wrapper.A86 (same header comment there
 * explains why: every real target is already independently
 * differential-tested elsewhere, so this test's own job is only
 * func1's dispatch logic itself -- which slot a (mod,func) lands on,
 * and whether `bx` arrives forced or ambient).
 *
 * Each stub sets *cx_out (or its struct's cx field) to the same
 * `0x9000 + slot #` marker its asm counterpart does, and returns
 * `AMBIENT_BX` verbatim for every target with no `bx` input of its
 * own (matching the asm stubs, which leave `bx` completely
 * untouched) -- except `load`, which echoes back whatever `bx` it
 * was actually called with, the one parameter this test cares about
 * (func1's own forcing for load_entry/cload_entry).
 */
#include <stdint.h>

#define AMBIENT_BX 0x55AA

typedef struct {
    uint16_t bx, cx;
} entry_bxcx;

typedef struct {
    uint16_t bx, cx;
    uint8_t reached_xios, al, ah, cl, dl;
} callbios_result;

uint16_t not_implemented(uint16_t *cx_out) { *cx_out = 0x9000; return AMBIENT_BX; }
uint16_t bad_entry(uint16_t *cx_out) { *cx_out = 0x9001; return AMBIENT_BX; }
uint16_t version_entry(const uint8_t *mem, uint16_t *cx_out) { (void)mem; *cx_out = 0x9002; return AMBIENT_BX; }

callbios_result callbios_entry(uint8_t *mem, uint16_t dx) {
    (void)mem; (void)dx;
    callbios_result r = {AMBIENT_BX, 0x9003, 0, 0, 0, 0, 0};
    return r;
}

/* load_entry/cload_entry are not separate C functions here (same as
 * the real asm, see supif.c's own header comment) -- func1 itself
 * calls `load`/`cload_entry` with the forced bx, so only these two
 * are stubbed. `load` echoes its own `bx` parameter verbatim. */
entry_bxcx load(uint8_t *mem, uint16_t dx, uint16_t bx, uint16_t sysdat,
                uint16_t user_retf_offset, uint16_t cs, int load_chain) {
    (void)mem; (void)dx; (void)sysdat; (void)user_retf_offset; (void)cs; (void)load_chain;
    entry_bxcx r = {bx, 0x900A};
    return r;
}

/* the real asm's own cload_entry has no marker of its own -- it tail-
 * jumps straight into `load:`'s own code (see
 * tests/src-c/fixtures/supif_wrapper.A86's own comment), so this stub
 * echoes `load`'s marker (0x900A) too, not a distinct one. */
entry_bxcx cload_entry(uint8_t *mem, uint16_t dx, uint16_t bx, uint16_t sysdat,
                        uint16_t user_retf_offset, uint16_t cs) {
    (void)mem; (void)dx; (void)sysdat; (void)user_retf_offset; (void)cs;
    entry_bxcx r = {bx, 0x900A};
    return r;
}

entry_bxcx clicmd_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat,
                         uint16_t user_retf_offset, uint16_t cs) {
    (void)mem; (void)dx; (void)sysdat; (void)user_retf_offset; (void)cs;
    entry_bxcx r = {AMBIENT_BX, 0x9005};
    return r;
}

entry_bxcx chain_entry(uint8_t *mem, uint16_t sysdat, uint16_t user_retf_offset, uint16_t cs) {
    (void)mem; (void)sysdat; (void)user_retf_offset; (void)cs;
    entry_bxcx r = {AMBIENT_BX, 0x900C};
    return r;
}

void rpl_entry(uint8_t *mem, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out) {
    (void)mem; (void)dx;
    *bx_out = AMBIENT_BX;
    *cx_out = 0x9006;
}

entry_bxcx parse_entry(uint8_t *mem, uint16_t dx) {
    (void)mem; (void)dx;
    entry_bxcx r = {AMBIENT_BX, 0x9007};
    return r;
}

uint16_t sysdat_entry(uint8_t *mem, uint16_t ds, uint16_t *cx_out) {
    (void)mem; (void)ds; *cx_out = 0x9008; return AMBIENT_BX;
}

uint16_t tod_entry(uint8_t *mem, uint16_t dx, uint16_t *cx_out) {
    (void)mem; (void)dx; *cx_out = 0x9009; return AMBIENT_BX;
}

uint16_t mpmversion_entry(const uint8_t *mem, uint16_t *cx_out) {
    (void)mem; *cx_out = 0x900B; return AMBIENT_BX;
}

uint16_t serial_entry(uint8_t *mem, uint16_t dx, uint16_t *cx_out) {
    (void)mem; (void)dx; *cx_out = 0x900D; return AMBIENT_BX;
}
