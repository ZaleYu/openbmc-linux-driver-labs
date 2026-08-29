# PECI protocol fundamentals

Platform Environment Control Interface (PECI) is a bidirectional, single-wire
management interface used to communicate with supported Intel processors and
chipset components. The BMC controller normally acts as the originator and the
CPU package as the target.

## Electrical and transaction model

PECI is not I2C even though both can carry management traffic. PECI has its own
signalling, timing negotiation, target addressing, command set, framing, FCS,
and completion behavior. Confirm the board voltage domain, pull-up, routing,
series components, CPU power state, and controller pin configuration.

A transaction conceptually includes:

1. target address and negotiated timing,
2. write and read lengths,
3. command and request bytes,
4. optional Assured Write FCS,
5. response bytes, completion code, and FCS where defined.

The controller serializes transfers. A timeout can mean an unpowered target,
electrical fault, wrong timing, bus contention, unsupported command, or a CPU
state in which that command is unavailable.

## CPU target addresses

Linux scans the standard CPU PECI target range `0x30` through `0x37`. The
address identifies a package on the PECI bus; it is not the same as a CPU core,
DIMM slot, PCI bus number, or OpenBMC inventory path.

## Common commands

| Command family | Typical purpose |
|---|---|
| Ping / GetDIB | Presence and device information |
| GetTemp | Package thermal margin information |
| RdPkgConfig | Package configuration and telemetry |
| WrPkgConfig | Controlled package configuration writes |
| RdPCIConfigLocal | CPU-local PCI configuration reads |
| RdEndpointConfig | Endpoint PCI/MMIO access on supported generations |

Command availability, index/parameter layout, fixed-point formats, and
completion codes vary with CPU model and PECI revision. Treat Intel platform
documentation and the upstream generation tables as authoritative.

## Temperature semantics

Raw PECI temperature data may represent a signed margin relative to a thermal
target rather than a direct Celsius value. Linux `peci-cputemp` converts the
generation-specific format and exports standard hwmon millidegree-Celsius
attributes. Use the label and threshold attributes together; do not assume
every `tempN_input` represents the same package/core measurement.

