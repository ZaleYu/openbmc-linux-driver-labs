# Debugging I2C Mux Topologies

Debug from upstream to downstream. Record the first layer that differs from the
schematic instead of immediately forcing transfers.

## 1. Physical parent

- Is the controller enabled and represented by an `i2c-N` adapter?
- Are upstream SDA/SCL at the correct voltage and idle high?
- Is the mux powered, out of reset, and at the expected address?
- Does the parent clock rate meet mux and endpoint limits?

Check `i2cdetect -l`, adapter `name`, pinctrl, clocks, reset, and kernel logs.

## 2. Mux client and driver

The mux client directory (for example `5-0070`) should exist and have the
expected `name`, `of_node`, and `driver` link. If it is instantiated but
unbound, check `compatible`, kernel configuration, module availability, probe
errors, reset polarity, and parent adapter functionality.

Do not force a userspace write to the selector while diagnosing a bound driver.
That changes shared state behind the mux core's back.

## 3. Logical channel adapters

Count and resolve the mux device's `channel-*` links. Missing channels usually
mean a probe/add-adapter failure or a DTS/binding mismatch. A channel number is
not the logical adapter number. For nested muxes, resolve every hop.

```sh
./scripts/show_mux_tree.sh
./scripts/debug_i2c_mux.sh 5 0x70 2
```

## 4. Endpoint

Confirm the endpoint is below the correct child bus, is powered, has the right
strap address, and is not held in reset. A `UU` cell in `i2cdetect` normally
means a kernel driver owns the address; it is not an error by itself.

## Symptom matrix

| Symptom | Likely direction |
|---|---|
| No parent adapter | controller, clocks, pinctrl, DT status, kernel config |
| Mux client exists, no driver | compatible/module mismatch or probe failure |
| Driver bound, no `channel-*` | add-adapter/probe error; inspect logs |
| Every channel shows same device | wrong selector encoding or channels bridged |
| Only one channel fails | branch power, pull-ups, connector, endpoint reset |
| Parent works after reset only | idle state, stuck child, mux reset/recovery |
| Timeout under concurrency | locking error, nested mux issue, external master |
| Intermittent NACK | rise time, hot-plug, power sequencing, wrong settle time |

## Logic-analyzer method

Capture at the parent and failing branch. Look for selector address/control,
settling interval, endpoint address, ACK/NACK, repeated START, arbitration loss,
and stuck-low recovery. If the selector transaction appears but the branch
does not connect, verify the control value and hardware. If no selector appears,
trace the logical adapter and mux callbacks.

## Deadlock and lockdep

A register-controlled selector under a parent-locked mux callback must not call
a helper that tries to lock the same adapter again. Symptoms include a blocked
task with no waveform. Compare the driver with the current in-tree pca954x
implementation and enable lockdep in a debug kernel when possible.

## Safe evidence bundle

Collect the DTS/DTB version, schematic topology, kernel version/config, adapter
inventory, sysfs links, driver bindings, power/reset measurements, recent logs,
and analyzer traces. The included diagnostic script issues no I2C transaction.

