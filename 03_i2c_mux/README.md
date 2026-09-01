# Linux I2C Mux Implementation and Learning for OpenBMC

This module is a beginner-friendly hands-on guide for understanding **I2C Multiplexers (I2C Muxes)** and **I2C Switches** in Linux and OpenBMC.

An I2C Mux allows one I2C controller to connect to multiple isolated downstream I2C buses. Common use cases include:

- Expanding the number of I2C devices that can be connected.
- Isolating the I2C buses of different modules or boards.
- Reusing the same I2C slave address on different channels.
- Preventing a faulty device from affecting the entire I2C bus.
- Managing hardware according to chassis, backplane, or slot location.

A typical architecture is shown below:

```text
I2C Controller
      |
      v
Physical Parent Adapter (Parent Bus)
      |
      v
I2C Mux Client (for example, address 0x70)
      |
      +---- Channel 0 --> Logical Child Adapter 0 --> Downstream Device
      |
      +---- Channel 1 --> Logical Child Adapter 1 --> Downstream Device
      |
      +---- Channel 2 --> Logical Child Adapter 2 --> Downstream Device
      |
      +---- Channel N --> Logical Child Adapter N --> Downstream Device
```

## Why Is an I2C Mux Needed?

I2C devices are identified by slave addresses. Normally, two devices with the same address cannot exist on the same I2C bus at the same time.

Assume that a system contains four temperature sensors, all with the fixed address `0x48`:

```text
Channel 0 --> Temperature Sensor @ 0x48
Channel 1 --> Temperature Sensor @ 0x48
Channel 2 --> Temperature Sensor @ 0x48
Channel 3 --> Temperature Sensor @ 0x48
```

If all of them are connected directly to the same I2C bus, the controller cannot determine which sensor it should communicate with.

After an I2C Mux is added, Linux can select a channel first and then access the `0x48` device on that channel. Therefore, the same I2C address can be reused on different channels.

## Terms Beginners Should Understand First

| Term | Description |
| --- | --- |
| I2C Controller | The hardware controller in the SoC that generates the I2C clock and transfers data |
| Parent Adapter | The upstream physical I2C bus to which the I2C Mux is connected |
| I2C Mux Client | The Mux device located on the parent bus, for example, `0x70` |
| Channel | A downstream path provided by the Mux, for example, Channels 0–3 |
| Child Adapter | A logical I2C adapter created by Linux for each channel |
| Endpoint Device | An actual device behind a channel, such as a temperature sensor, EEPROM, or PSU |
| `/dev/i2c-N` | A device node used by userspace to access an I2C adapter |

One important point is that **a Child Adapter is a logical bus created by Linux, but it still shares the same physical I2C controller underneath.**

For example:

```text
/dev/i2c-5   --> Physical Parent Bus
/dev/i2c-11  --> Mux Channel 0
/dev/i2c-12  --> Mux Channel 1
/dev/i2c-13  --> Mux Channel 2
/dev/i2c-14  --> Mux Channel 3
```

The bus numbers `11–14` are normally assigned dynamically while Linux is running. Do not assume that they will remain the same after every boot.

## I2C Mux Used in This Tutorial

This project uses a fictional four-channel, register-controlled I2C Mux:

```text
I2C Address: 0x70
Number of Channels: 4
```

Write the following value to select the corresponding channel:

```c
BIT(channel)
```

For example:

| Value Written | Result |
| --- | --- |
| `0x01` | Connect Channel 0 |
| `0x02` | Connect Channel 1 |
| `0x04` | Connect Channel 2 |
| `0x08` | Connect Channel 3 |
| `0x00` | Disconnect all channels |

For example, to select Channel 2:

```c
BIT(2) == 0x04
```

This device is used only for educational purposes. A real product must be developed according to the actual component datasheet, Device Tree binding, and upstream Linux driver.

A commonly used official Linux driver is:

```text
drivers/i2c/muxes/i2c-mux-pca954x.c
```

It supports various PCA954x, PCA984x, and compatible I2C Mux/Switch devices.

---

