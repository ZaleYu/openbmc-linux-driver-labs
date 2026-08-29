# Linux CAN and SocketCAN for OpenBMC

This package is a practical introduction to Classical CAN and CAN FD on Linux,
with an emphasis on controller drivers, SocketCAN, Device Tree, diagnostics,
and specialized OpenBMC deployments such as rack controllers, power shelves,
robotics, and vehicle-derived management systems.

```text
CAN transceiver -> CAN controller -> Linux CAN netdev -> SocketCAN
                -> can-utils / service -> D-Bus policy or telemetry
```

Unlike I2C, CAN is multi-master and message-oriented. Nodes arbitrate by CAN ID;
there is no destination address in a data frame. Application protocols must
define meaning, addressing, freshness, and safety above the CAN data link.

## Repository map

| Path | Purpose |
|---|---|
| `docs/protocol.md` | Arbitration, frames, bit timing, errors, CAN FD |
| `docs/linux-can-architecture.md` | SocketCAN, netdev, controller drivers, sources |
| `docs/device-tree.md` | On-SoC, SPI CAN FD, and dual-channel examples |
| `docs/userspace-tools.md` | iproute2, can-utils, raw sockets, filters |
| `docs/debugging.md` | Physical-to-application diagnostic workflow |
| `docs/openbmc-use-cases.md` | Power shelf, chassis, gateway, and security cases |
| `client-driver/` | Educational fictional MMIO CAN controller driver |
| `device-tree/` | Board fragments for common CAN topologies |
| `userspace/` | Minimal SocketCAN sender and filtered receiver |
| `scripts/` | vcan lab, real-interface setup, and diagnostics |

## Quick start without CAN hardware

```sh
sudo ./scripts/setup_vcan.sh up
cc -O2 -Wall -Wextra -Werror -o can_receive userspace/can_receive.c
cc -O2 -Wall -Wextra -Werror -o can_send userspace/can_send.c
./can_receive vcan0 123
# In another terminal:
./can_send vcan0 123 11 22 33 44
sudo ./scripts/setup_vcan.sh down
```

For real hardware, review the bitrate with the board and network owner before:

```sh
sudo ./scripts/can_bus_up.sh can0 500000
candump -e -x can0
```

## Safety and scope

- A wrong bitrate or wiring can disrupt every node on the shared bus.
- Use 120-ohm termination at the two physical ends, not at every node.
- Do not transmit arbitrary frames on production power, motion, or safety buses.
- `vcan` validates SocketCAN software, not controller, transceiver, timing, or EMC.
- The demo MMIO controller and register map are fictional and educational. Never
  attach it to real hardware or use it as a production driver.
- Validate all DTS fragments against the exact controller binding, schematic,
  oscillator, transceiver, GPIO polarity, and kernel branch.

## Recommended study order

1. Learn wired-AND arbitration, frame fields, ACK, and error confinement.
2. Use `vcan0` to practice sockets, filters, timestamps, and replay safely.
3. Bring up a terminated two-node physical bus at a known bitrate.
4. Trace a frame through `can_send` to `ndo_start_xmit()` and the controller IRQ.
5. Inject disconnect, missing ACK, bus-off, and restart scenarios.
6. Define how CAN data becomes authenticated OpenBMC state or control.

## References

- Linux SocketCAN: <https://docs.kernel.org/networking/can.html>
- CAN controller driver API: `include/linux/can/dev.h`
- CAN network ABI: `include/uapi/linux/can.h`
- Device Tree bindings: `Documentation/devicetree/bindings/net/can/`
- Kernel drivers: `drivers/net/can/`
