# Userspace tools

Install iproute2 and `can-utils`. Inspect first:

```sh
ip -details -statistics link show can0
ethtool -i can0
candump -e -x -ta can0
```

## Interface configuration

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 restart-ms 100
sudo ip link set can0 up
```

For CAN FD, only after verifying every node and timing requirement:

```sh
sudo ip link set can0 type can bitrate 500000 dbitrate 2000000 fd on
sudo ip link set can0 up
```

`ip -details` shows controller state, timing, clock, error counters, restart
count, supported modes, and driver statistics. Preserve this output before a
manual restart.

## can-utils

- `candump`: receive with filters, timestamps, error frames, and dropped count.
- `cansend`: transmit one Classical CAN or CAN FD frame.
- `cangen`: generate traffic for a controlled test network.
- `canplayer`: replay a capture; dangerous on production buses.
- `cansniffer`: show changing payload bytes interactively.
- `canbusload`: estimate bus utilization.
- `isotpsend`/`isotprecv`: ISO-TP transport tests.

Examples:

```sh
cansend can0 123#11223344
cansend can0 18DAF110#R        # RTR example; use only when protocol permits
candump can0,123:7FF           # standard-ID mask filter
candump can0,00000000:00000000 # all data frames
```

## Virtual CAN

`vcan` behaves like a CAN network interface without bitrate or hardware. It is
ideal for CI and protocol tests:

```sh
sudo ./scripts/setup_vcan.sh up
candump vcan0
cansend vcan0 123#DEADBEEF
```

It cannot validate arbitration timing, ACK behavior, termination, transceiver,
bus-off, or signal integrity.

## Socket API details

Raw sockets use `PF_CAN`, `SOCK_RAW`, and `CAN_RAW`. A program obtains the
interface index with `SIOCGIFINDEX` or `if_nametoindex()`, then binds a
`sockaddr_can`. Use `CAN_RAW_FILTER` to reduce wakeups. CAN FD requires
`CAN_RAW_FD_FRAMES` and `struct canfd_frame`; do not mix structure sizes.

Treat received CAN data as untrusted. Validate DLC/length before parsing,
perform endian conversion explicitly, enforce freshness/sequence requirements,
and rate-limit commands and error logs.
