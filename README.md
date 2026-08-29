# OpenBMC Linux Driver Development and Debugging Labs

`openbmc-linux-driver-labs` is a practical study repository for Linux kernel
drivers and subsystems commonly encountered while developing OpenBMC platforms.
It connects protocol fundamentals, board description, kernel architecture,
userspace interfaces, debugging, and OpenBMC integration instead of treating a
driver as an isolated C file.

```text
Hardware / protocol
        -> Device Tree and board wiring
        -> Linux controller, bus, or subsystem driver
        -> Linux client or function driver
        -> sysfs, hwmon, IIO, netdev, tty, or character device
        -> OpenBMC service and D-Bus
        -> policy, Redfish, IPMI, telemetry, or host management
```

The repository contains fourteen independent labs. Each lab can be studied on
its own, but the numbered order provides a progressive path from GPIO and
common serial buses to OpenBMC-specific management transports.

> [!IMPORTANT]
> This is an educational project and is not an official OpenBMC repository.
> Several devices, register maps, compatibles, and controller models are
> intentionally fictional or simplified. Do not copy an example directly into
> a production image without checking the hardware datasheet, schematic,
> upstream binding, kernel branch, security requirements, and failure policy.

## What this repository teaches

Every lab approaches a subsystem through the same engineering questions:

1. What happens electrically and on the wire?
2. How is the hardware represented in Device Tree?
3. Which Linux core objects and APIs carry the data?
4. Where are the upstream controller and subsystem sources?
5. When is a new driver required, and when should an existing driver be reused?
6. How can userspace inspect and test the interface safely?
7. How does the result reach an OpenBMC service, D-Bus, Redfish, or host path?
8. How should failures be reproduced, diagnosed, recovered, and reported?

## Lab index

### Foundations and serial buses

| Lab | Focus | Key subjects and exercises |
|---|---|---|
| [01_gpio_irq](01_gpio_irq/) | GPIO and interrupts | Electrical polarity, GPIO descriptor API, GPIO character-device ABI v2, threaded IRQ, debounce, `gpio-keys`, GPIO hogs, and OpenBMC presence/fault signals |
| [02_i2c_smbus](02_i2c_smbus/) | I2C and SMBus | Transactions, ACK/NACK, SMBus differences, `i2c-tools`, Device Tree clients, regmap, hwmon sensor driver, shared-bus failures, and recovery |
| [03_i2c_mux](03_i2c_mux/) | I2C mux topology | Parent and logical adapters, channel selection, mux locking, nested muxes, stable topology discovery, and OpenBMC PSU/sensor/FRU fan-out |
| [04_i3c](04_i3c/) | I3C | Dynamic Address Assignment, CCC, PID, IBI, Hot-Join, mixed I2C/I3C buses, target drivers, and kernel API compatibility |
| [05_spi](05_spi/) | SPI | Controller/device architecture, modes, chip select, full-duplex transfers, SPI-NOR, spidev limitations, and a demo SPI sensor |

### Sensors, control, and console paths

| Lab | Focus | Key subjects and exercises |
|---|---|---|
| [06_hwmon_iio](06_hwmon_iio/) | hwmon and IIO | Measurement fundamentals, raw ADC data, scaling, voltage dividers, buffered vs direct data, `iio-hwmon`, and OpenBMC sensor publication |
| [07_pwm_tach_fan](07_pwm_tach_fan/) | PWM, tachometer, and fan control | Duty cycle, tach pulses, RPM conversion, hwmon, thermal cooling devices, multi-fan topology, fail-safe behavior, and fan debugging |
| [08_uart_vuart](08_uart_vuart/) | UART and VUART | UART framing, tty/serial/serdev architecture, boot console ownership, ASPEED VUART, host-console logging, and MCU communication |

### OpenBMC management interfaces

| Lab | Focus | Key subjects and exercises |
|---|---|---|
| [09_mctp](09_mctp/) | MCTP | Endpoint IDs, routing, message types, Linux MCTP sockets, I2C/I3C transport bindings, endpoint discovery, and management-controller use cases |
| [10_peci](10_peci/) | PECI | CPU-side telemetry, controller and client architecture, AST2500/AST2600 integration, hwmon exposure, temperature inventory, and timeout debugging |
| [11_espi_kcs](11_espi_kcs/) | eSPI and KCS | Host/BMC channels, KCS state machine, IPMI request path, ASPEED Device Tree, channel ownership, lab responder, and host-reset failures |
| [12_usb](12_usb/) | USB host and gadget | Enumeration, descriptors, endpoints, HCD/UDC architecture, AST2600 virtual hub, configfs loopback, Virtual Media, and remote KVM HID |

### Specialized transports

| Lab | Focus | Key subjects and exercises |
|---|---|---|
| [13_can](13_can/) | CAN and SocketCAN | Arbitration, Classical CAN/CAN FD, bit timing, error confinement, bus-off, SocketCAN, `vcan`, an educational controller driver, and specialized chassis networks |
| [14_lin](14_lin/) | LIN | Single-master schedules, break/sync/PID, classic/enhanced checksum, UART/serdev integration, hwmon publication, and specialized actuator networks |

