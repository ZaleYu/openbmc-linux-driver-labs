#!/bin/sh
set -eu

"$(dirname "$0")/list_sensors.sh"
echo "Modules:"
lsmod | grep -Ei 'iio|hwmon|adc' || true
echo "Recent kernel messages:"
dmesg 2>/dev/null | grep -Ei 'iio|adc|hwmon|sensor|timeout|overflow' |
	tail -n 100 || true
echo "OpenBMC sensor services:"
systemctl --no-pager --type=service 2>/dev/null |
	grep -Ei 'sensor|dbus' || true
echo "Clock summary:"
grep -Ei 'adc|iio' /sys/kernel/debug/clk/clk_summary 2>/dev/null || true

