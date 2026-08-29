# Debugging eSPI and KCS

## Layered decision path

1. Confirm BMC/host power, reset, eSPI clock/reset and link/channel status.
2. Confirm host I/O decode address agrees across DTS, BIOS/SMBIOS and host OS.
3. Confirm ASPEED KCS driver probe, BMC IRQ count, and `/dev/ipmi-kcsN`.
4. Confirm exactly one kcsbridge owns the matching device.
5. Confirm `phosphor-ipmi-host` is running and D-Bus dependencies respond.
6. Trace one host request through KCS read, D-Bus dispatch, response and host.

Commands:

    dmesg | grep -Ei 'espi|lpc|kcs|ipmi|sirq|reset'
    cat /proc/interrupts
    fuser -v /dev/ipmi-kcs3
    systemctl status phosphor-ipmi-kcs@ipmi-kcs3.service
    journalctl -u phosphor-ipmi-kcs@ipmi-kcs3.service
    journalctl -u phosphor-ipmi-host.service

## Symptom map

- No device node: kernel config/module, DTS disabled, probe failure, wrong parent
  binding, or missing character-device client.
- Device exists, no traffic: wrong I/O address, host did not enumerate KCS,
  eSPI Peripheral channel disabled, host reset, or missing BMC interrupt.
- IBF stuck: BMC did not consume host input, handler/IRQ blocked, or owner dead.
- OBF stuck: host did not issue READ_BYTE, response framing is wrong, or host
  driver stopped polling.
- KCS ERROR/ABORT: illegal state transition, request too long, partial userspace
  read/write, bridge restart, or host timeout.
- Fast kernel transaction but slow command: D-Bus lookup/provider delay inside
  host-ipmid, not eSPI wire time.
- Failure after host reset: stale KCS phase, lost Virtual Wire/reset event, host
  I/O decode not restored, or service ordering.

## Capture strategy

Use dynamic debug and tracepoints where available, inspect ASPEED status
registers only with the correct SoC documentation, and trace bridge
`read/write/poll/ioctl` calls. A logic analyzer must support eSPI decoding;
probing only one signal without the negotiated link context is insufficient.

