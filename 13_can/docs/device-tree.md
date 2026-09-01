# CAN Device Tree

The Device Tree describes CAN hardware that Linux cannot discover automatically, such as:

- CAN controllers.
- MMIO register ranges.
- SPI chip selects.
- Interrupts.
- Oscillators/clocks.
- Resets.
- Pin multiplexing.
- CAN transceivers.
- Regulators.
- Standby/enable GPIOs.
- Connections between controllers and transceivers.

The Device Tree does not describe:

- CAN message IDs.
- Payload formats.
- Byte order.
- Scaling.
- Message periods.
- Timeouts.
- Sequence counters.
- Application protocols.
- Command authorization.

These should be defined by CANopen, J1939, ISO-TP, or a project-specific application protocol.

---

# 1. Separation Between Hardware Description and Communication Protocol

Suppose the system uses CAN ID `0x123` to transmit a temperature:

```text
CAN ID 0x123
Bytes 0–1: Temperature
Byte order: Little-endian
Scale: 0.1°C
Period: 100 ms
Timeout: 500 ms
```

None of this belongs in the CAN controller's Device Tree node.

The Device Tree only needs to describe:

```text
Which CAN controller
        |
        v
Which clock, IRQ, and pins it uses
        |
        v
Which CAN transceiver it connects to
        |
        v
How the transceiver is powered and leaves standby
```

The responsibilities are divided as follows:

| Layer | Responsibilities |
| --- | --- |
| Device Tree | Controller, clock, IRQ, pinmux, transceiver, GPIO |
| CAN controller driver | Bit timing, TX/RX, interrupts, bus-off |
| SocketCAN | CAN sockets, filters, network devices |
| Application protocol | CAN ID, payload, scaling, timeout, counter |
| OpenBMC service | D-Bus, inventory, telemetry, policy, Redfish |

---

# 2. Basic CAN Hardware Components

Typical CAN hardware consists of:

```text
SoC
 |
 +-- CAN Controller
        |
        | TX/RX Logic
        v
   CAN Transceiver
        |
        | CAN_H/CAN_L
        v
      CAN Bus
```

## CAN Controller

Responsible for:

- Creating and parsing CAN frames.
- Arbitration.
- CRC.
- ACK.
- Bit timing.
- TX/RX FIFOs.
- Error counters.
- Bus-off handling.
- Interrupts.

## CAN Transceiver

Responsible for:

- Converting controller TX/RX logic signals to CAN_H/CAN_L.
- Receiving differential signals.
- Standby/silent mode.
- Wake-up, depending on the component.
- Voltage and physical-layer interfacing.

An SoC with an integrated CAN controller still cannot normally connect its pins directly to CAN_H/CAN_L. An external CAN transceiver is usually required.

---

# 3. Two Common Types of CAN Controller

Linux systems commonly use two CAN controller architectures.

## On-SoC CAN Controller

The CAN controller is integrated into the SoC:

```text
SoC CAN Controller
        |
        v
External CAN Transceiver
        |
        v
CAN_H/CAN_L
```

An example is the Bosch M_CAN IP core.

## External SPI CAN Controller

If the SoC has no CAN controller, or additional CAN channels are required, an SPI-to-CAN controller can be used:

```text
SoC SPI Controller
        |
        v
MCP2518FD
        |
        v
CAN Transceiver
        |
        v
CAN_H/CAN_L
```

The MCP2518FD is a CAN FD controller operated by the SoC over SPI.

The Device Tree representations of these architectures are completely different:

| Architecture | Meaning of `reg` |
| --- | --- |
| On-SoC controller | MMIO base address and size |
| SPI CAN controller | SPI chip-select index |

---

# 4. On-SoC M_CAN Controller

`device-tree/m-can-controller.dts` demonstrates how to enable an M_CAN controller already declared in the SoC DTSI.

The SoC DTSI may first define:

```dts
m_can0: can@12340000 {
    compatible = "vendor,soc-m-can";
    reg = <0x12340000 0x1000>;

    interrupts = <0 42 4>;
    clocks = <&clock_controller 10>;

    status = "disabled";
};
```

The board DTS then enables it:

```dts
&m_can0 {
    status = "okay";
};
```

`&m_can0` does not create a new controller. It references and modifies an existing node in the SoC DTSI.

---

# 5. Why Is the SoC DTSI Set to `disabled` by Default?

An SoC may provide several CAN controllers, but not every board:

- Routes their pins out.
- Installs a transceiver.
- Provides the correct power rail.
- Provides a connector.
- Uses the function.

