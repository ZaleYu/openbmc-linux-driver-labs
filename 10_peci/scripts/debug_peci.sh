#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$script_dir/list_peci.sh"

echo "Kernel configuration:"
if [ -r /proc/config.gz ] && command -v zcat >/dev/null 2>&1; then
	zcat /proc/config.gz | grep -E 'CONFIG_(PECI|SENSORS_PECI)' || true
elif [ -r "/boot/config-$(uname -r)" ]; then
	grep -E 'CONFIG_(PECI|SENSORS_PECI)' "/boot/config-$(uname -r)" || true
else
	echo "Kernel config is not exposed."
fi

echo "Loaded PECI modules:"
lsmod 2>/dev/null | grep -Ei 'peci' || true
echo "PECI-related interrupts:"
grep -Ei 'peci' /proc/interrupts 2>/dev/null || true
echo "Recent kernel messages:"
dmesg 2>/dev/null | grep -Ei 'peci|cputemp|dimmtemp|hwmon|fcs|timeout' |
	tail -n 180 || true
echo "OpenBMC CPU sensor service:"
systemctl --no-pager --full status \
	xyz.openbmc_project.CPUSensor.service 2>/dev/null || true
echo "Recent CPU sensor journal:"
journalctl --no-pager -u xyz.openbmc_project.CPUSensor.service \
	-n 120 2>/dev/null || true