## Repository Structure

| Path | Description |
| --- | --- |
| `docs/i2c-mux-fundamentals.md` | Electrical characteristics, differences between Muxes and Switches, and hardware design principles |
| `docs/linux-i2c-mux-architecture.md` | Linux logical adapters, locking, callbacks, and core source-code locations |
| `docs/device-tree.md` | Register-controlled, nested, and GPIO-controlled Mux topologies |
| `docs/userspace-tools.md` | How to identify and safely access downstream I2C adapters |
| `docs/debugging.md` | Layered debugging methods from topology to I2C transactions |
| `docs/openbmc-use-cases.md` | OpenBMC use cases for PSUs, fans, FRUs, sensors, and backplanes |
| `client-driver/` | Educational Linux I2C Mux driver |
| `device-tree/` | DTS examples for three I2C Mux topologies |
| `userspace/` | Test programs for locating channel adapters and reading downstream devices |
| `scripts/` | Bus inventory, channel resolution, and non-intrusive diagnostic tools |

---

## How Does Linux Handle an I2C Mux?

After the I2C Mux driver is loaded successfully, Linux creates one Child Adapter for each Mux channel.

Applications and downstream device drivers do not need to control the Mux register directly. Instead, they issue normal I2C transfers through the Child Adapter.

For example, an application accesses:

```text
Device 0x48 on /dev/i2c-12
```

The Linux I2C Mux framework automatically performs the following operations:

```text
1. Lock the Parent Adapter
2. Call the select() callback
3. Write the Mux control register
4. Select the correct channel
5. Perform the downstream I2C transaction
6. Call deselect(), depending on the driver configuration
7. Unlock the Parent Adapter
```

The conceptual flow is shown below:

```text
Userspace or Sensor Driver
          |
          v
    Child Adapter
          |
          v
  I2C Mux Core select()
          |
          v
 Write Mux Control Register
          |
          v
   Select Target Channel
          |
          v
   Parent I2C Controller
          |
          v
 Downstream I2C Endpoint Device
```

Therefore, when a kernel driver is already managing the Mux, userspace should access the Child Adapter instead of writing the channel control register at `0x70` directly.

---

## Role of the Device Tree

The Device Tree describes hardware connections, including:

- The parent bus on which the I2C Mux is located.
- The I2C address of the Mux.
- The Mux model and its corresponding `compatible` string.
- The number of channels.
- The devices behind each channel.
- Whether a reset GPIO is present.
- Whether a channel should remain connected or be disconnected while idle.

A simplified example is shown below:

```dts
&i2c5 {
    status = "okay";

    i2c-mux@70 {
        compatible = "vendor,demo-i2c-mux";
        reg = <0x70>;

        i2c@0 {
            reg = <0>;

            sensor@48 {
                compatible = "vendor,demo-sensor";
                reg = <0x48>;
            };
        };

        i2c@1 {
            reg = <1>;

            eeprom@50 {
                compatible = "atmel,24c02";
                reg = <0x50>;
            };
        };
    };
};
```

The `reg` properties here have three different meanings:

- `reg = <0x70>` in `i2c-mux@70`: the I2C address of the Mux on the parent bus.
- `reg = <0>` in `i2c@0`: Mux Channel 0.
- `reg = <0x48>` in `sensor@48`: the I2C address of the sensor on the child bus.

When reading DTS, first determine whether the current node represents a Mux, a channel, or a downstream device. Only then can you interpret the meaning of `reg` correctly.

---

## Quick Start

### 1. Load `i2c-dev`

`i2c-dev` creates `/dev/i2c-N` device nodes, allowing userspace programs to access I2C adapters through `ioctl()`.

```sh
sudo modprobe i2c-dev
```

Check the device nodes:

```sh
ls -l /dev/i2c-*
```

### 2. Examine the Current I2C Mux Topology

The following script checks only the adapters, devices, and sysfs links already created by Linux. It does not actively issue read or write transactions on the I2C bus:

```sh
./scripts/show_mux_tree.sh
```