Therefore, the SoC DTSI normally begins with:

```dts
status = "disabled";
```

Only a board DTS for hardware that actually provides the function should change it to:

```dts
status = "okay";
```

If a board has no transceiver but the controller is forcibly enabled, Linux may still create `can0`, but communication over CAN_H/CAN_L will not work.

---

# 6. The `reg` Property of an On-SoC Controller

For an MMIO controller:

```dts
reg = <0x12340000 0x1000>;
```

This normally means:

```text
Base address = 0x12340000
Size         = 0x1000
```

This is the memory range through which the CPU accesses the CAN controller registers.

It is not:

- A CAN ID.
- A CAN bitrate.
- A CAN channel number.
- A transceiver address.
- A SocketCAN interface number.

---

# 7. `compatible`

```dts
compatible = "vendor,soc-m-can";
```

`compatible` selects the matching platform driver.

The matching process is:

```text
Device Tree compatible
        |
        v
Linux platform device
        |
        v
Driver of_match_table
        |
        v
CAN controller driver probe
        |
        v
Create CAN network device
```

Even if different SoCs use the Bosch M_CAN IP core, they may require:

- Different wrapper drivers.
- Different clocks.
- Different resets.
- Different message RAM arrangements.
- Different register layouts.
- Different power domains.

Therefore, do not copy another SoC's `compatible` merely because both pieces of hardware are labeled M_CAN.

---

# 8. M_CAN Message RAM

Some M_CAN controllers require message RAM to be described or configured.

Message RAM may be used for:

- Standard-ID filters.
- Extended-ID filters.
- RX FIFOs.
- RX buffers.
- TX event FIFOs.
- TX buffers.

Depending on the platform, message RAM may:

- Be integrated into the controller.
- Use shared SRAM.
- Require a Device Tree property to describe its allocation.
- Be configured automatically by a wrapper driver.

Whether an M_CAN configuration needs additional message-RAM properties must be determined from the M_CAN YAML binding in the target kernel.

Do not copy another SoC's message-RAM configuration directly. An incorrect allocation may cause:

- Overlapping TX/RX buffers.
- Driver probe failure.
- Lost frames.
- Memory corruption.
- CAN FD malfunctions.

---

# 9. Clock

CAN bit timing is calculated from the CAN controller clock.

The Device Tree may use:

```dts
clocks = <&clock_controller 10>;
clock-names = "cclk";
```

Some external controllers may instead use:

```dts
clock-frequency = <40000000>;
```

However, `clocks` and `clock-frequency` are not interchangeable properties.

## `clocks`

This indicates that the controller obtains its clock through the Linux Common Clock Framework:

```text
Clock provider
      |
      v
CAN controller
```

## `clock-frequency`

This may be used for a fixed clock or where a specific binding permits the frequency to be described directly.

Follow the controller binding. Do not add:

```dts
clock-frequency = <...>;
```

to an arbitrary node and assume the driver will use it.

---

# 10. Clock Is Not CAN Bitrate

Suppose the external oscillator is:

```text
40 MHz
```

This does not mean that the CAN bus bitrate is 40 Mbit/s.

The controller uses:

```text
40 MHz controller clock
        |
        v
Prescaler + time segments
        |
        v
500 kbit/s CAN bitrate
```

The bitrate is normally configured at runtime:

```sh
sudo ip link set can0 type can bitrate 500000
```

Linux calculates bit timing from the controller clock supplied by the Device Tree or Clock Framework.

If the Device Tree incorrectly specifies a 40 MHz clock as 20 MHz, Linux may calculate register values that produce the wrong physical bus speed.

---

# 11. Interrupt

```dts
interrupts = <...>;
```

The CAN controller uses an IRQ to notify Linux of:

- A received frame.
- TX completion.
- Error warning.
- Error-passive state.
- Bus-off state.
- RX FIFO overflow.
- Controller errors.
- Wake-up.

The number and format of interrupt cells are defined by the interrupt-controller binding.

For example, the format may be:

```text
Interrupt number + trigger type
```

or it may require:

```text
Interrupt type + number + flags
```

Do not simply copy an IRQ number from another platform.

---

# 12. Pinctrl

An on-SoC CAN controller must switch its pins to the CAN function:

```dts
&m_can0 {
    pinctrl-names = "default";
    pinctrl-0 = <&m_can0_pins>;

    status = "okay";
};
```

The pin group may contain:

```text
CAN_TX
CAN_RX
```

