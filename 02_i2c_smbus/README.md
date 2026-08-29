# Linux I2C/SMBus for OpenBMC

This module is a practical study package for the Linux I2C/SMBus stack used in
BMC firmware. It follows one data path from electrical transactions to an
OpenBMC sensor object:

```text
Sensor -> I2C controller -> Linux I2C core -> client driver -> hwmon
       -> OpenBMC sensor service -> D-Bus -> Redfish/IPMI/policy
```

The examples use a fictional `demo,temp-sensor` at address `0x48`. The device
is intentionally small enough to study while still demonstrating explicit
Device Tree enumeration, register access, hwmon integration, an optional alert
IRQ, a mux topology, and shared multi-master bus concerns.

## Repository map

| Path | Purpose |
|---|---|
| `docs/protocol.md` | I2C and SMBus protocol fundamentals |
| `docs/linux-i2c-architecture.md` | Adapter, core, client, driver, and hwmon data path |
| `docs/device-tree.md` | Device Tree matching and topology examples |
| `docs/userspace-tools.md` | Safe use of i2c-tools, sysfs, and `/dev/i2c-*` |
| `docs/debugging.md` | Layered debugging and fault isolation |
| `docs/openbmc-use-cases.md` | Server-management scenarios and OpenBMC integration |
| `client-driver/` | Educational Linux hwmon I2C client driver |
| `device-tree/` | Single-device, mux, and multi-master overlays |
| `userspace/` | Direct `/dev/i2c-*` examples for controlled bring-up |
| `scripts/` | Bus inventory, register dump, and diagnostic collection |

## Demo register map

| Address | Name | Access | Meaning |
|---:|---|---|---|
| `0x00` | `DEVICE_ID` | RO | Expected value `0xA5` |
| `0x01` | `TEMP_MSB` | RO | Signed 12-bit temperature, bits 11:4 |
| `0x02` | `TEMP_LSB` | RO | Signed 12-bit temperature, bits 3:0 in `[7:4]` |
| `0x03` | `STATUS` | RO/W1C | Bit 0 is the high-temperature alarm |
| `0x04` | `CONFIG` | RW | Device configuration |
| `0x05` | `TEMP_HIGH` | RW | Signed whole-degree high threshold |

The temperature conversion is `raw12 * 62.5 m°C`. This resembles common
digital temperature sensors but does not describe a specific commercial part.

## Quick start

Prerequisites on the target:

```sh
sudo modprobe i2c-dev
i2cdetect -l
```

Build the userspace examples:

```sh
cc -O2 -Wall -Wextra -o test_i2c_sensor userspace/test_i2c_sensor.c
cc -O2 -Wall -Wextra -o test_smbus_access userspace/test_smbus_access.c
sudo ./test_i2c_sensor /dev/i2c-1 0x48
sudo ./test_smbus_access /dev/i2c-1 0x48
```

Build the out-of-tree kernel module on a development target with matching
kernel headers:

```sh
make -C client-driver
sudo insmod client-driver/demo_i2c_sensor.ko
```

The normal embedded/OpenBMC path is to describe the sensor in Device Tree and
integrate the driver through the kernel configuration and Yocto layer. For a
temporary lab only, a client can be instantiated dynamically:

```sh
echo demo_i2c_sensor 0x48 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
```

Then find the hwmon node:

```sh
grep -H . /sys/class/hwmon/hwmon*/name
grep -H . /sys/class/hwmon/hwmon*/temp1_{input,max,alarm} 2>/dev/null
```

## Safety

- Verify the schematic, bus number, address, voltage, and register semantics.
- Do not force accesses with `i2cget -f` or `i2cset -f` while a kernel driver is
  bound unless the device owner explicitly permits it.
- `i2cdetect` is not a universal discovery protocol; probing can disturb some
  devices. Prefer Device Tree and known inventory.
- Never write unknown registers on power supplies, VRs, EEPROMs, fan
  controllers, security devices, or shared host/BMC buses.
- These examples are educational and require adaptation to the real datasheet,
  binding schema, controller, and board design.

## Recommended study order

1. Read `docs/protocol.md` and identify the transaction on a logic analyzer.
2. Read `docs/linux-i2c-architecture.md` and trace one hwmon read through the stack.
3. Adapt `device-tree/demo-i2c-sensor.dts` to the actual board controller label.
4. Run the userspace programs before binding the kernel driver.
5. Build and bind `demo_i2c_sensor.ko`, then read the hwmon attributes.
6. Study mux topology and bus numbering.
7. Exercise the debugging checklist with NACK, missing device, and alert faults.
8. Map hwmon into the OpenBMC D-Bus sensor architecture.

## References

- Linux kernel I2C documentation: <https://docs.kernel.org/i2c/>
- Device Tree bindings: `Documentation/devicetree/bindings/i2c/` in the kernel tree
- i2c-tools: <https://git.kernel.org/pub/scm/utils/i2c-tools/i2c-tools.git/>
- OpenBMC Entity Manager: <https://github.com/openbmc/entity-manager>
- OpenBMC dbus-sensors: <https://github.com/openbmc/dbus-sensors>

