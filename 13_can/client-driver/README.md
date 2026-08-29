# Demo MMIO CAN controller

This is a controller-driver study aid, not a driver for existing silicon. Its
register offsets and behavior are fictional. It demonstrates the Linux-facing
shape: `alloc_candev()`, `can_priv`, bit-timing capabilities, netdev operations,
echo skb ownership, RX allocation, TX completion, bus-off, IRQ handling, and
`register_candev()`.

Build only for source/API study against the exact target kernel:

```sh
make
```

Do not instantiate `demo,mmio-can` on a real board. For real work, begin with
the controller datasheet and an upstream driver such as `m_can`, `flexcan`,
`sja1000`, or `mcp251xfd`. A production driver also needs audited reset and
clock sequencing, correct timing fields, FIFO/mailbox concurrency, NAPI, error
frames and state transitions, PM, timestamping, statistics, and hardware tests.

Linux CAN APIs evolve. Compile and test against the OpenBMC kernel branch that
will ship; do not assume an out-of-tree module built for another kernel is ABI
compatible.