With an incorrect pinctrl configuration, Linux may still:

- Probe the driver successfully.
- Create `can0`.
- Accept `ip link set can0 up`.
- Increment software TX counters.

Nevertheless, the physical pins may carry no correct waveform.

Therefore, the existence of `can0` at runtime only proves that software initialization reached a certain point; it does not prove that the pinmux and physical signals are correct.

---

# 13. CAN Transceiver PHY

Some controller bindings connect the CAN controller to its transceiver through:

```dts
phys = <&can0_transceiver>;
phy-names = "can-phy";
```

Conceptually:

```text
&m_can0
   |
   | phys
   v
can0_transceiver
   |
   v
CAN_H/CAN_L
```

The transceiver node may describe:

- `compatible`.
- `#phy-cells`.
- `standby-gpios`.
- `enable-gpios`.
- `max-bitrate`.
- Power supplies.
- Wake-up behavior.
- SoC-specific controls.

The actual properties must comply with the transceiver binding.

---

# 14. Not Every Binding Uses `phys`

Different kernels and drivers may use:

```dts
phys = <&can_transceiver>;
```

or:

```dts
xceiver-supply = <&can_5v>;
```

They may place the following directly in the controller node:

```dts
standby-gpios = <...>;
```

They may also use a controller-specific property.

These approaches cannot be mixed arbitrarily.

First locate the following directory in the target kernel:

```text
Documentation/devicetree/bindings/net/can/
```

Then check:

- The CAN controller binding.
- The CAN transceiver binding.
- The SoC wrapper binding.
- Required and optional properties.

---

# 15. `standby-gpios`

Many CAN transceivers have a standby pin.

The Device Tree may describe it as:

```dts
standby-gpios = <&gpio0 10 GPIO_ACTIVE_HIGH>;
```

This means that the standby signal is active-high:

```text
GPIO high --> Transceiver standby
GPIO low  --> Transceiver normal mode
```

If the hardware is actually active-low but the property incorrectly specifies `GPIO_ACTIVE_HIGH`, Linux may keep the transceiver in standby continuously.

Common symptoms are:

```text
can0 exists
Bitrate configuration succeeds
The TX command reports no error
But CAN_H/CAN_L show no valid differential waveform
```

---

# 16. GPIO Polarity

Polarity should be expressed in the GPIO flags:

```dts
GPIO_ACTIVE_HIGH
GPIO_ACTIVE_LOW
```

Do not invent an additional property such as:

```dts
standby-active-low;
```

unless the binding explicitly defines it.

Verify every control pin on the transceiver:

| Pin | Possible function |
| --- | --- |
| STB | Standby |
| EN | Enable |
| S | Silent mode |
| SHDN | Shutdown |
| WAKE | Wake-up |

Similar names do not imply identical active levels.

Always cross-check:

```text
Transceiver datasheet
        +
Schematic
        +
GPIO inverter/transistor
        +
Device Tree binding
```

---

# 17. `xceiver-supply`

Some bindings use:

```dts
xceiver-supply = <&can_5v>;
```

This indicates that the CAN transceiver's power supply is managed through the regulator framework.

Conceptually:

```text
CAN driver
    |
    v
Regulator framework
    |
    v
CAN transceiver power rail
```

This may be used to:

- Power the transceiver during probe.
- Supply power when the interface is opened.
- Turn power off during suspend.
- Release the supply during removal.

A production design must still verify:

- Whether multiple transceivers share the rail.
- Whether disabling the rail affects another channel.
- Whether an unpowered transceiver affects the CAN bus.
- Power sequencing.
- Wake-up requirements.
- Fail-safe state.

---

# 18. Conceptual On-SoC Controller Example

```dts
#include <dt-bindings/gpio/gpio.h>

can0_transceiver: can-phy0 {
    compatible = "vendor,can-transceiver";
    #phy-cells = <0>;

    standby-gpios = <&gpio0 10 GPIO_ACTIVE_HIGH>;
    max-bitrate = <5000000>;
};

&m_can0 {
    pinctrl-names = "default";
    pinctrl-0 = <&m_can0_pins>;

    phys = <&can0_transceiver>;
    phy-names = "can-phy";

    status = "okay";
};
```

This is only an architectural example. Before using it, verify:

- The transceiver `compatible`.
- `#phy-cells`.
- `standby-gpios`.
- `max-bitrate`.
- `phy-names`.
- Whether the controller supports `phys`.

Do not put a fictitious `compatible` string into a production product.

---

# 19. External SPI CAN FD Controller

