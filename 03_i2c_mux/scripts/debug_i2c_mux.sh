#!/bin/sh
set -u

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
	echo "Usage: $0 PARENT_BUS MUX_ADDRESS [CHANNEL]" >&2
	echo "Example: $0 5 0x70 2" >&2
	exit 2
fi

parent=$1
address=$2
channel=${3:-}

case "$parent" in
	''|*[!0-9]*) echo "PARENT_BUS must be a non-negative integer" >&2; exit 2 ;;
esac
case "$channel" in
	'') ;;
	*[!0-9]*) echo "CHANNEL must be a non-negative integer" >&2; exit 2 ;;
esac
case "$address" in
	0x*|0X*) address_value=$((address)) ;;
	*[!0-9]*) echo "MUX_ADDRESS must be decimal or 0x-prefixed" >&2; exit 2 ;;
	*) address_value=$address ;;
esac
if [ "$address_value" -lt 0 ] || [ "$address_value" -gt 127 ]; then
	echo "MUX_ADDRESS must be a 7-bit address" >&2
	exit 2
fi

device=$(printf '%d-%04x' "$parent" "$address_value")
root="/sys/bus/i2c/devices/$device"

echo "I2C mux diagnostic report"
date 2>/dev/null || true
uname -a

echo
echo "Parent adapter"
for path in "/sys/bus/i2c/devices/i2c-$parent/name" \
	    "/sys/class/i2c-adapter/i2c-$parent/name"; do
	[ -r "$path" ] || continue
	printf '%s: ' "$path"
	cat "$path"
done

echo
echo "Mux client $device"
if [ ! -d "$root" ]; then
	echo "Not instantiated in sysfs: $root"
else
	for item in name modalias uevent; do
		[ -r "$root/$item" ] || continue
		echo "$item:"
		cat "$root/$item"
	done
	if [ -L "$root/driver" ]; then
		printf 'driver: '
		basename "$(readlink "$root/driver")"
	else
		echo "driver: unbound"
	fi
fi

echo
echo "Channel adapters"
found=0
for link in "$root"/channel-*; do
	[ -L "$link" ] || continue
	found=1
	target=$(readlink -f "$link" 2>/dev/null || readlink "$link")
	printf '%s -> %s' "$(basename "$link")" "$target"
	[ -r "$target/name" ] && printf ' (%s)' "$(cat "$target/name")"
	printf '\n'
done
[ "$found" -eq 1 ] || echo "No channel links found."

if [ -n "$channel" ]; then
	echo
	echo "Requested channel $channel"
	link="$root/channel-$channel"
	if [ -L "$link" ]; then
		target=$(readlink -f "$link" 2>/dev/null || readlink "$link")
		bus=${target##*/i2c-}
		echo "logical adapter: $target"
		echo "clients:"
		for client in /sys/bus/i2c/devices/"$bus"-*; do
			[ -e "$client" ] || continue
			basename "$client"
		done
	else
		echo "Missing link: $link"
	fi
fi

echo
echo "Recent relevant kernel messages"
dmesg 2>/dev/null | grep -Ei 'i2c|smbus|mux|timeout|arbitr|nack' | tail -n 120 || \
	echo "Kernel log unavailable or no matching messages."

echo
echo "No I2C transaction was issued by this script."

