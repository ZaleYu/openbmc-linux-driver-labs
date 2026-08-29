# Linux PECI for OpenBMC

This lab follows CPU telemetry from the PECI wire, through the Linux PECI bus
and hwmon drivers, to OpenBMC D-Bus and Redfish consumers.

## Contents

- `docs/` — protocol, Linux architecture, Device Tree, tools, debugging, and
  OpenBMC use cases.
- `kernel-study/` — current upstream source map and kernel config fragment.
- `device-tree/` — AST2500/AST2600 enablement and timing examples.
- `userspace/` — read-only PECI device inventory and hwmon dump programs.
- `scripts/` — inventory, diagnostics, and temperature snapshot tools.

## Data path

    Intel CPU package -> PECI wire -> BMC PECI controller
        -> Linux controller driver -> PECI core -> peci-cpu
        -> peci-cputemp / peci-dimmtemp -> hwmon sysfs
        -> OpenBMC sensor service -> D-Bus -> thermal policy / Redfish

PECI is processor- and generation-specific. Do not copy raw package/register
commands between CPU generations. Prefer upstream helpers and hwmon attributes
whose drivers already encode command format, completion codes, fixed-point
conversion, caching, and generation quirks.

## Quick start

    make -C userspace
    ./userspace/peci_inventory
    ./userspace/peci_hwmon_dump
    sudo ./scripts/debug_peci.sh

## References

- Linux `drivers/peci/` and `include/linux/peci.h`
- Linux `drivers/hwmon/peci/`
- Devicetree `peci-controller.yaml` and `peci-aspeed.yaml`
- OpenBMC `dbus-sensors`, `entity-manager`, and `peci-pcie`

