#!/bin/sh
set -eu

usage()
{
	echo "usage: $0 up|down [interface]" >&2
	exit 2
}

[ "$#" -ge 1 ] && [ "$#" -le 2 ] || usage
action=$1
iface=${2-vcan0}

case "$iface" in
	*[!A-Za-z0-9_.-]*|'') echo "invalid interface name" >&2; exit 2 ;;
esac

case "$action" in
	up)
		modprobe vcan
		if ! ip link show "$iface" >/dev/null 2>&1; then
			ip link add dev "$iface" type vcan
		fi
		ip link set "$iface" up
		ip -details link show "$iface"
		;;
	down)
		if ip link show "$iface" >/dev/null 2>&1; then
			ip link delete "$iface"
		else
			echo "$iface does not exist."
		fi
		;;
	*) usage ;;
esac
