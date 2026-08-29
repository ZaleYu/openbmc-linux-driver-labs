# Userspace Tools

## Inspect before accessing

Linux exposes I3C devices and identity attributes under `/sys/bus/i3c/devices`.
Depending on the kernel, useful files include:

- `pid`, `bcr`, `dcr`, `dynamic_address`, `hdrcap`, and `modalias`;
- `driver`, `of_node`, `uevent`, and power-management attributes;
- controller attributes such as bus mode and clock frequencies.

```sh
find /sys/bus/i3c/devices -maxdepth 2 -type f -readable -print
./scripts/list_i3c.sh
./inspect_i3c_device /sys/bus/i3c/devices/<device-name>
```

Use PID and topology for identity. A dynamic address is diagnostic state, not a
persistent application key.

## Why there is no `i2cget` equivalent here

The upstream/vendor I3C userspace character-device APIs and device names have
not historically been as uniform or stable as `/dev/i2c-*`. OpenBMC kernel
branches may carry `i3cdev` or MCTP-specific interfaces that differ from the
mainline branch. A sample tied to one private ioctl header would mislead users
on another platform.

This package therefore provides portable sysfs/hwmon programs. For direct lab
access, first inspect the exact target kernel's `drivers/i3c/i3cdev.c`, UAPI
header, Kconfig help, and platform tools. Never copy ioctl numbers into an
application or use an out-of-tree ABI without pinning its kernel version.

## Production path

Bind a kernel target driver and consume a stable subsystem interface:

- hwmon for temperatures, voltages, fan/power telemetry;
- IIO for sampled sensors;
- nvmem for device-managed nonvolatile data;
- MCTP sockets/interfaces for management transport;
- D-Bus objects exported by the appropriate OpenBMC service.

The demo driver registers hwmon attributes. The included `test_i3c_sensor`
reads `name`, `temp1_input`, `temp1_max`, `temp1_alarm`, and the optional IBI
diagnostic attributes through sysfs without bypassing the driver.

## Permissions and ownership

Use udev rules, service sandboxing, and group permissions instead of globally
writable device nodes. Only one component should own configuration registers.
If a debug utility and production driver access the same target concurrently,
they can race even when individual bus transfers are serialized.

## Dangerous operations

RSTDAA, SETNEWDA, ENTDAA, reset CCCs, forced driver unbind, and arbitrary
private writes can invalidate every target address or disrupt MCTP/telemetry.
Require an explicit maintenance state, an approved recovery sequence, and a
captured topology before issuing them.

