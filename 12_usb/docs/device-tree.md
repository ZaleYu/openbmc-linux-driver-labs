# Device Tree

USB-attached devices are discoverable and are normally absent from DTS. DTS
describes the SoC controller, PHY, connector routing, VBUS control, and role.
The examples enable nodes already declared by an AST2600 SoC `.dtsi`.

## AST2600 labels

Upstream/vendor trees and board revisions can differ. Verify node labels and
bindings in the exact kernel. Typical AST2600 trees expose `ehci0`, `ehci1`,
`udc`, and `vhub` labels. Enabling a label is insufficient if the board mux,
PHY, connector, or VBUS wiring selects another path.

## Host controller

See `device-tree/ast2600-usb-host.dts`. Enabling EHCI is appropriate only when
the corresponding pins reach a host connector and hardware safely supplies or
switches VBUS. Some boards use a regulator property or GPIO-controlled power
path defined by the applicable binding.

## Single-port gadget UDC

See `device-tree/ast2600-usb-gadget.dts`. A gadget needs a physical device port,
VBUS detection, and a userspace/kernel gadget configuration. `status = "okay"`
only probes the UDC; it does not automatically create Virtual Media or HID.

## Virtual hub

See `device-tree/ast2600-usb-vhub.dts`. ASPEED virtual hub hardware can expose
multiple downstream virtual devices to one upstream managed-host connection.
Endpoint budgets and platform wiring still apply. OpenBMC services must own and
coordinate UDC/gadget instances rather than racing at boot.

## Dual-role properties

For controllers that support role switching, bindings may use `dr_mode` with
`host`, `peripheral`, or `otg`, plus `usb-role-switch`, connector, extcon, or PHY
properties. Only use properties allowed by that controller's YAML schema.
AST2600 controller blocks are not interchangeable generic dual-role examples.

## Validation

Build snippets only after integrating them into the board DTS with its includes:

```sh
make ARCH=arm dtbs_check DT_SCHEMA_FILES=usb/<binding>.yaml
make ARCH=arm aspeed/<board>.dtb
```

At runtime correlate DT and driver state:

```sh
find /sys/bus/platform/drivers -maxdepth 2 -type l | grep -Ei 'usb|ehci|vhub'
ls -l /sys/class/udc
lsusb -t
```

Never copy register addresses, interrupts, clocks, or reset IDs between SoC
revisions. Include the SoC `.dtsi` and override only board-level properties.
