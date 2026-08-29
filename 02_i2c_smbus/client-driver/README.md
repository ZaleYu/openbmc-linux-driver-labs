# Demo I2C Sensor Driver

This is an educational Linux I2C client driver that exposes a fictional digital
temperature sensor through hwmon.

## Features demonstrated

- Device Tree and I2C ID matching.
- `probe()`-time adapter and device-ID checks.
- managed allocation and regmap initialization.
- signed 12-bit temperature conversion.
- standard hwmon input, maximum threshold, and alarm attributes.
- optional threaded alert IRQ.
- error propagation with `dev_err_probe()`.

## Build out of tree

```sh
make
sudo insmod demo_i2c_sensor.ko
dmesg | tail
```

The build host must have headers prepared for the exact running target kernel.
Cross compilation can be supplied through the usual kernel build variables:

```sh
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
     KDIR=/path/to/kernel/build
```

## Bind for a temporary lab

When the device is not already instantiated by Device Tree:

```sh
echo demo_i2c_sensor 0x48 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
grep -H . /sys/class/hwmon/hwmon*/name
```

Expected ABI for the matching hwmon directory:

```text
name
temp1_input
temp1_max
temp1_alarm
```

Example:

```sh
cat /sys/class/hwmon/hwmonX/temp1_input
echo 85000 | sudo tee /sys/class/hwmon/hwmonX/temp1_max
cat /sys/class/hwmon/hwmonX/temp1_alarm
```

Values are in millidegrees Celsius, as required by the hwmon ABI.

## Production adaptation

Before adapting this driver:

1. Replace the fictional register map and conversions with the datasheet.
2. Add a real vendor-compatible string and YAML binding.
3. Verify supported I2C/SMBus operations and PEC requirements.
4. Define writable ranges, volatile registers, and caching policy.
5. Decide whether alarms are latched, read-to-clear, or write-one-to-clear.
6. Add power management, reset, regulator, and clock handling if required.
7. Test negative temperature, boundaries, NACK, reset, timeout, and IRQ storms.
8. Integrate through the target OpenBMC kernel recipe/config fragment.

Do not dynamically bind a fictional driver to a real device merely because it
responds at address `0x48`.

