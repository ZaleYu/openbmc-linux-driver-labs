# OpenBMC use cases

LIN is uncommon in conventional servers. It is relevant mainly when an OpenBMC
platform inherits automotive/industrial subsystems or uses low-cost local
actuator/sensor networks.

## Actuator or cooling subnetwork

A specialized chassis could use LIN for pumps, valves, louvers, small fans, or
temperature modules. The master schedule collects status and publishes selected
values to D-Bus. Commands require interlocks, bounds, rate limiting, response
confirmation, and a fail-safe state when schedule or communication is lost.

## Battery or power module bridge

A gateway MCU may terminate a deterministic LIN network and expose a safer IPC
protocol to the BMC. This often isolates Linux scheduling from hard slot timing.
The BMC validates freshness and range before mapping data to health or Redfish.

## Manufacturing and diagnostics

LIN diagnostic frames may configure or test modules during manufacturing.
Restrict this capability in production images: diagnostic services can change
node identity, configuration, sleep state, or actuator behavior.

## Recommended architecture

```text
LIN controller/MCU -> validated transport -> OpenBMC LIN service
                   -> D-Bus sensor/inventory/health -> policy/Redfish
```

If Linux directly masters the bus, a single service must own the schedule.
Multiple processes must not independently send headers through one UART.
systemd ordering should bind transceiver power, controller availability,
schedule startup, D-Bus publication, and clean shutdown.

## Production requirements

- Version-control the LDF/signal database and generated artifacts.
- Define ID, publisher, length, checksum model, period, units, scaling, range.
- Track last-valid timestamp and expose stale/timeout status.
- Separate monitoring from control and authorize control paths.
- Handle node reset, sleep/wakeup, bootloader/diagnostic mode, and version skew.
- Budget schedule bandwidth and worst-case response time.
- Test physical faults, duplicate publishers, delayed slots, and service restart.
- Prefer a dedicated controller or MCU when Linux latency cannot meet timing.

Do not present this package as evidence that LIN is a standard OpenBMC transport.
It is a focused study of how a nonstandard board network can be integrated
cleanly into Linux and OpenBMC.
