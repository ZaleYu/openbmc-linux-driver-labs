# Device Tree

The AST2600 SoC include already declares `peci0` with MMIO, IRQ, clock, reset,
timeout, and frequency. A board DTS normally only enables the existing node:

    &peci0 {
        status = "okay";
    };

Do not duplicate the controller resources in a board DTS. The full node belongs
in the SoC `.dtsi`; the board supplies wiring-dependent enablement and only
overrides validated timing properties.

## Binding properties

| Property | Role |
|---|---|
| `compatible` | ASPEED generation match |
| `reg` | controller MMIO resource |
| `interrupts` | transfer completion/error IRQ |
| `clocks` | external reference clock |
| `resets` | controller reset control |
| `clock-frequency` | requested PECI operating frequency, 2 kHz–2 MHz |
| `cmd-timeout-ms` | command timeout, 1–1000 ms |
| `status` | board-level enablement |

The current ASPEED binding defaults to 1 MHz and a 1000 ms timeout. Do not
shorten the timeout to hide boot latency or increase frequency to hide a signal
integrity problem. Validate against the schematic, CPU platform requirements,
waveform, and worst-case commands.

## Multi-socket systems

One controller can discover multiple CPU packages at standard PECI addresses.
Do not add `cpu@30`, `cpu@31`, or hwmon child nodes: Linux scans and identifies
responders. OpenBMC inventory must still map each discovered package/socket to
the correct chassis and CPU object.

