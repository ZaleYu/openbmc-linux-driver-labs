# Userspace access

Inventory:

```sh
ls /sys/class/spi_master
find /sys/bus/spi/devices -maxdepth 2 -type l -o -type f
cat /sys/bus/spi/devices/spi0.0/modalias
readlink /sys/bus/spi/devices/spi0.0/driver
```

When a development target is intentionally bound to spidev, a node such as
`/dev/spidev0.0` appears. The stable ABI is `<linux/spi/spidev.h>` with ioctls
for mode, word size, speed, and `SPI_IOC_MESSAGE(N)`. The common `spidev_test`
program in kernel tools is also useful.

Binding spidev temporarily (replace device and verify first):

```sh
echo spidev | sudo tee /sys/bus/spi/devices/spi0.0/driver_override
echo spi0.0 | sudo tee /sys/bus/spi/drivers/spidev/bind
```

Clear the override after testing. Never unbind a boot flash, TPM, CPLD, or
device involved in active platform management. Raw writes can erase flash,
change power rails, reset hardware, or corrupt persistent configuration.

Use the programs in `userspace/` only on the fictional-compatible lab target.
Their Makefiles are intentionally unnecessary:

```sh
cc -O2 -Wall -Wextra -Werror -o test_spi_sensor test_spi_sensor.c
cc -O2 -Wall -Wextra -Werror -o spidev_transfer spidev_transfer.c
```

