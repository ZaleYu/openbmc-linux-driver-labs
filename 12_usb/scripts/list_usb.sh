#!/bin/sh
set -eu

echo "USB host topology"
if command -v lsusb >/dev/null 2>&1; then
	lsusb
	printf '\n'
	lsusb -t
else
	echo "lsusb is not installed (package: usbutils)"
fi

printf '\nUSB sysfs devices\n'
found=0
for node in /sys/bus/usb/devices/*; do
	[ -f "$node/idVendor" ] || continue
	found=1
	vid=$(cat "$node/idVendor")
	pid=$(cat "$node/idProduct")
	product=$(cat "$node/product" 2>/dev/null || echo "-")
	speed=$(cat "$node/speed" 2>/dev/null || echo "-")
	printf '%s %s:%s speed=%s product=%s\n' "${node##*/}" "$vid" "$pid" "$speed" "$product"
done
[ "$found" -eq 1 ] || echo "No enumerated USB devices found."

printf '\nUSB device controllers (gadget role)\n'
if [ -d /sys/class/udc ]; then
	set -- /sys/class/udc/*
	if [ -e "$1" ]; then
		for udc in "$@"; do
			state=$(cat "$udc/state" 2>/dev/null || echo "unknown")
			printf '%s state=%s\n' "${udc##*/}" "$state"
		done
	else
		echo "No UDC registered."
	fi
else
	echo "/sys/class/udc is unavailable."
fi
