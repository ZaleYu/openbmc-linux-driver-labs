# Linux architecture

    PWM controller hardware
        -> PWM provider driver / struct pwm_chip
        -> PWM core
        -> consumer: pwm-fan or platform-specific fan driver
        -> hwmon and optional thermal cooling device

    tach GPIO/counter/capture hardware
        -> IRQ/counter/controller driver
        -> pulse period or pulse count
        -> RPM through hwmon fanX_input

PWM consumers acquire a descriptor with `devm_pwm_get()`, read the reference
configuration, then apply one coherent `struct pwm_state` with
`pwm_apply_might_sleep()`. Do not split a state change across legacy
`pwm_config()` and `pwm_enable()` calls.

`pwm_get_state()` returns the last requested state, not necessarily the exact
waveform generated after hardware rounding. Measure the pin when frequency and
duty accuracy matter.

Hwmon standardizes `pwm1` as 0..255 and `fan1_input` as RPM. A driver should
only expose features implemented by hardware/software. Hwmon alarms represent
device indications; this demo does not invent a software alarm from
`fan1_min`.

Read these kernel areas:

- `drivers/pwm/` — SoC/controller providers.
- `drivers/hwmon/pwm-fan.c` — generic consumer, tach, cooling device.
- `drivers/hwmon/` — dedicated fan controllers and PMBus devices.
- `drivers/thermal/` — zones, governors and cooling devices.
- `drivers/counter/` — hardware counter/capture alternatives.

For a new board, change DT mapping and policy first. Modify a PWM provider only
for new controller hardware, clocking, polarity or register/errata support.

