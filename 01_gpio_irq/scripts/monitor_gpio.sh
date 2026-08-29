#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
	echo "Usage: $0 CHIP LINE [rising|falling|both]" >&2
	echo "Example: $0 gpiochip0 17 both" >&2
	exit 2
fi

chip=$1
line=$2
edge=${3:-both}

case "$edge" in
	rising|falling|both) ;;
	*) echo "Edge must be rising, falling, or both" >&2; exit 2 ;;
esac

command -v gpiomon >/dev/null 2>&1 || {
	echo "gpiomon is unavailable; install libgpiod tools" >&2
	exit 1
}

echo "Monitoring $chip line $line for $edge edges. Press Ctrl-C to stop."
exec gpiomon -c "$chip" -e "$edge" "$line"

