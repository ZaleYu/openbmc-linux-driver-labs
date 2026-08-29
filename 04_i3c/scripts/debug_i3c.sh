#!/bin/sh
set -u

if [ "$#" -gt 1 ]; then
	echo "Usage: $0 [DEVICE_NAME]" >&2
	echo "DEVICE_NAME is a basename under /sys/bus/i3c/devices." >&2
	exit 2
fi

requested=${1:-}
root=/sys/bus/i3c/devices

echo "I3C diagnostic report"
date 2>/dev/null || true
uname -a

echo
echo "Kernel configuration"
config="/boot/config-$(uname -r)"
if [ -r "$config" ]; then
	grep -E '^CONFIG_(I3C|I3C_|MCTP)' "$config" || true
elif [ -r /proc/config.gz ] && command -v zcat >/dev/null 2>&1; then
	zcat /proc/config.gz | grep -E '^CONFIG_(I3C|I3C_|MCTP)' || true
else
	echo "Kernel configuration unavailable."
fi

echo
echo "Loaded I3C-related modules"
if [ -r /proc/modules ]; then
	grep -Ei 'i3c|mctp' /proc/modules || echo "No matching modules listed."
fi

echo
echo "I3C sysfs devices"
if [ ! -d "$root" ]; then
	echo "$root is absent."
else
	for device in "$root"/*; do
		[ -e "$device" ] || continue
		if [ -n "$requested" ] && [ "$(basename "$device")" != "$requested" ]; then
			continue
		fi
		echo
		echo "[$(basename "$device")]"
		for item in pid bcr dcr dynamic_address hdrcap modalias ibi_count \
			    last_ibi_status uevent; do
			[ -r "$device/$item" ] || continue
			echo "$item:"
			cat "$device/$item"
		done
		if [ -L "$device/driver" ]; then
			printf 'driver: '
			basename "$(readlink "$device/driver")"
		else
			echo "driver: unbound or not a target object"
		fi
	done
fi

echo
echo "Interrupt counters mentioning I3C"
grep -Ei 'i3c' /proc/interrupts 2>/dev/null || echo "No matching IRQ label."

echo
echo "Recent relevant kernel messages"
dmesg 2>/dev/null | grep -Ei 'i3c|entdaa|rstdaa|hot.?join|ibi|mctp|arbitr|timeout' | \
	tail -n 160 || echo "Kernel log unavailable or no matching messages."

echo
echo "No I3C transaction was issued by this script."

