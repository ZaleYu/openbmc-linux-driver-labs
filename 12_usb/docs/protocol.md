# USB protocol essentials

## Roles and topology

USB is host-controlled. One host schedules traffic to addressed devices through
a tiered-star topology of hubs. A normal link is not peer-to-peer: devices do
not initiate arbitrary transfers. USB On-The-Go and Type-C role switching add
role negotiation, but the active data roles remain host and device.

USB 2.0 speeds are low (1.5 Mb/s), full (12 Mb/s), and high (480 Mb/s). USB 3.x
adds separate SuperSpeed signaling and descriptors. Cable, PHY, hub, controller,
and device must all support the negotiated speed.

## Enumeration

After attach and electrical detection, the host resets the port and talks to
endpoint zero at the default address. It reads descriptors, assigns a unique
address, selects a configuration, and lets interface drivers bind.

Important descriptors are:

| Descriptor | Describes |
|---|---|
| Device | USB version, EP0 size, VID/PID, device class, string indexes |
| Configuration | Power attributes and a collection of interfaces |
| Interface | A function and its class/subclass/protocol |
| Endpoint | Direction, transfer type, maximum packet, interval |
| String | Human-readable manufacturer, product, and serial text |

A composite device has multiple interfaces, often handled by different Linux
drivers. Device-level VID/PID matching is convenient for a lab but production
drivers should usually constrain interface class or interface number too.

## Endpoints and transfer types

- Control: setup/data/status transactions; endpoint zero is mandatory.
- Bulk: reliable, high-throughput traffic with no bandwidth guarantee.
- Interrupt: bounded polling interval for small latency-sensitive reports.
- Isochronous: reserved bandwidth and timing, without retry guarantees.

Endpoint direction is named from the host perspective: IN means device-to-host,
OUT means host-to-device. Endpoint addresses combine direction and endpoint
number. Transfers may contain multiple packets; a short packet can terminate a
bulk transfer and is not automatically an error.

## Control requests

Every setup packet contains request type/direction, request, value, index, and
length. Standard requests retrieve descriptors and set address/configuration.
Class and vendor requests define function-specific behavior. Validate lengths,
indexes, states, and authorization before acting on gadget-side requests.

## Power and reset

Descriptors advertise bus-powered/self-powered behavior and maximum current,
but hardware policy, Type-C/PD, and board power switches determine reality.
Suspend, resume, remote wakeup, disconnect, port reset, and host-controller
reset are normal lifecycle events; drivers must tolerate them.

## Common failure signatures

| Symptom | Likely layer |
|---|---|
| No connect event | VBUS, cable, connector mux, PHY, UDC/HCD, role |
| Device descriptor read error | Signal integrity, power, EP0 firmware |
| Enumerates but no driver | IDs, interface class, configuration, module |
| `-EPIPE` | Endpoint stalled or unsupported request |
| `-EPROTO` / `-EILSEQ` | Protocol/signal-integrity issue |
| `-ETIMEDOUT` | Lost device, wedged endpoint/controller, bad path |
| Repeated reset/disconnect | Power droop, cable, PHY, device crash |

Use a protocol analyzer when software logs cannot distinguish malformed packets
from electrical failures. A logic analyzer intended for I2C/SPI is generally
not an adequate USB protocol instrument.
