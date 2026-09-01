# Linux CAN and SocketCAN for OpenBMC

This lab covers the complete Linux software architecture for Classical CAN and CAN FD: physical/data-link layers, controller drivers, CAN network devices, SocketCAN, Device Tree, `can-utils`, virtual CAN, errors and Bus-Off recovery, and specialized OpenBMC applications.

```text
CAN Transceiver --> CAN Controller --> Linux Driver
    --> CAN Network Device (can0) --> SocketCAN
    --> can-utils / Userspace Service
    --> D-Bus Policy / Telemetry / Control
```

---

# 1. What Is CAN?

Controller Area Network is a multi-master, message-oriented bus used in automotive, industrial control, robotics, power/battery systems, rack and chassis management, and distributed embedded controllers.

All nodes share the bus and may transmit. CAN IDs control arbitration and express message meaning/priority. Hardware handles CRC, ACK, error detection, and error confinement. A Data Frame has no ordinary destination address.

---

# 2. CAN Versus I2C

| Item | CAN | I2C |
| --- | --- | --- |
| Model | Message-oriented | Address-oriented |
| Signals | Differential CAN_H/CAN_L | SDA/SCL |
| Control | Multi-master | Usually one controller; multi-master possible |
| Selection | CAN ID and protocol | Slave address |
| Arbitration | CAN ID | Address/data bits |
| Reception | Every node may observe | Addressed target responds |
| Errors | Counters, Error Frames, Bus-Off | ACK/NACK, timeout, recovery |
| Environment | Longer/noisier networks | Usually short on-board links |
| Linux API | Network device and SocketCAN | Adapter and `/dev/i2c-N` |

I2C addresses a target; CAN broadcasts an ID and lets filters/applications decide whether to process it.

---

# 3. CAN Has No Ordinary Destination Address

A frame contains CAN ID, length, payload, and control/CRC/ACK fields—not general Source and Destination fields.

| CAN ID | Example Meaning |
| --- | --- |
| `0x100` | Power Shelf Status |
| `0x110` | Fan Speed Telemetry |
| `0x120` | Battery Voltage |
| `0x123` | Demo Sensor Data |
| `0x200` | Rack Control Command |
| `0x700` | Diagnostic Message |

Source, destination, or node identity must be defined by the upper-layer protocol.

---

# 4. Broadcast Behavior

Every node may observe a frame. Processing depends on controller filters, SocketCAN filters, application protocol, node state, and security policy. An application not receiving a frame may simply mean that a filter discarded it; the frame may still have appeared on the wire.

---

# 5. Physical Layer

```text
Linux SoC --> CAN Controller --> CAN Transceiver
    --> CAN_H / CAN_L Twisted Pair
```

The controller builds frames, handles timing, arbitration, CRC, ACK, counters, buffers, and interrupts. The transceiver converts logic levels to differential bus signals and may provide standby, silent mode, and protection. They are separate components.

---

# 6. Dominant and Recessive

Simplified, dominant = 0 and recessive = 1. Dominant overrides recessive. A node that sends recessive but observes dominant knows another node is transmitting a higher-priority arbitration bit.

---

# 7. Arbitration

Simultaneous transmitters compare identifiers bit by bit. A node losing arbitration stops without corrupting the winner and retries later. Lower numeric IDs usually have higher priority for the same format, but standard/extended comparison also depends on the complete arbitration field, including IDE.

---

# 8. CAN ID and Priority

IDs identify message type and affect arbitration. Urgent traffic may use lower IDs, but excessive high-priority traffic can starve low-priority frames. Production design needs ID planning, message periods, worst-case bus-load and deadline analysis, and starvation evaluation.

---

# 9. Basic Frame Fields

```text
SOF --> Arbitration --> Control --> Data --> CRC --> ACK --> EOF
```

| Field | Purpose |
| --- | --- |
| SOF/EOF | Frame boundaries |
| Arbitration | ID, RTR, and related fields |
| Control | Format and length |
| Data | Payload |
| CRC | Error detection |
| ACK | Another controller received a valid frame |

---

# 10. Standard and Extended IDs

