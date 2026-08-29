# Demo I2C Mux Driver

`demo_i2c_mux.c` is an educational client driver for a fictional four-channel
mux at an I2C address such as `0x70`. It is intentionally separate from the
documentation so it can be copied, compiled, reviewed, and instrumented.

## Model

| Control byte | State |
|---:|---|
| `0x00` | all channels disconnected |
| `0x01` | channel 0 connected |
| `0x02` | channel 1 connected |
| `0x04` | channel 2 connected |
| `0x08` | channel 3 connected |

This is not a binding for a commercial component. Real parts differ in mux
versus switch behavior, register layout, reset, IRQ, power management, and idle
policy. Prefer an upstream driver such as `i2c-mux-pca954x` when it matches.

## What to study

- `i2c_mux_alloc()` stores private state and installs callbacks.
- `i2c_mux_add_adapter()` creates one logical child adapter per channel.
- flags `0` request parent-locked transfers.
- select/deselect use `__i2c_smbus_xfer()` because the parent is already locked.
- the cache is updated only after a successful selector write.
- probe failure and remove delete all child adapters and disconnect the mux.

Never copy the unlocked helper outside the mux-core locking context. In normal
client code, use the locking `i2c_transfer()` or SMBus helpers.

## Build

```sh
make
sudo insmod demo_i2c_mux.ko
dmesg | tail
```

The module targets the current three-argument form of
`i2c_mux_add_adapter(muxc, force_nr, chan_id)`. Older vendor kernels may expose
a different signature. Adapt to the exact target headers; do not paper over an
API mismatch with casts.

For in-tree integration, place the source under `drivers/i2c/muxes/`, add the
object to that directory's Makefile, merge the Kconfig entry appropriately, and
enable the symbol in the OpenBMC kernel configuration fragment.

## Review before adapting

1. Replace compatible, register encoding, channel count, and functionality bit.
2. Follow the binding schema and datasheet reset/power/idle requirements.
3. Re-evaluate parent-locked versus mux-locked semantics, especially if nested.
4. Add device-specific error checking, PM, reset, IRQ, and recovery support.
5. Test concurrency, probe unwind, unbind, suspend/resume, and failed channels.
6. Submit a reusable driver and YAML binding upstream where appropriate.

