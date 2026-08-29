#!/bin/sh
set -eu

gadget_root=/sys/kernel/config/usb_gadget
gadget="$gadget_root/openbmc_usb_lab"

usage()
{
	echo "usage: $0 --apply [udc-name] | --remove" >&2
	exit 2
}

require_root()
{
	[ "$(id -u)" -eq 0 ] || {
		echo "This operation requires root." >&2
		exit 1
	}
}

select_udc()
{
	requested=${1-}
	if [ -n "$requested" ]; then
		[ -e "/sys/class/udc/$requested" ] || {
			echo "UDC not found: $requested" >&2
			exit 1
		}
		echo "$requested"
		return
	fi
	set -- /sys/class/udc/*
	[ -e "$1" ] || {
		echo "No UDC is registered." >&2
		exit 1
	}
	[ "$#" -eq 1 ] || {
		echo "Multiple UDCs exist; specify one explicitly." >&2
		exit 1
	}
	echo "${1##*/}"
}

apply_gadget()
{
	udc=$(select_udc "${1-}")
	[ -d "$gadget_root" ] || {
		echo "Mount configfs and enable USB gadget configfs first." >&2
		exit 1
	}
	[ ! -e "$gadget" ] || {
		echo "$gadget already exists; remove it first." >&2
		exit 1
	}
	for owner in "$gadget_root"/*/UDC; do
		[ -f "$owner" ] || continue
		[ "$(cat "$owner")" != "$udc" ] || {
			echo "UDC $udc is already owned by ${owner%/UDC}." >&2
			exit 1
		}
	done

	mkdir "$gadget"
	printf '0xffff' > "$gadget/idVendor"
	printf '0xffff' > "$gadget/idProduct"
	printf '0x0200' > "$gadget/bcdUSB"
	printf '0x0100' > "$gadget/bcdDevice"
	mkdir "$gadget/strings/0x409"
	printf 'LAB-ONLY-0001' > "$gadget/strings/0x409/serialnumber"
	printf 'OpenBMC Study Lab' > "$gadget/strings/0x409/manufacturer"
	printf 'USB Bulk Loopback' > "$gadget/strings/0x409/product"
	mkdir "$gadget/configs/c.1"
	mkdir "$gadget/configs/c.1/strings/0x409"
	printf 'Loopback configuration' > "$gadget/configs/c.1/strings/0x409/configuration"
	mkdir "$gadget/functions/Loopback.0"
	ln -s "$gadget/functions/Loopback.0" "$gadget/configs/c.1/Loopback.0"
	printf '%s' "$udc" > "$gadget/UDC"
	echo "Bound lab-only ffff:ffff loopback gadget to $udc."
}

remove_gadget()
{
	[ -d "$gadget" ] || {
		echo "Lab gadget does not exist."
		return
	}
	printf '' > "$gadget/UDC"
	[ ! -L "$gadget/configs/c.1/Loopback.0" ] ||
		unlink "$gadget/configs/c.1/Loopback.0"
	rmdir "$gadget/functions/Loopback.0"
	rmdir "$gadget/configs/c.1/strings/0x409"
	rmdir "$gadget/configs/c.1"
	rmdir "$gadget/strings/0x409"
	rmdir "$gadget"
	echo "Removed the lab loopback gadget."
}

[ "$#" -ge 1 ] || usage
case "$1" in
	--apply)
		[ "$#" -le 2 ] || usage
		require_root
		apply_gadget "${2-}"
		;;
	--remove)
		[ "$#" -eq 1 ] || usage
		require_root
		remove_gadget
		;;
	*) usage ;;
esac
