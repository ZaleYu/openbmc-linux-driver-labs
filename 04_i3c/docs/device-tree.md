# I3C Device Tree

The Device Tree describes hardware information that Linux cannot obtain through automatic discovery alone, such as:

- The I3C controller’s MMIO, IRQ, clock, reset, and pin control.
- The bus’s I3C/I2C clock frequencies.
- The PID and Static Address of known I3C targets.
- A preferred Dynamic Address.
- Board-level GPIOs, power supplies, or other resources used by an I3C target.
- Legacy I2C targets sharing the I3C bus.
- The hardware topology of multiple controller-capable devices.

However, there is an important difference between I3C and traditional I2C:

> A native I3C target can be discovered automatically through Dynamic Address Assignment, so it does not necessarily require a child node in the Device Tree.

---

## 1. Understanding the Relationship Between Discoverability and the Device Tree

### Traditional I2C

I2C has no standardized device-discovery mechanism. Linux normally needs the Device Tree to tell the kernel:

```text
Which I2C Bus
    +
Which Address
    +
Which Type of Device
```

For example:

```dts
sensor@48 {
    compatible = "vendor,temp-sensor";
    reg = <0x48>;
};
```

Here, `compatible` is used to match the I2C driver, while `reg` specifies the fixed I2C address `0x48`.

### Native I3C

A native I3C target can return the following during `ENTDAA`:

```text
PID + BCR + DCR
```

The controller then assigns it a Dynamic Address, and the Linux I3C core matches a driver based on the discovered device identity.

The conceptual flow is:

```text
I3C Controller Starts
        |
        v
Execute ENTDAA
        |
        v
Target Returns PID, BCR, and DCR
        |
        v
Controller Assigns a Dynamic Address
        |
        v
Linux Creates an I3C Device
        |
        v
Match an I3C Target Driver by PID
```

Therefore, Linux may discover a native I3C target and bind its driver even when no Device Tree child node exists.

For example, the earlier demo driver uses:

```c
static const struct i3c_device_id demo_i3c_ids[] = {
    I3C_DEVICE(0x0123, 0x0456, NULL),
    { }
};
```

The driver matches primarily by Manufacturer ID `0x0123` and Part ID `0x0456`, rather than by a Device Tree `compatible` property.

---

## 2. When Is an I3C Target Node Still Needed?

Although a native I3C target can be discovered automatically, it may still need to be described in the Device Tree in the following situations.

### Board-Level Resources Are Required

For example, the target may also be connected to:

- A reset GPIO.
- An alert GPIO.
- An enable GPIO.
- A voltage regulator.
- A reference clock.
- A specific power sequence.

This information is not included in PID, BCR, or DCR, so Linux cannot obtain it through I3C discovery alone.

### The Target Has a Known Static Address

Some I3C targets have a Static Address before Dynamic Address Assignment. The controller can use that Static Address to execute `SETDASA` for the device.

### A Specific Dynamic Address Is Preferred

The `assigned-address` property can specify a preferred initial Dynamic Address.

However, it is only a preference, not a permanent identity.

### The PID Must Be Mapped to Board-Level Topology

For example, OpenBMC may need to know whether a sensor is located on the:

```text
Main Board
CPU Board
DIMM Slot
Fan Board
GPU Tray
Power Backplane
```

The PID identifies “which device this is,” but it does not necessarily express “where the device is physically installed.”

### Legacy I2C Targets Are Present on the Bus

An I2C target cannot report a PID automatically through I3C DAA. Therefore, a Device Tree node is normally still required to describe:

- `compatible`
- I2C address
- Legacy Virtual Register
- Device-specific properties

---

## 3. The Device Tree Does Not Perform DAA

The Device Tree is a hardware description, not an executable procedure.

It does not itself:

- Send `ENTDAA`.
- Assign Dynamic Addresses.
- Perform I3C transfers.
- Enable IBI.
- Control Controller Role Handoff.

These tasks are performed together by:

```text
Linux I3C Core
        +
I3C Controller Driver
        +
I3C Target Driver
```

The Device Tree primarily tells Linux:

```text
What the hardware is
How the hardware is connected
Which board-level restrictions exist
Which initial configuration is preferred
```

---

# 4. Address Cells on an I3C Bus

An I3C controller node normally uses:

```dts
#address-cells = <3>;
#size-cells = <0>;
```

## `#address-cells = <3>`

This means that the `reg` property of every child node under the controller consists of three 32-bit cells:

