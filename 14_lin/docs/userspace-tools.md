# Userspace tools

## Inspect before opening

```sh
./scripts/inspect_lin_uart.sh /dev/ttyS3
stty -F /dev/ttyS3 -a
udevadm info --attribute-walk /sys/class/tty/ttyS3
```

Opening or reconfiguring a console or serdev-owned UART can break the system.
If a DT child is bound through serdev, a normal tty node may not be available.

## Algorithm helper

`lin_frame_tool` calculates protected IDs and classic/enhanced checksums without
hardware. Inputs are hexadecimal:

```sh
./lin_frame_tool pid 12
./lin_frame_tool checksum classic 12 10 27
./lin_frame_tool checksum enhanced 12 10 27
```

For enhanced mode, the tool accepts the six-bit frame ID and internally creates
the PID. Diagnostic IDs `3C` and `3D` must use classic checksum in a real LIN
2.x implementation; the tool warns but remains an algorithm study utility.

## Experimental TTY master

`lin_tty_master` configures 8N1, requests 19200 baud, asserts break with
`TIOCSBRK`, sends `0x55` plus PID, reads a requested response length and checksum,
then validates it. It uses `poll()` timeouts and detects a matching header echo
before collecting the response.

This is not a conformance implementation. Linux scheduling, tty buffering,
driver break semantics, local echo, and USB-serial latency can violate LIN
timing. Measure the waveform and use a dedicated controller or real-time
scheduler when the product requires deterministic schedule slots.

## Capture and analysis

A LIN-aware analyzer is preferred because it decodes break, sync, PID parity,
checksum model, timing, and schedule. A UART decoder may show bytes but miss
break length, bus voltage, wake pulses, or collisions.

When recording frames, keep timestamps and error classifications. Application
debug needs the LDF/database: a byte stream alone cannot tell signal scaling,
publisher, expected period, or valid ranges.
