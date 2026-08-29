# CAN protocol essentials

## Physical bus

High-speed CAN normally uses a differential CAN_H/CAN_L twisted pair, a linear
bus, short stubs, and 120-ohm termination at both ends. Dominant bits override
recessive bits. Transceiver standby, ground reference, common-mode range, cable,
termination, and EMC are as important as controller software.

## Arbitration and identifiers

All ready nodes may begin together. They transmit while monitoring the bus; a
node that sends recessive but observes dominant loses arbitration without
corrupting the winning frame. Numerically smaller base identifiers have higher
priority. IDs identify message meaning/priority, not a receiver.

Classical CAN supports 11-bit standard and 29-bit extended identifiers. Remote
frames request data but are uncommon in modern designs. A data frame contains
0 to 8 payload bytes. CAN FD extends payload to 64 bytes and can use a faster
data phase, but every participating controller/transceiver must support the
configured network mode.

## Frame and integrity

Frames include start-of-frame, arbitration, control, data, CRC, ACK, and end.
Bit stuffing maintains synchronization. Receiving nodes acknowledge a valid
frame even if no application consumes its ID; therefore missing ACK indicates
physical/configuration absence, not a missing userspace reader.

## Bit timing

A bit is divided into synchronization, propagation, phase segments, and a
sample point. The controller derives time quanta from its clock and prescaler.
Bitrate alone is insufficient when long networks or CAN FD require a specific
sample point. Linux can calculate timing from bitrate when the driver publishes
its clock and `can_bittiming_const` correctly.

## Error confinement

Nodes maintain transmit and receive error counters and transition through:

| State | Meaning |
|---|---|
| Error-active | Normal active error signaling |
| Error-warning | Counter crossed warning threshold |
| Error-passive | Node limits its disruption after many errors |
| Bus-off | Transmitter disconnects logically after severe errors |

CAN detects bit, stuff, CRC, form, and ACK errors. Automatic bus-off restart may
be configured with `restart-ms`, but repeated automatic recovery can hide a
hard wiring or bitrate fault. Record cause and counters before restarting.

## CAN FD details

CAN FD adds FDF, optional bitrate switching (BRS), error-state indicator, larger
payloads, and a different CRC. DLC values above 8 encode 12, 16, 20, 24, 32,
48, or 64 bytes. Do not treat DLC as byte count without the standard mapping.
Classical-only nodes can disrupt an FD bus unless isolated or configured safely.

## Higher-layer responsibilities

CAN does not provide end-to-end authentication, encryption, node discovery,
message freshness, fragmentation policy, or application acknowledgements.
Protocols such as CANopen, J1939, ISO-TP, or a private design add these features.
A BMC design must document ID ownership, byte order, scaling, timeouts, sequence
counters, degraded state, and command authorization.
