# Linux eSPI and KCS for OpenBMC

This lab explains how a host exchanges IPMI messages with a BMC through eSPI/LPC.

```text
eSPI = Low-level transport bus between Host and BMC
KCS  = IPMI System Interface built on that transport
IPMI = Management command and response definitions
```

These components have different responsibilities and must not be confused.

---

# 1. Overall Architecture

```text
Host BIOS / OS
        |
Host IPMI System Interface Driver
        |
KCS Data / Status Registers
        |
LPC or eSPI Peripheral Channel
        |
ASPEED KCS BMC Driver
        |
/dev/ipmi-kcsN
        |
kcsbridge
        |
phosphor-host-ipmid
        |
OpenBMC D-Bus Services
        |
IPMI Response
```

The response returns through the reverse path to the Host IPMI driver.

---

# 2. What Is eSPI?

eSPI stands for Enhanced Serial Peripheral Interface. It is a low-pin-count system-management bus between a host and a BMC, EC, or other management controller.

```text
Older Platform: Host/PCH <-- LPC --> BMC
Newer Platform: Host/PCH <-- eSPI --> BMC
```

eSPI is not the ordinary SPI bus used for sensors. Despite its name, it defines its own channels, protocol, ownership, and platform-management behavior; it is not equivalent to a Device Tree SPI controller with CS, MOSI, and MISO.

---

# 3. Four Main eSPI Channels

| Channel | Primary Purpose |
| --- | --- |
| Peripheral | I/O, memory, DMA, and other peripheral cycles |
| Virtual Wire | Reset, power, sleep, SMI/NMI, and sideband signals |
| OOB | Out-of-band packets |
| Flash | Host/PCH access to BMC-managed flash |

Each channel has a separate function and may have a separate enable/negotiation state.

---

# 4. Peripheral Channel

This channel carries LPC-like I/O and memory accesses, KCS registers, UART/register windows, mailboxes, and platform-specific cycles.

KCS usually uses the eSPI Peripheral Channel:

```text
Host I/O Read --> eSPI Peripheral Channel
    --> BMC eSPI Controller --> KCS Hardware Register
```

---

# 5. Virtual Wire Channel

Virtual Wire replaces some physical sideband pins and may carry platform/host reset, sleep and power states, SMI, NMI, SCI, boot status, and other platform signals.

KCS availability may depend on these reset and power states. A loaded BMC KCS driver does not mean the host can send normal requests while still in reset.

---

# 6. OOB Channel

OOB means Out-of-Band. This packet-oriented channel may carry MCTP, platform-management messages, vendor protocols, or BMC/PCH communication.

```text
KCS --> Byte-oriented IPMI System Interface
OOB --> Packet-oriented Out-of-Band Channel
```

An OOB packet is not a KCS byte stream.

---

# 7. Flash Channel

The Flash Channel lets the Host/PCH access flash controlled or proxied by the BMC for firmware access, BIOS sharing, arbitration, update, protection, and recovery.

It has independent ownership, permissions, protected regions, arbitration, and security. Working KCS does not prove that Flash Channel is enabled correctly; validate every channel separately.

---

# 8. Relationship Among eSPI Channels

```text
Host/PCH --> eSPI Bus
              +-- Peripheral: KCS and I/O Cycles
              +-- Virtual Wire: Reset, Power, Sleep
              +-- OOB: Packet Management
              +-- Flash: BIOS/Host Flash Access
```

An established link does not mean every channel is enabled. An enabled Peripheral Channel also does not prove that the KCS decode window, driver, and userspace are configured.

---

# 9. What Is KCS?

KCS stands for Keyboard Controller Style. It is an IPMI System Interface through which the host exchanges IPMI requests/responses with the BMC using Data and Command/Status registers.

It is byte-oriented, state-machine based, transfers one byte at a time, suits low-frequency management commands, and is not intended for high-volume data.

---

# 10. KCS Is Not an IPMI Command Handler

KCS transports a sequence of IPMI bytes; it does not interpret Get Device ID, Get Sensor Reading, Chassis Control, or Set Boot Options.

