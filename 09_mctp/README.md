# Linux MCTP for OpenBMC

This lab teaches how MCTP (Management Component Transport Protocol) works in Linux/OpenBMC. It traces a management message from an OpenBMC application through:

```text
AF_MCTP Socket
Linux MCTP Core
Route / Neighbour
MCTP Transport Driver
I2C / I3C / PCIe / USB / Serial
```

to a remote management endpoint, with the response returning along the reverse path.

---

# 1. What Is MCTP?

MCTP is a DMTF management-message transport protocol used among servers, BMCs, storage, NICs, GPUs, NVMe devices, and other management components.

A BMC may manage PSUs, NICs, GPUs, NVMe SSDs, fan controllers, security devices, add-in cards, backplane controllers, and other management controllers connected over I2C/SMBus, I3C, PCIe VDM, USB, serial, or another MCTP binding.

MCTP provides a common logical transport layer so upper-layer protocols do not need separate implementations for every physical bus.

---

# 2. What Problem Does MCTP Solve?

Without MCTP, an application might directly handle every transport:

```text
PLDM Application
    +-- I2C Handling
    +-- I3C Handling
    +-- PCIe VDM Handling
    +-- USB Handling
```

With MCTP:

```text
PLDM / SPDM / NVMe-MI Application
                |
                v
              MCTP
          +-----+-----+
          v     v     v
         I2C   I3C  PCIe VDM
```

The application mainly chooses the destination endpoint, message type, and payload rather than directly operating every bus controller.

---

# 3. Transport and Message Type Are Different

An MCTP transport binding defines **how packets are carried**; the Message Type defines **which management protocol the payload represents**.

Common transports include MCTP over I2C/SMBus, I3C, PCIe VDM, USB, and serial. They handle link-layer addresses, framing, MTU, packet I/O, transport headers, and bus operations.

| Message Type | Protocol |
| --- | --- |
| `0x00` | MCTP Control |
| `0x01` | PLDM |
| `0x04` | NVMe-MI |
| `0x05` | SPDM |
| `0x7E` | PCI Vendor-Defined |
| `0x7F` | IANA Vendor-Defined |

```text
Transport    = Truck or Road
Message Type = Cargo
```

The same PLDM payload can travel over I2C, I3C, or PCIe VDM. Its meaning remains the same while lower-layer encapsulation and link addresses differ.

---

# 4. Common Upper-Layer Protocols

## PLDM

Platform Level Data Model is used for sensor monitoring, platform events, FRU data, firmware update, BIOS/platform configuration, and device control. It uses MCTP Message Type `0x01`.

## SPDM

Security Protocol and Data Model supports device authentication, certificate exchange, measurements, session establishment, and secure communication. MCTP itself provides transport, not authentication or encryption; SPDM or another security protocol must provide authentication, integrity, confidentiality, and anti-replay protection.

## NVMe-MI

NVMe Management Interface supports NVMe controller information, health monitoring, subsystem management, and sideband management.

## Vendor-Defined Messages

The lab uses `0x7E` for request/response testing only; it does not implement the full PCI Vendor-Defined Message format.

---

# 5. Repository Structure

| Directory | Contents |
| --- | --- |
| `docs/` | Protocol, Linux architecture, transport bindings, tools, debugging, and OpenBMC use cases |
| `kernel-study/` | Linux MCTP source map and minimal kernel configuration fragment |
| `device-tree/` | Device Tree examples for MCTP over I2C, I3C, and muxed I2C |
| `userspace/` | AF_MCTP requester and echo responder |
| `scripts/` | Interface inventory, route diagnostics, and network-namespace lab |

Recommended order: Protocol → Linux Architecture → AF_MCTP Example → Route/Neighbour → Transport Binding → OpenBMC Use Case.

---

# 6. Complete Data Path

```text
PLDM / SPDM / Vendor Application
                |
                v
          AF_MCTP Socket
                |
                v
          Linux MCTP Core
                |
                v
      Route and Neighbour Lookup
                |
                v
     MCTP Transport Netdevice
                |
                v
 I2C / I3C / PCIe VDM / USB / Serial
                |
                v
        Remote MCTP Endpoint
```

