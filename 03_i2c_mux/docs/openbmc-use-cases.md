# OpenBMC Use Cases

## Repeated-address field-replaceable units

Two or more PSUs often expose the same PMBus address. Place each behind a
separate channel and describe each endpoint under its channel bus. OpenBMC can
then create distinct hwmon/D-Bus objects tied to physical slots. The inventory
mapping must use topology/location, not only address `0x58`.

The same pattern applies to fan modules, batteries, backplanes, FRU EEPROMs,
and duplicated temperature sensors.

## Large sensor trees

A BMC may use one root controller, a board-level mux, and secondary muxes on
hot-plug modules. Keep a machine-readable mapping such as:

```text
root i2c5 -> mux 0x70 ch2 -> mux 0x71 ch1 -> PSU bay 3
```

At boot, validate the expected sysfs topology before starting monitoring
services. Dynamic bus numbers must not become physical identities. Use stable
configuration fields or resolve sysfs links.

## Fault isolation and availability

Idle-disconnect can keep a shorted, powered-off, or hot-plug branch from loading
other segments. Whether it works during a hard SDA/SCL short depends on mux and
board circuitry. Recovery may require controller bus recovery, mux reset, load
switch control, or service action. Design this path before software integration.

Rate-limit retries. A failed branch should not starve healthy PSU or thermal
telemetry sharing the parent. Expose persistent topology faults with useful
physical location and avoid alarm storms.

## Multi-master host/BMC systems

Potential architectures are:

- hardware arbitration and shared mux state with a defined protocol;
- an ownership GPIO/semaphore granted before selector plus endpoint transfer;
- channels partitioned so each master never changes the other's route;
- a BMC-only management fabric with host access through a mailbox/API.

Linux's I2C mux locks do not coordinate with another processor. Recovery and
timeout paths must release ownership. Include firmware update, host reset, BMC
reset, and surprise removal in validation.

## Development sequence

1. Build a schematic-to-DT table of root, mux address, channel, endpoint, IRQ,
   power/reset, voltage, and physical location.
2. Validate each segment electrically at conservative speed.
3. Bind the mux driver and verify every `channel-*` link.
4. Test known endpoint transactions on one logical child at a time.
5. Bind endpoint subsystem drivers and check stable OpenBMC inventory mapping.
6. Add concurrency, hot-plug, reset, stuck-bus, and external-master tests.
7. Test service restart and full BMC reboot without relying on old bus numbers.

## Production review

- Prefer an upstream mux driver and binding over a board-specific clone.
- Keep selector ownership in the kernel.
- Confirm bootloader and kernel agree on reset/idle state.
- Audit writes to power, VR, EEPROM, and security devices.
- Document topology in logs and service procedures.
- Test negative paths and recovery, not only sensor reads on a healthy bench.

