# I3C Protocol Fundamentals

I3C is a two-wire communication protocol developed by the MIPI Alliance. It is designed primarily to address the performance, device-discovery, interrupt, and system-management limitations of traditional I2C.

I3C still uses two signal lines:

```text
SCL: Clock
SDA: Data
```

It therefore looks similar to I2C, but I3C is not simply “a faster version of I2C.” It adds:

- Dynamic Address Assignment
- Standardized device discovery
- Common Command Codes
- In-Band Interrupts
- Hot-Join
- Multiple controller-capable devices
- Higher-speed SDR/HDR transfers
- More complete device identity and capability information

These features are particularly useful for servers, BMCs, sensor hubs, mobile devices, and systems that manage large numbers of sensors.

---

## 1. Why Is I3C Needed?

Traditional I2C is simple, inexpensive, and widely supported, but it has several limitations:

- A device address is usually fixed by hardware pins or registers.
- Devices with the same address cannot coexist directly on the same bus.
- There is no standardized device-discovery mechanism.
- A target normally requires an additional GPIO interrupt to notify the controller proactively.
- Speed and electrical loading are easily constrained when multiple devices share a bus.
- Hot-plugged devices or devices powered on after boot are difficult to add dynamically.
- Different vendors may use different device-identification methods.

I3C retains the two-wire SDA/SCL architecture and limited compatibility with legacy I2C while adding more complete bus-management capabilities.

| Feature | I2C | I3C |
| --- | --- | --- |
| Signal lines | SDA, SCL | SDA, SCL |
| Normal address | Fixed 7-bit or 10-bit address | Usually a dynamic 7-bit address |
| Device discovery | No unified mechanism | Supports ENTDAA |
| Device identity | Primarily address, Device Tree, or ACPI | PID, BCR, and DCR |
| Interrupt | Usually requires an additional GPIO | Supports In-Band Interrupt |
| Dynamic attachment | Normally unsupported | Supports Hot-Join |
| Signaling method | Primarily open-drain | Combination of open-drain and push-pull |
| High-speed mode | Fast, Fast-mode Plus, and High-speed | SDR and optional HDR modes |
| Bus-management commands | No unified standard | Common Command Codes |

> I3C and I2C can share a bus under specific conditions, but you must not assume that any arbitrary I2C device can be connected directly to an I3C bus.

---

## 2. Controller and Target

Newer specifications use the following terminology:

| New Term | Traditional Term | Function |
| --- | --- | --- |
| Controller | Master | Controls the clock, schedules transfers, and manages the bus |
| Target | Slave | Receives controller commands or returns data |

An I3C bus normally contains:

```text
Active Controller
      |
      +-- I3C Target A
      +-- I3C Target B
      +-- I3C Target C
      +-- Legacy I2C Target
```

The Active Controller is responsible for:

- Initializing the bus.
- Sending Common Command Codes.
- Performing Dynamic Address Assignment.
- Scheduling normal data transfers.
- Handling In-Band Interrupts.
- Handling Hot-Join.
- Performing Controller Role Handoff when necessary.

Some older Linux kernel APIs and source code still use the term `master`, such as `i3c_master_controller`. When reading Linux source code, understand that it refers to the I3C Controller.

---

## 3. Open-Drain and Push-Pull

I3C switches between open-drain and push-pull operation depending on the transfer phase.

### Open-Drain

In open-drain mode, a device can actively pull a signal low only. A pull-up resistor raises the signal when the device releases it.

```text
Output Low  --> The device actively pulls the signal low
Output High --> The device releases the signal; the pull-up raises it
```

The advantage is that multiple devices can safely perform wired-AND arbitration. This makes open-drain operation suitable for:

- Bus arbitration.
- Dynamic Address Assignment.
- Legacy I2C compatibility phases.
- Phases in which multiple targets may respond simultaneously.

The disadvantage is that signal rise time is affected by pull-up resistance, bus capacitance, and trace length, making it unsuitable for very high-speed transfers.

### Push-Pull

Push-pull output actively drives both high and low levels:

```text
Output Low  --> Actively drives a low level
Output High --> Actively drives a high level
```

Signals therefore switch faster and do not rely entirely on pull-up resistors to create rising edges.

I3C can use push-pull operation to increase speed during phases in which only one device is known to be driving the signal.

The simplified concept is:

