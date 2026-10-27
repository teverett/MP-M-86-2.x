/*
 * rt_syms.h -- the few /src-assem code offsets the runtime hands to the
 * module inits, which store them where the asm stores them (in PDs,
 * the dispatcher vector, LOAD's return address); nothing in protected
 * mode jumps to them. Each is the value tools/asm86 gives the label;
 * tests/src-c/test_syms.py checks them against the assembled source,
 * so the build itself doesn't need /src-assem.
 */
#ifndef RT_SYMS_H
#define RT_SYMS_H
#define SUP_USER_RETF 0x0AA7 /* SUP.A86: user_retf */
#define SUP_NOTICK 0x018C /* SUP.A86: notick */
#define SUP_IDLE 0x0178 /* SUP.A86: idle */
#define SUP_USER_ENTRY 0x01EC /* SUP.A86: user_entry */
#define RTM_FDISP 0x03DB /* RTM.A86: fdisp */
#define RTM_FARPDISP 0x0459 /* RTM.A86: farpdisp */
#endif