The response returns through the transport driver and core, which selects the application socket by EID, Message Type, and Tag.

---

# 7. What Is an Endpoint?

An endpoint is a management device that sends or receives MCTP messages, such as a BMC, GPU management controller, NIC, NVMe device, PSU controller, or security controller.

It may be a Bus Owner, management controller, managed device, bridge, or ordinary endpoint. Each endpoint is normally assigned an EID.

---

# 8. EID

An Endpoint ID is a logical address in an MCTP network:

```text
BMC EID  = 0x0A
GPU EID  = 0x08
NIC EID  = 0x09
NVMe EID = 0x0B
```

It differs from a hardware link address. For example:

```text
MCTP EID          = 0x08
I2C Slave Address = 0x1D
```

Applications normally address an endpoint by EID rather than by its I2C address.

---

# 9. An EID Is Not a Permanent Hardware Identity

EIDs are platform policy and may change after power cycles, network reinitialization, discovery, Bus Owner reassignment, hot-plug, slot changes, or bridge-topology changes.

OpenBMC must map an EID to physical slot, inventory object, UUID, hardware identity, transport, physical address, and supported Message Types.

---

# 10. MCTP Network ID

Linux uses a Network ID to distinguish MCTP routing domains:

```text
Network 1: Mainboard I2C MCTP
Network 2: PCIe VDM MCTP
Network 3: External Management Module
```

The same EID can exist in different networks, so the complete destination concept is normally `Network ID + Destination EID`.

---

# 11. Route

A route tells Linux which MCTP interface to use for an EID:

```text
EID 8     --> mctpi2c0
EID 9     --> mctp-pcie0
EID 20–30 --> MCTP Bridge
```

Without a route, `sendto()` may fail with “Network is unreachable,” no packet reaches the transport driver, and the requester times out. A route chooses the logical path; it does not necessarily contain the physical link address.

---

# 12. Neighbour

The neighbour table maps an EID to a transport-specific link address:

```text
Route:     EID 8 uses mctpi2c0
Neighbour: EID 8 maps to I2C Address 0x1D
```

| Component | Question Answered |
| --- | --- |
| Route | Which interface should be used? |
| Neighbour | Which physical/link address on that interface? |

Other transports may represent neighbours differently from I2C.

---

# 13. MCTP Transport Netdevice

Linux represents many MCTP transports as network devices, such as `mctpi2c0`, `mctpi3c0`, or `mctpserial0`. Names vary by kernel and platform.

```sh
ip link show
mctp link
```

The netdevice passes Linux MCTP packets to the actual transport driver.

---

# 14. AF_MCTP Socket

Applications create an MCTP datagram socket with:

```c
socket(AF_MCTP, SOCK_DGRAM, 0);
```

```text
Create Socket
    --> Bind Local EID / Message Type
    --> sendto() Request
    --> recvfrom() Response
```

AF_MCTP uses its own Network ID, EID, Message Type, Message Tag, and `struct sockaddr_mctp`; it is not IP/UDP. Applications do not manipulate I2C controller registers or construct complete link-layer frames.

---

# 15. Message Type and Socket Binding

An AF_MCTP socket can receive a specific Message Type:

```sh
sudo ./userspace/mctp_echo 0x7e
```

This socket receives Type `0x7E`, not PLDM `0x01` or SPDM `0x05`. Separate applications can therefore share one MCTP transport:

```text
0x01 --> PLDM Application
0x05 --> SPDM Application
0x7E --> Demo Echo Responder
```

---

# 16. MCTP Message Header and Payload

```text
+-------------------+
| MCTP Base Header  |
+-------------------+
| Message Type      |
+-------------------+
| Protocol Payload  |
+-------------------+
```

The base header contains Destination and Source EIDs, Start/End of Message, packet sequence, Tag Owner, and Message Tag. The Message Type is followed by the PLDM, SPDM, NVMe-MI, or vendor payload. The core transports packets; the application still interprets its protocol payload.

