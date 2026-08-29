#!/bin/sh
set -eu

found=0
for device in /dev/ipmi-kcs*; do
	[ -e "$device" ] || continue
	found=1
	echo "$device owners:"
	if command -v fuser >/dev/null 2>&1; then
		fuser -v "$device" 2>&1 || echo "  no current owner"
	else
		echo "  fuser is not installed"
	fi
done
[ "$found" -eq 1 ] || echo "No KCS character device to inspect."

echo "KCS/IPMI services:"
systemctl --no-pager --full --type=service 2>/dev/null |
	grep -Ei 'phosphor-ipmi-(kcs|host)|kcsbridge|host-ipmi' || true

