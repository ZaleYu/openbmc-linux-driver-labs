#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$script_dir/list_usb.sh"

printf '\nLoaded USB modules\n'
if [ -r /proc/modules ]; then
	awk '$1 ~ /(usb|xhci|ehci|ohci|udc|gadget)/ { print }' /proc/modules || true
else
	echo "/proc/modules unavailable"
fi

printf '\nHost controller and USB interrupts\n'
if [ -r /proc/interrupts ]; then
	grep -Ei 'usb|xhci|ehci|ohci|vhub' /proc/interrupts || echo "No matching IRQ lines."
else
	echo "/proc/interrupts unavailable"
fi

printf '\nConfigured gadgets\n'
gadget_root=/sys/kernel/config/usb_gadget
if [ -d "$gadget_root" ]; then
	find "$gadget_root" -maxdepth 3 -print
	for udc_file in "$gadget_root"/*/UDC; do
		[ -f "$udc_file" ] || continue
		printf '%s: %s\n' "$udc_file" "$(cat "$udc_file")"
	done
else
	echo "USB gadget configfs is not mounted or unavailable."
fi

printf '\nRecent kernel USB messages\n'
if command -v dmesg >/dev/null 2>&1; then
	dmesg 2>/dev/null | grep -Ei 'usb|xhci|ehci|ohci|udc|gadget|vhub' | tail -n 100 || true
else
	echo "dmesg unavailable"
fi
