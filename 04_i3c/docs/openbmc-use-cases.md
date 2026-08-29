# OpenBMC Use Cases

## MCTP over I3C

I3C can carry MCTP between a BMC and management endpoints with IBI-based receive
notification. Platform support depends on the OpenBMC kernel branch,
controller, endpoint firmware, and userspace MCTP stack. Validate endpoint
identity, EID assignment/routing, packet size, IBI flow control, reset recovery,
and bus ownership as one system—not independent drivers.

## High-density telemetry

IBI can reduce separate alert GPIOs for temperature, power, voltage-regulator,
fan-module, and board-health devices. Bind each target to the appropriate Linux
subsystem, then let OpenBMC sensor services publish D-Bus objects for policy,
Redfish, and logging. PID plus physical location should map the runtime device
to inventory; dynamic address alone is insufficient.

## I3C hubs and segmented systems

Hubs extend topology, isolate branches, and may expose legacy SMBus agents.
Document every upstream port, target port, PID, power domain, reset, and
physical slot. A failed/hot-plug branch must not starve thermal or power
telemetry elsewhere. Upstream support for a specific hub can differ markedly
between mainline and vendor/OpenBMC trees.

## Hot-plug modules

A drive backplane, accelerator tray, or PSU module may power up after BMC boot.
Use Hot-Join and DAA where hardware/firmware support it; otherwise define a
controlled rediscovery mechanism. Services must tolerate late appearance,
disappearance, a changed dynamic address, and duplicate/replaced hardware PID
instance fields.

## Multiple controllers

In a host/BMC design, controller-role handoff must define:

- which controller initializes the bus and assigns addresses;
- when the host may request ownership and how the BMC grants/refuses it;
- how both sides learn the target table and pending IBIs;
- what happens during host reset, BMC reboot, update, or ownership timeout;
- which side owns MCTP routing and recovery.

Linux locks coordinate one kernel, not another processor. Test cross-reset and
fault paths before enabling automatic role handoff.

## Development sequence

1. Build a schematic-to-software table: controller, PID, static/preferred
   address, BCR/DCR, bus mode, IBI, power/reset, and physical location.
2. Bring up a pure I3C bus at conservative settings, then add legacy I2C.
3. Verify DAA and sysfs identity before loading target drivers.
4. Validate private transfers and IBI under load and injected errors.
5. Integrate hwmon/IIO/MCTP with D-Bus inventory and policy services.
6. Test Hot-Join, target replacement, controller reset, stuck lines, queue
   exhaustion, service restart, and full firmware update.
7. Preserve logs and topology mapping for field diagnostics.

## Production review

- Prefer upstream controller/target/hub drivers and binding schemas.
- Pin and document vendor patches when upstream interfaces are insufficient.
- Rate-limit retries and IBIs; expose persistent physical-location faults.
- Protect management traffic and firmware-update commands with least privilege.
- Ensure bootloader and kernel agree on bus reset/address state.
- Never make a dynamic address a permanent OpenBMC inventory identifier.

