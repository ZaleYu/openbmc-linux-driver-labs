# Linux PWM, Tachometer and Fan Control for OpenBMC

This lab follows a fan-control path from PWM waveform and tach pulses to Linux
hwmon, the thermal framework, and OpenBMC fan policy.

## Contents

- `docs/` — fundamentals, Linux architecture, Device Tree, tools, debugging,
  and OpenBMC use cases.
- `client-driver/` — educational PWM consumer plus tach IRQ hwmon driver.
- `device-tree/` — custom lab, in-tree `pwm-fan`, and thermal cooling examples.
- `userspace/` — fan monitor and controlled PWM sweep programs.
- `scripts/` — inventory, diagnosis, and full-speed fail-safe helper.

## Data path

    temperature sensors -> D-Bus/thermal policy -> target PWM
                                                -> PWM provider -> fan
    fan tach output -> GPIO/IRQ or counter -> RPM -> hwmon -> D-Bus

Use the in-tree `pwm-fan` driver when it fits the hardware. The fictional
`openai,demo-pwm-tach-fan` driver exists to teach the APIs and is not a
production binding.

## Safety

A stopped fan can damage hardware. Keep a firmware/hardware full-speed default,
validate minimum start duty, add watchdog/fail-safe behavior, and never run the
sweep tool on a production server without thermal supervision.

## References

- Linux `Documentation/driver-api/pwm.rst`
- Linux `Documentation/hwmon/sysfs-interface.rst`
- Devicetree `hwmon/pwm-fan.yaml` and `hwmon/fan-common.yaml`

