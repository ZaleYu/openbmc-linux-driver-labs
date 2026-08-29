#!/bin/sh
set -u

echo "I2C adapters"
if command -v i2cdetect >/dev/null 2>&1; then
	i2cdetect -l
else
	for path in /sys/class/i2c-adapter/i2c-*; do
		[ -e "$path" ] || continue
		printf '%s' "$(basename "$path")"
		[ -r "$path/name" ] && printf ': %s' "$(cat "$path/name")"
		printf '\n'
	done
fi

echo
echo "Mux channel mappings"
found=0
for link in /sys/bus/i2c/devices/*/channel-*; do
	[ -L "$link" ] || continue
	found=1
	target=$(readlink -f "$link" 2>/dev/null || readlink "$link")
	printf '%s -> %s\n' "$link" "$target"
done
[ "$found" -eq 1 ] || echo "No channel-* links found."

echo
echo "Child adapters with mux_device backlinks"
found=0
for link in /sys/bus/i2c/devices/i2c-*/mux_device; do
	[ -L "$link" ] || continue
	found=1
	target=$(readlink -f "$link" 2>/dev/null || readlink "$link")
	printf '%s -> %s\n' "$(dirname "$link")" "$target"
done
[ "$found" -eq 1 ] || echo "No mux_device links found."

echo
echo "No I2C transaction was issued."

