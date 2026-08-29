#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
	echo "usage: $0 ENDPOINT_EID" >&2
	exit 2
fi
eid=$1
case "$eid" in
	''|*[!0-9]*) echo "EID must be decimal 1..254" >&2; exit 2 ;;
esac
[ "$eid" -ge 1 ] && [ "$eid" -le 254 ] || {
	echo "EID must be decimal 1..254" >&2
	exit 2
}

echo "Checking endpoint EID $eid"
if command -v mctp >/dev/null 2>&1; then
	echo "Matching neighbours:"
	mctp neigh 2>&1 | grep -E "(^|[[:space:]])$eid([[:space:]]|$)" || true
	echo "Matching routes:"
	mctp route 2>&1 | grep -E "(^|[[:space:]])$eid([[:space:]]|$)" || true
else
	echo "mctp utility not installed."
fi

echo "Discovery objects containing the EID:"
if command -v busctl >/dev/null 2>&1; then
	busctl tree xyz.openbmc_project.MCTP 2>/dev/null |
		grep -E "(^|[^0-9])$eid([^0-9]|$)" || true
else
	echo "busctl not installed."
fi

echo "Next: verify physical address, network, message types and endpoint state."