| Component | Responsibility |
| --- | --- |
| eSPI/LPC | Carries host I/O cycles |
| KCS Hardware/Driver | Implements the byte-oriented interface |
| `kcsbridge` | Bridges the character device to the OpenBMC IPMI service |
| `phosphor-host-ipmid` | Parses and handles IPMI commands |
| D-Bus Services | Provide sensor, inventory, power, and other functions |

---

# 11. KCS Registers

KCS normally exposes a Data Register and a Command/Status Register.

The Data Register carries request bytes (`NetFn/LUN`, command, payload) and response bytes (`NetFn/LUN`, command, Completion Code, response data).

The same I/O location may act as Command Register on host writes and Status Register on host reads. Status includes Input Buffer Full, Output Buffer Full, KCS state, errors, and control information. Follow the IPMI KCS System Interface specification for exact bits.

---

# 12. Host I/O Ports

The host accesses KCS through host-visible I/O ports, not BMC MMIO. A conceptual pair is:

```text
Data Port           = 0xCA2
Command/Status Port = 0xCA3
```

Actual ports depend on schematic, BIOS, ACPI, eSPI/LPC decode, BMC Device Tree, KCS channel, and platform policy.

```text
BIOS/ACPI Port = PCH Decode Window
               = BMC KCS Hardware Setting
               = Device Tree I/O Address
```

Any mismatch can make the host driver appear while every transaction times out.

---

# 13. KCS Channels

ASPEED BMCs may provide several hardware channels with different host ports, purposes, devices, host interfaces, and security policies.

Linux may create `/dev/ipmi-kcs0`, `/dev/ipmi-kcs1`, etc., but the character-device number is not guaranteed to equal the hardware channel number. Determine the mapping through sysfs, Device Tree, and kernel logs.

---

# 14. KCS State Machine

KCS is not a simple FIFO. Host and BMC follow states such as IDLE, WRITE, READ, and ERROR:

```text
IDLE
  | Write Start
  v
WRITE -- Request Bytes / Write End --> BMC Handles Command
  |
  v
READ -- Host Reads Response Bytes --> IDLE
```

Incorrect handling of Input/Output Buffer Full, state transitions, Write End, or Read Byte commands can enter ERROR state.

---

# 15. Example KCS Request

For `Get Device ID`:

```text
Host Driver --> Write Start --> NetFn/LUN --> Command --> Write End
    --> BMC Receives Complete Request
    --> phosphor-host-ipmid Handles It
    --> Completion Code + Response
    --> KCS READ State --> Host Reads Bytes
```

The KCS driver sees bytes; `phosphor-host-ipmid` understands the command.

---

# 16. BMC-Side Linux Architecture

```text
ASPEED KCS Hardware
        |
KCS BMC Controller Driver
        |
Linux KCS BMC Core
        |
/dev/ipmi-kcsN
        |
Userspace Owner
```

Source is commonly under `drivers/char/ipmi/kcs_bmc.c` and related files. The ASPEED driver configures channels and decode addresses, handles host I/O interrupts, transfers bytes to the core, and manages status bits. The core provides a generic character-device interface.

---

# 17. `/dev/ipmi-kcsN`

This is the BMC-side KCS character device. Userspace can receive host requests, return responses, wait for events, or run an authorized lab responder.

It is not an ordinary file or BMC shell command interface. `cat /dev/ipmi-kcs0` is not a correct test and may steal ownership from the production service.

---

# 18. `kcsbridge`

```text
/dev/ipmi-kcs0 --> kcsbridge
    --> OpenBMC Host IPMI D-Bus Interface
    --> phosphor-host-ipmid
```

It opens the device, waits for host requests, reads complete IPMI messages, forwards them to the service, obtains responses, and writes them back. It must not share the device with a test program.

---

# 19. `phosphor-host-ipmid`

This service handles host-interface IPMI commands such as device information, sensors, chassis control, boot options, storage/events, and OEM commands. It may call sensor, inventory, power, SEL/event, host-state, and OEM D-Bus services.

