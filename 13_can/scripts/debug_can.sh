#!/bin/sh
set -eu

iface=${1-can0}
case "$iface" in *[!A-Za-z0-9_.-]*|'') echo "invalid interface" >&2; exit 2 ;; esac

echo "CAN interfaces"
ip -details -statistics link show type can 2>/dev/null ||
	echo "No CAN interfaces reported."

printf '\nSelected interface: %s\n' "$iface"
if [ -e "/sys/class/net/$iface" ]; then
	ip -details -statistics link show "$iface"
	printf 'driver: '
	readlink "/sys/class/net/$iface/device/driver" 2>/dev/null || echo "unknown"
	printf 'device: '
	readlink "/sys/class/net/$iface/device" 2>/dev/null || echo "unknown"
else
	echo "Interface not present."
fi

printf '\nCAN-related interrupts\n'
if [ -r /proc/interrupts ]; then
	grep -Ei 'can|mcp25|m_can|flexcan' /proc/interrupts || echo "No matching IRQ."
else
	echo "/proc/interrupts unavailable."
fi

printf '\nCAN-related modules\n'
if [ -r /proc/modules ]; then
	awk '$1 ~ /(can|mcp25|m_can|flexcan)/ { print }' /proc/modules || true
else
	echo "/proc/modules unavailable."
fi

printf '\nRecent kernel messages\n'
dmesg 2>/dev/null | grep -Ei 'can|mcp25|m_can|flexcan|bus.off' | tail -n 100 || true

echo "For live error frames, run: candump -e -x -ta $iface"
