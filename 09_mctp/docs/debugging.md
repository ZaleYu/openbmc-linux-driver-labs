# Debugging MCTP

## Layered decision path

1. Confirm power, reset, clocks, pull-ups, mux route, PCIe link, or I3C DAA.
2. Confirm the transport driver created an `ARPHRD_MCTP` network interface.
3. Confirm the interface is up and assigned to the intended MCTP network.
4. Confirm the local EID, neighbour physical address, and route.
5. Confirm discovery and bus-owner policy in `mctpd` logs and D-Bus objects.
6. Capture a request and response; verify EIDs, SOM/EOM, sequence, tag and type.
7. Debug PLDM/SPDM only after the MCTP exchange is valid.

Commands:

    dmesg | grep -Ei 'mctp|i2c|i3c|pcie|smbus'
    mctp link; mctp addr; mctp neigh; mctp route
    ip -s -d link show
    journalctl -u mctpd.service
    busctl tree xyz.openbmc_project.MCTP

## Common symptoms

- No interface: missing kernel config, module, DTS property, controller mode, or
  unsupported transport hardware.
- Interface but no reply: wrong EID/network, neighbour address, route, bus
  ownership, mux channel, remote state, or message type.
- Works after boot but not reset: stale dynamic EID or physical-address mapping.
- Small payload works: incorrect MTU, fragmentation, sequence, buffer limit, or
  binding timeout.
- Intermittent loss: I2C arbitration, I3C address changes, IRQ latency, queue
  overflow, PCIe error, or overly aggressive application timeout.
- Response reaches kernel but not app: wrong bound type/EID/network/tag, another
  socket consumed traffic, or application timeout.

## Evidence collection

Collect topology and counters before restarting services. Record kernel and
mctpd logs with monotonic timestamps. On I2C/I3C, use a logic analyzer that
understands the binding; on PCIe, collect link/AER and VDM evidence. Dynamic
debug and tracepoints may alter timing, so correlate them with physical traces.

Never probe a shared management bus with generic raw writes. A malformed
Control, PLDM, or firmware-update command can change endpoint state.

