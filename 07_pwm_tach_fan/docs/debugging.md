# Debugging

## Decision path

1. Verify fan power, ground, connector pinout and pull-ups.
2. Measure PWM frequency, polarity and duty at the fan pin.
3. Measure tach edges and confirm pulses per revolution.
4. Inspect PWM provider/consumer binding, IRQ count and hwmon files.
5. Compare measured RPM with an optical tachometer when available.
6. Verify D-Bus sensor, zone mapping, control owner and fail-safe.

Useful checks:

    dmesg | grep -Ei 'pwm|fan|tach|thermal|irq|timeout'
    cat /proc/interrupts
    cat /sys/kernel/debug/pwm 2>/dev/null
    grep -Ei 'pwm|fan' /sys/kernel/debug/clk/clk_summary 2>/dev/null
    systemctl --no-pager --type=service | grep -Ei 'fan|sensor|thermal'

Common faults:

- PWM changes but RPM does not: wrong pinmux/CS, polarity, frequency, power or
  minimum start duty.
- RPM is exactly half/double: wrong PPR or edge counting.
- RPM is zero: missing pull-up, wrong GPIO/IRQ edge, fan stopped or no power.
- RPM jumps: bounce/noise, too-short window, integer overflow or stale edges.
- Fan pulses between speeds: competing control owners or unstable policy.
- Fans remain slow after service crash: missing fail-safe/watchdog.
- One fan failure overheats a zone: wrong inventory/zone/redundancy mapping.

Test stuck tach, disconnected fan, PWM write failure, service death, BMC reboot,
sensor loss and high-temperature emergency. Hardware thermal shutdown remains
the final protection layer.