```dts
reg = <cell0 cell1 cell2>;
```

Each cell is a 32-bit value.

Why are three cells needed?

An I3C target’s identity is more than a 7-bit address. It may also need to describe:

- A Static Address.
- A 48-bit PID.

One cell contains only 32 bits and cannot hold the complete 48-bit PID, so multiple cells are required.

## `#size-cells = <0>`

This means that a child device has no memory size to describe.

An MMIO device commonly uses:

```dts
reg = <base-address size>;
```

An I3C target is not a memory-mapped address range, so it does not require a size cell.

---

# 5. The `reg` Property of a Native I3C Target

A native I3C target uses three cells in `reg`:

```dts
reg = <static-address pid-upper pid-lower>;
```

| Cell | Contents |
| --- | --- |
| Cell 0 | Static Address; use `0` if no Static Address exists |
| Cell 1 | Upper PID portion, including the Manufacturer ID shifted left by one bit |
| Cell 2 | Lower PID portion, containing the Part ID, Instance ID, and Extra Information |

Conceptually:

```text
Cell 0          Cell 1                 Cell 2
+------------+  +-------------------+  +-----------------------------+
| Static Addr|  | Manufacturer Info |  | Part + Instance + Extra Info|
+------------+  +-------------------+  +-----------------------------+
```

This describes:

```text
The Device’s Static Address
        +
The Device’s Stable PID Identity
```

It does not describe the Dynamic Address currently used at runtime.

---

## 6. PID Example

Assume that a fictional I3C sensor has the following information:

```text
Manufacturer ID   = 0x0123
Part ID           = 0x0456
Instance ID       = 1
Extra Information = 0x004
Static Address    = None
```

The Device Tree can be written as:

```dts
sensor@0,24604561004 {
    reg = <0x0 0x246 0x04561004>;
    assigned-address = <0x30>;
};
```

The cells are explained below.

### Cell 0: Static Address

```dts
0x0
```

This indicates that the target has no usable Static Address.

If the actual target has a Static Address, enter the correct value according to the datasheet and binding schema.

### Cell 1: Upper PID Portion

```dts
0x246
```

The Manufacturer ID is:

```text
0x0123
```

According to the I3C Device Tree encoding, shift it left by one bit:

```text
0x0123 << 1 = 0x0246
```

Therefore, the second cell is:

```dts
0x246
```

> This shift is defined by the Device Tree binding’s PID encoding. It is not chosen arbitrarily by the driver.

### Cell 2: Lower PID Portion

```dts
0x04561004
```

This cell combines:

```text
Part ID           = 0x0456
Instance ID       = 0x1
Extra Information = 0x004
```

Conceptually:

```text
0x0456 1 004
   |   |  |
   |   |  +-- Extra Information
   |   +----- Instance ID
   +--------- Part ID
```

The result is:

```text
0x04561004
```

The complete three-cell value is:

```dts
reg = <0x0 0x246 0x04561004>;
```

---

# 7. Unit Address in the Node Name

The example node name is:

```dts
sensor@0,24604561004
```

The portion after `@` is the Unit Address. It must be consistent with the address/identity information described by `reg`.

The conceptual mapping is:

```text
Node name:
sensor@0,24604561004

reg:
<0x0 0x246 0x04561004>
```

This can be understood as:

```text
0           --> Static Address
246         --> PID Upper
04561004    --> PID Lower
```

The actual Unit Address format must follow the I3C binding schema and the results from `dtc`/`dt-schema` validation. Do not invent a different naming convention.

---

# 8. `assigned-address`

```dts
assigned-address = <0x30>;
```

`assigned-address` means:

```text
Prefer that the controller assign Dynamic Address 0x30
```

It does not declare:

```text
This device will always be located at 0x30
```

Whether the controller ultimately uses `0x30` may still be affected by:

- Another device already using `0x30`.
- Address reservation.
- The controller driver’s allocation policy.
- Bus reinitialization.
- DAA being performed again.
- Target Hot-Join.
- A change in controller role.
- Bus recovery.

The correct concepts are:

| Information | Stability | Purpose |
| --- | --- | --- |
| PID | Relatively stable | Identifies the target |
| Static Address | Determined by hardware or configuration | Initialization or `SETDASA` |
| `assigned-address` | Preference | Suggests an initial Dynamic Address |
| Dynamic Address | Runtime data | Communication during the current bus session |

An application should not hard-code:

```text
The sensor is always at Dynamic Address 0x30
```

Let the Linux I3C core and target driver manage the current Dynamic Address.

---

# 9. Complete Native I3C Target Example

The following is a simplified controller and sensor example:

```dts
&i3c0 {
    status = "okay";

    #address-cells = <3>;
    #size-cells = <0>;

    i3c-scl-hz = <12500000>;
    i2c-scl-hz = <1000000>;

    sensor@0,24604561004 {
        reg = <0x0 0x246 0x04561004>;
        assigned-address = <0x30>;
    };
};
```

The properties have the following purposes:

| Property | Purpose |
| --- | --- |
| `status = "okay"` | Enables the controller |
| `#address-cells = <3>` | Child `reg` uses three cells |
| `#size-cells = <0>` | Children have no size cell |
| `i3c-scl-hz` | Target clock frequency for the I3C bus |
| `i2c-scl-hz` | Clock frequency for legacy I2C devices on a mixed bus |
| `reg` | Describes the target’s Static Address and PID |
| `assigned-address` | Preferred initial Dynamic Address |

However, confirm the supported use and valid ranges of `i3c-scl-hz` and `i2c-scl-hz` against the I3C bus binding and controller binding in the kernel being used.

---

# 10. Is `compatible` Required?

A native I3C target driver normally matches primarily by the discovered PID, for example:

```c
I3C_DEVICE(0x0123, 0x0456, NULL)
```

Therefore, a native I3C target node does not necessarily need:

```dts
compatible = "vendor,device";
```

This differs from a traditional I2C driver, which commonly relies on `compatible`.

Comparison:

```text
Legacy I2C Target
    |
    +--> Normally matched by compatible + fixed address

Native I3C Target
    |
    +--> Primarily matched by the discovered PID
```

If a specific binding requires `compatible` or defines additional properties, follow that device’s YAML schema instead of relying only on a generic example.

---

# 11. Legacy I2C Target on an I3C Bus

An I3C bus can connect to legacy I2C targets under certain conditions.

For example, an EEPROM at address `0x50` can be placed on an I3C bus:

```dts
#include <dt-bindings/i3c/i3c.h>

&i3c0 {
    status = "okay";

    #address-cells = <3>;
    #size-cells = <0>;

    eeprom@50 {
        compatible = "atmel,24c02";
        reg = <0x50 0x0 (I2C_FM | I2C_FILTER)>;
    };
};
```

The three cells of a legacy I2C child have different meanings from those of a native I3C target:

```dts
reg = <i2c-address zero lvr>;
```

| Cell | Contents |
| --- | --- |
| Cell 0 | 7-bit address of the legacy I2C target |
| Cell 1 | Always `0` |
| Cell 2 | Encoded Legacy Virtual Register (LVR) |

Therefore:

```dts
reg = <0x50 0x0 (I2C_FM | I2C_FILTER)>;
```

means:

```text
I2C Address = 0x50
PID Upper   = Not applicable; use 0
LVR         = I2C_FM | I2C_FILTER
```

---

# 12. What Is the Legacy Virtual Register?

The Legacy Virtual Register, or LVR, tells the I3C core about the electrical and speed limitations of a legacy I2C target.

It can describe information such as:

- The I2C speed suitable for the device.
- Whether the device has a longer spike filter.
- Whether it may restrict the operating mode of the entire mixed bus.

Example:

```dts
#include <dt-bindings/i3c/i3c.h>

reg = <0x50 0x0 (I2C_FM | I2C_FILTER)>;
```

Conceptually, this means the I2C target:

```text
Uses I2C Fast Mode–type restrictions
        +
Has a spike filter that must be considered
```

The actual macro definitions come from:

```dts
#include <dt-bindings/i3c/i3c.h>
```

Do not copy the following from another development board without verification:

```dts
I2C_FM | I2C_FILTER
```

First check:

- The I2C target datasheet.
- Maximum SCL frequency.
- Spike-filter behavior.
- Clock stretching.
- Voltage.
- Bus capacitance.
- The controller’s mixed-bus capabilities.
- The Linux binding schema.

An incorrect LVR may cause the I3C core to select unsuitable timing or a bus mode for the entire bus.

---

# 13. Comparing `reg` for Native I3C and Legacy I2C

| Device Type | Cell 0 | Cell 1 | Cell 2 |
| --- | --- | --- | --- |
| Native I3C Target | Static Address or `0` | PID Upper | PID Lower |
| Legacy I2C Target | Fixed 7-bit I2C Address | `0` | LVR |

