#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 /dev/ttyX OUTPUT_FILE" >&2
	exit 2
fi
dev=$1
output=$2
[ -c "$dev" ] || { echo "not a character device: $dev" >&2; exit 1; }
old=$(stty -F "$dev" -g)
restore()
{
	stty -F "$dev" "$old" 2>/dev/null || true
}
trap restore EXIT INT TERM
stty -F "$dev" 115200 cs8 -cstopb -parenb -crtscts \
	-ixon -ixoff raw -echo
echo "Capturing $dev to $output; press Ctrl-C to stop" >&2
dd if="$dev" of="$output" bs=256 status=none

