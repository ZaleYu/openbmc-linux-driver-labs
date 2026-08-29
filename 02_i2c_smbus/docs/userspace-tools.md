# Userspace I2C Tools

## Prerequisites

Enable the kernel I2C character-device interface and install i2c-tools:

```sh
sudo modprobe i2c-dev
i2cdetect -l
ls -l /dev/i2c-* /sys/class/i2c-dev 2>/dev/null
```

Bus numbers are assigned to adapters, including mux channels. Confirm the
adapter name and topology before any access.

## i2c-tools

### List adapters

```sh
i2cdetect -l
```

### Scan a known-safe bus

```sh
sudo i2cdetect -y 1
```

`--` in the grid means no response; an address value means a response; `UU`
normally means a kernel driver owns that address. A scan is not guaranteed safe
and does not identify the device type.

### Read a byte register

```sh
sudo i2cget -y 1 0x48 0x00 b
```

### Write a byte register

```sh
sudo i2cset -y 1 0x48 0x05 0x55 b
```

Only write registers explicitly documented as writable. A raw write can reset
a device, alter power rails, commit EEPROM, clear a fault, or conflict with a
bound driver.

### Combined transfers

```sh
# Write register 0x01, then read two bytes with a repeated START.
sudo i2ctransfer -y 1 w1@0x48 0x01 r2
```

`i2ctransfer` makes the message shape explicit and is often preferable during
protocol bring-up.

### Dump a small known-safe range

Use `../scripts/dump_registers.sh`; it reads only the demo range. Avoid blind
full-address-space dumps because some devices change state when registers are
read.

## Dynamic client creation

For temporary testing when the device is not described by firmware:

```sh
echo demo_i2c_sensor 0x48 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
echo 0x48 | sudo tee /sys/bus/i2c/devices/i2c-1/delete_device
```

Production embedded systems should normally instantiate known devices from
Device Tree. Dynamic creation does not replace the board description.

## Driver binding

```sh
readlink /sys/bus/i2c/devices/1-0048/driver
cat /sys/bus/i2c/devices/1-0048/modalias
cat /sys/bus/i2c/devices/1-0048/uevent
```

Controlled unbind/rebind during development:

```sh
echo 1-0048 | sudo tee /sys/bus/i2c/drivers/demo_i2c_sensor/unbind
echo 1-0048 | sudo tee /sys/bus/i2c/drivers/demo_i2c_sensor/bind
```

Do not unbind a component responsible for active cooling, power sequencing, or
system safety without an approved fallback.

## Direct C examples

`test_i2c_sensor.c` uses `I2C_RDWR` to demonstrate the repeated-START register
read. `test_smbus_access.c` uses SMBus ioctl helpers and checks adapter
functionality. Direct userspace access is most appropriate for controlled
bring-up or diagnostics while no kernel function driver owns the address.

## Why `-f` is dangerous

The `-f` option forces access even when a kernel driver has claimed the client.
This can race with the driver, invalidate regmap cache, split a read-modify-write
sequence, consume a read-to-clear status, or change hardware state behind the
driver's back. Treat it as a last-resort laboratory option, not a normal debug
workflow.

