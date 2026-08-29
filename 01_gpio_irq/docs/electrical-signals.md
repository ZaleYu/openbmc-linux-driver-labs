# GPIO Electrical Signals

## GPIO is not a protocol

A GPIO line represents a digital electrical state. Software still needs the
board contract: voltage domain, input/output direction, polarity, pull resistor,
drive type, timing, reset value, ownership, and behavior during power changes.
The same logical function can be wired very differently across boards.

## Input, output, and high impedance

- **Input:** the controller samples the pin without intentionally driving it.
- **Push-pull output:** actively drives both low and high.
- **Open-drain output:** actively drives low and releases the line for a pull-up.
- **Open-source output:** actively drives high and releases the line for a pull-down.
- **High impedance:** the pin is released; an external circuit establishes its level.

Never infer electrical capability from a software name. A line routed through
an I2C GPIO expander, CPLD, level shifter, or open-drain network has different
latency and drive constraints from a native SoC pin.

## Physical versus logical value

Device Tree polarity converts physical voltage into a logical assertion:

| Declaration | Physical low | Physical high |
|---|---|---|
| `GPIO_ACTIVE_HIGH` | logical 0/inactive | logical 1/active |
| `GPIO_ACTIVE_LOW` | logical 1/active | logical 0/inactive |

Descriptor APIs such as `gpiod_get_value_cansleep()` and libgpiod normally
operate on logical values. Raw APIs bypass polarity and should be reserved for
cases that truly require physical levels.

For an active-low reset, setting logical `1` asserts reset and may drive the pin
low. Name variables `asserted` or `present` instead of `high` or `low` when
working in the logical domain.

## Pull-up, pull-down, and floating inputs

An input without a defined source can float and randomly cross the input
threshold. External pull resistors generally provide a reliable default. Some
controllers expose configurable internal bias, but it may be weak, unavailable
on a selected pin, or overridden by pinctrl.

A logic analyzer may decode a floating line as valid transitions. Use an
oscilloscope or meter to verify the voltage and transition quality.

## Edges and levels

- **Rising edge:** physical/logical transition from 0 to 1 in the selected domain.
- **Falling edge:** transition from 1 to 0.
- **Both edges:** report every change.
- **Level high/low:** interrupt remains asserted while the level condition holds.

IRQ trigger type and GPIO active polarity are related but not identical. An
active-low presence signal is physically falling when a module becomes present
and physically rising when removed. If software requests logical edge events,
verify how the selected kernel/userspace interface reports active-low lines.

Level interrupts require the source to be cleared or deasserted before the IRQ
is unmasked. Otherwise the handler can immediately retrigger.

## Mechanical bounce and noisy signals

Buttons and contacts can toggle repeatedly during one physical action. Long
wires, weak pull-ups, crosstalk, or slow edges may create similar symptoms.
Debounce options include:

- Hardware filtering in the GPIO controller.
- RC/Schmitt-trigger circuitry.
- Kernel or userspace time filtering.
- State confirmation after a delay.

Debounce is not a substitute for fixing an electrically invalid signal. Choose
the interval from signal requirements; do not silently discard safety events.

## Power sequencing concerns

BMC GPIOs often cross independent power domains. Review:

- Whether the source is powered when the BMC boots.
- Back-powering through protection diodes.
- Default pinmux and direction before Linux probes.
- Bootloader output level and handoff to Linux.
- Whether reset must remain asserted until clocks/rails stabilize.
- What happens when the host resets but the BMC stays alive.
- Whether BMC reset releases a power-enable or reset line unexpectedly.

For critical outputs, the safe state should be established by hardware and
boot firmware before a Linux consumer driver becomes available.

