# Linux I3C for OpenBMC

This module is a practical study package for the Linux I3C subsystem used in
modern BMC platforms. It follows discovery and traffic from the controller to
an I3C target driver and then to an OpenBMC-facing hwmon interface.

```text
I3C controller -> Linux I3C master core -> DAA / PID matching -> target driver
              -> private transfers + IBI -> hwmon -> OpenBMC sensor service
```

The examples use a fictional `demo,i3c-temp-sensor` with manufacturer ID
`0x0123`, part ID `0x0456`, a small register map, and an optional In-Band
Interrupt (IBI). The example targets the Linux 6.18 I3C client API used by the
study platform. Linux 6.19 changes the transfer API; see the compatibility note
before compiling against another kernel.

## Repository map

| Path | Purpose |
|---|---|
| `docs/protocol.md` | I3C SDR, CCC, DAA, PID, IBI, Hot-Join, and legacy I2C |
| `docs/linux-i3c-architecture.md` | Controller, core, device, driver, and source paths |
| `docs/device-tree.md` | Three-cell addressing and mixed-bus examples |
| `docs/userspace-tools.md` | Sysfs inspection and safe production access |
| `docs/debugging.md` | Layered controller/discovery/transfer/IBI debugging |
| `docs/openbmc-use-cases.md` | MCTP, sensors, hubs, multi-master, and hot-plug |
| `client-driver/` | Educational I3C hwmon target driver with IBI support |
| `device-tree/` | I3C target, mixed I3C/I2C, and multi-master fragments |
| `userspace/` | Portable sysfs inventory and hwmon test programs |
| `scripts/` | Bus inventory, diagnostics, and IBI log monitoring |

## Demo register map

| Address | Name | Access | Meaning |
|---:|---|---|---|
| `0x00` | `DEVICE_ID` | RO | Expected value `0xA5` |
| `0x01` | `TEMP_MSB` | RO | Signed 12-bit temperature bits 11:4 |
| `0x02` | `TEMP_LSB` | RO | Temperature bits 3:0 in bits 7:4 |
| `0x03` | `STATUS` | RO | Bit 0: high-temperature alarm |
| `0x04` | `CONFIG` | RW | Fictional configuration register |
| `0x05` | `TEMP_HIGH` | RW | Signed whole-degree threshold |

The conversion is `raw12 * 62.5 m°C`. This is not a commercial device.

## Quick start

Inspect a target system without issuing I3C traffic:

```sh
./scripts/list_i3c.sh
./scripts/debug_i3c.sh
cc -O2 -Wall -Wextra -Werror -o inspect_i3c_device \
   userspace/inspect_i3c_device.c
./inspect_i3c_device /sys/bus/i3c/devices/<device-name>
```

After the kernel target driver binds, locate and read its hwmon interface:

```sh
grep -H . /sys/class/hwmon/hwmon*/name
cc -O2 -Wall -Wextra -Werror -o test_i3c_sensor \
   userspace/test_i3c_sensor.c
./test_i3c_sensor /sys/class/hwmon/hwmonN
```

Build the out-of-tree driver with the exact Linux 6.18 target headers:

```sh
make -C client-driver
sudo insmod client-driver/demo_i3c_sensor.ko
```

## Safety and scope

- I3C is not simply faster I2C. DAA, CCCs, push-pull phases, IBI, Hot-Join,
  bus ownership, and legacy-device timing change bring-up and debugging.
- Dynamic addresses are runtime state. Identify devices by PID and physical
  topology, not only the current address.
- Do not send arbitrary CCCs, rerun DAA, reset addresses, or force userspace
  transfers on a production bus while kernel drivers are active.
- Generic userspace I3C ABIs differ across upstream and vendor/OpenBMC kernels.
  Prefer subsystem drivers and sysfs/D-Bus interfaces for production software.
- Adapt every example to the controller binding, target datasheet, schematic,
  voltage/timing constraints, kernel branch, and OpenBMC integration policy.

## Recommended study order

1. Read `docs/protocol.md` and compare I3C phases with I2C.
2. Trace boot discovery: RSTDAA/SETDASA or ENTDAA, PID/BCR/DCR, device model.
3. Decode the three-cell `reg` representation in the DTS examples.
4. Trace a private SDR read and an IBI through the demo driver.
5. Learn sysfs identity and driver binding without assuming dynamic addresses.
6. Debug a mixed I3C/I2C bus and a delayed Hot-Join target.
7. Map I3C devices to OpenBMC inventory, MCTP, telemetry, and recovery policy.

## References

- Linux I3C protocol: <https://docs.kernel.org/driver-api/i3c/protocol.html>
- Linux I3C device API: <https://docs.kernel.org/6.18/driver-api/i3c/device-driver-api.html>
- Linux I3C master API: <https://docs.kernel.org/driver-api/i3c/master-driver-api.html>
- Device Tree binding: `Documentation/devicetree/bindings/i3c/i3c.yaml`
- Kernel sources: `drivers/i3c/`, `include/linux/i3c/`

