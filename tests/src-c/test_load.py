#!/usr/bin/env python3
"""Differential test for src-c/kernel/load.c's load_finish against a
tail portion of src/kernel/LOAD.SUP's `load:` routine (`lb_more`
through `load_exit`). See load.c's own header comment for why only
this portion is ported -- the front half depends on real,
not-yet-ported disk I/O.

The reference run enters mid-routine at the real `lb_more` label
(same technique as src-c/kernel/clock.c's own test, entering at
`clockloop`), with `si`/`cx` pre-set to what the real asm's own
immediately-preceding instructions ("mov si,offset ldtab ! mov
cx,load_nldtabents") would have left them as, and `load_8080`
pre-zeroed the same way ("mov load_8080,0" also precedes `lb_more`).
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'load_wrapper.A86')
LOAD_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', f) for f in
          ('load.c', 'memory.c', 'util_mem.c', 'mau.c', 'ml.c', 'cpmmem.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')
KERNEL_DIR = os.path.join(REPO_ROOT, 'src', 'kernel')

STACK_TOP = 0xFFF0

SD_RLR = 0xA000
SD_MDUL, SD_MAL, SD_MFL = 0xA028, 0xA02A, 0xA02C
SD_LOAD_PD, SD_LOAD_FCB, SD_LOAD_NLDTABENTS = 0xA900, 0xA902, 0xA904
SD_LOAD_BASEP, SD_LOAD_8080, SD_LOAD_UDA, SD_LOAD_LSTK = 0xA906, 0xA908, 0xA909, 0xA90B
SD_LOAD_LDTAB = 0xA910
UDA_WRKSEG = 0xA012

LDTAB_START, LDTAB_MIN, LDTAB_MAX, LDTAB_TYPE = 0, 2, 4, 14
LDTABLEN = 17

P_UDA, P_STAT, P_PRIOR, P_USER, P_DSK, P_LDSK, P_LUSER = 0x10, 4, 5, 0x13, 0x12, 0x14, 0x15
PS_RUN = 0

BPG_CSEG, BPG_8080, BPG_ESEG, BPG_DMA = 3, 5, 15, 0x80
U_DMA_OFST, U_DMA_SEG, U_INITCS, U_INITDS, U_INITES, U_INITSS, U_STACK_SP = 0, 4, 0x50, 0x52, 0x54, 0x56, 0x34
LS_OFFSET, LS_FLAGS, LS_ROFFSET, LS_RCSEG = 0x56, 0x5A, 0x5C, 0x5E

ULEN = 0x100

SYSDAT = 0x3000
USER_RETF_OFFSET = 0x0002  # user_retf's own resolved address in the wrapper
CS_VAL = 0x0000            # the wrapper's own cseg loads at absolute 0

PD_ADDR = 0x5000
FCB_ADDR = 0x0030          # offset within WRKSEG's own segment
WRKSEG = 0x0040

CODE_SEG, DATA_SEG, LSTK_SEG = 0x0200, 0x0300, 0x0400

MS_LINK, MS_START, MS_LENGTH, MS_FLAGS, MS_MAU = 0, 2, 4, 6, 8
MF_LOAD = 0x01
MCB_EXT = 4
SAT_START, SAT_LENGTH, SAT_NALL, SATLEN = 0, 2, 4, 5


def put_sat(mem, seg, entry_no, start, length, nall, ents=4):
    base = phys(seg)
    mem[base] = ents
    off = base + entry_no * SATLEN
    mem_w16(mem, off + SAT_START, start)
    mem_w16(mem, off + SAT_LENGTH, length)
    mem[off + SAT_NALL] = nall


def phys(seg):
    return (seg * 16) & 0xFFFF


def mem_w16(mem, addr, val):
    mem[addr & 0xFFFF] = val & 0xFF
    mem[(addr + 1) & 0xFFFF] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    addr &= 0xFFFF
    return mem[addr] | (mem[(addr + 1) & 0xFFFF] << 8)


def put_ldtab(mem, entry_no, start, minv, maxv, type_):
    off = SD_LOAD_LDTAB + entry_no * LDTABLEN
    mem_w16(mem, off + LDTAB_START, start)
    mem_w16(mem, off + LDTAB_MIN, minv)
    mem_w16(mem, off + LDTAB_MAX, maxv)
    mem[off + LDTAB_TYPE] = type_


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [KERNEL_DIR])
    code = asm.segment_bytes('cseg')
    names = ('lb_more', 'user_retf', 'diskread', 'load_group', 'load', 'load_entry',
             'cload_entry', 'malloc_entry', 'mfree_entry', 'maualloc_entry', 'mlalloc_entry',
             'maufree_entry', 'mlfree_entry', 'cpmfree_entry')
    entries = {name: asm.symtab[name].value for name in names}
    return code, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *LOAD_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def base_memory(nldtabents):
    mem = bytearray(0x10000)
    mem_w16(mem, UDA_WRKSEG, WRKSEG)
    mem_w16(mem, SD_LOAD_NLDTABENTS, nldtabents)
    mem[SD_LOAD_8080] = 0
    return mem


MPMIF_STUB = 0x9000
F_SETDMA, F_SETDMAB, F_FREADSEQ = 0x1A, 0x33, 0x14
F_MALLOC, F_MEMFREE, F_MAUALLOC, F_MLALLOC = 0x80, 0x82, 0x309, 0x30B
F_MAUFREE, F_MLFREE = 0x30A, 0x30C
F_FREEMEM = 0x39  # cload_entry's own ld_freeit call -- resolves to cpmfree_entry
F_QREAD, F_QWRITE = 0x89, 0x8B  # load's own mxloadqpb calls -- result unused, true no-ops
FCB_NXTREC = 0x20
SD_LOAD_DISK_RECS, SD_LOAD_DISK = 0xA9E9, 0xB000


def run_reference(code, entry, mem, regs, call_hooks=None, jmp_hooks=None):
    image = bytearray(0x10000)
    image[0:len(code)] = code
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    image[0] = SYSDAT & 0xFF  # `sysdat dw 0` sits at cseg offset 0
    image[1] = (SYSDAT >> 8) & 0xFF
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, segmented=True,
                 call_stub_addrs={MPMIF_STUB}, call_hooks=call_hooks, jmp_hooks=jmp_hooks)
    sim.run()
    return sim.regs, image


def make_freadseq_hook(real_targets=None):
    """mpmif(f_setdmab)/mpmif(f_setdma)/mpmif(f_freadseq) hook for
    diskread's own 3-call sequence -- see load.c's own header comment
    for the "virtual disk" model this reproduces exactly (same
    fcb_nxtrec-indexed single-extent read, same SD_LOAD_DISK/
    SD_LOAD_DISK_RECS scratch layout).

    `real_targets`, if given, is {function_code: absolute_address} for
    any OTHER mpmif function code that should instead run for real --
    needed when the same "call mpmif" site is shared with routines
    (malloc_entry's own mpmif(f_malloc)/mpmif(f_memfree), and in turn
    mpmif(f_maualloc)/mpmif(f_mlalloc)) whose own real effect matters.
    Since i86sim.py's own call_hooks mechanism always treats the call
    as a no-op return, redirecting into real code instead needs doing
    by hand here: push the return address (sim.ip, already advanced
    past the call's own encoding by the time a hook runs) and jump
    sim.ip to the real target -- exactly what a real near CALL does,
    just performed from Python instead of the opcode handler."""
    state = {'dma_seg': 0, 'dma_ofst': 0}
    real_targets = real_targets or {}

    def hook(sim):
        cx = sim.regs['cx']
        if cx == F_SETDMAB:
            state['dma_seg'] = sim.regs['dx']
        elif cx == F_SETDMA:
            state['dma_ofst'] = sim.regs['dx']
        elif cx == F_FREADSEQ:
            fcb = sim.regs['dx']  # ds=u_wrkseg at this exact call
            wrkseg = mem_r16(sim.mem, UDA_WRKSEG)
            nxt_addr = (phys(wrkseg) + fcb + FCB_NXTREC) & 0xFFFF
            nxtrec = sim.mem[nxt_addr]
            recs = mem_r16(sim.mem, SD_LOAD_DISK_RECS)
            if nxtrec >= recs:
                bl = 1
            else:
                src = (SD_LOAD_DISK + nxtrec * 128) & 0xFFFF
                dst = (phys(state['dma_seg']) + state['dma_ofst']) & 0xFFFF
                for i in range(128):
                    sim.mem[(dst + i) & 0xFFFF] = sim.mem[(src + i) & 0xFFFF]
                sim.mem[nxt_addr] = (nxtrec + 1) & 0xFF
                bl = 0
            sim.regs['bx'] = (sim.regs['bx'] & 0xFF00) | bl
        elif cx in real_targets:
            ret_addr = sim.ip
            sim._push16(ret_addr)
            sim.ip = real_targets[cx]
        elif cx in (F_QREAD, F_QWRITE):
            pass  # true no-op -- load's own mxloadqpb result is never read
        else:
            raise AssertionError(f"unexpected mpmif function code {cx:#x}")

    return hook


def make_jmp_redirect_hook(real_targets):
    """jmp_hooks counterpart to make_freadseq_hook's own `real_targets`
    redirect: for a real tail-*jump* into mpmif (mfree_it's own closing
    "mov cx,f_mlfree ! jmp mpmif", not a CALL), no return address gets
    pushed -- the callee's own eventual "ret" must pop whatever's
    already on the stack from mfree_it's own original caller, exactly
    like a real tail call. i86sim.py's own jmp_hooks convention leaves
    `sim.ip` unchanged after the hook runs unless the hook itself sets
    it, so this just redirects ip with no stack effect at all."""
    def hook(sim):
        cx = sim.regs['cx']
        if cx in real_targets:
            sim.ip = real_targets[cx]
        else:
            raise AssertionError(f"unexpected mpmif function code {cx:#x} (jmp)")
    return hook


def put_disk_records(mem, records):
    mem_w16(mem, SD_LOAD_DISK_RECS, len(records))
    for i, rec in enumerate(records):
        padded = (rec + bytes(128))[:128]
        mem[SD_LOAD_DISK + i * 128:SD_LOAD_DISK + i * 128 + 128] = padded


def to_ctypes(mem):
    return (ctypes.c_uint8 * 0x10000)(*mem)


def main():
    code, entries = assemble_reference()
    lb_more, user_retf = entries['lb_more'], entries['user_retf']
    diskread_entry, load_group_entry = entries['diskread'], entries['load_group']
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        class BxCx(ctypes.Structure):
            _fields_ = [('bx', ctypes.c_uint16), ('cx', ctypes.c_uint16)]

        lib.load_finish.restype = BxCx
        lib.load_finish.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
        lib.diskread.restype = BxCx
        lib.diskread.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.load_group.restype = ctypes.c_uint16
        lib.load_group.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def run_both(mem):
            ref_regs, ref_img = run_reference(
                code, lb_more, mem, {'si': SD_LOAD_LDTAB, 'cx': mem_r16(mem, SD_LOAD_NLDTABENTS)})
            buf = to_ctypes(mem)
            got = lib.load_finish(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SYSDAT),
                                   ctypes.c_uint16(user_retf), ctypes.c_uint16(CS_VAL))
            port_img = bytes(buf)
            return (ref_regs['bx'], ref_regs['cx']), ref_img, (got.bx, got.cx), port_img

        def basep_snapshot(img, seg):
            base = phys(seg)
            return (mem[base + BPG_8080] if False else img[base + BPG_8080],
                    mem_r16(img, base + BPG_CSEG), mem_r16(img, base + BPG_ESEG),
                    img[base + 6], img[base + 7], img[base + 8],  # dlen (3 bytes)
                    mem_r16(img, base + 9))  # dseg

        def uda_snapshot(img, seg):
            base = phys(seg)
            return (mem_r16(img, base + U_DMA_OFST), mem_r16(img, base + U_DMA_SEG),
                    mem_r16(img, base + U_INITCS), mem_r16(img, base + U_INITDS),
                    mem_r16(img, base + U_INITES), mem_r16(img, base + U_INITSS),
                    mem_r16(img, base + U_STACK_SP))

        def lstk_snapshot(img, seg):
            base = phys(seg)
            return (mem_r16(img, base + LS_OFFSET), mem_r16(img, base + LS_FLAGS),
                    mem_r16(img, base + LS_ROFFSET), mem_r16(img, base + LS_RCSEG))

        # -- single Data-group .CMD, with a PD to initialize --
        # (bpg_cseg isn't test-injected: lbb_mre's own scan over ldtab is
        # what has to populate it, from the separate Code-group entry --
        # that's the real field this port's own differential test caught
        # a test-setup mistake on, not a /src or /src-c bug: a Data-group
        # .CMD still needs its own Code-group entry for bpg_cseg to ever
        # be filled in at all, same base page, different ldtab slot)
        mem = base_memory(3)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)  # UDA+LSTK entry
        put_ldtab(mem, 1, CODE_SEG, 8, 8, 1)
        put_ldtab(mem, 2, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem[PD_ADDR + P_USER] = 3
        mem[PD_ADDR + P_DSK] = 2
        mem_w16(mem, phys(WRKSEG) + FCB_ADDR, 0)  # fcb_dr == 0 -> keep p_dsk's copy
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)

        ref, ref_img, port, port_img = run_both(mem)
        ref_full = ref + basep_snapshot(ref_img, DATA_SEG) + \
            (ref_img[PD_ADDR + P_UDA] | (ref_img[PD_ADDR + P_UDA + 1] << 8),
             ref_img[PD_ADDR + P_STAT], ref_img[PD_ADDR + P_PRIOR],
             ref_img[PD_ADDR + P_LUSER], ref_img[PD_ADDR + P_LDSK]) + \
            uda_snapshot(ref_img, LSTK_SEG) + lstk_snapshot(ref_img, LSTK_SEG)
        port_full = port + basep_snapshot(port_img, DATA_SEG) + \
            (port_img[PD_ADDR + P_UDA] | (port_img[PD_ADDR + P_UDA + 1] << 8),
             port_img[PD_ADDR + P_STAT], port_img[PD_ADDR + P_PRIOR],
             port_img[PD_ADDR + P_LUSER], port_img[PD_ADDR + P_LDSK]) + \
            uda_snapshot(port_img, LSTK_SEG) + lstk_snapshot(port_img, LSTK_SEG)
        check("load_finish: Data-group .CMD, PD initialized", ref_full, port_full)

        # -- FCB has an explicit disk letter -> overrides p_ldsk --
        mem = base_memory(3)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)
        put_ldtab(mem, 1, CODE_SEG, 8, 8, 1)
        put_ldtab(mem, 2, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem[PD_ADDR + P_USER] = 3
        mem[PD_ADDR + P_DSK] = 2
        mem[phys(WRKSEG) + FCB_ADDR] = 5  # explicit disk letter
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)
        ref, ref_img, port, port_img = run_both(mem)
        ref_full = ref + (ref_img[PD_ADDR + P_LDSK],)
        port_full = port + (port_img[PD_ADDR + P_LDSK],)
        check("load_finish: explicit FCB disk letter overrides p_ldsk", ref_full, port_full)

        # -- 8080-model .CMD (only a Code group, no Data group) --
        mem = base_memory(2)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)
        put_ldtab(mem, 1, CODE_SEG, 0x08, 0x08, 1)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem[PD_ADDR + P_USER] = 0
        mem[PD_ADDR + P_DSK] = 0
        mem_w16(mem, phys(WRKSEG) + FCB_ADDR, 0)
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)
        ref, ref_img, port, port_img = run_both(mem)
        ref_full = ref + basep_snapshot(ref_img, CODE_SEG) + lstk_snapshot(ref_img, LSTK_SEG)
        port_full = port + basep_snapshot(port_img, CODE_SEG) + lstk_snapshot(port_img, LSTK_SEG)
        check("load_finish: 8080-model (code-only) .CMD", ref_full, port_full)

        # -- no PD to initialize (load_pd == 0) -- just the Base Page --
        mem = base_memory(1)
        put_ldtab(mem, 0, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, 0)
        ref, ref_img, port, port_img = run_both(mem)
        check("load_finish: load_pd == 0 -> base page only", ref, port)

        # -- no CSEG anywhere -> e_no_cseg --
        mem = base_memory(1)
        put_ldtab(mem, 0, DATA_SEG, 0x10, 0x10, 3)  # neither type 1 nor 2
        mem_w16(mem, SD_LOAD_PD, 0)
        ref, ref_img, port, port_img = run_both(mem)
        check("load_finish: no code/data group -> e_no_cseg", ref, port)

        # -- Data group's own bpg_cseg is 0 -> e_no_cseg (from the PD path) --
        mem = base_memory(2)
        put_ldtab(mem, 0, LSTK_SEG, (ULEN // 16), (ULEN // 16), 0)
        put_ldtab(mem, 1, DATA_SEG, 0x10, 0x10, 2)
        mem_w16(mem, SD_LOAD_PD, PD_ADDR)
        mem_w16(mem, phys(WRKSEG) + FCB_ADDR, 0)
        mem_w16(mem, SD_LOAD_FCB, FCB_ADDR)
        ref, ref_img, port, port_img = run_both(mem)
        check("load_finish: bpg_cseg == 0 -> e_no_cseg", ref, port)

        # ================= diskread =================
        P_FLAG = 6
        PF_CTLC = 0x80
        # A dedicated, code-free scratch segment for these two sections'
        # own FCB -- WRKSEG/FCB_ADDR above (load_finish's own choice)
        # physically lands at phys(0x40)+0x30 = 0x430, well inside this
        # wrapper's own ~1.5KB of cseg code (0-0x627), which load_finish's
        # own tests never actually read/write far enough into to notice;
        # fcb_nxtrec (offset 0x20 further in) would collide with real
        # code bytes there.
        DR_WRKSEG, DR_FCB_ADDR = 0x0100, 0x0000

        def diskread_case(label, dma_seg, dma_ofst, fcb_nxtrec_in, records, pf_ctlc=False):
            mem = base_memory(0)
            mem_w16(mem, UDA_WRKSEG, DR_WRKSEG)
            mem_w16(mem, SD_LOAD_FCB, DR_FCB_ADDR)
            mem[phys(DR_WRKSEG) + DR_FCB_ADDR + FCB_NXTREC] = fcb_nxtrec_in
            put_disk_records(mem, records)
            mem_w16(mem, SD_RLR, PD_ADDR)
            mem[PD_ADDR + P_FLAG] = PF_CTLC if pf_ctlc else 0
            ref_regs, ref_img = run_reference(code, diskread_entry, mem, {'dx': dma_ofst, 'cx': dma_seg},
                                               call_hooks={MPMIF_STUB: make_freadseq_hook()})
            buf = to_ctypes(mem)
            got = lib.diskread(ctypes.cast(buf, c_u8p), ctypes.c_uint16(dma_ofst), ctypes.c_uint16(dma_seg))
            port_img = bytes(buf)
            dma_base = phys(dma_seg) + dma_ofst
            nxt_addr = phys(DR_WRKSEG) + DR_FCB_ADDR + FCB_NXTREC
            ref = ((ref_regs['bx'], ref_regs['cx']), bytes(ref_img[dma_base:dma_base + 128]), ref_img[nxt_addr])
            port = ((got.bx, got.cx), bytes(port_img[dma_base:dma_base + 128]), port_img[nxt_addr])
            check(label, ref, port)

        diskread_case("diskread: success, copies record 0", DATA_SEG, 0x0010, 0,
                       [bytes([0x11]) * 128])
        diskread_case("diskread: success, advances to record 1", DATA_SEG, 0x0010, 1,
                       [bytes([0x11]) * 128, bytes([0x22]) * 128])
        diskread_case("diskread: pf_ctlc set -> e_abort (read still happens)", DATA_SEG, 0x0010, 0,
                       [bytes([0x33]) * 128], pf_ctlc=True)

        # ================= load_group =================
        LDTAB_FSTRT, LDTAB_FLEN = 0x0A, 0x0C
        SD_LOAD_INDMA = 0xA9E7

        def load_group_case(label, fstrt, flen, dest_seg, records, indma=0xFFFF):
            mem = base_memory(1)
            mem_w16(mem, UDA_WRKSEG, DR_WRKSEG)
            mem_w16(mem, SD_LOAD_FCB, DR_FCB_ADDR)
            mem[phys(DR_WRKSEG) + DR_FCB_ADDR + FCB_NXTREC] = 0
            put_disk_records(mem, records)
            mem_w16(mem, SD_RLR, PD_ADDR)
            mem[PD_ADDR + P_FLAG] = 0
            mem_w16(mem, SD_LOAD_INDMA, indma)
            put_ldtab(mem, 0, dest_seg, 0, 0, 2)
            mem_w16(mem, SD_LOAD_LDTAB + LDTAB_FSTRT, fstrt)
            mem_w16(mem, SD_LOAD_LDTAB + LDTAB_FLEN, flen)
            ref_regs, ref_img = run_reference(code, load_group_entry, mem, {'si': SD_LOAD_LDTAB},
                                               call_hooks={MPMIF_STUB: make_freadseq_hook()})
            buf = to_ctypes(mem)
            got_cx = lib.load_group(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SD_LOAD_LDTAB), ctypes.c_uint16(SYSDAT))
            port_img = bytes(buf)
            dest_base = phys(dest_seg)
            want_bytes = flen * 16
            ref = (ref_regs['cx'], bytes(ref_img[dest_base:dest_base + want_bytes]))
            port = (got_cx, bytes(port_img[dest_base:dest_base + want_bytes]))
            check(label, ref, port)

        # one full 128-byte record, straight to the destination (xfer_direct)
        load_group_case("load_group: one full record (xfer_direct)", 0, 8, DATA_SEG,
                         [bytes([0xAA]) * 128])
        # two full records
        load_group_case("load_group: two full records (xfer_direct x2)", 0, 16, DATA_SEG,
                         [bytes([0xAA]) * 128, bytes([0xBB]) * 128])
        # a partial (sub-record) length -- goes through the shared load_dma buffer
        load_group_case("load_group: partial record (via load_dma)", 0, 3, DATA_SEG,
                         [bytes([0xCC]) * 128])
        # fstrt already within what's "in dma" (load_indma == fstrt): the
        # transfer-from-buffer path runs without any new diskread call at
        # all -- pre-seed load_dma directly to prove that
        mem_setup = base_memory(1)
        mem_w16(mem_setup, UDA_WRKSEG, DR_WRKSEG)
        mem_w16(mem_setup, SD_LOAD_FCB, DR_FCB_ADDR)
        put_disk_records(mem_setup, [bytes([0xEE]) * 128])  # never actually read
        mem_w16(mem_setup, SD_RLR, PD_ADDR)
        mem_w16(mem_setup, SD_LOAD_INDMA, 0)
        SD_LOAD_DMA = 0xAB00
        mem_setup[SD_LOAD_DMA:SD_LOAD_DMA + 128] = bytes([0xDD]) * 128
        put_ldtab(mem_setup, 0, DATA_SEG, 0, 0, 2)
        mem_w16(mem_setup, SD_LOAD_LDTAB + LDTAB_FSTRT, 2)  # 2 paragraphs into the buffer
        mem_w16(mem_setup, SD_LOAD_LDTAB + LDTAB_FLEN, 3)   # take exactly 3 paragraphs
        ref_regs, ref_img = run_reference(code, load_group_entry, mem_setup, {'si': SD_LOAD_LDTAB},
                                           call_hooks={MPMIF_STUB: make_freadseq_hook()})
        buf = to_ctypes(mem_setup)
        got_cx = lib.load_group(ctypes.cast(buf, c_u8p), ctypes.c_uint16(SD_LOAD_LDTAB), ctypes.c_uint16(SYSDAT))
        port_img = bytes(buf)
        dest_base = phys(DATA_SEG)
        ref = (ref_regs['cx'], bytes(ref_img[dest_base:dest_base + 48]))
        port = (got_cx, bytes(port_img[dest_base:dest_base + 48]))
        check("load_group: already-in-dma transfer, no new read needed", ref, port)

        # ================= load (full end-to-end) =================
        # A real success case: one relative Code group (8080-model,
        # since there's no Data group), load_pd=0 (no PD init -- just
        # returns the Base Page). Exercises the whole chain for real:
        # diskread (header + content), get_ch_info, malloc_entry (via
        # a freshly-created MAU off the MFL), load_group, load_finish.
        lib.load.restype = BxCx
        lib.load.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16,
                              ctypes.c_uint16, ctypes.c_uint16, ctypes.c_int]
        lib.cload_entry.restype = BxCx
        lib.cload_entry.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16,
                                     ctypes.c_uint16, ctypes.c_uint16]

        P_MEM = 0x16
        LOAD_WRKSEG, LOAD_FCB = 0x4000, 0x2000  # phys(LOAD_WRKSEG) == 0, matching
            # malloc_entry's/maualloc_entry's own flat-vs-segmented convention
            # mismatch the same way test_memory.py's own SYSDAT choice does
        RLR_PD = 0x8000
        MD_FREE, MFL_MD = 0x4300, 0x4400
        CODE_SEG_WANT = 0x0500

        def build_ch_header(entries):
            rec = bytearray(128)
            for i, (form, length, base, minv, maxv) in enumerate(entries):
                off = i * 9
                rec[off] = form
                mem_w16(rec, off + 1, length)
                mem_w16(rec, off + 3, base)
                mem_w16(rec, off + 5, minv)
                mem_w16(rec, off + 7, maxv)
            return bytes(rec)

        mem = bytearray(0x10000)
        mem_w16(mem, UDA_WRKSEG, LOAD_WRKSEG)
        mem[phys(LOAD_WRKSEG) + LOAD_FCB + FCB_NXTREC] = 0
        mem_w16(mem, SD_RLR, RLR_PD)
        mem_w16(mem, RLR_PD + P_MEM, 0)  # the calling process's own p_mem list is empty
        mem_w16(mem, SD_MDUL, MD_FREE)  # one free MD for getmd
        mem_w16(mem, SD_MFL, MFL_MD)    # one free region for mlalloc_entry
        mem_w16(mem, MFL_MD + 0, 0)            # m_link -- end of list
        mem_w16(mem, MFL_MD + 2, CODE_SEG_WANT)  # m_start
        mem_w16(mem, MFL_MD + 4, 20)            # m_length (paragraphs) -- plenty
        mem_w16(mem, SD_MAL, 0)  # no active MAUs yet

        header = build_ch_header([(1, 8, 0, 8, 8)])  # one relative Code group, 1 record
        content = bytes([0xCC]) * 128
        put_disk_records(mem, [header, content])

        ref_regs, ref_img = run_reference(
            code, entries['load'], mem, {'dx': LOAD_FCB, 'bx': 0},
            call_hooks={MPMIF_STUB: make_freadseq_hook({
                F_MALLOC: entries['malloc_entry'], F_MEMFREE: entries['mfree_entry'],
                F_MAUALLOC: entries['maualloc_entry'], F_MLALLOC: entries['mlalloc_entry'],
            })})
        buf = to_ctypes(mem)
        got = lib.load(ctypes.cast(buf, c_u8p), ctypes.c_uint16(LOAD_FCB), ctypes.c_uint16(0),
                        ctypes.c_uint16(SYSDAT), ctypes.c_uint16(user_retf), ctypes.c_uint16(CS_VAL),
                        ctypes.c_int(0))
        port_img = bytes(buf)

        def load_snapshot(img, basep_seg):
            base = phys(basep_seg)
            # the group's own 8-paragraph (128-byte) allocation holds
            # the Base Page itself (bpg_8080/bpg_cseg) in its first
            # ~0x5C bytes, with the loaded content (0xCC) surviving in
            # the remainder -- BPG_DMA (a field *offset*, not "end of
            # struct") would read past this single-group allocation
            # into unrelated, always-zero memory, so it's not checked
            return (img[base + BPG_8080], mem_r16(img, base + BPG_CSEG),
                    bytes(img[base + 0x5C:base + 128]))

        ref = ((ref_regs['bx'], ref_regs['cx']), load_snapshot(ref_img, ref_regs['bx']))
        port = ((got.bx, got.cx), load_snapshot(port_img, got.bx))
        check("load: one relative Code group, full success end-to-end", ref, port)

        # -- cload_entry: chain-load cleanup, real success end to end --
        # the calling process (CALLER_PD) owns two segments: KEEP_MD,
        # matching the new PD's own (pre-existing) p_uda -- must survive
        # untouched -- and FREE_MD, mistakenly Load-flagged, which
        # ld_fnext must un-flag before cpmfree_entry's own free-all walk
        # genuinely frees it (and, since it's its MAU's only real SAT
        # entry, releases that MAU to the MFL too -- same mechanics as
        # test_cpmmem.py's own "free-all...released to MFL" case).
        # Afterward the (now-trimmed) p_mem list is reparented onto the
        # new PD, and the rest of `load` runs exactly as the plain
        # success case above (one relative Code group).
        CALLER_PD, CHILD_PD = 0x8000, 0x8100
        UDA_SEG_KEEP = 0x0050
        KEEP_MD, FREE_MD = 0x4200, 0x4250
        MAU_SEG2, MAU_REC2 = 0x0100, 0x4600
        MD_FREE2, MFL_MD2 = 0x4300, 0x4400
        DUMMY_SEG, DUMMY_MAU = 0x0150, 0x4700

        mem2 = bytearray(0x10000)
        mem_w16(mem2, UDA_WRKSEG, LOAD_WRKSEG)
        mem2[phys(LOAD_WRKSEG) + LOAD_FCB + FCB_NXTREC] = 0
        mem_w16(mem2, SD_RLR, CALLER_PD)
        mem_w16(mem2, CHILD_PD + P_UDA, UDA_SEG_KEEP)

        mem_w16(mem2, CALLER_PD + P_MEM, KEEP_MD)
        mem_w16(mem2, KEEP_MD + MS_LINK, FREE_MD)
        mem_w16(mem2, KEEP_MD + MS_START, UDA_SEG_KEEP)
        mem_w16(mem2, KEEP_MD + MS_FLAGS, MF_LOAD)
        # KEEP_MD's own ms_mau must point somewhere real -- after
        # reparenting onto CHILD_PD, malloc_entry's own mall_pdver tries
        # every existing MD's own MAU first; left at 0 it reads `m_start`
        # off real cseg bytes in the reference run (nonzero, since that's
        # genuine code) but off zeroed buffer in the port, diverging --
        # a MAU with no free space at all fails this step identically and
        # deterministically on both sides, falling through to mall_ml
        mem_w16(mem2, KEEP_MD + MS_MAU, DUMMY_MAU)
        mem_w16(mem2, DUMMY_MAU + MS_START, DUMMY_SEG)
        mem_w16(mem2, DUMMY_MAU + MS_LENGTH, 2)
        mem_w16(mem2, DUMMY_MAU + MS_LINK, 0)
        put_sat(mem2, DUMMY_SEG, 1, 0x10, 5, 1)
        put_sat(mem2, DUMMY_SEG, 2, 0x15, 10, 1)

        mem_w16(mem2, FREE_MD + MS_LINK, 0)
        mem_w16(mem2, FREE_MD + MS_START, 0x300)
        mem_w16(mem2, FREE_MD + MS_LENGTH, 0x10)
        mem_w16(mem2, FREE_MD + MS_FLAGS, MF_LOAD)  # mistakenly flagged -- ld_fnext must clear it
        mem_w16(mem2, FREE_MD + MS_MAU, MAU_REC2)
        mem_w16(mem2, MAU_REC2 + MS_START, MAU_SEG2)
        mem_w16(mem2, MAU_REC2 + MS_LENGTH, 0x1000)
        mem_w16(mem2, MAU_REC2 + MS_LINK, 0)
        # a second, untouched allocated entry keeps this MAU "still in use"
        # after FREE_MD's own entry is freed -- it never goes anywhere near
        # SD_MFL, so the later ldtab[0]-only allocation (22 paragraphs, off
        # MFL_MD2/CODE_SEG_WANT) can't collide with it; MAU-release itself
        # is already covered end to end by test_memory.py/test_cpmmem.py.
        # A third (boundary) entry is required too -- mau_collapse's own
        # real asm returns its "assume empty" default the moment it finds
        # a zero sat_start *before* ever examining entry 2's own nall, so
        # without a real entry 3 behind it entry 2 is never actually
        # checked at all.
        put_sat(mem2, MAU_SEG2, 1, 0x300, 0x10, 1)
        put_sat(mem2, MAU_SEG2, 2, 0x310, 0x10, 1)
        put_sat(mem2, MAU_SEG2, 3, 0x320, 0x10, 1)
        mem_w16(mem2, SD_MAL, MAU_REC2)
        mem_w16(mem2, SD_MFL, MFL_MD2)

        mem_w16(mem2, SD_MDUL, MD_FREE2)
        mem_w16(mem2, MD_FREE2 + MS_LINK, 0)
        mem_w16(mem2, MFL_MD2 + MS_LINK, 0)
        mem_w16(mem2, MFL_MD2 + MS_START, CODE_SEG_WANT)
        mem_w16(mem2, MFL_MD2 + MS_LENGTH, 100)  # comfortably enough for ldtab[0] alone (UDA+LDSTK, 22)

        # no Code/Data group at all -- get_ch_info finds nothing, so only
        # ldtab[0] (UDA+LDSTK) needs allocating; load_finish then hits its
        # own already-covered "no code/data group -> e_no_cseg" exit (see
        # the plain load_finish case for that), keeping this case focused
        # on the chain cleanup itself rather than load_group/disk content
        header2 = build_ch_header([])
        put_disk_records(mem2, [header2])

        chain_real_targets = {
            F_MALLOC: entries['malloc_entry'], F_MEMFREE: entries['mfree_entry'],
            F_MAUALLOC: entries['maualloc_entry'], F_MLALLOC: entries['mlalloc_entry'],
            F_MAUFREE: entries['maufree_entry'], F_MLFREE: entries['mlfree_entry'],
            F_FREEMEM: entries['cpmfree_entry'],
        }
        ref_regs2, ref_img2 = run_reference(
            code, entries['cload_entry'], mem2, {'dx': LOAD_FCB, 'bx': CHILD_PD},
            call_hooks={MPMIF_STUB: make_freadseq_hook(chain_real_targets)},
            jmp_hooks={MPMIF_STUB: make_jmp_redirect_hook(chain_real_targets)})
        buf2 = to_ctypes(mem2)
        got2 = lib.cload_entry(ctypes.cast(buf2, c_u8p), ctypes.c_uint16(LOAD_FCB), ctypes.c_uint16(CHILD_PD),
                                ctypes.c_uint16(SYSDAT), ctypes.c_uint16(user_retf), ctypes.c_uint16(CS_VAL))
        port_img2 = bytes(buf2)

        # CALLER_PD's own p_mem list is the one piece of state purely under
        # the chain cleanup's own control (trimmed by the free-all walk,
        # then handed wholesale to CHILD_PD) -- checked directly. What
        # happens *after* the cleanup (which MAU malloc_entry's own
        # existing-MAU-first search ends up satisfying ldtab[0]'s combined
        # allocation from) is mau.c's/ml.c's own already-independently-
        # verified "try an existing MAU before making a new one" logic,
        # not something this test needs to pin down a specific outcome
        # for -- both sides agreeing on the overall (bx, cx) and on
        # CALLER_PD ending up with nothing left is the meaningful check.
        ref2 = ((ref_regs2['bx'], ref_regs2['cx']), mem_r16(ref_img2, CALLER_PD + P_MEM))
        port2 = ((got2.bx, got2.cx), mem_r16(port_img2, CALLER_PD + P_MEM))
        check("cload_entry: real chain-load cleanup (free-all, reparent to new PD)", ref2, port2)

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
