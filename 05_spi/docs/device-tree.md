# SPI Device Tree

The Device Tree describes how an SPI controller connects to downstream peripherals, including:

- Controller MMIO, IRQ, clock, reset, and pin control.
- The Chip Select used by each peripheral.
- The maximum SPI clock accepted by a peripheral.
- Clock polarity and phase.
- GPIO Chip Selects.
- Special modes such as 3-wire, Dual SPI, and Quad SPI.
- Peripheral IRQs, reset GPIOs, voltage supplies, and external clocks.

The key hierarchy is:

```text
SPI Controller
    |
    +-- CS0 --> SPI Peripheral 0
    +-- CS1 --> SPI Peripheral 1
    +-- CS2 --> SPI Peripheral 2
```

An SPI peripheral is therefore a child node of its SPI controller.

---

# 1. SPI Controller and Peripheral

```dts
&spi0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    sensor@0 {
        compatible = "vendor,demo-spi-sensor";
        reg = <0>;
        spi-max-frequency = <10000000>;
    };

    flash@1 {
        compatible = "jedec,spi-nor";
        reg = <1>;
        spi-max-frequency = <50000000>;
    };
};
```

This represents:

```text
SPI Controller spi0
    +-- Native CS0 --> Demo Sensor
    +-- Native CS1 --> SPI NOR Flash
```

| Child Node | `reg` | Chip Select |
| --- | --- | --- |
| `sensor@0` | `<0>` | CS0 |
| `flash@1` | `<1>` | CS1 |

---

# 2. SPI `reg` Is Not a Memory Address

For a platform device, `reg` often describes an MMIO range:

```dts
uart@12340000 {
    reg = <0x12340000 0x1000>;
};
```

```text
Base Address = 0x12340000
Size         = 0x1000
```

For an SPI peripheral, however, `reg` is the **Chip Select index**:

```dts
sensor@0 { reg = <0>; };  // CS0
flash@1  { reg = <1>; };  // CS1
```

It is not a CPU memory address, SPI register address, or internal peripheral register address. An internal register such as sensor register `0x10` is sent as a protocol command by the SPI driver during a transfer; it is not placed in Device Tree `reg`.

---

# 3. Why One Address Cell?

SPI controllers normally use:

```dts
#address-cells = <1>;
#size-cells = <0>;
```

`#address-cells = <1>` means that each child’s `reg` contains one 32-bit cell. One cell is enough for the Chip Select index.

`#size-cells = <0>` means that children have no size cell. An SPI peripheral is not a memory range in the controller’s address space; it only needs to specify which Chip Select it uses.

---

# 4. Node Name and Unit Address

```dts
sensor@0 { reg = <0>; };
flash@1  { reg = <1>; };
```

The Unit Address after `@` should match `reg`:

| Node Name | `reg` | Meaning |
| --- | --- | --- |
| `sensor@0` | `<0>` | CS0 |
| `flash@1` | `<1>` | CS1 |
| `adc@2` | `<2>` | CS2 |

Incorrect:

```dts
sensor@1 {
    reg = <0>;
};
```

The node name says CS1 while `reg` says CS0, so `dtc` or `dtbs_check` may warn.

---

# 5. `compatible`

```dts
compatible = "vendor,demo-spi-sensor";
```

`compatible` identifies the type of SPI peripheral. Linux uses it to match the Device Tree table of an SPI protocol driver:

```c
static const struct of_device_id demo_spi_of_match[] = {
    { .compatible = "vendor,demo-spi-sensor" },
    { }
};
MODULE_DEVICE_TABLE(of, demo_spi_of_match);

static struct spi_driver demo_spi_driver = {
    .driver = {
        .name = "demo_spi_sensor",
        .of_match_table = demo_spi_of_match,
    },
    .probe = demo_spi_probe,
    .remove = demo_spi_remove,
};
```

```text
Device Tree compatible
        |
        v
Linux Creates spi_device
        |
        v
Search Driver of_match_table
        |
        v
Match demo_spi_sensor
        |
        v
Call demo_spi_probe()
```