---

# 17. Fragmentation and Reassembly

If a message exceeds the transport MTU, MCTP splits it into packets:

```text
Packet 0: SOM=1, EOM=0
Packet 1: SOM=0, EOM=0
Packet 2: SOM=0, EOM=1
```

The receiver uses SOM, EOM, and packet sequence to reassemble the complete message. AF_MCTP applications normally send complete messages rather than individual transport packets, but must obey kernel and transport size limits.

---

# 18. Requester, Responder, and Message Tag

MCTP uses a three-bit Message Tag to match requests and responses:

```text
Requester: Destination EID=8, Type=0x7E, Tag=3, Tag Owner=1
Responder: Destination=Requester, Type=0x7E, Tag=3, Tag Owner=0
```

The requester owns the tag. The responder uses the same tag, clears Tag Owner, and returns the message to the requester. Applications must handle matching, timeouts, concurrent requests, invalid responses, and unexpected types.

---

# 19. Quick Start

Build the userspace tools:

```sh
make -C userspace
```

This produces `userspace/mctp_echo` and `userspace/mctp_request`. The target kernel must support AF_MCTP.

Start the echo responder in one terminal:

```sh
sudo ./userspace/mctp_echo 0x7e
```

Send a request from another terminal:

```sh
sudo ./userspace/mctp_request 8 0x7e 01 02 03 04
```

| Argument | Meaning |
| --- | --- |
| `8` | Destination EID |
| `0x7e` | MCTP Message Type |
| `01 02 03 04` | Test payload bytes |

This requires a configured local address, an up interface, a route to EID 8, a neighbour when required, a reachable responder, and matching Message Types.

---

# 20. What Does the Test Validate?

```text
AF_MCTP Socket
    --> Local EID
    --> Route Lookup
    --> Neighbour Lookup
    --> MCTP Interface
    --> Request / Responder
    --> EID and Tag Swap
    --> Response
```

It does not implement complete PLDM, SPDM, NVMe-MI, PCI vendor-defined protocol, security verification, encryption, or production endpoint discovery.

---

# 21. Examining MCTP State

```sh
mctp link
mctp address
mctp route
mctp neigh
mctp help
```

Tool syntax may differ by version. Project scripts also provide interface inventory, address checks, route and neighbour diagnostics, and a network-namespace lab.

---

# 22. Understand the Topology Before Creating Routes

Do not assign EID 8 to every production device merely because the example uses it. EID allocation is platform policy.

| Endpoint | Example EID |
| --- | --- |
| BMC | `0x0A` |
| GPU 0 | `0x20` |
| GPU 1 | `0x21` |
| NIC 0 | `0x30` |
| NVMe Backplane | `0x40` |

Real allocation must consider Bus Owner, discovery, static/dynamic EIDs, bridges, hot-plug, multiple segments, inventory and D-Bus mapping, firmware policy, and coordination with other management controllers.

---

# 23. Bus Owner

A Bus Owner may discover endpoints, assign or confirm EIDs, maintain routes and neighbours, query endpoint UUIDs and Message Types, and handle endpoint insertion/removal.

The BMC often acts as Bus Owner, but not in every topology. If two managers modify EIDs, routes, and neighbours simultaneously, they can cause EID conflicts, overwritten routes, messages sent to the wrong device, duplicate endpoints, and inconsistent hot-plug state. Production systems must define ownership of routing state.

---

# 24. Role of `mctpd`

In kernel-based OpenBMC MCTP, `mctpd` may manage interfaces, discover endpoints, assign EIDs, create routes/neighbours, query endpoint properties, expose D-Bus objects, and handle endpoint insertion/removal.

```text
MCTP Endpoint
      |
Linux MCTP Transport and Core
      +--> mctpd: Endpoint and Routing Management
      +--> PLDM/SPDM Applications: Protocol Messages
```