| Format | Length |
| --- | --- |
| Standard | 11 bits (`0x000`–`0x7FF`) |
| Extended | 29 bits |

SocketCAN uses flags to distinguish formats. Do not mix 11- and 29-bit IDs without a defined protocol.

---

# 11. Classical CAN

Classical CAN carries up to 8 bytes per Data Frame, suitable for small sensor values, commands, status, heartbeat, faults, and short protocol messages. Longer data requires fragmentation, ISO-TP, or CAN FD.

---

# 12. CAN FD

CAN with Flexible Data-rate supports payloads up to 64 bytes, an optional faster data phase, improved CRC, BRS, and ESI.

- **BRS:** switches from nominal arbitration rate to a faster data rate.
- **ESI:** indicates the transmitter’s error state.
- Valid payload sizes above 8 follow FD DLC mapping: 12, 16, 20, 24, 32, 48, or 64 bytes.

---

# 13. CAN FD Is More Than a 64-Byte Payload

Controller hardware/driver, transceiver, SocketCAN, every relevant node, nominal/data timing, and board signal integrity must support FD. A Classical-only node may treat FD frames as errors and disrupt the bus.

---

# 14. Meaning of ACK

ACK means at least one other controller received a frame with valid format and CRC. It does not prove application processing, command acceptance, D-Bus update, completed action, persistent storage, or security validation.

End-to-end confirmation requires an application response, sequence number, timeout, and retry policy.

---

# 15. Error Detection

CAN detects Bit, Stuff, CRC, Form, and ACK errors. A node may transmit an Error Frame to invalidate the frame, and the sender may retry. These mechanisms do not provide authentication, encryption, freshness, or semantic validation.

---

# 16. Error Counters and Bus-Off

Controllers maintain TEC and REC and move through Error Active, Error Passive, and Bus-Off. Bus-Off isolates a severely faulty transmitter.

Common causes include missing peers/ACK, incorrect bitrate, reversed wiring, bad termination, disabled transceiver, shorted bus, wrong controller clock/timing, or mismatched FD data rate. Bus-Off is a protection mechanism, not simply a driver failure.

---

# 17. Bitrate and Bit Timing

Common rates include 125, 250, 500 kbit/s and 1 Mbit/s. Actual timing also depends on controller clock, prescaler, TSEG1/2, SJW, and sample point. An incorrect oscillator or Device Tree clock makes the real bus timing wrong even if userspace requests `500000`.

CAN FD additionally requires data-phase timing.

---

# 18. Termination

Use one 120 Ω terminator at each physical end, not at every node:

```text
120 Ω                                      120 Ω
  |                                          |
Node A ---- Node B ---- Node C ---- Node D
```

With power off, CAN_H-to-CAN_L often measures near 60 Ω because two 120 Ω resistors are in parallel. Other circuitry may affect the measurement.

---

# 19. Bus Topology

Use a bus/daisy-chain topology with short stubs. Long stubs cause reflections, ringing, sampling instability, and intermittent CRC errors. Higher bitrates generally permit shorter buses and stubs.

---

# 20. SocketCAN

SocketCAN is Linux’s CAN networking framework. It represents controllers as `can0`, `can1`, etc., and provides socket APIs instead of direct register access.

```text
Userspace --> PF_CAN Socket --> SocketCAN Core
    --> net_device --> CAN Driver --> Hardware
```

---

# 21. SocketCAN Protocol Family

```c
socket(PF_CAN, SOCK_RAW, CAN_RAW);
```

`AF_CAN` is equivalent for this purpose.

| Protocol | Purpose |
| --- | --- |
| `CAN_RAW` | Raw Classical/FD frames |
| `CAN_BCM` | Broadcast manager and periodic monitoring |
| `CAN_ISOTP` | ISO-TP transport |
| `CAN_J1939` | SAE J1939 |

The minimal examples use `CAN_RAW`.

---

# 22. Linux CAN Network Device

```sh
ip link show
ip -details -statistics link show can0
```

Detailed output includes state, bitrate, sample point, restart time, error state/counters, packet counts, and drops. CAN uses `net_device` but does not inherently use MAC addresses, IP, ARP, or Ethernet frames; no IP address is needed unless an upper layer defines one.