`device-tree/mcp2518fd-spi-can.dts` demonstrates the MCP2518FD.

Hardware architecture:

```text
SoC SPI Controller
        |
        | SCLK/MOSI/MISO/CS
        v
MCP2518FD CAN FD Controller
        |
        | TXCAN/RXCAN
        v
CAN Transceiver
        |
        v
CAN_H/CAN_L
```

The MCP2518FD is not a transceiver. It still requires an external CAN transceiver.

---

# 20. Child Node of an SPI CAN Controller

The MCP2518FD is an SPI peripheral, so it must be placed under an SPI controller:

```dts
&spi0 {
    status = "okay";

    can@0 {
        compatible = "microchip,mcp2518fd";
        reg = <0>;

        spi-max-frequency = <10000000>;

        interrupts = <...>;
        clocks = <&can0_osc>;

        status = "okay";
    };
};
```

Here:

```dts
reg = <0>;
```

means:

```text
SPI chip select 0
```

It does not mean:

- CAN ID 0.
- CAN channel 0.
- MMIO address 0.
- CAN bitrate.
- SocketCAN interface `can0`.

---

# 21. MCP2518FD Oscillator

The MCP2518FD requires an accurate clock source, such as an external crystal or oscillator.

The Device Tree may create a fixed clock:

```dts
can0_osc: can0-oscillator {
    compatible = "fixed-clock";
    #clock-cells = <0>;
    clock-frequency = <40000000>;
};
```

The MCP2518FD then references it:

```dts
clocks = <&can0_osc>;
```

This indicates a controller clock of:

```text
40 MHz
```

Verify:

- Whether the component uses a crystal or an oscillator.
- The actual frequency.
- Frequency tolerance.
- Load capacitance.
- Board population options.
- The supported range of the controller variant.
- The driver binding.

An incorrect clock frequency directly produces incorrect CAN bit timing.

---

# 22. Crystal Tolerance

Even when two nodes are both configured for:

```text
500 kbit/s
```

they may not communicate reliably if their clock errors are too large.

Consider:

- Oscillator accuracy.
- Temperature drift.
- Aging.
- CAN bit timing.
- SJW.
- Sample point.
- CAN FD data bitrate.
- Bus length.

The high-speed data phase of CAN FD is generally more sensitive to clock accuracy and signal integrity.

---

# 23. MCP2518FD Interrupt

The MCP2518FD uses an external interrupt pin to notify the SoC of:

- RX frames.
- TX completion.
- Errors.
- FIFO events.
- Bus-off state.
- Controller events.

The Device Tree may use:

```dts
interrupt-parent = <&gpio0>;
interrupts = <20 IRQ_TYPE_LEVEL_LOW>;
```

The actual trigger type must follow the datasheet, schematic, and binding.

Note that:

```text
Active-low
```

only describes the active level. It does not necessarily mean that a falling-edge trigger must be used.

If the interrupt pin remains low until the event is cleared, a level-low trigger is usually more appropriate than triggering only once on the high-to-low transition.

An incorrect IRQ type may cause:

- No further events after the first interrupt.
- An interrupt storm.
- RX frames stuck in the FIFO.
- Repeated driver timeouts.
- Lost interrupts under high load.

---

# 24. Interrupt Electrical Type

Also determine whether the interrupt pin is:

- Push-pull.
- Open-drain.
- Dependent on an external pull-up.
- Active-low.
- Pulsed.
- Level-based.

An open-drain signal without a pull-up may never return high.

If edge triggering is used for a signal that is actually level-based, event handling may also be unreliable.

Therefore, check the IRQ design as a whole:

```text
Chip datasheet
Schematic
GPIO input
Pull-up
Trigger type
Driver IRQ handling
```

---

# 25. `spi-max-frequency`

```dts
spi-max-frequency = <10000000>;
```

This means that the MCP2518FD SPI clock must not exceed:

```text
10 MHz
```

It is not the CAN bitrate.

The distinction is:

```text
SPI frequency
    |
    +--> Speed between the SoC and the MCP2518FD

CAN bitrate
    |
    +--> Speed between the MCP2518FD and the CAN bus
```

For example:

```text
SPI clock    = 10 MHz
CAN bitrate  = 500 kbit/s
CAN FD data  = 2 Mbit/s
```

These are three different parameters.

---
# 26. SPI Mode

The SPI mode used by the MCP2518FD must comply with its datasheet.

The SPI controller or peripheral node may configure the mode through:

```dts
spi-cpol;
spi-cpha;
```

