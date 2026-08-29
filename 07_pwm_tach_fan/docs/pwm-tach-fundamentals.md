# PWM and tachometer fundamentals

PWM controls average fan drive by switching a signal over a fixed period:

    duty percent = duty_time / period * 100
    frequency Hz = 1 / period_seconds

Four-wire server fans normally have power, ground, tach output and PWM input.
The PWM input often expects an open-collector/open-drain signal at a specified
frequency; confirm the fan datasheet. Three-wire fans may use voltage control
instead.

Tach is commonly an open-collector pulse signal and needs a pull-up. If the fan
generates P pulses per revolution:

    RPM = pulses_in_window * 60 / (P * window_seconds)

Two pulses per revolution is common but not universal. Wrong PPR produces an
exact scale error. Short sample windows respond quickly but quantize low RPM;
long windows are stable but detect stalls slowly.

## Practical behavior

- A fan may not start at the duty where it can continue spinning.
- Startup boost applies a higher duty for a limited time.
- Zero duty may mean stop, minimum speed, or undefined behavior.
- Mechanical inertia creates a delay between PWM change and RPM.
- Tach can remain noisy briefly after stop or power removal.
- Redundant fans influence airflow, so one failed fan may require all survivors
  to increase speed.

Separate open-loop duty control from closed-loop RPM control. A stable
closed-loop controller needs limits, anti-windup, slew control and fault logic.

