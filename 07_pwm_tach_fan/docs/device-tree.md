# PWM Fan Device Tree

The Device Tree describes the hardware connections among a PWM controller, a fan, and its tachometer feedback signal.

```text
PWM Controller
      | PWM Output
      v
External Transistor / Fan Driver
      |
      v
PWM Fan
      | Tachometer Pulses
      v
GPIO / Interrupt Controller
      |
      v
Linux pwm-fan Driver
      +--> Hwmon: PWM and RPM
      +--> Thermal Cooling Device: Cooling State
```

The upstream Linux `pwm-fan` driver can control one PWM output, adjust fan speed, calculate RPM from tach interrupts, expose hwmon interfaces, register a thermal cooling device, and switch cooling states according to thermal policy.

---

# 1. PWM Provider and Consumer

| Role | Description |
| --- | --- |
| PWM Provider | Controller that provides hardware PWM channels |
| PWM Consumer | Device that uses PWM, such as a fan, LED, or backlight |

```text
PWM Controller pwm0
        | Channel 2
        v
PWM Fan fan0
```

A consumer references its provider through `pwms`:

```dts
pwms = <&pwm0 2 40000 0>;
```

This means that `fan0` uses Channel 2 of `pwm0`. Unlike an SPI peripheral, a PWM consumer does not have to be a child of the PWM controller; it can be elsewhere in the tree and connect through the `&pwm0` phandle.

---

# 2. The `pwms` Property

A common PWM specifier is:

```dts
pwms = <&pwm-controller channel period flags>;
```

Example:

```dts
pwms = <&pwm0 2 40000 0>;
```

| Field | Value | Meaning |
| --- | --- | --- |
| PWM Provider | `&pwm0` | Use controller `pwm0` |
| Channel | `2` | Use PWM Channel 2 |
| Period | `40000` | Period of 40,000 ns |
| Flags | `0` | Normal polarity |

The number and meaning of cells are defined by the provider’s `#pwm-cells` and binding; not every controller uses exactly the same format.

---

# 3. Provider `#pwm-cells`

```dts
pwm0: pwm@12340000 {
    compatible = "vendor,soc-pwm";
    reg = <0x12340000 0x1000>;
    #pwm-cells = <3>;
    clocks = <&clock_controller 10>;
    resets = <&reset_controller 5>;
    status = "okay";
};
```

`#pwm-cells = <3>` means that three cells follow the phandle: Channel, Period, and Flags.

```text
&pwm0  --> Provider phandle, not one of the three cells
2      --> Cell 0: Channel
40000  --> Cell 1: Period
0      --> Cell 2: Flags
```

Controllers may differ in cell count, channel encoding, clock source, prescaler, polarity support, and minimum/maximum period. Consult the target SoC’s PWM-controller YAML binding.

---

# 4. Channel

In:

```dts
pwms = <&pwm0 2 40000 0>;
```

`2` is hardware PWM Channel 2. It is not a dynamic Linux number, GPIO number, fan index, cooling state, hwmon `pwm2`, or physical connector number.

Confirm it from the schematic, SoC pinmux, controller datasheet, and Device Tree binding. Only if the schematic shows `PWM2_OUT --> FAN0_PWM` should Channel 2 be used.

---

# 5. Period and Frequency

The period is normally expressed in nanoseconds:

```dts
pwms = <&pwm0 2 40000 0>;
```

```text
Frequency = 1 / Period
Frequency = 1,000,000,000 / 40,000
          = 25,000 Hz = 25 kHz
```

| Frequency | Period |
| --- | --- |
| 1 kHz | 1,000,000 ns |
| 10 kHz | 100,000 ns |
| 20 kHz | 50,000 ns |
| 25 kHz | 40,000 ns |
| 50 kHz | 20,000 ns |

```text
Period(ns) = 1,000,000,000 / Frequency(Hz)
```

The actual frequency may differ slightly due to the parent clock, divider, and hardware resolution.

---

# 6. Why Do Fans Often Use 25 kHz?

Many four-wire PWM fans use a control signal near 25 kHz, but the actual requirement comes from the fan specification.

```text
Pin 1: Ground
Pin 2: Power
Pin 3: Tachometer Output
Pin 4: PWM Control Input
```

Verify the allowed input-frequency range and voltage, active level, open-drain requirement, behavior at 0% duty, minimum startup duty, and stall-recovery behavior. Do not assume that every fan uses a 40,000 ns period.

---

# 7. PWM Polarity

With:

```dts
#include <dt-bindings/pwm/pwm.h>
```

use:

```dts
PWM_POLARITY_NORMAL
PWM_POLARITY_INVERTED
```

```dts
pwms = <&pwm0 2 40000 PWM_POLARITY_NORMAL>;
pwms = <&pwm0 2 40000 PWM_POLARITY_INVERTED>;
```

`0` is also commonly used for normal polarity. Normal polarity normally means active time is high; inverted polarity means active time is low.

---

# 8. Why Check the External Transistor?

The SoC PWM output may pass through an NPN transistor, NMOS, open-drain buffer, level shifter, fan-driver IC, or inverting gate.

```text
SoC PWM High --> NPN Conducts --> Fan PWM Input Pulled Low
```

The external circuit may therefore invert the signal. Incorrect polarity can make 100% stop the fan, 0% run it at full speed, higher cooling states reduce RPM, or the fan briefly stop during boot.

Verify the controller output, external inversion, fan input specification, and Linux PWM binding together.

---

# 9. Basic `pwm-fan` Example

```dts
#include <dt-bindings/interrupt-controller/irq.h>
#include <dt-bindings/pwm/pwm.h>

fan0: pwm-fan {
    compatible = "pwm-fan";
    pwms = <&pwm0 2 40000 PWM_POLARITY_NORMAL>;
    interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
    pulses-per-revolution = <2>;
    cooling-levels = <80 120 170 220 255>;
    #cooling-cells = <2>;
};
```

This describes the upstream `pwm-fan` driver, `pwm0` Channel 2, 40,000 ns/25 kHz normal-polarity PWM, Tach IRQ 17 on the falling edge, two pulses per revolution, and five cooling states.

---

# 10. `compatible = "pwm-fan"`

Linux uses this property to match the upstream `pwm-fan` driver, which integrates with the PWM, hwmon, thermal-cooling, and tach-interrupt frameworks.

Possible hwmon files include:

```text
/sys/class/hwmon/hwmonX/
├── name
├── pwm1
├── pwm1_enable
└── fan1_input
```

The exact attributes depend on kernel version, binding, tach IRQ availability, and driver implementation.

---

# 11. Upstream `pwm-fan` Supports One PWM per Device

A `pwms` property normally describes one PWM output. Four independently controlled fans generally require four nodes:

```dts
fan0: pwm-fan-0 { compatible = "pwm-fan"; pwms = <&pwm0 0 40000 0>; };
fan1: pwm-fan-1 { compatible = "pwm-fan"; pwms = <&pwm0 1 40000 0>; };
fan2: pwm-fan-2 { compatible = "pwm-fan"; pwms = <&pwm0 2 40000 0>; };
fan3: pwm-fan-3 { compatible = "pwm-fan"; pwms = <&pwm0 3 40000 0>; };
```

Validate node names, required properties, and whether cooling properties may be omitted against the kernel’s current `pwm-fan.yaml`.

---

# 12. Tachometer

A tachometer reports fan speed as pulses:

```text
Fan Rotation
      |
      v
Fixed Number of Pulses per Revolution
      |
      v
GPIO / Interrupt Controller Counts Edges
      |
      v
Linux Converts Pulses per Second to RPM
```

RPM means Revolutions Per Minute.

---

# 13. `interrupts`

```dts
interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
```

This means that the tach signal uses interrupt 17 and triggers on falling edges. The cell format depends on the interrupt controller and may require:

```dts
interrupt-parent = <&gpio0>;
interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
```

Always consult the GPIO/interrupt-controller binding.

---

# 14. Selecting a Tach Edge

A periodic tach signal can be counted on rising or falling edges. Confirm the signal’s electrical type, pull-up, controller edge support, debounce, ringing, whether the driver counts one or both edges, and how `pulses-per-revolution` is defined.

Counting both rising and falling edges may double the measured RPM.

---

# 15. Tach Electrical Considerations

Many fan tach outputs are open-collector or open-drain and require a pull-up resistor. Verify pull-up presence, voltage, resistance, unpowered-fan behavior, level shifting, GPIO-input pinmux, interrupt delivery, and noise or duplicate edges.

Without a pull-up, `fan1_input` may remain zero and interrupts may never occur.

---

# 16. `pulses-per-revolution`

```dts
pulses-per-revolution = <2>;
```

```text
RPM = Pulses per Second × 60 / Pulses per Revolution
```

At 100 pulses per second and PPR = 2:

```text
RPM = 100 × 60 / 2 = 3000 RPM
```

Setting PPR to 1 would report 6000 RPM; setting it to 4 would report half the actual speed. Use the fan datasheet rather than copying the common value 2.

