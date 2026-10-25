/*
 * bdosif_init_stubs.c -- link-only doubles for the three que1.c
 * entries bdosif.c's own `init` calls (makeq_entry/openq_entry/
 * writeq_entry) -- tests/src-c/test_bdosif.py only exercises
 * `entry`, never `init`, so these never actually run; they exist
 * purely so the library links without pulling in que1.c's own real
 * dependency chain (sysent.c/flag.c/findpd.c/proc.c/dsptch.c), which
 * entry's own dispatch has nothing to do with.
 */
#include <stdint.h>

typedef struct {
    uint16_t bx, cx;
} bdos_bxcx;

bdos_bxcx makeq_entry(uint8_t *mem, uint16_t dx, uint16_t sysdat) {
    (void)mem; (void)dx; (void)sysdat;
    bdos_bxcx r = {0, 0};
    return r;
}

bdos_bxcx openq_entry(uint8_t *mem, uint16_t dx) {
    (void)mem; (void)dx;
    bdos_bxcx r = {0, 0};
    return r;
}

bdos_bxcx writeq_entry(uint8_t *mem, uint16_t dx) {
    (void)mem; (void)dx;
    bdos_bxcx r = {0, 0};
    return r;
}
