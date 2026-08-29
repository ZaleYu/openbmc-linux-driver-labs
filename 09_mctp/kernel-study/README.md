# Kernel source study

This directory intentionally does not provide a fictional MCTP client kernel
module. Linux applications consume MCTP through AF_MCTP sockets; transport
drivers create MCTP netdevices and are tightly coupled to real hardware and a
DMTF binding.

Use the included config fragment as a starting point, then study the source map.
For a board port, enable an existing transport and configure Device Tree before
changing kernel code.

Validation on a target kernel:

    scripts/config --enable MCTP
    scripts/config --module MCTP_TRANSPORT_I2C
    make olddefconfig
    make drivers/net/mctp/

The I2C transport requires controller target/slave support. Check the specific
BMC SoC bus driver rather than assuming `CONFIG_I2C_SLAVE` is sufficient.