---

# 17. `cooling-levels`

```dts
cooling-levels = <80 120 170 220 255>;
```

| Cooling State | PWM Value | Approximate Duty Cycle |
| --- | --- | --- |
| 0 | 80 | 31.4% |
| 1 | 120 | 47.1% |
| 2 | 170 | 66.7% |
| 3 | 220 | 86.3% |
| 4 | 255 | 100% |

```text
Duty Cycle = PWM Value / 255 × 100%
```

A cooling state is neither a temperature nor a guaranteed RPM. Actual RPM also depends on the fan model, supply voltage, airflow resistance, temperature, aging, minimum speed, startup conditions, and the fan’s PWM transfer curve.

---

# 18. Why State 0 Is Not Necessarily PWM 0

With `<80 120 170 220 255>`, State 0 still runs at about 31% duty. This may be necessary because the fan cannot run reliably at lower duty, the server requires minimum airflow, repeated stop/start must be avoided, or fan-stop mode violates reliability requirements.

To allow stopping, a design might use:

```dts
cooling-levels = <0 80 120 170 220 255>;
```

Thermal, mechanical, hardware, and reliability teams must confirm whether stopping is safe. Server/BMC systems also need fail-safe behavior, such as full speed when a sensor, tach signal, or thermal service fails.

---

# 19. `#cooling-cells = <2>`

```dts
#cooling-cells = <2>;
```

Two cells follow the cooling-device phandle: minimum and maximum cooling state.

```dts
cooling-device = <&fan0 1 4>;
```

| Field | Value | Meaning |
| --- | --- | --- |
| Cooling Device | `&fan0` | Use `fan0` |
| Minimum State | `1` | Lowest allowed state is 1 |
| Maximum State | `4` | Highest allowed state is 4 |

With `<80 120 170 220 255>`, State 1 maps to PWM 120 and State 4 to PWM 255; this mapping does not permit State 0.

---

# 20. Thermal Trip and Cooling Map

```dts
thermal-zones {
    cpu-thermal {
        polling-delay = <1000>;
        polling-delay-passive = <250>;
        thermal-sensors = <&cpu_temp>;

        trips {
            cpu_hot: cpu-hot {
                temperature = <80000>;
                hysteresis = <5000>;
                type = "active";
            };
        };

        cooling-maps {
            map0 {
                trip = <&cpu_hot>;
                cooling-device = <&fan0 1 4>;
            };
        };
    };
};
```

Temperatures are normally in millidegrees Celsius: 80000 = 80°C and 5000 = 5°C.

```text
CPU Reaches 80°C Trip
      |
      v
Thermal Framework Activates fan0
      |
      v
Governor Selects States 1–4
      |
      v
PWM Duty and Fan RPM Increase
```

---

# 21. Hysteresis

```dts
temperature = <80000>;
hysteresis = <5000>;
```

The trip activates near 80°C and clears near 75°C. Without hysteresis, temperature fluctuations around the threshold can repeatedly toggle fan states, causing speed changes, noise, mechanical wear, excessive PWM/tach events, and apparent instability.

---

# 22. Role of the Thermal Governor

A cooling map specifies which trip controls which cooling device and the allowed state range. The thermal governor decides when to move among those states based on current and target temperature, trip type, temperature trend, algorithm, and state range.

`cooling-device = <&fan0 1 4>` does not force State 4 immediately at 80°C; it permits the thermal framework to control the fan within States 1–4 according to kernel policy.

---

# 23. Cooling-State-to-PWM Flow

```text
Temperature Sensor
        |
        v
Thermal Zone --> Trip --> Cooling Map
        |
        v
pwm-fan Cooling Device
        |
        v
Cooling State
        |
        v
cooling-levels[state]
        |
        v
PWM Value --> Duty Cycle --> Fan Speed
```

For State 3, `cooling-levels[3] = 220`, approximately 86.3% duty.

---

# 24. Regulator

The upstream binding can reference a fan power supply:

```dts
fan-supply = <&fan_12v>;
```

The regulator framework can manage fan power enable, suspend/resume, sequencing, stop, and driver probe/remove. Before allowing the rail to turn off, consider thermal safety, shared fan rails, leakage, BMC fail-safe, hardware watchdogs, and shutdown policy. Binding support does not mean ordinary software should be allowed to shut down every server fan.

---

# 25. Startup Behavior

A stopped fan may require more duty to start than to keep spinning:

```text
Minimum Running Duty: 25%
Required Startup Duty: 50%
```

