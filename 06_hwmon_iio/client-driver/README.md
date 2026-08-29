# Demo BMC ADC

This educational platform driver maps four fictional 32-bit registers:

| Offset | Channel | Format |
|---:|---:|---|
| `0x00` | 0 | 12-bit VCORE ADC code |
| `0x04` | 1 | 12-bit VDDIO ADC code |
| `0x08` | 2 | signed temperature in centi-degrees C |
| `0x0c` | 3 | 12-bit auxiliary ADC code |

Voltage scale is 1800/4096 mV per code. Temperature is returned as processed
milli-degrees C. The driver implements direct reads only. Real hardware may
need conversion start/polling, IRQ, FIFO, runtime PM, clocks, reset and
calibration.

Build with `make`, enable the provider DT node and load the module. Add
`iio-hwmon` to expose selected channels below `/sys/class/hwmon`. Never map this
fictional register layout onto unknown hardware.

