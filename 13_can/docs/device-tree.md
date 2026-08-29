# Device Tree

Device Tree describes the non-discoverable controller, oscillator/clock,
interrupt, pin routing, transceiver, regulators, and control GPIOs. It does not
list CAN message IDs or define the application protocol.

## On-SoC controller

`device-tree/m-can-controller.dts` enables an SoC node already declared by its
`.dtsi` and connects a transceiver through `phys`. The exact binding may instead
use `xceiver-supply`, `standby-gpios`, or a controller-specific property. Follow
the YAML schema in the target kernel.

## External SPI CAN FD controller

`device-tree/mcp2518fd-spi-can.dts` demonstrates an MCP2518FD with an oscillator,
active-low interrupt, SPI frequency, and optional transceiver supply. Check chip
variant, crystal tolerance, interrupt electrical type, SPI mode, and maximum
frequency against the board.

## Dual-channel topology

`device-tree/dual-can-transceivers.dts` highlights separate transceiver standby
lines. Never assume one GPIO can safely control both networks. Linux interface
numbers are discovery order and should not be hard-coded as physical identity;
use stable udev naming or topology-aware service configuration.

## Common properties

| Property | Purpose |
|---|---|
| `compatible` | Select controller/transceiver driver |
| `reg` | MMIO range or SPI chip select |
| `interrupts` | Controller IRQ |
| `clocks` / `clock-frequency` | Timing source |
| `pinctrl-*` | CAN TX/RX pin mux |
| `phys` | Link controller to CAN transceiver PHY |
| `standby-gpios` | Put transceiver into normal/standby mode |
| `xceiver-supply` | Transceiver power rail |
| `status` | Board-level enablement |

GPIO polarity belongs in the GPIO flags. Verify whether standby, enable, and
silent-mode pins are active high or active low.

## Validation

```sh
make ARCH=<arch> dtbs_check DT_SCHEMA_FILES=net/can/<binding>.yaml
make ARCH=<arch> <board>.dtb
```

At runtime check the resolved device, clock, IRQ, and network interface:

```sh
ip -details link show type can
readlink /sys/class/net/can0/device/driver
udevadm info --attribute-walk /sys/class/net/can0
cat /proc/interrupts | grep -i can
```

Copying an example without matching the schematic can hold the transceiver in
standby, generate the wrong bitrate, or drive a shared pin incorrectly.
