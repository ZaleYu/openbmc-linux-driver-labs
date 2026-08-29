# Userspace tools

## Read-only inventory first

```sh
lsusb
lsusb -t
lsusb -v -d 1234:5678
udevadm info --attribute-walk /sys/bus/usb/devices/1-2
./scripts/list_usb.sh
```

`lsusb -v` may issue control requests and often needs privileges. Avoid verbose
probing of unstable or security-sensitive production devices during incidents.
Sysfs exposes identity, negotiated speed, configuration, interfaces, power
policy, and driver links without providing a generic register map.

## usbfs and libusb

`/dev/bus/usb/BBB/DDD` is usbfs. libusb uses it for control, bulk, interrupt,
and isochronous transfers. Access is controlled by permissions/udev policy.
Claiming an interface may require detaching a kernel driver; doing that from a
production service can break storage, network, input, or management functions.

The package intentionally keeps its inventory tool libc-only. The bulk test
talks to the educational `/dev/demo_usbN` driver, not raw usbfs, so the driver
owns endpoint selection and disconnect handling.

## Gadget configfs

Configfs state is usually under `/sys/kernel/config/usb_gadget`. Creation order:

1. Create gadget and write VID/PID/device strings.
2. Create one or more configurations and configuration strings.
3. Create function instances such as `Loopback.0`.
4. Link functions into configurations.
5. Bind last by writing a name from `/sys/class/udc` to `UDC`.

Unbind first by writing an empty string to `UDC`, then unlink and remove the
configuration. The provided setup script requires explicit `--apply` or
`--remove` and refuses obvious UDC ownership conflicts.

## Useful system state

```sh
cat /sys/module/usbcore/parameters/autosuspend
cat /sys/kernel/debug/usb/devices          # debugfs, if mounted
find /sys/kernel/config/usb_gadget -maxdepth 3 -type f -print
journalctl -k -b | grep -Ei 'usb|xhci|ehci|udc|gadget'
```

`usbmon` captures kernel USB traffic (`modprobe usbmon`, then debugfs nodes),
and Wireshark can decode it. Captures may contain credentials, keyboard input,
storage contents, and other secrets; collect and share them accordingly.
