# Debugging hwmon and IIO

## Decision path

1. Check power, reference voltage, divider, pinmux and schematic.
2. Confirm controller probe and the expected IIO device/channel.
3. Compare raw codes with the ADC input measured by a multimeter.
4. Verify scale, offset, sign, resolution, endianness and divider math.
5. Confirm `iio-hwmon` consumed the intended channels.
6. Verify hwmon names/units, then the OpenBMC service and D-Bus object.

Useful commands:

    dmesg | grep -Ei 'iio|adc|hwmon|sensor|timeout|overflow'
    find /sys/bus/iio/devices -maxdepth 2 -type f | sort
    find /sys/class/hwmon -maxdepth 2 -type f | sort
    udevadm info /sys/bus/iio/devices/iio:device0

Common faults:

- Full-scale/zero: open input, missing reference, reset, pinmux or saturation.
- Fixed plausible value: stale conversion or wrong channel mux.
- Factor-of-1000: IIO/hwmon unit conversion.
- Wrong rail: divider not modeled or wrong resistor ratio.
- Negative becomes huge: sign-extension bug.
- Spikes: settling, grounding, source impedance, crosstalk or no averaging.
- No hwmon node: missing `iio-hwmon`, deferred probe or phandle error.
- D-Bus stale: stopped service, changed ownership or wedged publisher.

Fault injection should cover stuck, saturated, noisy and unavailable values.
The policy layer needs hysteresis and fail-safe handling.

