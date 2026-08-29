# Device Tree

AST2600 SoC DTSI defines an LPC/eSPI-facing syscon and KCS register blocks. A
board DTS selects a channel, assigns the host-visible I/O address, and enables
it. A common KCS3 address is `0xca2`, but firmware and hardware must agree.

    &kcs3 {
        status = "okay";
        aspeed,lpc-io-reg = <0xca2>;
    };

## Key properties

| Property | Meaning |
|---|---|
| `compatible` | ASPEED generation/binding match |
| `reg` | BMC-side IDR, ODR, and STR register resources |
| `interrupts` | interrupt delivered to the BMC CPU |
| `aspeed,lpc-io-reg` | host-visible data address and optional status address |
| `aspeed,lpc-interrupts` | optional host SIRQ number and polarity |
| `status` | board-level channel enablement |

The BMC interrupt and host SIRQ are different. `interrupts` describes the IRQ
into the BMC kernel. `aspeed,lpc-interrupts` configures an interrupt generated
toward the host and must match host firmware routing.

## Address coordination

Coordinate these sources:

- board schematic and chipset eSPI/LPC routing,
- BMC Device Tree host I/O decode,
- BIOS ACPI/SMBIOS IPMI interface description,
- host kernel `ipmi_si` parameters or enumeration,
- OpenBMC kcsbridge service instance.

Do not enable two KCS channels at overlapping I/O addresses. Do not add a host
SIRQ merely because an example uses IRQ 11; polling may be the intended host
mode, or the platform may route another SIRQ.

The examples only modify board-level KCS properties. They do not invent a
generic eSPI controller node because bindings and driver coverage must be taken
from the exact platform kernel.

