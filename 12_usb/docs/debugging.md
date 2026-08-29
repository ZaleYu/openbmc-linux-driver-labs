# Debugging USB

## Start with role and topology

Before changing software, identify the physical connector, active host/device
role, VBUS source, controller instance, PHY, intervening hub or mux, expected
speed, and cable. A device-to-device or host-to-host connection cannot enumerate
merely because the plugs fit.

## Layered workflow

1. **Electrical:** verify VBUS, ground, cable, port routing, power switch, PHY
   clock/reset, and signal integrity.
2. **Controller:** confirm HCD or UDC probe, interrupts, clocks, resets, runtime
   power, and the correct DT node.
3. **Enumeration:** watch connect/reset/address/configuration messages; inspect
   descriptors and repeated error codes.
4. **Binding:** identify the interface, modalias, selected driver, alternate
   setting, and endpoint descriptors.
5. **Transfer:** capture usbmon/protocol traffic; correlate URB status, lengths,
   short packets, stalls, timeouts, and disconnects.
6. **OpenBMC service:** inspect systemd/D-Bus ownership, backing media, host
   power state, authorization, and cleanup after failure.

Run `scripts/debug_usb.sh` for a read-mostly snapshot.

## Host checks

```sh
lsusb -t
readlink /sys/bus/usb/devices/1-2:1.0/driver
cat /sys/bus/usb/devices/1-2/power/control
cat /proc/interrupts | grep -Ei 'usb|xhci|ehci'
```

If a driver does not bind, compare the interface descriptors with its ID table
and verify its module/config. Avoid forcing `new_id` unless you have audited the
driver for that exact protocol; matching does not make incompatible hardware
safe.

## Gadget checks

```sh
ls -l /sys/class/udc
find /sys/kernel/config/usb_gadget -maxdepth 4 -print
cat /sys/kernel/config/usb_gadget/*/UDC 2>/dev/null
cat /sys/class/udc/*/state 2>/dev/null
```

No UDC often means disabled DT, missing driver, wrong controller/port, or reset/
clock failure. `not attached` after binding points toward VBUS, cable, routing,
or the managed host. Bind failures may mean insufficient endpoints or a UDC
already owned by another gadget.

## Dynamic debug and usbmon

Enable only targeted call sites and revert after collecting data:

```sh
mount -t debugfs none /sys/kernel/debug   # only if policy permits
modprobe usbmon
cat /sys/kernel/debug/usb/usbmon/0u
```

For dynamic debug, select exact USB source files through the control file rather
than enabling every USB message. Heavy logging changes timing and can flood a
BMC journal.

## Recovery criteria

A fix is incomplete unless transfers work after cable reconnect, managed-host
reset, BMC service restart, gadget unbind/rebind, suspend/resume, and an injected
transfer error. Confirm resources and UDC ownership are released on every path.
