# Demo GPIO and IRQ Driver

This is an educational platform GPIO consumer driver for fictional BMC board
signals. It is not a GPIO-controller provider driver.

## Features demonstrated

- Device Tree platform matching.
- descriptor-based managed GPIO requests.
- required and optional GPIO consumers.
- active-low logical semantics.
- GPIO-to-IRQ mapping.
- both-edge threaded IRQ handling with `IRQF_ONESHOT`.
- sleep-capable GPIO access for native controllers or expanders.
- optional provider debounce.
- sysfs notification, event count, and monotonic event timestamp.
- rate-limited interrupt logging.

## Build out of tree

```sh
make
sudo insmod demo_gpio_irq.ko
dmesg | tail
```

The build host must have headers prepared for the exact running target kernel.
Cross compilation can be supplied through standard kernel variables:

```sh
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
     KDIR=/path/to/kernel/build
```

## Device Tree contract

The driver expects:

```dts
compatible = "demo,gpio-irq-monitor";
presence-gpios = <...>;       /* required input */
fault-gpios = <...>;          /* optional input */
reset-gpios = <...>;          /* optional output */
debounce-interval-us = <...>; /* optional educational property */
```

A production-compatible string and property require a real YAML binding.

## Diagnostic attributes

Under the bound platform-device directory:

| Attribute | Access | Meaning |
|---|---|---|
| `present` | RO | Current logical presence assertion |
| `fault` | RO | Current logical fault assertion, if configured |
| `event_count` | RO | Total handled presence and fault edges |
| `last_event_ns` | RO | Last event time from the monotonic clock |
| `reset` | RW | Logical reset assertion, if configured |

For an active-low reset, writing `1` asserts the reset and may drive a physical
low level:

```sh
echo 1 | sudo tee /sys/bus/platform/devices/DEVICE/reset
echo 0 | sudo tee /sys/bus/platform/devices/DEVICE/reset
```

These attributes are educational, not a proposed stable kernel ABI. Production
drivers should use an established subsystem or a reviewed ABI and policy.

## Production adaptation

1. Replace the fictional binding and signals with the real component model.
2. Decide whether `gpio-keys`, regulator, reset-controller, LED, input, or an
   OpenBMC libgpiod daemon already provides the correct abstraction.
3. Select edge or level trigger based on hardware clearing behavior.
4. Avoid both-edge assumptions when the controller emulates them unreliably.
5. Add suspend/resume and wakeup only when the platform requires them.
6. Define output safe state across bootloader, probe, unbind, shutdown, and reset.
7. Add workqueue/state-machine logic instead of doing complex policy in an IRQ.
8. Test bounce, stuck level, storm, expander bus errors, and rapid hot-plug.

