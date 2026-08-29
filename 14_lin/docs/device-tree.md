# Device Tree

Device Tree describes the UART or dedicated controller, pinctrl, clock, IRQ,
DMA, transceiver controls, and attached non-discoverable protocol device. It
does not replace a LIN Description File (LDF) or encode the complete schedule.

## Educational serdev node

`device-tree/uart-lin-temperature.dts` places the fictional temperature device
under a UART. The child makes the UART a serdev controller rather than a normal
`/dev/ttyS*` port. The compatible is local to this package and has no upstream
binding. A real product must document a YAML binding and avoid conflicts with
console, debug, Bluetooth, or another UART consumer.

## Transceiver enable

`device-tree/uart-lin-transceiver.dts` adds an active-high enable GPIO consumed
by the demo. Verify whether the real pin is enable, sleep, standby, or active
low. Some transceivers require regulators and wake/interrupt GPIOs as well.

## Dual buses

`device-tree/dual-lin-uarts.dts` shows two independent UART-backed networks.
They require distinct transceivers, schedules, and ownership. Do not assume
UART numbering or probe order is stable product identity.

## Example properties

| Property | Meaning in this educational binding |
|---|---|
| `compatible` | Select the demo serdev protocol driver |
| `current-speed` | Requested UART baud rate, normally 19200 here |
| `demo,lin-frame-id` | Six-bit fictional sensor frame identifier |
| `demo,poll-interval-ms` | Demo schedule period |
| `enable-gpios` | Optional transceiver enable control |
| `status` | Board-level controller enablement |

The driver still enforces bounds; DT is not trusted application data.

## Validation

Because the demo compatible has no upstream schema, normal `dtbs_check` will
flag it until you create a binding. For production:

1. Define the hardware/protocol contract in YAML.
2. Validate property ranges and GPIO polarity.
3. Build the integrated board DTB with the exact kernel.
4. Confirm the UART is not the boot console.
5. Probe break, sync, baud, enable, and wake behavior on real hardware.

At runtime:

```sh
find /sys/bus/serial/devices -maxdepth 2 -type l -print
grep -H . /sys/class/hwmon/hwmon*/name
cat /proc/interrupts | grep -Ei 'uart|serial'
```
