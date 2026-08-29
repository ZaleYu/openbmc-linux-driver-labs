# Linux MCTP for OpenBMC

This lab follows an MCTP message from a management endpoint, through a physical
binding and the Linux networking stack, to an OpenBMC application.

## Contents

- `docs/` — protocol, Linux architecture, bindings, tools, debugging, and
  OpenBMC use cases.
- `kernel-study/` — source map and a minimal kernel configuration fragment.
- `device-tree/` — MCTP-over-I2C, I3C, and muxed-I2C examples.
- `userspace/` — AF_MCTP requester and echo responder.
- `scripts/` — interface inventory, route diagnosis, and namespace lab setup.

## Data path

    PLDM/SPDM application -> AF_MCTP socket -> Linux MCTP core
        -> route + neighbour -> MCTP transport netdevice
        -> I2C / I3C / PCIe VDM / USB / serial -> remote endpoint

MCTP transports complete a different job from MCTP message types. The transport
moves MCTP packets; PLDM, SPDM, NVMe-MI and vendor protocols define the payload.

## Quick start

Build the tools:

    make -C userspace

On a system with a configured MCTP route, start the responder in one terminal:

    sudo ./userspace/mctp_echo 0x7e

Send a vendor-defined test request from another terminal:

    sudo ./userspace/mctp_request 8 0x7e 01 02 03 04

EIDs and routes are platform policy. Do not copy the example EIDs into a
production topology without coordinating the bus owner and endpoint inventory.

## References

- DMTF DSP0236, Management Component Transport Protocol Base Specification
- Linux `Documentation/networking/mctp.rst`
- Linux `net/mctp/` and `drivers/net/mctp/`
- OpenBMC `mctpd` and the OpenBMC kernel-MCTP design

