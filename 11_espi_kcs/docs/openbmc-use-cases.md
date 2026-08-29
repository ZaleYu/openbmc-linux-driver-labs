# OpenBMC use cases

## In-band host IPMI

BIOS or the host OS sends KCS requests for device ID, sensors, FRU, SEL,
watchdog, chassis control, boot options, and OEM functions. kcsbridge converts
the kernel character-device transaction into the transport used by
`phosphor-host-ipmid`, which dispatches to providers and D-Bus services.

Network IPMI is a separate front end. When a command works through LAN but not
KCS, compare channel policy, bridge health, local timeouts and KCS framing
before debugging the shared command handler.

## BIOS configuration and boot coordination

Platforms may exchange BIOS attributes or boot-control data through OEM IPMI
commands over KCS. This creates early-boot requirements: KCS decode and the BMC
services must be ready before BIOS waits for a response. Use bounded waits and
a defined fallback so a BMC failure cannot indefinitely block host boot.

## Virtual Wire power/reset signals

eSPI Virtual Wires communicate sleep, reset, boot and other platform signals.
Power-control software may use GPIO, LPC/eSPI status, or platform-specific
drivers. Treat a KCS timeout during reset as a system-state event, not
automatically as a malformed IPMI packet.

## Multi-host and multi-channel systems

Blade or multi-node designs may expose one KCS channel per host. Maintain a
stable mapping between channel, host instance, I/O address, service instance,
inventory and power state. Never route a request from host 1 to host 0's IPMI
context.

Test these failures:

- BMC service restart during a KCS request,
- host reset in WRITE and READ phases,
- duplicated or missing kcsbridge instance,
- one D-Bus provider blocked beyond the host timeout,
- maximum-length and malformed request,
- SMS_ATN assertion/clear,
- eSPI link retraining while the BMC remains running,
- concurrent network and in-band IPMI commands.

