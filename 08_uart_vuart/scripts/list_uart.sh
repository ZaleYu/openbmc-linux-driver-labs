#!/bin/sh
set -eu

for p in /sys/class/tty/ttyS* /sys/class/tty/ttyVUART*; do
	[ -e "$p" ] || continue
	name=$(basename "$p")
	driver=unknown
	[ -L "$p/device/driver" ] &&
		driver=$(basename "$(readlink -f "$p/device/driver")")
	printf '%s driver=%s device=%s\n' "$name" "$driver" \
		"$(readlink -f "$p/device" 2>/dev/null || echo unknown)"
done
echo "Configured consoles:"
cat /proc/consoles 2>/dev/null || true
echo "Serial core state:"
cat /proc/tty/driver/serial 2>/dev/null || true