If the datasheet requires mode 0, these two properties should normally not be added.

An incorrect SPI mode may cause:

- Incorrect device-ID reads.
- Driver probe failure.
- Shifted register values.
- Numerous SPI errors at high speed.
- Failure to initialize the CAN controller.

Do not copy mode 3 from another SPI device into an MCP2518FD configuration.

---

# 27. MCP2518FD Variants

The MCP251xFD family may include several variants.

The Device Tree's:

```dts
compatible
```

must match the component actually installed on the board.

For example, verify:

- MCP2517FD.
- MCP2518FD.
- Other compatible variants.
- Silicon revision.
- The driver's supported-device list.

Similar part numbers do not guarantee identical:

- RAM.
- ECC.
- Clocks.
- Registers.
- Features.
- Device errata.

For production use, consult the target kernel binding for the permitted `compatible` values.

---

# 28. Conceptual MCP2518FD Example

```dts
#include <dt-bindings/gpio/gpio.h>
#include <dt-bindings/interrupt-controller/irq.h>

can0_osc: can0-oscillator {
    compatible = "fixed-clock";
    #clock-cells = <0>;
    clock-frequency = <40000000>;
};

&spi0 {
    status = "okay";

    can@0 {
        compatible = "microchip,mcp2518fd";
        reg = <0>;

        spi-max-frequency = <10000000>;

        clocks = <&can0_osc>;

        interrupt-parent = <&gpio0>;
        interrupts = <20 IRQ_TYPE_LEVEL_LOW>;

        xceiver-supply = <&can_5v>;

        status = "okay";
    };
};
```

This example represents:

```text
SPI controller:    spi0
Chip select:       CS0
CAN controller:    MCP2518FD
Maximum SPI clock: 10 MHz
CAN clock:         40 MHz
IRQ:               GPIO0_20, level-low
Transceiver rail:  can_5v
```

Validate the actual properties against the target kernel schema.

---

# 29. Dual-Channel Topology

`device-tree/dual-can-transceivers.dts` demonstrates two CAN networks:

```text
CAN Controller 0 --> Transceiver 0 --> CAN Network A
CAN Controller 1 --> Transceiver 1 --> CAN Network B
```

For example:

```text
Network A --> Rack Management
Network B --> Power Shelf
```

Treat the two channels as independent failure domains.

---

# 30. Why Should Transceiver Standby Controls Be Separate?

The ideal architecture is:

```text
GPIO 10 --> CAN0 transceiver standby
GPIO 11 --> CAN1 transceiver standby
```

This permits independent control of:

- Power.
- Suspend behavior.
- Fault isolation.
- Maintenance.
- Bus recovery.
- Silent mode.

If two transceivers share one standby GPIO:

```text
GPIO 10
   |
   +--> CAN0 standby
   |
   +--> CAN1 standby
```

disabling one bus may also disable the other.

This may be unsafe when:

- The two CAN buses belong to different safety domains.
- One bus must continue to provide power control.
- A failed bus must be isolated while the other remains operational.
- Their suspend policies differ.
- Their transceiver power rails differ.

Whether a GPIO may be shared must be determined from the schematic and system safety design. Do not assume it is acceptable merely to save one GPIO.

---

# 31. Conceptual Dual-Channel Example

```dts
can0_phy: can0-phy {
    compatible = "vendor,can-transceiver";
    #phy-cells = <0>;
    standby-gpios = <&gpio0 10 GPIO_ACTIVE_HIGH>;
};

can1_phy: can1-phy {
    compatible = "vendor,can-transceiver";
    #phy-cells = <0>;
    standby-gpios = <&gpio0 11 GPIO_ACTIVE_HIGH>;
};

&can_controller0 {
    phys = <&can0_phy>;
    phy-names = "can-phy";
    status = "okay";
};

&can_controller1 {
    phys = <&can1_phy>;
    phy-names = "can-phy";
    status = "okay";
};
```

This illustrates the architecture only. Replace `compatible` and the properties with definitions from the real bindings.

---

# 32. A Linux Interface Number Is Not a Physical Identity

Linux may create:

```text
can0
can1
```

but their numbering can be affected by:

- Driver probe order.
- Built-in versus module drivers.
- SPI probe timing.
- Device Tree changes.
- Kernel versions.
- Addition of a third controller.
- Controller initialization failure.

For example, an original mapping may be:

```text
can0 --> Rack Bus
can1 --> Power Shelf
```

After an SPI CAN controller is added, it may become:

