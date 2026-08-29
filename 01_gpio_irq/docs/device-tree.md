# Device Tree for GPIO and IRQ

## Provider and consumer

A GPIO controller is a provider. A device using its lines is a consumer.
Consumer properties normally follow `<function>-gpios`:

```dts
board-monitor {
    compatible = "demo,gpio-irq-monitor";
    presence-gpios = <&gpio 17 GPIO_ACTIVE_LOW>;
    fault-gpios = <&gpio 27 GPIO_ACTIVE_LOW>;
    reset-gpios = <&gpio 22 GPIO_ACTIVE_LOW>;
};
```

The driver requests `"presence"`, `"fault"`, and `"reset"`; gpiolib finds the
matching properties. The GPIO specifier cells are provider-specific. They
usually include a local line offset and flags, but a BMC controller may provide
macros such as an ASPEED port/bit mapping.

## Pinctrl versus GPIO

Pinctrl selects the pad function and electrical configuration. GPIO properties
connect consumers to already available GPIO providers. A typical board also
needs a pinctrl state:

```dts
&pinctrl {
    /* Provider-specific pin groups live here or in the SoC DTSI. */
};

&gpio {
    status = "okay";
};
```

Exact syntax is SoC-specific. Copying GPIO offsets from another board without
checking pinmux and package routing is unsafe.

## Active polarity

Use `GPIO_ACTIVE_LOW` when assertion is electrically low. The descriptor API
then returns logical values. Do not compensate twice by inverting both Device
Tree and driver code.

Signal names ending in `_N`, `_L`, or `#` are useful hints but the schematic
and component specification are authoritative.

## `gpio-keys`

Buttons and discrete event inputs often fit the standard `gpio-keys` binding.
It publishes Linux input events and already handles debounce and wakeup
properties. Prefer it over a custom driver when the semantics match.

See `../device-tree/gpio-keys-example.dts` for power, reset, and identify
buttons. OpenBMC services may monitor the input device or use libgpiod-based
configuration, depending on platform architecture.

## GPIO hogs

A GPIO hog claims a provider line automatically and establishes input or output
state without a consumer driver. Hogs are useful for simple board straps or
safe defaults that have no functional owner, but they have limitations:

- The line is not available to a later consumer.
- Sequencing, error handling, and runtime policy are limited.
- The initial level may still depend on bootloader and pre-kernel hardware state.

Use a real device driver for resets, regulators, enables, or sequencing that
belong to a managed component. See `../device-tree/gpio-hog-example.dts`.

## GPIO line names

`gpio-line-names` belongs to the GPIO provider node and assigns semantic names
to line offsets. libgpiod and OpenBMC monitors can select lines by name, avoiding
fragile global numbering. Names should be unique enough for system-wide lookup.

Line names describe board nets; consumer labels shown by `gpioinfo` describe
the process or driver currently holding them. They are different concepts.

## Interrupt properties versus GPIO-to-IRQ

Some devices specify an interrupt directly:

```dts
interrupt-parent = <&gpio>;
interrupts = <17 IRQ_TYPE_EDGE_BOTH>;
```

Other GPIO consumers obtain a descriptor from `presence-gpios` and call
`gpiod_to_irq()`. Do not describe and request the same line through two
independent ownership paths unless the binding explicitly defines that model.

## Validation

Production nodes require a real YAML binding and validation:

```sh
make dt_binding_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/.../vendor,device.yaml
make dtbs_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/.../vendor,device.yaml
```

The included overlays use generic labels and line offsets. Adapt them to the
target board, preprocess the dt-bindings includes through the kernel build, and
validate against the real controller and consumer schemas.