Native I3C example:

```dts
sensor@0,24604561004 {
    reg = <0x0 0x246 0x04561004>;
    assigned-address = <0x30>;
};
```

Legacy I2C example:

```dts
eeprom@50 {
    compatible = "atmel,24c02";
    reg = <0x50 0x0 (I2C_FM | I2C_FILTER)>;
};
```

Although both use three cells, the third cell has a completely different meaning:

```text
Native I3C --> Part of the PID
Legacy I2C --> LVR
```

When reading DTS, first determine whether the child is a native I3C target or a legacy I2C target.

---

# 14. I3C Controller Node

A complete I3C controller node normally contains:

- MMIO register range.
- Interrupt.
- Input clock.
- Reset controller.
- Pinctrl.
- DMA.
- Power domain.
- SoC-specific properties.
- I3C/I2C bus properties.

A simplified conceptual example is:

```dts
i3c0: i3c@12340000 {
    compatible = "vendor,soc-i3c";
    reg = <0x12340000 0x1000>;
    interrupts = <0 42 4>;
    clocks = <&clock_controller 10>;
    resets = <&reset_controller 5>;

    pinctrl-names = "default";
    pinctrl-0 = <&i3c0_pins>;

    #address-cells = <3>;
    #size-cells = <0>;

    i3c-scl-hz = <12500000>;
    i2c-scl-hz = <1000000>;

    status = "okay";
};
```

This is only a conceptual example. All of the following are SoC-specific:

```dts
compatible
reg
interrupts
clocks
resets
pinctrl
dmas
power-domains
```

Do not copy them directly from the DTS for another SoC.

For example, I3C controllers from ASPEED, Nuvoton, NXP, or other SoC vendors may have completely different:

- Register addresses.
- IRQ numbers.
- Clock IDs.
- Reset IDs.
- DMA configurations.
- Private properties.

---

# 15. What Is `&i3c0`?

A standalone DTS file in the package may use:

```dts
&i3c0 {
    status = "okay";
};
```

`&i3c0` references an existing Device Tree label.

It means that another DTSI already contains:

```dts
i3c0: i3c@12340000 {
    ...
};
```

Here:

```dts
i3c0:
```

is the label.

Therefore:

```dts
&i3c0
```

does not create a new controller. It modifies an existing controller node.

If the board DTSI has no `i3c0` label, compilation may report an error similar to:

```text
Reference to non-existent node or label "i3c0"
```

In that case:

- Find the actual label in the SoC DTSI.
- Use the correct node reference.
- Or create the correct controller node in a complete DTS.

---

# 16. The Controller Binding Is the Primary Reference

Each I3C controller may have a dedicated YAML binding, normally under:

```text
Documentation/devicetree/bindings/i3c/
```

During development, confirm:

- The correct `compatible` string.
- Required clocks.
- Required resets.
- Number of IRQs.
- DMA channels.
- Supported bus frequencies.
- Required properties.
- `additionalProperties` restrictions.
- Child-node format.
- Controller-specific properties.

Do not add properties that the binding does not define, such as:

```dts
secondary;
```

unless the controller’s YAML schema explicitly defines that property.

A property cannot be used merely because its name appears reasonable. The kernel driver must actually parse it, and the binding must allow it.

---

# 17. Multi-Controller Topology

An I3C bus may contain multiple controller-capable devices, but normal transactions are controlled by the current Active Controller.

For example:

```text
Primary / Active Controller
           |
           +-- Temperature Sensor
           |
           +-- EEPROM
           |
           +-- Secondary Controller-Capable Device
```

From the current Active Controller’s perspective, the secondary controller-capable device also appears as an I3C device on the bus until it takes control.

Its PID can therefore be described:

```dts
secondary-controller@0,xxxxyyyyyyyy {
    reg = <0x0 0xXXXX 0xYYYYYYYY>;
};
```

This describes only:

```text
This I3C device exists on the shared bus
        +
It has a particular PID
```

This child node alone does not necessarily tell another SoC controller:

```text
Start in Secondary Controller mode
```

Configuration that registers an SoC controller as a Secondary Controller may reside in:

- That controller’s own Device Tree node.
- A controller-specific property.
- Boot firmware.
- Secure firmware.
- The bootloader.
- SoC register configuration.
- Platform policy.

