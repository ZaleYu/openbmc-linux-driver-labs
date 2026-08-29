#!/bin/sh
set -u

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	echo "Usage: $0 DEVICE_NAME [SECONDS]" >&2
	exit 2
fi

device=$1
duration=${2:-0}
case "$device" in
	*/*|'') echo "DEVICE_NAME must be a sysfs basename" >&2; exit 2 ;;
esac
case "$duration" in
	''|*[!0-9]*) echo "SECONDS must be a non-negative integer" >&2; exit 2 ;;
esac

root="/sys/bus/i3c/devices/$device"
count_path="$root/ibi_count"
status_path="$root/last_ibi_status"
if [ ! -r "$count_path" ]; then
	echo "Missing readable demo attribute: $count_path" >&2
	exit 1
fi

echo "Monitoring $device IBI counters (Ctrl-C to stop)."
echo "This script reads sysfs only and generates no bus traffic."

elapsed=0
previous=
while [ "$duration" -eq 0 ] || [ "$elapsed" -lt "$duration" ]; do
	count=$(cat "$count_path")
	status=unavailable
	[ -r "$status_path" ] && status=$(cat "$status_path")
	if [ "$count" != "$previous" ]; then
		date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || true
		printf 'ibi_count=%s last_ibi_status=%s\n' "$count" "$status"
		previous=$count
	fi
	sleep 1
	elapsed=$((elapsed + 1))
done

