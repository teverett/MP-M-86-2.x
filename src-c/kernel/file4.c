/*
 * file4.c -- C port of a clean, self-contained subset of src-assem/kernel/
 * FILE4.BDO (the BDOS function-dispatch table): func24, func25,
 * func26, func29, func32, FUNC44, FUNC45, func51, func52, SET_ARET/openx06,
 * func15 (BDOS OPEN) + its own OPENX/OPENX0-OPENX09 cascade, func16
 * (BDOS CLOSE) + FUNC48 (flush buffers), func17/func18 (search for
 * first/next occurrence of a file, + internal `csearch`/
 * `reselectx`), func19 (delete), func20/func21 (sequential
 * read/write), func22 (MAKE), func23 (RENAME), func13/func14 (reset
 * disk system / select disk info) + func37 (reset drive, + internal
 * `reset_37`), func33/func34 (random read/write), func30 (set file
 * indicators), func35/func36 (file size, set random record), func40
 * (write random with zero fill) + FUNC46 (get disk free space), and
 * now func42/func43 (lock/unlock record).
 *
 * FILE4.BDO is now ported in full: every real function in its own
 * BDOS dispatch table has a C counterpart (`func13`-`func52`) --
 * `func26` (found genuinely missing, not merely deferred, while
 * wiring up `BDOSIF.BDO`'s own `entry`/`CALL_BDOS` dispatch table in
 * `bdosif.c`) is now ported too -- modulo
 * the handful that are bare `EQU FUNCRET` dispatch-table aliases with
 * no body of their own at all (`func38`/`func39`/`FUNC41`/`FUNC47`/
 * `FUNC49`/`FUNC50`, straight onto `BDOSIF.BDO`'s own empty `funcret:
 * ret`) and `GOBACK`, which is just a
 * 2-instruction tail jump into BDOSIF.BDO's own module-dispatch
 * epilogue (`BDOS_RETURN`), the same jump-table-dispatcher class
 * already deferred elsewhere -- not worth porting alone.
 *
 * `func19`-`func23` turned out to need a genuinely large amount of
 * new `FILE3.BDO` infrastructure (`src-c/kernel/file3.c`'s own
 * `diskread`/`sqread`, `diskwrite`/`sqwrite`, `test_disk_fcb`/
 * `test_lock`/`move_arecord`, `chk_xfcb_password1`, `init_xfcb`,
 * `set_pw`), plus a few small `FILE2.BDO` additions (`delet0`,
 * `sdcnt_eq_xdcnt`, non-`static` `dmposition`/`getdm`/`diocomp` in
 * `file1.c`) -- see each of those files' own header comments for the
 * detail on each piece. `func22` (MAKE) reuses `func15`'s own `OPENX1`/
 * `OPENX03`/`OPENX08` cascade directly (refactored out of the
 * previously-monolithic `openx` into their own functions for this),
 * entering at different points depending on whether a fresh olist
 * item is already known to be needed -- and hits its own instance of
 * the "`MAKE`'s own tail always leaves `ZF` clear" unreachable-branch
 * pattern `openreel` already found (see that function's own header
 * comment, `file2.c`).
 *
 * `func19`-`func23` all land clear of the DATA.BDO collision, but all
 * of them call into `file2.c`'s own colliding `search`/`close`/
 * `open`/`delet`/`rename_` clusters somewhere in their chain, so none
 * of them get a new differential test either, same reasoning as
 * `func15`'s own entry (`docs/src-c.md` has the full writeup).
 *
 * `func13`/`func14`/`func37` reuse `FILE3.BDO`'s own new `diskreset`
 * (see that function's own header comment for the per-disk olist
 * scan it does) and `file1.c`'s own `TEST_VECTOR1`/`SET_CDISK1` (both
 * split out of the already-ported `test_vector`/`setcdisk` to take
 * their own shift-amount/bit-position directly, since `diskreset`'s
 * own real asm calls them with `BD_CURDSK`, not `BD_SELDSK`). `func13`
 * calls `RESET_37` directly -- `func37`'s own real continuation after
 * its own `CALL DISKRESET` -- the same "call into the middle of
 * another function" pattern as `rename_`'s own `CALL DELETE1`.
 * `TEST_VECTOR1`/`SET_CDISK1` land inside the DATA.BDO collision
 * range (`file1.c`'s own header comment), so this cluster gets no new
 * differential test either, despite `diskreset`/`func13`/`func14`/
 * `func37` themselves being individually clear of it.
 *
 * `func33`/`func34` are both thin wrappers: `parsave36r` + `CHECK_FCB`
 * (one of its own real callers, same as `func20`/`func21`) + a tail
 * call into `rseek`'s own `rdread`/`rdwrite` (`file3.c`). Land clear
 * of the collision themselves, but `parsave36` (lands inside it, same
 * as `parsave`/`parsave33`) and `rdread`/`rdwrite`'s own transitive
 * `close`/`open`/`make`/`search` calls don't, so no new differential
 * test for these either.
 *
 * `func30` is another thin wrapper (`parsave33` + `copy_dma_in` +
 * `CHECK_WILD`/`RESELECTX`/`CHK_PASSWORD` + a tail call into
 * `indicators`, `file2.c`); the real asm's own `CALL indicators ! JMP
 * cpydirloc` is redundant (`indicators` already ends with its own
 * call into `cpydirloc`), so the second one isn't translated
 * separately. `func30` itself lands clear of the collision, but
 * `indicators` doesn't (`file2.c`'s own header comment), so no new
 * differential test here either.
 *
 * `func35`/`func36` are thin wrappers around `FILE3.BDO`'s own
 * `getfilesize`/`setrandom`. All land clear of the DATA.BDO
 * collision, and so do `setrandom`/`cmperr` themselves -- but
 * `getfilesize` calls into `file2.c`'s own colliding `search`/
 * `searchn`, so `func35` gets no new differential test; `func36`
 * doesn't touch that cluster at all (pure fcb-field arithmetic) and
 * would be a legitimate candidate for one, just not written yet
 * (time, not a technical blocker).
 *
 * `func16` replicates `CHECK_FCB1`'s own push-fallback/pop-on-success
 * convention by hand too, same as `func15` -- but its own fallback
 * target, `CLOSE00`, is a trivial `set_aret`-and-return, so it maps
 * straight onto `check_fcb1`'s own boolean return without needing a
 * `goto`. `FUNC48`'s own `FLUSH` (`BDOSIF.BDO`) is a tail jump into
 * the XIOS jump table, a boundary stub no-op same as `readf`/`writef`
 * (`file1.c`'s own `DIOCOMP`, now non-`static` for this, already
 * covers that half). Both land clear of the DATA.BDO flat-address
 * collision (`--symbols`: `func16`=0x2005, `FUNC48`=0x24d7), but
 * `func16` calls straight into `file2.c`'s own `close`/`close1`
 * cluster, which does *not* -- so, same as `func15`, no new
 * differential test for this pass either.
 *
 * `func17`/`func18` share `CSEARCH`'s own manual stack trick: it
 * `push`es `OFFSET dirtouser` then `jmp`s (not `call`s) into `SEARCH`/
 * `SEARCHN`, so that routine's own ordinary `ret` lands at `dirtouser`
 * instead of back in `CSEARCH` -- always, regardless of whether
 * anything was found, unlike every other manual-stack-trick case in
 * this port (`CHECK_FCB1`/`OPENX`), which only redirects on one
 * specific outcome. Modeled as a plain unconditional call into
 * `dirtouser` after `search`/`searchn`. `RESELECTX`'s own
 * `BD_ACTUAL_RC` write (from whatever `AL` happens to hold entering
 * it, along this call path) is genuinely incidental and never read
 * back by anything downstream here -- not modeled, same "undefined
 * artifact" convention as `selectdisk`'s own fixed `bx=0` (`file1.c`).
 * `func17`/`func18`/`csearch`/`reselectx` land clear of the DATA.BDO
 * collision, but `csearch` calls straight into `file2.c`'s own
 * colliding `search`/`searchn` cluster, so no new differential test
 * here either.
 *
 * `func15`/`openx`'s own full dependency chain (`file2.c`'s own
 * `open`/`OPEN1`/`opencopy`/`SWAP`/`SEARCHI`/`SET_DCNT_DBLK`/
 * `CHECK_WILD`; `file3.c`'s own `reselect`/`parsave33r`/
 * `COUNT_OPENS`/`CREATE_OLIST_ITEM`/`GET_DIR_MODE`/`GET_XFCB`/
 * `CMP_PW`/`STAMP1`; `file1.c`'s own `CHECK_FCB1`/`RO_TEST`) turned
 * out to already be almost entirely ported from earlier passes --
 * only `reselect`'s own cluster, `parsave`'s own family, `COUNT_OPENS`,
 * and `CREATE_OLIST_ITEM` were genuinely new (all added to `file3.c`
 * this same pass). `func15`'s own MPM branch replicates `CHECK_FCB1`'s
 * push-fallback/pop-on-success convention by hand -- see that
 * function's own header comment, `file1.c` -- but its real fallback
 * target (`OPEN_FILE`) and its own normal-success continuation turn
 * out to collapse into the same `goto open_file` here, since both
 * only diverge from it when `BD_HIGH_EXT==0x60`.
 *
 * `OPENX` itself never returns normally to `func15`'s own call sites
 * except on outright open failure -- on success it runs the *entire*
 * rest of the real BDOS OPEN call itself, the same manual-stack-trick
 * class as `CHECK_FCB1` (see `openx`'s own header comment for the
 * detail). `file2.c`'s own `open`/`OPEN1`/`opencopy` land inside
 * DATA.BDO's own dseg collision range in the combined wrapper bundle
 * (same class of gap as `searchn`/`CLOSE` elsewhere in this port, see
 * `file2.c`'s own header comment) -- so despite `func15`/`openx`
 * themselves being individually clear of it, no differential test
 * has been written for any of this, since it can't exercise the real
 * open/openx path without going through the colliding `open` call.
 *
 * `SET_ARET`/`openx06` were `FILE3.BDO`'s own two remaining blockers
 * for `TST_OLIST`/`CHK_OLIST` (`src-c/kernel/file3.c`'s own same
 * pass). `set_aret`'s own real tail (`REPORT_ERR`'s message-
 * formatting, reached when `BD_ERROR_MODE != 0xFF`) is pure display,
 * no observable effect under the convention -- unlike `file1.c`'s
 * own `goerr`, which shares this same `REPORT_ERR` tail but has one
 * real side effect of its own (`pf_ctlc`) checked *before* reaching
 * it, `set_aret` has no such check, so nothing here is observable
 * either way; not translated. Neither is `jmp GOBACK` itself, same
 * reasoning as every other file's own natural exit boundary.
 *
 * Runs against real memory -- see file1.c's header. `rlr` and the PDs
 * are DS (SYSDAT)-relative; the UDA fields these routines touch
 * (`u_dma_ofst`, `u_dma_seg`, `u_error_mode`, `u_mult_cnt`, `u_retseg`)
 * are the running process's UDA, reached through ES in the asm (BDOSIF
 * dispatches these functions directly, with ES still the caller's UDA).
 */
