# Userspace tools

Inventory:

    cat /proc/tty/driver/serial
    ls -l /sys/class/tty
    udevadm info /dev/ttyS4
    stty -F /dev/ttyS4 -a
    systemctl status serial-getty@ttyS4.service

Configure a raw port:

    stty -F /dev/ttyS4 115200 cs8 -cstopb -parenb \
        -ixon -ixoff -crtscts raw -echo

Useful programs include `picocom`, `minicom`, `screen`, `socat` and `hexdump`.
For binary protocols, log hex and timestamps; terminal rendering can hide NUL,
control characters and CR/LF conversion.

Only one process should own a protocol port unless an explicit multiplexer
provides sharing. Check `lsof`/`fuser`, getty, obmc-console, Bluetooth/GNSS
services and serdev binding before opening it.

The included `uart_loopback_test` requires TX-to-RX loopback and verifies a
binary pattern. `vuart_console_logger` captures bytes with timestamps without
sending data. Neither should be run against an active boot console without
planning for the consequences.

