# Linux PECI source map

| Path | Study focus |
|---|---|
| `drivers/peci/core.c` | bus/controller registration and address scan |
| `drivers/peci/device.c` | target identification and device lifecycle |
| `drivers/peci/request.c` | command construction, completion and FCS |
| `drivers/peci/cpu.c` | CPU helpers and auxiliary-device creation |
| `drivers/peci/controller/peci-aspeed.c` | AST controller registers, IRQ, timing, recovery |
| `drivers/peci/controller/peci-npcm.c` | Nuvoton controller comparison |
| `drivers/hwmon/peci/cputemp.c` | package/core conversion and hwmon channels |
| `drivers/hwmon/peci/dimmtemp.c` | generation-specific DIMM telemetry |
| `include/linux/peci.h` | controller, device and request structures |
| `include/linux/peci-cpu.h` | CPU command helper API and PCS indexes |

## Trace one read

1. hwmon `read()` decides whether cached data needs refresh.
2. cputemp/dimmtemp calls a `peci-cpu` helper.
3. `request.c` builds the generation-defined command and checks status.
4. PECI core serializes the controller bus.
5. `peci-aspeed` programs address/length/data, fires the command, waits for IRQ,
   checks timeout/FCS status, and copies response bytes.
6. hwmon converts the raw fixed-point value to millidegrees Celsius.

## Typical change locations

- Board bring-up: DTS and kernel config.
- New ASPEED/NPCM revision or erratum: controller driver and binding.
- New Intel CPU model: PECI CPU ID table and generation data.
- New sensor/register layout: hwmon functional driver.
- OpenBMC naming/thresholds: Entity Manager and sensor service configuration.

