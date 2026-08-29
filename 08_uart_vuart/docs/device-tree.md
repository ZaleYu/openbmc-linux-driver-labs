# Device Tree

## Physical UART and serdev child

    &uart3 {
        status = "okay";
        current-speed = <115200>;

        management-mcu {
            compatible = "openai,demo-uart-mcu";
        };
    };

The controller node owns MMIO, IRQ, clocks, reset, DMA and pinctrl. A fixed
peripheral is a child node. Do not also run getty or open the same UART through
a competing userspace service.

## Boot console

`/chosen/stdout-path` selects firmware/kernel console routing. Linux command
line `console=ttyS4,115200n8` may add or override console behavior depending on
boot flow. Keep aliases stable when bootloader and kernel both rely on serial
numbers.

## ASPEED VUART

AST2600 SoC DTS nodes use the compatible `aspeed,ast2500-vuart` with register,
interrupt and clock resources supplied by the SoC include. A board normally
enables the existing `&vuart1` node and supplies the host-facing LPC address and
SIRQ description.

    &vuart1 {
        status = "okay";
        aspeed,lpc-io-reg = <0x3f8>;
        aspeed,lpc-interrupts = <4 IRQ_TYPE_LEVEL_LOW>;
    };

The address, SIRQ and polarity must match host firmware and platform routing.
Do not copy `0x3f8/IRQ4` merely because it is conventional.

