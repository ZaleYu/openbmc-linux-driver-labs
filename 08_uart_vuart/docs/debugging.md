# Debugging

## Decision path

1. Identify BMC debug console, host console, management MCU and external UART.
2. Verify voltage, ground, direction, pinmux and internal UART routing.
3. Confirm controller/VUART probe, TTY name and owning process.
4. Confirm baud, data/parity/stop bits and flow control on both ends.
5. Check IRQ counts and RX/TX/error counters under load.
6. Capture electrical signals or use an internal/external loopback.
7. Verify obmc-console socket, client, logger and network SOL separately.

Useful commands:

    dmesg | grep -Ei 'tty|uart|serial|vuart|overrun|framing|parity'
    cat /proc/tty/driver/serial
    cat /proc/interrupts
    fuser -v /dev/ttyVUART0
    systemctl status obmc-console@ttyVUART0.service
    journalctl -u obmc-console@ttyVUART0.service

Symptoms:

- Nothing received: wrong route/pinmux, reset, clock, power, TTY or owner.
- Garbage: baud clock, parity/data bits, inversion or voltage mismatch.
- Lost bursts: FIFO overrun, IRQ latency, DMA issue, no RTS/CTS or slow reader.
- TX only/RX only: crossed/missing wire, direction, flow-control or mux state.
- Console stops after host reset: VUART/LPC/eSPI ownership or SIRQ reinit.
- Duplicate characters: echo enabled at more than one layer.
- CR/LF anomalies: TTY output/input translations not disabled.

Dynamic debug, ftrace and trace events can isolate driver paths, but tracing
changes timing. Use a logic analyzer for the wire truth.

