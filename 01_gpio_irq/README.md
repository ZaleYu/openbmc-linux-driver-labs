# Linux GPIO and IRQ for OpenBMC

This module is a practical study package for GPIO and GPIO-backed interrupts in
BMC firmware. It follows a board-management signal from the pin to OpenBMC:

```text
Board signal -> pinctrl -> GPIO controller/gpiolib -> irqchip/IRQ core
             -> kernel consumer or libgpiod daemon -> D-Bus/systemd policy
             -> host power, inventory, fault, LED, or Redfish state
```

The examples model three fictional BMC signals:

| Function | Direction | Polarity | Purpose |
|---|---|---|---|
| `presence` | Input | Active low | Detect a removable module |
| `fault` | Input | Active low | Receive a hardware fault indication |
| `reset` | Output | Active low | Control a peripheral reset |

The package demonstrates the descriptor-based GPIO consumer API, GPIO-to-IRQ
mapping, threaded interrupt handling, edge events through GPIO character-device
ABI v2, Device Tree mappings, debounce, line ownership, and OpenBMC GPIO use
cases.

## Repository map

| Path | Purpose |
|---|---|
| `docs/electrical-signals.md` | GPIO electrical behavior, polarity, edges, and debounce |
| `docs/linux-gpio-architecture.md` | pinctrl, gpiochip, descriptors, irqchip, and consumers |
| `docs/device-tree.md` | GPIO provider/consumer mappings and examples |
| `docs/userspace-tools.md` | libgpiod tools and GPIO character-device ABI v2 |
| `docs/debugging.md` | Layered GPIO/IRQ debugging and interrupt-storm analysis |
| `docs/openbmc-use-cases.md` | Presence, buttons, faults, power sequencing, and multi-host cases |
| `client-driver/` | Educational platform GPIO consumer with threaded IRQs |
| `device-tree/` | Consumer, gpio-keys, and GPIO-hog overlays |
| `userspace/` | Direct GPIO v2 event monitor and timed output controller |
| `scripts/` | GPIO inventory, event monitoring, and diagnostic collection |

## Quick start

Install libgpiod tools on the target and inspect line ownership before access:

```sh
gpiodetect
gpioinfo
```

Read or monitor a free line using libgpiod 2.x syntax:

```sh
gpioget --numeric -c gpiochip0 17
gpiomon -c gpiochip0 -e both 17
```

Build the direct GPIO character-device ABI v2 examples:

```sh
cc -O2 -Wall -Wextra -o monitor_gpio_v2 userspace/monitor_gpio_v2.c
cc -O2 -Wall -Wextra -o set_gpio_v2 userspace/set_gpio_v2.c
sudo ./monitor_gpio_v2 /dev/gpiochip0 17
sudo ./set_gpio_v2 /dev/gpiochip0 22 1 5 active-low
```

Build the out-of-tree kernel module on a development target with matching
kernel headers:

```sh
make -C client-driver
sudo insmod client-driver/demo_gpio_irq.ko
```

When the Device Tree node is present and the driver is bound, inspect:

```sh
grep -H . /sys/bus/platform/drivers/demo_gpio_irq/*/{present,fault,event_count,last_event_ns} 2>/dev/null
```

## Safety

- Obtain the schematic signal name, voltage domain, direction, polarity, and
  power-state behavior before requesting a line.
- Never drive an input, strap, shared host signal, reset, power-enable, or
  write-protect line merely because it appears as a free GPIO.
- GPIO logical values apply `GPIO_ACTIVE_LOW`; logical `1` means asserted, not
  necessarily a high voltage.
- `gpioset` owns and drives a line only while its process holds the request;
  the state after exit is not guaranteed.
- Do not steal a line from a kernel driver or OpenBMC daemon. Resolve ownership
  instead of using deprecated global GPIO-number interfaces.
- Interrupt storms can consume CPU and fill logs. Rate-limit diagnostics and
  fix polarity, trigger, debounce, and electrical causes.
- These examples are educational and require a real binding schema, hardware
  review, and fail-safe policy before production use.

## Recommended study order

1. Read `docs/electrical-signals.md` and map logical assertion to pin voltage.
2. Read `docs/linux-gpio-architecture.md` and separate pinctrl, GPIO, and IRQ roles.
3. Adapt `device-tree/demo-gpio-irq.dts` to the actual controller and line offsets.
4. Inspect all line names and consumers with `gpioinfo`.
5. Monitor an unclaimed input with `gpiomon` or `monitor_gpio_v2`.
6. Build the platform driver and inspect its sysfs state/event counters.
7. Compare the custom driver with the standard `gpio-keys` solution.
8. Inject bounce, missing pull-up, wrong polarity, and interrupt-storm faults.
9. Map the signal into OpenBMC inventory, button, power, or fault policy.

## References

- Linux kernel GPIO documentation: <https://docs.kernel.org/driver-api/gpio/>
- GPIO character-device ABI: <https://docs.kernel.org/userspace-api/gpio/chardev.html>
- libgpiod: <https://libgpiod.readthedocs.io/>
- Device Tree GPIO bindings: `Documentation/devicetree/bindings/gpio/`
- OpenBMC phosphor-gpio-monitor: <https://github.com/openbmc/phosphor-gpio-monitor>
- OpenBMC phosphor-buttons: <https://github.com/openbmc/phosphor-buttons>

