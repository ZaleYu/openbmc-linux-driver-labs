# Linux USB architecture

## Host path

An xHCI/EHCI/OHCI host-controller driver (HCD) registers buses with usbcore.
The hub driver discovers devices, usbcore builds device/interface objects, and
an interface driver binds using a `struct usb_device_id` table.

```text
USB port -> host controller/HCD -> usbcore + hub -> usb_interface
         -> class/vendor driver -> subsystem or /dev node -> application
```

Common host objects and APIs include `struct usb_device`, `struct
usb_interface`, `struct urb`, `usb_register()`, `usb_submit_urb()`,
`usb_control_msg()`, and `usb_bulk_msg()`. Synchronous message helpers are good
for simple serialized operations; streaming/high-rate drivers normally manage
multiple URBs and cancellation.

The demo driver scans the selected alternate setting for one bulk-IN and one
bulk-OUT endpoint, registers `/dev/demo_usbN`, serializes I/O, and keeps a
reference until all open file descriptors close after disconnect. It is a lab,
not a template for unrestricted production USB passthrough.

## Gadget path

A USB Device Controller (UDC) driver exposes hardware endpoints to the gadget
core. A gadget driver or libcomposite assembles one or more functions. Configfs
lets userspace create descriptors, configurations, function instances, and
links; writing a UDC name binds the completed gadget.

```text
OpenBMC service/backing store -> gadget function -> composite core
                              -> UDC driver -> device port -> managed host
```

Standard functions include mass storage, HID, ECM/NCM networking, serial, and
FunctionFS. Endpoint availability is finite; a composite layout may fail to
bind even if every individual function works alone.

## Binding boundaries

USB drivers usually bind to interfaces, not the whole device. Inspect
`/sys/bus/usb/devices/<bus>-<port>:<config>.<interface>/driver`. A device may
therefore have a storage driver on one interface and HID on another.

Device Tree normally describes non-discoverable controller hardware, PHYs,
role wiring, and status. It does not describe ordinary enumerated USB devices.
Platform firmware properties and connector/role-switch drivers may supplement
DT on dual-role platforms.

## Where source lives

| Area | Typical Linux path |
|---|---|
| Core, hub, sysfs | `drivers/usb/core/` |
| HCDs | `drivers/usb/host/` |
| UDC drivers | `drivers/usb/gadget/udc/` |
| Gadget functions/configfs | `drivers/usb/gadget/function/`, `.../configfs.c` |
| Class drivers | `drivers/usb/class/`, `drivers/usb/storage/` |
| Serial/network/HID | `drivers/usb/serial/`, `drivers/net/usb/`, `drivers/hid/` |
| API definitions | `include/linux/usb.h`, `include/linux/usb/gadget.h` |
| DT bindings | `Documentation/devicetree/bindings/usb/` |

## What usually changes

- New board: DTS status/PHY/role/connector/power wiring and kernel config.
- New USB peripheral: reuse a class driver or add an interface driver.
- New gadget product: configfs policy/service and authorized backing resources.
- Controller erratum: HCD/UDC quirks, reset, DMA, endpoint, or suspend paths.
- OpenBMC integration: Yocto packages, systemd ordering, D-Bus authorization,
  and state coordination with the managed host.

Do not modify an HCD merely because a board has a new USB device. First prove
the failure is below usbcore using port state, enumeration logs, and captures.
