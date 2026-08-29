# Linux SPI for OpenBMC

This lab follows one path from SPI electrical signaling to a Linux protocol
driver and OpenBMC integration.  The example device is fictional, so adapt the
register map and binding before using it on hardware.

## Contents

- `docs/` — protocol, Linux architecture, Device Tree, tools, debugging, and
  OpenBMC case studies.
- `client-driver/` — a small SPI hwmon protocol driver.
- `device-tree/` — single-device, shared-bus, and SPI-NOR examples.
- `userspace/` — Linux spidev tests using `SPI_IOC_MESSAGE`.
- `scripts/` — inventory and diagnosis helpers.

## Learning path

1. Read `docs/protocol.md` and confirm the target timing diagram.
2. Enable the controller and pinmux, then inspect `/sys/bus/spi/devices`.
3. Test an unbound development device with spidev.
4. Add the real compatible string and bind a kernel protocol driver.
5. Expose standard hwmon, MTD, TPM, or other subsystem interfaces to OpenBMC.

Never attach spidev and a kernel protocol driver to the same device at the same
time.  Start at a conservative clock and use a logic analyzer when mode,
chip-select timing, or command framing is uncertain.

## References

- Linux `Documentation/spi/spi-summary.rst`
- Linux `Documentation/spi/spidev.rst`
- Linux `Documentation/driver-api/spi.rst`
- Devicetree `spi-controller.yaml` and `spi-peripheral-props.yaml`

