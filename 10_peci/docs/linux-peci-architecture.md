# Linux PECI architecture

The upstream subsystem separates transport control, CPU command helpers, and
functional drivers.

    ASPEED/NPCM PECI hardware
      -> controller platform driver: struct peci_controller_ops
      -> PECI core and bus: struct peci_controller / peci_device
      -> peci-cpu generation driver
      -> auxiliary cputemp and dimmtemp devices
      -> hwmon drivers
      -> /sys/class/hwmon

## Controller layer

`drivers/peci/controller/peci-aspeed.c` maps registers, obtains IRQ/clock/reset,
sets timing, transfers request/response buffers, checks FCS/status, handles
completion and timeout, and calls `devm_peci_controller_add()`.

The ASPEED controller accepts buffers up to its hardware limit. The core owns
bus serialization while the controller implements one physical transfer.
Timeout recovery may reset and reinitialize a controller that did not return to
idle.

## Core and device discovery

`drivers/peci/core.c` registers the PECI bus and controllers, then scans CPU
addresses `0x30` to `0x37`. `drivers/peci/device.c` probes target identity and
creates a `peci_device` only when a supported responder is present.

Matching is based on CPU family/model information, not a Device Tree
`compatible` child for every package. `peci-cpu` exports helpers such as
`peci_temp_read()`, `peci_pcs_read()`, and PCI/MMIO accessors, then creates
auxiliary devices for supported functions.

## Functional layer

`drivers/hwmon/peci/cputemp.c` exposes package, DTS, thermal target, and selected
core temperatures. `dimmtemp.c` exposes DIMM temperatures where the CPU and
platform support them. These drivers cache data and contain per-generation
register locations and conversion rules.

For a new board, first change DTS and kernel configuration. Modify the PECI
controller only for SoC support or a hardware defect; modify CPU/hwmon code for
a new processor generation or verified command-layout difference.

