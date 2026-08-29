#!/bin/sh
set -eu

usage() {
	echo "Usage: $0 BUS [--scan]" >&2
	echo "Lists adapters by default. --scan probes BUS and may not be safe for every device." >&2
}

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	usage
	exit 2
fi

bus=$1
mode=${2:-}

case "$bus" in
	''|*[!0-9]*) echo "BUS must be a non-negative integer" >&2; exit 2 ;;
esac

command -v i2cdetect >/dev/null 2>&1 || {
	echo "i2cdetect is not installed (package: i2c-tools)" >&2
	exit 1
}

echo "Available adapters:"
i2cdetect -l

echo
echo "Selected adapter metadata:"
for path in "/sys/class/i2c-adapter/i2c-$bus/name" \
	    "/sys/bus/i2c/devices/i2c-$bus/name"; do
	if [ -r "$path" ]; then
		printf '%s: ' "$path"
		cat "$path"
	fi
done

if [ "$mode" = "--scan" ]; then
	echo
	echo "WARNING: probing can disturb some devices. Scanning i2c-$bus in 3 seconds."
	sleep 3
	i2cdetect -y "$bus"
elif [ -n "$mode" ]; then
	usage
	exit 2
else
	echo
	echo "No probe performed. Re-run with --scan only after verifying the bus is safe."
fi