CAN and LIN are not universal OpenBMC interfaces. They are included to show how
special-purpose board networks can be integrated without confusing application
protocols with the underlying Linux controller driver.

## Common lab layout

Most labs use this structure:

```text
NN_lab_name/
├── README.md
├── docs/
├── client-driver/       # or kernel-study/
├── device-tree/
├── userspace/
└── scripts/
```

| Path | Purpose |
|---|---|
| `README.md` | Scope, data path, quick start, safety notes, and study order |
| `docs/` | Protocol, Linux architecture, Device Tree, userspace tools, debugging, and OpenBMC use cases |
| `client-driver/` | Educational out-of-tree kernel driver, Kconfig, Makefile, and driver-specific notes |
| `kernel-study/` | Source maps and kernel configuration for subsystems where reusing and studying upstream code is more appropriate than inventing another client driver |
| `device-tree/` | Board fragments and topology examples; these are not complete drop-in board files |
| `userspace/` | Small C programs that inspect, exercise, or validate the Linux-facing ABI |
| `scripts/` | Inventory, read-mostly diagnostics, controlled setup, and fault-isolation helpers |

The MCTP, PECI, and eSPI/KCS labs intentionally use `kernel-study/`. Their value
is learning the existing upstream subsystem and data path, not creating a fake
kernel driver merely to make every directory look identical.

## Recommended learning paths

### Complete path

This order builds the concepts progressively while keeping related sensor and
management topics together:

```text
01 -> 02 -> 03 -> 05 -> 06 -> 07 -> 08
   -> 04 -> 09 -> 10 -> 11 -> 12 -> 13 -> 14
```

### OpenBMC interview and platform-development path

Focus first on the interfaces most likely to appear in a server BMC platform:

```text
01 -> 02 -> 03 -> 05 -> 06 -> 07 -> 08
   -> 09 -> 10 -> 11 -> 12
```

Then use I3C, CAN, and LIN as advanced or platform-specific extensions.

### Linux driver implementation path

For practice with matching, probe/remove, register access, IRQ, synchronization,
subsystem registration, and userspace ABI boundaries:

```text
01 -> 02 -> 05 -> 06 -> 07 -> 08 -> 12 -> 13
```

## How to study one lab

Use the following workflow instead of immediately loading the kernel module:

1. Read the lab `README.md` to understand its scope and fictional hardware.
2. Read the protocol or fundamentals document and identify electrical risks.
3. Read the Linux architecture document and draw the data path in your own words.
4. Find the corresponding upstream sources in the paths listed by the lab.
5. Compare each Device Tree node with the driver's match table and property reads.
6. Build the userspace programs with strict warnings.
7. Run inventory or read-only scripts before scripts that change hardware state.
8. Build the kernel module with the exact target kernel headers.
9. Integrate the DTS fragment into the real board tree and validate its binding.
10. Exercise normal traffic, inject failures, and prove recovery.
11. Trace how the Linux-facing value or endpoint reaches the OpenBMC service.

Keeping a short lab record is useful. Capture the target kernel commit, board
revision, schematic page, DT node, module configuration, exact commands, logs,
waveforms, expected result, observed result, and recovery procedure.

## Prerequisites

The exact packages depend on the selected lab. A typical development host needs:

- GCC or Clang, GNU Make, Git, and standard C development headers
- the exact target kernel source/build tree or matching kernel headers
- Device Tree Compiler and the kernel DT schema environment
- `i2c-tools`, `libgpiod` tools, `iproute2`, `can-utils`, and `usbutils` as needed
- an OpenBMC/Yocto development environment for image and service integration
- serial console access and permission to inspect kernel logs and sysfs
- protocol-appropriate hardware such as a logic analyzer, oscilloscope, USB
  analyzer, CAN interface, or LIN transceiver

Do not assume that every lab can run on one board. Raspberry Pi, AST2500,
AST2600, vendor BMC kernels, and upstream kernels expose different controllers,
bindings, configuration symbols, and APIs.

## Building userspace examples

Most userspace examples intentionally depend only on Linux UAPI and libc. Build
the selected source directly, or use its local Makefile when present:

```sh
cc -O2 -Wall -Wextra -Werror \
   -o test_i2c_sensor \
   02_i2c_smbus/userspace/test_i2c_sensor.c

make -C 09_mctp/userspace
```

The examples are small test tools, not stable product command-line interfaces.
Read their usage and the lab's safety notes before running them with privileges.

## Building kernel modules

Each `client-driver/Makefile` expects a configured kernel build tree:

```sh
make -C 02_i2c_smbus/client-driver \
     KDIR=/path/to/the/exact/kernel/build
```

Before loading a module, verify:

- the kernel release and configuration match the running target;
- the compatible and register model match actual hardware;
- no in-tree driver already owns the device;
- unload and recovery paths are available;
- the module is not being tested on a production management bus.

Some demo controller register maps are fictional and are provided only for
source study. Their README explicitly says when they must not be instantiated.

