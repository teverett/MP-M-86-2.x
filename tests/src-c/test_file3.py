#!/usr/bin/env python3
"""Differential test for src-c/kernel/file3.c against a self-contained
subset of src/kernel/FILE3.BDO: pack_sdcnt, compare, search_olist/
search_llist/searchn_list (+ internal search_listo). See file3.c's own
header comment for why the rest of the file is deferred.
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

WRAPPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'file3_wrapper.A86')
FILE3_C = [os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file3.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file1.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file2.c'),
           os.path.join(REPO_ROOT, 'src-c', 'kernel', 'file4.c')]
INCLUDE_DIR = os.path.join(REPO_ROOT, 'src-c', 'kernel')

STACK_TOP = 0xFFF0

BD_SDCNT, BD_SDBLK, BD_CURDSK, BD_PACKED_DCNT = 0x0C7D, 0x0C7F, 0x0C36, 0x0C37
BD_OPEN_ROOT, BD_LOCK_ROOT, BD_CUR_POS, BD_PRV_POS = 0x0C90, 0x0C92, 0x0C79, 0x0C7B
BD_BLKSHF, BD_BLKMSK = 0x0C4A, 0x0C4B
BD_FREE_ROOT = 0x0052
BD_PDADDR, BD_FLUSHED, BD_PD_CNT = 0x0C3A, 0x0C8D, 0x0C33
P_UDA, U_PD_CNT = 0x10, 0x1A
BD_ARET, BD_RETURN_FFFF, BD_ERROR_MODE = 0x0C0F, 0x0C69, 0x0C29
BD_SELDSK, BD_HIGH_EXT, BD_ACTUAL_RC, BD_USRCODE = 0x0C16, 0x0C34, 0x0C6C, 0x0C15
BD_INFO, BD_LINFO, BD_XFCB_READ_ONLY = 0x0C17, 0x0C9B, 0x0C35
RLR = 0x0068  # real resolved address in THIS wrapper's own combined layout
SD_RLR = 0xA000  # src-c/kernel/defs.h's own separate scratch address for the same field
P_FLAG = 0x06
PF_CTLC = 0x0080


def phys(seg):
    return (seg * 16) & 0xFFFF

# mpmif and its 9 XIOS-call helpers, plus XCRLF/XPRINT -- all real
# BDOSIF.BDO labels this combined bundle needs to assemble, even
# though nothing file3.c ports actually reaches them (see
# file1_wrapper.A86's own header comment for why these can't just be
# equ'd to a plain stub address).
STUBS = {0x48b, 0x4b8, 0x4bd, 0x4c2, 0x4c7, 0x4cc, 0x4d1, 0x4d6, 0x4db, 0x4e0, 0x409, 0x416}


def assemble_reference():
    asm = Assembler()
    asm.assemble(WRAPPER, [os.path.join(REPO_ROOT, 'src', 'kernel')])
    code = asm.segment_bytes('cseg')
    dseg = asm.segment_bytes('dseg')
    names = ('pack_sdcnt', 'compare', 'search_olist', 'search_llist', 'searchn_list',
              'delete_item', 'create_item', 'set_olist_item',
              'flush_files', 'flush_file0', 'remove_locks', 'inc_pdcnt',
              'pr_term', 'xcurselect', 'reselectx', 'goback', 'xiosif')
    entries = {name: asm.symtab[name].value for name in names}
    return code, dseg, entries


def build_c_library():
    lib_path = tempfile.mktemp(suffix='.dylib' if sys.platform == 'darwin' else '.so')
    subprocess.run(
        ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
         '-I', INCLUDE_DIR, *FILE3_C, '-o', lib_path],
        check=True,
    )
    return lib_path


def mem_w16(mem, addr, val):
    mem[addr] = val & 0xFF
    mem[addr + 1] = (val >> 8) & 0xFF


def mem_r16(mem, addr):
    return mem[addr] | (mem[addr + 1] << 8)


def base_memory():
    return bytearray(0x10000)


def run_reference(code, dseg, entry, mem, regs, stop_addrs=(), segmented=False, jmp_stub_addrs=()):
    # Same cseg/dseg layering as test_file1.py's own run_reference --
    # see file1_wrapper.A86's own header comment for why this is
    # needed for this combined bundle specifically.
    image = bytearray(0x10000)
    image[0:len(code)] = code
    image[0xC00:len(dseg)] = dseg[0xC00:]
    for addr, val in enumerate(mem):
        if val != 0:
            image[addr] = val
    image[BD_CURDSK] = mem[BD_CURDSK]  # dseg's own default there is 0xFF, not 0
    # BD_FREE_ROOT (0x52) sits below the 0xC00 dseg-restore boundary above,
    # so it keeps whatever raw cseg code byte was already there unless
    # explicitly overridden -- the enumerate loop above only overlays
    # *nonzero* mem bytes, which silently skips this field whenever its
    # real test value happens to have a zero byte (same class of gap
    # BD_CURDSK's own explicit override line already works around).
    image[BD_FREE_ROOT:BD_FREE_ROOT + 2] = mem[BD_FREE_ROOT:BD_FREE_ROOT + 2]
    # rlr's own real address (0x68, see test_file1.py's own identical
    # comment) needs the same unconditional overwrite, for the same
    # reason -- the "only nonzero mem bytes win" rule above silently
    # skips it whenever the PD address's own low byte happens to be 0.
    image[RLR:RLR + 2] = mem[RLR:RLR + 2]
    sim = I86Sim(image, {**regs, 'sp': STACK_TOP}, entry, stop_addrs=set(stop_addrs),
                 call_stub_addrs=STUBS, segmented=segmented, jmp_stub_addrs=set(jmp_stub_addrs))
    sim.run()
    return sim.regs, image


def olist_entry(link, drive, dcnt3, pdaddr):
    """link(2) || atts(1, low nibble = drive) || dcnt(3) || pdaddr(2) || opncnt(2)"""
    b = bytearray(11)
    mem_w16(b, 0, link)
    b[2] = drive & 0x0F
    b[3:6] = dcnt3
    mem_w16(b, 6, pdaddr)
    mem_w16(b, 8, 0)
    return bytes(b)


def main():
    code, dseg, entries = assemble_reference()
    lib_path = build_c_library()
    try:
        lib = ctypes.CDLL(lib_path)
        c_u8p = ctypes.POINTER(ctypes.c_uint8)

        lib.pack_sdcnt.argtypes = [c_u8p, ctypes.c_uint16]
        lib.compare.restype = ctypes.c_uint8
        lib.compare.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8]
        lib.search_olist.restype = ctypes.c_uint8
        lib.search_olist.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.search_llist.restype = ctypes.c_uint8
        lib.search_llist.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.searchn_list.restype = ctypes.c_uint8
        lib.searchn_list.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint8]
        lib.delete_item.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.create_item.restype = ctypes.c_uint8
        lib.create_item.argtypes = [c_u8p, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16)]
        lib.set_olist_item.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8]
        lib.flush_files.argtypes = [c_u8p, ctypes.c_uint16]
        lib.flush_file0.argtypes = [c_u8p, ctypes.c_uint16]
        lib.remove_locks.argtypes = [c_u8p, ctypes.c_uint16, ctypes.c_uint16]
        lib.inc_pdcnt.argtypes = [c_u8p, ctypes.c_uint16]
        lib.pr_term.argtypes = [c_u8p, ctypes.c_uint16]
        lib.xcurselect.argtypes = [c_u8p, ctypes.c_uint16]
        lib.reselectx.argtypes = [c_u8p, ctypes.c_uint16]

        failures = []

        def check(label, ref, port):
            status = "ok" if ref == port else "FAIL"
            print(f"[{status}] {label}: reference {ref} | port {port}")
            if status == "FAIL":
                failures.append(label)

        def to_ctypes(mem):
            return (ctypes.c_uint8 * 0x10000)(*mem)

        # -- pack_sdcnt: sdblk == 0, derive dblk from sdcnt --
        mem = base_memory()
        mem_w16(mem, BD_SDBLK, 0)
        mem_w16(mem, BD_SDCNT, 0x1234)
        mem[BD_BLKSHF] = 3
        mem[BD_BLKMSK] = 0x07
        ref_regs, ref_img = run_reference(code, dseg, entries['pack_sdcnt'], mem, {})
        buf = to_ctypes(mem)
        lib.pack_sdcnt(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        ref = bytes(ref_img[BD_PACKED_DCNT:BD_PACKED_DCNT + 3])
        port = bytes(bytes(buf)[BD_PACKED_DCNT:BD_PACKED_DCNT + 3])
        check("pack_sdcnt: sdblk == 0", ref, port)

        # -- pack_sdcnt: sdblk != 0, used directly --
        mem = base_memory()
        mem_w16(mem, BD_SDBLK, 0x0042)
        mem_w16(mem, BD_SDCNT, 0x5678)
        mem[BD_BLKSHF] = 4
        mem[BD_BLKMSK] = 0x0F
        ref_regs, ref_img = run_reference(code, dseg, entries['pack_sdcnt'], mem, {})
        buf = to_ctypes(mem)
        lib.pack_sdcnt(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        ref = bytes(ref_img[BD_PACKED_DCNT:BD_PACKED_DCNT + 3])
        port = bytes(bytes(buf)[BD_PACKED_DCNT:BD_PACKED_DCNT + 3])
        check("pack_sdcnt: sdblk != 0", ref, port)

        # compare itself returns via ZF, not a register i86sim's own
        # `regs` dict exposes -- already exercised indirectly through
        # search_olist's own ch == 5 cases below (both the matching
        # and mismatching packed_dcnt cases), which check compare's
        # real effect on cur_pos/prv_pos instead.

        # -- search_olist: empty list -> not found --
        mem = base_memory()
        mem_w16(mem, BD_OPEN_ROOT, 0)
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0100})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(1))
        ref = (mem_r16(ref_img, BD_PRV_POS),)
        port = (mem_r16(bytes(buf), BD_PRV_POS),)
        check("search_olist: empty list, prv_pos stays at root", ref, port)

        # -- search_olist: ch == 0 ("next item") always succeeds --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM:ITEM + 11] = olist_entry(0, 3, b'\x01\x02\x03', 0x5000)
        mem_w16(mem, BD_OPEN_ROOT, ITEM)
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0000})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(0))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("search_olist: ch == 0 returns the next item", ref, port)

        # -- search_olist: ch == 1, matching drive found --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM:ITEM + 11] = olist_entry(0, 5, b'\x00\x00\x00', 0)
        mem_w16(mem, BD_OPEN_ROOT, ITEM)
        mem[BD_CURDSK] = 5
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0100})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(1))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("search_olist: ch == 1, drive matches", ref, port)

        # -- search_olist: ch == 1, drive mismatch (target sorts after) -> keep walking --
        mem = base_memory()
        ITEM1, ITEM2 = 0x4100, 0x4200
        mem[ITEM1:ITEM1 + 11] = olist_entry(ITEM2, 2, b'\x00\x00\x00', 0)
        mem[ITEM2:ITEM2 + 11] = olist_entry(0, 9, b'\x00\x00\x00', 0)
        mem_w16(mem, BD_OPEN_ROOT, ITEM1)
        mem[BD_CURDSK] = 9
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0100})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(1))
        ref = (mem_r16(ref_img, BD_CUR_POS), mem_r16(ref_img, BD_PRV_POS))
        port = (mem_r16(bytes(buf), BD_CUR_POS), mem_r16(bytes(buf), BD_PRV_POS))
        check("search_olist: walks past a lower drive to find a match", ref, port)

        # -- search_olist: ch == 1, target sorts before this element -> not found, insertion point --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM:ITEM + 11] = olist_entry(0, 9, b'\x00\x00\x00', 0)
        mem_w16(mem, BD_OPEN_ROOT, ITEM)
        mem[BD_CURDSK] = 2
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0100})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(1))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("search_olist: not found, cur_pos is the insertion point", ref, port)

        # -- search_olist: ch == 5, matching drive + packed_dcnt + pdaddr --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM:ITEM + 11] = olist_entry(0, 4, b'\x11\x22\x33', 0x7777)
        mem_w16(mem, BD_OPEN_ROOT, ITEM)
        mem[BD_CURDSK] = 4
        mem[BD_PACKED_DCNT:BD_PACKED_DCNT + 3] = b'\x11\x22\x33'
        mem_w16(mem, BD_PACKED_DCNT + 3, 0x7777)  # pdaddr, right after packed_dcnt
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0500})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(5))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("search_olist: ch == 5, full match", ref, port)

        # -- search_olist: ch == 5, drive matches but packed_dcnt doesn't -> not found --
        mem[BD_PACKED_DCNT] = 0xFF  # mismatch
        ref_regs, ref_img = run_reference(code, dseg, entries['search_olist'], mem, {'cx': 0x0500})
        buf = to_ctypes(mem)
        got = lib.search_olist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(5))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("search_olist: ch == 5, drive matches, packed_dcnt doesn't", ref, port)

        # -- search_llist: same machinery, different root --
        mem = base_memory()
        ITEM = 0x4300
        mem[ITEM:ITEM + 11] = olist_entry(0, 7, b'\x00\x00\x00', 0)
        mem_w16(mem, BD_LOCK_ROOT, ITEM)
        mem[BD_CURDSK] = 7
        ref_regs, ref_img = run_reference(code, dseg, entries['search_llist'], mem, {'cx': 0x0100})
        buf = to_ctypes(mem)
        got = lib.search_llist(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(1))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("search_llist: matching drive", ref, port)

        # -- searchn_list: resumes from cur_pos, not a fixed root --
        mem = base_memory()
        ITEM1, ITEM2 = 0x4100, 0x4200
        mem[ITEM1:ITEM1 + 11] = olist_entry(ITEM2, 1, b'\x00\x00\x00', 0)
        mem[ITEM2:ITEM2 + 11] = olist_entry(0, 6, b'\x00\x00\x00', 0)
        mem_w16(mem, BD_CUR_POS, ITEM1)
        mem[BD_CURDSK] = 6
        ref_regs, ref_img = run_reference(code, dseg, entries['searchn_list'], mem, {'cx': 0x0100})
        buf = to_ctypes(mem)
        got = lib.searchn_list(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint8(1))
        ref = mem_r16(ref_img, BD_CUR_POS)
        port = mem_r16(bytes(buf), BD_CUR_POS)
        check("searchn_list: resumes from cur_pos", ref, port)

        # -- delete_item: unlink from a 2-node chain, push onto free list --
        mem = base_memory()
        PRV, ITEM, NEXT, OLD_FREE = 0x4100, 0x4200, 0x4300, 0x4400
        mem_w16(mem, PRV, ITEM)
        mem_w16(mem, ITEM, NEXT)
        mem_w16(mem, BD_PRV_POS, PRV)
        mem_w16(mem, BD_FREE_ROOT, OLD_FREE)
        ref_regs, ref_img = run_reference(code, dseg, entries['delete_item'], mem, {'bx': ITEM})
        buf = to_ctypes(mem)
        lib.delete_item(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(ITEM))
        port_img = bytes(buf)
        ref = (mem_r16(ref_img, PRV), mem_r16(ref_img, BD_FREE_ROOT), mem_r16(ref_img, ITEM),
               mem_r16(ref_img, BD_CUR_POS))
        port = (mem_r16(port_img, PRV), mem_r16(port_img, BD_FREE_ROOT), mem_r16(port_img, ITEM),
                mem_r16(port_img, BD_CUR_POS))
        check("delete_item: unlinks and pushes onto free list", ref, port)

        # -- create_item: free list empty -- returns 0 (Z set), nothing changes --
        mem = base_memory()
        mem_w16(mem, BD_FREE_ROOT, 0)
        ref_regs, ref_img = run_reference(code, dseg, entries['create_item'], mem, {})
        buf = to_ctypes(mem)
        bx_out = ctypes.c_uint16(0)
        got = lib.create_item(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.byref(bx_out))
        ref = mem_r16(ref_img, BD_FREE_ROOT)  # unchanged (still 0) either way
        port = mem_r16(bytes(buf), BD_FREE_ROOT)
        check("create_item: empty free list, nothing changes", (ref, 0), (port, 0 if got == 0 else 1))

        # -- create_item: pops free_root, links in after prv_pos --
        mem = base_memory()
        FREE, FREE_NEXT, PRV, PRV_NEXT = 0x4100, 0x4200, 0x4300, 0x4400
        mem_w16(mem, BD_FREE_ROOT, FREE)
        mem_w16(mem, FREE, FREE_NEXT)
        mem_w16(mem, BD_PRV_POS, PRV)
        mem_w16(mem, PRV, PRV_NEXT)
        ref_regs, ref_img = run_reference(code, dseg, entries['create_item'], mem, {})
        buf = to_ctypes(mem)
        bx_out = ctypes.c_uint16(0)
        got = lib.create_item(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.byref(bx_out))
        port_img = bytes(buf)
        ref = (mem_r16(ref_img, BD_FREE_ROOT), mem_r16(ref_img, PRV), mem_r16(ref_img, FREE),
               mem_r16(ref_img, BD_CUR_POS))
        port = (mem_r16(port_img, BD_FREE_ROOT), mem_r16(port_img, PRV), mem_r16(port_img, FREE),
                mem_r16(port_img, BD_CUR_POS))
        check("create_item: pops free list, links after prv_pos", ref, port)
        check("create_item: returns the popped item's address", FREE, bx_out.value if got else None)

        # -- set_olist_item: fills in attrs/drive byte + packed_dcnt/pdaddr, zeros opncnt --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM:ITEM + 11] = olist_entry(0, 0, b'\xaa\xaa\xaa', 0xaaaa)
        mem[BD_CURDSK] = 0x05  # low nibble = drive
        mem[BD_PACKED_DCNT:BD_PACKED_DCNT + 3] = b'\x11\x22\x33'
        mem_w16(mem, BD_PACKED_DCNT + 3, 0x7777)  # pdaddr
        ref_regs, ref_img = run_reference(code, dseg, entries['set_olist_item'], mem,
                                           {'bx': ITEM, 'ax': 0x0080})
        buf = to_ctypes(mem)
        lib.set_olist_item(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(ITEM), ctypes.c_uint8(0x80))
        ref = bytes(ref_img[ITEM:ITEM + 11])
        port = bytes(bytes(buf)[ITEM:ITEM + 11])
        check("set_olist_item: attrs|drive, packed_dcnt/pdaddr copy, opncnt zeroed", ref, port)

        # -- inc_pdcnt: ordinary bump --
        mem = base_memory()
        mem[BD_PD_CNT] = 0x10
        ref_regs, ref_img = run_reference(code, dseg, entries['inc_pdcnt'], mem, {})
        buf = to_ctypes(mem)
        lib.inc_pdcnt(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        check("inc_pdcnt: ordinary bump", ref_img[BD_PD_CNT], bytes(buf)[BD_PD_CNT])

        # -- inc_pdcnt: wraps past 0 --
        mem = base_memory()
        mem[BD_PD_CNT] = 0xF8
        ref_regs, ref_img = run_reference(code, dseg, entries['inc_pdcnt'], mem, {})
        buf = to_ctypes(mem)
        lib.inc_pdcnt(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        check("inc_pdcnt: wraps past 0", ref_img[BD_PD_CNT], bytes(buf)[BD_PD_CNT])

        # -- remove_locks: olist item without the lock bit -> no-op --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM:ITEM + 11] = olist_entry(0, 0, b'\x00\x00\x00', 0)  # attrs byte has no 0x40 bit
        ref_regs, ref_img = run_reference(code, dseg, entries['remove_locks'], mem, {'bx': ITEM})
        buf = to_ctypes(mem)
        lib.remove_locks(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(ITEM))
        check("remove_locks: no lock bit -> no-op", mem_r16(ref_img, BD_LOCK_ROOT),
              mem_r16(bytes(buf), BD_LOCK_ROOT))

        # -- remove_locks: removes every lock record owned by this item --
        mem = base_memory()
        ITEM = 0x4100
        mem[ITEM + 2] = 0x40  # lock bit set
        LOCK1, LOCK2 = 0x4300, 0x4400  # LOCK1 belongs to ITEM, LOCK2 doesn't
        mem_w16(mem, LOCK1, LOCK2)
        mem_w16(mem, (LOCK1 + 8) & 0xFFFF, ITEM)
        mem_w16(mem, LOCK2, 0)
        mem_w16(mem, (LOCK2 + 8) & 0xFFFF, 0x9999)
        mem_w16(mem, BD_LOCK_ROOT, LOCK1)
        mem_w16(mem, BD_FREE_ROOT, 0)
        ref_regs, ref_img = run_reference(code, dseg, entries['remove_locks'], mem, {'bx': ITEM})
        buf = to_ctypes(mem)
        lib.remove_locks(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0), ctypes.c_uint16(ITEM))
        port_img = bytes(buf)
        ref = (mem_r16(ref_img, BD_LOCK_ROOT), mem_r16(ref_img, BD_FREE_ROOT))
        port = (mem_r16(port_img, BD_LOCK_ROOT), mem_r16(port_img, BD_FREE_ROOT))
        check("remove_locks: removes the owned record, keeps the other", ref, port)

        # -- flush_file0: one matching-drive open file, owning PD not yet
        # counted this pass and matches BD_PDADDR -> PD_CNT bumped --
        mem = base_memory()
        ITEM, PD, PD_UDA_SEG = 0x4100, 0x5000, 0x0800
        mem[ITEM:ITEM + 11] = olist_entry(0, 3, b'\x00\x00\x00', PD)
        mem_w16(mem, BD_OPEN_ROOT, ITEM)
        mem[BD_CURDSK] = 3
        mem_w16(mem, PD + P_UDA, PD_UDA_SEG)
        mem[phys(PD_UDA_SEG) + U_PD_CNT] = 0
        mem_w16(mem, BD_PDADDR, PD)
        mem[BD_PD_CNT] = 0x10
        mem_w16(mem, BD_FREE_ROOT, 0)
        ref_regs, ref_img = run_reference(code, dseg, entries['flush_file0'], mem, {}, segmented=True)
        buf = to_ctypes(mem)
        lib.flush_file0(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        port_img = bytes(buf)

        def flush_snapshot(img):
            return (mem_r16(img, BD_OPEN_ROOT), img[phys(PD_UDA_SEG) + U_PD_CNT], img[BD_PD_CNT])

        check("flush_file0: flushes the file, marks + counts its PD", flush_snapshot(ref_img),
              flush_snapshot(port_img))

        # -- flush_files: only runs flush_file0 once (BD_FLUSHED guards it) --
        mem = base_memory()
        mem[BD_FLUSHED] = 1
        mem_w16(mem, BD_OPEN_ROOT, 0x4100)  # would crash if flush_file0 ran for real
        ref_regs, ref_img = run_reference(code, dseg, entries['flush_files'], mem, {}, segmented=True)
        buf = to_ctypes(mem)
        lib.flush_files(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        check("flush_files: already flushed -> no-op", mem_r16(ref_img, BD_OPEN_ROOT),
              mem_r16(bytes(buf), BD_OPEN_ROOT))

        # -- pr_term: two open files, one owned by the target PD
        # (removed), one not (kept) --
        mem = base_memory()
        ITEM1, ITEM2 = 0x4100, 0x4120
        TARGET_PD, OTHER_PD = 0x5000, 0x6000
        mem[ITEM1:ITEM1 + 11] = olist_entry(ITEM2, 0, b'\x00\x00\x00', TARGET_PD)
        mem[ITEM2:ITEM2 + 11] = olist_entry(0, 0, b'\x00\x00\x00', OTHER_PD)
        mem_w16(mem, BD_OPEN_ROOT, ITEM1)
        mem_w16(mem, BD_PDADDR, TARGET_PD)
        mem_w16(mem, BD_FREE_ROOT, 0)
        ref_regs, ref_img = run_reference(code, dseg, entries['pr_term'], mem, {},
                                           stop_addrs={entries['goback']}, segmented=True,
                                           jmp_stub_addrs={entries['xiosif']})
        buf = to_ctypes(mem)
        lib.pr_term(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        port_img = bytes(buf)
        check("pr_term: removes only the target PD's own open file",
              mem_r16(ref_img, BD_OPEN_ROOT), mem_r16(port_img, BD_OPEN_ROOT))

        # -- xcurselect: SELDSK != CURDSK, selectdisk's own fixed-fail
        # boundary stub (file1.c) always reaches selerror -- with
        # RETURN_FFFF set around the call, goerr's own RTN_PHY_ERRS
        # branch forces aret=0xFFFF and clears the flag again --
        mem = base_memory()
        mem[BD_SELDSK] = 1
        mem[BD_CURDSK] = 2
        mem[BD_ERROR_MODE] = 0
        mem_w16(mem, RLR, 0x5000)
        mem_w16(mem, SD_RLR, 0x5000)  # both the real bundle address (ref) and the
                                      # C port's own separate scratch one need it
        mem_w16(mem, 0x5000 + P_FLAG, 0)
        ref_regs, ref_img = run_reference(code, dseg, entries['xcurselect'], mem, {},
                                           stop_addrs={entries['goback']})
        buf = to_ctypes(mem)
        lib.xcurselect(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        port_img = bytes(buf)

        def xcurselect_snapshot(img):
            return (mem_r16(img, BD_ARET), img[BD_RETURN_FFFF], mem_r16(img, 0x5000 + P_FLAG))

        check("xcurselect: aret forced to 0xFFFF, flag cleared, pf_ctlc set",
              xcurselect_snapshot(ref_img), xcurselect_snapshot(port_img))

        # -- reselectx: HIGH_EXT forced to 0, ACTUAL_RC forced to 0,
        # same CURSELECT-chain tail as reselect -- SELDSK==CURDSK and
        # not 0xFF, so CURSELECT's own TEST_FF_DISK takes the plain
        # early return (no selerror involved here) --
        mem = base_memory()
        INFO_AT = 0x4200
        mem[INFO_AT] = 0  # auto-select (not an explicit drive request)
        mem_w16(mem, BD_INFO, INFO_AT)
        mem[BD_SELDSK] = 0
        mem[BD_CURDSK] = 0
        mem[BD_USRCODE] = 0
        mem[BD_HIGH_EXT] = 0xAA
        mem[BD_ACTUAL_RC] = 0xBB
        ref_regs, ref_img = run_reference(code, dseg, entries['reselectx'], mem, {})
        buf = to_ctypes(mem)
        lib.reselectx(ctypes.cast(buf, c_u8p), ctypes.c_uint16(0))
        port_img = bytes(buf)

        def reselectx_snapshot(img):
            return (img[BD_HIGH_EXT], img[BD_ACTUAL_RC], img[INFO_AT])

        check("reselectx: forces high_ext/actual_rc, runs the shared tail",
              reselectx_snapshot(ref_img), reselectx_snapshot(port_img))

        if failures:
            print(f"\n{len(failures)} case(s) failed: {', '.join(failures)}")
            return 1
        print("\nall cases match.")
        return 0
    finally:
        os.remove(lib_path)


if __name__ == '__main__':
    sys.exit(main())
