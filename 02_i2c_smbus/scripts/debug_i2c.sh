#!/bin/sh
set -u

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	echo "Usage: $0 BUS [ADDRESS]" >&2
	echo "ADDRESS example: 0x48. This script does not probe or write the bus." >&2
	exit 2
fi

bus=$1
address=${2:-}

case "$bus" in
	''|*[!0-9]*) echo "BUS must be a non-negative integer" >&2; exit 2 ;;
esac

echo "I2C diagnostic report"
date 2>/dev/null || true
uname -a

echo
echo "Adapters"
if command -v i2cdetect >/dev/null 2>&1; then
	i2cdetect -l
else
	echo "i2cdetect unavailable"
fi

echo
echo "Selected adapter"
for path in "/sys/class/i2c-adapter/i2c-$bus/name" \
	    "/sys/bus/i2c/devices/i2c-$bus/name"; do
	if [ -r "$path" ]; then
		printf '%s: ' "$path"
		cat "$path"
	fi
done

echo
echo "Devices on logical bus $bus"
for path in /sys/bus/i2c/devices/"$bus"-*; do
	[ -e "$path" ] || continue
	basename "$path"
done

if [ -n "$address" ]; then
	case "$address" in
		0x*|0X*) address_value=$((address)) ;;
		*) address_value=$address ;;
	esac
	device=$(printf '%d-%04x' "$bus" "$address_value" 2>/dev/null) || device=""

	echo
	echo "Requested client: $device"
	if [ -n "$device" ] && [ -d "/sys/bus/i2c/devices/$device" ]; then
		for item in name modalias uevent; do
			path="/sys/bus/i2c/devices/$device/$item"
			if [ -r "$path" ]; then
				echo "$item:"
				cat "$path"
			fi
		done
		if [ -L "/sys/bus/i2c/devices/$device/driver" ]; then
			printf 'driver: '
			basename "$(readlink "/sys/bus/i2c/devices/$device/driver")"
		else
			echo "driver: unbound"
		fi
	else
		echo "Client is not instantiated in sysfs."
	fi
fi

echo
echo "Mux channel links"
find /sys/bus/i2c/devices -maxdepth 2 -type l -name 'channel-*' -print 2>/dev/null || true

echo
echo "Recent kernel messages"
dmesg 2>/dev/null | grep -Ei 'i2c|smbus|timeout|arbitr|nack|demo_i2c' | tail -n 100 || \
	echo "Kernel log unavailable or no matching messages"

echo
echo "No I2C transaction was issued by this script."

