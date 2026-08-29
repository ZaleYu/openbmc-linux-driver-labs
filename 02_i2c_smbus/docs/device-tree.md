# Device Tree for I2C

## Controller and client roles

The controller node describes the SoC I2C controller: registers, IRQ, clocks,
reset, pinctrl, and controller-specific capabilities. Child nodes describe
devices physically connected to that bus segment.

```dts
&i2c1 {
    status = "okay";
    clock-frequency = <400000>;

    temperature-sensor@48 {
        compatible = "demo,temp-sensor";
        reg = <0x48>;
    };
};
```

For an I2C child, `reg` is normally the target address, not an MMIO address.
The node unit address (`@48`) should match it.

## Matching path

1. The board DTS enables the correct controller.
2. The I2C core creates a client at address `0x48`.
3. `compatible = "demo,temp-sensor"` matches the driver's OF table.
4. The driver verifies device identity and registers hwmon.

## Common properties

| Property | Meaning |
|---|---|
| `compatible` | Vendor/device identifier used for binding |
| `reg` | I2C target address on this bus segment |
| `status` | Usually `"okay"` to enable a disabled SoC node |
| `clock-frequency` | Requested controller bus frequency |
| `interrupt-parent`, `interrupts` | Alert/interrupt wiring |
| `reset-gpios` | Device reset wiring when defined by its binding |
| `wakeup-source` | Device can wake the system, if supported |

Properties must be defined by the device's YAML binding. Do not invent board
properties inside a production DTS without defining and validating a schema.

## Mux topology

An I2C mux creates a logical adapter for each enabled downstream segment. Two
devices may reuse address `0x48` if they are behind different isolated mux
channels. Linux bus numbers are runtime identifiers and may change as adapter
registration order changes; derive topology through sysfs or aliases rather
than hard-coding numbers in production applications.

Inspect links such as:

```sh
find /sys/bus/i2c/devices -maxdepth 2 -type l -name 'channel-*' -print -exec readlink {} \;
i2cdetect -l
```

See `../device-tree/i2c-mux-example.dts`.

## Multi-controller/multi-master buses

The generic `multi-master` property, when supported by the controller binding
and driver, indicates that another controller may use the same physical bus.
It does not create arbitration hardware or define ownership policy.

Before enabling a shared bus, verify:

- Both controllers implement electrical arbitration correctly.
- Pull-ups and voltage domains remain valid in every power state.
- Neither side resets or reconfigures a mux during the other's transaction.
- Retry/backoff and timeout behavior are defined.
- Firmware update, host reset, BMC reset, and power sequencing are covered.
- Bus recovery by one controller cannot corrupt the other controller's transfer.

See `../device-tree/multi-master-example.dts`. Its GPIO arbitration node is an
illustrative ownership mechanism; adapt it to the hardware design and binding.

## DTS versus Entity Manager JSON

- **DTS** describes kernel-visible hardware topology and wiring needed to
  instantiate kernel devices.
- **Entity Manager configuration** describes runtime system entities and their
  exposed features for OpenBMC userspace.

They may describe related hardware, but one is not a replacement for the
other. A kernel driver may expose hwmon, while Entity Manager and dbus-sensors
use board configuration to publish the correct D-Bus sensor object.

## Validation

For a production binding or board change:

```sh
make dt_binding_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/hwmon/demo,temp-sensor.yaml
make dtbs_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/hwmon/demo,temp-sensor.yaml
```

The included overlays rely on board labels such as `&i2c1` and `&gpio`; change
them to labels exported by the target board DTS before compiling.

