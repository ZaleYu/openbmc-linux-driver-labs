# Linux I2C Muxes for OpenBMC

This module is a practical study package for I2C multiplexers and switches in
Linux/OpenBMC. A mux lets one controller reach several electrically separated
segments and reuse the same slave address on different channels.

```text
I2C controller -> physical parent adapter -> mux client
                                      +--> logical channel adapter 0 -> devices
                                      +--> logical channel adapter 1 -> devices
                                      +--> logical channel adapter N -> devices
```

The examples use a fictional four-channel, register-controlled mux at `0x70`.
Writing `BIT(channel)` connects one channel; writing `0x00` disconnects all.
It exists only for teaching—use the real component datasheet and its upstream
driver (often `i2c-mux-pca954x`) on production hardware.

## Repository map

| Path | Purpose |
|---|---|
| `docs/i2c-mux-fundamentals.md` | Electrical behavior, muxes versus switches, and design rules |
| `docs/linux-i2c-mux-architecture.md` | Linux logical adapters, locking, callbacks, and source locations |
| `docs/device-tree.md` | Register-controlled, nested, and GPIO-controlled topology |
| `docs/userspace-tools.md` | Discover and safely access downstream adapters |
| `docs/debugging.md` | Layered topology and transaction troubleshooting |
| `docs/openbmc-use-cases.md` | PSU, fan, FRU, sensor, and backplane examples |
| `client-driver/` | Educational Linux I2C mux driver |
| `device-tree/` | Standalone DTS fragments for three topologies |
| `userspace/` | Resolve a channel and read a downstream device |
| `scripts/` | Inventory, channel resolution, and non-invasive diagnostics |

## Quick start

Inspect an existing Linux topology without issuing bus transactions:

```sh
sudo modprobe i2c-dev
./scripts/show_mux_tree.sh
./scripts/debug_i2c_mux.sh 5 0x70
```

Resolve a kernel-created channel adapter:

```sh
cc -O2 -Wall -Wextra -Werror -o resolve_mux_channel \
   userspace/resolve_mux_channel.c
./resolve_mux_channel 5 0x70 2
```

Only after verifying the downstream device, register semantics, and ownership:

```sh
cc -O2 -Wall -Wextra -Werror -o test_downstream_i2c \
   userspace/test_downstream_i2c.c
sudo ./test_downstream_i2c /dev/i2c-12 0x48 0x00 2
```

Build the demonstration module against the exact target kernel headers:

```sh
make -C client-driver
sudo insmod client-driver/demo_i2c_mux.ko
```

## Important safety rules

- Treat child bus numbers as runtime data unless board aliases deliberately pin
  them. Discover `channel-*` links instead of assuming `/dev/i2c-N`.
- Do not program the mux control register from userspace while its kernel driver
  owns it. Use the downstream logical adapter; Linux selects the channel.
- Avoid blind `i2cdetect`, `i2cset`, and forced access on production buses.
- Separate pull-ups, voltage domains, maximum capacitance, idle policy, and
  reset behavior are board-design decisions—not properties Linux can repair.
- A nested or multi-master topology needs a written ownership/locking model.

## Recommended study order

1. Understand the electrical topology and identical-address use case.
2. Follow one transfer through the parent adapter, select callback, and child.
3. Compare the DTS examples with the schematic and kernel binding schema.
4. Learn the sysfs `channel-*` mapping before using `/dev/i2c-*`.
5. Review the demo driver's parent-locking and unlocked selector transaction.
6. Debug one layer at a time: controller, mux client, channel adapter, endpoint.
7. Map physical locations and failure domains into the OpenBMC inventory model.

## References

- Linux I2C mux topology: <https://docs.kernel.org/i2c/i2c-topology.html>
- Linux I2C sysfs topology: <https://docs.kernel.org/i2c/i2c-sysfs.html>
- Kernel mux core: `drivers/i2c/i2c-mux.c` and `include/linux/i2c-mux.h`
- Production example: `drivers/i2c/muxes/i2c-mux-pca954x.c`
- Binding schemas: `Documentation/devicetree/bindings/i2c/i2c-mux*.yaml`

