/*
 * flag.c -- C port of src/kernel/FLAG.RTM.
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
 */
#include <stdint.h>

#include "defs.h"

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
static int flag_valid(const uint8_t *mem, uint8_t dl, uint16_t *si_out, uint16_t *bx_out) {
    if (dl >= mem[SD_NFLAGS]) return 0;
    uint16_t si = (uint16_t)(mem_r16(mem, SD_FLAGS) + dl * flglen);
    *si_out = si;
    *bx_out = mem_r16(mem, si);
    return 1;
}

/*
 * flag_set_entry -- Set Logical Interrupt Flag
 *	input:	dl = flag number
 *	output:	bx = 0 if okay, 0xFFFF if error
 *		cx = error code: 0, e_flag_ovrrun, or e_ill_flag
 */
flag_bxcx flag_set_entry(uint8_t *mem, uint8_t dl) {
    uint16_t si, bx;
    if (!flag_valid(mem, dl, &si, &bx)) return flag_bexit(e_ill_flag);

    if (mem[(uint16_t)(si + flg_ignore)] != flag_zig) {
        mem[(uint16_t)(si + flg_ignore)]--;
        return flag_exit();
    }
    /* fs_set */
    if (bx == flag_on) return flag_bexit(e_flag_ovrrun);
    if (bx == flag_off) {
        mem_w16(mem, (uint16_t)(si + flg_pd), flag_on);
        return flag_exit();
    }
    /* fs_noff: bx is a PD address waiting on this flag -- wake it */
    mem_w16(mem, (uint16_t)(bx + p_link), mem_r16(mem, SD_DRL));
    mem_w16(mem, SD_DRL, bx);
    mem[(uint16_t)(bx + p_stat)] = ps_run;
    mem_w16(mem, (uint16_t)(si + flg_pd), flag_off);
    return flag_exit();
}

/*
 * flag_wait_entry -- Wait for Logical Interrupt Flag
 *	input:	dl = flag number
 *	output:	bx = 0 if okay, 0xFFFF if error
 *		cx = error code: 0, e_flag_underrun, or e_ill_flag
 */
flag_bxcx flag_wait_entry(uint8_t *mem, uint8_t dl) {
    uint16_t si, bx;
    if (!flag_valid(mem, dl, &si, &bx)) return flag_bexit(e_ill_flag);

    if (bx == flag_on) {
        mem_w16(mem, (uint16_t)(si + flg_pd), flag_off);
        return flag_exit();
    }
    if (bx == flag_off) {
        uint16_t rlr_pd = mem_r16(mem, SD_RLR);
        mem[(uint16_t)(rlr_pd + p_stat)] = ps_flagwait;
        mem_w16(mem, UDA_DPARAM, si);
        /* call dsptch -- boundary stub, modeled as an instant no-op return */
        return flag_exit();
    }
    /* fw_noff: someone else is already waiting on this flag */
    return flag_bexit(e_flag_underrun);
}
