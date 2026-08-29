# eSPI and KCS protocol fundamentals

## eSPI channel model

Enhanced Serial Peripheral Interface connects a host chipset to a BMC or other
peripheral controller. It replaces many LPC-era signals with a packetized,
low-pin-count link. The four logical channels have different jobs.

| Channel | Typical use |
|---|---|
| Peripheral | Host I/O and memory cycles, including KCS register windows |
| Virtual Wire | Reset, power, sleep, boot and platform sideband signals |
| OOB | Management messages independent of normal host I/O cycles |
| Flash | Host access to flash through the eSPI endpoint/controller |

Link training and channel enablement are separate. A healthy Physical layer
does not prove that KCS I/O decoding, Virtual Wires, OOB, or Flash access works.
Host chipset policy, straps, BIOS, BMC pinmux and both endpoints must agree.

## KCS register protocol

Keyboard Controller Style (KCS) is an IPMI system interface with data and
status/command registers. The host writes a request byte-by-byte; the BMC state
machine receives it and returns a response byte-by-byte.

Important status bits include:

- IBF: host input buffer contains a byte for the BMC.
- OBF: BMC output buffer contains a byte for the host.
- CMD/DAT: incoming byte is a control code or data.
- SMS_ATN: BMC requests attention from system-management software.
- state bits: IDLE, READ, WRITE, or ERROR.

Important control codes are `WRITE_START`, `WRITE_END`, `READ_BYTE`, and
`GET_STATUS/ABORT`. The Linux BMC driver implements the state transitions and
reports protocol errors such as illegal control code or excessive length.

## IPMI framing above KCS

KCS moves an IPMI request containing NetFn/LUN, command, and optional data. The
response contains response NetFn/LUN, command, completion code, and data. KCS
does not implement the command itself; `phosphor-host-ipmid` dispatches it to an
IPMI provider or another D-Bus service.

Do not use LAN-session assumptions for KCS. It is a local, session-less system
interface, but commands still require channel policy, validation, timeouts, and
privilege treatment.

