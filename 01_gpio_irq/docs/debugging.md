# GPIO and IRQ Debugging

Debug the signal from schematic to policy. A missing OpenBMC action does not
prove that the GPIO controller or interrupt is broken.

## 1. Establish the hardware contract

Record:

- SoC/CPLD/expander and local line offset.
- Package ball, pinmux group, and board net name.
- Input/output/open-drain behavior.
- Active polarity, pull resistor, voltage domain, and safe default.
- Source power state and reset behavior.
- Expected edge/level and maximum event rate.
- Whether the line is shared with host firmware or another device.

## 2. Check pinctrl, GPIO, and ownership

```sh
gpiodetect
gpioinfo
cat /sys/kernel/debug/gpio 2>/dev/null
find /sys/kernel/debug/pinctrl -maxdepth 2 -type f 2>/dev/null
```

`/sys/kernel/debug/gpio` is diagnostic output, not a stable application ABI.
Pinctrl debugfs filenames are provider-specific.

Interpretation:

- No gpiochip: provider driver, clock/reset, DTS, or kernel-config issue.
- GPIO line absent/wrong name: controller range or `gpio-line-names` issue.
- Line is busy: identify the legitimate consumer before changing anything.
- Correct owner, wrong direction: consumer flags or pinctrl/provider problem.
- Correct logical value, wrong physical voltage: polarity misunderstanding or
  electrical/pad configuration issue.

## 3. Check platform-driver binding

```sh
ls -l /sys/bus/platform/drivers/demo_gpio_irq
dmesg | grep -Ei 'gpio|irq|pinctrl|demo_gpio|storm|spurious'
```

If the Device Tree node exists but the driver does not bind, check `compatible`,
module aliases, probe errors, and whether another consumer already holds a line.

## 4. Check IRQ registration and counts

```sh
cat /proc/interrupts
watch -n 0.5 cat /proc/interrupts
cat /proc/irq/IRQ_NUMBER/spurious 2>/dev/null
```

Look for:

- IRQ count never increments: pinmux, mapping, trigger, mask, parent IRQ, or no
  physical transition.
- Count increments but handler state is unchanged: bounce, wrong line, wrong
  polarity, or signal changed again before the thread read it.
- Count grows continuously: level source not cleared, floating input, bad pull,
  wrong trigger, or noisy hardware.
- Multiple GPIO lines share one parent IRQ: inspect provider pending/mask logic.

## 5. Enable focused logging/tracing

Dynamic debug, when enabled:

```sh
echo 'module demo_gpio_irq +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
echo 'file drivers/gpio/* +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
```

Available tracepoints vary:

```sh
grep -Ei 'gpio|irq' /sys/kernel/tracing/available_events 2>/dev/null
grep -Ei 'gpio|irq' /sys/kernel/debug/tracing/available_events 2>/dev/null
```

Use narrow filters. Logging every bounce in an interrupt storm can worsen the
problem and hide the first failure.

## 6. Measure the physical signal

Use a meter for static voltage, a logic analyzer for digital timing, and an
oscilloscope for rise/fall time, ringing, thresholds, and noise. Correlate an
edge timestamp with `/proc/interrupts` and application logs.

Verify the line at both sides of level shifters or isolators. A correct source
signal may not reach the BMC pad.

## 7. Sleep-capable expanders

GPIOs behind I2C/SPI expanders have bus latency and may use one parent interrupt
for many lines. Check:

- Parent IRQ wiring and trigger.
- Expander interrupt-status and mask registers.
- I2C/SPI errors and bus recovery.
- Whether the consumer incorrectly uses non-sleeping GPIO access in hard IRQ.
- Whether rapidly changing inputs exceed service bandwidth.

## 8. OpenBMC layer

```sh
systemctl --failed
journalctl -b | grep -Ei 'gpio|button|presence|power|fault'
busctl list | grep -Ei 'button|state|inventory'
```

Service and bus names vary by image. Determine which process owns the GPIO and
which D-Bus object or systemd target should change.

## Failure matrix

| Failure | Evidence | Next check |
|---|---|---|
| Wrong pinmux | GPIO reads fixed/wrong; pad in alternate function | pinctrl state and SoC pad register |
| Wrong polarity | Electrical state correct, logical state inverted | `GPIO_ACTIVE_LOW`, double inversion |
| Floating input | Random edges, unstable voltage | external/internal bias, wiring |
| Bounce | Cluster of edges around one action | hardware/software debounce |
| IRQ not mapped | `gpiod_to_irq()` error | provider IRQ support and parent IRQ |
| Wrong trigger | Initial event then storm/missing release | edge/level type and source clearing |
| Busy line | libgpiod returns `EBUSY` | `gpioinfo` consumer and kernel binding |
| Userspace failure | correct kernel counter/value, no action | daemon config, D-Bus, systemd target |

`../scripts/debug_gpio_irq.sh CHIP [LINE]` collects a non-destructive report. It
does not request, drive, or monitor the selected line.

