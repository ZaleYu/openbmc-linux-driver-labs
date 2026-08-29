# Debugging PECI

## Layered decision path

1. Confirm CPU standby/main power state, reset, PECI voltage, pull-up and route.
2. Confirm pinmux, controller clock/reset, IRQ and Device Tree `status`.
3. Confirm `peci-aspeed` registered a controller and discovered target devices.
4. Confirm `peci-cpu`, `peci-cputemp`, and `peci-dimmtemp` binding.
5. Confirm hwmon names, labels, values and error behavior.
6. Confirm OpenBMC sensor service, D-Bus objects, thresholds and Redfish output.

Commands:

    dmesg | grep -Ei 'peci|cputemp|dimmtemp|hwmon|timeout|fcs'
    ls -l /sys/bus/peci/devices /sys/bus/peci/drivers
    grep . /sys/class/hwmon/hwmon*/name
    cat /proc/interrupts
    journalctl -u xyz.openbmc_project.CPUSensor.service

## Symptom map

- No controller: kernel config/module, DTS, clock/reset, MMIO or IRQ failure.
- Controller but no CPU: host power/reset, wire/pull-up, address negotiation,
  unsupported target, frequency, or bus contention.
- CPU device but no hwmon: missing `PECI_CPU`/hwmon config, unsupported model,
  auxiliary-device binding, or module ordering.
- `-ETIMEDOUT`: no response, non-idle controller, command unavailable in the
  current CPU state, IRQ loss, or timing/electrical failure.
- FCS/completion errors: signal integrity, framing, unsupported command/length,
  CPU generation mismatch, or response status.
- Stale/implausible values: label mapping, fixed-point interpretation, cache,
  CPU state transition, service scaling, or threshold configuration.

## Practical evidence

Enable dynamic debug for PECI modules, record IRQ counters, and correlate logs
with host power transitions. Use an appropriate oscilloscope or PECI-capable
analyzer for wire-level timing. Controller register dumps help only after
matching them to the exact SoC revision and preserving failure state.

Test cold boot, warm reset, S5/S0 transitions, CPU replacement, one missing
socket, repeated service restart, and simultaneous telemetry consumers.

