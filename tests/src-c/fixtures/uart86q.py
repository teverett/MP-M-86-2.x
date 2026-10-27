"""Shared test-only simulated UART (8250/16450 @ 0x3F8, COM1) backing
both tools/i86sim.py's `io_in_hook`/`io_out_hook` and
src-c/loader/ldbios86q.c's own `io_in8`/`io_out8` C callbacks, for
tests/src-c/test_ldbios86q.py's differential test -- one real device
model driving both sides of the same comparison, same spirit as every
other fixture in this directory feeding the same memory image to both
the reference run and the C port.

Deliberately NOT a general 8250 emulation: only the two registers (LSR,
RXTX) src-assem/loader/LDBIOS86Q.A86's own CONST/CONIN/CONOUT touch, plus the
three one-time setup registers (LCR/IER/MCR) both LDBIOS86Q.A86's own
INIT and src-assem/loader/ROM86Q.A86's own `start:` write and never read back
-- recorded (in `setup_writes`) but otherwise ignored, same "don't
raise" job tests/src-c/fixtures/fdc86q.py's own Fdc86Q gives the 8237
DMA ports. TXRDY (LSR bit 5) is always reported set -- this model's own
`out8` "transmits" instantly (just appends to `tx`), so there's never a
reason for the real `cout_wait` polling loop to spin; it IS still a
real loop under this model (not special-cased away), just one that
always succeeds first try. RXRDY (LSR bit 0) reflects whether `rx` (a
byte queue fed by the test) still has bytes left.
"""

LSR = 0x3FD
RXTX = 0x3F8
LSR_RXRDY = 0x01
LSR_TXRDY = 0x20

SETUP_PORTS = frozenset({0x3F9, 0x3FB, 0x3FC})  # IER, LCR, MCR


class Uart86Q:
    def __init__(self, rx_bytes=b''):
        self.rx = list(rx_bytes)
        self.tx = []
        self.setup_writes = []

    def in8(self, port):
        if port == LSR:
            val = LSR_TXRDY
            if self.rx:
                val |= LSR_RXRDY
            return val
        if port == RXTX:
            if self.rx:
                return self.rx.pop(0)
            return 0
        raise ValueError(f"Uart86Q: unexpected IN from port 0x{port:04X}")

    def out8(self, port, value):
        if port == RXTX:
            self.tx.append(value & 0xFF)
            return
        if port in SETUP_PORTS:
            self.setup_writes.append((port, value & 0xFF))
            return
        raise ValueError(f"Uart86Q: unexpected OUT to port 0x{port:04X}")
