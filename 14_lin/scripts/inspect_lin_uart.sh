#!/bin/sh
set -eu

tty_path=${1-/dev/ttyS3}
tty_name=${tty_path##*/}

echo "TTY: $tty_path"
if [ -e "$tty_path" ]; then
	if command -v stty >/dev/null 2>&1; then
		stty -F "$tty_path" -a 2>/dev/null || echo "Cannot read termios (busy or permission denied)."
	fi
else
	echo "TTY node is absent; it may be disabled or owned by serdev."
fi

printf '\nSysfs\n'
if [ -e "/sys/class/tty/$tty_name" ]; then
	readlink "/sys/class/tty/$tty_name/device/driver" 2>/dev/null || true
	find "/sys/class/tty/$tty_name/device" -maxdepth 2 -print 2>/dev/null || true
else
	echo "No /sys/class/tty/$tty_name"
fi

printf '\nSerdev devices\n'
if [ -d /sys/bus/serial/devices ]; then
	find /sys/bus/serial/devices -maxdepth 2 -print
else
	echo "serdev sysfs bus is unavailable."
fi

printf '\nRecent serial/LIN messages\n'
dmesg 2>/dev/null | grep -Ei 'lin|serdev|tty|uart|serial' | tail -n 100 || true
