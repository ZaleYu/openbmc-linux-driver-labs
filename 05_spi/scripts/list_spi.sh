#!/bin/sh
set -eu

echo "SPI controllers:"
for p in /sys/class/spi_master/spi*; do
	[ -e "$p" ] || continue
	printf '  %s -> %s\n' "$(basename "$p")" "$(readlink -f "$p")"
done
echo "SPI devices:"
for p in /sys/bus/spi/devices/spi*.*; do
	[ -e "$p" ] || continue
	driver="unbound"
	[ -L "$p/driver" ] && driver=$(basename "$(readlink -f "$p/driver")")
	modalias=$(cat "$p/modalias" 2>/dev/null || echo unknown)
	printf '  %s driver=%s modalias=%s\n' "$(basename "$p")" "$driver" "$modalias"
done

