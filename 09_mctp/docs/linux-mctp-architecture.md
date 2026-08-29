# Linux MCTP architecture

The Linux MCTP core lives in `net/mctp/`. A physical binding driver in
`drivers/net/mctp/` exposes a networking interface with link type
`ARPHRD_MCTP`. Applications use `socket(AF_MCTP, SOCK_DGRAM, 0)`.

## Core objects

    transport hardware
      -> binding driver
      -> MCTP netdevice / interface
      -> neighbour table: EID to physical address
      -> route table: EID range to interface
      -> AF_MCTP datagram socket
      -> PLDM / SPDM / control application

An interface is one physical transport instance. A network is an EID address
space and may contain several interfaces. Routes select an interface for an EID
or range; neighbours resolve a destination EID to a binding-specific physical
address on that interface.

## Socket addressing

`struct sockaddr_mctp` selects:

- network number (`MCTP_NET_ANY` may be used where appropriate),
- local or peer EID,
- message type,
- message tag and `MCTP_TAG_OWNER`.

A requester normally sends with `MCTP_TAG_OWNER`; the kernel allocates a free
tag. The response arrives with the same tag value and owner bit cleared. A
responder binds an EID/type, receives a request, and replies to the returned
peer address without inventing a new tag. The datagram buffer includes the MCTP
message-type byte as its first byte even though the address also specifies the
type used for socket routing.

`MCTP_OPT_ADDR_EXT` enables `sockaddr_mctp_ext`, exposing interface index and
physical link-layer address. This is mainly for control-plane software such as
endpoint discovery; ordinary PLDM/SPDM applications should route by EID.

## Source locations

- `net/mctp/af_mctp.c`: socket family and bind/send/receive path
- `net/mctp/route.c`: routes, neighbours, fragmentation, reassembly
- `net/mctp/device.c`: interface and local-address management
- `net/mctp/neigh.c`: neighbour operations
- `drivers/net/mctp/mctp-i2c.c`: SMBus/I2C binding
- `drivers/net/mctp/mctp-i3c.c`: I3C binding
- `drivers/net/mctp/mctp-serial.c`: serial binding

For a new board, first configure the existing transport, DTS, routes, and
userspace discovery. Modify a transport driver only for new hardware support,
binding behavior, controller limitations, or a confirmed kernel defect.
