# Demo I3C Temperature Sensor Driver

`demo_i3c_sensor.c` is an educational Linux I3C target driver for a fictional temperature sensor. It demonstrates how a native I3C device can be connected to the Linux I3C core, hardware-monitoring framework, sysfs, and In-Band Interrupt subsystem.

The driver is intended for learning and code study. It will not work with real hardware until its PID, register map, temperature format, transfer limits, and IBI payload are replaced with values from a real device datasheet.

## Features

The driver demonstrates:

* I3C target-driver registration and PID-based matching
* I3C private read and write transfers
* Register-based temperature access
* Signed fixed-point temperature conversion
* Linux hwmon integration
* Configurable high-temperature threshold
* High-temperature alarm reporting
* In-Band Interrupt request, enable, handling, and cleanup
* Custom sysfs attributes for IBI diagnostics
* Mutex protection for shared transfer buffers
* Atomic variables for IBI statistics
* Device-managed resource allocation
* Graceful fallback to polling when IBI is unavailable

## Intended architecture

The driver sits between the Linux I3C core and the hwmon userspace interface:

```text
I3C temperature target
        |
        | SDA / SCL
        v
I3C controller hardware
        |
        v
I3C controller driver
        |
        v
Linux I3C core
        |
        v
demo_i3c_sensor.c
        |
        +-------------------+
        |                   |
        v                   v
Linux hwmon             IBI diagnostics
        |                   |
        v                   v
temp1_input            ibi_count
temp1_max              last_ibi_status
temp1_alarm
```

In an OpenBMC system, a sensor service can read the hwmon attributes and publish them on D-Bus:

```text
I3C sensor
    |
    v
Linux I3C driver
    |
    v
hwmon sysfs
    |
    v
OpenBMC sensor service
    |
    v
D-Bus
    |
    v
Redfish / Web UI / remote management
```

## Important terminology

| Term             | Meaning                                                        |
| ---------------- | -------------------------------------------------------------- |
| I3C controller   | The device that initializes and controls the I3C bus           |
| I3C target       | A device controlled through the I3C bus, such as this sensor   |
| PID              | A 48-bit Provisioned ID used to identify an I3C target         |
| Dynamic address  | A runtime address assigned by the active controller            |
| Private transfer | A device-specific data transfer performed after discovery      |
| CCC              | A standardized Common Command Code used to manage the I3C bus  |
| IBI              | An In-Band Interrupt sent over SDA/SCL without a separate GPIO |
| hwmon            | The Linux hardware-monitoring framework                        |

The dynamic address is not a permanent device identity. It may change after bus reset, Dynamic Address Assignment, Hot-Join, or controller changes. Linux therefore matches this driver using information derived from the target PID rather than a hard-coded dynamic address.

## Fictional register map

The demonstration sensor uses the following register map:

| Address | Register    | Access     | Description                                          |
| ------: | ----------- | ---------- | ---------------------------------------------------- |
|  `0x00` | `DEVICE_ID` | Read-only  | Expected to contain `0xA5`                           |
|  `0x01` | `TEMP_MSB`  | Read-only  | Temperature data, most significant byte              |
|  `0x02` | `TEMP_LSB`  | Read-only  | Temperature data, least significant byte             |
|  `0x03` | `STATUS`    | Read-only  | Alarm and status bits                                |
|  `0x04` | `CONFIG`    | Read/write | Reserved for device configuration                    |
|  `0x05` | `TEMP_HIGH` | Read/write | Signed high-temperature threshold in degrees Celsius |

The status register currently defines one bit:

```text
Bit 0 = High-temperature alarm
```

In the source code, it is represented by:

```c
#define DEMO_STATUS_HIGH_ALARM BIT(0)
```

## Driver-private data

Each matched I3C target receives one `demo_i3c_data` structure:

```c
struct demo_i3c_data {
    struct i3c_device *i3cdev;
    struct mutex lock;
    u8 tx_buf[2];
    u8 rx_buf[2];
    atomic64_t ibi_count;
    atomic_t last_ibi_status;
    bool ibi_requested;
};
```

| Member            | Purpose                                                |
| ----------------- | ------------------------------------------------------ |
| `i3cdev`          | Points to the Linux I3C device                         |
| `lock`            | Serializes register access and protects shared buffers |
| `tx_buf`          | Stores register addresses and outgoing data            |
| `rx_buf`          | Stores data returned by the target                     |
| `ibi_count`       | Counts received IBIs                                   |
| `last_ibi_status` | Stores the first byte of the most recent IBI payload   |
| `ibi_requested`   | Records whether IBI resources are active               |

