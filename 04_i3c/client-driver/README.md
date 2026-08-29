# Demo I3C Sensor Driver

This is a study-only hwmon driver for a fictional I3C temperature target with
manufacturer ID `0x0123`, part ID `0x0456`, device ID `0xA5`, and a two-byte
maximum IBI payload.

## What to study

- `I3C_DEVICE()` matches manufacturer and part fields discovered from PID.
- `i3c_device_get_info()` reports PID/BCR/DCR and the current dynamic address.
- two `i3c_priv_xfer` objects implement register-select plus read.
- allocated driver buffers satisfy the transfer buffer requirements and are
  serialized by a mutex.
- the IBI lifecycle is request, enable, disable, then free.
- hwmon exposes `temp1_input`, `temp1_max`, and `temp1_alarm`.
- diagnostic `ibi_count` and `last_ibi_status` attributes show event handling.

The IBI handler only updates atomics and emits a rate-limited alarm. Real code
must define payload parsing, lost-event behavior, queue depth, work scheduling,
and storm control from the datasheet and system requirements.

## Kernel version

This source targets Linux 6.18 and uses:

```c
struct i3c_priv_xfer;
i3c_device_do_priv_xfers();
```

Linux 6.19 introduces `struct i3c_xfer` and
`i3c_device_do_xfers(..., I3C_SDR)`. Compile against the exact target headers
and port the transfer definitions when using that API. Do not copy internal
kernel structures or silence type errors with casts.

## Build and inspect

```sh
make
sudo insmod demo_i3c_sensor.ko
dmesg | tail -n 50
grep -H . /sys/class/hwmon/hwmon*/name
grep -H . /sys/bus/i3c/devices/*/{pid,bcr,dcr,dynamic_address,ibi_count} 2>/dev/null
```

An out-of-tree target driver binds only if a real/emulated device returns the
matching PID and register behavior. Device Tree cannot manufacture that
identity.

## Adaptation checklist

1. Replace PID matching and register/IBI formats with the datasheet.
2. Validate transfer lengths, `actual_len`, retry rules, and I3C Mx errors.
3. Confirm the controller supports the required IBI payload and slot count.
4. Add reset, regulators, clocks, PM, and board resources when required.
5. Decide whether IBI failure is fatal or polling fallback is acceptable.
6. Test DAA, reattach, Hot-Join, suspend/resume, unbind, and controller reset.
7. Add a YAML binding only for real board resources that firmware must describe.
8. Prefer upstream integration over a permanent vendor-only clone.