---

# 20. Host IPMI Versus Network IPMI

```text
Host IPMI: Host OS --> KCS --> eSPI/LPC --> BMC
Network IPMI: Remote Client --> Ethernet --> RMCP/RMCP+ --> BMC
```

OpenBMC commonly uses `phosphor-host-ipmid` for host KCS and a network IPMI service such as `phosphor-net-ipmid` for network traffic. Both carry IPMI commands but use different transports.

---

# 21. BMC Shell Is Not Host KCS Either

Running `ipmitool` on the BMC does not necessarily use Host KCS. The path depends on the selected interface.

From the host, `ipmitool -I open mc info` normally uses the host kernel’s IPMI System Interface, possibly KCS. From another machine, `ipmitool -I lanplus -H <bmc-ip> ...` uses Network IPMI.

When debugging, identify the machine, `ipmitool` interface, and actual transport.

---

# 22. Repository Structure

| Directory | Contents |
| --- | --- |
| `docs/` | Protocol, Linux architecture, Device Tree, tools, debugging, and OpenBMC use cases |
| `kernel-study/` | Upstream KCS source map and kernel configuration |
| `device-tree/` | AST2600 KCS channel examples |
| `userspace/` | Read-only inventory and protected lab responder |
| `scripts/` | Inventory, stack diagnostics, and ownership checks |

---

# 23. Device Tree

The SoC DTSI normally defines AST2600 KCS channels; the board DTS enables one and configures its host I/O address:

```dts
&kcs3 {
    status = "okay";
    aspeed,lpc-io-reg = <0xca2>;
};
```

This is conceptual. Follow the target kernel’s IPMI/ASPEED KCS binding and current `compatible` list.

---

# 24. What Must Be Verified in Device Tree?

Verify the hardware channel, `compatible`, host I/O base, register range, interrupt, parent eSPI/LPC controller, resource conflicts, `status`, and matching BIOS/ACPI ports.

Do not copy channel, host port, IRQ, or register offset from another board; server boards using the same AST2600 may have different decode plans.

---

# 25. Device Tree Differences Between eSPI and LPC

KCS functionality is similar, but the physical path may use LPC or eSPI Peripheral Channel. Confirm host-interface selection, straps/pinmux, eSPI controller and Peripheral Channel state, decode windows, KCS attachment, and host firmware configuration.

The presence of `/dev/ipmi-kcs0` proves only that the BMC kernel driver registered; it does not prove that the host-to-BMC path works.

---

# 26. Quick Start

```sh
make -C userspace
./userspace/kcs_inventory
./scripts/debug_espi_kcs.sh
sudo ./scripts/debug_espi_kcs.sh
```

Expected tools include `kcs_inventory` and `kcs_lab_responder`. Inventory should inspect `/dev/ipmi-kcs*`, sysfs, binding, channel, OpenBMC services, and ownership without taking over the production channel.

Diagnostics may check kernel configuration, Device Tree, character devices, drivers, logs, `kcsbridge`, `phosphor-host-ipmid`, and device owner.

---

# 27. Lab Responder Safety Gate

The responder must require an explicit `--lab` option:

```sh
sudo ./userspace/kcs_lab_responder \
    --lab /dev/ipmi-kcs0
```

The exact syntax depends on the tool. This prevents accidental execution on a production BMC. A lab responder can consume host requests, return fixed test responses, replace `kcsbridge`, and expose the host to non-production data. Never run it alongside production `kcsbridge`.

---

# 28. One Userspace Owner per KCS Device

Correct production ownership:

```text
/dev/ipmi-kcs0 --> kcsbridge
```

Correct lab ownership:

```text
/dev/ipmi-kcs0 --> kcs_lab_responder
```

Two competing owners can cause open failures, stolen requests, mismatched responses, a stuck state machine, host timeouts, invalid Completion Codes, and loss of host-management functionality.

---

# 29. Checking Device Ownership