The transfer buffers belong to the device and are shared by all hwmon operations. A mutex is therefore required to prevent two callers from changing them at the same time.

For example, without the mutex, a temperature read and threshold write could overlap:

```text
Thread A: places TEMP_MSB in tx_buf[0]
Thread B: replaces tx_buf[0] with TEMP_HIGH
Thread A: performs a transfer using the wrong register
```

The mutex ensures that only one register transaction uses the buffers at a time.

## I3C private register reads

`demo_i3c_read_locked()` performs a register-selection write followed by a data read.

```text
Transfer 0: write one-byte register address
Transfer 1: read one or two data bytes
```

The two transfers are described with:

```c
struct i3c_priv_xfer xfers[2];
```

The `rnw` member means “read, not write”:

| `rnw`   | Operation      |
| ------- | -------------- |
| `false` | Write transfer |
| `true`  | Read transfer  |

A one-byte register read is conceptually:

```text
Write: [register address]
Read:  [register value]
```

A two-byte temperature read is:

```text
Write: [0x01]
Read:  [TEMP_MSB] [TEMP_LSB]
```

The driver submits both operations through:

```c
i3c_device_do_priv_xfers()
```

This sends the request through the Linux I3C core and the active I3C controller driver. It does not use `/dev/i2c-N`.

The function rejects invalid lengths:

```c
if (!length || length > sizeof(data->rx_buf))
    return -EINVAL;
```

Because `rx_buf` contains two bytes, valid read lengths are one or two bytes.

After the transfer, the driver also verifies the actual received length:

```c
if (xfers[1].actual_len != length)
    return -EIO;
```

A successful API return is not enough if the target returned fewer bytes than requested.

## I3C private register writes

`demo_i3c_write_locked()` writes a register address and one register value in a single private transfer:

```text
Byte 0: register address
Byte 1: register value
```

For example, setting the high-temperature threshold to 70°C produces:

```text
Register: 0x05
Value:    0x46
Transfer: 05 46
```

The driver verifies that exactly two bytes were transferred:

```c
return xfer.actual_len == 2 ? 0 : -EIO;
```

## Temperature data format

The fictional sensor returns a signed, left-aligned 12-bit temperature value:

```text
TEMP_MSB                TEMP_LSB
15                   8 7                    0
+----------------------+----------------------+
| T T T T T T T T      | T T T T x x x x      |
+----------------------+----------------------+
  12 temperature bits      4 unused bits
```

The driver combines the two bytes:

```c
raw = (s16)((bytes[0] << 8) | bytes[1]);
```

It then removes the four unused bits:

```c
raw >>= 4;
```

The value is signed, so negative temperatures can also be represented.

Each raw unit represents:

```text
0.0625°C
```

Linux hwmon reports temperatures in millidegrees Celsius:

```text
1°C = 1000 millidegrees Celsius
0.0625°C = 62.5 millidegrees Celsius
```

The conversion is:

```c
*value = DIV_ROUND_CLOSEST((long)raw * 625, 10);
```

Example:

```text
Raw value:       400
Temperature:     400 × 0.0625°C
Result:          25°C
Hwmon value:     25000
```

Therefore, a `temp1_input` value of `25000` means 25°C.

## Hwmon interface

The driver registers one temperature channel with three attributes:

```c
HWMON_CHANNEL_INFO(temp,
    HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_ALARM)
```

Linux creates files similar to:

```text
/sys/class/hwmon/hwmonX/
├── name
├── temp1_input
├── temp1_max
└── temp1_alarm
```

The exact `hwmonX` number is assigned at runtime and must not be hard-coded.

### Attribute behavior

| File          | Permission | Unit                 | Description                  |
| ------------- | ---------: | -------------------- | ---------------------------- |
| `temp1_input` |     `0444` | millidegrees Celsius | Current measured temperature |
| `temp1_max`   |     `0644` | millidegrees Celsius | High-temperature threshold   |
| `temp1_alarm` |     `0444` | Boolean              | High-temperature alarm state |

The permissions are selected by `demo_i3c_is_visible()`:

```text
0444 = readable by all users
0644 = writable by the owner, readable by other users
```

Returning `0` from `is_visible()` means that the requested attribute is not supported and should not be created.

## Reading hwmon attributes

`demo_i3c_hwmon_read()` handles the three readable temperature attributes.

### `temp1_input`

