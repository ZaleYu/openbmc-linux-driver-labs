# I2C and SMBus Protocol

## Electrical model

I2C normally uses two open-drain signals: serial data (`SDA`) and serial clock
(`SCL`). Devices pull a line low but do not actively drive it high. Pull-up
resistors restore the high level. This permits wired-AND arbitration and clock
stretching, but also makes capacitance, pull-up selection, leakage, voltage
domains, and stuck-low failures important during board bring-up.

Check the actual component specifications before selecting pull-ups. A logic
analyzer can decode protocol events, but an oscilloscope is required to assess
rise time, ringing, voltage level, and marginal signal integrity.

## Basic transfer

A controller (historically called a master) starts a transfer while the bus is
idle:

```text
START -> 7-bit address + R/W -> ACK -> data byte -> ACK/NACK -> ... -> STOP
```

- **START:** SDA falls while SCL is high.
- **STOP:** SDA rises while SCL is high.
- **Repeated START:** begins another address phase without releasing the bus.
- **ACK:** receiver pulls SDA low during the ninth clock.
- **NACK:** SDA remains high during the ninth clock.
- **Address:** commonly 7 bits; 10-bit addressing exists but is less common in BMC designs.

A typical register read is a combined transaction:

```text
START -> address+W -> register -> REPEATED START -> address+R -> data -> NACK -> STOP
```

Do not replace the repeated START with STOP/START unless the datasheet says the
device accepts it.

## Speed classes

Common nominal rates include 100 kHz Standard-mode, 400 kHz Fast-mode, and
1 MHz Fast-mode Plus. The usable rate is constrained by every device, mux,
level shifter, trace, connector, pull-up, and controller on the bus.

## Clock stretching and arbitration

A target may hold SCL low to delay the controller if both sides support clock
stretching. A controller must enforce a practical timeout so a failed target
does not block system management indefinitely.

On a multi-controller bus, each controller samples SDA while transmitting. If
it sends a high but observes a low, it loses arbitration and must stop driving,
then retry according to the platform policy. Arbitration support in hardware
does not define ownership policy; BMC and host firmware still need rules for
retries, reset, update, and power transitions.

## I2C versus SMBus

SMBus is derived from I2C and defines a more constrained command model and
additional behavior. Important differences include:

| Topic | I2C | SMBus |
|---|---|---|
| Transactions | Flexible messages | Defined byte/word/block protocols |
| Timeout | Not universally required | Includes timeout expectations |
| Clock range | Several I2C modes | More constrained by SMBus version |
| Integrity | Device-specific | Optional Packet Error Code (PEC) |
| Alert | Device-specific IRQ | SMBALERT# and Alert Response Address defined |

Linux exposes SMBus helpers such as `i2c_smbus_read_byte_data()` even when the
underlying controller implements them by translating to I2C messages. Query
adapter functionality rather than assuming every operation is available.

## Packet Error Code

PEC is a CRC-8 appended to supported SMBus transactions. It detects corruption
but does not provide authentication. Both endpoints and the selected operation
must support it. A PEC error can come from signal integrity, timing, the wrong
transaction shape, or enabling PEC on only one endpoint.

## Common failures

| Symptom | Likely directions |
|---|---|
| Address NACK | Device absent, reset/power off, wrong address, wrong mux channel |
| Data NACK | Unsupported command, write protection, device state |
| SCL stuck low | Stretching, short, controller/target fault |
| SDA stuck low | Interrupted transfer, target state machine, short |
| Arbitration lost | Another controller transmitted at the same time |
| Timeout | Stuck line, excessive stretching, controller IRQ/state error |
| Wrong data | Endianness, register pointer, stale cache, wrong device at address |
| Intermittent CRC/PEC | Electrical margin, timing, topology, wrong PEC setup |

