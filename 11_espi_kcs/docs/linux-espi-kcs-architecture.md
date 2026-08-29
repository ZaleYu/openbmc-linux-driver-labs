# Linux eSPI and KCS architecture

## Upstream KCS BMC stack

    ASPEED LPC/eSPI register block
      -> kcs_bmc_aspeed platform driver
      -> generic kcs_bmc device/client core
      -> kcs_bmc_cdev_ipmi state machine
      -> /dev/ipmi-kcsN
      -> userspace bridge and host IPMI daemon

`kcs_bmc_aspeed.c` locates channel registers in the parent ASPEED LPC syscon,
configures host I/O decode and optional host SIRQ, and converts hardware events
into generic KCS events.

`kcs_bmc.c` manages BMC-side devices and client ownership. The IPMI character
device client in `kcs_bmc_cdev_ipmi.c` implements WRITE/READ/ABORT phases,
buffer bounds, wait queue, `poll()`, request `read()`, response `write()`, and
SMS_ATN/abort ioctls. It registers misc devices named `ipmi-kcs<channel>`.

## Host-side versus BMC-side drivers

The host OS normally uses `ipmi_si` and the KCS state machine to initiate IPMI
commands. The BMC uses `kcs_bmc_*` to receive those commands. Debugging the
wrong side leads to misleading conclusions: the BMC `/dev/ipmi-kcs3` is not the
same interface as the host `/dev/ipmi0`.

## eSPI kernel-tree reality

KCS support above is present in upstream Linux and can work with the ASPEED host
I/O register window. Broader ASPEED eSPI channel drivers and their userspace
interfaces have historically varied across vendor/OpenBMC kernel branches.
Before copying an eSPI example, inspect the exact target tree for its Kconfig,
bindings, UAPI headers, and ABI version.

Do not design a production daemon around an unverified vendor ioctl. Keep the
portable KCS/IPMI path separated from platform-specific eSPI OOB, Flash, and
raw Peripheral-channel access.