Read the controller driver and YAML binding. Do not invent a generic property such as:

```dts
secondary;
```

If the binding does not define it, the property may:

- Be reported as an error by `dtbs_check`.
- Be completely ignored by the kernel driver.
- Mislead future maintainers into thinking that the function is enabled.

---

# 18. Device Tree Compilation and Schema Validation

Successful compilation with `dtc` alone is not enough.

`dtc` primarily checks:

- DTS syntax.
- Label references.
- Some structural issues.
- Some relationships between Unit Addresses and `reg`.

It may not fully determine:

- Whether a property complies with the I3C binding.
- Whether the meanings of `reg` cells are correct.
- Whether a `compatible` string requires a particular clock.
- Whether a required property is missing.
- Whether an undefined property is used.

Device Tree schema validation is therefore also required.

## Validate the I3C Bindings

Run this in the target Linux kernel source tree:

```sh
make dt_binding_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/i3c/
```

This checks the I3C YAML bindings and their related examples.

## Validate the Actual DTB

```sh
make dtbs_check
```

If only a particular architecture or DTB needs validation, use a more specific kernel build target to avoid checking every platform.

Use the kernel tree actually adopted by the target project because different kernel versions may have different:

- Binding schemas.
- Supported properties.
- Controller drivers.
- I3C core behavior.

---

# 19. Runtime Validation

After the DTS passes compilation and schema validation, verify its behavior on the actual system.

## Examine I3C Devices

```sh
find /sys/bus/i3c/devices -maxdepth 2 -type f -o -type l
```

You can also list them directly:

```sh
ls -l /sys/bus/i3c/devices/
```

## Locate PID, BCR, DCR, and Dynamic Address

The attributes exposed may differ slightly among kernel versions. Search for them first:

```sh
find /sys/bus/i3c/devices \
    \( -name pid \
    -o -name bcr \
    -o -name dcr \
    -o -name dynamic_address \
    -o -name modalias \) -print
```

Read each file found, for example:

```sh
cat /sys/bus/i3c/devices/<device>/pid
cat /sys/bus/i3c/devices/<device>/bcr
cat /sys/bus/i3c/devices/<device>/dcr
cat /sys/bus/i3c/devices/<device>/dynamic_address
cat /sys/bus/i3c/devices/<device>/modalias
```

Confirm that:

- The runtime PID matches the DTS description.
- BCR/DCR match the datasheet.
- A Dynamic Address was assigned successfully.
- `assigned-address` was adopted if possible.
- The target driver matched successfully.

## Examine Driver Binding

```sh
readlink /sys/bus/i3c/devices/<device>/driver
```

If there is no `driver` symbolic link, possible causes include:

- The PID does not match any driver.
- The driver was not built into the kernel.
- The kernel module was not loaded.
- `MODULE_DEVICE_TABLE()` is incorrect.
- Driver probe failed.
- PID encoding in the Device Tree is incorrect.

## Examine the Kernel Log

```sh
dmesg | grep -i i3c
```

You can also monitor messages in real time:

```sh
sudo dmesg -w
```

Remember:

```text
A target is described in DTS
```

does not mean:

```text
The physical hardware necessarily exists or responds
```

A native I3C target must still complete protocol-level discovery, address assignment, and driver binding.

---

# 20. Validating an I2C Client on a Mixed Bus

A legacy I2C target normally appears in the Linux I2C device model:

```sh
ls -l /sys/bus/i2c/devices/
```

Confirm that:

- The I2C client is created.
- Its address is correct.
- `compatible` matches the driver.
- The I2C driver probes successfully.
- Bus frequency meets its restrictions.
- The LVR matches the actual hardware.
- The device does not use incompatible clock stretching or spike filtering.

The presence of a device in sysfs alone does not prove that its electrical design and communication are correct.

You must still validate:

- Actual reads and writes.
- Waveforms.
- Clock frequency.
- ACK/NACK.
- Reset.
- Power sequence.
- Mixed I3C and I2C operation.

---

# 21. Recommended Complete Validation Sequence

```text
1. Read the Schematic
        |
        v
2. Confirm the Controller YAML Binding
        |
        v
3. Confirm the I3C Target PID and Static Address
        |
        v
4. Confirm the Legacy I2C Target Address and LVR
        |
        v
5. Compile the DTS
        |
        v
6. Run dt_binding_check / dtbs_check
        |
        v
7. Boot and Confirm Controller Probe
        |
        v
8. Confirm ENTDAA and the Dynamic Address
        |
        v
9. Confirm PID, BCR, and DCR
        |
        v
10. Confirm Target Driver Binding
        |
        v
11. Test Private Transfers and IBI
        |
        v
12. Validate the Legacy I2C Client
        |
        v
13. Inspect Waveforms with an I3C Analyzer
```

