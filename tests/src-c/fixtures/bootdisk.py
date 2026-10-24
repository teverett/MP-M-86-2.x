"""Shared test-only synthetic floppy image backing both
tools/i86sim.py's `int_hooks` (keyed on INT 13h) and
src-c/loader/rom86q.c's own `int13_fn` C callback, for
tests/src-c/test_rom86q.py's differential test -- same "one real
model, two call sites" shape as tests/src-c/fixtures/uart86q.py's own
Uart86Q.

Deliberately narrow: AH=2 (read sector)/AH=0 (reset) only, keyed by
(cyl, head, sector) with no other BIOS INT 13h semantics modeled.
"""

AH_RESET = 0x00
AH_READ = 0x02


class BootDisk:
    def __init__(self):
        self.sectors = {}   # (cyl, head, sector) -> bytes(512)
        self.fail_at = set()  # (cyl, head, sector) that always reports cf=1
        self.calls = []      # every (ah, al, ch, cl, dh, dl) call, in order

    def set_sector(self, cyl, head, sector, data):
        assert len(data) == 512
        self.sectors[(cyl, head, sector)] = bytes(data)

    def read_chs(self, ah, al, ch, cl, dh, dl):
        self.calls.append((ah, al, ch, cl, dh, dl))
        if ah == AH_RESET:
            return 0, bytes(512)
        assert ah == AH_READ and al == 1
        key = (ch, dh, cl)
        if key in self.fail_at:
            return 1, bytes(512)
        return 0, self.sectors.get(key, bytes(512))
