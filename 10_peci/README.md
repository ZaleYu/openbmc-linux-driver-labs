# Linux PECI for OpenBMC

This lab explains how Intel CPU temperature and platform telemetry travel through:

```text
PECI Wire
Linux PECI Controller Driver
Linux PECI Core
CPU / Temperature Driver
Hwmon Sysfs
OpenBMC Sensor Service
D-Bus
Thermal Policy / Redfish
```

to provide fan control, event logging, Web UI, and remote management for the BMC.

---

# 1. What Is PECI?

PECI stands for Platform Environment Control Interface. It is an Intel processor-sideband management interface that lets the BMC obtain CPU and platform information without depending on the host OS.

Common uses include CPU package/die/DTS temperatures, TjMax, Tcontrol, throttle thresholds, DIMM temperatures, package configuration, CPU/PCIe management information, presence and identity, platform telemetry, and error, throttle, or power state.

Conceptually, PECI is a dedicated management channel between the BMC and an Intel CPU. The BMC may monitor temperatures and control fans even before the host OS starts.

---

# 2. PECI Versus I2C

| Item | PECI | I2C |
| --- | --- | --- |
| Primary use | Intel CPU/platform management | General peripheral communication |
| Signals | Dedicated single-wire data signal | SDA + SCL |
| Clock | Embedded in protocol timing | Separate SCL |
| Devices | Intel processors/related endpoints | Sensors, EEPROMs, PMICs, etc. |
| Commands | PECI commands | I2C read/write or SMBus |
| Response status | Completion Code | ACK/NACK and data |
| Linux subsystem | PECI Core | I2C Core |

Do not use `i2cget`, `i2cset`, or `i2cdetect` for PECI; PECI does not appear as `/dev/i2c-N`.

---

# 3. PECI Hardware Architecture

```text
Intel CPU Package
        | PECI Signal
        v
BMC PECI Controller
        |
        v
Linux PECI Controller Driver
```

Multi-socket systems may connect CPU Socket 0 and 1 to the same controller with different PECI addresses. Addresses commonly begin at `0x30`, but must be confirmed from the CPU platform guide, schematic, socket configuration, PECI specification, and Linux driver support.

---

# 4. Single-Wire PECI Signal

PECI uses one bidirectional data line, normally with a pull-up:

```text
BMC PECI Controller ---- PECI_DATA ---- CPU
                              |
                           Pull-up
```

During bring-up, verify pinmux, pull-up, voltage, trace length/capacitance, CPU power and reset, controller clock, buffers/level shifters, and whether an unpowered CPU holds the line. Software cannot fix incorrect electrical design.

---

# 5. PECI Data Path

```text
Intel CPU Package
        | PECI Command / Response
        v
PECI Wire --> BMC Controller Hardware
        |
        v
Controller Driver --> Linux PECI Core --> peci-cpu
                                         +--> peci-cputemp
                                         +--> peci-dimmtemp
        |
        v
Hwmon Sysfs --> OpenBMC Sensor Service --> D-Bus
        +--> Thermal Policy / Fan Control
        +--> Event Logging
        +--> Redfish / Web UI
```

---

# 6. Repository Structure

| Directory | Contents |
| --- | --- |
| `docs/` | Protocol, Linux architecture, Device Tree, tools, debugging, and OpenBMC use cases |
| `kernel-study/` | Upstream source map and kernel configuration fragment |
| `device-tree/` | AST2500/AST2600 controller and timing examples |
| `userspace/` | Read-only device inventory and hwmon dump tools |
| `scripts/` | Inventory, diagnostics, and temperature snapshots |

The userspace tools read kernel-created sysfs/hwmon interfaces; they do not send arbitrary raw write commands to the CPU.

---

# 7. Linux PECI Software Architecture

## Layer 1: Controller Driver

Operates SoC hardware: bit rate, command buffer, transfer start, interrupt/completion wait, response, timeout, and bus errors.

## Layer 2: PECI Core

Registers controllers and buses, manages devices, provides transfer APIs, matches drivers, and separates controllers from client drivers.

