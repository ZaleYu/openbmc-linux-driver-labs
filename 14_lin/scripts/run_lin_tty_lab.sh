#!/bin/sh
set -eu

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
	echo "usage: $0 TTY ID_HEX DATA_LEN [lin_tty_master_path]" >&2
	exit 2
fi

tty_path=$1
frame_id=$2
data_len=$3
tool=${4-./lin_tty_master}

[ -c "$tty_path" ] || { echo "not a character device: $tty_path" >&2; exit 1; }
[ -x "$tool" ] || { echo "tool is not executable: $tool" >&2; exit 1; }

echo "WARNING: this sends a LIN header on $tty_path."
echo "Use only on an isolated lab bus with a LIN transceiver and one master."
"$tool" "$tty_path" "$frame_id" "$data_len"
