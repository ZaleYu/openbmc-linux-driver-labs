#!/bin/sh
set -eu

tool=${1-./lin_frame_tool}
[ -x "$tool" ] || {
	echo "Build the tool first: cc -O2 -Wall -Wextra -Werror -o lin_frame_tool userspace/lin_frame_tool.c" >&2
	exit 1
}

pid_output=$($tool pid 12)
checksum_output=$($tool checksum enhanced 12 10 27)
printf '%s\n%s\n' "$pid_output" "$checksum_output"
printf '%s' "$pid_output" | grep -q 'PID=0x92'
printf '%s' "$checksum_output" | grep -q 'enhanced-checksum=0x36'
echo "PASS: protected ID and enhanced checksum vectors"