## Layer 3: CPU and Functional Drivers

Drivers such as `peci-cpu`, `peci-cputemp`, and `peci-dimmtemp` identify CPUs, select commands, parse Completion Codes, handle generation differences, convert fixed-point temperatures, cache data, and create hwmon attributes.

## Layer 4: Hwmon and OpenBMC

Hwmon exposes standard files such as `temp1_input`, `temp1_max`, `temp1_crit`, and `temp1_label`; OpenBMC publishes them on D-Bus.

---

# 8. Role of `peci-cpu`

`peci-cpu` is the core driver for a CPU package on the PECI bus. It may detect CPU presence and family/model, create CPU devices, share information with functional drivers, and handle generation capabilities.

```text
PECI Controller --> PECI Core --> peci-cpu
                                  +--> peci-cputemp
                                  +--> peci-dimmtemp
```

It may not expose all temperature files directly; functional drivers create interfaces according to CPU type.

---

# 9. `peci-cputemp`

This CPU-temperature hwmon driver may expose package, die, DTS, Tcontrol, Tthrottle, TjMax, critical, and possibly per-core temperatures.

```text
/sys/class/hwmon/hwmonX/
├── name
├── temp1_input
├── temp1_label
├── temp1_max
├── temp1_crit
├── temp2_input
└── temp2_label
```

Attributes depend on CPU generation, kernel and driver support, power state, and available PECI commands.

---

# 10. `peci-dimmtemp`

This driver obtains DIMM temperatures through the CPU memory controller and PECI, then exposes hwmon inputs.

OpenBMC must map PECI channels to CPU socket, memory channel, DIMM slot, and inventory object; this mapping depends on CPU generation and platform topology.

---

# 11. PECI Commands

PECI commands may cover ping, device information, package configuration read/write, PCI configuration, endpoint configuration, and temperature/telemetry access.

```text
BMC Request:
Target Address + Command + Parameters + Write Length + Read Length
        |
        v
CPU Execution
        |
        v
Completion Code + Response Data
```

PECI is not an arbitrary register bus. Package indices and registers from one CPU generation must not be applied blindly to another.

---

# 12. Completion Code

The controller driver reports whether the physical transfer succeeded; the CPU Completion Code reports whether the CPU accepted and completed the command.

| Error Type | Example |
| --- | --- |
| Controller/Bus | Timeout or no wire response |
| Protocol | Incorrect response length |
| Completion Code | CPU busy, unsupported command, invalid data |
| Conversion | Format does not match CPU generation |

A completed wire transfer does not guarantee command acceptance.

---

# 13. Why Raw Commands Must Not Be Copied

Commands, package indices, parameters, offsets, lengths, Completion Codes, temperature encoding, PCI addressing, endpoint configuration, and power-state restrictions vary by CPU generation.

Incorrect commands can return wrong data or temperatures, repeatedly fail, time out during initialization, modify package configuration, or interfere with management software. Prefer the Linux PECI core helpers and upstream CPU/hwmon drivers.

---

# 14. CPU Temperature Is Not Necessarily Direct Celsius

A digital thermal sensor may report distance from TjMax rather than “70°C.” For example, TjMax 100°C with a 30°C margin corresponds to 70°C. Raw formats may also be fixed-point, such as 1/64°C per LSB. Exact sign and conversion depend on the command and CPU generation.

---

# 15. TjMax, Tcontrol, and Tthrottle

- **TjMax:** reference for the maximum processor junction temperature.
- **Tcontrol:** platform cooling-control reference.
- **Tthrottle:** threshold or state related to thermal throttling.

They are not interchangeable; follow CPU platform documentation and driver implementation.

---

# 16. Hwmon Temperature Units

Linux hwmon normally uses millidegrees Celsius:

```text
70000 = 70°C
42500 = 42.5°C
-5000 = -5°C
```

Thus `temp1_input` value `68000` means 68°C, not 68,000°C.

---

# 17. Why Does the Driver Cache Data?

PECI transfers are more expensive than reading memory; the CPU may also be busy or in an unsuitable power state. Frequent reads from multiple services can increase load, busy responses, timeouts, latency, and duplicate commands.

