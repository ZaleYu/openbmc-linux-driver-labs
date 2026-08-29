#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$script_dir/list_kcs.sh"
"$script_dir/check_kcs_ownership.sh"

echo "Kernel configuration:"
pattern='CONFIG_(ASPEED_KCS|IPMI_KCS_BMC|NPCM7XX_KCS|MFD_SYSCON)'
if [ -r /proc/config.gz ] && command -v zcat >/dev/null 2>&1; then
	zcat /proc/config.gz | grep -E "$pattern" || true
elif [ -r "/boot/config-$(uname -r)" ]; then
	grep -E "$pattern" "/boot/config-$(uname -r)" || true
else
	echo "Kernel config is not exposed."
fi

echo "Loaded modules:"
lsmod 2>/dev/null | grep -Ei 'kcs|ipmi|espi|lpc' || true
echo "Relevant interrupts:"
grep -Ei 'kcs|ipmi|espi|lpc' /proc/interrupts 2>/dev/null || true
echo "Recent kernel messages:"
dmesg 2>/dev/null | grep -Ei 'kcs|ipmi|espi|lpc|sirq|reset' |
	tail -n 180 || true
echo "Recent bridge journal:"
journalctl --no-pager -n 160 2>/dev/null |
	grep -Ei 'kcsbridge|ipmi-kcs|phosphor-ipmi-host' || true

