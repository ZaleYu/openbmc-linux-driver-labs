# MCTP protocol fundamentals

MCTP transports management messages between components such as a BMC, host
processor, NIC, GPU, FPGA, storage device, retimer, or management controller.
MCTP is the transport envelope; upper-layer protocols define the operation.

## Addresses and roles

An Endpoint ID (EID) is an 8-bit logical address within one MCTP network. EID
0 is the null address and 255 is reserved for wildcard use in Linux sockets.
An endpoint can have a static EID or receive one dynamically from the bus owner.
Physical addresses are binding-specific: an I2C slave address, I3C PID/dynamic
address, PCIe requester ID, USB path, or another link identifier.

Do not treat an EID as a globally unique hardware identity. Discovery must
associate EID, physical address, medium, and inventory identity, and must update
that association after reset or hot-plug.

## Packet fields

The transport header carries destination EID, source EID, start/end flags,
packet sequence, tag owner, and a three-bit message tag. The first byte of a
reassembled message identifies its MCTP message type.

| Item | Purpose |
|---|---|
| SOM / EOM | First and last packet of a fragmented message |
| Packet sequence | Detects missing or reordered fragments |
| Message tag | Correlates a request and response |
| Tag owner | Distinguishes the requester-owned tag direction |
| Message type | Selects Control, PLDM, SPDM, NVMe-MI, or vendor handler |

The Linux AF_MCTP API uses the message type in the socket address for routing,
and the application must also put the type byte at `buffer[0]`. The kernel and
transport add packet headers and perform fragmentation as required.

## Common message types

- `0x00`: MCTP Control Protocol
- `0x01`: PLDM
- `0x04`: NVMe-MI
- `0x05`: SPDM
- `0x7e`: PCI vendor-defined
- `0x7f`: IANA vendor-defined

Check the current DMTF registry before assigning a type. Vendor-defined traffic
still needs a documented vendor identifier, framing, timeout, and compatibility
policy.

## Reliability model

MCTP itself does not turn every message into a reliable transaction. The upper
layer owns command instance IDs, response timeouts, retry limits, duplicate
handling, and recovery. A request retry must account for operations that are not
idempotent. The MCTP tag correlates traffic but is not a substitute for an
upper-layer transaction identifier.
