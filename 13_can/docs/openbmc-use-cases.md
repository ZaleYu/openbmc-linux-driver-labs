# OpenBMC use cases

CAN is less universal in servers than I2C, PMBus, MCTP, or PECI, but it appears
in specialized systems where robust multi-drop communication and long cable
runs matter.

## Power shelf or battery system

A BMC or rack manager may collect voltage, current, temperature, alarm, and
contactor state from a power shelf/BMS. A userspace SocketCAN service validates
the application protocol, publishes D-Bus telemetry, and maps persistent faults
to chassis health. Safety-critical commands require explicit authorization,
state checks, freshness, and fail-safe behavior.

## Multi-node chassis

Fan trays, liquid-cooling units, power drawers, or modular enclosures may use a
private CAN protocol. Define unique message ownership, priority, heartbeat,
firmware compatibility, node identity, hot-plug behavior, and bandwidth budget.
CAN arbitration alone does not prevent two misconfigured nodes using one ID.

## CAN gateway

An OpenBMC service can bridge selected telemetry to D-Bus/Redfish or another
transport. Do not transparently bridge all frames. Use allowlists, rate limits,
directional policy, protocol validation, audit logs, and loop prevention.
Remote management authentication must not be confused with CAN-bus trust.

## Redundant or dual buses

Two controllers may serve independent A/B networks. Interface names can change
with probe order, so map physical identity from DT/udev rather than assuming
`can0` means channel A. Keep health, restart, and ownership separate per bus.

## Service architecture

```text
SocketCAN RX -> schema/range/freshness validation -> internal state
             -> D-Bus inventory/sensor/health -> Redfish or policy

Authorized command -> interlocks/rate limit -> CAN frame -> peer response
                   -> timeout/retry policy -> audit and health
```

Systemd should order the protocol service after interface setup, restart it
without creating duplicate cyclic transmitters, and stop transmission cleanly.
Yocto integration commonly adds kernel CAN options, iproute2/can-utils for debug
images, the service package, configuration, udev naming, and systemd units.

## Production checklist

- Document every ID, payload, unit, range, byte order, sender, and period.
- Define timeout, stale data, reboot, version mismatch, and duplicate-node policy.
- Separate monitoring from commands and restrict command-capable identities.
- Persist useful bus-off/restart/error counters without flooding storage.
- Validate startup ordering when transceiver enable GPIOs are shared.
- Test degraded operation under overload and a noisy or partitioned bus.