Production systems must use the real `compatible` defined by the device binding.

---

# 6. `spi-max-frequency`

```dts
spi-max-frequency = <10000000>;
```

The unit is Hz, so this is 10 MHz. It specifies the **maximum** SPI clock accepted by the peripheral; it does not guarantee an exact 10 MHz clock.

The actual clock depends on the peripheral and controller limits, parent clock, divider, driver rounding, SPI mode, PCB signal integrity, and per-transfer speed settings.

If the peripheral limit is 10 MHz but the controller can generate only 12, 8, or 4 MHz, it must not use 12 MHz and may choose 8 MHz. Therefore, interpret the property as “must not exceed 10 MHz.”

Verify the peripheral datasheet, controller datasheet, PCB signal integrity, and Linux controller driver together. During initial bring-up, start at a lower rate such as:

```dts
spi-max-frequency = <1000000>;
```

Then increase it gradually after communication is stable.

---

# 7. SPI Mode: CPOL and CPHA

SPI mode is determined by Clock Polarity (CPOL) and Clock Phase (CPHA):

```dts
spi-cpol;
spi-cpha;
```

- `spi-cpol` present: CPOL = 1, clock idles high. Absent: CPOL = 0, clock idles low.
- `spi-cpha` present: CPHA = 1, sampling/shifting uses the second-edge definition. Absent: CPHA = 0, sampling begins at the first edge.

| SPI Mode | CPOL | CPHA | Device Tree |
| --- | --- | --- | --- |
| Mode 0 | 0 | 0 | Neither property |
| Mode 1 | 0 | 1 | `spi-cpha` |
| Mode 2 | 1 | 0 | `spi-cpol` |
| Mode 3 | 1 | 1 | Both properties |

Mode 0:

```dts
sensor@0 {
    compatible = "vendor,demo-spi-sensor";
    reg = <0>;
    spi-max-frequency = <10000000>;
};
```

Mode 1 adds `spi-cpha;`, Mode 2 adds `spi-cpol;`, and Mode 3 adds both.

The mode must exactly match the peripheral datasheet. An incorrect mode can cause all-zero or all-one reads, one-bit shifts, incorrect device IDs, intermittent low-speed success, total high-speed failure, or data changing on the wrong clock edge.

---

# 8. Chip Select

Chip Select is abbreviated `CS` and may also be called `SS` (Slave Select) or `nCS` (active-low Chip Select).

```text
CS0 Active --> Peripheral 0
CS1 Active --> Peripheral 1
CS2 Active --> Peripheral 2
```

Most devices use active-low CS:

```text
CS = Low  --> Selected
CS = High --> Not selected
```

Some devices use active-high CS.

---

# 9. Native Chip Select

A native Chip Select is generated directly by the SPI controller hardware.

```dts
sensor@0 { reg = <0>; };
```

If hardware CS0 connects directly to the sensor, the controller driver uses native CS0. Benefits include hardware-controlled timing, more stable behavior between transfers, lower CPU overhead, and suitability for high-speed transfers. The number of native CS signals depends on the controller hardware.

---

# 10. GPIO Chip Select

When native CS signals are insufficient or CS is wired to GPIO, use `cs-gpios` in the controller node:

```dts
cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>,
           <&gpio0 11 GPIO_ACTIVE_LOW>;
```

```text
reg = <0> --> cs-gpios entry 0 --> GPIO0_10
reg = <1> --> cs-gpios entry 1 --> GPIO0_11
```

```dts
#include <dt-bindings/gpio/gpio.h>

&spi0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;
    cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>,
               <&gpio0 11 GPIO_ACTIVE_LOW>;

    sensor@0 {
        compatible = "vendor,demo-spi-sensor";
        reg = <0>;
        spi-max-frequency = <10000000>;
    };
    adc@1 {
        compatible = "vendor,demo-spi-adc";
        reg = <1>;
        spi-max-frequency = <5000000>;
    };
};
```

