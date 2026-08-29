# OpenBMC use cases

## CPU thermal monitoring

The kernel converts PECI telemetry into hwmon channels. A sensor service reads
those channels and publishes D-Bus `Sensor.Value` objects. Thermal policy,
fan-control, Redfish, and event services consume those objects.

Do not build fan policy directly around a raw PECI margin without verifying its
meaning. Use labels, absolute converted units, threshold ownership, hysteresis,
staleness detection, and a fail-safe fan state when CPU telemetry disappears.

## DIMM temperature

`peci-dimmtemp` can expose DIMM thermal data supported by the CPU generation.
The platform must map channels to physical sockets/channels/DIMM inventory.
Missing data can mean an empty slot, unsupported topology, host power state, or
a PECI error; it is not automatically a zero-degree reading.

## PCIe inventory through PECI

OpenBMC `peci-pcie` uses CPU PECI access to obtain PCIe device information,
publishes it on D-Bus, and allows bmcweb to populate Redfish PCIe resources.
The service must coordinate with host power/BIOS timing and cache inventory for
states where live access is unavailable.

## Multi-socket server

For two or more packages, validate separately:

- target address and Linux device for each package,
- socket/package ID and inventory association,
- cputemp/dimmtemp labels and thresholds,
- behavior when one CPU is absent or held in reset,
- scan and timeout cost during power transitions,
- fan fail-safe if only one socket stops reporting.

## Reliability and security

- Rate-limit expensive telemetry and inventory scans.
- Serialize or coordinate generation-specific access.
- Treat raw PECI writes and PCI/MMIO access as privileged.
- Validate CPU family/model before selecting register maps.
- Keep last-known data distinguishable from fresh data.
- Test service and BMC restarts without resetting the host.
- Audit threshold changes and firmware-update interactions.

