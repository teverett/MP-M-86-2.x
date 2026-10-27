"""Shared test-only simulated NEC 765/8272 floppy controller
(@ 0x3F0, the ISA port block src-assem/loader/LDBIOS86Q.A86's own
fdc_out_byte/fdc_in_byte poll) backing both tools/i86sim.py's
`io_in_hook`/`io_out_hook` and src-c/loader/ldbios86q.c's own
`io_in8`/`io_out8` C callbacks, for tests/src-c/test_ldbios86q.py's
differential test -- same "one real model, two call sites" shape as
tests/src-c/fixtures/uart86q.py's own Uart86Q.

Deliberately narrow: only the Main Status Register (MSR) and Data
register, and only enough state-machine behavior to let a command
byte stream in (MSR always reports ready-for-input, so
`fdc_out_byte`'s own polling loop always succeeds on the first try)
and a fixed result-byte sequence stream back out for SENSE INTERRUPT
STATUS (always ready-for-output once the command byte count for the
command in progress has been fully sent). Not a real controller
model: no seek/recalibrate/read/write state, no interrupt, no media.

Also records (without validating) the 8237 DMA channel-2 register
writes `dma_setup` (src-c/loader/ldbios86q.c) makes -- the actual
disk<->secbuf transfer those program is never modeled on either side
of the differential test (see that file's own header comment), so
this fixture's own job for them is just "don't raise."
"""

MSR = 0x3F4
DATA = 0x3F5
MSR_RQM = 0x80
MSR_DIO = 0x40
MSR_BUSY = 0x10

CMD_SENSE_INT = 0x08
CMD_READ = 0x46
CMD_WRITE = 0x45

# total bytes (command + parameters) this fixture knows how to count
# off before triggering that command's own result phase -- see out8's
# own comment for why the trigger has to wait for the LAST byte, not
# the first (a real controller only reports DIO=1/"ready to read a
# result" once the whole command it's now holding has actually been
# received; triggering any earlier would make `fdc_out_byte`'s own
# wait for DIO=0 -- "ready for another parameter byte" -- spin
# forever on this same command's own remaining parameter bytes).
CMD_LEN = {CMD_SENSE_INT: 1, CMD_READ: 9, CMD_WRITE: 9}
RESULT_LEN = {CMD_SENSE_INT: 2, CMD_READ: 7, CMD_WRITE: 7}

DMA_PORTS = frozenset({0x04, 0x05, 0x81, 0x0A, 0x0B, 0x0C})


class Fdc86Q:
    def __init__(self):
        self.sent = []          # every command/parameter byte written to DATA
        self.result_queue = []  # bytes queued up to be read back via DATA
        self.dma_writes = []    # every 8237 DMA channel-2 register write
        self._cmd_remaining = 0
        self._cmd_byte = None

    def queue_result(self, *bytes_):
        self.result_queue.extend(bytes_)

    def in8(self, port):
        if port == MSR:
            val = MSR_RQM
            if self.result_queue:
                val |= MSR_DIO
            return val
        if port == DATA:
            if self.result_queue:
                return self.result_queue.pop(0)
            return 0
        raise ValueError(f"Fdc86Q: unexpected IN from port 0x{port:04X}")

    def out8(self, port, value):
        if port == DATA:
            value &= 0xFF
            self.sent.append(value)
            if self._cmd_remaining == 0 and value in CMD_LEN:
                self._cmd_byte = value
                self._cmd_remaining = CMD_LEN[value] - 1  # this byte itself counts
            elif self._cmd_remaining > 0:
                self._cmd_remaining -= 1
            if self._cmd_remaining == 0 and self._cmd_byte is not None and not self.result_queue:
                self.queue_result(*([0x00] * RESULT_LEN[self._cmd_byte]))
                self._cmd_byte = None
            return
        if port == 0x3F2:  # DOR -- INIT's own motor/drive-select write
            self.dor = value & 0xFF
            return
        if port in DMA_PORTS:
            self.dma_writes.append((port, value & 0xFF))
            return
        raise ValueError(f"Fdc86Q: unexpected OUT to port 0x{port:04X}")
