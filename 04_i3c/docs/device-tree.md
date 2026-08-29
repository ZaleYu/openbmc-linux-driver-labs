# Device Tree

## Discovery first

I3C targets support Dynamic Address Assignment and are discoverable, so they do
not always need Device Tree nodes. Describe one when Linux needs board-specific
resources, a known PID association, a static address, or a preferred dynamic
address. The target driver still matches the discovered I3C identity.

An I3C controller bus uses:

```dts
#address-cells = <3>;
#size-cells = <0>;
```

## I3C target `reg`

For an I3C target, the three cells mean:

1. static I2C address, or zero when none;
2. upper PID portion containing manufacturer ID shifted left by one;
3. lower PID portion containing part ID, instance ID, and extra information.

For the fictional manufacturer `0x0123`, part `0x0456`, instance `1`, and
extra information `0x004`:

```dts
sensor@0,24604561004 {
    reg = <0x0 0x246 0x04561004>;
    assigned-address = <0x30>;
};
```

`assigned-address` requests an initial/preferred dynamic address. The PID is
the stable identity; software must not assume `0x30` forever.

## Legacy I2C target on an I3C bus

An I2C child's three cells are its 7-bit address, zero, and an encoded Legacy
Virtual Register (LVR):

```dts
#include <dt-bindings/i3c/i3c.h>

eeprom@50 {
    compatible = "atmel,24c02";
    reg = <0x50 0x0 (I2C_FM | I2C_FILTER)>;
};
```

The LVR tells the I3C core about spike-filter and frequency limitations. Do not
copy flags without checking the I2C component datasheet.

## Controller node

The controller's MMIO, interrupts, clocks, reset, pinctrl, DMA, and compatibles
are SoC-specific. Generic bus properties include `i3c-scl-hz`, `i2c-scl-hz`,
three address cells, and zero size cells. Use the exact controller YAML binding.
The standalone files in this package are fragments and use labels such as
`&i3c0` that must exist in the board DTSI.

## Multi-master representation

A secondary controller is also visible to the active primary as an I3C device.
The example describes its PID on the shared bus. Configuration that makes a SoC
controller register as secondary is controller-specific and may be expressed
in another node or firmware. Do not invent a generic `secondary` DT property if
the controller binding does not define it.

## Validation

Use the target kernel tree:

```sh
make dt_binding_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/i3c/
make dtbs_check
```

Then verify runtime PID, BCR, DCR, dynamic address, driver binding, and mixed-bus
I2C clients in sysfs. DTS syntax success alone does not prove electrical or
protocol correctness.

