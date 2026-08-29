# Linux eSPI and KCS for OpenBMC

This lab separates the host-to-BMC transport from the host-management protocol:

- eSPI is a physical/link bus with Peripheral, Virtual Wire, OOB, and Flash
  channels.
- KCS is a byte-oriented IPMI system interface commonly presented through LPC
  or the eSPI Peripheral channel.

## Contents

- `docs/` — protocol, Linux architecture, Device Tree, tools, debugging, and
  OpenBMC use cases.
- `kernel-study/` — upstream KCS source map and kernel configuration.
- `device-tree/` — AST2600 KCS channel examples.
- `userspace/` — read-only inventory and an explicitly gated lab responder.
- `scripts/` — KCS inventory, stack diagnostics, and ownership checks.

## Host IPMI data path

    Host BIOS/OS IPMI driver
      -> KCS I/O data/status registers
      -> LPC or eSPI Peripheral channel
      -> ASPEED KCS BMC driver
      -> /dev/ipmi-kcsN
      -> kcsbridge
      -> phosphor-host-ipmid
      -> D-Bus services / IPMI response

Do not confuse this with network IPMI (`phosphor-net-ipmid`) or the BMC shell.
Only one userspace owner should open a KCS character device at a time.

## Quick start

    make -C userspace
    ./userspace/kcs_inventory
    ./scripts/debug_espi_kcs.sh

The lab responder intentionally requires `--lab` and must never run while the
production KCS bridge owns the device.

## References

- eSPI Base Specification and IPMI KCS system-interface specification
- Linux `drivers/char/ipmi/kcs_bmc*.c`
- Devicetree `ipmi/aspeed,ast2400-kcs-bmc.yaml`
- OpenBMC `kcsbridge` and `phosphor-host-ipmid`

