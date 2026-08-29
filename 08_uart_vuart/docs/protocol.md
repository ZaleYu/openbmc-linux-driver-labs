# UART fundamentals

UART is asynchronous: the peers agree on baud rate and frame format rather than
sharing a clock. A common `115200 8N1` frame has one start bit, eight data bits,
no parity and one stop bit.

| Setting | Meaning |
|---|---|
| Baud | Symbol rate; both ends must match closely |
| Data bits | Commonly 7 or 8 |
| Parity | None, even, odd, mark, or space |
| Stop bits | Commonly 1 or 2 |
| Flow control | None, RTS/CTS hardware, or XON/XOFF software |

At 8N1, each payload byte consumes ten wire bits, so 115200 baud carries at
most about 11.5 kB/s before higher-level framing.

TTL/CMOS UART voltage is not RS-232 voltage. Never connect a BMC SoC pin
directly to an RS-232 connector without the correct transceiver. Confirm voltage
domain, pin direction, common ground, inversion and whether the pinmux routes
the signal internally or externally.

UART provides bytes, not messages. A device protocol must define framing:
newline, fixed length, length prefix, sentinel/escaping, timeout, sequence
number and checksum/CRC. Recovery from a lost byte must be designed.

Common errors are framing, parity, overrun and break. A readable stream with
garbled characters usually indicates baud/clock or format mismatch. Missing
bytes under load often indicate FIFO/IRQ/DMA latency or absent flow control.

