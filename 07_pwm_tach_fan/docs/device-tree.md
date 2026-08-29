# Device Tree

The PWM provider's binding defines cells such as channel, period in nanoseconds
and flags:

    pwms = <&pwm0 2 40000 0>; /* channel 2, 25 kHz, normal polarity */

The current in-tree `pwm-fan` binding supports one PWM, an optional tach
interrupt, `pulses-per-revolution`, regulator, startup behavior,
`cooling-levels`, and `#cooling-cells = <2>`.

    fan0: pwm-fan {
        compatible = "pwm-fan";
        pwms = <&pwm0 2 40000 0>;
        interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
        pulses-per-revolution = <2>;
        cooling-levels = <80 120 170 220 255>;
        #cooling-cells = <2>;
    };

The two cooling cells in a reference identify minimum and maximum cooling
states. Thermal trips map temperature events to this cooling device.

Check PWM polarity against the external transistor and fan specification. Check
whether the tach interrupt controller supports the selected edge and whether
the signal is self-resetting. Never copy SoC channel numbers or periods without
checking the schematic and bindings.

The custom driver examples use `tach-gpios` because they teach
`gpiod_to_irq()`. That property belongs only to the fictional binding; the
in-tree `pwm-fan` binding uses `interrupts`.

