# Kernel source study

This lab does not add a fictional PECI device driver. CPU commands and register
formats are generation-specific, while upstream already provides the ASPEED
controller, PECI core, CPU matcher, and hwmon functional drivers.

Apply `peci.config` to a target BMC kernel, enable the board DTS node, and build:

    scripts/kconfig/merge_config.sh .config peci.config
    make olddefconfig
    make drivers/peci/ drivers/hwmon/peci/

Then verify module dependencies with `modinfo` and test on the exact Intel CPU
generation. A successful compile does not prove electrical timing, command
availability, channel mapping, or CPU power-state behavior.

