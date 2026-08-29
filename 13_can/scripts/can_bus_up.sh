#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
	echo "usage: $0 INTERFACE BITRATE [RESTART_MS]" >&2
	exit 2
fi

iface=$1
bitrate=$2
restart_ms=${3-0}
case "$iface" in *[!A-Za-z0-9_.-]*|'') echo "invalid interface" >&2; exit 2 ;; esac
case "$bitrate" in *[!0-9]*|'') echo "invalid bitrate" >&2; exit 2 ;; esac
case "$restart_ms" in *[!0-9]*|'') echo "invalid restart-ms" >&2; exit 2 ;; esac

[ -e "/sys/class/net/$iface" ] || {
	echo "interface not found: $iface" >&2
	exit 1
}

echo "Configuring $iface: bitrate=$bitrate restart-ms=$restart_ms"
echo "This changes a real shared bus; press Ctrl-C now if not authorized."
ip link set "$iface" down
ip link set "$iface" type can bitrate "$bitrate" restart-ms "$restart_ms"
ip link set "$iface" up
ip -details -statistics link show "$iface"
