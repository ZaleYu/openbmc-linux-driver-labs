# OpenBMC use cases

## Closed-loop fan control

    temperature sensors -> D-Bus
        -> zone/controller policy -> requested PWM
        -> hwmon PWM -> fan
        -> tach RPM -> hwmon -> D-Bus -> fault/redundancy policy

OpenBMC commonly separates sensor publication, fan presence/monitoring, zone
control and inventory. Only one service should own each PWM output.

## Zones and redundancy

Map CPUs, GPUs, DIMMs, drives and PSUs to airflow zones. A zone may combine
multiple temperature inputs and several fans. The highest thermal demand often
wins. On a failed or absent fan, raise surviving fans, log the inventory
association and possibly throttle or shut down the host.

## Fail-safe design

- Hardware reset default: full or known-safe speed.
- Service startup: set safe duty before loading configuration.
- Missing/stale temperature: full speed, not zero.
- Missing tach at nonzero duty: retry/start boost, then fault.
- D-Bus owner loss or watchdog expiry: full speed.
- BMC update/reboot: define fan behavior throughout the gap.

## Dedicated controllers

Server boards may use SoC PWM/tach blocks, I2C fan controllers, CPLDs, FPGA
counters or PMBus devices. Prefer an existing kernel hwmon driver and standard
attributes. Keep board policy in OpenBMC rather than embedding every thermal
curve in the low-level driver unless hardware safety requires it.

Validate RPM tolerance, acoustic limits, thermal chamber behavior, fan swaps,
blocked airflow, single-fan failure, controller reset and all relevant host
power states.

