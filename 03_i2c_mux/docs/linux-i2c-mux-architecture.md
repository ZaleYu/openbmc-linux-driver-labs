# Linux I2C Mux Architecture

## Objects and data path

The physical controller registers an `i2c_adapter`. The mux is normally an
`i2c_client` on that parent. The I2C mux core creates one logical adapter per
registered channel. Endpoint clients bind below those logical adapters.

```text
endpoint driver -> child i2c_adapter -> mux core
                 -> select(channel) -> parent adapter transfer
                 -> deselect(channel, optional)
```

Each child appears in sysfs as `i2c-N`. The mux device exposes `channel-X`
symlinks to those adapters; a child adapter has a `mux_device` link back to the
mux. Bus numbers are not physical channel numbers.

## Core API used by the demo

Current kernels provide:

```c
i2c_mux_alloc(parent, dev, max_adapters, sizeof_priv,
              flags, select, deselect);
i2c_mux_add_adapter(muxc, force_nr, chan_id);
i2c_mux_del_adapters(muxc);
i2c_mux_priv(muxc);
```

Kernel APIs evolve. Some older trees have a different
`i2c_mux_add_adapter()` signature. Always compile against and inspect the exact
OpenBMC kernel tree shipped by the platform.

## Locking: the subtle part

With flags `0`, the parent/root adapter is locked across select, endpoint
transfer, and deselect. This makes the whole operation atomic relative to other
Linux clients on that root. If the selector itself is reached through that
same parent I2C bus, the callback must use an unlocked transfer helper such as
`__i2c_smbus_xfer()`; calling the normal locking helper can deadlock.

`I2C_MUX_LOCKED` selects the mux-locked model. Despite the name, this locks the
mux adapters rather than the complete parent path. It is useful when selector
control is independent of the parent transfer path, but nested topologies need
careful analysis. Follow the in-tree driver's locking model rather than copying
a flag without understanding it.

## Source locations worth reading

| Path | What to study or modify |
|---|---|
| `include/linux/i2c-mux.h` | Public API, flags, callbacks, private storage |
| `drivers/i2c/i2c-mux.c` | Adapter creation, transfer wrappers, locking |
| `drivers/i2c/muxes/i2c-mux-pca954x.c` | Register-controlled mux/switch example |
| `drivers/i2c/muxes/i2c-mux-gpio.c` | GPIO selector and settle timing |
| `drivers/i2c/muxes/i2c-mux-pinctrl.c` | Pinctrl-based paths |
| `Documentation/devicetree/bindings/i2c/` | Binding contract and valid properties |

Typical changes belong in the hardware-specific mux driver: compatible/data
table, register encoding, channel count, enable/idle state, reset, interrupts,
and power management. Do not change the generic mux core for one board quirk.

## Driver lifecycle

1. Match and probe the selector client.
2. Check parent capabilities and initialize/reset the component.
3. Allocate `i2c_mux_core` and driver-private state.
4. Add a logical adapter for every valid channel.
5. The I2C core enumerates endpoint nodes beneath each channel.
6. On removal or failed probe, delete all adapters and leave safe hardware state.

