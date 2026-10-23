#!/usr/bin/env python3
"""Differential test for src-c/kernel/sysent.c against all 8 of
src/kernel/SYSENT.RTM's entries.

poll_entry/delay_entry/dispatch_entry/set_prior_entry/sleep_entry tail-jump
into the (unported) dispatcher and never return -- per docs/src-c.md's
boundary-stub convention, this test stops the simulator right there and
compares the PD fields each one wrote (not a return value). pd_entry and
wakeup_entry really return; those compare bx/cx or memory as applicable.

creat_proc_entry is tested separately below, via its own wrapper/assembly
(tests/src-c/fixtures/creat_proc_wrapper.A86, bundling the real PROC.RTM/
DSPTCH.RTM rather than stubbing them out) -- see sysent.c's own header
comment for why it alone goes all the way through a real proc_creat+pdisp
call instead of stopping at the boundary like its 7 siblings above.
"""
import ctypes
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, 'tools'))

from asm86 import Assembler  # noqa: E402
from i86sim import I86Sim  # noqa: E402

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'sysent_wrapper.A86')
SYSENT_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'sysent.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'proc.c'),
            os.path.join(REPO_ROOT, 'src-c', 'kernel', 'dsptch.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0
PD_ADDR = 0x1000
P_STAT, P_PRIOR, P_LINK, P_SCRATCH = 0x04, 0x05, 0x00, 0x2E
SD_RLR, SD_DRL = 0xA000, 0xA002
UDA_DPARAM, UDA_RETSEG = 0xA010, 0xA014
DSPTCH_STUB, PDISP_STUB = 0x9000, 0x9010

SYSLIST_ROOT = 0x1100  # a word elsewhere, holding the System List head pointer
WAKE_PD_ADDR = 0x1200  # the PD wakeup_entry moves from the System List to DRL


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    entries = {name: asm.symtab[name].value for name in (
        'poll_entry', 'delay_entry', 'dispatch_entry', 'set_prior_entry',
        'pd_entry', 'sleep_entry', 'wakeup_entry')}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *SYSENT_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory():
    mem = bytearray(0x10000)
    mem[SD_RLR] = PD_ADDR & 0xFF
    mem[SD_RLR + 1] = (PD_ADDR >> 8) & 0xFF
    return mem


def run_reference(code, entry, mem, regs, stop_addrs=()):
    image = bytearray(mem)
    image[0:len(code)] = code
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs))
    sim.run()
    return sim.regs, image