The driver reads registers `0x01` and `0x02`, combines the signed 12-bit value, and converts it to millidegrees Celsius.

### `temp1_max`

The driver reads register `0x05` as a signed 8-bit number:

```c
*value = (long)(s8)regval * 1000;
```

Examples:

```text
Register value 70  -> 70000 m°C -> 70°C
Register value -5 -> -5000 m°C -> -5°C
```

### `temp1_alarm`

The driver reads the status register and checks bit 0:

```c
*value = !!(regval & DEMO_STATUS_HIGH_ALARM);
```

The double negation converts any nonzero bit result into exactly `1`:

```text
0 = no high-temperature alarm
1 = high-temperature alarm active
```

## Writing the temperature threshold

`demo_i3c_hwmon_write()` supports only `temp1_max`.

Userspace supplies the value in millidegrees Celsius:

```text
70000 m°C
```

The driver converts it to whole degrees:

```c
degrees = DIV_ROUND_CLOSEST(value, 1000);
```

It then checks the signed 8-bit register range:

```c
if (degrees < -128 || degrees > 127)
    return -ERANGE;
```

The accepted range is therefore:

```text
-128°C to 127°C
```

Finally, the converted value is written to `DEMO_REG_TEMP_HIGH`.

Because the demonstration register stores only whole degrees, a request such as `70500` millidegrees is rounded to the nearest whole degree before it is written.

## In-Band Interrupt support

I3C allows a target to request service through SDA/SCL without using a separate GPIO interrupt signal. This mechanism is called an In-Band Interrupt.

The demonstration requests the following IBI resources:

```c
struct i3c_ibi_setup ibi_setup = {
    .max_payload_len = 2,
    .num_slots = 4,
    .handler = demo_i3c_ibi_handler,
};
```

| Setting           | Meaning                                  |
| ----------------- | ---------------------------------------- |
| `max_payload_len` | Accept up to two payload bytes per IBI   |
| `num_slots`       | Pre-allocate four IBI handling slots     |
| `handler`         | Function called when an IBI is delivered |

The request and enable sequence is:

```text
i3c_device_request_ibi()
        |
        v
Allocate IBI resources
        |
        v
i3c_device_enable_ibi()
        |
        v
Allow the target to generate IBIs
```

Requesting resources and enabling the event are separate operations.

If IBI setup fails, the driver does not fail the entire probe. It prints an informational message and continues in polling mode:

```text
IBI available   -> hwmon reads plus asynchronous notifications
IBI unavailable -> hwmon reads still work
```

This allows the sensor to remain usable on controllers that do not support IBI.

## IBI handler

When the target generates an IBI, the Linux I3C subsystem calls:

```c
demo_i3c_ibi_handler()
```

The demonstration treats the first payload byte as the sensor status:

```c
if (payload && payload->len)
    status = ((const u8 *)payload->data)[0];
```

It then:

1. Stores the most recent status byte.
2. Increments the total IBI count.
3. Checks the high-temperature alarm bit.
4. Prints a rate-limited warning if the alarm is active.

```c
atomic_set(&data->last_ibi_status, status);
atomic64_inc(&data->ibi_count);
```

Atomic operations are used because the IBI handler may run concurrently with sysfs readers. They update these simple values without using a sleeping mutex.

The warning uses:

```c
dev_warn_ratelimited()
```

Rate limiting prevents a faulty or noisy target from flooding the kernel log with thousands of identical messages.

The handler intentionally performs only short, bounded work. A real driver should move expensive processing to a workqueue or another suitable execution context.

## IBI diagnostic attributes

The driver adds two read-only attributes to the I3C device:

```text
ibi_count
last_ibi_status
```

| Attribute         | Description                                 |
| ----------------- | ------------------------------------------- |
| `ibi_count`       | Total number of IBIs received since probe   |
| `last_ibi_status` | First payload byte from the most recent IBI |

Example values:

```text
ibi_count:       15
last_ibi_status: 0x01
```

These files belong to the I3C device, not the hwmon device. Their exact path depends on the I3C controller and the device naming assigned by the kernel.

They can be located with:

```sh
find /sys/bus/i3c/devices \
    \( -name ibi_count -o -name last_ibi_status \) -print
```

## Probe sequence

Linux calls `demo_i3c_probe()` after the I3C core discovers a matching target.

The initialization sequence is:

```text
Allocate driver-private data
        |
        v
Initialize mutex and atomic variables
        |
        v
Attach private data to the I3C device
        |
        v
Read and validate DEVICE_ID
        |
        v
Create IBI diagnostic attributes
        |
        v
Register the hwmon device
        |
        v
Request and enable IBI
        |
        v
Print PID and dynamic address
```