GPIO CS timing depends on software and the GPIO controller, may be less suitable for high speed, may not support every special CS behavior, and needs careful suspend/resume and bootloader-to-kernel state handling.

---

# 11. Mixing Native and GPIO Chip Selects

Some controllers support a mixture such as:

```text
CS0 --> Native
CS1 --> GPIO
CS2 --> Native
```

Some bindings may use empty entries conceptually like:

```dts
cs-gpios = <0>,
           <&gpio0 11 GPIO_ACTIVE_LOW>,
           <0>;
```

Whether this syntax is permitted must be verified in the target controller’s YAML binding.

---

# 12. `spi-cs-high`

```dts
spi-cs-high;
```

This declares active-high CS. Without it, Linux defaults to active-low CS.

With GPIO CS, consider the `GPIO_ACTIVE_LOW`/`GPIO_ACTIVE_HIGH` flag, `spi-cs-high`, actual hardware polarity, controller-driver interpretation, and controller binding together. The Device Tree, Linux SPI core active level, and peripheral datasheet must agree.

Conflicts can leave CS permanently active or never asserted, generate polarity warnings, select a device during probe, make multiple peripherals drive MISO, or accidentally trigger write/erase commands at boot.

---

# 13. `spi-3wire`

```dts
spi-3wire;
```

Normal SPI uses SCLK, MOSI, MISO, and CS. Three-wire SPI combines MOSI and MISO into a bidirectional SDIO line and is normally half-duplex.

Verify support in the peripheral, controller hardware, controller driver, pinmux, and data-line turnaround timing. Adding `spi-3wire` cannot give unsupported hardware this capability.

---

# 14. `spi-lsb-first`

```dts
spi-lsb-first;
```

SPI normally sends the most significant bit first. For `0xA6` (`1010 0110`), MSB-first order is `1,0,1,0,0,1,1,0`; LSB-first order is `0,1,1,0,0,1,0,1`.

LSB-first operation must be supported by the peripheral, controller hardware, and controller driver. Otherwise, `spi_setup()` or driver probe may fail.

---

# 15. `spi-rx-bus-width` and `spi-tx-bus-width`

Normal SPI uses one data line in each direction. Some SPI NOR, SPI NAND, displays, and high-speed devices support Dual (2), Quad (4), or Octal (8) SPI:

```dts
spi-rx-bus-width = <4>;
spi-tx-bus-width = <4>;
```

| Value | Mode |
| --- | --- |
| `1` | Single SPI |
| `2` | Dual SPI |
| `4` | Quad SPI |
| `8` | Octal SPI, when supported by the binding and controller |

Example:

```dts
flash@0 {
    compatible = "jedec,spi-nor";
    reg = <0>;
    spi-max-frequency = <50000000>;
    spi-rx-bus-width = <4>;
    spi-tx-bus-width = <1>;
};
```

This allows single-lane commands/writes and reads of up to four lanes. These properties describe peripheral capability and board wiring, not simply “four times the speed.” Actual use depends on the peripheral and controller drivers, hardware, configuration registers, opcodes, dummy cycles, physical data-line wiring, bootloader state, and binding.

---

# 16. Peripheral-Specific Properties

In addition to common SPI properties, a peripheral may require:

- `interrupts`, `interrupt-parent`
- `reset-gpios`, `enable-gpios`, `power-gpios`
- `vdd-supply`, `vref-supply`
- `clocks`, `clock-names`
- `pinctrl`, `wakeup-source`
- Vendor-specific properties

Example with IRQ, reset, and power:

```dts
sensor@0 {
    compatible = "vendor,demo-spi-sensor";
    reg = <0>;
    spi-max-frequency = <10000000>;
    interrupt-parent = <&gpio0>;
    interrupts = <20 IRQ_TYPE_EDGE_FALLING>;
    reset-gpios = <&gpio0 21 GPIO_ACTIVE_LOW>;
    vdd-supply = <&vdd_3v3>;
};
```