Inspect the Mux at `0x70` on Parent Bus 5:

```sh
./scripts/debug_i2c_mux.sh 5 0x70
```

This type of non-intrusive inspection should be preferred over `i2cdetect` because it does not probe every address on the bus.

### 3. Locate the Child Adapter for a Channel

First, compile the tool:

```sh
cc -O2 -Wall -Wextra -Werror \
   -o resolve_mux_channel \
   userspace/resolve_mux_channel.c
```

Locate Channel 2 in the following topology:

```text
Parent Bus: 5
Mux Address: 0x70
Channel: 2
```

Run:

```sh
./resolve_mux_channel 5 0x70 2
```

The program uses the `channel-*` links in sysfs to find the Child Adapter actually assigned by Linux instead of assuming a fixed `/dev/i2c-N` path.

### 4. Test a Downstream I2C Device

First, compile the test program:

```sh
cc -O2 -Wall -Wextra -Werror \
   -o test_downstream_i2c \
   userspace/test_downstream_i2c.c
```

Assume that the following information has been confirmed:

```text
Child Adapter: /dev/i2c-12
Device Address: 0x48
Register: 0x00
Read Length: 2 bytes
```

Run:

```sh
sudo ./test_downstream_i2c /dev/i2c-12 0x48 0x00 2
```

Linux automatically selects the correct Mux channel before the transfer. The test program does not need to manipulate `0x70` directly.

Before running the test, confirm that:

- `/dev/i2c-12` actually corresponds to the target channel.
- A device at `0x48` actually exists on that channel.
- Register `0x00` can be read safely.
- The register address width and data length used by the device are correct.
- The device is not exclusively owned by another kernel driver or management service.

### 5. Build the Educational Kernel Module

The kernel module must be built with headers and a build tree compatible with the kernel running on the target system:

```sh
make -C client-driver
```

Load the module:

```sh
sudo insmod client-driver/demo_i2c_mux.ko
```

Check the kernel log:

```sh
dmesg | tail -n 50
```

You can also verify whether the driver was bound successfully:

```sh
ls -l /sys/bus/i2c/drivers/
```

Remove the module:

```sh
sudo rmmod demo_i2c_mux
```

If `invalid module format` appears, the kernel version, configuration, or symbol versions used to build the module usually do not match the target system.

---

## How Can You Verify That the Driver Created Child Adapters Correctly?

First, list the I2C adapters in the system:

```sh
i2cdetect -l
```

You can also inspect sysfs:

```sh
ls -l /sys/class/i2c-adapter/
```

The Mux device normally appears at a path similar to:

```text
/sys/bus/i2c/devices/5-0070/
```

The naming format is:

```text
<Parent Bus Number>-<Four-Digit I2C Address>
```

Therefore:

```text
5-0070
```

means:

```text
Parent Bus = 5
Mux Address = 0x70
```

After the Mux driver creates the Child Adapters, the directory may contain:

```text
channel-0
channel-1
channel-2
channel-3
```

These symbolic links point to the I2C adapter that actually corresponds to each channel. Use these links to find the Child Bus instead of hard-coding a bus number in the program.

---

## Important Safety Rules

### 1. Do Not Assume That the Child Bus Number Is Fixed

The following implementation is risky:

```sh
DOWNSTREAM_BUS=/dev/i2c-12
```

Linux I2C bus numbers may change due to:

- Changes in driver probe order.
- Device Tree changes.
- Addition or removal of other I2C controllers.
- Kernel version changes.
- A driver changing from a module to built-in, or vice versa.

Unless the board design explicitly fixes the bus number through aliases, resolve it dynamically through the `channel-*` links in sysfs.

### 2. Do Not Modify Mux Registers from Userspace While a Kernel Driver Manages the Mux

Incorrect approach:

```sh
i2cset -y 5 0x70 0x04
```

If the kernel I2C Mux driver is managing `0x70` at the same time, modifying the register directly from userspace may make the channel state recorded by the kernel inconsistent with the actual hardware state. This can cause incorrect data reads, transfer failures, or race conditions.

