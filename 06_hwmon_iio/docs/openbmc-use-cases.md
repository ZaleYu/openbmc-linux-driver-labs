# OpenBMC use cases

BMC ADCs monitor core rails, standby power, batteries, thermistors and analog
fault outputs. A divider/rescale stage produces real rail voltage; a thermal
conversion produces temperature. Selected values can flow through iio-hwmon:

    physical signal -> ADC/IIO -> rescale -> hwmon -> dbus-sensors
                    -> D-Bus -> Redfish/IPMI -> health/thermal policy

PMBus power supplies and common I2C temperature/fan chips normally already have
native hwmon drivers. Use them instead of routing every sensor through IIO.

In multi-host and hot-plug systems, associate each reading with stable
inventory and power state. Removal, mux isolation and unpowered sensors must not
appear as valid zero. Avoid duplicate D-Bus objects after service restart or
configuration reload.

Decide whether thresholds live in hardware, kernel hwmon, the sensor service or
policy. Hardware alarms provide fast independent protection; userspace adds
inventory context and coordinated action. Avoid conflicting writers and define
hysteresis, severity and unavailable/stale behavior.

Production checks:

- Calibrate across temperature and board revisions.
- Verify units and labels end to end.
- Define polling rate and CPU/bus cost.
- Rate-limit logs and D-Bus changes.
- Test missing, saturated, noisy and stuck sensors.
- Compare Redfish/IPMI, hwmon and physical instruments.