```text
Arbitration or compatibility required --> Open-drain
One transfer participant selected      --> Push-pull may be used
```

This is one of the main reasons I3C can improve performance while retaining a two-wire bus.

---

## 4. Why Can I2C Devices Not Be Mixed Arbitrarily?

I3C provides limited support for legacy I2C targets, but not every I2C device is suitable for an I3C bus.

Before designing the bus, check:

- Whether the I2C target performs clock stretching.
- Whether the target’s spike filter could corrupt I3C waveforms.
- Whether the target can tolerate I3C bus signaling rates.
- Whether its static address conflicts with an I3C reserved address.
- Whether an unpowered target causes SDA/SCL leakage.
- Whether voltage levels are compatible.
- Whether pull-up resistance and bus capacitance meet the requirements.
- Whether the controller supports a mixed I2C/I3C bus.
- Whether the current bus mode permits that type of legacy device.

Some older I2C devices have long spike filters that may incorrectly treat high-speed I3C signals as noise. Some devices may also perform long clock-stretching operations that interfere with I3C bus timing.

Consequently, a real product must verify all of the following together:

```text
I3C Controller Specifications
       +
I3C/I2C Target Specifications
       +
MIPI I3C Specification
       +
Board-Level Electrical Design
```

---

## 5. I3C Device Identity

Each I3C target normally provides three important pieces of information:

| Field | Size | Purpose |
| --- | --- | --- |
| PID | 48 bits | Unique or semi-unique device-identification information |
| BCR | 8 bits | Describes the target’s bus capabilities |
| DCR | 8 bits | Describes the target’s device type |

### PID: Provisioned ID

The PID is the primary identity information for an I3C target and normally contains:

- Manufacturer ID
- Part ID
- Instance ID
- Additional identification information

You can think of it as the device’s identity card.

Even if the Dynamic Address changes after reinitialization, the PID still allows the controller to identify the target.

### BCR: Bus Characteristic Register

The BCR describes a target’s bus characteristics, such as:

- Whether it has Controller Capability.
- Whether it supports In-Band Interrupts.
- Whether its IBI includes additional data.
- Which bus roles or functions it supports.

It is similar to a table of the device’s communication capabilities.

### DCR: Device Characteristic Register

The DCR indicates the basic type or class of a device, such as a sensor or another device category.

It is similar to a device class code, but the DCR alone is usually insufficient to identify the exact model.

---

## 6. How Does Linux Match an I3C Driver?

With traditional I2C, Linux often locates the corresponding driver through the Device Tree `compatible` property:

```dts
sensor@48 {
    compatible = "vendor,temp-sensor";
    reg = <0x48>;
};
```

The main I3C concept is different. The controller first discovers the target and obtains:

```text
PID + BCR + DCR
```

The Linux I3C core then uses this identity information to match an I3C target driver.

A driver normally provides an ID table similar to:

```c
static const struct i3c_device_id demo_ids[] = {
    I3C_DEVICE(0x123, 0x4567, NULL),
    { }
};
```

Conceptually, this means:

```text
Manufacturer ID + Part ID --> Corresponding Driver
```

I3C target-driver matching therefore relies primarily on the discovered device identity rather than only on a Device Tree `compatible` string.

The Device Tree may still describe:

- The I3C controller.
- The bus mode.
- Known board-level devices.
- Legacy I2C targets.
- Static addresses.
- Preassigned addresses.
- Board-level configuration or other information that cannot be discovered automatically.

Applications, however, should not identify a device solely by a particular dynamic address.

---

## 7. Static Address and Dynamic Address

An I3C target may involve two types of addresses.

### Static Address

A Static Address is a fixed address that a device may have before bus initialization. Common sources include:

- Hardware pins.
- OTP.
- A non-volatile register.
- A Device Tree description.

A Static Address can help the controller locate or configure a device during initialization, but normal I3C communication usually uses a Dynamic Address.

### Dynamic Address

The Active Controller assigns a Dynamic Address during bus initialization.

For example:

```text
Target A PID = 0x123456789ABC --> Dynamic Address 0x09
Target B PID = 0x23456789ABCD --> Dynamic Address 0x0A
Target C PID = 0x3456789ABCDE --> Dynamic Address 0x0B
```

A Dynamic Address may change because of:

- A system reset.
- I3C bus reinitialization.
- Another Dynamic Address Assignment operation.
- A target rejoining the bus.
- Hot-Join.
- A change in controller role.
- Bus recovery.

