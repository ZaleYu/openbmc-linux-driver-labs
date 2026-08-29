# Demo LIN serdev sensor

This educational driver owns a UART through serdev, sends a one-second LIN
header schedule for frame ID `0x12`, validates a two-byte enhanced-checksum
response, and exports temperature through hwmon.

It demonstrates:

- serdev open, baud, parity, flow control, break, write, and receive callback
- protected-ID parity and enhanced LIN checksum
- delayed-work scheduling and response accumulation
- optional transceiver enable GPIO
- hwmon publication and stale-data rejection
- shutdown ordering with work cancellation and write flush

Build against the exact target kernel:

```sh
make
sudo insmod demo_lin_sensor.ko
grep -H . /sys/class/hwmon/hwmon*/name
```

The compatible, response format, and scheduling policy are fictional. Generic
Linux scheduling does not prove LIN timing compliance. A production driver must
handle local echo, response deadlines, per-ID lengths/checksum models, complete
schedule tables, collisions, error statistics, sleep/wakeup, PM, hardware
errata, and a validated binding. Measure every frame with real hardware.
