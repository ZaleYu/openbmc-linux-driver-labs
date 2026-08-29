#!/bin/sh
set -u

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
	echo "Usage: $0 MUX_DEVICE CHANNEL [--scan]" >&2
	echo "Example: $0 5-0070 2" >&2
	exit 2
fi

mux_device=$1
channel=$2
mode=${3:-}

case "$mux_device" in
	*[!0-9a-fA-F-]*|'') echo "Invalid MUX_DEVICE" >&2; exit 2 ;;
esac
case "$channel" in
	''|*[!0-9]*) echo "CHANNEL must be a non-negative integer" >&2; exit 2 ;;
esac
if [ -n "$mode" ] && [ "$mode" != "--scan" ]; then
	echo "Third argument must be --scan" >&2
	exit 2
fi

link="/sys/bus/i2c/devices/$mux_device/channel-$channel"
if [ ! -L "$link" ]; then
	echo "Channel link not found: $link" >&2
	exit 1
fi

target=$(readlink -f "$link" 2>/dev/null || readlink "$link")
base=$(basename "$target")
bus=${base#i2c-}
case "$bus" in
	''|*[!0-9]*) echo "Unexpected channel target: $target" >&2; exit 1 ;;
esac

echo "$mux_device channel $channel -> $target -> /dev/i2c-$bus"

if [ "$mode" != "--scan" ]; then
	echo "No scan requested. Add --scan only after checking endpoint safety."
	exit 0
fi
if ! command -v i2cdetect >/dev/null 2>&1; then
	echo "i2cdetect is not installed" >&2
	exit 1
fi

echo "WARNING: probing can disturb some I2C devices; starting scan in 3 seconds." >&2
sleep 3
exec i2cdetect -y "$bus"

