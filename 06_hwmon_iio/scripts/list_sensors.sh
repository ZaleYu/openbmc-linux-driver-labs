#!/bin/sh
set -eu

echo "IIO devices:"
for p in /sys/bus/iio/devices/iio:device*; do
	[ -d "$p" ] || continue
	printf '  %s name=%s\\n' "$(basename "$p")" \
		"$(cat "$p/name" 2>/dev/null || echo unknown)"
done
echo "hwmon devices:"
for p in /sys/class/hwmon/hwmon*; do
	[ -d "$p" ] || continue
	printf '  %s name=%s device=%s\\n' "$(basename "$p")" \
		"$(cat "$p/name" 2>/dev/null || echo unknown)" \
		"$(readlink -f "$p/device" 2>/dev/null || echo unknown)"
done

