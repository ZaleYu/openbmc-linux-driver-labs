#!/bin/sh
set -eu

echo "PECI bus devices:"
if [ -d /sys/bus/peci/devices ]; then
	for device in /sys/bus/peci/devices/*; do
		[ -e "$device" ] || continue
		driver=unbound
		[ -L "$device/driver" ] &&
			driver=$(basename "$(readlink -f "$device/driver")")
		printf '%s driver=%s path=%s\n' "$(basename "$device")" \
			"$driver" "$(readlink -f "$device")"
	done
else
	echo "PECI bus is not registered."
fi

echo "PECI hwmon devices:"
found=0
for hwmon in /sys/class/hwmon/hwmon*; do
	[ -r "$hwmon/name" ] || continue
	name=$(cat "$hwmon/name")
	case "$name" in
		*peci*) found=1; printf '%s name=%s device=%s\n' \
			"$(basename "$hwmon")" "$name" \
			"$(readlink -f "$hwmon/device" 2>/dev/null || echo unknown)" ;;
	esac
done
[ "$found" -eq 1 ] || echo "No PECI hwmon device found."

