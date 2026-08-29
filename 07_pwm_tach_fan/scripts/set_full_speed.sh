#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
	echo "usage: $0 /sys/class/hwmon/hwmonX" >&2
	exit 2
fi
hwmon=$1
[ -r "$hwmon/name" ] || { echo "invalid hwmon directory" >&2; exit 1; }
found=0
for pwm in "$hwmon"/pwm[0-9]*; do
	case $pwm in *_enable|*_freq|*_mode|*_auto*) continue ;; esac
	[ -f "$pwm" ] || continue
	printf '255\n' > "$pwm"
	printf 'set %s (%s) to 255\n' "$pwm" "$(cat "$hwmon/name")"
	found=1
done
[ "$found" -eq 1 ] || { echo "no writable PWM attribute found" >&2; exit 1; }

