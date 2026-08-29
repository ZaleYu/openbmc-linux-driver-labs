#!/bin/sh
set -eu

found=0
for path in /sys/class/net/*; do
	[ -e "$path/type" ] || continue
	[ "$(cat "$path/type")" = 290 ] || continue
	found=1
	name=$(basename "$path")
	state=$(cat "$path/operstate" 2>/dev/null || echo unknown)
	mtu=$(cat "$path/mtu" 2>/dev/null || echo unknown)
	printf '%s state=%s mtu=%s ifindex=%s\n' "$name" "$state" "$mtu" \
		"$(cat "$path/ifindex" 2>/dev/null || echo unknown)"
done
[ "$found" -eq 1 ] || echo "No ARPHRD_MCTP interfaces found."

if command -v mctp >/dev/null 2>&1; then
	echo "MCTP links:"
	mctp link 2>&1 || true
	echo "Local addresses:"
	mctp addr 2>&1 || true
	echo "Neighbours:"
	mctp neigh 2>&1 || true
	echo "Routes:"
	mctp route 2>&1 || true
else
	echo "mctp utility not installed; sysfs inventory only."
fi