| Component | Primary Responsibility |
| --- | --- |
| `mctpd` | Endpoints, EIDs, routes, neighbours |
| PLDM Application | PLDM payload processing |
| SPDM Application | Authentication, measurements, secure sessions |
| Linux MCTP Core | Sockets, routing, message delivery |
| Transport Driver | Packet transport over the physical bus |

---

# 25. Kernel MCTP Versus Older Userspace MCTP

OpenBMC has also used userspace MCTP implementations in which a library directly managed the transport. This lab focuses on kernel MCTP:

```text
AF_MCTP Socket
net/mctp/
drivers/net/mctp/
```

Kernel routes are not the same as older userspace `libmctp` routing tables. Before debugging, determine whether the platform uses kernel MCTP, userspace `libmctp`, or a transitional hybrid architecture.

---

# 26. Kernel Source Map

```text
net/mctp/
drivers/net/mctp/
include/net/mctp.h
include/uapi/linux/mctp.h
Documentation/networking/mctp.rst
```

| Location | Purpose |
| --- | --- |
| `net/mctp/` | Sockets, routes, neighbours, devices, core protocol logic |
| `drivers/net/mctp/` | Transport drivers for I2C, serial, PCIe, USB, and others |
| `include/uapi/linux/mctp.h` | Userspace AF_MCTP API |
| `Documentation/networking/mctp.rst` | Linux MCTP documentation |

Recommended reading: UAPI header → documentation → `af_mctp.c` → `route.c` → `neigh.c` → target transport driver. File names may vary by kernel version.

---

# 27. Kernel Configuration

Enable the core:

```text
CONFIG_MCTP
```

Also enable the required I2C, I3C, serial, USB, PCIe/PCC, or other transport symbols for the kernel version.

```sh
grep -R "config MCTP" net/mctp drivers/net/mctp
zcat /proc/config.gz | grep MCTP
grep MCTP /boot/config-$(uname -r)
```

`CONFIG_MCTP` alone is insufficient; the system also needs the transport and controller drivers, Device Tree/ACPI, interface address, route, neighbour, and endpoint configuration.

---

# 28. Role of the Device Tree

The examples cover MCTP over I2C, I3C, and muxed I2C. Device Tree describes controller capability, transport hardware resources, I2C/I3C controllers, mux topology, interrupts, GPIOs, and SoC-specific configuration.

EIDs are normally runtime platform policy and should not be copied into production merely because an example uses them. Follow the target kernel version, transport binding, controller binding, and OpenBMC platform design.

---

# 29. MCTP over I2C and a Mux

```text
BMC I2C Controller
        |
      I2C Mux
        +-- Channel 0 --> Endpoint A
        +-- Channel 1 --> Endpoint B
```

Linux must establish the parent adapter, mux device, child adapter, MCTP-over-I2C interface, and endpoint route/neighbour.

Debug in order:

```text
I2C Controller --> Mux Driver --> Child Adapter
    --> MCTP Transport --> Route/Neighbour --> Endpoint
```

An MCTP timeout may originate from the mux channel, pull-up, endpoint power, I2C address, route, neighbour, or EID conflict—not only the AF_MCTP application.

---

# 30. Network Namespace Lab

Network namespaces provide isolated network stacks and can teach local MCTP addresses, interface state, routes, neighbours, request/response behavior, incorrect routes, and endpoint isolation.

```text
Namespace A                         Namespace B
Requester                           Responder
EID 8                               EID 9
   +--------- Virtual MCTP Link --------+
```

The lab validates Linux routing and AF_MCTP behavior but does not replace real I2C/I3C/PCIe hardware testing.

---

# 31. Layered Debugging Procedure

## Layer 1: Application

Verify socket creation, Message Type, Destination EID, payload, request tag, timeout, and retry behavior.

## Layer 2: Local Address

Verify that the interface has a local EID so Linux can set Source EID and receive responses.

## Layer 3: Route

```sh
mctp route
```

Without a route, the message cannot reach the transport interface.

## Layer 4: Neighbour

For transports that need it, verify the Destination EID to link-layer-address mapping.

