# Debugging SPI

## Decision path

1. Confirm schematic voltage, pinmux, CS routing, reset and power.
2. Confirm controller probe, clocks and `/sys/class/spi_master`.
3. Confirm `spiB.C`, modalias, driver binding, mode and maximum speed.
4. Capture CS, SCLK, MOSI and MISO with a logic analyzer.
5. Start at a low clock and compare every bit with the datasheet.
6. Separate controller errors from target-protocol errors.
7. Verify the subsystem output (hwmon, MTD, TPM) and then OpenBMC service.

Useful checks:

```sh
dmesg | grep -Ei 'spi|mtd|nor|tpm|timeout|dma'
cat /sys/kernel/debug/clk/clk_summary 2>/dev/null
cat /proc/interrupts
mount -t debugfs none /sys/kernel/debug 2>/dev/null || true
echo 'file drivers/spi/* +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
```

For timing faults, inspect CPOL/CPHA, CS setup/hold, inter-transfer gaps, actual
clock rate, word length and byte order. For all-zero/all-one reads, check reset,
power, MISO direction, CS and whether another target drives MISO. For failures
only at high speed, inspect signal integrity, drive strength, sampling delay,
DMA thresholds and PCB length.

Trace controller callbacks with ftrace only on a development image; tracing can
change timing. Avoid raw probing of the BMC boot flash. Keep a recovery method
before changing flash partitions or protection bits.

