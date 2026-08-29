# Userspace tools

Inventory:

    ls -l /sys/bus/iio/devices/
    ls -l /sys/class/hwmon/
    cat /sys/bus/iio/devices/iio:device0/name
    cat /sys/class/hwmon/hwmon0/name
    sensors

Direct IIO values commonly use:

    processed = (raw + offset) * scale

Units depend on channel type and ABI. Hwmon units are standardized:
temperature is normally milli-degrees C, voltage millivolts, current
milliamps, power microwatts and fan speed RPM.

Kernel IIO tools include `lsiio`, `iio_info`, `iio_event_monitor` and
`iio_generic_buffer` depending on the image. `libiio` supports local and remote
IIO access.

Build the included tools:

    cc -O2 -Wall -Wextra -Werror -o read_iio_channel read_iio_channel.c
    cc -O2 -Wall -Wextra -Werror -o monitor_hwmon monitor_hwmon.c
    ./read_iio_channel /sys/bus/iio/devices/iio:device0 0
    ./monitor_hwmon /sys/class/hwmon/hwmon0 temp1_input 1000

