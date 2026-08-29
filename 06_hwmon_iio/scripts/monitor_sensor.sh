#!/bin/sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	echo "usage: $0 HWMON_ATTRIBUTE_PATH [INTERVAL_SECONDS]" >&2
	exit 2
fi
path=$1
interval=${2:-1}
[ -r "$path" ] || { echo "not readable: $path" >&2; exit 1; }
case $interval in
	*[!0-9]*|'') echo "interval must be an integer" >&2; exit 2 ;;
esac
while :; do
	value=$(cat "$path") || exit 1
	printf '%s %s %s\\n' "$(date -u +%FT%TZ)" "$path" "$value"
	sleep "$interval"
done

