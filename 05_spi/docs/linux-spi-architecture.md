# Linux SPI architecture

```text
SPI controller hardware
        |
controller driver: struct spi_controller, transfer_one_message/transfer_one
        |
Linux SPI core: queueing, locking, device matching
        |
struct spi_device: bus, chip select, mode, maximum speed
        |
protocol driver: struct spi_driver
        |
hwmon / IIO / input / MTD / TPM / netdev / other subsystem
```

A controller driver maps MMIO, clocks, reset, IRQ, FIFO and DMA resources, then
registers a `struct spi_controller`. A protocol driver understands one target's
command format and submits `struct spi_message` objects containing one or more
`struct spi_transfer` objects.

Important protocol-driver calls include `spi_sync()`, `spi_async()`,
`spi_write()`, `spi_read()`, `spi_write_then_read()`, and
`spi_sync_transfer()`. Several transfers in one message normally preserve
message ordering and allow CS behavior to be described with `cs_change` and
delays. Controller capability limits still apply.

## Where to read source

- `drivers/spi/spi.c` — SPI core and message queueing.
- `drivers/spi/spidev.c` — userspace character-device bridge.
- `drivers/spi/spi-aspeed-smc.c` — ASPEED static memory/SPI controller family.
- `drivers/spi/spi-aspeed.c` or the controller selected by the target SoC.
- `drivers/mtd/spi-nor/` — SPI-NOR framework and manufacturer support.
- `drivers/hwmon/`, `drivers/iio/`, `drivers/char/tpm/` — protocol consumers.

For a new board, first change pinctrl and Device Tree. Modify the controller
driver only for new SoC hardware, timing/DMA limitations, error recovery, or an
erratum. Modify/write a protocol driver for a new target command set. Do not
fork the SPI core for a board-specific issue.

The logical name `spiB.C` means bus B and chip select C. Bus numbers can change;
software should use stable topology or subsystem interfaces rather than assume
`/dev/spidev0.0` forever.

