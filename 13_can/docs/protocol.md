# CAN Protocol Essentials

CAN is a multi-master, message-oriented protocol. A node broadcasts a message with a CAN ID; every node can observe it and decides whether to process it according to the ID and upper-layer protocol.

Understanding CAN requires more than `cansend` and `candump`: physical bus design, dominant/recessive states, arbitration, frame format, ACK, bit timing, error confinement, CAN FD, and upper-layer responsibilities all matter.

---

# 1. Physical CAN Bus

High-speed CAN normally uses ISO 11898-2 differential signals:

```text
SoC / MCU --> CAN Controller --> CAN Transceiver
    --> CAN_H / CAN_L Twisted-Pair Bus
```

| Component | Responsibility |
| --- | --- |
| CAN Controller | Frames, arbitration, CRC, ACK, bit timing, error counters |
| CAN Transceiver | Converts logic signals to differential CAN_H/CAN_L |

An MCU may integrate the controller but usually still requires an external transceiver.

---

# 2. Differential Signaling

The receiver mainly observes the voltage difference between CAN_H and CAN_L. Differential transmission improves common-mode noise immunity and suits long, industrial, and automotive networks.

Ground is still important. Excessive ground offset can exceed the transceiver’s common-mode range, cause intermittent reception failures, or damage hardware.

---

# 3. Linear Bus and Stubs

High-speed CAN normally uses a linear bus with short stubs:

```text
120 Ω                                      120 Ω
  |                                          |
Node A ---- Node B ---- Node C ---- Node D
```

Avoid star topologies and long stubs because they cause reflections, ringing, edge distortion, sampling and CRC errors. Higher bitrates generally allow shorter total bus and stub lengths.

---

# 4. 120 Ω Termination

Place one 120 Ω terminator at each physical end—not at every node. Two in parallel measure approximately 60 Ω between CAN_H and CAN_L when powered off.

- About 120 Ω may indicate one terminator.
- About 40 Ω may indicate three terminators.

Split termination, chokes, transceiver circuits, bias resistors, and protection components can affect measurements.

---

# 5. Transceiver Standby

Transceivers may have Enable, Standby, Shutdown, Silent, or Wake pins. Incorrect GPIO polarity can leave `can0` present and UP with changing TX counters while producing no valid bus waveform.

Check Device Tree GPIO polarity, datasheet, regulator, pinmux, and measured waveform. A working controller driver does not prove the transceiver is enabled.

---

# 6. Dominant and Recessive

Simplified:

```text
Dominant  = Logical 0
Recessive = Logical 1
```

Dominant overrides recessive when nodes transmit simultaneously. This wired-AND-like behavior enables non-destructive arbitration.

---

# 7. Multi-Master Operation

Any ready node may begin transmitting when the bus is idle. No central controller grants access; simultaneous transmitters use arbitration to decide which frame continues.

---

# 8. Arbitration

Each transmitter monitors the bus while sending. A node that sends recessive but observes dominant knows that a higher-priority frame is present. It stops without sending an error or corrupting the winner and retries when the bus becomes idle.

This is non-destructive bitwise arbitration.

---

# 9. Arbitration Example

```text
Node A: 0x100 --> 001 0000 0000
Node B: 0x200 --> 010 0000 0000
```

At the first differing bit, B sends recessive 1 while A sends dominant 0, so B loses and A completes its frame. For identifiers of the same format, a numerically smaller base ID generally has higher priority.

---

# 10. Standard Versus Extended Arbitration

Do not compare every standard and extended frame using only the full numeric value. If they share the same first 11-bit base ID, the standard frame normally wins at the relevant SRR/IDE/RTR arbitration bits. Compare the complete arbitration field.

---

# 11. CAN ID Is Not a Receiver Address

A CAN ID normally represents message meaning and priority, not a simple destination address.

| CAN ID | Example Meaning |
| --- | --- |
| `0x080` | Emergency Shutdown |
| `0x100` | Power Fault |
| `0x123` | Temperature Telemetry |
| `0x200` | Fan Command |
| `0x700` | Diagnostic Information |

