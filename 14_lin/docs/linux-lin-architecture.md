# Linux LIN architecture

## No universal upstream LIN network stack

Linux has mature UART, TTY, serdev, networking, and CAN frameworks, but no
single upstream LIN subsystem/SocketLIN ABI that every controller uses. Always
inspect the platform kernel: BSPs may contain vendor-specific LIN netdev,
character-device, tty, remoteproc, or mailbox implementations.

Three common designs are:

1. Dedicated LIN controller driver with a vendor ABI or networking integration.
2. UART controller plus kernel serdev protocol driver.
3. UART TTY plus a tightly controlled userspace master service.

An external USB/SPI/UART-to-LIN adapter may expose yet another protocol.

## serdev path used by the demo

```text
UART controller driver -> serdev controller -> demo LIN protocol driver
                       -> break/header + RX callback -> checksum -> hwmon
```

Serdev lets a kernel peripheral driver own a UART without exporting a tty to
general userspace. Relevant APIs include `devm_serdev_device_open()`,
`serdev_device_set_baudrate()`, `serdev_device_break_ctl()`,
`serdev_device_write()`, and the `receive_buf` callback.

The UART controller must implement `break_ctl`; otherwise LIN break generation
returns `-EOPNOTSUPP`. Even when available, verify break and delimiter timing on
the wire. Local echo behavior also varies by UART/transceiver topology.

## Demo lifecycle

The fictional driver opens the serdev UART at 19200 8N1, disables flow control,
registers hwmon, and schedules a header once per second. It asserts break long
enough for at least 13 bits, sends sync and protected ID, then accepts a fixed
two-byte response plus enhanced checksum. A valid response updates
`temp1_input`; missing/invalid responses leave the last sample but age tracking
would be mandatory in production.

Production implementations need a real schedule state machine, response
deadlines, echo suppression, per-frame checksum selection, collision handling,
wakeup/sleep, PM, diagnostics, statistics, and synchronization with other users.

## Source locations

| Area | Linux source |
|---|---|
| UART core and drivers | `drivers/tty/serial/` |
| serdev bus | `drivers/tty/serdev/`, `include/linux/serdev.h` |
| TTY UAPI | `include/uapi/asm-generic/termbits.h`, `ioctls.h` |
| hwmon | `drivers/hwmon/`, `include/linux/hwmon.h` |
| platform BSP LIN | Search vendor tree for `lin`, controller name, or SoC IP |

## Architecture boundary

Keep UART/controller mechanics in the kernel. Keep the LDF-derived schedule,
signal database, business rules, D-Bus publication, and authorization in a
reviewed service unless deterministic timing requires a kernel/MCU scheduler.
Never silently invent a generic LIN ABI around one vendor implementation.
