# Linux CAN architecture

Linux represents CAN controllers as network devices such as `can0`. SocketCAN
uses the networking stack and standard socket APIs rather than a private
character-device ABI.

```text
Application -> PF_CAN socket -> protocol (RAW/BCM/ISO-TP/J1939)
            -> CAN net_device -> controller driver -> transceiver -> bus
```

## SocketCAN protocols

- CAN_RAW: individual Classical CAN or CAN FD frames with kernel filters.
- CAN_BCM: cyclic transmission, content monitoring, and timeout notification.
- CAN_ISOTP: ISO 15765-2 segmented transport for larger diagnostic messages.
- CAN_J1939: SAE J1939 addressing and transport over extended identifiers.

The package's C tools use CAN_RAW. They bind to an interface index, use
`struct can_frame`, and apply an exact receive filter in the kernel.

## Controller-driver lifecycle

A typical platform driver allocates `struct net_device` with `alloc_candev()`,
fills its embedded `struct can_priv`, maps MMIO, obtains IRQ/clock/reset, sets
bit-timing capabilities and netdev operations, then calls `register_candev()`.

`ndo_open()` calls `open_candev()`, programs timing and filters, enables IRQs,
and starts the queue. `ndo_start_xmit()` validates the skb, stores an echo skb,
programs a mailbox/FIFO, and stops the queue if no TX slot remains. The IRQ/NAPI
path handles RX, TX completion, and error state changes. `ndo_stop()` disables
hardware and calls `close_candev()`.

The demo driver shows this shape against a fictional register map. A real driver
must implement hardware reset, clocks, precise bit timing, mailbox/FIFO locking,
NAPI budgeting, timestamping, error frames, runtime PM, suspend/resume, and all
errata described by the silicon vendor.

## Source locations

| Component | Linux source |
|---|---|
| CAN core/netdev helpers | `drivers/net/can/dev/` |
| Controller drivers | `drivers/net/can/` |
| SPI controllers | `drivers/net/can/spi/` |
| M_CAN core/platform | `drivers/net/can/m_can/` |
| Socket protocols | `net/can/` |
| UAPI | `include/uapi/linux/can/` |
| Driver API | `include/linux/can/dev.h`, `.../skb.h` |

## What usually changes

- Board integration: DTS clocks, IRQ, pinctrl, transceiver GPIO/regulator.
- New external CAN IC: use/extend an SPI controller driver and binding.
- SoC erratum: controller reset, FIFO, interrupt, DMA, timing, or PM paths.
- Application protocol: userspace SocketCAN service, not the controller driver.
- OpenBMC integration: Yocto packages/config, systemd ordering, D-Bus mapping,
  authorization, health reporting, and recovery policy.

Keep bus mechanics in the kernel driver and product semantics in a reviewed
userspace protocol service.
