#!/bin/sh
set -eu

"$(dirname "$0")/list_uart.sh"
echo "Owners:"
for dev in /dev/ttyS* /dev/ttyVUART*; do
	[ -e "$dev" ] || continue
	fuser -v "$dev" 2>&1 || true
done
echo "Interrupts:"
grep -Ei 'uart|serial|vuart|tty' /proc/interrupts 2>/dev/null || true
echo "Recent kernel messages:"
dmesg 2>/dev/null | grep -Ei 'tty|uart|serial|vuart|overrun|framing|parity' |
	tail -n 120 || true
echo "OpenBMC console services:"
systemctl --no-pager --type=service 2>/dev/null |
	grep -Ei 'obmc-console|hostlogger|serial-getty' || true