```text
can0 --> SPI CAN
can1 --> Rack Bus
can2 --> Power Shelf
```

Therefore, a production service should not simply assume:

```text
Always use can0 to control the rack
```

---

# 33. Stable Naming and Topology Identification

A stable mapping can be established using:

- Udev rules.
- Device paths.
- Device Tree aliases, after confirming driver and kernel behavior.
- Driver names.
- SPI bus and chip select.
- MMIO addresses.
- OF nodes.
- Platform-specific configuration.

For example, service configuration can describe:

```text
Rack CAN:
Controller Device Tree path = ...
Expected driver = ...
Expected transceiver = ...
```

The service can then resolve the corresponding network interface at startup.

Do not depend solely on discovery order.

---

# 34. Common Properties

| Property | Purpose |
| --- | --- |
| `compatible` | Selects the controller or transceiver driver |
| `reg` | MMIO range, or chip select for an SPI child |
| `interrupts` | Controller IRQ |
| `clocks` | References a clock provider |
| `clock-frequency` | Describes a fixed frequency in bindings that support it |
| `pinctrl-*` | CAN TX/RX pinmux |
| `phys` | Connects a CAN controller to a CAN transceiver PHY |
| `phy-names` | PHY name |
| `standby-gpios` | Controls transceiver normal/standby mode |
| `enable-gpios` | Controls transceiver enable, if supported by the binding |
| `xceiver-supply` | Transceiver power rail |
| `resets` | Controller reset |
| `power-domains` | Controller power domain |
| `status` | Board-level enablement |
| `spi-max-frequency` | Maximum SPI clock between the SoC and an external SPI CAN controller |

Not every binding supports every property in this table. The matching YAML schema is authoritative.

---

# 35. Two Meanings of `reg`

## MMIO CAN Controller

```dts
can@12340000 {
    reg = <0x12340000 0x1000>;
};
```

This means:

```text
MMIO base + size
```

## SPI CAN Controller

```dts
&spi0 {
    can@0 {
        reg = <0>;
    };
};
```

This means:

```text
SPI chip select 0
```

To interpret `reg`, first identify the node's parent bus.

---

# 36. The Device Tree Does Not Configure CAN IDs

Incorrect concept:

```dts
&can0 {
    can-id = <0x123>;
};
```

Unless a specific binding explicitly defines one, a general CAN controller binding has no such property.

CAN IDs are selected by the userspace application or a higher-level protocol:

```c
frame.can_id = 0x123;
```

or:

```sh
cansend can0 123#11223344
```

The Device Tree creates a usable `can0`; it does not decide which frames the application transmits.

---

# 37. The Device Tree Does Not Set the General Runtime Bitrate

The Device Tree normally describes the:

```text
Controller input clock
```

Userspace configures the:

```text
CAN bus bitrate
```

For example:

```sh
sudo ip link set can0 type can bitrate 500000
```

This separation exists because the same controller may be used on:

- A 125 kbit/s network.
- A 250 kbit/s network.
- A 500 kbit/s network.
- A CAN FD network.

Bitrate is generally a network policy, not a fixed property of the controller hardware.

A transceiver binding may, however, describe:

```text
max-bitrate
```

This expresses a hardware limit and helps prevent configuration above the transceiver's capability.

---

# 38. Schema Validation

In the target Linux kernel tree, run:

```sh
make ARCH=<arch> dtbs_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/net/can/<binding>.yaml
```

Or, depending on kernel Makefile support:

```sh
make ARCH=<arch> dtbs_check \
    DT_SCHEMA_FILES=net/can/<binding>.yaml
```

To build a specific board DTB:

```sh
make ARCH=<arch> <board>.dtb
```

Examples of `<arch>` include:

```text
arm
arm64
```

The actual target depends on the platform.

---

# 39. What Can Schema Checking Detect?

Schema checking can verify:

- Whether `compatible` is valid.
- The number of `reg` cells.
- Whether `interrupts` is missing.
- Clock counts and names.
- Required properties.
- Undefined properties.
- GPIO cell formats.
- SPI child-node formats.
- Transceiver PHY references.
- The range of `max-bitrate`.

It cannot verify:

- Correct schematic wiring.
- The oscillator value actually installed.
- GPIO polarity through external inversion circuitry.
- Whether CAN_H and CAN_L are swapped.
- Correct termination.
- Network bitrate.
- Actual waveforms and EMC behavior.

---

# 40. Runtime: Inspecting CAN Interfaces

List all CAN interfaces:

```sh
ip -details link show type can
```

