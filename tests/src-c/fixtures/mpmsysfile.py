"""Shared test-only synthetic "MPM.SYS" file backing both
tools/i86sim.py's own `int_hooks` (keyed on the loader's own
synthetic `int 224`/`sys_vec`) and src-c/loader/ldmpm.c's own
`read_fn` C callback, for tests/src-c/test_ldmpm.py's differential
test -- same "one real model, two call sites" shape as
tests/src-c/fixtures/uart86q.py's own Uart86Q.

A real CP/M-style sequential read (`readsf`): each call returns the
next 128-byte logical record, or EOF once the file is exhausted.
`fail_at` injects a read error (al != 0/1) at a given 0-based call
index, for testing src-c/loader/ldmpm.c's own `rerr` path.
"""


class MpmSysFile:
    def __init__(self, records):
        self.records = list(records)
        self.pos = 0
        self.fail_at = None  # 0-based call index, or None

    def read_next(self):
        index = self.pos
        self.pos += 1
        if self.fail_at is not None and index == self.fail_at:
            return 0xFF, bytes(128)
        if index >= len(self.records):
            return 1, bytes(128)  # EOF
        return 0, self.records[index]
