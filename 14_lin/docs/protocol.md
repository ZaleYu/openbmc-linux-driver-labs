# LIN protocol essentials

## Roles and physical bus

LIN is a single-master, multiple-slave, single-wire serial network, commonly
up to 20 kbit/s. A LIN transceiver converts logic-level UART TX/RX into the
dominant/recessive bus voltage levels and supplies wake/sleep behavior. The bus
needs the correct master/slave termination and pull-up network.

The master owns time. It sends headers according to a schedule table; the node
configured as publisher for that identifier supplies the response. Other nodes
may subscribe. Arbitration is not performed as in CAN.

## Frame structure

| Field | Sender | Purpose |
|---|---|---|
| Break | Master | Dominant low for at least 13 nominal bit times |
| Break delimiter | Master | Recessive interval after break |
| Sync `0x55` | Master | Let slaves measure bit timing |
| Protected identifier | Master | 6-bit ID plus two parity bits |
| Data | Publisher | 1 to 8 bytes defined by schedule/protocol |
| Checksum | Publisher | Classic or enhanced integrity byte |

Protected-ID parity is:

```text
P0 = ID0 xor ID1 xor ID2 xor ID4
P1 = not(ID1 xor ID3 xor ID4 xor ID5)
PID = ID[5:0] | (P0 << 6) | (P1 << 7)
```

IDs `0x3C` and `0x3D` are reserved for diagnostic transport. Do not assign them
as ordinary unconditional application frames.

## Checksums

The checksum uses 8-bit one's-complement addition with end-around carry. The
classic checksum covers data bytes only. The enhanced checksum covers the PID
and data. Diagnostic frames use the classic form even in LIN 2.x profiles.

The checksum catches many transmission faults but does not authenticate the
sender, prevent replay, or provide application freshness.

## Schedule tables

The master schedule defines header order and slot timing. Unconditional frames
have a fixed publisher. Event-triggered frames can reduce bandwidth but require
collision resolution. Sporadic frames are sent when master-side data changes.
Diagnostic schedules carry configuration or transport requests/responses.

The slot must cover header, worst-case response, inter-byte space, clock error,
and implementation latency. A Linux userspace process cannot automatically
provide hard real-time slot timing.

## Sleep and wakeup

LIN nodes support network sleep and wakeup signaling. Transceiver enable/sleep
GPIO polarity, wake sources, and BMC suspend policy must be coordinated. A
stuck-awake transceiver can violate platform standby power; an unintended wake
can disturb the managed system.

## Common errors

- Missing response: wrong schedule/ID, sleeping slave, broken wire, publisher off.
- PID parity error: corrupt header, baud mismatch, break/sync problem.
- Checksum error: corruption, wrong classic/enhanced selection, wrong payload.
- Framing error: physical/timing fault or UART break handling.
- Collision: more than one publisher or unresolved event-triggered response.
- Stale value: schedule/service stopped even though the last sample looked valid.
