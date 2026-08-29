# I2C Mux Fundamentals

## What the component does

An I2C mux places selectable analog paths between an upstream SDA/SCL pair and
two or more downstream segments. A one-hot mux normally connects one segment;
an I2C switch may permit several simultaneously. Datasheets do not always use
these names consistently, so verify the control register and electrical table.

Common reasons to add one are:

- identical addresses on replaceable PSUs, DIMM groups, sensors, or FRU EEPROMs;
- isolation of a long, capacitive, hot-plug, or failure-prone branch;
- translation between voltage domains when the component explicitly supports it;
- topology expansion when a controller cannot meet total bus capacitance;
- separating security or ownership domains.

A mux does not create bandwidth. All child adapters serialize on the upstream
controller. It also does not automatically isolate a shorted segment unless
that segment is disconnected and the mux itself remains reachable.

## Typical control transaction

For the fictional four-channel device used here:

```text
START  0x70+W  ACK  0x04  ACK  STOP    # select channel 2
START  0x48+W  ACK  0x00  ACK
RESTART 0x48+R ACK  data... NACK STOP   # endpoint transfer
START  0x70+W  ACK  0x00  ACK  STOP    # optional idle disconnect
```

Linux presents the middle transfer through a child I2C adapter and performs
selection automatically. Applications should not reproduce this sequence by
manually writing the mux register.

## Electrical checklist

For every upstream and downstream segment document:

- voltage and whether pass-FET behavior supports that combination;
- pull-up value, effective parallel resistance, and rise time;
- segment capacitance, trace length, connectors, and hot-plug transients;
- maximum clock rate supported by every device on the active path;
- power-off leakage/back-power rules and power sequencing;
- mux reset state and whether a hardware reset GPIO is required;
- whether unused channels must remain disconnected;
- what happens when SDA or SCL is stuck low.

Measure SDA/SCL at both sides of the mux. A clean parent waveform does not prove
the selected child segment is healthy.

## Idle policy

Leaving a channel connected saves a selector write and can be required for
interrupt propagation on some designs. Disconnect-on-idle improves isolation,
allows identical addresses on switch-style parts, and limits a faulty branch.
The right policy depends on hardware behavior, IRQ routing, latency, and other
masters. Device Tree properties such as `i2c-mux-idle-disconnect` or
`idle-state` express the policy supported by a particular binding.

## Multi-master warning

A mux's selected state is shared hardware state. If a host and BMC can both
reach the selector, Linux locking protects only transfers made by that Linux
instance. Use a hardware arbiter, ownership protocol, mailbox, or partitioned
channels. Never assume two operating systems share an I2C lock.