Their formats are defined by the peripheral’s YAML binding. Do not assume that every SPI device uses the same properties.

---

# 17. SPI Controller Node

The controller node describes the SoC’s SPI controller hardware:

```dts
spi0: spi@12340000 {
    compatible = "vendor,soc-spi";
    reg = <0x12340000 0x1000>;
    interrupts = <0 42 4>;
    clocks = <&clock_controller 10>;
    resets = <&reset_controller 5>;
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>;
    #address-cells = <1>;
    #size-cells = <0>;
    status = "disabled";
};
```

Controller and peripheral `reg` have different meanings:

| Node | Meaning of `reg` |
| --- | --- |
| SPI Controller | MMIO base address and size |
| SPI Peripheral | Chip Select index |

```dts
spi@12340000 {
    reg = <0x12340000 0x1000>;
    sensor@0 { reg = <0>; };
};
```

This means the controller is at MMIO address `0x12340000`, and the sensor is connected to its CS0.

---

# 18. Resources Managed by the Controller

The controller node normally manages MMIO registers, controller IRQ, input clock, reset, pinmux, DMA, power domain, native and GPIO Chip Selects, controller-specific timing, FIFO, and transfer limits.

These are SoC-specific. Different controllers may have completely different `compatible` strings, register addresses, clock/reset IDs, IRQs, DMA channels, CS counts, FIFO sizes, maximum transfer sizes, and supported modes. Always consult the controller’s YAML binding.

---

# 19. Pinctrl

The pins must be switched to the SPI function:

```dts
pinctrl-names = "default";
pinctrl-0 = <&spi0_pins>;
```

The pin-controller definition may look like:

```dts
spi0_pins: spi0-pins {
    pins = "GPIO10", "GPIO11", "GPIO12", "GPIO13";
    function = "spi0";
};
```

The exact format is SoC-specific. With incorrect pinctrl, controller probe and device creation may succeed while the physical pins still output no valid SCLK, MOSI, MISO, or CS signals.

---

# 20. DMA

Some controllers use DMA for large transfers:

```dts
dmas = <&dma0 5>, <&dma0 6>;
dma-names = "tx", "rx";
```

DMA requirements, channel numbers, and names are defined by the controller binding. DMA is useful for displays, large ADC sample streams, SPI NOR/NAND, high-speed full-duplex devices, and long continuous transfers. Small register accesses may work normally through PIO.

---

# 21. Why Production Systems Should Not Use `"spidev"`

This may be tempting for direct userspace access:

```dts
test@0 {
    compatible = "spidev";
    reg = <0>;
    spi-max-frequency = <1000000>;
};
```

It is generally inappropriate for a production Device Tree. `compatible` should describe **what the hardware is**, not **which generic Linux driver should be used**. `"spidev"` is a Linux userspace-interface driver name, not a hardware model.

Use the real device binding, such as:

```dts
compatible = "vendor,actual-device";
```

Production devices should bind to a real hwmon, IIO, MTD/SPI NOR, input, network, RTC, GPIO-expander, or device-specific SPI driver. The kernel can then manage power, reset, interrupts, concurrency, suspend/resume, recovery, security, and permissions correctly.

---

# 22. Using Spidev During Development

For bring-up or short-term protocol verification, consider an actual `compatible` allowed by the kernel spidev driver, a board-specific entry correctly added to its match table, or temporary use of `driver_override`.

Given:

```text
/sys/bus/spi/devices/spi0.0
```

Inspect it:

```sh
readlink /sys/bus/spi/devices/spi0.0/driver
cat /sys/bus/spi/devices/spi0.0/modalias
```

Set an override:

```sh
echo spidev | sudo tee \
    /sys/bus/spi/devices/spi0.0/driver_override
```

If no other driver owns the device, bind it:

```sh
echo spi0.0 | sudo tee \
    /sys/bus/spi/drivers/spidev/bind
```

This may create `/dev/spidev0.0`. Clear the override after testing:

```sh
echo "" | sudo tee \
    /sys/bus/spi/devices/spi0.0/driver_override
```

Do not unbind an active production driver without understanding system state; doing so may interrupt flash, filesystems, sensors, networking, displays, or other functions. `driver_override` is for controlled development, not production architecture.

---

# 23. Complete Peripheral Example

```dts
#include <dt-bindings/gpio/gpio.h>
#include <dt-bindings/interrupt-controller/irq.h>

&spi0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    sensor@0 {
        compatible = "vendor,demo-spi-temperature";
        reg = <0>;
        spi-max-frequency = <10000000>;
        interrupt-parent = <&gpio0>;
        interrupts = <20 IRQ_TYPE_EDGE_FALLING>;
        reset-gpios = <&gpio0 21 GPIO_ACTIVE_LOW>;
        vdd-supply = <&vdd_3v3>;
    };
};
```

```text
Controller:   spi0
Chip Select:  CS0
Maximum Clock: 10 MHz
SPI Mode:     Mode 0
IRQ:          GPIO0_20, Falling Edge
Reset:        GPIO0_21, Active Low
Power:        vdd_3v3
```

With no `spi-cpol` or `spi-cpha`, the default is Mode 0. With no `spi-cs-high`, CS defaults to active-low.

---

# 24. Mode 3 with GPIO CS Example

```dts
#include <dt-bindings/gpio/gpio.h>

&spi1 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;
    cs-gpios = <&gpio1 5 GPIO_ACTIVE_LOW>;

    adc@0 {
        compatible = "vendor,demo-spi-adc";
        reg = <0>;
        spi-max-frequency = <5000000>;
        spi-cpol;
        spi-cpha;
    };
};
```

```text
Controller:   spi1
Peripheral:   ADC
CS Index:     0
CS Signal:    GPIO1_5
CS Polarity:  Active Low
Maximum Clock: 5 MHz
SPI Mode:     Mode 3
```

---

# 25. Device Tree Schema Validation

Successful `dtc` compilation is not enough. Run YAML schema validation in the target kernel tree:

```sh
make dt_binding_check DT_SCHEMA_FILES=spi
make dtbs_check DT_SCHEMA_FILES=spi
```

Or specify the binding path when supported:

```sh
make dt_binding_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/spi/
```

Schema checks can detect an invalid `compatible`, missing `spi-max-frequency`, incorrect `reg` cell count, undefined properties, invalid bus widths, missing clocks/resets/supplies, and child nodes that violate their peripheral bindings.

Use the kernel tree adopted by the target project because bindings differ between kernel versions.

---

# 26. Runtime Validation

List SPI devices:

```sh
ls -l /sys/bus/spi/devices/
```

Names use `spi<bus-number>.<chip-select>`. For example, `spi0.1` means SPI Bus 0, Chip Select 1.

Check driver binding and modalias:

```sh
readlink /sys/bus/spi/devices/spi0.0/driver
cat /sys/bus/spi/devices/spi0.0/modalias
```

If no driver link exists, the cause may be an unmatched `compatible`, unloaded or disabled driver, probe failure, unsupported mode, unsatisfied frequency, or missing IRQ/GPIO/clock/supply.

Inspect logs and spidev nodes:

```sh
dmesg | grep -i spi
sudo dmesg -w
ls -l /dev/spidev*
```

---

# 27. Layered Debugging Procedure

## Layer 1: Controller

Confirm `status = "okay"`, MMIO address, enabled clock, deasserted reset, correct IRQ, and successful controller probe.

## Layer 2: Pinctrl

With an oscilloscope or logic analyzer, confirm SCLK activity, MOSI data, correct MISO direction, CS assertion, and that pins are not left in GPIO or another function.

## Layer 3: Chip Select

Confirm the `reg`/CS mapping, native or GPIO configuration, active level, that only the target is selected during a transfer, and that multiple peripherals do not drive MISO simultaneously.

## Layer 4: SPI Mode