The output may include:

```text
can0
can1
```

Detailed information may include:

- Driver state.
- Bitrate.
- Sample point.
- Clock.
- Bit timing.
- Error counters.
- Restart time.
- Bus-off counter.
- CAN FD capability.

If the interface has no configured bitrate or is not yet up, some information may not be displayed completely.

---

# 41. Runtime: Inspecting Driver Binding

```sh
readlink /sys/class/net/can0/device/driver
```

This may point to:

```text
M_CAN driver
MCP251xFD driver
Another CAN controller driver
```

If the link does not exist, inspect the sysfs topology. A network device's path may pass through a platform, SPI, or other parent device.

You can also run:

```sh
udevadm info /sys/class/net/can0
```

---

# 42. Runtime: Inspecting the Complete Topology

```sh
udevadm info --attribute-walk /sys/class/net/can0
```

This displays successive layers such as:

- Network interface.
- Parent device.
- Driver.
- OF node.
- SPI bus.
- Chip select.
- MMIO address.
- Compatible string.
- Device path.

This is more suitable for identifying physical hardware than merely looking at the name `can0`.

For example, an SPI CAN controller's device path may contain:

```text
spi0.0
```

Conceptually, this means:

```text
SPI bus 0
Chip select 0
```

---

# 43. Runtime: Inspecting Interrupts

```sh
cat /proc/interrupts | grep -i can
```

If the driver or IRQ name does not contain `can`, also search with:

```sh
cat /proc/interrupts | grep -Ei 'm_can|mcp25|spi'
```

The interrupt counter should increase as CAN frames are transmitted or received.

If the interface exists but the IRQ counter does not increase, possible causes include:

- No frame reaches the controller.
- An incorrect interrupt property.
- An incorrect GPIO trigger type.
- Incorrect pinmux configuration.
- Different driver behavior involving polling or NAPI.
- No valid response from the external SPI controller.
- The controller did not actually start.

---

# 44. Runtime: Inspecting the Device Tree Node

Inspect the OF node corresponding to the network device:

```sh
readlink /sys/class/net/can0/device/of_node
```

Then inspect its compatible string:

```sh
tr '\0' '\n' \
    < /sys/class/net/can0/device/of_node/compatible
```

For an external SPI controller, you may need to follow parent devices in sysfs to locate `of_node`.

The Device Tree is represented in the kernel as a flattened Device Tree with binary properties. Strings may be NUL-separated, so `tr` makes them easier to read.

---

# 45. Runtime: Verifying the Clock

`ip -details link show can0` may display the CAN controller clock.

Depending on the platform, you can also inspect:

```text
debugfs clock summary
Controller driver log
Device Tree clock-frequency
Fixed-clock node
```

When debugfs is available:

```sh
sudo cat /sys/kernel/debug/clk/clk_summary
```

Availability depends on the kernel configuration and whether debugfs is mounted.

Typical symptoms of an incorrect clock include:

- ACK/errors on every frame.
- A 500 kbit/s setting that produces a different physical waveform rate.
- Occasional low-speed operation but complete failure at high speed.
- CAN FD data-phase errors.
- Rapid transition to bus-off.

---

# 46. Runtime: Additional MCP2518FD Checks

For an SPI CAN FD controller, also check:

```sh
dmesg | grep -Ei 'mcp251|mcp251x|spi'
```

Verify:

- That the SPI device was created.
- That the driver matched.
- Oscillator frequency.
- IRQ operation.
- SPI errors.
- ECC/RAM initialization.
- Transceiver supply.
- That `can0` was created by the correct SPI device.

If the MCP2518FD driver probe fails, no CAN network interface may be created at all.

---

# 47. Layered Debugging Procedure

## Layer 1: Schematic

Verify:

- Controller type.
- MMIO or SPI connection.
- Clock/oscillator.
- IRQ.
- Reset.
- CAN TX/RX.
- Transceiver.
- Standby/enable GPIOs.
- Regulator.
- CAN_H/CAN_L.
- Termination.

## Layer 2: Device Tree Binding

Verify:

- `compatible`.
- `reg`.
- Clock.
- IRQ trigger.
- Pinctrl.
- PHY.
- GPIO flags.
- Supply.
- Required properties.

## Layer 3: Driver Probe

Run:

```sh
dmesg | grep -i can
```

Then verify:

- Successful controller-driver probe.
- Successful SPI-device creation.
- Correct clock.
- Correct IRQ.
- Network-device creation.

## Layer 4: Transceiver

