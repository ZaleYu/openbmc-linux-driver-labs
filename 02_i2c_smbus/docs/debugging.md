# I2C Debugging

Debug one layer at a time. A D-Bus sensor failure does not prove that the
kernel driver or physical bus is failing.

## 1. Establish the expected topology

Collect the schematic facts first:

- Controller instance and pinmux.
- Voltage domain and pull-ups.
- Target address straps.
- Mux/switch/channel path.
- Reset, power-good, presence, and alert GPIOs.
- Shared-bus ownership and power-state rules.

Then inspect software topology:

```sh
i2cdetect -l
find /sys/bus/i2c/devices -maxdepth 2 -type l -name 'channel-*' -print
find /proc/device-tree -iname '*i2c*' -o -iname '*sensor*' 2>/dev/null
```

## 2. Check enumeration and binding

```sh
ls -l /sys/bus/i2c/devices
cat /sys/bus/i2c/devices/1-0048/{name,modalias,uevent} 2>/dev/null
readlink /sys/bus/i2c/devices/1-0048/driver 2>/dev/null
dmesg | grep -Ei 'i2c|smbus|demo_i2c|timeout|arbitr|nack'
```

Interpretation:

- No adapter: controller/pinctrl/clock/reset/DTS/kernel-config problem.
- Adapter exists, no client: missing/disabled child node or wrong mux path.
- Client exists, no driver: matching, module, config, or probe failure.
- Driver bound, no hwmon: probe registration failure or subsystem issue.
- hwmon exists, no D-Bus object: OpenBMC service/configuration issue.

## 3. Enable focused kernel logging

Dynamic debug, if enabled in the kernel:

```sh
echo 'file drivers/i2c/* +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
echo 'module demo_i2c_sensor +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
```

Prefer a narrow file or module filter on a production-like system; enabling all
I2C messages can perturb timing and flood the journal.

Useful tracing depends on the kernel configuration and controller driver:

```sh
mount -t debugfs none /sys/kernel/debug 2>/dev/null || true
grep -i i2c /sys/kernel/tracing/available_events 2>/dev/null
grep -i i2c /sys/kernel/debug/tracing/available_events 2>/dev/null
```

Use function-graph tracing only with narrow filters. Excessive tracing can
change timing-sensitive behavior.

## 4. Inspect controller state

Read the exact controller driver's source and determine:

- Where status and interrupt registers are logged.
- How NACK, arbitration loss, timeout, and FIFO errors map to errno.
- Whether transfers use byte mode, buffer/FIFO mode, or DMA.
- Whether bus recovery is implemented.
- Which resets and clocks are required.
- Which downstream/OpenBMC patches differ from upstream.

Do not add arbitrary retries in the client driver before understanding whether
the controller already retries or whether replaying a command is safe.

## 5. Observe the physical bus

Use a logic analyzer for transaction shape and an oscilloscope for electrical
quality. Trigger on the target address or a long-low condition. Check:

- Correct address and R/W bit.
- ACK/NACK location.
- Repeated START versus STOP/START.
- Byte order and PEC.
- Clock rate and clock stretching.
- Rise time and voltage levels.
- Arbitration by the other controller.
- Mux channel selection before the target access.

## 6. Handle a stuck bus

A safe recovery design may include controller reset, pinctrl switching, up to
nine SCL recovery pulses, STOP generation, target reset, mux isolation, or
power cycling. The exact order is board-specific.

Before recovery on a multi-controller bus, ensure the other controller is not
transmitting. Blindly toggling SCL can corrupt its transfer. Record which line
is low, which segment is isolated, and which recovery action restored the bus.

## 7. OpenBMC layer

```sh
systemctl --failed
journalctl -b | grep -Ei 'sensor|hwmon|entity|i2c'
busctl tree xyz.openbmc_project.Hwmon 2>/dev/null
busctl tree xyz.openbmc_project.EntityManager 2>/dev/null
```

Service names vary by image. Verify the actual owner with `busctl list` and
inspect the expected object/interface rather than assuming a fixed daemon.

## Failure matrix

| Failure | Expected evidence | Next check |
|---|---|---|
| Wrong address | Address NACK | straps, schematic, DTS `reg` |
| Device held reset | Address NACK | reset GPIO and power rail |
| Wrong mux channel | Upstream mux works, target NACKs | sysfs channel links, mux control |
| Arbitration loss | `-EAGAIN` or controller status | second controller, ownership policy |
| Stuck SDA | timeout, line low while idle | isolate segments, target reset/recovery |
| Bad matching | client exists but no driver | modalias, OF table, module aliases |
| Bad conversion | plausible raw bytes, wrong hwmon value | signed format, shift, scale, endianness |
| Userspace missing | valid hwmon, absent D-Bus object | Entity Manager/dbus-sensors config and logs |

`../scripts/debug_i2c.sh BUS [ADDRESS]` collects a non-destructive first-pass
report. It does not scan or write the bus unless explicitly requested elsewhere.

