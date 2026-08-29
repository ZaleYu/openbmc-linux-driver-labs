# Linux I2C Architecture

## Object model

```text
Controller hardware
  -> controller driver
  -> struct i2c_adapter + struct i2c_algorithm
  -> Linux I2C core
  -> struct i2c_client
  -> struct i2c_driver
  -> regmap / hwmon / nvmem / input / other subsystem
  -> sysfs, D-Bus service, or another kernel consumer
```

- **Controller driver:** controls MMIO, clocks, reset, interrupts, DMA/FIFO,
  timing, timeouts, and bus recovery.
- **`i2c_adapter`:** represents one logical bus. Mux channels normally appear
  as additional logical adapters.
- **`i2c_algorithm`:** supplies message transfer and/or SMBus operations.
- **`i2c_client`:** one enumerated target at one address on one adapter.
- **`i2c_driver`:** binds to clients by firmware description or ID table.
- **Function driver:** exposes device semantics through hwmon, IIO, nvmem,
  regulator, GPIO, LED, input, or another standard subsystem.

## Enumeration and binding

I2C does not enumerate devices by itself. Linux normally learns the topology
from Device Tree, ACPI, or board firmware. The core creates an `i2c_client`,
matches it to the driver's OF table or ID table, and invokes `probe()`.

Explicit firmware description is preferred. Legacy address scanning cannot
reliably identify arbitrary I2C devices because the protocol has no universal
device-identification operation.

The demo driver's matching chain is:

```text
compatible = "demo,temp-sensor"
  -> demo_i2c_of_match[]
  -> demo_i2c_driver
  -> demo_i2c_probe(client)
  -> hwmon registration
```

## Transfer APIs

Use the operation matching the datasheet and adapter capability:

- `i2c_transfer()` for one or more raw I2C messages.
- `i2c_smbus_read_byte_data()` and related helpers for SMBus protocols.
- `regmap` for register-oriented devices that benefit from common locking,
  formatting, update-bits, cache, and debug support.

Return values are Linux error codes such as `-ENXIO`, `-EREMOTEIO`, `-EAGAIN`,
or `-ETIMEDOUT`. Preserve useful errors rather than converting all failures to
`-EIO`.

## Where to read controller code

In a Linux kernel source tree, start with:

```text
drivers/i2c/i2c-core-base.c
drivers/i2c/i2c-core-of.c
drivers/i2c/i2c-core-smbus.c
drivers/i2c/busses/
include/linux/i2c.h
Documentation/i2c/
Documentation/devicetree/bindings/i2c/
```

For BMC SoCs, locate the controller driver by its Device Tree `compatible`,
then search the kernel tree:

```sh
rg 'vendor,soc-i2c' drivers/i2c Documentation/devicetree/bindings
rg 'struct i2c_algorithm' drivers/i2c/busses
rg 'i2c_add_adapter|devm_i2c_add_adapter' drivers/i2c/busses
```

Relevant upstream families include ASPEED and Nuvoton NPCM controllers, but an
OpenBMC machine may carry downstream patches. Always inspect the exact kernel
revision selected by the Yocto build.

## What normally changes

| Requirement | First place to change |
|---|---|
| Add a known board device | Board DTS/DTSI and kernel config |
| Add a new sensor function | Client/function driver and binding schema |
| Add an OpenBMC sensor | hwmon/IIO exposure plus userspace configuration |
| Correct bus clock or pin route | DTS, clocks, pinctrl, controller properties |
| Support a new SoC revision | Controller compatible, register layout, quirks |
| Fix timeout/arbitration/FIFO bug | Controller driver state machine |
| Recover a stuck bus | Controller/GPIO recovery plus board reset/isolation policy |
| Support a mux topology | Mux driver, DTS child buses, stable topology handling |

Do not modify the bus-controller driver merely because a new client is placed
on the board. Most board changes belong in Device Tree and the appropriate
client driver.

## Reading the demo driver

`client-driver/demo_i2c_sensor.c` demonstrates:

- OF and legacy ID matching.
- Adapter functionality validation.
- regmap-based register access.
- Device-ID validation.
- standard hwmon attributes (`temp1_input`, `temp1_max`, `temp1_alarm`).
- optional threaded interrupt handling.
- managed resource allocation and registration.

The fictional register map must be replaced with the real datasheet before the
driver is used on hardware.

