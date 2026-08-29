#!/bin/sh
set -eu

"$(dirname "$0")/list_fans.sh"
echo "PWM debug:"
cat /sys/kernel/debug/pwm 2>/dev/null || true
echo "Fan/tach interrupts:"
grep -Ei 'fan|tach|pwm' /proc/interrupts 2>/dev/null || true
echo "Recent kernel messages:"
dmesg 2>/dev/null | grep -Ei 'pwm|fan|tach|thermal|irq|timeout' |
	tail -n 100 || true
echo "OpenBMC fan services:"
systemctl --no-pager --type=service 2>/dev/null |
	grep -Ei 'fan|sensor|thermal' || true

