# Debugging I3C

Debug in layers: electrical controller, core initialization, discovery,
binding, private traffic, IBI/Hot-Join, then OpenBMC service integration.

## 1. Controller and physical bus

- Is the controller enabled, clocked, reset, pinned, powered, and interrupting?
- Are SDA/SCL idle levels and rise times correct?
- Does the driver register an I3C master and its legacy I2C adapter?
- Does the configured bus mode match every connected legacy I2C target?

Check boot logs, clock/reset/pinctrl state, controller registers, IRQ counts,
and waveforms. Use an analyzer that decodes I3C CCC/DAA/IBI, not only I2C.

## 2. Bus initialization and DAA

Capture broadcast CCCs, ENTDAA arbitration, returned PID/BCR/DCR, assigned
address, parity, and ACK. If one target is missing, check its power-up time,
static-address conflict, PID validity, DAA participation, and Hot-Join support.

Do not repeatedly trigger DAA on a live production bus as a discovery shortcut.
It can change shared state and disturb bound drivers.

## 3. Device model and binding

Confirm `/sys/bus/i3c/devices` contains the expected PID and modalias. If the
device exists but the driver does not bind, compare manufacturer/part fields to
the `i3c_device_id` table, check module aliases/configuration, and inspect probe
errors. A DT `compatible` does not replace PID-based I3C matching.

## 4. Private transfers

For each failure record transfer direction/length, dynamic address,
`actual_len`, Linux errno, and I3C M0/M1/M2 error code. Common directions:

| Symptom | Investigate |
|---|---|
| `-ENODEV` after reset | address lost, DAA/reattach did not complete |
| `-EAGAIN` | IBI, Hot-Join, or role request won arbitration; bounded retry |
| `-EIO` with Mx error | decode protocol-specific error and waveform |
| short `actual_len` | target limit, early termination, controller queue issue |
| only mixed bus fails | legacy LVR, spike filter, clock, static conflict |
| timeout | stuck line, target power, controller state machine, IRQ/DMA |

## 5. IBI

Verify request/enable succeeds, controller resources are available, ENEC is
sent, target enables the documented event, and the controller IRQ queues an IBI
slot. Test payload lengths, back-to-back events, queue exhaustion, suspend,
disable/free ordering, and storm rate limiting. The demo exposes an IBI count
and last status for observation; those attributes are educational, not ABI.

## 6. Hot-Join and role handoff

Power a target after boot and capture its Hot-Join request followed by DAA and
driver probe. Test the race between service startup and late enumeration. For
multiple controllers, verify DEFSLVS propagation, ownership request/grant,
address-table consistency, and behavior when either controller resets.

## Kernel debugging tools

- `dmesg`/journal with dynamic debug for `drivers/i3c/` and the controller;
- tracepoints, ftrace/function-graph, kprobes, and BPF where supported;
- `/proc/interrupts`, debugfs/controller diagnostics, lockdep, kmemleak, KASAN;
- targeted controller register dumps and devcoredump;
- I3C-aware logic analyzer correlated with kernel timestamps.

Avoid printk flooding inside IRQ or IBI paths; it changes timing. Capture
counters and use rate-limited logs or tracing.

## Evidence bundle

Save kernel version/config, DTB, schematic, power/reset timing, bus inventory,
PID/BCR/DCR/dynamic addresses, bindings, controller registers, IRQ counters,
logs, analyzer trace, exact failure injection, and recovery result. The
included `debug_i3c.sh` collects read-only software evidence.

