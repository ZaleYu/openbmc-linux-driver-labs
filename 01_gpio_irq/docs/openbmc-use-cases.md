# OpenBMC GPIO and IRQ Use Cases

GPIOs connect board state to management policy. Common examples include power
and reset buttons, power-good, POST complete, host reset, chassis intrusion,
identify button, fan/PSU/card presence, fault alerts, write protect, boot straps,
reset outputs, power enables, and mux selects.

## Case 1: Front-panel buttons

A power button is not simply a falling edge. Production behavior can include:

- Press and release events.
- Debounce.
- Short- versus long-press timing.
- Host power-state-dependent actions.
- Button masking during update or transition.
- Multi-host routing.
- D-Bus events and chassis policy.

Use the standard `gpio-keys` input path or the platform's OpenBMC button service
when it matches. OpenBMC `phosphor-buttons` exposes button/switch interfaces and
coordinates button events with policy handlers.

## Case 2: Presence detection

A PSU, fan, DIMM, drive, or PCIe card may assert a presence line. The OpenBMC
path can be:

```text
Presence pin -> libgpiod monitor -> inventory Present property
             -> bind/unbind related drivers -> health and Redfish inventory
```

Presence changes require debounce and ordering. On insertion, power and bus
isolation may need to stabilize before probing I2C/PCIe devices. On removal,
userspace must stop accessing the device before tearing down related drivers.

## Case 3: Host power signals

Signals such as power-good, POST complete, reset status, sleep states, and power
button output form a state machine. Do not handle them as independent shell
commands. Define:

- Expected levels in every host power state.
- Transition timeout and retry policy.
- What constitutes an unexpected power loss.
- Warm versus cold reset detection.
- Ordering of enables, resets, and power-good confirmation.
- Recovery after BMC or host restart mid-transition.

Projects such as OpenBMC `x86-power-control` can monitor named GPIO lines or
D-Bus properties and coordinate host power behavior.

## Case 4: Fault and alert inputs

Fault lines may be latched or level-held until a status register is read or a
component is reset. The IRQ handler should capture the event with minimal work;
a threaded handler or userspace service can then read detailed I2C/PMBus/CPLD
status and update health.

Avoid logging only “GPIO fault.” Include the line name, physical component,
asserted state, timestamp, related register evidence, and recovery result.

## Case 5: GPIO expanders

BMC boards often need more lines than the SoC provides. I2C/SPI expanders add:

- Sleepable access and bus failures.
- Shared parent interrupt demultiplexing.
- Longer event latency.
- Dependency on mux selection and expander power.
- Different availability during boot and recovery.

A hard IRQ handler must not perform an I2C transaction. Use nested/threaded IRQ
support and `*_cansleep()` consumer accessors.

## Case 6: SGPIO

Serial GPIO can carry many server backplane signals through a small pin count.
It is used for drive presence/activity/fault and other platform signals. Debug
requires separating the BMC SGPIO controller, serial frame/clock, backplane
mapping, and logical line ownership. Treat it as a GPIO provider with additional
transport timing and mapping concerns.

## Case 7: Multi-host systems

One BMC may manage several hosts. Grouped selector inputs and output mux controls
must be sampled atomically enough for the hardware encoding, validated against
allowed combinations, and associated with the correct host D-Bus namespace.
Transitions between selector bits can temporarily form invalid values; debounce
and stable-state confirmation are often required.

## Kernel driver versus OpenBMC daemon

| Requirement | Likely owner |
|---|---|
| Standard button/input semantics | `gpio-keys` and input subsystem |
| LED output | GPIO LED subsystem / phosphor-led-manager |
| Regulator/power enable | regulator or device-specific kernel driver |
| Simple presence-to-inventory policy | libgpiod/OpenBMC presence daemon |
| Tight hardware sequencing | kernel driver or dedicated power-control service |
| Fault with register follow-up | threaded kernel IRQ plus userspace policy, or daemon |

## Development workflow

1. Convert schematic GPIOs into a line/polarity/direction/power-state table.
2. Verify bootloader state, pinctrl, pulls, and safe defaults.
3. Add line names and consumer mappings in Device Tree.
4. Inspect ownership with `gpioinfo` before requesting anything.
5. Validate static levels and transitions with instruments.
6. Select a standard kernel subsystem or OpenBMC daemon where possible.
7. Add a custom consumer driver only when semantics require it.
8. Verify IRQ counts, debounce, event timestamps, and lost-event behavior.
9. Map to D-Bus inventory/state/health and systemd actions.
10. Test BMC reset, host reset, hot insertion/removal, noise, stuck level, and storms.

## Production review checklist

- Pinmux and direction cannot electrically conflict with another owner.
- Active-low is defined once and tested in both asserted states.
- Safe output state exists before Linux probe and after driver removal.
- Interrupt trigger matches hardware clearing behavior.
- Bounce/noise and maximum event rate are characterized.
- Critical events survive service restart or have state reconciliation.
- Presence changes order device probing and removal safely.
- Line names and D-Bus associations identify the correct physical component.
- Debug logs and counters are rate-limited and actionable.
- Kernel, DTS, OpenBMC configuration, and Yocto revisions are reproducible.

