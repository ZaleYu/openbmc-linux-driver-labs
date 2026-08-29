# Demo USB bulk host driver

This driver binds `ffff:ffff`, finds a bulk-IN and bulk-OUT endpoint, and exposes
`/dev/demo_usbN`. It is designed to pair with `setup_loopback_gadget.sh` on a
second USB-device-capable system.

The driver demonstrates interface probing, descriptor-based endpoint discovery,
minor registration, synchronous bulk messages, disconnect-safe lifetime through
`kref`, and serialized I/O. Production streaming drivers should normally use
asynchronous URBs, anchors, power management, and a real protocol.

Build against the exact target kernel:

```sh
make
sudo insmod demo_usb_bulk.ko
dmesg | tail
sudo ../usb_bulk_test /dev/demo_usb0 512
sudo rmmod demo_usb_bulk
```

Do not use `ffff:ffff` in a shipped product. Obtain/assign a legitimate identity,
match the correct interface, validate descriptors, define a versioned protocol,
handle suspend/reset, bound resource use, and add security review and tests.
