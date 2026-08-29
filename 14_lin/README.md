# Linux LIN for OpenBMC

This package explains Local Interconnect Network (LIN) from protocol timing to
a Linux UART/serdev implementation and specialized OpenBMC integration. LIN is
a low-cost, single-master scheduled bus; it is not a slower form of CAN.

```text
LIN schedule -> master UART/serdev -> LIN transceiver -> one-wire bus
             -> slave response -> checksum/timeout -> hwmon -> OpenBMC
```

Upstream Linux currently has no universal LIN subsystem comparable to
SocketCAN. Products use SoC/vendor LIN controller drivers, UART/TTY/serdev, or
external bridges. This package therefore labels its UART-backed demo and
Device Tree binding as educational rather than implying a standard ABI.

## Repository map

| Path | Purpose |
|---|---|
| `docs/protocol.md` | Break, sync, protected ID, response, checksum, schedule |
| `docs/linux-lin-architecture.md` | Real Linux integration choices and serdev path |
| `docs/device-tree.md` | UART/serdev, transceiver GPIO, and dual-bus examples |
| `docs/userspace-tools.md` | TTY setup, break generation, capture, algorithms |
| `docs/debugging.md` | Electrical, timing, framing, scheduler, and service debug |
| `docs/openbmc-use-cases.md` | Specialized chassis/actuator/bridge scenarios |
| `client-driver/` | Educational serdev LIN temperature master/client |
| `device-tree/` | Fictional binding fragments requiring product adaptation |
| `userspace/` | PID/checksum helper and experimental TTY master |
| `scripts/` | Inspection, algorithm self-test, and explicit TTY lab wrapper |

## Demo protocol

The fictional master requests frame identifier `0x12` every second. A slave
responds with signed little-endian temperature in 0.1 degrees Celsius followed
by an enhanced checksum. The driver validates the checksum and exports
`temp1_input` through hwmon.

```text
Master: BREAK | 0x55 | protected-ID(0x12 = 0x92)
Slave:  TEMP_LSB | TEMP_MSB | enhanced-checksum
```

This is not a commercial device protocol.

## Quick algorithm lab

```sh
cc -O2 -Wall -Wextra -Werror -o lin_frame_tool userspace/lin_frame_tool.c
./lin_frame_tool pid 12
./lin_frame_tool checksum enhanced 12 10 27
./scripts/test_lin_algorithms.sh ./lin_frame_tool
```

The experimental TTY master requires an isolated LIN UART/transceiver and must
not be used on a production network:

```sh
cc -O2 -Wall -Wextra -Werror -o lin_tty_master userspace/lin_tty_master.c
sudo ./scripts/run_lin_tty_lab.sh /dev/ttyS3 12 2 ./lin_tty_master
```

## Safety and scope

- LIN uses a dedicated transceiver; never connect a UART pin directly to LIN.
- Only one node may publish a response for a scheduled identifier.
- Generic TTY scheduling may not meet deterministic LIN timing under load.
- UARTs differ in break duration, local echo, FIFO behavior, and baud tolerance.
- Do not transmit arbitrary headers or diagnostic frames on a production bus.
- The demo DT compatible and response format are fictional; define and validate
  a binding for actual hardware before upstream or product use.

## References

- Linux serdev API: `include/linux/serdev.h`
- Linux TTY/serial drivers: `drivers/tty/serial/`
- Linux hwmon API: <https://docs.kernel.org/hwmon/hwmon-kernel-api.html>
- LIN specifications and conformance requirements: obtain the licensed/current
  documents applicable to the product and transceiver.
