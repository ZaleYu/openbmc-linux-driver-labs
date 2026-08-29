#!/bin/sh
set -eu

echo "== kernel =="
uname -r
echo "== modules =="
lsmod | grep -Ei 'spi|mtd|nor|tpm' || true
echo "== sysfs =="
"$(dirname "$0")/list_spi.sh"
echo "== device nodes =="
ls -l /dev/spidev* 2>/dev/null || echo "No spidev nodes"
echo "== recent log =="
dmesg 2>/dev/null | grep -Ei 'spi|mtd|nor|tpm|timeout|dma' | tail -n 80 || true