The correct approach is to access the corresponding Child Adapter:

```sh
i2cget -y <child-bus> 0x48 0x00
```

Linux selects the channel automatically.

### 3. Avoid Blind Scanning and Writing on Production Systems

The following tools may generate actual I2C transactions:

```text
i2cdetect
i2cget
i2cset
i2cdump
```

In particular, `i2cdetect` sends probe commands to many addresses. Some devices may interpret specific commands as writes, resets, or state changes. Therefore, do not use it until the hardware behavior has been confirmed.

Do not use these options casually:

```sh
i2cget -f
i2cset -f
```

The `-f` option forces access even when the address is already in use by a kernel driver. This may corrupt the driver state or interfere with an ongoing transaction.

### 4. Software Cannot Fix Electrical Hardware Design Problems

Each downstream channel of an I2C Mux may have its own:

- Pull-up resistors.
- Voltage domain.
- Bus capacitance.
- Clock-frequency limit.
- Level shifter.
- Reset signal.
- Power sequence.
- Idle-disconnect policy.

If the pull-up, voltage, or capacitance design is incorrect, I2C communication may still fail even when the Device Tree and driver are correct.

### 5. Ownership Must Be Defined for Nested Mux and Multi-Master Designs

A nested Mux means that another Mux is connected behind a Mux, for example:

```text
Controller
   |
   +-- Mux A Channel 2
          |
          +-- Mux B Channel 1
                 |
                 +-- Sensor
```

Multi-master means that the same I2C bus may be accessed by the BMC, host, or another management controller.

These architectures must clearly define:

- Which master may access the bus and when.
- Who is responsible for switching channels.
- Whether a hardware semaphore is required.
- Whether a bus ownership protocol exists.
- How recovery is performed after a timeout.
- Which state the Mux should return to after a system reset.

---

## Recommended Layered Debugging Procedure

When a downstream device cannot be accessed, do not immediately assume that the sensor driver is at fault. Check the following layers in order.

### Layer 1: I2C Controller

Confirm that the Parent Adapter exists:

```sh
i2cdetect -l
ls -l /sys/class/i2c-adapter/
```

Confirm that the controller driver probed successfully:

```sh
dmesg | grep -i i2c
```

### Layer 2: I2C Mux Client

Confirm that the Mux appears on the parent bus:

```sh
ls -l /sys/bus/i2c/devices/5-0070
```

Confirm that `compatible` and `reg` match the actual hardware.

### Layer 3: Mux Driver Binding

Confirm that the Mux device is bound to the correct driver:

```sh
readlink /sys/bus/i2c/devices/5-0070/driver
```

If there is no `driver` link, possible causes include:

- The driver is not loaded.
- The Device Tree `compatible` string does not match.
- Driver probe failed.
- The Mux does not respond.
- The reset GPIO is still asserted or power is not enabled.

### Layer 4: Child Adapter

Confirm that `channel-*` entries were created:

```sh
ls -l /sys/bus/i2c/devices/5-0070/channel-*
```

If the Mux client exists but no Child Adapter was created, inspect the driver probe log first.

### Layer 5: Endpoint Device

Confirm that the sensor, EEPROM, PSU, or other endpoint is on the correct channel, and check whether:

- The I2C address is correct.
- The Device Tree node is under the correct `i2c@N` node.
- The device is powered.
- Reset has been deasserted.
- The channel has the correct pull-up resistors.
- Another driver has already bound to the device.

### Layer 6: Data Semantics

Even when an I2C transfer succeeds, the returned data may still be incorrect. In this case, check:

- Whether the register address is correct.
- Whether the register address is 8-bit or 16-bit.
- Whether the data is big-endian or little-endian.
- Whether the SMBus protocol is required.
- Whether PEC is required.
- Whether a command must be written before performing a read.
- Whether the value requires scaling, an offset, or unit conversion.

---

## Common OpenBMC Use Cases

I2C Muxes are common in OpenBMC systems, especially in large servers, GPU servers, storage servers, and modular systems.

### PSU Management