All nodes may see a frame; hardware filters and applications decide whether to process it. Source, destination, group, or node addressing must be defined by CANopen, J1939, ISO-TP, or a custom protocol.

---

# 12. ID Planning Is Also Priority Planning

Urgent traffic should use higher priority (usually lower numeric IDs), but frequent high-priority traffic can starve lower-priority messages. Production design must analyze periods, deadlines, frame lengths, bitrate, worst-case blocking, bus load, and starvation.

---

# 13. Standard and Extended Identifiers

| Format | Identifier Length |
| --- | --- |
| Standard CAN | 11 bits (`0x000`–`0x7FF`) |
| Extended CAN | 29 bits |

SocketCAN uses a flag to mark extended frames; a large value alone does not automatically select extended format.

---

# 14. Classical CAN Frame

```text
SOF --> Arbitration --> Control --> Data --> CRC --> ACK --> EOF
```

| Field | Purpose |
| --- | --- |
| SOF | Begins the frame |
| Arbitration | CAN ID, RTR, IDE, etc. |
| Control | Format and DLC |
| Data | 0–8 payload bytes |
| CRC | Detects transmission corruption |
| ACK | Confirms at least one receiver accepted the frame physically |
| EOF | Ends the frame |

---

# 15. Data Frame

```text
CAN ID: 0x123
DLC:    4
Data:   11 22 33 44
```

Classical CAN carries 0–8 payload bytes. Byte order, scaling, and semantics are defined entirely by the upper-layer protocol.

---

# 16. Remote Frame

A Remote Frame requests a Data Frame with the same ID and is identified by RTR. If Data and Remote Frames with the same ID arbitrate, the Data Frame normally wins.

Remote Frames are uncommon in modern designs: CAN FD does not support them, periodic publish or explicit request/response is easier to control, controller support varies, and freshness/security are difficult to define.

---

# 17. Bit Stuffing

After five consecutive identical bits, the transmitter inserts the opposite bit to maintain synchronization. The receiver removes it; failure to observe the required opposite bit is a Stuff Error.

Stuffing makes actual bus time depend on payload bit patterns, so worst-case load calculations must include it.

---

# 18. CRC

The transmitter and receivers calculate and compare a CRC. A mismatch invalidates the frame, may trigger an Error Frame, and usually causes retransmission.

CRC detects accidental corruption but does not provide authentication, encryption, malicious-modification protection, or anti-replay.

---

# 19. ACK

During the ACK slot, the transmitter sends recessive; any controller that correctly received the frame can drive dominant.

ACK means that at least one other CAN controller received a valid format and CRC. It does not mean userspace was listening, the application accepted or executed the command, D-Bus changed, or security validation succeeded.

---

# 20. Why ACK Works Without a Userspace Reader

```text
CAN Controller Receives Valid Frame
        +--> Hardware Generates ACK
        v
Linux Driver --> SocketCAN Filter --> Userspace
```

ACK occurs before delivery to userspace, so it can happen without `candump`, an open socket, a matching filter, or application processing.

---

# 21. What Does Missing ACK Mean?

Common causes include only one active node, unpowered peers, listen-only mode, mismatched bitrate/sample point, reversed CAN_H/CAN_L, standby transceiver, bad termination/wiring, bus-off peers, or a Classical-only node unable to parse CAN FD.

It normally does not mean that remote `candump` is not running.

---

# 22. End-to-End Acknowledgement

Application success requires an upper-layer response, for example:

```text
Request ID 0x200: Set Fan 50%, Sequence 7
Response ID 0x201: Success, Sequence 7
```

The protocol must also handle timeout, retry, duplicates, sequence wrap, late responses, resets, and command idempotency.

---

# 23. Bit Timing

A bit contains Sync, Propagation, Phase Segment 1, and Phase Segment 2. Linux commonly combines Propagation + Phase Segment 1 as TSEG1:

```text
| Sync |       TSEG1        | TSEG2 |
                                  ^
                              Sample Point
```

The segments handle synchronization and delays. SJW limits the adjustment made during resynchronization.

---

# 24. Time Quantum

Time Quantum (TQ) is derived from controller clock and prescaler:

```text
Total TQ per Bit = 1 + TSEG1 + TSEG2
Bitrate = Clock / [BRP × (1 + TSEG1 + TSEG2)]
```

Controller register encodings may include `+1` details; let the Linux CAN framework and driver handle them.

---

# 25. Sample Point

```text
Sample Point = (1 + TSEG1) / (1 + TSEG1 + TSEG2)
```

A value of 87.5% samples near 87.5% of the bit time. Choose it based on network/transceiver delay, oscillator tolerance, bitrate, topology, and CAN FD data phase. Matching only `500000` bitrate may not provide enough margin on long or high-speed networks.

---

# 26. How Linux Calculates Bit Timing

The Linux CAN framework uses controller clock, `can_bittiming_const`, requested bitrate/sample point, and supported BRP/TSEG/SJW ranges.

```sh
sudo ip link set can0 type can bitrate 500000
```

The controller driver must report the correct clock, TSEG ranges, maximum SJW, BRP range, and BRP increment. Incorrect Device Tree or driver clocks produce incorrect register settings.

---

# 27. Specifying the Sample Point

```sh
sudo ip link set can0 type can \
    bitrate 500000 \
    sample-point 0.875

ip -details link show can0
```

The controller may choose the closest realizable value. Verify the resulting bitrate, sample point, TQ, prop-seg, phase-seg1/2, SJW, and BRP.

---

# 28. Error Detection

| Error | Meaning |
| --- | --- |
| Bit Error | Transmitted and observed bits differ, excluding valid arbitration/ACK cases |
| Stuff Error | Bit-stuffing rule violated |
| CRC Error | Receiver CRC does not match |
| Form Error | Invalid bit in a fixed-format field |
| ACK Error | No other node generated ACK |

A detecting node may send an Error Frame, invalidating the current frame. Automatic retransmission can create heavy traffic on a faulty bus.

---

# 29. Error Counters

Controllers maintain Transmit Error Counter (TEC) and Receive Error Counter (REC). Transmit errors are usually weighted more heavily because a faulty transmitter can disrupt the whole network. These counters confine faulty nodes.

---

# 30. Error Confinement States

| State | Meaning |
| --- | --- |
| Error Active | Normal operation; may send active error flags |
| Error Warning | Counter exceeds warning threshold |
| Error Passive | Many errors; error signaling is restricted |
| Bus-Off | Transmitter is logically isolated from the bus |

Warning commonly begins around 96 and Passive around 128, subject to implementation. Severe transmit errors beyond the limit (commonly TEC > 255) cause Bus-Off. REC alone normally does not cause Bus-Off.

---

# 31. Viewing Linux Error State

```sh
ip -details -statistics link show can0
candump -e can0
```

Look for `ERROR-ACTIVE`, `ERROR-WARNING`, `ERROR-PASSIVE`, `BUS-OFF`, and counters such as `berr-counter tx 128 rx 0`.

SocketCAN Error Frames report controller/bus status to userspace; they are not application data frames.

---

# 32. Bus-Off Recovery

```sh
sudo ip link set can0 type can restart-ms 100
sudo ip link set can0 type can restart
```

Before recovery, record detailed state, TX/RX errors, Error Frames, packet/drop counters, and Bus-Off count. Immediate automatic recovery can hide the root cause.

---

# 33. Risk of Repeated Restart

If peers use 250 kbit/s while `can0` uses 500 kbit/s, `restart-ms 100` can repeatedly restart, disrupt the bus, enter Bus-Off, and restart again.

Production systems need restart limits, backoff, fault logging, interface isolation, maintenance mode, fail-safe behavior, and defined human-intervention conditions.

---

# 34. CAN FD

