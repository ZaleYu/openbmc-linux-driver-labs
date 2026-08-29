# Userspace tools

Inventory:

    find /sys/class/pwm -maxdepth 3 -type f -print
    for h in /sys/class/hwmon/hwmon*; do cat "$h/name"; done
    sensors
    find /sys/class/thermal -maxdepth 2 -type f -print

Hwmon fan attributes commonly include:

- `fan1_input` — RPM.
- `fan1_min` / `fan1_max` — configured limits.
- `fan1_pulses` — pulses per revolution when supported.
- `pwm1` — 0..255 duty command.
- `pwm1_enable` — control method when supported; values are driver-specific
  within the ABI definition.

Do not assume `hwmonX` numbering is stable. Find the device by `name`, label,
physical device path or inventory mapping.

The generic PWM sysfs interface under `/sys/class/pwm` is useful for bring-up,
but a channel already requested by a kernel consumer cannot also be exported.
Production control should use the owning hwmon/thermal/OpenBMC interface.

Build the included tools:

    cc -O2 -Wall -Wextra -Werror -o monitor_fan monitor_fan.c
    cc -O2 -Wall -Wextra -Werror -o fan_sweep fan_sweep.c

`fan_sweep` requires a maximum-temperature guard and restores full speed on
normal exit or signal. It still cannot protect against process kill, kernel
failure or power events; use only on a supervised lab system.