Verify:

- Power rail.
- Standby GPIO.
- Enable GPIO.
- CAN_H/CAN_L waveform.
- That the transceiver is not in silent or shutdown mode.

## Layer 5: Runtime Bit Timing

Run:

```sh
ip -details link show can0
```

Confirm with the network owner:

- Bitrate.
- Sample point.
- CAN FD mode.
- Data bitrate.
- Restart policy.

## Layer 6: Physical Bus

Verify:

- 120 Ω termination at both ends.
- CAN_H/CAN_L are not swapped.
- Ground reference.
- Bus length.
- Stub length.
- Other nodes are powered.

## Layer 7: Application

Only then inspect:

- CAN ID.
- Standard versus extended frame format.
- Filters.
- Payload.
- Byte order.
- Timeout.
- Security.

---

# 48. Common Errors

## Treating `reg` as a CAN ID

For an SPI CAN controller:

```dts
reg = <0>;
```

means CS0, not CAN ID 0.

## Treating Oscillator Frequency as Bitrate

```dts
clock-frequency = <40000000>;
```

means a 40 MHz clock, not 40 Mbit/s CAN.

## Incorrect GPIO Polarity

The transceiver remains in standby, so `can0` exists but there is no physical waveform.

## Incorrect IRQ Type

Configuring an interrupt that remains low with an unsuitable edge trigger may lose subsequent events.

## SPI Frequency Above Specification

Access to the external CAN controller's registers becomes unreliable, causing probe or runtime errors.

## Incorrect Variant in `compatible`

Similar devices such as the MCP2517FD and MCP2518FD still require the correct compatible value supported by the binding.

## Shared Standby GPIO

Disabling one CAN network inadvertently disables another.

## Hard-Coding `can0`

After probe order changes, the service begins controlling the wrong physical bus.

## Checking Only That the DTS Compiles

A successful DTS build does not prove that:

- The oscillator is correct.
- The transceiver is enabled.
- The IRQ electrical type is correct.
- The bitrate is correct.
- CAN_H/CAN_L are wired correctly.
- The application protocol is correct.

---

# 49. Recommended Validation Order

```text
1. Cross-check the schematic
        |
        v
2. Find the correct controller/transceiver binding
        |
        v
3. Verify compatible, reg, clock, and IRQ
        |
        v
4. Verify pinctrl, GPIO polarity, and supply
        |
        v
5. Run dtbs_check
        |
        v
6. Build and deploy the DTB
        |
        v
7. Verify driver probe
        |
        v
8. Map can0/can1 to the physical topology
        |
        v
9. Verify that the transceiver leaves standby
        |
        v
10. Confirm the bitrate with the network owner
        |
        v
11. Bring the interface up
        |
        v
12. Inspect the waveform with an oscilloscope/analyzer
        |
        v
13. Inspect IRQ and error counters
        |
        v
14. Establish stable interface naming
        |
        v
15. Finally validate the application protocol
```

---

# 50. Summary of Core Concepts

1. **The Device Tree describes CAN hardware; it does not describe CAN message IDs or application protocols.**
2. **For an on-SoC CAN controller, `reg` is an MMIO range; for an SPI CAN controller, `reg` is a chip select.**
3. **The controller clock is the source for bit-timing calculations; it is not the CAN bus bitrate.**
4. **An on-SoC controller needs the correct pinctrl configuration to route CAN TX/RX to physical pins.**
5. **`phys` can connect a controller to a CAN transceiver, but whether it is used depends on the binding.**
6. **Some bindings instead use `xceiver-supply`, `standby-gpios`, or controller-specific properties; these approaches cannot be mixed arbitrarily.**
7. **Transceiver GPIO polarity must be determined from the GPIO flags, schematic, and datasheet.**
8. **The MCP2518FD is an SPI CAN FD controller, not a CAN transceiver.**
9. **An external SPI CAN controller requires verification of SPI frequency, mode, clock, IRQ, variant, and transceiver.**
10. **An active-low IRQ does not necessarily require a falling-edge trigger; if the signal remains low, a level-low trigger may be required.**
11. **Dual CAN channels should manage standby, power, and failure domains separately; sharing a GPIO cannot be assumed safe.**
12. **`can0`/`can1` reflect Linux discovery order and should not be treated as permanent physical identities.**
13. **Production services should use stable udev naming or topology-aware configuration.**
14. **Passing the DTS schema only proves structural compliance with the binding; it does not prove that the clock, wiring, transceiver, bitrate, or protocol is correct.**
