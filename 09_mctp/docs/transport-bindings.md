# Transport bindings and Device Tree

MCTP defines a common message format over multiple physical media. Each binding
defines physical addressing, packet framing, MTU, integrity checks, discovery
details, and medium-specific timing.

## SMBus/I2C

Linux `CONFIG_MCTP_TRANSPORT_I2C` implements the DSP0237 binding. It also needs
I2C target/slave capability because another endpoint must be able to initiate a
message to the BMC. The Device Tree property `mctp-controller` marks the bus as
an MCTP endpoint.

Electrical ownership is critical. MCTP-over-I2C is not enabled merely by adding
a property if the BMC controller lacks target mode, the mux blocks unsolicited
traffic, or another master owns the segment. Validate bus speed, slave address,
mux idle state, multi-master arbitration, and reset behavior.

## I3C

Linux `CONFIG_MCTP_TRANSPORT_I3C` implements the DSP0233 binding. The I3C bus
node uses `mctp-controller`; endpoints are normally discoverable through I3C
DAA and their MCTP device characteristics rather than static child nodes.
IBIs, dynamic addresses, MRL/MWL, PID identity, hot-join, and controller resets
all affect recovery.

## PCIe, USB, and serial

PCIe VDM is common for accelerators and NICs, but availability depends on the
BMC/host PCIe topology and kernel/platform support. USB and serial bindings are
useful for point-to-point or test setups. Do not assume that an MCTP socket
application needs to change when the underlying binding changes; normally only
network/interface provisioning changes.

## MTU and fragmentation

The route MTU must reflect the transport. The MCTP core fragments a message and
reassembles received packets. Debug both layers: a valid I2C transaction can
still contain a broken MCTP sequence, and a correct MCTP message can still be
rejected by PLDM or SPDM.

