# Userspace tools

Modern upstream Linux presents PECI functionality through kernel drivers and
subsystem interfaces rather than a generic stable raw-command userspace ABI.
Use hwmon for temperature data and a purpose-built kernel/OpenBMC service for
generation-specific functions.

Inventory:

    find /sys/bus/peci/devices -maxdepth 2 -type l -o -type f
    ls -l /sys/bus/peci/devices
    grep . /sys/class/hwmon/hwmon*/name
    sensors

Read one hwmon device safely:

    name=/sys/class/hwmon/hwmon3
    cat "$name/name"
    grep . "$name"/temp*_label "$name"/temp*_input

Hwmon temperatures are integer millidegrees Celsius. `85000` means 85 °C.
Channel numbering is dynamic: locate a device using its `name` and labels, not
a hard-coded `hwmon3/temp1_input` path.

The included `peci_inventory` prints controllers/devices and driver bindings.
`peci_hwmon_dump` locates PECI hwmon devices and emits labeled temperatures.
Both are read-only and use only libc.

For OpenBMC, also inspect:

    systemctl status xyz.openbmc_project.CPUSensor.service
    journalctl -u xyz.openbmc_project.CPUSensor.service
    busctl tree xyz.openbmc_project.CPUSensor
    busctl tree xyz.openbmc_project.ObjectMapper

Avoid `devmem` and undocumented raw commands on a running server. Writes can
change package state, conflict with thermal management, or be invalid for the
installed CPU generation.