The following design is therefore unreliable:

```text
Always assume that a particular sensor is at Dynamic Address 0x0A
```

The correct approach is to use the I3C core to locate the target by its device identity and let the kernel manage its current Dynamic Address.

### Difference Between Address, Identity, and Location

Beginners often confuse these three concepts:

| Concept | Meaning |
| --- | --- |
| Dynamic Address | The communication address used in the current bus session |
| PID | The target’s hardware identity |
| Physical Location | The board, slot, or connector where the target is installed |

A Dynamic Address is not a permanent identity, and a PID alone does not necessarily indicate a physical slot location.

In OpenBMC, the PID normally must also be mapped to board topology, inventory, or slot information.

---

## 8. Dynamic Address Assignment

Dynamic Address Assignment, or DAA, is an important I3C feature.

Through `ENTDAA`, the Active Controller can discover targets that do not yet have Dynamic Addresses and assign available addresses to them.

### Simplified ENTDAA Flow

```text
Controller: Broadcast ENTDAA
     |
     v
Unaddressed Targets: Participate in arbitration simultaneously
     |
     v
Priority Target: Sends PID + BCR + DCR
     |
     v
Controller: Assigns a Dynamic Address and parity bit
     |
     v
Target: ACK
     |
     v
Remaining Unaddressed Targets: Continue in the next arbitration round
```

Expressed as a conversation:

```text
Controller: Which unaddressed targets are present?
Targets:    Begin sending their PIDs simultaneously
Target A:   Wins arbitration and sends its PID, BCR, and DCR
Controller: Assigns Dynamic Address 0x09
Target A:   ACK

Controller: Continues ENTDAA
Target B:   Sends its PID, BCR, and DCR
Controller: Assigns Dynamic Address 0x0A
Target B:   ACK
```

The process continues until no eligible unaddressed targets remain.

### Arbitration Concept

Multiple targets may send identity information simultaneously, but an open-drain bus can use bit arbitration to determine which one finishes first.

In general, during arbitration:

```text
Low (0) dominates High (1)
```

A target that transmits High but observes Low knows that it lost the current arbitration round. It stops transmitting and waits for the next round.

### SETDASA

If the controller knows a target’s Static Address, it can use `SETDASA` to assign a Dynamic Address based on that Static Address.

```text
Known Static Address
       |
       v
Send SETDASA
       |
       v
Target Receives a Dynamic Address
```

### `assigned-address`

The Device Tree `assigned-address` property can express a preferred initial Dynamic Address.

It should be understood as:

```text
A preference or initial assignment request
```

It is not an immutable, permanent device identity.

Applications still should not hard-code it because the actual address can be affected by:

- Address conflicts.
- Controller policy.
- Bus reinitialization.
- Hot-Join.
- Kernel implementation.

---

## 9. Common Command Codes

Common Command Codes, or CCCs, are standardized I3C bus-management commands.

CCCs are broadly divided into two categories:

| Type | Description |
| --- | --- |
| Broadcast CCC | Sent to all applicable targets on the bus |
| Directed CCC | Sent to one or more specified targets |

### Address Management

| CCC | Function |
| --- | --- |
| `ENTDAA` | Starts Dynamic Address Assignment |
| `RSTDAA` | Clears Dynamic Addresses |
| `SETDASA` | Assigns a Dynamic Address based on a Static Address |
| `SETNEWDA` | Changes the Dynamic Address of an addressed target |

### Event Control

| CCC | Function |
| --- | --- |
| `ENEC` | Enables specified events |
| `DISEC` | Disables specified events |

Managed events may include:

- In-Band Interrupt.
- Controller Role Request.
- Hot-Join.

### Identity and Capability

| CCC | Function |
| --- | --- |
| `GETPID` | Reads the Provisioned ID |
| `GETBCR` | Reads the Bus Characteristic Register |
| `GETDCR` | Reads the Device Characteristic Register |
| `GETCAPS` | Reads extended target capabilities |

Actual support depends on the I3C specification version and target implementation.

### Transfer Length

| CCC | Function |
| --- | --- |
| `GETMRL` | Gets the Maximum Read Length |
| `GETMWL` | Gets the Maximum Write Length |
| `SETMRL` | Sets the Maximum Read Length |
| `SETMWL` | Sets the Maximum Write Length |

