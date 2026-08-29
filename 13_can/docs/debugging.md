# Debugging CAN

## Layered workflow

1. Confirm topology, two end terminators, common ground, transceiver supply,
   standby/silent pins, cable, and connector pinout.
2. Verify controller probe, pinctrl, clock, IRQ, transceiver binding, and netdev.
3. Compare nominal/data bitrate, sample point, FD/BRS mode, and oscillator
   tolerance with every node.
4. Capture interface state, error counters, statistics, error frames, and logs.
5. Observe CAN_H/CAN_L with a CAN-aware analyzer or oscilloscope.
6. Validate ID ownership, DLC, byte order, scaling, sequence, timeout, and policy.

Run `scripts/debug_can.sh can0` for a read-only snapshot.

## Symptom guide

| Symptom | Focus |
|---|---|
| Interface absent | DTS, driver/module, clock/reset, SPI probe, IRQ |
| `NOARP` but down | Normal CAN flag; configure bitrate and set up |
| TX errors/no ACK | Only one node, wrong bitrate, wiring, silent mode |
| Error-passive/bus-off | Persistent physical/timing fault or missing ACK |
| RX works, TX fails | Standby/silent pin, arbitration, TX wiring, permission |
| Sporadic CRC/stuff errors | Termination, stubs, EMI, ground, sample point |
| Socket receives nothing | Interface down, filters, wrong ID flags, namespace |
| CAN FD failure only | FD capability, data bitrate, BRS, transceiver bandwidth |

## Evidence collection

```sh
ip -details -statistics link show can0
candump -e -x -ta can0
journalctl -k -b | grep -Ei 'can|mcp25|m_can|bus.off'
cat /proc/interrupts
```

Error frames are diagnostic metadata delivered through SocketCAN when enabled;
they are not ordinary frames placed on the physical bus. `candump -e` decodes
them. Controller error counters from `ip -details` are often more useful than a
single generic kernel error.

## Bus-off recovery

Do not restart first and investigate later. Record state and counters, stop
transmission, correct bitrate/wiring/termination, then either:

```sh
sudo ip link set can0 type can restart
```

or configure a reviewed `restart-ms` policy. Confirm that repeated bus-off is
reported to OpenBMC health rather than hidden in an endless restart loop.

## Fault-injection checklist

Test missing peer/ACK, open termination, transceiver standby, bitrate mismatch,
bus-off, interface down/up, BMC service restart, queue overload, duplicate/
out-of-order application messages, and controller suspend/resume. Use an
isolated lab bus; never inject faults into a production control network.