Verify CPOL and CPHA against the datasheet:

```text
Wrong Clock Idle Level --> Check CPOL first
Wrong Sampling Edge    --> Check CPHA first
```

## Layer 5: Clock Frequency

First reduce the speed:

```dts
spi-max-frequency = <1000000>;
```

Low-speed success but high-speed failure may indicate signal integrity, long traces, drive strength, divider, peripheral timing, level-shifter, or GPIO-CS timing problems.

## Layer 6: Protocol

Verify command opcode, register address and width, dummy bytes, read/write bit, byte order, CS-hold timing, and transfer length.

## Layer 7: Peripheral Resources

Verify power, reset, enable GPIO, external clock, IRQ, and power sequence.

---

# 28. Common Mistakes

## Treating `reg` as an Internal Peripheral Register

```dts
sensor@10 { reg = <0x10>; };
```

If `0x10` was intended as an internal sensor register, this is wrong. For an SPI child, it means CS16.

## Node Name and `reg` Do Not Match

```dts
sensor@1 { reg = <0>; };  // Wrong
sensor@0 { reg = <0>; };  // Correct
```

## Treating `spi-max-frequency` as a Guaranteed Speed

It is an upper limit; the actual clock may be lower.

## Incorrect SPI Mode

Missing `spi-cpha` or `spi-cpol` can shift every data bit.

## Incorrect GPIO CS Polarity

Conflicts among `cs-gpios` flags, `spi-cs-high`, and hardware polarity can leave the device permanently selected.

## Requesting an Unsupported Special Mode

Adding `spi-3wire`, `spi-lsb-first`, or `spi-rx-bus-width = <4>` does not guarantee hardware support.

## Using `"spidev"` in Production

The Device Tree should describe actual hardware, not a generic Linux driver name.

## Checking Only for a sysfs Device

The presence of `spi0.0` proves only that Linux created an SPI device. It does not prove correct wiring, mode, clock, power, MISO response, or driver protocol.

---

# 29. Recommended Validation Sequence

```text
1. Inspect the Schematic
        |
2. Confirm the SPI Controller Binding
        |
3. Confirm Pinctrl
        |
4. Confirm CS Index and Polarity
        |
5. Confirm SPI Mode
        |
6. Confirm Maximum Frequency
        |
7. Run dt_binding_check
        |
8. Run dtbs_check
        |
9. Confirm Controller Probe
        |
10. Confirm spi_device and Driver Binding
        |
11. Perform the First Transfer at Low Speed
        |
12. Validate Waveforms with a Logic Analyzer
        |
13. Validate IRQ, Reset, Power, and Special Modes
```

---

# 30. Summary of Core Concepts

1. **An SPI peripheral is a child node of an SPI controller.**
2. **A peripheral’s `reg` is the Chip Select index—not a memory address or internal register address.**
3. **An SPI controller normally uses:**

   ```dts
   #address-cells = <1>;
   #size-cells = <0>;
   ```

4. **`compatible` matches the actual SPI protocol driver.**
5. **`spi-max-frequency` is the maximum permitted clock, not a guaranteed exact clock.**
6. **`spi-cpol` and `spi-cpha` determine the mode:**

   ```text
   Mode 0: Neither
   Mode 1: spi-cpha
   Mode 2: spi-cpol
   Mode 3: spi-cpol + spi-cpha
   ```

7. **`spi-cs-high` declares active-high CS; GPIO flags, Linux configuration, and hardware polarity must agree.**
8. **`spi-3wire`, `spi-lsb-first`, and multi-lane transfers require support from the peripheral, controller hardware, and controller driver.**
9. **Controller MMIO, IRQ, clock, reset, DMA, pinctrl, and `cs-gpios` are SoC-specific and must follow the YAML binding.**
10. **Production systems should not use generic `compatible = "spidev"`; bind the real device to its subsystem driver.**
11. **Successful DTS compilation does not prove communication. Verify CS, mode, clock, protocol, power, and actual waveforms.**