Even if the controller can theoretically transfer a large amount of data, it must not exceed the maximum read or write length supported by the target.

### Multi-Controller Information

Commands such as `DEFSLVS` can communicate information about devices and controller-capable devices on the bus.

Controller Role Handoff also uses related protocol procedures and commands.

### Reset and Recovery

Newer I3C specification versions add more mechanisms related to reset, error recovery, and target reset.

Before using them, confirm:

- Which specification version the controller driver supports.
- Which CCCs the target supports.
- Whether the Linux I3C core implements them.
- Whether reset affects the entire bus.
- Whether DAA must be performed again after reset.

> CCCs change the protocol state of a target or the entire bus and should not be sent directly by arbitrary shell scripts.

CCCs should be managed by:

- The Linux I3C core.
- The I3C controller driver.
- A reviewed I3C target driver.
- A diagnostic tool with clearly defined permissions and state management.

---

## 10. Private SDR Transfer

After a target has been discovered and assigned a Dynamic Address, its driver can perform device-specific private transfers.

A private transfer can be understood as:

```text
Not a standard CCC,
but a device-data transfer between the controller and a particular target
```

For example, an educational sensor might use:

```text
1. Write a 1-byte register address
2. Read 2 bytes of sensor data
```

The conceptual flow is:

```text
Target Driver
     |
     +-- Write: Register Selector 0x01
     |
     +-- Read: Obtain 2 Bytes of Data
```

This resembles traditional I2C register access:

```text
Write Register Address
Repeated START
Read Register Data
```

However, the actual transfer is performed through:

```text
I3C Target Driver
       |
       v
Linux I3C Core
       |
       v
I3C Controller Driver
       |
       v
I3C Hardware Controller
```

It is not an ordinary I2C transaction executed directly through `/dev/i2c-N`, and some data phases can use push-pull signaling.

The driver must still follow the target datasheet’s definitions for:

- Maximum Read Length.
- Maximum Write Length.
- Register Address Width.
- Byte Order.
- Command Format.
- Transfer Delay.
- Error Handling.

---

## 11. In-Band Interrupt

A traditional I2C sensor normally needs an additional GPIO to notify the controller proactively:

```text
SDA
SCL
INT GPIO
```

An I3C In-Band Interrupt (IBI) allows a target to request service directly through SDA/SCL:

```text
SDA
SCL
```

An additional interrupt GPIO is not necessarily required.

### Basic IBI Flow

```text
Target: Issues an IBI Request
     |
     v
Controller: Arbitrates and Accepts the Request
     |
     v
Target: Sends Its Dynamic Address
     |
     v
Target: Optionally Includes an IBI Payload
     |
     v
Controller Driver: Notifies the Target Driver
     |
     v
Target Driver: Performs the Corresponding Handling
```

IBI can be used for:

- Sensor threshold events.
- Notification that new data is available.
- Fault notifications.
- State changes.
- FIFO watermark events.
- Error events.

### IBI Payload

Depending on its capabilities and protocol, a target can include additional data such as:

```text
Event Type
Status Byte
FIFO Level
Fault Code
```

The payload format is normally defined by the target datasheet.

### IBI Arbitration Priority

When multiple targets issue IBIs simultaneously, arbitration is required.

In general, a lower Dynamic Address has a higher IBI arbitration priority.

Dynamic Address Assignment may therefore affect the interrupt priority of important devices; it is not always simply a matter of choosing any unused address.

### Linux Driver Considerations

An I3C target driver normally needs to:

1. Request IBI resources.
2. Preallocate enough IBI slots.
3. Register an IBI handler.
4. Enable IBI.
5. Disable IBI and release its resources during removal or suspend.

The IBI handler must:

- Complete quickly.
- Avoid blocking.
- Parse the payload length correctly.
- Handle queue overflow.
- Handle lost events.
- Prevent interrupt storms.
- Defer time-consuming work to a workqueue or thread context.

Although IBI reduces the need for GPIOs, it does not eliminate interrupt-design work. You must still verify:

- Controller queue depth.
- Maximum payload size.
- Interrupt latency.
- Suspend/resume policy.
- Burst events.
- Overflow recovery.
- Target reset.
- Bus-error recovery.

---

## 12. Hot-Join

Hot-Join allows an I3C target that was absent, unpowered, or not ready during bus initialization to request to join the bus later.

For example:

