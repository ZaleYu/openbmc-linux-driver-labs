# Device Tree

The ADC provider describes controller hardware: MMIO, interrupts, clocks,
resets, reference supply, pinctrl and IIO phandle arguments.

    adc@1e6e9000 {
        compatible = "openai,demo-bmc-adc";
        reg = <0x1e6e9000 0x100>;
        #io-channel-cells = <1>;
    };

An `iio-hwmon` consumer selects provider channels:

    board-hwmon {
        compatible = "iio-hwmon";
        io-channels = <&adc 0>, <&adc 1>, <&adc 2>;
    };

The cell after `&adc` is interpreted by the provider. In this demo it is the
channel number. Use the actual SoC binding in production. A new board usually
changes channel wiring, divider/rescale nodes, labels and consumers—not the ADC
controller driver. Validate with `make dtbs_check`.

The current `iio-hwmon.yaml` binding permits only `compatible` and
`io-channels`. Do not add `io-channel-names` to that node. When labels are
required, verify how the actual IIO provider exposes channel labels and how the
target kernel's iio-hwmon driver consumes them.

Entity Manager JSON configures OpenBMC userspace; it does not replace DT
enumeration of a fixed ADC controller and its kernel consumers.