---

# 23. CAN Controller Driver

Drivers under `drivers/net/can/` configure timing/mode, enable hardware, manage TX mailboxes and RX, handle interrupts and Error Frames, update statistics, perform Bus-Off handling, and register network devices.

Helpers are commonly in `include/linux/can/dev.h`; userspace ABI is in `include/uapi/linux/can.h`.

---

# 24. TX Data Path

```text
can_send --> write()/send() --> SocketCAN --> Network Queue
    --> Driver ndo_start_xmit() --> TX Mailbox
    --> Arbitration --> Transceiver --> CAN_H/CAN_L
```

Hardware normally retries after arbitration loss. A completion interrupt lets the driver update TX accounting and echo handling.

---

# 25. RX Data Path

```text
CAN_H/CAN_L --> Transceiver --> RX FIFO --> IRQ
    --> Driver Builds Socket Buffer --> SocketCAN
    --> CAN_RAW Filter --> can_receive
```

A controller may receive a frame successfully even when a socket filter prevents delivery to an application.

---

# 26. SocketCAN Filters

```c
struct can_filter filter;
filter.can_id = 0x123;
filter.can_mask = CAN_SFF_MASK;
```

Matching concept:

```text
Received ID & Mask == Filter ID & Mask
```

Filters may select one ID, ranges/patterns, standard/extended frames, Error Frames, inverse matches, or multiple entries. Kernel-side filtering reduces userspace load.

---

# 27. Virtual CAN

`vcan` is a virtual CAN network device requiring no controller, transceiver, wiring, or termination.

```text
can_send --> SocketCAN --> vcan0 --> SocketCAN --> can_receive
```

It is useful for socket, binding, frame-format, ID-filter, FD-payload, timestamp, protocol, record/replay, unit-test, and CI testing.

---

# 28. What `vcan` Cannot Test

It cannot validate the hardware driver, `ndo_start_xmit()`, interrupts, DMA, FIFO, transceiver, bit timing, arbitration timing, ACK, error counters, Bus-Off, termination, signal integrity, EMC, or wiring.

`vcan` success proves only that the SocketCAN application logic is basically correct.

---

# 29. Repository Structure

| Path | Contents |
| --- | --- |
| `docs/protocol.md` | Arbitration, frames, timing, errors, CAN FD |
| `docs/linux-can-architecture.md` | SocketCAN, netdevice, driver, source map |
| `docs/device-tree.md` | On-SoC, SPI CAN FD, dual-channel examples |
| `docs/userspace-tools.md` | iproute2, can-utils, raw sockets, filters |
| `docs/debugging.md` | Physical-to-application debugging |
| `docs/openbmc-use-cases.md` | Power shelf, chassis, gateway, security |
| `client-driver/` | Educational fictional MMIO driver |
| `device-tree/` | Board DTS fragments |
| `userspace/` | Minimal sender and filtered receiver |
| `scripts/` | vcan lab, interface setup, diagnostics |

---

# 30. Quick Start with `vcan0`

```sh
sudo ./scripts/setup_vcan.sh up
ip -details link show vcan0

cc -O2 -Wall -Wextra -Werror \
   -o can_receive userspace/can_receive.c
cc -O2 -Wall -Wextra -Werror \
   -o can_send userspace/can_send.c

./can_receive vcan0 123
./can_send vcan0 123 11 22 33 44

sudo ./scripts/setup_vcan.sh down
```

This sends ID `0x123` with four bytes and should display something like `vcan0 123 [4] 11 22 33 44`. Exact input syntax depends on the tool implementation.

---

# 31. Using `can-utils`

| Tool | Purpose |
| --- | --- |
| `candump` | Display frames |
| `cansend` | Send one frame |
| `cangen` | Generate test traffic |
| `canplayer` | Replay captured traffic |
| `cansniffer` | Observe changing data |
| `canbusload` | Estimate bus load |
| `isotpsend`/`isotprecv` | ISO-TP transport |

```sh
candump vcan0
cansend vcan0 123#11223344
```