```text
System Boot
   |
   +-- Sensor A Is Powered --> Participates in Initial DAA
   |
   +-- Sensor B Is Not Powered
            |
            v
       Powered On Later
            |
            v
       Issues Hot-Join
            |
            v
Controller Schedules New DAA
            |
            v
Sensor B Receives a Dynamic Address
```

Hot-Join is suitable for:

- Pluggable modules.
- Sensor boards with delayed power-on.
- Power-gated devices.
- Hot-swappable server modules.
- Devices with long startup times.

### Hot-Join Is Not the Same as Ordinary USB Hot-Plug

Hot-Join addresses only dynamic attachment at the I3C protocol layer. The system must still handle:

- Whether power is stable.
- Whether contact bounce causes signal glitches.
- Whether the target meets the bus electrical requirements.
- How the new device is added to the Linux device model.
- Whether the driver matches successfully.
- How OpenBMC inventory is created or updated.
- How resources are cleaned up after device removal.
- How sensor, D-Bus, and Redfish data are synchronized.

---

## 13. Multiple Controller-Capable Devices

An I3C bus may contain multiple controller-capable devices, but under normal conditions only the current Active Controller can initiate normal transfers.

```text
Current Active Controller
          |
          +-- Target A
          +-- Target B
          +-- Controller-Capable Target C
```

Although Target C has controller capability, it cannot initiate normal transactions like the Active Controller until Role Handoff has been completed.

### Controller Role Handoff

A simplified flow is:

```text
Controller-Capable Device: Issues a Role Request
             |
             v
Current Controller: Evaluates Whether to Allow It
             |
             v
Perform Protocol-Defined Role Handoff
             |
             v
New Controller Takes Over the Bus
```

This differs from the traditional I2C multi-master concept.

With I2C multi-master operation, two masters may independently attempt to acquire the bus without complete system-level coordination and resolve conflicts through arbitration.

I3C Controller Role Handoff is an explicit protocol-state transition:

```text
It does not mean that two controllers may independently transmit at any time.
Instead, the current controller transfers its role to another device.
```

The system must define:

- Which devices are allowed to become the Active Controller.
- When handoff is permitted.
- How normal transfers are paused during handoff.
- How Dynamic Addresses and device state remain consistent.
- Whether the Linux driver supports the procedure.
- Who performs recovery if the handoff fails.

---

## 14. SDR and HDR

### SDR: Single Data Rate

SDR is a commonly used I3C transfer mode. It retains an easy-to-understand clock/data relationship and can use push-pull operation during appropriate phases to improve speed.

Private transfers in a typical I3C target driver normally begin with SDR.

### HDR: High Data Rate

HDR is an optional higher-speed transfer mode. Different I3C specification versions may define different HDR modes.

HDR support is not guaranteed in every controller, target, or Linux driver.

Before enabling it, confirm:

- Controller hardware support.
- Target support.
- Linux controller-driver support.
- PCB layout and signal-integrity compliance.
- Correct logic-analyzer decoding.
- That legacy I2C devices on a mixed bus are not adversely affected.

Beginners should first understand:

```text
Bus Initialization
Dynamic Address Assignment
CCC
Private SDR Transfer
IBI
```

Then continue with HDR.

---

## 15. Linux I3C Software Architecture

A simplified architecture is shown below:

```text
I3C Target Driver
        |
        | Private Transfer / IBI / CCC
        v
Linux I3C Core
        |
        | Address, Device, Bus, and Protocol Management
        v
I3C Controller Driver
        |
        | Operates Registers, FIFO, DMA, and IRQ
        v
I3C Controller Hardware
        |
        v
SDA / SCL
        |
        v
I3C Target
```

The responsibilities of each layer are:

| Component | Primary Responsibility |
| --- | --- |
| Target Driver | Implements the functions of a particular sensor, EEPROM, or management device |
| I3C Core | Manages buses, targets, Dynamic Addresses, and driver matching |
| Controller Driver | Converts Linux I3C requests into hardware operations |
| Controller Hardware | Generates the actual SDA/SCL waveforms |
| Target Hardware | Executes CCCs, private transfers, IBIs, and related operations |

If a device does not work, first determine which layer contains the problem instead of examining only the target driver.

---

## 16. Potential OpenBMC Use Cases

OpenBMC can use I3C to manage different types of devices in a server, including:

- Temperature sensors.
- Voltage/current monitors.
- Power-management devices.
- DIMM or memory-sideband devices.
- Board-management controllers.
- Security devices.
- Pluggable sensor modules.
- Backplane-management devices.

The conceptual flow is:

```text
I3C Sensor
    |
    v
Linux I3C Target Driver
    |
    v
Hwmon / IIO / Custom Kernel Interface
    |
    v
OpenBMC Sensor Service
    |
    v
D-Bus
    |
    v
Redfish / Web UI / Remote Management
```

If a target reports a warning event through IBI, the flow may be:

```text
Target Fault
    |
    v
IBI
    |
    v
I3C Controller Driver
    |
    v
Target Driver
    |
    v
OpenBMC Event / Logging Service
    |
    v
Redfish Event
```

In addition to identifying a PID, OpenBMC must know the target’s physical location, such as:

```text
Main Board
CPU Board
DIMM Slot
Fan Board
Power Backplane
GPU Tray
```

The PID identifies the device. The inventory model identifies the device’s location and purpose within the entire server.

---

## 17. Electrical Design Checklist

### Voltage

Confirm that:

- The controller and all targets have compatible I/O voltages.
- Whether a level shifter is required.
- The level shifter supports I3C push-pull signaling.
- An unpowered target does not pull a signal low.
- Power-off leakage meets the requirements.

A level shifter designed only for traditional open-drain I2C may not be suitable for I3C.

### Pull-Up

Confirm that:

- Pull-up resistance meets the controller and target specifications.
- Pull-ups are not duplicated across multiple boards.
- The total equivalent resistance is not too low.
- Rise time during open-drain phases meets the requirements.

### Bus Capacitance

Bus capacitance is affected by:

- PCB traces.
- Connectors.
- Cables.
- Target pins.
- Level shifters.
- Logic-analyzer probes.
- Multiple inactive devices.

Excessive bus capacitance causes slow signal rise, distorted edges, or unstable high-speed operation.

### Topology

Check:

- Trace length.
- Stub length.
- Number of connectors.
- Branch topology.
- Whether pluggable modules are present.
- Whether an I3C hub or another isolation architecture is needed.

### Mixed Bus

If legacy I2C targets are present, verify:

- Spike filter behavior.
- Clock stretching.
- Maximum clock rate.
- Static addresses.
- Reserved addresses.
- Bus mode.
- Whether a target might misinterpret an I3C broadcast.

### Waveform

Use a logic analyzer or oscilloscope with I3C decoding support to examine:

- START/STOP.
- Open-drain phases.
- Push-pull phases.
- ENTDAA.
- Dynamic Addresses.
- CCCs.
- Private transfers.
- IBIs.
- Hot-Join.
- ACK/NACK.
- Parity and error states.

An analyzer that supports only I2C may not decode a complete I3C transaction correctly.

---

## 18. Recommended Debugging Sequence

### Layer 1: Controller Hardware

Confirm that:

- Clock and reset are correct.
- Pinmux is configured for I3C.
- The controller is powered.
- Interrupts work correctly.
- The kernel recognizes the controller.

### Layer 2: Controller Driver

Inspect the kernel log:

```sh
dmesg | grep -i i3c
```

Confirm that:

- The controller driver probed successfully.
- Bus initialization completed.
- No timeout occurred.
- No arbitration, parity, or transfer error occurred.
- The controller supports the required I3C mode.

### Layer 3: Electrical Bus

Use an oscilloscope or I3C-capable analyzer to confirm that:

- SDA/SCL are not stuck low.
- Pull-ups are correct.
- Rise and fall times are reasonable.
- There is no push-pull contention.
- The target is powered.
- Voltage levels are correct.

### Layer 4: DAA and Device Discovery

Confirm that:

- `ENTDAA` is sent.
- The target participates in arbitration.
- PID/BCR/DCR are returned successfully.
- The controller assigns a Dynamic Address.
- The target acknowledges it.
- No address conflict exists.

### Layer 5: Driver Matching

Confirm that Linux finds the corresponding driver using PID, BCR, or DCR information.

If the target was discovered but no driver was bound, possible causes include:

- The `i3c_device_id` does not match.
- The PID definition is incorrect.
- The driver is not loaded.
- The kernel configuration is not enabled.
- The module alias is incorrect.
- Driver probe returned an error.

### Layer 6: Private Transfer

Confirm that:

- The register address is correct.
- Read/write length does not exceed the limit.
- The Dynamic Address is still valid.
- The target has not entered reset or a low-power state.
- The controller queue has not overflowed.
- Transfer completion is handled correctly.
- Byte order and data format are correct.

### Layer 7: IBI

Confirm that:

- The target BCR indicates IBI support.
- The driver requested IBI resources.
- Enough IBI slots were allocated.
- The corresponding event was enabled.
- The controller queue is not full.
- Payload length and format are correct.
- The handler does not run for too long.
- IBI is re-enabled after suspend if necessary.

### Layer 8: OpenBMC Userspace

Confirm that:

- The kernel driver creates an hwmon, IIO, or other interface.
- The sensor service reads values successfully.
- The D-Bus object is created.
- Inventory maps to the correct physical location.
- Redfish presents the correct information.
- Userspace updates its state after Hot-Join or reset.

---

## 19. Failure Scenarios That Must Be Tested

I3C validation should not cover only normal reads and writes. It should also test:

- A target that is absent during boot.
- A target that powers on later and issues Hot-Join.
- A target resetting during a transfer.
- The bus performing DAA again.
- A Dynamic Address changing.
- Multiple targets issuing IBIs simultaneously.
- IBI queue overflow.
- A target continuously generating an interrupt storm.
- SDA stuck low.
- SCL stuck low.
- Private-transfer timeout.
- A CCC being NACKed.
- A target returning an incorrect PID/BCR/DCR.
- A legacy I2C target performing clock stretching on a mixed bus.
- Controller Role Handoff failure.
- System suspend/resume.
- BMC warm reset.
- Target power cycle.
- Unstable connector contact.
- Device rediscovery after bus recovery.

---

## 20. Important Safety Principles

- Do not treat a Dynamic Address as a permanent device identity.
- Do not assume that any arbitrary I2C target can be placed on an I3C bus.
- Do not send CCCs that change bus state from arbitrary shell scripts.
- Do not bypass the kernel I3C core and manipulate controller registers directly while it manages the bus.
- Do not ignore the target’s Maximum Read/Write Length.
- Do not perform long-running or blocking work in an IBI handler.
- Do not rely only on an ordinary I2C analyzer to determine whether I3C waveforms are correct.
- Do not assume that Hot-Join provides complete hardware hot-plug support.
- Do not assume that Controller Role Handoff is equivalent to uncoordinated I2C multi-master operation.
- Do not expect software to fix an incorrect pull-up, voltage, capacitance, or level-shifter design.

---

## 21. Recommended Learning Sequence

1. Review I2C SDA, SCL, addresses, ACK/NACK, and open-drain operation.
2. Understand why I3C combines open-drain and push-pull signaling.
3. Distinguish among Static Address, Dynamic Address, and PID.
4. Understand the purposes of PID, BCR, and DCR.
5. Trace a complete ENTDAA procedure.
6. Learn the common CCCs and their scope.
7. Practice Private SDR Transfers.
8. Learn IBI resource allocation, handler design, and overflow handling.
9. Understand the relationship between Hot-Join and rerunning DAA.
10. Understand Controller Role Handoff.
11. Read the Linux I3C core and controller driver.
12. Map kernel devices to OpenBMC sensors, D-Bus, and inventory.

---

## 22. Summary of Core Concepts

The most important concept in I3C is not simply “increasing the I2C clock rate.” It is the creation of a more complete two-wire bus-management mechanism:

```text
PID / BCR / DCR
        |
        v
Device Discovery and Identification
        |
        v
Dynamic Address Assignment
        |
        v
CCCs Manage Bus State
        |
        v
Private SDR/HDR Data Transfer
        |
        +--> IBI Proactive Notification
        |
        +--> Hot-Join Dynamic Attachment
        |
        +--> Controller Role Handoff
```

Beginners should remember these four points:

1. **The PID is the device identity; the Dynamic Address is the current communication address.**
2. **A Dynamic Address may change and therefore must not be hard-coded by an application.**
3. **IBI can send an interrupt request through SDA/SCL, but it still requires complete queue, latency, and recovery design.**
4. **I3C can coexist with some I2C targets, but protocol and electrical compatibility must be confirmed first.**

For production development, always follow the applicable MIPI I3C specification version, controller datasheet, target datasheet, Linux kernel implementation, and board-level schematic.
