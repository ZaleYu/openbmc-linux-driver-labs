# hwmon versus IIO

## hwmon

Hwmon is for system-health values such as temperature, voltage, current, power,
energy, humidity, fan speed and PWM control. It exports a stable sysfs ABI below
`/sys/class/hwmon/hwmonX`: `temp1_input`, `in1_input`, `fan1_input`, limits and
alarms.

A native hwmon driver normally uses
`devm_hwmon_device_register_with_info()`, `struct hwmon_chip_info`, channel
configuration bits and `struct hwmon_ops`. Do not create a custom attribute when
a standard hwmon attribute already exists.

## IIO

IIO covers ADCs, DACs and sensors where channel metadata, raw/scale/offset,
events, triggers and buffered acquisition matter. Devices appear below
`/sys/bus/iio/devices/iio:deviceX`; buffered devices also expose
`/dev/iio:deviceX`.

Important objects are `struct iio_dev`, `struct iio_chan_spec`,
`struct iio_info` and `read_raw()`. A direct-mode ADC does not require a buffer.

## Selection rule

- A fan controller or board temperature monitor normally belongs to hwmon.
- A general-purpose ADC, IMU or high-rate sensor normally belongs to IIO.
- An IIO ADC channel used for slow health monitoring can be exported by the
  generic `iio-hwmon` consumer.
- Do not expose one measurement twice to OpenBMC unless ownership and naming
  are deliberately defined.

Read `drivers/hwmon/`, `drivers/iio/`, `drivers/iio/industrialio-*.c` and
`drivers/hwmon/iio_hwmon.c` in the kernel source.

