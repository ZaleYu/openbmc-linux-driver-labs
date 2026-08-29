# Linux USB for OpenBMC

This study package follows USB from wire protocol and enumeration through the
Linux host and gadget stacks to practical OpenBMC virtual-media and KVM use
cases. It deliberately covers both roles: a BMC may consume USB devices as a
host and emulate devices toward a managed host as a gadget.

```text
Host role:   device -> HCD -> USB core -> interface driver -> userspace/service
Gadget role: host PC -> UDC -> composite/configfs -> function -> backing data
```

The lab pairs a Linux configfs `Loopback` gadget with an educational host-side
bulk driver using lab-only VID:PID `ffff:ffff`. Never ship that identity; a
product needs an assigned VID/PID and appropriately narrow interface matching.

## Repository map

| Path | Purpose |
|---|---|
| `docs/protocol.md` | Topology, descriptors, enumeration, transfers, and power |
| `docs/linux-usb-architecture.md` | Host, gadget, HCD, UDC, driver model, sources |
| `docs/device-tree.md` | AST2600 host, UDC, and virtual-hub board fragments |
| `docs/userspace-tools.md` | sysfs, usbfs, libusb, configfs, and common tools |
| `docs/debugging.md` | Layered enumeration, transfer, gadget, and signal debug |
| `docs/openbmc-use-cases.md` | Virtual media, KVM HID, composite, and host cases |
| `client-driver/` | Educational USB bulk host interface driver |
| `device-tree/` | Board-level AST2600 USB examples |
| `userspace/` | Portable inventory and bulk-loopback tests |
| `scripts/` | Inventory, diagnostics, and explicit gadget setup |

## Loopback lab

On a USB-device-capable target, review then create the gadget:

```sh
sudo ./scripts/setup_loopback_gadget.sh --apply [udc-name]
```

Connect its device port to a separate Linux host. On that host:

```sh
make -C client-driver
sudo insmod client-driver/demo_usb_bulk.ko
cc -O2 -Wall -Wextra -Werror -o usb_bulk_test userspace/usb_bulk_test.c
sudo ./usb_bulk_test /dev/demo_usb0 512
```

Remove the lab gadget explicitly:

```sh
sudo ./scripts/setup_loopback_gadget.sh --remove
```

The two roles normally run on different machines or USB controllers. Do not
bind a UDC already owned by Virtual Media, KVM, or another production gadget.

## Safety and scope

- A USB port cannot be inferred to be host or device merely from its connector.
- Never expose arbitrary BMC files or block devices through mass storage.
- Treat USB descriptors and requests as untrusted input on both host and gadget.
- Do not unbind production HCD/UDC drivers or change gadget configurations on a
  live managed system without an approved recovery path.
- The DTS snippets are board fragments: verify labels, PHYs, clocks, resets,
  pin routing, and role selection in the exact vendor kernel and schematic.

## Recommended study order

1. Read protocol and enumeration, then inspect a known device with `lsusb -v`.
2. Trace host binding from sysfs interface to `usb_driver.probe()`.
3. Trace gadget binding from configfs function through UDC endpoint allocation.
4. Run the loopback lab and capture bind, bulk OUT, and bulk IN behavior.
5. Map Virtual Media and HID functions to OpenBMC services and security policy.
6. Practice failures: missing VBUS, bad cable, no UDC, endpoint mismatch, reset.

## References

- Linux USB API: <https://docs.kernel.org/driver-api/usb/usb.html>
- USB gadget configfs: <https://docs.kernel.org/usb/gadget_configfs.html>
- Gadget testing: <https://docs.kernel.org/usb/gadget-testing.html>
- USB sysfs ABI: `Documentation/ABI/stable/sysfs-bus-usb`
- Kernel sources: `drivers/usb/`, `include/linux/usb/`