### 1. Allocate private data

```c
data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
```

`devm_kzalloc()` allocates zero-initialized memory associated with the device. Linux automatically releases it when the device is removed.

### 2. Save driver data

```c
i3cdev_set_drvdata(i3cdev, data);
```

Other callbacks can retrieve it later with:

```c
i3cdev_get_drvdata(i3cdev);
```

### 3. Validate the device ID

The driver reads register `0x00` and expects:

```text
0xA5
```

PID matching selects the driver, while the register-level device ID provides an additional check that the expected register map is present.

The probe fails if the register cannot be read or contains an unexpected value.

### 4. Register sysfs and hwmon

The driver creates its IBI diagnostic attributes and registers the standard hwmon interface.

Both registrations use device-managed APIs, so their resources are removed automatically.

### 5. Configure IBI

If IBI request and enable both succeed, `ibi_requested` is set to `true`.

If enable fails after a successful request, the driver immediately frees the requested IBI resources before continuing without IBI.

### 6. Report the discovered device

The driver obtains current I3C information with:

```c
i3c_device_get_info()
```

It prints the PID and current dynamic address:

```text
demo sensor PID 024608ac0000 at dynamic address 0x09 with IBI
```

The dynamic address is useful for diagnostics, but applications must not treat it as a permanent identifier.

## Remove sequence

When the driver is unloaded or the device is removed, Linux calls:

```c
demo_i3c_remove()
```

If IBI is active, the driver:

```text
Disables IBI
    |
    v
Prevents new IBI delivery
    |
    v
Frees IBI resources
```

This ordering prevents a new IBI from arriving while its resources are being destroyed.

Memory, hwmon registration, and the custom sysfs group are cleaned up automatically because they were created with device-managed APIs.

## PID-based driver matching

The driver supports one fictional I3C identity:

```c
static const struct i3c_device_id demo_i3c_ids[] = {
    I3C_DEVICE(0x0123, 0x0456, NULL),
    { }
};
```

The fields represent:

```text
Manufacturer ID: 0x0123
Part ID:         0x0456
Driver data:     NULL
```

The final empty entry terminates the ID table.

The I3C core compares discovered target identity information against this table. A matching target causes Linux to call `demo_i3c_probe()`.

```c
MODULE_DEVICE_TABLE(i3c, demo_i3c_ids);
```

This exports the supported IDs as module metadata and helps userspace module-loading tools associate the device with the driver.

For real hardware, replace the fictional manufacturer and part IDs with values derived from the target PID and verified against the device documentation.

## Driver registration

The driver is registered with:

```c
static struct i3c_driver demo_i3c_driver = {
    .driver = {
        .name = "demo_i3c_sensor",
    },
    .probe = demo_i3c_probe,
    .remove = demo_i3c_remove,
    .id_table = demo_i3c_ids,
};
```

The fields connect the driver to:

| Field       | Purpose                              |
| ----------- | ------------------------------------ |
| `.name`     | Kernel driver name                   |
| `.probe`    | Initializes a matching target        |
| `.remove`   | Cleans up when the target is removed |
| `.id_table` | Lists supported I3C identities       |

The following macro creates the module initialization and exit functions:

```c
module_i3c_driver(demo_i3c_driver);
```

## Kernel requirements

The target kernel must include:

* I3C core support
* The correct I3C controller driver
* Hardware-monitoring support
* Loadable module support if building this driver as a module

Relevant kernel configuration options commonly include:

```text
CONFIG_I3C
CONFIG_HWMON
CONFIG_MODULES
```

The exact I3C controller option depends on the SoC.

Check the running kernel configuration with:

```sh
zcat /proc/config.gz | grep -E 'CONFIG_(I3C|HWMON|MODULES)='
```

On systems without `/proc/config.gz`, try:

```sh
grep -E 'CONFIG_(I3C|HWMON|MODULES)=' /boot/config-$(uname -r)
```

The controller must already be enabled through the platform firmware description, such as Device Tree or ACPI, and its driver must successfully initialize the I3C bus.

## Building the module

Build the module against the exact kernel build tree used by the target system.

If the accompanying Makefile supports standard external-module builds:

```sh
make
```

Alternatively, a typical external-module command is:

```sh
make -C /lib/modules/$(uname -r)/build M="$PWD" modules
```

The output should include:

```text
demo_i3c_sensor.ko
```

