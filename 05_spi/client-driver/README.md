# Demo SPI sensor driver

The fictional device uses SPI mode 0 and an 8-bit register command. Read bit 7
is set for reads. Temperature and threshold are signed big-endian centi-degrees
C. The driver exports `temp1_input`, `temp1_max`, and `temp1_max_alarm` through
hwmon.

Build out-of-tree with `make`, enable the matching DT node, and load with
`sudo insmod demo_spi_sensor.ko`. Confirm the device ID and hwmon files in
`dmesg` and `/sys/class/hwmon`. Never bind this driver to unknown real hardware.

The mutex keeps a multi-register operation coherent at the protocol-driver
level. The SPI core serializes messages on the controller; it cannot define the
target's application-level atomicity for us.