Startup must overcome bearing friction, rotor inertia, airflow resistance, low-temperature lubricant resistance, and aging. Some binding/driver versions provide stop-to-start PWM and timing properties:

```text
Stopped Fan --> Apply Higher Startup Duty --> Wait for Rotation --> Reduce to Target Duty
```

Check the exact property names and support in the target kernel’s `Documentation/devicetree/bindings/hwmon/pwm-fan.yaml`; do not copy properties from a newer kernel into an older one.

---

# 26. Upstream `interrupts` Versus Educational `tach-gpios`

The upstream binding uses:

```dts
interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
```

The driver obtains the IRQ directly from the platform device.

An educational custom driver may instead use:

```dts
tach-gpios = <&gpio0 17 GPIO_ACTIVE_HIGH>;
```

and:

```c
tach_gpiod = devm_gpiod_get(dev, "tach", GPIOD_IN);
irq = gpiod_to_irq(tach_gpiod);
```

```text
Upstream: DT interrupts --> Interrupt Framework --> pwm-fan IRQ
Custom:   tach-gpios --> GPIO Descriptor --> gpiod_to_irq() --> IRQ
```

---

# 27. `tach-gpios` Is Not a Generic `pwm-fan` Property

This is valid only for a custom binding that explicitly defines it:

```dts
demo-fan {
    compatible = "vendor,demo-pwm-tach-fan";
    pwms = <&pwm0 2 40000 0>;
    tach-gpios = <&gpio0 17 GPIO_ACTIVE_HIGH>;
};
```

It is useful for teaching GPIO descriptors, `devm_gpiod_get()`, `gpiod_to_irq()`, edge interrupts, and pulse counting.

For upstream `compatible = "pwm-fan"`, follow `pwm-fan.yaml` and use `interrupts`. Unless the binding, driver, schema, and platform integration are modified, upstream `pwm-fan` will not read `tach-gpios`; `dtbs_check` may fail and RPM reporting may be unavailable.

---

# 28. The Binding Determines Whether a Property Is Valid

A plausible property name is not automatically valid:

```text
compatible
    |
    v
Selects YAML Binding
    |
    v
Binding Defines Allowed Properties
    |
    v
Driver Parses Those Properties
```

A custom compatible, binding, and driver can define `tach-gpios`; `compatible = "pwm-fan"` must follow the upstream binding.

---

# 29. Complete Upstream `pwm-fan` Example

```dts
#include <dt-bindings/interrupt-controller/irq.h>
#include <dt-bindings/pwm/pwm.h>

fan0: pwm-fan {
    compatible = "pwm-fan";
    pwms = <&pwm0 2 40000 PWM_POLARITY_NORMAL>;
    interrupt-parent = <&gpio0>;
    interrupts = <17 IRQ_TYPE_EDGE_FALLING>;
    pulses-per-revolution = <2>;
    fan-supply = <&fan_12v>;
    cooling-levels = <80 120 170 220 255>;
    #cooling-cells = <2>;
};
```

This specifies `pwm0` Channel 2, 25 kHz normal-polarity PWM, GPIO0 interrupt 17 on falling edges, two pulses per revolution, `fan_12v`, and Cooling States 0–4.

---

# 30. Complete Custom-Driver Example

```dts
#include <dt-bindings/gpio/gpio.h>
#include <dt-bindings/pwm/pwm.h>

demo_fan0: demo-pwm-tach-fan {
    compatible = "vendor,demo-pwm-tach-fan";
    pwms = <&pwm0 2 40000 PWM_POLARITY_NORMAL>;
    tach-gpios = <&gpio0 17 GPIO_ACTIVE_HIGH>;
    pulses-per-revolution = <2>;
    cooling-levels = <80 120 170 220 255>;
    #cooling-cells = <2>;
};
```

The custom driver must parse:

```c
devm_gpiod_get(dev, "tach", GPIOD_IN);
```

GPIO consumer properties follow `<con_id>-gpios`, so the consumer ID `"tach"` maps to `tach-gpios`.

---

# 31. Runtime Validation

Find the hwmon device:

```sh
for path in /sys/class/hwmon/hwmon*; do
    if [ -r "$path/name" ]; then
        printf "%s: %s\n" "$path" "$(cat "$path/name")"
    fi
done
```

Assuming `/sys/class/hwmon/hwmon3`, read PWM and RPM:

```sh
cat /sys/class/hwmon/hwmon3/pwm1
cat /sys/class/hwmon/hwmon3/fan1_input
```

PWM 170 is about 66.7% duty; `fan1_input` 3200 means 3200 RPM.

