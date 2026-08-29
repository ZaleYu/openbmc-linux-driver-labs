#!/bin/sh
set -eu

chip=${1:-}

command -v gpiodetect >/dev/null 2>&1 || {
	echo "gpiodetect is unavailable; install libgpiod tools" >&2
	exit 1
}
command -v gpioinfo >/dev/null 2>&1 || {
	echo "gpioinfo is unavailable; install libgpiod tools" >&2
	exit 1
}

echo "libgpiod tools:"
gpioinfo --version 2>/dev/null | head -n 1 || true

echo
echo "GPIO chips:"
gpiodetect

echo
if [ -n "$chip" ]; then
	echo "Lines on $chip:"
	gpioinfo -c "$chip"
else
	echo "All GPIO lines:"
	gpioinfo
fi

echo
echo "A consumer name means the line may already be owned."

