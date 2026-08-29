# OpenBMC use cases

## Virtual Media

The BMC exposes an image or redirected stream as a USB mass-storage device to
the managed host for OS installation and recovery. The data path crosses remote
authentication, image handling, a backing file/block abstraction, gadget mass
storage, UDC, and host firmware/OS.

Security requirements include explicit authorization, read-only mode where
possible, path and symlink validation, image-size limits, session isolation,
audit events, cleanup on disconnect, and no exposure of arbitrary BMC storage.
Coordinate media insertion/ejection with host power and reset transitions.

## Remote KVM HID

A gadget HID keyboard and mouse carry remote-console input to the managed host.
Use fixed, reviewed report descriptors and rate limits. Release all pressed keys
when a session ends or the transport fails. Prevent unauthenticated services
from injecting reports and make ownership visible to operators.

## Composite gadget

A product may combine Virtual Media, HID, serial, or networking in one USB
configuration. Validate endpoint capacity, Windows/Linux firmware behavior,
stable serial numbers, configuration ordering, and recovery when only one
function fails. A virtual hub may instead expose distinct virtual devices.

## BMC as USB host

Possible host-side uses include service storage, cellular/modem devices, debug
adapters, security tokens, or board-specific peripherals. Minimize enabled USB
classes, constrain udev/systemd reactions, mount removable media safely, and
treat every descriptor/filesystem as untrusted.

## AST2600 virtual hub

The ASPEED virtual hub lets one upstream link present several virtual downstream
devices. It is useful for separating KVM HID and Virtual Media identities, but
requires central ownership of endpoints, device slots, and lifecycle. Multiple
services must not independently bind the same UDC.

## Failure scenarios to practice

- Managed host resets while Virtual Media is transferring.
- BMC service crashes with the UDC still bound.
- A second service attempts to claim the UDC.
- The remote image disappears mid-session.
- Cable reconnect changes enumeration timing or device number.
- Composite configuration exceeds hardware endpoint capacity.
- Host autosuspend or BMC runtime PM interrupts a long transfer.

For every scenario define the user-visible state, journal evidence, automatic
cleanup, retry limit, and manual recovery. Never identify a physical path by a
volatile usbfs device number alone; use topology and stable product policy.