Verify module information:

```sh
modinfo ./demo_i3c_sensor.ko
```

Load the module:

```sh
sudo insmod ./demo_i3c_sensor.ko
```

Or, after installing it into the module tree:

```sh
sudo modprobe demo_i3c_sensor
```

Check the kernel log:

```sh
dmesg | tail -n 50
```

Remove the module:

```sh
sudo rmmod demo_i3c_sensor
```

An `invalid module format` error usually means the module was built against an incompatible kernel version, configuration, or symbol set.

## Verifying the hwmon interface

Locate the hwmon device by name:

```sh
for path in /sys/class/hwmon/hwmon*; do
    if [ -r "$path/name" ]; then
        printf '%s: %s\n' "$path" "$(cat "$path/name")"
    fi
done
```

Look for:

```text
demo_i3c_sensor
```

Assume the device appears as:

```text
/sys/class/hwmon/hwmon3
```

The number is only an example and may be different after every boot.

### Read the current temperature

```sh
cat /sys/class/hwmon/hwmon3/temp1_input
```

Example:

```text
25500
```

This means:

```text
25.5°C
```

### Read the high-temperature threshold

```sh
cat /sys/class/hwmon/hwmon3/temp1_max
```

### Set the threshold to 70°C

```sh
echo 70000 | sudo tee /sys/class/hwmon/hwmon3/temp1_max
```

### Read the alarm state

```sh
cat /sys/class/hwmon/hwmon3/temp1_alarm
```

Possible results:

| Value | Meaning                       |
| ----: | ----------------------------- |
|   `0` | No high-temperature alarm     |
|   `1` | High-temperature alarm active |

### Read all values with `sensors`

If `lm-sensors` is installed:

```sh
sensors
```

The output may resemble:

```text
demo_i3c_sensor-i3c-0
Adapter: I3C adapter
temp1:        +25.5°C  (high = +70.0°C)
```

The exact label and adapter name depend on the platform and userspace configuration.

## Verifying IBI operation

Locate the custom attributes:

```sh
find /sys/bus/i3c/devices \
    \( -name ibi_count -o -name last_ibi_status \) -print
```

Read the total IBI count:

```sh
cat /sys/bus/i3c/devices/<device>/ibi_count
```

Read the most recent status byte:

```sh
cat /sys/bus/i3c/devices/<device>/last_ibi_status
```

Monitor kernel messages while generating a high-temperature event:

```sh
sudo dmesg -w
```

When bit 0 of the IBI payload is set, the driver reports:

```text
high-temperature IBI received
```

The message is rate-limited, so repeated events may not produce one log line per IBI.

## Error handling

The driver uses standard negative Linux error codes:

| Error         | Meaning in this driver                        |
| ------------- | --------------------------------------------- |
| `-ENOMEM`     | Private-data allocation failed                |
| `-EINVAL`     | Invalid private-read length                   |
| `-EIO`        | Transfer completed with an unexpected length  |
| `-ENODEV`     | Register-level device ID did not match        |
| `-EOPNOTSUPP` | Unsupported hwmon type, channel, or attribute |
| `-ERANGE`     | Threshold is outside the signed 8-bit range   |

`dev_err_probe()` is used during probe to report initialization errors while preserving the original error code.

## Common debugging steps

### Driver does not probe

Check:

* Is the I3C controller enabled?
* Did the controller driver initialize successfully?
* Was the target discovered during DAA?
* Does the target PID match `demo_i3c_ids[]`?
* Is the module loaded?
* Is `CONFIG_I3C` enabled?

Commands:

```sh
dmesg | grep -i i3c
lsmod | grep demo_i3c_sensor
modinfo ./demo_i3c_sensor.ko
```

### Probe reports an unexpected device ID

Possible causes include:

* `DEMO_REG_DEVICE_ID` does not match the real register map.
* `DEMO_DEVICE_ID` is incorrect.
* The target uses a different register-access format.
* The private-transfer sequence is incorrect for the device.
* The wrong target matched the fictional PID.

### No hwmon files appear

Check whether probe reached the hwmon registration step:

```sh
dmesg | grep -i demo
```

Also verify:

```text
CONFIG_HWMON=y
```

or:

```text
CONFIG_HWMON=m
```

### Temperature value is incorrect

Check:

* Signed versus unsigned representation
* MSB/LSB order
* Number of valid bits
* Left- or right-aligned format
* LSB resolution
* Required scale and offset
* Target maximum read length

The demonstration assumes:

```text
Signed 12-bit value
Left-aligned in a 16-bit register pair
0.0625°C per LSB
Big-endian byte order
```

### IBI is unavailable

This does not necessarily indicate that normal sensor access is broken.

Possible reasons include:

* The target does not support IBI.
* The I3C controller does not support IBI.
* The controller driver does not implement IBI.
* The requested payload size is unsupported.
* The requested slot count cannot be allocated.
* IBI is disabled by bus policy.
* The target BCR does not advertise the expected capability.

The driver continues in polling mode when this occurs.

### IBI count does not increase

Check:

* Did `i3c_device_request_ibi()` succeed?
* Did `i3c_device_enable_ibi()` succeed?
* Is the target configured to generate the event?
* Is the alarm threshold actually crossed?
* Is the payload format correct?
* Is the controller receiving and acknowledging IBI requests?
* Is the controller queue overflowing?

Use an I3C-capable logic analyzer when protocol-level inspection is necessary. A basic I2C decoder may not correctly interpret I3C traffic.

## Adapting the driver to real hardware

At minimum, replace or verify the following items.

### Identity

```c
I3C_DEVICE(0x0123, 0x0456, NULL)
```

Replace the fictional manufacturer and part IDs with values for the real target.

### Register map

Replace:

```c
DEMO_REG_DEVICE_ID
DEMO_REG_TEMP_MSB
DEMO_REG_TEMP_LSB
DEMO_REG_STATUS
DEMO_REG_CONFIG
DEMO_REG_TEMP_HIGH
```

### Device ID

Replace:

```c
#define DEMO_DEVICE_ID 0xa5
```

If the real device has no register-level device ID, remove or redesign that validation step.

### Temperature conversion

Verify:

* Number of temperature bits
* Signed encoding
* Byte order
* Bit alignment
* LSB resolution
* Scale
* Offset
* Rounding requirements

### Private-transfer format

Some devices may require:

* A 16-bit register address
* A command byte
* Separate transfers
* Different read and write lengths
* Delays between operations
* CRC or PEC-like validation
* Vendor-defined protocol framing

### Threshold format

The demonstration uses a signed 8-bit whole-degree threshold. A real target may use the same fixed-point format as its temperature readings.

### IBI payload

Verify:

* Maximum payload length
* Mandatory Data Byte behavior
* Payload byte order
* Event type fields
* Status bits
* Whether the event must be acknowledged or cleared
* How interrupt storms and overflow should be handled

### Power management

A production driver may also require:

* Runtime PM
* System suspend/resume callbacks
* IBI disable and re-enable around suspend
* Register restoration after reset
* Dynamic-address recovery
* Target reinitialization after bus reset

## Safety notes

* Do not hard-code the target dynamic address.
* Do not bypass the Linux I3C core to access controller registers directly.
* Do not assume an arbitrary I2C target can safely share the I3C bus.
* Do not perform long or sleeping operations in the IBI handler.
* Do not ignore actual transfer lengths.
* Do not use the fictional PID or register map with production hardware.
* Verify voltage, pull-ups, bus capacitance, signal integrity, and mixed-bus compatibility.
* Test reset, power sequencing, Hot-Join, IBI bursts, and stuck SDA/SCL conditions.

## Suggested learning path

1. Read the fictional register map.
2. Study `demo_i3c_data` and identify the shared resources.
3. Follow one register read through `demo_i3c_read_locked()`.
4. Follow the raw temperature conversion.
5. Map the hwmon callbacks to their sysfs files.
6. Follow the complete `probe()` sequence.
7. Study the IBI request, enable, handler, disable, and free lifecycle.
8. Review the PID-based ID table.
9. Compare the demonstration with a real I3C target datasheet.
10. Compare it with an upstream Linux I3C target driver.

## Key takeaways

* I3C target drivers are matched using discovered target identity, not a fixed dynamic address.
* Device-specific register access uses I3C private transfers.
* Standard Linux frameworks such as hwmon should be used to expose sensor data.
* Hwmon temperatures are represented in millidegrees Celsius.
* IBI provides interrupt-like notification over SDA/SCL, but still requires resource management, bounded handling, overflow planning, and cleanup.
* Device-managed APIs simplify normal resource cleanup, while IBI resources are explicitly disabled and freed.
* This driver is a learning template and must be adapted carefully before use with real hardware.

## License

This demonstration driver is licensed under:

```text
GPL-2.0-only
```

See the SPDX identifier at the top of `demo_i3c_sensor.c`.
