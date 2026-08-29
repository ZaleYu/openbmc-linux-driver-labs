# Linux hwmon and IIO for OpenBMC

This lab explains how Linux represents slow platform-health measurements and
general ADC/sensor data. It implements a fictional memory-mapped BMC ADC as an
IIO provider, then uses the in-tree `iio-hwmon` consumer to expose selected
channels through the standard hwmon ABI.

## Contents

- `docs/` — concepts, architecture, Device Tree, tools, debugging, OpenBMC.
- `client-driver/` — educational four-channel MMIO IIO ADC driver.
- `device-tree/` — provider, iio-hwmon and voltage-divider examples.
- `userspace/` — strict readers for IIO and hwmon sysfs.
- `scripts/` — inventory, monitoring and diagnosis helpers.

## Data path

    ADC registers -> IIO provider -> raw/scale channels
                                  -> iio-hwmon -> hwmon sysfs
                                                -> dbus-sensors
                                                -> D-Bus/Redfish/policy

The hardware and compatible string are fictional. Replace the register map,
conversion math, binding, clocks, resets, IRQ and calibration with the real SoC
documentation before use.

## References

- Linux `Documentation/driver-api/iio/`
- Linux `Documentation/hwmon/`
- Devicetree `hwmon/iio-hwmon.yaml`

