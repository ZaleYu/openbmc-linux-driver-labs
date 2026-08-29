#!/bin/sh
set -eu

found=0
for device in /dev/ipmi-kcs*; do
	[ -e "$device" ] || continue
	found=1
	name=$(basename "$device")
	driver=unknown
	[ -L "/sys/class/misc/$name/device/driver" ] &&
		driver=$(basename "$(readlink -f "/sys/class/misc/$name/device/driver")")
	printf '%s driver=%s sysfs=%s\n' "$device" "$driver" \
		"$(readlink -f "/sys/class/misc/$name/device" 2>/dev/null || echo unknown)"
done
[ "$found" -eq 1 ] || echo "No /dev/ipmi-kcs* device found."

echo "Platform devices mentioning LPC/eSPI/KCS:"
for device in /sys/bus/platform/devices/*; do
	[ -e "$device" ] || continue
	case "$(basename "$device")" in
		*lpc*|*espi*|*kcs*) printf '%s\n' "$(basename "$device")" ;;
	esac
done

