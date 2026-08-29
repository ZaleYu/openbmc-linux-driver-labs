#!/bin/sh
set -eu

for h in /sys/class/hwmon/hwmon*; do
	[ -d "$h" ] || continue
	name=$(cat "$h/name" 2>/dev/null || echo unknown)
	found=0
	for f in "$h"/fan*_input "$h"/pwm[0-9]*; do
		[ -f "$f" ] && found=1
	done
	[ "$found" -eq 1 ] || continue
	printf '%s name=%s device=%s\n' "$(basename "$h")" "$name" \
		"$(readlink -f "$h/device" 2>/dev/null || echo unknown)"
	for f in "$h"/fan*_input "$h"/pwm[0-9]*; do
		[ -f "$f" ] || continue
		printf '  %s=%s\n' "$(basename "$f")" \
			"$(cat "$f" 2>/dev/null || echo unreadable)"
	done
done
