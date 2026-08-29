# Device Tree

An SPI peripheral is a child of its controller. `reg` is the chip-select index,
not a memory address. The controller normally uses `#address-cells = <1>` and
`#size-cells = <0>`.

Common peripheral properties:

- `compatible` selects the protocol driver.
- `reg` selects CS.
- `spi-max-frequency` is the target limit, not a promised clock.
- `spi-cpol` and `spi-cpha` select modes 2/3 and 1/3 respectively.
- `spi-cs-high`, `spi-3wire`, `spi-lsb-first` describe special signaling.
- `spi-rx-bus-width` and `spi-tx-bus-width` describe multi-lane transfers.
- `interrupts`, reset GPIOs, supplies, and clocks remain device-specific.

The controller node also owns pinctrl, clocks, reset, interrupts, DMA and
possibly `cs-gpios`. Check its binding. Native and GPIO chip selects may be
mixed, but polarity must agree with both the GPIO flags and `spi-cs-high`.

Run schema checks in the kernel tree:

```sh
make dt_binding_check DT_SCHEMA_FILES=spi
make dtbs_check DT_SCHEMA_FILES=spi
```

Do not use a generic `compatible = "spidev"` in production DT. Modern kernels
expect a real compatible listed by spidev or a temporary `driver_override` for
development. Production targets should bind to an actual subsystem driver.

