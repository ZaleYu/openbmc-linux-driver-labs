# Userspace Tools

## Inventory first

```sh
i2cdetect -l
find /sys/bus/i2c/devices -maxdepth 2 -type l \
    \( -name 'channel-*' -o -name mux_device \) -print
```

For a mux instantiated as `5-0070`, resolve channel 2 without assuming its bus
number:

```sh
readlink -f /sys/bus/i2c/devices/5-0070/channel-2
```

If the result ends in `i2c-12`, downstream transactions use `/dev/i2c-12`.
Opening that adapter lets the kernel select and optionally deselect the mux.
Do not open `/dev/i2c-5` and write address `0x70` yourself.

Useful read-only attributes include adapter `name`, client `name`, `modalias`,
`uevent`, `driver`, `of_node`, and the topology symlinks. The exact sysfs layout
varies by kernel, so scripts should tolerate missing optional attributes.

## i2c-tools

```sh
i2cdetect -l                       # list adapters; no device probing
i2cdetect -y 12                    # probe: potentially disruptive
i2cget -y 12 0x48 0x00 b          # device-specific register read
i2ctransfer -y 12 w1@0x48 0x00 r2 # combined write/read
```

Only run the last three commands when the endpoint datasheet permits their
exact transaction shapes. `i2cdetect` uses probe forms that some devices
interpret as commands. Never use `-f` merely to bypass a bound kernel driver.

## Included programs

`resolve_mux_channel` turns parent bus, mux address, and hardware channel into
the current logical `/dev/i2c-N`. `test_downstream_i2c` performs one combined
8-bit-register read on that logical adapter. It deliberately has no write mode.

```sh
./resolve_mux_channel 5 0x70 2
sudo ./test_downstream_i2c /dev/i2c-12 0x48 0x00 2
```

Direct userspace access is useful for controlled bring-up. Production OpenBMC
software should usually bind the endpoint's kernel driver and consume hwmon,
IIO, nvmem, GPIO, or another subsystem interface.

## Ownership and races

The mux core serializes Linux transfers, but userspace and kernel endpoint
drivers can still compete for a device's internal register pointer or state.
External masters are outside Linux locking entirely. Define a single owner or
an explicit cross-master arbitration protocol.

