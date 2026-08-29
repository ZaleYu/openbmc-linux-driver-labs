# Device Tree

Device Tree describes topology, not an operation sequence. A mux node is a
client of its parent I2C controller; each `i2c@N` child is a bus containing its
own endpoint nodes.

## Register-controlled shape

```dts
&i2c5 {
    status = "okay";

    i2c-mux@70 {
        compatible = "nxp,pca9548";
        reg = <0x70>;
        #address-cells = <1>;
        #size-cells = <0>;
        i2c-mux-idle-disconnect;

        i2c@0 {
            reg = <0>;
            #address-cells = <1>;
            #size-cells = <0>;

            eeprom@50 {
                compatible = "atmel,24c02";
                reg = <0x50>;
            };
        };
    };
};
```

Here, the child `reg` is a channel selector, while the endpoint `reg` is an I2C
address. They are different namespaces. Follow the exact YAML binding for the
compatible; do not invent properties accepted by no driver.

## Repeated addresses are valid

`sensor@48` can exist on channel 0 and channel 1 because only one isolated path
is active. Linux creates separate client objects under separate logical
adapters. The address must still be unique within each simultaneously connected
segment.

## GPIO-controlled mux

An `i2c-mux-gpio` is normally a platform node, not an I2C client. It references
the parent adapter with `i2c-parent`, the selector lines with `mux-gpios`, and
uses child `reg` values as GPIO selector states. `settle-time-us` permits analog
paths to stabilize. An `idle-state` must be a real safe hardware state.

## Nested muxes

A second mux is simply an I2C client below a first mux channel. Verify:

- both addresses are reachable only through the intended path;
- lock nesting matches both drivers' locking models;
- reset and idle-disconnect cannot make an interrupt or management path vanish;
- aliases and inventory labels remain comprehensible;
- worst-case selector overhead and fault recovery are acceptable.

Deep nesting is sometimes unavoidable, but a flat schematic is easier to
operate. Preserve a physical-location map in board documentation.

## Bus numbering and validation

Do not encode observed dynamic bus numbers in applications. Resolve the
`channel-*` sysfs symlink or assign deliberate DT aliases when stable legacy
numbers are required. Validate a finished tree with the kernel's schema tools,
for example the target tree's `make dt_binding_check` and `make dtbs_check`.
The files here are fragments and require board-specific controller labels,
GPIO phandles, compatibles, and includes.