Different PSU slots may use the same PMBus address:

```text
Channel 0 --> PSU 0 @ 0x58
Channel 1 --> PSU 1 @ 0x58
Channel 2 --> PSU 2 @ 0x58
Channel 3 --> PSU 3 @ 0x58
```

The Mux allows the BMC to monitor each PSU separately, including:

- Voltage
- Current
- Power
- Temperature
- Fan speed
- Fault status

### Fan Board

Multiple hot-swappable fan modules may use the same fan-controller address. A Mux can distinguish the modules by their physical locations.

### FRU EEPROM

FRU EEPROMs in different slots often all use `0x50`:

```text
Channel 0 --> Slot 0 FRU EEPROM @ 0x50
Channel 1 --> Slot 1 FRU EEPROM @ 0x50
Channel 2 --> Slot 2 FRU EEPROM @ 0x50
```

### Sensor Board

Different boards may use the same addresses for temperature sensors, voltage monitors, or ADC devices. Through Mux channels, OpenBMC can map them to different inventory objects.

### Backplanes and Storage

A drive backplane may use multiple layers of I2C Muxes to manage:

- Drive slots.
- Expanders.
- Temperature sensors.
- FRU EEPROMs.
- LED controllers.
- Hot-swap controllers.

For OpenBMC, reading sensor values is not enough. It must also map:

```text
Parent Bus + Mux Address + Channel + Endpoint Address
```

to the actual:

```text
Chassis / Board / Slot / PSU / Fan / Drive
```

Only then can D-Bus, inventory, sensor services, and Redfish represent the hardware location correctly.

---

## Recommended Learning Sequence

1. Understand the basics of I2C SDA, SCL, addresses, and pull-up resistors.
2. Understand why devices with the same address cannot be placed directly on the same bus.
3. Learn the relationships among the Parent Adapter, Mux Client, Channel, and Child Adapter.
4. Trace one complete transfer: Child Adapter → `select()` → Parent Adapter → Endpoint.
5. Compare the schematic, Device Tree, and kernel binding schema.
6. Learn how to locate the actual Child Adapter through the `channel-*` links in sysfs.
7. Study the parent lock and selector transaction in the demo driver.
8. Debug layer by layer in the order of Controller, Mux, Channel, and Endpoint.
9. Study nested Muxes, multi-master operation, timeouts, and bus recovery.
10. Map hardware channels to OpenBMC inventory objects, sensors, and physical slots.

---

## Key Points When Reading the Demo Driver

When reading `client-driver/demo_i2c_mux.c`, pay particular attention to:

- How the driver initializes the Mux through `probe()`.
- How it obtains the Parent I2C Adapter.
- How it calls the I2C Mux core to create Child Adapters.
- How `select()` calculates `BIT(channel)`.
- Why the selector transaction must avoid locking the Parent Adapter twice.
- Whether `deselect()` is used to switch the Mux back to `0x00`.
- How created Child Adapters are cleaned up when probe fails.
- How they are unregistered during removal.
- How the number of channels is determined by driver data or the Device Tree.
- How multiple Child Adapters share the same Parent Controller.

After understanding these topics, you can continue by reading the official implementations:

```text
drivers/i2c/i2c-mux.c
include/linux/i2c-mux.h
drivers/i2c/muxes/i2c-mux-pca954x.c
```

---

## References

- [Linux I2C mux topology](https://docs.kernel.org/i2c/i2c-topology.html)
- [Linux I2C sysfs topology](https://docs.kernel.org/i2c/i2c-sysfs.html)
- Kernel Mux Core: `drivers/i2c/i2c-mux.c`
- Kernel Mux API: `include/linux/i2c-mux.h`
- Official driver example: `drivers/i2c/muxes/i2c-mux-pca954x.c`
- Device Tree bindings: `Documentation/devicetree/bindings/i2c/i2c-mux*.yaml`

> The Mux and driver in this project are intended only for understanding the Linux I2C Mux framework. For production hardware development, always follow the component datasheet, schematic, Device Tree binding, and corresponding upstream Linux driver.