Drivers therefore cache recent values: the first read performs and parses a transfer; subsequent reads within a short interval return the cached result.

---

# 18. Role of the Device Tree

Device Tree describes controller MMIO, interrupt, clock, reset, pin control, timing/frequency, SoC properties, and status.

```dts
&peci0 {
    status = "okay";
};
```

`&peci0` references a controller already defined in the SoC DTSI. A conceptual full node is:

```dts
peci0: peci-controller@12340000 {
    compatible = "vendor,soc-peci";
    reg = <0x12340000 0x1000>;
    interrupts = <0 42 4>;
    clocks = <&clock_controller 10>;
    resets = <&reset_controller 5>;
    status = "disabled";
};
```

The real node must follow the target SoC binding.

---

# 19. AST2500 and AST2600

These common OpenBMC BMC SoCs may differ in `compatible`, registers, clocks, resets, timing, interrupts, driver features, and maximum bit rate. Do not copy AST2600 properties or timing to AST2500. Check the target kernel’s PECI controller/ASPEED bindings and SoC DTSI.

---

# 20. Timing Configuration

Timing and bit rate must satisfy the BMC controller, CPU platform, board traces, pull-up, buffers, CPU power state, and kernel driver.

Excessive speed can cause timeouts, frame-check errors, unstable responses, cold-boot failures, or failure on one socket. Excessively low speed can delay sensor updates, hwmon reads, polling, and thermal decisions.

Use the controller binding, SoC datasheet, CPU platform guide, and measured waveform together rather than copying another platform.

---

# 21. Kernel Device Discovery

After controller probe, Linux creates a PECI bus:

```sh
ls -l /sys/bus/peci/
find /sys/bus/peci/devices -maxdepth 2 -print
```

Common directories are `/sys/bus/peci/devices/` and `/sys/bus/peci/drivers/`. Device names vary by kernel and controller. Do not infer a CPU socket only from a fixed name; inspect sysfs and driver binding.

---

# 22. Quick Start

Build:

```sh
make -C userspace
```

Expected tools:

```text
userspace/peci_inventory
userspace/peci_hwmon_dump
```

Run inventory and hwmon dump:

```sh
./userspace/peci_inventory
./userspace/peci_hwmon_dump
sudo ./scripts/debug_peci.sh
```

Inventory checks buses, devices, driver binding, addresses, and sysfs. The hwmon tool reads names, labels, inputs, thresholds, and available CPU/DIMM temperatures. Diagnostic scripts should prefer read-only operations and avoid arbitrary raw writes on production systems.

---

# 23. Examining Hwmon Manually

Find devices by name:

```sh
for path in /sys/class/hwmon/hwmon*; do
    if [ -r "$path/name" ]; then
        printf "%s: %s\n" "$path" "$(cat "$path/name")"
    fi
done
```

Look for names such as `peci_cputemp` or `peci_dimmtemp`, then inspect:

```sh
grep -H . /sys/class/hwmon/hwmon3/temp*_input
grep -H . /sys/class/hwmon/hwmon3/temp*_label
grep -H . /sys/class/hwmon/hwmon3/temp*_max
grep -H . /sys/class/hwmon/hwmon3/temp*_crit
```

Do not hard-code `hwmon3`; numbering may change after reboot. Locate the device through its `name` file.

---

# 24. OpenBMC Sensor Service

The service periodically reads `temp*_input` and creates D-Bus sensor objects:

```text
Hwmon temp1_input
        |
        v
OpenBMC Sensor Service
        |
        v
xyz.openbmc_project.Sensor.Value
        |
        v
D-Bus Sensor Object
```

Objects may include current value, unit, warning/critical thresholds, availability, functional state, associations, and inventory path.

---

# 25. Entity Manager

Entity Manager JSON describes board-level sensor and inventory relationships, including hwmon-to-CPU mapping, sensor name, thresholds, D-Bus path, socket, associations, and polling/configuration.