```sh
sudo lsof /dev/ipmi-kcs0
sudo fuser -v /dev/ipmi-kcs0
systemctl status kcsbridge
systemctl status phosphor-host-ipmid
systemctl list-units | grep -Ei 'kcs|ipmi'
```

Unit names may be instantiated, such as `kcsbridge@0.service`. Do not start a lab responder before confirming ownership.

---

# 30. Correct Lab Test Procedure

Use only an authorized development board, isolated environment, unused channel, or setup where host-IPMI interruption is acceptable.

```text
1. Identify the Test Channel
2. Confirm No Production Impact
3. Stop Its kcsbridge
4. Confirm No Owner with lsof/fuser
5. Start the --lab Responder
6. Send a Test IPMI Command from the Host
7. Stop the Lab Responder
8. Restart kcsbridge
9. Verify Production IPMI Recovery
```

Stopping production service interrupts host IPMI and requires explicit lab authorization.

---

# 31. Host-Side Validation

```text
ipmitool --> /dev/ipmi0 --> Host OpenIPMI/System Interface
    --> KCS I/O Ports
```

```sh
ls -l /dev/ipmi*
dmesg | grep -Ei 'ipmi|kcs'
lsmod | grep -E 'ipmi|kcs'
sudo ipmitool -I open mc info
```

Common modules include `ipmi_si`, `ipmi_devintf`, and `ipmi_msghandler`, depending on kernel/platform. A successful command shows that the host userspace, host kernel driver, KCS, eSPI/LPC, and BMC IPMI service are basically working.

---

# 32. BMC-Side Validation

```sh
ls -l /dev/ipmi-kcs*
find /sys -path '*kcs*' -maxdepth 6 -print 2>/dev/null
ls -l /sys/bus/platform/drivers/ | grep -i kcs
dmesg | grep -Ei 'espi|lpc|kcs|ipmi'
systemctl list-units | grep -Ei 'kcs|ipmi'
sudo lsof /dev/ipmi-kcs0
```

---

# 33. Kernel Configuration

The BMC kernel needs KCS BMC core, ASPEED KCS driver, character-device support, the relevant eSPI/LPC controller, platform driver, and Device Tree support. Common symbols include:

```text
CONFIG_IPMI_KCS_BMC
CONFIG_ASPEED_KCS_IPMI_BMC
```

Symbols vary by kernel:

```sh
grep -R "config.*KCS" drivers/char/ipmi
zcat /proc/config.gz | grep -E 'KCS|IPMI|ESPI|LPC'
grep -E 'KCS|IPMI|ESPI|LPC' /boot/config-$(uname -r)
```

The Host kernel is a KCS/IPMI consumer; the BMC kernel is a KCS responder. Do not confuse host `ipmi_si` with BMC `kcs_bmc`.

---

# 34. Device Tree Schema Validation

```sh
make dt_binding_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/ipmi/

make dtbs_check \
    DT_SCHEMA_FILES=Documentation/devicetree/bindings/ipmi/
```

Check `compatible`, register range, channel, host I/O address, interrupt, undefined/required properties, and parent eSPI/LPC controller.

Schema success does not prove host decode correctness; BIOS, ACPI, PCH, and BMC settings must still agree.

---

# 35. Layered Debugging Procedure

## Layer 1: Host Application

Verify `ipmitool -I open`, `/dev/ipmi0`, host IPMI drivers, and that a network interface was not selected accidentally.

## Layer 2: Host IPMI Driver

Verify `ipmi_si` probe, ACPI/SMBIOS port, KCS interface type, and absence of repeated timeouts.

## Layer 3: Host I/O Decode

Verify Data/Status ports, PCH/eSPI Peripheral decode, matching BIOS/BMC ports, and no legacy-device conflict.

## Layer 4: eSPI/LPC

Verify the physical link, reset, Peripheral Channel, Virtual Wire/power state, and error counters.

## Layer 5: BMC KCS Driver

Verify enabled node, successful probe, interrupts, character device, and channel-to-port mapping.

