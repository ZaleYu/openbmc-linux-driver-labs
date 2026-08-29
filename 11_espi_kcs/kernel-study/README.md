# Kernel source study

This lab reuses the upstream ASPEED KCS BMC driver and IPMI character-device
client. It does not provide a fake eSPI controller: raw eSPI channel interfaces
must match the exact vendor/platform kernel and hardware.

Apply the config fragment to the BMC kernel:

    scripts/kconfig/merge_config.sh .config espi-kcs.config
    make olddefconfig
    make drivers/char/ipmi/

After enabling the board DTS channel, verify `ipmi-kcsN` in `/sys/class/misc`
and `/dev`. Kernel success is only the BMC half; test host enumeration and a
complete IPMI command as well.