---

# 22. Common Mistakes

## Putting the Dynamic Address in `reg`

Incorrect concept:

```text
The current Dynamic Address is 0x30,
so set the first reg cell to 0x30
```

The first cell for a native I3C target is the Static Address, not the current Dynamic Address.

Express a Dynamic Address preference through:

```dts
assigned-address = <0x30>;
```

## Treating `assigned-address` as a Permanent Address

`assigned-address` is only an initial value or preference. It cannot replace the PID.

## Describing a Native I3C Target with a Single-Cell I2C `reg`

Incorrect example:

```dts
sensor@30 {
    reg = <0x30>;
};
```

An I3C controller bus uses three address cells. A native I3C target must describe its Static Address and PID according to the binding.

## Interpreting Native I3C and Legacy I2C Target Cells Identically

Although both use three cells, their meanings differ:

```text
I3C: Static Address + PID
I2C: I2C Address + 0 + LVR
```

## Copying LVR Flags Directly

Select the LVR according to the actual I2C target’s electrical and timing characteristics.

## Inventing a Controller Property

For example:

```dts
secondary;
```

Use it only if both the binding and driver explicitly support it.

## Checking Only Whether the DTS Compiles

Successful DTS compilation does not prove that:

- The PID is correct.
- The target exists.
- DAA succeeds.
- The driver matches.
- IBI works.
- The mixed bus is electrically compatible.
- Waveforms meet the specification.

---

# 23. Mapping to the Demo I3C Sensor Driver

Device Tree example:

```dts
sensor@0,24604561004 {
    reg = <0x0 0x246 0x04561004>;
    assigned-address = <0x30>;
};
```

Demo driver ID table:

```c
static const struct i3c_device_id demo_i3c_ids[] = {
    I3C_DEVICE(0x0123, 0x0456, NULL),
    { }
};
```

The mapping is:

| Device Tree | Driver |
| --- | --- |
| Manufacturer ID `0x0123` encoded as `0x246` | `I3C_DEVICE(0x0123, ...)` |
| Part ID `0x0456` | `I3C_DEVICE(..., 0x0456, NULL)` |
| Instance ID `1` | The driver may distinguish it further if needed |
| Extra Information `0x004` | The driver may use it further if needed |
| `assigned-address = <0x30>` | Not a driver-matching condition |
| Runtime Dynamic Address | Managed by the I3C core |

The complete flow is:

```text
Device Tree Describes the Known PID and Board-Level Information
        |
        v
I3C Controller Performs Discovery / Address Assignment
        |
        v
Linux Obtains the Actual PID
        |
        v
Manufacturer ID 0x0123
Part ID 0x0456
        |
        v
Match the demo_i3c_sensor Driver
        |
        v
Call demo_i3c_probe()
```

---

# 24. Summary of Core Concepts

Beginners should remember the following points:

1. **A native I3C target can be discovered automatically and therefore does not necessarily require a Device Tree child node.**
2. **A target can still be described in the Device Tree when Linux needs its Static Address, known PID, preferred Dynamic Address, or board-level resources.**
3. **An I3C bus uses three address cells:**

   ```dts
   #address-cells = <3>;
   #size-cells = <0>;
   ```

4. **The `reg` property of a native I3C target is:**

   ```text
   Static Address + PID Upper + PID Lower
   ```

5. **The `reg` property of a legacy I2C target is:**

   ```text
   I2C Address + 0 + LVR
   ```

6. **`assigned-address` is only a preferred Dynamic Address and must not be treated as a permanent identity.**
7. **An I3C driver primarily matches the discovered PID instead of hard-coding a Dynamic Address.**
8. **A controller’s MMIO, IRQ, clock, reset, DMA, and Secondary Mode are SoC-specific and must follow the corresponding YAML binding.**
9. **Successful DTS compilation only indicates that its syntax or basic structure is valid. It does not prove that the hardware, electrical design, or I3C protocol works correctly.**
10. **Runtime validation must still verify PID, BCR, DCR, Dynamic Address, driver binding, private transfers, IBI, and mixed-bus I2C clients.**
