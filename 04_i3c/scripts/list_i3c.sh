#!/bin/sh
set -u

root=/sys/bus/i3c/devices

echo "I3C bus inventory"
if [ ! -d "$root" ]; then
	echo "I3C sysfs bus not present: $root"
	echo "Check CONFIG_I3C and the controller driver."
	exit 0
fi

found=0
for device in "$root"/*; do
	[ -e "$device" ] || continue
	found=1
	echo
	echo "$(basename "$device")"
	for item in pid bcr dcr dynamic_address hdrcap modalias; do
		[ -r "$device/$item" ] || continue
		printf '  %-18s ' "$item:"
		tr -d '\n' < "$device/$item"
		printf '\n'
	done
	if [ -L "$device/driver" ]; then
		printf '  %-18s %s\n' "driver:" \
			"$(basename "$(readlink "$device/driver")")"
	else
		printf '  %-18s %s\n' "driver:" "unbound or controller object"
	fi
done

[ "$found" -eq 1 ] || echo "No I3C devices found."

echo
echo "Legacy I2C adapters exposed by I3C controllers"
found=0
for adapter in /sys/class/i2c-adapter/i2c-*; do
	[ -r "$adapter/name" ] || continue
	name=$(cat "$adapter/name")
	case "$name" in
		*[Ii]3[Cc]*)
			found=1
			printf '%s: %s\n' "$(basename "$adapter")" "$name"
			;;
	esac
done
[ "$found" -eq 1 ] || echo "No adapter name containing I3C found."

echo
echo "No bus transaction was issued."

