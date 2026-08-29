# Linux MCTP source map

## Core

| Path | Study focus |
|---|---|
| `net/mctp/af_mctp.c` | socket creation, bind, sendmsg, recvmsg |
| `net/mctp/route.c` | routing, output, fragmentation, reassembly |
| `net/mctp/device.c` | interface and local EID configuration |
| `net/mctp/neigh.c` | EID-to-link-address resolution |
| `net/mctp/test/` | KUnit expectations and edge cases |
| `include/uapi/linux/mctp.h` | stable userspace ABI |
| `include/net/mctp.h` | internal packet metadata and helpers |

## Transports

| Path | When it matters |
|---|---|
| `drivers/net/mctp/mctp-i2c.c` | SMBus/I2C framing, target RX, mux buses |
| `drivers/net/mctp/mctp-i3c.c` | PID addressing, DAA, IBI, MRL/MWL |
| `drivers/net/mctp/mctp-serial.c` | point-to-point lab/serial binding |
| `drivers/net/mctp/mctp-usb.c` | USB transport enumeration and transfer |

## Typical changes

- New board: DTS, kernel config, mctpd policy, Yocto package/service.
- New endpoint: discovery and higher-layer PLDM/SPDM support, not MCTP core.
- Controller erratum: I2C/I3C/PCIe controller or transport error path.
- New medium: implement the corresponding DMTF binding as a netdevice driver.
- Core bug: route, neighbour, tag, fragment/reassembly, namespace, or socket ABI.

Before modifying code, reproduce with a small AF_MCTP program, dump link/route
state, and identify whether the failure is physical, binding, core, discovery,
or upper-layer protocol.