---

# 32. Real CAN Interface Bring-Up

Confirm the network bitrate with its owner first:

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
ip -details -statistics link show can0
candump -e -x can0
```

Do not assume 500 kbit/s without knowing the bus configuration.

---

# 33. CAN FD Interface Setup

```sh
sudo ip link set can0 down
sudo ip link set can0 type can \
    bitrate 500000 \
    dbitrate 2000000 \
    fd on
sudo ip link set can0 up
```

Before use, verify controller, driver, transceiver, peer-node FD support, sample points, oscillator, schematic, and network-owner approval.

---

# 34. Bus-Off Recovery

```sh
sudo ip link set can0 type can restart-ms 100
sudo ip link set can0 type can restart
```

Automatic restart does not replace root-cause analysis. Wrong wiring or timing can create an endless restart/error/Bus-Off loop. Production policy should define restart limits, backoff, logging, disable/degraded modes, fail-safe behavior, and human intervention.

---

# 35. Device Tree: On-SoC Controller

```dts
&can0 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&can0_pins>;
};
```

A conceptual SoC node contains `compatible`, MMIO, interrupt, clocks, resets, and disabled status. Verify all of these, plus pinmux, transceiver, FD capability, and controller-specific properties against the binding.

---

# 36. Device Tree: CAN Transceiver

```dts
can_transceiver0: can-phy {
    compatible = "can-transceiver";
    standby-gpios = <&gpio0 10 GPIO_ACTIVE_HIGH>;
    max-bitrate = <5000000>;
};

&can0 {
    status = "okay";
    phys = <&can_transceiver0>;
    phy-names = "can-phy";
};
```

Actual property names follow the kernel binding. Incorrect standby polarity can produce a valid `can0` with no CAN_H/CAN_L waveform.

---

# 37. Device Tree: SPI CAN Controller

```text
SoC SPI Controller --> SPI CAN Controller
    --> CAN Transceiver --> CAN_H/CAN_L
```

```dts
&spi0 {
    status = "okay";
    can@0 {
        compatible = "vendor,spi-can-controller";
        reg = <0>;
        spi-max-frequency = <10000000>;
        interrupts = <...>;
        clocks = <&can_oscillator>;
        status = "okay";
    };
};
```

Verify SPI mode/frequency/CS, IRQ, oscillator, CAN timing, reset, transceiver, and FD support. An incorrect oscillator value produces incorrect CAN timing.

---

# 38. Dual-Channel CAN

Two controllers may serve different buses, bitrates, transceivers, termination, policies, services, and failure domains. Even identical controller types may require different clocks, pinmux, IRQs, standby GPIOs, and bitrate settings.

---

# 39. OpenBMC Applications

CAN is not mandatory in every OpenBMC server but may support rack controllers, power shelves, battery cabinets, robotics/industrial chassis, vehicle-derived platforms, liquid-cooling controllers, PDUs, and gateways.

```text
Power Shelf CAN Node --> can0 --> OpenBMC CAN Service
    --> Decode Voltage/Current/Fault
    --> D-Bus Sensor/Inventory --> Redfish
