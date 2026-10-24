"""Shared test-only combined UART+FDC device that just records every
OUT and reports the FDC always ready-for-input on every IN -- for a
test case whose own job is to check the OUT *sequence* a routine
produces (e.g. a hardware-init routine's own register writes), not to
model any real device state machine. See tests/src-c/fixtures/
uart86q.py/fdc86q.py for the real device models used once a routine's
own control flow genuinely depends on what comes back from a read.
"""


class IoRecorder:
    def __init__(self):
        self.writes = []

    def in8(self, port):
        if port == 0x3F4:  # fdc_msr
            return 0x80  # RQM set, DIO/BUSY clear
        raise ValueError(f"IoRecorder: unexpected IN from port 0x{port:04X}")

    def out8(self, port, value):
        self.writes.append((port, value & 0xFF))
