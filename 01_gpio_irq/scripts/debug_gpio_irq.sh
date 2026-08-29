#!/bin/sh
set -u

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	echo "Usage: $0 CHIP [LINE]" >&2
	echo "Example: $0 gpiochip0 17" >&2
	echo "This script does not request, monitor, or drive the line." >&2
	exit 2
fi

chip=$1
line=${2:-}

echo "GPIO/IRQ diagnostic report"
date 2>/dev/null || true
uname -a

echo
echo "GPIO chips"
if command -v gpiodetect >/dev/null 2>&1; then
	gpiodetect
else
	echo "gpiodetect unavailable"
fi

echo
echo "Selected GPIO information"
if command -v gpioinfo >/dev/null 2>&1; then
	if [ -n "$line" ]; then
		gpioinfo -c "$chip" "$line" 2>&1 || true
	else
		gpioinfo -c "$chip" 2>&1 || true
	fi
else
	echo "gpioinfo unavailable"
fi

echo
echo "Kernel GPIO debug view"
if [ -r /sys/kernel/debug/gpio ]; then
	cat /sys/kernel/debug/gpio
else
	echo "/sys/kernel/debug/gpio unavailable"
fi

echo
echo "Interrupt counters"
if [ -r /proc/interrupts ]; then
	cat /proc/interrupts
else
	echo "/proc/interrupts unavailable"
fi

echo
echo "Pinctrl providers"
if [ -d /sys/kernel/debug/pinctrl ]; then
	find /sys/kernel/debug/pinctrl -mindepth 1 -maxdepth 1 -print 2>/dev/null
else
	echo "pinctrl debugfs unavailable"
fi

echo
echo "Recent kernel messages"
dmesg 2>/dev/null | grep -Ei 'gpio|irq|pinctrl|demo_gpio|spurious|storm' | \
	tail -n 120 || echo "Kernel log unavailable or no matching messages"

echo
echo "No GPIO line was requested or modified by this script."

