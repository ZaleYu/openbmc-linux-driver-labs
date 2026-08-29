# Demo PWM/tach fan driver

The driver requests one PWM and one tach GPIO, converts the GPIO to a rising
edge IRQ, counts pulses for one-second windows, and exports:

- `fan1_input` — calculated RPM.
- `fan1_min` — software policy value for userspace.
- `pwm1` — manual duty command from 0 to 255.

`fan1_min` does not create a fake hwmon alarm. A real latched controller alarm
may expose an alarm attribute; otherwise OpenBMC should compare RPM with policy.

The cleanup path cancels sampling and requests full duty. This is useful but not
a complete safety mechanism: hardware reset defaults, watchdogs and independent
thermal protection are still required.

Build with `make` and adapt the fictional DT binding. For a normal PWM fan,
compare this code with the in-tree `drivers/hwmon/pwm-fan.c` before creating a
custom driver.