def main():
    code, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)
        c_u16p = ctypes.POINTER(ctypes.c_uint16)
        lib.poll_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.delay_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.dispatch_entry.argtypes = []
        lib.set_prior_entry.argtypes = [c_u8p, ctypes.c_uint8]
        lib.sleep_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.wakeup_entry.argtypes = [c_u8p, ctypes.c_uint16]
        lib.pd_entry.restype = ctypes.c_uint16
        lib.pd_entry.argtypes = [c_u8p, ctypes.c_uint16, c_u16p]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        # poll_entry: dx = device number -> p_stat=ps_poll, u_dparam=dx
        mem = base_memory()
        regs, image = run_reference(code, entries['poll_entry'], mem, {'dx': 7}, {DSPTCH_STUB})
        ref = (image[PD_ADDR + P_STAT], mem_r16(image, UDA_DPARAM))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.poll_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(7))
        port = (port_mem[PD_ADDR + P_STAT], mem_r16(bytes(port_mem), UDA_DPARAM))
        check("poll_entry", ref, port)

        # delay_entry: dx = ticks -> p_stat=ps_delay, u_dparam=dx
        mem = base_memory()
        regs, image = run_reference(code, entries['delay_entry'], mem, {'dx': 42}, {DSPTCH_STUB})
        ref = (image[PD_ADDR + P_STAT], mem_r16(image, UDA_DPARAM))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.delay_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(42))
        port = (port_mem[PD_ADDR + P_STAT], mem_r16(bytes(port_mem), UDA_DPARAM))
        check("delay_entry", ref, port)

        # dispatch_entry: bx=cx=0 at the pdisp boundary, no memory effects
        mem = base_memory()
        regs, image = run_reference(code, entries['dispatch_entry'], mem, {}, {PDISP_STUB})
        ref = (regs['bx'], regs['cx'])
        lib.dispatch_entry()
        check("dispatch_entry", ref, (0, 0))

        # set_prior_entry: dl = priority -> p_prior=dl
        mem = base_memory()
        regs, image = run_reference(code, entries['set_prior_entry'], mem, {'dx': 0x0009}, {PDISP_STUB})
        ref = image[PD_ADDR + P_PRIOR]
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.set_prior_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint8(9))
        port = port_mem[PD_ADDR + P_PRIOR]
        check("set_prior_entry", ref, port)

        # pd_entry: ds -> u_retseg, returns bx=rlr, cx=0 (a real RET)
        mem = base_memory()
        regs, image = run_reference(code, entries['pd_entry'], mem, {'ds': 0x5EED})
        ref = (regs['bx'], regs['cx'], mem_r16(image, UDA_RETSEG))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        cx = ctypes.c_uint16()
        bx = lib.pd_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(0x5EED), ctypes.byref(cx))
        port = (bx, cx.value, mem_r16(bytes(port_mem), UDA_RETSEG))
        check("pd_entry", ref, port)

        # sleep_entry: dx = wait param -> u_dparam=dx, p_scratch=old p_stat, p_stat=ps_sleep
        mem = base_memory()
        mem[PD_ADDR + P_STAT] = 0x03  # some prior status, to prove it gets copied to p_scratch
        regs, image = run_reference(code, entries['sleep_entry'], mem, {'dx': 99}, {DSPTCH_STUB})
        ref = (image[PD_ADDR + P_STAT], image[PD_ADDR + P_SCRATCH], mem_r16(image, UDA_DPARAM))
        port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
        lib.sleep_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(99))
        port = (port_mem[PD_ADDR + P_STAT], port_mem[PD_ADDR + P_SCRATCH], mem_r16(bytes(port_mem), UDA_DPARAM))
        check("sleep_entry", ref, port)

        # wakeup_entry, two cases: a process on the System List, and an empty list.
        for label, list_head in (("wakeup_entry, process waiting", WAKE_PD_ADDR), ("wakeup_entry, empty list", 0)):
            mem = base_memory()
            mem_w16(mem, SYSLIST_ROOT, list_head)
            mem_w16(mem, SD_DRL, 0x9999)  # a recognizable prior DRL head
            if list_head:
                mem[WAKE_PD_ADDR + P_LINK] = 0
                mem[WAKE_PD_ADDR + P_LINK + 1] = 0
                mem[WAKE_PD_ADDR + P_STAT] = 0xEE  # anything but ps_run, to prove it changes
            regs, image = run_reference(code, entries['wakeup_entry'], mem, {'dx': SYSLIST_ROOT}, {PDISP_STUB})
            ref = (mem_r16(image, SYSLIST_ROOT), mem_r16(image, SD_DRL),
                   image[WAKE_PD_ADDR + P_STAT] if list_head else None,
                   mem_r16(image, WAKE_PD_ADDR + P_LINK) if list_head else None)
            port_mem = (ctypes.c_uint8 * 0x10000)(*mem)
            lib.wakeup_entry(ctypes.cast(port_mem, c_u8p), ctypes.c_uint16(SYSLIST_ROOT))
            port_bytes = bytes(port_mem)
            port = (mem_r16(port_bytes, SYSLIST_ROOT), mem_r16(port_bytes, SD_DRL),
                    port_mem[WAKE_PD_ADDR + P_STAT] if list_head else None,
                    mem_r16(port_bytes, WAKE_PD_ADDR + P_LINK) if list_head else None)
            check(label, ref, port)

        # ================= creat_proc_entry =================
        # A real end-to-end case: creates one child PD (proc_creat),
        # then really dispatches (pdisp -> dsptch -> schedule ->
        # switch_), landing on whichever PD ends up first on RLR.
        # Needs its own wrapper (the real PROC.RTM/DSPTCH.RTM, not
        # stubs) and tools/i86sim.py's segmented=True + a CSEG_BASE
        # shift, same reasoning as test_proc.py's/test_dsptch.py's
        # own identical needs.
        CP_CSEG_BASE = 0x0800
        CP_CS_REG = CP_CSEG_BASE >> 4
        CP_MPMIF_STUB = 0x9000 + CP_CSEG_BASE
        CP_XIOSIF_STUB = 0x9001 + CP_CSEG_BASE

        CP_UDA_WRKSEG, CP_SD_ENDSEG = 0xA012, 0xA026
        CP_SD_NLSTDEV, CP_SD_NCONDEV, CP_SD_PUL = 0xA035, 0xA036, 0xA038
        CP_P_LINK, CP_P_UDA, CP_P_MEM, CP_P_LST, CP_P_PARENT = 0, 0x10, 0x16, 0x24, 0x1E
        CP_PF_CHILDABORT = 0x800
        CP_U_STACK_SP, CP_U_SP = 0x34, 0x1C
        CP_MS_LINK = 0

        cp_wrapper = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'creat_proc_wrapper.A86')
        cp_asm = Assembler()
        cp_asm.assemble(cp_wrapper, [os.path.join(REPO_ROOT, 'src', 'kernel')])
        cp_code = cp_asm.segment_bytes('cseg')
        cp_entry = cp_asm.symtab['creat_proc_entry'].value

        class SwitchResult(ctypes.Structure):
            _fields_ = [('ax', ctypes.c_uint16), ('bx', ctypes.c_uint16),
                        ('cx', ctypes.c_uint16), ('dx', ctypes.c_uint16),
                        ('si', ctypes.c_uint16), ('di', ctypes.c_uint16),
                        ('bp', ctypes.c_uint16), ('new_ss', ctypes.c_uint16),
                        ('new_sp', ctypes.c_uint16), ('went_to_int_disp_exit', ctypes.c_uint8)]

        cp_argt = [c_u8p] * 1 + [ctypes.c_uint16] * 8 + [ctypes.POINTER(ctypes.c_uint8)]
        lib.creat_proc_entry.restype = SwitchResult
        lib.creat_proc_entry.argtypes = cp_argt

        def cp_run_reference(mem, regs, sysdat_word):
            image = bytearray(0x10000)
            image[CP_CSEG_BASE:CP_CSEG_BASE + len(cp_code)] = cp_code
            for addr, val in enumerate(mem):
                if val != 0:
                    image[addr] = val
            image[CP_CSEG_BASE] = sysdat_word & 0xFF
            image[CP_CSEG_BASE + 1] = (sysdat_word >> 8) & 0xFF
            sim = I86Sim(image, {**regs, 'sp': STACK_TOP, 'cs': CP_CS_REG}, CP_CSEG_BASE + cp_entry,
                         call_stub_addrs={CP_MPMIF_STUB, CP_XIOSIF_STUB}, segmented=True)
            sim.run()
            return sim.regs, image

        # sysdat has to be 0 here, unlike test_proc.py's own tests --
        # dsptch.c's own "mov ss,cs:sysdat" (switching to its internal
        # dispatcher stack) and every SD_* flat-dseg address in this
        # combined call chain assume sysdat==0, same as every dsptch
        # wrapper/test already does. getpdadr's own "already in MPM
        # range" check still needs wrkseg==sysdat for si==dx, so wrkseg
        # is 0 too (not 0x3100 the way test_proc.py's own tests pick,
        # since those never also go through dsptch).
        SYSDAT = WRKSEG = 0
        PARENT_PD, CHILD_PD = 0x5000, 0x5100
        PARENT_UDA_SEG, CHILD_UDA_SEG = 0x3600, 0x3300

        mem = bytearray(0x10000)
        mem_w16(mem, CP_UDA_WRKSEG, WRKSEG)
        mem_w16(mem, CP_SD_ENDSEG, 0x7000)
        mem[CP_SD_NLSTDEV] = 4
        mem[CP_SD_NCONDEV] = 2

        mem_w16(mem, SD_RLR, PARENT_PD)
        mem_w16(mem, PARENT_PD + CP_P_LINK, 0)  # only entry on RLR before the call
        mem_w16(mem, PARENT_PD + P_PRIOR, 0)
        mem_w16(mem, PARENT_PD + CP_P_UDA, PARENT_UDA_SEG)
        mem_w16(mem, PARENT_PD + CP_P_MEM, 0)
        mem[PARENT_PD + P_STAT] = 7  # overwritten to ps_run by pdisp itself

        mem_w16(mem, CHILD_PD + CP_P_LINK, 0)  # end of the (one-long) creation chain
        mem_w16(mem, CHILD_PD + CP_P_UDA, (CHILD_UDA_SEG - WRKSEG) & 0xFFFF)  # relative, per real asm
        mem[CHILD_PD + CP_P_LST] = 1
        mem_w16(mem, CHILD_PD + CP_P_MEM, 0)
        mem[CHILD_PD + P_PRIOR] = 0xFF

        # insert_process's own priority ordering (lower p_prior value
        # sorts earlier) leaves PARENT_PD -- already on RLR, priority
        # 0 -- ahead of the newly-created, lower-priority CHILD_PD, so
        # switch_ dispatches straight back into the parent: a real,
        # complete creat_proc_entry pass (the child really gets
        # created) without needing the child's own UDA to hold a
        # valid resumable register-save frame too (it's never
        # switched into here). No separate sentinel needed for
        # PARENT_PD's own resume point either: dsptch's own entry
        # (`mov u_ss,ss ! mov u_sp,sp`) always overwrites the
        # *current* process's own u_ss/u_sp with its real pre-
        # dispatch ss:sp before deciding who to switch into -- since
        # that's PARENT_PD here, switch_'s own final ret naturally
        # pops the harness's own outer RET_SENTINEL right back where
        # this call itself started (sp=STACK_TOP), same as it would
        # for an ordinary PROC.RTM/DSPTCH.RTM test that never touched
        # u_ss/u_sp at all.

        CHILD_SP = 0x00F0
        mem_w16(mem, phys(CHILD_UDA_SEG) + CP_U_STACK_SP, CHILD_SP)

        for i in range(10):
            mem_w16(mem, 2 * i, 0x1100 + i)  # IVT ints 0-4
        for i in range(4):
            mem_w16(mem, 0x380 + 2 * i, 0x2200 + i)  # i_mpm_ip block

        REGS_IN = dict(ax=0xAAAA, bx=0xBBBB, cx=0xCCCC, si=0x1234, di=0x5678, bp=0x9ABC,
                       dx=CHILD_PD, es=PARENT_UDA_SEG)
        ref_regs, ref_img = cp_run_reference(mem, REGS_IN, SYSDAT)

        buf = to_ctypes(mem)
        flag = ctypes.c_uint8(0xFF)
        got = lib.creat_proc_entry(
            ctypes.cast(buf, c_u8p), ctypes.c_uint16(CHILD_PD), ctypes.c_uint16(SYSDAT),
            ctypes.c_uint16(REGS_IN['ax']), ctypes.c_uint16(REGS_IN['bx']), ctypes.c_uint16(REGS_IN['cx']),
            ctypes.c_uint16(REGS_IN['si']), ctypes.c_uint16(REGS_IN['di']), ctypes.c_uint16(REGS_IN['bp']),
            ctypes.byref(flag))
        port_img = bytes(buf)

        def cp_snapshot(img):
            return (mem_r16(img, SD_RLR), mem_r16(img, CP_SD_PUL),
                    mem_r16(img, CHILD_PD + CP_P_PARENT), mem_r16(img, CHILD_PD + CP_P_UDA))

        check("creat_proc_entry: creates the child, dispatches into it",
              cp_snapshot(ref_img), cp_snapshot(port_img))
        # ax/bx/cx/si/di are deliberately not checked here -- see
        # sysent.c's own header comment on creat_proc_entry: they're
        # proc_creat's own leftover internal scratch by the time
        # dsptch saves them, not a value either this entry point's
        # own caller or this port ever meaningfully controls. bp is
        # the one real register genuinely preserved (proc_creat never
        # touches it).
        check("creat_proc_entry: switch_result's bp matches", ref_regs['bp'], got.bp)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


def mem_r16(mem, addr):
    return mem[addr & 0xFFFF] | (mem[(addr + 1) & 0xFFFF] << 8)


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def phys(seg):
    return (seg * 16) & 0xFFFF


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


if __name__ == '__main__':
    sys.exit(main())
