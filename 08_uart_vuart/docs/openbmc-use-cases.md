# OpenBMC use cases

## Host console

`obmc-console-server` owns the host TTY and exposes it through a Unix domain
socket. Clients can consume the stream locally, through SSH port 2200, through
IPMI Serial-over-LAN integration, or through a host logging service. Multiple
clients share one underlying console, so access control and input arbitration
matter.

Host firmware must route its console to the matching UART base address and IRQ.
The BMC VUART must be enabled, the console server must open the correct TTY, and
network-facing services must connect to the correct console ID.

## Multi-host and muxed consoles

Blade or multi-host systems may have several VUARTs or a GPIO-controlled UART
mux. A switch must disconnect or notify clients of the old route, serialize mux
ownership and mark gaps in logs. Stable console IDs should map to inventory.

## Management MCU

A fixed MCU, power sequencer or debug processor can use a physical UART with a
serdev driver. Define framing, CRC, request IDs, timeouts, retry limits, reset
recovery, version negotiation and rate limits. Do not parse an unbounded line in
kernel memory.

## Security and reliability

- Treat console input as privileged control of the host.
- Restrict remote access and audit sessions.
- Redact or protect logs that may contain secrets.
- Bound log size and handle storage failure.
- Test BMC/host resets, baud mismatch, stuck BREAK, RX flood and client death.
- Ensure the BMC shell cannot accidentally bind to the host-console TTY.

