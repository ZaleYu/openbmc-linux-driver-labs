# OpenBMC use cases

## Endpoint discovery and EID assignment

The BMC commonly acts as a bus owner. `mctpd` discovers physical endpoints,
assigns or learns EIDs, updates kernel neighbours/routes, and publishes endpoint
information on D-Bus. Higher-layer services should consume that inventory
instead of hard-coding an EID that can change after reset or replacement.

## PLDM platform management

PLDM over MCTP can expose sensors, effecters, inventory records, events, and
firmware update operations for NICs, GPUs, retimers, FPGAs, and satellite
controllers. Debug the complete path:

    endpoint -> binding -> kernel MCTP -> mctpd discovery
        -> PLDM daemon -> D-Bus -> Redfish / policy

A PLDM timeout is not automatically a PLDM bug; prove that the request was
routed and that a correctly tagged response returned.

## SPDM security

SPDM over MCTP can authenticate a component and retrieve measurements. The
security service first needs a trusted mapping between discovered endpoint and
platform inventory. EID reuse, hot-plug, certificate validation, session state,
timeouts, and reset recovery must be included in the threat model.

## Multi-transport and bridging

A GPU server may expose endpoints on I2C/I3C and PCIe VDM at the same time.
MCTP networks prevent overlapping EID spaces from becoming ambiguous. A bridge
adds routing and discovery responsibilities; route loops and duplicated EIDs
must be prevented.

## Fault-injection checklist

- Reset an endpoint after EID assignment.
- Replace it with a device at the same physical address.
- Remove a mux channel or PCIe link during a fragmented response.
- Send concurrent PLDM and SPDM traffic.
- Exhaust request tags and application transaction IDs.
- Delay, duplicate, truncate, or reorder packets in a test transport.
- Restart `mctpd` and each consumer independently.
- Reboot the BMC while the host and endpoints remain powered.

