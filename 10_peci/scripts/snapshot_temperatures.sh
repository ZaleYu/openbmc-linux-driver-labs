#!/bin/sh
set -eu

found=0
for hwmon in /sys/class/hwmon/hwmon*; do
	[ -r "$hwmon/name" ] || continue
	name=$(cat "$hwmon/name")
	case "$name" in *peci*) ;; *) continue ;; esac
	found=1
	printf '%s (%s)\n' "$name" "$hwmon"
	for input in "$hwmon"/temp*_input; do
		[ -r "$input" ] || continue
		stem=${input%_input}
		channel=$(basename "$stem")
		label=$channel
		[ -r "${stem}_label" ] && label=$(cat "${stem}_label")
		value=$(cat "$input")
		case "$value" in
			-*) magnitude=${value#-}; sign=- ;;
			*) magnitude=$value; sign= ;;
		esac
		whole=$((magnitude / 1000))
		fraction=$((magnitude % 1000))
		printf '  %-30s %s%d.%03d C\n' "$label" "$sign" \
			"$whole" "$fraction"
	done
done
[ "$found" -eq 1 ] || { echo "No PECI hwmon device found." >&2; exit 1; }

