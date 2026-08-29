#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 /dev/spidevB.C ./test_spi_sensor" >&2
	exit 2
fi
dev=$1
tester=$2
[ -c "$dev" ] || { echo "$dev is not a character device" >&2; exit 1; }
[ -x "$tester" ] || { echo "$tester is not executable" >&2; exit 1; }
echo "Reading only the documented fictional sensor registers from $dev"
exec "$tester" "$dev"

