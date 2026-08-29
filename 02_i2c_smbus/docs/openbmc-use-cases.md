# OpenBMC I2C/SMBus Use Cases

I2C is a board-management fabric. Typical targets include temperature sensors,
FRU EEPROMs, fan controllers, GPIO expanders, power supplies, voltage
regulators, hot-swap controllers, clocks, security devices, and muxes.

## Sensor path

```text
Device Tree -> I2C client -> kernel hwmon
            -> phosphor-hwmon or dbus-sensors
            -> xyz.openbmc_project.Sensor.Value
            -> thermal/power policy, IPMI, Redfish
```

The kernel driver should expose standard subsystem semantics. OpenBMC
userspace should supply inventory names, thresholds, associations, and platform
policy. Avoid inventing a private sysfs ABI when hwmon, IIO, nvmem, regulator,
LED, GPIO, or another established subsystem fits.

## Case 1: Multiple identical sensors behind a mux

Two `0x48` sensors can coexist behind isolated PCA954x channels. During bring-up:

1. Confirm the upstream mux appears and binds.
2. Resolve channel-to-logical-adapter links in sysfs.
3. Access only the expected downstream channel.
4. Verify each sensor's physical location and OpenBMC inventory association.
5. Avoid treating runtime bus numbers as stable board identity.

A wrong channel mapping can publish valid temperatures under the wrong FRU
name, which is more dangerous than a visibly missing sensor.

## Case 2: PMBus power supplies

Redundant PSUs often expose PMBus telemetry and FRU data, possibly behind muxes.
Development must cover presence changes, hot removal, transient NACKs, PEC,
command compatibility, write protection, and failure isolation. Do not use
blind `i2cset` operations on a live power device.

OpenBMC configuration commonly associates an I2C bus and address with a power
supply connector. Userspace can then create inventory and sensor objects while
the Linux PMBus/hwmon driver supplies standardized measurements.

## Case 3: Shared host/BMC bus

When the BMC and host are both controllers:

- Define ownership for boot, runtime, firmware update, reset, and shutdown.
- Handle arbitration loss as a normal recoverable event when appropriate.
- Use bounded retry with backoff only for idempotent operations.
- Coordinate mux switching and recovery.
- Test one side resetting mid-transfer.
- Verify the BMC still provides essential management when the host is wedged.

Hardware multi-master support is necessary but insufficient. A platform may
instead use a GPIO semaphore, an I2C arbiter, a bus switch, or a mailbox-based
ownership protocol.

## Case 4: Dynamic FRU-based configuration

Entity Manager can represent replaceable hardware detected by a separate
detection service. Its `Exposes` records describe features that reactors such
as dbus-sensors use. Entity Manager does not replace the Linux bus controller
or directly manage the hardware.

Debug the stages independently:

```text
FRU readable?
 -> entity detected?
 -> configuration interface published?
 -> reactor created the sensor?
 -> hwmon/direct access succeeds?
 -> D-Bus object has correct value and association?
```

## Case 5: MCTP transport

Some BMC platforms carry management protocols such as MCTP over an I2C/SMBus
physical binding. In that case, ordinary register-tool assumptions may be
wrong: address ownership, endpoint discovery, message framing, and the kernel
networking/MCTP stack become part of the data path. Keep transport debugging
separate from device-register debugging.

## Development workflow

1. Turn the schematic into a bus/mux/address/power/reset table.
2. Verify controller pinmux, clock, reset, and kernel configuration.
3. Add/validate Device Tree nodes and binding schemas.
4. Prove minimal safe transfers with a logic analyzer attached.
5. Bind the kernel function driver and validate its standard subsystem ABI.
6. Integrate the driver and DTS through the OpenBMC Yocto machine/kernel layer.
7. Configure the sensor/inventory service and verify D-Bus.
8. Verify Redfish/IPMI and the policy consumer.
9. Inject NACK, target reset, mux error, bus stuck, stale data, and service restart.
10. Repeat across host/BMC reset and power-state transitions.

## Production review checklist

- Every address is unique on each physical segment.
- Mux idle/disconnect behavior is intentional.
- Safety-critical devices cannot be casually unbound or overwritten.
- Timeouts and retries are bounded and operation-aware.
- Errors propagate to health/fail-safe policy rather than becoming stale values.
- Sensor names and inventory associations identify physical location correctly.
- Kernel, DTS, userspace configuration, and Yocto revisions are reproducible.
- Debug logs provide controller, logical bus, physical path, address, and errno.