## Using Device Tree examples

Files in `device-tree/` are fragments, not complete standalone DTBs. Integrate
them into the target board source with the correct SoC `.dtsi`, labels, clocks,
resets, interrupts, pinctrl, GPIO polarity, regulators, and physical routing.

Validate the integrated tree where a binding exists:

```sh
make ARCH=<arch> dtbs_check
make ARCH=<arch> <board>.dtb
```

A successful DT compile checks syntax and schema constraints; it does not prove
that the schematic, voltage, timing, address, or board population is correct.

## Using scripts safely

Start with scripts named `list_*`, `inspect_*`, `show_*`, or `debug_*`. Review
every script before running it as root. Scripts that scan a bus, write a
register, alter fan speed, configure a CAN interface, bind a USB gadget, or
respond to host KCS traffic can change live system behavior.

Recommended practice:

1. Run on an isolated lab target.
2. Record the original state.
3. Confirm the exact bus, device, channel, or UDC.
4. Keep console and power-cycle recovery available.
5. Restore the original state and verify the next transaction succeeds.

## Debugging model

Every failure should be narrowed across three layers:

| Layer | Questions and evidence |
|---|---|
| Hardware/protocol | Is the device powered and connected? Are voltage, pull-ups, termination, clock, reset, waveform, timing, ACK/checksum, and physical ownership correct? |
| Linux kernel | Did the controller probe? Is the DT node enabled? Did matching and binding occur? Are IRQ, DMA, timeout, locking, error paths, runtime PM, and subsystem registration correct? |
| OpenBMC userspace | Does the expected sysfs/netdev/tty object exist? Is the service running? Is D-Bus configuration correct? Are stale values, retries, fail-safe policy, and Redfish/IPMI presentation correct? |

Useful Linux mechanisms include kernel logs, dynamic debug, tracepoints, ftrace,
function graph tracing, debugfs, sysfs, driver bind/unbind, IRQ statistics,
network statistics, protocol analyzers, and targeted fault injection.

## OpenBMC integration boundary

Device Tree describes non-discoverable hardware and kernel resources. OpenBMC
configuration describes product inventory and userspace policy. They may refer
to the same physical component, but they are not interchangeable.

A typical sensor path is:

```text
Physical device -> Linux bus/client driver -> hwmon or IIO
                -> OpenBMC sensor service -> D-Bus
                -> thermal/health policy -> Redfish or IPMI
```

A typical host-management path is:

```text
Managed host -> eSPI/KCS, VUART, USB, PECI, or MCTP
             -> Linux subsystem -> OpenBMC service
             -> D-Bus, IPMI, Redfish, telemetry, or control policy
```

## Validation status and limitations

The repository is designed for source study and controlled labs. Individual
project READMEs document their own validation status. In general:

- userspace programs can be compiled independently when their UAPI is available;
- shell scripts can be syntax-checked without hardware;
- kernel modules require the exact target headers and configuration;
- DTS fragments require integration into a complete board tree;
- successful compilation is not hardware validation;
- APIs may differ between upstream Linux, OpenBMC vendor branches, and future
  kernel releases.

When adapting a lab, prefer changing the board DTS, configuration, or real client
driver before modifying a mature controller driver. Controller changes should
be driven by verified hardware behavior, errata, timing, or SoC support.

## Repository policy

- Keep examples small enough to study but complete enough to demonstrate error
  handling, ownership, synchronization, and cleanup.
- Mark fictional hardware and lab-only identities prominently.
- Do not commit generated modules, DTBs, captures, logs, or local build outputs.
- Use SPDX license identifiers on new source files.
- Preserve safety warnings when reusing an example.
- Cite upstream documentation and inspect the source from the kernel branch
  actually used by the target.

## License

Unless a file contains a different SPDX license identifier, this repository is
licensed under the GNU General Public License v2.0 only (`GPL-2.0-only`); see
[LICENSE](LICENSE).

Files marked `SPDX-License-Identifier: MIT` are licensed under the MIT License;
see [LICENSES/MIT.txt](LICENSES/MIT.txt).

The legacy `SPDX-License-Identifier: GPL-2.0` identifiers already present in
some examples are interpreted as `GPL-2.0-only` in this repository.

Copyright (c) 2026 Zale Yu.

## Upstream references

- [OpenBMC documentation](https://github.com/openbmc/docs)
- [Linux driver API](https://docs.kernel.org/driver-api/)
- [Linux Device Tree usage model](https://docs.kernel.org/devicetree/usage-model.html)
- [Linux hwmon](https://docs.kernel.org/hwmon/)
- [Linux IIO](https://docs.kernel.org/driver-api/iio/)
- [Linux I2C](https://docs.kernel.org/i2c/)
- [Linux SPI](https://docs.kernel.org/spi/)
- [Linux GPIO](https://docs.kernel.org/driver-api/gpio/)
- [Linux MCTP](https://docs.kernel.org/networking/mctp.html)
- [Linux SocketCAN](https://docs.kernel.org/networking/can.html)