```text
Entity Manager JSON --> D-Bus Configuration
    --> Sensor Service --> PECI Hwmon Sensor
    --> D-Bus Temperature Sensor
```

The kernel driver answers “how to obtain the temperature”; Entity Manager and the sensor service answer “which product component it represents and how it is exposed.”

---

# 26. Thermal Policy

```text
CPU Temperature --> D-Bus Sensor --> Thermal/Fan Policy
    --> PWM Fan Target --> Fan Speed
```

Example policy:

```text
CPU < 60°C  --> Fan Low
CPU 60–75°C --> Fan Medium
CPU > 75°C  --> Fan High
Sensor Fail --> Fan Full Speed
```

Production policy normally adds hysteresis, multiple-sensor aggregation, fan-failure detection, sensor timeouts, fail-safe behavior, redundancy, acoustic policy, and coordination with CPU throttling.

---

# 27. Redfish

```text
PECI Temperature --> Hwmon --> D-Bus --> bmcweb / Redfish
    --> Remote Management Client
```

Redfish can expose CPU temperature, thresholds, health, critical alarms, processor inventory, and thermal collections. The PECI driver does not generate Redfish JSON directly; OpenBMC userspace converts kernel data to D-Bus and Redfish.

---

# 28. `peci-pcie`

`peci-pcie` obtains PCIe/processor-related management data through PECI. It differs from temperature hwmon drivers and is not a Linux PCIe host-controller driver.

```text
PECI
  +--> cputemp: CPU Temperature
  +--> dimmtemp: DIMM Temperature
  +--> peci-pcie: PCIe/Processor Management Information
```

Exact functions, D-Bus interfaces, and supported CPU generations depend on current OpenBMC source and platform configuration.

---

# 29. Effect of CPU Power State

```text
CPU Unpowered       --> Possibly No PECI Response
CPU in Reset        --> Temporary Timeout
Just Powered On     --> Some Commands Unavailable
Low-Power State     --> Delayed or Limited Response
Host Off            --> Some Capabilities May Remain
```

A timeout is not necessarily a driver bug. Check host power state, CPU presence, power-good, reset, VR power, platform initialization, BIOS/management-engine state, and PECI pin electrical state.

---

# 30. Layered Debugging Procedure

## Layer 1: CPU and Power

Verify CPU installation, power rails, power-good, reset, host power state, and socket address.

## Layer 2: PECI Wire

Use an oscilloscope or supported analyzer to check stuck-low state, pull-up, voltage, edge timing, BMC requests, and CPU responses.

## Layer 3: Device Tree

Verify `status`, `compatible`, MMIO, IRQ, clock, reset, pinctrl, and binding-compliant timing.

## Layer 4: Controller Driver

```sh
dmesg | grep -i peci
```

Confirm successful probe/registration and no persistent clock, reset, timeout, or interrupt errors.

## Layer 5: PECI Core and CPU Device

```sh
ls -l /sys/bus/peci/devices/
ls -l /sys/bus/peci/drivers/
```

Confirm device creation, address, `peci-cpu` binding, and successful probe.

## Layer 6: Hwmon

Find `peci_cputemp` and `peci_dimmtemp` by `name`. If a device exists without hwmon, the CPU may be unsupported, the driver disabled/unloaded, identification failed, or power state may block the command.

## Layer 7: OpenBMC D-Bus

Verify sensor-service operation, Entity Manager configuration, hwmon matching, D-Bus object creation, availability, and thresholds.

## Layer 8: Redfish

Verify D-Bus sensor existence, bmcweb access, inventory association, correct resources, units, and thresholds.

---

# 31. Common Problems

## `/sys/bus/peci/devices/` Is Empty

Possible causes: disabled controller, Device Tree error, unloaded driver, unpowered CPU, incorrect pinmux/electrical design, or wrong address.

## CPU Device Exists but No `peci-cputemp`

Possible causes: disabled hwmon driver, unsupported CPU generation, unloaded module, model-identification failure, old kernel, or unsuitable power state.

## Temperature Is Always Zero

Possible causes: wrong hwmon path, invalid driver data, CPU not ready, conversion error, unavailable sensor, or no valid cached data yet.

