#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$script_dir/list_mctp.sh"

echo "Kernel configuration:"
config=/proc/config.gz
if [ -r "$config" ] && command -v zcat >/dev/null 2>&1; then
	zcat "$config" | grep -E 'CONFIG_(MCTP|I2C_SLAVE|I3C)=' || true
elif [ -r "/boot/config-$(uname -r)" ]; then
	grep -E 'CONFIG_(MCTP|I2C_SLAVE|I3C)=' "/boot/config-$(uname -r)" || true
else
	echo "Kernel config is not exposed."
fi

echo "Loaded modules:"
lsmod 2>/dev/null | grep -Ei 'mctp|i2c|i3c' || true
echo "Recent kernel messages:"
dmesg 2>/dev/null | grep -Ei 'mctp|i2c|i3c|smbus' | tail -n 160 || true
echo "mctpd service:"
systemctl --no-pager --full status mctpd.service 2>/dev/null || true
echo "Recent mctpd journal:"
journalctl --no-pager -u mctpd.service -n 120 2>/dev/null || true