CAN FD adds FDF, BRS, ESI, payloads up to 64 bytes, revised CRC rules, and an optional faster data phase.

```text
Arbitration Phase: Nominal Bitrate
        |
FDF / BRS / Control
        |
Data Phase: Optional Higher Bitrate
        |
Payload + CRC
```

---

# 35. FDF

FDF distinguishes Classical CAN from CAN FD. A Classical-only node that cannot tolerate an FD frame may report a Form Error and send an Error Frame. Never enable FD on one production node without validating the entire bus.

---

# 36. BRS

Bit Rate Switch allows arbitration at the nominal rate and data at a faster rate:

```text
Nominal Bitrate = 500 kbit/s
Data Bitrate    = 2 Mbit/s
```

All nodes need compatible nominal timing; every node receiving the FD frame needs compatible data-phase timing.

---

# 37. ESI

Error State Indicator shows whether the transmitter is Error Active or Error Passive. It is not an application error code or a complete health report.

---

# 38. CAN FD DLC

| DLC | Payload Bytes |
| --- | --- |
| 0–8 | 0–8 |
| 9 | 12 |
| 10 | 16 |
| 11 | 20 |
| 12 | 24 |
| 13 | 32 |
| 14 | 48 |
| 15 | 64 |

DLC 15 means 64 bytes, not 15. SocketCAN applications should use the CAN FD frame structure and length and let the kernel/driver encode DLC.

---

# 39. CAN FD Data-Phase Bit Timing

```sh
sudo ip link set can0 type can \
    bitrate 500000 \
    sample-point 0.875 \
    dbitrate 2000000 \
    dsample-point 0.750 \
    fd on
```

This configures 500 kbit/s nominal at 87.5% and 2 Mbit/s data at 75%. Higher data rates demand attention to transceiver delay, delay compensation, oscillators, bus/stub length, connectors, signal integrity, and EMC.

---

# 40. Classical-Only Nodes and CAN FD

A Classical-only node may interpret an FD frame as a Form Error and destroy it with an Error Frame. Before deployment, validate every controller, transceiver, firmware image, FD-tolerant/listen-only capability, gateway, and network mode—not only the sender and intended receiver.

---

# 41. What CAN Does Not Provide

The CAN data-link layer does not automatically provide end-to-end authentication, encryption, node discovery, freshness, application acknowledgement, long-message fragmentation policy, command authorization, units, byte order, scale, timeout, or device inventory. Upper-layer protocols/applications must provide them.

---

# 42. Common Upper-Layer Protocols

## CANopen

Provides Object Dictionary, PDO/SDO, node management, device profiles, and process data.

## J1939

Common in commercial vehicles and heavy equipment; defines 29-bit ID encoding, source addresses, PGNs, network management, and multi-packet transport.

## ISO-TP

Transports long messages using Single, First, Consecutive, and Flow Control frames; common for diagnostics and request/response protocols.

## Private Protocol

A custom protocol must define ID allocation, versioning, byte order, scaling, counters, CRC/MAC, timeout, fragmentation, compatibility, and errors. None of these protocols automatically guarantees complete authentication or encryption.

---

# 43. What Must a BMC Protocol Define?

| Item | Required Definition |
| --- | --- |
| CAN ID Ownership | Which node may transmit each ID? |
| Direction | BMC→Device or Device→BMC? |
| Period | How often? |
| Timeout | When is data considered invalid? |
| Byte Order | Little- or big-endian? |
| Scale / Offset | How is raw data converted? |
| Signedness | Signed or unsigned? |
| Sequence | How are duplicates, loss, and replay detected? |
| Version | How are upgrades kept compatible? |
| Authorization | Which control commands are allowed? |
| Degraded State | How does the system degrade? |
| Fail-Safe | What safe action follows invalid data? |

---

# 44. Byte Order and Scaling Example

For ID `0x123`:

```text
Bytes 0–1: Voltage, little-endian, 10 mV/unit
Bytes 2–3: Current, little-endian, 10 mA/unit
Byte 4:     Signed temperature, 1°C/unit
Byte 5:     Sequence counter
Byte 6:     Status
Byte 7:     Reserved
```