## Temperature Is Negative

This may be a valid raw offset relative to TjMax. The upstream driver should convert it; userspace that interprets raw values directly may display misleading negative values.

## Temperature Updates Slowly

Possible causes: driver cache, polling interval, bit rate, busy CPU, many sensors, or timeout/retry behavior.

## Readable Only When Host Is On

This may match platform design. Confirm expected behavior during host-off, standby, and reset states.

---

# 32. Kernel Configuration

Enable the PECI core, controller driver, CPU driver, CPU-temperature driver, DIMM-temperature driver, and HWMON. Actual symbols vary by kernel.

```sh
grep -R "config.*PECI" drivers/peci drivers/hwmon/peci
zcat /proc/config.gz | grep -E 'PECI|HWMON'
grep -E 'PECI|HWMON' /boot/config-$(uname -r)
```

---

# 33. Device Tree Schema Validation

```sh
make dt_binding_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/peci/

make dtbs_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/peci/
```

Check controller `compatible`, `reg`, interrupt, clock, reset, timing, undefined properties, and required properties. Binding paths may differ by kernel version.

---

# 34. Safety Considerations

PECI can support package configuration, PCI configuration access, endpoint configuration, writes, and platform control—not only temperature reads.

Unreviewed raw writes can change configuration, interfere with consumers, cause sensor timeouts, trigger unexpected CPU behavior, create Completion Code errors, or temporarily make thermal data unavailable.

Prefer read-only hwmon and upstream helpers; validate each CPU generation separately; log Completion Codes/timeouts; coordinate access frequency with the sensor service.

---

# 35. Why Prefer Upstream Hwmon?

Upstream `peci-cputemp` and `peci-dimmtemp` already handle CPU family/model differences, command format, Completion Codes, lengths, fixed-point conversion, TjMax/Tcontrol, caching, power states, errors, and attribute naming.

Direct raw userspace access must reimplement all of this and may conflict with the kernel driver. Prefer:

```text
PECI Controller Driver --> Linux PECI Core
    --> Upstream PECI Hwmon Driver --> Standard Hwmon Sysfs
```

---

# 36. Recommended Learning Sequence

1. Understand PECI as the BMC–Intel CPU sideband interface.
2. Distinguish PECI from I2C.
3. Understand controller, core, CPU, and hwmon driver responsibilities.
4. Examine `/sys/bus/peci/`.
5. Inventory CPU devices with `peci_inventory`.
6. Read temperatures with `peci_hwmon_dump`.
7. Understand millidegrees Celsius.
8. Understand TjMax, Tcontrol, and relative temperature.
9. Distinguish Completion Codes from bus errors.
10. Compare AST2500/AST2600 bindings.
11. Trace hwmon to OpenBMC D-Bus.
12. Trace D-Bus to thermal policy and Redfish.
13. Study generation-specific raw commands last.

---

# 37. Summary of Core Concepts

1. **PECI is a dedicated BMC–Intel CPU management interface, not I2C.**
2. **Linux PECI comprises the controller driver, core, CPU/functional drivers, and hwmon.**
3. **`peci-cpu` handles the CPU device and generation; `peci-cputemp` and `peci-dimmtemp` handle temperatures.**
4. **Hwmon uses millidegrees Celsius: `70000` means 70°C.**
5. **Raw temperature may be fixed-point relative to TjMax rather than direct Celsius.**
6. **Completion Code reports CPU command acceptance; controller timeout is a different layer.**
7. **Commands and registers are generation-specific and must not be copied blindly.**
8. **Prefer upstream helpers and hwmon drivers over arbitrary userspace raw commands.**
9. **OpenBMC converts hwmon data to D-Bus for thermal policy and Redfish.**
10. **No response may reflect power, reset, pull-up, pinmux, or timing—not only a driver problem.**
11. **Successful DTS compilation does not prove operation; validate waveforms, discovery, hwmon, D-Bus, and Redfish.**
12. **Production systems require fan fail-safe behavior for sensor failures and PECI timeouts.**
