# Linux UART and VUART for OpenBMC

This lab separates three related paths:

- A physical UART controller exposed by the Linux serial core and TTY layer.
- A fixed UART-attached device managed by a kernel `serdev` client.
- A BMC virtual UART that presents a host console through LPC/eSPI plumbing.

## Contents

- `docs/` — protocol, Linux architecture, Device Tree, tools, debugging, and
  OpenBMC console use cases.
- `client-driver/` — educational serdev management-MCU client.
- `device-tree/` — serdev child, ASPEED VUART, and boot-console examples.
- `userspace/` — UART loopback test and timestamped console logger.
- `scripts/` — inventory, diagnosis, and safe console capture.

## OpenBMC host-console path

    Host firmware/Linux console -> Host UART/LPC/eSPI -> BMC VUART TTY
        -> obmc-console-server -> Unix socket
        -> local client / SSH 2200 / IPMI SOL / host logger

The BMC debug console and the host console are different endpoints. Confirm
which side owns a port before starting getty, obmc-console, or a test tool.

## References

- Linux `Documentation/driver-api/serial/`
- Linux `include/linux/serdev.h`
- Devicetree `serial/8250.yaml` and `serial/serial.yaml`
- OpenBMC `docs/console.md` and `obmc-console`

