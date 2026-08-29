# Linux GPIO and IRQ Architecture

## Layer model

```text
Pin/pad hardware
  -> pinctrl/pinmux driver
  -> GPIO controller driver (struct gpio_chip)
  -> gpiolib descriptor (struct gpio_desc)
  -> consumer driver or GPIO character device

GPIO line capable of interrupt
  -> gpio_chip IRQ integration / struct irq_chip / irqdomain
  -> Linux IRQ number
  -> request_irq() or devm_request_threaded_irq()
  -> consumer interrupt handler
```

### Pinctrl

Selects whether a physical pad is GPIO or an alternate function and may
configure bias, drive strength, slew rate, and input enable. A valid GPIO Device
Tree reference does not guarantee that the pad mux is configured correctly.

### GPIO controller and gpiolib

The provider driver registers a `struct gpio_chip`. Each chip exports local line
offsets, direction/value operations, optional configuration, and sometimes IRQ
support. Consumers should use descriptors rather than deprecated system-global
integer GPIO numbers.

### IRQ controller integration

Many SoC GPIO controllers also provide an `irq_chip` and irqdomain. Other GPIO
providers, especially expanders, may deliver nested interrupts through a parent
IRQ. GPIO and IRQ are separate namespaces; `gpiod_to_irq()` maps a requested
input descriptor to a Linux IRQ number.

## Consumer API

The demo driver uses:

- `devm_gpiod_get(dev, "presence", GPIOD_IN)` for `presence-gpios`.
- `devm_gpiod_get_optional()` for optional `fault-gpios` and `reset-gpios`.
- `gpiod_get_value_cansleep()` so native and sleep-capable expanders work.
- `gpiod_set_value_cansleep()` for the reset output.
- `gpiod_to_irq()` to obtain Linux IRQ mappings.
- `devm_request_threaded_irq()` for callbacks allowed to sleep.
- `gpiod_set_debounce()` when the provider supports hardware debounce.

Use `gpiod_get_value()` only when the GPIO provider is guaranteed not to sleep.
A threaded IRQ is the safer teaching model for GPIOs that may sit behind I2C/SPI
expanders. A hard IRQ handler must not call sleepable accessors.

## IRQ execution model

```text
Electrical transition
 -> GPIO controller latches status
 -> parent IRQ/irqdomain dispatch
 -> hard-IRQ top half (optional/minimal)
 -> threaded handler
 -> read logical state
 -> update state/counter and wake consumer
```

The demo passes `NULL` as the primary handler and uses `IRQF_ONESHOT`, so the
IRQ core runs the threaded handler while keeping the interrupt line serialized.

## Edge versus level implementation

Edge-triggered interrupts record transitions but can overflow or lose events if
software cannot service them fast enough. Level-triggered interrupts preserve
the asserted condition but require the source to be cleared. Choose the trigger
from the hardware contract, not convenience.

With both-edge emulation, some controllers reprogram the next polarity after
each interrupt. A transition during that window can be missed. Read the exact
controller implementation when event loss matters.

## Where to read source code

Start in the exact Linux revision selected by the OpenBMC Yocto build:

```text
drivers/gpio/
drivers/pinctrl/
drivers/irqchip/
drivers/input/keyboard/gpio_keys.c
drivers/leds/leds-gpio.c
drivers/gpio/gpiolib*.c
include/linux/gpio/consumer.h
include/linux/gpio/driver.h
include/linux/interrupt.h
Documentation/driver-api/gpio/
Documentation/devicetree/bindings/gpio/
```

For BMC SoCs, search the controller `compatible` string and inspect native GPIO
and SGPIO drivers, for example ASPEED or Nuvoton NPCM implementations:

```sh
rg 'vendor,soc-gpio' drivers/gpio drivers/pinctrl Documentation/devicetree/bindings
rg 'struct gpio_chip|gpio_irq_chip' drivers/gpio
rg 'devm_gpiochip_add_data' drivers/gpio
```

## What normally changes

| Requirement | First place to change |
|---|---|
| Route a pin to GPIO | Board pinctrl/pinmux DTS |
| Name/map a board signal | Consumer node, `*-gpios`, `gpio-line-names` |
| Add a button | Standard `gpio-keys` node/configuration |
| Add presence/fault policy | OpenBMC monitor configuration or consumer driver |
| Add a safe default output | Real device driver; GPIO hog only when appropriate |
| Support new SoC GPIO hardware | GPIO/pinctrl provider driver and binding |
| Fix IRQ trigger/mask/ack | GPIO irqchip/provider driver |
| Fix expander latency | Expander driver, parent IRQ, bus, and consumer design |

Do not change the SoC GPIO controller driver for each new board signal. Most
board work belongs in pinctrl, Device Tree consumers, and OpenBMC policy.