#include <stdint.h>

#include "defs.h"

extern void parsave33r(uint8_t *phys, uint16_t ds_bdos);
extern void copy_dma_in(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern void clrmodnum(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t check_fcb1(uint8_t *phys, uint16_t ds_bdos);
extern void home(uint8_t *phys, uint16_t ds_bdos);
extern void setenddir(uint8_t *phys, uint16_t ds_bdos);
extern void reset_checksum_fcb(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t check_wild(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t get_atts(uint8_t *phys, uint16_t ds_bdos);
extern void open(uint8_t *phys, uint16_t ds_bdos);
extern void swap_sdcnt(uint8_t *phys, uint16_t ds_bdos);
extern void set_dcnt_dblk(uint8_t *phys, uint16_t ds_bdos);
extern void searchi(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern uint8_t searchn(uint8_t *phys, uint16_t ds_bdos);
extern void open1(uint8_t *phys, uint16_t ds_bdos, uint8_t found);
extern uint8_t endofdir(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t ro_test(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern uint8_t get_dir_mode(uint8_t *phys, uint16_t ds_bdos);
extern get_xfcb_result get_xfcb(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t cmp_pw(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern void pack_sdcnt(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t search_olist(uint8_t *phys, uint16_t ds_bdos, uint8_t ch);
extern uint8_t searchn_list(uint8_t *phys, uint16_t ds_bdos, uint8_t ch);
extern uint8_t compare_pds(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern uint8_t count_opens(uint8_t *phys, uint16_t ds_bdos);
extern void create_olist_item(uint8_t *phys, uint16_t ds_bdos);
extern uint16_t get_rra(uint8_t *phys, uint16_t ds_bdos);
extern void stamp1(uint8_t *phys, uint16_t ds_bdos);
extern void seekcopy(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t nowrite(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t getmodnum(uint8_t *phys, uint16_t ds_bdos, uint16_t *bx_out);
extern void close(uint8_t *phys, uint16_t ds_bdos);
extern void close1(uint8_t *phys, uint16_t ds_bdos);
extern void delete_item(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern void remove_locks(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern void stamp2(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t diocomp(uint8_t *phys, uint16_t ds_bdos, uint8_t al);
extern void report_err(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern void xcurselect(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t intrnl_disk_reset(uint8_t *phys, uint16_t ds_bdos, uint16_t bx);
extern void setro(uint8_t *phys, uint16_t ds_bdos);
extern void parsave(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern void parunsave(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t set_pw0(uint8_t *phys, uint16_t ds_bdos, uint16_t saved_bx, uint16_t si, uint16_t src,
                       uint16_t *bx_out);
extern void curselect(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t search(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern uint16_t getexta(uint8_t *phys, uint16_t ds_bdos);
extern uint16_t getdptra(uint8_t *phys, uint16_t ds_bdos);
extern void dirtouser(uint8_t *phys, uint16_t ds_bdos);
extern void move(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t bx, uint8_t cl);
extern void parsave33(uint8_t *phys, uint16_t ds_bdos);
extern void clr_ext(uint8_t *phys, uint16_t ds_bdos);
extern void delet(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t check_fcb(uint8_t *phys, uint16_t ds_bdos);
extern void sqread(uint8_t *phys, uint16_t ds_bdos);
extern void sqwrite(uint8_t *phys, uint16_t ds_bdos);
extern void reselect(uint8_t *phys, uint16_t ds_bdos);
extern void reselectx(uint8_t *phys, uint16_t ds_bdos); /* RESELECTX, now ported for real -- see below */
extern void make(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t chk_xfcb_password1(uint8_t *phys, uint16_t ds_bdos, uint16_t bx, uint8_t al);
extern void delet0(uint8_t *phys, uint16_t ds_bdos, uint8_t found);
extern void init_xfcb(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t set_pw(uint8_t *phys, uint16_t ds_bdos, uint16_t bx, uint16_t *bx_out);
extern void sdcnt_eq_xdcnt(uint8_t *phys, uint16_t ds_bdos);
extern void fix_olist_item(uint8_t *phys, uint16_t ds_bdos);
extern void rename_(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t diskreset(uint8_t *phys, uint16_t ds_bdos);
extern uint16_t test_vector1(uint16_t dx, uint8_t cl);
extern void set_seldsk(uint8_t *phys, uint16_t ds_bdos);
extern void parsave36r(uint8_t *phys, uint16_t ds_bdos);
extern void parsave36(uint8_t *phys, uint16_t ds_bdos);
extern void rdread(uint8_t *phys, uint16_t ds_bdos);
extern void rdwrite(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t chk_password(uint8_t *phys, uint16_t ds_bdos);
extern void indicators(uint8_t *phys, uint16_t ds_bdos);
extern void getfilesize(uint8_t *phys, uint16_t ds_bdos);
extern void setrandom(uint8_t *phys, uint16_t ds_bdos, uint16_t base);
extern uint8_t rseek1(uint8_t *phys, uint16_t ds_bdos, uint8_t read_mode);
extern void diskwrite(uint8_t *phys, uint16_t ds_bdos);
extern void tmp_select(uint8_t *phys, uint16_t ds_bdos, uint8_t dl);
extern uint16_t get_nalbs(uint8_t *phys, uint16_t ds_bdos);
extern void copy_dma_out(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern void lock_(uint8_t *phys, uint16_t ds_bdos, uint8_t lock_unlock);
extern void openx1(uint8_t *phys, uint16_t ds_bdos);

/* func24 -- return the login vector
 *	output:	returns bx = dlog, *cx_out = 0
 */
uint16_t func24_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    *cx_out = 0;
    return seg_r16(ds, BD_DLOG);
}

/* func25 -- return selected disk number
 *	output:	returns bx = the running PD's own current default disk
 *		(zero-extended), *cx_out = 0
 */
uint16_t func25_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = seg_r16(ds, SYSDAT_RLR);
    *cx_out = 0;
    return ds[(uint16_t)(bx + p_dsk)];
}

/* func29 -- return r/o bit vector
 *	output:	returns bx = rodsk, *cx_out = 0
 */
uint16_t func29_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    *cx_out = 0;
    return seg_r16(ds, BD_RODSK);
}

/* func32 -- set user code
 *	input:	dl = new user code, or 0xFF to interrogate the current one
 *	output:	returns bx = the running PD's own current user code
 *		(zero-extended) when interrogating, else 0; *cx_out = 0
 */
uint16_t func32_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t si = seg_r16(ds, SYSDAT_RLR);
    *cx_out = 0;
    if (dl == 0xFF) {
        return ds[(uint16_t)(si + p_user)];
    }
    ds[(uint16_t)(si + p_user)] = (uint8_t)(dl & 0x0F);
    return 0;
}

/* func44 -- set multi-sector count
 *	input:	dl = new count (valid range 1-16)
 *	output:	*bx_out = 0xFF, *cx_out = 0 if dl is out of range
 *		(u_mult_cnt untouched); else *bx_out = 0, *cx_out = 0
 */
void func44_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    *cx_out = 0;
    if (dl == 0 || dl >= 17) {
        *bx_out = 0xFF;
        return;
    }
    seg_w8(far_ptr(phys, rlr_uda(ds), 0), u_mult_cnt, dl);
    *bx_out = 0;
}

/* func45 -- set bdos error mode
 *	input:	dl = new error mode
 *	output:	*bx_out = 0, *cx_out = 0
 */
void func45_entry(uint8_t *phys, uint16_t ds_bdos, uint8_t dl, uint16_t *bx_out, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(far_ptr(phys, rlr_uda(ds), 0), u_error_mode, dl);
    *bx_out = 0;
    *cx_out = 0;
}

/* func26 -- set the subsequent dma address (MPM branch; CPM's own
 * `dmaad`/`setdata` branch is dead in this tree, same convention as
 * every other `if CPM`/`if MPM` split elsewhere)
 *	input:	dx = new dma offset
 *	output:	u_dma_ofst = dx; *bx_out = 0, *cx_out = 0
 */
void func26_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(far_ptr(phys, rlr_uda(ds), 0), u_dma_ofst, dx);
    *bx_out = 0;
    *cx_out = 0;
}

/* func51 -- set dma base
 *	input:	dx = new dma segment
 *	output:	*bx_out = 0, *cx_out = 0
 */
void func51_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t dx, uint16_t *bx_out, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(far_ptr(phys, rlr_uda(ds), 0), u_dma_seg, dx);
    *bx_out = 0;
    *cx_out = 0;
}

/* func52 -- get dma
 *	output:	returnseg = DS (the BDOS's data segment, SYSDAT -- SUPIF
 *		enters the BDOS with DS = cs:sysdat), u_retseg = u_dma_seg,
 *		returns bx = u_dma_ofst, *cx_out = 0
 */
uint16_t func52_entry(uint8_t *phys, uint16_t ds_bdos, uint16_t *cx_out) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint8_t *uda = far_ptr(phys, rlr_uda(ds), 0);
    seg_w16(ds, BD_RETURNSEG, ds_bdos);
    seg_w16(uda, u_retseg, seg_r16(uda, u_dma_seg));
    *cx_out = 0;
    return seg_r16(uda, u_dma_ofst);
}

/*
 * GOBACK -- `mov sp,save_sp ! jmp bdos_return`: back out of the running
 * BDOS function to CALL_BDOS's exit (bdosif.c sets the save point). It's
 * armed only inside a real BDOS call, so the host tests, which call the
 * functions directly, carry on as before. Built only for kernel.elf
 * (i686 GCC's __builtin_setjmp/longjmp; the host's arm64 clang has
 * neither), where one process at a time is in the BDOS (MXdisk).
 */
void *bdos_save_sp[5];
int bdos_save_sp_armed;

void bdos_goback(void) {
#if BDOS_GOBACK
    if (bdos_save_sp_armed) __builtin_longjmp(bdos_save_sp, 1);
#endif
}

/*
 * set_aret -- SET_ARET: report a BDOS error (al = error index):
 * ARET's own high byte (the extended error code) is set to al, and
 * ARET's own low byte (BD_LRET) is forced to 0xFF (the generic
 * "failed" sentinel, via the real asm's own shared `LRET_EQ_FF`,
 * `src-c/kernel/file2.c`'s own `searchn`). Then GOBACK if the error
 * mode is "return errors" (0FFh), else REPORT_ERR with the extended
 * error's message (XERR_LIST, from error 3) -- neither returns in a
 * real BDOS call. Not `static`: `src-c/kernel/
 * file3.c`'s own `chk_pw` also tail-calls it directly (with a
 * different error index than `openx06`'s own fixed 5), an ordinary
 * cross-file call, same as `que1.c`'s own into `que2.c`.
 */
void set_aret(uint8_t *phys, uint16_t ds_bdos, uint8_t al) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, (uint16_t)(BD_ARET + 1), al);
    seg_w8(ds, BD_LRET, 0xFF);
    if (seg_r8(ds, BD_ERROR_MODE) == 0xFF) {
        bdos_goback(); /* return physical & extended errors */
        return;
    }
    uint16_t cx = (uint16_t)((uint8_t)(al - 3) * 2);
    report_err(phys, ds_bdos, seg_r16(ds, (uint16_t)(BD_XERR_LIST + cx)));
}

/*
 * openx06 -- error: file opened by another process in an
 * incompatible mode. Never returns normally, see `set_aret`'s own
 * header comment.
 */
void openx06(uint8_t *phys, uint16_t ds_bdos) {
    set_aret(phys, ds_bdos, 5);
}

/*
 * file_exists -- error: a rename's own new filename already exists.
 * Never returns normally, see `set_aret`'s own header comment.
 */
void file_exists(uint8_t *phys, uint16_t ds_bdos) {
    set_aret(phys, ds_bdos, 8);
}

/* openx2 -- OPENX2: place the create/update time stamp in the xfcb */
static void openx2(uint8_t *phys, uint16_t ds_bdos) {
    stamp1(phys, ds_bdos);
    seekcopy(phys, ds_bdos);
}

/*
 * openx08 -- OPENX08/OPENX09: shared tail once an olist item for this
 * open/make is known (BD_CUR_POS) -- record the item's own address in
 * the fcb's ranrec field for an unlocked-mode open, then (skipped for
 * MAKE, which calls this via `openx1`/`openx03_create` too) stamp the
 * xfcb if date/time stamping is active and an xfcb was actually found.
 */
static void openx08(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (seg_r8(ds, BD_ATTRIBUTES) & 0x40) {
        uint16_t rra = get_rra(phys, ds_bdos);
        seg_w16(ds, rra, seg_r16(ds, BD_CUR_POS));
        seg_w8(ds, BD_PARLG, 35);
    }
    seg_w8(ds, BD_COMP_FCB_CKS, 0xFF); /* true */
    if (seg_r8(ds, BD_MAKE_FLAG)) {
        return; /* ret */
    }
    uint8_t dirlbl = get_dir_mode(phys, ds_bdos);
    if (!(dirlbl & 0x40)) {
        return; /* ret32 -- no date-stamping active */
    }
    if (endofdir(phys, ds_bdos)) {
        return; /* ret32 -- xfcb not found */
    }
    if (nowrite(phys, ds_bdos)) {
        return; /* ret32 -- disk read-only */
    }
    openx2(phys, ds_bdos);
}

/*
 * openx03 -- OPENX03/OPENX034/OPENX035: create a fresh olist item,
 * erroring out if this process already has too many files open.
 * Not `static`: `src-c/kernel/file4.c`'s own `func22` (BDOS MAKE)
 * also enters here directly (skipping `openx03_create`'s own
 * free-list-empty check, already known false there).
 *	output: returns 1 on success (a new item now at BD_CUR_POS), 0
 *		on error (`set_aret` already called)
 */
uint8_t openx03(uint8_t *phys, uint16_t ds_bdos) {
    if (!count_opens(phys, ds_bdos)) {
        set_aret(phys, ds_bdos, 10); /* openx034 -- too many open files */
        return 0;
    }
    create_olist_item(phys, ds_bdos); /* openx035 */
    return 1;
}

/*
 * openx03_create -- OPENX01/OPENX02: create a fresh olist item for
 * this open/make, erroring out first if the olist's own free list is
 * empty, then via `openx03`.
 */
static uint8_t openx03_create(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (seg_r16(ds, BD_FREE_ROOT) == 0) {
        set_aret(phys, ds_bdos, 11); /* openx02 -- olist full */
        return 0;
    }
    return openx03(phys, ds_bdos);
}

/*
 * openx05 -- OPENX05: this olist item was already open -- bump its
 * own open count and finish via `openx08`.
 */
static void openx05(uint8_t *phys, uint16_t ds_bdos, uint16_t item) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, (uint16_t)(item + 8), (uint16_t)(seg_r16(ds, (uint16_t)(item + 8)) + 1));
    openx08(phys, ds_bdos);
}

/*
 * openx04 -- OPENX04/OPENX07: an existing olist item matched this
 * open's own drive; if its attributes agree, either it belongs to
 * this process (bump it, `openx05`) or another process has it open
 * in an incompatible mode (`openx06`, error) depending on locked/
 * unlocked mode and (for unlocked mode) `compare_pds`.
 */
static void openx04(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t item = seg_r16(ds, BD_CUR_POS);
    uint8_t al = (uint8_t)(seg_r8(ds, BD_ATTRIBUTES) | ds[(uint16_t)(item + 2)]);
    if (al != ds[(uint16_t)(item + 2)]) {
        openx06(phys, ds_bdos);
        return;
    }
    if (al & 0x80) {
        /* openx07 */
        if (!compare_pds(phys, ds_bdos, item)) {
            openx06(phys, ds_bdos);
            return;
        }
        openx05(phys, ds_bdos, item);
        return;
    }
    /* has this file been opened by this process already? */
    seg_w16(ds, BD_CUR_POS, seg_r16(ds, BD_PRV_POS));
    if (!searchn_list(phys, ds_bdos, 5)) {
        /* openx01 -- no match: treat as a fresh olist item */
        if (!openx03_create(phys, ds_bdos)) {
            return;
        }
        openx08(phys, ds_bdos);
        return;
    }
    openx05(phys, ds_bdos, seg_r16(ds, BD_CUR_POS));
}

/*
 * openx1 -- OPENX1: is this file already open (by this process or
 * another)? Dispatches to `openx04`'s own cascade if so, otherwise
 * creates a fresh olist item (`openx03_create`) and finishes
 * (`openx08`). `FILE4.BDO`'s own `func22` (BDOS MAKE) also enters
 * here directly, once its own fcb's own module/extent fields are
 * set up, same real entry point `OPEN`'s own cascade uses.
 */
void openx1(uint8_t *phys, uint16_t ds_bdos) {
    pack_sdcnt(phys, ds_bdos);
    if (search_olist(phys, ds_bdos, 3)) {
        openx04(phys, ds_bdos);
        return;
    }
    if (!openx03_create(phys, ds_bdos)) {
        return;
    }
    openx08(phys, ds_bdos);
}

/*
 * openx -- OPENX: the real asm's own `POP BX ;DISCARD RETURN ADDRESS`
 * right after a successful `endofdir` check means this never returns
 * normally to `func15`'s own `CALL_OPEN`/`OPEN_USER_ZERO` call sites
 * except on outright failure (the manual-stack-trick convention,
 * same as `check_fcb1`'s own header comment, `file1.c`) -- on
 * success it instead runs the *entire* rest of the real OPEN call
 * itself (OPENX0 through OPENX2's own final `ret`/`jmp goback`),
 * which is why this single function covers all of OPENX/OPENX0/
 * OPENX00/OPENX1/OPENX2/OPENX04-09 at once.
 *	output: returns 1 if the real asm's own `RET30` was taken (open
 *		failed outright -- the caller must itself continue
 *		exactly as `func15`'s own retry-under-user-0 logic
 *		does); returns 0 once this function has already run
 *		the whole rest of the open to completion (success *or*
 *		a later password/olist error -- either way nothing is
 *		left for the caller to do)
 *
 * The MPM branch's own second `OR AL,AL ;IS USER NUMBER ZERO? ;JNZ
 * OPENX0` (right after `MOV BX,INFO`) tests the same AL the first
 * `OR AL,AL ;JNZ OPENX0` just proved is 0 -- nothing between the two
 * touches AL -- so it can never actually branch; not modeled, see
 * this file's own "translate faithfully but note unreachable"
 * convention precedent (`docs/src-c.md`).
 */
uint8_t openx(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (endofdir(phys, ds_bdos)) {
        return 1; /* RET30 */
    }

    uint8_t al = seg_r8(ds, BD_HIGH_EXT);
    if (al != 0x60) {
        if (al != 0) {
            goto openx0;
        }
        uint16_t fcb = seg_r16(ds, BD_INFO);
        if (!ro_test(phys, ds_bdos, fcb)) {
            goto openx0;
        }
        if (!(ds[(uint16_t)(fcb + fcb_type + 1)] & 0x80)) {
            goto openx0;
        }
    }
    /* openx00 */
    {
        uint16_t fcb = seg_r16(ds, BD_INFO);
        seg_w8(ds, BD_ATTRIBUTES, 0x20);
        if (ds[(uint16_t)(fcb + fcb_type + 1)] & 0x80) {
            goto openx0;
        }
        seg_w8(ds, BD_HIGH_EXT, 0);
        seg_w8(ds, BD_LRET, 0xFF); /* jmp LRET_EQ_FF */
        return 0;
    }

openx0:
    reset_checksum_fcb(phys, ds_bdos);
    {
        uint8_t dirlbl = get_dir_mode(phys, ds_bdos);
        if (dirlbl != 0) {
            get_xfcb_result r = get_xfcb(phys, ds_bdos);
            uint8_t pwal = r.al;
            if ((pwal & 0xC0) != 0 && (dirlbl & 0x80) != 0) {
                uint8_t pwmode = (uint8_t)(pwal & 0xC0);
                if (!cmp_pw(phys, ds_bdos, r.bx)) {
                    if (!(pwmode & 0x40)) {
                        set_aret(phys, ds_bdos, 7); /* CHK_PW1 -- password error */
                        return 0;
                    }
                    seg_w8(ds, BD_XFCB_READ_ONLY, 0x80);
                }
            }
        }
    }
    openx1(phys, ds_bdos);
    return 0;
}

/*
 * func15 -- BDOS function 15, OPEN: copy the caller's own fcb in,
 * validate/checksum it (replicating `CHECK_FCB1`'s own push-
 * fallback/pop-on-success convention by hand -- see that function's
 * own header comment, `file1.c` -- its real fallback target and
 * `OPEN_FILE`'s own normal-success continuation turn out to be the
 * same place whenever `BD_HIGH_EXT` isn't `0x60`, so both collapse
 * into one `goto open_file`), determine the open mode, and attempt
 * the open -- retrying once under user 0 in read-only mode if the
 * first attempt fails and this isn't already a user-0 open.
 */
void func15(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave33r(phys, ds_bdos);
    copy_dma_in(phys, ds_bdos, 8);
    clrmodnum(phys, ds_bdos);

    seg_w8(ds, BD_MAKE_FLAG, 0);
    seg_w8(ds, (uint16_t)(BD_SDCNT + 1), 0xFF);
    seg_w8(ds, BD_CHECK_FCB_RET, 0xFF); /* true */
    uint8_t valid = check_fcb1(phys, ds_bdos);
    if (!valid || seg_r8(ds, BD_HIGH_EXT) != 0x60) {
        goto open_file;
    }
    home(phys, ds_bdos);
    setenddir(phys, ds_bdos);
    goto open_user_zero;

open_file:
    seg_w8(ds, seg_r16(ds, BD_INFO), seg_r8(ds, BD_USRCODE)); /* fcb(0) = usrcode */
    seg_w8(ds, (uint16_t)(BD_SDCNT + 1), 0xFF);

    reset_checksum_fcb(phys, ds_bdos);
    (void)check_wild(phys, ds_bdos);
    {
        uint8_t al = (uint8_t)(get_atts(phys, ds_bdos) & 0xC0);
        if (al == 0xC0) {
            /* both open-mode bits on: default to locked-only */
            al = (uint8_t)(al & 0x40);
        }
        seg_w8(ds, BD_HIGH_EXT, al);
        /* ATT_SET: ah = al>>1 (al is always 0/0x40/0x80 here, so ah
         * is always 0/0x20/0x40); 0x80 (read/only) only when ah==0 */
        {
            uint8_t ah = (uint8_t)(al >> 1);
            seg_w8(ds, BD_ATTRIBUTES, (ah == 0) ? 0x80 : ah);
        }

        if (al & 0x80) {
            goto call_open;
        }
        if (seg_r8(ds, BD_USRCODE) == 0) {
            goto call_open;
        }
        seg_w8(ds, (uint16_t)(BD_XDCNT + 1), 0xFE);
        seg_w8(ds, BD_SEARCH_USER0, 0xFF);
        seg_w8(ds, (uint16_t)(BD_SDCNT0 + 1), 0xFF);
    }

call_open:
    open(phys, ds_bdos);
    if (openx(phys, ds_bdos)) {
        /* open failed outright -- try once more under user 0 */
        if (!seg_r8(ds, BD_SEARCH_USER0)) {
            return; /* ret30 */
        }
        seg_w8(ds, BD_SEARCH_USER0, 0);
        if (seg_r8(ds, (uint16_t)(BD_XDCNT + 1)) == 0xFE) {
            return; /* ret30 -- file doesn't exist under user 0 either */
        }
        swap_sdcnt(phys, ds_bdos);
        set_dcnt_dblk(phys, ds_bdos);
        seg_w8(ds, BD_HIGH_EXT, 0x60);
    } else {
        return; /* openx already ran the whole open to completion */
    }

open_user_zero:
    seg_w8(ds, seg_r16(ds, BD_INFO), 0);
    searchi(phys, ds_bdos, namlen);
    uint8_t found = searchn(phys, ds_bdos);
    open1(phys, ds_bdos, found);
    (void)openx(phys, ds_bdos);
}

/*
 * func48 -- BDOS function 48, flush blocking/deblocking buffers:
 * `FLUSH` (`BDOSIF.BDO`, `mov al,io_flush ! jmps xiosif`), then
 * `DIOCOMP` on its result.
 */
void func48(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t al = (uint8_t)XIOSIF(bdos, phys, ds_bdos, io_flush, 0, 0); /* call FLUSH */
    (void)diocomp(phys, ds_bdos, al);
}

/*
 * func16 -- BDOS function 16, CLOSE: replicates `CHECK_FCB1`'s own
 * push-fallback/pop-on-success convention by hand, same as `func15`
 * (see that function's own header comment) -- its own fallback
 * target, `CLOSE00`, sets a checksum-error and returns, so it maps
 * directly to an early `set_aret`/return instead of a `goto`. A
 * partial close (`FCB(MODNUM)` bit 7 set) skips the directory update
 * via `BD_DONT_CLOSE`; a full close that was the file's own last
 * open reference also removes its olist entry (and, if unlocked, its
 * own lock-list entries), then stamps/rewrites the xfcb if date/time
 * stamping is active.
 */
void func16(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave33r(phys, ds_bdos);
    seg_w8(ds, BD_ATTRIBUTES, get_atts(phys, ds_bdos));

    seg_w8(ds, BD_CHECK_FCB_RET, 0xFF); /* true */
    if (!check_fcb1(phys, ds_bdos)) {
        set_aret(phys, ds_bdos, 6); /* close00 -- checksum error */
        return;
    }
    seg_w8(ds, (uint16_t)(BD_SDCNT + 1), 0xFF);
    uint16_t modbx;
    uint8_t modnum = getmodnum(phys, ds_bdos, &modbx);
    if (modnum & 0x80) {
        /* close01 -- partial close, don't update the directory fcb */
        seg_w8(ds, BD_DONT_CLOSE, 0xFF); /* true */
        close1(phys, ds_bdos);
    } else {
        close(phys, ds_bdos); /* close02 -- full close */
    }

    if (seg_r8(ds, BD_LRET) == 0xFF) {
        return; /* ret34 -- error on close */
    }
    func48(phys, ds_bdos); /* flush blocking/deblocking buffers */
    if (seg_r8(ds, BD_ATTRIBUTES) & 0x80) {
        return; /* ret34 -- partial close, nothing more to do */
    }

    pack_sdcnt(phys, ds_bdos);
    /* find this process's own olist item for this file */
    if (search_olist(phys, ds_bdos, 5)) {
        uint16_t item = seg_r16(ds, BD_CUR_POS);
        uint16_t opncnt = (uint16_t)(seg_r16(ds, (uint16_t)(item + 8)) - 1);
        seg_w16(ds, (uint16_t)(item + 8), opncnt);
        if (opncnt == 0xFFFF) {
            /* last reference closed -- remove the olist entry */
            seg_w16(ds, BD_FILE_ID, item);
            delete_item(phys, ds_bdos, item);
            reset_checksum_fcb(phys, ds_bdos);
            if (seg_r8(ds, BD_HIGH_EXT) & 0x80) {
                remove_locks(phys, ds_bdos, seg_r16(ds, BD_FILE_ID));
            }
        }
    }
    /* close03 */

    if (!(getmodnum(phys, ds_bdos, &modbx) & 0x40)) {
        return; /* ret34 -- file write flag not set */
    }
    if (!(get_dir_mode(phys, ds_bdos) & 0x20)) {
        return; /* ret34 -- update timestamping not active */
    }
    if (!get_xfcb(phys, ds_bdos).found) {
        return; /* ret34 -- no xfcb */
    }
    stamp2(phys, ds_bdos);
    seekcopy(phys, ds_bdos);
}

/*
 * csearch -- CSEARCH: shared tail for `func17` (search mode) and
 * `func18` (searchn mode, `is_searchn` true) -- a wildcard drive byte
 * (`FCB(0)=='?'`) searches the whole currently-selected disk for
 * every fcb (`CURSELECT`, `cl=0`); otherwise `RESELECTX` picks the
 * fcb's own drive and `cl=NAMLEN`. The real asm's own manual stack
 * trick (`push OFFSET dirtouser` then `jmp SEARCH`/`jmp SEARCHN`,
 * letting that routine's own real `ret` land there instead of coming
 * back here) always runs `dirtouser` afterward regardless of whether
 * anything was found, so it's modeled as a plain unconditional call.
 */
static void csearch(uint8_t *phys, uint16_t ds_bdos, uint8_t is_searchn) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t bx = seg_r16(ds, BD_INFO);
    uint8_t cl;
    if (ds[bx] == '?') {
        curselect(phys, ds_bdos);
        cl = 0;
    } else {
        uint16_t extbx = getexta(phys, ds_bdos);
        if (ds[extbx] != '?') {
            clr_ext(phys, ds_bdos);
            clrmodnum(phys, ds_bdos);
        }
        reselectx(phys, ds_bdos);
        cl = namlen;
    }
    if (is_searchn) {
        (void)searchn(phys, ds_bdos);
    } else {
        (void)search(phys, ds_bdos, cl);
    }
    dirtouser(phys, ds_bdos);
}

/*
 * func17 -- BDOS function 17, search for the first occurrence of a
 * file: save the caller's own fcb location (`BD_SRCHABASE`/
 * `BD_SRCHAOFST`, restored by `func18`), then `csearch` in search
 * mode.
 */
void func17(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_SRCHABASE, seg_r16(ds, BD_PARAMETERSEGMENT));
    seg_w16(ds, BD_SRCHAOFST, seg_r16(ds, BD_INFO));
    parsave33(phys, ds_bdos);
    csearch(phys, ds_bdos, 0);
}

/*
 * func18 -- BDOS function 18, search for the next occurrence: copy
 * the user's own dma buffer back into the directory buffer (the
 * caller may have modified the previous match), restore `func17`'s
 * own saved fcb location, then `csearch` in searchn mode. The dma
 * buffer is at DMABASE:dmaad (the asm's `mov ds,dmabase`).
 */
void func18(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    /* copy user's dma to dir buffer: DMABASE:dmaad -> DS:buffa */
    const uint8_t *dma = far_ptr(phys, seg_r16(ds, BD_DMABASE), 0);
    uint16_t si = seg_r16(ds, BD_DMAAD), di = seg_r16(ds, BD_BUFFA);
    for (uint8_t i = 0; i < 128; i++) {
        seg_w8(ds, (uint16_t)(di + i), seg_r8(dma, (uint16_t)(si + i)));
    }
    seg_w16(ds, BD_PARAMETERSEGMENT, seg_r16(ds, BD_SRCHABASE));
    seg_w16(ds, BD_INFO, seg_r16(ds, BD_SRCHAOFST));
    parsave33(phys, ds_bdos);
    seg_w16(ds, BD_SRCHA, seg_r16(ds, BD_INFO));
    csearch(phys, ds_bdos, 1);
}

/* func19 -- BDOS function 19, delete a file */
void func19(uint8_t *phys, uint16_t ds_bdos) {
    parsave33r(phys, ds_bdos);
    copy_dma_in(phys, ds_bdos, 8);
    delet(phys, ds_bdos);
}

/*
 * func20 -- BDOS function 20, sequential read. `CHECK_FCB`'s own
 * header comment (`file1.c`) has the detail on why this is one of
 * its two real callers.
 */
void func20(uint8_t *phys, uint16_t ds_bdos) {
    parsave33r(phys, ds_bdos);
    if (!check_fcb(phys, ds_bdos)) {
        return;
    }
    sqread(phys, ds_bdos);
}

/* func21 -- BDOS function 21, sequential write */
void func21(uint8_t *phys, uint16_t ds_bdos) {
    parsave33r(phys, ds_bdos);
    if (!check_fcb(phys, ds_bdos)) {
        return;
    }
    sqwrite(phys, ds_bdos);
}

/*
 * func22 -- BDOS function 22, MAKE: create a new file. Reuses the
 * `OPENX1`/`OPENX03`/`OPENX08` cascade `func15`'s own `openx` already
 * builds (see those functions' own header comments), entering it at
 * different points depending on whether a fresh olist item is
 * already known to be needed.
 *
 * The real asm's own `CALL MAKE; JNZ MAKE00` (right after setting
 * `BD_MAKE_XFCB`) can never take the "make failed" branch: `MAKE`'s
 * own real tail (`setfwf`'s `OR AL,fwfmsk`) always leaves `AL`
 * nonzero/ZF clear, the same unreachable pattern `openreel`'s own
 * `jz openerr` hits after its own call to `MAKE` (`file2.c`'s own
 * header comment) -- so the fallback "delete the fcb just created,
 * report no room" path is dead code, not translated.
 */
void func22(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave33(phys, ds_bdos);
    copy_dma_in(phys, ds_bdos, 9);
    seg_w8(ds, BD_ATTRIBUTES, get_atts(phys, ds_bdos));
    seg_w8(ds, BD_FCB_EXISTS, 0);
    clr_ext(phys, ds_bdos);
    clrmodnum(phys, ds_bdos);
    reselect(phys, ds_bdos);
    if (!check_wild(phys, ds_bdos)) {
        return;
    }
    seg_w16(ds, BD_XDCNT, 0xFFFF);
    seg_w8(ds, (uint16_t)(BD_SDCNT + 1), 0xFF);

    open(phys, ds_bdos);
    reset_checksum_fcb(phys, ds_bdos);
    if (!endofdir(phys, ds_bdos)) {
        file_exists(phys, ds_bdos); /* error -- file already exists */
        return;
    }

    {
        uint8_t al = (uint8_t)((seg_r8(ds, BD_ATTRIBUTES) & 0x80) >> 1);
        if (al == 0) {
            al = 0x80;
        }
        seg_w8(ds, BD_MAKE_FLAG, al);
    }

    uint8_t at_makex01 = 0;
    if (seg_r8(ds, (uint16_t)(BD_SDCNT + 1)) == 0xFF) {
        at_makex01 = 1;
    } else {
        pack_sdcnt(phys, ds_bdos);
        if (!search_olist(phys, ds_bdos, 3)) {
            at_makex01 = 1;
        }
    }
    if (at_makex01) {
        if (seg_r16(ds, BD_FREE_ROOT) == 0) {
            set_aret(phys, ds_bdos, 11); /* openx02 -- olist full */
            return;
        }
        /* falls into make_x03 */
    } else {
        /* makex02 */
        uint16_t item = seg_r16(ds, BD_CUR_POS);
        uint8_t al = (uint8_t)(seg_r8(ds, BD_MAKE_FLAG) & ds[(uint16_t)(item + 2)]);
        if (al == 0) {
            openx06(phys, ds_bdos);
            return;
        }
        if (!compare_pds(phys, ds_bdos, item)) {
            if (seg_r8(ds, BD_MAKE_FLAG) & 0x80) {
                openx06(phys, ds_bdos);
                return;
            }
        }
        /* falls into make_x03 */
    }

    /* make_x03 */
    if (seg_r8(ds, BD_FCB_EXISTS) && (get_dir_mode(phys, ds_bdos) & 0x80)) {
        get_xfcb_result r = get_xfcb(phys, ds_bdos);
        if (r.al & 0xC0) {
            if (!chk_xfcb_password1(phys, ds_bdos, r.bx, (uint8_t)(r.al & 0xC0))) {
                return;
            }
        }
    }

    /* makex04 */
    make(phys, ds_bdos);
    reset_checksum_fcb(phys, ds_bdos);
    if (endofdir(phys, ds_bdos)) {
        return; /* ret36 -- no room in directory */
    }

    {
        uint8_t dirlbl = get_dir_mode(phys, ds_bdos);
        if (dirlbl == 0) {
            goto make4;
        }
        get_xfcb_result r = get_xfcb(phys, ds_bdos);
        if (r.found) {
            goto make000;
        }
        if (!(dirlbl & 0x10)) {
            goto make4; /* no automatic xfcb creation specified */
        }
        seg_w8(ds, BD_MAKE_XFCB, 0xFF); /* true */
        make(phys, ds_bdos);
        /* the real asm's own "jnz make00" here can never fail to
         * trigger -- see this function's own header comment */
        goto make00;
    }

make000:
    if (seg_r8(ds, BD_FCB_EXISTS)) {
        goto make4;
    }
make00:
    init_xfcb(phys, ds_bdos);
    seg_w8(ds, BD_COMP_FCB_CKS, 0xFF); /* true */
    if (seg_r8(ds, BD_ATTRIBUTES) & 0x40) {
        /* user is setting a password -- pick up its mode from the
         * 9th byte of the dma buffer */
        uint16_t si = (uint16_t)(seg_r16(ds, BD_XDMAAD) + 8);
        uint8_t al = (uint8_t)(ds[si] & 0xE0);
        if (al == 0) {
            al = 0x80; /* default to read mode */
        }
        uint16_t xfcb_bx = (uint16_t)(getdptra(phys, ds_bdos) + fcb_extnum);
        ds[xfcb_bx] = al;
        uint16_t pw_bx;
        uint8_t ch = set_pw(phys, ds_bdos, xfcb_bx, &pw_bx);
        ds[pw_bx] = ch;
    }
    openx2(phys, ds_bdos);

make4:
    seg_w8(ds, BD_ATTRIBUTES, seg_r8(ds, BD_MAKE_FLAG));
    seg_w8(ds, BD_HIGH_EXT, (uint8_t)((seg_r8(ds, BD_MAKE_FLAG) & 0x40) << 1));

    if (seg_r8(ds, (uint16_t)(BD_SDCNT + 1)) == 0) {
        sdcnt_eq_xdcnt(phys, ds_bdos);
        pack_sdcnt(phys, ds_bdos);
        if (!openx03(phys, ds_bdos)) {
            return;
        }
        openx08(phys, ds_bdos);
    } else {
        fix_olist_item(phys, ds_bdos);
        openx1(phys, ds_bdos);
    }
}

/* func23 -- BDOS function 23, rename a file */
void func23(uint8_t *phys, uint16_t ds_bdos) {
    parsave33r(phys, ds_bdos);
    copy_dma_in(phys, ds_bdos, 8);
    rename_(phys, ds_bdos);
}

/*
 * reset_37 -- RESET_37: `func37`'s own real continuation after its
 * own `CALL DISKRESET` -- clear every disk BD_INFO addresses out of
 * BD_DLOG/BD_RODSK/BD_RLOG, and invalidate BD_CURDSK if it was one of
 * them. Not `static`: `func13` also calls `RESET_37` directly (its
 * own `DISKRESET` call already done, with a different `BD_INFO`),
 * the same "call into the middle of another function" pattern as
 * `rename_`'s own `CALL DELETE1` (`file2.c`'s own header comment).
 */
void reset_37(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    uint16_t ax = (uint16_t)~seg_r16(ds, BD_INFO);
    seg_w16(ds, BD_DLOG, (uint16_t)(ax & seg_r16(ds, BD_DLOG)));
    seg_w16(ds, BD_RODSK, (uint16_t)(ax & seg_r16(ds, BD_RODSK)));
    seg_w16(ds, BD_RLOG, (uint16_t)(ax & seg_r16(ds, BD_RLOG)));
    if (!test_vector1(seg_r16(ds, BD_DLOG), seg_r8(ds, BD_CURDSK))) {
        seg_w8(ds, BD_CURDSK, 0xFF);
    }
}

/* func37 -- BDOS function 37, reset drive */
void func37(uint8_t *phys, uint16_t ds_bdos) {
    (void)diskreset(phys, ds_bdos);
    reset_37(phys, ds_bdos);
}

/*
 * func13 -- BDOS function 13, reset disk system: if nothing was open
 * anywhere, wipe every disk vector outright (`RESET_ALL`); otherwise
 * only clear the disks `DISKRESET` actually found safe to reset
 * (`RESET_37`, `func37`'s own real continuation).
 */
void func13(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_INFO, seg_r16(ds, BD_DLOG));
    if (diskreset(phys, ds_bdos)) {
        /* reset_all */
        seg_w16(ds, BD_RODSK, 0);
        seg_w16(ds, BD_DLOG, 0);
        seg_w16(ds, BD_RLOG, 0);
        seg_w16(ds, BD_TLOG, 0);
    } else {
        reset_37(phys, ds_bdos);
    }
    seg_w8(ds, BD_SELDSK, 0);
    seg_w8(ds, BD_CURDSK, 0xFF);
    func48(phys, ds_bdos);
}

/* func14 -- BDOS function 14, select disk info */
void func14(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    set_seldsk(phys, ds_bdos);
    curselect(phys, ds_bdos);
    uint16_t bx = seg_r16(ds, SYSDAT_RLR);
    ds[(uint16_t)(bx + p_dsk)] = seg_r8(ds, BD_SELDSK);
}

/*
 * func33 -- BDOS function 33, random disk read. One of `CHECK_FCB`'s
 * own real callers too (`check_fcb1`'s own header comment, `file1.c`,
 * has the full writeup).
 */
void func33(uint8_t *phys, uint16_t ds_bdos) {
    parsave36r(phys, ds_bdos);
    if (!check_fcb(phys, ds_bdos)) {
        return;
    }
    rdread(phys, ds_bdos);
}

/* func34 -- BDOS function 34, random disk write */
void func34(uint8_t *phys, uint16_t ds_bdos) {
    parsave36r(phys, ds_bdos);
    if (!check_fcb(phys, ds_bdos)) {
        return;
    }
    rdwrite(phys, ds_bdos);
}

/*
 * func30 -- BDOS function 30, set file indicators. The real asm's
 * own tail, `CALL indicators ! JMP cpydirloc`, is redundant --
 * `indicators` (`file2.c`) already ends with its own call into
 * `cpydirloc` -- so the second one is just a no-op repeat, not
 * translated separately.
 */
void func30(uint8_t *phys, uint16_t ds_bdos) {
    parsave33(phys, ds_bdos);
    copy_dma_in(phys, ds_bdos, 8);
    if (!check_wild(phys, ds_bdos)) {
        return;
    }
    reselectx(phys, ds_bdos);
    if (!chk_password(phys, ds_bdos)) {
        return;
    }
    indicators(phys, ds_bdos);
}

/* func35 -- BDOS function 35, return file size (0-65536) */
void func35(uint8_t *phys, uint16_t ds_bdos) {
    parsave36r(phys, ds_bdos);
    getfilesize(phys, ds_bdos);
}

/* func36 -- BDOS function 36, set random record */
void func36(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave36(phys, ds_bdos);
    setrandom(phys, ds_bdos, seg_r16(ds, BD_INFO));
}

/*
 * func40 -- BDOS function 40, random disk write with zero fill of an
 * unallocated block. Calls `rseek1` directly (not `rseek`, which
 * would stomp `BD_SEQIO` back to 0 right after this sets it to 2 --
 * see `rseek`'s own header comment, `file3.c`) and `diskwrite`
 * directly (not `rdwrite`, for the same reason).
 */
void func40(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave36r(phys, ds_bdos);
    if (!check_fcb(phys, ds_bdos)) {
        return;
    }
    seg_w8(ds, BD_SEQIO, 2);
    if (!rseek1(phys, ds_bdos, 0)) {
        return;
    }
    diskwrite(phys, ds_bdos);
}

/*
 * func46 -- BDOS function 46, get disk free space: count the zero
 * bits in `dl`'s own allocation vector via a per-byte popcount (the
 * real asm's own `SHR`/`JNC` loop strips leading zero bits for free
 * before counting each `1` bit it finds -- same total either way),
 * subtract from `BD_MAXALL+1` for the block count, then convert to a
 * record count by shifting left `BD_BLKSHF` bits (done here in a
 * wider type instead of chasing the real asm's own split 16+8-bit
 * shift-and-carry, same final 24-bit result either way -- verified
 * by hand against a worked example).
 *
 * `func40`/`func46`/`tmp_select` all land clear of the DATA.BDO
 * collision, but `diskwrite` (`func40`'s own tail) and `GET_NALBS`
 * (`func46`'s own tail, `file2.c`) don't, so no new differential
 * test for either.
 */
void func46(uint8_t *phys, uint16_t ds_bdos, uint8_t dl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    tmp_select(phys, ds_bdos, dl);

    uint16_t alloca = seg_r16(ds, BD_ALLOCA);
    uint16_t nalbs = get_nalbs(phys, ds_bdos);
    uint16_t true_bits = 0;
    for (uint16_t i = 0; i < nalbs; i++) {
        uint8_t al = ds[(uint16_t)(alloca + i)];
        while (al != 0) {
            if (al & 1) {
                true_bits++;
            }
            al = (uint8_t)(al >> 1);
        }
    }

    uint16_t avail_blocks = (uint16_t)(seg_r16(ds, BD_MAXALL) + 1 - true_bits);
    uint32_t avail_records = (uint32_t)avail_blocks << seg_r8(ds, BD_BLKSHF);
    seg_w16(ds, BD_COMMON_DMA, (uint16_t)(avail_records & 0xFFFF));
    seg_w8(ds, (uint16_t)(BD_COMMON_DMA + 2), (uint8_t)((avail_records >> 16) & 0xFF));
    copy_dma_out(phys, ds_bdos, 3);
}

/*
 * func42/func43 -- BDOS functions 42/43, lock/unlock record: both
 * thin wrappers setting `BD_LOCK_UNLOCK` before a tail call into
 * `FILE3.BDO`'s own `lock_` (see that function's own header comment
 * for the full writeup, including why its own stack trick doesn't
 * need modeling -- `LOCK_PERR` is provably dead code). This closes
 * out `FILE4.BDO` entirely.
 */
void func42(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, BD_LOCK_UNLOCK, 0xFF); /* true */
    lock_(phys, ds_bdos, 1);
}

/* func43 -- BDOS function 43, unlock record */
void func43(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, BD_LOCK_UNLOCK, 0);
    lock_(phys, ds_bdos, 0);
}

/* func27 -- return the login (allocation) vector address, in DS */
void func27(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_RETURNSEG, ds_bdos);
    xcurselect(phys, ds_bdos);
    seg_w16(ds, BD_ARET, seg_r16(ds, BD_ALLOCA)); /* sthlret */
}

/* func28 -- write protect the current disk, once no other process has
 * files open on it (INTRNL_DISK_RESET; setro either way, as the asm) */
void func28(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w8(ds, BD_SET_RO_FLAG, 1);
    uint16_t bx = (uint16_t)(1u << (seg_r8(ds, BD_SELDSK) & 0x1F)); /* drive vector from seldsk */
    (void)intrnl_disk_reset(phys, ds_bdos, bx);
    setro(phys, ds_bdos);
}

/* func31 -- return the address of the disk parameter block, in DS */
void func31(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_RETURNSEG, ds_bdos);
    xcurselect(phys, ds_bdos);
    seg_w16(ds, BD_ARET, seg_r16(ds, BD_DPBADDR)); /* sthlret */
}

/* FUNC101 -- return directory label data for drive dl */
void func101(uint8_t *phys, uint16_t ds_bdos, uint8_t dl) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    tmp_select(phys, ds_bdos, dl);
    seg_w8(ds, BD_LRET, get_dir_mode(phys, ds_bdos)); /* staret */
}

/* FUNC104 -- set the current date and time from the caller's 4 bytes
 * (the seconds are zeroed) */
void func104(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave(phys, ds_bdos, 4);
    seg_w8(ds, BD_PARCOPFL, 0);
    move(phys, ds_bdos, seg_r16(ds, BD_INFO), SYSDAT_TOD, 4);
    seg_w8(ds, (uint16_t)(SYSDAT_TOD + 4), 0);
}

/* FUNC105 -- get the current date and time into the caller's 4 bytes */
void func105(uint8_t *phys, uint16_t ds_bdos, uint16_t dx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    seg_w16(ds, BD_INFOSAVE, dx);
    seg_w16(ds, BD_INFO, BD_LOC_PAR_AREA);
    move(phys, ds_bdos, SYSDAT_TOD, BD_LOC_PAR_AREA, 4);
    seg_w8(ds, BD_PARLG, 4);
    parunsave(phys, ds_bdos);
}

/* FUNC106 -- set the default password (stored backwards, encrypted) */
void func106(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave(phys, ds_bdos, 8);
    seg_w8(ds, BD_PARCOPFL, 0);
    uint16_t bx = seg_r16(ds, BD_INFO);
    uint16_t bx_out;
    (void)set_pw0(phys, ds_bdos, bx, (uint16_t)(BD_DF_PASSWORD + 7), bx, &bx_out);
}

extern void copy_dma_in(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern void reselectx(uint8_t *phys, uint16_t ds_bdos);
extern void checkwrite(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t search(uint8_t *phys, uint16_t ds_bdos, uint8_t cl);
extern void make(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t endofdir(uint8_t *phys, uint16_t ds_bdos);
extern void init_xfcb(uint8_t *phys, uint16_t ds_bdos);
extern void init_xfcb0(uint8_t *phys, uint16_t ds_bdos, uint8_t ch, uint8_t cl);
extern void stamp1(uint8_t *phys, uint16_t ds_bdos);
extern void stamp2(uint8_t *phys, uint16_t ds_bdos);
extern uint8_t chk_xfcb_password1(uint8_t *phys, uint16_t ds_bdos, uint16_t bx, uint8_t al);
extern uint8_t check_wild(uint8_t *phys, uint16_t ds_bdos);
extern void seekcopy(uint8_t *phys, uint16_t ds_bdos);

/* GET_XFCB1 -- the xfcb (or dir label) in the directory buffer: bx =
 * .xfcb(ex), returns (xfcb(ex) & 0E0h) | 1 */
static uint8_t get_xfcb1(uint8_t *phys, uint16_t ds_bdos, uint16_t *bx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    *bx = (uint16_t)(getdptra(phys, ds_bdos) + fcb_extnum);
    return (uint8_t)((ds[*bx] & 0xE0) | 1);
}

/* SDL2 .. SDL3: if the user's fcb(ex) asks for one (bit 0), assign the
 * new password (from the second 8 bytes at the DMA) to the xfcb at bx
 * (.xfcb(12)); then write the dir label or xfcb to the directory */
static void sdl2(uint8_t *phys, uint16_t ds_bdos, uint16_t bx) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    if (ds[getexta(phys, ds_bdos)] & 1) {
        seg_w16(ds, BD_XDMAAD, (uint16_t)(seg_r16(ds, BD_XDMAAD) + 8));
        uint16_t sum_at;
        uint8_t ch = set_pw(phys, ds_bdos, bx, &sum_at);
        ds[sum_at] = ch; /* set the password checksum byte */
        seg_w16(ds, BD_XDMAAD, (uint16_t)(seg_r16(ds, BD_XDMAAD) - 8));
    }
    seekcopy(phys, ds_bdos);
}

/* FUNC100 -- set the directory label from the user's fcb (its ex byte:
 * 80h passwords, 40h access stamps, 20h update stamps, 01h assign a
 * password to the label) */
void func100(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    copy_dma_in(phys, ds_bdos, 16);
    parsave33(phys, ds_bdos);
    reselectx(phys, ds_bdos);
    checkwrite(phys, ds_bdos);
    ds[seg_r16(ds, BD_INFO)] = 0x20; /* does a dir lbl exist on the drive? */
    seg_w16(ds, BD_XDCNT, 0xFFFF);   /* initialized for make */
    if (!search(phys, ds_bdos, 1)) {
        seg_w8(ds, BD_MAKE_XFCB, 0xFF); /* no - make one */
        make(phys, ds_bdos);
        if (endofdir(phys, ds_bdos)) return; /* no directory space */
        init_xfcb(phys, ds_bdos);
        stamp1(phys, ds_bdos); /* create date and time stamp */
    }
    /* sdl1 */
    stamp2(phys, ds_bdos); /* update date and time stamp */
    uint16_t bx;
    uint8_t al = get_xfcb1(phys, ds_bdos, &bx);
    if (!chk_xfcb_password1(phys, ds_bdos, bx, al)) return; /* verify the password */
    init_xfcb0(phys, ds_bdos, 0, 0); /* update the dir lbl name */
    bx = (uint16_t)(getdptra(phys, ds_bdos) + 12);
    al = (uint8_t)(ds[getexta(phys, ds_bdos)] | 1); /* the user's data byte, + "dir lbl exists" */
    ds[bx] = al;
    ds[seg_r16(ds, BD_DRVLBLA)] = al; /* the drive's dir lbl data byte */
    sdl2(phys, ds_bdos, bx);
}

/* FUNC102 -- read a file's xfcb into the user's fcb */
void func102(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    parsave33(phys, ds_bdos);
    reselectx(phys, ds_bdos);
    if (!check_wild(phys, ds_bdos)) return;
    get_xfcb_result r = get_xfcb(phys, ds_bdos); /* does an xfcb exist for the file? */
    if (!r.found) {
        seg_w8(ds, BD_LRET, 0xFF); /* no: staret */
        return;
    }
    move(phys, ds_bdos, getdptra(phys, ds_bdos), seg_r16(ds, BD_INFO), fcb_nxtrec);
}

/* FUNC103 -- write or update a file's xfcb (password mode and
 * password), making one if the drive has a dir label */
void func103(uint8_t *phys, uint16_t ds_bdos) {
    uint8_t *ds = far_ptr(phys, ds_bdos, 0);
    copy_dma_in(phys, ds_bdos, 16);
    parsave33(phys, ds_bdos);
    reselectx(phys, ds_bdos);
    if (!check_wild(phys, ds_bdos)) return;
    seg_w16(ds, BD_XDCNT, 0xFFFF); /* initialized for make */
    get_xfcb_result r = get_xfcb(phys, ds_bdos);
    if (!r.found) {
        if (get_dir_mode(phys, ds_bdos) == 0) { /* no dir lbl */
            seg_w8(ds, BD_LRET, 0xFF);
            return;
        }
        seg_w8(ds, BD_MAKE_XFCB, 0xFF);
        if (!search(phys, ds_bdos, fcb_extnum)) return; /* does the file exist? */
        make(phys, ds_bdos);
        if (endofdir(phys, ds_bdos)) return; /* no directory space */
        init_xfcb(phys, ds_bdos);
    }
    /* wxfcb1: CHK_XFCB_PASSWORD */
    uint16_t bx;
    uint8_t al = (uint8_t)(get_xfcb1(phys, ds_bdos, &bx) - 1);
    if (!chk_xfcb_password1(phys, ds_bdos, bx, al)) return;
    uint16_t si = getexta(phys, ds_bdos);
    if (ds[bx] == 0 && !(ds[si] & 1)) { /* password mode stays zero */
        seekcopy(phys, ds_bdos);
        return;
    }
    /* wxfcb2: the new password mode */
    al = (uint8_t)(ds[si] & 0xE0);
    if (al == 0) al = 0x80;
    ds[bx] = al;
    sdl2(phys, ds_bdos, bx);
}