```

---

# 40. Converting CAN to D-Bus

SocketCAN provides only `CAN ID + Payload`. An OpenBMC service must decode byte order and scaling, validate ranges/counters/freshness, convert engineering units, and then create a D-Bus sensor.

Never convert unvalidated raw payloads directly into trusted sensor values or control actions.

---

# 41. CAN Has No Built-In Authentication

Classical CAN and CAN FD normally lack sender authentication, encryption, anti-replay, freshness, access control, and end-to-end acknowledgement. Any connected node may attempt to spoof an ID.

For power, fan, motion, battery, safety, or firmware commands, consider authentication, rolling/sequence counters, timestamps, timeouts, plausibility checks, source association, rate limits, authorization, and fail-safe behavior.

---

# 42. Freshness and Heartbeat

Because CAN is broadcast and connectionless, applications must detect stale data. For a 100 ms power-shelf message, a policy might mark it Stale after 300 ms and Unavailable/Fault after 1 s.

A rolling counter can detect repeated, skipped, or replayed frames.

---

# 43. Gateway Security Boundary

An OpenBMC gateway between internal/external CAN and Ethernet/Redfish must define which IDs may be exported, which remote commands may become CAN commands, payload ranges, authentication, rate limits, bus load, logging, fail-safe behavior, and isolation.

Do not expose a raw CAN bus to a remote network without policy enforcement.

---

# 44. Layered Debugging Procedure

1. **Physical wiring:** polarity, ground, two terminators, stubs, connector, cable.
2. **Transceiver:** power, enable/standby, silent mode, voltage, differential waveform.
3. **Controller:** probe, clock, reset, pinmux, IRQ, FD capability.
4. **Timing:** nominal rate/sample point, oscillator, FD data rate, BRS.
5. **Interface:** inspect state, counters, errors, drops with `ip -details -statistics`.
6. **SocketCAN:** interface binding, ID format/flags, filters, FD option, length.
7. **Protocol:** byte order, scale, offset, signedness, counters, CRC, timeout, period, role, permissions.
8. **OpenBMC:** service, D-Bus, availability, inventory, Redfish, fail-safe.

---

# 45. Common Problems

- **No `can0`:** disabled driver/node, clock/reset failure, SPI-controller probe failure, IRQ/oscillator error.
- **`can0` but no waveform:** interface down, no TX, standby transceiver, wrong pinmux/wiring.
- **Rapid Bus-Off:** no ACK peer, wrong bitrate, reversed wires, termination, standby, wrong clock.
- **Receiver sees nothing:** filter or ID-format mismatch, wrong interface/bitrate, Bus-Off sender, FD disabled.
- **`vcan` works but hardware fails:** investigate Device Tree, driver, clock, IRQ, transceiver, timing, wiring, termination.

---

# 46. Safety and Scope

- Wrong timing or wiring can disrupt a shared bus.
- Terminate only the two physical ends.
- Do not send arbitrary frames, use `cangen`, or replay unknown captures on production power/motion/safety buses.
- `vcan` validates software only.
- Demo MMIO register maps are fictional.
- Validate DTS against binding, schematic, oscillator, and transceiver.
- Link-layer ACK does not prove application acceptance.
- Safety-critical data needs authentication, freshness, and fail-safe handling.

---

# 47. Recommended Learning Sequence

1. Learn CAN_H/CAN_L and termination.
2. Understand dominant/recessive and arbitration.
3. Study frame, ACK, CRC, Error Frames, counters, and Bus-Off.
4. Compare Classical CAN and CAN FD.
5. Practice SocketCAN with `vcan0` and CAN_RAW sender/receiver.
6. Practice filters and timestamps.
7. Build a correctly terminated two-node physical bus.
8. Validate bitrate/ACK with low traffic.
9. Trace `write()` to `ndo_start_xmit()` and IRQ to receiver.
10. Test missing ACK, disconnect, and Bus-Off.
11. Define payload, freshness, timeout, and D-Bus mapping.
12. Evaluate production security and fail-safe behavior.

---

# 48. Summary of Core Concepts

1. **CAN is a multi-master, message-oriented bus—not address-oriented I2C.**
2. **A Data Frame has no ordinary destination address; its ID expresses type and priority.**
3. **Dominant overrides recessive, enabling non-destructive arbitration.**
4. **Lower IDs generally have higher priority, with additional standard/extended arbitration details.**
5. **Classical CAN carries 8 bytes; CAN FD carries up to 64 and may use a faster data phase.**
6. **ACK proves physical reception by another controller, not application execution.**
7. **Repeated errors move nodes from Error Active to Passive and potentially Bus-Off.**
8. **Linux exposes controllers as `can0` network devices and applications use SocketCAN.**
9. **`vcan0` tests software and CI, not hardware timing, transceivers, or EMC.**
10. **Real buses require common timing and two 120 Ω end terminators.**
11. **OpenBMC must convert raw frames into range-, freshness-, identity-, and security-validated D-Bus state.**
12. **CAN normally lacks authentication, encryption, and anti-replay; upper layers must provide them.**