## Layer 5: MCTP Interface

Verify that the interface exists and is up, Network ID and MTU are correct, a local EID exists, and the interface is in the expected namespace.

## Layer 6: Transport Driver

Verify probe, controller state, transmitted and received packets, transport headers, timeouts, and retries.

## Layer 7: Physical Bus

- I2C/I3C: pull-ups, addresses, clocks, mux channel, ACK/NACK, and multi-master ownership.
- PCIe VDM: link, BDF/routing, VDM support, and endpoint firmware.
- USB: enumeration, interface binding, endpoints, disconnect/resume.
- Serial: baud rate, framing, flow control, and line noise.

## Layer 8: Remote Endpoint

Verify power, assigned EID, Message Type support, responder service, response tag, and whether another Bus Owner reconfigured the endpoint.

---

# 32. Common Mistakes

- **Treating EID as I2C address:** They belong to different layers.
- **Interface without route:** An interface alone does not make an endpoint reachable.
- **Route without neighbour:** Linux may know the interface but not the physical destination.
- **Requester/responder type mismatch:** A `0x7E` responder does not receive `0x01`.
- **Treating MCTP as PLDM:** MCTP transports messages; PLDM defines commands and data.
- **Assuming MCTP provides security:** Authentication and encryption require SPDM or another security protocol.
- **Copying example EIDs:** This can cause conflicts and incorrect routing/inventory mapping.
- **Conflicting manual configuration and `mctpd`:** Routing state may be overwritten.
- **Checking only the application:** A timeout can originate in transport, mux, route, neighbour, or remote endpoint.

---

# 33. OpenBMC Use Cases

## GPU Management

```text
OpenBMC PLDM Application --> AF_MCTP --> MCTP over PCIe VDM
    --> GPU Management Controller
```

Uses include inventory, temperature, firmware update, fault events, and device control.

## NVMe Management

```text
OpenBMC NVMe-MI Application --> MCTP --> NVMe Endpoint
```

Uses include health information, controller status, subsystem management, and sideband diagnostics.

## Security Device

```text
OpenBMC SPDM Requester --> MCTP --> Remote Device
```

Uses include certificates, authentication, measurements, and secure sessions.

## Add-In Cards

MCTP lets the BMC use a common upper-layer management model for add-in cards on different transports.

---

# 34. Recommended Learning Sequence

1. Understand endpoints and EIDs.
2. Distinguish Message Types from transport bindings.
3. Understand routes versus neighbours.
4. Examine Linux MCTP interfaces.
5. Create two endpoints with the namespace lab.
6. Run the echo responder.
7. Send a vendor-defined request.
8. Observe Source/Destination EIDs and Message Tags.
9. Remove a route intentionally and observe the error.
10. Configure an incorrect neighbour and observe the timeout.
11. Study MCTP over I2C/I3C.
12. Study OpenBMC `mctpd`.
13. Continue with PLDM, SPDM, or NVMe-MI.

---

# 35. Summary of Core Concepts

1. **MCTP is a management-message transport layer, not PLDM, SPDM, or NVMe-MI itself.**
2. **A transport binding carries packets; Message Type identifies the payload protocol.**
3. **An EID is an MCTP logical address—not an I2C address, PCIe BDF, or permanent identity.**
4. **A route selects the interface; a neighbour maps an EID to a link-layer address.**
5. **Linux applications send and receive complete messages through AF_MCTP sockets.**
6. **Message Types allow PLDM, SPDM, NVMe-MI, and vendor applications to share one network.**
7. **Message Tag and Tag Owner match requests with responses.**
8. **`mctpd` manages endpoints, EIDs, routes, neighbours, and D-Bus information; upper-layer applications process protocol payloads.**
9. **MCTP does not imply authentication or encryption; security requires SPDM or another protocol.**
10. **Production EIDs and routes are platform policy and must not be copied directly from lab examples.**
11. **Debug in layers: Application, Address, Route, Neighbour, Interface, Transport, Physical Bus, and Remote Endpoint.**
