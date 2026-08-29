# Measurement fundamentals

For an ideal unsigned N-bit ADC:

    voltage = raw_code * reference_voltage / 2^N

Real systems also need reference tolerance, gain/offset calibration, input
settling, source impedance, quantization, noise, averaging and saturation
handling. A resistor divider must be undone before reporting the rail voltage.
Thermistors require a nonlinear resistance-to-temperature conversion.

The fictional BMC ADC uses a 1.8 V reference and 12-bit samples:

| Channel | Type | Meaning |
|---:|---|---|
| 0 | voltage | VCORE sense |
| 1 | voltage | VDDIO sense |
| 2 | temperature | die temperature in centi-degrees C |
| 3 | voltage | auxiliary input |

Voltage channels expose `raw` and `scale`. Temperature exposes `processed` in
milli-degrees C. The demo masks reserved ADC bits and sign-extends temperature.

OpenBMC health monitoring normally needs stable, low-rate readings with
thresholds, hysteresis, timeout detection and fail-safe behavior. Faster
sampling does not automatically improve accuracy.