Find the cooling device:

```sh
for path in /sys/class/thermal/cooling_device*; do
    printf "%s: " "$path"
    cat "$path/type" 2>/dev/null
done
```

Then read:

```sh
cat /sys/class/thermal/cooling_deviceX/max_state
cat /sys/class/thermal/cooling_deviceX/cur_state
```

Five cooling levels produce `max_state = 4` because states are numbered 0–4.

---

# 32. Device Tree Schema Validation

In the target kernel tree:

```sh
make dt_binding_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/hwmon/pwm-fan.yaml

make dtbs_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/hwmon/pwm-fan.yaml
```

Also validate the PWM-controller binding because the provider defines `#pwm-cells`, channel range, supported polarity, and period range. A custom driver using `tach-gpios` should have its own YAML binding rather than ignoring schema errors.

---

# 33. Layered Debugging Procedure

## Layer 1: PWM Controller

Confirm `status = "okay"`, clock, reset, pinmux, successful probe, and that Channel 2 exists.

## Layer 2: PWM Waveform

Use an oscilloscope or analyzer to verify approximately 25 kHz, correct duty and polarity, expected 0%/100% behavior, external inversion, and input voltage.

## Layer 3: Fan

Verify supply, PWM pin, minimum startup duty, stop support, actual rotation, and that RPM rises with PWM.

## Layer 4: Tach Signal

Verify pin, pull-up, GPIO input mode, edge selection, interrupt count, PPR, and absence of noise-driven duplicate counts.

## Layer 5: Hwmon

Compare:

```text
PWM Value Increases
    --> Measured Duty Increases
    --> Fan RPM Increases
    --> fan1_input Increases
```

## Layer 6: Thermal Cooling

Verify cooling-device creation, `max_state`, changing `cur_state`, cooling-level mapping, trip references, hysteresis, and governor behavior.

---

# 34. Common Mistakes

- **Wrong period unit:** `40000` is normally nanoseconds, corresponding to 25 kHz—not 40,000 Hz.
- **Copied channel number:** Another board’s Channel 2 may not connect to your `FAN_PWM`.
- **Wrong polarity:** Ignoring transistor inversion can reverse fan control.
- **Wrong tach edge:** Unsupported or double-edge counting can produce zero or double RPM.
- **Wrong PPR:** Directly scales RPM incorrectly.
- **Cooling state out of range:** Five levels have States 0–4, so `<&fan0 0 5>` is invalid.
- **Treating cooling state as RPM:** It is an index into PWM levels, not a guaranteed speed.
- **Using `tach-gpios` with upstream `pwm-fan`:** The upstream binding may accept only `interrupts`.
- **Checking compilation only:** Successful DTS compilation does not prove the channel, frequency, fan rotation, tach IRQ, RPM, thermal policy, or fail-safe behavior.

---

# 35. Recommended Validation Sequence

```text
1. Inspect the Schematic
2. Confirm the PWM Controller Binding
3. Confirm the PWM Channel
4. Convert Frequency to Period
5. Confirm Polarity and External Circuit
6. Confirm Tach GPIO/IRQ and Pull-Up
7. Confirm Pulses per Revolution
8. Run dt_binding_check / dtbs_check
9. Inspect the PWM Waveform
10. Confirm Fan Startup and Speed Control
11. Validate Tach RPM
12. Validate Cooling States
13. Validate Thermal Trip and Hysteresis
14. Test Sensor Failure and Fan Fail-Safe
```

---

# 36. Summary of Core Concepts

1. **`pwms` connects a PWM consumer to a PWM provider.**
2. **A common specifier is Provider + Channel + Period (ns) + Flags.**
3. **`<&pwm0 2 40000 0>` means Channel 2, 40,000 ns, 25 kHz, normal polarity.**
4. **Select the channel from the schematic, pinmux, and controller binding.**
5. **Account for polarity inversion by external transistors or level shifters.**
6. **`interrupts` describes the tach IRQ for upstream `pwm-fan`.**
7. **`pulses-per-revolution` determines pulse-to-RPM conversion.**
8. **`cooling-levels` maps cooling states to PWM values—not temperatures or fixed RPM.**
9. **`#cooling-cells = <2>` supplies minimum and maximum states in a thermal reference.**
10. **A cooling map connects a thermal trip to the PWM fan.**
11. **Upstream `pwm-fan` uses `interrupts`; educational `tach-gpios` belongs only to a custom binding.**
12. **Successful DTS compilation does not prove safety. Validate waveforms, RPM, thermal policy, stall detection, and fail-safe behavior.**
