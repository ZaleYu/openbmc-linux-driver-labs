# I3C Protocol Fundamentals

## Why I3C exists

I3C retains a two-wire SDA/SCL topology and limited legacy I2C coexistence,
while adding dynamic addressing, higher-rate SDR/HDR transfers, standardized
discovery, In-Band Interrupts, Hot-Join, and controller-role handoff. The bus
uses open-drain phases where arbitration or legacy compatibility requires it
and push-pull phases where higher speed is safe.

Do not assume any arbitrary I2C target can share an I3C bus. Clock stretching,
spike filters, static-address conflicts, electrical limits, and bus mode must
be checked against the controller, target, and I3C specification.

## Address and identity

An I3C target exposes a 48-bit Provisioned ID (PID), an 8-bit Bus
Characteristic Register (BCR), and an 8-bit Device Characteristic Register
(DCR). The PID normally contains manufacturer, part, instance, and extra
information fields. Linux matches an I3C target driver primarily from this
discovered identity, not from a Device Tree `compatible` string.

A target may have a static address before bus initialization, but normal I3C
communication uses a dynamic address assigned by the active controller. The
address may change after reset, rediscovery, Hot-Join, or controller changes.

## Dynamic Address Assignment

During ENTDAA, eligible targets arbitrate by transmitting their PID, BCR, and
DCR. The controller assigns a free dynamic address to each discovered target.
Targets with a known static address may instead receive SETDASA. A Device Tree
`assigned-address` is a preference/initial assignment request, not a physical
identity that applications should hard-code.

An oversimplified ENTDAA sequence is:

```text
Controller: broadcast ENTDAA
Targets:    arbitrate and return PID + BCR + DCR
Controller: assigns dynamic address and parity
Target:     ACKs; next unaddressed target participates
```

## Common Command Codes

CCCs are standardized bus-management commands. Some are broadcast and some are
directed. Important groups include:

- address management: `ENTDAA`, `RSTDAA`, `SETDASA`, `SETNEWDA`;
- event control: `ENEC`, `DISEC`;
- identity/capability reads: `GETPID`, `GETBCR`, `GETDCR`, `GETCAPS`;
- transfer limits: `GETMRL`, `GETMWL`, `SETMRL`, `SETMWL`;
- multi-master information: `DEFSLVS` and controller-role requests;
- reset/recovery commands supported by later specification versions.

CCCs alter shared bus state. They belong in the I3C core/master driver or a
well-reviewed device driver, not arbitrary shell scripts.

## Private SDR transfers

After discovery, a target driver sends device-specific private transfers to
the target's dynamic address. The demo sensor performs a one-byte register
selector write followed by a data read. This resembles an I2C register access,
but it travels through the I3C core/controller API and may use push-pull data
phases. The target's documented maximum read/write length still applies.

## In-Band Interrupts

IBI lets a target request service on SDA/SCL without a separate GPIO. The
controller arbitrates and delivers an optional payload to the target driver's
registered handler. Lower dynamic addresses have higher IBI arbitration
priority. A driver must pre-allocate enough IBI slots, keep handling bounded,
and tolerate loss/overflow according to system policy.

IBI does not eliminate all interrupt-design work: controller queue depth,
payload format, latency, suspend policy, storm control, and recovery must be
validated.

## Hot-Join and multiple controllers

Hot-Join allows a target that was absent or unpowered during initialization to
request discovery later. The active controller schedules DAA and registers the
new device. Multiple controller-capable devices may share a bus, but only the
current controller drives normal transactions. Role handoff is protocol state,
not two masters independently issuing transfers as on a loosely coordinated
I2C multi-master bus.

## Electrical checklist

- Confirm voltage, pull-ups, bus capacitance, trace/connector topology, and
  target power-off leakage.
- Validate open-drain and push-pull waveforms with an I3C-capable analyzer.
- Check mixed-bus I2C spike-filter and speed limitations.
- Confirm all static/dynamic address reservations and broadcast conflicts.
- Test reset, power sequencing, Hot-Join, IBI bursts, and stuck SDA/SCL.

