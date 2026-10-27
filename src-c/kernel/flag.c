/*
 * flag.c -- C port of src-assem/kernel/FLAG.RTM.
 *
 * flag_set_entry and flag_wait_entry both call the file's own flag_valid,
 * an ordinary internal call/return here (unlike SYSENT.RTM/IDLE.SUP, no
 * boundary stub needed for it). flag_wait_entry's one path that calls the
 * unported dispatcher (`call dsptch`, in `fw_noff`) IS a boundary stub, per
 * docs/src-c.md's convention: modeled as an instant no-op return, which is
 * correct here because nothing downstream reads a register the dispatcher
 * might have set.
 *
 * Not modeled: the real `flag_valid` pops its own return address into AX
 * and pushes the caller's FLAGS in its place (so it can `jmp flag_bexit`
 * straight past its caller's own stack frame on an invalid flag number,
 * while still correctly restoring the ORIGINAL caller's flags once
 * flag_bexit's `popf` runs) -- a delicate manual critical-section idiom
 * with no C equivalent and no observable effect on bx/cx, which is all
 * this port's callers can see. See tools/i86sim.py's CALL/RET sentinel
 * for how the test oracle executes the real mechanism instead of
 * replicating it here.
 *
 * Runs against real memory (defs.h's far_ptr), like proc.c/dsptch.c/
 * sysent.c: `phys` is the base of real physical memory and `sysdat`
 * the real SYSDAT segment (the real asm's DS). `nflags`, the `flags`
 * table pointer, the flag entries it points to, and PDs are
 * SYSDAT-relative; u_dparam goes to the running process's UDA (the real
 * asm's ambient ES, p_uda[rlr]).
 */
#include <stdint.h>

#include "defs.h"

DSPTCH_HOOK(flag) /* call dsptch, inside the RTM (defs.h) */

typedef struct {
    uint16_t bx, cx;
} flag_bxcx;

/* flag_exit -- Successful Exit */
static flag_bxcx flag_exit(void) {
    flag_bxcx r = {0, 0};
    return r;
}

/* flag_bexit -- Exit with Error */
static flag_bxcx flag_bexit(uint16_t cx) {
    flag_bxcx r = {0xFFFF, cx};
    return r;
}

/*
 * flag_valid -- Check validity of flag number
 *	input:	dl = flag number
 *	output:	on success, si_out/bx_out are set (si = ptr to flag entry,
 *		bx = its contents) and this returns 1
 *		on failure, returns 0 (the caller should flag_bexit(e_ill_flag))
 */
static int flag_valid(const uint8_t *sd, uint8_t dl, uint16_t *si_out, uint16_t *bx_out) {
    if (dl >= seg_r8(sd, SYSDAT_NFLAGS)) return 0;
    uint16_t si = (uint16_t)(seg_r16(sd, SYSDAT_FLAGS) + dl * flglen);
    *si_out = si;
    *bx_out = seg_r16(sd, si);
    return 1;
}

/*
 * flag_set_entry -- Set Logical Interrupt Flag
 *	input:	dl = flag number
 *	output:	bx = 0 if okay, 0xFFFF if error
 *		cx = error code: 0, e_flag_ovrrun, or e_ill_flag
 */
flag_bxcx flag_set_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t si, bx;
    if (!flag_valid(sd, dl, &si, &bx)) return flag_bexit(e_ill_flag);

    uint8_t ignore = seg_r8(sd, (uint16_t)(si + flg_ignore));
    if (ignore != flag_zig) {
        seg_w8(sd, (uint16_t)(si + flg_ignore), (uint8_t)(ignore - 1));
        return flag_exit();
    }
    /* fs_set */
    if (bx == flag_on) return flag_bexit(e_flag_ovrrun);
    if (bx == flag_off) {
        seg_w16(sd, (uint16_t)(si + flg_pd), flag_on);
        return flag_exit();
    }
    /* fs_noff: bx is a PD address waiting on this flag -- wake it */
    seg_w16(sd, (uint16_t)(bx + p_link), seg_r16(sd, SYSDAT_DRL));
    seg_w16(sd, SYSDAT_DRL, bx);
    seg_w8(sd, (uint16_t)(bx + p_stat), ps_run);
    seg_w16(sd, (uint16_t)(si + flg_pd), flag_off);
    return flag_exit();
}

/*
 * flag_wait_entry -- Wait for Logical Interrupt Flag
 *	input:	dl = flag number
 *	output:	bx = 0 if okay, 0xFFFF if error
 *		cx = error code: 0, e_flag_underrun, or e_ill_flag
 */
flag_bxcx flag_wait_entry(uint8_t *phys, uint16_t sysdat, uint8_t dl) {
    uint8_t *sd = far_ptr(phys, sysdat, 0);
    uint16_t si, bx;
    if (!flag_valid(sd, dl, &si, &bx)) return flag_bexit(e_ill_flag);

    if (bx == flag_on) {
        seg_w16(sd, (uint16_t)(si + flg_pd), flag_off);
        return flag_exit();
    }
    if (bx == flag_off) {
        seg_w8(sd, (uint16_t)(seg_r16(sd, SYSDAT_RLR) + p_stat), ps_flagwait);
        seg_w16(far_ptr(phys, rlr_uda(sd), 0), u_dparam, si);
        DSPTCH(flag, phys, sysdat, 0); /* call dsptch: wait for the set */
        return flag_exit();
    }
    /* fw_noff: someone else is already waiting on this flag */
    return flag_bexit(e_flag_underrun);
}
