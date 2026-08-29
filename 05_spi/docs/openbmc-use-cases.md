# OpenBMC use cases

## Boot and recovery flash

BMC firmware commonly lives in SPI-NOR. Linux exposes it through MTD and fixed
partitions or a firmware-management stack. Updates require erase/write/verify,
power-loss design, write protection, rollback policy, and a recovery image.
Never validate a raw SPI experiment on the active boot flash.

## Host firmware access

Some BMCs mediate access to host BIOS flash. Hardware straps, eSPI/LPC state,
flash mux ownership, write protection and host power state determine who may
drive the bus. A second controller in DT does not by itself make concurrent
access safe; ownership must be enforced in hardware and platform software.

## TPM, CPLD and board controllers

TPM-over-SPI uses the kernel TPM stack and security timeouts, not arbitrary
spidev transactions. CPLDs/FPGAs may provide board ID, resets, power sequencing
or telemetry. Prefer a kernel MFD/regmap driver that creates standard child
interfaces instead of a private raw command daemon.

## Shared bus with multiple chip selects

Targets share clock/data but have independent CS. Validate that unselected
targets tri-state MISO, each device gets its own mode/frequency, and the
controller can switch configuration between queued messages. Debug one CS at a
time and capture all CS lines when cross-device corruption is suspected.

## End-to-end sensor path

```text
SPI sensor -> protocol driver -> hwmon sysfs -> dbus-sensors
           -> D-Bus sensor object -> Redfish/IPMI/thermal policy
```

Entity Manager may describe board configuration consumed by userspace, but it
does not replace Device Tree controller/peripheral enumeration. For a fixed
on-board SPI target, DT plus the kernel subsystem driver is normally the source
of hardware truth.

