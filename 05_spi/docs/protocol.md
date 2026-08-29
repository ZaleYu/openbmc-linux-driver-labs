# SPI protocol essentials

SPI is a synchronous, normally full-duplex link. A host drives `SCLK` and one
chip-select per target. `MOSI` carries host-to-target data and `MISO` carries
target-to-host data. Unlike I2C, SPI has no address phase, ACK, arbitration, or
standard device discovery. The board description must identify every target.

## Clock modes

| Mode | CPOL | CPHA | Idle clock | Sampling edge |
|---|---:|---:|---|---|
| 0 | 0 | 0 | Low | Leading |
| 1 | 0 | 1 | Low | Trailing |
| 2 | 1 | 0 | High | Leading |
| 3 | 1 | 1 | High | Trailing |

Always derive the mode from the device timing diagram. A wrong mode may return
plausible but shifted data. Also check MSB/LSB order, bits per word, maximum
frequency, CS setup/hold time, and whether CS must remain asserted between a
command and its response.

## Example register protocol

The demo uses mode 0, 8-bit words, MSB first. Bit 7 of the first byte means
read; bits 6:0 select a register. Read data is clocked out during following
dummy bytes. A write sends a register byte followed by its value.

| Register | Meaning |
|---:|---|
| `0x00` | Device ID, expected `0x5a` |
| `0x01..0x02` | Signed temperature in centi-degrees C, big-endian |
| `0x03` | Status; bit 0 is over-temperature alarm |
| `0x04..0x05` | Writable maximum temperature, centi-degrees C |

## Common variations

- Three-wire SPI shares input and output on one data pin.
- Dual/quad/octal SPI uses multiple data lanes, especially for flash.
- Daisy chains pass shifted data through multiple devices and do not map to
  independent chip selects.
- Some targets require half-duplex transfers or inter-transfer delays.
- SPI-NOR has standardized command families and belongs behind the MTD stack.

Electrical failures include contention on MISO, floating MISO, level mismatch,
excessive edge ringing, wrong pull resistors, crossed data lines, and a target
remaining selected across another device's transfer.

