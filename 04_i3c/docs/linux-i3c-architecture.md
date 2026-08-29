# Linux I3C Architecture

## Data path

```text
SoC controller driver
  -> struct i3c_master_controller + controller ops
  -> Linux I3C core: bus init, CCC, DAA, device model, locking
  -> struct i3c_device discovered by PID/BCR/DCR
  -> struct i3c_driver
  -> subsystem interface: hwmon, MCTP, IIO, nvmem, etc.
```

The I3C master also exposes an I2C adapter for legacy I2C devices described on
the same bus. Those legacy devices continue to bind normal `i2c_driver`
instances. An I3C target binds an `i3c_driver` through its ID table.

## Target-driver API in Linux 6.18

The demo uses:

```c
struct i3c_priv_xfer;
i3c_device_do_priv_xfers(dev, xfers, nxfers);
i3c_device_get_info(dev, &info);
i3c_device_request_ibi(dev, &setup);
i3c_device_enable_ibi(dev);
i3c_device_disable_ibi(dev);
i3c_device_free_ibi(dev);
module_i3c_driver(driver);
```

Private-transfer calls sleep and must not run in atomic context. Transfer data
buffers must satisfy the controller/core DMA requirements; the demo keeps them
in driver-owned allocated memory protected by a mutex.

## Linux 6.19 API transition

Linux 6.19 replaces `struct i3c_priv_xfer` with the more general
`struct i3c_xfer` and uses:

```c
i3c_device_do_xfers(dev, xfers, nxfers, I3C_SDR);
```

This package deliberately targets the user's Linux 6.18 study kernel. Compile
against the exact OpenBMC/Raspberry Pi kernel headers and adapt the transfer
objects rather than adding casts or copying private core definitions.

## Controller-driver API

A controller driver fills `struct i3c_master_controller_ops`. Important hooks
cover bus initialization/cleanup, DAA, CCC support/transmission, private I3C
transfers, legacy I2C transfers, target attach/detach/reattach, IBI allocation
and control, Hot-Join, and optional speed/retry features. It registers with
`i3c_master_register()` and unregisters with `i3c_master_unregister()`.

The controller implementation owns hardware queues, DMA, IRQ handling,
timeouts, error decoding, and serialization of transactions submitted through
the core.

## Source locations

| Path | What to study or modify |
|---|---|
| `drivers/i3c/master.c` | Core discovery, bus state, sysfs, locking, DAA, Hot-Join |
| `drivers/i3c/device.c` | Target-driver API, private transfers, IBI lifecycle |
| `include/linux/i3c/device.h` | Public target-driver structures and ID macros |
| `include/linux/i3c/master.h` | Controller structures, ops, bus modes, helpers |
| `drivers/i3c/master/` | Cadence, DesignWare, AST2600/vendor controller drivers |
| `drivers/i3c/mctp/` | MCTP over I3C implementations where available |
| `Documentation/devicetree/bindings/i3c/` | Generic and controller bindings |

Typical SoC changes belong in the controller-specific driver: clocks, resets,
timing, FIFO/DMA, IRQ, DAA implementation, CCC capability, IBI queues, Hot-Join,
runtime PM, and error recovery. A target-specific register map belongs in its
target driver. Avoid changing the generic core for a board-only workaround.

## Locking and retry

Maintenance operations such as address changes exclude normal bus use. Normal
transfers can be submitted from multiple target drivers; the controller driver
must prevent hardware commands from interleaving incorrectly. A private
transfer can return `-EAGAIN` when an IBI, Hot-Join, or role request wins
arbitration. Retry in sleepable context with a bounded policy; never spin in a
tight loop or hide persistent faults.

