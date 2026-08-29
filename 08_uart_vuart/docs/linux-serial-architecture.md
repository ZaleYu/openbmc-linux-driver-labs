# Linux serial architecture

    UART hardware
      -> low-level controller driver: struct uart_port + struct uart_ops
      -> serial core
      -> TTY core + line discipline
      -> /dev/ttyS*, console/getty/application

The low-level driver maps registers, clocks, reset, IRQ, FIFO and DMA; handles
startup/shutdown, termios, TX/RX and modem control; and registers ports with the
serial core. Read `drivers/tty/serial/`, especially `8250/`, `amba-pl011.c` and
the driver selected by the BMC SoC.

The TTY layer provides buffering, termios and line disciplines. Raw binary
protocol applications should configure raw mode rather than inherit canonical
input processing, echo, CR/LF translation or software flow control.

## serdev

Serdev binds a kernel client to a device described as a child of a serial
controller. It is appropriate for fixed Bluetooth, GNSS, management MCU and
similar devices that need a kernel driver—not for a general login console.

A serdev client installs `struct serdev_device_ops`, opens the device, chooses
baud/flow control, receives bytes through `receive_buf()` and writes through
`serdev_device_write()` or related helpers. The synchronous write helper uses
`write_wakeup` to resume after a controller accepts only part of a buffer.

## VUART

An ASPEED VUART is 16550-compatible on the BMC side but connects to the host
through an internal LPC/eSPI-visible UART window instead of external TX/RX pins.
The in-tree implementation is `drivers/tty/serial/8250/8250_aspeed_vuart.c`.
Userspace still sees a TTY, commonly named like `ttyVUART0` by platform rules.