Payload `34 12 10 00 2A 07 01 00` yields:

```text
Voltage = 0x1234 = 4660 × 10 mV = 46.6 V
Current = 0x0010 = 16 × 10 mA = 160 mA
Temperature = 0x2A = 42°C
```

Applications must follow the protocol exactly rather than guessing order or units.

---

# 45. Freshness

A valid frame may contain stale data. Use sequence/rolling counters, timestamps, alive counters, timeout, or heartbeat.

For an expected 100 ms period:

```text
No frame for <300 ms --> Temporarily Stale
No frame for >1 s    --> Sensor Unavailable
No frame for >5 s    --> Enter Fail-Safe
```

Without freshness handling, old D-Bus values may falsely appear current.

---

# 46. Command Authorization

CAN cannot prove sender identity. Any attached node may try to transmit a power-off ID such as `0x200`.

Consider source policy, message authentication codes, rolling counters, challenge/response, physical isolation, gateway ACLs, rate limits, command-state validation, and event logging. An internal bus is not inherently trusted.

---

# 47. End-to-End Responsibilities

```text
Physical Layer
    +--> Wiring, Termination, Transceiver, EMC
CAN Data Link
    +--> Arbitration, CRC, ACK, Error Confinement
Transport / Application Protocol
    +--> Fragmentation, Addressing, Sequence, Timeout
Security
    +--> Authentication, Authorization, Anti-Replay
OpenBMC Policy
    +--> D-Bus, Redfish, Fail-Safe, Logging
```

Each layer solves different problems; link-layer CRC and ACK cannot replace application safety.

---

# 48. Recommended Debugging Sequence

1. Verify CAN_H/CAN_L wiring.
2. Measure termination with power off.
3. Verify ground reference.
4. Verify transceiver power and standby.
5. Verify controller clock.
6. Verify nominal bitrate and sample point.
7. For FD, verify data bitrate and sample point.
8. Inspect error counters.
9. Record Error Frames.
10. Confirm ACK.
11. Confirm standard/extended format.
12. Confirm socket filters.
13. Validate byte order, scale, and counters.
14. Validate timeout, degraded state, and fail-safe.
15. Enable automatic Bus-Off restart only after diagnosis.

---

# 49. Summary of Core Concepts

1. **High-speed CAN uses a differential twisted pair, linear bus, short stubs, and 120 Ω termination at both ends.**
2. **Controller and transceiver are separate; controller operation does not prove transceiver enablement.**
3. **Dominant overrides recessive, enabling non-destructive arbitration.**
4. **A node sending recessive but observing dominant loses arbitration without corrupting the winner.**
5. **CAN IDs describe meaning and priority, not ordinary destination addresses.**
6. **Classical CAN supports 11-bit and 29-bit IDs and up to 8 payload bytes.**
7. **Remote Frames request data but are uncommon and unsupported by CAN FD.**
8. **Bit stuffing maintains synchronization; CRC, Form, Bit, Stuff, and ACK mechanisms detect errors.**
9. **ACK is generated by controller hardware and does not require a listening application.**
10. **Missing ACK usually indicates no valid peer, timing/wiring errors, or transceiver/bus problems.**
11. **Bitrate is only part of timing; also validate sample point, SJW, clock, and delay.**
12. **Error counters move nodes through Active, Warning, Passive, and Bus-Off states.**
13. **`restart-ms` can recover Bus-Off but repeated restart can hide wiring or timing faults.**
14. **CAN FD supports 64 bytes and a faster data phase; DLC 9–15 does not equal byte count directly.**
15. **Classical-only nodes may disrupt FD frames, so validate the entire bus.**
16. **CAN does not provide authentication, encryption, freshness, discovery, or application acknowledgement.**
17. **OpenBMC protocols must define ID ownership, byte order, scaling, timeout, sequence, degraded state, and command authorization.**
