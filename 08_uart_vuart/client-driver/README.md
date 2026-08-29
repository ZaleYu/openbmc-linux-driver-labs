# Demo UART management-MCU client

This educational serdev client:

- Opens a fixed UART child described in Device Tree.
- Requests 115200 baud with no hardware flow control.
- Sends `PING\n` every five seconds.
- Receives newline-delimited replies in the serdev callback.
- Bounds each line to 63 bytes; an overflowed frame is discarded through its
  next newline so parsing resumes on a clean boundary.
- Stores only the most recent line and reports it with dynamic debug.

Enable dynamic debug:

    echo 'module demo_uart_mcu +p' > /sys/kernel/debug/dynamic_debug/control

The protocol is fictional. A production implementation needs message type,
length, checksum/CRC, sequence ID, response timeout, retry/backoff, reset and
firmware-version compatibility. Do not use unbounded strings or blocking work
inside `receive_buf()`.

Use serdev only for a fixed kernel-managed peripheral. A host console should
remain a TTY owned by obmc-console, not bind to this driver.
