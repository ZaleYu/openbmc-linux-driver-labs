# Debugging LIN

## Layered workflow

1. Confirm single master, transceiver supply/enable, termination/pull-up,
   ground, connector, and bus idle voltage.
2. Confirm UART/controller probe, exclusive ownership, pinctrl, clock, IRQ/DMA,
   baud, parity, flow control, and break support.
3. Measure break length, delimiter, sync byte, bit time, PID parity, response
   start, inter-byte spacing, checksum, and complete slot time.
4. Compare schedule and publisher configuration with the LDF/protocol database.
5. Verify service freshness, timeout, D-Bus mapping, recovery, and host state.

## Symptom guide

| Symptom | Focus |
|---|---|
| No bus activity | UART ownership, pinctrl, enable GPIO, schedule stopped |
| Header only | Slave power/sleep, wrong ID, no publisher, broken response path |
| `0x55` distorted | Baud/clock, transceiver, signal integrity |
| PID parity errors | Header corruption, ID/PID confusion, baud mismatch |
| Checksum errors | Classic vs enhanced, wrong length, collision, corruption |
| Duplicate response | Two publishers configured for one ID |
| Works idle, fails loaded | Userspace scheduling/FIFO/latency problem |
| Stale OpenBMC value | Poll work stopped, timeout not propagated, service issue |

## Kernel evidence

```sh
journalctl -k -b | grep -Ei 'serial|uart|serdev|lin|tty'
cat /proc/interrupts | grep -Ei 'serial|uart'
find /sys/bus/serial/devices -maxdepth 2 -print
grep -H . /sys/class/hwmon/hwmon*/{name,temp1_input} 2>/dev/null
```

Target dynamic debug to the exact UART and protocol driver. Excessive per-byte
logging changes timing, especially on a small BMC.

## Timing checks

At 19200 bit/s, one nominal bit is about 52.1 microseconds, so a 13-bit break is
about 677 microseconds. The demo requests a longer interval, but the UART driver
and hardware determine the actual line behavior. Measure it; do not infer
conformance from the requested sleep duration.

## Recovery tests

Test sleeping/missing slave, bad checksum, wrong PID parity, response timeout,
transceiver disable/re-enable, BMC service restart, UART error, managed-host
power transition, and schedule overload. Confirm stale sensor data becomes
unavailable or faulted rather than remaining indefinitely plausible.