## Layer 6: Device Ownership

```sh
sudo lsof /dev/ipmi-kcsN
```

Only the expected owner should exist.

## Layer 7: `kcsbridge`

Verify service state, correct device, no I/O errors, request forwarding, and response writes.

## Layer 8: `phosphor-host-ipmid`

Verify service state, registered handlers, available D-Bus services, no exceptions, and correct Completion Codes.

---

# 36. Common Problems

## `/dev/ipmi-kcs0` Does Not Exist

Possible causes: disabled kernel configuration or Device Tree, probe failure, uninitialized parent controller, or character-device creation failure.

## BMC Device Exists but Host Finds No IPMI

Possible causes: missing BIOS/ACPI declaration, port mismatch, disabled Peripheral Channel, incorrect power/reset state, missing PCH decode, or wrong channel.

## Host Finds KCS but Every Command Times Out

Possible causes: stopped `kcsbridge`, wrong device, competing owner, unresponsive `phosphor-host-ipmid`, stuck state machine, or port mismatch.

## Network IPMI Works but Host KCS Fails

This is possible because Network IPMI uses Ethernet while Host IPMI uses KCS/eSPI/LPC. Network success does not prove the KCS path.

## Lab Responder Cannot Open the Device

Production `kcsbridge` may own it. Do not forcibly take it over.

## Host Receives an Invalid Response

Check responder formatting, NetFn/LUN, command, Completion Code, length, and request/response state synchronization.

---

# 37. Security and Product Considerations

KCS is a host-to-BMC management entry point that may allow power and chassis control, boot options, sensor queries, OEM commands, firmware/platform management, and event access.

- Only authorized services should open the device.
- Never run a lab responder in production.
- Do not log sensitive payloads.
- Validate permissions and inputs for OEM commands.
- Recover correctly from host resets/timeouts.
- Malformed requests must not crash daemons.
- KCS flooding must not exhaust BMC resources.
- Manage eSPI channel and Flash permissions separately.
- Do not make device-node permissions overly broad.

An internal channel is not inherently secure.

---

# 38. Recommended Learning Sequence

1. Distinguish eSPI, KCS, and IPMI.
2. Learn the four eSPI channels.
3. Understand KCS Data/Status registers.
4. Trace the KCS write/read state machine.
5. Map host I/O ports to BMC channels.
6. Examine `/dev/ipmi-kcsN`.
7. Check `kcsbridge` ownership.
8. Trace requests to `phosphor-host-ipmid`.
9. Test from the host with `ipmitool -I open`.
10. Compare host and network IPMI.
11. Test the lab responder only in isolation.
12. Then study eSPI OOB, Virtual Wire, and Flash Channel.

---

# 39. Summary of Core Concepts

1. **eSPI is the Host/BMC bus; KCS is the byte-oriented IPMI System Interface; IPMI defines management commands.**
2. **KCS usually travels over LPC or the eSPI Peripheral Channel.**
3. **eSPI also has Virtual Wire, OOB, and Flash channels with distinct functions.**
4. **KCS uses Data and Command/Status registers plus a strict read/write state machine.**
5. **The Host uses an IPMI System Interface driver; the BMC uses a KCS BMC driver.**
6. **`/dev/ipmi-kcsN` is a BMC character device, not a shell command interface.**
7. **`kcsbridge` forwards character-device requests to `phosphor-host-ipmid`.**
8. **`phosphor-host-ipmid` interprets commands and queries D-Bus sensor, power, and inventory services.**
9. **Host KCS IPMI, Network IPMI, and the BMC shell are different paths.**
10. **One KCS character device can have only one userspace owner at a time.**
11. **The lab responder must require `--lab` and never run while production `kcsbridge` owns the device.**
12. **A valid DTS and existing `/dev/ipmi-kcsN` do not prove the complete eSPI/LPC path.**
13. **Debug in layers: Host Driver, I/O Decode, eSPI/LPC, BMC Driver, Ownership, `kcsbridge`, and `phosphor-host-ipmid`.**
