#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
	echo "Usage: $0 BUS ADDRESS" >&2
	echo "Example: $0 1 0x48" >&2
	exit 2
fi

bus=$1
address=$2

case "$bus" in
	''|*[!0-9]*) echo "BUS must be a non-negative integer" >&2; exit 2 ;;
esac

command -v i2cget >/dev/null 2>&1 || {
	echo "i2cget is not installed (package: i2c-tools)" >&2
	exit 1
}

echo "Reading only the fictional demo register range 0x00..0x05."
echo "Do not use this script on a real device until its read semantics are verified."

reg=0
while [ "$reg" -le 5 ]; do
	hex=$(printf '0x%02x' "$reg")
	value=$(i2cget -y "$bus" "$address" "$hex" b)
	printf '%s: %s\n' "$hex" "$value"
	reg=$((reg + 1))
done

