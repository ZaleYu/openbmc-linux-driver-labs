# Userspace tools

The `mctp` command from the mctp-tools project manages kernel MCTP links,
addresses, neighbours, and routes. Exact subcommand syntax can vary by packaged
version; use `mctp help` on the target image.

Typical inspection:

    mctp link
    mctp addr
    mctp neigh
    mctp route
    ip -details link show type mctp

A minimal static lab commonly performs these logical operations:

1. Enable the MCTP link and assign it to a network.
2. Add the local BMC EID to the interface.
3. Add a neighbour mapping for the remote EID and physical address.
4. Add a route for the remote EID through the interface.

Production OpenBMC systems generally let `mctpd` own discovery and dynamic EID
assignment. Do not run an ad-hoc script that races the daemon and rewrites its
routes.

Useful system checks:

    ls /sys/class/net
    cat /proc/net/dev
    systemctl status mctpd.service
    journalctl -u mctpd.service
    busctl tree xyz.openbmc_project.MCTP

The included programs use only Linux UAPI headers and libc. `mctp_request`
prepends the selected message-type byte to the supplied hex payload and waits
for the matching response.
`mctp_echo` is a lab responder; never bind it to Control, PLDM, or SPDM on a
production system because it would compete with the real protocol daemon.
